// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Specialty brush entities: force walls, killboxes, eyes, lights and repair targets.
#include <cmath>

#include "g_local.h"

namespace {

constexpr spawnflags_t SPAWNFLAG_FORCEWALL_START_ON = 1_spawnflag;

constexpr spawnflags_t SPAWNFLAG_KILLBOX_DEADLY_COOP = 2_spawnflag;
constexpr spawnflags_t SPAWNFLAG_KILLBOX_EXACT_COLLISION = 4_spawnflag;

constexpr spawnflags_t SPAWNFLAG_FUNC_EYE_FIRED_TARGETS = 17_spawnflag_bit; // internal use only

constexpr spawnflags_t SPAWNFLAG_ROTATING_LIGHT_START_OFF = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAG_ROTATING_LIGHT_ALARM = 2_spawnflag;

constexpr spawnflags_t SPAWNFLAG_BOBBING_X_AXIS = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAG_BOBBING_Y_AXIS = 2_spawnflag;

constexpr float DEFAULT_BOBBING_CYCLE_SECONDS = 4.0f;
constexpr float DEFAULT_BOBBING_HEIGHT = 32.0f;
constexpr float DEFAULT_PENDULUM_SWING_DEGREES = 30.0f;
constexpr float MIN_PENDULUM_LENGTH = 8.0f;

void EmitWeldingSparks(gentity_t *ent, int32_t count) {
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_WELDING_SPARKS);
	gi.WriteByte(count);
	gi.WritePosition(ent->s.origin);
	gi.WriteDir(vec3_origin);
	gi.WriteByte(irandom(0xe0, 0xe8));
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);
}

vec3_t BrushCenter(gentity_t *ent) {
	return (ent->absmax + ent->absmin) * 0.5f;
}

void SetupForceWallLine(gentity_t *ent) {
	ent->offset = BrushCenter(ent);

	ent->pos1[2] = ent->absmax[2];
	ent->pos2[2] = ent->absmax[2];

	if (ent->size[0] > ent->size[1]) {
		ent->pos1[0] = ent->absmin[0];
		ent->pos2[0] = ent->absmax[0];
		ent->pos1[1] = ent->offset[1];
		ent->pos2[1] = ent->offset[1];
	} else {
		ent->pos1[0] = ent->offset[0];
		ent->pos2[0] = ent->offset[0];
		ent->pos1[1] = ent->absmin[1];
		ent->pos2[1] = ent->absmax[1];
	}
}

float SineMoverCycleSeconds(const gentity_t *ent) {
	return max(ent->wait, gi.frame_time_s);
}

float SineMoverPhaseAt(const gentity_t *ent, const gtime_t at_time) {
	const float cycle_seconds = SineMoverCycleSeconds(ent);
	const float elapsed = (at_time - ent->timestamp).seconds();
	return sinf((fmodf(elapsed, cycle_seconds) / cycle_seconds) * (PIf * 2.0f));
}

vec3_t SineMoverOriginAt(const gentity_t *ent, const gtime_t at_time) {
	return ent->pos1 + (ent->pos2 * SineMoverPhaseAt(ent, at_time));
}

vec3_t SineMoverAnglesAt(const gentity_t *ent, const gtime_t at_time) {
	return ent->move_angles + (ent->movedir * SineMoverPhaseAt(ent, at_time));
}

void PrimeSineMoverFrame(gentity_t *ent) {
	const gtime_t target_time = level.time + FRAME_TIME_MS;
	ent->velocity = (SineMoverOriginAt(ent, target_time) - ent->s.origin) * (1.0f / gi.frame_time_s);
	ent->avelocity = (SineMoverAnglesAt(ent, target_time) - ent->s.angles) * (1.0f / gi.frame_time_s);
}

PRETHINK(sine_mover_prethink) (gentity_t *self) -> void {
	if (self->flags & FL_TEAMSLAVE) {
		PrimeSineMoverFrame(self);
		return;
	}

	for (gentity_t *part = self; part; part = part->teamchain)
		PrimeSineMoverFrame(part);
}

void SetupSineMover(gentity_t *ent, const float cycle_seconds, const float phase) {
	ent->solid = SOLID_BSP;
	ent->movetype = MOVETYPE_PUSH;
	ent->flags |= FL_Q3_SINE_MOVER;
	ent->wait = max(cycle_seconds, gi.frame_time_s);
	ent->timestamp = level.time + gtime_t::from_sec(ent->wait * phase);
	ent->prethink = sine_mover_prethink;

	ent->s.origin = SineMoverOriginAt(ent, level.time);
	ent->s.angles = SineMoverAnglesAt(ent, level.time);
	PrimeSineMoverFrame(ent);

	gi.linkentity(ent);
}

} // namespace

