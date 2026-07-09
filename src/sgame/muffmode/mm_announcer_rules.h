// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct announce_action_t {
	bool play_vo;
	bool play_backup;
	bool play_sting;
};

// Pure preference routing for announcer playback (no entity or asset checks).
inline announce_action_t MM_AnnounceDecision(
	bool use_expanded, bool has_stem, bool use_backup, bool has_backup, bool backup_alongside_vo) noexcept
{
	announce_action_t action {};

	if (!use_expanded || (!has_stem && use_backup)) {
		action.play_backup = has_backup;
		return action;
	}

	if (use_expanded && has_stem) {
		action.play_vo = true;
		if (backup_alongside_vo && has_backup)
			action.play_sting = true;
	}

	return action;
}
