// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "muffmode/mm_parse.h"
#include "shared/gameplay.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

// Server <-> client HUD stat contract (ps.stats[] slots consumed by CS_STATUSBAR layout).
// Vanilla base: layout tokens must be parseable by stock Q2RE cg_screen.cpp (no ifbit).
// MM client enhancement: optional hook after notify (see mm_hud_enhancements.h). Layout parse is authoritative.
//
// STAT_GAMETYPE_HUD       — "Gametype: …" for 5s after team join / gametype / ruleset change; else hidden
// STAT_RULESET_HUD        — ruleset, same 5s notice window as STAT_GAMETYPE_HUD
// STAT_ROUND_NUMBER       — unused in the DM layout (the match limit rides on STAT_SCORELIMIT)
// STAT_CTF_FLAG_PIC       — CTF/Strike: carried flag blink; RR: current-team badge (top-right row 3)
// STAT_CENTER_LINE        — duel header pic; LMS/Freeze Tag: primary POV configstring lane
// STAT_SCORELIMIT         — CONFIG_STORY_SCORELIMIT index: match limit as small white right-aligned
//                           text under the miniscore rows; 0 = hidden, and the layout MUST gate on
//                           this stat (value 0 would otherwise index configstring 0, the level name)
// STAT_COUNTDOWN          — layout xv 118 yb -256 num(3); vanilla: centre = xv+50-8*l (118 exact for 1-digit); MM cgame re-centres per digit count
// STAT_MINISCORE_*        — SetMiniScoreStats; visible only during MATCH_IN_PROGRESS (hidden through
//                           every warmup stage, the countdown, and intermission)
//                           CS_STATUSBAR miniscore rows (team icons or FFA/Duel/RR/Horde player skins).
// STAT_HORDE_REMAINING    — Horde only: remaining monsters (num)
// STAT_HORDE_WAVE         — Horde only: current wave number (num); repurposes the formerly-unused
//                           warmup-notice slot (warmup guidance uses centerprint, not the statusbar)
// STAT_ARENA_ROLE         — Strike top-right or CA/Freeze centre: primary POV configstring lane
// STAT_LIVES              — Horde/LMS: right stack num(1) at yt 42; coop: lives_num at yt 2
// STAT_MATCH_STATE        — CONFIG_MATCH_STATE (xv 0 yb -78 stat_string, bottom-centre); warmup: WARMUP
// STAT_SPECTATOR          — CONFIG_SPECTATOR_MODE_* when spectating

inline constexpr size_t MM_STATUSBAR_LAYOUT_MAX_CHARS = 5280; // CS_SIZE(CS_STATUSBAR)

inline bool MM_StatusbarLayoutLengthWithinBudget(size_t len)
{
	return len <= MM_STATUSBAR_LAYOUT_MAX_CHARS;
}

inline bool MM_MiniscoreValVisible(int16_t stat_value)
{
	return stat_value != 0;
}

// Bottom miniscore geometry — shared by mm_statusbar layout.
namespace muffmode::hud {
inline constexpr int32_t kMiniscorePicXr = -26;
inline constexpr int32_t kMiniscoreNumXr = -78;
inline constexpr int32_t kMiniscoreHighlightXr = -28;
// Match-limit line under the rows: right edge of the small white text, on the right HUD stack
// column rather than the score-digit edge (num(3) at kMiniscoreNumXr would end at -28).
inline constexpr int32_t kMiniscoreLimitTextXr = -2;
inline constexpr int32_t kMiniscoreHighlightYInset = -2;
inline constexpr int32_t kMiniscoreNumFieldWidth = 3;
inline constexpr int32_t kMiniscoreRowStep = 27;
inline constexpr int32_t kMiniscorePicSize = 24;
inline constexpr int32_t kMiniscoreMetaBelowScoreStep = 26;
inline constexpr int32_t kBottomMiniscoreRow1Yb = -110;
inline constexpr int32_t kBottomMiniscoreRow2Yb = kBottomMiniscoreRow1Yb + kMiniscoreRowStep;
inline constexpr int32_t kBottomMiniscoreMetaRow1Yb = kBottomMiniscoreRow2Yb + kMiniscoreMetaBelowScoreStep;
} // namespace muffmode::hud

