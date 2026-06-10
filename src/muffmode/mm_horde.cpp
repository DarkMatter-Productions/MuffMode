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
	int32_t                 spawn_points = 1;
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
	{ "weapon_hyperblaster", 5, -1, 0.70f, 0 },
	{ "weapon_rocketlauncher", 6, -1, 0.65f, 0 },
	{ "weapon_railgun", 8, -1, 0.45f, 0 },

	{ "ammo_shells", -1, -1, 1.25f, 0 },
	{ "ammo_bullets", -1, -1, 1.25f, 0 },
	{ "ammo_grenades", 2, -1, 1.25f, 0 },
	{ "ammo_cells", 5, -1, 1.0f, 0 },
	{ "ammo_rockets", 6, -1, 1.0f, 0 },
	{ "ammo_slugs", 8, -1, 0.9f, 0 },
};

// Tuned for a 12-wave arc: soldiers -> gekks/flyers -> mid-tier -> heavies (8-10) -> commander finale (11-12).
// 1-point chaff stays available all game so leftover budget points are always spendable.
// Soldier-family weights are kept low so waves 2-4 diversify quickly (soldier share ~73/61/46%).
constexpr weighted_item_t monsters[] = {
	// chaff
	{ "monster_soldier_light", -1, -1, 1.00f, -0.04f, { IT_HEALTH_SMALL }, 1 },
	{ "monster_soldier", -1, -1, 0.75f, -0.03f, { IT_AMMO_BULLETS_SMALL, IT_HEALTH_SMALL }, 1 },
	{ "monster_soldier_ss", 2, 9, 0.85f, -0.08f, { IT_AMMO_SHELLS_SMALL, IT_HEALTH_SMALL }, 1 },
	// early variety
	{ "monster_gekk", 2, 10, 1.35f, -0.10f, {}, 2 },
	{ "monster_soldier_hypergun", 2, 10, 0.90f, 0, { IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL }, 2 },
	{ "monster_soldier_lasergun", 3, 10, 0.90f, 0.03f, { IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL }, 2 },
	{ "monster_soldier_ripper", 3, 10, 0.90f, 0.03f, { IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL }, 2 },
	{ "monster_infantry", 3, -1, 1.05f, 0.05f, { IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS }, 2 },
	{ "monster_flyer", 3, -1, 1.10f, 0.02f, { IT_AMMO_CELLS_SMALL }, 2 },
	// mid-tier
	{ "monster_gunner", 4, -1, 1.05f, 0.15f, { IT_AMMO_GRENADES, IT_AMMO_BULLETS_SMALL }, 3 },
	{ "monster_berserk", 4, 14, 1.05f, 0.05f, { IT_ARMOR_SHARD }, 3 },
	{ "monster_parasite", 4, 14, 1.00f, -0.05f, {}, 3 },
	{ "monster_gladb", 5, 14, 1.00f, 0.05f, { IT_AMMO_CELLS_SMALL }, 3 },
	{ "monster_stalker", 5, 14, 0.95f, 0.05f, { IT_AMMO_CELLS_SMALL }, 3 },
	{ "monster_brain", 6, 14, 0.95f, 0, { IT_AMMO_CELLS_SMALL }, 3 },
	{ "monster_mutant", 6, 14, 0.90f, 0, {}, 3 },
	{ "monster_floater", 6, 14, 0.90f, 0, {}, 3 },
	{ "monster_gladiator", 7, -1, 1.00f, 0.10f, { IT_AMMO_SLUGS }, 4 },
	// heavies
	{ "monster_hover", 8, -1, 0.85f, 0, {}, 4 },
	{ "monster_guncmdr", 8, -1, 0.50f, 0.10f, { IT_AMMO_GRENADES, IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS, IT_AMMO_CELLS_SMALL }, 5 },
	{ "monster_chick", 8, -1, 0.95f, 0, { IT_AMMO_ROCKETS_SMALL, IT_AMMO_ROCKETS }, 4 },
	{ "monster_daedalus", 9, -1, 0.85f, 0.05f, { IT_AMMO_CELLS_SMALL }, 5 },
	{ "monster_medic", 9, -1, 0.80f, 0, { IT_HEALTH_SMALL, IT_HEALTH_MEDIUM }, 5 },
	{ "monster_tank", 10, -1, 0.80f, 0.05f, { IT_AMMO_ROCKETS }, 6 },
	{ "monster_chick_heat", 10, -1, 0.85f, 0.05f, { IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS }, 4 },
	{ "monster_shambler", 10, -1, 0.75f, 0.05f, {}, 6 },
	// finale
	{ "monster_tank_commander", 11, -1, 0.45f, 0.15f, { IT_AMMO_ROCKETS_SMALL, IT_AMMO_BULLETS_SMALL, IT_AMMO_ROCKETS, IT_AMMO_BULLETS }, 8 },
	{ "monster_medic_commander", 11, -1, 0.40f, 0.12f, { IT_AMMO_CELLS_SMALL, IT_HEALTH_MEDIUM, IT_HEALTH_LARGE }, 8 },
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

		// clamp so "-1 = always available" rows ramp from wave 1, not wave -1
		float weight = item.weight + ((level.round_number - max(1, item.min_level)) * item.lvl_w_adjust);

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

static const char *Horde_PickMonster(weighted_item_t const **out_row, int remaining_points)
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
		if (monster.spawn_points > remaining_points)
			continue;

		// clamp so "-1 = always available" rows ramp from wave 1, not wave -1
		float weight = monster.weight + ((level.round_number - max(1, monster.min_level)) * monster.lvl_w_adjust);

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

// When weighted pick finds nothing (e.g. all weights zero), use the highest-tier affordable row still valid for this wave.
static const char *Horde_PickMonsterFallback(weighted_item_t const **out_row, int remaining_points)
{
	const weighted_item_t *choice = nullptr;

	if (out_row)
		*out_row = nullptr;
	int32_t                    best_cap = -1;

	for (auto &monster : monsters) {
		if (monster.min_level != -1 && level.round_number < monster.min_level)
			continue;
		if (monster.spawn_points > remaining_points)
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
			if (monster.spawn_points > remaining_points)
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

static const char *Horde_PickMonsterForWave(weighted_item_t const **out_row, int remaining_points)
{
	if (const char *pick = Horde_PickMonster(out_row, remaining_points))
		return pick;

	static int32_t fallback_warn_wave = -1;
	const char    *fallback = Horde_PickMonsterFallback(out_row, remaining_points);

	if (fallback && fallback_warn_wave != level.round_number) {
		fallback_warn_wave = level.round_number;
		gi.Com_PrintFmt("MM_Horde: no weighted monster for wave {}; using fallback {}\n", level.round_number, fallback);
	}

	return fallback;
}
} // namespace

extern cvar_t *g_horde_starting_wave;
extern cvar_t *g_horde_points_base;
extern cvar_t *g_horde_points_per_wave;
extern cvar_t *g_horde_points_min;
extern cvar_t *g_horde_points_max;
extern cvar_t *g_horde_spawn_interval_min;
extern cvar_t *g_horde_spawn_interval_max;
extern cvar_t *g_horde_warmup_cap;
extern cvar_t *g_horde_wave_spawn_delay_ms;
extern cvar_t *g_horde_player_scale;
extern cvar_t *g_horde_player_scale_factor;
extern cvar_t *g_horde_player_scale_max;
extern cvar_t *g_horde_lives;
extern cvar_t *g_horde_mark_monsters_threshold;
extern cvar_t *g_horde_mark_monsters_max;
extern cvar_t *g_horde_map_scale;
extern cvar_t *g_horde_map_scale_ref;
extern cvar_t *g_horde_map_scale_factor;

static bool HordeActive()
{
	return g_gametype->integer == static_cast<int>(GT_HORDE);
}

static int Horde_MarkMonsterSlots()
{
	return clamp(g_horde_mark_monsters_max->integer, 1, static_cast<int>(POI_HORDE_MONSTER_END - POI_HORDE_MONSTER_0 + 1));
}

static bool Horde_ClientWantsMonsterMarkers(gclient_t *cl)
{
	if (!cl || !cl->pers.connected)
		return false;
	if (ClientIsPlaying(cl))
		return true;

	return cl->eliminated && cl->sess.team != TEAM_SPECTATOR;
}

static bool Horde_IsLivingMonster(const gentity_t *ent)
{
	if (!ent->inuse || !(ent->svflags & SVF_MONSTER))
		return false;
	if (ent->health <= 0 || ent->deadflag || (ent->svflags & SVF_DEADMONSTER))
		return false;
	if (ent->monsterinfo.aiflags & AI_DO_NOT_COUNT)
		return false;

	return true;
}

static void Horde_SendMonsterPOI(gentity_t *player, int slot, const vec3_t &pos)
{
	gi.WriteByte(svc_poi);
	gi.WriteShort(static_cast<uint16_t>(POI_HORDE_MONSTER_0 + slot));
	gi.WriteShort(600);
	gi.WritePosition(pos);
	gi.WriteShort(level.pic_ping);
	gi.WriteByte(208);
	gi.WriteByte(POI_FLAG_NONE);
	gi.unicast(player, false);
}

static void Horde_ClearMonsterPOI(gentity_t *player, int slot)
{
	gi.WriteByte(svc_poi);
	gi.WriteShort(static_cast<uint16_t>(POI_HORDE_MONSTER_0 + slot));
	gi.WriteShort(0xFFFF);
	gi.WritePosition(vec3_origin);
	gi.WriteShort(0);
	gi.WriteByte(0);
	gi.WriteByte(POI_FLAG_NONE);
	gi.unicast(player, false);
}

static void Horde_ClearMonsterPOIsForClient(gentity_t *player)
{
	const int slots = Horde_MarkMonsterSlots();

	for (int slot = 0; slot < slots; slot++)
		Horde_ClearMonsterPOI(player, slot);
}

static void Horde_ClearMonsterPOIsForAll()
{
	for (auto ec : active_clients()) {
		if (!ec->client || !Horde_ClientWantsMonsterMarkers(ec->client))
			continue;

		Horde_ClearMonsterPOIsForClient(ec);
	}

	level.horde_mark_living = -1;
}

static void MM_Horde_UpdateMonsterMarkers()
{
	if (!HordeActive())
		return;
	if (level.round_state != roundst_t::ROUND_IN_PROGRESS)
		return;

	const int threshold = g_horde_mark_monsters_threshold->integer;
	const int living = level.total_monsters - level.killed_monsters;

	if (threshold < 1 || living > threshold) {
		if (level.horde_mark_living >= 0 && level.horde_mark_living <= threshold)
			Horde_ClearMonsterPOIsForAll();

		level.horde_mark_living = static_cast<int16_t>(living);
		return;
	}

	const bool newly_marking = level.horde_mark_living > threshold || level.horde_mark_living < 0;
	const bool count_changed = level.horde_mark_living != living;
	const bool throttle = level.horde_mark_time > level.time && !count_changed;

	if (throttle)
		return;

	level.horde_mark_time = level.time + 500_ms;
	level.horde_mark_living = static_cast<int16_t>(living);

	if (newly_marking) {
		for (auto ec : active_clients()) {
			if (!ec->client || !Horde_ClientWantsMonsterMarkers(ec->client))
				continue;

			gi.local_sound(ec, CHAN_AUTO, gi.soundindex("misc/help_marker.wav"), 1.f, ATTN_NORM, 0, GetUnicastKey());
		}
	}

	const int max_slots = Horde_MarkMonsterSlots();
	gentity_t *marked[8] = {};
	int        num_marked = 0;

	for (size_t i = 1; i < globals.num_entities && num_marked < max_slots; i++) {
		gentity_t *ent = &g_entities[i];

		if (!Horde_IsLivingMonster(ent))
			continue;

		marked[num_marked++] = ent;
	}

	for (auto ec : active_clients()) {
		if (!ec->client || !Horde_ClientWantsMonsterMarkers(ec->client))
			continue;

		for (int slot = 0; slot < max_slots; slot++) {
			if (slot < num_marked) {
				vec3_t pos = marked[slot]->s.origin;
				pos[2] += marked[slot]->maxs[2] * 0.5f;
				Horde_SendMonsterPOI(ec, slot, pos);
			} else {
				Horde_ClearMonsterPOI(ec, slot);
			}
		}
	}
}

static int Horde_LivesPerWave()
{
	return max(1, g_horde_lives->integer);
}

static bool Horde_ClientIsActiveFighter(gentity_t *ec)
{
	if (!ec->client || !ClientIsPlaying(ec->client))
		return false;
	if (ec->client->eliminated)
		return false;
	if (ec->health > 0)
		return true;

	return ec->client->pers.lives > 0;
}

static bool Horde_HasActiveFighter()
{
	for (auto ec : active_clients()) {
		if (Horde_ClientIsActiveFighter(ec))
			return true;
	}

	return false;
}

static void MM_Horde_GrantWaveLives()
{
	const int lives = Horde_LivesPerWave();

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client))
			continue;

		const bool was_eliminated = ec->client->eliminated;

		ec->client->pers.lives = lives;
		ec->client->eliminated = false;
		ec->client->horde_elim_msg_wave = 0;

		// Eliminated fighters spectate in freecam with deadflag cleared and health restored.
		if (was_eliminated || ec->deadflag || ec->health <= 0)
			ClientRespawn(ec);
	}
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

