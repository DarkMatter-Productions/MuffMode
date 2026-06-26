// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "g_local.h"
#include "muffmode/mm_red_rover_rules.h"

#include <string>

// Appends the standard scoreboard footer (gametype header, score limit, match time / rank line,
// menu hint). Byte layout must stay identical to the inline copies in hud.cpp.
inline void MM_AppendScoreboardFooter(gentity_t *ent, std::string &string, bool use_formatted_score)
{
	fmt::format_to(std::back_inserter(string), FMT_STRING("xv 0 yv -40 cstring2 \"{} on {}\" "), level.gametype_name, level.level_name);
	fmt::format_to(std::back_inserter(string), FMT_STRING("xv 0 yv -30 cstring2 \"Score Limit: {}\" "), GT_ScoreLimit());

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
			if (use_formatted_score) {
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
