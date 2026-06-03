// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_horde.h"

#include <climits>

namespace {
// Weighted spawn table row. monsters[] uses drops[] for death loot; items[] uses classname only.
struct weighted_item_t {
	const char             *classname;
	int32_t                 min_level = -1, max_level = -1;
	float                   weight = 1.0f;
	float                   lvl_w_adjust = 0;
	std::array<item_id_t, 4> drops = {};
};

constexpr weighted_item_t items[] = {
	{ "item_health_small" },

	{ "item_health", -1, -1, 1.0f, 0 },
	{ "item_health_large", -1, -1, 0.85f, 0 },

	{ "item_armor_shard" },
	{ "item_armor_jacket", -1, 4, 0.65f, 0 },
	{ "item_armor_combat", 2, -1, 0.62f, 0 },
	{ "item_armor_body", 4, -1, 0.35f, 0 },

	{ "weapon_shotgun", -1, -1, 0.98f, 0 },
	{ "weapon_supershotgun", 2, -1, 1.02f, 0 },
	{ "weapon_machinegun", -1, -1, 1.05f, 0 },
	{ "weapon_chaingun", 3, -1, 1.01f, 0 },
	{ "weapon_grenadelauncher", 4, -1, 0.75f, 0 },

	{ "ammo_shells", -1, -1, 1.25f, 0 },
	{ "ammo_bullets", -1, -1, 1.25f, 0 },
	{ "ammo_grenades", 2, -1, 1.25f, 0 },
};

constexpr weighted_item_t monsters[] = {
	{ "monster_soldier_light", -1, 7, 1.50f, -0.45f, { IT_HEALTH_SMALL } },
	{ "monster_soldier", -1, 7, 0.85f, -0.25f, { IT_AMMO_BULLETS_SMALL, IT_HEALTH_SMALL } },
	{ "monster_soldier_ss", 2, 7, 1.01f, -0.125f, { IT_AMMO_SHELLS_SMALL, IT_HEALTH_SMALL } },
	{ "monster_soldier_hypergun", 2, 9, 1.2f, 0.15f, { IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL } },
	{ "monster_soldier_lasergun", 3, 9, 1.15f, 0.2f, { IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL } },
	{ "monster_soldier_ripper", 3, 9, 1.25f, 0.25f, { IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL } },
	{ "monster_infantry", 3, 16, 1.05f, 0.125f, { IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS } },
	{ "monster_gunner", 4, 16, 1.08f, 0.5f, { IT_AMMO_GRENADES, IT_AMMO_BULLETS_SMALL } },
	{ "monster_berserk", 4, 16, 1.05f, 0.1f, { IT_ARMOR_SHARD } },
	{ "monster_parasite", 5, 16, 1.04f, -0.08f, {} },
	{ "monster_gladiator", 5, 16, 1.07f, 0.3f, { IT_AMMO_SLUGS } },
	{ "monster_gekk", 6, 16, 0.99f, -0.15f, {} },
	{ "monster_brain", 6, 16, 0.95f, 0, { IT_AMMO_CELLS_SMALL } },
	{ "monster_flyer", 6, 16, 0.92f, 0.15f, { IT_AMMO_CELLS_SMALL } },
	{ "monster_floater", 7, 16, 0.9f, 0, {} },
	{ "monster_mutant", 7, 16, 0.85f, 0, {} },
	{ "monster_hover", 8, 16, 0.8f, 0, {} },
	{ "monster_guncmdr", 8, -1, 0, 0.125f, { IT_AMMO_GRENADES, IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS, IT_AMMO_CELLS_SMALL } },
	{ "monster_chick", 9, 20, 1.01f, -0.05f, { IT_AMMO_ROCKETS_SMALL, IT_AMMO_ROCKETS } },
	{ "monster_daedalus", 9, -1, 0.99f, 0.05f, { IT_AMMO_CELLS_SMALL } },
	{ "monster_medic", 10, 16, 0.95f, -0.05f, { IT_HEALTH_SMALL, IT_HEALTH_MEDIUM } },
	{ "monster_tank", 11, -1, 0.85f, 0, { IT_AMMO_ROCKETS } },
	{ "monster_chick_heat", 12, -1, 0.87f, 0.065f, { IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS } },
	{ "monster_tank_commander", 12, -1, 0.45f, 0.16f, { IT_AMMO_ROCKETS_SMALL, IT_AMMO_BULLETS_SMALL, IT_AMMO_ROCKETS, IT_AMMO_BULLETS } },
	{ "monster_medic_commander", 13, -1, 0.4f, 0.15f, { IT_AMMO_CELLS_SMALL, IT_HEALTH_MEDIUM, IT_HEALTH_LARGE } },
	{ "monster_kamikaze", 13, -1, 0.85f, 0.04f, {} },
};

struct picked_item_t {
	const weighted_item_t *item;
	float                  weight;
};

gentity_t *FindClosestPlayerToPoint(vec3_t point)
{
	float      bestplayerdistance = 9999999;
	gentity_t *closest = nullptr;

	for (auto ec : active_clients()) {
		if (ec->health <= 0 || ec->client->eliminated)
			continue;

		vec3_t v = point - ec->s.origin;
		float  playerdistance = v.length();

		if (playerdistance < bestplayerdistance) {
			bestplayerdistance = playerdistance;
			closest = ec;
		}
	}

	return closest;
}

gitem_t *Horde_PickItem()
{
	static std::array<picked_item_t, q_countof(items)> picked_items;
	size_t                                              num_picked_items = 0;
	float                                               total_weight = 0;

	for (auto &item : items) {
		if (item.min_level != -1 && level.round_number < item.min_level)
			continue;
		if (item.max_level != -1 && level.round_number > item.max_level)
			continue;

		float weight = item.weight + ((level.round_number - item.min_level) * item.lvl_w_adjust);

		if (weight <= 0)
			continue;

		total_weight += weight;
		picked_items[num_picked_items++] = { &item, total_weight };
	}

	if (!total_weight)
		return nullptr;

	float r = frandom() * total_weight;

	for (size_t i = 0; i < num_picked_items; i++)
		if (r < picked_items[i].weight)
			return FindItemByClassname(picked_items[i].item->classname);

	return nullptr;
}

static gitem_t *Horde_PickDropItem(const weighted_item_t *monster_row)
{
	if (monster_row) {
		item_id_t choices[4];
		int       num_choices = 0;

		for (item_id_t id : monster_row->drops) {
			if (id != IT_NULL)
				choices[num_choices++] = id;
		}

		if (num_choices > 0)
			return GetItemByIndex(choices[irandom(num_choices)]);
	}

	return Horde_PickItem();
}

static const char *Horde_PickMonster(weighted_item_t const **out_row)
{
	static std::array<picked_item_t, q_countof(monsters)> picked_monsters;
	size_t                                                num_picked_monsters = 0;
	float                                                 total_weight = 0;

	if (out_row)
		*out_row = nullptr;

	for (auto &monster : monsters) {
		if (monster.min_level != -1 && level.round_number < monster.min_level)
			continue;
		if (monster.max_level != -1 && level.round_number > monster.max_level)
			continue;

		float weight = monster.weight + ((level.round_number - monster.min_level) * monster.lvl_w_adjust);

		if (weight <= 0)
			continue;

		total_weight += weight;
		picked_monsters[num_picked_monsters++] = { &monster, total_weight };
	}

	if (!total_weight)
		return nullptr;

	float r = frandom() * total_weight;

	for (size_t i = 0; i < num_picked_monsters; i++) {
		if (r < picked_monsters[i].weight) {
			if (out_row)
				*out_row = picked_monsters[i].item;
			return picked_monsters[i].item->classname;
		}
	}

	return nullptr;
}

// When weighted pick finds nothing (e.g. all weights zero), use the highest-tier table row still valid for this wave.
static const char *Horde_PickMonsterFallback(weighted_item_t const **out_row)
{
	const weighted_item_t *choice = nullptr;

	if (out_row)
		*out_row = nullptr;
	int32_t                    best_cap = -1;

	for (auto &monster : monsters) {
		if (monster.min_level != -1 && level.round_number < monster.min_level)
			continue;

		const int32_t cap = monster.max_level == -1 ? INT32_MAX : monster.max_level;
		if (level.round_number > cap)
			continue;

		if (cap > best_cap) {
			best_cap = cap;
			choice = &monster;
		}
	}

	if (!choice) {
		best_cap = -1;
		for (auto &monster : monsters) {
			if (monster.min_level != -1 && level.round_number < monster.min_level)
				continue;

			const int32_t cap = monster.max_level == -1 ? INT32_MAX : monster.max_level;
			if (cap > best_cap) {
				best_cap = cap;
				choice = &monster;
			}
		}
	}

	if (out_row && choice)
		*out_row = choice;

	return choice ? choice->classname : nullptr;
}

static const char *Horde_PickMonsterForWave(weighted_item_t const **out_row)
{
	if (const char *pick = Horde_PickMonster(out_row))
		return pick;

	static int32_t fallback_warn_wave = -1;
	const char    *fallback = Horde_PickMonsterFallback(out_row);

	if (fallback && fallback_warn_wave != level.round_number) {
		fallback_warn_wave = level.round_number;
		gi.Com_PrintFmt("MM_Horde: no weighted monster for wave {}; using fallback {}\n", level.round_number, fallback);
	}

	return fallback;
}
} // namespace