/*QUAKED func_force_wall (1 0 1) ? START_ON x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
A vertical particle force wall. Turns on and solid when triggered.
If someone is in the force wall when it turns on, they're telefragged.

START_ON - forcewall begins activated. triggering will turn it off.

style - color of particles to use.
	208: green, 240: red, 241: blue, 224: orange
*/
static THINK(force_wall_think) (gentity_t *self) -> void {
	if (!self->wait) {
		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_FORCEWALL);
		gi.WritePosition(self->pos1);
		gi.WritePosition(self->pos2);
		gi.WriteByte(self->style);
		gi.multicast(self->offset, MULTICAST_PVS, false);
	}

	self->think = force_wall_think;
	self->nextthink = level.time + 10_hz;
}

static USE(force_wall_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	if (!self->wait) {
		self->wait = 1;
		self->think = nullptr;
		self->nextthink = 0_ms;
		self->solid = SOLID_NOT;
		gi.linkentity(self);
	} else {
		self->wait = 0;
		self->think = force_wall_think;
		self->nextthink = level.time + 10_hz;
		self->solid = SOLID_BSP;
		gi.linkentity(self);
		KillBox(self, false);
	}
}

void SP_func_force_wall(gentity_t *ent) {
	gi.setmodel(ent, ent->model);
	SetupForceWallLine(ent);

	if (!ent->style)
		ent->style = 208;

	ent->movetype = MOVETYPE_NONE;
	ent->wait = 1;

	if (ent->spawnflags.has(SPAWNFLAG_FORCEWALL_START_ON)) {
		ent->solid = SOLID_BSP;
		ent->think = force_wall_think;
		ent->nextthink = level.time + 10_hz;
	} else {
		ent->solid = SOLID_NOT;
	}

	ent->use = force_wall_use;
	ent->svflags = SVF_NOCLIENT;

	gi.linkentity(ent);
}

/*QUAKED func_killbox (1 0 0) ? x DEADLY_COOP EXACT_COLLISION x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Kills everything inside when fired, irrespective of protection.
*/
static USE(use_killbox) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	if (self->spawnflags.has(SPAWNFLAG_KILLBOX_DEADLY_COOP))
		level.deadly_kill_box = true;

	self->solid = SOLID_TRIGGER;
	gi.linkentity(self);

	KillBox(self, false, MOD_TELEFRAG, self->spawnflags.has(SPAWNFLAG_KILLBOX_EXACT_COLLISION));

	self->solid = SOLID_NOT;
	gi.linkentity(self);

	level.deadly_kill_box = false;
}

void SP_func_killbox(gentity_t *ent) {
	gi.setmodel(ent, ent->model);
	ent->use = use_killbox;
	ent->svflags = SVF_NOCLIENT;
}

/*QUAKED func_eye (0 1 0) ? x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Camera-like eye that can track entities.
"pathtarget" point to an info_notnull (which gets freed after spawn) to automatically set
the eye_position
"target"/"killtarget"/"delay"/"message" target keys to fire when we first spot a player
"eye_position" manually set the eye position; note that this is in "forward right up" format, relative to
the origin of the brush and using the entity's angles
"radius" default 512, detection radius for entities
"speed" default 45, how fast, in degrees per second, we should move on each axis to reach the target
"vision_cone" default 0.5 for half cone; how wide the cone of vision should be (relative to their initial angles)
"wait" default 0, the amount of time to wait before returning to neutral angles
*/
static THINK(func_eye_think) (gentity_t *self) -> void {
	// find enemy to track
	float closest_dist = 0;
	gentity_t *closest_player = nullptr;

	for (auto player : active_clients()) {
		vec3_t dir = player->s.origin - self->s.origin;
		float dist = dir.normalize();

		if (dir.dot(self->movedir) < self->yaw_speed)
			continue;

		if (dist >= self->splash_radius)
			continue;

		if (!closest_player || dist < closest_dist) {
			closest_player = player;
			closest_dist = dist;
		}
	}

	self->enemy = closest_player;

	// tracking player
	vec3_t wanted_angles;

	vec3_t fwd, rgt, up;
	AngleVectors(self->s.angles, fwd, rgt, up);

	vec3_t eye_pos = self->s.origin;
	eye_pos += fwd * self->move_origin[0];
	eye_pos += rgt * self->move_origin[1];
	eye_pos += up * self->move_origin[2];

	if (self->enemy) {
		if (!(self->spawnflags & SPAWNFLAG_FUNC_EYE_FIRED_TARGETS)) {
			G_UseTargets(self, self->enemy);
			self->spawnflags |= SPAWNFLAG_FUNC_EYE_FIRED_TARGETS;
		}

		vec3_t dir = (self->enemy->s.origin - eye_pos).normalized();
		wanted_angles = vectoangles(dir);

		self->s.frame = 2;
		self->timestamp = level.time + gtime_t::from_sec(self->wait);
	} else {
		if (self->timestamp <= level.time) {
			// return to neutral
			wanted_angles = self->move_angles;
			self->s.frame = 0;
		} else {
			wanted_angles = self->s.angles;
		}
	}

	for (int i = 0; i < 2; i++) {
		float current = anglemod(self->s.angles[i]);
		float ideal = wanted_angles[i];

		if (current == ideal)
			continue;

		float move = ideal - current;

		if (ideal > current) {
			if (move >= 180)
				move = move - 360;
		} else {
			if (move <= -180)
				move = move + 360;
		}

		if (move > 0) {
			if (move > self->speed)
				move = self->speed;
		} else {
			if (move < -self->speed)
				move = -self->speed;
		}

		self->s.angles[i] = anglemod(current + move);
	}

	self->nextthink = level.time + FRAME_TIME_S;
}

