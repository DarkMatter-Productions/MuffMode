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

// Strategy-driven targeting: a monster picks a fighter by combining bounded utility terms
// into a weighted mean, then multiplying by a reachability gate. Higher score wins.
enum class mm_horde_strategy_t : uint8_t {
	Balanced = 0,
	Swarm,
	Hunter,
	Ambusher,
	Skirmisher,
	Artillery,
	Bruiser,
	Support,
	Aquatic,
	Count,
};

// Soft-term weights plus the three per-strategy shape parameters.
struct mm_horde_strategy_weights_t {
	float prox = 0.f;
	float free = 0.f;
	float threat = 0.f;
	float vuln = 0.f;
	float isolation = 0.f;
	float prox_half = 768.f;   // world units at which the proximity term reads 0.5
	float gate_bias = 0.f;     // added to the hull-derived reachability gate sharpening
	float switch_scale = 1.f;  // multiplier on the base retarget switch margin
};

// The five bounded soft terms for one (monster, candidate) pair.
struct mm_horde_target_terms_t {
	float prox = 0.f;
	float free = 0.f;
	float threat = 0.f;
	float vuln = 0.f;
	float isolation = 0.f;
};

constexpr float MM_HORDE_MAX_BOSS_SCALE = 16.f;
constexpr float MM_HORDE_MAX_COMBAT_MULTIPLIER = 1000.f;
constexpr float MM_HORDE_MAX_TARGET_WEIGHT = 8.f;
constexpr float MM_HORDE_MAX_WEIGHT_TUNING = 2.f;
// A provably unreachable fighter still scores this fraction of its core, so a monster
// hemmed in by unreachable fighters picks the least-bad one instead of stalling on a tie.
constexpr float MM_HORDE_GATE_FLOOR = 0.05f;

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

// ---------------------------------------------------------------------------------------
// Strategy targeting: soft utility terms. Each returns a bounded [0,1] value where higher
// means "more attractive target", so no term can dominate by unit magnitude alone.
// ---------------------------------------------------------------------------------------

// Replaces the legacy unbounded raw-distance term, which is why distance used to swamp
// every other consideration at range. Strictly decreasing, never saturating, never zero.
inline float MM_Horde_ProximityUtility(float distance_units, float half_units)
{
	const float half = MM_Horde_ClampFiniteFloat(half_units, 768.f, 1.f, 1.0e6f);
	const float distance = MM_Horde_ClampFiniteFloat(distance_units, 0.f, 0.f, 1.0e9f);
	return half / (half + distance);
}

// Target-load freedom: fewer monsters already committed to a fighter means more attractive.
inline float MM_Horde_LoadUtility(int monsters_targeting, float load_span)
{
	const float span = MM_Horde_ClampFiniteFloat(load_span, 3.f, 1.f, 1024.f);
	const float load = static_cast<float>(std::max(0, monsters_targeting));
	return 1.f - std::clamp(load / span, 0.f, 1.f);
}

// The target-load histogram counts every live monster against its own current enemy, so a
// retargeting monster is already inside its incumbent's tally but not inside any challenger's.
// Scoring the raw counts would hand every challenger a free one-attacker advantage, which is
// larger than the switch margin and would make a monster alternate targets forever. Score each
// challenger as it would read after the switch instead.
inline int MM_Horde_ComparableTargetLoad(int cached_load, bool is_incumbent, bool has_incumbent)
{
	if (!has_incumbent || is_incumbent)
		return cached_load;

	return MM_Horde_SaturatingIncrement(cached_load);
}

// Straggler detection. A fighter with no living allies at all is maximally isolated; the
// legacy helper returned 0 (reading as "maximally grouped") in that case.
inline float MM_Horde_IsolationUtility(float nearest_ally_units, int ally_count, float span_units)
{
	if (ally_count <= 0)
		return 1.f;

	const float span = MM_Horde_ClampFiniteFloat(span_units, 1024.f, 1.f, 1.0e6f);
	const float nearest = MM_Horde_ClampFiniteFloat(nearest_ally_units, 0.f, 0.f, 1.0e9f);
	return std::clamp(nearest / span, 0.f, 1.f);
}

// How dangerous a fighter is right now. Sub-weights sum to 1, so the result is bounded
// without a divide. Every input is a per-moment signal, never a per-match total.
inline float MM_Horde_ThreatUtility(float powerup01, float weapon01, float streak01,
	float firing01, float amp01)
{
	return std::clamp(
		0.32f * MM_Horde_Probability(powerup01) +
		0.28f * MM_Horde_Probability(weapon01) +
		0.20f * MM_Horde_Probability(streak01) +
		0.12f * MM_Horde_Probability(firing01) +
		0.08f * MM_Horde_Probability(amp01), 0.f, 1.f);
}

