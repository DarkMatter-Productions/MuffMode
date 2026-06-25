// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Miscellaneous brush/function entities.

#include "brush_misc.h"

namespace {

constexpr spawnflags_t SPAWNFLAG_WALL_TRIGGER_SPAWN = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAG_WALL_TOGGLE = 2_spawnflag;
constexpr spawnflags_t SPAWNFLAG_WALL_START_ON = 4_spawnflag;
constexpr spawnflags_t SPAWNFLAG_WALL_ANIMATED = 8_spawnflag;
constexpr spawnflags_t SPAWNFLAG_WALL_ANIMATED_FAST = 16_spawnflag;

constexpr spawnflags_t SPAWNFLAG_ANIMATION_START_ON = 1_spawnflag;

constexpr spawnflags_t SPAWNFLAGS_OBJECT_TRIGGER_SPAWN = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAGS_OBJECT_ANIMATED = 2_spawnflag;
constexpr spawnflags_t SPAWNFLAGS_OBJECT_ANIMATED_FAST = 4_spawnflag;

constexpr spawnflags_t SPAWNFLAGS_EXPLOSIVE_TRIGGER_SPAWN = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAGS_EXPLOSIVE_ANIMATED = 2_spawnflag;
constexpr spawnflags_t SPAWNFLAGS_EXPLOSIVE_ANIMATED_FAST = 4_spawnflag;
constexpr spawnflags_t SPAWNFLAGS_EXPLOSIVE_INACTIVE = 8_spawnflag;
constexpr spawnflags_t SPAWNFLAGS_EXPLOSIVE_ALWAYS_SHOOTABLE = 16_spawnflag;

void ApplyAnimationEffects(gentity_t *self, spawnflags_t animated, spawnflags_t animated_fast)
{
	if (self->spawnflags.has(animated))
		self->s.effects |= EF_ANIM_ALL;
	if (self->spawnflags.has(animated_fast))
		self->s.effects |= EF_ANIM_ALLFAST;
}

} // namespace

/*QUAKED func_wall (0 .5 .8) ? TRIGGER_SPAWN TOGGLE START_ON ANIMATED ANIMATED_FAST x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This is just a solid wall if not inhibited

TRIGGER_SPAWN	the wall will not be present until triggered
				it will then blink in to existance; it will
				kill anything that was in it's way

TOGGLE			only valid for TRIGGER_SPAWN walls
				this allows the wall to be turned on and off

START_ON		only valid for TRIGGER_SPAWN walls
				the wall will initially be present
*/
static USE(func_wall_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	if (self->solid == SOLID_NOT) {
		self->solid = SOLID_BSP;
		self->svflags &= ~SVF_NOCLIENT;
		gi.linkentity(self);
		KillBox(self, false);
	} else {
		self->solid = SOLID_NOT;
		self->svflags |= SVF_NOCLIENT;
		gi.linkentity(self);
	}

	if (!self->spawnflags.has(SPAWNFLAG_WALL_TOGGLE))
		self->use = nullptr;
}

void SP_func_wall(gentity_t *self) {
	self->movetype = MOVETYPE_PUSH;
	gi.setmodel(self, self->model);

	ApplyAnimationEffects(self, SPAWNFLAG_WALL_ANIMATED, SPAWNFLAG_WALL_ANIMATED_FAST);

	// just a wall
	if (!self->spawnflags.has(SPAWNFLAG_WALL_TRIGGER_SPAWN | SPAWNFLAG_WALL_TOGGLE | SPAWNFLAG_WALL_START_ON)) {
		self->solid = SOLID_BSP;
		gi.linkentity(self);
		return;
	}

	// it must be TRIGGER_SPAWN
	if (!(self->spawnflags & SPAWNFLAG_WALL_TRIGGER_SPAWN))
		self->spawnflags |= SPAWNFLAG_WALL_TRIGGER_SPAWN;

	// yell if the spawnflags are odd
	if (self->spawnflags.has(SPAWNFLAG_WALL_START_ON)) {
		if (!self->spawnflags.has(SPAWNFLAG_WALL_TOGGLE)) {
			gi.Com_Print("func_wall START_ON without TOGGLE\n");
			self->spawnflags |= SPAWNFLAG_WALL_TOGGLE;
		}
	}

	self->use = func_wall_use;
	if (self->spawnflags.has(SPAWNFLAG_WALL_START_ON)) {
		self->solid = SOLID_BSP;
	} else {
		self->solid = SOLID_NOT;
		self->svflags |= SVF_NOCLIENT;
	}
	gi.linkentity(self);
}