extern cvar_t *g_horde_starting_wave;
extern cvar_t *g_horde_monsters_base;
extern cvar_t *g_horde_monsters_per_wave;
extern cvar_t *g_horde_monsters_min;
extern cvar_t *g_horde_monsters_max;
extern cvar_t *g_horde_spawn_interval_min;
extern cvar_t *g_horde_spawn_interval_max;
extern cvar_t *g_horde_warmup_cap;
extern cvar_t *g_horde_overrun_limit;
extern cvar_t *g_horde_wave_spawn_delay_ms;
extern cvar_t *g_horde_player_scale;
extern cvar_t *g_horde_player_scale_factor;
extern cvar_t *g_horde_player_scale_max;

static bool HordeActive()
{
	return g_gametype->integer == static_cast<int>(GT_HORDE);
}

static float Horde_MultiplierFromFighters(int fighters)
{
	if (!g_horde_player_scale->integer)
		return 1.f;

	float factor = g_horde_player_scale_factor->value;
	if (factor < 0.f)
		factor = 0.f;

	return 1.f + (fighters - 1) * factor;
}

int MM_Horde_CountFighters()
{
	int fighters = 0;

	for (auto ec : active_clients()) {
		if (ec->health <= 0 || ec->client->eliminated)
			continue;
		fighters++;
	}

	const int max_fighters = clamp(g_horde_player_scale_max->integer, 1, 32);
	return clamp(max(fighters, 1), 1, max_fighters);
}