static float Horde_MapScaleMultiplier()
{
	if (!g_horde_map_scale->integer)
		return 1.f;

	if (level.horde_map_scale_mult != 0.f)
		return level.horde_map_scale_mult;

	if (level.num_spawn_spots < 2)
	{
		level.horde_map_scale_mult = 1.f;
		return 1.f;
	}

	vec3_t bmin = level.spawn_spots[0]->s.origin;
	vec3_t bmax = bmin;
	for (int i = 1; i < level.num_spawn_spots; i++)
	{
		const vec3_t &o = level.spawn_spots[i]->s.origin;
		bmin.x = min(bmin.x, o.x);
		bmin.y = min(bmin.y, o.y);
		bmin.z = min(bmin.z, o.z);
		bmax.x = max(bmax.x, o.x);
		bmax.y = max(bmax.y, o.y);
		bmax.z = max(bmax.z, o.z);
	}

	const float diagonal = (bmax - bmin).length();
	const float ref      = max(1.f, g_horde_map_scale_ref->value);
	const float factor   = clamp(g_horde_map_scale_factor->value, 0.f, 10.f);
	const float ratio    = diagonal / ref;
	const float mult     = 1.f + (ratio - 1.f) * factor;

	level.horde_map_scale_mult = max(0.1f, mult);
	return level.horde_map_scale_mult;
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

int MM_Horde_WavePointBudget()
{
	const int   fighters = MM_Horde_CountFighters();
	const float pmult    = Horde_MultiplierFromFighters(fighters);
	const float msmult   = Horde_MapScaleMultiplier();
	const int   base     = g_horde_points_base->integer;
	const int   per_wave = g_horde_points_per_wave->integer;
	const int   min_pts  = g_horde_points_min->integer;
	const int   max_pts  = g_horde_points_max->integer;
	int         budget   = base + level.round_number * per_wave;

	if (min_pts > 0)
		budget = max(budget, min_pts);
	if (max_pts > 0)
		budget = min(budget, max_pts);

	return max(1, static_cast<int>(budget * pmult * msmult));
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

void MM_Horde_OnRoundCountdown()
{
	if (notGT(GT_HORDE))
		return;

	MM_Horde_GrantWaveLives();
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

void MM_Horde_NotifyEliminatedSpectator(gentity_t *ent)
{
	if (!HordeActive())
		return;
	if (level.round_state != roundst_t::ROUND_IN_PROGRESS)
		return;
	if (!ent->client || !ent->client->eliminated)
		return;
	if (ent->client->sess.team == TEAM_SPECTATOR)
		return;
	if (ent->client->horde_elim_msg_wave == level.round_number)
		return;

	ent->client->horde_elim_msg_wave = static_cast<int16_t>(level.round_number);
	gi.LocClient_Print(ent, PRINT_CENTER, "You will rejoin when the next wave countdown begins.");
}

void MM_Horde_OnPlayerDeath(gentity_t *ent)
{
	if (!HordeActive())
		return;
	if (level.round_state != roundst_t::ROUND_IN_PROGRESS)
		return;
	if (!ent->client || !ClientIsPlaying(ent->client))
		return;

	if (ent->client->pers.lives > 0)
		ent->client->pers.lives--;

	if (ent->client->pers.lives <= 0) {
		ClientSetEliminated(ent);
		ent->client->respawn_time = level.time + 1_sec;
		MM_Horde_NotifyEliminatedSpectator(ent);
	}
}

bool MM_Horde_CheckAllFightersLost()
{
	if (!HordeActive())
		return false;
	if (level.round_state != roundst_t::ROUND_IN_PROGRESS)
		return false;
	if (level.num_playing_clients < 1)
		return false;
	if (Horde_HasActiveFighter())
		return false;

	gi.Broadcast_Print(PRINT_CENTER, "DEFEATED!");
	QueueIntermission("ALL FIGHTERS LOST!", true, false);
	return true;
}

bool MM_Horde_CheckDesertionDefeat()
{
	if (!HordeActive())
		return false;
	if (level.match_state != matchst_t::MATCH_IN_PROGRESS)
		return false;
	if (level.intermission_queued || level.intermission_time)
		return false;

	gi.Broadcast_Print(PRINT_CENTER, "DEFEATED!");
	QueueIntermission("ALL FIGHTERS LOST!", true, false);
	return true;
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

	Horde_ClearMonsterPOIsForAll();
	level.horde_mark_time = 0_ms;
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

	if (MM_Horde_CheckAllFightersLost())
		return false;

	MM_Horde_RunSpawning();
	MM_Horde_UpdateMonsterMarkers();

	if (level.horde_all_spawned && !(level.total_monsters - level.killed_monsters)) {
		gi.LocBroadcast_Print(PRINT_CENTER, "Monsters eliminated!\n");
		gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex("ctf/flagcap.wav"), 1, ATTN_NONE, 0);
		return true;
	}

	return false;
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

static void Horde_PrecacheTableMonsters()
{
	for (auto &monster : monsters) {
		gentity_t *e = G_Spawn();
		e->classname = monster.classname;
		// don't let precache spawns inflate level.total_monsters; it starves
		// the warmup spawner, which caps on total_monsters - killed_monsters
		e->monsterinfo.aiflags |= AI_DO_NOT_COUNT;
		ED_CallSpawn(e);
		if (e->inuse)
			G_FreeEntity(e);
	}
}

void MM_Horde_Init()
{
	if (notGT(GT_HORDE))
		return;

	// The monster table's content curve peaks at waves 11-12; the global default
	// roundlimit of 8 would end the match before heavies and commanders appear.
	// Apply a horde default of 12 once per load, only when still at the global default.
	static bool roundlimit_defaulted = false;
	if (!roundlimit_defaulted) {
		roundlimit_defaulted = true;
		if (roundlimit->integer == 8) {
			gi.cvar_forceset("roundlimit", "12");
			gi.Com_PrintFmt("MM_Horde: roundlimit at global default (8), using horde default of 12.\n");
		}
	}

	Horde_PrecacheTableMonsters();
}

void MM_Horde_BeginWave()
{
	if (notGT(GT_HORDE))
		return;

	MM_Horde_CleanWaveTransition();

	const int fighters = MM_Horde_CountFighters();
	level.horde_fighters_snapshotted = static_cast<int8_t>(fighters);
	level.horde_spawn_points_remaining = MM_Horde_WavePointBudget();

	const int delay_ms = max(0, g_horde_wave_spawn_delay_ms->integer);
	level.horde_monster_spawn_time = level.time + gtime_t::from_ms(delay_ms);
}

// Horde spawn points are deathmatch player spawns; their origins are placed for
// the player hull (which gets a +9 lift and stuck-fixing in client spawn code) and
// can sit low enough that monster hulls start embedded in the floor — on bloodrun
// every spawn origin is only 15u above its floor. A monster spawned embedded in a
// thin floor gets teleported through it by M_droptofloor (a trace does not clip
// against a brush it starts inside), e.g. into the blood pool under the walkway at
// 1104 208 -633. Lift the origin clear before validating, and nudge as a fallback.
// Also rejects spots whose ground is liquid. Returns false if the spot is unusable.
static bool Horde_ValidateSpawnOrigin(vec3_t &origin, const vec3_t &check_mins, const vec3_t &check_maxs)
{
	origin[2] += 16.f;

	if (!CheckSpawnPoint(origin, check_mins, check_maxs)) {
		if (G_FixStuckObject_Generic(origin, check_mins, check_maxs,
				[](const vec3_t &start, const vec3_t &mins, const vec3_t &maxs, const vec3_t &end) {
					return gi.trace(start, mins, maxs, end, nullptr, MASK_MONSTERSOLID);
				}) == stuck_result_t::NO_GOOD_POSITION)
			return false;
		if (!CheckSpawnPoint(origin, check_mins, check_maxs))
			return false;
	}

	trace_t tr = gi.trace(origin, check_mins, check_maxs, origin - vec3_t{ 0.f, 0.f, 64.f }, nullptr, MASK_MONSTERSOLID);
	if (gi.pointcontents(tr.endpos) & (CONTENTS_LAVA | CONTENTS_SLIME))
		return false;

	return true;
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

	if (!warmup && level.horde_spawn_points_remaining <= 0) {
		level.horde_all_spawned = true;
		return;
	}

	if (level.horde_monster_spawn_time <= level.time) {
		const int              remaining = warmup ? INT_MAX : level.horde_spawn_points_remaining;
		const weighted_item_t *monster_row = nullptr;
		const char            *monster_class = Horde_PickMonsterForWave(&monster_row, remaining);
		if (!monster_class) {
			if (!warmup)
				level.horde_all_spawned = true;
			else
				level.horde_monster_spawn_time = level.time + 5_sec;
			return;
		}

		gentity_t *e = G_Spawn();
		e->classname = monster_class;
		select_spawn_result_t result = SelectDeathmatchSpawnPoint(nullptr, vec3_origin, SPAWN_FARTHEST, false, true, false, false);

		if (result.any_valid && result.spot) {
			// Validate spawn point fits a large monster (tank commander is the worst-case hull).
			// CheckSpawnPoint also rejects non-world solids (doors, movers) unlike a raw startsolid check.
			constexpr vec3_t horde_check_mins = { -32.f, -32.f, -16.f };
			constexpr vec3_t horde_check_maxs = {  32.f,  32.f,  64.f };
			vec3_t spawn_origin = result.spot->s.origin;
			if (!Horde_ValidateSpawnOrigin(spawn_origin, horde_check_mins, horde_check_maxs)) {
				// Try a different candidate by excluding the failed spot from selection.
				// avoid_point is honoured when g_dm_respawn_point_min_dist > 0 (default 256).
				select_spawn_result_t retry = SelectDeathmatchSpawnPoint(nullptr, result.spot->s.origin, SPAWN_FARTHEST, false, true, false, false);
				bool retry_ok = false;
				if (retry.any_valid && retry.spot && retry.spot != result.spot) {
					spawn_origin = retry.spot->s.origin;
					if (Horde_ValidateSpawnOrigin(spawn_origin, horde_check_mins, horde_check_maxs)) {
						result = retry;
						retry_ok = true;
					}
				}
				if (!retry_ok) {
					// No spot can safely hold a large monster right now. Spawning anyway would
					// place it embedded and let monster_start_go's stuck-fixing relocate it
					// through thin floors or into walls; skip this attempt and retry shortly.
					G_FreeEntity(e);
					level.horde_monster_spawn_time = warmup ? level.time + 5_sec : level.time + 1_sec;
					return;
				}
			}

			e->s.origin = spawn_origin;
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

			if (!warmup && monster_row) {
				level.horde_spawn_points_remaining -= monster_row->spawn_points;

				if (level.horde_spawn_points_remaining <= 0)
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
