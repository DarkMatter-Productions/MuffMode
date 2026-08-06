// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// [MuffMode] Tally, timing and presentation rules behind the post-scoreboard
// next-map pick, ported from WORR's MapSelector. WORR runs the pick from a
// client-side RmlUi page; MuffMode is server-side only and draws it through the
// layout menu instead, so only the arithmetic is shared. Kept free of game
// headers so the host suite can exercise it without a live level.

// WORR offers three maps and there is no reason to differ: the layout menu is
// read at a glance during a few seconds of intermission, not browsed.
inline constexpr size_t MM_MAP_PICK_CHOICES = 3;

// Bounds on the configured pick window. The floor leaves time to read three
// names and move the cursor; the ceiling keeps a misconfigured server from
// parking everyone at the intermission camera.
inline constexpr int MM_MAP_PICK_MIN_SECONDS = 5;
inline constexpr int MM_MAP_PICK_MAX_SECONDS = 60;

// Width of the countdown bar. MENU_MAX_LINE_CHARS is 28, so a full bar still
// fits on one centred row with room to spare.
inline constexpr int MM_MAP_PICK_BAR_SEGMENTS = 24;

using mm_map_pick_counts_t = std::array<int, MM_MAP_PICK_CHOICES>;
using mm_map_pick_offered_t = std::array<bool, MM_MAP_PICK_CHOICES>;

// Returns how long the pick stays open, or 0 when the feature is switched off.
// Zero and negatives read as "disabled" rather than clamping up to the floor, so
// one cvar both enables the pick and sets its length.
inline int MM_MapPickDurationSeconds(int configured)
{
	if (configured <= 0)
		return 0;

	return std::clamp(configured, MM_MAP_PICK_MIN_SECONDS, MM_MAP_PICK_MAX_SECONDS);
}

// True once a map holds more than half the eligible voters, which ends the pick
// early. Integer division is deliberate and matches WORR: with four voters a map
// needs three, so the remaining two voters could not have overturned it.
inline bool MM_MapPickHasMajority(int leader_votes, int voters)
{
	if (voters <= 0)
		return false;

	return leader_votes > voters / 2;
}

// Resolves the winning choice, or -1 when nothing was offered. `roll` is any
// value from the caller's RNG; it only breaks ties, so an unvoted pick still
// lands on a real map instead of falling through to the outgoing one.
inline int MM_MapPickWinner(const mm_map_pick_counts_t &counts,
	const mm_map_pick_offered_t &offered, uint32_t roll)
{
	int best = 0;
	for (size_t i = 0; i < MM_MAP_PICK_CHOICES; i++)
		if (offered[i])
			best = std::max(best, counts[i]);

	// A map that nobody voted for is still a candidate when nobody voted at all,
	// so the "no votes" case draws from every offer rather than from a tie at 0.
	std::array<int, MM_MAP_PICK_CHOICES> pool {};
	size_t pool_size = 0;
	for (size_t i = 0; i < MM_MAP_PICK_CHOICES; i++) {
		if (!offered[i])
			continue;
		if (best > 0 && counts[i] != best)
			continue;
		pool[pool_size++] = static_cast<int>(i);
	}

	if (pool_size == 0)
		return -1;

	return pool[roll % pool_size];
}

// Filled segment count for the countdown bar. Rounds up so the bar only empties
// once the pick is genuinely over.
inline int MM_MapPickBarSegments(float remaining_seconds, float total_seconds)
{
	if (!(total_seconds > 0.0f) || !(remaining_seconds > 0.0f))
		return 0;

	const float fraction = std::min(remaining_seconds / total_seconds, 1.0f);
	const int filled = static_cast<int>(
		std::ceil(fraction * static_cast<float>(MM_MAP_PICK_BAR_SEGMENTS)));

	return std::clamp(filled, 0, MM_MAP_PICK_BAR_SEGMENTS);
}
