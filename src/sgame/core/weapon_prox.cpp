// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Proximity mine projectile behavior.
#include "g_local.h"
#include "muffmode/mm_arena.h"

namespace {

constexpr gtime_t PROX_TIME_TO_LIVE = 45_sec;
constexpr gtime_t PROX_TIME_DELAY = 500_ms;
constexpr float PROX_BOUND_SIZE = 96.0f;
constexpr float PROX_DAMAGE_RADIUS = 192.0f;
constexpr int32_t PROX_HEALTH = 20;
constexpr int32_t PROX_DAMAGE = 90;
constexpr float PROX_STOP_EPSILON = 0.1f;

gtime_t ProxLifetimeForMultiplier(int32_t multiplier) {
	switch (multiplier) {
	case 2:
		return 30_sec;
	case 4:
		return 15_sec;
	case 8:
		return 10_sec;
	default:
		return PROX_TIME_TO_LIVE;
	}
}

} // namespace

static THINK(Prox_Explode) (gentity_t *ent) -> void {
	// PMM - changed teammaster to "mover" .. owner of the field is the prox
	if (ent->teamchain && ent->teamchain->owner == ent)
		G_FreeEntity(ent->teamchain);

	gentity_t *owner = ent;

	if (ent->teammaster) {
		owner = ent->teammaster;
		PlayerNoise(owner, ent->s.origin, PNOISE_IMPACT);
	}

	// play quad sound if appropriate
	if (ent->dmg > PROX_DAMAGE)
		gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage3.wav"), 1, ATTN_NORM, 0);

	ent->takedamage = false;
	T_RadiusDamage(ent, owner, static_cast<float>(ent->dmg), ent, PROX_DAMAGE_RADIUS, DAMAGE_NONE, MOD_PROX);

	const vec3_t origin = ent->s.origin + (ent->velocity * -0.02f);
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(ent->groundentity ? TE_GRENADE_EXPLOSION : TE_ROCKET_EXPLOSION);
	gi.WritePosition(origin);
	gi.multicast(ent->s.origin, MULTICAST_PHS, false);

	G_FreeEntity(ent);
}

static DIE(prox_die) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod) -> void {
	self->takedamage = false;

	// if set off by another prox, delay a little (chained explosions)
	if (strcmp(inflictor->classname, "prox_mine")) {
		Prox_Explode(self);
	} else {
		self->think = Prox_Explode;
		self->nextthink = level.time + FRAME_TIME_S;
	}
}

static TOUCH(Prox_Field_Touch) (gentity_t *ent, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	if (deathmatch->integer && notGT(GT_ARENA) && IsCombatDisabled())
		return;

	if (!(other->svflags & SVF_MONSTER) && !other->client)
		return;

	// trigger the prox mine if it's still there, and still mine.
	gentity_t *prox = ent->owner;

	if (GT(GT_ARENA) && !MM_Arena_CanInteract(prox, other))
		return;

	// teammate avoidance
	if (CheckTeamDamage(prox->teammaster, other))
		return;

	if (!deathmatch->integer && other->client)
		return;

	if (other == prox) // don't set self off
		return;

	if (prox->think == Prox_Explode) // we're set to blow!
		return;

	if (prox->teamchain == ent) {
		gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/proxwarn.wav"), 1, ATTN_NORM, 0);
		prox->think = Prox_Explode;
		prox->nextthink = level.time + PROX_TIME_DELAY;
		return;
	}

	ent->solid = SOLID_NOT;
	G_FreeEntity(ent);
}

static THINK(prox_seek) (gentity_t *ent) -> void {
	if (level.time > gtime_t::from_sec(ent->wait)) {
		Prox_Explode(ent);
	} else {
		ent->s.frame++;

		if (ent->s.frame > 13)
			ent->s.frame = 9;

		ent->think = prox_seek;
		ent->nextthink = level.time + 10_hz;
	}
}

static THINK(prox_open) (gentity_t *ent) -> void {
	if (ent->s.frame == 9) {
		// set the owner to nullptr so the owner can walk through it.
		ent->s.sound = 0;

		if (deathmatch->integer)
			ent->owner = nullptr;

		if (ent->teamchain)
			ent->teamchain->touch = Prox_Field_Touch;

		gentity_t *search = nullptr;
		while ((search = findradius(search, ent->s.origin, PROX_DAMAGE_RADIUS + 10.0f)) != nullptr) {
			if (GT(GT_ARENA) && !MM_Arena_CanInteract(ent, search))
				continue;

			if (!search->classname) // tag token and other weird stuff
				continue;

			// teammate avoidance
			if (CheckTeamDamage(search, ent->teammaster))
				continue;

			// if it's a monster or player with health > 0,
			// or it's a player start point, and we can see it, blow up.
			if (search != ent &&
				((((search->svflags & SVF_MONSTER) ||
					(deathmatch->integer && (search->client || (search->classname && !strcmp(search->classname, "prox_mine"))))) &&
					(search->health > 0)) ||
					(deathmatch->integer &&
						((!strncmp(search->classname, "info_player_", 12)) ||
							(!strcmp(search->classname, "misc_teleporter_dest")) ||
							(!strncmp(search->classname, "item_flag_", 10))))) &&
				visible(search, ent)) {
				gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/proxwarn.wav"), 1, ATTN_NORM, 0);
				Prox_Explode(ent);
				return;
			}
		}

		const gtime_t lifetime = g_dm_strong_mines->integer ? PROX_TIME_TO_LIVE : ProxLifetimeForMultiplier(ent->dmg / PROX_DAMAGE);
		ent->wait = (level.time + lifetime).seconds();
		ent->think = prox_seek;
		ent->nextthink = level.time + 200_ms;
	} else {
		if (ent->s.frame == 0)
			gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/proxopen.wav"), 1, ATTN_NORM, 0);

		ent->s.frame++;
		ent->think = prox_open;
		ent->nextthink = level.time + 10_hz;
	}
}

