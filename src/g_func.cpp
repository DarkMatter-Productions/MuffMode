// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include "g_local.h"

/*
=========================================================

  PLATS

  movement options:

  linear
  smooth start, hard stop
  smooth start, smooth stop

  start
  end
  acceleration
  speed
  deceleration
  begin sound
  end sound
  target fired when reaching end
  wait at end

  object characteristics that use move segments
  ---------------------------------------------
  movetype_push, or movetype_stop
  action when touched
  action when blocked
  action when used
	disabled?
  auto trigger spawning


=========================================================
*/

constexpr spawnflags_t SPAWNFLAG_DOOR_START_OPEN = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAG_DOOR_CRUSHER = 4_spawnflag;
constexpr spawnflags_t SPAWNFLAG_DOOR_NOMONSTER = 8_spawnflag;
constexpr spawnflags_t SPAWNFLAG_DOOR_ANIMATED = 16_spawnflag;
constexpr spawnflags_t SPAWNFLAG_DOOR_TOGGLE = 32_spawnflag;
constexpr spawnflags_t SPAWNFLAG_DOOR_ANIMATED_FAST = 64_spawnflag;

constexpr spawnflags_t SPAWNFLAG_DOOR_ROTATING_X_AXIS = 64_spawnflag;
constexpr spawnflags_t SPAWNFLAG_DOOR_ROTATING_Y_AXIS = 128_spawnflag;
constexpr spawnflags_t SPAWNFLAG_DOOR_ROTATING_INACTIVE = 0x10000_spawnflag; // Paril: moved to non-reserved
constexpr spawnflags_t SPAWNFLAG_DOOR_ROTATING_SAFE_OPEN = 0x20000_spawnflag;

// support routine for setting moveinfo sounds
static inline int32_t G_GetMoveinfoSoundIndex(gentity_t *self, const char *default_value, const char *wanted_value) {
	if (!wanted_value) {
		if (default_value)
			return gi.soundindex(default_value);

		return 0;
	} else if (!*wanted_value || *wanted_value == '0' || *wanted_value == ' ')
		return 0;

	return gi.soundindex(wanted_value);
}

void G_SetMoveinfoSounds(gentity_t *self, const char *default_start, const char *default_mid, const char *default_end) {
	self->moveinfo.sound_start = G_GetMoveinfoSoundIndex(self, default_start, st.noise_start);
	self->moveinfo.sound_middle = G_GetMoveinfoSoundIndex(self, default_mid, st.noise_middle);
	self->moveinfo.sound_end = G_GetMoveinfoSoundIndex(self, default_end, st.noise_end);
}

//
// Support routines for movement (changes in origin using velocity)
//

static THINK(Move_Done) (gentity_t *ent) -> void {
	ent->velocity = {};
	ent->moveinfo.endfunc(ent);
}

static THINK(Move_Final) (gentity_t *ent) -> void {
	if (ent->moveinfo.remaining_distance == 0) {
		Move_Done(ent);
		return;
	}

	// [Paril-KEX] use exact remaining distance
	ent->velocity = (ent->moveinfo.dest - ent->s.origin) * (1.f / gi.frame_time_s);

	ent->think = Move_Done;
	ent->nextthink = level.time + FRAME_TIME_S;
}

static THINK(Move_Begin) (gentity_t *ent) -> void {
	float frames;

	if ((ent->moveinfo.speed * gi.frame_time_s) >= ent->moveinfo.remaining_distance) {
		Move_Final(ent);
		return;
	}
	ent->velocity = ent->moveinfo.dir * ent->moveinfo.speed;
	frames = floor((ent->moveinfo.remaining_distance / ent->moveinfo.speed) / gi.frame_time_s);
	ent->moveinfo.remaining_distance -= frames * ent->moveinfo.speed * gi.frame_time_s;
	ent->nextthink = level.time + (FRAME_TIME_S * frames);
	ent->think = Move_Final;
}

void Think_AccelMove_New(gentity_t *ent);
void Think_AccelMove(gentity_t *ent);
bool Think_AccelMove_MoveInfo(moveinfo_t *moveinfo);

static constexpr float AccelerationDistance(float target, float rate) {
	return (target * ((target / rate) + 1) / 2);
}

static inline void Move_Regular(gentity_t *ent, const vec3_t &dest, void(*endfunc)(gentity_t *self)) {
	if (level.current_entity == ((ent->flags & FL_TEAMSLAVE) ? ent->teammaster : ent)) {
		Move_Begin(ent);
	} else {
		ent->nextthink = level.time + FRAME_TIME_S;
		ent->think = Move_Begin;
	}
}

void Move_Calc(gentity_t *ent, const vec3_t &dest, void(*endfunc)(gentity_t *self)) {
	ent->velocity = {};
	ent->moveinfo.dest = dest;
	ent->moveinfo.dir = dest - ent->s.origin;
	ent->moveinfo.remaining_distance = ent->moveinfo.dir.normalize();
	ent->moveinfo.endfunc = endfunc;

	if (ent->moveinfo.speed == ent->moveinfo.accel && ent->moveinfo.speed == ent->moveinfo.decel) {
		Move_Regular(ent, dest, endfunc);
	} else {
		// accelerative
		ent->moveinfo.current_speed = 0;

		if (gi.tick_rate == 10)
			ent->think = Think_AccelMove;
		else {
			// [Paril-KEX] rewritten to work better at higher tickrates
			ent->moveinfo.curve_frame = 0;
			ent->moveinfo.num_subframes = (0.1f / gi.frame_time_s) - 1;

			float total_dist = ent->moveinfo.remaining_distance;

			std::vector<float> distances;

			if (ent->moveinfo.num_subframes) {
				distances.push_back(0);
				ent->moveinfo.curve_frame = 1;
			} else
				ent->moveinfo.curve_frame = 0;

			// simulate 10hz movement
			while (ent->moveinfo.remaining_distance) {
				if (!Think_AccelMove_MoveInfo(&ent->moveinfo))
					break;

				ent->moveinfo.remaining_distance -= ent->moveinfo.current_speed;
				distances.push_back(total_dist - ent->moveinfo.remaining_distance);
			}

			if (ent->moveinfo.num_subframes)
				distances.push_back(total_dist);

			ent->moveinfo.subframe = 0;
			ent->moveinfo.curve_ref = ent->s.origin;
			ent->moveinfo.curve_positions = make_savable_memory<float, TAG_LEVEL>(distances.size());
			std::copy(distances.begin(), distances.end(), ent->moveinfo.curve_positions.ptr);

			ent->moveinfo.num_frames_done = 0;

			ent->think = Think_AccelMove_New;
		}

		ent->nextthink = level.time + FRAME_TIME_S;
	}
}

THINK(Think_AccelMove_New) (gentity_t *ent) -> void {
	float t = 0.f;
	float target_dist;

	if (ent->moveinfo.num_subframes) {
		if (ent->moveinfo.subframe == ent->moveinfo.num_subframes + 1) {
			ent->moveinfo.subframe = 0;
			ent->moveinfo.curve_frame++;

			if (ent->moveinfo.curve_frame == ent->moveinfo.curve_positions.count) {
				Move_Final(ent);
				return;
			}
		}

		t = (ent->moveinfo.subframe + 1) / ((float)ent->moveinfo.num_subframes + 1);

		target_dist = lerp(ent->moveinfo.curve_positions[ent->moveinfo.curve_frame - 1], ent->moveinfo.curve_positions[ent->moveinfo.curve_frame], t);
		ent->moveinfo.subframe++;
	} else {
		if (ent->moveinfo.curve_frame == ent->moveinfo.curve_positions.count) {
			Move_Final(ent);
			return;
		}

		target_dist = ent->moveinfo.curve_positions[ent->moveinfo.curve_frame++];
	}

	ent->moveinfo.num_frames_done++;
	vec3_t target_pos = ent->moveinfo.curve_ref + (ent->moveinfo.dir * target_dist);
	ent->velocity = (target_pos - ent->s.origin) * (1.f / gi.frame_time_s);
	ent->nextthink = level.time + FRAME_TIME_S;
}

//
// Support routines for angular movement (changes in angle using avelocity)
//

static THINK(AngleMove_Done) (gentity_t *ent) -> void {
	ent->avelocity = {};
	ent->moveinfo.endfunc(ent);
}

static THINK(AngleMove_Final) (gentity_t *ent) -> void {
	vec3_t move;

	if (ent->moveinfo.state == STATE_UP) {
		if (ent->moveinfo.reversing)
			move = ent->moveinfo.end_angles_reversed - ent->s.angles;
		else
			move = ent->moveinfo.end_angles - ent->s.angles;
	} else
		move = ent->moveinfo.start_angles - ent->s.angles;

	if (!move) {
		AngleMove_Done(ent);
		return;
	}

	ent->avelocity = move * (1.0f / gi.frame_time_s);

	ent->think = AngleMove_Done;
	ent->nextthink = level.time + FRAME_TIME_S;
}

static THINK(AngleMove_Begin) (gentity_t *ent) -> void {
	vec3_t destdelta;
	float  len;
	float  traveltime;
	float  frames;

	// accelerate as needed
	if (ent->moveinfo.speed < ent->speed) {
		ent->moveinfo.speed += ent->accel;
		if (ent->moveinfo.speed > ent->speed)
			ent->moveinfo.speed = ent->speed;
	}

	// set destdelta to the vector needed to move
	if (ent->moveinfo.state == STATE_UP) {
		if (ent->moveinfo.reversing)
			destdelta = ent->moveinfo.end_angles_reversed - ent->s.angles;
		else
			destdelta = ent->moveinfo.end_angles - ent->s.angles;
	} else
		destdelta = ent->moveinfo.start_angles - ent->s.angles;

	// calculate length of vector
	len = destdelta.length();

	// divide by speed to get time to reach dest
	traveltime = len / ent->moveinfo.speed;

	if (traveltime < gi.frame_time_s) {
		AngleMove_Final(ent);
		return;
	}

	frames = floor(traveltime / gi.frame_time_s);

	// scale the destdelta vector by the time spent traveling to get velocity
	ent->avelocity = destdelta * (1.0f / traveltime);

	//  if we're done accelerating, act as a normal rotation
	if (ent->moveinfo.speed >= ent->speed) {
		// set nextthink to trigger a think when dest is reached
		ent->nextthink = level.time + (FRAME_TIME_S * frames);
		ent->think = AngleMove_Final;
	} else {
		ent->nextthink = level.time + FRAME_TIME_S;
		ent->think = AngleMove_Begin;
	}
}

