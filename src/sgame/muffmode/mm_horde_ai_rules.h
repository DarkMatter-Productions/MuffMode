// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>

// Host-testable helpers for horde AI orchestration (Tier 0).

enum class mm_horde_target_role_t {
	Balanced,
	Hunter,
	Bulwark,
};

constexpr float MM_HORDE_MAX_BOSS_SCALE = 16.f;
constexpr float MM_HORDE_MAX_COMBAT_MULTIPLIER = 1000.f;

inline float MM_Horde_ClampFiniteFloat(float value, float fallback, float lower, float upper)
{
	if (lower > upper)
		std::swap(lower, upper);
	if (!std::isfinite(value))
		value = fallback;
	if (!std::isfinite(value))
		value = lower;
	return std::clamp(value, lower, upper);
}

inline int MM_Horde_ClampFiniteInt(double value, int fallback, int lower, int upper)
{
	if (lower > upper)
		std::swap(lower, upper);
	if (!std::isfinite(value))
		return std::clamp(fallback, lower, upper);
	if (value <= static_cast<double>(lower))
		return lower;
	if (value >= static_cast<double>(upper))
		return upper;
	return static_cast<int>(value);
}

inline int MM_Horde_SaturatingAdd(int lhs, int rhs)
{
	const int64_t sum = static_cast<int64_t>(lhs) + static_cast<int64_t>(rhs);
	return static_cast<int>(std::clamp(sum,
		static_cast<int64_t>(std::numeric_limits<int>::min()),
		static_cast<int64_t>(std::numeric_limits<int>::max())));
}

inline int MM_Horde_SaturatingIncrement(int value)
{
	return value < std::numeric_limits<int>::max() ? value + 1 : value;
}

inline float MM_Horde_Probability(float value, float fallback = 0.f)
{
	return MM_Horde_ClampFiniteFloat(value, fallback, 0.f, 1.f);
}

inline int MM_Horde_PresetWeight(int value)
{
	return std::clamp(value, 0, 12);
}

inline bool MM_Horde_ShouldSelectPreset(bool boss_wave, bool allow_boss_waves,
	float chance, float roll)
{
	if (boss_wave && !allow_boss_waves)
		return false;

	const float probability = MM_Horde_Probability(chance);
	return probability > 0.f && std::isfinite(roll) && roll >= 0.f && roll < probability;
}

inline float MM_Horde_PresetEntityScale(float value, float fallback = 1.f)
{
	// This is intentionally narrower than the boss scale range: Wildcard Waves must
	// remain usable on ordinary multiplayer maps with ordinary monster spawn points.
	return MM_Horde_ClampFiniteFloat(value, fallback, 0.5f, 1.5f);
}

inline int MM_Horde_ScaleInt(int value, float scale, int fallback,
	int lower = std::numeric_limits<int>::min(), int upper = std::numeric_limits<int>::max())
{
	if (lower > upper)
		std::swap(lower, upper);
	if (!std::isfinite(scale))
		return std::clamp(fallback, lower, upper);
	return MM_Horde_ClampFiniteInt(static_cast<double>(value) * static_cast<double>(scale),
		fallback, lower, upper);
}

