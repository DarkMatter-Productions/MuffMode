// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include "g_local.h"

static gentity_t *QuadHog_FindSpawn() {
	return SelectDeathmatchSpawnPoint(nullptr, vec3_origin, SPAWN_FAR_HALF, true, true, false, true).spot;
}

static size_t ItemEntityCount() {
	return min(static_cast<size_t>(globals.num_entities), static_cast<size_t>(game.maxentities));
}

static void QuadHod_ClearAll() {
	gentity_t *ent;
	gentity_t *end = &g_entities[ItemEntityCount()];

	for (ent = g_entities; ent < end; ent++) {

		if (!ent->inuse)
			continue;

		if (ent->client) {
			ent->client->pu_time_quad = 0_ms;
			ent->client->pers.inventory[IT_POWERUP_QUAD] = 0;
			continue;
		}

		if (!ent->classname)
			continue;

		if (!ent->item)
			continue;

		if (ent->item->id != IT_POWERUP_QUAD)
			continue;

		G_FreeEntity(ent);
	}
}

void QuadHog_Spawn(gitem_t *item, gentity_t *spot, bool reset) {
	gentity_t *ent;
	vec3_t	 forward, right;
	vec3_t	 angles = vec3_origin;

	QuadHod_ClearAll();

	ent = G_Spawn();

	ent->classname = item->classname;
	ent->item = item;
	ent->spawnflags = SPAWNFLAG_ITEM_DROPPED;
	ent->s.effects = item->world_model_flags | EF_COLOR_SHELL;
	ent->s.renderfx = RF_GLOW | RF_NO_LOD | RF_SHELL_BLUE;
	ent->mins = { -15, -15, -15 };
	ent->maxs = { 15, 15, 15 };
	gi.setmodel(ent, item->world_model);
	ent->solid = SOLID_TRIGGER;
	ent->movetype = MOVETYPE_TOSS;
	ent->touch = Touch_Item;
	ent->owner = ent;
	ent->nextthink = level.time + 30_sec;
	ent->think = QuadHog_DoSpawn;

	angles[PITCH] = 0;
	angles[YAW] = (float)irandom(360);
	angles[ROLL] = 0;

	AngleVectors(angles, forward, right, nullptr);
	ent->s.origin = spot->s.origin;
	ent->s.origin[2] += 16;
	ent->velocity = forward * 100;
	ent->velocity[2] = 300;

	gi.LocBroadcast_Print(PRINT_CENTER, "The Quad {}!\n", reset ? "respawned" : "has spawned");
	gi.sound(ent, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex("misc/alarm.wav"), 1, ATTN_NONE, 0);

	gi.linkentity(ent);
}

THINK(QuadHog_DoSpawn) (gentity_t *ent) -> void {
	gentity_t *spot;
	gitem_t *it = GetItemByIndex(IT_POWERUP_QUAD);

	if (!it)
		return;

	if ((spot = QuadHog_FindSpawn()) != nullptr)
		QuadHog_Spawn(it, spot, false);

	if (ent)
		G_FreeEntity(ent);
}

THINK(QuadHog_DoReset) (gentity_t *ent) -> void {
	gentity_t *spot;
	gitem_t *it = GetItemByIndex(IT_POWERUP_QUAD);

	if (!it)
		return;

	if ((spot = QuadHog_FindSpawn()) != nullptr)
		QuadHog_Spawn(it, spot, true);

	if (ent)
		G_FreeEntity(ent);
}

void QuadHog_SetupSpawn(gtime_t delay) {
	gentity_t *ent;

	if (!g_quadhog->integer)
		return;

	ent = G_Spawn();
	ent->nextthink = level.time + delay;
	ent->think = QuadHog_DoSpawn;
}

//======================================================================

/*------------------------------------------------------------------------*/
/* TECH																	  */
/*------------------------------------------------------------------------*/

constexpr gtime_t TECH_TIMEOUT = 60_sec; // seconds before techs spawn again

