// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_horde.h"
#include "muffmode/mm_horde_ai.h"
#include "muffmode/mm_horde_ai_rules.h"
#include "muffmode/mm_horde_tables.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

extern cvar_t *g_horde_enhanced_ai;

namespace muffmode::horde {

namespace {

constexpr float kTargetSpreadWeight = 512.0f;
// Rough average spawn_points per monster body; used to turn wave point budget into an expected kill rate.
constexpr float kAdaptivePointsPerMonster = 2.0f;
// Seconds over which we expect a wave's monster bodies to be cleared at a "normal" pace.
constexpr float kAdaptiveWaveSeconds = 90.0f;
// Tactical spawns never pick a spot closer than this to any living fighter (independent of theme).
constexpr float kSpawnMinPlayerDistance = 192.0f;

struct AdaptiveState {
	int     wave = -1;
	int     waveBudgetStart = 0;
	int     playerDeathsWave = 0;
	int     monstersKilledAtStart = 0;
	gtime_t waveStartTime = 0_ms;
};

AdaptiveState adaptive_state;
float         adaptive_last_wave_pressure = 1.0f;

// Single-threaded reuse buffer (game DLL is not multi-threaded).
gtime_t                              target_load_frame = 0_ms;
std::array<int, MAX_CLIENTS + 1>     target_load = {};

gentity_t *ClosestPlayerToPoint(vec3_t point)
{
	float      best_player_distance = std::numeric_limits<float>::max();
	gentity_t *closest = nullptr;

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client) || ec->health <= 0 || ec->client->eliminated)
			continue;

		const vec3_t delta = point - ec->s.origin;
		const float  player_distance = delta.length();

		if (player_distance < best_player_distance) {
			best_player_distance = player_distance;
			closest = ec;
		}
	}

	return closest;
}

void CountMonstersTargetingPlayers(int *counts, int count_capacity)
{
	if (!counts || count_capacity <= 0)
		return;

	std::fill_n(counts, count_capacity, 0);

	for (size_t i = 1; i < globals.num_entities; i++) {
		gentity_t *ent = &g_entities[i];

		if (!ent->inuse || !(ent->svflags & SVF_MONSTER))
			continue;
		if (ent->health <= 0 || ent->deadflag || (ent->svflags & SVF_DEADMONSTER))
			continue;
		if (ent->monsterinfo.aiflags & AI_DO_NOT_COUNT)
			continue;
		if (!ent->enemy || !ent->enemy->client)
			continue;

		const int slot = static_cast<int>(ent->enemy - g_entities);
		if (slot < 1 || slot >= count_capacity)
			continue;

		counts[slot]++;
	}
}

float FighterHealthFraction()
{
	float health_sum = 0.0f;
	float max_sum = 0.0f;
	int   fighters = 0;

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client) || ec->health <= 0 || ec->client->eliminated)
			continue;

		health_sum += static_cast<float>(max(ec->health, 0));
		max_sum += static_cast<float>(max(ec->max_health, 1));
		fighters++;
	}

	if (fighters <= 0 || max_sum <= 0.0f)
		return 1.0f;

	return health_sum / max_sum;
}

bool SpawnPointClear(gentity_t *spot)
{
	const vec3_t point = spot->s.origin + vec3_t{ 0.0f, 0.0f, 9.0f };
	return !gi.trace(point, PLAYER_MINS, PLAYER_MAXS, point, spot, CONTENTS_PLAYER | CONTENTS_MONSTER).startsolid;
}

bool SpawnFarEnoughFromFighters(gentity_t *spot, float min_dist)
{
	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client) || ec->health <= 0 || ec->client->eliminated)
			continue;

		if ((spot->s.origin - ec->s.origin).length() < min_dist)
			return false;
	}

	return true;
}

bool SpawnSpotUsable(gentity_t *spot, vec3_t avoid_point)
{
	if (!spot || !spot->inuse)
		return false;

	float cv_dist = g_dm_respawn_point_min_dist->value;
	if (cv_dist > 512.0f)
		cv_dist = 512.0f;
	else if (cv_dist < 0.0f)
		cv_dist = 0.0f;

	if (avoid_point && cv_dist > 0.0f) {
		const vec3_t delta = spot->s.origin - avoid_point;
		if (delta.length() <= cv_dist)
			return false;
	}

	return true;
}

bool TacticalSpawnSpotValid(gentity_t *spot, vec3_t avoid_point, float min_player_dist)
{
	if (!SpawnSpotUsable(spot, avoid_point))
		return false;
	if (!SpawnFarEnoughFromFighters(spot, min_player_dist))
		return false;
	if (!SpawnPointClear(spot))
		return false;

	return true;
}

