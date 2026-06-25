// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// g_misc.c

#include "g_local.h"

/*QUAKED func_group (0 0 0) ? x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Used to group brushes together just for editor convenience.
*/

//=====================================================

static USE(Use_Areaportal) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	ent->count ^= 1; // toggle state
	gi.SetAreaPortalState(ent->style, ent->count);
}

/*QUAKED func_areaportal (0 0 0) ? x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP

This is a non-visible object that divides the world into
areas that are seperated when this portal is not activated.
Usually enclosed in the middle of a door.
*/
void SP_func_areaportal(gentity_t *ent) {
	ent->use = Use_Areaportal;
	ent->count = 0; // always start closed;
}

//=====================================================

// Shared gib, debris and simple explosion helpers live in g_misc_gibs.cpp.

/*QUAKED path_corner (.5 .3 0) (-8 -8 -8) (8 8 8) TELEPORT x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Target: next path corner
Pathtarget: gets used when an entity that has
	this path_corner targeted touches it
*/

static TOUCH(path_corner_touch) (gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	vec3_t	 v;
	gentity_t *next;

	if (other->movetarget != self)
		return;

	if (other->enemy)
		return;

	if (self->pathtarget) {
		const char *savetarget;

		savetarget = self->target;
		self->target = self->pathtarget;
		G_UseTargets(self, other);
		self->target = savetarget;
	}

	// see m_move; this is just so we don't needlessly check it
	self->flags |= FL_PARTIALGROUND;

	if (self->target)
		next = G_PickTarget(self->target);
	else
		next = nullptr;

	// [Paril-KEX] don't teleport to a point_combat, it means HOLD for them.
	if ((next) && !strcmp(next->classname, "path_corner") && next->spawnflags.has(SPAWNFLAG_PATH_CORNER_TELEPORT)) {
		v = next->s.origin;
		v[2] += next->mins[2];
		v[2] -= other->mins[2];
		other->s.origin = v;
		next = G_PickTarget(next->target);
		other->s.event = EV_OTHER_TELEPORT;
	}

	other->goalentity = other->movetarget = next;

	if (self->wait) {
		other->monsterinfo.pausetime = level.time + gtime_t::from_sec(self->wait);
		other->monsterinfo.stand(other);
		return;
	}

	if (!other->movetarget) {
		// N64 cutscene behavior
		if (other->hackflags & HACKFLAG_END_CUTSCENE) {
			G_FreeEntity(other);
			return;
		}

		other->monsterinfo.pausetime = HOLD_FOREVER;
		other->monsterinfo.stand(other);
	} else {
		v = other->goalentity->s.origin - other->s.origin;
		other->ideal_yaw = vectoyaw(v);
	}
}

void SP_path_corner(gentity_t *self) {
	if (!self->targetname) {
		gi.Com_PrintFmt("{} with no targetname\n", *self);
		G_FreeEntity(self);
		return;
	}

	self->solid = SOLID_TRIGGER;
	self->touch = path_corner_touch;
	self->mins = { -8, -8, -8 };
	self->maxs = { 8, 8, 8 };
	self->svflags |= SVF_NOCLIENT;
	gi.linkentity(self);
}

/*QUAKED point_combat (0.5 0.3 0) (-8 -8 -8) (8 8 8) HOLD x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Makes this the target of a monster and it will head here
when first activated before going after the activator.  If
hold is selected, it will stay here.
*/
TOUCH(point_combat_touch) (gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	gentity_t *activator;

	if (other->movetarget != self)
		return;

	if (self->target) {
		other->target = self->target;
		other->goalentity = other->movetarget = G_PickTarget(other->target);
		if (!other->goalentity) {
			gi.Com_PrintFmt("{} target {} does not exist\n", *self, self->target);
			other->movetarget = self;
		}
		// [Paril-KEX] allow them to be re-used
		//self->target = nullptr;
	} else if (self->spawnflags.has(SPAWNFLAG_POINT_COMBAT_HOLD) && !(other->flags & (FL_SWIM | FL_FLY))) {
		// already standing
		if (other->monsterinfo.aiflags & AI_STAND_GROUND)
			return;

		other->monsterinfo.pausetime = HOLD_FOREVER;
		other->monsterinfo.aiflags |= AI_STAND_GROUND | AI_REACHED_HOLD_COMBAT | AI_THIRD_EYE;
		other->monsterinfo.stand(other);
	}

	if (other->movetarget == self) {
		// [Paril-KEX] if we're holding, keep movetarget set; we will
		// use this to make sure we haven't moved too far from where
		// we want to "guard".
		if (!self->spawnflags.has(SPAWNFLAG_POINT_COMBAT_HOLD)) {
			other->target = nullptr;
			other->movetarget = nullptr;
		}

		other->goalentity = other->enemy;
		other->monsterinfo.aiflags &= ~AI_COMBAT_POINT;
	}

	if (self->pathtarget) {
		const char *savetarget;

		savetarget = self->target;
		self->target = self->pathtarget;
		if (other->enemy && other->enemy->client)
			activator = other->enemy;
		else if (other->oldenemy && other->oldenemy->client)
			activator = other->oldenemy;
		else if (other->activator && other->activator->client)
			activator = other->activator;
		else
			activator = other;
		G_UseTargets(self, activator);
		self->target = savetarget;
	}
}

