// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

// Host-testable helpers for horde AI orchestration (Tier 0).

enum class mm_horde_target_role_t {
	Balanced,
	Hunter,
	Bulwark,
};

inline float MM_Horde_ComputeTargetLoadScore(int monsters_targeting, float distance, float spread_weight)
{
	return monsters_targeting * spread_weight + distance;
}

inline float MM_Horde_ComputeRoleTargetScore(int monsters_targeting, float distance, float nearest_ally_distance,
	float health_frac, float spread_weight, float isolation_weight, float health_weight,
	mm_horde_target_role_t role)
{
	float score = MM_Horde_ComputeTargetLoadScore(monsters_targeting, std::max(distance, 0.f),
		std::max(spread_weight, 0.f));

	const float isolation = std::clamp(nearest_ally_distance / 1024.f, 0.f, 1.f);
	health_frac = std::clamp(health_frac, 0.f, 1.f);

	if (role == mm_horde_target_role_t::Hunter)
		score -= isolation * std::max(isolation_weight, 0.f);
	else if (role == mm_horde_target_role_t::Bulwark)
		score -= health_frac * std::max(health_weight, 0.f);

	return score;
}

inline bool MM_Horde_IsBossWave(int wave, int min_wave, int interval)
{
	return wave >= min_wave && interval > 0 && ((wave - min_wave) % interval) == 0;
}

inline bool MM_Horde_ShouldEliminateMidWaveSpawn(bool wave_in_progress, bool already_eliminated, int lives)
{
	return wave_in_progress && !already_eliminated && lives <= 0;
}

inline bool MM_Horde_ShouldRestAfterSpawn(int spawns_in_burst, int burst_size)
{
	return burst_size > 0 && spawns_in_burst >= burst_size;
}

inline bool MM_Horde_ReinforcementReady(int kills, int kills_required, int used, int max_per_wave)
{
	return max_per_wave > 0 && used < max_per_wave && kills >= std::max(1, kills_required);
}

inline bool MM_Horde_WaveCleared(bool all_spawned, int living_threats)
{
	return all_spawned && living_threats <= 0;
}

inline int MM_Horde_PerformanceTier(int kill_streak, int kills_per_tier, int max_tier)
{
	if (kill_streak <= 0 || max_tier <= 0)
		return 0;

	return std::clamp(kill_streak / std::max(1, kills_per_tier), 0, max_tier);
}

inline float MM_Horde_DropChance(float base_chance, float bonus_per_tier, int performance_tier)
{
	return std::clamp(base_chance + std::max(0, performance_tier) * bonus_per_tier, 0.f, 1.f);
}

inline int MM_Horde_EffectiveMinTypes(int base_min_types, int wave, int ramp_interval, int eligible_types)
{
	if (eligible_types <= 0)
		return 0;

	int minimum = std::max(1, base_min_types);
	if (ramp_interval > 0 && wave > 1)
		minimum += (wave - 1) / ramp_interval;

	return std::clamp(minimum, 1, eligible_types);
}

inline int MM_Horde_MonsterUnlockWave(int min_level)
{
	return std::max(1, min_level);
}

inline bool MM_Horde_BossWithinTierWindow(int boss_min_wave, int newest_min_wave, int tier_window)
{
	return boss_min_wave >= newest_min_wave - std::max(0, tier_window);
}

inline bool MM_Horde_BossInSelectionBand(int boss_min_wave, int newest_min_wave, int tier_window,
	int wave, int first_boss_wave, int boss_interval)
{
	const int previous_boss_wave = wave <= std::max(1, first_boss_wave)
		? 0
		: wave - std::max(1, boss_interval);
	return boss_min_wave > previous_boss_wave ||
		MM_Horde_BossWithinTierWindow(boss_min_wave, newest_min_wave, tier_window);
}

inline int MM_Horde_EffectiveBossUnits(int authored_units, bool pairs_enabled, int max_health_bars)
{
	if (authored_units <= 1 || !pairs_enabled)
		return 1;

	return std::clamp(authored_units, 1, std::max(1, max_health_bars));
}