inline int MM_Horde_ComputeWaveBudget(int wave, int base, int per_wave, int min_points,
	int max_points, int peak_wave, float late_factor, float player_multiplier, float map_multiplier)
{
	const int64_t safe_wave = std::max<int64_t>(0, wave);
	const int64_t safe_peak = std::max<int64_t>(0, peak_wave);
	const double growth = MM_Horde_ClampFiniteFloat(late_factor, 0.f, 0.f,
		MM_HORDE_MAX_COMBAT_MULTIPLIER);

	double budget = static_cast<double>(base);
	if (safe_wave <= safe_peak) {
		budget += static_cast<double>(safe_wave) * static_cast<double>(per_wave);
	} else {
		budget += static_cast<double>(safe_peak) * static_cast<double>(per_wave);
		budget += static_cast<double>(safe_wave - safe_peak) * static_cast<double>(per_wave) * growth;
	}

	if (min_points > 0)
		budget = std::max(budget, static_cast<double>(min_points));
	if (max_points > 0)
		budget = std::min(budget, static_cast<double>(max_points));

	const float player_scale = MM_Horde_ClampFiniteFloat(player_multiplier, 1.f, 0.f,
		MM_HORDE_MAX_COMBAT_MULTIPLIER);
	const float map_scale = MM_Horde_ClampFiniteFloat(map_multiplier, 1.f, 0.f,
		MM_HORDE_MAX_COMBAT_MULTIPLIER);
	budget *= static_cast<double>(player_scale) * static_cast<double>(map_scale);

	return MM_Horde_ClampFiniteInt(budget, 1, 1, std::numeric_limits<int>::max());
}

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
	return wave >= min_wave && interval > 0 &&
		((static_cast<int64_t>(wave) - static_cast<int64_t>(min_wave)) % interval) == 0;
}

inline bool MM_Horde_ShouldEliminateMidWaveSpawn(bool wave_in_progress, bool already_eliminated, int lives)
{
	return wave_in_progress && !already_eliminated && lives <= 0;
}

inline bool MM_Horde_SourceMonsterInhibitedBySkill(int skill_level,
	bool not_easy, bool not_medium, bool not_hard)
{
	return (skill_level == 0 && not_easy) ||
		(skill_level == 1 && not_medium) ||
		(skill_level >= 2 && not_hard);
}