void SP_point_combat(gentity_t *self) {
	if (deathmatch->integer && !ai_allow_dm_spawn->integer) {
		G_FreeEntity(self);
		return;
	}
	self->solid = SOLID_TRIGGER;
	self->touch = point_combat_touch;
	self->mins = { -8, -8, -16 };
	self->maxs = { 8, 8, 16 };
	self->svflags = SVF_NOCLIENT;
	gi.linkentity(self);
}

/*QUAKED info_null (0 0.5 0) (-4 -4 -4) (4 4 4) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Used as a positional target for spotlights, etc.
*/
void SP_info_null(gentity_t *self) {
	G_FreeEntity(self);
}

/*QUAKED info_notnull (0 0.5 0) (-4 -4 -4) (4 4 4) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Used as a positional target for entities.
*/
void SP_info_notnull(gentity_t *self) {
	self->absmin = self->s.origin;
	self->absmax = self->s.origin;
}

/*QUAKED misc_explobox (0 .5 .8) (-16 -16 0) (16 16 40) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Large exploding box.  You can override its mass (100),
health (80), and dmg (150).
*/

static TOUCH(barrel_touch) (gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	float  ratio;
	vec3_t v;

	if ((!other->groundentity) || (other->groundentity == self))
		return;
	else if (!other_touching_self)
		return;

	ratio = (float)other->mass / (float)self->mass;
	v = self->s.origin - other->s.origin;
	M_walkmove(self, vectoyaw(v), 20 * ratio * gi.frame_time_s);
}

static THINK(barrel_explode) (gentity_t *self) -> void {
	self->takedamage = false;

	T_RadiusDamage(self, self->activator, (float)self->dmg, nullptr, (float)(self->dmg + 40), DAMAGE_NONE, MOD_BARREL);

	ThrowGibs(self, (1.5f * self->dmg / 200.f), {
		{ 2, "models/objects/debris1/tris.md2", GIB_METALLIC | GIB_DEBRIS },
		{ 4, "models/objects/debris3/tris.md2", GIB_METALLIC | GIB_DEBRIS },
		{ 8, "models/objects/debris2/tris.md2", GIB_METALLIC | GIB_DEBRIS }
		});

	if (self->groundentity)
		BecomeExplosion2(self);
	else
		BecomeExplosion1(self);
}

static THINK(barrel_burn) (gentity_t *self) -> void {
	if (level.time >= self->timestamp)
		self->think = barrel_explode;

	self->s.effects |= EF_BARREL_EXPLODING;
	self->s.sound = gi.soundindex("weapons/bfg__l1a.wav");
	self->nextthink = level.time + FRAME_TIME_S;
}

DIE(barrel_delay) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod) -> void {
	// allow "dead" barrels waiting to explode to still receive knockback
	if (self->think == barrel_burn || self->think == barrel_explode)
		return;

	// allow big booms to immediately blow up barrels (rockets, rail, other explosions) because it feels good and powerful
	if (damage >= 90) {
		self->think = barrel_explode;
		self->activator = attacker;
	} else {
		self->timestamp = level.time + 750_ms;
		self->think = barrel_burn;
		self->activator = attacker;
	}

}

static THINK(barrel_think) (gentity_t *self) -> void {
	// the think needs to be first since later stuff may override.
	self->think = barrel_think;
	self->nextthink = level.time + FRAME_TIME_S;

	M_CatagorizePosition(self, self->s.origin, self->waterlevel, self->watertype);
	self->flags |= FL_IMMUNE_SLIME;
	self->air_finished = level.time + 100_sec;
	M_WorldEffects(self);
}

THINK(barrel_start) (gentity_t *self) -> void {
	M_droptofloor(self);
	self->think = barrel_think;
	self->nextthink = level.time + FRAME_TIME_S;
}