static_assert(CONFIG_POV_CENTER_POOL >= CS_GENERAL && CONFIG_POV_CENTER_POOL < CS_GENERAL + MAX_GENERAL,
	"CONFIG_POV_CENTER_POOL base must live inside the general configstring region");
static_assert(CONFIG_COUNTDOWN_HEADER < CONFIG_POV_CENTER_POOL,
	"CONFIG_COUNTDOWN_HEADER must not overlap the per-client POV configstring pool");
static_assert(CONFIG_LAST <= CS_GENERAL + MAX_GENERAL,
	"CONFIG_LAST must not exceed general configstring region");

inline constexpr size_t CONFIG_POV_CENTER_POOL_SLOTS = (CS_GENERAL + MAX_GENERAL) - CONFIG_POV_CENTER_POOL;
inline constexpr int CONFIG_POV_CENTER_SECONDARY_POOL =
	CONFIG_FOLLOW_PLAYER_NAME + static_cast<int>(MAX_LOBBY_PLAYERS);

static_assert(CONFIG_POV_CENTER_POOL_SLOTS >= MAX_LOBBY_PLAYERS,
	"primary POV configstring lane must fit every supported lobby client");
static_assert(CONFIG_POV_CENTER_SECONDARY_POOL >= CONFIG_FOLLOW_PLAYER_NAME);
static_assert(CONFIG_POV_CENTER_SECONDARY_POOL + static_cast<int>(MAX_LOBBY_PLAYERS) <=
	CONFIG_FOLLOW_PLAYER_NAME_END,
	"secondary POV lane must fit in unused follow-name configstrings");
static_assert(CONFIG_POV_CENTER_SECONDARY_POOL + static_cast<int>(MAX_LOBBY_PLAYERS) <=
	CONFIG_COOP_RESPAWN_STRING,
	"secondary POV lane must not overlap later shared configstrings");

enum class mm_pov_configstring_lane_t : size_t {
	Primary = 0,   // Center/role text.
	Secondary = 1 // Simultaneous per-viewer status text.
};

inline size_t MM_PovConfigStringLaneWidth(size_t max_clients)
{
	const size_t requested = max_clients > 0 ? max_clients : 1;
	return std::min(requested, MAX_LOBBY_PLAYERS);
}

inline int MM_PovConfigStringForClient(size_t client_index, size_t max_clients, mm_pov_configstring_lane_t lane)
{
	const size_t lane_width = MM_PovConfigStringLaneWidth(max_clients);
	if (client_index >= lane_width)
		return 0;

	int lane_base = 0;
	switch (lane) {
	case mm_pov_configstring_lane_t::Primary:
		lane_base = CONFIG_POV_CENTER_POOL;
		break;
	case mm_pov_configstring_lane_t::Secondary:
		lane_base = CONFIG_POV_CENTER_SECONDARY_POOL;
		break;
	default:
		return 0;
	}

	return lane_base + static_cast<int>(client_index);
}

// Tokens absent from stock Q2RE cg_screen.cpp — fatal if emitted (orphaned endif from endifstat).
inline constexpr std::string_view MM_STATUSBAR_BANNED_TOKENS[] = {
	"ifbit ",
};