static THINK(func_eye_setup) (gentity_t *self) -> void {
	gentity_t *eye_pos = G_PickTarget(self->pathtarget);

	if (!eye_pos)
		gi.Com_PrintFmt("{}: bad target\n", *self);
	else
		self->move_origin = eye_pos->s.origin - self->s.origin;

	self->movedir = self->move_origin.normalized();

	self->think = func_eye_think;
	self->nextthink = level.time + 10_hz;
}

void SP_func_eye(gentity_t *ent) {
	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_BSP;
	gi.setmodel(ent, ent->model);

	if (!st.radius)
		ent->splash_radius = 512;
	else
		ent->splash_radius = st.radius;

	if (!ent->speed)
		ent->speed = 45;

	if (!ent->yaw_speed)
		ent->yaw_speed = 0.5f;

	ent->speed *= gi.frame_time_s;
	ent->move_angles = ent->s.angles;

	ent->wait = 1.0f;

	if (ent->pathtarget) {
		ent->think = func_eye_setup;
		ent->nextthink = level.time + 10_hz;
	} else {
		ent->think = func_eye_think;
		ent->nextthink = level.time + 10_hz;

		vec3_t right, up;
		AngleVectors(ent->move_angles, ent->movedir, right, up);

		vec3_t move_origin = ent->move_origin;
		ent->move_origin = ent->movedir * move_origin[0];
		ent->move_origin += right * move_origin[1];
		ent->move_origin += up * move_origin[2];
	}

	gi.linkentity(ent);
}

/*QUAKED rotating_light (0 .5 .8) (-8 -8 -8) (8 8 8) START_OFF ALARM x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Rotating dynamic spot light.
"health"	if set, the light may be killed.
*/
static THINK(rotating_light_alarm) (gentity_t *self) -> void {
	if (self->spawnflags.has(SPAWNFLAG_ROTATING_LIGHT_START_OFF)) {
		self->think = nullptr;
		self->nextthink = 0_ms;
	} else {
		gi.sound(self, CHAN_NO_PHS_ADD | CHAN_VOICE, self->moveinfo.sound_start, 1, ATTN_STATIC, 0);
		self->nextthink = level.time + 1_sec;
	}
}

static DIE(rotating_light_killed) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod) -> void {
	EmitWeldingSparks(self, 30);

	self->s.effects &= ~EF_SPINNINGLIGHTS;
	self->use = nullptr;

	self->think = G_FreeEntity;
	self->nextthink = level.time + FRAME_TIME_S;
}

static USE(rotating_light_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	if (self->spawnflags.has(SPAWNFLAG_ROTATING_LIGHT_START_OFF)) {
		self->spawnflags &= ~SPAWNFLAG_ROTATING_LIGHT_START_OFF;
		self->s.effects |= EF_SPINNINGLIGHTS;

		if (self->spawnflags.has(SPAWNFLAG_ROTATING_LIGHT_ALARM)) {
			self->think = rotating_light_alarm;
			self->nextthink = level.time + FRAME_TIME_S;
		}
	} else {
		self->spawnflags |= SPAWNFLAG_ROTATING_LIGHT_START_OFF;
		self->s.effects &= ~EF_SPINNINGLIGHTS;
	}
}

void SP_rotating_light(gentity_t *self) {
	self->movetype = MOVETYPE_STOP;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex("models/objects/light/tris.md2");
	self->s.frame = 0;
	self->use = rotating_light_use;

	if (self->spawnflags.has(SPAWNFLAG_ROTATING_LIGHT_START_OFF))
		self->s.effects &= ~EF_SPINNINGLIGHTS;
	else
		self->s.effects |= EF_SPINNINGLIGHTS;

	if (!self->speed)
		self->speed = 32;

	if (!self->health)
		self->health = 10;

	self->max_health = self->health;
	self->die = rotating_light_killed;
	self->takedamage = true;

	if (self->spawnflags.has(SPAWNFLAG_ROTATING_LIGHT_ALARM))
		self->moveinfo.sound_start = gi.soundindex("misc/alarm.wav");

	gi.linkentity(self);
}