void SP_misc_explobox(gentity_t *self) {
	/*
	if (deathmatch->integer)
	{ // auto-remove for deathmatch
		G_FreeEntity(self);
		return;
	}
	*/
	gi.modelindex("models/objects/debris1/tris.md2");
	gi.modelindex("models/objects/debris2/tris.md2");
	gi.modelindex("models/objects/debris3/tris.md2");
	gi.soundindex("weapons/bfg__l1a.wav");

	self->solid = SOLID_BBOX;
	self->movetype = MOVETYPE_STEP;

	self->model = "models/objects/barrels/tris.md2";
	self->s.modelindex = gi.modelindex(self->model);

	float scale = self->s.scale;
	if (!scale)
		scale = 1.0f;
	self->mins = { -16 * scale, -16 * scale, 0 };
	self->maxs = { 16 * scale, 16 * scale, 40 * scale };

	if (!self->mass)
		self->mass = 50;
	if (!self->health)
		self->health = 10;
	if (!self->dmg)
		self->dmg = 150;

	self->die = barrel_delay;
	self->takedamage = true;
	self->flags |= FL_TRAP;

	self->touch = barrel_touch;

	self->think = barrel_start;
	self->nextthink = level.time + 20_hz;

	gi.linkentity(self);
}

//
// miscellaneous specialty items
//

/*QUAKED misc_blackhole (1 .5 0) (-8 -8 -8) (8 8 8) AUTO_NOISE x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/objects/black/tris.md2"
*/

constexpr spawnflags_t SPAWNFLAG_BLACKHOLE_AUTO_NOISE = 1_spawnflag;

USE(misc_blackhole_use) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	/*
	gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_BOSSTPORT);
	gi.WritePosition (ent->s.origin);
	gi.multicast (ent->s.origin, MULTICAST_PVS);
	*/
	G_FreeEntity(ent);
}

THINK(misc_blackhole_think) (gentity_t *self) -> void {
	if (self->timestamp <= level.time) {
		if (++self->s.frame >= 19)
			self->s.frame = 0;

		self->timestamp = level.time + 10_hz;
	}

	if (self->spawnflags.has(SPAWNFLAG_BLACKHOLE_AUTO_NOISE)) {
		self->s.angles[PITCH] += 50.0f * gi.frame_time_s;
		self->s.angles[YAW] += 50.0f * gi.frame_time_s;
	}

	self->nextthink = level.time + FRAME_TIME_MS;
}

void SP_misc_blackhole(gentity_t *ent) {
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_NOT;
	ent->mins = { -64, -64, 0 };
	ent->maxs = { 64, 64, 8 };
	ent->s.modelindex = gi.modelindex("models/objects/black/tris.md2");
	ent->s.renderfx = RF_TRANSLUCENT;
	ent->use = misc_blackhole_use;
	ent->think = misc_blackhole_think;
	ent->nextthink = level.time + 20_hz;

	if (ent->spawnflags.has(SPAWNFLAG_BLACKHOLE_AUTO_NOISE)) {
		ent->s.sound = gi.soundindex("world/blackhole.wav");
		ent->s.loop_attenuation = ATTN_NORM;
	}

	gi.linkentity(ent);
}

/*QUAKED misc_eastertank (1 .5 0) (-32 -32 -16) (32 32 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */

THINK(misc_eastertank_think) (gentity_t *self) -> void {
	if (++self->s.frame < 293)
		self->nextthink = level.time + 10_hz;
	else {
		self->s.frame = 254;
		self->nextthink = level.time + 10_hz;
	}
}

void SP_misc_eastertank(gentity_t *ent) {
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_BBOX;
	ent->mins = { -32, -32, -16 };
	ent->maxs = { 32, 32, 32 };
	ent->s.modelindex = gi.modelindex("models/monsters/tank/tris.md2");
	ent->s.frame = 254;
	ent->think = misc_eastertank_think;
	ent->nextthink = level.time + 20_hz;
	gi.linkentity(ent);
}

/*QUAKED misc_easterchick (1 .5 0) (-32 -32 0) (32 32 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */

THINK(misc_easterchick_think) (gentity_t *self) -> void {
	if (++self->s.frame < 247)
		self->nextthink = level.time + 10_hz;
	else {
		self->s.frame = 208;
		self->nextthink = level.time + 10_hz;
	}
}