static void AngleMove_Calc(gentity_t *ent, void(*endfunc)(gentity_t *self)) {
	ent->avelocity = {};
	ent->moveinfo.endfunc = endfunc;

	//  if we're supposed to accelerate, this will tell anglemove_begin to do so
	if (ent->accel != ent->speed)
		ent->moveinfo.speed = 0;

	if (level.current_entity == ((ent->flags & FL_TEAMSLAVE) ? ent->teammaster : ent)) {
		AngleMove_Begin(ent);
	} else {
		ent->nextthink = level.time + FRAME_TIME_S;
		ent->think = AngleMove_Begin;
	}
}

/*
==============
Think_AccelMove

The team has completed a frame of movement, so
change the speed for the next frame
==============
*/
static void plat_CalcAcceleratedMove(moveinfo_t *moveinfo) {
	float accel_dist;
	float decel_dist;

	if (moveinfo->remaining_distance < moveinfo->accel) {
		moveinfo->move_speed = moveinfo->speed;
		moveinfo->current_speed = moveinfo->remaining_distance;
		return;
	}

	accel_dist = AccelerationDistance(moveinfo->speed, moveinfo->accel);
	decel_dist = AccelerationDistance(moveinfo->speed, moveinfo->decel);

	if ((moveinfo->remaining_distance - accel_dist - decel_dist) < 0) {
		float f;

		f = (moveinfo->accel + moveinfo->decel) / (moveinfo->accel * moveinfo->decel);
		moveinfo->move_speed = moveinfo->current_speed =
			(-2 + sqrt(4 - 4 * f * (-2 * moveinfo->remaining_distance))) / (2 * f);
		decel_dist = AccelerationDistance(moveinfo->move_speed, moveinfo->decel);
	} else
		moveinfo->move_speed = moveinfo->speed;

	moveinfo->decel_distance = decel_dist;
};

static void plat_Accelerate(moveinfo_t *moveinfo) {
	// are we decelerating?
	if (moveinfo->remaining_distance <= moveinfo->decel_distance) {
		if (moveinfo->remaining_distance < moveinfo->decel_distance) {
			if (moveinfo->next_speed) {
				moveinfo->current_speed = moveinfo->next_speed;
				moveinfo->next_speed = 0;
				return;
			}
			if (moveinfo->current_speed > moveinfo->decel) {
				moveinfo->current_speed -= moveinfo->decel;

				// [Paril-KEX] fix platforms in xdm6, etc
				if (fabsf(moveinfo->current_speed) < 0.01f)
					moveinfo->current_speed = moveinfo->remaining_distance + 1;
			}
		}
		return;
	}

	// are we at full speed and need to start decelerating during this move?
	if (moveinfo->current_speed == moveinfo->move_speed)
		if ((moveinfo->remaining_distance - moveinfo->current_speed) < moveinfo->decel_distance) {
			float p1_distance;
			float p2_distance;
			float distance;

			p1_distance = moveinfo->remaining_distance - moveinfo->decel_distance;
			p2_distance = moveinfo->move_speed * (1.0f - (p1_distance / moveinfo->move_speed));
			distance = p1_distance + p2_distance;
			moveinfo->current_speed = moveinfo->move_speed;
			moveinfo->next_speed = moveinfo->move_speed - moveinfo->decel * (p2_distance / distance);
			return;
		}

	// are we accelerating?
	if (moveinfo->current_speed < moveinfo->speed) {
		float old_speed;
		float p1_distance;
		float p1_speed;
		float p2_distance;
		float distance;

		old_speed = moveinfo->current_speed;

		// figure simple acceleration up to move_speed
		moveinfo->current_speed += moveinfo->accel;
		if (moveinfo->current_speed > moveinfo->speed)
			moveinfo->current_speed = moveinfo->speed;

		// are we accelerating throughout this entire move?
		if ((moveinfo->remaining_distance - moveinfo->current_speed) >= moveinfo->decel_distance)
			return;

		// during this move we will accelerate from current_speed to move_speed
		// and cross over the decel_distance; figure the average speed for the
		// entire move
		p1_distance = moveinfo->remaining_distance - moveinfo->decel_distance;
		p1_speed = (old_speed + moveinfo->move_speed) / 2.0f;
		p2_distance = moveinfo->move_speed * (1.0f - (p1_distance / p1_speed));
		distance = p1_distance + p2_distance;
		moveinfo->current_speed =
			(p1_speed * (p1_distance / distance)) + (moveinfo->move_speed * (p2_distance / distance));
		moveinfo->next_speed = moveinfo->move_speed - moveinfo->decel * (p2_distance / distance);
		return;
	}

	// we are at constant velocity (move_speed)
	return;
}

bool Think_AccelMove_MoveInfo(moveinfo_t *moveinfo) {
	if (moveinfo->current_speed == 0)		// starting or blocked
		plat_CalcAcceleratedMove(moveinfo);

	plat_Accelerate(moveinfo);

	// will the entire move complete on next frame?
	return moveinfo->remaining_distance > moveinfo->current_speed;
}

// Paril: old acceleration code; this is here only to support old save games.
THINK(Think_AccelMove) (gentity_t *ent) -> void {
	// [Paril-KEX] calculate distance dynamically
	if (ent->moveinfo.state == STATE_UP)
		ent->moveinfo.remaining_distance = (ent->moveinfo.start_origin - ent->s.origin).length();
	else
		ent->moveinfo.remaining_distance = (ent->moveinfo.end_origin - ent->s.origin).length();

	// will the entire move complete on next frame?
	if (!Think_AccelMove_MoveInfo(&ent->moveinfo)) {
		Move_Final(ent);
		return;
	}

	if (ent->moveinfo.remaining_distance <= ent->moveinfo.current_speed) {
		Move_Final(ent);
		return;
	}

	ent->velocity = ent->moveinfo.dir * (ent->moveinfo.current_speed * 10);
	ent->nextthink = level.time + 10_hz;
	ent->think = Think_AccelMove;
}

void plat_go_down(gentity_t *ent);

MOVEINFO_ENDFUNC(plat_hit_top) (gentity_t *ent) -> void {
	if (!(ent->flags & FL_TEAMSLAVE)) {
		if (ent->moveinfo.sound_end)
			gi.sound(ent, CHAN_NO_PHS_ADD | CHAN_VOICE, ent->moveinfo.sound_end, 1, ATTN_STATIC, 0);
	}
	ent->s.sound = 0;
	ent->moveinfo.state = STATE_TOP;

	ent->think = plat_go_down;
	ent->nextthink = level.time + 3_sec;
}

MOVEINFO_ENDFUNC(plat_hit_bottom) (gentity_t *ent) -> void {
	if (!(ent->flags & FL_TEAMSLAVE)) {
		if (ent->moveinfo.sound_end)
			gi.sound(ent, CHAN_NO_PHS_ADD | CHAN_VOICE, ent->moveinfo.sound_end, 1, ATTN_STATIC, 0);
	}
	ent->s.sound = 0;
	ent->moveinfo.state = STATE_BOTTOM;

	plat2_kill_danger_area(ent);
}

THINK(plat_go_down) (gentity_t *ent) -> void {
	if (!(ent->flags & FL_TEAMSLAVE)) {
		if (ent->moveinfo.sound_start)
			gi.sound(ent, CHAN_NO_PHS_ADD | CHAN_VOICE, ent->moveinfo.sound_start, 1, ATTN_STATIC, 0);
	}

	ent->s.sound = ent->moveinfo.sound_middle;

	ent->moveinfo.state = STATE_DOWN;
	Move_Calc(ent, ent->moveinfo.end_origin, plat_hit_bottom);
	if (g_mover_debug->integer)
		gi.Com_PrintFmt("Go down {}\n", *ent);
}

static void plat_go_up(gentity_t *ent) {
	if (!(ent->flags & FL_TEAMSLAVE)) {
		if (ent->moveinfo.sound_start)
			gi.sound(ent, CHAN_NO_PHS_ADD | CHAN_VOICE, ent->moveinfo.sound_start, 1, ATTN_STATIC, 0);
	}

	ent->s.sound = ent->moveinfo.sound_middle;

	ent->moveinfo.state = STATE_UP;
	Move_Calc(ent, ent->moveinfo.start_origin, plat_hit_top);

	plat2_spawn_danger_area(ent);
	if (g_mover_debug->integer)
		gi.Com_PrintFmt("Go up {}\n", *ent);
}

MOVEINFO_BLOCKED(plat_blocked) (gentity_t *self, gentity_t *other) -> void {
	if (!(other->svflags & SVF_MONSTER) && (!other->client)) {
		// give it a chance to go away on it's own terms (like gibs)
		T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, DAMAGE_NONE, MOD_CRUSH);
		// if it's still there, nuke it
		if (other && other->inuse && other->solid)
			BecomeExplosion1(other);
		return;
	}

	//  gib dead things
	if (other->health < 1)
		T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, 100, 1, DAMAGE_NONE, MOD_CRUSH);

	T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MOD_CRUSH);

	// [Paril-KEX] killed the thing, so don't switch directions
	if (!other->inuse || !other->solid)
		return;

	if (g_mover_debug->integer)
		gi.Com_PrintFmt("Blocked {} - speed:{} accel:{} decel:{}\n", *self, self->speed, self->accel, self->decel);

	if (self->moveinfo.state == STATE_UP)
		plat_go_down(self);
	else if (self->moveinfo.state == STATE_DOWN)
		plat_go_up(self);
}

constexpr spawnflags_t SPAWNFLAG_PLAT_LOW_TRIGGER = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAG_PLAT_NO_MONSTER = 2_spawnflag;

static USE(Use_Plat) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	// if a monster is using us, then allow the activity when stopped.
	if ((other->svflags & SVF_MONSTER) && !(ent->spawnflags & SPAWNFLAG_PLAT_NO_MONSTER)) {
		if (ent->moveinfo.state == STATE_TOP)
			plat_go_down(ent);
		else if (ent->moveinfo.state == STATE_BOTTOM)
			plat_go_up(ent);

		return;
	}

	if (g_mover_debug->integer)
		gi.Com_PrintFmt("Use {}\n", *ent);

	if (ent->think)
		return; // already down
	plat_go_down(ent);
}

