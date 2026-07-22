// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <algorithm>
#include <cmath>

// Host-testable helpers for horde AI orchestration (Tier 0).

inline float MM_Horde_ComputeTargetLoadScore(int monsters_targeting, float distance, float spread_weight)
{
	return monsters_targeting * spread_weight + distance;
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

// A mid-wave (re)spawn with lives remaining is a living respawn, not an elimination —
// mirrors the LMS lives model. Only auto-eliminate spawners who are already out of lives.
inline bool MM_Horde_ShouldEliminateMidWaveSpawn(bool wave_in_progress, bool already_eliminated, int lives)
{
	return wave_in_progress && !already_eliminated && lives <= 0;
}