void SP_misc_easterchick(gentity_t *ent) {
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_BBOX;
	ent->mins = { -32, -32, 0 };
	ent->maxs = { 32, 32, 32 };
	ent->s.modelindex = gi.modelindex("models/monsters/bitch/tris.md2");
	ent->s.frame = 208;
	ent->think = misc_easterchick_think;
	ent->nextthink = level.time + 20_hz;
	gi.linkentity(ent);
}

/*QUAKED misc_easterchick2 (1 .5 0) (-32 -32 0) (32 32 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */

THINK(misc_easterchick2_think) (gentity_t *self) -> void {
	if (++self->s.frame < 287)
		self->nextthink = level.time + 10_hz;
	else {
		self->s.frame = 248;
		self->nextthink = level.time + 10_hz;
	}
}

void SP_misc_easterchick2(gentity_t *ent) {
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_BBOX;
	ent->mins = { -32, -32, 0 };
	ent->maxs = { 32, 32, 32 };
	ent->s.modelindex = gi.modelindex("models/monsters/bitch/tris.md2");
	ent->s.frame = 248;
	ent->think = misc_easterchick2_think;
	ent->nextthink = level.time + 20_hz;
	gi.linkentity(ent);
}

/*QUAKED monster_commander_body (1 .5 0) (-32 -32 0) (32 32 48) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Not really a monster, this is the Tank Commander's decapitated body.
There should be a item_commander_head that has this as it's target.
*/

THINK(commander_body_think) (gentity_t *self) -> void {
	if (++self->s.frame < 24)
		self->nextthink = level.time + 10_hz;
	else
		self->nextthink = 0_ms;

	if (self->s.frame == 22)
		gi.sound(self, CHAN_BODY, gi.soundindex("tank/thud.wav"), 1, ATTN_NORM, 0);
}

USE(commander_body_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	self->think = commander_body_think;
	self->nextthink = level.time + 10_hz;
	gi.sound(self, CHAN_BODY, gi.soundindex("tank/pain.wav"), 1, ATTN_NORM, 0);
}

THINK(commander_body_drop) (gentity_t *self) -> void {
	self->movetype = MOVETYPE_TOSS;
	self->s.origin[2] += 2;
}

void SP_monster_commander_body(gentity_t *self) {
	self->movetype = MOVETYPE_NONE;
	self->solid = SOLID_BBOX;
	self->model = "models/monsters/commandr/tris.md2";
	self->s.modelindex = gi.modelindex(self->model);
	self->mins = { -32, -32, 0 };
	self->maxs = { 32, 32, 48 };
	self->use = commander_body_use;
	self->takedamage = true;
	self->flags = FL_GODMODE;
	gi.linkentity(self);

	gi.soundindex("tank/thud.wav");
	gi.soundindex("tank/pain.wav");

	self->think = commander_body_drop;
	self->nextthink = level.time + 50_hz;
}

/*QUAKED misc_banner (1 .5 0) (-4 -4 -4) (4 4 4) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
The origin is the bottom of the banner.
The banner is 128 tall.
model="models/objects/banner/tris.md2"
*/
static THINK(misc_banner_think) (gentity_t *ent) -> void {
	ent->s.frame = (ent->s.frame + 1) % 16;
	ent->nextthink = level.time + 10_hz;
}

void SP_misc_banner(gentity_t *ent) {
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_NOT;
	ent->s.modelindex = gi.modelindex("models/objects/banner/tris.md2");
	ent->s.frame = irandom(16);
	gi.linkentity(ent);

	ent->think = misc_banner_think;
	ent->nextthink = level.time + 10_hz;
}

/*-----------------------------------------------------------------------*/
/*QUAKED misc_ctf_banner (1 .5 0) (-4 -64 0) (4 64 248) TEAM_BLUE x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
The origin is the bottom of the banner.
The banner is 248 tall.
*/
THINK(misc_ctf_banner_think) (gentity_t *ent) -> void {
	ent->s.frame = (ent->s.frame + 1) % 16;
	ent->nextthink = level.time + 10_hz;
}

constexpr spawnflags_t SPAWNFLAG_CTF_BANNER_BLUE = 1_spawnflag;

void SP_misc_ctf_banner(gentity_t *ent) {
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_NOT;
	ent->s.modelindex = gi.modelindex("models/ctf/banner/tris.md2");
	if (ent->spawnflags.has(SPAWNFLAG_CTF_BANNER_BLUE)) // TEAM_BLUE
		ent->s.skinnum = 1;

	ent->s.frame = irandom(16);
	gi.linkentity(ent);

	ent->think = misc_ctf_banner_think;
	ent->nextthink = level.time + 10_hz;
}

