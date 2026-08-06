// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Tesla mine projectile behavior.
#include "g_local.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_ordnance_identity.h"

namespace {

constexpr gtime_t TESLA_TIME_TO_LIVE = 30_sec;
constexpr float TESLA_DAMAGE_RADIUS = 128.0f;
constexpr int32_t TESLA_DAMAGE = 3;
constexpr int32_t TESLA_KNOCKBACK = 8;

constexpr gtime_t TESLA_ACTIVATE_TIME = 3_sec;

constexpr int32_t TESLA_EXPLOSION_DAMAGE_MULT = 50; // this is the amount the damage is multiplied by for underwater explosions
constexpr float TESLA_EXPLOSION_RADIUS = 200.0f;

constexpr int TESLA_DANGEROUS_CONTENTS = CONTENTS_SLIME | CONTENTS_LAVA;
constexpr int TESLA_CONDUCTIVE_CONTENTS = TESLA_DANGEROUS_CONTENTS | CONTENTS_WATER;

bool TeslaGenerationMatches(const gentity_t *entity, int32_t generation) {
	return entity && entity->inuse && entity->spawn_count == generation;
}

gentity_t *TeslaOwner(const gentity_t *tesla) {
	return MM_ResolveOrdnanceOwner(tesla,
		tesla ? tesla->teammaster : nullptr);
}

gentity_t *TeslaField(const gentity_t *tesla) {
	if (!tesla || !tesla->teamchain)
		return nullptr;

	gentity_t *field = tesla->teamchain;
	if (!MM_OrdnanceEntityIdentityMatches(field, tesla->style,
		tesla->sounds) || MM_ResolveOrdnanceOwner(field) != tesla)
		return nullptr;
	return field;
}

void TeslaDiscard(gentity_t *tesla) {
	if (!tesla)
		return;

	const int32_t tesla_generation = tesla->spawn_count;
	if (gentity_t *field = TeslaField(tesla)) {
		tesla->teamchain = nullptr;
		G_FreeEntity(field);
	}
	if (TeslaGenerationMatches(tesla, tesla_generation))
		G_FreeEntity(tesla);
}

struct tesla_box_context_t {
	gentity_t *tesla;
	gentity_t *owner;
};

bool IsTeslaTargetCandidate(gentity_t *candidate, gentity_t *self, gentity_t *team_damage_owner, bool reject_traps) {
	if (!candidate->inuse)
		return false;
	if (candidate == self)
		return false;
	if (GT(GT_ARENA) && !MM_Arena_CanInteract(self, candidate))
		return false;
	if (candidate->health < 1)
		return false;

	// don't hit teammates
	if (candidate->client) {
		if (!deathmatch->integer)
			return false;
		if (CheckTeamDamage(candidate, team_damage_owner))
			return false;
	}

	if (!(candidate->svflags & SVF_MONSTER) && !(candidate->flags & FL_DAMAGEABLE) && !candidate->client)
		return false;

	// don't hit other teslas in SP/coop
	if (reject_traps && !deathmatch->integer && candidate->classname && (candidate->flags & FL_TRAP))
		return false;

	return true;
}

bool IsSpawnProtectedEntity(const gentity_t *entity) {
	if (!entity->classname)
		return false;

	return !strncmp(entity->classname, "info_player_", 12) ||
		!strcmp(entity->classname, "misc_teleporter_dest") ||
		!strncmp(entity->classname, "item_flag_", 10);
}

} // namespace

static void tesla_remove(gentity_t *self) {
	gentity_t *owner = TeslaOwner(self);
	if (!owner) {
		TeslaDiscard(self);
		return;
	}

	self->takedamage = false;

	if (gentity_t *field = TeslaField(self)) {
		self->teamchain = nullptr;
		G_FreeEntity(field);
	} else if (self->air_finished) {
		TeslaDiscard(self);
		return;
	}

	self->owner = owner; // Going away, set the owner correctly.
	// grenade explode does damage to self->enemy
	self->enemy = nullptr;

	// play quad sound if quadded and an underwater explosion
	if (self->splash_radius && self->dmg > (TESLA_DAMAGE * TESLA_EXPLOSION_DAMAGE_MULT))
		gi.sound(self, CHAN_ITEM, gi.soundindex("items/damage3.wav"), 1, ATTN_NORM, 0);

	Grenade_Explode(self);
}

static DIE(tesla_die) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod) -> void {
	tesla_remove(self);
}

static void tesla_blow(gentity_t *self) {
	if (!TeslaOwner(self)) {
		TeslaDiscard(self);
		return;
	}
	self->dmg *= TESLA_EXPLOSION_DAMAGE_MULT;
	self->splash_radius = TESLA_EXPLOSION_RADIUS;
	tesla_remove(self);
}

static TOUCH(tesla_zap) (gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	const int32_t field_generation = self->spawn_count;
	gentity_t *tesla = MM_ResolveOrdnanceOwner(self);
	if (tesla && TeslaOwner(tesla) && TeslaField(tesla) == self)
		return;

	if (tesla)
		TeslaDiscard(tesla);
	if (TeslaGenerationMatches(self, field_generation))
		G_FreeEntity(self);
}

