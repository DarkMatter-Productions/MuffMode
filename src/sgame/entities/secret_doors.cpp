// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Secret door brush entities.
#include "g_local.h"

void door_secret_move1(gentity_t *self);
void door_secret_move2(gentity_t *self);
void door_secret_move3(gentity_t *self);
void door_secret_move4(gentity_t *self);
void door_secret_move5(gentity_t *self);
void door_secret_move6(gentity_t *self);
void door_secret_done(gentity_t *self);

void door_secret2_move1(gentity_t *self);
void door_secret2_move2(gentity_t *self);
void door_secret2_move3(gentity_t *self);
void door_secret2_move4(gentity_t *self);
void door_secret2_move5(gentity_t *self);
static void door_secret2_move6(gentity_t *self);
void door_secret2_done(gentity_t *self);

namespace {

constexpr spawnflags_t SPAWNFLAG_SECRET_ALWAYS_SHOOT = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAG_SECRET_1ST_LEFT = 2_spawnflag;
constexpr spawnflags_t SPAWNFLAG_SECRET_1ST_DOWN = 4_spawnflag;

constexpr spawnflags_t SPAWNFLAG_SEC_OPEN_ONCE = 1_spawnflag; // stays open
constexpr spawnflags_t SPAWNFLAG_SEC_1ST_DOWN = 4_spawnflag; // 1st move is down from arrow
constexpr spawnflags_t SPAWNFLAG_SEC_YES_SHOOT = 16_spawnflag; // shootable even if targeted
constexpr spawnflags_t SPAWNFLAG_SEC_MOVE_RIGHT = 32_spawnflag;
constexpr spawnflags_t SPAWNFLAG_SEC_MOVE_FORWARD = 64_spawnflag;

bool SecretDoorCanBeShot(gentity_t *ent) {
	return !ent->targetname || ent->spawnflags.has(SPAWNFLAG_SECRET_ALWAYS_SHOOT);
}

bool SecretDoor2CanBeShot(gentity_t *ent) {
	return !ent->targetname || ent->spawnflags.has(SPAWNFLAG_SEC_YES_SHOOT);
}

bool CrushLooseBlocker(gentity_t *self, gentity_t *other) {
	if ((other->svflags & SVF_MONSTER) || other->client)
		return false;

	// give it a chance to go away on it's own terms (like gibs)
	T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, 100000, 1, DAMAGE_NONE, MOD_CRUSH);

	// if it's still there, nuke it
	if (other && other->inuse && other->solid)
		BecomeExplosion1(other);

	return true;
}

} // namespace

/*
=============================================================================

SECRET DOOR 1

=============================================================================
*/

/*QUAKED func_door_secret (0 .5 .8) ? ALWAYS_SHOOT 1ST_LEFT 1ST_DOWN x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
A secret door.  Slide back and then to the side.

ALWAYS_SHOOT	door is shootable even if targeted
1ST_LEFT		1st move is left of arrow
1ST_DOWN		1st move is down from arrow
OPEN_ONCE		doors never closes

"angle"		determines the direction
"dmg"		damage to inflict when blocked (default 2)
"wait"		how long to hold in the open position (default 5, -1 means hold)
*/
static USE(door_secret_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	// make sure we're not already moving
	if (self->s.origin)
		return;

	Move_Calc(self, self->pos1, door_secret_move1);
	door_use_areaportals(self, true);
}

MOVEINFO_ENDFUNC(door_secret_move1) (gentity_t *self) -> void {
	self->nextthink = level.time + 1_sec;
	self->think = door_secret_move2;
}

THINK(door_secret_move2) (gentity_t *self) -> void {
	Move_Calc(self, self->pos2, door_secret_move3);
}

MOVEINFO_ENDFUNC(door_secret_move3) (gentity_t *self) -> void {
	if (self->wait == -1)
		return;

	self->nextthink = level.time + gtime_t::from_sec(self->wait);
	self->think = door_secret_move4;
}

THINK(door_secret_move4) (gentity_t *self) -> void {
	Move_Calc(self, self->pos1, door_secret_move5);
}

MOVEINFO_ENDFUNC(door_secret_move5) (gentity_t *self) -> void {
	self->nextthink = level.time + 1_sec;
	self->think = door_secret_move6;
}

THINK(door_secret_move6) (gentity_t *self) -> void {
	Move_Calc(self, vec3_origin, door_secret_done);
}

