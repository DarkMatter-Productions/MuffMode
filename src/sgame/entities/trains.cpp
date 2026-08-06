// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Train, elevator, timer and conveyor brush entities.
#include "g_local.h"

void train_next(gentity_t *self);
void train_piece_wait(gentity_t *self);

namespace {

gentity_t *ResolveTrainCorner(gentity_t *self)
{
	if (!self || !self->target_ent)
		return nullptr;
	gentity_t *const corner = self->target_ent;
	if (!corner->inuse || (self->target_ent_generation &&
		corner->spawn_count != self->target_ent_generation)) {
		self->target_ent = nullptr;
		self->target_ent_generation = 0;
		return nullptr;
	}
	if (!self->target_ent_generation)
		self->target_ent_generation = corner->spawn_count;
	return corner;
}

constexpr spawnflags_t SPAWNFLAG_TRAIN_TOGGLE = 2_spawnflag;
constexpr spawnflags_t SPAWNFLAG_TRAIN_BLOCK_STOPS = 4_spawnflag;
constexpr spawnflags_t SPAWNFLAG_TRAIN_FIX_OFFSET = 16_spawnflag;
constexpr spawnflags_t SPAWNFLAG_TRAIN_USE_ORIGIN = 32_spawnflag;

constexpr spawnflags_t SPAWNFLAG_TIMER_START_ON = 1_spawnflag;

constexpr spawnflags_t SPAWNFLAG_CONVEYOR_START_ON = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAG_CONVEYOR_TOGGLE = 2_spawnflag;

vec3_t TrainDestinationFor(gentity_t *self, gentity_t *corner) {
	if (self->spawnflags.has(SPAWNFLAG_TRAIN_USE_ORIGIN))
		return corner->s.origin;

	vec3_t dest = corner->s.origin - self->mins;

	if (self->spawnflags.has(SPAWNFLAG_TRAIN_FIX_OFFSET))
		dest -= vec3_t{ 1.0f, 1.0f, 1.0f };

	return dest;
}

void SetTrainOriginFromCorner(gentity_t *self, gentity_t *corner) {
	self->s.origin = TrainDestinationFor(self, corner);
}

void ApplyPathCornerMoveSpeed(gentity_t *self, gentity_t *corner) {
	if (!corner->speed)
		return;

	self->speed = corner->speed;
	self->moveinfo.speed = corner->speed;
	self->moveinfo.accel = corner->accel ? corner->accel : corner->speed;
	self->moveinfo.decel = corner->decel ? corner->decel : corner->speed;
	self->moveinfo.current_speed = 0.0f;
}

void StartTrainMove(gentity_t *self, const vec3_t &dest, void(*endfunc)(gentity_t *self)) {
	self->s.sound = self->moveinfo.sound_middle;
	self->moveinfo.state = STATE_TOP;
	self->moveinfo.start_origin = self->s.origin;
	self->moveinfo.end_origin = dest;
	Move_Calc(self, dest, endfunc);
	self->spawnflags |= SPAWNFLAG_TRAIN_START_ON;
}

void MoveTeamChainWithTrain(gentity_t *self, const vec3_t &dest) {
	if (!self->spawnflags.has(SPAWNFLAG_TRAIN_MOVE_TEAMCHAIN))
		return;

	const vec3_t dir = dest - self->s.origin;

	for (gentity_t *e = self->teamchain; e; e = e->teamchain) {
		const vec3_t dst = dir + e->s.origin;

		e->moveinfo.start_origin = e->s.origin;
		e->moveinfo.end_origin = dst;
		e->moveinfo.state = STATE_TOP;
		e->speed = self->speed;
		e->moveinfo.speed = self->moveinfo.speed;
		e->moveinfo.accel = self->moveinfo.accel;
		e->moveinfo.decel = self->moveinfo.decel;
		e->movetype = MOVETYPE_PUSH;
		Move_Calc(e, dst, train_piece_wait);
	}
}

} // namespace

