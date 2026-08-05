// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <algorithm>
#include <cmath>

// [MuffMode] Placement rules behind deathmatch prop respawning (misc_explobox and
// func_explosive), ported from WORR's Respawn_Think. A destroyed prop does not
// blink back the instant its timer expires: it waits until no player can see the
// spot, nothing is standing in it, and nobody is close enough for the arrival to
// feel like a teleport. Kept free of game headers so the host suite can exercise
// the geometry without a live entity world.

struct mm_ent_respawn_vec_t {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

// A prop stays away while a player is turned even loosely toward its spot. WORR
// uses a cosine of 0.15, so anything inside roughly 81 degrees of the view axis
// counts as "in front".
inline constexpr float MM_ENT_RESPAWN_FACING_DOT = 0.15f;

// Half-extent of the keep-clear box around the spot. Nothing returns while a
// player's bounding box overlaps it, even when the spot is behind them.
inline constexpr float MM_ENT_RESPAWN_CLEARANCE = 128.0f;

// The telefrag probe is lifted this far so a prop resting on the floor does not
// read its own ground plane as an obstruction.
inline constexpr float MM_ENT_RESPAWN_PROBE_LIFT = 9.0f;

// How long a blocked spot waits before it is tested again.
inline constexpr float MM_ENT_RESPAWN_RETRY_SECONDS = 1.0f;

// Bounds on the configured delay. The floor keeps a prop from cycling faster than
// the retry cadence can even measure; the ceiling is an hour, well past any round.
inline constexpr float MM_ENT_RESPAWN_MIN_DELAY_SECONDS = 1.0f;
inline constexpr float MM_ENT_RESPAWN_MAX_DELAY_SECONDS = 3600.0f;

// Returns the configured respawn delay in seconds, or 0 when the feature is off.
// Zero, negatives and NaN all read as "disabled" rather than clamping up to the
// floor, so an operator can switch prop respawning off without a second cvar.
inline float MM_EntRespawnDelaySeconds(float configured)
{
	if (!(configured > 0.0f))
		return 0.0f;

	return std::clamp(configured, MM_ENT_RESPAWN_MIN_DELAY_SECONDS,
		MM_ENT_RESPAWN_MAX_DELAY_SECONDS);
}

// Midpoint of a spawn-time bounding box in world space. Brush models carry an
// origin of 0,0,0 with world-space bounds, so the recorded origin alone is not a
// usable reference point for either the visibility or the proximity test.
inline mm_ent_respawn_vec_t MM_EntRespawnBoxCenter(const mm_ent_respawn_vec_t &origin,
	const mm_ent_respawn_vec_t &mins, const mm_ent_respawn_vec_t &maxs)
{
	return {
		origin.x + (mins.x + maxs.x) * 0.5f,
		origin.y + (mins.y + maxs.y) * 0.5f,
		origin.z + (mins.z + maxs.z) * 0.5f
	};
}

// True when target lies within the facing cone of a viewer looking along forward,
// which must be unit length.
inline bool MM_EntRespawnFacesPoint(const mm_ent_respawn_vec_t &viewer,
	const mm_ent_respawn_vec_t &forward, const mm_ent_respawn_vec_t &target)
{
	const float dx = target.x - viewer.x;
	const float dy = target.y - viewer.y;
	const float dz = target.z - viewer.z;
	const float length_squared = dx * dx + dy * dy + dz * dz;

	// A viewer standing exactly on the spot has no direction to compare against.
	// Normalizing anyway yields NaN, and every NaN comparison reads as "not
	// facing" -- which would drop the prop straight into the player. Treat a
	// zero-length separation as the most blocking answer instead.
	if (!(length_squared > 0.0f))
		return true;

	const float dot = (dx * forward.x + dy * forward.y + dz * forward.z) /
		std::sqrt(length_squared);

	return dot > MM_ENT_RESPAWN_FACING_DOT;
}

// Standard AABB overlap; touching faces count as overlapping.
inline bool MM_EntRespawnBoxesOverlap(const mm_ent_respawn_vec_t &a_mins,
	const mm_ent_respawn_vec_t &a_maxs, const mm_ent_respawn_vec_t &b_mins,
	const mm_ent_respawn_vec_t &b_maxs)
{
	return !(b_mins.x > a_maxs.x || b_maxs.x < a_mins.x ||
		b_mins.y > a_maxs.y || b_maxs.y < a_mins.y ||
		b_mins.z > a_maxs.z || b_maxs.z < a_mins.z);
}

// True when a player box sits inside the keep-clear volume around a spot.
inline bool MM_EntRespawnWithinClearance(const mm_ent_respawn_vec_t &center,
	const mm_ent_respawn_vec_t &player_mins, const mm_ent_respawn_vec_t &player_maxs)
{
	const mm_ent_respawn_vec_t clear_mins {
		center.x - MM_ENT_RESPAWN_CLEARANCE,
		center.y - MM_ENT_RESPAWN_CLEARANCE,
		center.z - MM_ENT_RESPAWN_CLEARANCE
	};
	const mm_ent_respawn_vec_t clear_maxs {
		center.x + MM_ENT_RESPAWN_CLEARANCE,
		center.y + MM_ENT_RESPAWN_CLEARANCE,
		center.z + MM_ENT_RESPAWN_CLEARANCE
	};

	return MM_EntRespawnBoxesOverlap(clear_mins, clear_maxs, player_mins, player_maxs);
}