MOVEINFO_ENDFUNC(door_secret_done) (gentity_t *self) -> void {
	if (SecretDoorCanBeShot(self)) {
		self->health = 0;
		self->takedamage = true;
	}

	door_use_areaportals(self, false);
}

MOVEINFO_BLOCKED(door_secret_blocked) (gentity_t *self, gentity_t *other) -> void {
	if (CrushLooseBlocker(self, other))
		return;

	if (level.time < self->touch_debounce_time)
		return;

	self->touch_debounce_time = level.time + 500_ms;

	T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 1, DAMAGE_NONE, MOD_CRUSH);
}

static DIE(door_secret_die) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod) -> void {
	self->takedamage = false;
	door_secret_use(self, attacker, attacker);
}

void SP_func_door_secret(gentity_t *ent) {
	G_SetMoveinfoSounds(ent, "doors/dr1_strt.wav", "doors/dr1_mid.wav", "doors/dr1_end.wav");

	ent->attenuation = ATTN_STATIC;

	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_BSP;
	ent->svflags |= SVF_DOOR;
	gi.setmodel(ent, ent->model);

	ent->moveinfo.blocked = door_secret_blocked;
	ent->use = door_secret_use;

	if (SecretDoorCanBeShot(ent)) {
		ent->health = 0;
		ent->takedamage = true;
		ent->die = door_secret_die;
	}

	if (!ent->dmg)
		ent->dmg = 2;

	if (!ent->wait)
		ent->wait = 5;

	ent->moveinfo.accel = ent->moveinfo.decel = ent->moveinfo.speed = 50;

	// calculate positions
	vec3_t forward, right, up;
	AngleVectors(ent->s.angles, forward, right, up);
	ent->s.angles = {};

	const float side = 1.0f - (ent->spawnflags.has(SPAWNFLAG_SECRET_1ST_LEFT) ? 2.0f : 0.0f);
	const float width = ent->spawnflags.has(SPAWNFLAG_SECRET_1ST_DOWN) ? fabsf(up.dot(ent->size)) : fabsf(right.dot(ent->size));
	const float length = fabsf(forward.dot(ent->size));

	if (ent->spawnflags.has(SPAWNFLAG_SECRET_1ST_DOWN))
		ent->pos1 = ent->s.origin + (up * (-1.0f * width));
	else
		ent->pos1 = ent->s.origin + (right * (side * width));

	ent->pos2 = ent->pos1 + (forward * length);

	if (ent->health) {
		ent->takedamage = true;
		ent->die = door_killed;
		ent->max_health = ent->health;
	} else if (ent->targetname && ent->message) {
		gi.soundindex("misc/talk.wav");
		ent->touch = door_touch;
	}

	gi.linkentity(ent);
}

/*
=============================================================================

SECRET DOOR 2

=============================================================================
*/

/*QUAKED func_door_secret2 (0 .5 .8) ? OPEN_ONCE x 1ST_DOWN x ALWAYS_SHOOT SLIDE_RIGHT SLIDE_FORWARD x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Basic secret door. Slides back, then to the left. Angle determines direction.

FLAGS:
OPEN_ONCE = not implemented yet
1ST_DOWN = 1st move is forwards/backwards
ALWAYS_SHOOT = even if targeted, keep shootable
SLIDE_RIGHT = the sideways move will be to right of arrow
SLIDE_FORWARD = the to/fro move will be forward

"angle"		determines the direction
"dmg"		damage to inflict when blocked (default 2)
"wait"		how long to hold in the open position (default 5, -1 means hold)
*/
static USE(door_secret2_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	if (self->flags & FL_TEAMSLAVE)
		return;

	// trigger all paired doors
	for (gentity_t *ent = self; ent; ent = ent->teamchain)
		Move_Calc(ent, ent->moveinfo.start_origin, door_secret2_move1);
}

static DIE(door_secret2_killed) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod) -> void {
	self->health = self->max_health;
	self->takedamage = false;

	if (self->flags & FL_TEAMSLAVE && self->teammaster && self->teammaster->takedamage != false)
		door_secret2_killed(self->teammaster, inflictor, attacker, damage, point, mod);
	else
		door_secret2_use(self, inflictor, attacker);
}

// Wait after first movement...
MOVEINFO_ENDFUNC(door_secret2_move1) (gentity_t *self) -> void {
	self->nextthink = level.time + 1_sec;
	self->think = door_secret2_move2;
}