static TOUCH(Touch_Plat_Center) (gentity_t *ent, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	if (!other->client)
		return;

	if (other->health <= 0)
		return;

	ent = ent->enemy; // now point at the plat, not the trigger
	if (ent->moveinfo.state == STATE_BOTTOM)
		plat_go_up(ent);
	else if (ent->moveinfo.state == STATE_TOP)
		ent->nextthink = level.time + 1_sec; // the player is still on the plat, so delay going down

	if (g_mover_debug->integer)
		gi.Com_PrintFmt("Touch center {} - speed:{} accel:{} decel:{}\n", *ent, ent->speed, ent->accel, ent->decel);
}

// plat2's change the trigger field
gentity_t *plat_spawn_inside_trigger(gentity_t *ent) {
	gentity_t *trigger;
	vec3_t	 tmin, tmax;

	//
	// middle trigger
	//
	trigger = G_Spawn();
	trigger->touch = Touch_Plat_Center;
	trigger->movetype = MOVETYPE_NONE;
	trigger->solid = SOLID_TRIGGER;
	trigger->enemy = ent;

	tmin[0] = ent->mins[0] + 25;
	tmin[1] = ent->mins[1] + 25;
	tmin[2] = ent->mins[2];

	tmax[0] = ent->maxs[0] - 25;
	tmax[1] = ent->maxs[1] - 25;
	tmax[2] = ent->maxs[2] + 8;

	tmin[2] = tmax[2] - (ent->pos1[2] - ent->pos2[2] + st.lip);

	if (ent->spawnflags.has(SPAWNFLAG_PLAT_LOW_TRIGGER))
		tmax[2] = tmin[2] + 8;

	if (tmax[0] - tmin[0] <= 0) {
		tmin[0] = (ent->mins[0] + ent->maxs[0]) * 0.5f;
		tmax[0] = tmin[0] + 1;
	}
	if (tmax[1] - tmin[1] <= 0) {
		tmin[1] = (ent->mins[1] + ent->maxs[1]) * 0.5f;
		tmax[1] = tmin[1] + 1;
	}

	trigger->mins = tmin;
	trigger->maxs = tmax;

	gi.linkentity(trigger);

	return trigger; // PGM 11/17/97
}

/*QUAKED func_plat (0 .5 .8) ? PLAT_LOW_TRIGGER x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
speed	default 150

Plats are always drawn in the extended position, so they will light correctly.

If the plat is the target of another trigger or button, it will start out disabled in the extended position until it is triggered, when it will lower and become a normal plat.

"speed"	overrides default 200.
"accel" overrides default 500
"lip"	overrides default 8 pixel lip

If the "height" key is set, that will determine the amount the plat moves, instead of being implicitly determined by the model's height.

Set "sounds" to one of the following:
1) base fast
2) chain slow
*/
void SP_func_plat(gentity_t *ent) {
	ent->s.angles = {};
	ent->solid = SOLID_BSP;
	ent->movetype = MOVETYPE_PUSH;

	gi.setmodel(ent, ent->model);

	ent->moveinfo.blocked = plat_blocked;

	if (!ent->speed)
		ent->speed = 20;
	else
		ent->speed *= 0.1f;

	if (!ent->accel)
		ent->accel = 5;
	else
		ent->accel *= 0.1f;

	if (!ent->decel)
		ent->decel = 5;
	else
		ent->decel *= 0.1f;
#if 0
	if (g_mover_speed_scale->value != 1.0f) {
		ent->speed = floor(ent->speed * g_mover_speed_scale->value);
		ent->accel = floor(ent->accel * g_mover_speed_scale->value);
		ent->decel = floor(ent->decel * g_mover_speed_scale->value);
	}
#endif
	if (g_mover_debug->integer)
		gi.Com_PrintFmt("Spawning {} - speed:{} accel:{} decel:{}\n", *ent, ent->speed, ent->accel, ent->decel);

	if (!ent->dmg)
		ent->dmg = 2;

	if (!st.lip)
		st.lip = 8;

	// pos1 is the top position, pos2 is the bottom
	ent->pos1 = ent->s.origin;
	ent->pos2 = ent->s.origin;
	if (st.height)
		ent->pos2[2] -= st.height;
	else
		ent->pos2[2] -= (ent->maxs[2] - ent->mins[2]) - st.lip;

	ent->use = Use_Plat;

	plat_spawn_inside_trigger(ent); // the "start moving" trigger

	if (ent->targetname) {
		ent->moveinfo.state = STATE_UP;
	} else {
		ent->s.origin = ent->pos2;
		gi.linkentity(ent);
		ent->moveinfo.state = STATE_BOTTOM;
	}

	ent->moveinfo.speed = ent->speed;
	ent->moveinfo.accel = ent->accel;
	ent->moveinfo.decel = ent->decel;
	ent->moveinfo.wait = ent->wait;
	ent->moveinfo.start_origin = ent->pos1;
	ent->moveinfo.start_angles = ent->s.angles;
	ent->moveinfo.end_origin = ent->pos2;
	ent->moveinfo.end_angles = ent->s.angles;

	G_SetMoveinfoSounds(ent, "plats/pt1_strt.wav", "plats/pt1_mid.wav", "plats/pt1_end.wav");
}

/*QUAKED func_plat2 (0 .5 .8) ? PLAT_LOW_TRIGGER PLAT2_TOGGLE PLAT2_TOP PLAT2_START_ACTIVE x BOX_LIFT x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
speed	default 150

PLAT_LOW_TRIGGER - creates a short trigger field at the bottom
PLAT2_TOGGLE - plat will not return to default position.
PLAT2_TOP - plat's default position will the the top.
PLAT2_START_ACTIVE - plat will trigger it's targets each time it hits top
BOX_LIFT - this indicates that the lift is a box, rather than just a platform

Plats are always drawn in the extended position, so they will light correctly.

If the plat is the target of another trigger or button, it will start out disabled in the extended position until it is trigger, when it will lower and become a normal plat.

"speed"	overrides default 200.
"accel" overrides default 500
"lip"	no default

If the "height" key is set, that will determine the amount the plat moves, instead of being implicitly determoveinfoned by the model's height.

*/

constexpr spawnflags_t SPAWNFLAGS_PLAT2_TOGGLE = 2_spawnflag;
constexpr spawnflags_t SPAWNFLAGS_PLAT2_TOP = 4_spawnflag;
constexpr spawnflags_t SPAWNFLAGS_PLAT2_START_ACTIVE = 8_spawnflag;
constexpr spawnflags_t SPAWNFLAGS_PLAT2_BOX_LIFT = 32_spawnflag;

void plat2_go_down(gentity_t *ent);
void plat2_go_up(gentity_t *ent);

void plat2_spawn_danger_area(gentity_t *ent) {
	vec3_t mins, maxs;

	mins = ent->mins;
	maxs = ent->maxs;
	maxs[2] = ent->mins[2] + 64;

	SpawnBadArea(mins, maxs, 0_ms, ent);
}

void plat2_kill_danger_area(gentity_t *ent) {
	gentity_t *t;

	t = nullptr;
	while ((t = G_FindByString<&gentity_t::classname>(t, "bad_area"))) {
		if (t->owner == ent)
			G_FreeEntity(t);
	}
}

MOVEINFO_ENDFUNC(plat2_hit_top) (gentity_t *ent) -> void {
	if (!(ent->flags & FL_TEAMSLAVE)) {
		if (ent->moveinfo.sound_end)
			gi.sound(ent, CHAN_NO_PHS_ADD | CHAN_VOICE, ent->moveinfo.sound_end, 1, ATTN_STATIC, 0);
	}
	ent->s.sound = 0;
	ent->moveinfo.state = STATE_TOP;

	if (ent->plat2flags & PLAT2_CALLED) {
		ent->plat2flags = PLAT2_WAITING;
		if (!ent->spawnflags.has(SPAWNFLAGS_PLAT2_TOGGLE)) {
			ent->think = plat2_go_down;
			ent->nextthink = level.time + 5_sec;
		}
		if (deathmatch->integer)
			ent->last_move_time = level.time - 1_sec;
		else
			ent->last_move_time = level.time - 2_sec;
	} else if (!(ent->spawnflags & SPAWNFLAGS_PLAT2_TOP) && !ent->spawnflags.has(SPAWNFLAGS_PLAT2_TOGGLE)) {
		ent->plat2flags = PLAT2_NONE;
		ent->think = plat2_go_down;
		ent->nextthink = level.time + 2_sec;
		ent->last_move_time = level.time;
	} else {
		ent->plat2flags = PLAT2_NONE;
		ent->last_move_time = level.time;
	}

	G_UseTargets(ent, ent);
}

MOVEINFO_ENDFUNC(plat2_hit_bottom) (gentity_t *ent) -> void {
	if (!(ent->flags & FL_TEAMSLAVE)) {
		if (ent->moveinfo.sound_end)
			gi.sound(ent, CHAN_NO_PHS_ADD | CHAN_VOICE, ent->moveinfo.sound_end, 1, ATTN_STATIC, 0);
	}
	ent->s.sound = 0;
	ent->moveinfo.state = STATE_BOTTOM;

	if (ent->plat2flags & PLAT2_CALLED) {
		ent->plat2flags = PLAT2_WAITING;
		if (!(ent->spawnflags & SPAWNFLAGS_PLAT2_TOGGLE)) {
			ent->think = plat2_go_up;
			ent->nextthink = level.time + 5_sec;
		}
		if (deathmatch->integer)
			ent->last_move_time = level.time - 1_sec;
		else
			ent->last_move_time = level.time - 2_sec;
	} else if (ent->spawnflags.has(SPAWNFLAGS_PLAT2_TOP) && !ent->spawnflags.has(SPAWNFLAGS_PLAT2_TOGGLE)) {
		ent->plat2flags = PLAT2_NONE;
		ent->think = plat2_go_up;
		ent->nextthink = level.time + 2_sec;
		ent->last_move_time = level.time;
	} else {
		ent->plat2flags = PLAT2_NONE;
		ent->last_move_time = level.time;
	}

	plat2_kill_danger_area(ent);
	G_UseTargets(ent, ent);
}

THINK(plat2_go_down) (gentity_t *ent) -> void {
	if (!(ent->flags & FL_TEAMSLAVE)) {
		if (ent->moveinfo.sound_start)
			gi.sound(ent, CHAN_NO_PHS_ADD | CHAN_VOICE, ent->moveinfo.sound_start, 1, ATTN_STATIC, 0);
	}

	ent->s.sound = ent->moveinfo.sound_middle;

	ent->moveinfo.state = STATE_DOWN;
	ent->plat2flags |= PLAT2_MOVING;

	Move_Calc(ent, ent->moveinfo.end_origin, plat2_hit_bottom);
}

