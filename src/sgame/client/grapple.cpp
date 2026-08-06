// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Grapple and off-hand hook weapon behavior.
#include "g_local.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_freezetag.h"
#include "muffmode/mm_ordnance_identity.h"

namespace {
constexpr float GRAPPLE_NORMAL_VOLUME = 1.0f;
constexpr float GRAPPLE_SILENCED_VOLUME = 0.2f;

float GrappleSoundVolume(const gentity_t *ent) {
	return ent && ent->client && ent->client->silencer_shots ? GRAPPLE_SILENCED_VOLUME : GRAPPLE_NORMAL_VOLUME;
}
} // namespace

/*
======================================================================

GRAPPLE

======================================================================
*/

// self is grapple, not player
static bool Weapon_Grapple_HasValidOwner(const gentity_t *self) {
	return self && self->owner && self->owner->inuse && self->owner->client &&
		self->owner->spawn_count == self->count;
}

static void Weapon_Grapple_Reset(gentity_t *self) {
	if (!self)
		return;

	gentity_t *owner = self->owner;
	if (!Weapon_Grapple_HasValidOwner(self)) {
		G_FreeEntity(self);
		return;
	}

	if (owner->client->grapple_ent != self) {
		// If the client already dropped its hook pointer, finish clearing the
		// movement state this orphan represented. Do not disturb a newer hook.
		if (owner && owner->client && !owner->client->grapple_ent) {
			owner->client->grapple_release_time = level.time + 1_sec;
			owner->client->grapple_state = GRAPPLE_STATE_FLY;
			owner->flags &= ~FL_NO_KNOCKBACK;
		}
		G_FreeEntity(self);
		return;
	}

	gi.sound(owner, CHAN_WEAPON, gi.soundindex("weapons/grapple/grreset.wav"), GrappleSoundVolume(owner), ATTN_NORM, 0);

	gclient_t *cl = owner->client;
	cl->grapple_ent = nullptr;
	cl->grapple_release_time = level.time + 1_sec;
	cl->grapple_state = GRAPPLE_STATE_FLY; // we're firing, not on hook
	owner->flags &= ~FL_NO_KNOCKBACK;
	G_FreeEntity(self);
}

void Weapon_Grapple_DoReset(gclient_t *cl) {
	if (cl && cl->grapple_ent)
		Weapon_Grapple_Reset(cl->grapple_ent);
}

static TOUCH(Weapon_Grapple_Touch) (gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	if (!Weapon_Grapple_HasValidOwner(self) || !other) {
		Weapon_Grapple_Reset(self);
		return;
	}

	if (other == self->owner)
		return;

	if (self->owner->client->grapple_state != GRAPPLE_STATE_FLY)
		return;

	if (tr.surface && (tr.surface->flags & SURF_SKY)) {
		Weapon_Grapple_Reset(self);
		return;
	}

	self->velocity = {};

	PlayerNoise(self->owner, self->s.origin, PNOISE_IMPACT);

	if (MM_FreezeTag_IsFrozen(other)) {
		self->owner->client->grapple_state = GRAPPLE_STATE_PULL;
		self->enemy = other;
		self->solid = SOLID_NOT;

		gi.sound(self, CHAN_WEAPON, gi.soundindex("weapons/grapple/grhit.wav"), GrappleSoundVolume(self->owner), ATTN_NORM, 0);
		self->s.sound = gi.soundindex("weapons/grapple/grpull.wav");
		return;
	}

	if (other->takedamage) {
		if (self->dmg)
			T_Damage(other, self, self->owner, self->velocity, self->s.origin, tr.plane.normal, self->dmg, 1, DAMAGE_NONE | DAMAGE_STAT_ONCE, MOD_GRAPPLE);
		Weapon_Grapple_Reset(self);
		return;
	}

	self->owner->client->grapple_state = GRAPPLE_STATE_PULL; // we're on hook
	self->enemy = other;

	self->solid = SOLID_NOT;

	gi.sound(self, CHAN_WEAPON, gi.soundindex("weapons/grapple/grhit.wav"), GrappleSoundVolume(self->owner), ATTN_NORM, 0);
	self->s.sound = gi.soundindex("weapons/grapple/grpull.wav");

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_SPARKS);
	gi.WritePosition(self->s.origin);
	gi.WriteDir(tr.plane.normal);
	gi.multicast(self->s.origin, MULTICAST_PVS, false);
}

// draw beam between grapple and self
static void Weapon_Grapple_DrawCable(gentity_t *self) {
	if (self->owner->client->grapple_state == GRAPPLE_STATE_HANG)
		return;

	vec3_t start, dir;
	P_ProjectSource(self->owner, self->owner->client->v_angle, { 7, 2, -9 }, start, dir);

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_GRAPPLE_CABLE_2);
	gi.WriteEntity(self->owner);
	gi.WritePosition(start);
	gi.WritePosition(self->s.origin);
	gi.multicast(self->s.origin, MULTICAST_PVS, false);
}