// How finishable a fighter is right now. Sub-weights sum to 1. A protected or invisible
// fighter is a poor finisher pick rather than an invisible one.
inline float MM_Horde_VulnerabilityUtility(float health_deficit01, float defense_deficit01,
	float hurt01, float dry01, float helpless01, bool protected_now)
{
	float score =
		0.34f * MM_Horde_Probability(health_deficit01) +
		0.20f * MM_Horde_Probability(defense_deficit01) +
		0.18f * MM_Horde_Probability(hurt01) +
		0.16f * MM_Horde_Probability(dry01) +
		0.12f * MM_Horde_Probability(helpless01);

	if (protected_now)
		score *= 0.30f;

	return std::clamp(score, 0.f, 1.f);
}

// ---------------------------------------------------------------------------------------
// Hull and traversal. Size never gets a soft term of its own: it modulates the reachability
// gate and three weights, which keeps the per-term dominance bound a true bound.
// ---------------------------------------------------------------------------------------

// 0 for a nimble body, 1 for a genuinely large one. Derived from the live hull width, so it
// automatically tracks s.scale, boss scaling, and Wildcard preset scaling.
inline float MM_Horde_HullBulk(float hull_width_units, float nimble_units, float bulk_units)
{
	float nimble = MM_Horde_ClampFiniteFloat(nimble_units, 40.f, 0.f, 1.0e6f);
	float bulk = MM_Horde_ClampFiniteFloat(bulk_units, 96.f, 0.f, 1.0e6f);
	if (nimble > bulk)
		std::swap(nimble, bulk);

	const float span = std::max(1.f, bulk - nimble);
	const float width = MM_Horde_ClampFiniteFloat(hull_width_units, nimble, 0.f, 1.0e6f);
	return std::clamp((width - nimble) / span, 0.f, 1.f);
}

// Vertical reach in world units. The can_jump gate mirrors the engine: traversal heights are
// only ever submitted to the pathfinder when the monster can jump or flies.
inline float MM_Horde_ClimbBudget(bool flyer, bool swimmer, bool can_jump,
	float jump_height, float drop_height, float step_budget)
{
	if (flyer)
		return 1024.f;
	if (swimmer)
		return 320.f;

	const float step = MM_Horde_ClampFiniteFloat(step_budget, 18.f, 0.f, 512.f);
	if (!can_jump)
		return step;

	const float up = MM_Horde_ClampFiniteFloat(jump_height, 0.f, 0.f, 4096.f);
	const float down = MM_Horde_ClampFiniteFloat(drop_height, 0.f, 0.f, 4096.f);
	return step + std::max(up, down);
}

// A wider body needs a gentler ramp to gain the same height.
inline float MM_Horde_ClimbSlope(float base_slope, float bulk01)
{
	const float base = MM_Horde_ClampFiniteFloat(base_slope, 1.f, 0.f, 8.f);
	return base * (1.f - 0.5f * MM_Horde_Probability(bulk01));
}

// Large, ground-bound bodies weigh reachability far more heavily than small agile ones.
inline float MM_Horde_GateSharpen(float gate_bias, float bulk01, float agility01)
{
	const float bias = MM_Horde_ClampFiniteFloat(gate_bias, 0.f, -1.f, 1.f);
	const float bulk = MM_Horde_Probability(bulk01);
	const float agility = MM_Horde_Probability(agility01);
	return std::clamp(bias + bulk * (1.f - agility), 0.f, 1.f);
}

// ---------------------------------------------------------------------------------------
// Reachability. A multiplicative gate rather than a soft term, so "cannot get there" is a
// real veto instead of one more thing to outvote.
// ---------------------------------------------------------------------------------------

