// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "muffmode/mm_arena_rules.h"

#include <string>

enum gametype_t : int;

enum class gametype_avail_t {
	Enabled,
	Disabled,
	Removed,
};

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

// [MuffMode] Gametype/ruleset orchestration hooks.
gametype_avail_t MM_GetGametypeAvailability(gametype_t gt);
bool             MM_IsGametypeEnabled(gametype_t gt);
gametype_t       MM_CurrentGametype();
int              MM_CurrentGametypeFlags();
bool             MM_GametypeHasFlag(int flag);
gametype_t       MM_SanitizeGametype(gametype_t gt);
void             MM_SanitizeCurrentGametype();
std::string      MM_GetEnabledGametypesList();

void MM_CheckRuleset();
void MM_ChangeGametype(gametype_t gt, bool force_cfg = false);
void MM_GTChanges();
void MM_SyncGametypeTracking();
void MM_GTSetLongName();
