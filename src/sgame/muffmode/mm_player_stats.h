// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <optional>
#include <string_view>

struct gentity_t;
struct gclient_t;
enum team_t;

constexpr float MM_PLAYER_STATS_DEFAULT_RATING = 1500.0f;
constexpr float MM_PLAYER_STATS_MIN_RATING = 0.0f;
constexpr float MM_PLAYER_STATS_MAX_RATING = 65535.0f;
constexpr float MM_PLAYER_STATS_ELO_K = 32.0f;
// A player's first N matches in a gametype move faster toward their true
// rating, the same idea chess federations use for new players. Composes with
// MM_PLAYER_STATS_TEAM_WEIGHT_MAX below; k_max_skill_rating_change in
// mm_client_profile.cpp must stay >= their product.
constexpr float MM_PLAYER_STATS_ELO_K_PROVISIONAL = 48.0f;
constexpr int32_t MM_PLAYER_STATS_PROVISIONAL_MATCH_THRESHOLD = 20;
constexpr size_t MM_PLAYER_STATS_PENDING_SETTLEMENT_LIMIT = 4096;

inline constexpr bool MM_PlayerStatsPendingQueueHasCapacity(
	size_t current_size) noexcept
{
	return current_size < MM_PLAYER_STATS_PENDING_SETTLEMENT_LIMIT;
}

inline constexpr bool MM_PlayerStatsPendingQueueCanAdmit(
	size_t current_size, size_t additional_results) noexcept
{
	return current_size <= MM_PLAYER_STATS_PENDING_SETTLEMENT_LIMIT &&
		additional_results <=
			MM_PLAYER_STATS_PENDING_SETTLEMENT_LIMIT - current_size;
}

inline constexpr bool MM_PlayerStatsMatchCanBeRanked(
	bool has_bot_participant,
	bool has_unready_human_participant,
	bool has_non_atomic_departure = false) noexcept
{
	return !has_bot_participant && !has_unready_human_participant &&
		!has_non_atomic_departure;
}

inline constexpr bool MM_PlayerStatsShouldApplyRecordedSettlement(
	bool recorded_is_current_match, bool has_exact_pending_write) noexcept
{
	return recorded_is_current_match || has_exact_pending_write;
}

enum class mm_player_stats_outcome_t : uint8_t {
	no_contest,
	win,
	loss,
	draw,
	abandon
};

struct mm_player_stats_rating_update_t {
	float rating = MM_PLAYER_STATS_DEFAULT_RATING;
	int32_t change = 0;
};

inline constexpr bool MM_PlayerStats_RuntimeOwnerMatches(
	bool initialized, std::string_view stored_social_id,
	int32_t stored_spawn_count, std::string_view current_social_id,
	int32_t current_spawn_count) noexcept
{
	if (!initialized)
		return false;
	if (!current_social_id.empty())
		return stored_social_id == current_social_id;
	return stored_social_id.empty() &&
		stored_spawn_count == current_spawn_count;
}

// Pure rating helpers are public so host tests and team-building policies can
// share the exact same arithmetic as match settlement.
inline float MM_PlayerStats_NormalizeRating(float rating) noexcept
{
	if (!std::isfinite(rating))
		return MM_PLAYER_STATS_DEFAULT_RATING;
	return std::clamp(rating,
		MM_PLAYER_STATS_MIN_RATING, MM_PLAYER_STATS_MAX_RATING);
}

inline int32_t MM_PlayerStats_DisplayRating(float rating) noexcept
{
	return static_cast<int32_t>(MM_PlayerStats_NormalizeRating(rating));
}

inline float MM_PlayerStats_EloExpected(
	float rating, float opponent_rating) noexcept
{
	rating = MM_PlayerStats_NormalizeRating(rating);
	opponent_rating = MM_PlayerStats_NormalizeRating(opponent_rating);
	const float exponent = std::clamp(
		(opponent_rating - rating) / 400.0f, -10.0f, 10.0f);
	return 1.0f / (1.0f + std::pow(10.0f, exponent));
}

