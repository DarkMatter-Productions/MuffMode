// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_horde.h"

namespace {
struct weighted_item_t;

using weight_adjust_func_t = void (*)(const weighted_item_t &item, float &weight);

void adjust_weight_health(const weighted_item_t &item, float &weight) {}
void adjust_weight_weapon(const weighted_item_t &item, float &weight) {}
void adjust_weight_ammo(const weighted_item_t &item, float &weight) {}
void adjust_weight_armor(const weighted_item_t &item, float &weight) {}

constexpr struct weighted_item_t {
	const char             *classname;
	int32_t                 min_level = -1, max_level = -1;
	float                   weight = 1.0f;
	float                   lvl_w_adjust = 0;
	int                     flags;
	item_id_t               item[4];
	weight_adjust_func_t    adjust_weight = nullptr;
} items[] = {
	{ "item_health_small" },

	{ "item_health", -1, -1, 1.0f, 0, 0, { IT_NULL }, adjust_weight_health },
	{ "item_health_large", -1, -1, 0.85f, 0, 0, { IT_NULL }, adjust_weight_health },

	{ "item_armor_shard" },
	{ "item_armor_jacket", -1, 4, 0.65f, 0, 0, { IT_NULL }, adjust_weight_armor },
	{ "item_armor_combat", 2, -1, 0.62f, 0, 0, { IT_NULL }, adjust_weight_armor },
	{ "item_armor_body", 4, -1, 0.35f, 0, 0, { IT_NULL }, adjust_weight_armor },

	{ "weapon_shotgun", -1, -1, 0.98f, 0, 0, { IT_NULL }, adjust_weight_weapon },
	{ "weapon_supershotgun", 2, -1, 1.02f, 0, 0, { IT_NULL }, adjust_weight_weapon },
	{ "weapon_machinegun", -1, -1, 1.05f, 0, 0, { IT_NULL }, adjust_weight_weapon },
	{ "weapon_chaingun", 3, -1, 1.01f, 0, 0, { IT_NULL }, adjust_weight_weapon },
	{ "weapon_grenadelauncher", 4, -1, 0.75f, 0, 0, { IT_NULL }, adjust_weight_weapon },

	{ "ammo_shells", -1, -1, 1.25f, 0, 0, { IT_NULL }, adjust_weight_ammo },
	{ "ammo_bullets", -1, -1, 1.25f, 0, 0, { IT_NULL }, adjust_weight_ammo },
	{ "ammo_grenades", 2, -1, 1.25f, 0, 0, { IT_NULL }, adjust_weight_ammo },
};

constexpr weighted_item_t monsters[] = {
	{ "monster_soldier_light", -1, 7, 1.50f, -0.45f, MF_GROUND, { IT_HEALTH_SMALL } },
	{ "monster_soldier", -1, 7, 0.85f, -0.25f, MF_GROUND, { IT_AMMO_BULLETS_SMALL, IT_HEALTH_SMALL } },
	{ "monster_soldier_ss", 2, 7, 1.01f, -0.125f, MF_GROUND, { IT_AMMO_SHELLS_SMALL, IT_HEALTH_SMALL } },
	{ "monster_soldier_hypergun", 2, 9, 1.2f, 0.15f, MF_GROUND, { IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL } },
	{ "monster_soldier_lasergun", 3, 9, 1.15f, 0.2f, MF_GROUND, { IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL } },
	{ "monster_soldier_ripper", 3, 9, 1.25f, 0.25f, MF_GROUND, { IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL } },
	{ "monster_infantry", 3, 16, 1.05f, 0.125f, MF_GROUND, { IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS } },
	{ "monster_gunner", 4, 16, 1.08f, 0.5f, MF_GROUND, { IT_AMMO_GRENADES, IT_AMMO_BULLETS_SMALL } },
	{ "monster_berserk", 4, 16, 1.05f, 0.1f, MF_GROUND, { IT_ARMOR_SHARD } },
	{ "monster_parasite", 5, 16, 1.04f, -0.08f, MF_GROUND, { IT_NULL } },
	{ "monster_gladiator", 5, 16, 1.07f, 0.3f, MF_GROUND, { IT_AMMO_SLUGS } },
	{ "monster_gekk", 6, 16, 0.99f, -0.15f, MF_GROUND | MF_WATER, { IT_NULL } },
	{ "monster_brain", 6, 16, 0.95f, 0, MF_GROUND, { IT_AMMO_CELLS_SMALL } },
	{ "monster_flyer", 6, 16, 0.92f, 0.15f, MF_GROUND | MF_AIR, { IT_AMMO_CELLS_SMALL } },
	{ "monster_floater", 7, 16, 0.9f, 0, MF_GROUND | MF_AIR, { IT_NULL } },
	{ "monster_mutant", 7, 16, 0.85f, 0, MF_GROUND, { IT_NULL } },
	{ "monster_hover", 8, 16, 0.8f, 0, MF_GROUND | MF_AIR, { IT_NULL } },
	{ "monster_guncmdr", 8, -1, 0, 0.125f, MF_GROUND | MF_MEDIUM, { IT_AMMO_GRENADES, IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS, IT_AMMO_CELLS_SMALL } },
	{ "monster_chick", 9, 20, 1.01f, -0.05f, MF_GROUND, { IT_AMMO_ROCKETS_SMALL, IT_AMMO_ROCKETS } },
	{ "monster_daedalus", 9, -1, 0.99f, 0.05f, MF_GROUND | MF_AIR, { IT_AMMO_CELLS_SMALL } },
	{ "monster_medic", 10, 16, 0.95f, -0.05f, MF_GROUND, { IT_HEALTH_SMALL, IT_HEALTH_MEDIUM } },
	{ "monster_tank", 11, -1, 0.85f, 0, MF_GROUND | MF_MEDIUM, { IT_AMMO_ROCKETS } },
	{ "monster_chick_heat", 12, -1, 0.87f, 0.065f, MF_GROUND, { IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS } },
	{ "monster_tank_commander", 12, -1, 0.45f, 0.16f, MF_GROUND | MF_MEDIUM, { IT_AMMO_ROCKETS_SMALL, IT_AMMO_BULLETS_SMALL, IT_AMMO_ROCKETS, IT_AMMO_BULLETS } },
	{ "monster_medic_commander", 13, -1, 0.4f, 0.15f, MF_GROUND | MF_MEDIUM, { IT_AMMO_CELLS_SMALL, IT_HEALTH_MEDIUM, IT_HEALTH_LARGE } },
	{ "monster_kamikaze", 13, -1, 0.85f, 0.04f, MF_GROUND | MF_AIR, { IT_NULL } },
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

		if (item.adjust_weight)
			item.adjust_weight(item, weight);

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

const char *Horde_PickMonster()
{
	static std::array<picked_item_t, q_countof(monsters)> picked_monsters;
	size_t                                                num_picked_monsters = 0;
	float                                                 total_weight = 0;

	for (auto &monster : monsters) {
		if (monster.min_level != -1 && level.round_number < monster.min_level)
			continue;
		if (monster.max_level != -1 && level.round_number > monster.max_level)
			continue;

		float weight = monster.weight + ((level.round_number - monster.min_level) * monster.lvl_w_adjust);

		if (monster.adjust_weight)
			monster.adjust_weight(monster, weight);

		if (weight <= 0)
			continue;

		total_weight += weight;
		picked_monsters[num_picked_monsters++] = { &monster, total_weight };
	}

	if (!total_weight)
		return nullptr;

	float r = frandom() * total_weight;

	for (size_t i = 0; i < num_picked_monsters; i++)
		if (r < picked_monsters[i].weight)
			return picked_monsters[i].item->classname;

	return nullptr;
}
} // namespace

extern cvar_t *g_horde_starting_wave;

static constexpr int32_t HORDE_OVERRUN_LIMIT = 100;

static bool HordeActive()
{
	return g_gametype->integer == static_cast<int>(GT_HORDE);
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

void MM_Horde_OnRoundEnd()
{
	if (notGT(GT_HORDE))
		return;

	level.horde_all_spawned = false;
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

	if ((level.total_monsters - level.killed_monsters) < HORDE_OVERRUN_LIMIT)
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

	level.horde_num_monsters_to_spawn = clamp(15 + (level.round_number * 5), 20, 80);
	level.horde_monster_spawn_time = level.time + 500_ms;
}

void MM_Horde_RunSpawning()
{
	if (notGT(GT_HORDE))
		return;

	bool warmup = level.match_state == MATCH_WARMUP_DEFAULT || level.match_state == MATCH_WARMUP_READYUP;

	if (!warmup && level.round_state != ROUND_IN_PROGRESS)
		return;

	if (warmup && (level.total_monsters - level.killed_monsters >= 30))
		return;

	if (level.horde_all_spawned)
		return;

	if (level.horde_monster_spawn_time <= level.time) {
		const char *monster_class = Horde_PickMonster();
		if (!monster_class)
			return;

		gentity_t *e = G_Spawn();
		e->classname = monster_class;
		select_spawn_result_t result = SelectDeathmatchSpawnPoint(nullptr, vec3_origin, SPAWN_FARTHEST, false, true, false, false);

		if (result.any_valid && result.spot) {
			e->s.origin = result.spot->s.origin;
			e->s.angles = result.spot->s.angles;

			e->item = Horde_PickItem();
			ED_CallSpawn(e);
			level.horde_monster_spawn_time = warmup ? level.time + 5_sec : level.time + random_time(0.3_sec, 0.5_sec);

			e->enemy = FindClosestPlayerToPoint(e->s.origin);
			if (e->enemy)
				FoundTarget(e);

			if (!warmup) {
				level.horde_num_monsters_to_spawn--;

				if (!level.horde_num_monsters_to_spawn)
					level.horde_all_spawned = true;
			}
		} else
			level.horde_monster_spawn_time = warmup ? level.time + 5_sec : level.time + 1_sec;
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