static BoxEntitiesResult_t tesla_think_active_BoxFilter(gentity_t *check, void *data) {
	auto *context = static_cast<tesla_box_context_t *>(data);
	return IsTeslaTargetCandidate(check, context->tesla, context->owner, true)
		? BoxEntitiesResult_t::Keep : BoxEntitiesResult_t::Skip;
}

static THINK(tesla_think_active) (gentity_t *self) -> void {
	static gentity_t *touch[MAX_ENTITIES];
	static int32_t touch_generations[MAX_ENTITIES];
	gentity_t *owner = TeslaOwner(self);
	gentity_t *field = TeslaField(self);
	if (!owner || !field) {
		TeslaDiscard(self);
		return;
	}

	if (level.time > self->air_finished) {
		tesla_remove(self);
		return;
	}

	if (deathmatch->integer && notGT(GT_ARENA) && IsCombatDisabled())
		return;

	vec3_t start = self->s.origin;
	start[2] += 16;
	tesla_box_context_t context { self, owner };

	const size_t num = gi.BoxEntities(
		field->absmin,
		field->absmax,
		touch,
		MAX_ENTITIES,
		AREA_SOLID,
		tesla_think_active_BoxFilter,
		&context);
	for (size_t i = 0; i < num; i++)
		touch_generations[i] = touch[i]->spawn_count;

	for (size_t i = 0; i < num; i++) {
		// if the tesla died while zapping things, stop zapping.
		if (!self->inuse)
			return;

		auto *hit = touch[i];
		if (!TeslaGenerationMatches(hit, touch_generations[i]))
			continue;
		if (!IsTeslaTargetCandidate(hit, self, owner, false))
			continue;

		const trace_t tr = gi.traceline(start, hit->s.origin, self, MASK_PROJECTILE);
		if (tr.fraction == 1 || tr.ent == hit) {
			const vec3_t dir = hit->s.origin - start;
			const int32_t tesla_generation = self->spawn_count;
			const int32_t target_generation = hit->spawn_count;
			const int target_arena = MM_Arena_Id(hit);

			// PMM - play quad sound if it's above the "normal" damage
			if (self->dmg > TESLA_DAMAGE)
				gi.sound(self, CHAN_ITEM, gi.soundindex("items/damage3.wav"), 1, ATTN_NORM, 0);

			// PGM - don't do knockback to walking monsters
			const int knockback = ((hit->svflags & SVF_MONSTER) && !(hit->flags & (FL_FLY | FL_SWIM))) ? 0 : TESLA_KNOCKBACK;
			T_Damage(hit, self, owner, dir, tr.endpos, tr.plane.normal,
				self->dmg, knockback, DAMAGE_NONE | DAMAGE_STAT_ONCE, MOD_TESLA);
			if (!TeslaGenerationMatches(self, tesla_generation))
				return;

			owner = TeslaOwner(self);
			field = TeslaField(self);
			if (!owner || !field) {
				TeslaDiscard(self);
				return;
			}
			if (!MM_OrdnanceEntityIdentityMatches(hit, target_generation,
				target_arena))
				continue;

			gi.WriteByte(svc_temp_entity);
			gi.WriteByte(TE_LIGHTNING);
			gi.WriteEntity(self); // source entity
			gi.WriteEntity(hit); // destination entity
			gi.WritePosition(start);
			gi.WritePosition(tr.endpos);
			gi.multicast(start, MULTICAST_PVS, false);
		}
	}

	if (!TeslaOwner(self) || !TeslaField(self)) {
		TeslaDiscard(self);
		return;
	}
	self->think = tesla_think_active;
	self->nextthink = level.time + 10_hz;
}

static THINK(tesla_activate) (gentity_t *self) -> void {
	if (!TeslaOwner(self)) {
		TeslaDiscard(self);
		return;
	}

	if (gi.pointcontents(self->s.origin) & TESLA_CONDUCTIVE_CONTENTS) {
		tesla_blow(self);
		return;
	}

	// only check for spawn points in deathmatch
	if (deathmatch->integer) {
		for (auto *search = findradius(nullptr, self->s.origin, 1.5f * TESLA_DAMAGE_RADIUS);
			search;
			search = findradius(search, self->s.origin, 1.5f * TESLA_DAMAGE_RADIUS)) {
			if (GT(GT_ARENA) && !MM_Arena_CanInteract(self, search))
				continue;

			// [Paril-KEX] don't allow traps to be placed near flags or teleporters
			// if it's a monster or player with health > 0
			// or it's a player start point
			// and we can see it
			// blow up
			if (IsSpawnProtectedEntity(search) && visible(search, self)) {
				BecomeExplosion1(self);
				return;
			}
		}
	}

	auto *trigger = G_Spawn();
	trigger->s.origin = self->s.origin;
	trigger->mins = { -TESLA_DAMAGE_RADIUS, -TESLA_DAMAGE_RADIUS, self->mins[2] };
	trigger->maxs = { TESLA_DAMAGE_RADIUS, TESLA_DAMAGE_RADIUS, TESLA_DAMAGE_RADIUS };
	trigger->movetype = MOVETYPE_NONE;
	trigger->solid = SOLID_TRIGGER;
	MM_CaptureOrdnanceOwner(trigger, self);
	self->style = trigger->spawn_count;
	trigger->touch = tesla_zap;
	trigger->classname = "tesla trigger";
	// doesn't need to be marked as a teamslave since the move code for bounce looks for teamchains
	gi.linkentity(trigger);

	self->s.angles = {};
	// clear the owner if in deathmatch
	if (deathmatch->integer)
		self->owner = nullptr;
	self->teamchain = trigger;
	self->think = tesla_think_active;
	self->nextthink = level.time + FRAME_TIME_S;
	self->air_finished = level.time + TESLA_TIME_TO_LIVE;
}