// A slope model, not a raw height delta: a walker climbs dz over a horizontal run when the
// average grade is gentle enough. A raw delta would make a heavy refuse a fighter one storey
// up a long ramp it can actually walk.
inline float MM_Horde_VerticalAccess(float vertical_delta, float horizontal_units,
	float climb_budget, float slope, float span_units)
{
	const float rise = std::fabs(MM_Horde_ClampFiniteFloat(vertical_delta, 0.f, -1.0e9f, 1.0e9f));
	const float run = MM_Horde_ClampFiniteFloat(horizontal_units, 0.f, 0.f, 1.0e9f);
	const float budget = MM_Horde_ClampFiniteFloat(climb_budget, 18.f, 0.f, 1.0e6f);
	const float grade = MM_Horde_ClampFiniteFloat(slope, 1.f, 0.f, 8.f);
	const float span = MM_Horde_ClampFiniteFloat(span_units, 512.f, 1.f, 1.0e6f);

	const float climbable = budget + grade * run;
	if (rise <= climbable)
		return 1.f;

	return std::clamp(1.f - (rise - climbable) / span, 0.f, 1.f);
}

// Product of the independent feasibility signals. An enemy we are currently in contact with
// is reachable by definition, so contact floors the result.
inline float MM_Horde_ComputeAccess(float area_factor, float vertical_factor,
	float habitat_factor, float phs_factor, float evidence_factor,
	bool in_contact, float contact_floor)
{
	float access = MM_Horde_Probability(area_factor, 1.f) *
		MM_Horde_Probability(vertical_factor, 1.f) *
		MM_Horde_Probability(habitat_factor, 1.f) *
		MM_Horde_Probability(phs_factor, 1.f) *
		MM_Horde_Probability(evidence_factor, 1.f);

	if (in_contact)
		access = std::max(access, MM_Horde_Probability(contact_floor, 0.9f));

	return std::clamp(access, 0.f, 1.f);
}

// sharpen 0 leaves the gate linear in access; sharpen 1 squares it. One multiply-add, no powf.
inline float MM_Horde_GateFromAccess(float access01, float sharpen, float gate_floor)
{
	const float access = MM_Horde_Probability(access01, 1.f);
	const float sharp = MM_Horde_Probability(sharpen, 0.f);
	const float floor_value = MM_Horde_Probability(gate_floor, MM_HORDE_GATE_FLOOR);
	const float shaped = access * (1.f - sharp + sharp * access);
	return std::clamp(floor_value + (1.f - floor_value) * shaped, 0.f, 1.f);
}

// ---------------------------------------------------------------------------------------
// Unreachable memory. A per-fighter bit mask, not a single remembered slot: marking B must
// never un-mark A, or a monster oscillates forever between two fighters it cannot reach.
// ---------------------------------------------------------------------------------------

inline bool MM_Horde_UnreachMaskTest(uint64_t lo, uint64_t hi, int slot)
{
	if (slot < 1 || slot > 128)
		return false;

	return slot <= 64
		? ((lo >> (slot - 1)) & uint64_t{ 1 }) != 0
		: ((hi >> (slot - 65)) & uint64_t{ 1 }) != 0;
}

inline void MM_Horde_UnreachMaskSet(uint64_t &lo, uint64_t &hi, int slot)
{
	if (slot < 1 || slot > 128)
		return;

	if (slot <= 64)
		lo |= uint64_t{ 1 } << (slot - 1);
	else
		hi |= uint64_t{ 1 } << (slot - 65);
}

inline void MM_Horde_UnreachMaskClear(uint64_t &lo, uint64_t &hi, int slot)
{
	if (slot < 1 || slot > 128)
		return;

	if (slot <= 64)
		lo &= ~(uint64_t{ 1 } << (slot - 1));
	else
		hi &= ~(uint64_t{ 1 } << (slot - 65));
}

// Fresh evidence of "no route to this fighter" is a near-veto that decays back to neutral,
// so a monster retries once the world has had time to change (doors, lifts, the fighter moving).
inline float MM_Horde_UnreachFactor(bool marked, int64_t age_ms, int64_t window_ms,
	float floor_factor)
{
	if (!marked || window_ms <= 0 || age_ms < 0 || age_ms >= window_ms)
		return 1.f;

	const float floor_value = MM_Horde_Probability(floor_factor, 0.12f);
	const float progress = static_cast<float>(age_ms) / static_cast<float>(window_ms);
	return std::clamp(floor_value + (1.f - floor_value) * progress, 0.f, 1.f);
}

// ---------------------------------------------------------------------------------------
// Weights and final score.
// ---------------------------------------------------------------------------------------

