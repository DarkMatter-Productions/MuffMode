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
extern cvar_t *g_horde_target_spread_weight;
extern cvar_t *g_horde_retarget_interval;

namespace muffmode::horde {

namespace {

// Rough average spawn_points per monster body; used to turn wave point budget into an expected kill rate.
constexpr float kAdaptivePointsPerMonster = 2.0f;
// Seconds over which we expect a wave's monster bodies to be cleared at a "normal" pace.
constexpr float kAdaptiveWaveSeconds = 90.0f;
// Tactical spawns never pick a spot closer than this to any living fighter (independent of theme).
constexpr float kSpawnMinPlayerDistance = 192.0f;
constexpr float kTargetIsolationWeightFraction = 0.50f;
constexpr float kTargetHealthWeightFraction = 0.375f;

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
gtime_t                              target_load_frame = gtime_t::from_ms(-1);
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
		if (ent->monsterinfo.aiflags & AI_GOOD_GUY)
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

bool SpawnPointClear(gentity_t *spot, const vec3_t &check_mins, const vec3_t &check_maxs)
{
	const float lift = max(1.0f, -check_mins[2]);
	const vec3_t point = spot->s.origin + vec3_t{ 0.0f, 0.0f, lift };
	const trace_t tr = gi.trace(point, check_mins, check_maxs, point, spot, MASK_MONSTERSOLID);
	return !tr.startsolid && !tr.allsolid;
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

bool TacticalSpawnSpotValid(gentity_t *spot, vec3_t avoid_point, float min_player_dist,
	const vec3_t &check_mins, const vec3_t &check_maxs)
{
	if (!SpawnSpotUsable(spot, avoid_point))
		return false;
	if (!SpawnFarEnoughFromFighters(spot, min_player_dist))
		return false;
	if (!SpawnPointClear(spot, check_mins, check_maxs))
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

bool IsHunterClassname(const char *classname)
{
	if (!classname)
		return false;

	return !Q_strcasecmp(classname, "monster_gekk") ||
		!Q_strcasecmp(classname, "monster_berserk") ||
		!Q_strcasecmp(classname, "monster_parasite") ||
		!Q_strcasecmp(classname, "monster_brain") ||
		!Q_strcasecmp(classname, "monster_mutant") ||
		!Q_strcasecmp(classname, "monster_stalker") ||
		!Q_strcasecmp(classname, "monster_flipper");
}

bool IsBulwarkClassname(const char *classname)
{
	if (!classname)
		return false;

	return !Q_strcasecmp(classname, "monster_gladb") ||
		!Q_strcasecmp(classname, "monster_gladiator") ||
		!Q_strcasecmp(classname, "monster_guncmdr") ||
		!Q_strcasecmp(classname, "monster_tank") ||
		!Q_strcasecmp(classname, "monster_tank_commander") ||
		!Q_strcasecmp(classname, "monster_shambler") ||
		!Q_strcasecmp(classname, "monster_supertank") ||
		!Q_strcasecmp(classname, "monster_boss2") ||
		!Q_strcasecmp(classname, "monster_carrier") ||
		!Q_strcasecmp(classname, "monster_makron");
}

bool IsRangedAnchorClassname(const char *classname)
{
	if (!classname)
		return false;

	return !Q_strcasecmp(classname, "monster_gladb") ||
		!Q_strcasecmp(classname, "monster_gladiator") ||
		!Q_strcasecmp(classname, "monster_chick") ||
		!Q_strcasecmp(classname, "monster_chick_heat") ||
		!Q_strcasecmp(classname, "monster_boss2") ||
		!Q_strcasecmp(classname, "monster_carrier") ||
		!Q_strcasecmp(classname, "monster_makron");
}

mm_horde_target_role_t TargetRole(const gentity_t *from)
{
	const char *classname = from ? from->classname : nullptr;

	if (IsHunterClassname(classname))
		return mm_horde_target_role_t::Hunter;
	if (IsBulwarkClassname(classname))
		return mm_horde_target_role_t::Bulwark;
	return mm_horde_target_role_t::Balanced;
}

bool IsAquaticMonster(const gentity_t *from)
{
	return from && ((from->flags & FL_SWIM) ||
		(from->classname && !Q_strcasecmp(from->classname, "monster_flipper")));
}

bool HasLivingWaterFighter()
{
	for (auto ec : active_clients())
		if (ClientIsPlaying(ec->client) && ec->health > 0 && !ec->client->eliminated &&
			ec->waterlevel >= WATER_WAIST)
			return true;

	return false;
}

float NearestLivingAllyDistance(const gentity_t *candidate)
{
	float nearest = std::numeric_limits<float>::max();
	int   allies = 0;

	for (auto ec : active_clients()) {
		if (ec == candidate || !ClientIsPlaying(ec->client) || ec->health <= 0 || ec->client->eliminated)
			continue;

		nearest = min(nearest, (ec->s.origin - candidate->s.origin).length());
		allies++;
	}

	return allies > 0 ? nearest : 0.0f;
}

float TargetScore(gentity_t *from, gentity_t *candidate, int monsters_targeting)
{
	const float distance = (candidate->s.origin - (from ? from->s.origin : vec3_origin)).length();
	const float nearest_ally = NearestLivingAllyDistance(candidate);
	const float health_frac = static_cast<float>(max(candidate->health, 0)) /
		static_cast<float>(max(candidate->max_health, 1));
	const float spread_weight = max(0.0f, g_horde_target_spread_weight->value);

	return MM_Horde_ComputeRoleTargetScore(monsters_targeting, distance, nearest_ally, health_frac,
		spread_weight, spread_weight * kTargetIsolationWeightFraction,
		spread_weight * kTargetHealthWeightFraction, TargetRole(from));
}

gentity_t *FindBestTarget(gentity_t *from, gentity_t *current)
{
	const bool water_only = IsAquaticMonster(from) && HasLivingWaterFighter();
	gentity_t *best = nullptr;
	float      best_score = std::numeric_limits<float>::max();

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client) || ec->health <= 0 || ec->client->eliminated)
			continue;
		if (water_only && ec->waterlevel < WATER_WAIST)
			continue;

		const int slot = static_cast<int>(ec - g_entities);
		if (slot < 0 || slot >= static_cast<int>(target_load.size()))
			continue;

		int load = target_load[slot];
		if (current && ec != current)
			load++;

		const float score = TargetScore(from, ec, load);
		if (score < best_score) {
			best_score = score;
			best = ec;
		}
	}

