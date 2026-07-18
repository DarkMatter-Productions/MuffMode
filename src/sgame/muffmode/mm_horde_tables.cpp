// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_horde_ai_rules.h"
#include "muffmode/mm_horde_tables.h"

#include <array>
#include <limits>

extern cvar_t *g_horde_boss_tier_window;
extern cvar_t *g_horde_boss_interval;
extern cvar_t *g_horde_boss_min_wave;
extern cvar_t *g_horde_boss_machinegames;
extern cvar_t *g_horde_boss_pairs;
extern cvar_t *g_horde_boss_repeat_window;
extern cvar_t *g_horde_boss_scale_limit;
extern cvar_t *g_horde_content_peak_wave;
extern cvar_t *g_horde_drop_profile_bias;
extern cvar_t *g_horde_weight_floor;

namespace muffmode::horde {

const std::array<WeightedItem, kHordeItemCount> kItems = {{
	{ "item_health_small" },

	{ "item_health", -1, -1, 1.0f, 0.0f },
	{ "item_health_large", -1, -1, 0.85f, 0.0f },

	{ "item_armor_shard" },
	{ "item_armor_jacket", -1, 4, 0.65f, 0.0f },
	{ "item_armor_combat", 2, -1, 0.62f, 0.0f },
	{ "item_armor_body", 4, -1, 0.35f, 0.0f },

	{ "weapon_shotgun", -1, -1, 0.98f, 0.0f },
	{ "weapon_supershotgun", 2, -1, 1.02f, 0.0f },
	{ "weapon_machinegun", -1, -1, 1.05f, 0.0f },
	{ "weapon_chaingun", 3, -1, 1.01f, 0.0f },
	{ "weapon_grenadelauncher", 4, -1, 0.75f, 0.0f },
	{ "weapon_hyperblaster", 5, -1, 0.70f, 0.0f },
	{ "weapon_rocketlauncher", 6, -1, 0.65f, 0.0f },
	{ "weapon_railgun", 8, -1, 0.45f, 0.0f },

	{ "ammo_shells", -1, -1, 1.25f, 0.0f },
	{ "ammo_bullets", -1, -1, 1.25f, 0.0f },
	{ "ammo_grenades", 2, -1, 1.25f, 0.0f },
	{ "ammo_cells", 5, -1, 1.0f, 0.0f },
	{ "ammo_rockets", 6, -1, 1.0f, 0.0f },
	{ "ammo_slugs", 8, -1, 0.9f, 0.0f },
}};

// Tuned for a 12-wave arc: soldiers -> gekks/flyers -> mid-tier -> heavies (8-10) -> commander finale (11-12).
// 1-point chaff stays available all game so leftover budget points are always spendable.
// Soldier-family weights are kept low so waves 2-4 diversify quickly (soldier share ~73/61/46%).
const std::array<WeightedItem, kHordeMonsterCount> kMonsters = {{
	// chaff
	{ "monster_soldier_light", -1, -1, 1.00f, -0.04f,
		{ IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL,
			IT_HEALTH_SMALL, IT_ARMOR_SHARD }, 1, HCAT_SWARM },
	{ "monster_soldier", -1, -1, 0.75f, -0.03f,
		{ IT_AMMO_SHELLS_SMALL, IT_AMMO_SHELLS_SMALL, IT_AMMO_SHELLS_SMALL, IT_AMMO_SHELLS_SMALL,
			IT_AMMO_SHELLS, IT_HEALTH_SMALL, IT_ARMOR_SHARD }, 1, HCAT_SWARM },
	{ "monster_soldier_ss", 2, 9, 0.85f, -0.08f,
		{ IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS_SMALL,
			IT_AMMO_BULLETS, IT_HEALTH_SMALL, IT_ARMOR_SHARD }, 1, HCAT_SWARM },
	// early variety
	{ "monster_gekk", 2, 10, 1.35f, -0.10f,
		{ IT_HEALTH_SMALL, IT_HEALTH_SMALL, IT_HEALTH_SMALL, IT_HEALTH_SMALL,
			IT_ARMOR_SHARD, IT_ARMOR_SHARD, IT_HEALTH_MEDIUM }, 2, HCAT_SWARM | HCAT_MELEE | HCAT_INFEST },
	{ "monster_soldier_hypergun", 2, 10, 0.90f, 0.0f,
		{ IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL,
			IT_AMMO_CELLS, IT_HEALTH_SMALL }, 2 },
	{ "monster_soldier_lasergun", 3, 10, 0.90f, 0.03f,
		{ IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL,
			IT_AMMO_CELLS, IT_HEALTH_SMALL }, 2 },
	{ "monster_soldier_ripper", 3, 10, 0.90f, 0.03f,
		{ IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL,
			IT_AMMO_CELLS, IT_HEALTH_SMALL }, 2 },
	{ "monster_infantry", 3, -1, 1.05f, 0.05f,
		{ IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS_SMALL,
			IT_AMMO_BULLETS, IT_AMMO_BULLETS, IT_HEALTH_SMALL }, 2 },
	{ "monster_flyer", 3, -1, 1.10f, 0.02f,
		{ IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL,
			IT_AMMO_CELLS, IT_HEALTH_SMALL }, 2, HCAT_AERIAL },
	// mid-tier
	{ "monster_gunner", 4, -1, 1.05f, 0.15f,
		{ IT_AMMO_GRENADES, IT_AMMO_GRENADES, IT_AMMO_GRENADES, IT_AMMO_BULLETS_SMALL,
			IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS, IT_HEALTH_SMALL }, 3 },
	// max_level was 14 (capped at the finale); uncapped to -1 so Melee/Infestation/Heavy/Aerial
	// themes keep on-category bodies past wave 14. Active through wave 14 already, so waves 1-14
	// are unchanged - this only adds them at wave 15+.
	{ "monster_berserk", 4, -1, 1.05f, 0.05f,
		{ IT_HEALTH_SMALL, IT_HEALTH_SMALL, IT_HEALTH_SMALL, IT_HEALTH_SMALL,
			IT_ARMOR_SHARD, IT_ARMOR_SHARD, IT_ARMOR_SHARD }, 3, HCAT_MELEE },
	{ "monster_parasite", 4, -1, 1.00f, -0.05f,
		{ IT_HEALTH_SMALL, IT_HEALTH_SMALL, IT_HEALTH_SMALL, IT_HEALTH_SMALL,
			IT_HEALTH_SMALL, IT_ARMOR_SHARD, IT_ARMOR_SHARD }, 3, HCAT_INFEST },
	{ "monster_gladb", 5, -1, 1.00f, 0.05f,
		{ IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL,
			IT_AMMO_CELLS, IT_ARMOR_SHARD }, 3, HCAT_HEAVY },
	{ "monster_stalker", 5, -1, 0.95f, 0.05f,
		{ IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL,
			IT_HEALTH_SMALL, IT_ARMOR_SHARD }, 3, HCAT_INFEST },
	{ "monster_brain", 6, -1, 0.95f, 0.0f,
		{ IT_HEALTH_SMALL, IT_HEALTH_SMALL, IT_HEALTH_SMALL, IT_ARMOR_SHARD,
			IT_ARMOR_SHARD, IT_AMMO_CELLS_SMALL }, 3, HCAT_MELEE | HCAT_INFEST },
	{ "monster_mutant", 6, -1, 0.90f, 0.0f,
		{ IT_HEALTH_SMALL, IT_HEALTH_SMALL, IT_HEALTH_SMALL, IT_HEALTH_SMALL,
			IT_ARMOR_SHARD, IT_ARMOR_SHARD }, 3, HCAT_MELEE },
	{ "monster_floater", 6, -1, 0.90f, 0.0f,
		{ IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL,
			IT_HEALTH_SMALL, IT_ARMOR_SHARD }, 3, HCAT_AERIAL },
	{ "monster_gladiator", 7, -1, 1.00f, 0.10f,
		{ IT_AMMO_SLUGS_SMALL, IT_AMMO_SLUGS, IT_AMMO_SLUGS, IT_AMMO_SLUGS,
			IT_AMMO_SLUGS, IT_AMMO_SLUGS, IT_HEALTH_MEDIUM }, 4, HCAT_HEAVY },
	// heavies
	{ "monster_hover", 8, -1, 0.85f, 0.0f,
		{ IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS,
			IT_HEALTH_SMALL, IT_ARMOR_SHARD }, 4, HCAT_AERIAL },
	{ "monster_guncmdr", 8, -1, 0.50f, 0.10f,
		{ IT_AMMO_GRENADES, IT_AMMO_GRENADES, IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS_SMALL,
			IT_AMMO_BULLETS, IT_AMMO_CELLS_SMALL, IT_HEALTH_MEDIUM }, 5, HCAT_HEAVY },
	{ "monster_chick", 8, -1, 0.95f, 0.0f,
		{ IT_AMMO_ROCKETS_SMALL, IT_AMMO_ROCKETS_SMALL, IT_AMMO_ROCKETS_SMALL, IT_AMMO_ROCKETS_SMALL,
			IT_AMMO_ROCKETS, IT_AMMO_ROCKETS, IT_HEALTH_MEDIUM }, 4 },
	{ "monster_daedalus", 9, -1, 0.85f, 0.05f,
		{ IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS,
			IT_AMMO_CELLS, IT_HEALTH_MEDIUM }, 5, HCAT_AERIAL },
	{ "monster_medic", 9, -1, 0.80f, 0.0f,
		{ IT_HEALTH_SMALL, IT_HEALTH_SMALL, IT_HEALTH_MEDIUM, IT_HEALTH_MEDIUM,
			IT_HEALTH_MEDIUM, IT_HEALTH_LARGE, IT_AMMO_CELLS_SMALL }, 5 },
	{ "monster_tank", 10, -1, 0.80f, 0.05f,
		{ IT_AMMO_ROCKETS_SMALL, IT_AMMO_ROCKETS, IT_AMMO_ROCKETS, IT_AMMO_ROCKETS,
			IT_AMMO_BULLETS, IT_AMMO_BULLETS, IT_HEALTH_MEDIUM }, 6, HCAT_HEAVY },
	{ "monster_chick_heat", 10, -1, 0.85f, 0.05f,
		{ IT_AMMO_ROCKETS_SMALL, IT_AMMO_ROCKETS_SMALL, IT_AMMO_ROCKETS_SMALL, IT_AMMO_ROCKETS_SMALL,
			IT_AMMO_ROCKETS, IT_AMMO_ROCKETS, IT_HEALTH_MEDIUM }, 4 },
	{ "monster_shambler", 10, -1, 0.75f, 0.05f,
		{ IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS, IT_HEALTH_MEDIUM,
			IT_HEALTH_MEDIUM, IT_HEALTH_LARGE, IT_ARMOR_SHARD }, 6, HCAT_HEAVY },
	// finale
	{ "monster_tank_commander", 11, -1, 0.45f, 0.15f,
		{ IT_AMMO_ROCKETS_SMALL, IT_AMMO_ROCKETS, IT_AMMO_ROCKETS, IT_AMMO_BULLETS_SMALL,
			IT_AMMO_BULLETS, IT_AMMO_BULLETS, IT_HEALTH_LARGE }, 8, HCAT_HEAVY },
	{ "monster_medic_commander", 11, -1, 0.40f, 0.12f,
		{ IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS, IT_HEALTH_MEDIUM, IT_HEALTH_MEDIUM,
			IT_HEALTH_LARGE, IT_HEALTH_LARGE, IT_ARMOR_COMBAT }, 8 },
}};

static_assert(kMonsters.size() <= 32, "horde_wave_roster bitmask supports at most 32 monster rows");

const std::array<ThemeDefinition, kHordeThemeCount> kThemes = {{
	{ Theme::Swarm,       HCAT_SWARM,  3, 1.30f, "THE SWARM APPROACHES!" },
	{ Theme::Aerial,      HCAT_AERIAL, 4, 1.00f, "AERIAL ASSAULT!" },
	{ Theme::Heavy,       HCAT_HEAVY,  7, 0.80f, "HEAVY ASSAULT!" },
	{ Theme::Melee,       HCAT_MELEE,  4, 1.10f, "THEY'RE CLOSING IN!" },
	{ Theme::Infestation, HCAT_INFEST, 4, 1.15f, "INFESTATION!" },
}};

namespace {

constexpr BossDefinition MakeBoss(const char *id, const char *classname, const char *display_name,
	const char *source_map, int32_t min_level, float weight, int32_t spawn_points,
	const vec3_t &mins, const vec3_t &maxs, float health_multiplier = 1.f,
	float model_scale = 0.f, uint8_t units = 1, bool machinegames = false,
	float damage_multiplier = 1.f, uint32_t spawnflags = 0, int32_t power_armor_type = -1,
	int32_t power_armor_power = -1, int32_t monster_slots = -1,
	const char *reinforcements = nullptr)
{
	return {
		id, classname, display_name, source_map, min_level, -1, weight, spawn_points,
		mins, maxs, health_multiplier, model_scale, units, machinegames,
		damage_multiplier, spawnflags, power_armor_type, power_armor_power,
		monster_slots, reinforcements
	};
}

constexpr vec3_t kSupertankMins = { -64.f, -64.f, 0.f };
constexpr vec3_t kSupertankMaxs = { 64.f, 64.f, 112.f };
constexpr vec3_t kHornetMins = { -56.f, -56.f, 0.f };
constexpr vec3_t kHornetMaxs = { 56.f, 56.f, 80.f };
constexpr vec3_t kCarrierMins = { -56.f, -56.f, -44.f };
constexpr vec3_t kCarrierMaxs = { 56.f, 56.f, 44.f };
constexpr vec3_t kMakronMins = { -30.f, -30.f, 0.f };
constexpr vec3_t kMakronMaxs = { 30.f, 30.f, 90.f };
constexpr vec3_t kTankMins = { -32.f, -32.f, -16.f };
constexpr vec3_t kTankMaxs = { 32.f, 32.f, 64.f };
constexpr vec3_t kShamblerMins = { -32.f, -32.f, -24.f };
constexpr vec3_t kShamblerMaxs = { 32.f, 32.f, 64.f };

} // namespace

// MachineGames campaign values come from the shipped mgu*.bsp entity lumps. Difficulty-
// split encounters use the stronger 1.25 variant for the portable Horde profile; when Horde
// runs on the original map, the surviving authored anchor overrides it with that difficulty's
// exact value.
const std::array<BossDefinition, kHordeBossCount> kBosses = {{
	// Core Quake II roster.
	MakeBoss("supertank", "monster_supertank", "Supertank", nullptr,
		6, 0.80f, 12, kSupertankMins, kSupertankMaxs),
	MakeBoss("guardian", "monster_guardian", "Guardian", nullptr,
		10, 0.60f, 16, { -96.f, -96.f, -66.f }, { 96.f, 96.f, 62.f }),
	MakeBoss("hornet", "monster_boss2", "Hornet", nullptr,
		9, 0.65f, 16, kHornetMins, kHornetMaxs),
	MakeBoss("carrier", "monster_carrier", "Carrier", nullptr,
		10, 0.55f, 18, kCarrierMins, kCarrierMaxs),
	MakeBoss("black_widow", "monster_widow", "Black Widow", nullptr,
		11, 0.50f, 20, { -40.f, -40.f, 0.f }, { 40.f, 40.f, 144.f }),
	MakeBoss("makron", "monster_makron", "Makron", "mgu1m5",
		12, 0.55f, 20, kMakronMins, kMakronMaxs, 1.f, 1.25f),
	MakeBoss("black_widow_ii", "monster_widow2", "Black Widow II", nullptr,
		14, 0.45f, 22, { -70.f, -70.f, 0.f }, { 70.f, 70.f, 144.f }),

	// Call of the Machine: canonical named encounters and its explicit mini-boss.
	MakeBoss("gate_warden", "monster_boss2", "Gate Warden", "mgu1m3",
		6, 0.85f, 18, kHornetMins, kHornetMaxs, 2.f, 1.25f, 1, true),
	MakeBoss("children_of_makron", "monster_makron", "Children of Makron", "mgu1m5",
		12, 0.60f, 18, kMakronMins, kMakronMaxs, 1.f, 0.8f, 2, true),
	MakeBoss("bloodstarved_mutant", "monster_mutant", "Bloodstarved Mutant", "mgu2m2",
		6, 0.90f, 10, { -18.f, -18.f, -24.f }, { 18.f, 18.f, 30.f },
		6.f, 1.5f, 1, true, 1.15f),
	MakeBoss("strogg_supertank", "monster_supertank", "Strogg Supertank", "mgu3m4",
		8, 0.80f, 14, kSupertankMins, kSupertankMaxs, 1.25f, 0.f, 1, true),
	MakeBoss("strogg_carrier", "monster_carrier", "Strogg Carrier", "mgu3m4",
		10, 0.65f, 20, kCarrierMins, kCarrierMaxs, 1.25f, 0.f, 1, true),
	MakeBoss("strogg_megatank", "monster_boss5", "Strogg Megatank", "mgu3m4",
		10, 0.70f, 18, kSupertankMins, kSupertankMaxs, 1.25f, 0.f, 1, true),
	MakeBoss("ancient_carrier", "monster_carrier", "Ancient Carrier", "mgu3secret",
		10, 0.55f, 20, kCarrierMins, kCarrierMaxs, 1.25f, 0.f, 1, true),
	MakeBoss("commander", "monster_tank_commander", "Commander", "mgu4m1",
		8, 0.65f, 16, kTankMins, kTankMaxs, 2.f, 1.3f, 1, true,
		1.f, 16, IT_POWER_SHIELD, 250),
	MakeBoss("garbage_carrier", "monster_carrier", "Garbage Carrier", "mgu4m3",
		10, 0.65f, 20, kCarrierMins, kCarrierMaxs, 1.25f, 0.f, 1, true,
		1.f, 0, -1, -1, 4, "monster_stalker 1"),
	MakeBoss("arachnid", "monster_arachnid", "Arachnid", "mgu5m2",
		9, 0.65f, 15, { -48.f, -48.f, -20.f }, { 48.f, 48.f, 48.f },
		1.5f, 0.f, 1, true),
	MakeBoss("system_administrator", "monster_makron", "The System Administrator", "mgu5m3",
		11, 0.65f, 18, kMakronMins, kMakronMaxs, 0.75f, 0.f, 1, true),
	MakeBoss("janitor", "monster_supertank", "The Janitor", "mgu5m3",
		6, 0.55f, 10, kSupertankMins, kSupertankMaxs, 1.f, 0.2f, 1, true, 1.25f),
	MakeBoss("overburden", "monster_supertank", "Overburden", "mgu6m1",
		8, 0.65f, 14, kSupertankMins, kSupertankMaxs, 1.f, 0.f, 1, true,
		1.f, 0, IT_POWER_SCREEN),
	MakeBoss("underminer", "monster_supertank", "The Underminer", "mgu6m2",
		11, 0.70f, 18, kSupertankMins, kSupertankMaxs, 2.f, 0.f, 1, true),
	MakeBoss("modir", "monster_shambler", "Modir", "mgu6m3",
		12, 0.45f, 28, kShamblerMins, kShamblerMaxs, 40.f, 5.5f, 1, true, 1.15f),
	MakeBoss("servitor_of_creation", "monster_boss2", "Servitor of Creation", "mguboss",
		9, 0.60f, 18, kHornetMins, kHornetMaxs, 1.25f, 1.125f, 1, true,
		1.f, 8),
	MakeBoss("servitors_of_creation", "monster_supertank", "Servitors of Creation", "mguboss",
		10, 0.55f, 18, kSupertankMins, kSupertankMaxs, 1.25f, 0.f, 1, true),
	MakeBoss("masters_of_the_machine", "monster_shambler", "Masters of the Machine", "mguboss",
		11, 0.65f, 14, kShamblerMins, kShamblerMaxs, 3.f, 1.125f, 2, true,
		1.f, 1),
}};

const std::array<DirectorMonster, kHordeAquaticCount> kAquatics = {{
	{ "monster_flipper", "Flipper", 2, -1, 1.00f, 1, { -16.f, -16.f, -8.f }, { 16.f, 16.f, 20.f },
		{ IT_HEALTH_SMALL, IT_HEALTH_SMALL, IT_HEALTH_SMALL, IT_HEALTH_SMALL,
			IT_ARMOR_SHARD, IT_ARMOR_SHARD, IT_HEALTH_MEDIUM } },
	{ "monster_gekk", "Gekk", 2, -1, 0.70f, 2, { -18.f, -18.f, -24.f }, { 18.f, 18.f, 24.f } },
}};

const BossDefinition kFallbackBoss = MakeBoss(
	"tank_commander", "monster_tank_commander", "Tank Commander", nullptr,
	1, 1.f, 8, kTankMins, kTankMaxs);

bool IsLateWave()
{
	return GT(GT_HORDE) && level.round_number > g_horde_content_peak_wave->integer;
}

int CountThemeCandidates(HordeCategory category, int wave)
{
	const bool late_wave = GT(GT_HORDE) && wave > g_horde_content_peak_wave->integer;
	int        count = 0;

	for (const auto &monster : kMonsters) {
		if (monster.min_level != -1 && wave < monster.min_level)
			continue;
		if (monster.max_level != -1 && wave > monster.max_level)
			continue;
		if ((monster.categories & category) == HordeCategory::None)
			continue;

		float weight = monster.weight + ((wave - max(1, monster.min_level)) * monster.lvl_w_adjust);
		if (late_wave)
			weight = max(weight, g_horde_weight_floor->value);
		if (weight <= 0.0f)
			continue;

		count++;
	}

	return count;
}

const ThemeDefinition *FindTheme(Theme theme)
{
	if (theme == Theme::None)
		return nullptr;

	for (const auto &def : kThemes)
		if (def.theme == theme)
			return &def;

	return nullptr;
}

HordeCategory ActiveThemeCategory()
{
	const ThemeDefinition *def = FindTheme(static_cast<Theme>(level.horde_wave_theme));
	return def ? def->category : HordeCategory::None;
}

namespace {

struct PickedItem {
	const WeightedItem *item;
	float               weight;
};

constexpr std::array<item_id_t, 7> kChampionDrops = {
	IT_ARMOR_COMBAT, IT_ARMOR_COMBAT, IT_ARMOR_BODY, IT_BANDOLIER,
	IT_PACK, IT_ADRENALINE, IT_HEALTH_LARGE,
};

constexpr std::array<item_id_t, 6> kBossPowerups = {
	IT_POWERUP_QUAD, IT_POWERUP_DOUBLE, IT_POWERUP_PROTECTION,
	IT_POWERUP_HASTE, IT_POWERUP_REGEN, IT_POWERUP_INVISIBILITY,
};

gitem_t *PickItem()
{
	static std::array<PickedItem, kHordeItemCount> picked_items;
	size_t                                         num_picked_items = 0;
	float                                          total_weight = 0.0f;

	// Past the content peak, freeze the loot curve at the peak so late waves keep dropping the
	// early/mid weapons instead of only top-tier gear. Waves <= peak use the true wave number.
	const int loot_wave = IsLateWave() ? g_horde_content_peak_wave->integer : level.round_number;

	for (const auto &item : kItems) {
		if (item.min_level != -1 && loot_wave < item.min_level)
			continue;
		if (item.max_level != -1 && loot_wave > item.max_level)
			continue;

		// Clamp so "-1 = always available" rows ramp from wave 1, not wave -1.
		const float weight = item.weight + ((loot_wave - max(1, item.min_level)) * item.lvl_w_adjust);

		if (weight <= 0.0f)
			continue;

		total_weight += weight;
		picked_items[num_picked_items++] = { &item, total_weight };
	}

	if (total_weight <= 0.0f)
		return nullptr;

	const float roll = frandom() * total_weight;

	for (size_t i = 0; i < num_picked_items; i++)
		if (roll < picked_items[i].weight)
			return FindItemByClassname(picked_items[i].item->classname);

	return nullptr;
}

const char *PickMonster(const WeightedItem **out_row, int remaining_points, HordeCategory theme_category, uint32_t roster_mask)
{
	static std::array<PickedItem, kHordeMonsterCount> picked_monsters;
	size_t                                            num_picked_monsters = 0;
	float                                             total_weight = 0.0f;
	const bool                                        late_wave = IsLateWave();

	if (out_row)
		*out_row = nullptr;

	for (const auto &monster : kMonsters) {
		if (monster.min_level != -1 && level.round_number < monster.min_level)
			continue;
		if (monster.max_level != -1 && level.round_number > monster.max_level)
			continue;
		if (monster.spawn_points > remaining_points)
			continue;
		if (theme_category != HordeCategory::None && (monster.categories & theme_category) == HordeCategory::None)
			continue;
		if (roster_mask && !(roster_mask & (1u << static_cast<size_t>(&monster - kMonsters.data()))))
			continue;

		// Clamp so "-1 = always available" rows ramp from wave 1, not wave -1.
		float weight = monster.weight + ((level.round_number - max(1, monster.min_level)) * monster.lvl_w_adjust);

		// Past the content peak, decayed weights would cull chaff and starve themes; hold a floor
		// so every still-eligible row stays spendable in late waves.
		if (late_wave)
			weight = max(weight, g_horde_weight_floor->value);

		if (weight <= 0.0f)
			continue;

		total_weight += weight;
		picked_monsters[num_picked_monsters++] = { &monster, total_weight };
	}

	if (total_weight <= 0.0f)
		return nullptr;

	const float roll = frandom() * total_weight;

	for (size_t i = 0; i < num_picked_monsters; i++) {
		if (roll < picked_monsters[i].weight) {
			if (out_row)
				*out_row = picked_monsters[i].item;
			return picked_monsters[i].item->classname;
		}
	}

	return nullptr;
}

// When weighted pick finds nothing (e.g. all weights zero), use the highest-tier affordable row still
// valid for this wave. HordeCategory::None means any; themed waves keep fallback on-category.
const char *PickMonsterFallback(const WeightedItem **out_row, int remaining_points, HordeCategory theme_category = HordeCategory::None)
{
	const WeightedItem *choice = nullptr;

	if (out_row)
		*out_row = nullptr;

	int32_t best_cap = -1;

	for (const auto &monster : kMonsters) {
		if (monster.min_level != -1 && level.round_number < monster.min_level)
			continue;
		if (monster.spawn_points > remaining_points)
			continue;
		if (theme_category != HordeCategory::None && (monster.categories & theme_category) == HordeCategory::None)
			continue;

		const int32_t cap = monster.max_level == -1 ? std::numeric_limits<int32_t>::max() : monster.max_level;
		if (level.round_number > cap)
			continue;

		if (cap > best_cap) {
			best_cap = cap;
			choice = &monster;
		}
	}

	if (!choice) {
		best_cap = -1;
		for (const auto &monster : kMonsters) {
			if (monster.min_level != -1 && level.round_number < monster.min_level)
				continue;
			if (monster.spawn_points > remaining_points)
				continue;
			if (theme_category != HordeCategory::None && (monster.categories & theme_category) == HordeCategory::None)
				continue;

			const int32_t cap = monster.max_level == -1 ? std::numeric_limits<int32_t>::max() : monster.max_level;
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

} // namespace

const WeightedItem *FindMonsterRow(const char *classname)
{
	if (!classname)
		return nullptr;

	for (const auto &monster : kMonsters)
		if (!Q_strcasecmp(monster.classname, classname))
			return &monster;

	return nullptr;
}

const DirectorMonster *FindAquaticRow(const char *classname)
{
	if (!classname)
		return nullptr;

	for (const auto &monster : kAquatics)
		if (!Q_strcasecmp(monster.classname, classname))
			return &monster;

	return nullptr;
}

gitem_t *PickDropItem(const std::array<item_id_t, 8> *drops)
{
	gitem_t *profile_item = nullptr;

	if (drops) {
		std::array<item_id_t, 8> choices = {};
		int                      num_choices = 0;

		for (item_id_t id : *drops) {
			if (id != IT_NULL)
				choices[num_choices++] = id;
		}

		if (num_choices > 0)
			profile_item = GetItemByIndex(choices[irandom(num_choices)]);
	}

	if (profile_item && frandom() < clamp(g_horde_drop_profile_bias->value, 0.f, 1.f))
		return profile_item;

	if (gitem_t *generic = PickItem())
		return generic;

	return profile_item;
}

gitem_t *PickChampionDrop()
{
	gitem_t *item = GetItemByIndex(random_element(kChampionDrops));
	return item ? item : PickItem();
}

gitem_t *PickBossDrop(float powerup_chance)
{
	if (frandom() < clamp(powerup_chance, 0.f, 1.f))
		if (gitem_t *powerup = GetItemByIndex(random_element(kBossPowerups)))
			return powerup;

	return PickChampionDrop();
}

gitem_t *UpgradeDrop(gitem_t *item)
{
	if (!item)
		return nullptr;

	item_id_t upgraded = IT_NULL;
	switch (item->id) {
	case IT_AMMO_SHELLS_SMALL: upgraded = IT_AMMO_SHELLS; break;
	case IT_AMMO_SHELLS: upgraded = IT_AMMO_SHELLS_LARGE; break;
	case IT_AMMO_BULLETS_SMALL: upgraded = IT_AMMO_BULLETS; break;
	case IT_AMMO_BULLETS: upgraded = IT_AMMO_BULLETS_LARGE; break;
	case IT_AMMO_CELLS_SMALL: upgraded = IT_AMMO_CELLS; break;
	case IT_AMMO_CELLS: upgraded = IT_AMMO_CELLS_LARGE; break;
	case IT_AMMO_ROCKETS_SMALL: upgraded = IT_AMMO_ROCKETS; break;
	case IT_AMMO_SLUGS_SMALL: upgraded = IT_AMMO_SLUGS; break;
	case IT_AMMO_SLUGS: upgraded = IT_AMMO_SLUGS_LARGE; break;
	case IT_HEALTH_SMALL: upgraded = IT_HEALTH_MEDIUM; break;
	case IT_HEALTH_MEDIUM: upgraded = IT_HEALTH_LARGE; break;
	case IT_HEALTH_LARGE: upgraded = IT_HEALTH_MEGA; break;
	case IT_ARMOR_SHARD: upgraded = IT_ARMOR_JACKET; break;
	case IT_ARMOR_JACKET: upgraded = IT_ARMOR_COMBAT; break;
	case IT_ARMOR_COMBAT: upgraded = IT_ARMOR_BODY; break;
	default: break;
	}

	gitem_t *upgrade = upgraded != IT_NULL ? GetItemByIndex(upgraded) : nullptr;
	return upgrade ? upgrade : item;
}

const char *PickMonsterForWave(const WeightedItem **out_row, int remaining_points)
{
	// Themed waves and roster waves are mutually exclusive (BeginWave only builds a roster when the
	// theme is NONE).
	const HordeCategory theme_category = ActiveThemeCategory();

	// Themed waves are strict: a category banner means every spawn must be on-category. Try the
	// weighted pick, then a category-respecting fallback; never fall through to the unrestricted
	// pool. Returning null ends the wave cleanly (leftover budget forfeited) instead of spawning an
	// off-theme body under a themed banner.
	if (theme_category != HordeCategory::None) {
		if (const char *pick = PickMonster(out_row, remaining_points, theme_category, 0))
			return pick;
		return PickMonsterFallback(out_row, remaining_points, theme_category);
	}

	// Non-themed waves bias by this wave's roster, then fall through to the unrestricted pool and the
	// fallback picker so a wave can never stall.
	if (level.horde_wave_roster)
		if (const char *pick = PickMonster(out_row, remaining_points, HordeCategory::None, level.horde_wave_roster))
			return pick;

	if (const char *pick = PickMonster(out_row, remaining_points, HordeCategory::None, 0))
		return pick;

	static int32_t fallback_warn_wave = -1;
	const char    *fallback = PickMonsterFallback(out_row, remaining_points);

	if (fallback && fallback_warn_wave != level.round_number) {
		fallback_warn_wave = level.round_number;
		gi.Com_PrintFmt("MM_Horde: no weighted monster for wave {}; using fallback {}\n", level.round_number, fallback);
	}

	return fallback;
}

namespace {

template<size_t N>
const DirectorMonster *PickDirectorMonster(const std::array<DirectorMonster, N> &monsters, int wave,
	int remaining_points, int min_level_floor = std::numeric_limits<int>::min())
{
	std::array<float, N> cumulative = {};
	size_t               count = 0;
	float                total_weight = 0.0f;

	for (const auto &monster : monsters) {
		if (monster.min_level != -1 && wave < monster.min_level)
			continue;
		if (monster.min_level < min_level_floor)
			continue;
		if (monster.max_level != -1 && wave > monster.max_level)
			continue;
		if (monster.spawn_points > remaining_points)
			continue;
		if (monster.weight <= 0.0f)
			continue;

		total_weight += monster.weight;
		cumulative[count++] = total_weight;
	}

	if (count == 0 || total_weight <= 0.0f)
		return nullptr;

	const float roll = frandom() * total_weight;
	size_t      eligible_index = 0;

	for (const auto &monster : monsters) {
		if (monster.min_level != -1 && wave < monster.min_level)
			continue;
		if (monster.min_level < min_level_floor)
			continue;
		if (monster.max_level != -1 && wave > monster.max_level)
			continue;
		if (monster.spawn_points > remaining_points || monster.weight <= 0.0f)
			continue;

		if (roll < cumulative[eligible_index++])
			return &monster;
	}

	return nullptr;
}

void PrecacheMonster(const char *classname)
{
	gentity_t *entity = G_Spawn();
	entity->classname = classname;
	entity->monsterinfo.aiflags |= AI_DO_NOT_COUNT;
	st = {};
	ED_CallSpawn(entity);
	if (entity->inuse)
		G_FreeEntity(entity);
}

} // namespace

const BossDefinition *FindBossDefinition(const char *id)
{
	if (!id || !*id)
		return nullptr;

	for (const auto &boss : kBosses)
		if (!Q_strcasecmp(id, boss.id) || !Q_strcasecmp(id, boss.display_name))
			return &boss;

	if (!Q_strcasecmp(id, kFallbackBoss.id) || !Q_strcasecmp(id, kFallbackBoss.display_name))
		return &kFallbackBoss;

	return nullptr;
}

int EffectiveBossUnits(const BossDefinition &boss)
{
	return MM_Horde_EffectiveBossUnits(boss.units, g_horde_boss_pairs->integer != 0,
		static_cast<int>(MAX_HEALTH_BARS));
}

float EffectiveBossScale(const BossDefinition &boss, float authored_scale)
{
	return MM_Horde_EffectiveBossScale(boss.model_scale, authored_scale,
		max(0.f, g_horde_boss_scale_limit->value));
}

bool BossAvailableForWave(const BossDefinition &boss, int wave)
{
	if (boss.min_level != -1 && wave < boss.min_level)
		return false;
	if (boss.max_level != -1 && wave > boss.max_level)
		return false;
	if (boss.machinegames && !g_horde_boss_machinegames->integer)
		return false;
	if (boss.units > 1 && !g_horde_boss_pairs->integer)
		return false;
	return boss.weight > 0.f;
}

const BossDefinition *PickBossForWave(int wave, const BossDefinition *const *recent,
	size_t recent_count)
{
	int newest_min_wave = 1;
	for (const auto &boss : kBosses)
		if (BossAvailableForWave(boss, wave))
			newest_min_wave = max(newest_min_wave, boss.min_level);

	const size_t requested_exclusions = min(recent_count,
		static_cast<size_t>(max(0, g_horde_boss_repeat_window->integer)));
	auto in_selection_band = [wave, newest_min_wave](const BossDefinition &boss) {
		return MM_Horde_BossInSelectionBand(boss.min_level, newest_min_wave,
			g_horde_boss_tier_window->integer, wave, g_horde_boss_min_wave->integer,
			g_horde_boss_interval->integer);
	};

	// Relax the oldest exclusions one at a time when a narrow tier contains fewer
	// profiles than the configured repeat window.
	for (size_t excluded = requested_exclusions + 1; excluded-- > 0;) {
		float total_weight = 0.f;

		for (const auto &boss : kBosses) {
			if (!BossAvailableForWave(boss, wave) || !in_selection_band(boss))
				continue;

			bool was_recent = false;
			for (size_t i = 0; i < excluded; i++)
				if (recent[i] == &boss) {
					was_recent = true;
					break;
				}
			if (!was_recent)
				total_weight += boss.weight;
		}

		if (total_weight <= 0.f)
			continue;

		const float roll = frandom() * total_weight;
		float cumulative = 0.f;

		for (const auto &boss : kBosses) {
			if (!BossAvailableForWave(boss, wave) || !in_selection_band(boss))
				continue;

			bool was_recent = false;
			for (size_t i = 0; i < excluded; i++)
				if (recent[i] == &boss) {
					was_recent = true;
					break;
				}
			if (was_recent)
				continue;

			cumulative += boss.weight;
			if (roll < cumulative)
				return &boss;
		}
	}

	return nullptr;
}

const DirectorMonster *PickAquaticForWave(int wave, int remaining_points)
{
	return PickDirectorMonster(kAquatics, wave, remaining_points);
}

void PrecacheTableMonsters()
{
	for (const auto &monster : kMonsters)
		PrecacheMonster(monster.classname);
}

void PrecacheDirectorMonsters()
{
	for (size_t i = 0; i < kBosses.size(); i++) {
		bool already_precached = false;
		for (size_t j = 0; j < i; j++)
			if (!Q_strcasecmp(kBosses[i].classname, kBosses[j].classname)) {
				already_precached = true;
				break;
			}
		if (!already_precached)
			PrecacheMonster(kBosses[i].classname);
	}
	for (const auto &monster : kAquatics)
		PrecacheMonster(monster.classname);
}

void PrecacheRewardItems()
{
	auto precache_id = [](item_id_t id) {
		if (gitem_t *item = GetItemByIndex(id))
			PrecacheItem(item);
	};

	for (const auto &item : kItems)
		if (gitem_t *game_item = FindItemByClassname(item.classname))
			PrecacheItem(game_item);

	for (const auto &monster : kMonsters)
		for (item_id_t id : monster.drops)
			if (id != IT_NULL)
				precache_id(id);

	for (item_id_t id : kChampionDrops)
		precache_id(id);
	for (item_id_t id : kBossPowerups)
		precache_id(id);

	static constexpr std::array<item_id_t, 13> upgrades = {
		IT_AMMO_SHELLS_LARGE, IT_AMMO_BULLETS_LARGE, IT_AMMO_CELLS_LARGE,
		IT_AMMO_ROCKETS, IT_AMMO_SLUGS_LARGE, IT_HEALTH_MEDIUM, IT_HEALTH_LARGE,
		IT_HEALTH_MEGA, IT_ARMOR_JACKET, IT_ARMOR_COMBAT, IT_ARMOR_BODY,
		IT_AMMO_SHELLS, IT_AMMO_BULLETS,
	};
	for (item_id_t id : upgrades)
		precache_id(id);
}

} // namespace muffmode::horde
