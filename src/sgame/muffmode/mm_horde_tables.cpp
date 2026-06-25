// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_horde_tables.h"

#include <array>
#include <limits>

extern cvar_t *g_horde_content_peak_wave;
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
	{ "monster_soldier_light", -1, -1, 1.00f, -0.04f, { IT_HEALTH_SMALL }, 1, HCAT_SWARM },
	{ "monster_soldier", -1, -1, 0.75f, -0.03f, { IT_AMMO_BULLETS_SMALL, IT_HEALTH_SMALL }, 1, HCAT_SWARM },
	{ "monster_soldier_ss", 2, 9, 0.85f, -0.08f, { IT_AMMO_SHELLS_SMALL, IT_HEALTH_SMALL }, 1, HCAT_SWARM },
	// early variety
	{ "monster_gekk", 2, 10, 1.35f, -0.10f, {}, 2, HCAT_SWARM | HCAT_MELEE | HCAT_INFEST },
	{ "monster_soldier_hypergun", 2, 10, 0.90f, 0.0f, { IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL }, 2 },
	{ "monster_soldier_lasergun", 3, 10, 0.90f, 0.03f, { IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL }, 2 },
	{ "monster_soldier_ripper", 3, 10, 0.90f, 0.03f, { IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL }, 2 },
	{ "monster_infantry", 3, -1, 1.05f, 0.05f, { IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS }, 2 },
	{ "monster_flyer", 3, -1, 1.10f, 0.02f, { IT_AMMO_CELLS_SMALL }, 2, HCAT_AERIAL },
	// mid-tier
	{ "monster_gunner", 4, -1, 1.05f, 0.15f, { IT_AMMO_GRENADES, IT_AMMO_BULLETS_SMALL }, 3 },
	// max_level was 14 (capped at the finale); uncapped to -1 so Melee/Infestation/Heavy/Aerial
	// themes keep on-category bodies past wave 14. Active through wave 14 already, so waves 1-14
	// are unchanged - this only adds them at wave 15+.
	{ "monster_berserk", 4, -1, 1.05f, 0.05f, { IT_ARMOR_SHARD }, 3, HCAT_MELEE },
	{ "monster_parasite", 4, -1, 1.00f, -0.05f, {}, 3, HCAT_INFEST },
	{ "monster_gladb", 5, -1, 1.00f, 0.05f, { IT_AMMO_CELLS_SMALL }, 3, HCAT_HEAVY },
	{ "monster_stalker", 5, -1, 0.95f, 0.05f, { IT_AMMO_CELLS_SMALL }, 3, HCAT_INFEST },
	{ "monster_brain", 6, -1, 0.95f, 0.0f, { IT_AMMO_CELLS_SMALL }, 3, HCAT_MELEE | HCAT_INFEST },
	{ "monster_mutant", 6, -1, 0.90f, 0.0f, {}, 3, HCAT_MELEE },
	{ "monster_floater", 6, -1, 0.90f, 0.0f, {}, 3, HCAT_AERIAL },
	{ "monster_gladiator", 7, -1, 1.00f, 0.10f, { IT_AMMO_SLUGS }, 4, HCAT_HEAVY },
	// heavies
	{ "monster_hover", 8, -1, 0.85f, 0.0f, {}, 4, HCAT_AERIAL },
	{ "monster_guncmdr", 8, -1, 0.50f, 0.10f, { IT_AMMO_GRENADES, IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS, IT_AMMO_CELLS_SMALL }, 5, HCAT_HEAVY },
	{ "monster_chick", 8, -1, 0.95f, 0.0f, { IT_AMMO_ROCKETS_SMALL, IT_AMMO_ROCKETS }, 4 },
	{ "monster_daedalus", 9, -1, 0.85f, 0.05f, { IT_AMMO_CELLS_SMALL }, 5, HCAT_AERIAL },
	{ "monster_medic", 9, -1, 0.80f, 0.0f, { IT_HEALTH_SMALL, IT_HEALTH_MEDIUM }, 5 },
	{ "monster_tank", 10, -1, 0.80f, 0.05f, { IT_AMMO_ROCKETS }, 6, HCAT_HEAVY },
	{ "monster_chick_heat", 10, -1, 0.85f, 0.05f, { IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS }, 4 },
	{ "monster_shambler", 10, -1, 0.75f, 0.05f, {}, 6, HCAT_HEAVY },
	// finale
	{ "monster_tank_commander", 11, -1, 0.45f, 0.15f, { IT_AMMO_ROCKETS_SMALL, IT_AMMO_BULLETS_SMALL, IT_AMMO_ROCKETS, IT_AMMO_BULLETS }, 8, HCAT_HEAVY },
	{ "monster_medic_commander", 11, -1, 0.40f, 0.12f, { IT_AMMO_CELLS_SMALL, IT_HEALTH_MEDIUM, IT_HEALTH_LARGE }, 8 },
}};

static_assert(kMonsters.size() <= 32, "horde_wave_roster bitmask supports at most 32 monster rows");