/*QUAKED func_train (0 .5 .8) ? START_ON TOGGLE BLOCK_STOPS MOVE_TEAMCHAIN FIX_OFFSET USE_ORIGIN x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Trains are moving platforms that players can ride.
The targets origin specifies the min point of the train at each corner.
The train spawns at the first target it is pointing at.
If the train is the target of a button or trigger, it will not begin moving until activated.
speed	default 100
dmg		default	2
noise	looping sound to play when the train is in motion

To have other entities move with the train, set all the piece's team value to the same thing. They will move in unison.
*/
MOVEINFO_BLOCKED(train_blocked) (gentity_t *self, gentity_t *other) -> void {
	if (!(other->svflags & SVF_MONSTER) && (!other->client)) {
		// give it a chance to go away on it's own terms (like gibs)
		T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, DAMAGE_NONE, MOD_CRUSH);
		// if it's still there, nuke it
		if (other && other->inuse && other->solid)
			BecomeExplosion1(other);
		return;
	}

	if (level.time < self->touch_debounce_time)
		return;

	if (!self->dmg)
		return;

	self->touch_debounce_time = level.time + 500_ms;
	T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MOD_CRUSH);
}

MOVEINFO_ENDFUNC(train_wait) (gentity_t *self) -> void {
	gentity_t *const current_corner = ResolveTrainCorner(self);
	if (!current_corner) {
		// The path corner can be removed by a target while the train is in
		// flight. Stop cleanly instead of following a retired entity slot.
		self->s.sound = 0;
		self->velocity = {};
		self->nextthink = 0_ms;
		self->spawnflags &= ~SPAWNFLAG_TRAIN_START_ON;
		return;
	}
	if (current_corner->pathtarget) {
		gentity_t *ent = current_corner;
		const char *savetarget = ent->target;
		const int32_t self_generation = self->spawn_count;

		ent->target = ent->pathtarget;
		if (!G_UseTargets(ent, self->activator))
			return;
		ent->target = savetarget;

		// make sure we didn't get killed by a killtarget
		if (!self->inuse || self->spawn_count != self_generation)
			return;
	}

	if (self->moveinfo.wait) {
		if (self->moveinfo.wait > 0) {
			self->nextthink = level.time + gtime_t::from_sec(self->moveinfo.wait);
			self->think = train_next;
		} else if (self->spawnflags.has(SPAWNFLAG_TRAIN_TOGGLE)) {
			self->target_ent = nullptr;
			self->target_ent_generation = 0;
			self->spawnflags &= ~SPAWNFLAG_TRAIN_START_ON;
			self->velocity = {};
			self->nextthink = 0_ms;
		}

		if (!(self->flags & FL_TEAMSLAVE)) {
			if (self->moveinfo.sound_end)
				gi.sound(self, CHAN_NO_PHS_ADD | CHAN_VOICE, self->moveinfo.sound_end, 1, ATTN_STATIC, 0);
		}

		self->s.sound = 0;
	} else {
		train_next(self);
	}
}

MOVEINFO_ENDFUNC(train_piece_wait) (gentity_t *self) -> void {}

THINK(train_next) (gentity_t *self) -> void {
	bool first = true;

again:
	if (!self->target) {
		self->s.sound = 0;
		return;
	}

	gentity_t *ent = G_PickTarget(self->target);
	if (!ent) {
		gi.Com_PrintFmt("{}: train_next: bad target {}\n", *self, self->target);
		return;
	}

	self->target = ent->target;

	// check for a teleport path_corner
	if (ent->spawnflags.has(SPAWNFLAG_PATH_CORNER_TELEPORT)) {
		if (!first) {
			gi.Com_PrintFmt("{}: connected teleport path_corners\n", *ent);
			return;
		}

		first = false;
		SetTrainOriginFromCorner(self, ent);
		self->s.old_origin = self->s.origin;
		self->s.event = EV_OTHER_TELEPORT;
		gi.linkentity(self);
		goto again;
	}

	ApplyPathCornerMoveSpeed(self, ent);

	self->moveinfo.wait = ent->wait;
	self->target_ent = ent;
	self->target_ent_generation = ent->spawn_count;

	if (!(self->flags & FL_TEAMSLAVE)) {
		if (self->moveinfo.sound_start)
			gi.sound(self, CHAN_NO_PHS_ADD | CHAN_VOICE, self->moveinfo.sound_start, 1, ATTN_STATIC, 0);
	}

	const vec3_t dest = TrainDestinationFor(self, ent);
	StartTrainMove(self, dest, train_wait);
	MoveTeamChainWithTrain(self, dest);
}

