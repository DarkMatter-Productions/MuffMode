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
	// [MuffMode] Horde timed techs: a tech's lifetime travels with it. A dropped tech carries its
	// remaining deadline (ent->timestamp), so re-grabbing resumes that countdown rather than
	// refreshing it; a fresh/champion-dropped tech starts a full g_horde_tech_duration window.
	if (ent->timestamp)
		other->client->tech_expire_time = ent->timestamp;
	else if (GT(GT_HORDE) && g_horde_tech_duration->integer > 0)
		other->client->tech_expire_time = level.time + gtime_t::from_sec(g_horde_tech_duration->integer);
	else
		other->client->tech_expire_time = 0_ms;
	return true;
}

// [MuffMode] Horde timed techs: remove a held tech once its pickup timer elapses (it simply
// vanishes, like an expired powerup). No-op when tech_expire_time is 0 (permanent / not timed).
void Tech_ApplyExpiry(gentity_t *ent) {
	if (!ent->client || !ent->client->tech_expire_time)
		return;
	if (ent->client->tech_expire_time > level.time)
		return;

	gitem_t *tech = Tech_Held(ent);
	ent->client->tech_expire_time = 0_ms;
	if (tech) {
		ent->client->pers.inventory[tech->id] = 0;
		gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/power2.wav"), 1, ATTN_NORM, 0);
	}
}

static gentity_t *Tech_Spawn(gitem_t *item, gentity_t *spot);
static void Tech_ScheduleRelocate(gentity_t *tech);

static gentity_t *FindTechSpawn() {
	return SelectDeathmatchSpawnPoint(nullptr, vec3_origin, SPAWN_FAR_HALF, true, true, false, true).spot;
}

static THINK(Tech_Think) (gentity_t *tech) -> void {
	gentity_t *spot;

	if ((spot = FindTechSpawn()) != nullptr) {
		Tech_Spawn(tech->item, spot);
		G_FreeEntity(tech);
	} else {
		Tech_ScheduleRelocate(tech);
	}
}

// [MuffMode] In Horde, techs stay where they spawn/drop by default (no relocation churn);
// other modes (and g_horde_tech_relocate 1) keep the periodic relocate to a new spot.
static void Tech_ScheduleRelocate(gentity_t *tech) {
	if (GT(GT_HORDE) && !g_horde_tech_relocate->integer) {
		tech->nextthink = 0_ms; // stay put
		return;
	}
	tech->nextthink = level.time + TECH_TIMEOUT;
	tech->think = Tech_Think;
}

// [MuffMode] A dropped tech carrying a timed-tech deadline (tech->timestamp) vanishes when that
// deadline passes, so its lifetime keeps ticking on the ground and dropping/re-grabbing it can't
// refresh the timer.
static THINK(Tech_WorldExpire) (gentity_t *tech) -> void {
	G_FreeEntity(tech);
}

// Schedule a dropped tech: if it carries a timer deadline, expire it at that time; otherwise
// fall back to the normal relocation behavior.
static void Tech_ScheduleDropped(gentity_t *tech) {
	if (tech->timestamp) {
		tech->nextthink = tech->timestamp;
		tech->think = Tech_WorldExpire;
	} else {
		Tech_ScheduleRelocate(tech);
	}
}

static THINK(Tech_Make_Touchable) (gentity_t *tech) -> void {
	tech->touch = Touch_Item;
	Tech_ScheduleDropped(tech);
}

void Tech_Drop(gentity_t *ent, gitem_t *item) {
	gentity_t *tech;

	tech = Drop_Item(ent, item);
	tech->timestamp = ent->client->tech_expire_time; // carry the remaining timer with the tech
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
			dropped->timestamp = ent->client->tech_expire_time; // carry the remaining timer
			Tech_ScheduleDropped(dropped);
			dropped->owner = nullptr;
			ent->client->pers.inventory[tech_ids[i]] = 0;
		}
	}
}

// Spawn a tech at `origin`. `toss` pops it out with a random horizontal nudge (used at spawn
// points, which sit in open areas); when false the tech settles straight down onto the spot it
// was placed at, so a validated random floor position isn't flung into a pit or off a ledge.
static gentity_t *Tech_SpawnAtOrigin(gitem_t *item, const vec3_t &origin, bool toss) {
	gentity_t	*ent = G_Spawn();

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

	ent->s.origin = origin;

	if (toss) {
		vec3_t forward, right;
		vec3_t angles = { 0, (float)irandom(360), 0 };
		AngleVectors(angles, forward, right, nullptr);
		ent->velocity = forward * 100;
		ent->velocity[2] = 300;
	}

	Tech_ScheduleRelocate(ent);

	gi.linkentity(ent);
	return ent;
}

static gentity_t *Tech_Spawn(gitem_t *item, gentity_t *spot) {
	vec3_t origin = spot->s.origin;
	origin[2] += 16;
	return Tech_SpawnAtOrigin(item, origin, true);
}

