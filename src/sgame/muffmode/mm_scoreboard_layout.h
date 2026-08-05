// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "g_local.h"
#include "muffmode/mm_red_rover_rules.h"

#include <string>

// Room-scoped footer for Arena, in the same visual grammar as MM_AppendScoreboardFooter below.
// Arena cannot reuse that function: GT_ScoreLimit() is 0 under GT_ARENA and level.match_state
// never leaves warmup there, so the limit line, the place line and the menu hint would all be
// wrong or missing. Every value is therefore supplied by the caller.
struct mm_scoreboard_footer_ctx_t {
	std::string_view title;			// e.g. "Rocket Arena on q2dm1"
	std::string_view limit_label;	// e.g. "Best of"
	std::string_view limit_value;	// e.g. "3"
	std::string_view status_line;	// Live-match status; skipped when empty.
	std::string_view victor_msg;	// Intermission only; skipped when empty.
	bool intermission = false;
	int64_t intermission_frame = 0;
	int total_match_ms = 0;			// Intermission only; 0 omits the line.
};

// Composes the footer as one indivisible block. The caller builds this FIRST and reserves its
// measured length before appending any body chunk, so the footer can never be squeezed out by a
// busy board — and the intermission prompt, an "ifgef ... endif" pair that would Com_Error the
// client if split, is always emitted whole or not at all.
inline std::string MM_BuildArenaScoreboardFooter(const mm_scoreboard_footer_ctx_t &ctx)
{
	std::string block;
	fmt::format_to(std::back_inserter(block), FMT_STRING("xv 0 yv -40 cstring2 \"{}\" "), ctx.title);
	if (!ctx.limit_label.empty())
		fmt::format_to(std::back_inserter(block), FMT_STRING("xv 0 yv -30 cstring2 \"{}: {}\" "), ctx.limit_label, ctx.limit_value);

	if (ctx.intermission) {
		if (ctx.total_match_ms > 0)
			fmt::format_to(std::back_inserter(block), FMT_STRING("xv 0 yv -50 cstring2 \"Total Match Time: {}\" "), G_TimeStringMs(ctx.total_match_ms, false));
		if (!ctx.victor_msg.empty())
			fmt::format_to(std::back_inserter(block), FMT_STRING("xv 0 yv -10 cstring2 \"{}\" "), ctx.victor_msg);

		fmt::format_to(std::back_inserter(block), FMT_STRING("ifgef {} yb -48 xv 0 loc_cstring2 0 \"$m_eou_press_button\" endif "), ctx.intermission_frame);
	} else {
		if (!ctx.status_line.empty())
			fmt::format_to(std::back_inserter(block), FMT_STRING("xv 0 yv -10 cstring2 \"{}\" "), ctx.status_line);

		fmt::format_to(std::back_inserter(block), FMT_STRING("xv 0 yb -48 cstring2 \"{}\" "), "Use inventory bind to toggle menu.");
	}

	return block;
}

inline const char *MM_ScoreboardLimitLabel()
{
	if (GT(GT_STRIKE) || GT(GT_CTF))
		return "Capture Limit";
	if (GTF(GTF_ROUNDS))
		return "Round Limit";
	return "Score Limit";
}

// Appends the standard scoreboard footer (gametype header, score limit, match time / rank line,
// menu hint). Byte layout must stay identical to the inline copies in hud.cpp.
inline void MM_AppendScoreboardFooter(gentity_t *ent, std::string &string, bool use_formatted_score)
{
	fmt::format_to(std::back_inserter(string), FMT_STRING("xv 0 yv -40 cstring2 \"{} on {}\" "), level.gametype_name, level.level_name);
	fmt::format_to(std::back_inserter(string), FMT_STRING("xv 0 yv -30 cstring2 \"{}: {}\" "), MM_ScoreboardLimitLabel(), GT_ScoreLimit());

	if (level.intermission_time) {
		if (level.match_start_time) {
			int t = (level.intermission_time - level.match_start_time - 1_sec).milliseconds();
			fmt::format_to(std::back_inserter(string), FMT_STRING("xv 0 yv -50 cstring2 \"Total Match Time: {}\" "), G_TimeStringMs(t, false));
		}
		if (level.intermission_victor_msg[0])
			fmt::format_to(std::back_inserter(string), FMT_STRING("xv 0 yv -10 cstring2 \"{}\" "), level.intermission_victor_msg);

		fmt::format_to(std::back_inserter(string), FMT_STRING("ifgef {} yb -48 xv 0 loc_cstring2 0 \"$m_eou_press_button\" endif "), (level.intermission_server_frame + (5_sec).frames()));
	} else if (level.match_state == MATCH_IN_PROGRESS) {
		if (ent->client && ClientIsPlaying(ent->client) && ent->client->resp.score && level.num_playing_clients > 1) {
			if (GT(GT_LMS)) {
				if (use_formatted_score) {
					const char *score = G_Fmt("{}", ent->client->resp.score).data();
					fmt::format_to(std::back_inserter(string), FMT_STRING("xv 0 yv -10 cstring2 \"{} place with {} round wins\" "),
						G_PlaceString(ent->client->resp.rank + 1), score);
				} else {
					fmt::format_to(std::back_inserter(string), FMT_STRING("xv 0 yv -10 cstring2 \"{} place with {} round wins\" "),
						G_PlaceString(ent->client->resp.rank + 1), ent->client->resp.score);
				}
			} else if (use_formatted_score) {
				const char *score = G_Fmt("{}", ent->client->resp.score).data();
				fmt::format_to(std::back_inserter(string), FMT_STRING("xv 0 yv -10 cstring2 \"{} place with a score of {}\" "),
					G_PlaceString(ent->client->resp.rank + 1), score);
			} else {
				fmt::format_to(std::back_inserter(string), FMT_STRING("xv 0 yv -10 cstring2 \"{} place with a score of {}\" "),
					G_PlaceString(ent->client->resp.rank + 1), ent->client->resp.score);
			}
		}

		fmt::format_to(std::back_inserter(string), FMT_STRING("xv 0 yb -48 cstring2 \"{}\" "), "Use inventory bind to toggle menu.");
	}
}