inline mm_player_stats_rating_update_t MM_PlayerStats_ApplyRating(
	float rating, float actual_score, float expected_score,
	float k_factor = MM_PLAYER_STATS_ELO_K) noexcept
{
	rating = MM_PlayerStats_NormalizeRating(rating);
	actual_score = std::clamp(actual_score, 0.0f, 1.0f);
	expected_score = std::clamp(expected_score, 0.0f, 1.0f);
	// Retain the fractional Elo result and truncate only the separate display
	// delta. This intentionally improves on WORR's uint16_t assignment, which
	// discarded the fractional component and accumulated rating drift.
	const float requested_change =
		k_factor * (actual_score - expected_score);
	const float updated = MM_PlayerStats_NormalizeRating(
		rating + requested_change);
	const int32_t applied_change = static_cast<int32_t>(
		updated - rating);
	return { updated, applied_change };
}

// A player under the provisional threshold in this gametype moves faster
// toward their true rating; everyone else keeps today's flat K.
inline constexpr float MM_PlayerStats_KFactor(int32_t matches_played) noexcept
{
	return matches_played < MM_PLAYER_STATS_PROVISIONAL_MATCH_THRESHOLD
		? MM_PLAYER_STATS_ELO_K_PROVISIONAL
		: MM_PLAYER_STATS_ELO_K;
}

inline float MM_PlayerStats_FfaActualScore(
	const int32_t *scores, size_t count, size_t player_index) noexcept
{
	if (!scores || count < 2 || player_index >= count)
		return 0.5f;

	float result = 0.0f;
	for (size_t i = 0; i < count; ++i) {
		if (i == player_index)
			continue;
		if (scores[player_index] > scores[i])
			result += 1.0f;
		else if (scores[player_index] == scores[i])
			result += 0.5f;
	}

	return result / static_cast<float>(count - 1);
}

inline float MM_PlayerStats_FfaExpectedScore(
	const float *ratings, size_t count, size_t player_index) noexcept
{
	if (!ratings || count < 2 || player_index >= count)
		return 0.5f;

	float result = 0.0f;
	for (size_t i = 0; i < count; ++i) {
		if (i == player_index)
			continue;
		result += MM_PlayerStats_EloExpected(
			ratings[player_index], ratings[i]);
	}

	return result / static_cast<float>(count - 1);
}

// The serial is process-local and advances once per real match start. Client
// sessions remember the last serial persisted, preventing a disconnect ghost
// and the normal match-end path from saving the same participant twice.
uint32_t MM_PlayerStats_CurrentMatchSerial() noexcept;
bool MM_PlayerStats_IsMatchOpen() noexcept;

// Lifecycle integration. TeamTransition runs before deliberate team mutation;
// inactivity moves pause first and settle only when reservation capture fails.
void MM_PlayerStats_OnProfileLoaded(gentity_t *ent);
void MM_PlayerStats_OnMatchStart();
void MM_PlayerStats_OnMatchEnd(bool no_contest = false);
void MM_PlayerStats_OnMatchAbort();
// Retries exact, already-computed profile settlements with bounded backoff.
void MM_PlayerStats_RunFrame();
void MM_PlayerStats_Shutdown();
// A reconnect reservation pauses play time without settling an abandonment.
// Resume reopens the timer only when that participant is still unsettled.
void MM_PlayerStats_OnClientPause(gentity_t *ent);
void MM_PlayerStats_OnClientResume(gentity_t *ent);
void MM_PlayerStats_OnClientDisconnect(gentity_t *ent);
// Settles an expired reservation using its saved gameplay session without
// importing state from a reconnect that may currently occupy the client slot.
void MM_PlayerStats_OnReservedClientExpired(
	gentity_t *slot, gclient_t *saved_client);
void MM_PlayerStats_OnTeamTransition(
	gentity_t *ent, team_t old_team, team_t new_team);

float MM_PlayerStats_Rating(const gentity_t *ent) noexcept;
int32_t MM_PlayerStats_RatingChange(const gentity_t *ent) noexcept;
// Is this session one where the rating actually means something? Arena Rooms are explicitly
// unranked, bots never carry a profile, and a client whose persistent profile failed to load is
// unranked for the session. Shared so `sr` and the join notice can never disagree about it.
bool MM_PlayerStats_SessionIsRanked(const gentity_t *ent) noexcept;
std::optional<mm_player_stats_outcome_t> MM_PlayerStats_CurrentOutcome(
	const gentity_t *ent) noexcept;
const char *MM_PlayerStats_OutcomeName(mm_player_stats_outcome_t outcome) noexcept;

// Full `sr` command body; core/commands.cpp only needs a thin wrapper and
// command-table entry.
void MM_CmdSkillRating(gentity_t *ent);