	return best;
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

select_spawn_result_t SelectSpawnPoint(vec3_t avoid_point, const vec3_t &check_mins, const vec3_t &check_maxs)
{
	struct Candidate {
		gentity_t *spot;
		float      dist;
		float      bearing;
		float      z;
	};

	// Single-threaded reuse buffer (game DLL is not multi-threaded).
	static std::vector<Candidate> candidates;
	static std::vector<Candidate> close_candidates;
	candidates.clear();
	close_candidates.clear();

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
		if (!SpawnSpotUsable(spot, avoid_point) || !SpawnPointClear(spot, check_mins, check_maxs))
			return;

		const vec3_t delta = spot->s.origin - cluster;
		Candidate candidate = {
			spot,
			delta.length(),
			std::atan2(delta.y, delta.x),
			spot->s.origin[2],
		};

		if (TacticalSpawnSpotValid(spot, avoid_point, min_player_dist, check_mins, check_maxs))
			candidates.push_back(candidate);
		else
			close_candidates.push_back(candidate);
	};

	gentity_t *spot = nullptr;
	while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_deathmatch")) != nullptr)
		try_add_spot(spot);

	// If every deathmatch spot is too close, still give team spawns a chance to
	// provide a tactically safe location before falling back to the close set.
	if (candidates.empty()) {
		spot = nullptr;
		while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_team_red")) != nullptr)
			try_add_spot(spot);
		spot = nullptr;
		while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_team_blue")) != nullptr)
			try_add_spot(spot);
	}

	if (candidates.empty())
		candidates.swap(close_candidates);

	if (candidates.empty())
		return SelectDeathmatchSpawnPoint(nullptr, avoid_point, SPAWN_FARTHEST, false, true, false, false);
	if (candidates.size() == 1)
		return { candidates.front().spot, true };