/*QUAKED func_object_repair (1 .5 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
An object to be repaired.
The default delay is 1 second
"delay" the delay in seconds for spark to occur
*/
static THINK(object_repair_fx) (gentity_t *ent) -> void {
	ent->nextthink = level.time + gtime_t::from_sec(ent->delay);

	if (ent->health <= 100)
		ent->health++;
	else
		EmitWeldingSparks(ent, 10);
}

static THINK(object_repair_dead) (gentity_t *ent) -> void {
	G_UseTargets(ent, ent);
	ent->nextthink = level.time + 10_hz;
	ent->think = object_repair_fx;
}

static THINK(object_repair_sparks) (gentity_t *ent) -> void {
	if (ent->health <= 0) {
		ent->nextthink = level.time + 10_hz;
		ent->think = object_repair_dead;
		return;
	}

	ent->nextthink = level.time + gtime_t::from_sec(ent->delay);
	EmitWeldingSparks(ent, 10);
}

void SP_object_repair(gentity_t *ent) {
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_BBOX;
	ent->classname = "object_repair";
	ent->mins = { -8, -8, 8 };
	ent->maxs = { 8, 8, 8 };
	ent->think = object_repair_sparks;
	ent->nextthink = level.time + 1_sec;
	ent->health = 100;

	if (!ent->delay)
		ent->delay = 1.0;
}

/*
===============================================================================

BOBBING

===============================================================================
*/


/*QUAKED func_bobbing (0 .5 .8) ? X_AXIS Y_AXIS
Normally bobs on the Z axis
"model2"	.md3 model to also draw
"height"	amplitude of bob (32 default)
"speed"		seconds to complete a bob cycle (4 default)
"phase"		the 0.0 to 1.0 offset in the cycle to start at
"dmg"		damage to inflict when blocked (2 default)
"color"		constantLight color
"light"		constantLight radius
*/
void SP_func_bobbing(gentity_t *ent) {
	const float cycle_seconds = ent->speed > 0.0f ? ent->speed : DEFAULT_BOBBING_CYCLE_SECONDS;
	const float height = st.height != 0.0f ? st.height : DEFAULT_BOBBING_HEIGHT;
	const float phase = st.phase;

	if (!ent->dmg)
		ent->dmg = 2;
	ent->speed = cycle_seconds;

	ent->pos1 = ent->s.origin;
	ent->pos2 = {};
	ent->move_angles = ent->s.angles;
	ent->movedir = {};

	if (ent->spawnflags.has(SPAWNFLAG_BOBBING_X_AXIS))
		ent->pos2[0] = height;
	else if (ent->spawnflags.has(SPAWNFLAG_BOBBING_Y_AXIS))
		ent->pos2[1] = height;
	else
		ent->pos2[2] = height;

	gi.setmodel(ent, ent->model);
	SetupSineMover(ent, cycle_seconds, phase);
}

/*
===============================================================================

PENDULUM

===============================================================================
*/


/*QUAKED func_pendulum (0 .5 .8) ?
You need to have an origin brush as part of this entity.
Pendulums always swing north / south on unrotated models.  Add an angles field to the model to allow rotation in other directions.
Pendulum frequency is a physical constant based on the length of the beam and gravity.
"model2"	.md3 model to also draw
"speed"		the number of degrees each way the pendulum swings, (30 default)
"phase"		the 0.0 to 1.0 offset in the cycle to start at
"dmg"		damage to inflict when blocked (2 default)
"color"		constantLight color
"light"		constantLight radius
*/
void SP_func_pendulum(gentity_t *ent) {
	const float swing_degrees = ent->speed != 0.0f ? ent->speed : DEFAULT_PENDULUM_SWING_DEGREES;

	if (!ent->dmg)
		ent->dmg = 2;
	ent->speed = swing_degrees;

	gi.setmodel(ent, ent->model);

	float length = fabsf(ent->mins[2]);
	if (length < 8)
		length = MIN_PENDULUM_LENGTH;

	const float gravity = max(g_gravity->value, 1.0f);
	const float freq = (1.0f / (PIf * 2.0f)) * sqrtf(gravity / (3.0f * length));
	const float cycle_seconds = 1.0f / freq;

	ent->pos1 = ent->s.origin;
	ent->pos2 = {};
	ent->move_angles = ent->s.angles;
	ent->movedir = { 0.0f, 0.0f, swing_degrees };

	SetupSineMover(ent, cycle_seconds, st.phase);
}