inline float MM_Horde_EffectiveBossScale(float profile_scale, float authored_scale, float scale_limit)
{
	float scale = authored_scale > 0.f
		? authored_scale
		: (profile_scale > 0.f ? profile_scale : 1.f);

	if (scale_limit > 0.f)
		scale = std::min(scale, scale_limit);

	return std::max(0.05f, scale);
}

inline float MM_Horde_BossWaveMultiplier(int wave, int unlock_wave, float growth_per_wave)
{
	const int waves_since_unlock = std::max(0, wave - std::max(1, unlock_wave));
	return 1.f + waves_since_unlock * std::max(0.f, growth_per_wave);
}

inline bool MM_Horde_BossEncounterDefeated(bool deployment_pending, int living_bosses)
{
	return !deployment_pending && living_bosses <= 0;
}

inline int MM_Horde_ScaleOutgoingDamage(int damage, float scale)
{
	if (damage <= 0 || !std::isfinite(scale) || scale <= 0.f || scale == 1.f)
		return damage;

	const double scaled = std::ceil(static_cast<double>(damage) * static_cast<double>(scale));
	if (scaled >= static_cast<double>(std::numeric_limits<int>::max()))
		return std::numeric_limits<int>::max();

	return std::max(1, static_cast<int>(scaled));
}

inline float MM_Horde_ClampAdaptiveSpawnMult(float mult)
{
	return std::clamp(mult, 0.65f, 1.35f);
}

// Returns a spawn-interval multiplier: >1 speeds spawns when players are coasting,
// <1 slows when they are struggling. Inputs are normalized 0-1 unless noted.
inline float MM_Horde_ComputeAdaptiveSpawnMult(float health_frac, float death_pressure, float clear_ratio)
{
	health_frac = std::clamp(health_frac, 0.f, 1.f);
	death_pressure = std::clamp(death_pressure, 0.f, 1.f);
	clear_ratio = std::max(0.f, clear_ratio);

	float mult = 1.f;

	if (health_frac > 0.65f && death_pressure < 0.35f && clear_ratio > 1.1f) {
		const float coast = std::min({ (health_frac - 0.65f) / 0.35f,
			(0.35f - death_pressure) / 0.35f,
			(clear_ratio - 1.1f) / 0.9f });
		mult = 1.f + 0.35f * std::clamp(coast, 0.f, 1.f);
	} else if (health_frac < 0.4f || death_pressure > 0.75f || clear_ratio < 0.55f) {
		float struggle = 0.f;
		if (health_frac < 0.4f)
			struggle = std::max(struggle, (0.4f - health_frac) / 0.4f);
		if (death_pressure > 0.75f)
			struggle = std::max(struggle, (death_pressure - 0.75f) / 0.25f);
		if (clear_ratio < 0.55f)
			struggle = std::max(struggle, (0.55f - clear_ratio) / 0.55f);
		mult = 1.f - 0.35f * std::clamp(struggle, 0.f, 1.f);
	}

	return MM_Horde_ClampAdaptiveSpawnMult(mult);
}

inline float MM_Horde_ComputeAdaptiveBudgetMult(float prev_wave_pressure)
{
	prev_wave_pressure = MM_Horde_ClampAdaptiveSpawnMult(prev_wave_pressure);
	if (prev_wave_pressure > 1.05f)
		return std::clamp(prev_wave_pressure, 1.f, 1.08f);
	if (prev_wave_pressure < 0.9f)
		return std::clamp(prev_wave_pressure, 0.92f, 1.f);
	return 1.f;
}

// Post-peak budget taper: escalation on uses the stronger on_factor; off keeps legacy off_factor.
inline float MM_Horde_EffectiveLateWaveFactor(bool escalation, float off_factor, float on_factor)
{
	return escalation ? on_factor : off_factor;
}

// Post-peak concurrent monster cap: ramps per wave past peak, clamped. Waves at or before peak use base_cap.
inline int MM_Horde_LateMaxAlive(int base_cap, int round, int peak, int per_wave_bonus, int cap_ceiling, bool escalation)
{
	if (!escalation || round <= peak)
		return base_cap;

	const int raw = base_cap + (round - peak) * per_wave_bonus;
	return std::min(raw, cap_ceiling);
}
