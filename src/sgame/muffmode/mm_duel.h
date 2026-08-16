// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

struct gentity_t;

constexpr size_t MM_DuelLogicalParticipantCount(
	size_t connected, size_t reserved) noexcept
{
	return connected > std::numeric_limits<size_t>::max() - reserved
		? std::numeric_limits<size_t>::max()
		: connected + reserved;
}

struct mm_duel_score_entry_t {
	int client_num = -1;
	int32_t spawn_count = 0;
	int32_t score = 0;
	std::string player_name;
	std::string social_id;
	bool bot = false;
};

struct mm_duel_score_view_t {
	std::array<mm_duel_score_entry_t, 2> players{};
	size_t participant_count = 0;
	bool tied = false;
};

enum class mm_duel_final_outcome_t : uint8_t {
	unavailable,
	draw,
	win,
	loss
};

inline constexpr bool MM_DuelForfeitAllowed(
	size_t participant_count, bool tied, size_t participant_index) noexcept
{
	return participant_count == 2 && participant_index < 2 &&
		(tied || participant_index == 1);
}

inline constexpr bool MM_DuelDepartureForfeitAllowed(
	size_t participant_count, size_t participant_index) noexcept
{
	return participant_count == 2 && participant_index < 2;
}

inline constexpr bool MM_DuelJoinWouldQueue(
	size_t occupied_slots, bool roster_open, bool intermission) noexcept
{
	return occupied_slots >= 2 || !roster_open || intermission;
}

inline constexpr bool MM_DuelQueueOrderBefore(
	uint64_t lhs_order, int lhs_client_num,
	uint64_t rhs_order, int rhs_client_num) noexcept
{
	// Zero is the legacy/unassigned value. Treat it as older than every newly
	// allocated order so an in-flight queue can be upgraded without newcomers
	// jumping its existing members.
	if (lhs_order != rhs_order) {
		if (lhs_order == 0)
			return true;
		if (rhs_order == 0)
			return false;
		return lhs_order < rhs_order;
	}
	return lhs_client_num < rhs_client_num;
}

// [MuffMode] GT_DUEL queue and match-end handling.
bool MM_Duel_AddPlayer();
size_t MM_Duel_OccupiedSlots();
bool MM_Duel_JoinWouldQueue() noexcept;
uint64_t MM_Duel_AllocateQueueOrder();
mm_duel_score_view_t MM_Duel_CurrentScoreView();
void MM_Duel_ResetFinalResult();
bool MM_Duel_CommitForfeit(gentity_t *forfeiter);
bool MM_Duel_CommitDepartureForfeit(gentity_t *forfeiter);
mm_duel_final_outcome_t MM_Duel_FinalOutcome(
	int client_num, std::string_view social_id = {}) noexcept;
mm_duel_final_outcome_t MM_Duel_FinalOutcome(
	int client_num, int32_t spawn_count,
	std::string_view social_id = {}) noexcept;
mm_duel_final_outcome_t MM_Duel_FinalOutcomeForClient(
	const gentity_t *ent) noexcept;
void MM_Duel_RemoveLoser();
void MM_Duel_MatchEnd_AdjustScores();
void MM_Duel_QueueSpectatorBots();
void MM_Duel_ScoreboardMessage(gentity_t *ent, gentity_t *killer);
void MM_Duel_CmdForfeit(gentity_t *ent);