THINK(plat2_go_up) (gentity_t *ent) -> void {
	if (!(ent->flags & FL_TEAMSLAVE)) {
		if (ent->moveinfo.sound_start)
			gi.sound(ent, CHAN_NO_PHS_ADD | CHAN_VOICE, ent->moveinfo.sound_start, 1, ATTN_STATIC, 0);
	}

	ent->s.sound = ent->moveinfo.sound_middle;

	ent->moveinfo.state = STATE_UP;
	ent->plat2flags |= PLAT2_MOVING;

	plat2_spawn_danger_area(ent);

	Move_Calc(ent, ent->moveinfo.start_origin, plat2_hit_top);
}

static void plat2_operate(gentity_t *ent, gentity_t *other) {
	int		 otherState;
	gtime_t	 pauseTime;
	float	 platCenter;
	gentity_t *trigger;

	trigger = ent;
	ent = ent->enemy; // now point at the plat, not the trigger

	if (ent->plat2flags & PLAT2_MOVING)
		return;

	if ((ent->last_move_time + 2_sec) > level.time)
		return;

	platCenter = (trigger->absmin[2] + trigger->absmax[2]) / 2;

	if (ent->moveinfo.state == STATE_TOP) {
		otherState = STATE_TOP;
		if (ent->spawnflags.has(SPAWNFLAGS_PLAT2_BOX_LIFT)) {
			if (platCenter > other->s.origin[2])
				otherState = STATE_BOTTOM;
		} else {
			if (trigger->absmax[2] > other->s.origin[2])
				otherState = STATE_BOTTOM;
		}
	} else {
		otherState = STATE_BOTTOM;
		if (other->s.origin[2] > platCenter)
			otherState = STATE_TOP;
	}

	ent->plat2flags = PLAT2_MOVING;

	if (deathmatch->integer)
		pauseTime = 300_ms;
	else
		pauseTime = 500_ms;

	if (ent->moveinfo.state != otherState) {
		ent->plat2flags |= PLAT2_CALLED;
		pauseTime = 100_ms;
	}

	ent->last_move_time = level.time;

	if (ent->moveinfo.state == STATE_BOTTOM) {
		ent->think = plat2_go_up;
		ent->nextthink = level.time + pauseTime;
	} else {
		ent->think = plat2_go_down;
		ent->nextthink = level.time + pauseTime;
	}
}

static TOUCH(Touch_Plat_Center2) (gentity_t *ent, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	// this requires monsters to actively trigger plats, not just step on them.

	// FIXME - commented out for E3
	// if (!other->client)
	//	return;

	if (other->health <= 0)
		return;

	// PMM - don't let non-monsters activate plat2s
	if ((!(other->svflags & SVF_MONSTER)) && (!other->client))
		return;

	plat2_operate(ent, other);
}

MOVEINFO_BLOCKED(plat2_blocked) (gentity_t *self, gentity_t *other) -> void {
	if (!(other->svflags & SVF_MONSTER) && (!other->client)) {
		// give it a chance to go away on it's own terms (like gibs)
		T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, DAMAGE_NONE, MOD_CRUSH);
		// if it's still there, nuke it
		if (other && other->inuse && other->solid)
			BecomeExplosion1(other);
		return;
	}

	// gib dead things
	if (other->health < 1) {
		T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, 100, 1, DAMAGE_NONE, MOD_CRUSH);
	}

	T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MOD_CRUSH);

	// [Paril-KEX] killed, so don't change direction
	if (!other->inuse || !other->solid)
		return;

	if (self->moveinfo.state == STATE_UP)
		plat2_go_down(self);
	else if (self->moveinfo.state == STATE_DOWN)
		plat2_go_up(self);
}

static USE(Use_Plat2) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	gentity_t *trigger;

	if (ent->moveinfo.state > STATE_BOTTOM)
		return;
	// [Paril-KEX] disabled this; causes confusing situations
	//if ((ent->last_move_time + 2_sec) > level.time)
	//	return;

	uint32_t i;
	const uint32_t entity_limit = min(globals.num_entities, game.maxentities);
	for (i = 1, trigger = g_entities + 1; i < entity_limit; i++, trigger++) {
		if (!trigger->inuse)
			continue;
		if (trigger->touch == Touch_Plat_Center2) {
			if (trigger->enemy == ent) {
				//				Touch_Plat_Center2 (trigger, activator, nullptr, nullptr);
				plat2_operate(trigger, activator);
				return;
			}
		}
	}
}

static USE(plat2_activate) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	gentity_t *trigger;

	//	if(ent->targetname)
	//		ent->targetname[0] = 0;

	ent->use = Use_Plat2;

	trigger = plat_spawn_inside_trigger(ent); // the "start moving" trigger

	trigger->maxs[0] += 10;
	trigger->maxs[1] += 10;
	trigger->mins[0] -= 10;
	trigger->mins[1] -= 10;

	gi.linkentity(trigger);

	trigger->touch = Touch_Plat_Center2; // Override trigger touch function

	plat2_go_down(ent);
}

void SP_func_plat2(gentity_t *ent) {
	gentity_t *trigger;

	ent->s.angles = {};
	ent->solid = SOLID_BSP;
	ent->movetype = MOVETYPE_PUSH;

	gi.setmodel(ent, ent->model);

	ent->moveinfo.blocked = plat2_blocked;

	if (!ent->speed)
		ent->speed = 20;
	else
		ent->speed *= 0.1f;

	if (!ent->accel)
		ent->accel = 5;
	else
		ent->accel *= 0.1f;

	if (!ent->decel)
		ent->decel = 5;
	else
		ent->decel *= 0.1f;

	if (deathmatch->integer) {
		ent->speed *= 2;
		ent->accel *= 2;
		ent->decel *= 2;
	}

	if (g_mover_speed_scale->value != 1.0f) {
		ent->speed *= g_mover_speed_scale->value;
		ent->accel *= g_mover_speed_scale->value;
		ent->decel *= g_mover_speed_scale->value;
	}

	// PMM Added to kill things it's being blocked by
	if (!ent->dmg)
		ent->dmg = 2;

	//	if (!st.lip)
	//		st.lip = 8;

	// pos1 is the top position, pos2 is the bottom
	ent->pos1 = ent->s.origin;
	ent->pos2 = ent->s.origin;

	if (st.height)
		ent->pos2[2] -= (st.height - st.lip);
	else
		ent->pos2[2] -= (ent->maxs[2] - ent->mins[2]) - st.lip;

	ent->moveinfo.state = STATE_TOP;

	if (ent->targetname && !(ent->spawnflags & SPAWNFLAGS_PLAT2_START_ACTIVE)) {
		ent->use = plat2_activate;
	} else {
		ent->use = Use_Plat2;

		trigger = plat_spawn_inside_trigger(ent); // the "start moving" trigger

		// PGM - debugging??
		trigger->maxs[0] += 10;
		trigger->maxs[1] += 10;
		trigger->mins[0] -= 10;
		trigger->mins[1] -= 10;

		gi.linkentity(trigger);

		trigger->touch = Touch_Plat_Center2; // Override trigger touch function

		if (!(ent->spawnflags & SPAWNFLAGS_PLAT2_TOP)) {
			ent->s.origin = ent->pos2;
			ent->moveinfo.state = STATE_BOTTOM;
		}
	}

	gi.linkentity(ent);

	ent->moveinfo.speed = ent->speed;
	ent->moveinfo.accel = ent->accel;
	ent->moveinfo.decel = ent->decel;
	ent->moveinfo.wait = ent->wait;
	ent->moveinfo.start_origin = ent->pos1;
	ent->moveinfo.start_angles = ent->s.angles;
	ent->moveinfo.end_origin = ent->pos2;
	ent->moveinfo.end_angles = ent->s.angles;

	G_SetMoveinfoSounds(ent, "plats/pt1_strt.wav", "plats/pt1_mid.wav", "plats/pt1_end.wav");
}


//====================================================================

/*QUAKED func_rotating (0 .5 .8) ? START_ON REVERSE X_AXIS Y_AXIS TOUCH_PAIN STOP ANIMATED ANIMATED_FAST NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP x COOP_ONLY x ACCEL
You need to have an origin brush as part of this entity.
The center of that brush will be the point around which it is rotated. It will rotate around the Z axis by default.
You can check either the X_AXIS or Y_AXIS box to change that.

func_rotating will use it's targets when it stops and starts.

"speed" determines how fast it moves; default value is 100.
"dmg"	damage to inflict when blocked (2 default)
"accel" if specified, is how much the rotation speed will increase per .1sec.

REVERSE will cause the it to rotate in the opposite direction.
STOP mean it will stop moving instead of pushing entities
ACCEL means it will accelerate to it's final speed and decelerate when shutting down.
*/

// Paril: Rogue added a spawnflag in func_rotating that
// is a reserved editor flag.
constexpr spawnflags_t SPAWNFLAG_ROTATING_START_ON = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAG_ROTATING_REVERSE = 2_spawnflag;
constexpr spawnflags_t SPAWNFLAG_ROTATING_X_AXIS = 4_spawnflag;
constexpr spawnflags_t SPAWNFLAG_ROTATING_Y_AXIS = 8_spawnflag;
constexpr spawnflags_t SPAWNFLAG_ROTATING_TOUCH_PAIN = 16_spawnflag;
constexpr spawnflags_t SPAWNFLAG_ROTATING_STOP = 32_spawnflag;
constexpr spawnflags_t SPAWNFLAG_ROTATING_ANIMATED = 64_spawnflag;
constexpr spawnflags_t SPAWNFLAG_ROTATING_ANIMATED_FAST = 128_spawnflag;
constexpr spawnflags_t SPAWNFLAG_ROTATING_ACCEL = 0x00010000_spawnflag;

static THINK(rotating_accel) (gentity_t *self) -> void {
	float current_speed = self->avelocity.length();
	if (current_speed >= (self->speed - self->accel)) { // done
		self->avelocity = self->movedir * self->speed;
		G_UseTargets(self, self);
	} else {
		current_speed += self->accel;
		self->avelocity = self->movedir * current_speed;
		self->think = rotating_accel;
		self->nextthink = level.time + FRAME_TIME_S;
	}
}

