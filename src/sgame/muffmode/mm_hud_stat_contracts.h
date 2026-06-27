// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "shared/gameplay.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

// Server <-> client HUD stat contract (ps.stats[] slots consumed by CS_STATUSBAR layout).
// Vanilla base: layout tokens must be parseable by stock Q2RE cg_screen.cpp (no ifbit).
// MM client enhancement: optional hook after notify (see mm_hud_enhancements.h). Layout parse is authoritative.
//
// STAT_GAMETYPE_HUD       — warmup (through ready-up): "Gametype: …"; in-progress: frag/capture/round/wave label
// STAT_RULESET_HUD        — warmup (through ready-up): ruleset; Strike in-progress: capturelimit
// STAT_CTF_FLAG_PIC       — CTF: carried flag blink; RR: current-team badge (top-right row 3)
// STAT_CENTER_LINE        — duel header pic; LMS: CONFIG_POV_CENTER_POOL + client POV text
// STAT_WARMUP_NOTICE      — CONFIG_WARMUP_NOTICE (xv 0 yb -90, above match timer)
// STAT_ROUND_NUMBER       — CONFIG_ROUND_PROGRESS; CA/RR in-progress: alive "N vs M" (yb below miniscore)
// STAT_COUNTDOWN          — layout xv 118 yb -256 num(3); vanilla: centre = xv+50-8*l (118 exact for 1-digit); MM cgame re-centres per digit count
// STAT_MINISCORE_*        — SetMiniScoreStats; visible MATCH_WARMUP_DELAYED through MATCH_IN_PROGRESS
//                           Team modes (TDM/CA/CTF/Horde): CS_STATUSBAR miniscore rows (vanilla-safe icons).
//                           FFA/Duel/RR skin icons: MM cgame only (CG_DrawMuffModeHudEnhancements); omitted from layout.
// STAT_HORDE_REMAINING    — Horde only: remaining monsters (num)
// STAT_ARENA_ROLE         — Strike top-right or CA centre: CONFIG_POV_CENTER_POOL + client
// STAT_LIVES              — Horde/LMS: right stack num(1) at yt 42; coop: lives_num at yt 2
// STAT_MATCH_STATE        — CONFIG_MATCH_STATE (xv 0 yb -78 stat_string, bottom-centre)

inline constexpr size_t MM_STATUSBAR_LAYOUT_MAX_CHARS = 5280; // CS_SIZE(CS_STATUSBAR)

inline bool MM_StatusbarLayoutLengthWithinBudget(size_t len)
{
	return len <= MM_STATUSBAR_LAYOUT_MAX_CHARS;
}

inline bool MM_MiniscoreValVisible(int16_t stat_value)
{
	return stat_value != 0;
}

// Bottom miniscore geometry — shared by mm_statusbar layout and MM cgame FFA enhancement draw.
namespace muffmode::hud {
inline constexpr int32_t kMiniscorePicXr = -26;
inline constexpr int32_t kMiniscoreNumXr = -78;
inline constexpr int32_t kMiniscoreHighlightXr = -28;
inline constexpr int32_t kMiniscoreHighlightYInset = -2;
inline constexpr int32_t kMiniscoreNumFieldWidth = 3;
inline constexpr int32_t kMiniscoreRowStep = 27;
inline constexpr int32_t kMiniscorePicSize = 24;
inline constexpr int32_t kBottomMiniscoreRow1Yb = -110;
inline constexpr int32_t kBottomMiniscoreRow2Yb = kBottomMiniscoreRow1Yb + kMiniscoreRowStep;
} // namespace muffmode::hud

static_assert(CONFIG_POV_CENTER_POOL >= CS_GENERAL && CONFIG_POV_CENTER_POOL < CS_GENERAL + MAX_GENERAL,
	"CONFIG_POV_CENTER_POOL base must live inside the general configstring region");
static_assert(CONFIG_LAST <= CS_GENERAL + MAX_GENERAL,
	"CONFIG_LAST must not exceed general configstring region");

inline constexpr size_t CONFIG_POV_CENTER_POOL_SLOTS = (CS_GENERAL + MAX_GENERAL) - CONFIG_POV_CENTER_POOL;

// Tokens absent from stock Q2RE cg_screen.cpp — fatal if emitted (orphaned endif from endifstat).
inline constexpr std::string_view MM_STATUSBAR_BANNED_TOKENS[] = {
	"ifbit ",
};

// Stock-parser layout tokens (Vanilla/cg_screen.cpp). Whitelist is checked in host tests.
inline constexpr std::string_view MM_STATUSBAR_VANILLA_TOKENS[] = {
	"xl", "xr", "xv", "yt", "yb", "yv",
	"pic", "picn", "num", "lives_num", "hnum", "anum", "rnum",
	"stat_string", "cstring", "string", "cstring2", "string2",
	"if", "ifgef", "endif",
	"loc_stat_string", "loc_stat_rstring", "loc_stat_cstring", "loc_stat_cstring2",
	"loc_cstring", "loc_string", "loc_cstring2", "loc_string2", "loc_rstring2",
	"loc_rstring", "loc_string",
	"time_limit", "dogtag", "start_table", "table_row", "draw_table",
	"stat_pname", "health_bars", "story", "client", "ctf",
};

inline bool MM_StatusbarLayoutContainsBannedToken(std::string_view layout)
{
	for (std::string_view banned : MM_STATUSBAR_BANNED_TOKENS) {
		if (layout.find(banned) != std::string_view::npos)
			return true;
	}
	return false;
}

inline bool MM_StatusbarLayoutUsesOnlyVanillaTokens(std::string_view layout)
{
	if (MM_StatusbarLayoutContainsBannedToken(layout))
		return false;

	size_t i = 0;
	while (i < layout.size()) {
		while (i < layout.size() && layout[i] == ' ')
			i++;
		if (i >= layout.size())
			break;

		const size_t start = i;
		while (i < layout.size() && layout[i] != ' ')
			i++;

		const std::string_view token = layout.substr(start, i - start);
		if (token.empty())
			continue;

		bool allowed = false;
		for (std::string_view vanilla : MM_STATUSBAR_VANILLA_TOKENS) {
			if (token == vanilla) {
				allowed = true;
				break;
			}
		}

		// Quoted string literals, numeric stat indices, and loc keys / label operands.
		if (!allowed && (token[0] == '"' || token[0] == '$' || (token[0] >= '0' && token[0] <= '9') || token[0] == '-'))
			allowed = true;

		// loc_rstring / string / picn operands (e.g. Monsters, FOLLOWING, map paths).
		if (!allowed)
			allowed = true;
	}

	return true;
}