bool AllowTechs() {
	if (!strcmp(g_allow_techs->string, "auto")) {
		// [MuffMode] "auto" enables techs in the modes built around them: CTF and Horde.
		if (GT(GT_HORDE))
			return ItemSpawnsEnabled();
		return !!(GT(GT_CTF) && !(g_instagib->integer || GT(GT_INSTAGIB)) && !(g_nadefest->integer || GT(GT_NADEFEST)));
	} else
		return !!(g_allow_techs->integer && ItemSpawnsEnabled());
}

void Tech_ReturnToWorld(item_id_t tech_id, const vec3_t &fallback_origin,
	gtime_t expire_time)
{
	gitem_t *item = GetItemByIndex(tech_id);
	if (!item || !(item->flags & IF_TECH) || !AllowTechs())
		return;
	if (expire_time && expire_time <= level.time)
		return;

	// Return exactly the one copy owned by the expiring snapshot. Horde permits
	// same-type copies from wave rolls and champion rewards even in otherwise
	// unique configurations, so global same-type suppression would lose items.

	gentity_t *tech = nullptr;
	if (gentity_t *spot = FindTechSpawn())
		tech = Tech_Spawn(item, spot);
	else
		tech = Tech_SpawnAtOrigin(item, fallback_origin, false);

	if (expire_time) {
		tech->timestamp = expire_time;
		Tech_ScheduleDropped(tech);
	}
}

static THINK(Tech_SpawnAll) (gentity_t *ent) -> void {
	gentity_t *spot;

	if (!AllowTechs())
		return;

	int num = 0;
	if (!strcmp(g_allow_techs->string, "auto"))
		num = 1;
	else
		// g_allow_techs is documented and treated everywhere else as a
		// boolean. Normalize all enabled numeric values to one copy of each
		// tech; negative or oversized values must never become an unbounded
		// size_t loop or exhaust the entity pool.
		num = !!g_allow_techs->integer;

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

	// [MuffMode] In Horde reset mode the wave lifecycle owns tech spawning (cleared at the
	// countdown, spawned at wave start), so skip the generic map-load spawn to avoid
	// double-spawning / pre-placing at DM spawn points. Persist mode keeps this spawn.
	if (GT(GT_HORDE) && g_horde_tech_reset_each_wave->integer)
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

// [MuffMode] Horde: remove all techs (world entities + held) — called at the countdown to
// the next wave so none linger during the downtime between waves.
void Tech_HordeClear() {
	gentity_t *ent;
	size_t i;
	const size_t entity_count = ItemEntityCount();

	for (ent = g_entities + 1, i = 1; i < entity_count; i++, ent++) {
		if (ent->inuse && ent->item && (ent->item->flags & IF_TECH))
			G_FreeEntity(ent);
	}

	for (auto player : active_clients())
		for (size_t j = 0; j < q_countof(tech_ids); j++)
			player->client->pers.inventory[tech_ids[j]] = 0;
}

// How many techs to spawn this Horde wave: fixed (g_horde_tech_count 1-4) or adaptive
// (0 = ceil(players/2)), clamped to the number of tech types.
static int Tech_HordeWaveCount() {
	const int types = static_cast<int>(q_countof(tech_ids));
	const int configured = g_horde_tech_count->integer;
	if (configured > 0)
		return std::min(configured, types);

	const int adaptive = (level.num_playing_human_clients + 1) / 2; // ceil(players / 2)
	return std::clamp(adaptive, 1, types);
}

// [MuffMode] Horde: spawn this wave's techs at wave start. By default each is picked
// independently (duplicates allowed — e.g. three AutoDocs), matching Horde's chaos;
// g_horde_tech_unique 1 reverts to a distinct, no-repeat random subset.
void Tech_HordeSpawnWave() {
	if (!AllowTechs())
		return;

	const int types = static_cast<int>(q_countof(tech_ids));
	const int count = Tech_HordeWaveCount();
	if (count < 1)
		return;

	const bool unique = g_horde_tech_unique->integer != 0;

	// Unique mode: shuffle the indices once and take the first `count` (distinct types).
	int order[q_countof(tech_ids)];
	if (unique) {
		for (int i = 0; i < types; i++)
			order[i] = i;
		for (int i = types - 1; i > 0; i--)
			std::swap(order[i], order[irandom(i + 1)]);
	}

	for (int k = 0; k < count; k++) {
		const int idx = unique ? order[k] : irandom(types);
		gitem_t *it = GetItemByIndex(tech_ids[idx]);
		if (!it)
			continue;
		// Prefer scattering anywhere on the map's walkable floor; fall back to a DM
		// spawn point if no valid free-floor spot is found.
		vec3_t pos;
		if (g_horde_tech_spawn_anywhere->integer && MM_Horde_PickTechSpawnPos(pos))
			Tech_SpawnAtOrigin(it, pos, false); // settle in place on the validated floor spot
		else if (gentity_t *spot = FindTechSpawn())
			Tech_Spawn(it, spot);
	}
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