// Every strategy keeps max(weight)/total <= 0.35, which is what makes "no single factor
// dominates" a mechanical property rather than a tuning claim.
inline mm_horde_strategy_weights_t MM_Horde_StrategyWeights(mm_horde_strategy_t strategy)
{
	switch (strategy) {
	case mm_horde_strategy_t::Swarm:
		return { 1.8f, 1.8f, 0.5f, 0.8f, 0.3f, 640.f, -0.10f, 0.75f };
	case mm_horde_strategy_t::Hunter:
		return { 1.5f, 1.0f, 0.5f, 1.5f, 1.3f, 768.f, -0.10f, 0.85f };
	case mm_horde_strategy_t::Ambusher:
		return { 1.1f, 0.9f, 0.4f, 1.4f, 1.8f, 1024.f, -0.15f, 0.85f };
	case mm_horde_strategy_t::Skirmisher:
		return { 1.7f, 1.2f, 1.0f, 1.0f, 0.5f, 896.f, -0.35f, 0.90f };
	case mm_horde_strategy_t::Artillery:
		return { 1.0f, 1.3f, 1.7f, 0.7f, 0.4f, 1280.f, 0.10f, 1.30f };
	case mm_horde_strategy_t::Bruiser:
		return { 1.3f, 1.2f, 1.5f, 0.7f, 0.3f, 1152.f, 0.35f, 1.60f };
	case mm_horde_strategy_t::Support:
		return { 1.5f, 1.2f, 1.2f, 0.6f, 0.0f, 768.f, 0.05f, 1.20f };
	case mm_horde_strategy_t::Aquatic:
		return { 1.5f, 1.1f, 0.7f, 1.1f, 0.6f, 768.f, 0.20f, 1.10f };
	case mm_horde_strategy_t::Balanced:
	case mm_horde_strategy_t::Count:
	default:
		return { 1.6f, 1.4f, 0.9f, 0.9f, 0.5f, 768.f, 0.00f, 1.00f };
	}
}

// g_horde_target_spread_weight keeps its name and its 512 default but now scales a bounded
// weight: 512 maps to 1.0, and 0 still means "ignore target load entirely" as documented.
inline float MM_Horde_NormalizedSpreadWeight(float spread_cvar, float reference, float max_weight)
{
	const float ref = MM_Horde_ClampFiniteFloat(reference, 512.f, 1.f, 1.0e6f);
	const float max_w = MM_Horde_ClampFiniteFloat(max_weight, MM_HORDE_MAX_TARGET_WEIGHT,
		0.f, MM_HORDE_MAX_TARGET_WEIGHT);
	const float raw = MM_Horde_ClampFiniteFloat(spread_cvar, 512.f, 0.f, 1.0e6f);
	return std::clamp(raw / ref, 0.f, max_w);
}

// A bulkier body stops chasing stragglers into tight geometry and grows less distance-sensitive,
// because it would rather travel further to a fighter it can actually reach.
inline mm_horde_strategy_weights_t MM_Horde_ApplySizeToWeights(
	mm_horde_strategy_weights_t weights, float bulk01)
{
	const float bulk = MM_Horde_Probability(bulk01);
	weights.isolation = std::max(0.f, weights.isolation) * (1.f - 0.50f * bulk);
	weights.prox = std::max(0.f, weights.prox) * (1.f - 0.25f * bulk);
	weights.prox_half = MM_Horde_ClampFiniteFloat(weights.prox_half, 768.f, 1.f, 8192.f) *
		(1.f + 0.25f * bulk);
	return weights;
}

inline mm_horde_strategy_weights_t MM_Horde_ApplyWeightTuning(mm_horde_strategy_weights_t weights,
	float spread_normalized, float threat_mult, float vuln_mult)
{
	const float spread = MM_Horde_ClampFiniteFloat(spread_normalized, 1.f, 0.f,
		MM_HORDE_MAX_TARGET_WEIGHT);
	const float threat = MM_Horde_ClampFiniteFloat(threat_mult, 1.f, 0.f, MM_HORDE_MAX_WEIGHT_TUNING);
	const float vuln = MM_Horde_ClampFiniteFloat(vuln_mult, 1.f, 0.f, MM_HORDE_MAX_WEIGHT_TUNING);

	weights.free = std::clamp(std::max(0.f, weights.free) * spread, 0.f, MM_HORDE_MAX_TARGET_WEIGHT);
	weights.threat = std::clamp(std::max(0.f, weights.threat) * threat, 0.f, MM_HORDE_MAX_TARGET_WEIGHT);
	weights.vuln = std::clamp(std::max(0.f, weights.vuln) * vuln, 0.f, MM_HORDE_MAX_TARGET_WEIGHT);
	weights.prox = std::clamp(std::max(0.f, weights.prox), 0.f, MM_HORDE_MAX_TARGET_WEIGHT);
	weights.isolation = std::clamp(std::max(0.f, weights.isolation), 0.f, MM_HORDE_MAX_TARGET_WEIGHT);
	return weights;
}