/*QUAKED misc_ctf_small_banner (1 .5 0) (-4 -32 0) (4 32 124) TEAM_BLUE x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
The origin is the bottom of the banner.
The banner is 124 tall.
*/
void SP_misc_ctf_small_banner(gentity_t *ent) {
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_NOT;
	ent->s.modelindex = gi.modelindex("models/ctf/banner/small.md2");
	if (ent->spawnflags.has(SPAWNFLAG_CTF_BANNER_BLUE)) // TEAM_BLUE
		ent->s.skinnum = 1;

	ent->s.frame = irandom(16);
	gi.linkentity(ent);

	ent->think = misc_ctf_banner_think;
	ent->nextthink = level.time + 10_hz;
}

/*QUAKED misc_deadsoldier (1 .5 0) (-16 -16 0) (16 16 16) ON_BACK ON_STOMACH BACK_DECAP FETAL_POS SIT_DECAP IMPALED x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This is the dead player model. Comes in 6 exciting different poses!
*/

constexpr spawnflags_t SPAWNFLAGS_DEADSOLDIER_ON_BACK = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAGS_DEADSOLDIER_ON_STOMACH = 2_spawnflag;
constexpr spawnflags_t SPAWNFLAGS_DEADSOLDIER_BACK_DECAP = 4_spawnflag;
constexpr spawnflags_t SPAWNFLAGS_DEADSOLDIER_FETAL_POS = 8_spawnflag;
constexpr spawnflags_t SPAWNFLAGS_DEADSOLDIER_SIT_DECAP = 16_spawnflag;
constexpr spawnflags_t SPAWNFLAGS_DEADSOLDIER_IMPALED = 32_spawnflag;

DIE(misc_deadsoldier_die) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod) -> void {
	if (self->health > -30)
		return;

	gi.sound(self, CHAN_BODY, gi.soundindex("misc/udeath.wav"), 1, ATTN_NORM, 0);
	ThrowGibs(self, damage, {
		{ 4, "models/objects/gibs/sm_meat/tris.md2" },
		{ "models/objects/gibs/head2/tris.md2", GIB_HEAD }
		});
}

void SP_misc_deadsoldier(gentity_t *ent) {
	if (deathmatch->integer) { // auto-remove for deathmatch
		G_FreeEntity(ent);
		return;
	}

	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_BBOX;
	ent->s.modelindex = gi.modelindex("models/deadbods/dude/tris.md2");

	// Defaults to frame 0
	if (ent->spawnflags.has(SPAWNFLAGS_DEADSOLDIER_ON_STOMACH))
		ent->s.frame = 1;
	else if (ent->spawnflags.has(SPAWNFLAGS_DEADSOLDIER_BACK_DECAP))
		ent->s.frame = 2;
	else if (ent->spawnflags.has(SPAWNFLAGS_DEADSOLDIER_FETAL_POS))
		ent->s.frame = 3;
	else if (ent->spawnflags.has(SPAWNFLAGS_DEADSOLDIER_SIT_DECAP))
		ent->s.frame = 4;
	else if (ent->spawnflags.has(SPAWNFLAGS_DEADSOLDIER_IMPALED))
		ent->s.frame = 5;
	else if (ent->spawnflags.has(SPAWNFLAGS_DEADSOLDIER_ON_BACK))
		ent->s.frame = 0;
	else
		ent->s.frame = 0;

	ent->mins = { -16, -16, 0 };
	ent->maxs = { 16, 16, 16 };
	ent->deadflag = true;
	ent->takedamage = true;
	// nb: SVF_MONSTER is here so it bleeds
	ent->svflags |= SVF_MONSTER | SVF_DEADMONSTER;
	ent->die = misc_deadsoldier_die;
	ent->monsterinfo.aiflags |= AI_GOOD_GUY | AI_DO_NOT_COUNT;

	gi.linkentity(ent);
}

// Scripted ship, flyby, missile and nuke misc entities live in g_misc_vehicles.cpp.

