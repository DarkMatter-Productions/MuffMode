// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <algorithm>
#include <cmath>

// [MuffMode] Pure gib presentation maths, kept free of game headers so the host
// tests can exercise it directly. mm_gibs.cpp static_asserts the threshold below
// against GIB_HEALTH so the two cannot drift.

// Post-mortem health a player has to fall below before gibbing at all.
constexpr int MM_GIBS_HEALTH_THRESHOLD = -40;

// Health span that buys one extra severity tier.
constexpr int MM_GIBS_SEVERITY_STEP = 40;

constexpr int MM_GIBS_MAX_SEVERITY = 4;

// Severity tier 1-4, measured from the gib threshold rather than from zero: a
// player who died at exactly the threshold is tier 1, and each further 40 points
// of overkill buys another tier.
constexpr int MM_GibsSeverity(int health) {
	const int overkill = MM_GIBS_HEALTH_THRESHOLD - health;
	if (overkill <= 0)
		return 1;

	return std::min((overkill / MM_GIBS_SEVERITY_STEP) + 1, MM_GIBS_MAX_SEVERITY);
}

// Upper bound on each limb model a death may throw, before the per-death random
// roll picks an actual count in [0, max].
struct mm_gib_limb_budget_t {
	int legs = 0;
	int bones = 0;
	int forearms = 0;
	int arms = 0;
	int torsos = 0;
};

constexpr mm_gib_limb_budget_t MM_GibsLimbBudget(int severity) {
	const int tier = std::clamp(severity, 1, MM_GIBS_MAX_SEVERITY);

	mm_gib_limb_budget_t budget;
	budget.legs = std::min(tier, 2);
	budget.bones = std::min(tier * 2, 4);
	budget.forearms = std::min(tier, 2);
	budget.arms = std::min(tier, 2);
	// A torso chunk only reads as a torso when there is enough of the player
	// left over to have produced one.
	budget.torsos = tier >= 3 ? 1 : 0;
	return budget;
}

constexpr int MM_GibsLimbBudgetTotal(const mm_gib_limb_budget_t &budget) {
	return budget.legs + budget.bones + budget.forearms + budget.arms + budget.torsos;
}

// Meat chunks a deathmatch death throws. Vanilla stacks each tier it passes on
// top of a flat 8, so a deep overkill pays for all of them.
constexpr int MM_GibsMeatCount(int health, bool deathmatch) {
	int count = 8;
	if (!deathmatch)
		return count;

	if (health < -100)
		count += 10;
	if (health < -200)
		count += 12;
	if (health < -300)
		count += 16;

	return count;
}

// Worst case a single death can cost the entity pool: every meat tier, every
// limb maximum, and the head.
constexpr int MM_GibsWorstCaseCount() {
	return MM_GibsMeatCount(-1000, true) +
		MM_GibsLimbBudgetTotal(MM_GibsLimbBudget(MM_GIBS_MAX_SEVERITY)) + 1;
}

struct mm_gib_launch_t {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

// Named so the three limits cannot be passed in the wrong order.
struct mm_gib_launch_bounds_t {
	float max_horizontal = 600.0f;
	float min_vertical = 150.0f;
	float max_vertical = 600.0f;
};

// Bounds a launch velocity without squaring off its direction. Vanilla clamps
// each axis on its own, which both rotates the spray toward the diagonals and
// discards the knockback the killing blow already applied to the corpse; scaling
// the horizontal component against a single ceiling keeps the bearing intact.
inline mm_gib_launch_t MM_GibsClipLaunch(mm_gib_launch_t velocity,
	const mm_gib_launch_bounds_t &bounds) {
	const float horizontal = std::sqrt((velocity.x * velocity.x) + (velocity.y * velocity.y));

	if (horizontal > bounds.max_horizontal && horizontal > 0.0f) {
		const float scale = bounds.max_horizontal / horizontal;
		velocity.x *= scale;
		velocity.y *= scale;
	}

	velocity.z = std::clamp(velocity.z, bounds.min_vertical, bounds.max_vertical);
	return velocity;
}

// Per-frame multiplier for a "retain this fraction of speed per second" drag, so
// that water motion does not shift with the server's tick rate.
inline float MM_GibsWaterDrag(float retained_per_second, float frame_time_s) {
	if (retained_per_second <= 0.0f)
		return 0.0f;
	if (retained_per_second >= 1.0f)
		return 1.0f;

	return std::pow(retained_per_second, frame_time_s);
}