// pull the player toward the grapple
void Weapon_Grapple_Pull(gentity_t *self) {
	vec3_t hookdir, v;
	float  vlen;

	if (!Weapon_Grapple_HasValidOwner(self)) {
		Weapon_Grapple_Reset(self);
		return;
	}

	if (self->owner->client->pers.weapon && self->owner->client->pers.weapon->id == IT_WEAPON_GRAPPLE &&
		!(self->owner->client->newweapon || ((self->owner->client->latched_buttons | self->owner->client->buttons) & BUTTON_HOLSTER)) &&
		self->owner->client->weaponstate != WEAPON_FIRING &&
		self->owner->client->weaponstate != WEAPON_ACTIVATING) {
		if (!self->owner->client->newweapon)
			self->owner->client->newweapon = self->owner->client->pers.weapon;

		Weapon_Grapple_Reset(self);
		return;
	}

	if (self->enemy) {
		if (self->enemy->solid == SOLID_NOT) {
			Weapon_Grapple_Reset(self);
			return;
		}
		if (self->enemy->client && !MM_FreezeTag_IsFrozen(self->enemy)) {
			Weapon_Grapple_Reset(self);
			return;
		}
		if (self->enemy->solid == SOLID_BBOX) {
			v = self->enemy->size * 0.5f;
			v += self->enemy->s.origin;
			self->s.origin = v + self->enemy->mins;
			gi.linkentity(self);
		} else
			self->velocity = self->enemy->velocity;

		if (self->enemy->deadflag) { // he died
			Weapon_Grapple_Reset(self);
			return;
		}
	}

	Weapon_Grapple_DrawCable(self);

	if (self->enemy && MM_FreezeTag_IsFrozen(self->enemy)) {
		vec3_t target = self->owner->s.origin;
		target[2] += self->owner->viewheight;
		vec3_t body = self->enemy->s.origin;
		body[2] += 16.0f;
		hookdir = target - body;
		vlen = hookdir.length();

		if (vlen > 48.0f) {
			hookdir.normalize();
			self->enemy->velocity = hookdir * g_grapple_pull_speed->value;
			G_AddGravity(self->enemy);
			// [MuffMode] Freeze Tag: the pull writes velocity onto a grounded
			// MOVETYPE_TOSS body, which G_Physics_Toss never integrates. Route it
			// through the same unstick-and-clamp path gunfire uses.
			MM_FreezeTag_ApplyBodyImpulse(self->enemy);
			gi.linkentity(self->enemy);
		} else {
			self->enemy->velocity = {};
		}

		self->owner->flags &= ~FL_NO_KNOCKBACK;
		return;
	}

	if (self->owner->client->grapple_state > GRAPPLE_STATE_FLY) {
		// pull player toward grapple
		vec3_t forward, up;

		AngleVectors(self->owner->client->v_angle, forward, nullptr, up);
		v = self->owner->s.origin;
		v[2] += self->owner->viewheight;
		hookdir = self->s.origin - v;

		vlen = hookdir.length();

		if (self->owner->client->grapple_state == GRAPPLE_STATE_PULL &&
			vlen < 64) {
			self->owner->client->grapple_state = GRAPPLE_STATE_HANG;
			self->s.sound = gi.soundindex("weapons/grapple/grhang.wav");
		}

		hookdir.normalize();
		hookdir = hookdir * g_grapple_pull_speed->value;
		self->owner->velocity = hookdir;
		self->owner->flags |= FL_NO_KNOCKBACK;
		G_AddGravity(self->owner);
	}
}

static DIE(Weapon_Grapple_Die) (gentity_t *self, gentity_t *other, gentity_t *inflictor, int damage, const vec3_t &point, const mod_t &mod) -> void {
	if (mod.id == MOD_CRUSH)
		Weapon_Grapple_Reset(self);
}

static bool Weapon_Grapple_FireHook(gentity_t *self, const vec3_t &start, const vec3_t &dir, int damage, float speed, effects_t effect) {
	// [MuffMode] Engine-facing hook calls must not manufacture an ownerless grapple.
	if (!self || !self->client)
		return false;

	gentity_t	*grapple;
	trace_t	tr;
	vec3_t	normalized = dir.normalized();

	grapple = G_Spawn();
	grapple->s.origin = start;
	grapple->s.old_origin = start;
	grapple->s.angles = vectoangles(normalized);
	grapple->velocity = normalized * speed;
	grapple->movetype = MOVETYPE_FLYMISSILE;
	grapple->clipmask = MASK_PROJECTILE;
	// [Paril-KEX]
	if (!G_ShouldPlayersCollide(true))
		grapple->clipmask &= ~CONTENTS_PLAYER;
	grapple->solid = SOLID_BBOX;
	grapple->s.effects |= effect;
	grapple->s.modelindex = gi.modelindex("models/weapons/grapple/hook/tris.md2");
	MM_CaptureOrdnanceOwner(grapple, self);
	grapple->touch = Weapon_Grapple_Touch;
	grapple->dmg = damage;
	grapple->flags |= FL_NO_KNOCKBACK | FL_NO_DAMAGE_EFFECTS;
	grapple->takedamage = true;
	grapple->die = Weapon_Grapple_Die;
	self->client->grapple_ent = grapple;
	self->client->grapple_state = GRAPPLE_STATE_FLY; // we're firing, not on hook
	gi.linkentity(grapple);

	tr = gi.traceline(self->s.origin, grapple->s.origin, grapple, grapple->clipmask);
	if (tr.fraction < 1.0f &&
		(notGT(GT_ARENA) || MM_Arena_CanInteract(self, tr.ent))) {
		grapple->s.origin = tr.endpos + (tr.plane.normal * 1.f);
		grapple->touch(grapple, tr.ent, tr, false);
		return false;
	}

	grapple->s.sound = gi.soundindex("weapons/grapple/grfly.wav");

	return true;
}