// [Paril-KEX]
/*QUAKED func_animation (0 .5 .8) ? START_ON x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Similar to func_wall, but triggering it will toggle animation
state rather than going on/off.

START_ON		will start in alterate animation
*/
USE(func_animation_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	self->bmodel_anim.alternate = !self->bmodel_anim.alternate;
}

void SP_func_animation(gentity_t *self) {
	if (!self->bmodel_anim.enabled) {
		gi.Com_PrintFmt("{} has no animation data\n", *self);
		G_FreeEntity(self);
		return;
	}

	self->movetype = MOVETYPE_PUSH;
	gi.setmodel(self, self->model);
	self->solid = SOLID_BSP;

	self->use = func_animation_use;
	self->bmodel_anim.alternate = self->spawnflags.has(SPAWNFLAG_ANIMATION_START_ON);

	if (self->bmodel_anim.alternate)
		self->s.frame = self->bmodel_anim.alt_start;
	else
		self->s.frame = self->bmodel_anim.start;

	gi.linkentity(self);
}

/*QUAKED func_object (0 .5 .8) ? TRIGGER_SPAWN ANIMATED ANIMATED_FAST x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This is solid bmodel that will fall if it's support it removed.
*/
TOUCH(func_object_touch) (gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	// only squash thing we fall on top of
	if (other_touching_self)
		return;
	if (tr.plane.normal[2] < 1.0f)
		return;
	if (other->takedamage == false)
		return;
	if (other->damage_debounce_time > level.time)
		return;
	T_Damage(other, self, self, vec3_origin, closest_point_to_box(other->s.origin, self->absmin, self->absmax), tr.plane.normal, self->dmg, 1, DAMAGE_NONE, MOD_CRUSH);
	other->damage_debounce_time = level.time + 10_hz;
}

THINK(func_object_release) (gentity_t *self) -> void {
	self->movetype = MOVETYPE_TOSS;
	self->touch = func_object_touch;
}

USE(func_object_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	self->solid = SOLID_BSP;
	self->svflags &= ~SVF_NOCLIENT;
	self->use = nullptr;
	func_object_release(self);
	KillBox(self, false);
}

void SP_func_object(gentity_t *self) {
	gi.setmodel(self, self->model);

	self->mins[0] += 1;
	self->mins[1] += 1;
	self->mins[2] += 1;
	self->maxs[0] -= 1;
	self->maxs[1] -= 1;
	self->maxs[2] -= 1;

	if (!self->dmg)
		self->dmg = 100;

	if (!(self->spawnflags & SPAWNFLAGS_OBJECT_TRIGGER_SPAWN)) {
		self->solid = SOLID_BSP;
		self->movetype = MOVETYPE_PUSH;
		self->think = func_object_release;
		self->nextthink = level.time + 20_hz;
	} else {
		self->solid = SOLID_NOT;
		self->movetype = MOVETYPE_PUSH;
		self->use = func_object_use;
		self->svflags |= SVF_NOCLIENT;
	}

	ApplyAnimationEffects(self, SPAWNFLAGS_OBJECT_ANIMATED, SPAWNFLAGS_OBJECT_ANIMATED_FAST);

	self->clipmask = MASK_MONSTERSOLID;
	self->flags |= FL_NO_STANDING;

	gi.linkentity(self);
}