	std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
		return a.dist < b.dist;
	});

	if (!g_horde_enhanced_ai->integer)
		return { candidates.back().spot, true };

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

	if (IsHunterClassname(classname))
		ent->monsterinfo.combat_style = COMBAT_MELEE;
	else if (IsRangedAnchorClassname(classname))
		ent->monsterinfo.combat_style = COMBAT_RANGED;
	else if (IsRangedGruntClassname(classname))
		ent->monsterinfo.combat_style = COMBAT_MIXED;

	if (SupportsBlindfireClassname(classname))
		ent->monsterinfo.blindfire = true;

	const float retarget_sec = max(0.0f, g_horde_retarget_interval->value);
	if (retarget_sec > 0.0f) {
		ent->monsterinfo.horde_retarget_time = level.time +
			random_time(gtime_t::from_sec(retarget_sec * 0.75f), gtime_t::from_sec(retarget_sec * 1.25f));
	}
}

gentity_t *PickTarget(gentity_t *from)
{
	const vec3_t origin = from ? from->s.origin : vec3_origin;

	if (!g_horde_enhanced_ai->integer)
		return ClosestPlayerToPoint(origin);

	RefreshTargetLoadCache();

	gentity_t *best = FindBestTarget(from, nullptr);

	// Same-frame retarget/spawn callers share the frozen load cache; bump the chosen slot so the
	// next picker sees this assignment (e.g. mass re-acquire on death).
	if (best) {
		const int best_slot = static_cast<int>(best - g_entities);
		if (best_slot >= 0 && best_slot < static_cast<int>(target_load.size()))
			target_load[best_slot]++;
	}

	return best ? best : ClosestPlayerToPoint(origin);
}

bool MaybeRetarget(gentity_t *monster)
{
	if (!g_horde_enhanced_ai->integer || !monster || monster->health <= 0)
		return false;
	if (g_horde_retarget_interval->value <= 0.0f || monster->monsterinfo.horde_retarget_time > level.time)
		return false;
	if (monster->monsterinfo.aiflags & (AI_MEDIC | AI_COMBAT_POINT | AI_SOUND_TARGET))
		return false;
	if (!monster->enemy || !monster->enemy->client || monster->enemy->health <= 0 ||
		monster->enemy->client->eliminated)
		return false;

	const float retarget_sec = max(0.1f, g_horde_retarget_interval->value);
	monster->monsterinfo.horde_retarget_time = level.time +
		random_time(gtime_t::from_sec(retarget_sec * 0.75f), gtime_t::from_sec(retarget_sec * 1.25f));

	if (visible(monster, monster->enemy) && range_to(monster, monster->enemy) < 256.0f)
		return false;

	RefreshTargetLoadCache();

	gentity_t *current = monster->enemy;
	gentity_t *best = FindBestTarget(monster, current);
	if (!best || best == current)
		return false;

	const int current_slot = static_cast<int>(current - g_entities);
	const int best_slot = static_cast<int>(best - g_entities);
	if (current_slot < 0 || current_slot >= static_cast<int>(target_load.size()) ||
		best_slot < 0 || best_slot >= static_cast<int>(target_load.size()))
		return false;

	const float current_score = TargetScore(monster, current, target_load[current_slot]);
	const float best_score = TargetScore(monster, best, target_load[best_slot] + 1);
	const float switch_margin = max(32.0f, g_horde_target_spread_weight->value * 0.25f);
	if (best_score + switch_margin >= current_score)
		return false;

	target_load[current_slot] = max(0, target_load[current_slot] - 1);
	target_load[best_slot]++;
	monster->oldenemy = nullptr;
	monster->enemy = best;
	monster->goalentity = best;
	FoundTarget(monster);
	return true;
}

} // namespace muffmode::horde

gentity_t *MM_Horde_PickTarget(gentity_t *from)
{
	return muffmode::horde::PickTarget(from);
}

bool MM_Horde_MaybeRetarget(gentity_t *monster)
{
	return muffmode::horde::MaybeRetarget(monster);
}
