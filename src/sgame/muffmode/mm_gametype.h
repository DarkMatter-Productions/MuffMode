// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "muffmode/mm_arena_rules.h"
#include "muffmode/mm_gametype_table.h"

#include <string>

// Resolve an arbitrary cvar integer at the game/module boundary before it is
// used as an array index. Keeping the requested and effective values together
// also makes Arena's fail-closed map activation rule impossible to bypass at a
// call site that remembered to clamp but forgot to apply the Arena fallback.
struct mm_gametype_boundary_t {
	int configured_index;
	int effective_index;
	bool requested_in_range;
};

inline constexpr mm_gametype_boundary_t MM_ResolveGametypeBoundary(
	int requested, int first, int last, int arena_gametype,
	int fallback_gametype, bool arena_active) noexcept
{
	const int configured = requested < first ? first :
		(requested > last ? last : requested);
	return {
		configured,
		MM_ArenaEffectiveGametype(configured, arena_gametype,
			fallback_gametype, arena_active),
		requested == configured
	};
}

// [MuffMode] The one resolution both the sanitizer and gameplay use.
//
// Previously the sanitizer enforced availability while the gameplay macros only
// clamped the range, so the two could disagree for a whole level:
//   * `g_gametype 11` (GT_BALL, removed) survived the clamp, and GTF() read
//     _gt[GT_BALL] == 0 -- no frags, no teams, no rounds -- until the next
//     poller tick, which never runs during a timeout.
//   * an out-of-range-high value clamped onto GT_LAST, which is GT_ARENA and
//     is Enabled, so `g_gametype 9999` silently selected MuffMode Arena rather
//     than the documented safe default.
// Folding range and availability into one resolver removes both.
struct mm_gametype_resolution_t {
	int  configured;			// in range and available
	int  effective;				// configured, with Arena failed closed
	int  flags;
	bool requested_in_range;
	bool requested_available;
};

inline constexpr mm_gametype_resolution_t MM_ResolveGametypeState(
	int requested, bool arena_active) noexcept
{
	const mm_gametype_boundary_t bounded = MM_ResolveGametypeBoundary(
		requested, (int)GT_FIRST, (int)GT_LAST, (int)GT_ARENA,
		(int)GT_FFA, arena_active);

	// Out-of-range falls to Deathmatch at both ends rather than landing on
	// whichever enumerator happens to sit at the boundary.
	const int ranged = bounded.requested_in_range
		? bounded.configured_index : (int)GT_FFA;

	const bool available =
		MM_GT_TABLE[static_cast<size_t>(ranged)].availability ==
		gametype_avail_t::Enabled;
	const int configured = available ? ranged : (int)GT_FFA;

	const int effective = MM_ArenaEffectiveGametype(
		configured, (int)GT_ARENA, (int)GT_FFA, arena_active);

	return { configured, effective,
		MM_GT_TABLE[static_cast<size_t>(effective)].flags,
		bounded.requested_in_range, available };
}

// [MuffMode] Gametype/ruleset orchestration hooks.
gametype_avail_t MM_GetGametypeAvailability(gametype_t gt);
bool             MM_IsGametypeEnabled(gametype_t gt);
gametype_t       MM_CurrentGametype();
int              MM_CurrentGametypeFlags();
bool             MM_GametypeHasFlag(int flag);
void             MM_SanitizeCurrentGametype();
std::string      MM_GetEnabledGametypesList();

void MM_CheckRuleset();
void MM_ChangeGametype(gametype_t gt, bool force_cfg = false);
void MM_GTSetLongName();

namespace muffmode {
namespace gametype {

// True when a bad gametype index would index off the end of the descriptor
// table. Kept as a named predicate because the diagnostics distinguish an
// out-of-range index from an in-range but unavailable one.
bool MM_IsValidGametypeIndex(gametype_t gt);

} // namespace gametype
} // namespace muffmode