static THINK(rotating_decel) (gentity_t *self) -> void {
	float current_speed = self->avelocity.length();
	if (current_speed <= self->decel) { // done
		self->avelocity = {};
		G_UseTargets(self, self);
		self->touch = nullptr;
	} else {
		current_speed -= self->decel;
		self->avelocity = self->movedir * current_speed;
		self->think = rotating_decel;
		self->nextthink = level.time + FRAME_TIME_S;
	}
}

MOVEINFO_BLOCKED(rotating_blocked) (gentity_t *self, gentity_t *other) -> void {
	if (!self->dmg)
		return;
	if (level.time < self->touch_debounce_time)
		return;

	if (!(other->svflags & SVF_MONSTER) && (!other->client)) {
		// give it a chance to go away on it's own terms (like gibs)
		T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, DAMAGE_NONE, MOD_CRUSH);
		// if it's still there, nuke it
		if (other && other->inuse && other->solid)
			BecomeExplosion1(other);
		return;
	}

	self->touch_debounce_time = level.time + 10_hz;
	T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MOD_CRUSH);
}

static TOUCH(rotating_touch) (gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	if (self->avelocity[0] || self->avelocity[1] || self->avelocity[2])
		T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MOD_CRUSH);
}

static USE(rotating_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	if (self->avelocity) {
		self->s.sound = 0;

		if (self->spawnflags.has(SPAWNFLAG_ROTATING_ACCEL)) // Decelerate
			rotating_decel(self);
		else {
			self->avelocity = {};
			G_UseTargets(self, self);
			self->touch = nullptr;
		}
	} else {
		self->s.sound = self->moveinfo.sound_middle;

		if (self->spawnflags.has(SPAWNFLAG_ROTATING_ACCEL)) // accelerate
			rotating_accel(self);
		else {
			self->avelocity = self->movedir * self->speed;
			G_UseTargets(self, self);
		}
		if (self->spawnflags.has(SPAWNFLAG_ROTATING_TOUCH_PAIN))
			self->touch = rotating_touch;
	}
}

void SP_func_rotating(gentity_t *ent) {
	ent->solid = SOLID_BSP;
	if (ent->spawnflags.has(SPAWNFLAG_ROTATING_STOP))
		ent->movetype = MOVETYPE_STOP;
	else
		ent->movetype = MOVETYPE_PUSH;

	if (st.noise) {
		ent->moveinfo.sound_middle = gi.soundindex(st.noise);

		// [Paril-KEX] for rhangar1 doors
		if (!st.was_key_specified("attenuation"))
			ent->attenuation = ATTN_STATIC;
		else {
			if (ent->attenuation == -1) {
				ent->s.loop_attenuation = ATTN_LOOP_NONE;
				ent->attenuation = ATTN_NONE;
			} else {
				ent->s.loop_attenuation = ent->attenuation;
			}
		}
	}

	// set the axis of rotation
	ent->movedir = {};
	if (ent->spawnflags.has(SPAWNFLAG_ROTATING_X_AXIS))
		ent->movedir[2] = 1.0;
	else if (ent->spawnflags.has(SPAWNFLAG_ROTATING_Y_AXIS))
		ent->movedir[0] = 1.0;
	else // Z_AXIS
		ent->movedir[1] = 1.0;

	// check for reverse rotation
	if (ent->spawnflags.has(SPAWNFLAG_ROTATING_REVERSE))
		ent->movedir = -ent->movedir;

	if (!ent->speed)
		ent->speed = 100;

	if (g_mover_speed_scale->value != 1.0f) {
		ent->speed *= g_mover_speed_scale->value;
		ent->accel *= g_mover_speed_scale->value;
		ent->decel *= g_mover_speed_scale->value;
	}

	if (!st.was_key_specified("dmg"))
		ent->dmg = 2;

	ent->use = rotating_use;
	if (ent->dmg)
		ent->moveinfo.blocked = rotating_blocked;

	if (ent->spawnflags.has(SPAWNFLAG_ROTATING_START_ON))
		ent->use(ent, nullptr, nullptr);

	if (ent->spawnflags.has(SPAWNFLAG_ROTATING_ANIMATED))
		ent->s.effects |= EF_ANIM_ALL;
	if (ent->spawnflags.has(SPAWNFLAG_ROTATING_ANIMATED_FAST))
		ent->s.effects |= EF_ANIM_ALLFAST;

	if (ent->spawnflags.has(SPAWNFLAG_ROTATING_ACCEL)) // Accelerate / Decelerate
	{
		if (!ent->accel)
			ent->accel = 1;
		else if (ent->accel > ent->speed)
			ent->accel = ent->speed;

		if (!ent->decel)
			ent->decel = 1;
		else if (ent->decel > ent->speed)
			ent->decel = ent->speed;
	}

	gi.setmodel(ent, ent->model);
	gi.linkentity(ent);
}

/*QUAKED func_spinning (0 .5 .8) ? x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Randomly spinning object, not controllable.

"speed" set speed of rotation (default 100)
"dmg" set damage inflicted when blocked (default 2)
*/
static THINK(func_spinning_think) (gentity_t *ent) -> void {
	if (ent->timestamp <= level.time) {
		ent->timestamp = level.time + random_time(1_sec, 6_sec);
		ent->movedir = { ent->decel + frandom(ent->speed - ent->decel), ent->decel + frandom(ent->speed - ent->decel), ent->decel + frandom(ent->speed - ent->decel) };

		for (size_t i = 0; i < 3; i++) {
			if (brandom())
				ent->movedir[i] = -ent->movedir[i];
		}
	}

	for (size_t i = 0; i < 3; i++) {
		if (ent->avelocity[i] == ent->movedir[i])
			continue;

		if (ent->avelocity[i] < ent->movedir[i])
			ent->avelocity[i] = min(ent->movedir[i], ent->avelocity[i] + ent->accel);
		else
			ent->avelocity[i] = max(ent->movedir[i], ent->avelocity[i] - ent->accel);
	}

	ent->nextthink = level.time + FRAME_TIME_MS;
}

// [Paril-KEX]
void SP_func_spinning(gentity_t *ent) {
	ent->solid = SOLID_BSP;

	if (!ent->speed)
		ent->speed = 100;
	if (!ent->dmg)
		ent->dmg = 2;

	ent->movetype = MOVETYPE_PUSH;

	ent->timestamp = 0_ms;
	ent->nextthink = level.time + FRAME_TIME_MS;
	ent->think = func_spinning_think;

	gi.setmodel(ent, ent->model);
	gi.linkentity(ent);
}

/*
======================================================================

BUTTONS

======================================================================
*/

/*QUAKED func_button (0 .5 .8) ? x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
When a button is touched, it moves some distance in the direction of it's angle, triggers all of it's targets, waits some time, then returns to it's original position where it can be triggered again.

"angle"		determines the opening direction
"target"	all entities with a matching targetname will be used
"speed"		override the default 40 speed
"wait"		override the default 1 second wait (-1 = never return)
"lip"		override the default 4 pixel lip remaining at end of move
"health"	if set, the button must be killed instead of touched
"sounds"
1) silent
2) steam metal
3) wooden clunk
4) metallic click
5) in-out
*/

MOVEINFO_ENDFUNC(button_done) (gentity_t *self) -> void {
	self->moveinfo.state = STATE_BOTTOM;
	if (!self->bmodel_anim.enabled) {
		if (level.is_n64)
			self->s.frame = 0;
		else
			self->s.effects &= ~EF_ANIM23;
		self->s.effects |= EF_ANIM01;
	} else
		self->bmodel_anim.alternate = false;
}

static THINK(button_return) (gentity_t *self) -> void {
	self->moveinfo.state = STATE_DOWN;

	Move_Calc(self, self->moveinfo.start_origin, button_done);

	if (self->health)
		self->takedamage = true;
}

MOVEINFO_ENDFUNC(button_wait) (gentity_t *self) -> void {
	self->moveinfo.state = STATE_TOP;

	if (!self->bmodel_anim.enabled) {
		self->s.effects &= ~EF_ANIM01;
		if (level.is_n64)
			self->s.frame = 2;
		else
			self->s.effects |= EF_ANIM23;
	} else
		self->bmodel_anim.alternate = true;

	G_UseTargets(self, self->activator);

	if (self->moveinfo.wait >= 0) {
		self->nextthink = level.time + gtime_t::from_sec(self->moveinfo.wait);
		self->think = button_return;
	}
}

static void button_fire(gentity_t *self) {
	if (self->moveinfo.state == STATE_UP || self->moveinfo.state == STATE_TOP)
		return;

	self->moveinfo.state = STATE_UP;
	if (self->moveinfo.sound_start && !(self->flags & FL_TEAMSLAVE))
		gi.sound(self, CHAN_NO_PHS_ADD | CHAN_VOICE, self->moveinfo.sound_start, 1, ATTN_STATIC, 0);
	Move_Calc(self, self->moveinfo.end_origin, button_wait);
}

static USE(button_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	self->activator = activator;
	button_fire(self);
}

static TOUCH(button_touch) (gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	if (!other->client)
		return;

	if (other->health <= 0)
		return;

	self->activator = other;
	button_fire(self);
}

static DIE(button_killed) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod) -> void {
	self->activator = attacker;
	self->health = self->max_health;
	self->takedamage = false;
	button_fire(self);
}