/*QUAKED light_mine1 (0 1 0) (-2 -2 -12) (2 2 12) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
void SP_light_mine1(gentity_t *ent) {
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_NOT;
	ent->svflags = SVF_DEADMONSTER;
	ent->s.modelindex = gi.modelindex("models/objects/minelite/light1/tris.md2");
	gi.linkentity(ent);
}

/*QUAKED light_mine2 (0 1 0) (-2 -2 -12) (2 2 12) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
void SP_light_mine2(gentity_t *ent) {
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_NOT;
	ent->svflags = SVF_DEADMONSTER;
	ent->s.modelindex = gi.modelindex("models/objects/minelite/light2/tris.md2");
	gi.linkentity(ent);
}

/*QUAKED misc_gib_arm (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Intended for use with the target_spawner
*/
void SP_misc_gib_arm(gentity_t *ent) {
	gi.setmodel(ent, "models/objects/gibs/arm/tris.md2");
	ent->solid = SOLID_NOT;
	ent->s.effects |= EF_GIB;
	ent->takedamage = true;
	ent->die = gib_die;
	ent->movetype = MOVETYPE_TOSS;
	ent->deadflag = true;
	ent->avelocity[0] = frandom(200);
	ent->avelocity[1] = frandom(200);
	ent->avelocity[2] = frandom(200);
	ent->think = G_FreeEntity;
	ent->nextthink = level.time + 10_sec;
	gi.linkentity(ent);
}

/*QUAKED misc_gib_leg (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Intended for use with the target_spawner
*/
void SP_misc_gib_leg(gentity_t *ent) {
	gi.setmodel(ent, "models/objects/gibs/leg/tris.md2");
	ent->solid = SOLID_NOT;
	ent->s.effects |= EF_GIB;
	ent->takedamage = true;
	ent->die = gib_die;
	ent->movetype = MOVETYPE_TOSS;
	ent->deadflag = true;
	ent->avelocity[0] = frandom(200);
	ent->avelocity[1] = frandom(200);
	ent->avelocity[2] = frandom(200);
	ent->think = G_FreeEntity;
	ent->nextthink = level.time + 10_sec;
	gi.linkentity(ent);
}

/*QUAKED misc_gib_head (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Intended for use with the target_spawner
*/
void SP_misc_gib_head(gentity_t *ent) {
	gi.setmodel(ent, "models/objects/gibs/head/tris.md2");
	ent->solid = SOLID_NOT;
	ent->s.effects |= EF_GIB;
	ent->takedamage = true;
	ent->die = gib_die;
	ent->movetype = MOVETYPE_TOSS;
	ent->deadflag = true;
	ent->avelocity[0] = frandom(200);
	ent->avelocity[1] = frandom(200);
	ent->avelocity[2] = frandom(200);
	ent->think = G_FreeEntity;
	ent->nextthink = level.time + 10_sec;
	gi.linkentity(ent);
}

/*QUAKED misc_flare (1.0 1.0 0.0) (-32 -32 -32) (32 32 32) RED GREEN BLUE LOCK_ANGLE x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Creates a flare seen in the N64 version.
*/

static constexpr spawnflags_t SPAWNFLAG_FLARE_RED = 1_spawnflag;
static constexpr spawnflags_t SPAWNFLAG_FLARE_GREEN = 2_spawnflag;
static constexpr spawnflags_t SPAWNFLAG_FLARE_BLUE = 4_spawnflag;
static constexpr spawnflags_t SPAWNFLAG_FLARE_LOCK_ANGLE = 8_spawnflag;

static USE(misc_flare_use) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	ent->svflags ^= SVF_NOCLIENT;
	gi.linkentity(ent);
}

void SP_misc_flare(gentity_t *ent) {
	ent->s.modelindex = 1;
	ent->s.renderfx = RF_FLARE;
	ent->solid = SOLID_NOT;
	ent->s.scale = st.radius;

	if (ent->spawnflags.has(SPAWNFLAG_FLARE_RED))
		ent->s.renderfx |= RF_SHELL_RED;

	if (ent->spawnflags.has(SPAWNFLAG_FLARE_GREEN))
		ent->s.renderfx |= RF_SHELL_GREEN;

	if (ent->spawnflags.has(SPAWNFLAG_FLARE_BLUE))
		ent->s.renderfx |= RF_SHELL_BLUE;

	if (ent->spawnflags.has(SPAWNFLAG_FLARE_LOCK_ANGLE))
		ent->s.renderfx |= RF_FLARE_LOCK_ANGLE;

	if (st.image && *st.image) {
		ent->s.renderfx |= RF_CUSTOMSKIN;
		ent->s.frame = gi.imageindex(st.image);
	}

	ent->mins = { -32, -32, -32 };
	ent->maxs = { 32, 32, 32 };

	ent->s.modelindex2 = st.fade_start_dist;
	ent->s.modelindex3 = st.fade_end_dist;

	if (ent->targetname)
		ent->use = misc_flare_use;

	gi.linkentity(ent);
}