int MM_Horde_WaveQuota()
{
	const int fighters = MM_Horde_CountFighters();
	const float mult = Horde_MultiplierFromFighters(fighters);
	const int   base = g_horde_monsters_base->integer;
	const int   per_wave = g_horde_monsters_per_wave->integer;
	const int   min_m = g_horde_monsters_min->integer;
	const int   max_m = g_horde_monsters_max->integer;
	const int   raw = base + level.round_number * per_wave;
	const int   scaled = static_cast<int>(raw * mult);

	return clamp(scaled, min_m, max_m);
}

static int Horde_EffectiveOverrunLimitForFighters(int fighters)
{
	int limit = g_horde_overrun_limit->integer;
	if (limit < 1)
		limit = 100;

	const float mult = Horde_MultiplierFromFighters(fighters);
	return max(1, static_cast<int>(limit * mult));
}

static gtime_t Horde_SpawnInterval(bool warmup)
{
	if (warmup)
		return 5_sec;

	const float min_sec = max(0.05f, g_horde_spawn_interval_min->value);
	const float max_sec = max(min_sec, g_horde_spawn_interval_max->value);
	return random_time(gtime_t::from_sec(min_sec), gtime_t::from_sec(max_sec));
}

bool MM_Horde_ShouldSkipEntitiesReset()
{
	return HordeActive();
}

int MM_Horde_CountdownWaveNumber()
{
	if (notGT(GT_HORDE))
		return level.round_number + 1;

	if (!level.round_number && g_horde_starting_wave->integer > 0)
		return g_horde_starting_wave->integer;

	return level.round_number + 1;
}

void MM_Horde_AdvanceRoundNumber()
{
	if (notGT(GT_HORDE))
		return;

	if (!level.round_number && g_horde_starting_wave->integer > 0)
		level.round_number = g_horde_starting_wave->integer;
	else
		level.round_number++;
}

void MM_Horde_OnRoundStarted()
{
	if (notGT(GT_HORDE))
		return;

	gi.LocBroadcast_Print(PRINT_CHAT, "Wave {} has begun!\n", level.round_number);
	gi.LocBroadcast_Print(PRINT_CENTER, brandom() ? "INCOMING!" : "LOCK AND LOAD!");
	AnnouncerSound(world, "fight", nullptr, false);
	MM_Horde_BeginWave();
}

void MM_Horde_CleanWaveTransition()
{
	if (!HordeActive())
		return;

	// Remove dead monster corpses between waves (Horde skips Entities_Reset).
	for (size_t i = globals.num_entities; i > 1; i--) {
		gentity_t *ent = &g_entities[i - 1];

		if (!ent->inuse)
			continue;
		if (!(ent->svflags & SVF_MONSTER))
			continue;
		if (ent->health > 0 && !ent->deadflag && !(ent->svflags & SVF_DEADMONSTER))
			continue;

		G_FreeEntity(ent);
	}

	level.total_monsters = 0;
	level.killed_monsters = 0;

	if (g_debug_monster_kills->integer)
		level.monsters_registered.fill(nullptr);
}

void MM_Horde_OnRoundEnd()
{
	if (notGT(GT_HORDE))
		return;

	level.horde_all_spawned = false;
	MM_Horde_CleanWaveTransition();
}