bool IsRangedGruntClassname(const char *classname)
{
	if (!classname)
		return false;

	return !Q_strcasecmp(classname, "monster_soldier") ||
		!Q_strcasecmp(classname, "monster_soldier_light") ||
		!Q_strcasecmp(classname, "monster_soldier_ss") ||
		!Q_strcasecmp(classname, "monster_soldier_hypergun") ||
		!Q_strcasecmp(classname, "monster_soldier_lasergun") ||
		!Q_strcasecmp(classname, "monster_soldier_ripper") ||
		!Q_strcasecmp(classname, "monster_infantry") ||
		!Q_strcasecmp(classname, "monster_gunner");
}

bool SupportsBlindfireClassname(const char *classname)
{
	if (!classname)
		return false;

	return !Q_strcasecmp(classname, "monster_gunner") ||
		!Q_strcasecmp(classname, "monster_chick") ||
		!Q_strcasecmp(classname, "monster_chick_heat") ||
		!Q_strcasecmp(classname, "monster_guncmdr") ||
		!Q_strcasecmp(classname, "monster_infantry") ||
		!Q_strcasecmp(classname, "monster_soldier_hypergun") ||
		!Q_strcasecmp(classname, "monster_soldier_lasergun") ||
		!Q_strcasecmp(classname, "monster_soldier_ripper");
}

} // namespace

void RefreshTargetLoadCache()
{
	if (target_load_frame == level.time)
		return;

	target_load_frame = level.time;
	CountMonstersTargetingPlayers(target_load.data(), static_cast<int>(target_load.size()));
}

float AdaptivePressureMult()
{
	if (!g_horde_enhanced_ai->integer)
		return 1.0f;

	const float health_frac = FighterHealthFraction();
	const int   fighters = max(1, MM_Horde_CountFighters());
	const float death_pressure = static_cast<float>(adaptive_state.playerDeathsWave) / static_cast<float>(fighters);

	const int killed = max(0, level.killed_monsters - adaptive_state.monstersKilledAtStart);
	const float elapsed = (level.time - adaptive_state.waveStartTime).seconds();
	const float clear_rate = elapsed > 0.0f ? static_cast<float>(killed) / elapsed : 0.0f;
	// Compare monsters/sec cleared against an expected monsters/sec derived from budget, not raw points.
	const float expected_monsters = adaptive_state.waveBudgetStart > 0
		? static_cast<float>(adaptive_state.waveBudgetStart) / kAdaptivePointsPerMonster
		: 0.0f;
	const float expected_rate = expected_monsters / kAdaptiveWaveSeconds;
	const float clear_ratio = expected_rate > 0.0f ? clear_rate / expected_rate : 1.0f;

	return MM_Horde_ComputeAdaptiveSpawnMult(health_frac, death_pressure, clear_ratio);
}

void Adaptive_BeginWave()
{
	if (!g_horde_enhanced_ai->integer) {
		adaptive_state = {};
		adaptive_last_wave_pressure = 1.0f;
		return;
	}

	const float budget_mult = MM_Horde_ComputeAdaptiveBudgetMult(adaptive_last_wave_pressure);
	if (budget_mult != 1.0f)
		level.horde_spawn_points_remaining =
			max(1, static_cast<int>(level.horde_spawn_points_remaining * budget_mult));

	adaptive_state.wave = level.round_number;
	adaptive_state.waveBudgetStart = level.horde_spawn_points_remaining;
	adaptive_state.playerDeathsWave = 0;
	adaptive_state.monstersKilledAtStart = level.killed_monsters;
	adaptive_state.waveStartTime = level.time;
	adaptive_last_wave_pressure = 1.0f;
}

void Adaptive_RecordWaveEnd()
{
	if (!g_horde_enhanced_ai->integer)
		return;

	adaptive_last_wave_pressure = AdaptivePressureMult();
}

void Adaptive_RecordPlayerDeath()
{
	if (g_horde_enhanced_ai->integer)
		adaptive_state.playerDeathsWave++;
}