/*QUAKED func_explosive (0 .5 .8) ? TRIGGER_SPAWN ANIMATED ANIMATED_FAST INACTIVE ALWAYS_SHOOTABLE x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Any brush that you want to explode or break apart.  If you want an
ex0plosion, set dmg and it will do a radius explosion of that amount
at the center of the bursh.

If targeted it will not be shootable.

INACTIVE - specifies that the entity is not explodable until triggered. If you use this you must
target the entity you want to trigger it. This is the only entity approved to activate it.

health defaults to 100.

mass defaults to 75.  This determines how much debris is emitted when
it explodes.  You get one large chunk per 100 of mass (up to 8) and
one small chunk per 25 of mass (up to 16).  So 800 gives the most.
*/
static DIE(func_explosive_explode) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod) -> void {
	size_t   count;
	int		 mass;
	gentity_t *master;
	bool	 done = false;

	self->takedamage = false;

	if (self->dmg)
		T_RadiusDamage(self, attacker, (float)self->dmg, nullptr, (float)(self->dmg + 40), DAMAGE_NONE, MOD_EXPLOSIVE);

	self->velocity = inflictor->s.origin - self->s.origin;
	self->velocity.normalize();
	self->velocity *= 150;

	mass = self->mass;
	if (!mass)
		mass = 75;

	// big chunks
	if (mass >= 100) {
		count = mass / 100;
		if (count > 8)
			count = 8;
		ThrowGibs(self, 1, {
			{ count, "models/objects/debris1/tris.md2", GIB_METALLIC | GIB_DEBRIS }
			});
	}

	// small chunks
	count = mass / 25;
	if (count > 16)
		count = 16;
	ThrowGibs(self, 2, {
		{ count, "models/objects/debris2/tris.md2", GIB_METALLIC | GIB_DEBRIS }
		});

	// PMM - if we're part of a train, clean ourselves out of it
	if (self->flags & FL_TEAMSLAVE) {
		if (self->teammaster) {
			master = self->teammaster;
			if (master && master->inuse) // because mappers (other than jim (usually)) are stupid....
			{
				while (!done) {
					if (master->teamchain == self) {
						master->teamchain = self->teamchain;
						done = true;
					}
					master = master->teamchain;
				}
			}
		}
	}

	G_UseTargets(self, attacker);

	self->s.origin = (self->absmin + self->absmax) * 0.5f;

	if (self->noise_index)
		gi.positioned_sound(self->s.origin, self, CHAN_AUTO, self->noise_index, 1, ATTN_NORM, 0);

	if (self->dmg)
		BecomeExplosion1(self);
	else
		G_FreeEntity(self);
}

static USE(func_explosive_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	// Paril: pass activator to explode as attacker. this fixes
	// "strike" trying to centerprint to the relay. Should be
	// a safe change.
	func_explosive_explode(self, self, activator, self->health, vec3_origin, MOD_EXPLOSIVE);
}

static USE(func_explosive_activate) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	int approved;

	approved = 0;
	// PMM - looked like target and targetname were flipped here
	if (other != nullptr && other->target) {
		if (!strcmp(other->target, self->targetname))
			approved = 1;
	}
	if (!approved && activator != nullptr && activator->target) {
		if (!strcmp(activator->target, self->targetname))
			approved = 1;
	}

	if (!approved)
		return;

	self->use = func_explosive_use;
	if (!self->health)
		self->health = 100;
	self->die = func_explosive_explode;
	self->takedamage = true;
}

static USE(func_explosive_spawn) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	self->solid = SOLID_BSP;
	self->svflags &= ~SVF_NOCLIENT;
	self->use = nullptr;
	gi.linkentity(self);
	KillBox(self, false);
}

void SP_func_explosive(gentity_t *self) {
	/*
	if (deathmatch->integer)
	{ // auto-remove for deathmatch
		G_FreeEntity(self);
		return;
	}
	*/
	self->movetype = MOVETYPE_PUSH;

	gi.modelindex("models/objects/debris1/tris.md2");
	gi.modelindex("models/objects/debris2/tris.md2");

	gi.setmodel(self, self->model);

	if (self->spawnflags.has(SPAWNFLAGS_EXPLOSIVE_TRIGGER_SPAWN)) {
		self->svflags |= SVF_NOCLIENT;
		self->solid = SOLID_NOT;
		self->use = func_explosive_spawn;
	} else if (self->spawnflags.has(SPAWNFLAGS_EXPLOSIVE_INACTIVE)) {
		self->solid = SOLID_BSP;
		if (self->targetname)
			self->use = func_explosive_activate;
	} else {
		self->solid = SOLID_BSP;
		if (self->targetname)
			self->use = func_explosive_use;
	}

	ApplyAnimationEffects(self, SPAWNFLAGS_EXPLOSIVE_ANIMATED, SPAWNFLAGS_EXPLOSIVE_ANIMATED_FAST);

	if (self->spawnflags.has(SPAWNFLAGS_EXPLOSIVE_ALWAYS_SHOOTABLE) || ((self->use != func_explosive_use) && (self->use != func_explosive_activate))) {
		if (!self->health)
			self->health = 100;
		self->die = func_explosive_explode;
		self->takedamage = true;
	}

	if (self->sounds) {
		if (self->sounds == 1)
			self->noise_index = gi.soundindex("world/brkglas.wav");
		else
			gi.Com_PrintFmt("{}: invalid \"sounds\" {}\n", *self, self->sounds);
	}

	gi.linkentity(self);
}