void SP_func_button(gentity_t *ent) {
	vec3_t abs_movedir;
	float  dist;

	G_SetMovedir(ent->s.angles, ent->movedir);
	ent->movetype = MOVETYPE_STOP;
	ent->solid = SOLID_BSP;
	gi.setmodel(ent, ent->model);

	if (ent->sounds != 1)
		G_SetMoveinfoSounds(ent, "switches/butn2.wav", nullptr, nullptr);
	else
		G_SetMoveinfoSounds(ent, nullptr, nullptr, nullptr);

	if (!ent->speed)
		ent->speed = 40;
	if (!ent->accel)
		ent->accel = ent->speed;
	if (!ent->decel)
		ent->decel = ent->speed;

	if (g_mover_speed_scale->value != 1.0f) {
		ent->speed *= g_mover_speed_scale->value;
		ent->accel *= g_mover_speed_scale->value;
		ent->decel *= g_mover_speed_scale->value;
	}

	if (!ent->wait)
		ent->wait = 3;
	if (!st.lip)
		st.lip = 4;

	ent->pos1 = ent->s.origin;
	abs_movedir[0] = fabsf(ent->movedir[0]);
	abs_movedir[1] = fabsf(ent->movedir[1]);
	abs_movedir[2] = fabsf(ent->movedir[2]);
	dist = abs_movedir[0] * ent->size[0] + abs_movedir[1] * ent->size[1] + abs_movedir[2] * ent->size[2] - st.lip;
	ent->pos2 = ent->pos1 + (ent->movedir * dist);

	ent->use = button_use;

	if (!ent->bmodel_anim.enabled)
		ent->s.effects |= EF_ANIM01;

	if (ent->health) {
		ent->max_health = ent->health;
		ent->die = button_killed;
		ent->takedamage = true;
	} else if (!ent->targetname)
		ent->touch = button_touch;

	ent->moveinfo.state = STATE_BOTTOM;

	ent->moveinfo.speed = ent->speed;
	ent->moveinfo.accel = ent->accel;
	ent->moveinfo.decel = ent->decel;
	ent->moveinfo.wait = ent->wait;
	ent->moveinfo.start_origin = ent->pos1;
	ent->moveinfo.start_angles = ent->s.angles;
	ent->moveinfo.end_origin = ent->pos2;
	ent->moveinfo.end_angles = ent->s.angles;

	gi.linkentity(ent);
}

/*
======================================================================

DOORS

  spawn a trigger surrounding the entire team unless it is
  already targeted by another

======================================================================
*/

/*QUAKED func_door (0 .5 .8) ? START_OPEN x CRUSHER NOMONSTER ANIMATED TOGGLE ANIMATED_FAST x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
START_OPEN	the door to moves to its destination when spawned, and operate in reverse.  It is used to temporarily or permanently close off an area when triggered (not useful for touch or takedamage doors).
NOMONSTER	monsters will not trigger this door
TOGGLE		wait in both the start and end states for a trigger event.

"message"	is printed when the door is touched if it is a trigger door and it hasn't been fired yet
"angle"		determines the opening direction
"targetname" if set, no touch field will be spawned and a remote button or trigger field activates the door.
"health"	if set, door must be shot open
"speed"		movement speed (100 default)
"wait"		wait before returning (3 default, -1 = never return)
"lip"		lip remaining at end of move (8 default)
"dmg"		damage to inflict when blocked (2 default)
"sounds"
1)	silent
2)	light
3)	medium
4)	heavy
*/

void door_use_areaportals(gentity_t *self, bool open) {
	gentity_t *t = nullptr;

	if (!self->target)
		return;

	while ((t = G_FindByString<&gentity_t::targetname>(t, self->target))) {
		if (Q_strcasecmp(t->classname, "func_areaportal") == 0) {
			gi.SetAreaPortalState(t->style, open);
		}
	}
}

void door_go_down(gentity_t *self);

static void door_play_sound(gentity_t *self, int32_t sound) {
	if (!self->teammaster) {
		gi.sound(self, CHAN_NO_PHS_ADD | CHAN_VOICE, sound, 1, self->attenuation, 0);
		return;
	}

	vec3_t p = {};
	int32_t c = 0;

	for (gentity_t *t = self->teammaster; t; t = t->teamchain) {
		p += (t->absmin + t->absmax) * 0.5f;
		c++;
	}

	if (c == 1) {
		gi.sound(self, CHAN_NO_PHS_ADD | CHAN_VOICE, sound, 1, self->attenuation, 0);
		return;
	}

	p /= c;

	if (gi.pointcontents(p) & CONTENTS_SOLID) {
		gi.sound(self, CHAN_NO_PHS_ADD | CHAN_VOICE, sound, 1, self->attenuation, 0);
		return;
	}

	gi.positioned_sound(p, self, CHAN_NO_PHS_ADD | CHAN_VOICE, sound, 1, self->attenuation, 0);
}

MOVEINFO_ENDFUNC(door_hit_top) (gentity_t *self) -> void {
	if (!(self->flags & FL_TEAMSLAVE)) {
		if (self->moveinfo.sound_end)
			door_play_sound(self, self->moveinfo.sound_end);
	}
	self->s.sound = 0;
	self->moveinfo.state = STATE_TOP;
	if (self->spawnflags.has(SPAWNFLAG_DOOR_TOGGLE))
		return;
	if (self->moveinfo.wait >= 0) {
		self->think = door_go_down;
		self->nextthink = level.time + gtime_t::from_sec(self->moveinfo.wait);
	}

	if (self->spawnflags.has(SPAWNFLAG_DOOR_START_OPEN))
		door_use_areaportals(self, false);
}

MOVEINFO_ENDFUNC(door_hit_bottom) (gentity_t *self) -> void {
	if (!(self->flags & FL_TEAMSLAVE)) {
		if (self->moveinfo.sound_end)
			door_play_sound(self, self->moveinfo.sound_end);
	}
	self->s.sound = 0;
	self->moveinfo.state = STATE_BOTTOM;

	if (!self->spawnflags.has(SPAWNFLAG_DOOR_START_OPEN))
		door_use_areaportals(self, false);
}

THINK(door_go_down) (gentity_t *self) -> void {
	if (!(self->flags & FL_TEAMSLAVE)) {
		if (self->moveinfo.sound_start)
			door_play_sound(self, self->moveinfo.sound_start);
	}

	self->s.sound = self->moveinfo.sound_middle;

	if (self->max_health) {
		self->takedamage = true;
		self->health = self->max_health;
	}

	self->moveinfo.state = STATE_DOWN;
	if (strcmp(self->classname, "func_door") == 0 ||
		strcmp(self->classname, "func_water") == 0 ||
		strcmp(self->classname, "func_door_secret") == 0)
		Move_Calc(self, self->moveinfo.start_origin, door_hit_bottom);
	else if (strcmp(self->classname, "func_door_rotating") == 0)
		AngleMove_Calc(self, door_hit_bottom);

	if (self->spawnflags.has(SPAWNFLAG_DOOR_START_OPEN))
		door_use_areaportals(self, true);
}

static void door_go_up(gentity_t *self, gentity_t *activator) {
	if (self->moveinfo.state == STATE_UP)
		return; // already going up

	if (self->moveinfo.state == STATE_TOP) { // reset top wait time
		if (self->moveinfo.wait >= 0)
			self->nextthink = level.time + gtime_t::from_sec(self->moveinfo.wait);
		return;
	}

	if (!(self->flags & FL_TEAMSLAVE)) {
		if (self->moveinfo.sound_start)
			door_play_sound(self, self->moveinfo.sound_start);
	}

	self->s.sound = self->moveinfo.sound_middle;

	self->moveinfo.state = STATE_UP;
	if (strcmp(self->classname, "func_door") == 0 ||
		strcmp(self->classname, "func_water") == 0 ||
		strcmp(self->classname, "func_door_secret") == 0)
		Move_Calc(self, self->moveinfo.end_origin, door_hit_top);
	else if (strcmp(self->classname, "func_door_rotating") == 0)
		AngleMove_Calc(self, door_hit_top);

	G_UseTargets(self, activator);

	if (!(self->spawnflags & SPAWNFLAG_DOOR_START_OPEN))
		door_use_areaportals(self, true);
}

static THINK(smart_water_go_up) (gentity_t *self) -> void {
	float	distance;
	gentity_t *lowestPlayer;
	float	lowestPlayerPt;

	if (self->moveinfo.state == STATE_TOP) { // reset top wait time
		if (self->moveinfo.wait >= 0)
			self->nextthink = level.time + gtime_t::from_sec(self->moveinfo.wait);
		return;
	}

	if (self->health) {
		if (self->absmax[2] >= self->health) {
			self->velocity = {};
			self->nextthink = 0_ms;
			self->moveinfo.state = STATE_TOP;
			return;
		}
	}

	if (!(self->flags & FL_TEAMSLAVE)) {
		if (self->moveinfo.sound_start)
			gi.sound(self, CHAN_NO_PHS_ADD | CHAN_VOICE, self->moveinfo.sound_start, 1, ATTN_STATIC, 0);
	}

	self->s.sound = self->moveinfo.sound_middle;

	// find the lowest player point.
	lowestPlayerPt = 999999;
	lowestPlayer = nullptr;
	for (auto ec : active_clients()) {
		// don't count dead or unused player slots
		if (ec->health > 0) {
			if (ec->absmin[2] < lowestPlayerPt) {
				lowestPlayerPt = ec->absmin[2];
				lowestPlayer = ec;
			}
		}
	}

	if (!lowestPlayer) {
		return;
	}

	distance = lowestPlayerPt - self->absmax[2];

	// for the calculations, make sure we intend to go up at least a little.
	if (distance < self->accel) {
		distance = 100;
		self->moveinfo.speed = 5;
	} else
		self->moveinfo.speed = distance / self->accel;

	if (self->moveinfo.speed < 5)
		self->moveinfo.speed = 5;
	else if (self->moveinfo.speed > self->speed)
		self->moveinfo.speed = self->speed;

	// FIXME - should this allow any movement other than straight up?
	self->moveinfo.dir = { 0, 0, 1 };
	self->velocity = self->moveinfo.dir * self->moveinfo.speed;
	self->moveinfo.remaining_distance = distance;

	if (self->moveinfo.state != STATE_UP) {
		G_UseTargets(self, lowestPlayer);
		door_use_areaportals(self, true);
		self->moveinfo.state = STATE_UP;
	}

	self->think = smart_water_go_up;
	self->nextthink = level.time + FRAME_TIME_S;
}

static USE(door_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	gentity_t *ent;
	vec3_t	 center;

	if (self->flags & FL_TEAMSLAVE)
		return;

	if ((strcmp(self->classname, "func_door_rotating") == 0) && self->spawnflags.has(SPAWNFLAG_DOOR_ROTATING_SAFE_OPEN) &&
		(self->moveinfo.state == STATE_BOTTOM || self->moveinfo.state == STATE_DOWN)) {
		if (self->moveinfo.dir) {
			vec3_t forward = (activator->s.origin - self->s.origin).normalized();
			self->moveinfo.reversing = forward.dot(self->moveinfo.dir) > 0;
		}
	}

	if (self->spawnflags.has(SPAWNFLAG_DOOR_TOGGLE)) {
		if (self->moveinfo.state == STATE_UP || self->moveinfo.state == STATE_TOP) {
			// trigger all paired doors
			for (ent = self; ent; ent = ent->teamchain) {
				ent->message = nullptr;
				ent->touch = nullptr;
				door_go_down(ent);
			}
			return;
		}
	}

	//  smart water is different
	center = self->mins + self->maxs;
	center *= 0.5f;
	if ((strcmp(self->classname, "func_water") == 0) && (gi.pointcontents(center) & MASK_WATER) && self->spawnflags.has(SPAWNFLAG_WATER_SMART)) {
		self->message = nullptr;
		self->touch = nullptr;
		self->enemy = activator;
		smart_water_go_up(self);
		return;
	}

	// trigger all paired doors
	for (ent = self; ent; ent = ent->teamchain) {
		ent->message = nullptr;
		ent->touch = nullptr;
		door_go_up(ent, activator);
	}
};

