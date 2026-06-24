// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Scripted ship, flyby, missile and nuke misc entities.
#include "g_local.h"

namespace {

constexpr float DEFAULT_FLYBY_SPEED = 300.0f;

using misc_vehicle_use_t = void (*)(gentity_t *self, gentity_t *other, gentity_t *activator);

bool RequireVehicleTarget(gentity_t *ent, bool colon_message) {
	if (ent->target)
		return true;

	if (colon_message)
		gi.Com_PrintFmt("{}: no target\n", *ent);
	else
		gi.Com_PrintFmt("{} without a target\n", *ent);

	G_FreeEntity(ent);
	return false;
}

void SetupTrainVehicle(gentity_t *ent, const char *model, const vec3_t &mins, const vec3_t &maxs, misc_vehicle_use_t use, bool start_on = false) {
	if (!ent->speed)
		ent->speed = DEFAULT_FLYBY_SPEED;

	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_NOT;
	ent->s.modelindex = gi.modelindex(model);
	ent->mins = mins;
	ent->maxs = maxs;

	ent->think = func_train_find;
	ent->nextthink = level.time + 10_hz;
	ent->use = use;
	ent->svflags |= SVF_NOCLIENT;
	ent->moveinfo.accel = ent->moveinfo.decel = ent->moveinfo.speed = ent->speed;

	if (start_on && !(ent->spawnflags & SPAWNFLAG_TRAIN_START_ON))
		ent->spawnflags |= SPAWNFLAG_TRAIN_START_ON;

	gi.linkentity(ent);
}

void StartVisibleTrain(gentity_t *self, gentity_t *other, gentity_t *activator) {
	self->svflags &= ~SVF_NOCLIENT;
	self->use = train_use;
	train_use(self, other, activator);
}

} // namespace

/*QUAKED misc_viper (1 .5 0) (-16 -16 0) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This is the Viper for the flyby bombing.
It is trigger_spawned, so you must have something use it for it to show up.
There must be a path for it to follow once it is activated.

"speed"		How fast the Viper should fly
*/
USE(misc_viper_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	StartVisibleTrain(self, other, activator);
}

void SP_misc_viper(gentity_t *ent) {
	if (!RequireVehicleTarget(ent, false))
		return;

	SetupTrainVehicle(ent, "models/ships/viper/tris.md2", { -16, -16, 0 }, { 16, 16, 32 }, misc_viper_use);
}

/*QUAKED misc_bigviper (1 .5 0) (-176 -120 -24) (176 120 72) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This is a large stationary viper as seen in Paul's intro
*/
void SP_misc_bigviper(gentity_t *ent) {
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_BBOX;
	ent->mins = { -176, -120, -24 };
	ent->maxs = { 176, 120, 72 };
	ent->s.modelindex = gi.modelindex("models/ships/bigviper/tris.md2");
	gi.linkentity(ent);
}

/*QUAKED misc_viper_bomb (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
"dmg"	how much boom should the bomb make?
*/
TOUCH(misc_viper_bomb_touch) (gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	G_UseTargets(self, self->activator);

	self->s.origin[2] = self->absmin[2] + 1;
	T_RadiusDamage(self, self, static_cast<float>(self->dmg), nullptr, static_cast<float>(self->dmg + 40), DAMAGE_NONE, MOD_BOMB);
	BecomeExplosion2(self);
}

PRETHINK(misc_viper_bomb_prethink) (gentity_t *self) -> void {
	self->groundentity = nullptr;

	float diff = (self->timestamp - level.time).seconds();
	if (diff < -1.0f)
		diff = -1.0f;

	vec3_t v = self->moveinfo.dir * (1.0f + diff);
	v[2] = diff;

	diff = self->s.angles[ROLL];
	self->s.angles = vectoangles(v);
	self->s.angles[ROLL] = diff + 10;
}

USE(misc_viper_bomb_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	gentity_t *viper = G_FindByString<&gentity_t::classname>(nullptr, "misc_viper");

	if (!viper) {
		gi.Com_PrintFmt("{}: no misc_viper for bomb\n", *self);
		return;
	}

	self->solid = SOLID_BBOX;
	self->svflags &= ~SVF_NOCLIENT;
	self->s.effects |= EF_ROCKET;
	self->use = nullptr;
	self->movetype = MOVETYPE_TOSS;
	self->prethink = misc_viper_bomb_prethink;
	self->touch = misc_viper_bomb_touch;
	self->activator = activator;

	self->velocity = viper->moveinfo.dir * viper->moveinfo.speed;

	self->timestamp = level.time;
	self->moveinfo.dir = viper->moveinfo.dir;
}

void SP_misc_viper_bomb(gentity_t *self) {
	self->movetype = MOVETYPE_NONE;
	self->solid = SOLID_NOT;
	self->mins = { -8, -8, -8 };
	self->maxs = { 8, 8, 8 };

	self->s.modelindex = gi.modelindex("models/objects/bomb/tris.md2");

	if (!self->dmg)
		self->dmg = 1000;

	self->use = misc_viper_bomb_use;
	self->svflags |= SVF_NOCLIENT;

	gi.linkentity(self);
}

/*QUAKED misc_strogg_ship (1 .5 0) (-16 -16 0) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This is a Storgg ship for the flybys.
It is trigger_spawned, so you must have something use it for it to show up.
There must be a path for it to follow once it is activated.

"speed"		How fast it should fly
*/
USE(misc_strogg_ship_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	StartVisibleTrain(self, other, activator);
}