const std::array<ThemeDefinition, kHordeThemeCount> kThemes = {{
	{ Theme::Swarm,       HCAT_SWARM,  3, 1.30f, "THE SWARM APPROACHES!" },
	{ Theme::Aerial,      HCAT_AERIAL, 4, 1.00f, "AERIAL ASSAULT!" },
	{ Theme::Heavy,       HCAT_HEAVY,  7, 0.80f, "HEAVY ASSAULT!" },
	{ Theme::Melee,       HCAT_MELEE,  4, 1.10f, "THEY'RE CLOSING IN!" },
	{ Theme::Infestation, HCAT_INFEST, 4, 1.15f, "INFESTATION!" },
}};

bool IsLateWave()
{
	return GT(GT_HORDE) && level.round_number > g_horde_content_peak_wave->integer;
}

int CountThemeCandidates(uint32_t category, int wave)
{
	const bool late_wave = GT(GT_HORDE) && wave > g_horde_content_peak_wave->integer;
	int        count = 0;

	for (const auto &monster : kMonsters) {
		if (monster.min_level != -1 && wave < monster.min_level)
			continue;
		if (monster.max_level != -1 && wave > monster.max_level)
			continue;
		if (!(monster.categories & category))
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

uint32_t ActiveThemeCategory()
{
	const ThemeDefinition *def = FindTheme(static_cast<Theme>(level.horde_wave_theme));
	return def ? def->category : 0;
}

namespace {

struct PickedItem {
	const WeightedItem *item;
	float               weight;
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

const char *PickMonster(const WeightedItem **out_row, int remaining_points, uint32_t theme_category, uint32_t roster_mask)
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
		if (theme_category && !(monster.categories & theme_category))
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
// valid for this wave. theme_category (0 = any) keeps a themed wave's fallback on-category.
const char *PickMonsterFallback(const WeightedItem **out_row, int remaining_points, uint32_t theme_category = 0)
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
		if (theme_category && !(monster.categories & theme_category))
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
			if (theme_category && !(monster.categories & theme_category))
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

gitem_t *PickDropItem(const WeightedItem *monster_row)
{
	if (monster_row) {
		std::array<item_id_t, 4> choices = {};
		int                      num_choices = 0;

		for (item_id_t id : monster_row->drops) {
			if (id != IT_NULL)
				choices[num_choices++] = id;
		}

		if (num_choices > 0)
			return GetItemByIndex(choices[irandom(num_choices)]);
	}

	return PickItem();
}

gitem_t *PickChampionDrop()
{
	static constexpr std::array<item_id_t, 6> champion_drops = {
		IT_ARMOR_COMBAT, IT_ARMOR_COMBAT, IT_ARMOR_BODY,
		IT_ADRENALINE, IT_POWERUP_DOUBLE, IT_POWERUP_QUAD,
	};

	gitem_t *item = GetItemByIndex(random_element(champion_drops));
	return item ? item : PickItem();
}

const char *PickMonsterForWave(const WeightedItem **out_row, int remaining_points)
{
	// Themed waves and roster waves are mutually exclusive (BeginWave only builds a roster when the
	// theme is NONE).
	const uint32_t theme_category = ActiveThemeCategory();

	// Themed waves are strict: a category banner means every spawn must be on-category. Try the
	// weighted pick, then a category-respecting fallback; never fall through to the unrestricted
	// pool. Returning null ends the wave cleanly (leftover budget forfeited) instead of spawning an
	// off-theme body under a themed banner.
	if (theme_category) {
		if (const char *pick = PickMonster(out_row, remaining_points, theme_category, 0))
			return pick;
		return PickMonsterFallback(out_row, remaining_points, theme_category);
	}

	// Non-themed waves bias by this wave's roster, then fall through to the unrestricted pool and the
	// fallback picker so a wave can never stall.
	if (level.horde_wave_roster)
		if (const char *pick = PickMonster(out_row, remaining_points, 0, level.horde_wave_roster))
			return pick;

	if (const char *pick = PickMonster(out_row, remaining_points, 0, 0))
		return pick;

	static int32_t fallback_warn_wave = -1;
	const char    *fallback = PickMonsterFallback(out_row, remaining_points);

	if (fallback && fallback_warn_wave != level.round_number) {
		fallback_warn_wave = level.round_number;
		gi.Com_PrintFmt("MM_Horde: no weighted monster for wave {}; using fallback {}\n", level.round_number, fallback);
	}

	return fallback;
}

void PrecacheTableMonsters()
{
	for (const auto &monster : kMonsters) {
		gentity_t *entity = G_Spawn();
		entity->classname = monster.classname;
		// Don't let precache spawns inflate level.total_monsters; it starves the warmup spawner,
		// which caps on total_monsters - killed_monsters.
		entity->monsterinfo.aiflags |= AI_DO_NOT_COUNT;
		ED_CallSpawn(entity);
		if (entity->inuse)
			G_FreeEntity(entity);
	}
}

} // namespace muffmode::horde