static TOUCH(Touch_DoorTrigger) (gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	if (other->health <= 0)
		return;

	if (!(other->svflags & SVF_MONSTER) && (!other->client))
		return;

	if (self->owner->spawnflags.has(SPAWNFLAG_DOOR_NOMONSTER) && (other->svflags & SVF_MONSTER))
		return;

	if (level.time < self->touch_debounce_time)
		return;
	self->touch_debounce_time = level.time + 1_sec;

	door_use(self->owner, other, other);
}

static THINK(Think_CalcMoveSpeed) (gentity_t *self) -> void {
	gentity_t *ent;
	float	 min;
	float	 time;
	float	 newspeed;
	float	 ratio;
	float	 dist;

	if (self->flags & FL_TEAMSLAVE)
		return; // only the team master does this

	// find the smallest distance any member of the team will be moving
	min = fabsf(self->moveinfo.distance);
	for (ent = self->teamchain; ent; ent = ent->teamchain) {
		dist = fabsf(ent->moveinfo.distance);
		if (dist < min)
			min = dist;
	}

	time = min / self->moveinfo.speed;

	// adjust speeds so they will all complete at the same time
	for (ent = self; ent; ent = ent->teamchain) {
		newspeed = fabsf(ent->moveinfo.distance) / time;
		ratio = newspeed / ent->moveinfo.speed;
		if (ent->moveinfo.accel == ent->moveinfo.speed)
			ent->moveinfo.accel = newspeed;
		else
			ent->moveinfo.accel *= ratio;
		if (ent->moveinfo.decel == ent->moveinfo.speed)
			ent->moveinfo.decel = newspeed;
		else
			ent->moveinfo.decel *= ratio;
		ent->moveinfo.speed = newspeed;
	}
}

static THINK(Think_SpawnDoorTrigger) (gentity_t *ent) -> void {
	gentity_t *other;
	vec3_t	 mins, maxs;

	if (ent->flags & FL_TEAMSLAVE)
		return; // only the team leader spawns a trigger

	mins = ent->absmin;
	maxs = ent->absmax;

	for (other = ent->teamchain; other; other = other->teamchain) {
		AddPointToBounds(other->absmin, mins, maxs);
		AddPointToBounds(other->absmax, mins, maxs);
	}

	// expand
	mins[0] -= 60;
	mins[1] -= 60;
	maxs[0] += 60;
	maxs[1] += 60;

	other = G_Spawn();
	other->mins = mins;
	other->maxs = maxs;
	other->owner = ent;
	other->solid = SOLID_TRIGGER;
	other->movetype = MOVETYPE_NONE;
	other->touch = Touch_DoorTrigger;
	gi.linkentity(other);

	Think_CalcMoveSpeed(ent);
}

MOVEINFO_BLOCKED(door_blocked) (gentity_t *self, gentity_t *other) -> void {
	gentity_t *ent;

	if (!(other->svflags & SVF_MONSTER) && (!other->client)) {
		// give it a chance to go away on it's own terms (like gibs)
		T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, DAMAGE_NONE, MOD_CRUSH);
		// if it's still there, nuke it
		if (other && other->inuse)
			BecomeExplosion1(other);
		return;
	}

	if (self->dmg && !(level.time < self->touch_debounce_time)) {
		self->touch_debounce_time = level.time + 10_hz;
		T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MOD_CRUSH);
	}

	// [Paril-KEX] don't allow wait -1 doors to return
	if (self->spawnflags.has(SPAWNFLAG_DOOR_CRUSHER) || self->wait == -1)
		return;

	// if a door has a negative wait, it would never come back if blocked,
	// so let it just squash the object to death real fast
	if (self->moveinfo.wait >= 0) {
		if (self->moveinfo.state == STATE_DOWN) {
			for (ent = self->teammaster; ent; ent = ent->teamchain)
				door_go_up(ent, ent->activator);
		} else {
			for (ent = self->teammaster; ent; ent = ent->teamchain)
				door_go_down(ent);
		}
	}
}

DIE(door_killed) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod) -> void {
	gentity_t *ent;

	for (ent = self->teammaster; ent; ent = ent->teamchain) {
		ent->health = ent->max_health;
		ent->takedamage = false;
	}
	door_use(self->teammaster, attacker, attacker);
}

TOUCH(door_touch) (gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	if (!other->client)
		return;

	if (level.time < self->touch_debounce_time)
		return;

	self->touch_debounce_time = level.time + 5_sec;

	gi.LocCenter_Print(other, "{}", self->message);
	gi.sound(other, CHAN_AUTO, gi.soundindex("misc/talk1.wav"), 1, ATTN_NORM, 0);
}

static THINK(Think_DoorActivateAreaPortal) (gentity_t *ent) -> void {
	door_use_areaportals(ent, true);

	if (ent->health || ent->targetname)
		Think_CalcMoveSpeed(ent);
	else
		Think_SpawnDoorTrigger(ent);
}

void SP_func_door(gentity_t *ent) {
	vec3_t abs_movedir;

	if (ent->sounds != 1)
		G_SetMoveinfoSounds(ent, "doors/dr1_strt.wav", "doors/dr1_mid.wav", "doors/dr1_end.wav");
	else
		G_SetMoveinfoSounds(ent, nullptr, nullptr, nullptr);

	// [Paril-KEX] for rhangar1 doors
	if (!st.was_key_specified("attenuation"))
		ent->attenuation = ATTN_STATIC;
	else {
		if (ent->attenuation == -1) {
			ent->s.loop_attenuation = ATTN_LOOP_NONE;
			ent->attenuation = ATTN_NONE;
		} else {
			ent->s.loop_attenuation = ent->attenuation;
		}
	}

	G_SetMovedir(ent->s.angles, ent->movedir);
	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_BSP;
	ent->svflags |= SVF_DOOR;
	gi.setmodel(ent, ent->model);

	ent->moveinfo.blocked = door_blocked;
	ent->use = door_use;

	if (!ent->speed)
		ent->speed = 100;
	if (deathmatch->integer)
		ent->speed *= 2;
	if (g_fast_doors->integer)
		ent->speed *= 2;

	if (g_mover_speed_scale->value != 1.0f) {
		ent->speed *= g_mover_speed_scale->value;
		ent->accel *= g_mover_speed_scale->value;
		ent->decel *= g_mover_speed_scale->value;
	}

	if (!ent->accel)
		ent->accel = ent->speed;
	if (!ent->decel)
		ent->decel = ent->speed;

	if (!ent->wait)
		ent->wait = 3;
	if (!st.lip)
		st.lip = 8;
	if (!ent->dmg)
		ent->dmg = 2;

	// calculate second position
	ent->pos1 = ent->s.origin;
	abs_movedir[0] = fabsf(ent->movedir[0]);
	abs_movedir[1] = fabsf(ent->movedir[1]);
	abs_movedir[2] = fabsf(ent->movedir[2]);
	ent->moveinfo.distance =
		abs_movedir[0] * ent->size[0] + abs_movedir[1] * ent->size[1] + abs_movedir[2] * ent->size[2] - st.lip;
	ent->pos2 = ent->pos1 + (ent->movedir * ent->moveinfo.distance);

	// if it starts open, switch the positions
	if (ent->spawnflags.has(SPAWNFLAG_DOOR_START_OPEN)) {
		ent->s.origin = ent->pos2;
		ent->pos2 = ent->pos1;
		ent->pos1 = ent->s.origin;
	}

	ent->moveinfo.state = STATE_BOTTOM;

	if (ent->health) {
		ent->takedamage = true;
		ent->die = door_killed;
		ent->max_health = ent->health;
	} else if (ent->targetname) {
		if (ent->message) {
			gi.soundindex("misc/talk.wav");
			ent->touch = door_touch;
		}
		ent->flags |= FL_LOCKED;
	}

	ent->moveinfo.speed = ent->speed;
	ent->moveinfo.accel = ent->accel;
	ent->moveinfo.decel = ent->decel;
	ent->moveinfo.wait = ent->wait;
	ent->moveinfo.start_origin = ent->pos1;
	ent->moveinfo.start_angles = ent->s.angles;
	ent->moveinfo.end_origin = ent->pos2;
	ent->moveinfo.end_angles = ent->s.angles;

	if (ent->spawnflags.has(SPAWNFLAG_DOOR_ANIMATED))
		ent->s.effects |= EF_ANIM_ALL;
	if (ent->spawnflags.has(SPAWNFLAG_DOOR_ANIMATED_FAST))
		ent->s.effects |= EF_ANIM_ALLFAST;

	// to simplify logic elsewhere, make non-teamed doors into a team of one
	if (!ent->team)
		ent->teammaster = ent;

	gi.linkentity(ent);

	ent->nextthink = level.time + FRAME_TIME_S;

	if (ent->spawnflags.has(SPAWNFLAG_DOOR_START_OPEN))
		ent->think = Think_DoorActivateAreaPortal;
	else if (ent->health || ent->targetname)
		ent->think = Think_CalcMoveSpeed;
	else
		ent->think = Think_SpawnDoorTrigger;
}

static USE(Door_Activate) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	self->use = nullptr;

	if (self->health) {
		self->takedamage = true;
		self->die = door_killed;
		self->max_health = self->health;
	}

	if (self->health)
		self->think = Think_CalcMoveSpeed;
	else
		self->think = Think_SpawnDoorTrigger;
	self->nextthink = level.time + FRAME_TIME_S;
}

