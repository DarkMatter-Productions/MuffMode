// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstddef>
#include <optional>

inline std::optional<size_t> MM_FragWarningIndex(int fraglimit, int leader_score) {
	if (fraglimit <= 3)
		return std::nullopt;

	const int score_diff = fraglimit - leader_score;
	if (score_diff < 1 || score_diff > 3)
		return std::nullopt;

	return static_cast<size_t>(score_diff - 1);
}

inline bool MM_UseTeamScoreLimit(bool teams, bool red_rover) {
	return teams && !red_rover;
}

// Red Rover (Quake Live rounds): a round ends the moment one side is emptied of players
// - that team's opponents have "cleared the board". Requires at least two players so a
// lone survivor (everyone else disconnected) doesn't trip an instant round-end.
inline bool MM_RedRoverRoundShouldEnd(int red_count, int blue_count) {
	if (red_count + blue_count < 2)
		return false;
	return red_count == 0 || blue_count == 0;
}

inline bool MM_RedRoverBlocksManualTeamSwitch(int current_team, int desired_team, int spectator_team, bool red_rover, bool match_in_progress) {
	return red_rover &&
		match_in_progress &&
		current_team != spectator_team &&
		desired_team != spectator_team &&
		desired_team != current_team;
}

inline size_t MM_ScoreboardFooterReserve(bool intermission) {
	return intermission ? 320u : 96u;
}

inline bool MM_ScoreboardCanAppend(size_t current_length, size_t append_length, size_t max_length, bool intermission) {
	const size_t footer_reserve = MM_ScoreboardFooterReserve(intermission);
	if (max_length <= footer_reserve || current_length > max_length - footer_reserve)
		return false;

	return append_length <= (max_length - footer_reserve - current_length);
}