void SP_misc_strogg_ship(gentity_t *ent) {
	if (!RequireVehicleTarget(ent, false))
		return;

	SetupTrainVehicle(ent, "models/ships/strogg1/tris.md2", { -16, -16, 0 }, { 16, 16, 32 }, misc_strogg_ship_use);
}

/*QUAKED misc_satellite_dish (1 .5 0) (-64 -64 0) (64 64 128) x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/objects/satellite/tris.md2"
*/
THINK(misc_satellite_dish_think) (gentity_t *self) -> void {
	self->s.frame++;
	if (self->s.frame < 38)
		self->nextthink = level.time + 10_hz;
}

USE(misc_satellite_dish_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	self->s.frame = 0;
	self->think = misc_satellite_dish_think;
	self->nextthink = level.time + 10_hz;
}

void SP_misc_satellite_dish(gentity_t *ent) {
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_BBOX;
	ent->mins = { -64, -64, 0 };
	ent->maxs = { 64, 64, 128 };
	ent->s.modelindex = gi.modelindex("models/objects/satellite/tris.md2");
	ent->use = misc_satellite_dish_use;
	gi.linkentity(ent);
}

/*QUAKED misc_crashviper (1 .5 0) (-176 -120 -24) (176 120 72) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
A large viper about to crash.
*/
void SP_misc_crashviper(gentity_t *ent) {
	if (!RequireVehicleTarget(ent, true))
		return;

	SetupTrainVehicle(ent, "models/ships/bigviper/tris.md2", { -16, -16, 0 }, { 16, 16, 32 }, misc_viper_use);
}

/*QUAKED misc_viper_missile (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
"dmg"	how much boom should the bomb make? the default value is 250
*/
static USE(misc_viper_missile_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	self->enemy = G_FindByString<&gentity_t::targetname>(nullptr, self->target);

	if (!self->enemy) {
		gi.Com_PrintFmt("{}: no missile target\n", *self);
		return;
	}

	vec3_t start = self->s.origin;
	vec3_t dir = self->enemy->s.origin - start;
	dir.normalize();

	monster_fire_rocket(self, start, dir, self->dmg, 500, MZ2_CHICK_ROCKET_1);

	self->nextthink = level.time + 10_hz;
	self->think = G_FreeEntity;
}

void SP_misc_viper_missile(gentity_t *self) {
	self->movetype = MOVETYPE_NONE;
	self->solid = SOLID_NOT;
	self->mins = { -8, -8, -8 };
	self->maxs = { 8, 8, 8 };

	if (!self->dmg)
		self->dmg = 250;

	self->s.modelindex = gi.modelindex("models/objects/bomb/tris.md2");

	self->use = misc_viper_missile_use;
	self->svflags |= SVF_NOCLIENT;

	gi.linkentity(self);
}

/*QUAKED misc_transport (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Maxx's transport at end of game
*/
void SP_misc_transport(gentity_t *ent) {
	if (!RequireVehicleTarget(ent, true))
		return;

	SetupTrainVehicle(ent, "models/objects/ship/tris.md2", { -16, -16, 0 }, { 16, 16, 32 }, misc_strogg_ship_use, true);
}

/*QUAKED misc_amb4 (1 0 0) (-16 -16 -16) (16 16 16) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Mal's amb4 loop entity
*/
static cached_soundindex amb4sound;

static THINK(amb4_think) (gentity_t *ent) -> void {
	ent->nextthink = level.time + 2.7_sec;
	gi.sound(ent, CHAN_VOICE, amb4sound, 1, ATTN_NONE, 0);
}

void SP_misc_amb4(gentity_t *ent) {
	ent->think = amb4_think;
	ent->nextthink = level.time + 1_sec;
	amb4sound.assign("world/amb4.wav");
	gi.linkentity(ent);
}

/*QUAKED misc_nuke (1 0 0) (-16 -16 -16) (16 16 16) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
static THINK(misc_nuke_think) (gentity_t *self) -> void {
	Nuke_Explode(self);
}

static USE(misc_nuke_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	gentity_t *nuke = G_Spawn();
	nuke->s.origin = self->s.origin;
	nuke->clipmask = MASK_PROJECTILE;
	nuke->solid = SOLID_NOT;
	nuke->mins = { -1, -1, 1 };
	nuke->maxs = { 1, 1, 1 };
	nuke->owner = self;
	nuke->teammaster = self;
	nuke->nextthink = level.time + FRAME_TIME_S;
	nuke->dmg = 800;
	nuke->splash_radius = 8192;
	nuke->think = misc_nuke_think;
}

void SP_misc_nuke(gentity_t *ent) {
	ent->use = misc_nuke_use;
}

/*QUAKED misc_nuke_core (1 0 0) (-16 -16 -16) (16 16 16) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Toggles visible/not visible. Starts visible.
*/
static USE(misc_nuke_core_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	if (self->svflags & SVF_NOCLIENT)
		self->svflags &= ~SVF_NOCLIENT;
	else
		self->svflags |= SVF_NOCLIENT;
}

void SP_misc_nuke_core(gentity_t *ent) {
	gi.setmodel(ent, "models/objects/core/tris.md2");
	gi.linkentity(ent);

	ent->use = misc_nuke_core_use;
}
