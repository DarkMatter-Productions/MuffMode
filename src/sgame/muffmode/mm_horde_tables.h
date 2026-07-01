// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "g_local.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace muffmode::horde {

// Themed-wave categories. A monster row may carry several (bitwise OR); None = no theme.
enum class HordeCategory : uint32_t {
	None   = 0,
	Swarm  = 1u << 0, // sheer numbers of cheap/light bodies
	Aerial = 1u << 1, // flying - vertical threat
	Heavy  = 1u << 2, // armored bruisers - burst-damage check
	Melee  = 1u << 3, // chargers - keep them at range
	Infest = 1u << 4, // wall/ceiling crawlers - ambush
};
MAKE_ENUM_BITFLAGS(HordeCategory);

constexpr HordeCategory HCAT_SWARM = HordeCategory::Swarm;
constexpr HordeCategory HCAT_AERIAL = HordeCategory::Aerial;
constexpr HordeCategory HCAT_HEAVY = HordeCategory::Heavy;
constexpr HordeCategory HCAT_MELEE = HordeCategory::Melee;
constexpr HordeCategory HCAT_INFEST = HordeCategory::Infest;

// Weighted spawn table row. monsters[] uses drops[] for death loot; items[] uses classname only.
struct WeightedItem {
	const char             *classname;
	int32_t                 min_level = -1;
	int32_t                 max_level = -1;
	float                   weight = 1.0f;
	float                   lvl_w_adjust = 0.0f;
	std::array<item_id_t, 4> drops = {};
	int32_t                 spawn_points = 1;
	HordeCategory           categories = HordeCategory::None; // HCAT_* mask for themed waves
};

enum class Theme : int8_t { None = 0, Swarm, Aerial, Heavy, Melee, Infestation };

// A themed wave filters the monster pool to one category, scales the spawn budget, and shows a banner.
struct ThemeDefinition {
	Theme         theme;
	HordeCategory category;    // HCAT_* this theme allows
	int32_t       min_wave;    // earliest wave this theme can appear
	float         budget_mult; // >1 = more, cheaper bodies; <1 = fewer, tougher
	const char   *announce;    // center-print banner
};

constexpr size_t kHordeItemCount = 21;
constexpr size_t kHordeMonsterCount = 28;
constexpr size_t kHordeThemeCount = 5;

extern const std::array<WeightedItem, kHordeItemCount> kItems;
extern const std::array<WeightedItem, kHordeMonsterCount> kMonsters;
extern const std::array<ThemeDefinition, kHordeThemeCount> kThemes;

bool IsLateWave();
int CountThemeCandidates(HordeCategory category, int wave);
const ThemeDefinition *FindTheme(Theme theme);
HordeCategory ActiveThemeCategory();

gitem_t *PickDropItem(const WeightedItem *monster_row);
gitem_t *PickChampionDrop();
const char *PickMonsterForWave(const WeightedItem **out_row, int remaining_points);
void PrecacheTableMonsters();

} // namespace muffmode::horde
