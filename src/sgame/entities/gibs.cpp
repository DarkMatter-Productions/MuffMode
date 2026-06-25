// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Shared gib, debris and simple explosion helpers.
#include "g_local.h"

namespace {

/*
=============
GibSink

After sitting around for x seconds, fall into the ground and disappear.
=============
*/
THINK(GibSink) (gentity_t *ent) -> void {
	if (level.time > ent->timestamp) {
		ent->svflags = SVF_NOCLIENT;
		ent->takedamage = false;
		ent->solid = SOLID_NOT;
		G_FreeEntity(ent);
		return;
	}

	ent->nextthink = level.time + 50_ms;
	ent->s.origin[2] -= 0.5f;
}

TOUCH(gib_touch) (gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	if (tr.plane.normal[2] > 0.7f) {
		self->s.angles[PITCH] = clamp(self->s.angles[PITCH], -5.0f, 5.0f);
		self->s.angles[ROLL] = clamp(self->s.angles[ROLL], -5.0f, 5.0f);
	}
}

void EmitExplosion(gentity_t *self, temp_event_t event) {
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(event);
	gi.WritePosition(self->s.origin);
	gi.multicast(self->s.origin, MULTICAST_PHS, false);

	G_FreeEntity(self);
}

gtime_t GibLifetime() {
	if (g_instagib->integer || GT(GT_INSTAGIB))
		return random_time(1_sec, 5_sec);

	return random_time(10_sec, 20_sec);
}

gtime_t ClientHeadLifetime() {
	if (g_instagib->integer)
		return random_time(1_sec, 5_sec);

	return random_time(10_sec, 20_sec);
}

} // namespace

void VelocityForDamage(int damage, vec3_t &v) {
	v[0] = 100.0f * crandom();
	v[1] = 100.0f * crandom();
	v[2] = frandom(200.0f, 300.0f);

	if (damage < 50)
		v *= 0.7f;
	else
		v *= 1.2f;
}

void ClipGibVelocity(gentity_t *ent) {
	ent->velocity[0] = clamp(ent->velocity[0], -300.0f, 300.0f);
	ent->velocity[1] = clamp(ent->velocity[1], -300.0f, 300.0f);
	ent->velocity[2] = clamp(ent->velocity[2], 200.0f, 500.0f);
}

DIE(gib_die) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod) -> void {
	if (mod.id == MOD_CRUSH)
		G_FreeEntity(self);
}