// Start moving sideways w/sound...
THINK(door_secret2_move2) (gentity_t *self) -> void {
	Move_Calc(self, self->moveinfo.end_origin, door_secret2_move3);
}

// Wait here until time to go back...
MOVEINFO_ENDFUNC(door_secret2_move3) (gentity_t *self) -> void {
	if (!self->spawnflags.has(SPAWNFLAG_SEC_OPEN_ONCE)) {
		self->nextthink = level.time + gtime_t::from_sec(self->wait);
		self->think = door_secret2_move4;
	}
}

// Move backward...
THINK(door_secret2_move4) (gentity_t *self) -> void {
	Move_Calc(self, self->moveinfo.start_origin, door_secret2_move5);
}

// Wait 1 second...
MOVEINFO_ENDFUNC(door_secret2_move5) (gentity_t *self) -> void {
	self->nextthink = level.time + 1_sec;
	self->think = door_secret2_move6;
}

static THINK(door_secret2_move6) (gentity_t *self) -> void {
	Move_Calc(self, self->move_origin, door_secret2_done);
}

MOVEINFO_ENDFUNC(door_secret2_done) (gentity_t *self) -> void {
	if (SecretDoor2CanBeShot(self)) {
		self->health = 1;
		self->takedamage = true;
		self->die = door_secret2_killed;
	}
}

MOVEINFO_BLOCKED(door_secret2_blocked) (gentity_t *self, gentity_t *other) -> void {
	if (!(self->flags & FL_TEAMSLAVE))
		T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, 0, DAMAGE_NONE, MOD_CRUSH);
}

static TOUCH(door_secret2_touch) (gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	if (other->health <= 0)
		return;

	if (!(other->client))
		return;

	if (self->monsterinfo.attack_finished > level.time)
		return;

	self->monsterinfo.attack_finished = level.time + 2_sec;

	if (self->message)
		gi.LocCenter_Print(other, self->message);
}

void SP_func_door_secret2(gentity_t *ent) {
	G_SetMoveinfoSounds(ent, "doors/dr1_strt.wav", "doors/dr1_mid.wav", "doors/dr1_end.wav");

	vec3_t forward, right, up;
	AngleVectors(ent->s.angles, forward, right, up);
	ent->move_origin = ent->s.origin;
	ent->move_angles = ent->s.angles;

	G_SetMovedir(ent->s.angles, ent->movedir);
	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_BSP;
	gi.setmodel(ent, ent->model);

	float lr_size = 0.0f;
	float fb_size = 0.0f;

	if (ent->move_angles[1] == 0 || ent->move_angles[1] == 180) {
		lr_size = ent->size[1];
		fb_size = ent->size[0];
	} else if (ent->move_angles[1] == 90 || ent->move_angles[1] == 270) {
		lr_size = ent->size[0];
		fb_size = ent->size[1];
	} else {
		gi.Com_Print("Secret door not at 0,90,180,270!\n");
		G_FreeEntity(ent);
		return;
	}

	if (ent->spawnflags.has(SPAWNFLAG_SEC_MOVE_FORWARD))
		forward *= fb_size;
	else
		forward *= fb_size * -1.0f;

	if (ent->spawnflags.has(SPAWNFLAG_SEC_MOVE_RIGHT))
		right *= lr_size;
	else
		right *= lr_size * -1.0f;

	if (ent->spawnflags.has(SPAWNFLAG_SEC_1ST_DOWN)) {
		ent->moveinfo.start_origin = ent->s.origin + forward;
		ent->moveinfo.end_origin = ent->moveinfo.start_origin + right;
	} else {
		ent->moveinfo.start_origin = ent->s.origin + right;
		ent->moveinfo.end_origin = ent->moveinfo.start_origin + forward;
	}

	ent->touch = door_secret2_touch;
	ent->moveinfo.blocked = door_secret2_blocked;
	ent->use = door_secret2_use;

	if (!ent->dmg)
		ent->dmg = 2;

	if (!ent->wait)
		ent->wait = 5;

	ent->moveinfo.accel = ent->moveinfo.decel = ent->moveinfo.speed = 50;

	if (SecretDoor2CanBeShot(ent)) {
		ent->health = 1;
		ent->max_health = ent->health;
		ent->takedamage = true;
		ent->die = door_secret2_killed;
	}

	gi.linkentity(ent);
}