/*QUAKED func_door_rotating (0 .5 .8) ? START_OPEN REVERSE CRUSHER NOMONSTER ANIMATED TOGGLE X_AXIS Y_AXIS NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP RESERVED1 COOP_ONLY RESERVED2 INACTIVE SAFE_OPEN
TOGGLE causes the door to wait in both the start and end states for a trigger event.

START_OPEN	the door to moves to its destination when spawned, and operate in reverse.  It is used to temporarily or permanently close off an area when triggered (not useful for touch or takedamage doors).
NOMONSTER	monsters will not trigger this door

You need to have an origin brush as part of this entity.  The center of that brush will be
the point around which it is rotated. It will rotate around the Z axis by default.  You can
check either the X_AXIS or Y_AXIS box to change that.

"distance" is how many degrees the door will be rotated.
"speed" determines how fast the door moves; default value is 100.
"accel" if specified,is how much the rotation speed will increase each .1 sec. (default: no accel)

REVERSE will cause the door to rotate in the opposite direction.
INACTIVE will cause the door to be inactive until triggered.
SAFE_OPEN will cause the door to open in reverse if you are on the `angles` side of the door.

"message"	is printed when the door is touched if it is a trigger door and it hasn't been fired yet
"angle"		determines the opening direction
"targetname" if set, no touch field will be spawned and a remote button or trigger field activates the door.
"health"	if set, door must be shot open
"speed"		movement speed (100 default)
"wait"		wait before returning (3 default, -1 = never return)
"dmg"		damage to inflict when blocked (2 default)
"sounds"
1)	silent
2)	light
3)	medium
4)	heavy
*/

void SP_func_door_rotating(gentity_t *ent) {
	if (ent->spawnflags.has(SPAWNFLAG_DOOR_ROTATING_SAFE_OPEN))
		G_SetMovedir(ent->s.angles, ent->moveinfo.dir);

	ent->s.angles = {};

	// set the axis of rotation
	ent->movedir = {};
	if (ent->spawnflags.has(SPAWNFLAG_DOOR_ROTATING_X_AXIS))
		ent->movedir[2] = 1.0;
	else if (ent->spawnflags.has(SPAWNFLAG_DOOR_ROTATING_Y_AXIS))
		ent->movedir[0] = 1.0;
	else // Z_AXIS
		ent->movedir[1] = 1.0;

	// check for reverse rotation
	if (ent->spawnflags.has(SPAWNFLAG_DOOR_REVERSE))
		ent->movedir = -ent->movedir;

	if (!st.distance) {
		gi.Com_PrintFmt("{}: no distance set\n", *ent);
		st.distance = 90;
	}

	ent->pos1 = ent->s.angles;
	ent->pos2 = ent->s.angles + (ent->movedir * st.distance);
	ent->pos3 = ent->s.angles + (ent->movedir * -st.distance);
	ent->moveinfo.distance = (float)st.distance;

	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_BSP;
	ent->svflags |= SVF_DOOR;
	gi.setmodel(ent, ent->model);

	ent->moveinfo.blocked = door_blocked;
	ent->use = door_use;

	if (!ent->speed)
		ent->speed = 100;
	if (g_fast_doors->integer)
		ent->speed *= 2;

	if (g_mover_speed_scale->value != 1.0f) {
		ent->speed *= g_mover_speed_scale->value;
		ent->accel *= g_mover_speed_scale->value;
		ent->decel *= g_mover_speed_scale->value;
	}

	if (!ent->accel)
		ent->accel = ent->speed;
	if (!ent->decel)
		ent->decel = ent->speed;

	if (!ent->wait)
		ent->wait = 3;
	if (!ent->dmg)
		ent->dmg = 2;

	if (ent->sounds != 1)
		G_SetMoveinfoSounds(ent, "doors/dr1_strt.wav", "doors/dr1_mid.wav", "doors/dr1_end.wav");
	else
		G_SetMoveinfoSounds(ent, nullptr, nullptr, nullptr);

	// [Paril-KEX] for rhangar1 doors
	if (!st.was_key_specified("attenuation"))
		ent->attenuation = ATTN_STATIC;
	else {
		if (ent->attenuation == -1) {
			ent->s.loop_attenuation = ATTN_LOOP_NONE;
			ent->attenuation = ATTN_NONE;
		} else {
			ent->s.loop_attenuation = ent->attenuation;
		}
	}

	// if it starts open, switch the positions
	if (ent->spawnflags.has(SPAWNFLAG_DOOR_START_OPEN)) {
		if (ent->spawnflags.has(SPAWNFLAG_DOOR_ROTATING_SAFE_OPEN)) {
			ent->spawnflags &= ~SPAWNFLAG_DOOR_ROTATING_SAFE_OPEN;
			gi.Com_PrintFmt("{}: SAFE_OPEN is not compatible with START_OPEN\n", *ent);
		}

		ent->s.angles = ent->pos2;
		ent->pos2 = ent->pos1;
		ent->pos1 = ent->s.angles;
		ent->movedir = -ent->movedir;
	}

	if (ent->health) {
		ent->takedamage = true;
		ent->die = door_killed;
		ent->max_health = ent->health;
	}

	if (ent->targetname && ent->message) {
		gi.soundindex("misc/talk.wav");
		ent->touch = door_touch;
	}

	ent->moveinfo.state = STATE_BOTTOM;
	ent->moveinfo.speed = ent->speed;
	ent->moveinfo.accel = ent->accel;
	ent->moveinfo.decel = ent->decel;
	ent->moveinfo.wait = ent->wait;
	ent->moveinfo.start_origin = ent->s.origin;
	ent->moveinfo.start_angles = ent->pos1;
	ent->moveinfo.end_origin = ent->s.origin;
	ent->moveinfo.end_angles = ent->pos2;
	ent->moveinfo.end_angles_reversed = ent->pos3;

	if (ent->spawnflags.has(SPAWNFLAG_DOOR_ANIMATED))
		ent->s.effects |= EF_ANIM_ALL;

	// to simplify logic elsewhere, make non-teamed doors into a team of one
	if (!ent->team)
		ent->teammaster = ent;

	gi.linkentity(ent);

	ent->nextthink = level.time + FRAME_TIME_S;
	if (ent->health || ent->targetname)
		ent->think = Think_CalcMoveSpeed;
	else
		ent->think = Think_SpawnDoorTrigger;

	if (ent->spawnflags.has(SPAWNFLAG_DOOR_ROTATING_INACTIVE)) {
		ent->takedamage = false;
		ent->die = nullptr;
		ent->think = nullptr;
		ent->nextthink = 0_ms;
		ent->use = Door_Activate;
	}
}

MOVEINFO_BLOCKED(smart_water_blocked) (gentity_t *self, gentity_t *other) -> void {
	if (!(other->svflags & SVF_MONSTER) && (!other->client)) {
		// give it a chance to go away on it's own terms (like gibs)
		T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, DAMAGE_NONE, MOD_LAVA);
		// if it's still there, nuke it
		if (other && other->inuse && other->solid)
			BecomeExplosion1(other);
		return;
	}

	T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, 100, 1, DAMAGE_NONE, MOD_LAVA);
}

/*QUAKED func_water (0 .5 .8) ? START_OPEN SMART x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
func_water is a moveable water brush.  It must be targeted to operate.  Use a non-water texture at your own risk.

START_OPEN causes the water to move to its destination when spawned and operate in reverse.

SMART causes the water to adjust its speed depending on distance to player.
(speed = distance/accel, min 5, max self->speed)
"accel"		for smart water, the divisor to determine water speed. default 20 (smaller = faster)

"health"	maximum height of this water brush
"angle"		determines the opening direction (up or down only)
"speed"		movement speed (25 default)
"wait"		wait before returning (-1 default, -1 = TOGGLE)
"lip"		lip remaining at end of move (0 default)
"sounds"	(yes, these need to be changed)
0)	no sound
1)	water
2)	lava
*/

void SP_func_water(gentity_t *self) {
	vec3_t abs_movedir;

	G_SetMovedir(self->s.angles, self->movedir);
	self->movetype = MOVETYPE_PUSH;
	self->solid = SOLID_BSP;
	gi.setmodel(self, self->model);

	switch (self->sounds) {
	default:
		G_SetMoveinfoSounds(self, nullptr, nullptr, nullptr);
		break;

	case 1: // water
	case 2: // lava
		G_SetMoveinfoSounds(self, "world/mov_watr.wav", nullptr, "world/stp_watr.wav");
		break;
	}

	self->attenuation = ATTN_STATIC;

	// calculate second position
	self->pos1 = self->s.origin;
	abs_movedir[0] = fabsf(self->movedir[0]);
	abs_movedir[1] = fabsf(self->movedir[1]);
	abs_movedir[2] = fabsf(self->movedir[2]);
	self->moveinfo.distance =
		abs_movedir[0] * self->size[0] + abs_movedir[1] * self->size[1] + abs_movedir[2] * self->size[2] - st.lip;
	self->pos2 = self->pos1 + (self->movedir * self->moveinfo.distance);

	// if it starts open, switch the positions
	if (self->spawnflags.has(SPAWNFLAG_DOOR_START_OPEN)) {
		self->s.origin = self->pos2;
		self->pos2 = self->pos1;
		self->pos1 = self->s.origin;
	}

	self->moveinfo.start_origin = self->pos1;
	self->moveinfo.start_angles = self->s.angles;
	self->moveinfo.end_origin = self->pos2;
	self->moveinfo.end_angles = self->s.angles;

	self->moveinfo.state = STATE_BOTTOM;

	if (!self->speed)
		self->speed = 25;

	if (g_mover_speed_scale->value != 1.0f) {
		self->speed *= g_mover_speed_scale->value;
		self->accel *= g_mover_speed_scale->value;
		self->decel *= g_mover_speed_scale->value;
	}

	self->moveinfo.accel = self->moveinfo.decel = self->moveinfo.speed = self->speed;

	if (self->spawnflags.has(SPAWNFLAG_WATER_SMART)) // smart water
	{
		// this is actually the divisor of the lowest player's distance to determine speed.
		// self->speed then becomes the cap of the speed.
		if (!self->accel)
			self->accel = 20;
		self->moveinfo.blocked = smart_water_blocked;
	}

	if (!self->wait)
		self->wait = -1;
	self->moveinfo.wait = self->wait;

	self->use = door_use;

	if (self->wait == -1)
		self->spawnflags |= SPAWNFLAG_DOOR_TOGGLE;

	gi.linkentity(self);
}

// Train, elevator, timer and conveyor entities live in g_func_train.cpp.

// Secret door entities live in g_func_secret_doors.cpp.

// Force walls, killboxes, tracking eyes, rotating lights and repair entities live in g_func_specials.cpp.