static THINK(misc_hologram_think) (gentity_t *ent) -> void {
	ent->s.angles[YAW] += 100 * gi.frame_time_s;
	ent->nextthink = level.time + FRAME_TIME_MS;
	ent->s.alpha = frandom(0.2f, 0.6f);
}

/*QUAKED misc_hologram (1.0 1.0 0.0) (-16 -16 0) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Ship hologram seen in the N64 version.
*/
void SP_misc_hologram(gentity_t *ent) {
	ent->solid = SOLID_NOT;
	ent->s.modelindex = gi.modelindex("models/ships/strogg1/tris.md2");
	ent->mins = { -16, -16, 0 };
	ent->maxs = { 16, 16, 32 };
	ent->s.effects = EF_HOLOGRAM;
	ent->think = misc_hologram_think;
	ent->nextthink = level.time + FRAME_TIME_MS;
	ent->s.alpha = frandom(0.2f, 0.6f);
	ent->s.scale = 0.75f;
	gi.linkentity(ent);
}


/*QUAKED misc_fireball (0 .5 .8) (-8 -8 -8) (8 8 8) NO_EXPLODE x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Lava Balls. Shamelessly copied from Quake 1, like N64 guys
probably did too.
*/

constexpr spawnflags_t SPAWNFLAG_LAVABALL_NO_EXPLODE = 1_spawnflag;

static TOUCH(fire_touch) (gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	if (self->spawnflags.has(SPAWNFLAG_LAVABALL_NO_EXPLODE)) {
		G_FreeEntity(self);
		return;
	}

	if (other->takedamage)
		T_Damage(other, self, self, vec3_origin, self->s.origin, vec3_origin, 20, 0, DAMAGE_NONE, MOD_EXPLOSIVE);

	if (gi.pointcontents(self->s.origin) & CONTENTS_LAVA)
		G_FreeEntity(self);
	else
		BecomeExplosion1(self);
}

static THINK(fire_fly) (gentity_t *self) -> void {
	gentity_t *fireball = G_Spawn();
	fireball->s.effects = EF_FIREBALL;
	fireball->s.renderfx = RF_MINLIGHT;
	fireball->solid = SOLID_BBOX;
	fireball->movetype = MOVETYPE_TOSS;
	fireball->clipmask = MASK_SHOT;
	fireball->velocity[0] = crandom() * 50;
	fireball->velocity[1] = crandom() * 50;
	fireball->avelocity = { crandom() * 360, crandom() * 360, crandom() * 360 };
	fireball->velocity[2] = (self->speed * 1.75f) + (frandom() * 200);
	fireball->classname = "fireball";
	gi.setmodel(fireball, "models/objects/gibs/sm_meat/tris.md2");
	fireball->s.origin = self->s.origin;
	fireball->nextthink = level.time + 5_sec;
	fireball->think = G_FreeEntity;
	if (!deathmatch->integer)
		fireball->touch = fire_touch;
	fireball->spawnflags = self->spawnflags;
	gi.linkentity(fireball);
	self->nextthink = level.time + random_time(5_sec);
}

void SP_misc_lavaball(gentity_t *self) {
	self->classname = "fireball";
	self->nextthink = level.time + random_time(5_sec);
	self->think = fire_fly;
	if (!self->speed)
		self->speed = 185;
}


void SP_info_landmark(gentity_t *self) {
	self->absmin = self->s.origin;
	self->absmax = self->s.origin;
}

#include "monsters/m_player.h"

static USE(misc_player_mannequin_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	self->monsterinfo.aiflags |= AI_TARGET_ANGER;
	self->enemy = activator;

	switch (self->count) {
	case GESTURE_FLIP_OFF:
		self->s.frame = FRAME_flip01;
		self->monsterinfo.nextframe = FRAME_flip12;
		break;

	case GESTURE_SALUTE:
		self->s.frame = FRAME_salute01;
		self->monsterinfo.nextframe = FRAME_salute11;
		break;

	case GESTURE_TAUNT:
		self->s.frame = FRAME_taunt01;
		self->monsterinfo.nextframe = FRAME_taunt17;
		break;

	case GESTURE_WAVE:
		self->s.frame = FRAME_wave01;
		self->monsterinfo.nextframe = FRAME_wave11;
		break;

	case GESTURE_POINT:
		self->s.frame = FRAME_point01;
		self->monsterinfo.nextframe = FRAME_point12;
		break;
	}
}