static void train_resume(gentity_t *self) {
	gentity_t *const corner = ResolveTrainCorner(self);
	if (!corner)
		return;
	const vec3_t dest = TrainDestinationFor(self, corner);
	StartTrainMove(self, dest, train_wait);
}

THINK(func_train_find) (gentity_t *self) -> void {
	if (!self->target) {
		gi.Com_PrintFmt("{}: train_find: no target\n", *self);
		return;
	}

	gentity_t *ent = G_PickTarget(self->target);
	if (!ent) {
		gi.Com_PrintFmt("{}: train_find: target {} not found\n", *self, self->target);
		return;
	}

	self->target = ent->target;
	SetTrainOriginFromCorner(self, ent);
	gi.linkentity(self);

	// if not triggered, start immediately
	if (!self->targetname)
		self->spawnflags |= SPAWNFLAG_TRAIN_START_ON;

	if (self->spawnflags.has(SPAWNFLAG_TRAIN_START_ON)) {
		self->nextthink = level.time + FRAME_TIME_S;
		self->think = train_next;
		self->activator = self;
	}
}

USE(train_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	self->activator = activator;

	if (self->spawnflags.has(SPAWNFLAG_TRAIN_START_ON)) {
		if (!self->spawnflags.has(SPAWNFLAG_TRAIN_TOGGLE))
			return;

		self->spawnflags &= ~SPAWNFLAG_TRAIN_START_ON;
		self->velocity = {};
		self->nextthink = 0_ms;
	} else {
		if (ResolveTrainCorner(self))
			train_resume(self);
		else
			train_next(self);
	}
}

void SP_func_train(gentity_t *self) {
	self->movetype = MOVETYPE_PUSH;
	self->s.angles = {};
	self->moveinfo.blocked = train_blocked;

	if (self->spawnflags.has(SPAWNFLAG_TRAIN_BLOCK_STOPS)) {
		self->dmg = 0;
	} else if (!self->dmg) {
		self->dmg = 100;
	}

	self->solid = SOLID_BSP;
	gi.setmodel(self, self->model);

	if (st.noise) {
		self->moveinfo.sound_middle = gi.soundindex(st.noise);

		// [Paril-KEX] for rhangar1 doors
		if (!st.was_key_specified("attenuation"))
			self->attenuation = ATTN_STATIC;
		else {
			if (self->attenuation == -1) {
				self->s.loop_attenuation = ATTN_LOOP_NONE;
				self->attenuation = ATTN_NONE;
			} else {
				self->s.loop_attenuation = self->attenuation;
			}
		}
	}

	if (!self->speed)
		self->speed = 100;

	if (g_mover_speed_scale->value != 1.0f) {
		self->speed *= g_mover_speed_scale->value;
		self->accel *= g_mover_speed_scale->value;
		self->decel *= g_mover_speed_scale->value;
	}

	self->moveinfo.speed = self->speed;
	self->moveinfo.accel = self->moveinfo.decel = self->moveinfo.speed;
	self->use = train_use;

	gi.linkentity(self);

	if (self->target) {
		// start trains on the second frame, to make sure their targets have had
		// a chance to spawn
		self->nextthink = level.time + FRAME_TIME_S;
		self->think = func_train_find;
	} else {
		gi.Com_PrintFmt("{}: no target\n", *self);
	}
}