gentity_t *ThrowGib(gentity_t *self, const char *gibname, int damage, gib_type_t type, float scale) {
	gentity_t *gib = nullptr;

	if (type & GIB_HEAD) {
		gib = self;
		gib->s.event = EV_OTHER_TELEPORT;
		// remove setskin so that it doesn't set the skin wrongly later
		self->monsterinfo.setskin = nullptr;
	} else {
		gib = G_Spawn();
	}

	const vec3_t size = self->size * 0.5f;
	const vec3_t origin = (self->absmin + vec3_t{ 1, 1, 1 }) + size;

	int32_t i = 0;
	for (; i < 3; i++) {
		gib->s.origin = origin + vec3_t{ crandom(), crandom(), crandom() }.scaled(size);

		// try 3 times to get a good, non-solid position
		if (!(gi.pointcontents(gib->s.origin) & MASK_SOLID))
			break;
	}

	if (i == 3 && gib != self) {
		G_FreeEntity(gib);
		return nullptr;
	}

	gib->s.modelindex = gi.modelindex(gibname);
	gib->s.modelindex2 = 0;
	gib->s.scale = scale;
	gib->solid = SOLID_NOT;
	gib->svflags |= SVF_DEADMONSTER;
	gib->svflags &= ~SVF_MONSTER;
	gib->clipmask = MASK_SOLID;
	gib->s.effects = EF_NONE;
	gib->s.renderfx = RF_LOW_PRIORITY | RF_FULLBRIGHT;

	if (!(type & GIB_DEBRIS)) {
		if (type & GIB_ACID)
			gib->s.effects |= EF_GREENGIB;
		else
			gib->s.effects |= EF_GIB;
		gib->s.renderfx |= RF_IR_VISIBLE;
	}

	gib->flags |= FL_NO_KNOCKBACK | FL_NO_DAMAGE_EFFECTS;
	gib->takedamage = true;
	gib->die = gib_die;
	gib->classname = "gib";
	gib->s.skinnum = (type & GIB_SKINNED) ? self->s.skinnum : 0;
	gib->s.frame = 0;
	gib->mins = gib->maxs = {};
	gib->s.sound = 0;
	gib->monsterinfo.engine_sound = 0;

	if (!(type & GIB_METALLIC)) {
		gib->movetype = MOVETYPE_TOSS;
	} else {
		gib->movetype = MOVETYPE_BOUNCE;
	}

	if (type & GIB_DEBRIS) {
		const vec3_t debris_velocity = {
			100.0f * crandom(),
			100.0f * crandom(),
			100.0f + 100.0f * crandom()
		};
		gib->velocity = self->velocity + (debris_velocity * damage);
	} else {
		vec3_t velocity_delta;
		VelocityForDamage(damage, velocity_delta);
		const float velocity_scale = (type & GIB_METALLIC) ? 1.0f : ((type & GIB_ACID) ? 3.0f : 0.5f);
		gib->velocity = self->velocity + (velocity_delta * velocity_scale);
		ClipGibVelocity(gib);
	}

	if (type & GIB_UPRIGHT) {
		gib->touch = gib_touch;
		gib->flags |= FL_ALWAYS_TOUCH;
	}

	gib->avelocity[0] = frandom(600);
	gib->avelocity[1] = frandom(600);
	gib->avelocity[2] = frandom(600);

	gib->s.angles[PITCH] = frandom(359);
	gib->s.angles[YAW] = frandom(359);
	gib->s.angles[ROLL] = frandom(359);

	gib->think = GibSink;
	gib->nextthink = level.time + GibLifetime();
	gib->timestamp = gib->nextthink + 1.5_sec;

	gi.linkentity(gib);

	gib->watertype = gi.pointcontents(gib->s.origin);
	gib->waterlevel = (gib->watertype & MASK_WATER) ? WATER_FEET : WATER_NONE;

	return gib;
}

void ThrowClientHead(gentity_t *self, int damage) {
	vec3_t vd;
	const char *gibname = nullptr;

	if (brandom()) {
		gibname = "models/objects/gibs/head2/tris.md2";
		self->s.skinnum = 1; // second skin is player
	} else {
		gibname = "models/objects/gibs/skull/tris.md2";
		self->s.skinnum = 0;
	}

	self->s.origin[2] += 16;
	self->s.frame = 0;
	gi.setmodel(self, gibname);
	self->mins = { -8, -8, 0 };
	self->maxs = { 8, 8, 8 };

	self->takedamage = true;
	self->solid = SOLID_TRIGGER;
	self->svflags |= SVF_DEADMONSTER;
	self->s.effects = EF_GIB;
	self->s.renderfx = RF_LOW_PRIORITY | RF_FULLBRIGHT | RF_IR_VISIBLE;
	self->s.sound = 0;
	self->flags |= FL_NO_KNOCKBACK | FL_NO_DAMAGE_EFFECTS;

	self->movetype = MOVETYPE_BOUNCE;
	VelocityForDamage(damage, vd);
	self->velocity += vd;

	if (self->client) {
		self->client->anim_priority = ANIM_DEATH;
		self->client->anim_end = self->s.frame;
	} else {
		self->think = nullptr;
		self->nextthink = 0_ms;
	}

	self->think = GibSink;
	self->nextthink = level.time + ClientHeadLifetime();
	self->timestamp = self->nextthink + 1.5_sec;

	gi.linkentity(self);
}

void BecomeExplosion1(gentity_t *self) {
	EmitExplosion(self, TE_EXPLOSION1);
}

void BecomeExplosion2(gentity_t *self) {
	EmitExplosion(self, TE_EXPLOSION2);
}