static bool Tech_PlayerHasATech(gentity_t *ent) {
	if (Tech_Held(ent) != nullptr) {
		if (level.time - ent->client->tech_last_message_time > 5_sec) {
			gi.LocCenter_Print(ent, "$g_already_have_tech");
			ent->client->tech_last_message_time = level.time;
		}
		return true; // has this one
	}
	return false;
}

gitem_t *Tech_Held(gentity_t *ent) {
	for (size_t i = 0; i < q_countof(tech_ids); i++) {
		if (ent->client->pers.inventory[tech_ids[i]])
			return GetItemByIndex(tech_ids[i]);
	}
	return nullptr;
}

bool Tech_Pickup(gentity_t *ent, gentity_t *other) {
	// client only gets one tech
	if (Tech_PlayerHasATech(other))
		return false;

	other->client->pers.inventory[ent->item->id]++;
	other->client->tech_regen_time = level.time;
	return true;
}

static void Tech_Spawn(gitem_t *item, gentity_t *spot);

static gentity_t *FindTechSpawn() {
	return SelectDeathmatchSpawnPoint(nullptr, vec3_origin, SPAWN_FAR_HALF, true, true, false, true).spot;
}

static THINK(Tech_Think) (gentity_t *tech) -> void {
	gentity_t *spot;

	if ((spot = FindTechSpawn()) != nullptr) {
		Tech_Spawn(tech->item, spot);
		G_FreeEntity(tech);
	} else {
		tech->nextthink = level.time + TECH_TIMEOUT;
		tech->think = Tech_Think;
	}
}

static THINK(Tech_Make_Touchable) (gentity_t *tech) -> void {
	tech->touch = Touch_Item;
	tech->nextthink = level.time + TECH_TIMEOUT;
	tech->think = Tech_Think;
}

void Tech_Drop(gentity_t *ent, gitem_t *item) {
	gentity_t *tech;

	tech = Drop_Item(ent, item);
	tech->nextthink = level.time + 1_sec;
	tech->think = Tech_Make_Touchable;
	ent->client->pers.inventory[item->id] = 0;
}

void Tech_DeadDrop(gentity_t *ent) {
	gentity_t *dropped;
	int		 i;

	i = 0;
	for (; i < q_countof(tech_ids); i++) {
		if (ent->client->pers.inventory[tech_ids[i]]) {
			dropped = Drop_Item(ent, GetItemByIndex(tech_ids[i]));
			// hack the velocity to make it bounce random
			dropped->velocity[0] = crandom_open() * 300;
			dropped->velocity[1] = crandom_open() * 300;
			dropped->nextthink = level.time + TECH_TIMEOUT;
			dropped->think = Tech_Think;
			dropped->owner = nullptr;
			ent->client->pers.inventory[tech_ids[i]] = 0;
		}
	}
}

static void Tech_Spawn(gitem_t *item, gentity_t *spot) {
	gentity_t	*ent = G_Spawn();
	vec3_t	forward, right;
	vec3_t	angles = { 0, (float)irandom(360), 0 };

	ent->classname = item->classname;
	ent->item = item;
	ent->spawnflags = SPAWNFLAG_ITEM_DROPPED;
	ent->s.effects = item->world_model_flags;
	ent->s.renderfx = RF_GLOW | RF_NO_LOD;
	ent->mins = { -15, -15, -15 };
	ent->maxs = { 15, 15, 15 };
	gi.setmodel(ent, ent->item->world_model);
	ent->solid = SOLID_TRIGGER;
	ent->movetype = MOVETYPE_TOSS;
	ent->touch = Touch_Item;
	ent->owner = ent;

	AngleVectors(angles, forward, right, nullptr);
	ent->s.origin = spot->s.origin;
	ent->s.origin[2] += 16;
	ent->velocity = forward * 100;
	ent->velocity[2] = 300;

	ent->nextthink = level.time + TECH_TIMEOUT;
	ent->think = Tech_Think;

	gi.linkentity(ent);
}