static void Weapon_Grapple_DoFire(gentity_t *ent, const vec3_t &g_offset, int damage, effects_t effect) {
	if (ent->client->grapple_state > GRAPPLE_STATE_FLY)
		return; // it's already out

	vec3_t start, dir;
	P_ProjectSource(ent, ent->client->v_angle, vec3_t{ 24, 8, -8 + 2 } + g_offset, start, dir);

	if (Weapon_Grapple_FireHook(ent, start, dir, damage, g_grapple_fly_speed->value, effect))
		gi.sound(ent, CHAN_WEAPON, gi.soundindex("weapons/grapple/grfire.wav"), GrappleSoundVolume(ent), ATTN_NORM, 0);

	PlayerNoise(ent, start, PNOISE_WEAPON);
}

static void Weapon_Grapple_Fire(gentity_t *ent) {
	Weapon_Grapple_DoFire(ent, vec3_origin, g_grapple_damage->integer, EF_NONE);
}

void Weapon_Grapple(gentity_t *ent) {
	constexpr int pause_frames[] = { 10, 18, 27, 0 };
	constexpr int fire_frames[] = { 6, 0 };
	int			  prevstate;

	// if the the attack button is still down, stay in the firing frame
	if ((ent->client->buttons & (BUTTON_ATTACK | BUTTON_HOLSTER)) &&
		ent->client->weaponstate == WEAPON_FIRING &&
		ent->client->grapple_ent)
		ent->client->ps.gunframe = 6;

	if (!(ent->client->buttons & (BUTTON_ATTACK | BUTTON_HOLSTER)) &&
		ent->client->grapple_ent) {
		Weapon_Grapple_Reset(ent->client->grapple_ent);
		if (ent->client->weaponstate == WEAPON_FIRING)
			ent->client->weaponstate = WEAPON_READY;
	}

	if ((ent->client->newweapon || ((ent->client->latched_buttons | ent->client->buttons) & BUTTON_HOLSTER)) &&
		ent->client->grapple_state > GRAPPLE_STATE_FLY &&
		ent->client->weaponstate == WEAPON_FIRING) {
		// he wants to change weapons while grappled
		if (!ent->client->newweapon)
			ent->client->newweapon = ent->client->pers.weapon;
		ent->client->weaponstate = WEAPON_DROPPING;
		ent->client->ps.gunframe = 32;
	}

	prevstate = ent->client->weaponstate;
	Weapon_Generic(ent, 5, 10, 31, 36, pause_frames, fire_frames,
		Weapon_Grapple_Fire);

	// if the the attack button is still down, stay in the firing frame
	if ((ent->client->buttons & (BUTTON_ATTACK | BUTTON_HOLSTER)) &&
		ent->client->weaponstate == WEAPON_FIRING &&
		ent->client->grapple_ent)
		ent->client->ps.gunframe = 6;

	// if we just switched back to grapple, immediately go to fire frame
	if (prevstate == WEAPON_ACTIVATING &&
		ent->client->weaponstate == WEAPON_READY &&
		ent->client->grapple_state > GRAPPLE_STATE_FLY) {
		if (!(ent->client->buttons & (BUTTON_ATTACK | BUTTON_HOLSTER)))
			ent->client->ps.gunframe = 6;
		else
			ent->client->ps.gunframe = 5;
		ent->client->weaponstate = WEAPON_FIRING;
	}
}


/*
======================================================================

OFF-HAND HOOK

======================================================================
*/

static void Weapon_Hook_DoFire(gentity_t *ent, const vec3_t &g_offset, int damage, effects_t effect) {
	if (ent->client->grapple_state > GRAPPLE_STATE_FLY)
		return; // it's already out

	vec3_t start, dir;
	P_ProjectSource(ent, ent->client->v_angle, vec3_t{ 24, 0, 0 } + g_offset, start, dir);

	if (Weapon_Grapple_FireHook(ent, start, dir, damage, g_grapple_fly_speed->value, effect))
		gi.sound(ent, CHAN_WEAPON, gi.soundindex("weapons/grapple/grfire.wav"), GrappleSoundVolume(ent), ATTN_NORM, 0);

	PlayerNoise(ent, start, PNOISE_WEAPON);
}

void Weapon_Hook(gentity_t *ent) {
	Weapon_Hook_DoFire(ent, vec3_origin, g_grapple_damage->integer, EF_NONE);
}