static TOUCH(prox_land) (gentity_t *ent, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	if (tr.surface && (tr.surface->flags & SURF_SKY)) {
		G_FreeEntity(ent);
		return;
	}

	if (tr.plane.normal) {
		const vec3_t land_point = ent->s.origin + (tr.plane.normal * -10.0f);
		if (gi.pointcontents(land_point) & (CONTENTS_SLIME | CONTENTS_LAVA)) {
			Prox_Explode(ent);
			return;
		}
	}

	movetype_t movetype = MOVETYPE_NONE;

	if (!tr.plane.normal || (other->svflags & SVF_MONSTER) || other->client || (other->flags & FL_DAMAGEABLE)) {
		if (other != ent->teammaster)
			Prox_Explode(ent);

		return;
	} else if (other != world) {
		// Here we need to check to see if we can stop on this entity.
		const bool stick_ok = (other->movetype == MOVETYPE_PUSH) && (tr.plane.normal[2] > 0.7f);

		// PMM - code stolen from sgame/core/physics.cpp (ClipVelocity)
		vec3_t out;
		const float backoff = ent->velocity.dot(tr.plane.normal) * 1.5f;

		for (int32_t i = 0; i < 3; i++) {
			const float change = tr.plane.normal[i] * backoff;
			out[i] = ent->velocity[i] - change;

			if (out[i] > -PROX_STOP_EPSILON && out[i] < PROX_STOP_EPSILON)
				out[i] = 0.0f;
		}

		if (out[2] > 60)
			return;

		movetype = MOVETYPE_BOUNCE;

		// if we're here, we're going to stop on an entity
		if (stick_ok) {
			ent->velocity = {};
			ent->avelocity = {};
		} else {
			if (tr.plane.normal[2] > 0.7f)
				Prox_Explode(ent);

			return;
		}
	} else if (other->s.modelindex != MODELINDEX_WORLD) {
		return;
	}

	vec3_t dir = vectoangles(tr.plane.normal);
	vec3_t forward, right, up;
	AngleVectors(dir, forward, right, up);

	if (gi.pointcontents(ent->s.origin) & (CONTENTS_LAVA | CONTENTS_SLIME)) {
		Prox_Explode(ent);
		return;
	}

	ent->svflags &= ~SVF_PROJECTILE;

	gentity_t *field = G_Spawn();
	field->s.origin = ent->s.origin;
	field->mins = { -PROX_BOUND_SIZE, -PROX_BOUND_SIZE, -PROX_BOUND_SIZE };
	field->maxs = { PROX_BOUND_SIZE, PROX_BOUND_SIZE, PROX_BOUND_SIZE };
	field->movetype = MOVETYPE_NONE;
	field->solid = SOLID_TRIGGER;
	field->owner = ent;
	field->classname = "prox_field";
	field->teammaster = ent;
	gi.linkentity(field);

	ent->velocity = {};
	ent->avelocity = {};

	// rotate to vertical
	dir[PITCH] = dir[PITCH] + 90;
	ent->s.angles = dir;
	ent->takedamage = true;
	ent->movetype = movetype;
	ent->die = prox_die;
	ent->teamchain = field;
	ent->health = PROX_HEALTH;
	ent->nextthink = level.time;
	ent->think = prox_open;
	ent->touch = nullptr;
	ent->solid = SOLID_BBOX;

	gi.linkentity(ent);
}

static THINK(Prox_Think) (gentity_t *self) -> void {
	if (self->timestamp <= level.time) {
		Prox_Explode(self);
		return;
	}

	self->s.angles = vectoangles(self->velocity.normalized());
	self->s.angles[PITCH] -= 90;
	self->nextthink = level.time;
}

void fire_prox(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, int prox_damage_multiplier, int speed) {
	const vec3_t dir = vectoangles(aimdir);
	vec3_t forward, right, up;
	AngleVectors(dir, forward, right, up);

	gentity_t *prox = G_Spawn();
	prox->s.origin = start;
	prox->velocity = aimdir * speed;

	const float gravity_adjustment = level.gravity / 800.0f;
	prox->velocity += up * (200.0f + crandom() * 10.0f) * gravity_adjustment;
	prox->velocity += right * (crandom() * 10.0f);

	prox->s.angles = dir;
	prox->s.angles[PITCH] -= 90;
	prox->movetype = MOVETYPE_BOUNCE;
	prox->solid = SOLID_BBOX;
	prox->svflags |= SVF_PROJECTILE;
	prox->s.effects |= EF_GRENADE;
	prox->flags |= (FL_DODGE | FL_TRAP);
	prox->clipmask = MASK_PROJECTILE | CONTENTS_LAVA | CONTENTS_SLIME;

	// [Paril-KEX]
	if (self->client && !G_ShouldPlayersCollide(true))
		prox->clipmask &= ~CONTENTS_PLAYER;

	prox->s.renderfx |= RF_IR_VISIBLE;
	prox->mins = { -6, -6, -6 };
	prox->maxs = { 6, 6, 6 };
	prox->s.modelindex = gi.modelindex("models/weapons/g_prox/tris.md2");
	prox->owner = self;
	prox->teammaster = self;
	prox->touch = prox_land;
	prox->think = Prox_Think;
	prox->nextthink = level.time;
	prox->dmg = PROX_DAMAGE * prox_damage_multiplier;
	prox->classname = "prox_mine";
	prox->flags |= FL_DAMAGEABLE;
	prox->flags |= FL_MECHANICAL;
	prox->timestamp = level.time + ProxLifetimeForMultiplier(prox_damage_multiplier);

	gi.linkentity(prox);
}