// Stock-parser layout tokens (Vanilla/cg_screen.cpp). Whitelist is checked in host tests.
inline constexpr std::string_view MM_STATUSBAR_VANILLA_TOKENS[] = {
	"xl", "xr", "xv", "yt", "yb", "yv",
	"pic", "picn", "num", "lives_num", "hnum", "anum", "rnum",
	"stat_string", "stat_string2", "cstring", "string", "cstring2", "string2",
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

inline bool MM_StatusbarReadToken(std::string_view layout, size_t &pos, std::string_view &token)
{
	while (pos < layout.size() && MM_IsAsciiWhitespace(layout[pos]))
		pos++;
	if (pos >= layout.size())
		return false;

	if (layout[pos] == '"') {
		const size_t start = pos++;
		while (pos < layout.size() && layout[pos] != '"')
			pos++;
		if (pos >= layout.size()) {
			pos = start;
			return false;
		}
		pos++;
		token = layout.substr(start, pos - start);
		return true;
	}

	const size_t start = pos;
	while (pos < layout.size() && !MM_IsAsciiWhitespace(layout[pos]))
		pos++;
	token = layout.substr(start, pos - start);
	return true;
}

inline bool MM_StatusbarTokenIsInteger(std::string_view token)
{
	if (token.empty())
		return false;

	size_t i = token[0] == '-' ? 1 : 0;
	if (i == token.size())
		return false;

	for (; i < token.size(); i++) {
		if (token[i] < '0' || token[i] > '9')
			return false;
	}

	return true;
}

inline bool MM_StatusbarTokenIsTextOperand(std::string_view token)
{
	return !token.empty() &&
		(token.front() == '"' ||
		 token.front() == '$' ||
		 (token.find_first_of(" \t\r\n\v\f") == std::string_view::npos &&
		  !MM_StatusbarTokenIsInteger(token)));
}

inline bool MM_StatusbarIsCommand(std::string_view token)
{
	for (std::string_view vanilla : MM_STATUSBAR_VANILLA_TOKENS) {
		if (token == vanilla)
			return true;
	}
	return false;
}

inline int MM_StatusbarNumericOperands(std::string_view command)
{
	if (command == "xl" || command == "xr" || command == "xv" ||
		command == "yt" || command == "yb" || command == "yv" ||
		command == "if" || command == "pic" || command == "stat_string" ||
		command == "stat_string2" || command == "lives_num" ||
		command == "stat_pname" || command == "loc_stat_string" ||
		command == "loc_stat_rstring" || command == "loc_stat_cstring" ||
		command == "loc_stat_cstring2") {
		return 1;
	}

	if (command == "ifgef" || command == "num")
		return 2;

	return 0;
}

inline int MM_StatusbarTextOperands(std::string_view command)
{
	if (command == "picn" || command == "cstring" || command == "string" ||
		command == "cstring2" || command == "string2") {
		return 1;
	}

	if (command == "loc_cstring" || command == "loc_string" ||
		command == "loc_cstring2" || command == "loc_string2" ||
		command == "loc_rstring2" || command == "loc_rstring") {
		return 1;
	}

	return 0;
}

inline bool MM_StatusbarTokenOperandsValid(std::string_view layout, size_t &pos, std::string_view command)
{
	std::string_view token;
	const int numeric_operands = MM_StatusbarNumericOperands(command);
	for (int i = 0; i < numeric_operands; i++) {
		if (!MM_StatusbarReadToken(layout, pos, token) || !MM_StatusbarTokenIsInteger(token))
			return false;
	}

	if (command == "loc_cstring" || command == "loc_string" ||
		command == "loc_cstring2" || command == "loc_string2" ||
		command == "loc_rstring2" || command == "loc_rstring") {
		if (!MM_StatusbarReadToken(layout, pos, token) || !MM_StatusbarTokenIsInteger(token))
			return false;
	}

	const int text_operands = MM_StatusbarTextOperands(command);
	for (int i = 0; i < text_operands; i++) {
		if (!MM_StatusbarReadToken(layout, pos, token) || !MM_StatusbarTokenIsTextOperand(token))
			return false;
	}

	return true;
}

inline bool MM_StatusbarLayoutUsesOnlyVanillaTokens(std::string_view layout)
{
	if (MM_StatusbarLayoutContainsBannedToken(layout))
		return false;

	size_t i = 0;
	std::string_view token;
	while (MM_StatusbarReadToken(layout, i, token)) {
		if (!MM_StatusbarIsCommand(token))
			return false;
		if (!MM_StatusbarTokenOperandsValid(layout, i, token))
			return false;
	}

	while (i < layout.size() && MM_IsAsciiWhitespace(layout[i]))
		i++;

	return true;
}