inline int MM_Horde_HudCounterValue(int value)
{
	return std::clamp(value, 0, static_cast<int>(std::numeric_limits<int16_t>::max()));
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

inline bool MM_Horde_StallRecoveryDue(bool all_spawned, int living_threats,
	int64_t now_ms, int64_t last_progress_ms, float timeout_seconds)
{
	if (!all_spawned || living_threats <= 0 || now_ms < last_progress_ms)
		return false;

	const float timeout = MM_Horde_ClampFiniteFloat(timeout_seconds, 90.f, 0.f, 3600.f);
	if (timeout <= 0.f)
		return false;

	const int64_t timeout_ms = static_cast<int64_t>(std::ceil(timeout * 1000.f));
	return now_ms - last_progress_ms >= timeout_ms;
}

inline int MM_Horde_PerformanceTier(int kill_streak, int kills_per_tier, int max_tier)
{
	if (kill_streak <= 0 || max_tier <= 0)
		return 0;

	return std::clamp(kill_streak / std::max(1, kills_per_tier), 0, max_tier);
}

inline float MM_Horde_DropChance(float base_chance, float bonus_per_tier, int performance_tier)
{
	base_chance = MM_Horde_ClampFiniteFloat(base_chance, 0.f, 0.f, 1.f);
	bonus_per_tier = MM_Horde_ClampFiniteFloat(bonus_per_tier, 0.f, 0.f, 1.f);
	const double chance = static_cast<double>(base_chance) +
		static_cast<double>(std::max(0, performance_tier)) * bonus_per_tier;
	return static_cast<float>(std::clamp(chance, 0.0, 1.0));
}

inline int MM_Horde_EffectiveMinTypes(int base_min_types, int wave, int ramp_interval, int eligible_types)
{
	if (eligible_types <= 0)
		return 0;

	int64_t minimum = std::max<int64_t>(1, base_min_types);
	if (ramp_interval > 0 && wave > 1)
		minimum += (static_cast<int64_t>(wave) - 1) / ramp_interval;

	return static_cast<int>(std::clamp<int64_t>(minimum, 1, eligible_types));
}

inline int MM_Horde_MonsterUnlockWave(int min_level)
{
	return std::max(1, min_level);
}

inline bool MM_Horde_BossWithinTierWindow(int boss_min_wave, int newest_min_wave, int tier_window)
{
	return static_cast<int64_t>(boss_min_wave) >=
		static_cast<int64_t>(newest_min_wave) - std::max(0, tier_window);
}

inline bool MM_Horde_BossInSelectionBand(int boss_min_wave, int newest_min_wave, int tier_window,
	int wave, int first_boss_wave, int boss_interval)
{
	const int64_t previous_boss_wave = wave <= std::max(1, first_boss_wave)
		? 0
		: static_cast<int64_t>(wave) - std::max(1, boss_interval);
	return static_cast<int64_t>(boss_min_wave) > previous_boss_wave ||
		MM_Horde_BossWithinTierWindow(boss_min_wave, newest_min_wave, tier_window);
}

inline int MM_Horde_EffectiveBossUnits(int authored_units, bool pairs_enabled, int max_health_bars)
{
	if (authored_units <= 1 || !pairs_enabled)
		return 1;

	return std::clamp(authored_units, 1, std::max(1, max_health_bars));
}

inline bool MM_Horde_BossPlacementSufficient(size_t authored_spots, size_t fallback_spots,
	int required_units)
{
	const size_t required = static_cast<size_t>(std::max(1, required_units));
	return authored_spots >= required || fallback_spots >= required - std::min(authored_spots, required);
}

inline float MM_Horde_EffectiveBossScale(float profile_scale, float authored_scale, float scale_limit)
{
	float scale = std::isfinite(authored_scale) && authored_scale > 0.f
		? authored_scale
		: (std::isfinite(profile_scale) && profile_scale > 0.f ? profile_scale : 1.f);

	if (std::isfinite(scale_limit) && scale_limit > 0.f)
		scale = std::min(scale, scale_limit);

	return std::clamp(scale, 0.05f, MM_HORDE_MAX_BOSS_SCALE);
}

inline float MM_Horde_BossWaveMultiplier(int wave, int unlock_wave, float growth_per_wave)
{
	const int64_t waves_since_unlock = std::max<int64_t>(0,
		static_cast<int64_t>(wave) - std::max(1, unlock_wave));
	const float growth = MM_Horde_ClampFiniteFloat(growth_per_wave, 0.f, 0.f,
		MM_HORDE_MAX_COMBAT_MULTIPLIER);
	const double multiplier = 1.0 + static_cast<double>(waves_since_unlock) * growth;
	return static_cast<float>(std::min(multiplier,
		static_cast<double>(MM_HORDE_MAX_COMBAT_MULTIPLIER)));
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
	return MM_Horde_ClampFiniteFloat(mult, 1.f, 0.65f, 1.35f);
}

// Returns a spawn-interval multiplier: >1 speeds spawns when players are coasting,
// <1 slows when they are struggling. Inputs are normalized 0-1 unless noted.
inline float MM_Horde_ComputeAdaptiveSpawnMult(float health_frac, float death_pressure, float clear_ratio)
{
	health_frac = MM_Horde_Probability(health_frac, 1.f);
	death_pressure = MM_Horde_Probability(death_pressure, 0.f);
	clear_ratio = MM_Horde_ClampFiniteFloat(clear_ratio, 1.f, 0.f,
		MM_HORDE_MAX_COMBAT_MULTIPLIER);

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
	const int safe_base = std::max(0, base_cap);
	if (!escalation || round <= peak || safe_base == 0)
		return safe_base;

	const int64_t raw = static_cast<int64_t>(safe_base) +
		(static_cast<int64_t>(round) - static_cast<int64_t>(peak)) * std::max(0, per_wave_bonus);
	const int64_t bounded = std::min(raw,
		static_cast<int64_t>(std::max(safe_base, cap_ceiling)));
	return static_cast<int>(std::clamp(bounded,
		int64_t{ 0 },
		static_cast<int64_t>(std::numeric_limits<int>::max())));
}