static THINK(tesla_think) (gentity_t *ent) -> void {
	gentity_t *owner = TeslaOwner(ent);
	if (!owner) {
		TeslaDiscard(ent);
		return;
	}

	if (gi.pointcontents(ent->s.origin) & TESLA_DANGEROUS_CONTENTS) {
		tesla_remove(ent);
		return;
	}

	ent->s.angles = {};

	if (!ent->s.frame)
		gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/teslaopen.wav"), 1, ATTN_NORM, 0);

	ent->s.frame++;
	if (ent->s.frame > 14) {
		ent->s.frame = 14;
		ent->think = tesla_activate;
		ent->nextthink = level.time + 10_hz;
	} else {
		if (ent->s.frame > 9) {
			if (ent->s.frame == 10) {
				if (owner->client) {
					PlayerNoise(owner, ent->s.origin, PNOISE_WEAPON);
				}
				ent->s.skinnum = 1;
			} else if (ent->s.frame == 12) {
				ent->s.skinnum = 2;
			} else if (ent->s.frame == 14) {
				ent->s.skinnum = 3;
			}
		}
		ent->think = tesla_think;
		ent->nextthink = level.time + 10_hz;
	}
}

static TOUCH(tesla_lava) (gentity_t *ent, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	if (!TeslaOwner(ent)) {
		TeslaDiscard(ent);
		return;
	}

	if (tr.contents & TESLA_DANGEROUS_CONTENTS) {
		tesla_blow(ent);
		return;
	}

	if (ent->velocity) {
		if (frandom() > 0.5f)
			gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/hgrenb1a.wav"), 1, ATTN_NORM, 0);
		else
			gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/hgrenb2a.wav"), 1, ATTN_NORM, 0);
	}
}

void fire_tesla(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, int tesla_damage_multiplier, int speed) {
	const vec3_t dir = vectoangles(aimdir);
	vec3_t forward, right, up;
	AngleVectors(dir, forward, right, up);

	auto *tesla = G_Spawn();
	tesla->s.origin = start;
	tesla->velocity = aimdir * speed;

	const float gravity_adjustment = level.gravity / 800.0f;

	tesla->velocity += up * (200.0f + crandom() * 10.0f) * gravity_adjustment;
	tesla->velocity += right * (crandom() * 10.0f);

	tesla->s.angles = {};
	tesla->movetype = MOVETYPE_BOUNCE;
	tesla->solid = SOLID_BBOX;
	tesla->s.effects |= EF_GRENADE;
	tesla->s.renderfx |= RF_IR_VISIBLE;
	tesla->mins = { -12, -12, 0 };
	tesla->maxs = { 12, 12, 20 };
	tesla->s.modelindex = gi.modelindex("models/weapons/g_tesla/tris.md2");

	MM_CaptureOrdnanceOwner(tesla, self); // PGM - we don't want it owned by self YET.
	tesla->teammaster = self;

	tesla->wait = (level.time + TESLA_TIME_TO_LIVE).seconds();
	tesla->think = tesla_think;
	tesla->nextthink = level.time + TESLA_ACTIVATE_TIME;

	// blow up on contact with lava & slime code
	tesla->touch = tesla_lava;

	tesla->health = deathmatch->integer ? 20 : 50;

	tesla->takedamage = true;
	tesla->die = tesla_die;
	tesla->dmg = TESLA_DAMAGE * tesla_damage_multiplier;
	tesla->classname = "tesla_mine";
	tesla->flags |= (FL_DAMAGEABLE | FL_TRAP);
	tesla->clipmask = (MASK_PROJECTILE | CONTENTS_SLIME | CONTENTS_LAVA) & ~CONTENTS_DEADMONSTER;

	// [Paril-KEX]
	if (self->client && !G_ShouldPlayersCollide(true))
		tesla->clipmask &= ~CONTENTS_PLAYER;

	tesla->flags |= FL_MECHANICAL;

	gi.linkentity(tesla);
}