bool MM_Horde_UpdateRoundInProgress()
{
	if (notGT(GT_HORDE))
		return false;

	MM_Horde_RunSpawning();

	if (level.horde_all_spawned && !(level.total_monsters - level.killed_monsters)) {
		gi.LocBroadcast_Print(PRINT_CENTER, "Monsters eliminated!\n");
		gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex("ctf/flagcap.wav"), 1, ATTN_NONE, 0);
		return true;
	}

	return false;
}

bool MM_Horde_CheckOverrun()
{
	if (notGT(GT_HORDE))
		return false;

	int overrun_limit = level.horde_overrun_limit;
	if (overrun_limit < 1)
		overrun_limit = Horde_EffectiveOverrunLimitForFighters(MM_Horde_CountFighters());

	if ((level.total_monsters - level.killed_monsters) < overrun_limit)
		return false;

	gi.Broadcast_Print(PRINT_CENTER, "DEFEATED!");
	QueueIntermission("OVERRUN BY MONSTERS!", true, false);
	return true;
}

bool MM_Horde_CheckMatchEnd()
{
	if (notGT(GT_HORDE))
		return false;

	if (roundlimit->integer <= 0 || level.round_number < roundlimit->integer)
		return false;

	QueueIntermission(G_Fmt("{} WINS with a final score of {}.", game.clients[level.sorted_clients[0]].resp.netname,
		game.clients[level.sorted_clients[0]].resp.score).data(),
		false, false);
	return true;
}

bool MM_Horde_SkipFragScoreLimit()
{
	return HordeActive();
}

bool MM_Horde_SkipMercyLimit()
{
	return HordeActive();
}

void MM_Horde_Init()
{
	// precache-all path disabled; see commented block in git history.
}

void MM_Horde_BeginWave()
{
	if (notGT(GT_HORDE))
		return;

	MM_Horde_CleanWaveTransition();

	const int fighters = MM_Horde_CountFighters();
	level.horde_fighters_snapshotted = static_cast<int8_t>(fighters);
	level.horde_num_monsters_to_spawn = static_cast<int16_t>(MM_Horde_WaveQuota());
	level.horde_overrun_limit = static_cast<int16_t>(Horde_EffectiveOverrunLimitForFighters(fighters));

	const int delay_ms = max(0, g_horde_wave_spawn_delay_ms->integer);
	level.horde_monster_spawn_time = level.time + gtime_t::from_ms(delay_ms);
}

void MM_Horde_RunSpawning()
{
	if (notGT(GT_HORDE))
		return;

	bool warmup = level.match_state == MATCH_WARMUP_DEFAULT || level.match_state == MATCH_WARMUP_READYUP;

	if (!warmup && level.round_state != ROUND_IN_PROGRESS)
		return;

	const int warmup_cap = max(1, g_horde_warmup_cap->integer);
	if (warmup && (level.total_monsters - level.killed_monsters >= warmup_cap))
		return;

	if (level.horde_all_spawned)
		return;

	if (level.horde_monster_spawn_time <= level.time) {
		const weighted_item_t *monster_row = nullptr;
		const char            *monster_class = Horde_PickMonsterForWave(&monster_row);
		if (!monster_class) {
			level.horde_monster_spawn_time = warmup ? level.time + 5_sec : level.time + 1_sec;
			return;
		}

		gentity_t *e = G_Spawn();
		e->classname = monster_class;
		select_spawn_result_t result = SelectDeathmatchSpawnPoint(nullptr, vec3_origin, SPAWN_FARTHEST, false, true, false, false);

		if (result.any_valid && result.spot) {
			e->s.origin = result.spot->s.origin;
			e->s.angles = result.spot->s.angles;

			e->item = Horde_PickDropItem(monster_row);
			ED_CallSpawn(e);

			if (!e->inuse || !(e->svflags & SVF_MONSTER)) {
				if (e->inuse)
					G_FreeEntity(e);
				level.horde_monster_spawn_time = warmup ? level.time + 5_sec : level.time + 1_sec;
				return;
			}

			level.horde_monster_spawn_time = level.time + Horde_SpawnInterval(warmup);

			e->enemy = FindClosestPlayerToPoint(e->s.origin);
			if (e->enemy)
				FoundTarget(e);

			if (!warmup) {
				level.horde_num_monsters_to_spawn--;

				if (!level.horde_num_monsters_to_spawn)
					level.horde_all_spawned = true;
			}
		} else {
			G_FreeEntity(e);
			level.horde_monster_spawn_time = warmup ? level.time + 5_sec : level.time + 1_sec;
		}
	}
}

void MM_Horde_AdjustPlayerScore(gclient_t *cl, int32_t offset)
{
	if (notGT(GT_HORDE))
		return;
	if (!cl || !cl->pers.connected)
		return;

	if (IsScoringDisabled())
		return;

	G_AdjustPlayerScore(cl, offset, false, 0);
}