bool AllowTechs() {
	if (!strcmp(g_allow_techs->string, "auto"))
		return !!(GT(GT_CTF) && !(g_instagib->integer || GT(GT_INSTAGIB)) && !(g_nadefest->integer || GT(GT_NADEFEST)));
	else
		return !!(g_allow_techs->integer && ItemSpawnsEnabled());
}

static THINK(Tech_SpawnAll) (gentity_t *ent) -> void {
	gentity_t *spot;

	if (!AllowTechs())
		return;

	int num = 0;
	if (!strcmp(g_allow_techs->string, "auto"))
		num = 1;
	else
		num = g_allow_techs->integer;

	if (!num)
		return;

	gitem_t *it = nullptr;
	for (size_t i = 0; i < q_countof(tech_ids); i++) {
		it = GetItemByIndex(tech_ids[i]);
		if (!it)
			continue;
		for (size_t j = 0; j < num; j++)
			if ((spot = FindTechSpawn()) != nullptr)
				Tech_Spawn(it, spot);
	}
	if (ent)
		G_FreeEntity(ent);
}

void Tech_SetupSpawn() {
	if (!AllowTechs())
		return;

	gentity_t *ent = G_Spawn();
	ent->nextthink = level.time + 2_sec;
	ent->think = Tech_SpawnAll;
}

void Tech_Reset() {
	gentity_t *ent;
	size_t i;
	const size_t entity_count = ItemEntityCount();

	for (ent = g_entities + 1, i = 1; i < entity_count; i++, ent++) {
		if (ent->inuse)
			if (ent->item && (ent->item->flags & IF_TECH))
				G_FreeEntity(ent);
	}
	Tech_SetupSpawn();
	//Tech_SpawnAll(nullptr);
}

int Tech_ApplyDisruptorShield(gentity_t *ent, int dmg) {
	float volume = 1.0;

	if (ent->client && ent->client->silencer_shots)
		volume = 0.2f;

	if (dmg && ent->client && ent->client->pers.inventory[IT_TECH_DISRUPTOR_SHIELD]) {
		// make noise
		gi.sound(ent, CHAN_AUX, gi.soundindex("ctf/tech1.wav"), volume, ATTN_NORM, 0);
		return dmg / 2;
	}
	return dmg;
}

int Tech_ApplyPowerAmp(gentity_t *ent, int dmg) {
	if (dmg && ent->client && ent->client->pers.inventory[IT_TECH_POWER_AMP]) {
		return dmg * 2;
	}
	return dmg;
}

bool Tech_ApplyPowerAmpSound(gentity_t *ent) {
	float volume = 1.0;

	if (ent->client && ent->client->silencer_shots)
		volume = 0.2f;

	if (ent->client &&
		ent->client->pers.inventory[IT_TECH_POWER_AMP]) {
		if (ent->client->tech_sound_time < level.time) {
			ent->client->tech_sound_time = level.time + 1_sec;
			if (ent->client->pu_time_quad > level.time)
				gi.sound(ent, CHAN_AUX, gi.soundindex("ctf/tech2x.wav"), volume, ATTN_NORM, 0);
			else
				gi.sound(ent, CHAN_AUX, gi.soundindex("ctf/tech2.wav"), volume, ATTN_NORM, 0);
		}
		return true;
	}
	return false;
}

bool Tech_ApplyTimeAccel(gentity_t *ent) {
	if (ent->client &&
		ent->client->pers.inventory[IT_TECH_TIME_ACCEL])
		return true;
	return false;
}

void Tech_ApplyTimeAccelSound(gentity_t *ent) {
	float volume = 1.0;

	if (ent->client && ent->client->silencer_shots)
		volume = 0.2f;

	if (ent->client &&
		ent->client->pers.inventory[IT_TECH_TIME_ACCEL] &&
		ent->client->tech_sound_time < level.time) {
		ent->client->tech_sound_time = level.time + 1_sec;
		gi.sound(ent, CHAN_AUX, gi.soundindex("ctf/tech3.wav"), volume, ATTN_NORM, 0);
	}
}