select_spawn_result_t SelectSpawnPoint(vec3_t avoid_point)
{
	if (!g_horde_enhanced_ai->integer)
		return SelectDeathmatchSpawnPoint(nullptr, avoid_point, SPAWN_FARTHEST, false, true, false, false);

	struct Candidate {
		gentity_t *spot;
		float      dist;
		float      bearing;
		float      z;
	};

	// Single-threaded reuse buffer (game DLL is not multi-threaded).
	static std::vector<Candidate> candidates;
	candidates.clear();

	vec3_t cluster = vec3_origin;
	int    fighters = 0;

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client) || ec->health <= 0 || ec->client->eliminated)
			continue;

		cluster += ec->s.origin;
		fighters++;
	}

	if (fighters < 1)
		return SelectDeathmatchSpawnPoint(nullptr, avoid_point, SPAWN_FARTHEST, false, true, false, false);

	cluster /= static_cast<float>(fighters);

	float cv_dist = g_dm_respawn_point_min_dist->value;
	if (cv_dist > 512.0f)
		cv_dist = 512.0f;
	else if (cv_dist < 0.0f)
		cv_dist = 0.0f;
	const float min_player_dist = max(kSpawnMinPlayerDistance, cv_dist);

	auto try_add_spot = [&](gentity_t *spot) {
		if (!TacticalSpawnSpotValid(spot, avoid_point, min_player_dist))
			return;

		const vec3_t delta = spot->s.origin - cluster;
		candidates.push_back({
			spot,
			delta.length(),
			std::atan2(delta.y, delta.x),
			spot->s.origin[2],
		});
	};

	gentity_t *spot = nullptr;
	while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_deathmatch")) != nullptr)
		try_add_spot(spot);

	if (candidates.empty()) {
		spot = nullptr;
		while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_team_red")) != nullptr)
			try_add_spot(spot);
		spot = nullptr;
		while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_team_blue")) != nullptr)
			try_add_spot(spot);
	}

	if (candidates.size() <= 1)
		return SelectDeathmatchSpawnPoint(nullptr, avoid_point, SPAWN_FARTHEST, false, true, false, false);

	std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
		return a.dist < b.dist;
	});

	const float min_dist = candidates.front().dist;
	const float max_dist = candidates.back().dist;
	const float dist_span = max(1.0f, max_dist - min_dist);

	const HordeCategory theme = ActiveThemeCategory();
	const bool          prefer_close = (theme & (HCAT_MELEE | HCAT_INFEST)) != HordeCategory::None;
	const bool          prefer_aerial = (theme & HCAT_AERIAL) != HordeCategory::None;

	const float ideal_t = prefer_close ? 0.25f : (prefer_aerial ? 0.75f : 0.55f);
	const float ideal_dist = min_dist + dist_span * ideal_t;
	const float cluster_z = cluster[2];

	gentity_t *best = nullptr;
	float      best_score = std::numeric_limits<float>::lowest();

	for (const auto &candidate : candidates) {
		const float dist_penalty = -std::fabs(candidate.dist - ideal_dist);
		const float aerial_bonus = prefer_aerial ? (candidate.z - cluster_z) * 0.25f : 0.0f;
		const float flank_bonus = (1.0f - std::fabs(candidate.dist - ideal_dist) / dist_span) * 128.0f;
		const float bearing_variety = std::fabs(std::sin(candidate.bearing)) * 32.0f;
		const float score = dist_penalty + aerial_bonus + flank_bonus + bearing_variety + frandom() * 16.0f;

		if (score > best_score) {
			best_score = score;
			best = candidate.spot;
		}
	}

	if (best)
		return { best, true };

	return SelectDeathmatchSpawnPoint(nullptr, avoid_point, SPAWN_FARTHEST, false, true, false, false);
}

void ApplySpawnRoleTuning(gentity_t *ent, const char *classname)
{
	if (!g_horde_enhanced_ai->integer || !ent || !classname)
		return;

	if (IsRangedGruntClassname(classname))
		ent->monsterinfo.combat_style = COMBAT_MIXED;

	if (SupportsBlindfireClassname(classname))
		ent->monsterinfo.blindfire = true;
}

gentity_t *PickTarget(gentity_t *from)
{
	const vec3_t origin = from ? from->s.origin : vec3_origin;

	if (!g_horde_enhanced_ai->integer)
		return ClosestPlayerToPoint(origin);

	RefreshTargetLoadCache();

	gentity_t *best = nullptr;
	float      best_score = std::numeric_limits<float>::max();

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client) || ec->health <= 0 || ec->client->eliminated)
			continue;

		const int slot = static_cast<int>(ec - g_entities);
		if (slot < 0 || slot >= static_cast<int>(target_load.size()))
			continue;

		const float distance = (ec->s.origin - origin).length();
		const float score = MM_Horde_ComputeTargetLoadScore(target_load[slot], distance, kTargetSpreadWeight);

		if (score < best_score) {
			best_score = score;
			best = ec;
		}
	}

	// Same-frame retarget/spawn callers share the frozen load cache; bump the chosen slot so the
	// next picker sees this assignment (e.g. mass re-acquire on death).
	if (best) {
		const int best_slot = static_cast<int>(best - g_entities);
		if (best_slot >= 0 && best_slot < static_cast<int>(target_load.size()))
			target_load[best_slot]++;
	}

	return best ? best : ClosestPlayerToPoint(origin);
}

} // namespace muffmode::horde

gentity_t *MM_Horde_PickTarget(gentity_t *from)
{
	return muffmode::horde::PickTarget(from);
}