/*QUAKED trigger_elevator (0.3 0.1 0.6) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
static USE(trigger_elevator_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	if (self->movetarget->nextthink)
		return;

	if (!other->pathtarget) {
		gi.Com_PrintFmt("{}: elevator used with no pathtarget\n", *self);
		return;
	}

	gentity_t *target = G_PickTarget(other->pathtarget);
	if (!target) {
		gi.Com_PrintFmt("{}: elevator used with bad pathtarget: {}\n", *self, other->pathtarget);
		return;
	}

	self->movetarget->target_ent = target;
	self->movetarget->target_ent_generation = target->spawn_count;
	train_resume(self->movetarget);
}

static THINK(trigger_elevator_init) (gentity_t *self) -> void {
	if (!self->target) {
		gi.Com_PrintFmt("{}: has no target\n", *self);
		return;
	}

	self->movetarget = G_PickTarget(self->target);
	if (!self->movetarget) {
		gi.Com_PrintFmt("{}: unable to find target {}\n", *self, self->target);
		return;
	}

	if (strcmp(self->movetarget->classname, "func_train") != 0) {
		gi.Com_PrintFmt("{}: target {} is not a train\n", *self, self->target);
		return;
	}

	self->use = trigger_elevator_use;
	self->svflags = SVF_NOCLIENT;
}

void SP_trigger_elevator(gentity_t *self) {
	self->think = trigger_elevator_init;
	self->nextthink = level.time + FRAME_TIME_S;
}

/*QUAKED func_timer (0.3 0.1 0.6) (-8 -8 -8) (8 8 8) START_ON x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
"wait"			base time between triggering all targets, default is 1
"random"		wait variance, default is 0

so, the basic time between firing is a random time between
(wait - random) and (wait + random)

"delay"			delay before first firing when turned on, default is 0

"pausetime"		additional delay used only the very first time
				and only if spawned with START_ON

These can used but not touched.
*/
static THINK(func_timer_think) (gentity_t *self) -> void {
	if (!G_UseTargets(self, self->activator))
		return;
	self->nextthink = level.time + gtime_t::from_sec(self->wait + crandom() * self->random);
}

static USE(func_timer_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	self->activator = activator;

	// if on, turn it off
	if (self->nextthink) {
		self->nextthink = 0_ms;
		return;
	}

	// turn it on
	if (self->delay)
		self->nextthink = level.time + gtime_t::from_sec(self->delay);
	else
		func_timer_think(self);
}

void SP_func_timer(gentity_t *self) {
	if (!self->wait)
		self->wait = 1.0f;

	self->use = func_timer_use;
	self->think = func_timer_think;

	if (self->random >= self->wait) {
		self->random = self->wait - gi.frame_time_s;
		gi.Com_PrintFmt("{}: random >= wait\n", *self);
	}

	if (self->spawnflags.has(SPAWNFLAG_TIMER_START_ON)) {
		self->nextthink = level.time + 1_sec + gtime_t::from_sec(st.pausetime + self->delay + self->wait + crandom() * self->random);
		self->activator = self;
	}

	self->svflags = SVF_NOCLIENT;
}

/*QUAKED func_conveyor (0 .5 .8) ? START_ON TOGGLE x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Conveyors are stationary brushes that move what's on them.
The brush should be have a surface with at least one current content enabled.
speed	default 100
*/
static USE(func_conveyor_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	if (self->spawnflags.has(SPAWNFLAG_CONVEYOR_START_ON)) {
		self->speed = 0;
		self->spawnflags &= ~SPAWNFLAG_CONVEYOR_START_ON;
	} else {
		self->speed = static_cast<float>(self->count);
		self->spawnflags |= SPAWNFLAG_CONVEYOR_START_ON;
	}

	if (!self->spawnflags.has(SPAWNFLAG_CONVEYOR_TOGGLE))
		self->count = 0;
}

void SP_func_conveyor(gentity_t *self) {
	if (!self->speed)
		self->speed = 100;

	if (!self->spawnflags.has(SPAWNFLAG_CONVEYOR_START_ON)) {
		self->count = static_cast<int32_t>(self->speed);
		self->speed = 0;
	}

	self->use = func_conveyor_use;

	gi.setmodel(self, self->model);
	self->solid = SOLID_BSP;
	gi.linkentity(self);
}