// Weighted mean of the bounded soft terms. The most any one term can move the result is
// exactly its own weight share, which is the dominance bound the strategy table maintains.
inline float MM_Horde_ComputeCoreUtility(const mm_horde_target_terms_t &terms,
	const mm_horde_strategy_weights_t &weights)
{
	const float wp = std::max(0.f, weights.prox);
	const float wf = std::max(0.f, weights.free);
	const float wt = std::max(0.f, weights.threat);
	const float wv = std::max(0.f, weights.vuln);
	const float wi = std::max(0.f, weights.isolation);
	const float total = wp + wf + wt + wv + wi;

	if (!(total > 0.f))
		return 0.f;

	const float sum = wp * MM_Horde_Probability(terms.prox) +
		wf * MM_Horde_Probability(terms.free) +
		wt * MM_Horde_Probability(terms.threat) +
		wv * MM_Horde_Probability(terms.vuln) +
		wi * MM_Horde_Probability(terms.isolation);

	return std::clamp(sum / total, 0.f, 1.f);
}

// Higher is better. Note this is the opposite sense from the legacy role scorer.
inline float MM_Horde_ComputeGatedTargetScore(const mm_horde_target_terms_t &terms,
	const mm_horde_strategy_weights_t &weights, float access01, float sharpen, float gate_floor)
{
	return MM_Horde_ComputeCoreUtility(terms, weights) *
		MM_Horde_GateFromAccess(access01, sharpen, gate_floor);
}

inline float MM_Horde_TargetSwitchMargin(float base_margin, float switch_scale)
{
	const float base = MM_Horde_ClampFiniteFloat(base_margin, 0.05f, 0.f, 1.f);
	const float scale = MM_Horde_ClampFiniteFloat(switch_scale, 1.f, 0.f, 8.f);
	return std::clamp(base * scale, 0.f, 1.f);
}

// Hysteresis, with one escape: staying latched onto a fighter the body provably cannot reach
// is the exact failure this model exists to fix, so a collapsed incumbent waives the margin.
inline bool MM_Horde_ShouldSwitchTarget(float best_score, float current_score, float margin,
	float current_access, float best_access, float abandon_threshold, float rescue_threshold)
{
	if (!std::isfinite(best_score) || !std::isfinite(current_score))
		return false;

	const float current = MM_Horde_Probability(current_access, 1.f);
	const float best = MM_Horde_Probability(best_access, 1.f);
	const float abandon = MM_Horde_Probability(abandon_threshold, 0.25f);
	const float rescue = MM_Horde_Probability(rescue_threshold, 0.55f);
	const float effective = (current < abandon && best >= rescue)
		? 0.f
		: MM_Horde_ClampFiniteFloat(margin, 0.05f, 0.f, 1.f);

	return best_score > current_score + effective;
}

// Relentless pursuit: vanilla parks a monster for 5-10 seconds after a nav failure, which
// reads as the threat losing interest. Horde caps that lockout so a transient path failure
// becomes a short retry instead. Returns the clamped absolute lockout time in milliseconds.
inline int64_t MM_Horde_ClampPursuitLockoutMs(int64_t lockout_ms, int64_t now_ms, float max_seconds)
{
	const float seconds = MM_Horde_ClampFiniteFloat(max_seconds, 2.f, 0.f, 60.f);
	const int64_t limit = now_ms + static_cast<int64_t>(std::ceil(static_cast<double>(seconds) * 1000.0));
	return std::min(lockout_ms, limit);
}

// True when a blind pursuer covered less ground than the sample window demands, meaning it
// is wedged on geometry rather than closing. The first sample of a chase has nothing to
// compare against, so it never reports a stall.
inline bool MM_Horde_PursuitStalled(bool sample_valid, float moved_units, float min_progress_units)
{
	if (!sample_valid || !std::isfinite(moved_units))
		return false;

	return moved_units < std::max(0.f, min_progress_units);
}

// True when the cached pursuit goal has drifted far enough from the live target to be
// re-pinned. Vanilla walks a player trail that goes cold, stranding monsters behind the fight.
inline bool MM_Horde_ShouldRepinPursuitGoal(float goal_to_target_units, float slack_units)
{
	if (!std::isfinite(goal_to_target_units))
		return true;

	return goal_to_target_units > std::max(0.f, slack_units);
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