static THINK(misc_player_mannequin_think) (gentity_t *self) -> void {
	if (self->teleport_time <= level.time) {
		self->s.frame++;

		if ((self->monsterinfo.aiflags & AI_TARGET_ANGER) == 0) {
			if (self->s.frame > FRAME_stand40) {
				self->s.frame = FRAME_stand01;
			}
		} else {
			if (self->s.frame > self->monsterinfo.nextframe) {
				self->s.frame = FRAME_stand01;
				self->monsterinfo.aiflags &= ~AI_TARGET_ANGER;
				self->enemy = nullptr;
			}
		}

		self->teleport_time = level.time + 10_hz;
	}

	if (self->enemy != nullptr) {
		const vec3_t vec = (self->enemy->s.origin - self->s.origin);
		self->ideal_yaw = vectoyaw(vec);
		M_ChangeYaw(self);
	}

	self->nextthink = level.time + FRAME_TIME_MS;
}

static void SetupMannequinModel(gentity_t *self, const int32_t modelType, const char *weapon, const char *skin) {
	const char *modelName = nullptr;
	const char *defaultSkin = nullptr;

	switch (modelType) {
	case 1: {
		self->s.skinnum = (MAX_CLIENTS - 1);
		modelName = "female";
		defaultSkin = "venus";
		break;
	}

	case 2: {
		self->s.skinnum = (MAX_CLIENTS - 2);
		modelName = "male";
		defaultSkin = "rampage";
		break;
	}

	case 3: {
		self->s.skinnum = (MAX_CLIENTS - 3);
		modelName = "cyborg";
		defaultSkin = "oni911";
		break;
	}

	default: {
		self->s.skinnum = (MAX_CLIENTS - 1);
		modelName = "female";
		defaultSkin = "venus";
		break;
	}
	}

	if (modelName != nullptr) {
		self->model = G_Fmt("players/{}/tris.md2", modelName).data();

		const char *weaponName = nullptr;
		if (weapon != nullptr) {
			weaponName = G_Fmt("players/{}/{}.md2", modelName, weapon).data();
		} else {
			weaponName = G_Fmt("players/{}/{}.md2", modelName, "w_hyperblaster").data();
		}
		self->s.modelindex2 = gi.modelindex(weaponName);

		const char *skinName = nullptr;
		if (skin != nullptr) {
			skinName = G_Fmt("mannequin\\{}/{}", modelName, skin).data();
		} else {
			skinName = G_Fmt("mannequin\\{}/{}", modelName, defaultSkin).data();
		}
		gi.configstring(CS_PLAYERSKINS + self->s.skinnum, skinName);
	}
}

/*QUAKED misc_player_mannequin (1.0 1.0 0.0) (-32 -32 -32) (32 32 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
	Creates a player mannequin that stands around.

	NOTE: this is currently very limited, and only allows one unique model
	from each of the three player model types.

 "distance"		- Sets the type of gesture mannequin when use when triggered
 "height"		- Sets the type of model to use ( valid numbers: 1 - 3 )
 "goals"		- Name of the weapon to use.
 "image"		- Name of the player skin to use.
 "radius"		- How much to scale the model in-game
*/
void SP_misc_player_mannequin(gentity_t *self) {
	self->movetype = MOVETYPE_NONE;
	self->solid = SOLID_BBOX;
	if (!st.was_key_specified("effects"))
		self->s.effects = EF_NONE;
	if (!st.was_key_specified("renderfx"))
		self->s.renderfx = RF_MINLIGHT;
	self->mins = { -16, -16, -24 };
	self->maxs = { 16, 16, 32 };
	self->yaw_speed = 30;
	self->ideal_yaw = 0;
	self->teleport_time = level.time + 10_hz;
	self->s.modelindex = MODELINDEX_PLAYER;
	self->count = st.distance;

	SetupMannequinModel(self, st.height, st.goals, st.image);

	self->s.scale = 1.0f;
	if (ai_model_scale->value > 0.0f) {
		self->s.scale = ai_model_scale->value;
	} else if (st.radius > 0.0f) {
		self->s.scale = st.radius;
	}

	self->mins *= self->s.scale;
	self->maxs *= self->s.scale;

	self->think = misc_player_mannequin_think;
	self->nextthink = level.time + FRAME_TIME_MS;

	if (self->targetname) {
		self->use = misc_player_mannequin_use;
	}

	gi.linkentity(self);
}

/*QUAKED misc_model (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
void SP_misc_model(gentity_t *ent) {
	gi.setmodel(ent, ent->model);
	gi.linkentity(ent);
}


// Continued in g_misc_vehicles.cpp.
