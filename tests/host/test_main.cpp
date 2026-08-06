// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "fake_game_import.h"
#include "hud_text.h"
#include "shared/q_std.h"
#include "muffmode/mm_admin.h"
#include "muffmode/mm_announcer_rules.h"
#include "muffmode/mm_arena_rules.h"
#include "muffmode/mm_awards_rules.h"
#include "muffmode/mm_client_profile.h"
#include "muffmode/mm_client_refs.h"
#include "muffmode/mm_centerprint.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_duel.h"
#include "muffmode/mm_ent_respawn_rules.h"
#include "muffmode/mm_freezetag_rules.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_gametype_cfg_rules.h"
#include "muffmode/mm_ghost.h"
#include "muffmode/mm_gibs_rules.h"
#include "muffmode/mm_hud_stat_contracts.h"
#include "muffmode/mm_horde_ai_rules.h"
#include "muffmode/mm_items_rules.h"
#include "muffmode/mm_lms_rules.h"
#include "muffmode/mm_loc_parse.h"
#include "muffmode/mm_map_pick_rules.h"
#include "muffmode/mm_map_pool.h"
#include "muffmode/mm_maps.h"
#include "muffmode/mm_match_stats.h"
#include "muffmode/mm_menu.h"
#include "muffmode/mm_message_budget.h"
#include "muffmode/mm_motd.h"
#include "muffmode/mm_ordnance_identity.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_pconfig_rules.h"
#include "muffmode/mm_reliable_text.h"
#include "muffmode/mm_player_stats.h"
#include "muffmode/mm_red_rover_rules.h"
#include "muffmode/mm_spawn_rules.h"
#include "muffmode/mm_time_format.h"
#include "muffmode/mm_util.h"
#include "muffmode/mm_vote_menu.h"
#include "shared/gameplay.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct test_case_t {
	const char *name;
	void (*fn)();
};

std::vector<test_case_t> &tests() {
	static std::vector<test_case_t> registered;
	return registered;
}

struct test_registrar_t {
	test_registrar_t(const char *name, void (*fn)()) {
		tests().push_back({ name, fn });
	}
};

struct test_failure_t : std::exception {
	std::string message;

	explicit test_failure_t(std::string message_) : message(std::move(message_)) {}

	const char *what() const noexcept override {
		return message.c_str();
	}
};

void check(bool condition, const char *expression, const char *file, int line) {
	if (condition)
		return;

	char buffer[1024];
	std::snprintf(buffer, sizeof(buffer), "%s:%d: check failed: %s", file, line, expression);
	throw test_failure_t(buffer);
}

#define MM_TEST(name) \
	void name(); \
	test_registrar_t name##_registrar(#name, name); \
	void name()

#define MM_CHECK(expr) check(!!(expr), #expr, __FILE__, __LINE__)
#define MM_CHECK_FALSE(expr) check(!(expr), "!(" #expr ")", __FILE__, __LINE__)
#define MM_CHECK_EQ(lhs, rhs) check(((lhs) == (rhs)), #lhs " == " #rhs, __FILE__, __LINE__)

MM_TEST(parse_helpers_trim_ascii_whitespace_without_touching_inner_text) {
	MM_CHECK_EQ(std::string(MM_TrimAsciiWhitespace(std::string_view(" \tARENA\r\n"))), std::string("ARENA"));
	MM_CHECK_EQ(std::string(MM_TrimAsciiWhitespace(std::string_view("A B"))), std::string("A B"));
	MM_CHECK(MM_TrimAsciiWhitespace(std::string_view(" \t\r\n")).empty());
}

MM_TEST(player_stats_elo_is_symmetric_and_applies_signed_changes) {
	MM_CHECK_EQ(MM_PlayerStats_EloExpected(1500.0f, 1500.0f), 0.5f);
	const float favorite = MM_PlayerStats_EloExpected(1700.0f, 1300.0f);
	const float underdog = MM_PlayerStats_EloExpected(1300.0f, 1700.0f);
	MM_CHECK(std::fabs((favorite + underdog) - 1.0f) < 0.00001f);

	const auto winner = MM_PlayerStats_ApplyRating(1500.0f, 1.0f, 0.5f);
	const auto loser = MM_PlayerStats_ApplyRating(1500.0f, 0.0f, 0.5f);
	MM_CHECK_EQ(winner.rating, 1516.0f);
	MM_CHECK_EQ(winner.change, 16);
	MM_CHECK_EQ(loser.rating, 1484.0f);
	MM_CHECK_EQ(loser.change, -16);
}

MM_TEST(player_stats_retains_fractional_elo_and_truncates_only_the_delta) {
	const float expected = MM_PlayerStats_EloExpected(1600.0f, 1500.0f);
	const auto winner = MM_PlayerStats_ApplyRating(1600.0f, 1.0f, expected);
	MM_CHECK(std::fabs(winner.rating - 1611.5186f) < 0.001f);
	MM_CHECK_EQ(winner.change, 11);

	const auto loser = MM_PlayerStats_ApplyRating(
		1500.0f, 0.0f, 1.0f - expected);
	MM_CHECK(std::fabs(loser.rating - 1488.4814f) < 0.001f);
	MM_CHECK_EQ(loser.change, -11);
}

MM_TEST(player_stats_rating_bounds_and_invalid_values_are_safe) {
	MM_CHECK_EQ(MM_PlayerStats_NormalizeRating(
		std::numeric_limits<float>::quiet_NaN()),
		MM_PLAYER_STATS_DEFAULT_RATING);
	const auto floor = MM_PlayerStats_ApplyRating(0.0f, 0.0f, 1.0f);
	const auto ceiling = MM_PlayerStats_ApplyRating(
		MM_PLAYER_STATS_MAX_RATING, 1.0f, 0.0f);
	MM_CHECK_EQ(floor.rating, MM_PLAYER_STATS_MIN_RATING);
	MM_CHECK_EQ(floor.change, 0);
	MM_CHECK_EQ(ceiling.rating, MM_PLAYER_STATS_MAX_RATING);
	MM_CHECK_EQ(ceiling.change, 0);
	MM_CHECK_EQ(MM_PlayerStats_DisplayRating(1511.99f), 1511);
	MM_CHECK_EQ(MM_PlayerStats_DisplayRating(-12.0f), 0);
	MM_CHECK_EQ(MM_PlayerStats_DisplayRating(
		std::numeric_limits<float>::infinity()),
		static_cast<int32_t>(MM_PLAYER_STATS_DEFAULT_RATING));
}

MM_TEST(player_stats_ffa_scores_ties_pairwise) {
	const int32_t scores[] = { 10, 10, 0 };
	MM_CHECK_EQ(MM_PlayerStats_FfaActualScore(scores, 3, 0), 0.75f);
	MM_CHECK_EQ(MM_PlayerStats_FfaActualScore(scores, 3, 1), 0.75f);
	MM_CHECK_EQ(MM_PlayerStats_FfaActualScore(scores, 3, 2), 0.0f);

	const float ratings[] = { 1500.0f, 1500.0f, 1500.0f };
	MM_CHECK_EQ(MM_PlayerStats_FfaExpectedScore(ratings, 3, 0), 0.5f);
	MM_CHECK_EQ(MM_PlayerStats_FfaExpectedScore(nullptr, 3, 0), 0.5f);
}

MM_TEST(player_stats_runtime_identity_uses_generation_only_without_social_id) {
	MM_CHECK_FALSE(MM_PlayerStats_RuntimeOwnerMatches(
		false, {}, 7, {}, 7));
	MM_CHECK(MM_PlayerStats_RuntimeOwnerMatches(
		true, "account", 7, "account", 8));
	MM_CHECK_FALSE(MM_PlayerStats_RuntimeOwnerMatches(
		true, "account-a", 7, "account-b", 7));
	MM_CHECK(MM_PlayerStats_RuntimeOwnerMatches(
		true, {}, 7, {}, 7));
	MM_CHECK_FALSE(MM_PlayerStats_RuntimeOwnerMatches(
		true, {}, 7, {}, 8));
}

MM_TEST(duel_reconnect_reservations_continue_to_occupy_player_slots) {
	MM_CHECK_EQ(MM_DuelLogicalParticipantCount(1, 1), size_t{2});
	MM_CHECK_EQ(MM_DuelLogicalParticipantCount(0, 2), size_t{2});
	MM_CHECK_EQ(MM_DuelLogicalParticipantCount(2, 1), size_t{3});
	MM_CHECK_EQ(MM_DuelLogicalParticipantCount(
		std::numeric_limits<size_t>::max(), 1),
		std::numeric_limits<size_t>::max());
}

MM_TEST(duel_forfeit_policy_uses_the_logical_two_player_result) {
	MM_CHECK(MM_DuelForfeitAllowed(2, false, 1));
	MM_CHECK_FALSE(MM_DuelForfeitAllowed(2, false, 0));
	MM_CHECK(MM_DuelForfeitAllowed(2, true, 0));
	MM_CHECK(MM_DuelForfeitAllowed(2, true, 1));
	MM_CHECK_FALSE(MM_DuelForfeitAllowed(1, true, 0));
	MM_CHECK_FALSE(MM_DuelForfeitAllowed(2, true, 2));
}

MM_TEST(duel_departure_forfeits_regardless_of_the_quitters_score_rank) {
	MM_CHECK(MM_DuelDepartureForfeitAllowed(2, 0));
	MM_CHECK(MM_DuelDepartureForfeitAllowed(2, 1));
	MM_CHECK_FALSE(MM_DuelDepartureForfeitAllowed(1, 0));
	MM_CHECK_FALSE(MM_DuelDepartureForfeitAllowed(2, 2));
}

MM_TEST(match_stats_result_events_stop_at_the_frozen_end_time) {
	MM_CHECK(MM_MatchStatsAcceptsResultEvents(true, false, 0));
	MM_CHECK_FALSE(MM_MatchStatsAcceptsResultEvents(true, false, 1));
	MM_CHECK_FALSE(MM_MatchStatsAcceptsResultEvents(false, false, 0));
	MM_CHECK_FALSE(MM_MatchStatsAcceptsResultEvents(true, true, 0));
}

MM_TEST(match_stats_exports_only_clients_who_entered_play) {
	MM_CHECK_FALSE(MM_MatchStatsHasRecordedParticipation(0, 0));
	MM_CHECK(MM_MatchStatsHasRecordedParticipation(1, 0));
	MM_CHECK(MM_MatchStatsHasRecordedParticipation(0, 1));
	MM_CHECK_FALSE(MM_MatchStatsCanRefreshArchivedParticipant(0, 0, 0));
	MM_CHECK(MM_MatchStatsCanRefreshArchivedParticipant(1, 0, 0));
	MM_CHECK(MM_MatchStatsCanRefreshArchivedParticipant(0, 0, 1));
}

MM_TEST(match_stats_frozen_results_require_the_original_live_connection) {
	MM_CHECK(MM_MatchStatsCanDeliverFrozenResult(true, true, 17, 17));
	MM_CHECK_FALSE(MM_MatchStatsCanDeliverFrozenResult(false, true, 17, 17));
	MM_CHECK_FALSE(MM_MatchStatsCanDeliverFrozenResult(true, false, 17, 17));
	MM_CHECK_FALSE(MM_MatchStatsCanDeliverFrozenResult(true, true, 17, 18));
}

MM_TEST(match_stats_uses_reserved_profile_readiness_when_authoritative) {
	MM_CHECK_FALSE(MM_MatchStatsEffectiveProfileReadiness(false, false, false));
	MM_CHECK(MM_MatchStatsEffectiveProfileReadiness(true, false, false));
	MM_CHECK(MM_MatchStatsEffectiveProfileReadiness(false, true, true));
	MM_CHECK_FALSE(MM_MatchStatsEffectiveProfileReadiness(true, true, false));
}

MM_TEST(match_stats_time_limit_conversion_is_overflow_safe) {
	MM_CHECK_EQ(MM_MatchStatsTimeLimitSeconds(
		-std::numeric_limits<float>::infinity()), 0);
	MM_CHECK_EQ(MM_MatchStatsTimeLimitSeconds(-1.0f), 0);
	MM_CHECK_EQ(MM_MatchStatsTimeLimitSeconds(0.0f), 0);
	MM_CHECK_EQ(MM_MatchStatsTimeLimitSeconds(0.5f), 30);
	MM_CHECK_EQ(MM_MatchStatsTimeLimitSeconds(1.5f), 90);
	MM_CHECK_EQ(MM_MatchStatsTimeLimitSeconds(1.0f / 60.0f), 1);
	MM_CHECK_EQ(MM_MatchStatsTimeLimitSeconds(
		std::numeric_limits<float>::quiet_NaN()), 0);
	MM_CHECK_EQ(MM_MatchStatsTimeLimitSeconds(
		std::numeric_limits<float>::infinity()), 0);
	MM_CHECK_EQ(MM_MatchStatsTimeLimitSeconds(
		std::numeric_limits<float>::max()), std::numeric_limits<int>::max());
}

MM_TEST(match_stats_catalog_artifact_types_have_a_hard_limit) {
	MM_CHECK(MM_MatchStatsCatalogTypeCountValid(0));
	MM_CHECK(MM_MatchStatsCatalogTypeCountValid(
		MM_MATCH_CATALOG_ARTIFACT_TYPE_LIMIT));
	MM_CHECK_FALSE(MM_MatchStatsCatalogTypeCountValid(
		MM_MATCH_CATALOG_ARTIFACT_TYPE_LIMIT + 1));
	MM_CHECK_FALSE(MM_MatchStatsCatalogTypeCountValid(
		std::numeric_limits<size_t>::max()));
}

MM_TEST(match_stats_ctf_aggregate_serialization_does_not_wrap) {
	MM_CHECK_EQ(MM_MatchStatsWideCounterSum(
		std::numeric_limits<uint32_t>::max(), 1),
		static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1);
	MM_CHECK_EQ(MM_MatchStatsWideCounterSum(
		std::numeric_limits<uint32_t>::max(),
		std::numeric_limits<uint32_t>::max()),
		static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) * 2);
	MM_CHECK_EQ(MM_MatchStatsSaturatingDurationSum(40, 2), uint64_t { 42 });
	MM_CHECK_EQ(MM_MatchStatsSaturatingDurationSum(
		std::numeric_limits<uint64_t>::max(), 1),
		std::numeric_limits<uint64_t>::max());
}

MM_TEST(match_stats_export_logs_enforce_their_hard_capacity) {
	MM_CHECK(MM_MatchStatsLogHasCapacity(0, MM_MATCH_EVENT_LIMIT));
	MM_CHECK(MM_MatchStatsLogHasCapacity(
		MM_MATCH_EVENT_LIMIT - 1, MM_MATCH_EVENT_LIMIT));
	MM_CHECK_FALSE(MM_MatchStatsLogHasCapacity(
		MM_MATCH_EVENT_LIMIT, MM_MATCH_EVENT_LIMIT));
	MM_CHECK_FALSE(MM_MatchStatsLogHasCapacity(
		MM_MATCH_DEATH_EVENT_LIMIT + 1, MM_MATCH_DEATH_EVENT_LIMIT));
	MM_CHECK_FALSE(MM_MatchStatsLogHasCapacity(
		MM_MATCH_DEPARTED_PLAYER_LIMIT, MM_MATCH_DEPARTED_PLAYER_LIMIT));
}

MM_TEST(player_stats_pending_settlement_queue_has_a_hard_capacity) {
	MM_CHECK(MM_PlayerStatsPendingQueueHasCapacity(0));
	MM_CHECK(MM_PlayerStatsPendingQueueHasCapacity(
		MM_PLAYER_STATS_PENDING_SETTLEMENT_LIMIT - 1));
	MM_CHECK_FALSE(MM_PlayerStatsPendingQueueHasCapacity(
		MM_PLAYER_STATS_PENDING_SETTLEMENT_LIMIT));
	MM_CHECK(MM_PlayerStatsPendingQueueCanAdmit(0,
		MM_PLAYER_STATS_PENDING_SETTLEMENT_LIMIT));
	MM_CHECK(MM_PlayerStatsPendingQueueCanAdmit(
		MM_PLAYER_STATS_PENDING_SETTLEMENT_LIMIT - 2, 2));
	MM_CHECK_FALSE(MM_PlayerStatsPendingQueueCanAdmit(
		MM_PLAYER_STATS_PENDING_SETTLEMENT_LIMIT - 1, 2));
	MM_CHECK_FALSE(MM_PlayerStatsPendingQueueCanAdmit(
		std::numeric_limits<size_t>::max(), 0));
}

MM_TEST(player_stats_ranking_requires_ready_humans_and_atomic_departures) {
	MM_CHECK(MM_PlayerStatsMatchCanBeRanked(false, false));
	MM_CHECK_FALSE(MM_PlayerStatsMatchCanBeRanked(true, false));
	MM_CHECK_FALSE(MM_PlayerStatsMatchCanBeRanked(false, true));
	MM_CHECK_FALSE(MM_PlayerStatsMatchCanBeRanked(true, true));
	MM_CHECK_FALSE(MM_PlayerStatsMatchCanBeRanked(false, false, true));
}

MM_TEST(player_stats_replays_only_current_or_pending_settlements) {
	MM_CHECK(MM_PlayerStatsShouldApplyRecordedSettlement(true, false));
	MM_CHECK(MM_PlayerStatsShouldApplyRecordedSettlement(false, true));
	MM_CHECK(MM_PlayerStatsShouldApplyRecordedSettlement(true, true));
	MM_CHECK_FALSE(MM_PlayerStatsShouldApplyRecordedSettlement(false, false));
}

MM_TEST(client_lifetime_reference_matching_rejects_unrelated_slots) {
	const int departing = 1;
	const int noise = 2;
	const int unrelated = 3;
	MM_CHECK(MM_ClientLifetimeReferenceMatches(
		&departing, &departing, &noise));
	MM_CHECK(MM_ClientLifetimeReferenceMatches(
		&noise, &departing, &noise));
	MM_CHECK_FALSE(MM_ClientLifetimeReferenceMatches(
		&unrelated, &departing, &noise));
}

MM_TEST(match_stats_hits_require_a_live_target_and_safe_once_policy) {
	MM_CHECK_FALSE(MM_MatchStatsShouldCountHit(
		false, false, false, true, false));
	MM_CHECK_FALSE(MM_MatchStatsShouldCountHit(
		true, true, false, true, false));
	MM_CHECK(MM_MatchStatsShouldCountHit(true, false, false, true, true));
	MM_CHECK(MM_MatchStatsShouldCountHit(true, false, true, false, false));
	MM_CHECK(MM_MatchStatsShouldCountHit(true, false, true, true, false));
	MM_CHECK_FALSE(MM_MatchStatsShouldCountHit(
		true, false, true, true, true));
}

MM_TEST(lobby_player_limit_clamps_to_muffmode_supported_capacity) {
	MM_CHECK_EQ(MAX_CLIENTS, size_t { 256 });
	MM_CHECK_EQ(MAX_LOBBY_PLAYERS, size_t { 128 });
	MM_CHECK_EQ(MM_ClampLobbyPlayerCount(std::numeric_limits<int64_t>::min()), 1u);
	MM_CHECK_EQ(MM_ClampLobbyPlayerCount(0), 1u);
	MM_CHECK_EQ(MM_ClampLobbyPlayerCount(1), 1u);
	MM_CHECK_EQ(MM_ClampLobbyPlayerCount(127), 127u);
	MM_CHECK_EQ(MM_ClampLobbyPlayerCount(128), 128u);
	MM_CHECK_EQ(MM_ClampLobbyPlayerCount(129), 128u);
	MM_CHECK_EQ(MM_ClampLobbyPlayerCount(std::numeric_limits<int64_t>::max()), 128u);
	MM_CHECK_EQ(MM_GHOST_MAX_CLIENT_CAPACITY, MAX_LOBBY_PLAYERS);
}

MM_TEST(multiplayer_menus_capture_usercmds_for_players_and_spectators) {
	MM_CHECK(MM_MenuCapturesUsercmd(true, mm_menu_client_state_t::Playing));
	MM_CHECK(MM_MenuCapturesUsercmd(true, mm_menu_client_state_t::Spectator));
	MM_CHECK_FALSE(MM_MenuCapturesUsercmd(false, mm_menu_client_state_t::Playing));
	MM_CHECK_FALSE(MM_MenuCapturesUsercmd(false, mm_menu_client_state_t::Spectator));
}

MM_TEST(shared_tokenizer_stops_at_unterminated_quoted_eof) {
	const char unterminated[] = { '"', 'x', '\0', 'Y', '\0' };
	const char *cursor = unterminated;
	MM_CHECK_EQ(std::string(COM_Parse(&cursor)), std::string("x"));
	MM_CHECK(cursor == nullptr);

	const char closed[] = "\"first\" second";
	cursor = closed;
	MM_CHECK_EQ(std::string(COM_Parse(&cursor)), std::string("first"));
	MM_CHECK(cursor != nullptr);
	MM_CHECK_EQ(std::string(COM_Parse(&cursor)), std::string("second"));
}

MM_TEST(shared_tokenizer_treats_a_zero_capacity_buffer_as_the_shared_buffer) {
	// COM_Parse's own defaults are (nullptr, 0), so a caller buffer with no
	// capacity has to take the shared-buffer path too. Taking the caller-buffer
	// path wrote the terminator one byte outside the caller's storage.
	char guarded[2] = { '\x7f', '\x7f' };
	const char source[] = "\"alpha\" beta";
	const char *cursor = source;

	const char *token = COM_Parse(&cursor, guarded, 0);
	MM_CHECK_EQ(std::string(token), std::string("alpha"));
	MM_CHECK(token != guarded);
	MM_CHECK_EQ(guarded[0], '\x7f');
	MM_CHECK_EQ(guarded[1], '\x7f');
}

MM_TEST(shared_tokenizer_component_parse_leaves_the_shared_token_intact) {
	// ED_ParseEntity hands field loaders the value token, which lives in the
	// shared buffer. Vector and colour loaders re-tokenise that value, so they
	// must supply their own buffers -- an unbuffered COM_Parse rewrites the very
	// string it is reading.
	const char source[] = "\"-11.5 0 24\"";
	const char *cursor = source;
	const char *value = COM_Parse(&cursor);
	const char *component_cursor = value;

	char component[MAX_TOKEN_CHARS];
	MM_CHECK_EQ(std::string(COM_Parse(&component_cursor, component, sizeof(component))),
		std::string("-11.5"));
	MM_CHECK_EQ(std::string(COM_Parse(&component_cursor, component, sizeof(component))),
		std::string("0"));
	MM_CHECK_EQ(std::string(COM_Parse(&component_cursor, component, sizeof(component))),
		std::string("24"));
	MM_CHECK_EQ(std::string(value), std::string("-11.5 0 24"));
}

MM_TEST(hud_line_copy_truncates_without_losing_the_next_line) {
	std::string input(32, 'x');
	input += "\nnext";
	char line[8];

	const cg_hud_line_t first = CG_CopyHUDLine(input.c_str(), line);
	MM_CHECK_EQ(first.length, size_t { 7 });
	MM_CHECK_EQ(std::string(line), std::string(7, 'x'));
	MM_CHECK(first.next != nullptr);
	MM_CHECK_EQ(*first.next, '\n');

	const cg_hud_line_t second = CG_CopyHUDLine(first.next + 1, line);
	MM_CHECK_EQ(second.length, size_t { 4 });
	MM_CHECK_EQ(std::string(line), std::string("next"));
	MM_CHECK_EQ(*second.next, '\0');
}

MM_TEST(client_packed_stat_ids_reject_signed_and_narrowing_overflow) {
	uint8_t packed_id = 0x5a;
	MM_CHECK(CG_TryPackedStatId(0, AMMO_MAX, packed_id));
	MM_CHECK_EQ(packed_id, uint8_t { 0 });
	MM_CHECK(CG_TryPackedStatId(AMMO_MAX - 1, AMMO_MAX, packed_id));
	MM_CHECK_EQ(packed_id, static_cast<uint8_t>(AMMO_MAX - 1));

	for (const int32_t invalid : {
		std::numeric_limits<int32_t>::min(), -1, static_cast<int32_t>(AMMO_MAX),
		256, std::numeric_limits<int32_t>::max() }) {
		packed_id = 0x5a;
		MM_CHECK_FALSE(CG_TryPackedStatId(invalid, AMMO_MAX, packed_id));
		MM_CHECK_EQ(packed_id, uint8_t { 0x5a });
	}

	MM_CHECK(CG_TryPackedStatId(255, 256, packed_id));
	MM_CHECK_EQ(packed_id, uint8_t { 255 });
	packed_id = 0x5a;
	MM_CHECK_FALSE(CG_TryPackedStatId(256, 300, packed_id));
	MM_CHECK_EQ(packed_id, uint8_t { 0x5a });
}

MM_TEST(client_hud_coordinates_scale_with_checked_integer_arithmetic) {
	int32_t pixel = 0;
	MM_CHECK(CG_TryScaleHUDCoordinate(10, 2, 3, pixel));
	MM_CHECK_EQ(pixel, 23);
	// Pixel-space adjustments are deliberately not multiplied by the HUD scale.
	// Client blocks mix a fixed 32-pixel inset with scaled logical spacing.
	MM_CHECK(CG_TryScaleHUDCoordinate(96, 2, 32, pixel));
	MM_CHECK_EQ(pixel, 224);
	MM_CHECK(CG_TryScaleHUDCoordinate(0, 3, 100 - 8, pixel));
	MM_CHECK_EQ(pixel, 92);
	MM_CHECK(CG_TryScaleHUDCoordinate(std::numeric_limits<int32_t>::max(), 1, 0, pixel));
	MM_CHECK_EQ(pixel, std::numeric_limits<int32_t>::max());
	MM_CHECK(CG_TryScaleHUDCoordinate(std::numeric_limits<int32_t>::min(), 1, 0, pixel));
	MM_CHECK_EQ(pixel, std::numeric_limits<int32_t>::min());
	MM_CHECK(CG_TryScaleHUDCoordinate(std::numeric_limits<int32_t>::max(), -1, 0, pixel));
	MM_CHECK_EQ(pixel, -std::numeric_limits<int32_t>::max());
	MM_CHECK(CG_TryAddHUDPixels(std::numeric_limits<int32_t>::max() - 8, 8, pixel));
	MM_CHECK_EQ(pixel, std::numeric_limits<int32_t>::max());
	MM_CHECK(CG_TryAddHUDPixels(std::numeric_limits<int32_t>::min() + 8, -8, pixel));
	MM_CHECK_EQ(pixel, std::numeric_limits<int32_t>::min());

	const auto check_rejected = [&](int64_t logical, int32_t scale, int64_t adjustment) {
		pixel = 0x5a5a;
		MM_CHECK_FALSE(CG_TryScaleHUDCoordinate(logical, scale, adjustment, pixel));
		MM_CHECK_EQ(pixel, 0x5a5a);
	};
	check_rejected(static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1, 1, 0);
	check_rejected(static_cast<int64_t>(std::numeric_limits<int32_t>::min()) - 1, 1, 0);
	check_rejected(std::numeric_limits<int32_t>::max(), 2, 0);
	check_rejected(std::numeric_limits<int32_t>::min(), -1, 0);
	check_rejected(std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::max(),
		std::numeric_limits<int64_t>::max());
	check_rejected(std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max(),
		std::numeric_limits<int64_t>::min());
	pixel = 0x5a5a;
	MM_CHECK_FALSE(CG_TryAddHUDPixels(std::numeric_limits<int32_t>::max(), 1, pixel));
	MM_CHECK_EQ(pixel, 0x5a5a);
	MM_CHECK_FALSE(CG_TryAddHUDPixels(std::numeric_limits<int32_t>::min(), -1, pixel));
	MM_CHECK_EQ(pixel, 0x5a5a);
	MM_CHECK(CG_TryHUDTextExtent(1023, 8, 4, pixel));
	MM_CHECK_EQ(pixel, 32736);
	MM_CHECK_FALSE(CG_TryHUDTextExtent(
		std::numeric_limits<size_t>::max(), 8, 4, pixel));
	MM_CHECK_FALSE(CG_TryHUDTextExtent(1, 8, 0, pixel));
	MM_CHECK_FALSE(CG_TryHUDTextExtent(
		static_cast<size_t>(std::numeric_limits<int32_t>::max()), 8, 2, pixel));

	MM_CHECK_EQ(CG_HUDViewportCenter(100, 320), int64_t { 260 });
	MM_CHECK_EQ(CG_HUDViewportCenter(-100, 320), int64_t { 60 });
}

MM_TEST(client_hud_blink_and_layout_limits_have_stable_boundaries) {
	MM_CHECK_FALSE(CG_HUDBlinkVisible(0));
	MM_CHECK_FALSE(CG_HUDBlinkVisible(255));
	MM_CHECK(CG_HUDBlinkVisible(256));
	MM_CHECK(CG_HUDBlinkVisible(511));
	MM_CHECK_FALSE(CG_HUDBlinkVisible(512));
	MM_CHECK_FALSE(CG_HUDBlinkVisible(1000, 0));

	MM_CHECK(CG_IsValidHUDLocalizationArgCount(0, MAX_LOCALIZATION_ARGS));
	MM_CHECK(CG_IsValidHUDLocalizationArgCount(
		static_cast<int32_t>(MAX_LOCALIZATION_ARGS), MAX_LOCALIZATION_ARGS));
	MM_CHECK_FALSE(CG_IsValidHUDLocalizationArgCount(-1, MAX_LOCALIZATION_ARGS));
	MM_CHECK_FALSE(CG_IsValidHUDLocalizationArgCount(
		static_cast<int32_t>(MAX_LOCALIZATION_ARGS + 1), MAX_LOCALIZATION_ARGS));
	MM_CHECK(CG_IsValidHUDNumericWidth(0));
	MM_CHECK(CG_IsValidHUDNumericWidth(15));
	MM_CHECK_FALSE(CG_IsValidHUDNumericWidth(-1));
	MM_CHECK_FALSE(CG_IsValidHUDNumericWidth(16));
	MM_CHECK_FALSE(CG_IsValidHUDNumericWidth(std::numeric_limits<int32_t>::max()));

	MM_CHECK(CG_HUDLayoutConditionsBalanced(0, false));
	MM_CHECK_FALSE(CG_HUDLayoutConditionsBalanced(1, false));
	MM_CHECK_FALSE(CG_HUDLayoutConditionsBalanced(1, true));
	MM_CHECK_FALSE(CG_HUDLayoutConditionsBalanced(0, true));
}

MM_TEST(client_typewriter_prefixes_preserve_long_lines_and_utf8_boundaries) {
	const std::string utf8 = "A\xC2\xA3" "B";
	MM_CHECK_EQ(CG_FindEndOfUTF8Codepoint(utf8, 1), size_t { 1 });
	MM_CHECK_EQ(CG_FindEndOfUTF8Codepoint(utf8, 2), size_t { 3 });
	MM_CHECK_EQ(CG_FindEndOfUTF8Codepoint(utf8, 4), std::string_view::npos);
	MM_CHECK_EQ(CG_CenterPrintVisiblePrefix(utf8, 1, false), std::string_view("A"));
	MM_CHECK_EQ(CG_CenterPrintVisiblePrefix(utf8, 3, false),
		std::string_view(utf8.data(), 3));
	MM_CHECK_EQ(CG_CenterPrintVisiblePrefix(utf8, 0, true), std::string_view(utf8));
	MM_CHECK_EQ(CG_CompleteUTF8PrefixLength(utf8), utf8.size());
	MM_CHECK_EQ(CG_CompleteUTF8PrefixLength(
		std::string_view("A\xC2", 2)), size_t { 1 });
	MM_CHECK_EQ(CG_CompleteUTF8PrefixLength(
		std::string_view("A\xE2\x82", 3)), size_t { 1 });
	MM_CHECK_EQ(CG_CompleteUTF8PrefixLength(
		std::string_view("A\xF0\x9F\x92", 4)), size_t { 1 });

	const std::string long_line(400, 'x');
	MM_CHECK_EQ(CG_CenterPrintVisiblePrefix(long_line, 350, false).size(), size_t { 350 });
	MM_CHECK_EQ(CG_CenterPrintVisiblePrefix(long_line, 999, false).size(), size_t { 400 });
	MM_CHECK_EQ(CG_CenterPrintVisiblePrefix(long_line, 0, true).size(), size_t { 400 });
}

MM_TEST(admin_authentication_policy_is_exact_non_empty_and_throttled) {
	using muffmode::admin::MM_EvaluateAdminAttempt;
	using muffmode::admin::MM_AdminPasswordMatches;
	using muffmode::admin::mm_admin_attempt_result_t;

	MM_CHECK(MM_AdminPasswordMatches("AbC-123", "AbC-123"));
	MM_CHECK_FALSE(MM_AdminPasswordMatches("AbC-123", "abc-123"));
	MM_CHECK_FALSE(MM_AdminPasswordMatches("AbC-123", "AbC-12"));
	MM_CHECK_FALSE(MM_AdminPasswordMatches("AbC-123", "AbC-1234"));
	MM_CHECK_FALSE(MM_AdminPasswordMatches("", ""));
	MM_CHECK_FALSE(MM_AdminPasswordMatches(nullptr, "AbC-123"));
	MM_CHECK_FALSE(MM_AdminPasswordMatches("AbC-123", nullptr));

	MM_CHECK(MM_EvaluateAdminAttempt(false, "AbC-123", "AbC-123") ==
		mm_admin_attempt_result_t::Accepted);
	MM_CHECK(MM_EvaluateAdminAttempt(false, "AbC-123", "abc-123") ==
		mm_admin_attempt_result_t::InvalidCredentials);
	MM_CHECK(MM_EvaluateAdminAttempt(true, "AbC-123", "AbC-123") ==
		mm_admin_attempt_result_t::Throttled);
}

MM_TEST(packed_powerup_stats_use_ammo_padding_without_moving_key_stat) {
	MM_CHECK_EQ(NUM_AMMO_STATS, size_t { 7 });
	MM_CHECK_EQ(NUM_POWERUP_STATS, size_t { 3 });
	MM_CHECK_EQ(STAT_POWERUP_INFO_START, 41);
	MM_CHECK_EQ(STAT_POWERUP_INFO_END, 43);
	MM_CHECK_EQ(STAT_KEY_A, 44);

	std::array<uint16_t, NUM_AMMO_STATS + NUM_POWERUP_STATS + 1> words {};
	uint16_t *const ammo = words.data();
	uint16_t *const powerups = ammo + NUM_AMMO_STATS;
	words.back() = 0x5a5a;
	for (uint8_t id = 0; id < AMMO_MAX; id++) {
		const uint16_t value = static_cast<uint16_t>((id * 37) & AMMO_VALUE_INFINITE);
		G_SetAmmoStat(ammo, id, value);
		MM_CHECK_EQ(G_GetAmmoStat(ammo, id), value);
		MM_CHECK_EQ(words.back(), uint16_t { 0x5a5a });
	}
	G_SetAmmoStat(ammo, AMMO_PROX, AMMO_VALUE_INFINITE);

	for (uint8_t id = 0; id < POWERUP_MAX; id++) {
		const uint16_t value = id % 4;
		G_SetPowerupStat(ammo, powerups, id, value);
		MM_CHECK_EQ(G_GetPowerupStat(ammo, powerups, id), value);
		MM_CHECK_EQ(G_GetAmmoStat(ammo, AMMO_PROX), AMMO_VALUE_INFINITE);
		MM_CHECK_EQ(words.back(), uint16_t { 0x5a5a });
	}

	for (uint16_t value = 0; value < 4; value++) {
		G_SetPowerupStat(ammo, powerups, 24, value);
		MM_CHECK_EQ(G_GetPowerupStat(ammo, powerups, 24), value);
		MM_CHECK_EQ(G_GetAmmoStat(ammo, AMMO_PROX), AMMO_VALUE_INFINITE);
		MM_CHECK_EQ(words.back(), uint16_t { 0x5a5a });
	}

	const auto snapshot = words;
	G_SetAmmoStat(ammo, static_cast<uint8_t>(AMMO_MAX), AMMO_VALUE_INFINITE);
	G_SetAmmoStat(ammo, uint8_t { 255 }, AMMO_VALUE_INFINITE);
	MM_CHECK_EQ(G_GetAmmoStat(ammo, static_cast<uint8_t>(AMMO_MAX)), uint16_t { 0 });
	MM_CHECK_EQ(G_GetAmmoStat(ammo, uint8_t { 255 }), uint16_t { 0 });
	G_SetPowerupStat(ammo, powerups, static_cast<uint8_t>(POWERUP_MAX), 3);
	MM_CHECK_EQ(G_GetPowerupStat(
		ammo, powerups, static_cast<uint8_t>(POWERUP_MAX)), uint16_t { 0 });
	MM_CHECK(words == snapshot);
}

MM_TEST(capped_gametype_cfg_filters_every_maxclients_assignment) {
	using muffmode::gametype::MM_GtCfgLineViolatesSlotCap;
	using muffmode::gametype::MM_IsSlotCappedCapacity;

	MM_CHECK(MM_IsSlotCappedCapacity(4, 64, 4));
	MM_CHECK_FALSE(MM_IsSlotCappedCapacity(16, 4, 4));
	MM_CHECK(MM_IsSlotCappedCapacity(0, 4, 4));
	MM_CHECK_FALSE(MM_IsSlotCappedCapacity(0, 16, 4));
	MM_CHECK_FALSE(MM_IsSlotCappedCapacity(0, 0, 4));

	constexpr std::array<std::string_view, 17> allowed = {
		"maxclients",
		"maxclients 4",
		"set maxclients 4",
		"seta \"maxclients\" \"4\"",
		"setu maxclients 4",
		"set foo \"quoted; maxclients 64\"",
		"echo \"maxclients 64; still quoted\"",
		"echo \"alias grow 'set maxclients 64'\"",
		"echo safe",
		"wait; set maxclients 4",
		"fraglimit 20",
		"set fraglimit 20",
		"toggle g_friendly_fire",
		"inc timelimit 5",
		"reset fraglimit",
		"// maxclients 64",
		"set maxclients"
	};
	for (const std::string_view line : allowed)
		MM_CHECK_FALSE(MM_GtCfgLineViolatesSlotCap(line, 4));

	constexpr std::array<std::string_view, 37> rejected = {
		"maxclients 16",
		"set maxclients 16",
		"seta maxclients 16",
		"setu maxclients 16",
		"sets maxclients 16",
		"cvar_set maxclients 16",
		"cvar_forceset maxclients 16",
		"set maxclients nope",
		"set \"maxclients\" \"$dynamic\"",
		"echo safe; maxclients 16",
		"set foo bar; cvar_forceset \"maxclients\" \"64\"",
		"echo safe // comment; seta maxclients 16",
		"echo ok; kexmultiplayer 1",
		"MAXCLIENTS 5",
		"SeTa MAXCLIENTS 5",
		"maxclients 0",
		"maxclients -1",
		"toggle maxclients",
		"inc maxclients 1",
		"add maxclients 1",
		"reset maxclients",
		"cvar_toggle maxclients",
		"cvar_inc maxclients 1",
		"cvar_add maxclients 1",
		"cvar_reset maxclients",
		"exec gt-FFA.cfg",
		"EXECQ gt-FFA.cfg",
		"alias grow \"set maxclients 16\"",
		"vstr next_gametype",
		"if 1 \"set maxclients 16\"",
		"ifnot 0 \"set maxclients 16\"",
		"delay 1 \"set maxclients 16\"",
		"defer \"set maxclients 16\"",
		"cvar_restart",
		"echo safe; exec gt-FFA.cfg",
		"echo safe; vstr next_gametype",
		"\"exec"
	};
	for (const std::string_view line : rejected)
		MM_CHECK(MM_GtCfgLineViolatesSlotCap(line, 4));
}

MM_TEST(mymap_queue_entries_keep_their_own_modifiers) {
	using namespace muffmode::maps;

	mymap_modifier_modes_t first {};
	mymap_modifier_modes_t second {};
	MM_CHECK(ApplyMyMapModifierToken(first, "+pu"));
	MM_CHECK(ApplyMyMapModifierToken(first, "-wp"));
	MM_CHECK(ApplyMyMapModifierToken(second, "-am"));
	MM_CHECK(ApplyMyMapModifierToken(second, "+ht"));

	const mymap_modifier_modes_t first_snapshot = first;
	MM_CHECK(ApplyMyMapModifierToken(second, "-PU"));
	MM_CHECK(first == first_snapshot);
	MM_CHECK_EQ(second[MyMapModifierIndex(mymap_modifier_id_t::Powerups)], int8_t { -1 });

	const mymap_modifier_modes_t invalid_snapshot = second;
	for (const std::string_view token : { "pu", "+p", "+unknown", "*pu" })
		MM_CHECK_FALSE(ApplyMyMapModifierToken(second, token));
	MM_CHECK(second == invalid_snapshot);

	std::vector<mymap_queue_entry_t> queue {
		{ "q2dm1", first },
		{ "q2dm2", second }
	};
	mymap_queue_entry_t popped;
	MM_CHECK(PopMyMapQueueFront(queue, popped));
	MM_CHECK_EQ(popped.map_name, std::string("q2dm1"));
	MM_CHECK_EQ(popped.modifier_modes[MyMapModifierIndex(mymap_modifier_id_t::Powerups)], int8_t { 1 });
	MM_CHECK_EQ(popped.modifier_modes[MyMapModifierIndex(mymap_modifier_id_t::Weapons)], int8_t { -1 });
	MM_CHECK_EQ(popped.modifier_modes[MyMapModifierIndex(mymap_modifier_id_t::Ammo)], int8_t { 0 });

	MM_CHECK(PopMyMapQueueFront(queue, popped));
	MM_CHECK_EQ(popped.map_name, std::string("q2dm2"));
	MM_CHECK_EQ(popped.modifier_modes[MyMapModifierIndex(mymap_modifier_id_t::Ammo)], int8_t { -1 });
	MM_CHECK_EQ(popped.modifier_modes[MyMapModifierIndex(mymap_modifier_id_t::Health)], int8_t { 1 });
	MM_CHECK_EQ(popped.modifier_modes[MyMapModifierIndex(mymap_modifier_id_t::Powerups)], int8_t { -1 });
	MM_CHECK_FALSE(PopMyMapQueueFront(queue, popped));
}

MM_TEST(mymap_pending_selection_matches_canonical_target_once) {
	using namespace muffmode::maps;

	mymap_modifier_modes_t selected_modes {};
	MM_CHECK(ApplyMyMapModifierToken(selected_modes, "+pu"));
	MM_CHECK(ApplyMyMapModifierToken(selected_modes, "-wp"));

	mymap_pending_selection_t pending;
	MM_CHECK(pending.Arm({ "Q2DM\\Power", selected_modes }));
	MM_CHECK(pending.active());
	MM_CHECK_EQ(pending.map_name(), std::string("Q2DM\\Power"));

	mymap_modifier_modes_t applied_modes {};
	MM_CHECK_EQ(
		pending.TakeForMap("q2dm/power", applied_modes),
		mymap_pending_load_result_t::matched);
	MM_CHECK(applied_modes == selected_modes);
	MM_CHECK_FALSE(pending.active());

	applied_modes.fill(-1);
	MM_CHECK_EQ(
		pending.TakeForMap("q2dm/power", applied_modes),
		mymap_pending_load_result_t::none);
	mymap_modifier_modes_t unchanged_after_consume {};
	unchanged_after_consume.fill(-1);
	MM_CHECK(applied_modes == unchanged_after_consume);
}

MM_TEST(mymap_pending_selection_cancels_on_first_different_load) {
	using namespace muffmode::maps;

	mymap_modifier_modes_t selected_modes {};
	MM_CHECK(ApplyMyMapModifierToken(selected_modes, "-am"));

	mymap_pending_selection_t pending;
	MM_CHECK(pending.Arm({ "q2dm1", selected_modes }));
	mymap_modifier_modes_t output {};
	output.fill(1);
	const mymap_modifier_modes_t unchanged_output = output;
	MM_CHECK_EQ(
		pending.TakeForMap("q2dm2", output),
		mymap_pending_load_result_t::cancelled);
	MM_CHECK_FALSE(pending.active());
	MM_CHECK(output == unchanged_output);

	// A failed or overridden transition is deliberately not retried: even a
	// later load of the original target cannot inherit the cancelled modes.
	MM_CHECK_EQ(
		pending.TakeForMap("q2dm1", output),
		mymap_pending_load_result_t::none);
	MM_CHECK(output == unchanged_output);
}

MM_TEST(mymap_pending_selection_preserves_first_intermission_transition) {
	using namespace muffmode::maps;

	mymap_modifier_modes_t first_modes {};
	mymap_modifier_modes_t second_modes {};
	MM_CHECK(ApplyMyMapModifierToken(first_modes, "+pu"));
	MM_CHECK(ApplyMyMapModifierToken(second_modes, "-wp"));

	mymap_pending_selection_t pending;
	MM_CHECK(pending.Arm({ "q2dm1", first_modes }));
	MM_CHECK_FALSE(pending.Arm({ "q2dm2", second_modes }));
	MM_CHECK_EQ(pending.map_name(), std::string("q2dm1"));

	mymap_modifier_modes_t applied_modes {};
	MM_CHECK_EQ(
		pending.TakeForMap("q2dm1", applied_modes),
		mymap_pending_load_result_t::matched);
	MM_CHECK(applied_modes == first_modes);
}

MM_TEST(parse_int_rejects_malformed_values) {
	MM_CHECK_EQ(*MM_ParseIntArg("42"), 42);
	MM_CHECK_EQ(*MM_ParseIntArg("-7"), -7);
	MM_CHECK_FALSE(MM_ParseIntArg(""));
	MM_CHECK_FALSE(MM_ParseIntArg("+7"));
	MM_CHECK_FALSE(MM_ParseIntArg("12x"));
	MM_CHECK_FALSE(MM_ParseIntArg("2147483648"));
	MM_CHECK_FALSE(MM_ParseIntArg("-2147483649"));
}

MM_TEST(parse_non_negative_int_rejects_negative_and_malformed_values) {
	MM_CHECK_EQ(*MM_ParseNonNegativeIntArg("0"), 0);
	MM_CHECK_EQ(*MM_ParseNonNegativeIntArg("2147483647"), 2147483647);
	MM_CHECK_FALSE(MM_ParseNonNegativeIntArg("-1"));
	MM_CHECK_FALSE(MM_ParseNonNegativeIntArg("+1"));
	MM_CHECK_FALSE(MM_ParseNonNegativeIntArg("1.0"));
}

MM_TEST(reinforcement_specs_keep_classname_and_strength_paired) {
	const auto stalker = MM_ParseReinforcementSpec("monster_stalker 1");
	MM_CHECK(stalker.has_value());
	MM_CHECK_EQ(stalker->classname, std::string_view("monster_stalker"));
	MM_CHECK_EQ(stalker->strength, 1);

	const auto gladiator = MM_ParseReinforcementSpec("\tmonster_gladiator   6 \r\n");
	MM_CHECK(gladiator.has_value());
	MM_CHECK_EQ(gladiator->classname, std::string_view("monster_gladiator"));
	MM_CHECK_EQ(gladiator->strength, 6);

	MM_CHECK_FALSE(MM_ParseReinforcementSpec(""));
	MM_CHECK_FALSE(MM_ParseReinforcementSpec("monster_stalker"));
	MM_CHECK_FALSE(MM_ParseReinforcementSpec("monster_stalker 0"));
	MM_CHECK_FALSE(MM_ParseReinforcementSpec("monster_stalker -1"));
	MM_CHECK_FALSE(MM_ParseReinforcementSpec("monster_stalker 1 trailing"));
}

MM_TEST(parse_uint32_rejects_signed_malformed_and_overflow_values) {
	MM_CHECK_EQ(*MM_ParseUInt32Arg("0"), 0u);
	MM_CHECK_EQ(*MM_ParseUInt32Arg("4294967295"), std::numeric_limits<uint32_t>::max());
	MM_CHECK_FALSE(MM_ParseUInt32Arg(""));
	MM_CHECK_FALSE(MM_ParseUInt32Arg("-1"));
	MM_CHECK_FALSE(MM_ParseUInt32Arg("+1"));
	MM_CHECK_FALSE(MM_ParseUInt32Arg(" 1"));
	MM_CHECK_FALSE(MM_ParseUInt32Arg("1x"));
	MM_CHECK_FALSE(MM_ParseUInt32Arg("4294967296"));
	MM_CHECK_EQ(*MM_ParseCanonicalUInt32Arg("0"), 0u);
	MM_CHECK_EQ(*MM_ParseCanonicalUInt32Arg("4294967295"),
		std::numeric_limits<uint32_t>::max());
	MM_CHECK_FALSE(MM_ParseCanonicalUInt32Arg("00"));
	MM_CHECK_FALSE(MM_ParseCanonicalUInt32Arg("01"));
	MM_CHECK_FALSE(MM_ParseCanonicalUInt32Arg("+1"));

	const char embedded_null[] = { '1', '\0', '2' };
	MM_CHECK_FALSE(MM_ParseUInt32Text(std::string_view(embedded_null, sizeof(embedded_null))));
	MM_CHECK_FALSE(MM_ParseCanonicalUInt32Text(
		std::string_view(embedded_null, sizeof(embedded_null))));
}

MM_TEST(parse_bool_accepts_common_tokens_without_guessing) {
	MM_CHECK_EQ(*MM_ParseBoolArg("1"), true);
	MM_CHECK_EQ(*MM_ParseBoolArg("ON"), true);
	MM_CHECK_EQ(*MM_ParseBoolArg("enabled"), true);
	MM_CHECK_EQ(*MM_ParseBoolArg("0"), false);
	MM_CHECK_EQ(*MM_ParseBoolArg("off"), false);
	MM_CHECK_EQ(*MM_ParseBoolArg("Disable"), false);
	MM_CHECK_FALSE(MM_ParseBoolArg(nullptr));
	MM_CHECK_FALSE(MM_ParseBoolArg(""));
	MM_CHECK_FALSE(MM_ParseBoolArg("2"));
	MM_CHECK_FALSE(MM_ParseBoolArg(" on"));
}

MM_TEST(item_touch_client_pickup_requires_playing_live_unfrozen_client) {
	MM_CHECK(MM_ItemTouchClientMayPickup(true, 1, false));
	MM_CHECK(MM_ItemTouchClientMayPickup(true, 100, false));
	MM_CHECK_FALSE(MM_ItemTouchClientMayPickup(false, 100, false));
	MM_CHECK_FALSE(MM_ItemTouchClientMayPickup(true, 0, false));
	MM_CHECK_FALSE(MM_ItemTouchClientMayPickup(true, -1, false));
	MM_CHECK_FALSE(MM_ItemTouchClientMayPickup(true, 100, true));
}

MM_TEST(parse_clamped_int_defaults_invalid_values_and_bounds_valid_numbers) {
	MM_CHECK_EQ(MM_ParseClampedIntArgOrDefault("2", 7, 0, 3), 2);
	MM_CHECK_EQ(MM_ParseClampedIntArgOrDefault("-4", 7, 0, 3), 0);
	MM_CHECK_EQ(MM_ParseClampedIntArgOrDefault("99", 7, 0, 3), 3);
	MM_CHECK_EQ(MM_ParseClampedIntArgOrDefault("bad", 7, 0, 3), 7);
	MM_CHECK_EQ(MM_ParseClampedIntArgOrDefault(nullptr, 7, 0, 3), 7);
	MM_CHECK_EQ(MM_ParseClampedIntArgOrDefault("2", 7, 3, 0), 7);
}

MM_TEST(parse_float_rejects_non_finite_and_trailing_values) {
	MM_CHECK_EQ(*MM_ParseFloatArg("12.5"), 12.5f);
	MM_CHECK_EQ(*MM_ParseFloatArg("-0.25"), -0.25f);
	MM_CHECK_FALSE(MM_ParseFloatArg("nan"));
	MM_CHECK_FALSE(MM_ParseFloatArg("inf"));
	MM_CHECK_FALSE(MM_ParseFloatArg(" 1.0"));
	MM_CHECK_FALSE(MM_ParseFloatArg("1.0x"));

	const char embedded_null[] = { '1', '.', '0', '\0', '2' };
	MM_CHECK_FALSE(MM_ParseFloatArg(std::string_view(embedded_null, sizeof(embedded_null))));
}

MM_TEST(parse_cfg_int_accepts_quoted_or_plain_values) {
	MM_CHECK_EQ(*MM_ParseCfgIntArg("16"), 16);
	MM_CHECK_EQ(*MM_ParseCfgIntArg(" \t16\t "), 16);
	MM_CHECK_EQ(*MM_ParseCfgIntArg("\r\n16\r\n"), 16);
	MM_CHECK_EQ(*MM_ParseCfgIntArg("\"8\""), 8);
	MM_CHECK_EQ(*MM_ParseCfgIntArg("\" 8 \""), 8);
	MM_CHECK_EQ(*MM_ParseCfgIntArg(" \"4\" \t\r\n"), 4);
	MM_CHECK_FALSE(MM_ParseCfgIntArg("\"4"));
	MM_CHECK_FALSE(MM_ParseCfgIntArg("\"\""));
	MM_CHECK_FALSE(MM_ParseCfgIntArg("4 extra"));
	MM_CHECK_FALSE(MM_ParseCfgIntArg("\"4\" extra"));
}

MM_TEST(string_helpers_copy_overlap_and_truncate_display_text_safely) {
	char buffer[8] = "abcdef";
	muffmode::CopyString(buffer, std::string_view(buffer + 1, 4));
	MM_CHECK_EQ(std::string(buffer), std::string("bcde"));

	muffmode::CopyString(buffer, "123456789");
	MM_CHECK_EQ(std::string(buffer), std::string("1234567"));

	MM_CHECK_EQ(muffmode::TruncateWithEllipsis("abcdef", 8), std::string("abcdef"));
	MM_CHECK_EQ(muffmode::TruncateWithEllipsis("abcdef", 6), std::string("abcdef"));
	MM_CHECK_EQ(muffmode::TruncateWithEllipsis("abcdef", 5), std::string("ab..."));
	MM_CHECK_EQ(muffmode::TruncateWithEllipsis("abcdef", 3), std::string("..."));
	MM_CHECK_EQ(muffmode::TruncateWithEllipsis("abcdef", 2), std::string(".."));
	MM_CHECK(muffmode::TruncateWithEllipsis("abcdef", 0).empty());
}

MM_TEST(map_tokens_reject_traversal_segments_and_unsafe_characters) {
	using muffmode::maps::IsSafeMapTokenText;

	MM_CHECK(IsSafeMapTokenText("base1", 64));
	MM_CHECK(IsSafeMapTokenText("q64/outpost", 64));
	MM_CHECK(IsSafeMapTokenText("glug!", 64));
	MM_CHECK(muffmode::maps::MapTokensEqual("q64/outpost", "Q64\\OUTPOST"));
	MM_CHECK_FALSE(muffmode::maps::MapTokensEqual("q64/outpost", "q64/outpost2"));
	MM_CHECK_FALSE(IsSafeMapTokenText("", 64));
	MM_CHECK_FALSE(IsSafeMapTokenText(".", 64));
	MM_CHECK_FALSE(IsSafeMapTokenText("..", 64));
	MM_CHECK_FALSE(IsSafeMapTokenText("../base1", 64));
	MM_CHECK_FALSE(IsSafeMapTokenText("q64/./outpost", 64));
	MM_CHECK_FALSE(IsSafeMapTokenText("q64//outpost", 64));
	MM_CHECK_FALSE(IsSafeMapTokenText("q64\\..\\base1", 64));
	MM_CHECK_FALSE(IsSafeMapTokenText("bad;map", 64));
	MM_CHECK_FALSE(IsSafeMapTokenText("bad|map", 64));
	MM_CHECK_FALSE(IsSafeMapTokenText(std::string(64, 'a'), 64));
}

MM_TEST(map_pool_config_filenames_are_bounded_safe_leaves) {
	using muffmode::map_pool::IsSafeConfigLeaf;

	MM_CHECK(IsSafeConfigLeaf("muffmode-map-pool.json", 128));
	MM_CHECK(IsSafeConfigLeaf("map_cycle-2.txt", 128));
	MM_CHECK_FALSE(IsSafeConfigLeaf("", 128));
	MM_CHECK_FALSE(IsSafeConfigLeaf(".", 128));
	MM_CHECK_FALSE(IsSafeConfigLeaf("..", 128));
	MM_CHECK_FALSE(IsSafeConfigLeaf("maps..json", 128));
	MM_CHECK_FALSE(IsSafeConfigLeaf("../maps.json", 128));
	MM_CHECK_FALSE(IsSafeConfigLeaf("config/maps.json", 128));
	MM_CHECK_FALSE(IsSafeConfigLeaf("config\\maps.json", 128));
	MM_CHECK_FALSE(IsSafeConfigLeaf("maps list.json", 128));
	MM_CHECK_FALSE(IsSafeConfigLeaf("C:maps.json", 128));
	MM_CHECK_FALSE(IsSafeConfigLeaf("maps.", 128));
	MM_CHECK_FALSE(IsSafeConfigLeaf("CON", 128));
	MM_CHECK_FALSE(IsSafeConfigLeaf("nul.json", 128));
	MM_CHECK_FALSE(IsSafeConfigLeaf("Com9.txt", 128));
	MM_CHECK_FALSE(IsSafeConfigLeaf("LPT1.cycle", 128));
	MM_CHECK_FALSE(IsSafeConfigLeaf(std::string("maps\0.json", 10), 128));
	MM_CHECK_FALSE(IsSafeConfigLeaf(std::string(128, 'a'), 128));
}

MM_TEST(structured_map_identifiers_match_engine_and_windows_path_semantics) {
	using muffmode::map_pool::IsCanonicalStructuredMapIdentifier;
	using muffmode::map_pool::IsSafeStructuredMapIdentifier;

	MM_CHECK(IsSafeStructuredMapIdentifier("q2dm1", 64));
	MM_CHECK(IsSafeStructuredMapIdentifier("q64/q2dm1", 64));
	MM_CHECK(IsSafeStructuredMapIdentifier("Q64\\Q2DM1", 64));
	MM_CHECK(IsSafeStructuredMapIdentifier("glug!", 64));
	MM_CHECK(IsSafeStructuredMapIdentifier("maps", 64));
	MM_CHECK(IsSafeStructuredMapIdentifier("map.v2", 64));
	MM_CHECK(IsCanonicalStructuredMapIdentifier("q2dm1", 64));
	MM_CHECK(IsCanonicalStructuredMapIdentifier("q64/q2dm1", 64));

	MM_CHECK_FALSE(IsCanonicalStructuredMapIdentifier("Q2DM1", 64));
	MM_CHECK_FALSE(IsCanonicalStructuredMapIdentifier("q64\\q2dm1", 64));
	MM_CHECK_FALSE(IsSafeStructuredMapIdentifier("maps/q2dm1", 64));
	MM_CHECK_FALSE(IsSafeStructuredMapIdentifier("MAPS\\Q2DM1", 64));
	MM_CHECK_FALSE(IsSafeStructuredMapIdentifier("q2dm1.bsp", 64));
	MM_CHECK_FALSE(IsSafeStructuredMapIdentifier("q2dm1.CIN", 64));
	MM_CHECK_FALSE(IsSafeStructuredMapIdentifier("q2dm1.dm2", 64));
	MM_CHECK_FALSE(IsSafeStructuredMapIdentifier("q2dm1.pcx", 64));
	MM_CHECK_FALSE(IsSafeStructuredMapIdentifier("q2dm1.png", 64));
	MM_CHECK_FALSE(IsSafeStructuredMapIdentifier("q2dm1+base1", 64));
	MM_CHECK_FALSE(IsSafeStructuredMapIdentifier("q2dm1$start", 64));
	MM_CHECK_FALSE(IsSafeStructuredMapIdentifier("!q2dm1", 64));
	MM_CHECK_FALSE(IsSafeStructuredMapIdentifier("foo./bar", 64));
	MM_CHECK_FALSE(IsSafeStructuredMapIdentifier("con", 64));
	MM_CHECK_FALSE(IsSafeStructuredMapIdentifier("folder/nul.txt", 64));
	MM_CHECK_FALSE(IsSafeStructuredMapIdentifier("com1/q2dm1", 64));
	MM_CHECK_FALSE(IsSafeStructuredMapIdentifier(
		std::string("q2dm1\xc3\xa9", 7), 64));
}

MM_TEST(map_pool_display_text_requires_safe_well_formed_utf8) {
	using muffmode::map_pool::IsSafeDisplayText;
	using muffmode::map_pool::IsWellFormedUtf8;

	const std::string accented("Cold Z\xc3\xa9ro", 10);
	const std::string emoji("Map \xf0\x9f\x97\xba", 8);
	MM_CHECK(IsWellFormedUtf8(accented));
	MM_CHECK(IsWellFormedUtf8(emoji));
	MM_CHECK(IsSafeDisplayText(accented, 32));
	MM_CHECK(IsSafeDisplayText(emoji, 32));
	MM_CHECK(IsSafeDisplayText("", 0));

	MM_CHECK_FALSE(IsWellFormedUtf8(std::string("\xc0\xaf", 2)));
	MM_CHECK_FALSE(IsWellFormedUtf8(std::string("\xed\xa0\x80", 3)));
	MM_CHECK_FALSE(IsWellFormedUtf8(std::string("\xf4\x90\x80\x80", 4)));
	MM_CHECK_FALSE(IsWellFormedUtf8(std::string("\xe2\x82", 2)));
	MM_CHECK_FALSE(IsWellFormedUtf8(std::string("\x80", 1)));
	MM_CHECK_FALSE(IsSafeDisplayText("line\nbreak", 32));
	MM_CHECK_FALSE(IsSafeDisplayText(std::string("\xc2\x85", 2), 32));
	MM_CHECK_FALSE(IsSafeDisplayText(std::string("\xe2\x80\xa8", 3), 32));
	MM_CHECK_FALSE(IsSafeDisplayText(std::string("\xe2\x80\xae", 3), 32));
	MM_CHECK_FALSE(IsSafeDisplayText(std::string("\xe2\x81\xa6", 3), 32));
	MM_CHECK_FALSE(IsSafeDisplayText(std::string("\xd8\x9c", 2), 32));
	MM_CHECK_FALSE(IsSafeDisplayText(std::string("\xef\xbb\xbf", 3), 32));
}

MM_TEST(map_cycle_parser_preserves_order_comments_and_case_insensitive_deduplication) {
	using muffmode::map_pool::ParseCycleText;

	const auto result = ParseCycleText(
		"// stock and rerelease maps\n"
		"q2dm1 q64/outpost\n"
		"Q2DM1 q64\\outpost /* duplicates */ glug!\n"
		"q2dm2/* adjacent comment */q2dm3\n",
		64);

	MM_CHECK(result.valid);
	MM_CHECK(result.error.empty());
	MM_CHECK_EQ(result.tokens_seen, 7u);
	MM_CHECK_EQ(result.invalid_tokens, 0u);
	MM_CHECK_EQ(result.duplicate_tokens, 2u);
	MM_CHECK_EQ(result.maps.size(), 5u);
	MM_CHECK_EQ(result.maps[0], std::string("q2dm1"));
	MM_CHECK_EQ(result.maps[1], std::string("q64/outpost"));
	MM_CHECK_EQ(result.maps[2], std::string("glug!"));
	MM_CHECK_EQ(result.maps[3], std::string("q2dm2"));
	MM_CHECK_EQ(result.maps[4], std::string("q2dm3"));
}

MM_TEST(map_cycle_parser_handles_bom_and_rejects_engine_map_command_tokens) {
	using muffmode::map_pool::ParseCycleText;

	const std::string with_bom =
		std::string("\xef\xbb\xbf", 3) + "q2dm1 q2dm2";
	const auto bom = ParseCycleText(with_bom, 64);
	MM_CHECK(bom.valid);
	MM_CHECK_EQ(bom.maps.size(), 2u);
	MM_CHECK_EQ(bom.maps[0], std::string("q2dm1"));

	const auto unsafe = ParseCycleText(
		"q2dm1 q2dm2.bsp maps/q2dm3 q2dm4+base1 q2dm5$start !q2dm6 q2dm7",
		64);
	MM_CHECK(unsafe.valid);
	MM_CHECK_EQ(unsafe.tokens_seen, 7u);
	MM_CHECK_EQ(unsafe.invalid_tokens, 5u);
	MM_CHECK_EQ(unsafe.maps.size(), 2u);
	MM_CHECK_EQ(unsafe.maps[0], std::string("q2dm1"));
	MM_CHECK_EQ(unsafe.maps[1], std::string("q2dm7"));
}

MM_TEST(map_cycle_parser_discards_unsafe_tokens_without_discarding_valid_order) {
	using muffmode::map_pool::ParseCycleText;

	const auto result = ParseCycleText(
		"q2dm1 ../base1 bad;map q2dm2",
		64);

	MM_CHECK(result.valid);
	MM_CHECK_EQ(result.tokens_seen, 4u);
	MM_CHECK_EQ(result.invalid_tokens, 2u);
	MM_CHECK_EQ(result.duplicate_tokens, 0u);
	MM_CHECK_EQ(result.maps.size(), 2u);
	MM_CHECK_EQ(result.maps[0], std::string("q2dm1"));
	MM_CHECK_EQ(result.maps[1], std::string("q2dm2"));
}

MM_TEST(map_cycle_parser_rejects_malformed_or_over_limit_input) {
	using muffmode::map_pool::ParseCycleText;

	const auto unterminated = ParseCycleText("q2dm1 /* missing end", 64);
	MM_CHECK_FALSE(unterminated.valid);
	MM_CHECK_EQ(unterminated.error, std::string("cycle has an unterminated block comment"));

	const auto unexpected_end = ParseCycleText("q2dm1 */ q2dm2", 64);
	MM_CHECK_FALSE(unexpected_end.valid);
	MM_CHECK_EQ(
		unexpected_end.error,
		std::string("cycle has an unexpected block-comment terminator"));

	const char with_nul[] = { 'q', '2', 'd', 'm', '1', '\0', 'q', '2', 'd', 'm', '2' };
	const auto embedded_nul =
		ParseCycleText(std::string_view(with_nul, sizeof(with_nul)), 64);
	MM_CHECK_FALSE(embedded_nul.valid);
	MM_CHECK_EQ(embedded_nul.error, std::string("cycle contains an embedded NUL byte"));

	const auto token_limit = ParseCycleText("q2dm1 q2dm2 q2dm3", 64, 2);
	MM_CHECK_FALSE(token_limit.valid);
	MM_CHECK_EQ(token_limit.tokens_seen, 3u);
	MM_CHECK_EQ(token_limit.error, std::string("cycle contains too many tokens"));

	const auto path_limit = ParseCycleText("1234567 12345678 q2dm1", 8);
	MM_CHECK(path_limit.valid);
	MM_CHECK_EQ(path_limit.tokens_seen, 3u);
	MM_CHECK_EQ(path_limit.invalid_tokens, 1u);
	MM_CHECK_EQ(path_limit.maps.size(), 2u);
	MM_CHECK_EQ(path_limit.maps[0], std::string("1234567"));
	MM_CHECK_EQ(path_limit.maps[1], std::string("q2dm1"));
}

MM_TEST(map_pool_mode_preferences_and_relaxation_order_are_explicit) {
	using namespace muffmode::map_pool;

	const mode_selection_t arena =
		ResolveModeSelection(true, false, false, false, true, true);
	MM_CHECK_EQ(arena.preferred, MAP_MODE_ARENA);
	MM_CHECK_EQ(arena.fallback, MAP_MODE_NONE);

	const mode_selection_t ctf =
		ResolveModeSelection(false, true, false, true, true, true);
	MM_CHECK_EQ(ctf.preferred, MAP_MODE_CTF);
	MM_CHECK_EQ(ctf.fallback, MAP_MODE_NONE);

	const mode_selection_t duel =
		ResolveModeSelection(false, false, true, false, true, false);
	MM_CHECK_EQ(duel.preferred, MAP_MODE_DUEL);
	MM_CHECK_EQ(duel.fallback, MAP_MODE_DM);

	const mode_selection_t duel_without_preferred_maps =
		ResolveModeSelection(false, false, true, false, false, false);
	MM_CHECK_EQ(duel_without_preferred_maps.preferred, MAP_MODE_DM);
	MM_CHECK_EQ(duel_without_preferred_maps.fallback, MAP_MODE_NONE);

	const mode_selection_t teams =
		ResolveModeSelection(false, false, false, true, false, true);
	MM_CHECK_EQ(teams.preferred, MAP_MODE_TDM);
	MM_CHECK_EQ(teams.fallback, MAP_MODE_DM);

	MM_CHECK_EQ(SELECTION_RELAXATIONS.size(), 3u);
	MM_CHECK(SELECTION_RELAXATIONS[0].enforce_player_bounds);
	MM_CHECK(SELECTION_RELAXATIONS[0].enforce_cooldown);
	MM_CHECK(SELECTION_RELAXATIONS[1].enforce_player_bounds);
	MM_CHECK_FALSE(SELECTION_RELAXATIONS[1].enforce_cooldown);
	MM_CHECK_FALSE(SELECTION_RELAXATIONS[2].enforce_player_bounds);
	MM_CHECK_FALSE(SELECTION_RELAXATIONS[2].enforce_cooldown);
}

MM_TEST(map_pool_reload_action_prioritizes_pending_full_transactions) {
	using muffmode::map_pool::reload_action_t;
	using muffmode::map_pool::reload_context_t;
	using muffmode::map_pool::ResolveReloadAction;

	MM_CHECK_EQ(ResolveReloadAction(1, 1, 2, 2), reload_action_t::none);
	MM_CHECK_EQ(ResolveReloadAction(2, 1, 2, 1), reload_action_t::pool);
	MM_CHECK_EQ(ResolveReloadAction(1, 1, 2, 1), reload_action_t::cycle);
	reload_context_t pending;
	pending.pool_reload_pending = true;
	MM_CHECK_EQ(
		ResolveReloadAction(1, 1, 2, 1, pending),
		reload_action_t::pool);
	MM_CHECK_EQ(
		ResolveReloadAction(1, 1, 1, 1, pending),
		reload_action_t::none);

	reload_context_t disable_pending = pending;
	disable_pending.cycle_disabled = true;
	MM_CHECK_EQ(
		ResolveReloadAction(1, 1, 2, 1, disable_pending),
		reload_action_t::cycle);
	MM_CHECK_EQ(
		ResolveReloadAction(2, 1, 2, 1, disable_pending),
		reload_action_t::cycle);

	reload_context_t disable_all = disable_pending;
	disable_all.pool_disabled = true;
	MM_CHECK_EQ(
		ResolveReloadAction(2, 1, 2, 1, disable_all),
		reload_action_t::pool);
}

MM_TEST(changelevel_tokens_allow_unit_markers_without_allowing_commands) {
	using muffmode::maps::IsSafeChangeLevelTokenText;

	MM_CHECK(IsSafeChangeLevelTokenText("base1", 64));
	MM_CHECK(IsSafeChangeLevelTokenText("*base1", 64));
	MM_CHECK(IsSafeChangeLevelTokenText("base1+unit_start", 64));
	MM_CHECK(IsSafeChangeLevelTokenText("*victor1.pcx", 64));
	MM_CHECK_FALSE(IsSafeChangeLevelTokenText("", 64));
	MM_CHECK_FALSE(IsSafeChangeLevelTokenText("*", 64));
	MM_CHECK_FALSE(IsSafeChangeLevelTokenText("**base1", 64));
	MM_CHECK_FALSE(IsSafeChangeLevelTokenText("base1;quit", 64));
	MM_CHECK_FALSE(IsSafeChangeLevelTokenText("base1\"\nquit", 64));
	MM_CHECK_FALSE(IsSafeChangeLevelTokenText(std::string(64, 'a'), 64));
}

MM_TEST(map_vote_snapshot_refreshes_only_for_source_revision_changes) {
	using muffmode::vote_menu::MapMenuSnapshotNeedsRefresh;
	using muffmode::vote_menu::MapMenuSourceRevision;

	int list_a = 0;
	int list_b = 0;
	int pool_a = 0;
	int pool_b = 0;
	const MapMenuSourceRevision original { &list_a, 7, &pool_a, 11, 23 };

	MM_CHECK(MapMenuSnapshotNeedsRefresh(false, original, original));
	MM_CHECK_FALSE(MapMenuSnapshotNeedsRefresh(true, original, original));
	MM_CHECK(MapMenuSnapshotNeedsRefresh(
		true, original, { &list_a, 8, &pool_a, 11, 23 }));
	MM_CHECK(MapMenuSnapshotNeedsRefresh(
		true, original, { &list_a, 7, &pool_a, 12, 23 }));
	MM_CHECK(MapMenuSnapshotNeedsRefresh(
		true, original, { &list_b, 7, &pool_a, 11, 23 }));
	MM_CHECK(MapMenuSnapshotNeedsRefresh(
		true, original, { &list_a, 7, &pool_b, 11, 23 }));
	MM_CHECK(MapMenuSnapshotNeedsRefresh(
		true, original, { &list_a, 7, &pool_a, 11, 24 }));
}

MM_TEST(motd_filenames_reject_paths_devices_and_shell_metacharacters) {
	using muffmode::motd::IsSafeFilenameText;

	MM_CHECK(IsSafeFilenameText("motd.txt", 64));
	MM_CHECK(IsSafeFilenameText(".motd", 64));
	MM_CHECK_FALSE(IsSafeFilenameText("", 64));
	MM_CHECK_FALSE(IsSafeFilenameText(".", 64));
	MM_CHECK_FALSE(IsSafeFilenameText("..", 64));
	MM_CHECK_FALSE(IsSafeFilenameText("motd.", 64));
	MM_CHECK_FALSE(IsSafeFilenameText("../motd.txt", 64));
	MM_CHECK_FALSE(IsSafeFilenameText("motd?.txt", 64));
	MM_CHECK_FALSE(IsSafeFilenameText("motd|old.txt", 64));
	MM_CHECK_FALSE(IsSafeFilenameText(std::string("bad\x7fname.txt", 12), 64));
	MM_CHECK_FALSE(IsSafeFilenameText(std::string(64, 'a'), 64));
}

MM_TEST(reliable_motd_output_is_utf8_safe_and_bounded_for_256_kib_files) {
	using namespace muffmode::reliable_text;
	using muffmode::map_pool::IsWellFormedUtf8;

	constexpr size_t motd_file_bytes = 256 * 1024;
	std::string motd;
	motd.reserve(motd_file_bytes);
	while (motd.size() < motd_file_bytes)
		motd += "\xf0\x9f\x98\x80";
	MM_CHECK_EQ(motd.size(), motd_file_bytes);

	std::string response = "Message of the Day:\n";
	response += motd;
	response += "\n";
	const auto raw_plan = PlanChunks(
		response, kMaxPrintPayloadBytes, kMaxMotdCommandMessages);
	MM_CHECK(raw_plan.truncated);
	MM_CHECK(raw_plan.chunks.size() <= kMaxMotdCommandMessages);
	for (const std::string_view chunk : raw_plan.chunks) {
		MM_CHECK(chunk.size() <= kMaxPrintPayloadBytes);
		MM_CHECK(IsWellFormedUtf8(chunk));
	}

	constexpr size_t response_budget =
		kMaxMotdCommandMessages * kGuaranteedPrintContentBytes;
	const std::string bounded = MakeBoundedPreview(response, response_budget);
	MM_CHECK(bounded.size() <= response_budget);
	MM_CHECK(IsWellFormedUtf8(bounded));
	MM_CHECK_EQ(
		bounded.substr(bounded.size() - kTruncatedNotice.size()),
		std::string(kTruncatedNotice));

	const auto bounded_plan = PlanChunks(
		bounded, kMaxPrintPayloadBytes, kMaxMotdCommandMessages);
	MM_CHECK_FALSE(bounded_plan.truncated);
	MM_CHECK(bounded_plan.chunks.size() <= kMaxMotdCommandMessages);
	std::string reconstructed;
	for (const std::string_view chunk : bounded_plan.chunks) {
		MM_CHECK(chunk.size() <= kMaxPrintPayloadBytes);
		MM_CHECK(IsWellFormedUtf8(chunk));
		reconstructed.append(chunk);
	}
	MM_CHECK_EQ(reconstructed, bounded);

	const std::string automatic = MakeBoundedPreview(
		motd, kMaxPrintPayloadBytes);
	MM_CHECK(automatic.size() <= kMaxPrintPayloadBytes);
	MM_CHECK(IsWellFormedUtf8(automatic));
	MM_CHECK_EQ(
		automatic.substr(automatic.size() - kTruncatedNotice.size()),
		std::string(kTruncatedNotice));
}

MM_TEST(reliable_preview_does_not_split_utf8_at_byte_budget) {
	using namespace muffmode::reliable_text;
	using muffmode::map_pool::IsWellFormedUtf8;

	std::string text(510, 'a');
	text += "\xf0\x9f\x98\x80";
	const std::string preview = MakeBoundedPreview(text, 512, "...");

	MM_CHECK_EQ(preview.size(), size_t { 512 });
	MM_CHECK(IsWellFormedUtf8(preview));
	MM_CHECK_EQ(preview.substr(509), std::string("..."));
}

MM_TEST(maximal_mymap_queue_fits_the_capped_four_message_fanout) {
	using namespace muffmode::maps;
	using namespace muffmode::reliable_text;

	std::vector<mymap_queue_entry_t> queue;
	queue.reserve(kMaxMyMapQueueEntries + 1);
	for (size_t i = 0; i < kMaxMyMapQueueEntries; i++) {
		mymap_queue_entry_t entry;
		entry.map_name.assign(63, 'a');
		entry.map_name[0] = static_cast<char>('a' + (i % 26));
		entry.map_name[1] = static_cast<char>('a' + ((i / 26) % 26));
		entry.modifier_modes.fill(1);
		queue.push_back(std::move(entry));
	}
	queue.push_back({ "ignored", {} });

	std::string display = "MyMap Queue => ";
	display += FormatMyMapQueueEntries(queue);
	display += "\n";
	MM_CHECK_EQ(display.find("ignored"), std::string::npos);
	MM_CHECK(display.size() <=
		kMaxMyMapQueueMessages * kGuaranteedPrintContentBytes);

	const auto plan = PlanChunks(
		display, kMaxPrintPayloadBytes, kMaxMyMapQueueMessages);
	MM_CHECK_FALSE(plan.truncated);
	MM_CHECK_EQ(plan.chunks.size(), kMaxMyMapQueueMessages);
	std::string reconstructed;
	for (const std::string_view chunk : plan.chunks) {
		MM_CHECK(chunk.size() <= kMaxPrintPayloadBytes);
		reconstructed.append(chunk);
	}
	MM_CHECK_EQ(reconstructed, display);

	const std::string delta = "MyMap queued => " +
		FormatMyMapQueueEntry(queue.front()) + "\n";
	const auto delta_plan = PlanChunks(
		delta, kMaxPrintPayloadBytes, size_t { 1 });
	MM_CHECK_FALSE(delta_plan.truncated);
	MM_CHECK_EQ(delta_plan.chunks.size(), size_t { 1 });
	MM_CHECK(delta.size() <= kGuaranteedPrintContentBytes);

	const std::string oversized(16 * 1024, 'x');
	const std::string capped = MakeBoundedPreview(
		oversized,
		kMaxMyMapQueueMessages * kGuaranteedPrintContentBytes);
	const auto capped_plan = PlanChunks(
		capped, kMaxPrintPayloadBytes, kMaxMyMapQueueMessages);
	MM_CHECK_FALSE(capped_plan.truncated);
	MM_CHECK(capped_plan.chunks.size() <= kMaxMyMapQueueMessages);
	MM_CHECK_EQ(
		capped.substr(capped.size() - kTruncatedNotice.size()),
		std::string(kTruncatedNotice));
}

MM_TEST(player_config_social_ids_encode_to_safe_unique_path_stems) {
	using namespace muffmode::pconfig;

	const auto plain = EncodeSocialIdConfigStem("a.b-c_123", 63, 251);
	const auto path_like = EncodeSocialIdConfigStem("..abc/def\\ghi.", 63, 251);
	const auto reserved = EncodeSocialIdConfigStem("con", 63, 251);
	const auto max_fit = EncodeSocialIdConfigStem(std::string(123, 'a'), 123, 251);

	MM_CHECK(plain);
	MM_CHECK(path_like);
	MM_CHECK(reserved);
	MM_CHECK(max_fit);
	MM_CHECK_EQ(*plain, std::string("sid-612e622d635f313233"));
	MM_CHECK_EQ(*path_like, std::string("sid-2e2e6162632f6465665c6768692e"));
	MM_CHECK_EQ(*reserved, std::string("sid-636f6e"));
	MM_CHECK_FALSE(EncodeSocialIdConfigStem("", 63, 251));
	MM_CHECK_FALSE(EncodeSocialIdConfigStem(std::string(64, 'a'), 63, 251));
	MM_CHECK_FALSE(EncodeSocialIdConfigStem(std::string(124, 'a'), 124, 251));
	MM_CHECK_FALSE(EncodeSocialIdConfigStem("ab", 63, 7));
	MM_CHECK(*EncodeSocialIdConfigStem("ab", 63, 251) != *EncodeSocialIdConfigStem("a/b", 63, 251));
}

MM_TEST(player_config_legacy_worr_profile_stems_match_original_sanitizer) {
	using namespace muffmode::pconfig;

	MM_CHECK_EQ(*LegacyWorrSocialIdConfigStem("a.b-c_123", 63, 251),
		std::string("ab-c_123"));
	MM_CHECK_EQ(*LegacyWorrSocialIdConfigStem("../abc\\def", 63, 251),
		std::string("abcdef"));
	MM_CHECK_FALSE(LegacyWorrSocialIdConfigStem("...", 63, 251));
	MM_CHECK_FALSE(LegacyWorrSocialIdConfigStem("abc", 2, 251));
	MM_CHECK_FALSE(LegacyWorrSocialIdConfigStem("abc", 63, 2));
}

MM_TEST(player_config_tokens_parse_deterministically) {
	using namespace muffmode::pconfig;

	MM_CHECK_EQ(*ParseBoolToken("on"), true);
	MM_CHECK_EQ(*ParseBoolToken("DISABLED"), false);
	MM_CHECK_FALSE(ParseBoolToken("toggle"));

	MM_CHECK_EQ(*ParseKillBeepToken("off"), 0);
	MM_CHECK_EQ(*ParseKillBeepToken("beep-boop"), 2);
	MM_CHECK_EQ(*ParseKillBeepToken("tangtang"), 4);
	MM_CHECK_FALSE(ParseKillBeepToken("5"));

	MM_CHECK_EQ(*ParseFollowViewToken("first-person"), true);
	MM_CHECK_EQ(*ParseFollowViewToken("third"), false);
	MM_CHECK_EQ(*ParseFollowViewToken("on"), true);
	MM_CHECK_FALSE(ParseFollowViewToken("sideways"));
}

MM_TEST(player_config_comment_and_arg_helpers_are_quote_aware) {
	using namespace muffmode::pconfig;

	MM_CHECK_EQ(std::string(StripConfigComment("id 1 # show crosshair id")), std::string("id 1"));
	MM_CHECK_EQ(std::string(StripConfigComment("timer on ; local note")), std::string("timer on"));
	MM_CHECK_EQ(std::string(StripConfigComment("kb clang // preferred")), std::string("kb clang"));
	MM_CHECK_EQ(std::string(StripConfigComment("eskin \"male/#hash\" # note")), std::string("eskin \"male/#hash\""));
	MM_CHECK_EQ(std::string(StripConfigComment("eskin \"male//slash\" // note")), std::string("eskin \"male//slash\""));

	std::string_view value;
	MM_CHECK(ReadSingleConfigArg("on", value));
	MM_CHECK_EQ(std::string(value), std::string("on"));
	MM_CHECK(ReadSingleConfigArg(" \"third\" ", value));
	MM_CHECK_EQ(std::string(value), std::string("third"));
	MM_CHECK(ReadSingleConfigArg("\"male/grunt\"", value));
	MM_CHECK_EQ(std::string(value), std::string("male/grunt"));
	MM_CHECK_FALSE(ReadSingleConfigArg("\"unterminated", value));
	MM_CHECK_FALSE(ReadSingleConfigArg("\"first\" extra", value));
	MM_CHECK_FALSE(ReadSingleConfigArg("first extra", value));
}

MM_TEST(player_config_comment_sanitizer_respects_output_limit) {
	using namespace muffmode::pconfig;

	MM_CHECK(SanitizeConfigCommentText("", 0).empty());
	MM_CHECK_EQ(SanitizeConfigCommentText("", 3), std::string("Pla"));
	MM_CHECK_EQ(SanitizeConfigCommentText("\t\n", 4), std::string("Play"));
	MM_CHECK_EQ(SanitizeConfigCommentText("A\nB\tC", 16), std::string("A B C"));
	MM_CHECK_EQ(SanitizeConfigCommentText("abcdef", 3), std::string("abc"));
}

MM_TEST(player_config_skin_paths_match_command_safety_rules) {
	using namespace muffmode::pconfig;

	constexpr size_t max_qpath = 64;
	constexpr size_t max_netname_length = 31;
	constexpr size_t player_skin_configstring_size = 96;
	const std::string longest_fit = std::string("model/") + std::string(49, 'a');
	const std::string too_long = std::string("model/") + std::string(50, 'a');

	MM_CHECK(IsSafeSkinPath("male/grunt"));
	MM_CHECK(IsSafeSkinPath("players/male/grunt"));
	MM_CHECK(IsDisableToken("default"));
	MM_CHECK_FALSE(IsSafeSkinPath("male"));
	MM_CHECK_FALSE(IsSafeSkinPath("/male/grunt"));
	MM_CHECK_FALSE(IsSafeSkinPath("male/"));
	MM_CHECK_FALSE(IsSafeSkinPath("male//grunt"));
	MM_CHECK_FALSE(IsSafeSkinPath("male/gr.unt"));
	MM_CHECK(SkinFitsPlayerConfigString(longest_fit, max_netname_length, player_skin_configstring_size));
	MM_CHECK_FALSE(SkinFitsPlayerConfigString(too_long, max_netname_length, player_skin_configstring_size));
	MM_CHECK(IsStorableSkinPath(longest_fit, max_qpath, max_netname_length, player_skin_configstring_size));
	MM_CHECK_FALSE(IsStorableSkinPath(too_long, max_qpath, max_netname_length, player_skin_configstring_size));
	MM_CHECK_FALSE(IsStorableSkinPath("male/gr.unt", max_qpath, max_netname_length, player_skin_configstring_size));
	MM_CHECK_FALSE(IsStorableSkinPath(longest_fit, longest_fit.size(), max_netname_length, player_skin_configstring_size));
}

MM_TEST(player_config_length_helpers_reject_overflowing_bounds) {
	using namespace muffmode::pconfig;

	MM_CHECK_FALSE(EncodeSocialIdConfigStem("a", 63, 0));
	MM_CHECK_FALSE(EncodeSocialIdConfigStem("a", 63, 5));
	MM_CHECK_FALSE(SkinFitsPlayerConfigString("male/grunt", std::numeric_limits<size_t>::max(), std::numeric_limits<size_t>::max()));
	MM_CHECK_FALSE(SkinFitsPlayerConfigString("male/grunt", 95, 96));
	MM_CHECK(SkinFitsPlayerConfigString("male/grunt", 31, 96));
}

MM_TEST(match_time_formatting_handles_negative_zero_and_extreme_values) {
	MM_CHECK_EQ(MM_FormatMatchTime(0), std::string("00:00"));
	MM_CHECK_EQ(MM_FormatMatchTime(500), std::string("00:00"));
	MM_CHECK_EQ(MM_FormatMatchTime(-500), std::string("-00:00"));
	MM_CHECK_EQ(MM_FormatMatchTime(3661000), std::string("1:01:01"));
	MM_CHECK_EQ(MM_FormatMatchTimeMs(5), std::string("00:00.005"));
	MM_CHECK_EQ(MM_FormatMatchTimeMs(-5), std::string("-00:00.005"));
	MM_CHECK_EQ(MM_FormatMatchTimeMs(std::numeric_limits<int>::min()), std::string("-596:31:23.648"));
}

MM_TEST(command_argument_contracts_match_phase_one_fixes) {
	MM_CHECK(MM_IsTeleportArgcValid(4));
	MM_CHECK(MM_IsTeleportArgcValid(7));
	MM_CHECK_FALSE(MM_IsTeleportArgcValid(3));
	MM_CHECK_FALSE(MM_IsTeleportArgcValid(5));
	MM_CHECK_FALSE(MM_IsTeleportArgcValid(8));

	MM_CHECK(MM_IsSpawnArgcValid(2));
	MM_CHECK(MM_IsSpawnArgcValid(4));
	MM_CHECK(MM_IsSpawnArgcValid(6));
	MM_CHECK_FALSE(MM_IsSpawnArgcValid(1));
	MM_CHECK_FALSE(MM_IsSpawnArgcValid(3));
	MM_CHECK_FALSE(MM_IsSpawnArgcValid(5));
}

MM_TEST(command_contract_helpers_enforce_exact_arity_and_timeout_bounds) {
	MM_CHECK(MM_IsExactArgcValid(2, 2));
	MM_CHECK_FALSE(MM_IsExactArgcValid(1, 2));
	MM_CHECK_FALSE(MM_IsExactArgcValid(3, 2));
	MM_CHECK(MM_IsArgcInRangeValid(2, 1, 3));
	MM_CHECK(MM_IsArgcInRangeValid(3, 1, 3));
	MM_CHECK_FALSE(MM_IsArgcInRangeValid(0, 1, 3));
	MM_CHECK_FALSE(MM_IsArgcInRangeValid(4, 1, 3));
	MM_CHECK_FALSE(MM_IsArgcInRangeValid(2, 3, 1));

	MM_CHECK_EQ(MM_ClampTimeoutSeconds(-1), 0);
	MM_CHECK_EQ(MM_ClampTimeoutSeconds(0), 0);
	MM_CHECK_EQ(MM_ClampTimeoutSeconds(120), 120);
	MM_CHECK_EQ(MM_ClampTimeoutSeconds(3600), 3600);
	MM_CHECK_EQ(MM_ClampTimeoutSeconds(3601), 3600);
}

MM_TEST(player_profile_recreation_restores_the_full_preference_snapshot) {
	MM_CHECK_EQ(MM_ClientProfilePreferenceMergeMask(
		false, MM_CLIENT_PROFILE_PREFERENCE_SHOW_TIMER),
		MM_CLIENT_PROFILE_PREFERENCE_SHOW_TIMER);
	MM_CHECK_EQ(MM_ClientProfilePreferenceMergeMask(
		true, MM_CLIENT_PROFILE_PREFERENCE_SHOW_TIMER),
		MM_CLIENT_PROFILE_PREFERENCE_ALL);
	MM_CHECK_EQ(MM_ClientProfilePreferenceMergeMask(
		false, std::numeric_limits<mm_client_profile_preference_mask_t>::max()),
		MM_CLIENT_PROFILE_PREFERENCE_ALL);
}

MM_TEST(item_override_commands_require_exact_known_names_and_admin_authority) {
	MM_CHECK(MM_IsItemOverrideCvarFor("disable_weapon_bfg", "q2dm1", "weapon_bfg"));
	MM_CHECK(MM_IsItemOverrideCvarFor("replace_item_quad", "q2dm1", "item_quad"));
	MM_CHECK(MM_IsItemOverrideCvarFor("q2dm1_disable_weapon_bfg", "q2dm1", "weapon_bfg"));
	MM_CHECK(MM_IsItemOverrideCvarFor("q2dm1_replace_item_quad", "q2dm1", "item_quad"));

	MM_CHECK_FALSE(MM_IsItemOverrideCvarFor("g_disable_player_collision", "q2dm1", "weapon_bfg"));
	MM_CHECK_FALSE(MM_IsItemOverrideCvarFor("foo_disable_weapon_bfg", "q2dm1", "weapon_bfg"));
	MM_CHECK_FALSE(MM_IsItemOverrideCvarFor("xdisable_weapon_bfg", "q2dm1", "weapon_bfg"));
	MM_CHECK_FALSE(MM_IsItemOverrideCvarFor("disable_weapon_bfg_extra", "q2dm1", "weapon_bfg"));
	MM_CHECK_FALSE(MM_IsItemOverrideCvarFor("q2dm10_disable_weapon_bfg", "q2dm1", "weapon_bfg"));

	MM_CHECK(MM_ClientMaySetItemOverride(true, true));
	MM_CHECK_FALSE(MM_ClientMaySetItemOverride(false, true));
	MM_CHECK_FALSE(MM_ClientMaySetItemOverride(true, false));
	MM_CHECK_FALSE(MM_ClientMaySetItemOverride(false, false));
}

MM_TEST(flood_history_contracts_bound_extreme_server_values) {
	constexpr int history_capacity = 10;

	MM_CHECK_EQ(MM_ClampFloodMessageCount(std::numeric_limits<int>::min(), history_capacity), 0);
	MM_CHECK_EQ(MM_ClampFloodMessageCount(-1, history_capacity), 0);
	MM_CHECK_EQ(MM_ClampFloodMessageCount(0, history_capacity), 0);
	MM_CHECK_EQ(MM_ClampFloodMessageCount(1, history_capacity), 1);
	MM_CHECK_EQ(MM_ClampFloodMessageCount(history_capacity, history_capacity), history_capacity);
	MM_CHECK_EQ(MM_ClampFloodMessageCount(history_capacity + 1, history_capacity), history_capacity);
	MM_CHECK_EQ(MM_ClampFloodMessageCount(std::numeric_limits<int>::max(), history_capacity), history_capacity);

	MM_CHECK_EQ(MM_FloodHistoryIndex({ 0, 4, history_capacity }), 7);
	MM_CHECK_EQ(MM_FloodHistoryIndex({ 0, history_capacity, history_capacity }), 1);
	for (int head = 0; head < history_capacity; ++head) {
		for (const int count : { 1, history_capacity, history_capacity + 1,
				std::numeric_limits<int>::max() }) {
			const int index = MM_FloodHistoryIndex({ head, count, history_capacity });
			MM_CHECK(index >= 0);
			MM_CHECK(index < history_capacity);
		}
	}

	MM_CHECK_EQ(MM_NormalizeRingIndex(std::numeric_limits<int>::min(), history_capacity), 2);
	MM_CHECK_EQ(MM_NormalizeRingIndex(-1, history_capacity), 9);
	MM_CHECK_EQ(MM_NormalizeRingIndex(std::numeric_limits<int>::max(), history_capacity), 7);
}

MM_TEST(frames_per_server_frame_is_bounded_without_losing_pause_semantics) {
	MM_CHECK_EQ(MM_ClampFramesPerServerFrame(std::numeric_limits<int>::min()), 0);
	MM_CHECK_EQ(MM_ClampFramesPerServerFrame(-1), 0);
	MM_CHECK_EQ(MM_ClampFramesPerServerFrame(0), 0);
	MM_CHECK_EQ(MM_ClampFramesPerServerFrame(1), 1);
	MM_CHECK_EQ(MM_ClampFramesPerServerFrame(MM_MAX_FRAMES_PER_SERVER_FRAME),
		MM_MAX_FRAMES_PER_SERVER_FRAME);
	MM_CHECK_EQ(MM_ClampFramesPerServerFrame(MM_MAX_FRAMES_PER_SERVER_FRAME + 1),
		MM_MAX_FRAMES_PER_SERVER_FRAME);
	MM_CHECK_EQ(MM_ClampFramesPerServerFrame(std::numeric_limits<int>::max()),
		MM_MAX_FRAMES_PER_SERVER_FRAME);
}

MM_TEST(arena_players_per_team_is_bounded_by_half_the_server_capacity) {
	MM_CHECK_EQ(MM_ArenaNormalizePlayersPerTeam(-1, -1), 1);
	MM_CHECK_EQ(MM_ArenaNormalizePlayersPerTeam(0, 0), 1);
	MM_CHECK_EQ(MM_ArenaNormalizePlayersPerTeam(2, 1), 1);
	MM_CHECK_EQ(MM_ArenaNormalizePlayersPerTeam(2, 2), 1);
	MM_CHECK_EQ(MM_ArenaNormalizePlayersPerTeam(2, 3), 1);
	MM_CHECK_EQ(MM_ArenaNormalizePlayersPerTeam(1, 4), 1);
	MM_CHECK_EQ(MM_ArenaNormalizePlayersPerTeam(2, 4), 2);
	MM_CHECK_EQ(MM_ArenaNormalizePlayersPerTeam(3, 4), 2);
	MM_CHECK_EQ(MM_ArenaNormalizePlayersPerTeam(99, 16), 8);
	MM_CHECK_EQ(MM_ArenaNormalizePlayersPerTeam(
		std::numeric_limits<int>::max(), static_cast<int>(MAX_LOBBY_PLAYERS)), 64);
	MM_CHECK_EQ(
		MM_ArenaNormalizePlayersPerTeam(std::numeric_limits<int>::max(), std::numeric_limits<int>::max()),
		std::numeric_limits<int>::max() / 2);
}

MM_TEST(arena_team_zero_never_matches_and_round_over_revalidates_pairing) {
	MM_CHECK_FALSE(MM_ArenaLogicalTeamIdMatches(0, 0));
	MM_CHECK_FALSE(MM_ArenaLogicalTeamIdMatches(7, 0));
	MM_CHECK_FALSE(MM_ArenaLogicalTeamIdMatches(0, 7));
	MM_CHECK(MM_ArenaLogicalTeamIdMatches(7, 7));
	MM_CHECK_FALSE(MM_ArenaLogicalTeamIdMatches(8, 7));

	MM_CHECK(MM_ArenaStateRequiresValidPairing(mm_arena_state_t::Empty));
	MM_CHECK(MM_ArenaStateRequiresValidPairing(mm_arena_state_t::Warmup));
	MM_CHECK(MM_ArenaStateRequiresValidPairing(mm_arena_state_t::MatchCountdown));
	MM_CHECK(MM_ArenaStateRequiresValidPairing(mm_arena_state_t::RoundCountdown));
	MM_CHECK(MM_ArenaStateRequiresValidPairing(mm_arena_state_t::RoundOver));
	MM_CHECK_FALSE(MM_ArenaStateRequiresValidPairing(mm_arena_state_t::Running));
	MM_CHECK_FALSE(MM_ArenaStateRequiresValidPairing(mm_arena_state_t::MatchOver));
	MM_CHECK_FALSE(MM_ArenaStateRequiresValidPairing(mm_arena_state_t::Paused));

	MM_CHECK(MM_ArenaInvalidPairingCancelsSeries(mm_arena_state_t::MatchCountdown));
	MM_CHECK(MM_ArenaInvalidPairingCancelsSeries(mm_arena_state_t::RoundCountdown));
	MM_CHECK(MM_ArenaInvalidPairingCancelsSeries(mm_arena_state_t::RoundOver));
	MM_CHECK_FALSE(MM_ArenaInvalidPairingCancelsSeries(mm_arena_state_t::Warmup));
}

MM_TEST(arena_menu_pagination_clamps_pages_and_keeps_every_item_reachable) {
	const auto empty = MM_ArenaPageRange(0, 20, 10);
	MM_CHECK_EQ(empty.page, 0);
	MM_CHECK_EQ(empty.page_count, 1);
	MM_CHECK_EQ(empty.first, 0);
	MM_CHECK_EQ(empty.last, 0);

	const auto first = MM_ArenaPageRange(25, -4, 10);
	MM_CHECK_EQ(first.page, 0);
	MM_CHECK_EQ(first.page_count, 3);
	MM_CHECK_EQ(first.first, 0);
	MM_CHECK_EQ(first.last, 10);

	const auto middle = MM_ArenaPageRange(25, 1, 10);
	MM_CHECK_EQ(middle.page, 1);
	MM_CHECK_EQ(middle.first, 10);
	MM_CHECK_EQ(middle.last, 20);

	const auto last = MM_ArenaPageRange(25, 99, 10);
	MM_CHECK_EQ(last.page, 2);
	MM_CHECK_EQ(last.first, 20);
	MM_CHECK_EQ(last.last, 25);
}

MM_TEST(arena_bot_room_score_prefers_a_human_who_has_nobody_to_fight) {
	const auto room = [](int id, int humans, int bots, int eligible_teams,
		bool paired, bool joinable, bool practice) {
		mm_arena_bot_room_view_t view;
		view.id = id;
		view.humans = humans;
		view.bots = bots;
		view.eligible_teams = eligible_teams;
		view.paired = paired;
		view.joinable = joinable;
		view.practice = practice;
		return view;
	};

	const auto human_waiting = room(1, 1, 0, 1, false, true, false);
	const auto human_fighting = room(2, 2, 0, 2, true, true, false);
	const auto lone_bot = room(3, 0, 1, 1, false, true, false);
	const auto deserted = room(4, 0, 0, 0, false, true, false);
	const auto locked = room(5, 1, 0, 1, false, false, false);

	MM_CHECK(MM_ArenaBotRoomScore(human_waiting) > MM_ArenaBotRoomScore(human_fighting));
	MM_CHECK(MM_ArenaBotRoomScore(human_fighting) > MM_ArenaBotRoomScore(lone_bot));
	MM_CHECK(MM_ArenaBotRoomScore(lone_bot) > MM_ArenaBotRoomScore(deserted));
	MM_CHECK_EQ(MM_ArenaBotRoomScore(locked), -1);

	// Crowding only reorders rooms inside a band; it can never beat a better band.
	const auto crowded_human_room = room(6, 1, 40, 1, false, true, false);
	MM_CHECK(MM_ArenaBotRoomScore(crowded_human_room) > MM_ArenaBotRoomScore(human_fighting));
	MM_CHECK(MM_ArenaBotRoomScore(human_waiting) > MM_ArenaBotRoomScore(crowded_human_room));

	// A practice room with a human in it still outranks an empty one.
	MM_CHECK(MM_ArenaBotRoomScore(room(7, 1, 0, 0, false, true, true)) >
		MM_ArenaBotRoomScore(room(8, 0, 0, 0, false, true, true)));
}

MM_TEST(arena_bot_room_stickiness_absorbs_band_noise_without_crossing_a_band) {
	// Within a band, staying put wins: a bot should not hop between two comparable rooms just
	// because their populations drift. Crowding is capped at 99, so the bonus covers exactly
	// that much noise.
	mm_arena_bot_room_view_t here;
	here.id = 5;
	here.humans = 1;
	here.bots = 99;
	here.paired = true;
	here.joinable = true;
	here.already_here = true;

	mm_arena_bot_room_view_t peer;
	peer.id = 1;
	peer.humans = 1;
	peer.bots = 0;
	peer.paired = true;
	peer.joinable = true;

	MM_CHECK(MM_ArenaBotRoomScore(here) > MM_ArenaBotRoomScore(peer));
	MM_CHECK_FALSE(MM_ArenaBotPrefersRoom(peer, here));

	// It must never carry a room across a band: a bot idling in a deserted room still leaves for
	// a human who has nobody to fight.
	mm_arena_bot_room_view_t idle;
	idle.id = 3;
	idle.joinable = true;
	idle.already_here = true;

	mm_arena_bot_room_view_t human_waiting;
	human_waiting.id = 9;
	human_waiting.humans = 1;
	human_waiting.paired = false;
	human_waiting.joinable = true;

	MM_CHECK(MM_ArenaBotRoomScore(human_waiting) > MM_ArenaBotRoomScore(idle));
	MM_CHECK(MM_ArenaBotPrefersRoom(human_waiting, idle));

	// Nor can it keep a bot in a room it may no longer enter.
	here.joinable = false;
	MM_CHECK_EQ(MM_ArenaBotRoomScore(here), -1);
	MM_CHECK(MM_ArenaBotPrefersRoom(peer, here));
}

MM_TEST(arena_rover_warmup_accepts_a_roster_parked_on_one_side) {
	// Red Rover reshuffles one pool across both fixed sides at round start, so a roster sitting
	// entirely on Red is a startable room, not a player shortage. The caller supplies the rover
	// pairing rule; this pins the balance side of it.
	mm_arena_warmup_inputs_t in;
	in.paired = true;
	in.red_size = 3;
	in.blue_size = 0;
	in.eligible_fighters = 3;
	in.allow_unbalanced = true;
	in.min_players = 2;

	const auto status = MM_ArenaWarmupStatus(in);
	MM_CHECK(status.ready_to_start);
	MM_CHECK(status.requisite == mm_arena_warmup_req_t::None);

	// The same lopsided roster in a room that does NOT allow unbalanced sides is still a balance
	// problem, so the rover exemption cannot leak into Clan Arena.
	in.allow_unbalanced = false;
	MM_CHECK(MM_ArenaWarmupStatus(in).requisite == mm_arena_warmup_req_t::Balance);
	MM_CHECK_FALSE(MM_ArenaWarmupStatus(in).ready_to_start);
}

MM_TEST(arena_layout_budget_reserves_the_measured_footer_not_a_constant) {
	// The arena footer is richer than the shared deathmatch one and its length varies by room,
	// so the body is budgeted against the footer actually composed.
	MM_CHECK(MM_ArenaLayoutCanAppend(0, 800, 1024, 224));
	MM_CHECK(MM_ArenaLayoutCanAppend(0, 224, 1024, 800));

	// Exact boundary: the last byte that still leaves the reserve intact, and the first that
	// does not.
	MM_CHECK(MM_ArenaLayoutCanAppend(0, 1024 - 224, 1024, 224));
	MM_CHECK_FALSE(MM_ArenaLayoutCanAppend(0, 1024 - 223, 1024, 224));
	MM_CHECK(MM_ArenaLayoutCanAppend(700, 100, 1024, 224));
	MM_CHECK_FALSE(MM_ArenaLayoutCanAppend(700, 101, 1024, 224));

	// A body already past the reserve boundary refuses everything, including a zero-length
	// append, rather than wrapping around on unsigned arithmetic.
	MM_CHECK_FALSE(MM_ArenaLayoutCanAppend(900, 0, 1024, 224));
	MM_CHECK_FALSE(MM_ArenaLayoutCanAppend(0, 0, 224, 224));
	MM_CHECK_FALSE(MM_ArenaLayoutCanAppend(0, 0, 100, 224));

	// A zero reserve degenerates to a plain capacity check.
	MM_CHECK(MM_ArenaLayoutCanAppend(0, 1024, 1024, 0));
	MM_CHECK_FALSE(MM_ArenaLayoutCanAppend(1, 1024, 1024, 0));
}

MM_TEST(arena_bot_room_choice_breaks_ties_on_the_lowest_room_id) {
	mm_arena_bot_room_view_t low;
	low.id = 2;
	low.humans = 1;
	low.joinable = true;

	mm_arena_bot_room_view_t high = low;
	high.id = 9;

	MM_CHECK(MM_ArenaBotPrefersRoom(low, high));
	MM_CHECK_FALSE(MM_ArenaBotPrefersRoom(high, low));
	// Strictly irreflexive, so the caller's "first or better" scan is a total order.
	MM_CHECK_FALSE(MM_ArenaBotPrefersRoom(low, low));

	// A better score wins regardless of id ordering.
	mm_arena_bot_room_view_t unpaired_human = high;
	unpaired_human.paired = false;
	mm_arena_bot_room_view_t paired_human = low;
	paired_human.paired = true;
	MM_CHECK(MM_ArenaBotPrefersRoom(unpaired_human, paired_human));
}

MM_TEST(arena_bot_team_action_founds_an_opposing_side_for_a_waiting_human) {
	mm_arena_bot_team_view_t view;

	// A lone human with a team of their own: create the other side rather than join theirs.
	view.lone_human_team = true;
	MM_CHECK(MM_ArenaBotTeamAction(view) == mm_arena_bot_team_action_t::CreateNew);

	// ...unless the room forbids new teams, in which case filling a side beats sitting out.
	view.can_create = false;
	view.has_join_target = true;
	MM_CHECK(MM_ArenaBotTeamAction(view) == mm_arena_bot_team_action_t::JoinOpposing);

	// With more than one team present the bot fills whichever side the caller nominated.
	view = {};
	view.has_join_target = true;
	MM_CHECK(MM_ArenaBotTeamAction(view) == mm_arena_bot_team_action_t::JoinOpposing);

	// No team has room: found one.
	view.has_join_target = false;
	MM_CHECK(MM_ArenaBotTeamAction(view) == mm_arena_bot_team_action_t::CreateNew);

	// Nowhere to go and nothing to create: do nothing rather than thrash.
	view.can_create = false;
	MM_CHECK(MM_ArenaBotTeamAction(view) == mm_arena_bot_team_action_t::None);

	// A settled roster is never churned.
	view = {};
	view.has_own_team = true;
	view.has_join_target = true;
	MM_CHECK(MM_ArenaBotTeamAction(view) == mm_arena_bot_team_action_t::Stay);

	// Fixed-team rooms pick a side and never create.
	view = {};
	view.room_uses_fixed = true;
	view.has_join_target = true;
	MM_CHECK(MM_ArenaBotTeamAction(view) == mm_arena_bot_team_action_t::JoinOpposing);
	view.has_own_team = true;
	MM_CHECK(MM_ArenaBotTeamAction(view) == mm_arena_bot_team_action_t::Stay);

	// Practice auto-enrols, so there is nothing for the bot to decide.
	view = {};
	view.room_uses_teams = false;
	view.lone_human_team = true;
	view.has_join_target = true;
	MM_CHECK(MM_ArenaBotTeamAction(view) == mm_arena_bot_team_action_t::None);
}

MM_TEST(arena_warmup_reports_players_before_balance_before_readyup) {
	mm_arena_warmup_inputs_t in;
	in.min_players = 2;

	// No pairing at all: the room needs bodies.
	MM_CHECK(MM_ArenaWarmupStatus(in).requisite == mm_arena_warmup_req_t::MorePlayers);
	MM_CHECK_FALSE(MM_ArenaWarmupStatus(in).ready_to_start);

	// Paired but below the server's minimum is still a player shortage.
	in.paired = true;
	in.red_size = 1;
	in.blue_size = 1;
	in.eligible_fighters = 2;
	in.min_players = 4;
	MM_CHECK(MM_ArenaWarmupStatus(in).requisite == mm_arena_warmup_req_t::MorePlayers);

	// Uneven sides are a balance problem, not a player shortage.
	in.min_players = 2;
	in.blue_size = 2;
	in.eligible_fighters = 3;
	MM_CHECK(MM_ArenaWarmupStatus(in).requisite == mm_arena_warmup_req_t::Balance);

	// ...but not when the room explicitly allows unbalanced sides.
	in.allow_unbalanced = true;
	MM_CHECK(MM_ArenaWarmupStatus(in).requisite == mm_arena_warmup_req_t::None);
	MM_CHECK(MM_ArenaWarmupStatus(in).ready_to_start);

	// Everything satisfied and no ready-up required: start immediately.
	in.allow_unbalanced = false;
	in.blue_size = 1;
	in.eligible_fighters = 2;
	MM_CHECK(MM_ArenaWarmupStatus(in).ready_to_start);
	MM_CHECK(MM_ArenaWarmupStatus(in).requisite == mm_arena_warmup_req_t::None);

	// Ready-up required and nobody ready: hold, and say so.
	in.readyup_required = true;
	in.playing_humans = 2;
	in.ready_humans = 0;
	MM_CHECK_FALSE(MM_ArenaWarmupStatus(in).ready_to_start);
	MM_CHECK(MM_ArenaWarmupStatus(in).requisite == mm_arena_warmup_req_t::ReadyUp);
}

MM_TEST(arena_warmup_readyup_uses_a_clamped_ready_percentile) {
	mm_arena_warmup_inputs_t in;
	in.paired = true;
	in.red_size = 1;
	in.blue_size = 1;
	in.eligible_fighters = 2;
	in.readyup_required = true;
	in.playing_humans = 2;
	in.ready_humans = 1;

	in.ready_percentage = 0.51f;
	MM_CHECK_FALSE(MM_ArenaWarmupStatus(in).ready_to_start);
	in.ready_humans = 2;
	MM_CHECK(MM_ArenaWarmupStatus(in).ready_to_start);

	// Out-of-range cvar values clamp rather than producing nonsense.
	in.ready_humans = 1;
	in.ready_percentage = -1.0f;
	MM_CHECK(MM_ArenaWarmupStatus(in).ready_to_start);
	in.ready_percentage = 2.0f;
	MM_CHECK_FALSE(MM_ArenaWarmupStatus(in).ready_to_start);

	// A zero percentile still requires at least one ready human, matching native warmup.
	in.ready_humans = 0;
	in.ready_percentage = 0.0f;
	MM_CHECK_FALSE(MM_ArenaWarmupStatus(in).ready_to_start);
}

MM_TEST(arena_warmup_runs_a_bots_only_room_only_where_humans_are_optional) {
	mm_arena_warmup_inputs_t in;
	in.paired = true;
	in.red_size = 1;
	in.blue_size = 1;
	in.eligible_fighters = 2;
	in.readyup_required = true;
	in.playing_humans = 0;
	in.playing_bots = 2;

	in.allow_no_humans = true;
	MM_CHECK(MM_ArenaWarmupStatus(in).ready_to_start);

	in.allow_no_humans = false;
	MM_CHECK_FALSE(MM_ArenaWarmupStatus(in).ready_to_start);
	MM_CHECK(MM_ArenaWarmupStatus(in).requisite == mm_arena_warmup_req_t::MorePlayers);

	// An empty room never starts, however permissive the server is.
	in.allow_no_humans = true;
	in.playing_bots = 0;
	MM_CHECK_FALSE(MM_ArenaWarmupStatus(in).ready_to_start);
}

MM_TEST(arena_countdown_beeps_on_tens_and_every_second_under_ten) {
	MM_CHECK(MM_ArenaCountdownBeepDue(30, 0));
	MM_CHECK(MM_ArenaCountdownBeepDue(20, 30));
	MM_CHECK(MM_ArenaCountdownBeepDue(10, 20));
	MM_CHECK(MM_ArenaCountdownBeepDue(9, 10));
	MM_CHECK(MM_ArenaCountdownBeepDue(1, 2));

	MM_CHECK_FALSE(MM_ArenaCountdownBeepDue(29, 30));
	MM_CHECK_FALSE(MM_ArenaCountdownBeepDue(11, 20));

	// The latch is monotonic: never re-announce a second already spoken.
	MM_CHECK_FALSE(MM_ArenaCountdownBeepDue(9, 9));
	MM_CHECK_FALSE(MM_ArenaCountdownBeepDue(9, 3));

	MM_CHECK_FALSE(MM_ArenaCountdownBeepDue(0, 1));
	MM_CHECK_FALSE(MM_ArenaCountdownBeepDue(-1, 1));
}

MM_TEST(arena_scoreboard_waiting_header_clears_the_tallest_player_column) {
	MM_CHECK_EQ(MM_ArenaScoreboardRowY(MM_ARENA_SB_FIGHTER_TOP, 0), MM_ARENA_SB_FIGHTER_TOP);
	MM_CHECK_EQ(MM_ArenaScoreboardRowY(MM_ARENA_SB_FIGHTER_TOP, 3),
		MM_ARENA_SB_FIGHTER_TOP + 3 * MM_ARENA_SB_ROW_STRIDE);

	// The header always sits at least one full row below the last drawn player row.
	for (int red = 0; red <= MM_ARENA_SB_MAX_ROWS_PER_SIDE; red++) {
		for (int blue = 0; blue <= MM_ARENA_SB_MAX_ROWS_PER_SIDE; blue++) {
			const int tallest = red > blue ? red : blue;
			const int last_row_y = MM_ArenaScoreboardRowY(MM_ARENA_SB_FIGHTER_TOP,
				tallest > 0 ? tallest - 1 : 0);
			MM_CHECK(MM_ArenaScoreboardWaitingHeaderY(red, blue) >=
				last_row_y + MM_ARENA_SB_ROW_STRIDE);
		}
	}

	// Asymmetric columns are cleared by the taller of the two, not the first.
	MM_CHECK_EQ(MM_ArenaScoreboardWaitingHeaderY(0, 6), MM_ArenaScoreboardWaitingHeaderY(6, 0));
}

MM_TEST(arena_scoreboard_ready_markers_track_the_rooms_own_warmup) {
	MM_CHECK(MM_ArenaScoreboardShowsReadyMarkers(mm_arena_state_t::Warmup, true));
	MM_CHECK(MM_ArenaScoreboardShowsReadyMarkers(mm_arena_state_t::Empty, true));

	MM_CHECK_FALSE(MM_ArenaScoreboardShowsReadyMarkers(mm_arena_state_t::MatchCountdown, true));
	MM_CHECK_FALSE(MM_ArenaScoreboardShowsReadyMarkers(mm_arena_state_t::RoundCountdown, true));
	MM_CHECK_FALSE(MM_ArenaScoreboardShowsReadyMarkers(mm_arena_state_t::Running, true));
	MM_CHECK_FALSE(MM_ArenaScoreboardShowsReadyMarkers(mm_arena_state_t::RoundOver, true));
	MM_CHECK_FALSE(MM_ArenaScoreboardShowsReadyMarkers(mm_arena_state_t::MatchOver, true));
	MM_CHECK_FALSE(MM_ArenaScoreboardShowsReadyMarkers(mm_arena_state_t::Paused, true));

	// Rooms that do not use ready-up never draw the marker column.
	MM_CHECK_FALSE(MM_ArenaScoreboardShowsReadyMarkers(mm_arena_state_t::Warmup, false));
	MM_CHECK_FALSE(MM_ArenaScoreboardShowsReadyMarkers(mm_arena_state_t::Empty, false));
}

MM_TEST(arena_queue_order_uses_team_id_as_a_stable_tiebreaker) {
	MM_CHECK(MM_ArenaQueueKeyPrecedes(1, 250, 2, 1));
	MM_CHECK_FALSE(MM_ArenaQueueKeyPrecedes(2, 1, 1, 250));
	MM_CHECK(MM_ArenaQueueKeyPrecedes(7, 2, 7, 3));
	MM_CHECK_FALSE(MM_ArenaQueueKeyPrecedes(7, 3, 7, 2));
	MM_CHECK_FALSE(MM_ArenaQueueKeyPrecedes(7, 2, 7, 2));
}

MM_TEST(arena_map_contract_profiles_tagged_multi_and_legacy_idmap) {
	mm_arena_map_contract_t tagged;
	tagged.syntax_valid = true;
	tagged.first_entity_is_worldspawn = true;
	tagged.world_arena_key_count = 1;
	tagged.world_arena_value_valid = true;
	tagged.declared_rooms = 2;
	tagged.has_lobby_point = true;
	// Both active sides respawn before the countdown. Named intermissions are
	// optional, but each room needs two usable fighter-start entities.
	tagged.fighter_starts[1] = 2;
	tagged.fighter_starts[2] = 2;

	const auto tagged_validation = MM_ArenaValidateMapContract(tagged);
	MM_CHECK(static_cast<bool>(tagged_validation));
	MM_CHECK_EQ(tagged_validation.profile,
		mm_arena_map_profile_t::TaggedMulti);
	MM_CHECK(MM_ArenaMapRoomDeclared(tagged_validation.profile,
		tagged.declared_rooms, 1));
	MM_CHECK(MM_ArenaMapRoomDeclared(tagged_validation.profile,
		tagged.declared_rooms, 2));
	MM_CHECK_FALSE(MM_ArenaMapRoomDeclared(tagged_validation.profile,
		tagged.declared_rooms, 0));
	MM_CHECK_FALSE(MM_ArenaMapRoomDeclared(tagged_validation.profile,
		tagged.declared_rooms, 3));

	auto invalid = tagged;
	invalid.syntax_valid = false;
	MM_CHECK_EQ(MM_ArenaValidateMapContract(invalid).error,
		mm_arena_map_error_t::MalformedEntityLump);

	invalid = tagged;
	invalid.first_entity_is_worldspawn = false;
	MM_CHECK_EQ(MM_ArenaValidateMapContract(invalid).error,
		mm_arena_map_error_t::MissingWorldspawn);

	invalid = tagged;
	invalid.world_arena_key_count = 0;
	MM_CHECK_EQ(MM_ArenaValidateMapContract(invalid).error,
		mm_arena_map_error_t::MissingArenaKey);

	invalid = tagged;
	invalid.world_arena_key_count = 2;
	MM_CHECK_EQ(MM_ArenaValidateMapContract(invalid).error,
		mm_arena_map_error_t::DuplicateArenaKey);

	invalid = tagged;
	invalid.declared_rooms = MM_ARENA_MAP_MAX_ROOMS + 1;
	MM_CHECK_EQ(MM_ArenaValidateMapContract(invalid).error,
		mm_arena_map_error_t::InvalidArenaCount);

	invalid = tagged;
	invalid.has_lobby_point = false;
	MM_CHECK_EQ(MM_ArenaValidateMapContract(invalid).error,
		mm_arena_map_error_t::MissingLobbyPoint);

	invalid = tagged;
	invalid.fighter_starts[2] = 1;
	const auto missing_starts = MM_ArenaValidateMapContract(invalid);
	MM_CHECK_EQ(missing_starts.error,
		mm_arena_map_error_t::MissingFighterStarts);
	MM_CHECK_EQ(missing_starts.room, 2);

	auto legacy = tagged;
	legacy.declared_rooms = 0;
	legacy.has_lobby_point = false;
	legacy.idmap_fighter_starts = 2;
	MM_CHECK(MM_ArenaMapUsesExplicitIdmap(legacy));
	const auto legacy_validation = MM_ArenaValidateMapContract(legacy);
	MM_CHECK(static_cast<bool>(legacy_validation));
	MM_CHECK_EQ(legacy_validation.profile,
		mm_arena_map_profile_t::LegacyIdmap);
	MM_CHECK_FALSE(MM_ArenaMapRoomDeclared(legacy_validation.profile,
		legacy.declared_rooms, 1));

	invalid = legacy;
	invalid.idmap_fighter_starts = 1;
	const auto missing_idmap_start = MM_ArenaValidateMapContract(invalid);
	MM_CHECK_EQ(missing_idmap_start.error,
		mm_arena_map_error_t::MissingFighterStarts);
	MM_CHECK_EQ(missing_idmap_start.room, 1);
}

MM_TEST(arena_map_contract_opt_in_allows_legacy_idmaps_without_a_world_key) {
	mm_arena_map_contract_t contract;
	contract.syntax_valid = true;
	contract.first_entity_is_worldspawn = true;
	contract.idmap_fighter_starts = 2;

	MM_CHECK_EQ(MM_ArenaValidateMapContract(contract).error,
		mm_arena_map_error_t::MissingArenaKey);
	const auto opted_in = MM_ArenaValidateMapContract(contract, true);
	MM_CHECK(static_cast<bool>(opted_in));
	MM_CHECK_EQ(opted_in.profile, mm_arena_map_profile_t::LegacyIdmap);
}

MM_TEST(arena_effective_gametype_fails_closed_until_map_validation) {
	constexpr int ffa = 1;
	constexpr int duel = 2;
	constexpr int arena = 14;

	MM_CHECK_EQ(MM_ArenaEffectiveGametype(arena, arena, ffa, false), ffa);
	MM_CHECK_EQ(MM_ArenaEffectiveGametype(arena, arena, ffa, true), arena);
	MM_CHECK_EQ(MM_ArenaEffectiveGametype(duel, arena, ffa, false), duel);
	MM_CHECK_EQ(MM_ArenaEffectiveGametype(duel, arena, ffa, true), duel);
}

MM_TEST(arena_map_contract_rejects_out_of_range_or_malformed_world_counts) {
	mm_arena_map_contract_t contract;
	contract.syntax_valid = true;
	contract.first_entity_is_worldspawn = true;
	contract.world_arena_key_count = 1;
	contract.has_lobby_point = true;

	for (const int invalid_count : {
		std::numeric_limits<int>::min(), -1,
		MM_ARENA_MAP_MAX_ROOMS + 1, std::numeric_limits<int>::max()
	}) {
		contract.world_arena_value_valid = true;
		contract.declared_rooms = invalid_count;
		MM_CHECK_EQ(MM_ArenaValidateMapContract(contract).error,
			mm_arena_map_error_t::InvalidArenaCount);
	}

	contract.world_arena_value_valid = false;
	contract.declared_rooms = 1;
	MM_CHECK_EQ(MM_ArenaValidateMapContract(contract).error,
		mm_arena_map_error_t::InvalidArenaCount);
}

MM_TEST(arena_best_of_is_odd_bounded_and_has_a_majority_threshold) {
	MM_CHECK_EQ(MM_ArenaNormalizeBestOf(std::numeric_limits<int>::min()), 1);
	MM_CHECK_EQ(MM_ArenaNormalizeBestOf(0), 1);
	MM_CHECK_EQ(MM_ArenaNormalizeBestOf(1), 1);
	MM_CHECK_EQ(MM_ArenaNormalizeBestOf(2), 3);
	MM_CHECK_EQ(MM_ArenaNormalizeBestOf(3), 3);
	MM_CHECK_EQ(MM_ArenaNormalizeBestOf(10), 11);
	MM_CHECK_EQ(MM_ArenaNormalizeBestOf(98), 99);
	MM_CHECK_EQ(MM_ArenaNormalizeBestOf(99), 99);
	MM_CHECK_EQ(MM_ArenaNormalizeBestOf(std::numeric_limits<int>::max()), 99);

	MM_CHECK_EQ(MM_ArenaWinsNeeded(1), 1);
	MM_CHECK_EQ(MM_ArenaWinsNeeded(2), 2);
	MM_CHECK_EQ(MM_ArenaWinsNeeded(3), 2);
	MM_CHECK_EQ(MM_ArenaWinsNeeded(5), 3);
	MM_CHECK_EQ(MM_ArenaWinsNeeded(99), 50);
	MM_CHECK_EQ(MM_ArenaWinsNeeded(100), 50);
}

MM_TEST(arena_round_resolution_preserves_mutual_wipes_as_replay_draws) {
	MM_CHECK_EQ(MM_ArenaResolveRound(0, 0, false), mm_arena_round_result_t::Draw);
	MM_CHECK_EQ(MM_ArenaResolveRound(0, 0, true), mm_arena_round_result_t::Draw);
	MM_CHECK_EQ(MM_ArenaResolveRound(-1, -1, false), mm_arena_round_result_t::Draw);

	MM_CHECK_EQ(MM_ArenaResolveRound(1, 0, false), mm_arena_round_result_t::Red);
	MM_CHECK_EQ(MM_ArenaResolveRound(0, 1, false), mm_arena_round_result_t::Blue);
	MM_CHECK_EQ(MM_ArenaResolveRound(4, -1, true), mm_arena_round_result_t::Red);
	MM_CHECK_EQ(MM_ArenaResolveRound(-1, 4, true), mm_arena_round_result_t::Blue);

	MM_CHECK_EQ(MM_ArenaResolveRound(1, 1, false), mm_arena_round_result_t::Ongoing);
	MM_CHECK_EQ(MM_ArenaResolveRound(4, 2, false), mm_arena_round_result_t::Ongoing);
	MM_CHECK_EQ(MM_ArenaResolveRound(1, 1, true), mm_arena_round_result_t::Draw);
	MM_CHECK_EQ(MM_ArenaResolveRound(4, 2, true), mm_arena_round_result_t::Draw);
}

MM_TEST(arena_series_winner_requires_the_normalized_best_of_majority) {
	MM_CHECK_EQ(MM_ArenaSeriesWinner(0, 0, 5), mm_arena_round_result_t::Ongoing);
	MM_CHECK_EQ(MM_ArenaSeriesWinner(2, 2, 5), mm_arena_round_result_t::Ongoing);
	MM_CHECK_EQ(MM_ArenaSeriesWinner(3, 2, 5), mm_arena_round_result_t::Red);
	MM_CHECK_EQ(MM_ArenaSeriesWinner(2, 3, 5), mm_arena_round_result_t::Blue);
	MM_CHECK_EQ(MM_ArenaSeriesWinner(2, 0, 2), mm_arena_round_result_t::Red);
	MM_CHECK_EQ(MM_ArenaSeriesWinner(0, 50, 100), mm_arena_round_result_t::Blue);
	MM_CHECK_EQ(MM_ArenaSeriesWinner(3, 3, 5), mm_arena_round_result_t::Draw);
}

MM_TEST(arena_team_eligibility_treats_players_per_team_as_a_cap) {
	MM_CHECK(MM_ArenaTeamEligible(1, 1));
	MM_CHECK(MM_ArenaTeamEligible(3, 3));
	MM_CHECK(MM_ArenaTeamEligible(1, 3));
	MM_CHECK(MM_ArenaTeamEligible(2, 3));
	MM_CHECK_FALSE(MM_ArenaTeamEligible(0, 3));
	MM_CHECK_FALSE(MM_ArenaTeamEligible(-1, 3));
	MM_CHECK_FALSE(MM_ArenaTeamEligible(4, 3));
	MM_CHECK_FALSE(MM_ArenaTeamEligible(1, 0));
	MM_CHECK_FALSE(MM_ArenaTeamEligible(1, -1));
}

MM_TEST(arena_fixed_clan_and_rover_teams_have_unlimited_size) {
	MM_CHECK(MM_ArenaTeamSizeIsUnlimited(mm_arena_type_t::ClanArena));
	MM_CHECK(MM_ArenaTeamSizeIsUnlimited(mm_arena_type_t::RedRover));
	MM_CHECK_FALSE(MM_ArenaTeamSizeIsUnlimited(
		mm_arena_type_t::RocketArena));
	MM_CHECK_FALSE(MM_ArenaTeamSizeIsUnlimited(mm_arena_type_t::Practice));

	MM_CHECK(MM_ArenaTeamEligibleForType(
		mm_arena_type_t::ClanArena, static_cast<int>(MAX_LOBBY_PLAYERS), 1));
	MM_CHECK(MM_ArenaTeamEligibleForType(
		mm_arena_type_t::RedRover, static_cast<int>(MAX_LOBBY_PLAYERS), 1));
	MM_CHECK_FALSE(MM_ArenaTeamEligibleForType(
		mm_arena_type_t::ClanArena, 0, 1));
	MM_CHECK_FALSE(MM_ArenaTeamEligibleForType(
		mm_arena_type_t::RocketArena, 2, 1));
	MM_CHECK(MM_ArenaTeamEligibleForType(
		mm_arena_type_t::RocketArena, 1, 1));
}

MM_TEST(arena_team_transfer_protects_the_empty_destination_from_cleanup) {
	MM_CHECK(MM_ArenaShouldDestroyEmptyTeam(0, false));
	MM_CHECK_FALSE(MM_ArenaShouldDestroyEmptyTeam(0, true));
	MM_CHECK_FALSE(MM_ArenaShouldDestroyEmptyTeam(1, false));
	MM_CHECK_FALSE(MM_ArenaShouldDestroyEmptyTeam(1, true));
}

MM_TEST(arena_protection_modes_keep_self_damage_separate_from_team_damage) {
	MM_CHECK_FALSE(MM_ArenaProtectionBlocks(mm_arena_protection_t::None, true, true));
	MM_CHECK_FALSE(MM_ArenaProtectionBlocks(mm_arena_protection_t::None, true, false));
	MM_CHECK(MM_ArenaProtectionBlocks(mm_arena_protection_t::SelfAndTeam, true, true));
	MM_CHECK(MM_ArenaProtectionBlocks(mm_arena_protection_t::SelfAndTeam, true, false));
	MM_CHECK_FALSE(MM_ArenaProtectionBlocks(mm_arena_protection_t::Team, true, true));
	MM_CHECK(MM_ArenaProtectionBlocks(mm_arena_protection_t::Team, true, false));
	MM_CHECK_FALSE(MM_ArenaProtectionBlocks(mm_arena_protection_t::SelfAndTeam, false, false));
}

MM_TEST(arena_types_preserve_ra3_team_queue_and_practice_semantics) {
	MM_CHECK(MM_ArenaWinnerStays(mm_arena_type_t::RocketArena));
	MM_CHECK_FALSE(MM_ArenaWinnerStays(mm_arena_type_t::ClanArena));
	MM_CHECK(MM_ArenaUsesFixedTeams(mm_arena_type_t::ClanArena));
	MM_CHECK(MM_ArenaUsesFixedTeams(mm_arena_type_t::RedRover));
	MM_CHECK_FALSE(MM_ArenaUsesFixedTeams(mm_arena_type_t::RocketArena));
	MM_CHECK(MM_ArenaRoundEliminates(mm_arena_type_t::RocketArena));
	MM_CHECK_FALSE(MM_ArenaRoundEliminates(mm_arena_type_t::Practice));
	MM_CHECK_FALSE(MM_ArenaUsesLogicalTeams(mm_arena_type_t::Practice));
	MM_CHECK(MM_ArenaUsesLogicalTeams(mm_arena_type_t::RocketArena));
	MM_CHECK(MM_ArenaShouldAutoEnrollPractice(
		mm_arena_type_t::Practice, mm_arena_role_t::Observer));
	MM_CHECK(MM_ArenaShouldAutoEnrollPractice(
		mm_arena_type_t::Practice, mm_arena_role_t::Fighter));
	MM_CHECK_FALSE(MM_ArenaShouldAutoEnrollPractice(
		mm_arena_type_t::Practice, mm_arena_role_t::Coach));
	MM_CHECK_FALSE(MM_ArenaShouldAutoEnrollPractice(
		mm_arena_type_t::RocketArena, mm_arena_role_t::Observer));
}

MM_TEST(arena_follow_and_freecam_restrictions_are_competition_only) {
	MM_CHECK(MM_ArenaFollowAllowedByCompetition(
		false, false, false, false));
	MM_CHECK(MM_ArenaFollowAllowedByCompetition(
		false, true, false, false));
	MM_CHECK_FALSE(MM_ArenaFollowAllowedByCompetition(
		true, false, false, false));
	MM_CHECK(MM_ArenaFollowAllowedByCompetition(
		true, false, false, true));
	MM_CHECK(MM_ArenaFollowAllowedByCompetition(
		true, true, true, false));
	MM_CHECK_FALSE(MM_ArenaFollowAllowedByCompetition(
		true, true, false, true));

	MM_CHECK(MM_ArenaFreecamAllowedByCompetition(false, false));
	MM_CHECK(MM_ArenaFreecamAllowedByCompetition(true, true));
	MM_CHECK_FALSE(MM_ArenaFreecamAllowedByCompetition(true, false));
}

MM_TEST(arena_live_roster_lock_only_applies_during_fight_and_timeout) {
	MM_CHECK(MM_ArenaFighterRosterLocked(mm_arena_type_t::RocketArena,
		mm_arena_state_t::Running, true, true));
	MM_CHECK(MM_ArenaFighterRosterLocked(mm_arena_type_t::ClanArena,
		mm_arena_state_t::Paused, true, true));
	MM_CHECK_FALSE(MM_ArenaFighterRosterLocked(mm_arena_type_t::RocketArena,
		mm_arena_state_t::MatchCountdown, true, true));
	MM_CHECK_FALSE(MM_ArenaFighterRosterLocked(mm_arena_type_t::RocketArena,
		mm_arena_state_t::RoundOver, true, true));
	MM_CHECK_FALSE(MM_ArenaFighterRosterLocked(mm_arena_type_t::RocketArena,
		mm_arena_state_t::Running, true, false));
	MM_CHECK_FALSE(MM_ArenaFighterRosterLocked(mm_arena_type_t::Practice,
		mm_arena_state_t::Running, true, true));
}

MM_TEST(arena_competition_commands_allow_only_explicit_admin_overrides) {
	MM_CHECK(MM_ArenaCompetitionCommandAllowed(true, false, false));
	MM_CHECK(MM_ArenaCompetitionCommandAllowed(true, true, false));
	MM_CHECK_FALSE(MM_ArenaCompetitionCommandAllowed(false, false, false));
	MM_CHECK_FALSE(MM_ArenaCompetitionCommandAllowed(false, true, false));
	MM_CHECK(MM_ArenaCompetitionCommandAllowed(false, true, true));

	bool ready = false;
	MM_CHECK(MM_ArenaReadyCommandValue({}, false, ready));
	MM_CHECK(ready);
	MM_CHECK(MM_ArenaReadyCommandValue({}, true, ready));
	MM_CHECK_FALSE(ready);
	MM_CHECK(MM_ArenaReadyCommandValue("1", false, ready));
	MM_CHECK(ready);
	MM_CHECK(MM_ArenaReadyCommandValue("0", true, ready));
	MM_CHECK_FALSE(ready);
	MM_CHECK_FALSE(MM_ArenaReadyCommandValue("yes", false, ready));
}

MM_TEST(arena_team_lock_command_arity_is_exact) {
	MM_CHECK(MM_ArenaTeamLockArgumentsValid(true, 2));
	MM_CHECK(MM_ArenaTeamLockArgumentsValid(true, 3));
	MM_CHECK_FALSE(MM_ArenaTeamLockArgumentsValid(true, 1));
	MM_CHECK_FALSE(MM_ArenaTeamLockArgumentsValid(true, 4));

	MM_CHECK(MM_ArenaTeamLockArgumentsValid(false, 2));
	MM_CHECK_FALSE(MM_ArenaTeamLockArgumentsValid(false, 1));
	MM_CHECK_FALSE(MM_ArenaTeamLockArgumentsValid(false, 3));
}

MM_TEST(arena_timeout_owner_team_controls_timein_for_active_team_members) {
	MM_CHECK(MM_ArenaCanCallTimeout(
		mm_arena_state_t::Running, true, true));
	MM_CHECK_FALSE(MM_ArenaCanCallTimeout(
		mm_arena_state_t::MatchCountdown, true, true));
	MM_CHECK_FALSE(MM_ArenaCanCallTimeout(
		mm_arena_state_t::Running, false, true));
	MM_CHECK_FALSE(MM_ArenaCanCallTimeout(
		mm_arena_state_t::Running, true, false));

	MM_CHECK(MM_ArenaCanCallTimein(mm_arena_state_t::Paused,
		true, true, false, 7, 7));
	MM_CHECK_FALSE(MM_ArenaCanCallTimein(mm_arena_state_t::Paused,
		true, true, false, 8, 7));
	MM_CHECK_FALSE(MM_ArenaCanCallTimein(mm_arena_state_t::Paused,
		true, true, true, 7, 7));
	MM_CHECK_FALSE(MM_ArenaCanCallTimein(mm_arena_state_t::Paused,
		true, false, false, 7, 7));
}

MM_TEST(arena_deferred_proposals_layer_without_discarding_prior_changes) {
	mm_arena_settings_t current;
	current.health = 100;
	current.rounds = 1;
	mm_arena_settings_t pending = current;
	pending.health = 200;

	const mm_arena_settings_t layered =
		MM_ArenaProposalBase(current, pending, true);
	MM_CHECK_EQ(layered.health, 200);
	MM_CHECK_EQ(layered.rounds, 1);
	const mm_arena_settings_t immediate =
		MM_ArenaProposalBase(current, pending, false);
	MM_CHECK_EQ(immediate.health, 100);
}

MM_TEST(arena_runtime_lifetimes_keep_room_and_team_ownership_scoped) {
	MM_CHECK_FALSE(MM_ArenaUsesGenericNoPlayersTimeout(true));
	MM_CHECK(MM_ArenaUsesGenericNoPlayersTimeout(false));

	MM_CHECK(MM_ArenaDelayedActivatorValid(
		true, true, 17, 17, 2, 2));
	MM_CHECK_FALSE(MM_ArenaDelayedActivatorValid(
		false, true, 17, 17, 2, 2));
	MM_CHECK_FALSE(MM_ArenaDelayedActivatorValid(
		true, false, 17, 17, 2, 2));
	MM_CHECK_FALSE(MM_ArenaDelayedActivatorValid(
		true, true, 17, 18, 2, 2));
	MM_CHECK_FALSE(MM_ArenaDelayedActivatorValid(
		true, true, 17, 17, 2, 3));
	MM_CHECK(MM_ArenaDelayedActivatorValid(
		true, true, 17, 17, 0, 0));
	MM_CHECK_FALSE(MM_ArenaDelayedActivatorValid(
		true, true, 17, 17, 0, 1));
	MM_CHECK(MM_ArenaDelayedActivatorValid(
		true, true, 17, 17, -1, 9));

	MM_CHECK_EQ(MM_ArenaInvitesAfterMemberTransfer(0x07, 4, 5), 0x04);
	MM_CHECK_EQ(MM_ArenaInvitesAfterMemberTransfer(0x03, 4, 0), 0x00);
	MM_CHECK_EQ(MM_ArenaInvitesAfterMemberTransfer(0x07, 4, 4), 0x07);
	MM_CHECK_EQ(MM_ArenaInvitesAfterMemberTransfer(0x07, 0, 5), 0x07);
}

MM_TEST(arena_spectator_invites_and_config_braces_follow_ra3_safety_rules) {
	MM_CHECK_FALSE(MM_ArenaSpectatorInviteCommandAllowed(false));
	MM_CHECK(MM_ArenaSpectatorInviteCommandAllowed(true));
	MM_CHECK_EQ(MM_ArenaClearSpectatorInviteBits(0x07), 0x04);
	MM_CHECK_EQ(MM_ArenaClearSpectatorInviteBits(0x03), 0x00);

	const std::array<std::string_view, 6> balanced {
		"map", "{", "arena", "{", "}", "}"
	};
	const std::array<std::string_view, 3> missing_close {
		"map", "{", "health"
	};
	const std::array<std::string_view, 2> stray_close {
		"}", "{"
	};
	MM_CHECK(MM_ArenaConfigBracesBalanced(balanced));
	MM_CHECK_FALSE(MM_ArenaConfigBracesBalanced(missing_close));
	MM_CHECK_FALSE(MM_ArenaConfigBracesBalanced(stray_close));

	// A following `key:` starts a new setting even when the previous native or
	// RA2 assignment omitted its optional terminating semicolon.
	MM_CHECK(MM_ArenaConfigStartsColonSetting(true, "health", ":"));
	MM_CHECK(MM_ArenaConfigStartsColonSetting(false, "format", ":"));
	MM_CHECK(MM_ArenaConfigStartsColonSetting(false, "maploop", ":"));
	MM_CHECK_FALSE(MM_ArenaConfigStartsColonSetting(
		false, "unknown_extension", ":"));
	MM_CHECK_FALSE(MM_ArenaConfigStartsColonSetting(true, "health", "100"));
}

MM_TEST(arena_votes_use_ra3_expiry_and_unanimous_lock_thresholds) {
	MM_CHECK_FALSE(MM_ArenaVotePassesAtExpiry(0, 0, 0));
	MM_CHECK_FALSE(MM_ArenaVotePassesAtExpiry(2, 1, 3));
	MM_CHECK(MM_ArenaVotePassesAtExpiry(2, 0, 3));
	MM_CHECK_FALSE(MM_ArenaVotePassesAtExpiry(3, 1, 6));
	MM_CHECK(MM_ArenaVotePassesAtExpiry(4, 1, 6));

	MM_CHECK_FALSE(MM_ArenaLockVotePassesAtExpiry(5, 0, 5, 6));
	MM_CHECK(MM_ArenaLockVotePassesAtExpiry(6, 0, 6, 6));
	MM_CHECK_FALSE(MM_ArenaLockVotePassesAtExpiry(5, 1, 6, 6));
	MM_CHECK_FALSE(MM_ArenaLockVotePassesAtExpiry(6, 0, 7, 6));
	MM_CHECK(MM_ArenaLockVotePassesAtExpiry(6, 0, 6, 0));
}

MM_TEST(arena_successful_votes_restore_every_players_proposal_allowance) {
	std::array<std::uint8_t, 4> tries { 2, 1, 0, 2 };
	MM_ArenaResolveVoteTries(tries, false);
	MM_CHECK_EQ(tries[0], 2);
	MM_CHECK_EQ(tries[1], 1);
	MM_CHECK_EQ(tries[2], 0);
	MM_CHECK_EQ(tries[3], 2);

	MM_ArenaResolveVoteTries(tries, true);
	for (std::uint8_t used : tries)
		MM_CHECK_EQ(used, 0);
}

MM_TEST(arena_flood_bypass_is_only_ca_competition_team_chat) {
	MM_CHECK(MM_ArenaTeamChatBypassesFlood(
		mm_arena_type_t::ClanArena, true, true));
	MM_CHECK_FALSE(MM_ArenaTeamChatBypassesFlood(
		mm_arena_type_t::ClanArena, false, true));
	MM_CHECK_FALSE(MM_ArenaTeamChatBypassesFlood(
		mm_arena_type_t::ClanArena, true, false));
	MM_CHECK_FALSE(MM_ArenaTeamChatBypassesFlood(
		mm_arena_type_t::RocketArena, true, true));
	MM_CHECK_FALSE(MM_ArenaTeamChatBypassesFlood(
		mm_arena_type_t::RedRover, true, true));
	MM_CHECK_FALSE(MM_ArenaTeamChatBypassesFlood(
		mm_arena_type_t::Practice, true, true));
	MM_CHECK(MM_ArenaTeamChatUsesArenaScope(mm_arena_type_t::Practice));
	MM_CHECK_FALSE(MM_ArenaTeamChatUsesArenaScope(
		mm_arena_type_t::RocketArena));
}

MM_TEST(arena_team_chat_separates_live_and_observing_teammates) {
	MM_CHECK(MM_ArenaTeamChatStatesMatch(true, true, false, false));
	MM_CHECK(MM_ArenaTeamChatStatesMatch(false, false, false, false));
	MM_CHECK_FALSE(MM_ArenaTeamChatStatesMatch(true, false, false, false));
	MM_CHECK_FALSE(MM_ArenaTeamChatStatesMatch(false, true, false, false));
	MM_CHECK(MM_ArenaTeamChatStatesMatch(true, false, true, false));
	MM_CHECK(MM_ArenaTeamChatStatesMatch(false, true, false, true));
}

MM_TEST(arena_red_rover_transfers_and_scores_kills_and_world_deaths) {
	const auto kill = MM_ArenaRedRoverScoreDelta(true);
	MM_CHECK_EQ(kill.killer, 1);
	MM_CHECK_EQ(kill.victim, -1);

	const auto world = MM_ArenaRedRoverScoreDelta(false);
	MM_CHECK_EQ(world.killer, 0);
	MM_CHECK_EQ(world.victim, -1);

	MM_CHECK(MM_ArenaRedRoverDestinationIsRed(false, true, true));
	MM_CHECK_FALSE(MM_ArenaRedRoverDestinationIsRed(true, true, false));
	MM_CHECK_FALSE(MM_ArenaRedRoverDestinationIsRed(true, false, false));
	MM_CHECK(MM_ArenaRedRoverDestinationIsRed(false, false, true));
}

MM_TEST(arena_weapon_mask_keeps_only_supported_bits_and_allows_melee_only) {
	constexpr std::uint32_t all_arena_weapons = 0x1ffu;

	MM_CHECK_EQ(MM_ArenaSanitizeWeaponMask(0u), 0u);
	MM_CHECK_EQ(MM_ArenaSanitizeWeaponMask(1u), 1u);
	MM_CHECK_EQ(MM_ArenaSanitizeWeaponMask(1u << 8), 1u << 8);
	MM_CHECK_EQ(MM_ArenaSanitizeWeaponMask(all_arena_weapons), all_arena_weapons);
	MM_CHECK_EQ(MM_ArenaSanitizeWeaponMask(1u << 9), 0u);
	MM_CHECK_EQ(MM_ArenaSanitizeWeaponMask(0xa5a5a5a5u), 0x1a5u);
	MM_CHECK_EQ(MM_ArenaSanitizeWeaponMask(std::numeric_limits<std::uint32_t>::max()), all_arena_weapons);
}

MM_TEST(arena_ra3_weapon_digits_and_names_map_to_q2re_roles) {
	MM_CHECK_EQ(MM_ArenaWeaponFlagForDigit('0'), 0u);
	MM_CHECK_EQ(MM_ArenaWeaponFlagForDigit('1'), MM_ARENA_WEAPON_GAUNTLET);
	MM_CHECK_EQ(MM_ArenaWeaponFlagForDigit('2'), MM_ARENA_WEAPON_MACHINEGUN);
	MM_CHECK_EQ(MM_ArenaWeaponFlagForDigit('3'), MM_ARENA_WEAPON_SHOTGUN);
	MM_CHECK_EQ(MM_ArenaWeaponFlagForDigit('4'), MM_ARENA_WEAPON_GRENADE_LAUNCHER);
	MM_CHECK_EQ(MM_ArenaWeaponFlagForDigit('5'), MM_ARENA_WEAPON_ROCKET_LAUNCHER);
	MM_CHECK_EQ(MM_ArenaWeaponFlagForDigit('6'), MM_ARENA_WEAPON_LIGHTNING);
	MM_CHECK_EQ(MM_ArenaWeaponFlagForDigit('7'), MM_ARENA_WEAPON_RAILGUN);
	MM_CHECK_EQ(MM_ArenaWeaponFlagForDigit('8'), MM_ARENA_WEAPON_PLASMA);
	MM_CHECK_EQ(MM_ArenaWeaponFlagForDigit('9'), MM_ARENA_WEAPON_BFG);

	MM_CHECK_EQ(MM_ArenaWeaponFlagForName("chainfist"), MM_ARENA_WEAPON_GAUNTLET);
	MM_CHECK_EQ(MM_ArenaWeaponFlagForName("machinegun"), MM_ARENA_WEAPON_MACHINEGUN);
	MM_CHECK_EQ(MM_ArenaWeaponFlagForName("shotgun"), MM_ARENA_WEAPON_SHOTGUN);
	MM_CHECK_EQ(MM_ArenaWeaponFlagForName("grenadelauncher"), MM_ARENA_WEAPON_GRENADE_LAUNCHER);
	MM_CHECK_EQ(MM_ArenaWeaponFlagForName("rocketlauncher"), MM_ARENA_WEAPON_ROCKET_LAUNCHER);
	MM_CHECK_EQ(MM_ArenaWeaponFlagForName("lightninggun"), MM_ARENA_WEAPON_LIGHTNING);
	MM_CHECK_EQ(MM_ArenaWeaponFlagForName("railgun"), MM_ARENA_WEAPON_RAILGUN);
	MM_CHECK_EQ(MM_ArenaWeaponFlagForName("hyperblaster"), MM_ARENA_WEAPON_PLASMA);
	MM_CHECK_EQ(MM_ArenaWeaponFlagForName("bfg"), MM_ARENA_WEAPON_BFG);
}

MM_TEST(arena_ra2_and_ra3_weapon_lists_use_their_own_number_rows) {
	MM_CHECK_EQ(MM_ArenaResolveConfigWeaponListFormat(false, false, ""),
		mm_arena_weapon_list_format_t::RA2);
	MM_CHECK_EQ(MM_ArenaResolveConfigWeaponListFormat(true, false, ""),
		mm_arena_weapon_list_format_t::RA2);
	MM_CHECK_EQ(MM_ArenaResolveConfigWeaponListFormat(false, true, ""),
		mm_arena_weapon_list_format_t::RA3);
	MM_CHECK_EQ(MM_ArenaResolveConfigWeaponListFormat(true, false, "ra3"),
		mm_arena_weapon_list_format_t::RA3);
	MM_CHECK_EQ(MM_ArenaResolveConfigWeaponListFormat(false, true, "ra2"),
		mm_arena_weapon_list_format_t::RA2);
	MM_CHECK_EQ(MM_ArenaResolveConfigWeaponListFormat(false, false, "native"),
		mm_arena_weapon_list_format_t::RA3);

	const auto ra2 = MM_ArenaParseWeaponList("2 3 4 5 6 7 8 9 0",
		mm_arena_weapon_list_format_t::RA2);
	MM_CHECK(ra2.valid);
	MM_CHECK_FALSE(ra2.grapple);
	MM_CHECK_EQ(ra2.mask,
		static_cast<std::uint32_t>(
			MM_ARENA_WEAPON_SHOTGUN |
			MM_ARENA_WEAPON_MACHINEGUN |
			MM_ARENA_WEAPON_GRENADE_LAUNCHER |
			MM_ARENA_WEAPON_ROCKET_LAUNCHER |
			MM_ARENA_WEAPON_PLASMA |
			MM_ARENA_WEAPON_RAILGUN |
			MM_ARENA_WEAPON_BFG));

	const auto ra3 = MM_ArenaParseWeaponList("1234567890",
		mm_arena_weapon_list_format_t::RA3);
	MM_CHECK(ra3.valid);
	MM_CHECK(ra3.grapple);
	MM_CHECK_EQ(ra3.mask,
		static_cast<std::uint32_t>(MM_ARENA_WEAPON_ALL));

	const auto ra2_slot_one = MM_ArenaParseWeaponList("1",
		mm_arena_weapon_list_format_t::RA2);
	MM_CHECK_FALSE(ra2_slot_one.valid);
	MM_CHECK_EQ(ra2_slot_one.mask, 0u);
	MM_CHECK_FALSE(ra2_slot_one.grapple);

	// Some stock RA2 rows omit the semicolon before `armor: 100`; even if a
	// recovery parser presents that trailing token here, 100 is not a BFG row.
	const auto ra2_missing_semicolon = MM_ArenaParseWeaponList(
		"1 2 3 4 5 6 7 8 9 armor:100",
		mm_arena_weapon_list_format_t::RA2);
	MM_CHECK(ra2_missing_semicolon.valid);
	MM_CHECK_EQ(ra2_missing_semicolon.mask,
		static_cast<std::uint32_t>(
			MM_ARENA_WEAPON_SHOTGUN |
			MM_ARENA_WEAPON_MACHINEGUN |
			MM_ARENA_WEAPON_GRENADE_LAUNCHER |
			MM_ARENA_WEAPON_ROCKET_LAUNCHER |
			MM_ARENA_WEAPON_PLASMA |
			MM_ARENA_WEAPON_RAILGUN));
	MM_CHECK_FALSE(ra2_missing_semicolon.grapple);

	const auto ra3_zero = MM_ArenaParseWeaponList("0",
		mm_arena_weapon_list_format_t::RA3);
	MM_CHECK(ra3_zero.valid);
	MM_CHECK_EQ(ra3_zero.mask, 0u);
	MM_CHECK(ra3_zero.grapple);
}

MM_TEST(arena_stock_plasma_ammo_is_not_misparsed_or_user_votable) {
	mm_arena_settings_t settings;
	settings.plasma_ammo = 0;
	const std::uint32_t original_weapons = settings.weapon_mask;
	MM_CHECK(MM_ArenaSettingIsAmmo("plasma"));
	MM_CHECK(MM_ArenaApplyAmmoSetting(settings, "plasma", 100));
	MM_CHECK_EQ(settings.plasma_ammo, 100);
	MM_CHECK_EQ(settings.weapon_mask, original_weapons);
	MM_CHECK_EQ(MM_ArenaVoteFlagForSetting("plasma"), 0u);
	MM_CHECK_EQ(MM_ArenaVoteFlagForSetting("shells"), 0u);
	MM_CHECK_EQ(MM_ArenaVoteFlagForSetting("plasmagun"),
		static_cast<std::uint32_t>(MM_ARENA_VOTE_HYPERBLASTER));
	MM_CHECK_EQ(MM_ArenaVoteFlagForSetting("weapons"),
		static_cast<std::uint32_t>(MM_ARENA_VOTE_WEAPONS));
}

MM_TEST(arena_ra3_defaults_and_pickup_aliases_are_preserved) {
	const mm_arena_settings_t defaults;
	MM_CHECK_EQ(defaults.health, 100);
	MM_CHECK_EQ(defaults.armor, 100);
	MM_CHECK_EQ(defaults.grenades, 20);
	MM_CHECK_EQ(defaults.plasma_ammo, 100);
	MM_CHECK_EQ(defaults.bfg_ammo, 20);
	MM_CHECK_EQ(defaults.rocket_speed, 900);
	MM_CHECK_EQ(defaults.vote_tries, 2);
	MM_CHECK_EQ(defaults.weapon_mask,
		static_cast<std::uint32_t>(MM_ARENA_WEAPON_STANDARD));
	MM_CHECK_EQ(defaults.vote_allow_mask & MM_ARENA_VOTE_EXCESSIVE, 0u);
	MM_CHECK_EQ(defaults.vote_allow_mask & MM_ARENA_VOTE_GRAPPLE, 0u);
	MM_CHECK(defaults.vote_allow_mask & MM_ARENA_VOTE_WEAPONS);

	MM_CHECK_EQ(MM_ArenaParseType("practicearena",
		mm_arena_type_t::RocketArena), mm_arena_type_t::Practice);
	MM_CHECK_EQ(MM_ArenaResolvePickupType(true,
		mm_arena_type_t::RocketArena, mm_arena_type_t::RedRover),
		mm_arena_type_t::RedRover);
	MM_CHECK_EQ(MM_ArenaResolvePickupType(false,
		mm_arena_type_t::RocketArena, mm_arena_type_t::RedRover),
		mm_arena_type_t::RocketArena);
}

MM_TEST(red_rover_frag_warning_never_indexes_below_zero) {
	MM_CHECK_FALSE(MM_FragWarningIndex(3, 0));
	MM_CHECK_EQ(*MM_FragWarningIndex(10, 9), 0u);
	MM_CHECK_EQ(*MM_FragWarningIndex(10, 8), 1u);
	MM_CHECK_EQ(*MM_FragWarningIndex(10, 7), 2u);
	MM_CHECK_FALSE(MM_FragWarningIndex(10, 10));
	MM_CHECK_FALSE(MM_FragWarningIndex(10, 11));
	MM_CHECK_FALSE(MM_FragWarningIndex(10, 6));
}

MM_TEST(red_rover_uses_individual_scorelimit_and_blocks_only_manual_side_switches) {
	const int spectator = 0;
	const int red = 1;
	const int blue = 2;

	MM_CHECK(MM_UseTeamScoreLimit(true, false));
	MM_CHECK_FALSE(MM_UseTeamScoreLimit(true, true));
	MM_CHECK_FALSE(MM_UseTeamScoreLimit(false, false));

	MM_CHECK(MM_RedRoverBlocksManualTeamSwitch(red, blue, spectator, true, true));
	MM_CHECK_FALSE(MM_RedRoverBlocksManualTeamSwitch(spectator, red, spectator, true, true));
	MM_CHECK_FALSE(MM_RedRoverBlocksManualTeamSwitch(red, spectator, spectator, true, true));
	MM_CHECK_FALSE(MM_RedRoverBlocksManualTeamSwitch(red, blue, spectator, false, true));
	MM_CHECK_FALSE(MM_RedRoverBlocksManualTeamSwitch(red, blue, spectator, true, false));
}

MM_TEST(red_rover_round_ends_only_when_one_team_is_cleared) {
	// a round ends the moment one side is emptied (>= 2 players total)
	MM_CHECK(MM_RedRoverRoundShouldEnd(3, 0));
	MM_CHECK(MM_RedRoverRoundShouldEnd(0, 4));
	MM_CHECK(MM_RedRoverRoundShouldEnd(2, 0));

	// both teams populated: round continues
	MM_CHECK_FALSE(MM_RedRoverRoundShouldEnd(3, 1));
	MM_CHECK_FALSE(MM_RedRoverRoundShouldEnd(1, 1));

	// lone survivor (everyone else gone) must not trip an instant round-end
	MM_CHECK_FALSE(MM_RedRoverRoundShouldEnd(1, 0));
	MM_CHECK_FALSE(MM_RedRoverRoundShouldEnd(0, 1));
	MM_CHECK_FALSE(MM_RedRoverRoundShouldEnd(0, 0));
}

MM_TEST(lms_round_resolves_to_last_active_fighter) {
	// a winner is declared when exactly one active fighter remains (>= 2 participated)
	MM_CHECK(MM_LMSRoundHasWinner(1, 2));
	MM_CHECK(MM_LMSRoundHasWinner(1, 8));

	// more than one fighter left, or none left: not yet a single winner
	MM_CHECK_FALSE(MM_LMSRoundHasWinner(2, 3));
	MM_CHECK_FALSE(MM_LMSRoundHasWinner(0, 2));

	// a lone participant (everyone else gone) must not be handed a round win
	MM_CHECK_FALSE(MM_LMSRoundHasWinner(1, 1));
	MM_CHECK_FALSE(MM_LMSRoundHasWinner(0, 1));
}

MM_TEST(lms_round_is_a_draw_only_on_mutual_elimination) {
	// every fighter eliminated with >= 2 participants is a draw
	MM_CHECK(MM_LMSRoundIsDraw(0, 2));
	MM_CHECK(MM_LMSRoundIsDraw(0, 5));

	// survivors remain: not a draw
	MM_CHECK_FALSE(MM_LMSRoundIsDraw(1, 2));
	MM_CHECK_FALSE(MM_LMSRoundIsDraw(2, 4));

	// lone participant can neither win nor draw the round
	MM_CHECK_FALSE(MM_LMSRoundIsDraw(0, 1));
}

MM_TEST(map_pick_duration_reads_zero_and_negatives_as_disabled) {
	MM_CHECK_EQ(MM_MapPickDurationSeconds(0), 0);
	MM_CHECK_EQ(MM_MapPickDurationSeconds(-15), 0);

	MM_CHECK_EQ(MM_MapPickDurationSeconds(15), 15);
	// Anything shorter than the floor clamps up; anything past the ceiling clamps down.
	MM_CHECK_EQ(MM_MapPickDurationSeconds(1), MM_MAP_PICK_MIN_SECONDS);
	MM_CHECK_EQ(MM_MapPickDurationSeconds(6000), MM_MAP_PICK_MAX_SECONDS);
}

MM_TEST(map_pick_majority_needs_more_than_half_the_voters) {
	// Two of three cannot be overturned by the last voter.
	MM_CHECK(MM_MapPickHasMajority(2, 3));
	MM_CHECK_FALSE(MM_MapPickHasMajority(1, 3));

	// With an even count a bare half still leaves a tie available.
	MM_CHECK_FALSE(MM_MapPickHasMajority(2, 4));
	MM_CHECK(MM_MapPickHasMajority(3, 4));

	// A lone voter decides immediately; no voters decide nothing.
	MM_CHECK(MM_MapPickHasMajority(1, 1));
	MM_CHECK_FALSE(MM_MapPickHasMajority(0, 0));
	MM_CHECK_FALSE(MM_MapPickHasMajority(1, 0));
}

MM_TEST(map_pick_winner_takes_the_lead_and_ignores_unoffered_slots) {
	const mm_map_pick_offered_t two_offers { true, true, false };

	MM_CHECK_EQ(MM_MapPickWinner({ 1, 3, 0 }, two_offers, 0), 1);
	MM_CHECK_EQ(MM_MapPickWinner({ 4, 2, 0 }, two_offers, 7), 0);

	// A tally left over on a slot that was never offered cannot win.
	MM_CHECK_EQ(MM_MapPickWinner({ 1, 0, 9 }, two_offers, 0), 0);

	// Nothing offered means nothing to pick.
	MM_CHECK_EQ(MM_MapPickWinner({ 5, 5, 5 }, { false, false, false }, 0), -1);
}

MM_TEST(map_pick_winner_breaks_ties_and_still_lands_on_a_map_with_no_votes) {
	const mm_map_pick_offered_t all_offered { true, true, true };

	// A three-way tie draws from all three; the roll selects which.
	MM_CHECK_EQ(MM_MapPickWinner({ 2, 2, 2 }, all_offered, 0), 0);
	MM_CHECK_EQ(MM_MapPickWinner({ 2, 2, 2 }, all_offered, 1), 1);
	MM_CHECK_EQ(MM_MapPickWinner({ 2, 2, 2 }, all_offered, 5), 2);

	// A two-way tie only draws from the tied pair, never from the trailing map.
	MM_CHECK_EQ(MM_MapPickWinner({ 3, 1, 3 }, all_offered, 0), 0);
	MM_CHECK_EQ(MM_MapPickWinner({ 3, 1, 3 }, all_offered, 1), 2);

	// Nobody voted: every offer is still a live candidate rather than a dead pick.
	MM_CHECK_EQ(MM_MapPickWinner({ 0, 0, 0 }, all_offered, 1), 1);
	MM_CHECK_EQ(MM_MapPickWinner({ 0, 0, 0 }, { false, true, true }, 0), 1);
	MM_CHECK_EQ(MM_MapPickWinner({ 0, 0, 0 }, { false, true, true }, 1), 2);
}

MM_TEST(map_pick_bar_fills_on_open_and_empties_only_when_time_is_up) {
	MM_CHECK_EQ(MM_MapPickBarSegments(15.0f, 15.0f), MM_MAP_PICK_BAR_SEGMENTS);
	MM_CHECK_EQ(MM_MapPickBarSegments(0.0f, 15.0f), 0);

	// Rounding up keeps a sliver of bar for the last partial second.
	MM_CHECK_EQ(MM_MapPickBarSegments(0.1f, 15.0f), 1);
	MM_CHECK_EQ(MM_MapPickBarSegments(7.5f, 15.0f), MM_MAP_PICK_BAR_SEGMENTS / 2);

	// Degenerate inputs draw nothing rather than overrunning the row.
	MM_CHECK_EQ(MM_MapPickBarSegments(5.0f, 0.0f), 0);
	MM_CHECK_EQ(MM_MapPickBarSegments(-5.0f, 15.0f), 0);
	MM_CHECK_EQ(MM_MapPickBarSegments(90.0f, 15.0f), MM_MAP_PICK_BAR_SEGMENTS);
}

MM_TEST(ent_respawn_delay_reads_zero_and_garbage_as_disabled) {
	MM_CHECK_EQ(MM_EntRespawnDelaySeconds(0.0f), 0.0f);
	MM_CHECK_EQ(MM_EntRespawnDelaySeconds(-30.0f), 0.0f);
	MM_CHECK_EQ(MM_EntRespawnDelaySeconds(std::numeric_limits<float>::quiet_NaN()), 0.0f);

	MM_CHECK_EQ(MM_EntRespawnDelaySeconds(60.0f), 60.0f);
	// Sub-second delays clamp up to the retry cadence; anything past an hour clamps down.
	MM_CHECK_EQ(MM_EntRespawnDelaySeconds(0.25f), MM_ENT_RESPAWN_MIN_DELAY_SECONDS);
	MM_CHECK_EQ(MM_EntRespawnDelaySeconds(99999.0f), MM_ENT_RESPAWN_MAX_DELAY_SECONDS);
}

MM_TEST(ent_respawn_box_center_handles_brush_model_origins) {
	// A misc_explobox carries its bounds around its own origin.
	const mm_ent_respawn_vec_t barrel = MM_EntRespawnBoxCenter(
		{ 128.0f, -64.0f, 32.0f }, { -16.0f, -16.0f, 0.0f }, { 16.0f, 16.0f, 40.0f });
	MM_CHECK_EQ(barrel.x, 128.0f);
	MM_CHECK_EQ(barrel.y, -64.0f);
	MM_CHECK_EQ(barrel.z, 52.0f);

	// A func_explosive brush sits at origin 0,0,0 with world-space bounds, so the
	// recorded origin on its own is not a usable reference point.
	const mm_ent_respawn_vec_t brush = MM_EntRespawnBoxCenter(
		{ 0.0f, 0.0f, 0.0f }, { 200.0f, 300.0f, 0.0f }, { 264.0f, 364.0f, 64.0f });
	MM_CHECK_EQ(brush.x, 232.0f);
	MM_CHECK_EQ(brush.y, 332.0f);
	MM_CHECK_EQ(brush.z, 32.0f);
}

MM_TEST(ent_respawn_facing_test_blocks_viewers_turned_toward_the_spot) {
	const mm_ent_respawn_vec_t viewer { 0.0f, 0.0f, 0.0f };
	const mm_ent_respawn_vec_t looking_east { 1.0f, 0.0f, 0.0f };

	MM_CHECK(MM_EntRespawnFacesPoint(viewer, looking_east, { 512.0f, 0.0f, 0.0f }));
	MM_CHECK(!MM_EntRespawnFacesPoint(viewer, looking_east, { -512.0f, 0.0f, 0.0f }));

	// 60 degrees off-axis is cos 0.5, still inside the cone; 85 degrees is not.
	MM_CHECK(MM_EntRespawnFacesPoint(viewer, looking_east, { 100.0f, 173.2f, 0.0f }));
	MM_CHECK(!MM_EntRespawnFacesPoint(viewer, looking_east, { 100.0f, 1143.0f, 0.0f }));

	// A viewer standing exactly on the spot has no direction to compare against.
	// Normalizing anyway gives NaN, which would read as "clear" and drop the prop
	// straight into the player, so the degenerate case has to block.
	MM_CHECK(MM_EntRespawnFacesPoint(viewer, looking_east, viewer));
}

MM_TEST(ent_respawn_clearance_keeps_props_off_nearby_players) {
	const mm_ent_respawn_vec_t spot { 0.0f, 0.0f, 0.0f };
	const mm_ent_respawn_vec_t player_mins { -16.0f, -16.0f, -24.0f };
	const mm_ent_respawn_vec_t player_maxs { 16.0f, 16.0f, 32.0f };

	auto player_at = [&](float x, float y, float z) {
		return MM_EntRespawnWithinClearance(spot,
			{ x + player_mins.x, y + player_mins.y, z + player_mins.z },
			{ x + player_maxs.x, y + player_maxs.y, z + player_maxs.z });
	};

	MM_CHECK(player_at(0.0f, 0.0f, 0.0f));
	MM_CHECK(player_at(120.0f, 0.0f, 0.0f));
	// The box is a half-extent, so a player's own bounds still reach in past 128.
	MM_CHECK(player_at(140.0f, 0.0f, 0.0f));
	MM_CHECK(!player_at(160.0f, 0.0f, 0.0f));
	MM_CHECK(!player_at(0.0f, 0.0f, 300.0f));
	// Clearance is a box, not a sphere: the corners reach further than the faces.
	MM_CHECK(player_at(140.0f, 140.0f, 140.0f));
}

MM_TEST(ent_respawn_box_overlap_counts_touching_faces) {
	const mm_ent_respawn_vec_t a_mins { 0.0f, 0.0f, 0.0f };
	const mm_ent_respawn_vec_t a_maxs { 10.0f, 10.0f, 10.0f };

	MM_CHECK(MM_EntRespawnBoxesOverlap(a_mins, a_maxs, { 5.0f, 5.0f, 5.0f }, { 15.0f, 15.0f, 15.0f }));
	MM_CHECK(MM_EntRespawnBoxesOverlap(a_mins, a_maxs, { 10.0f, 0.0f, 0.0f }, { 20.0f, 10.0f, 10.0f }));
	MM_CHECK(!MM_EntRespawnBoxesOverlap(a_mins, a_maxs, { 10.1f, 0.0f, 0.0f }, { 20.0f, 10.0f, 10.0f }));
	MM_CHECK(!MM_EntRespawnBoxesOverlap(a_mins, a_maxs, { 0.0f, 0.0f, -20.0f }, { 10.0f, 10.0f, -0.1f }));
	// Fully contained on every axis still overlaps.
	MM_CHECK(MM_EntRespawnBoxesOverlap(a_mins, a_maxs, { 2.0f, 2.0f, 2.0f }, { 3.0f, 3.0f, 3.0f }));
}

MM_TEST(freezetag_round_resolves_wipes_draws_and_time_ties) {
	const mm_freezetag_team_counts_t red_full { 3, 3, 260 };
	const mm_freezetag_team_counts_t blue_full { 2, 2, 180 };
	const mm_freezetag_team_counts_t red_frozen { 3, 0, 0 };
	const mm_freezetag_team_counts_t blue_frozen { 2, 0, 0 };

	MM_CHECK_EQ(MM_FreezeTagResolveRound(red_frozen, blue_full, false), mm_freezetag_round_result_t::BlueWinsByFreeze);
	MM_CHECK_EQ(MM_FreezeTagResolveRound(red_full, blue_frozen, false), mm_freezetag_round_result_t::RedWinsByFreeze);
	MM_CHECK_EQ(MM_FreezeTagResolveRound(red_frozen, blue_frozen, false), mm_freezetag_round_result_t::Draw);
	MM_CHECK_EQ(MM_FreezeTagResolveRound({ 0, 0, 0 }, blue_full, false), mm_freezetag_round_result_t::Continue);
	MM_CHECK_EQ(MM_FreezeTagResolveRound(red_full, blue_full, false), mm_freezetag_round_result_t::Continue);
	MM_CHECK_EQ(MM_FreezeTagResolveRound({ 3, 2, 200 }, { 3, 1, 220 }, true), mm_freezetag_round_result_t::RedWinsByActiveCount);
	MM_CHECK_EQ(MM_FreezeTagResolveRound({ 3, 1, 220 }, { 3, 2, 200 }, true), mm_freezetag_round_result_t::BlueWinsByActiveCount);
	MM_CHECK_EQ(MM_FreezeTagResolveRound({ 3, 2, 160 }, { 3, 2, 140 }, true), mm_freezetag_round_result_t::RedWinsByHealth);
	MM_CHECK_EQ(MM_FreezeTagResolveRound({ 3, 2, 140 }, { 3, 2, 160 }, true), mm_freezetag_round_result_t::BlueWinsByHealth);
	MM_CHECK_EQ(MM_FreezeTagResolveRound({ 3, 2, 160 }, { 3, 2, 160 }, true), mm_freezetag_round_result_t::Draw);
}

MM_TEST(freezetag_late_spawns_wait_for_next_round) {
	MM_CHECK(MM_FreezeTagSpawnShouldWaitForNextRound(true, true, true, false));
	MM_CHECK(MM_FreezeTagSpawnShouldWaitForNextRound(true, true, false, true));
	MM_CHECK_FALSE(MM_FreezeTagSpawnShouldWaitForNextRound(true, true, false, false));
	MM_CHECK_FALSE(MM_FreezeTagSpawnShouldWaitForNextRound(true, false, true, false));
	MM_CHECK_FALSE(MM_FreezeTagSpawnShouldWaitForNextRound(false, true, true, false));
}

MM_TEST(freezetag_round_countdown_respawns_round_players) {
	MM_CHECK(MM_FreezeTagRoundCountdownShouldRespawn(true, true, true, true));
	MM_CHECK_FALSE(MM_FreezeTagRoundCountdownShouldRespawn(false, true, true, true));
	MM_CHECK_FALSE(MM_FreezeTagRoundCountdownShouldRespawn(true, false, true, true));
	MM_CHECK_FALSE(MM_FreezeTagRoundCountdownShouldRespawn(true, true, false, true));
	MM_CHECK_FALSE(MM_FreezeTagRoundCountdownShouldRespawn(true, true, true, false));
}

MM_TEST(freezetag_round_participants_exclude_waiting_late_joiners) {
	MM_CHECK(MM_FreezeTagClientCountsForRound(true, true, false));
	MM_CHECK_FALSE(MM_FreezeTagClientCountsForRound(true, true, true));
	MM_CHECK_FALSE(MM_FreezeTagClientCountsForRound(true, false, false));
	MM_CHECK_FALSE(MM_FreezeTagClientCountsForRound(false, true, false));
}

MM_TEST(freezetag_death_conversion_respects_match_state_and_same_team_deaths) {
	MM_CHECK(MM_FreezeTagDeathShouldFreeze(true, true, true, true, false, false, true, false, false, false));
	MM_CHECK(MM_FreezeTagDeathShouldFreeze(true, true, true, true, false, false, false, false, false, false));
	MM_CHECK(MM_FreezeTagDeathShouldFreeze(true, true, true, true, false, false, true, true, true, false));
	MM_CHECK_FALSE(MM_FreezeTagDeathShouldFreeze(false, true, true, true, false, false, true, false, false, false));
	MM_CHECK_FALSE(MM_FreezeTagDeathShouldFreeze(true, false, true, true, false, false, true, false, false, false));
	MM_CHECK_FALSE(MM_FreezeTagDeathShouldFreeze(true, true, false, true, false, false, true, false, false, false));
	MM_CHECK_FALSE(MM_FreezeTagDeathShouldFreeze(true, true, true, false, false, false, true, false, false, false));
	MM_CHECK_FALSE(MM_FreezeTagDeathShouldFreeze(true, true, true, true, true, false, true, false, false, false));
	MM_CHECK_FALSE(MM_FreezeTagDeathShouldFreeze(true, true, true, true, false, true, true, false, false, false));
	MM_CHECK_FALSE(MM_FreezeTagDeathShouldFreeze(true, true, true, true, false, false, true, false, true, false));
	MM_CHECK(MM_FreezeTagDeathShouldFreeze(true, true, true, true, false, false, true, false, true, true));
}

MM_TEST(freezetag_arena_loadout_is_mode_policy_not_global_arena_flag) {
	MM_CHECK(MM_FreezeTagUsesArenaLoadout(false, true, true));
	MM_CHECK_FALSE(MM_FreezeTagUsesArenaLoadout(false, true, false));
	MM_CHECK_FALSE(MM_FreezeTagUsesArenaLoadout(false, false, true));
	MM_CHECK(MM_FreezeTagUsesArenaLoadout(true, false, false));
	MM_CHECK_FALSE(MM_FreezeTagDropWeaponOnFreeze(true));
	MM_CHECK(MM_FreezeTagDropWeaponOnFreeze(false));
}

MM_TEST(freezetag_arena_thaws_select_rocket_launcher_when_available) {
	constexpr int fallback_weapon = 3;
	constexpr int rocket_launcher = 7;

	MM_CHECK_EQ(MM_FreezeTagThawWeaponId(true, true, fallback_weapon, rocket_launcher), rocket_launcher);
	MM_CHECK_EQ(MM_FreezeTagThawWeaponId(true, false, fallback_weapon, rocket_launcher), fallback_weapon);
	MM_CHECK_EQ(MM_FreezeTagThawWeaponId(false, true, fallback_weapon, rocket_launcher), fallback_weapon);
}

MM_TEST(freezetag_hud_status_formats_relative_and_spectator_counts) {
	const mm_freezetag_team_counts_t red { 3, 2, 180 };
	const mm_freezetag_team_counts_t blue { 4, 1, 90 };

	MM_CHECK_EQ(MM_FreezeTagFormatHudStatus(red, blue, mm_freezetag_hud_team_t::Red), std::string("Alive A 2/3 E 1/4"));
	MM_CHECK_EQ(MM_FreezeTagFormatHudStatus(red, blue, mm_freezetag_hud_team_t::Blue), std::string("Alive A 1/4 E 2/3"));
	MM_CHECK_EQ(MM_FreezeTagFormatHudStatus(red, blue, mm_freezetag_hud_team_t::Neutral), std::string("Alive R 2/3 B 1/4"));
	MM_CHECK_EQ(MM_FreezeTagFormatHudStatus({ -1, 5, 0 }, { 2, -3, 0 }, mm_freezetag_hud_team_t::Neutral), std::string("Alive R 0/0 B 0/2"));

	MM_CHECK_EQ(MM_FreezeTagFormatCompactAliveStatus(red, blue, mm_freezetag_hud_team_t::Red), std::string("2 vs 1"));
	MM_CHECK_EQ(MM_FreezeTagFormatCompactAliveStatus(red, blue, mm_freezetag_hud_team_t::Blue), std::string("1 vs 2"));
	MM_CHECK_EQ(MM_FreezeTagFormatCompactAliveStatus(red, blue, mm_freezetag_hud_team_t::Neutral), std::string("2 vs 1"));
}

MM_TEST(freezetag_round_hud_status_formats_score_and_limit) {
	MM_CHECK_EQ(MM_FreezeTagFormatRoundHudStatus(2, 8, 1, 0), std::string("Round 2"));
	MM_CHECK_EQ(MM_FreezeTagFormatRoundHudStatus(2, 0, 1, 0), std::string("Round 2"));
	MM_CHECK_EQ(MM_FreezeTagFormatRoundHudStatus(0, 8, -2, 3), std::string());
}

MM_TEST(freezetag_thaw_rate_rewards_teamwork_without_extremes) {
	MM_CHECK_EQ(MM_FreezeTagThawRate(0, 0.5f), 0.0f);
	MM_CHECK_EQ(MM_FreezeTagThawRate(1, 0.5f), 1.0f);
	MM_CHECK_EQ(MM_FreezeTagThawRate(2, 0.5f), 1.5f);
	MM_CHECK_EQ(MM_FreezeTagThawRate(3, 0.5f), 2.0f);
	MM_CHECK_EQ(MM_FreezeTagThawRate(3, -1.0f), 1.0f);
	MM_CHECK_EQ(MM_FreezeTagThawRate(32, 4.0f), 8.0f);
}

MM_TEST(freezetag_thaw_progress_is_clamped_for_hud) {
	MM_CHECK_EQ(MM_FreezeTagThawProgressPercent(0.0f, 3.0f), 0);
	MM_CHECK_EQ(MM_FreezeTagThawProgressPercent(1.5f, 3.0f), 50);
	MM_CHECK_EQ(MM_FreezeTagThawProgressPercent(3.0f, 3.0f), 99);
	MM_CHECK_EQ(MM_FreezeTagThawProgressPercent(30.0f, 3.0f), 99);
	MM_CHECK_EQ(MM_FreezeTagThawProgressPercent(1.0f, 0.0f), 0);
	MM_CHECK_EQ(MM_FreezeTagThawProgressPercent(-1.0f, 3.0f), 0);
}

MM_TEST(freezetag_thaw_assist_threshold_scales_with_server_timing) {
	MM_CHECK_EQ(MM_FreezeTagThawAssistThreshold(3.0f), 0.75f);
	MM_CHECK_FALSE(MM_FreezeTagThawAssistQualifies(0.70f, 3.0f));
	MM_CHECK(MM_FreezeTagThawAssistQualifies(0.75f, 3.0f));
	MM_CHECK_EQ(MM_FreezeTagThawAssistThreshold(30.0f), 1.0f);
	MM_CHECK_FALSE(MM_FreezeTagThawAssistQualifies(0.99f, 30.0f));
	MM_CHECK(MM_FreezeTagThawAssistQualifies(1.0f, 30.0f));
	MM_CHECK_EQ(MM_FreezeTagThawAssistThreshold(0.1f), 0.1f);
	MM_CHECK(MM_FreezeTagThawAssistQualifies(0.1f, 0.1f));
	MM_CHECK_FALSE(MM_FreezeTagThawAssistQualifies(1.0f, 0.0f));
}

MM_TEST(freezetag_bot_look_pitch_keeps_the_thawer_model_upright) {
	// Mirrors the model-pitch fold in ClientEndServerFrame (src/sgame/client/view.cpp):
	// it only folds the > 180 case, so a negative wrap falls straight through the divide.
	const auto model_pitch = [](float v_angle_pitch) {
		return v_angle_pitch > 180.0f ? (-360.0f + v_angle_pitch) / 3.0f : v_angle_pitch / 3.0f;
	};

	// The regression: vectoangles() returns -352 for a bot glancing 8 degrees down at a
	// body by its feet, and the raw value renders the thawer past horizontal.
	MM_CHECK(model_pitch(-352.0f) < -100.0f);
	MM_CHECK_EQ(model_pitch(MM_FreezeTagNormalizeLookPitch(-352.0f)), 8.0f / 3.0f);

	// Standing directly over a body: vectoangles special-cases straight down to -270.
	MM_CHECK_EQ(MM_FreezeTagNormalizeLookPitch(-270.0f), 89.0f);
	// Looking up stays negative-pitch in Quake's wrapped convention.
	MM_CHECK_EQ(MM_FreezeTagNormalizeLookPitch(-8.0f), 352.0f);
	MM_CHECK_EQ(MM_FreezeTagNormalizeLookPitch(-90.0f), 271.0f);
	// Already-legal angles pass through untouched.
	MM_CHECK_EQ(MM_FreezeTagNormalizeLookPitch(45.0f), 45.0f);
	MM_CHECK_EQ(MM_FreezeTagNormalizeLookPitch(300.0f), 300.0f);
	// Unreachable bands clamp to the edges PM_ClampAngles allows.
	MM_CHECK_EQ(MM_FreezeTagNormalizeLookPitch(120.0f), 89.0f);
	MM_CHECK_EQ(MM_FreezeTagNormalizeLookPitch(200.0f), 271.0f);
	// Multiple wraps still land in range.
	MM_CHECK_EQ(MM_FreezeTagNormalizeLookPitch(-720.5f), 359.5f);
}

MM_TEST(freezetag_frozen_knockback_floors_weapons_that_carry_no_kick) {
	// Direct rocket/BFG hits pass knockback 0 and rely on splash, so scaling alone
	// can never shove an ice block. Fall back on damage, Quake 3 style.
	MM_CHECK_EQ(MM_FreezeTagFrozenKnockback(0, 110, 1.0f, true), 110);
	// A weapon that does carry kick keeps it when it already beats the damage floor.
	MM_CHECK_EQ(MM_FreezeTagFrozenKnockback(200, 100, 1.0f, true), 200);
	// The floor is capped at the Quake 3 convention's 200.
	MM_CHECK_EQ(MM_FreezeTagFrozenKnockback(0, 100000, 1.0f, true), 200);
	// Scale still applies and rounds up, so small hits never silently vanish.
	MM_CHECK_EQ(MM_FreezeTagFrozenKnockback(0, 8, 1.5f, true), 12);
	MM_CHECK_EQ(MM_FreezeTagFrozenKnockback(2, 0, 0.1f, true), 1);
	// Scale 0 (and a negative misconfiguration) disable shoving entirely.
	MM_CHECK_EQ(MM_FreezeTagFrozenKnockback(200, 100, 0.0f, true), 0);
	MM_CHECK_EQ(MM_FreezeTagFrozenKnockback(200, 100, -1.0f, true), 0);
	// FL_NO_KNOCKBACK must not be resurrected by the damage floor.
	MM_CHECK_EQ(MM_FreezeTagFrozenKnockback(0, 110, 1.0f, false), 0);
	MM_CHECK_EQ(MM_FreezeTagFrozenKnockback(-5, 110, 1.0f, false), 0);
}

MM_TEST(freezetag_frozen_slide_friction_stays_in_a_usable_band) {
	// A misconfigured value can neither freeze bodies in place nor make them
	// perpetual sliders.
	MM_CHECK_EQ(MM_FreezeTagFrozenSlideFriction(0.9f), 0.9f);
	MM_CHECK_EQ(MM_FreezeTagFrozenSlideFriction(2.0f), 0.99f);
	MM_CHECK_EQ(MM_FreezeTagFrozenSlideFriction(-1.0f), 0.1f);
	// Bodies keep vanilla's stop rule, so gravity along a ramp can never hold
	// one above a lower private threshold and creep it downhill all round.
	MM_CHECK_EQ(kMMFreezeTagTossStopSpeed, 60.0f);
}

MM_TEST(freezetag_shove_unsticks_lifts_and_clamps) {
	// A hit that the stop rule would erase in the same frame leaves the body
	// parked rather than bobbing it in place; the velocity still accumulates,
	// so sustained fire eventually crosses the bar.
	MM_CHECK_FALSE(MM_FreezeTagShoveShouldUnstick(5.0f, -10.0f));
	MM_CHECK_FALSE(MM_FreezeTagShoveShouldUnstick(59.0f, -10.0f));
	// A real shove breaks ground contact.
	MM_CHECK(MM_FreezeTagShoveShouldUnstick(60.0f, -10.0f));
	MM_CHECK(MM_FreezeTagShoveShouldUnstick(250.0f, -10.0f));
	// So does any upward component, matching G_Physics_Toss's own rule.
	MM_CHECK(MM_FreezeTagShoveShouldUnstick(1.0f, 5.0f));

	// The hop is a floor, not an addition, so shooting down at a body still lifts it.
	MM_CHECK_EQ(MM_FreezeTagShoveLiftVelocity(-40.0f, 24.0f), 24.0f);
	MM_CHECK_EQ(MM_FreezeTagShoveLiftVelocity(300.0f, 24.0f), 300.0f);
	MM_CHECK_EQ(MM_FreezeTagShoveLiftVelocity(-40.0f, 0.0f), 0.0f);

	MM_CHECK_EQ(MM_FreezeTagShoveSpeedScale(350.0f, 700.0f), 1.0f);
	MM_CHECK_EQ(MM_FreezeTagShoveSpeedScale(1400.0f, 700.0f), 0.5f);
	MM_CHECK_EQ(MM_FreezeTagShoveSpeedScale(0.0f, 700.0f), 1.0f);
	MM_CHECK_EQ(MM_FreezeTagShoveSpeedScale(-5.0f, 700.0f), 1.0f);

	MM_CHECK(MM_FreezeTagHazardShouldRelease(3.0f, 3.0f));
	MM_CHECK_FALSE(MM_FreezeTagHazardShouldRelease(2.9f, 3.0f));
	// Zero is a kill switch, not an instant release.
	MM_CHECK_FALSE(MM_FreezeTagHazardShouldRelease(100.0f, 0.0f));
}

MM_TEST(scoreboard_footer_reserve_keeps_layout_room_available) {
	MM_CHECK_EQ(MM_ScoreboardFooterReserve(false), 96u);
	MM_CHECK_EQ(MM_ScoreboardFooterReserve(true), 320u);
	MM_CHECK(MM_ScoreboardCanAppend(0, 1, 1400, false));
	MM_CHECK(MM_ScoreboardCanAppend(1304, 0, 1400, false));
	MM_CHECK_FALSE(MM_ScoreboardCanAppend(1304, 1, 1400, false));
	MM_CHECK_FALSE(MM_ScoreboardCanAppend(1080, 1, 1400, true));

	// The real budget is MAX_STRING_CHARS (1024), not the 1400 above; the boundaries there are
	// what actually decide whether an arena board keeps its footer.
	MM_CHECK(MM_ScoreboardCanAppend(0, 928, 1024, false));
	MM_CHECK_FALSE(MM_ScoreboardCanAppend(0, 929, 1024, false));
	MM_CHECK(MM_ScoreboardCanAppend(0, 704, 1024, true));
	MM_CHECK_FALSE(MM_ScoreboardCanAppend(0, 705, 1024, true));

	// A budget smaller than its own footer reserve can never accept anything, not even nothing.
	MM_CHECK_FALSE(MM_ScoreboardCanAppend(0, 0, 96, false));
	MM_CHECK_FALSE(MM_ScoreboardCanAppend(0, 0, 320, true));
}

MM_TEST(fake_command_import_models_argv_contract) {
	fake_command_import_t fake;
	fake.set_args({ "teleport", "1", "2", "3" });

	MM_CHECK_EQ(fake.argc(), 4);
	MM_CHECK_EQ(std::string(fake.argv(0)), "teleport");
	MM_CHECK_EQ(std::string(fake.argv(3)), "3");
	MM_CHECK_EQ(std::string(fake.argv(99)), "");
	MM_CHECK_EQ(std::string(fake.args()), "1 2 3");
	MM_CHECK(MM_IsTeleportArgcValid(fake.argc()));
}

MM_TEST(horde_target_load_score_prefers_lower_burden_at_equal_distance) {
	const float spread = 512.f;
	const float dist = 1024.f;

	MM_CHECK(MM_Horde_ComputeTargetLoadScore(0, dist, spread) < MM_Horde_ComputeTargetLoadScore(3, dist, spread));
	MM_CHECK(MM_Horde_ComputeTargetLoadScore(1, dist, spread) < MM_Horde_ComputeTargetLoadScore(1, dist + spread, spread));
}

MM_TEST(horde_role_targeting_distinguishes_hunters_and_bulwarks) {
	const float grouped_hunter = MM_Horde_ComputeRoleTargetScore(1, 800.f, 64.f, 0.5f,
		512.f, 256.f, 192.f, mm_horde_target_role_t::Hunter);
	const float isolated_hunter = MM_Horde_ComputeRoleTargetScore(1, 800.f, 1024.f, 0.5f,
		512.f, 256.f, 192.f, mm_horde_target_role_t::Hunter);
	MM_CHECK(isolated_hunter < grouped_hunter);

	const float wounded_bulwark = MM_Horde_ComputeRoleTargetScore(1, 800.f, 256.f, 0.2f,
		512.f, 256.f, 192.f, mm_horde_target_role_t::Bulwark);
	const float healthy_bulwark = MM_Horde_ComputeRoleTargetScore(1, 800.f, 256.f, 1.0f,
		512.f, 256.f, 192.f, mm_horde_target_role_t::Bulwark);
	MM_CHECK(healthy_bulwark < wounded_bulwark);
}

MM_TEST(horde_relentless_pursuit_never_parks_a_threat) {
	// A nav failure may only hold a pursuer for the configured retry window.
	MM_CHECK_EQ(MM_Horde_ClampPursuitLockoutMs(12'000, 2'000, 2.f), 4'000);
	// Shorter waits are left alone.
	MM_CHECK_EQ(MM_Horde_ClampPursuitLockoutMs(2'500, 2'000, 2.f), 2'500);
	// Zero seconds retries on the next frame; non-finite falls back to the 2 second default.
	MM_CHECK_EQ(MM_Horde_ClampPursuitLockoutMs(12'000, 2'000, 0.f), 2'000);
	MM_CHECK_EQ(MM_Horde_ClampPursuitLockoutMs(12'000, 2'000,
		std::numeric_limits<float>::quiet_NaN()), 4'000);
	// The clamp is bounded, so an absurd value cannot restore an indefinite park.
	MM_CHECK_EQ(MM_Horde_ClampPursuitLockoutMs(1'000'000, 0, 1.0e9f), 60'000);

	// The first sample of a chase has no baseline, so it never reports a stall.
	MM_CHECK_FALSE(MM_Horde_PursuitStalled(false, 0.f, 24.f));
	MM_CHECK(MM_Horde_PursuitStalled(true, 8.f, 24.f));
	MM_CHECK_FALSE(MM_Horde_PursuitStalled(true, 24.f, 24.f));
	MM_CHECK_FALSE(MM_Horde_PursuitStalled(true,
		std::numeric_limits<float>::quiet_NaN(), 24.f));

	// A goal within slack of the fighter is still good; drift or garbage re-pins it.
	MM_CHECK_FALSE(MM_Horde_ShouldRepinPursuitGoal(64.f, 128.f));
	MM_CHECK(MM_Horde_ShouldRepinPursuitGoal(512.f, 128.f));
	MM_CHECK(MM_Horde_ShouldRepinPursuitGoal(
		std::numeric_limits<float>::infinity(), 128.f));
}

// Shared helpers for the strategy-targeting tests below.
namespace horde_strategy_test {

constexpr float kTol = 1e-4f;

inline bool Near(float lhs, float rhs, float tol = kTol) {
	return std::isfinite(lhs) && std::isfinite(rhs) && std::fabs(lhs - rhs) <= tol;
}

inline float TotalWeight(const mm_horde_strategy_weights_t &w) {
	return w.prox + w.free + w.threat + w.vuln + w.isolation;
}

inline float MaxWeight(const mm_horde_strategy_weights_t &w) {
	return std::max({ w.prox, w.free, w.threat, w.vuln, w.isolation });
}

inline mm_horde_target_terms_t UniformTerms(float value) {
	return { value, value, value, value, value };
}

} // namespace horde_strategy_test

MM_TEST(horde_legacy_role_score_is_unchanged) {
	// Pins the legacy scorer so the g_horde_target_model 0 rollback path cannot drift.
	MM_CHECK_EQ(MM_Horde_ComputeTargetLoadScore(2, 100.f, 512.f), 1124.f);

	MM_CHECK_EQ(MM_Horde_ComputeRoleTargetScore(1, 800.f, 256.f, 0.5f, 512.f, 256.f, 192.f,
		mm_horde_target_role_t::Balanced), 1312.f);
	MM_CHECK_EQ(MM_Horde_ComputeRoleTargetScore(1, 800.f, 256.f, 0.5f, 512.f, 256.f, 192.f,
		mm_horde_target_role_t::Hunter), 1248.f);
	// Lower is better in the legacy model, so a Bulwark subtracting on health prefers healthy targets.
	MM_CHECK_EQ(MM_Horde_ComputeRoleTargetScore(1, 800.f, 256.f, 0.5f, 512.f, 256.f, 192.f,
		mm_horde_target_role_t::Bulwark), 1216.f);
}

MM_TEST(horde_no_single_soft_factor_dominates) {
	using namespace horde_strategy_test;

	for (uint8_t i = 0; i < static_cast<uint8_t>(mm_horde_strategy_t::Count); i++) {
		const mm_horde_strategy_weights_t w =
			MM_Horde_StrategyWeights(static_cast<mm_horde_strategy_t>(i));

		MM_CHECK(w.prox >= 0.f && w.free >= 0.f && w.threat >= 0.f && w.vuln >= 0.f &&
			w.isolation >= 0.f);
		MM_CHECK(TotalWeight(w) > 0.f);
		MM_CHECK(w.prox_half > 0.f);
		// The shipped dominance bound: no term can move the score by more than 35%.
		MM_CHECK(MaxWeight(w) / TotalWeight(w) <= 0.35f);
	}

	// Operator tuning may deliberately bias the model, but must never produce a negative,
	// non-finite, or all-zero weight set -- that would make the score meaningless.
	const float extremes[] = { 0.f, 1.f, 2.f, -5.f, 1.0e9f,
		std::numeric_limits<float>::quiet_NaN() };
	for (uint8_t i = 0; i < static_cast<uint8_t>(mm_horde_strategy_t::Count); i++) {
		for (float spread : extremes) {
			for (float mult : extremes) {
				const mm_horde_strategy_weights_t w = MM_Horde_ApplyWeightTuning(
					MM_Horde_StrategyWeights(static_cast<mm_horde_strategy_t>(i)),
					MM_Horde_NormalizedSpreadWeight(spread, 512.f, 8.f), mult, mult);

				MM_CHECK(std::isfinite(w.prox) && std::isfinite(w.free) &&
					std::isfinite(w.threat) && std::isfinite(w.vuln) &&
					std::isfinite(w.isolation));
				MM_CHECK(w.prox >= 0.f && w.free >= 0.f && w.threat >= 0.f && w.vuln >= 0.f &&
					w.isolation >= 0.f);
				MM_CHECK(TotalWeight(w) > 0.f);
			}
		}
	}
}

MM_TEST(horde_gated_score_is_bounded_and_reach_can_veto) {
	using namespace horde_strategy_test;

	const mm_horde_strategy_weights_t balanced =
		MM_Horde_StrategyWeights(mm_horde_strategy_t::Balanced);

	MM_CHECK(Near(MM_Horde_ComputeGatedTargetScore(UniformTerms(1.f), balanced, 1.f, 0.f,
		MM_HORDE_GATE_FLOOR), 1.f));
	MM_CHECK(Near(MM_Horde_ComputeGatedTargetScore(UniformTerms(0.f), balanced, 1.f, 0.f,
		MM_HORDE_GATE_FLOOR), 0.f));

	// A weightless strategy scores zero rather than dividing by zero.
	const mm_horde_strategy_weights_t weightless = {};
	MM_CHECK_EQ(MM_Horde_ComputeCoreUtility(UniformTerms(1.f), weightless), 0.f);

	// Reachability is a real veto: a perfect but unreachable fighter loses to a mediocre
	// reachable one, for every strategy.
	for (uint8_t i = 0; i < static_cast<uint8_t>(mm_horde_strategy_t::Count); i++) {
		const mm_horde_strategy_weights_t w =
			MM_Horde_StrategyWeights(static_cast<mm_horde_strategy_t>(i));

		const float unreachable = MM_Horde_ComputeGatedTargetScore(UniformTerms(1.f), w, 0.f,
			0.f, MM_HORDE_GATE_FLOOR);
		const float reachable = MM_Horde_ComputeGatedTargetScore(UniformTerms(0.30f), w, 1.f,
			0.f, MM_HORDE_GATE_FLOOR);

		MM_CHECK(Near(unreachable, MM_HORDE_GATE_FLOOR));
		MM_CHECK(unreachable < reachable);
	}

	// Non-finite terms clamp instead of poisoning the score.
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float poisoned = MM_Horde_ComputeGatedTargetScore({ nan, nan, nan, nan, nan },
		balanced, nan, nan, nan);
	MM_CHECK(std::isfinite(poisoned) && poisoned >= 0.f && poisoned <= 1.f);
}

MM_TEST(horde_gate_sharpening_hits_heavies_hardest) {
	using namespace horde_strategy_test;

	const float floor_value = MM_HORDE_GATE_FLOOR;

	for (float a = 0.f; a <= 1.0f; a += 0.125f) {
		// sharpen 0 is linear in access; sharpen 1 is the squared form.
		MM_CHECK(Near(MM_Horde_GateFromAccess(a, 0.f, floor_value),
			floor_value + (1.f - floor_value) * a));
		MM_CHECK(Near(MM_Horde_GateFromAccess(a, 1.f, floor_value),
			floor_value + (1.f - floor_value) * a * a));
	}

	// Endpoints are shared by every sharpening, so a heavy is never penalised on a fully
	// reachable or fully unreachable fighter -- only in between.
	for (float s = 0.f; s <= 1.f; s += 0.25f) {
		MM_CHECK(Near(MM_Horde_GateFromAccess(0.f, s, floor_value), floor_value));
		MM_CHECK(Near(MM_Horde_GateFromAccess(1.f, s, floor_value), 1.f));
	}

	// Monotone increasing in access, monotone decreasing in sharpening.
	MM_CHECK(MM_Horde_GateFromAccess(0.25f, 0.5f, floor_value) <
		MM_Horde_GateFromAccess(0.75f, 0.5f, floor_value));
	MM_CHECK(MM_Horde_GateFromAccess(0.5f, 1.f, floor_value) <
		MM_Horde_GateFromAccess(0.5f, 0.f, floor_value));

	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float guarded = MM_Horde_GateFromAccess(nan, nan, nan);
	MM_CHECK(std::isfinite(guarded) && guarded >= 0.f && guarded <= 1.f);
}

MM_TEST(horde_proximity_utility_is_monotone_and_bounded) {
	using namespace horde_strategy_test;

	MM_CHECK(Near(MM_Horde_ProximityUtility(0.f, 768.f), 1.f));
	MM_CHECK(Near(MM_Horde_ProximityUtility(768.f, 768.f), 0.5f));

	// Strictly decreasing and never saturating, which is why distance no longer swamps
	// every other consideration at range.
	MM_CHECK(MM_Horde_ProximityUtility(0.f, 768.f) > MM_Horde_ProximityUtility(768.f, 768.f));
	MM_CHECK(MM_Horde_ProximityUtility(768.f, 768.f) > MM_Horde_ProximityUtility(2048.f, 768.f));
	MM_CHECK(MM_Horde_ProximityUtility(2048.f, 768.f) > MM_Horde_ProximityUtility(8000.f, 768.f));
	MM_CHECK(MM_Horde_ProximityUtility(1.0e6f, 768.f) > 0.f);

	const float nan = std::numeric_limits<float>::quiet_NaN();
	MM_CHECK(Near(MM_Horde_ProximityUtility(nan, 768.f), 1.f));
	MM_CHECK(Near(MM_Horde_ProximityUtility(768.f, nan), 0.5f));
}

MM_TEST(horde_load_utility_keeps_spread_dominant) {
	using namespace horde_strategy_test;

	MM_CHECK(Near(MM_Horde_LoadUtility(0, 3.f), 1.f));
	MM_CHECK(Near(MM_Horde_LoadUtility(1, 3.f), 2.f / 3.f));
	MM_CHECK(Near(MM_Horde_LoadUtility(3, 3.f), 0.f));
	MM_CHECK(Near(MM_Horde_LoadUtility(9, 3.f), 0.f));
	MM_CHECK(Near(MM_Horde_LoadUtility(-4, 3.f), 1.f));
	MM_CHECK(Near(MM_Horde_LoadUtility(1, std::numeric_limits<float>::quiet_NaN()), 2.f / 3.f));

	// Target spread must remain the dominant retarget driver, as it is today: a GENUINE extra
	// attacker has to move the score by more than the switch margin. This only holds as a
	// statement about real load differences because the incumbent's self-count is normalized
	// away first -- see horde_target_load_comparison_cancels_the_self_count below.
	const mm_horde_strategy_weights_t w = MM_Horde_StrategyWeights(mm_horde_strategy_t::Balanced);
	const float delta = (w.free / TotalWeight(w)) *
		(MM_Horde_LoadUtility(0, 3.f) - MM_Horde_LoadUtility(1, 3.f));
	MM_CHECK(delta > MM_Horde_TargetSwitchMargin(0.05f, w.switch_scale));
}

MM_TEST(horde_target_load_comparison_cancels_the_self_count) {
	using namespace horde_strategy_test;

	// The load histogram counts a monster against its own enemy, so scoring raw counts would
	// give every challenger a free one-attacker edge. Challengers are scored post-switch.
	MM_CHECK_EQ(MM_Horde_ComparableTargetLoad(2, true, true), 2);   // incumbent: already counts me
	MM_CHECK_EQ(MM_Horde_ComparableTargetLoad(2, false, true), 3);  // challenger: would gain me
	// Acquiring with no current enemy contributes to nobody, so raw counts are already fair.
	MM_CHECK_EQ(MM_Horde_ComparableTargetLoad(2, false, false), 2);
	MM_CHECK_EQ(MM_Horde_ComparableTargetLoad(0, false, false), 0);
	MM_CHECK_EQ(MM_Horde_ComparableTargetLoad(std::numeric_limits<int>::max(), false, true),
		std::numeric_limits<int>::max());

	// Two identical fighters must tie once the self-count is cancelled, so the switch margin
	// is real hysteresis. Scoring the raw counts instead flips it into a permanent switch bias
	// that exceeds the margin for every strategy, and the monster thrashes forever.
	for (uint8_t i = 0; i < static_cast<uint8_t>(mm_horde_strategy_t::Count); i++) {
		const mm_horde_strategy_weights_t w =
			MM_Horde_StrategyWeights(static_cast<mm_horde_strategy_t>(i));

		const int incumbent = MM_Horde_ComparableTargetLoad(1, true, true);
		const int challenger = MM_Horde_ComparableTargetLoad(0, false, true);
		MM_CHECK_EQ(incumbent, challenger);

		mm_horde_target_terms_t a = UniformTerms(0.5f);
		mm_horde_target_terms_t b = UniformTerms(0.5f);
		a.free = MM_Horde_LoadUtility(incumbent, 3.f);
		b.free = MM_Horde_LoadUtility(challenger, 3.f);

		const float current_score = MM_Horde_ComputeGatedTargetScore(a, w, 1.f, 0.f,
			MM_HORDE_GATE_FLOOR);
		const float best_score = MM_Horde_ComputeGatedTargetScore(b, w, 1.f, 0.f,
			MM_HORDE_GATE_FLOOR);

		MM_CHECK_FALSE(MM_Horde_ShouldSwitchTarget(best_score, current_score,
			MM_Horde_TargetSwitchMargin(0.05f, w.switch_scale), 1.f, 1.f, 0.25f, 0.55f));
	}
}

MM_TEST(horde_isolation_treats_solo_fighter_as_isolated) {
	using namespace horde_strategy_test;

	// The legacy helper reported a lone fighter as maximally grouped; it is the opposite.
	MM_CHECK(Near(MM_Horde_IsolationUtility(0.f, 0, 1024.f), 1.f));
	MM_CHECK(Near(MM_Horde_IsolationUtility(4096.f, 0, 1024.f), 1.f));

	MM_CHECK(Near(MM_Horde_IsolationUtility(0.f, 3, 1024.f), 0.f));
	MM_CHECK(Near(MM_Horde_IsolationUtility(1024.f, 3, 1024.f), 1.f));
	MM_CHECK(Near(MM_Horde_IsolationUtility(2048.f, 3, 1024.f), 1.f));

	const float nan = std::numeric_limits<float>::quiet_NaN();
	MM_CHECK(Near(MM_Horde_IsolationUtility(nan, 3, 1024.f), 0.f));
	MM_CHECK(Near(MM_Horde_IsolationUtility(512.f, 3, nan), 0.5f));
}

MM_TEST(horde_threat_and_vulnerability_are_normalized) {
	using namespace horde_strategy_test;

	MM_CHECK(Near(MM_Horde_ThreatUtility(1.f, 1.f, 1.f, 1.f, 1.f), 1.f));
	MM_CHECK(Near(MM_Horde_ThreatUtility(0.f, 0.f, 0.f, 0.f, 0.f), 0.f));
	// A quad carrier reads as more dangerous than an unbuffed one; a railgun beats a blaster.
	MM_CHECK(MM_Horde_ThreatUtility(1.f, 0.5f, 0.f, 0.f, 0.f) >
		MM_Horde_ThreatUtility(0.f, 0.5f, 0.f, 0.f, 0.f));
	MM_CHECK(MM_Horde_ThreatUtility(0.f, 0.90f, 0.f, 0.f, 0.f) >
		MM_Horde_ThreatUtility(0.f, 0.10f, 0.f, 0.f, 0.f));
	// Streak saturates, so a runaway score cannot make one fighter a permanent magnet.
	MM_CHECK_EQ(MM_Horde_ThreatUtility(0.f, 0.f, 20.f, 0.f, 0.f),
		MM_Horde_ThreatUtility(0.f, 0.f, 1.f, 0.f, 0.f));

	MM_CHECK(Near(MM_Horde_VulnerabilityUtility(1.f, 1.f, 1.f, 1.f, 1.f, false), 1.f));
	MM_CHECK(Near(MM_Horde_VulnerabilityUtility(0.f, 0.f, 0.f, 0.f, 0.f, false), 0.f));
	// A hurt fighter is a better finisher pick than a healthy one.
	MM_CHECK(MM_Horde_VulnerabilityUtility(0.8f, 0.f, 0.f, 0.f, 0.f, false) >
		MM_Horde_VulnerabilityUtility(0.0f, 0.f, 0.f, 0.f, 0.f, false));
	// Protection makes a fighter a poor finisher pick rather than an invisible one.
	MM_CHECK(Near(MM_Horde_VulnerabilityUtility(1.f, 1.f, 1.f, 1.f, 1.f, true), 0.30f));

	const float nan = std::numeric_limits<float>::quiet_NaN();
	MM_CHECK(std::isfinite(MM_Horde_ThreatUtility(nan, nan, nan, nan, nan)));
	MM_CHECK(std::isfinite(MM_Horde_VulnerabilityUtility(nan, nan, nan, nan, nan, false)));
}

MM_TEST(horde_climb_budget_matches_monster_traversal) {
	using namespace horde_strategy_test;

	MM_CHECK(Near(MM_Horde_ClimbBudget(true, false, false, 0.f, 0.f, 18.f), 1024.f));
	MM_CHECK(Near(MM_Horde_ClimbBudget(false, true, false, 0.f, 0.f, 18.f), 320.f));

	// A tank sets no jump or drop height anywhere in the tree, so it runs on a bare step.
	MM_CHECK(Near(MM_Horde_ClimbBudget(false, false, false, 0.f, 0.f, 18.f), 18.f));
	MM_CHECK(Near(MM_Horde_ClimbBudget(false, false, true, 68.f, 256.f, 18.f), 274.f));
	MM_CHECK(Near(MM_Horde_ClimbBudget(false, false, true, 40.f, 192.f, 18.f), 210.f));
	// Traversal heights only count when the monster can actually jump, mirroring the engine.
	MM_CHECK(Near(MM_Horde_ClimbBudget(false, false, false, 68.f, 256.f, 18.f), 18.f));

	const float nan = std::numeric_limits<float>::quiet_NaN();
	MM_CHECK(Near(MM_Horde_ClimbBudget(false, false, true, nan, nan, 18.f), 18.f));
}

MM_TEST(horde_vertical_access_believes_in_ramps) {
	using namespace horde_strategy_test;

	// Tank: 18-unit climb budget, 0.79 slope after size modulation.
	const float tank_budget = 18.f, tank_slope = 0.79f;

	// A long ramp is walkable, so height alone must not veto the fighter at the top of it.
	MM_CHECK(Near(MM_Horde_VerticalAccess(128.f, 400.f, tank_budget, tank_slope, 512.f), 1.f));
	MM_CHECK(Near(MM_Horde_VerticalAccess(400.f, 1000.f, tank_budget, tank_slope, 512.f), 1.f));
	// A sheer drop right next to the monster is not.
	const float ledge = MM_Horde_VerticalAccess(128.f, 60.f, tank_budget, tank_slope, 512.f);
	const float storey = MM_Horde_VerticalAccess(400.f, 60.f, tank_budget, tank_slope, 512.f);
	MM_CHECK(ledge > 0.85f && ledge < 0.90f);
	MM_CHECK(storey > 0.30f && storey < 0.40f);
	MM_CHECK(storey < ledge);

	// A gekk clears the same ledge outright.
	MM_CHECK(Near(MM_Horde_VerticalAccess(128.f, 60.f, 274.f, 1.f, 512.f), 1.f));
	// A supertank's gentler slope is strictly less permissive than a tank's.
	MM_CHECK(MM_Horde_VerticalAccess(128.f, 60.f, tank_budget, 0.50f, 512.f) < ledge);

	// Height below reads the same as height above.
	MM_CHECK_EQ(MM_Horde_VerticalAccess(-400.f, 60.f, tank_budget, tank_slope, 512.f), storey);

	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float guarded = MM_Horde_VerticalAccess(nan, nan, nan, nan, nan);
	MM_CHECK(std::isfinite(guarded) && guarded >= 0.f && guarded <= 1.f);
}

MM_TEST(horde_hull_bulk_and_slope_scale_with_size) {
	using namespace horde_strategy_test;

	// Real hull widths: soldier 32, guncmdr 40, tank 64, arachnid 96, supertank 128.
	MM_CHECK(Near(MM_Horde_HullBulk(32.f, 40.f, 96.f), 0.f));
	MM_CHECK(Near(MM_Horde_HullBulk(40.f, 40.f, 96.f), 0.f));
	MM_CHECK(Near(MM_Horde_HullBulk(64.f, 40.f, 96.f), 24.f / 56.f));
	MM_CHECK(Near(MM_Horde_HullBulk(96.f, 40.f, 96.f), 1.f));
	MM_CHECK(Near(MM_Horde_HullBulk(192.f, 40.f, 96.f), 1.f));
	MM_CHECK(Near(MM_Horde_HullBulk(std::numeric_limits<float>::quiet_NaN(), 40.f, 96.f), 0.f));
	// Inverted bounds are swapped rather than producing a negative span.
	MM_CHECK(Near(MM_Horde_HullBulk(64.f, 96.f, 40.f), 24.f / 56.f));

	MM_CHECK(Near(MM_Horde_ClimbSlope(1.f, 0.f), 1.f));
	MM_CHECK(Near(MM_Horde_ClimbSlope(1.f, 1.f), 0.5f));
	MM_CHECK(MM_Horde_ClimbSlope(1.f, 1.f) < MM_Horde_ClimbSlope(1.f, 0.5f));
	MM_CHECK(MM_Horde_ClimbSlope(1.f, 0.5f) < MM_Horde_ClimbSlope(1.f, 0.f));

	// A bulky body stops chasing stragglers and grows less distance-sensitive.
	const mm_horde_strategy_weights_t base =
		MM_Horde_StrategyWeights(mm_horde_strategy_t::Balanced);
	const mm_horde_strategy_weights_t heavy = MM_Horde_ApplySizeToWeights(base, 1.f);
	MM_CHECK(heavy.isolation < base.isolation);
	MM_CHECK(heavy.prox < base.prox);
	MM_CHECK(heavy.prox_half > base.prox_half);
	MM_CHECK(heavy.isolation >= 0.f && std::isfinite(heavy.isolation));
	MM_CHECK(TotalWeight(heavy) > 0.f);

	// Gate sharpening rises with bulk and falls with agility, so a flyer is barely affected.
	MM_CHECK(MM_Horde_GateSharpen(0.35f, 1.f, 0.07f) > MM_Horde_GateSharpen(0.35f, 0.f, 0.07f));
	MM_CHECK(Near(MM_Horde_GateSharpen(0.10f, 1.f, 1.f), 0.10f));
	MM_CHECK(Near(MM_Horde_GateSharpen(-0.35f, 0.f, 1.f), 0.f));
}

MM_TEST(horde_access_is_a_bounded_feasibility_product) {
	using namespace horde_strategy_test;

	MM_CHECK(Near(MM_Horde_ComputeAccess(1.f, 1.f, 1.f, 1.f, 1.f, false, 0.90f), 1.f));

	// Any single impossible factor collapses the product.
	MM_CHECK(Near(MM_Horde_ComputeAccess(0.f, 1.f, 1.f, 1.f, 1.f, false, 0.90f), 0.f));
	MM_CHECK(Near(MM_Horde_ComputeAccess(1.f, 0.f, 1.f, 1.f, 1.f, false, 0.90f), 0.f));
	MM_CHECK(Near(MM_Horde_ComputeAccess(1.f, 1.f, 0.f, 1.f, 1.f, false, 0.90f), 0.f));

	// Stacked discounts compound but stay positive.
	const float stacked = MM_Horde_ComputeAccess(0.30f, 1.f, 0.15f, 0.45f, 1.f, false, 0.90f);
	MM_CHECK(stacked > 0.f && stacked < 0.03f);

	// An enemy we are in contact with is reachable by definition.
	MM_CHECK(Near(MM_Horde_ComputeAccess(0.f, 0.f, 0.f, 0.f, 0.f, true, 0.90f), 0.90f));

	const float nan = std::numeric_limits<float>::quiet_NaN();
	MM_CHECK(Near(MM_Horde_ComputeAccess(nan, nan, nan, nan, nan, false, nan), 1.f));
}

MM_TEST(horde_unreach_mask_remembers_multiple_fighters) {
	using namespace horde_strategy_test;

	uint64_t lo = 0, hi = 0;
	MM_Horde_UnreachMaskSet(lo, hi, 3);
	MM_Horde_UnreachMaskSet(lo, hi, 70);
	MM_CHECK(MM_Horde_UnreachMaskTest(lo, hi, 3));
	MM_CHECK(MM_Horde_UnreachMaskTest(lo, hi, 70));
	MM_CHECK_FALSE(MM_Horde_UnreachMaskTest(lo, hi, 4));
	MM_CHECK_FALSE(MM_Horde_UnreachMaskTest(lo, hi, 69));

	// Out-of-range slots are inert in both directions.
	MM_Horde_UnreachMaskSet(lo, hi, 0);
	MM_Horde_UnreachMaskSet(lo, hi, 129);
	MM_Horde_UnreachMaskSet(lo, hi, -1);
	MM_CHECK_FALSE(MM_Horde_UnreachMaskTest(lo, hi, 0));
	MM_CHECK_FALSE(MM_Horde_UnreachMaskTest(lo, hi, 129));
	MM_CHECK_FALSE(MM_Horde_UnreachMaskTest(lo, hi, -1));

	// The anti-oscillation property: clearing one fighter must not clear another, or a
	// monster ping-pongs forever between two fighters it cannot reach.
	MM_Horde_UnreachMaskClear(lo, hi, 3);
	MM_CHECK_FALSE(MM_Horde_UnreachMaskTest(lo, hi, 3));
	MM_CHECK(MM_Horde_UnreachMaskTest(lo, hi, 70));

	// Boundary slots of each word.
	uint64_t blo = 0, bhi = 0;
	MM_Horde_UnreachMaskSet(blo, bhi, 1);
	MM_Horde_UnreachMaskSet(blo, bhi, 64);
	MM_Horde_UnreachMaskSet(blo, bhi, 65);
	MM_Horde_UnreachMaskSet(blo, bhi, 128);
	MM_CHECK(MM_Horde_UnreachMaskTest(blo, bhi, 1) && MM_Horde_UnreachMaskTest(blo, bhi, 64));
	MM_CHECK(MM_Horde_UnreachMaskTest(blo, bhi, 65) && MM_Horde_UnreachMaskTest(blo, bhi, 128));

	// Fresh evidence is a near-veto that decays back to neutral so the monster retries.
	MM_CHECK(Near(MM_Horde_UnreachFactor(true, 0, 12000, 0.12f), 0.12f));
	MM_CHECK(Near(MM_Horde_UnreachFactor(true, 6000, 12000, 0.12f), 0.56f));
	MM_CHECK(Near(MM_Horde_UnreachFactor(true, 12000, 12000, 0.12f), 1.f));
	MM_CHECK(Near(MM_Horde_UnreachFactor(false, 0, 12000, 0.12f), 1.f));
	MM_CHECK(Near(MM_Horde_UnreachFactor(true, -5, 12000, 0.12f), 1.f));
	MM_CHECK(Near(MM_Horde_UnreachFactor(true, 0, 0, 0.12f), 1.f));
	MM_CHECK(MM_Horde_UnreachFactor(true, 3000, 12000, 0.12f) <
		MM_Horde_UnreachFactor(true, 9000, 12000, 0.12f));
}

MM_TEST(horde_spread_weight_maps_onto_a_normalized_weight) {
	using namespace horde_strategy_test;

	// The cvar keeps its name and its 512 default, but now scales a bounded weight.
	MM_CHECK(Near(MM_Horde_NormalizedSpreadWeight(512.f, 512.f, 8.f), 1.f));
	MM_CHECK(Near(MM_Horde_NormalizedSpreadWeight(0.f, 512.f, 8.f), 0.f));
	MM_CHECK(Near(MM_Horde_NormalizedSpreadWeight(1024.f, 512.f, 8.f), 2.f));
	MM_CHECK(Near(MM_Horde_NormalizedSpreadWeight(1.0e30f, 512.f, 8.f), 8.f));
	MM_CHECK(Near(MM_Horde_NormalizedSpreadWeight(-100.f, 512.f, 8.f), 0.f));
	MM_CHECK(Near(MM_Horde_NormalizedSpreadWeight(
		std::numeric_limits<float>::quiet_NaN(), 512.f, 8.f), 1.f));

	// Zeroing the cvar still means "ignore target load entirely", as documented.
	const mm_horde_strategy_weights_t w = MM_Horde_ApplyWeightTuning(
		MM_Horde_StrategyWeights(mm_horde_strategy_t::Balanced),
		MM_Horde_NormalizedSpreadWeight(0.f, 512.f, 8.f), 1.f, 1.f);
	MM_CHECK_EQ(w.free, 0.f);
	MM_CHECK(TotalWeight(w) > 0.f);
}

MM_TEST(horde_switch_margin_prevents_thrashing_and_releases_traps) {
	using namespace horde_strategy_test;

	MM_CHECK(Near(MM_Horde_TargetSwitchMargin(0.05f, 1.0f), 0.05f));
	MM_CHECK(Near(MM_Horde_TargetSwitchMargin(0.05f, 1.6f), 0.08f));
	MM_CHECK(Near(MM_Horde_TargetSwitchMargin(0.05f, 0.75f), 0.0375f));
	MM_CHECK(Near(MM_Horde_TargetSwitchMargin(0.05f,
		std::numeric_limits<float>::quiet_NaN()), 0.05f));

	// A marginally better candidate does not steal the target.
	MM_CHECK_FALSE(MM_Horde_ShouldSwitchTarget(0.50f, 0.47f, 0.05f, 1.f, 1.f, 0.25f, 0.55f));
	// A clearly better one does.
	MM_CHECK(MM_Horde_ShouldSwitchTarget(0.56f, 0.47f, 0.05f, 1.f, 1.f, 0.25f, 0.55f));
	// An incumbent the monster provably cannot reach waives the margin -- staying latched
	// onto an unreachable fighter is the failure this model exists to fix.
	MM_CHECK(MM_Horde_ShouldSwitchTarget(0.48f, 0.47f, 0.05f, 0.10f, 0.80f, 0.25f, 0.55f));
	// ...but only when there is a genuinely reachable rescue candidate.
	MM_CHECK_FALSE(MM_Horde_ShouldSwitchTarget(0.48f, 0.47f, 0.05f, 0.10f, 0.30f, 0.25f, 0.55f));
	// Never switch on a worse score, even with the override armed.
	MM_CHECK_FALSE(MM_Horde_ShouldSwitchTarget(0.40f, 0.47f, 0.05f, 0.10f, 0.80f, 0.25f, 0.55f));

	const float nan = std::numeric_limits<float>::quiet_NaN();
	MM_CHECK_FALSE(MM_Horde_ShouldSwitchTarget(nan, 0.47f, 0.05f, 1.f, 1.f, 0.25f, 0.55f));
	MM_CHECK_FALSE(MM_Horde_ShouldSwitchTarget(0.56f, nan, 0.05f, 1.f, 1.f, 0.25f, 0.55f));
}

MM_TEST(horde_boss_wave_cadence_is_configurable) {
	MM_CHECK_FALSE(MM_Horde_IsBossWave(5, 6, 6));
	MM_CHECK(MM_Horde_IsBossWave(6, 6, 6));
	MM_CHECK_FALSE(MM_Horde_IsBossWave(7, 6, 6));
	MM_CHECK(MM_Horde_IsBossWave(12, 6, 6));
	MM_CHECK_FALSE(MM_Horde_IsBossWave(12, 6, 0));
}

MM_TEST(horde_midwave_spawns_preserve_remaining_lives) {
	MM_CHECK(MM_Horde_ShouldEliminateMidWaveSpawn(true, false, 0));
	MM_CHECK_FALSE(MM_Horde_ShouldEliminateMidWaveSpawn(true, false, 1));
	MM_CHECK_FALSE(MM_Horde_ShouldEliminateMidWaveSpawn(true, true, 1));
	MM_CHECK_FALSE(MM_Horde_ShouldEliminateMidWaveSpawn(false, false, 0));
}

MM_TEST(horde_converted_monsters_preserve_skill_inhibition) {
	MM_CHECK(MM_Horde_SourceMonsterInhibitedBySkill(0, true, false, false));
	MM_CHECK_FALSE(MM_Horde_SourceMonsterInhibitedBySkill(0, false, true, true));
	MM_CHECK(MM_Horde_SourceMonsterInhibitedBySkill(1, false, true, false));
	MM_CHECK_FALSE(MM_Horde_SourceMonsterInhibitedBySkill(1, true, false, true));
	MM_CHECK(MM_Horde_SourceMonsterInhibitedBySkill(2, false, false, true));
	MM_CHECK(MM_Horde_SourceMonsterInhibitedBySkill(3, false, false, true));
	MM_CHECK_FALSE(MM_Horde_SourceMonsterInhibitedBySkill(3, true, true, false));
}

MM_TEST(horde_hud_counters_do_not_wrap_signed_stats) {
	MM_CHECK_EQ(MM_Horde_HudCounterValue(-1), 0);
	MM_CHECK_EQ(MM_Horde_HudCounterValue(42), 42);
	MM_CHECK_EQ(MM_Horde_HudCounterValue(std::numeric_limits<int16_t>::max()),
		std::numeric_limits<int16_t>::max());
	MM_CHECK_EQ(MM_Horde_HudCounterValue(std::numeric_limits<int>::max()),
		std::numeric_limits<int16_t>::max());
}

MM_TEST(horde_burst_and_reinforcement_thresholds_are_bounded) {
	MM_CHECK_FALSE(MM_Horde_ShouldRestAfterSpawn(5, 6));
	MM_CHECK(MM_Horde_ShouldRestAfterSpawn(6, 6));
	MM_CHECK_FALSE(MM_Horde_ShouldRestAfterSpawn(100, 0));

	MM_CHECK_FALSE(MM_Horde_ReinforcementReady(11, 12, 0, 1));
	MM_CHECK(MM_Horde_ReinforcementReady(12, 12, 0, 1));
	MM_CHECK_FALSE(MM_Horde_ReinforcementReady(12, 12, 1, 1));
	MM_CHECK_FALSE(MM_Horde_ReinforcementReady(12, 12, 0, 0));

	MM_CHECK_FALSE(MM_Horde_WaveCleared(false, 0));
	MM_CHECK_FALSE(MM_Horde_WaveCleared(true, 1));
	MM_CHECK(MM_Horde_WaveCleared(true, 0));

	MM_CHECK_FALSE(MM_Horde_StallRecoveryDue(false, 1, 90'000, 0, 90.f));
	MM_CHECK_FALSE(MM_Horde_StallRecoveryDue(true, 0, 90'000, 0, 90.f));
	MM_CHECK_FALSE(MM_Horde_StallRecoveryDue(true, 1, 89'999, 0, 90.f));
	MM_CHECK(MM_Horde_StallRecoveryDue(true, 1, 90'000, 0, 90.f));
	MM_CHECK_FALSE(MM_Horde_StallRecoveryDue(true, 1, 90'000, 0, 0.f));
	MM_CHECK_FALSE(MM_Horde_StallRecoveryDue(true, 1, 1'000, 2'000, 1.f));
	MM_CHECK(MM_Horde_StallRecoveryDue(true, 1, 90'000, 0,
		std::numeric_limits<float>::quiet_NaN()));
}

MM_TEST(horde_performance_tiers_scale_score_and_drop_momentum) {
	MM_CHECK_EQ(MM_Horde_PerformanceTier(0, 5, 3), 0);
	MM_CHECK_EQ(MM_Horde_PerformanceTier(4, 5, 3), 0);
	MM_CHECK_EQ(MM_Horde_PerformanceTier(5, 5, 3), 1);
	MM_CHECK_EQ(MM_Horde_PerformanceTier(14, 5, 3), 2);
	MM_CHECK_EQ(MM_Horde_PerformanceTier(99, 5, 3), 3);
	MM_CHECK_EQ(MM_Horde_PerformanceTier(4, 0, 3), 3);
	MM_CHECK_EQ(MM_Horde_PerformanceTier(10, 5, 0), 0);

	MM_CHECK_EQ(MM_Horde_DropChance(0.35f, 0.08f, 0), 0.35f);
	MM_CHECK_EQ(MM_Horde_DropChance(0.35f, 0.08f, 3), 0.59f);
	MM_CHECK_EQ(MM_Horde_DropChance(0.9f, 0.2f, 3), 1.0f);
	MM_CHECK_EQ(MM_Horde_DropChance(-1.0f, 0.2f, 0), 0.0f);
	MM_CHECK_EQ(MM_Horde_Probability(std::numeric_limits<float>::quiet_NaN(), 0.35f), 0.35f);
	MM_CHECK_EQ(MM_Horde_Probability(std::numeric_limits<float>::infinity(), 0.35f), 0.35f);
}

MM_TEST(horde_progression_ramps_roster_breadth_and_unlock_showcases) {
	MM_CHECK_EQ(MM_Horde_EffectiveMinTypes(3, 1, 3, 12), 3);
	MM_CHECK_EQ(MM_Horde_EffectiveMinTypes(3, 4, 3, 12), 4);
	MM_CHECK_EQ(MM_Horde_EffectiveMinTypes(3, 10, 3, 12), 6);
	MM_CHECK_EQ(MM_Horde_EffectiveMinTypes(3, 20, 3, 5), 5);
	MM_CHECK_EQ(MM_Horde_EffectiveMinTypes(3, 20, 0, 12), 3);
	MM_CHECK_EQ(MM_Horde_EffectiveMinTypes(3, 20, 3, 0), 0);

	MM_CHECK_EQ(MM_Horde_MonsterUnlockWave(-1), 1);
	MM_CHECK_EQ(MM_Horde_MonsterUnlockWave(1), 1);
	MM_CHECK_EQ(MM_Horde_MonsterUnlockWave(8), 8);
}

MM_TEST(horde_wildcard_policy_is_opt_in_and_bounded) {
	MM_CHECK_EQ(MM_Horde_PresetWeight(-1), 0);
	MM_CHECK_EQ(MM_Horde_PresetWeight(7), 7);
	MM_CHECK_EQ(MM_Horde_PresetWeight(999), 12);

	MM_CHECK_FALSE(MM_Horde_ShouldSelectPreset(false, false, 0.f, 0.f));
	MM_CHECK_FALSE(MM_Horde_ShouldSelectPreset(true, false, 1.f, 0.f));
	MM_CHECK(MM_Horde_ShouldSelectPreset(true, true, 1.f, 0.f));
	MM_CHECK(MM_Horde_ShouldSelectPreset(false, false, 0.1f, 0.099f));
	MM_CHECK_FALSE(MM_Horde_ShouldSelectPreset(false, false, 0.1f, 0.1f));
	MM_CHECK_FALSE(MM_Horde_ShouldSelectPreset(false, false,
		std::numeric_limits<float>::quiet_NaN(), 0.f));
	MM_CHECK_FALSE(MM_Horde_ShouldSelectPreset(false, false, 1.f,
		std::numeric_limits<float>::quiet_NaN()));

	MM_CHECK_EQ(MM_Horde_PresetEntityScale(0.1f), 0.5f);
	MM_CHECK_EQ(MM_Horde_PresetEntityScale(0.65f), 0.65f);
	MM_CHECK_EQ(MM_Horde_PresetEntityScale(2.f), 1.5f);
	MM_CHECK_EQ(MM_Horde_PresetEntityScale(
		std::numeric_limits<float>::infinity()), 1.f);
}

MM_TEST(horde_boss_tier_window_keeps_late_bosses_progressive) {
	MM_CHECK(MM_Horde_BossWithinTierWindow(9, 12, 3));
	MM_CHECK(MM_Horde_BossWithinTierWindow(12, 12, 3));
	MM_CHECK_FALSE(MM_Horde_BossWithinTierWindow(8, 12, 3));
	MM_CHECK_FALSE(MM_Horde_BossWithinTierWindow(1, 12, 3));
	MM_CHECK(MM_Horde_BossWithinTierWindow(12, 14, 2));
	MM_CHECK_FALSE(MM_Horde_BossWithinTierWindow(11, 14, 2));

	// A profile that unlocked after the previous scheduled boss wave gets one
	// draw even if the newest tier has already moved beyond the carryover window.
	MM_CHECK(MM_Horde_BossInSelectionBand(8, 12, 3, 12, 6, 6));
	MM_CHECK_FALSE(MM_Horde_BossInSelectionBand(6, 12, 3, 12, 6, 6));
	MM_CHECK(MM_Horde_BossInSelectionBand(14, 14, 2, 18, 6, 6));
	MM_CHECK(MM_Horde_BossInSelectionBand(6, 10, 1, 10, 10, 6));
}

MM_TEST(horde_boss_encounter_profiles_bound_pairs_scale_and_endless_growth) {
	MM_CHECK_EQ(MM_Horde_EffectiveBossUnits(2, true, 2), 2);
	MM_CHECK_EQ(MM_Horde_EffectiveBossUnits(3, true, 2), 2);
	MM_CHECK_EQ(MM_Horde_EffectiveBossUnits(2, false, 2), 1);
	MM_CHECK_EQ(MM_Horde_EffectiveBossUnits(1, true, 2), 1);
	MM_CHECK(MM_Horde_BossPlacementSufficient(1, 0, 1));
	MM_CHECK(MM_Horde_BossPlacementSufficient(1, 1, 2));
	MM_CHECK(MM_Horde_BossPlacementSufficient(0, 2, 2));
	MM_CHECK_FALSE(MM_Horde_BossPlacementSufficient(0, 1, 2));
	MM_CHECK_FALSE(MM_Horde_BossPlacementSufficient(1, 0, 2));

	MM_CHECK_EQ(MM_Horde_EffectiveBossScale(5.5f, 0.f, 2.5f), 2.5f);
	MM_CHECK_EQ(MM_Horde_EffectiveBossScale(1.25f, 0.8f, 2.5f), 0.8f);
	MM_CHECK_EQ(MM_Horde_EffectiveBossScale(0.f, 0.f, 0.f), 1.f);
	MM_CHECK_EQ(MM_Horde_EffectiveBossScale(-1.f, -1.f, 0.f), 1.f);

	MM_CHECK_EQ(MM_Horde_BossWaveMultiplier(12, 12, 0.05f), 1.f);
	MM_CHECK_EQ(MM_Horde_BossWaveMultiplier(18, 12, 0.05f), 1.3f);
	MM_CHECK_EQ(MM_Horde_BossWaveMultiplier(18, 12, -1.f), 1.f);
	MM_CHECK_EQ(MM_Horde_BossWaveMultiplier(std::numeric_limits<int>::max(), 1,
		std::numeric_limits<float>::infinity()), 1.f);
	MM_CHECK_EQ(MM_Horde_BossWaveMultiplier(std::numeric_limits<int>::max(), 1,
		std::numeric_limits<float>::max()), MM_HORDE_MAX_COMBAT_MULTIPLIER);
	MM_CHECK_EQ(MM_Horde_EffectiveBossScale(std::numeric_limits<float>::infinity(), 0.f, 0.f), 1.f);
	MM_CHECK_EQ(MM_Horde_EffectiveBossScale(1.f, std::numeric_limits<float>::max(), 0.f),
		MM_HORDE_MAX_BOSS_SCALE);

	MM_CHECK_FALSE(MM_Horde_BossEncounterDefeated(true, 0));
	MM_CHECK_FALSE(MM_Horde_BossEncounterDefeated(false, 1));
	MM_CHECK(MM_Horde_BossEncounterDefeated(false, 0));
}

MM_TEST(horde_outgoing_damage_scale_supports_buffs_and_reductions) {
	MM_CHECK_EQ(MM_Horde_ScaleOutgoingDamage(100, 0.f), 100);
	MM_CHECK_EQ(MM_Horde_ScaleOutgoingDamage(100, 1.f), 100);
	MM_CHECK_EQ(MM_Horde_ScaleOutgoingDamage(100, 0.5f), 50);
	MM_CHECK_EQ(MM_Horde_ScaleOutgoingDamage(100, 1.25f), 125);
	MM_CHECK_EQ(MM_Horde_ScaleOutgoingDamage(1, 0.1f), 1);
	MM_CHECK_EQ(MM_Horde_ScaleOutgoingDamage(0, 2.f), 0);
	MM_CHECK_EQ(MM_Horde_ScaleOutgoingDamage(100, std::numeric_limits<float>::infinity()), 100);
	MM_CHECK_EQ(MM_Horde_ScaleOutgoingDamage(100, std::numeric_limits<float>::max()),
		std::numeric_limits<int>::max());
}

MM_TEST(horde_adaptive_spawn_mult_bounds_and_direction) {
	MM_CHECK_EQ(MM_Horde_ClampAdaptiveSpawnMult(2.f), 1.35f);
	MM_CHECK_EQ(MM_Horde_ClampAdaptiveSpawnMult(0.1f), 0.65f);
	MM_CHECK_EQ(MM_Horde_ClampAdaptiveSpawnMult(std::numeric_limits<float>::quiet_NaN()), 1.f);

	const float coasting = MM_Horde_ComputeAdaptiveSpawnMult(0.95f, 0.1f, 1.5f);
	const float struggling = MM_Horde_ComputeAdaptiveSpawnMult(0.2f, 0.9f, 0.3f);
	MM_CHECK(coasting > 1.f);
	MM_CHECK(struggling < 1.f);
	MM_CHECK_EQ(MM_Horde_ComputeAdaptiveBudgetMult(1.f), 1.f);
}

MM_TEST(horde_late_escalation_budget_factor) {
	MM_CHECK_EQ(MM_Horde_EffectiveLateWaveFactor(false, 0.35f, 0.6f), 0.35f);
	MM_CHECK_EQ(MM_Horde_EffectiveLateWaveFactor(true, 0.35f, 0.6f), 0.6f);
}

MM_TEST(horde_late_escalation_max_alive_cap) {
	const int base = 60;
	const int peak = 12;

	MM_CHECK_EQ(MM_Horde_LateMaxAlive(base, 12, peak, 2, 70, false), 60);
	MM_CHECK_EQ(MM_Horde_LateMaxAlive(base, 50, peak, 2, 70, false), 60);
	MM_CHECK_EQ(MM_Horde_LateMaxAlive(base, 12, peak, 2, 70, true), 60);
	MM_CHECK_EQ(MM_Horde_LateMaxAlive(base, 15, peak, 2, 70, true), 66);
	MM_CHECK_EQ(MM_Horde_LateMaxAlive(base, 17, peak, 2, 70, true), 70);
	MM_CHECK_EQ(MM_Horde_LateMaxAlive(base, 18, peak, 2, 70, true), 70);
	MM_CHECK_EQ(MM_Horde_LateMaxAlive(base, 19, peak, 2, 72, true), 72);
	MM_CHECK_EQ(MM_Horde_LateMaxAlive(std::numeric_limits<int>::max(),
		std::numeric_limits<int>::max(), 0, std::numeric_limits<int>::max(),
		std::numeric_limits<int>::max(), true), std::numeric_limits<int>::max());
	MM_CHECK_EQ(MM_Horde_LateMaxAlive(-10, 20, peak, 2, 70, true), 0);
	MM_CHECK_EQ(MM_Horde_LateMaxAlive(base, 20, peak, -2, 70, true), base);
	MM_CHECK_EQ(MM_Horde_LateMaxAlive(base, 20, peak, 2, 10, true), base);
}

MM_TEST(horde_budget_math_and_integer_scaling_are_saturating) {
	MM_CHECK_EQ(MM_Horde_ComputeWaveBudget(12, 15, 5, 0, 0, 12, 0.6f, 1.f, 1.f), 75);
	MM_CHECK_EQ(MM_Horde_ComputeWaveBudget(18, 15, 5, 0, 0, 12, 0.6f, 1.f, 1.f), 93);
	MM_CHECK_EQ(MM_Horde_ComputeWaveBudget(std::numeric_limits<int>::max(),
		std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), 0, 0,
		std::numeric_limits<int>::max(), std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max(), std::numeric_limits<float>::max()),
		std::numeric_limits<int>::max());
	MM_CHECK_EQ(MM_Horde_ComputeWaveBudget(12, 15, 5, 0, 0, 12,
		std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::quiet_NaN()), 75);

	MM_CHECK_EQ(MM_Horde_ScaleInt(100, 1.5f, 100, 0, 1000), 150);
	MM_CHECK_EQ(MM_Horde_ScaleInt(100, std::numeric_limits<float>::infinity(), 100, 0, 1000), 100);
	MM_CHECK_EQ(MM_Horde_ScaleInt(100, std::numeric_limits<float>::infinity(), 75, 1000, 0), 75);
	MM_CHECK_EQ(MM_Horde_ScaleInt(100, 2.f, 75, 150, 0), 150);
	MM_CHECK_EQ(MM_Horde_ScaleInt(std::numeric_limits<int>::max(), 2.f, 1),
		std::numeric_limits<int>::max());
	MM_CHECK_EQ(MM_Horde_SaturatingAdd(std::numeric_limits<int>::max(), 1),
		std::numeric_limits<int>::max());
	MM_CHECK_EQ(MM_Horde_SaturatingIncrement(std::numeric_limits<int>::max()),
		std::numeric_limits<int>::max());
}

MM_TEST(loc_line_parser_extracts_position_and_multiword_label) {
	float xyz[3] = { 0.f, 0.f, 0.f };
	std::string label;

	// Real .loc files use "<x> <y> <z> <label>".
	MM_CHECK(MM_ParseLocLine("6029 435 7361 HIGH RL", xyz, label));
	MM_CHECK_EQ(xyz[0], 6029.f);
	MM_CHECK_EQ(xyz[1], 435.f);
	MM_CHECK_EQ(xyz[2], 7361.f);
	MM_CHECK_EQ(label, std::string("HIGH RL"));

	label.clear();
	MM_CHECK(MM_ParseLocLine("\t-2\t3\t4\t  ARENA  \r\n", xyz, label));
	MM_CHECK_EQ(xyz[0], -2.f);
	MM_CHECK_EQ(xyz[1], 3.f);
	MM_CHECK_EQ(xyz[2], 4.f);
	MM_CHECK_EQ(label, std::string("ARENA"));

	label.clear();
	MM_CHECK(MM_ParseLocLine("1 2 3  HIGH\tRL\tNOW  \r\n", xyz, label));
	MM_CHECK_EQ(label, std::string("HIGH RL NOW"));
}

MM_TEST(loc_line_parser_rejects_malformed_lines) {
	float xyz[3] = { 0.f, 0.f, 0.f };
	std::string label;

	MM_CHECK_FALSE(MM_ParseLocLine(nullptr, xyz, label));
	MM_CHECK_FALSE(MM_ParseLocLine("1 2 3 LABEL", nullptr, label));
	MM_CHECK_FALSE(MM_ParseLocLine("", xyz, label));
	MM_CHECK_FALSE(MM_ParseLocLine("   \r\n", xyz, label));
	MM_CHECK_FALSE(MM_ParseLocLine("1 2", xyz, label));
	MM_CHECK_FALSE(MM_ParseLocLine("1 2 3", xyz, label));
	MM_CHECK_FALSE(MM_ParseLocLine("1 2 3 \t\v\r\n", xyz, label));
	MM_CHECK_FALSE(MM_ParseLocLine("1 2 x LABEL", xyz, label));
	MM_CHECK_FALSE(MM_ParseLocLine("1 2 3x LABEL", xyz, label));
	MM_CHECK_FALSE(MM_ParseLocLine("1 2 nan LABEL", xyz, label));
	MM_CHECK_FALSE(MM_ParseLocLine("1 2 inf LABEL", xyz, label));
	MM_CHECK_FALSE(MM_ParseLocLine("1 2 1e1000 LABEL", xyz, label));
}

MM_TEST(loc_labels_are_sanitized_and_capped) {
	MM_CHECK_EQ(MM_SanitizeLocLabel("  HIGH\tRL\nNOW  "), std::string("HIGH RL NOW"));
	MM_CHECK_EQ(MM_SanitizeLocLabel("A\vB\fC"), std::string("A B C"));
	MM_CHECK(MM_SanitizeLocLabel("\t\r\n").empty());
	MM_CHECK_EQ(MM_SanitizeLocLabel(std::string(80, 'x')).size(), MM_MAX_LOC_LABEL_CHARS);
}

MM_TEST(loc_body_requires_macro_and_substitutes_location) {
	MM_CHECK(MM_BuildLocBody(nullptr, "WATER").empty());
	MM_CHECK(MM_BuildLocBody("", "WATER").empty());
	MM_CHECK(MM_BuildLocBody("need backup", "WATER").empty());

	MM_CHECK_EQ(MM_BuildLocBody("at %l", "ARENA"), std::string("at [ARENA]"));
	MM_CHECK_EQ(MM_BuildLocBody("ENEMY at %l", "MEGA"), std::string("ENEMY at [MEGA]"));
	MM_CHECK_EQ(MM_BuildLocBody("%L then %l", "RAIL"), std::string("[RAIL] then [RAIL]"));
	MM_CHECK_EQ(MM_BuildLocBody("\"hold %l\"", "BOX"), std::string("hold [BOX]"));
	MM_CHECK_EQ(MM_BuildLocBody("  at %l  ", "ARENA"), std::string("at [ARENA]"));
	MM_CHECK_EQ(MM_BuildLocBody(" \"hold %l\" ", "BOX"), std::string("hold [BOX]"));
	MM_CHECK_EQ(MM_BuildLocBody("at %l", "HIGH\tRL\nNOW"), std::string("at [HIGH RL NOW]"));

	MM_CHECK_FALSE(MM_BuildLocBody("%h %a", "WATER").empty());
	MM_CHECK_EQ(MM_BuildLocBody("at %l %h", "MEGA"), std::string("at [MEGA] %h"));
	MM_CHECK(MM_LocBodyHasMacro("need %w now"));
	MM_CHECK_FALSE(MM_LocBodyHasMacro("just plain text"));
}

MM_TEST(loc_body_caps_length) {
	std::string long_text = "%l " + std::string(400, 'x');
	const std::string body = MM_BuildLocBody(long_text.c_str(), "WATER");
	MM_CHECK_EQ(body.size(), 150u);
	MM_CHECK_EQ(body.substr(body.size() - 3), std::string("..."));
}

MM_TEST(hud_statusbar_layout_rejects_banned_ifbit_token) {
	MM_CHECK(MM_StatusbarLayoutContainsBannedToken("ifbit 14 1 loc_rstring OFFENSE endif "));
	MM_CHECK_FALSE(MM_StatusbarLayoutUsesOnlyVanillaTokens("ifbit 14 1 loc_rstring OFFENSE endif "));
}

MM_TEST(hud_statusbar_layout_vanilla_token_whitelist) {
	// Representative vanilla-base fragments including loc_rstring operands.
	const char *sample =
		"if 29 xl 0 yb -78 if 29 stat_string 29 endif "
		"if 271 xr -24 yt 34 if 271 loc_stat_rstring 271 endif "
		"if 272 xv 0 yt 48 if 272 loc_stat_rstring 272 endif "
		"if 264 xr -16 yt 42 if 264 num 3 264 endif "
		"if 47 xr -24 yt 58 loc_rstring 0 $g_lives endif "
		"if 258 xr -78 yt 68 if 258 num 3 258 endif "
		"if 258 xr -2 yt 94 loc_rstring 0 Wave endif "
		"if 265 xr -24 yt 104 loc_rstring 0 Monsters endif "
		"if 18 xl 0 yb -60 if 18 pic 18 endif endif";
	MM_CHECK_FALSE(MM_StatusbarLayoutContainsBannedToken(sample));
	MM_CHECK(MM_StatusbarLayoutUsesOnlyVanillaTokens(sample));

	MM_CHECK(MM_StatusbarLayoutUsesOnlyVanillaTokens("if 1 string2 \"USE VIEW JUMP\" endif "));
	MM_CHECK(MM_StatusbarLayoutUsesOnlyVanillaTokens("if 1 loc_rstring 0 Monsters endif "));
	MM_CHECK(MM_StatusbarLayoutUsesOnlyVanillaTokens("if 1 loc_rstring 0 Wave endif "));
	MM_CHECK_FALSE(MM_StatusbarLayoutUsesOnlyVanillaTokens("if 1 unknown_token 2 endif "));
	MM_CHECK_FALSE(MM_StatusbarLayoutUsesOnlyVanillaTokens("if not_a_stat endif "));
	MM_CHECK_FALSE(MM_StatusbarLayoutUsesOnlyVanillaTokens("string2 \"unterminated "));
	MM_CHECK_FALSE(MM_StatusbarLayoutUsesOnlyVanillaTokens("xv string_operand "));
}

MM_TEST(hud_stat_count_within_max_stats) {
	MM_CHECK((int)STAT_LAST <= (int)MAX_STATS);
	MM_CHECK((int)STAT_ARENA_ROLE < (int)MAX_STATS);
	MM_CHECK((int)STAT_SCORELIMIT < (int)MAX_STATS);
}

MM_TEST(hud_pov_configstring_lanes_do_not_overlap_global_countdown_or_each_other) {
	MM_CHECK(CONFIG_COUNTDOWN_HEADER < CONFIG_POV_CENTER_POOL);
	MM_CHECK_EQ(CONFIG_FOLLOW_PLAYER_NAME_END - CONFIG_FOLLOW_PLAYER_NAME,
		static_cast<int>(MAX_CLIENTS));
	MM_CHECK_EQ(CONFIG_POV_CENTER_SECONDARY_POOL,
		CONFIG_FOLLOW_PLAYER_NAME + static_cast<int>(MAX_LOBBY_PLAYERS));

	const int primary_first = MM_PovConfigStringForClient(
		0, MAX_LOBBY_PLAYERS, mm_pov_configstring_lane_t::Primary);
	const int primary_last = MM_PovConfigStringForClient(
		MAX_LOBBY_PLAYERS - 1, MAX_LOBBY_PLAYERS,
		mm_pov_configstring_lane_t::Primary);
	const int secondary_first = MM_PovConfigStringForClient(
		0, MAX_LOBBY_PLAYERS, mm_pov_configstring_lane_t::Secondary);
	const int secondary_last = MM_PovConfigStringForClient(
		MAX_LOBBY_PLAYERS - 1, MAX_LOBBY_PLAYERS,
		mm_pov_configstring_lane_t::Secondary);

	for (const int configstring : {
			primary_first, primary_last, secondary_first, secondary_last }) {
		MM_CHECK(configstring >= CS_GENERAL);
		MM_CHECK(configstring < CS_GENERAL + MAX_GENERAL);
		MM_CHECK(configstring != CONFIG_COUNTDOWN_HEADER);
	}
	MM_CHECK(primary_last < secondary_first || secondary_last < primary_first);
	MM_CHECK_EQ(primary_first, CONFIG_POV_CENTER_POOL);
	MM_CHECK_EQ(secondary_first, CONFIG_POV_CENTER_SECONDARY_POOL);
	MM_CHECK_EQ(MM_PovConfigStringForClient(
		MAX_LOBBY_PLAYERS, MAX_LOBBY_PLAYERS,
		mm_pov_configstring_lane_t::Primary), 0);
	MM_CHECK_EQ(MM_PovConfigStringForClient(
		MAX_LOBBY_PLAYERS, MAX_LOBBY_PLAYERS,
		mm_pov_configstring_lane_t::Secondary), 0);
	MM_CHECK_EQ(MM_PovConfigStringForClient(
		0, MAX_CLIENTS, static_cast<mm_pov_configstring_lane_t>(2)), 0);
}

MM_TEST(gametype_boundary_bounds_integer_extremes_before_effective_lookup) {
	constexpr int ffa = 1;
	constexpr int duel = 2;
	constexpr int arena = 14;

	const auto int_min = MM_ResolveGametypeBoundary(
		std::numeric_limits<int>::min(), ffa, arena, arena, ffa, false);
	MM_CHECK_EQ(int_min.configured_index, ffa);
	MM_CHECK_EQ(int_min.effective_index, ffa);
	MM_CHECK_FALSE(int_min.requested_in_range);

	const auto minus_one = MM_ResolveGametypeBoundary(
		-1, ffa, arena, arena, ffa, true);
	MM_CHECK_EQ(minus_one.configured_index, ffa);
	MM_CHECK_EQ(minus_one.effective_index, ffa);
	MM_CHECK_FALSE(minus_one.requested_in_range);

	const auto first = MM_ResolveGametypeBoundary(
		ffa, ffa, arena, arena, ffa, false);
	MM_CHECK_EQ(first.configured_index, ffa);
	MM_CHECK_EQ(first.effective_index, ffa);
	MM_CHECK(first.requested_in_range);

	const auto middle = MM_ResolveGametypeBoundary(
		duel, ffa, arena, arena, ffa, false);
	MM_CHECK_EQ(middle.configured_index, duel);
	MM_CHECK_EQ(middle.effective_index, duel);
	MM_CHECK(middle.requested_in_range);

	const auto last_inactive = MM_ResolveGametypeBoundary(
		arena, ffa, arena, arena, ffa, false);
	MM_CHECK_EQ(last_inactive.configured_index, arena);
	MM_CHECK_EQ(last_inactive.effective_index, ffa);
	MM_CHECK(last_inactive.requested_in_range);

	const auto last_active = MM_ResolveGametypeBoundary(
		arena, ffa, arena, arena, ffa, true);
	MM_CHECK_EQ(last_active.configured_index, arena);
	MM_CHECK_EQ(last_active.effective_index, arena);
	MM_CHECK(last_active.requested_in_range);

	const auto int_max_inactive = MM_ResolveGametypeBoundary(
		std::numeric_limits<int>::max(), ffa, arena, arena, ffa, false);
	MM_CHECK_EQ(int_max_inactive.configured_index, arena);
	MM_CHECK_EQ(int_max_inactive.effective_index, ffa);
	MM_CHECK_FALSE(int_max_inactive.requested_in_range);

	const auto int_max_active = MM_ResolveGametypeBoundary(
		std::numeric_limits<int>::max(), ffa, arena, arena, ffa, true);
	MM_CHECK_EQ(int_max_active.configured_index, arena);
	MM_CHECK_EQ(int_max_active.effective_index, arena);
	MM_CHECK_FALSE(int_max_active.requested_in_range);
}

MM_TEST(hud_stat_contract_miniscore_val_visibility) {
	MM_CHECK_FALSE(MM_MiniscoreValVisible(0));
	MM_CHECK(MM_MiniscoreValVisible(1));
	MM_CHECK(MM_StatusbarLayoutLengthWithinBudget(MM_STATUSBAR_LAYOUT_MAX_CHARS));
	MM_CHECK_FALSE(MM_StatusbarLayoutLengthWithinBudget(MM_STATUSBAR_LAYOUT_MAX_CHARS + 1));
}

MM_TEST(centerprint_marker_goes_after_any_leading_bind_run) {
	// Stock clients only parse binds at offset 0, so the marker must never precede them.
	MM_CHECK_EQ(MM_CenterPrintMarkerOffset(""), (size_t)0);
	MM_CHECK_EQ(MM_CenterPrintMarkerOffset("You have joined the game."), (size_t)0);
	MM_CHECK_EQ(MM_CenterPrintMarkerOffset("%bind:inven:Open menu%ready?"),
		strlen("%bind:inven:Open menu%"));
	MM_CHECK_EQ(MM_CenterPrintMarkerOffset("%bind:a%%bind:b%go"),
		strlen("%bind:a%%bind:b%"));

	// An unterminated bind is left alone rather than running off the end.
	MM_CHECK_EQ(MM_CenterPrintMarkerOffset("%bind:inven"), (size_t)0);
	MM_CHECK_EQ(MM_CenterPrintMarkerOffset("%bin"), (size_t)0);
}

MM_TEST(centerprint_marker_skips_localization_keys_and_empty_messages) {
	MM_CHECK(MM_CenterPrintBaseAcceptsMarker("You fragged someone"));
	MM_CHECK(MM_CenterPrintBaseAcceptsMarker("%bind:inven:Open menu%ready?"));

	// '$' keys are looked up whole on the client; a marker would break the lookup.
	MM_CHECK_FALSE(MM_CenterPrintBaseAcceptsMarker("$g_already_have_tech"));
	MM_CHECK_FALSE(MM_CenterPrintBaseAcceptsMarker("%bind:inven:Open menu%$g_you_need"));

	// Nothing to mark.
	MM_CHECK_FALSE(MM_CenterPrintBaseAcceptsMarker(""));
	MM_CHECK_FALSE(MM_CenterPrintBaseAcceptsMarker("%bind:inven:Open menu%"));

	// Text that already starts with a space is still marked: the client strips exactly one, so
	// the original leading space survives the round trip.
	MM_CHECK(MM_CenterPrintBaseAcceptsMarker(" leading space"));
}

MM_TEST(centerprint_marker_level_masks_broadcast_and_notify_bitflags) {
	MM_CHECK(MM_IsCenterPrintLevel(PRINT_CENTER));
	MM_CHECK(MM_IsCenterPrintLevel((print_type_t)(PRINT_CENTER | PRINT_BROADCAST)));
	MM_CHECK(MM_IsCenterPrintLevel((print_type_t)(PRINT_CENTER | PRINT_NO_NOTIFY)));

	MM_CHECK_FALSE(MM_IsCenterPrintLevel(PRINT_HIGH));
	MM_CHECK_FALSE(MM_IsCenterPrintLevel(PRINT_CHAT));
	MM_CHECK_FALSE(MM_IsCenterPrintLevel(PRINT_TYPEWRITER));
	MM_CHECK_FALSE(MM_IsCenterPrintLevel((print_type_t)(PRINT_HIGH | PRINT_BROADCAST)));
}

MM_TEST(announcer_decision_off_with_backup_plays_backup_only) {
	const announce_action_t action = MM_AnnounceDecision(false, true, false, true, false);
	MM_CHECK_FALSE(action.play_vo);
	MM_CHECK(action.play_backup);
	MM_CHECK_FALSE(action.play_sting);
}

MM_TEST(announcer_voice_pack_is_opt_in_by_default) {
	MM_CHECK_FALSE(MM_ANNOUNCER_DEFAULT_ENABLED);
}

MM_TEST(announcer_decision_off_without_backup_is_silent) {
	const announce_action_t action = MM_AnnounceDecision(false, true, false, false, false);
	MM_CHECK_FALSE(action.play_vo);
	MM_CHECK_FALSE(action.play_backup);
	MM_CHECK_FALSE(action.play_sting);
}

MM_TEST(announcer_decision_on_with_stem_plays_vo_only) {
	const announce_action_t action = MM_AnnounceDecision(true, true, false, true, false);
	MM_CHECK(action.play_vo);
	MM_CHECK_FALSE(action.play_backup);
	MM_CHECK_FALSE(action.play_sting);
}

MM_TEST(announcer_decision_on_with_stem_and_sting_plays_vo_and_sting) {
	const announce_action_t action = MM_AnnounceDecision(true, true, false, true, true);
	MM_CHECK(action.play_vo);
	MM_CHECK_FALSE(action.play_backup);
	MM_CHECK(action.play_sting);
}

MM_TEST(announcer_decision_on_null_stem_with_use_backup_plays_backup) {
	const announce_action_t action = MM_AnnounceDecision(true, false, true, true, false);
	MM_CHECK_FALSE(action.play_vo);
	MM_CHECK(action.play_backup);
	MM_CHECK_FALSE(action.play_sting);
}

MM_TEST(announcer_decision_on_null_stem_without_use_backup_is_silent) {
	const announce_action_t action = MM_AnnounceDecision(true, false, false, true, false);
	MM_CHECK_FALSE(action.play_vo);
	MM_CHECK_FALSE(action.play_backup);
	MM_CHECK_FALSE(action.play_sting);
}

MM_TEST(spawn_rules_reset_adopts_single_entity_string_copy) {
	mm_level_cpp_state_t state;
	state.vote_arg = "map q2dm1";
	state.entstring = "old_map_entities";
	state.match_id = "match-old";

	std::string lump = "{ \"classname\" \"worldspawn\" }";
	MM_ResetLevelCppState(state, std::move(lump));

	MM_CHECK(state.vote_arg.empty());
	MM_CHECK(state.match_id.empty());
	MM_CHECK_EQ(state.entstring, std::string("{ \"classname\" \"worldspawn\" }"));
	MM_CHECK_EQ(std::string(MM_EntityStringCStr(state)), state.entstring);
	MM_CHECK(lump.empty());
}

MM_TEST(spawn_rules_repeated_resets_do_not_retain_prior_strings) {
	mm_level_cpp_state_t state;

	for (int i = 0; i < 8; i++) {
		std::string lump = "entity_lump_" + std::to_string(i);
		const std::string expected = lump;
		MM_ResetLevelCppState(state, std::move(lump));
		MM_CHECK_EQ(state.entstring, expected);
		MM_CHECK(state.vote_arg.empty());
		MM_CHECK(state.match_id.empty());
	}

	MM_ResetLevelCppState(state, std::string("final"));
	MM_CHECK_EQ(state.entstring, std::string("final"));
}

MM_TEST(spawn_rules_entity_generation_advances_without_signed_overflow) {
	MM_CHECK_EQ(MM_NextEntityGeneration(0), 1);
	MM_CHECK_EQ(MM_NextEntityGeneration(-1), 0);
	MM_CHECK_EQ(
		MM_NextEntityGeneration(std::numeric_limits<int32_t>::max()),
		std::numeric_limits<int32_t>::min());
}

MM_TEST(ordnance_owner_identity_rejects_disconnect_and_client_slot_reuse) {
	constexpr int32_t captured_generation = 41;
	constexpr int captured_arena = 3;

	MM_CHECK(MM_OrdnanceGenerationMatches(
		captured_generation, true, captured_generation));
	MM_CHECK_FALSE(MM_OrdnanceGenerationMatches(
		captured_generation, false, captured_generation));
	MM_CHECK_FALSE(MM_OrdnanceGenerationMatches(
		captured_generation, true, captured_generation + 1));

	MM_CHECK(MM_OrdnanceIdentityMatches(captured_generation,
		captured_arena, true, true, captured_generation, true,
		captured_arena));
	MM_CHECK_FALSE(MM_OrdnanceIdentityMatches(captured_generation,
		captured_arena, false, false, captured_generation, true,
		captured_arena));
	MM_CHECK_FALSE(MM_OrdnanceIdentityMatches(captured_generation,
		captured_arena, true, false, captured_generation, true,
		captured_arena));
	MM_CHECK_FALSE(MM_OrdnanceIdentityMatches(captured_generation,
		captured_arena, true, true, captured_generation + 1, true,
		captured_arena));
	MM_CHECK_FALSE(MM_OrdnanceIdentityMatches(
		std::numeric_limits<int32_t>::max(), captured_arena, true, true,
		std::numeric_limits<int32_t>::min(), true, captured_arena));
}

MM_TEST(ordnance_owner_identity_keeps_origin_arena_authoritative) {
	constexpr int32_t generation = 7;

	MM_CHECK(MM_OrdnanceIdentityMatches(generation, 5, true, true,
		generation, true, 5));
	MM_CHECK_FALSE(MM_OrdnanceIdentityMatches(generation, 5, true, true,
		generation, true, 6));
	MM_CHECK_FALSE(MM_OrdnanceIdentityMatches(generation, 0, true, true,
		generation, true, 1));
	MM_CHECK(MM_OrdnanceIdentityMatches(generation, 5, true, true,
		generation, false, 6));
}

MM_TEST(spawn_rules_entity_lump_preflight_accepts_valid_world) {
	const std::string lump =
		"// cached effective map entities\n"
		"{\n"
		"\"classname\" \"worldspawn\"\n"
		"\"message\" \"Test Map\"\n"
		"}\n"
		"{ \"classname\" \"info_player_deathmatch\" \"origin\" \"0 0 24\" }\n";

	const mm_entity_lump_validation_t result = MM_ValidateEntityLump(lump, 8);
	MM_CHECK(result.valid);
	MM_CHECK_EQ(result.entity_count, 2u);
	MM_CHECK_EQ(result.error, nullptr);
}

MM_TEST(spawn_rules_entity_lump_preflight_rejects_malformed_or_unsafe_input) {
	MM_CHECK_FALSE(MM_ValidateEntityLump("", 8).valid);
	MM_CHECK_FALSE(MM_ValidateEntityLump(
		"{ \"classname\" \"info_player_deathmatch\" }", 8).valid);
	MM_CHECK_FALSE(MM_ValidateEntityLump(
		"{ \"classname\" \"worldspawn\" ", 8).valid);
	MM_CHECK_FALSE(MM_ValidateEntityLump(
		"{ \"classname\" \"worldspawn }", 8).valid);
	MM_CHECK_FALSE(MM_ValidateEntityLump(
		"{ \"classname\" \"worldspawn\" } { \"origin\" \"0 0 0\" }", 8).valid);
	MM_CHECK_FALSE(MM_ValidateEntityLump(
		"{ \"classname\" \"worldspawn\" } { \"classname\" \"worldspawn\" }", 8).valid);
	MM_CHECK_FALSE(MM_ValidateEntityLump(
		"{ \"classname\" \"worldspawn\" } { \"classname\" \"item_quad\" }", 1).valid);
	MM_CHECK_FALSE(MM_ValidateEntityLump(
		"{ \"classname\" \"WorldSpawn\" }", 8).valid);
	MM_CHECK_FALSE(MM_ValidateEntityLump(
		"{ \"classname\" \"worldspawn\" \"message\" \"}junk\" }", 8).valid);
	MM_CHECK(MM_ValidateEntityLump(
		"{ \"classname\" \"worldspawn\" }junk", 8).valid);

	std::string embedded_nul = "{ \"classname\" \"worldspawn\" }";
	embedded_nul.insert(4, 1, '\0');
	MM_CHECK_FALSE(MM_ValidateEntityLump(embedded_nul, 8).valid);
}

MM_TEST(spawn_rules_worldless_map_states_skip_the_entity_contract) {
	// The startup demo loop ("map demo1.dm2") and cinematics reach SpawnEntities
	// with an empty entity lump, which is only fatal for a real map.
	MM_CHECK_FALSE(MM_MapStateHasWorld("demo1.dm2"));
	MM_CHECK_FALSE(MM_MapStateHasWorld("DEMO2.DM2"));
	MM_CHECK_FALSE(MM_MapStateHasWorld("idlog.cin"));
	MM_CHECK_FALSE(MM_MapStateHasWorld("conback.pcx"));
	MM_CHECK_FALSE(MM_MapStateHasWorld("credits.png"));
	MM_CHECK_FALSE(MM_MapStateHasWorld("demo1.dm2$start"));

	MM_CHECK(MM_MapStateHasWorld("q2dm1"));
	MM_CHECK(MM_MapStateHasWorld("maps/q2dm1.bsp"));
	MM_CHECK(MM_MapStateHasWorld("q64/outpost"));
	MM_CHECK(MM_MapStateHasWorld("base1$start"));
	// The engine only treats these as media when a stem precedes the extension.
	MM_CHECK(MM_MapStateHasWorld(".dm2"));
	MM_CHECK(MM_MapStateHasWorld("dm2"));
	MM_CHECK(MM_MapStateHasWorld("dm2demo"));
	MM_CHECK(MM_MapStateHasWorld(""));
	MM_CHECK(MM_MapStateHasWorld(static_cast<const char *>(nullptr)));
}

MM_TEST(spawn_rules_replace_path_filename_preserves_directory) {
	char path[64] = "C:\\Games\\Quake2\\game_x64.dll";
	MM_CHECK(MM_ReplacePathFilename(path, sizeof(path), "muffmode_alloc.log"));
	MM_CHECK_EQ(std::string(path), std::string("C:\\Games\\Quake2\\muffmode_alloc.log"));

	char unix_path[64] = "/opt/quake2/game_x64.dll";
	MM_CHECK(MM_ReplacePathFilename(unix_path, sizeof(unix_path), "muffmode_alloc.log"));
	MM_CHECK_EQ(std::string(unix_path), std::string("/opt/quake2/muffmode_alloc.log"));

	char bare[32] = "game_x64.dll";
	MM_CHECK(MM_ReplacePathFilename(bare, sizeof(bare), "muffmode_alloc.log"));
	MM_CHECK_EQ(std::string(bare), std::string("muffmode_alloc.log"));

	char tiny[8] = "a\\b.dll";
	MM_CHECK_FALSE(MM_ReplacePathFilename(tiny, sizeof(tiny), "muffmode_alloc.log"));
}

MM_TEST(spawn_rules_join_directory_file_adds_separator_when_needed) {
	char out[64] = {};
	MM_CHECK(MM_JoinDirectoryFile(out, sizeof(out), "C:\\Temp", "muffmode_alloc.log"));
	MM_CHECK_EQ(std::string(out), std::string("C:\\Temp\\muffmode_alloc.log"));

	MM_CHECK(MM_JoinDirectoryFile(out, sizeof(out), "C:\\Temp\\", "muffmode_alloc.log"));
	MM_CHECK_EQ(std::string(out), std::string("C:\\Temp\\muffmode_alloc.log"));

	char tiny[8] = {};
	MM_CHECK_FALSE(MM_JoinDirectoryFile(tiny, sizeof(tiny), "C:\\Temp", "muffmode_alloc.log"));
}

MM_TEST(spawn_rules_entity_value_unescape_matches_the_legacy_grammar) {
	char out[32];

	MM_CHECK_EQ(MM_UnescapeEntityValue("plain", out, sizeof(out)), size_t{ 5 });
	MM_CHECK_EQ(std::string(out), std::string("plain"));

	MM_CHECK_EQ(MM_UnescapeEntityValue("a\\nb", out, sizeof(out)), size_t{ 3 });
	MM_CHECK_EQ(std::string(out), std::string("a\nb"));

	// Every other escape collapses to a lone backslash and drops the escaped byte.
	MM_CHECK_EQ(MM_UnescapeEntityValue("a\\tb", out, sizeof(out)), size_t{ 3 });
	MM_CHECK_EQ(std::string(out), std::string("a\\b"));

	MM_CHECK_EQ(MM_UnescapeEntityValue("a\\\\b", out, sizeof(out)), size_t{ 3 });
	MM_CHECK_EQ(std::string(out), std::string("a\\b"));

	MM_CHECK_EQ(MM_UnescapeEntityValue("", out, sizeof(out)), size_t{ 0 });
	MM_CHECK_EQ(std::string(out), std::string());
}

MM_TEST(spawn_rules_entity_value_unescape_terminates_a_trailing_backslash) {
	// The escaped byte a trailing backslash consumes is the terminator itself.
	// Expanding in place left the allocation unterminated, so every later reader
	// of that field walked off the end of it.
	char out[8];
	std::fill(std::begin(out), std::end(out), '#');

	MM_CHECK_EQ(MM_UnescapeEntityValue("a\\", out, sizeof(out)), size_t{ 2 });
	MM_CHECK_EQ(std::string(out), std::string("a\\"));

	MM_CHECK_EQ(MM_UnescapeEntityValue("\\", out, sizeof(out)), size_t{ 1 });
	MM_CHECK_EQ(std::string(out), std::string("\\"));

	// ED_NewString sizes its allocation strlen + 1; expansion must never grow.
	for (const std::string_view sample :
		{ "", "\\", "\\n", "\\\\", "a\\nb\\", "plain" }) {
		MM_CHECK(MM_UnescapedEntityValueLength(sample) <= sample.size());
		MM_CHECK_EQ(MM_UnescapedEntityValueLength(sample),
			MM_UnescapeEntityValue(sample, out, sizeof(out)));
	}
}

MM_TEST(spawn_rules_entity_value_unescape_never_writes_past_capacity) {
	char scratch[8];
	std::fill(std::begin(scratch), std::end(scratch), '#');

	MM_CHECK_EQ(MM_UnescapeEntityValue("abcdefgh", scratch, 4), size_t{ 3 });
	MM_CHECK_EQ(std::string(scratch), std::string("abc"));
	MM_CHECK_EQ(scratch[4], '#');

	std::fill(std::begin(scratch), std::end(scratch), '#');
	MM_CHECK_EQ(MM_UnescapeEntityValue("abc", scratch, 0), size_t{ 0 });
	MM_CHECK_EQ(scratch[0], '#');
	MM_CHECK_EQ(MM_UnescapeEntityValue("abc", nullptr, sizeof(scratch)), size_t{ 0 });
}

MM_TEST(spawn_rules_entity_color_packs_channels_without_overflow_or_bleed) {
	// 0..1 components scale to bytes; red lands in the sign bit of the field.
	MM_CHECK_EQ(MM_PackEntityColorRgba(std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f }),
		static_cast<int32_t>(0xffffffffu));
	MM_CHECK_EQ(MM_PackEntityColorRgba(std::array<float, 4>{ 1.0f, 0.0f, 0.0f, 1.0f }),
		static_cast<int32_t>(0xff0000ffu));
	MM_CHECK_EQ(MM_PackEntityColorRgba(std::array<float, 4>{ 0.5f, 0.0f, 0.0f, 1.0f }),
		static_cast<int32_t>(0x7f0000ffu));

	// Any component above 1 means the tuple is already authored in byte range.
	MM_CHECK_EQ(MM_PackEntityColorRgba(std::array<float, 4>{ 255.0f, 128.0f, 0.0f, 255.0f }),
		static_cast<int32_t>(0xff8000ffu));

	// Out-of-range map data clamps instead of bleeding into the neighbouring
	// channel, and never converts a float outside int32_t's range.
	MM_CHECK_EQ(MM_PackEntityColorRgba(std::array<float, 4>{ 300.0f, 2.0f, 0.0f, 0.0f }),
		static_cast<int32_t>(0xff020000u));
	MM_CHECK_EQ(MM_PackEntityColorRgba(std::array<float, 4>{ -1.0e30f, 1.0e30f, 0.0f, 0.0f }),
		static_cast<int32_t>(0x00ff0000u));
}

MM_TEST(ghost_restore_authority_comes_only_from_the_current_connection) {
	MM_CHECK_FALSE(MM_GhostRestoreAdminState(false, false));
	MM_CHECK(MM_GhostRestoreAdminState(true, false));
	MM_CHECK(MM_GhostRestoreAdminState(false, true));
	MM_CHECK(MM_GhostRestoreAdminState(true, true));
	MM_CHECK(MM_GhostReconnectPreferencesUseInstalledProfile(false));
	MM_CHECK_FALSE(MM_GhostReconnectPreferencesUseInstalledProfile(true));
}

MM_TEST(ghost_restore_epoch_rejects_state_from_another_round) {
	MM_CHECK(MM_GhostRestoreEpochMatches(0, 0));
	MM_CHECK(MM_GhostRestoreEpochMatches(17, 17));
	MM_CHECK_FALSE(MM_GhostRestoreEpochMatches(16, 17));
}

MM_TEST(ghost_world_epoch_distinguishes_rebuilds_from_persistent_horde_waves) {
	MM_CHECK(MM_GhostSnapshotBelongsToWorld(true, 0, 0));
	MM_CHECK(MM_GhostSnapshotBelongsToWorld(true, 17, 17));
	MM_CHECK_FALSE(MM_GhostSnapshotBelongsToWorld(false, 17, 17));
	MM_CHECK_FALSE(MM_GhostSnapshotBelongsToWorld(true, 16, 17));
}

MM_TEST(ghost_same_match_world_reset_preserves_session_membership_only) {
	// A round reset invalidates position/inventory through its world epoch while
	// the same match still owns the authenticated team/duel reservation.
	MM_CHECK_FALSE(MM_GhostSnapshotBelongsToWorld(true, 16, 17));
	MM_CHECK(MM_GhostSessionBelongsToMatch(true, true));
	MM_CHECK_FALSE(MM_GhostSessionBelongsToMatch(false, true));
	MM_CHECK_FALSE(MM_GhostSessionBelongsToMatch(true, false));

	const auto playing = MM_GhostSessionMembershipPolicy(true, false);
	MM_CHECK(playing.reapply_saved_membership);
	MM_CHECK(playing.clear_follow_target);
	MM_CHECK_FALSE(playing.use_free_spectator);

	const auto spectator = MM_GhostSessionMembershipPolicy(true, true);
	MM_CHECK(spectator.reapply_saved_membership);
	MM_CHECK(spectator.clear_follow_target);
	MM_CHECK(spectator.use_free_spectator);

	const auto stale_match = MM_GhostSessionMembershipPolicy(false, false);
	MM_CHECK_FALSE(stale_match.reapply_saved_membership);
	MM_CHECK_FALSE(stale_match.clear_follow_target);
	MM_CHECK_FALSE(stale_match.use_free_spectator);

	// A normal elimination spawn may choose a follow target. The same spawn
	// entered as a ghost-abort fallback must remain in normalized freecam so it
	// cannot retain a client-slot pointer or bypass deferred presentation.
	MM_CHECK_FALSE(MM_GhostSpawnUsesDeferredPresentation(false, false));
	MM_CHECK(MM_GhostSpawnUsesDeferredPresentation(true, false));
	MM_CHECK(MM_GhostSpawnUsesDeferredPresentation(false, true));
	MM_CHECK(MM_GhostSpawnMayAutoFollow(false));
	MM_CHECK_FALSE(MM_GhostSpawnMayAutoFollow(true));
	MM_CHECK(MM_GhostSpawnMayAutoJoin(false));
	MM_CHECK_FALSE(MM_GhostSpawnMayAutoJoin(true));
	MM_CHECK_FALSE(MM_GhostSpawnNeedsPersistentInitialization(false, false));
	MM_CHECK_FALSE(MM_GhostSpawnNeedsPersistentInitialization(true, true));
	MM_CHECK(MM_GhostSpawnNeedsPersistentInitialization(true, false));
	MM_CHECK(MM_GhostDisconnectMayCaptureSnapshot(false));
	MM_CHECK_FALSE(MM_GhostDisconnectMayCaptureSnapshot(true));
	MM_CHECK_FALSE(MM_GhostAbortMarkerSurvivesSystemClear(
		false, true, false, true));
	MM_CHECK_FALSE(MM_GhostAbortMarkerSurvivesSystemClear(
		true, false, false, true));
	MM_CHECK_FALSE(MM_GhostAbortMarkerSurvivesSystemClear(
		true, true, true, false));
	MM_CHECK(MM_GhostAbortMarkerSurvivesSystemClear(
		true, true, false, true));
	MM_CHECK(MM_GhostAbortMarkerSurvivesSystemClear(
		true, true, true, true));
}

MM_TEST(ghost_old_wave_techs_defer_to_horde_reset_policy_only) {
	MM_CHECK(MM_GhostHordeWaveOwnsTechReset(true, true, 16, 17));
	MM_CHECK_FALSE(MM_GhostHordeWaveOwnsTechReset(true, true, 17, 17));
	MM_CHECK_FALSE(MM_GhostHordeWaveOwnsTechReset(true, false, 16, 17));
	MM_CHECK_FALSE(MM_GhostHordeWaveOwnsTechReset(false, true, 16, 17));
}

MM_TEST(ghost_snapshot_reserves_its_slot_until_cleanup_finishes) {
	MM_CHECK(MM_GhostSnapshotNeedsCleanup(true));
	MM_CHECK_FALSE(MM_GhostSnapshotNeedsCleanup(false));
	MM_CHECK(MM_GhostSnapshotReservesSlot(true, true));
	MM_CHECK_FALSE(MM_GhostSnapshotReservesSlot(false, true));
	MM_CHECK_FALSE(MM_GhostSnapshotReservesSlot(true, false));
	MM_CHECK_FALSE(MM_GhostSnapshotReservesSlot(false, false));
}

MM_TEST(ghost_pending_restore_keeps_saved_round_participant_counted) {
	MM_CHECK(MM_GhostReservedParticipantCountsForRound(
		true, true, true, true, true, false));
	MM_CHECK_FALSE(MM_GhostReservedParticipantCountsForRound(
		false, true, true, true, true, false));
	MM_CHECK_FALSE(MM_GhostReservedParticipantCountsForRound(
		true, false, true, true, true, false));
	MM_CHECK_FALSE(MM_GhostReservedParticipantCountsForRound(
		true, true, false, true, true, false));
	MM_CHECK_FALSE(MM_GhostReservedParticipantCountsForRound(
		true, true, true, false, true, false));
	MM_CHECK_FALSE(MM_GhostReservedParticipantCountsForRound(
		true, true, true, true, false, false));
	MM_CHECK_FALSE(MM_GhostReservedParticipantCountsForRound(
		true, true, true, true, true, true));
}

MM_TEST(ghost_skin_sync_maps_each_peer_in_both_directions) {
	constexpr size_t capacity = 4;
	constexpr size_t restored = 1;

	auto pair = MM_GhostSkinSyncPair(restored, 0, capacity);
	MM_CHECK(pair.valid);
	MM_CHECK_EQ(pair.viewer_index, 0u);
	MM_CHECK_EQ(pair.target_index, restored);

	pair = MM_GhostSkinSyncPair(restored, 1, capacity);
	MM_CHECK(pair.valid);
	MM_CHECK_EQ(pair.viewer_index, restored);
	MM_CHECK_EQ(pair.target_index, 0u);

	MM_CHECK_FALSE(MM_GhostSkinSyncPair(restored, 2, capacity).valid);
	MM_CHECK_FALSE(MM_GhostSkinSyncPair(restored, 3, capacity).valid);

	pair = MM_GhostSkinSyncPair(restored, 6, capacity);
	MM_CHECK(pair.valid);
	MM_CHECK_EQ(pair.viewer_index, 3u);
	MM_CHECK_EQ(pair.target_index, restored);
	MM_CHECK_FALSE(MM_GhostSkinSyncPair(restored, 8, capacity).valid);
	MM_CHECK_FALSE(MM_GhostSkinSyncPair(capacity, 0, capacity).valid);
}

MM_TEST(ghost_restore_placement_never_telefrags_a_crowded_position) {
	MM_CHECK_EQ(MM_GhostRestorePlacementStrategy(false, false, false),
		mm_ghost_restore_placement_t::SavedPosition);
	MM_CHECK_EQ(MM_GhostRestorePlacementStrategy(false, true, true),
		mm_ghost_restore_placement_t::SavedPosition);
	MM_CHECK_EQ(MM_GhostRestorePlacementStrategy(true, true, false),
		mm_ghost_restore_placement_t::FallbackSpawn);
	MM_CHECK_EQ(MM_GhostRestorePlacementStrategy(true, false, false),
		mm_ghost_restore_placement_t::Wait);
	MM_CHECK_EQ(MM_GhostRestorePlacementStrategy(true, true, true),
		mm_ghost_restore_placement_t::Wait);

	// At MuffMode's 128-client ceiling, every saved hull may be occupied at
	// once. None becomes eligible until a genuinely clear fallback exists.
	for (size_t client = 0; client < MM_GHOST_MAX_CLIENT_CAPACITY; client++)
		MM_CHECK_EQ(MM_GhostRestorePlacementStrategy(true, true, true),
			mm_ghost_restore_placement_t::Wait);
}

MM_TEST(ghost_deferred_presentation_is_connection_and_transition_owned) {
	MM_CHECK(MM_GhostMayRunRestoreCommit(true, false, false));
	MM_CHECK_FALSE(MM_GhostMayRunRestoreCommit(false, false, false));
	MM_CHECK_FALSE(MM_GhostMayRunRestoreCommit(true, true, false));
	MM_CHECK_FALSE(MM_GhostMayRunRestoreCommit(true, false, true));
	MM_CHECK(MM_GhostMayRunDeferredPresentation(false, false));
	MM_CHECK_FALSE(MM_GhostMayRunDeferredPresentation(true, false));
	MM_CHECK_FALSE(MM_GhostMayRunDeferredPresentation(false, true));
	MM_CHECK_FALSE(MM_GhostMayRunDeferredPresentation(true, true));
	MM_CHECK(MM_GhostShouldDeferSnapshotCleanup(true, true));
	MM_CHECK_FALSE(MM_GhostShouldDeferSnapshotCleanup(false, true));
	MM_CHECK_FALSE(MM_GhostShouldDeferSnapshotCleanup(true, false));

	MM_CHECK(MM_GhostRestoreEpochMatches(41, 41));
	MM_CHECK_FALSE(MM_GhostRestoreEpochMatches(41, 42));
	MM_CHECK(MM_GhostSnapshotBelongsToWorld(true, 7, 7));
	MM_CHECK_FALSE(MM_GhostSnapshotBelongsToWorld(true, 7, 8));
	MM_CHECK_FALSE(MM_GhostSnapshotBelongsToWorld(false, 7, 7));
}

MM_TEST(reliable_fanout_budget_is_overflow_safe_and_fail_closed) {
	mm_reliable_fanout_budget_t budget{
		MM_GHOST_POST_RESTORE_SKIN_MESSAGES_PER_FRAME,
		MM_GHOST_POST_RESTORE_SKIN_MESSAGES_PER_FRAME *
			MM_GHOST_MAX_SKIN_CONFIGSTRING_MESSAGE_BYTES
	};

	MM_CHECK(MM_ReliableFanoutTryReserve(
		budget, MM_GHOST_MAX_SKIN_CONFIGSTRING_MESSAGE_BYTES));
	MM_CHECK(MM_ReliableFanoutTryReserve(
		budget, MM_GHOST_MAX_SKIN_CONFIGSTRING_MESSAGE_BYTES));
	MM_CHECK_FALSE(MM_ReliableFanoutTryReserve(budget, 1));
	MM_CHECK_EQ(budget.messages, MM_GHOST_POST_RESTORE_SKIN_MESSAGES_PER_FRAME);
	MM_CHECK_EQ(budget.bytes,
		MM_GHOST_POST_RESTORE_SKIN_MESSAGES_PER_FRAME *
			MM_GHOST_MAX_SKIN_CONFIGSTRING_MESSAGE_BYTES);

	mm_reliable_fanout_budget_t byte_limited{
		3, MM_GHOST_MAX_SKIN_CONFIGSTRING_MESSAGE_BYTES
	};
	MM_CHECK(MM_ReliableFanoutTryReserve(
		byte_limited, MM_GHOST_MAX_SKIN_CONFIGSTRING_MESSAGE_BYTES));
	MM_CHECK_FALSE(MM_ReliableFanoutTryReserve(byte_limited, 1));
	MM_CHECK_EQ(byte_limited.messages, 1u);
	MM_CHECK_EQ(byte_limited.bytes,
		MM_GHOST_MAX_SKIN_CONFIGSTRING_MESSAGE_BYTES);

	mm_reliable_fanout_budget_t corrupt{ 3, 8, 0, 9 };
	MM_CHECK_FALSE(MM_ReliableFanoutTryReserve(corrupt, 0));
	MM_CHECK_EQ(corrupt.messages, 0u);
	MM_CHECK_EQ(corrupt.bytes, 9u);

	mm_reliable_fanout_budget_t maximum{
		2, std::numeric_limits<size_t>::max()
	};
	MM_CHECK(MM_ReliableFanoutTryReserve(
		maximum, std::numeric_limits<size_t>::max()));
	MM_CHECK_FALSE(MM_ReliableFanoutTryReserve(maximum, 1));
}

std::array<mm_ghost_skin_sync_slot_t, MM_GHOST_MAX_CLIENT_CAPACITY>
ReadyGhostSkinSyncSlots()
{
	std::array<mm_ghost_skin_sync_slot_t, MM_GHOST_MAX_CLIENT_CAPACITY> slots{};
	for (size_t i = 0; i < slots.size(); i++)
		slots[i] = { true, true, true, static_cast<int32_t>(1000 + i) };
	return slots;
}

MM_TEST(ghost_due_restore_commits_once_after_64_substeps_and_remain_fair) {
	constexpr size_t capacity = MM_GHOST_MAX_CLIENT_CAPACITY;
	constexpr size_t simulated_substeps = 64;
	std::array<bool, capacity> due{};
	due.fill(true);

	// Substeps only advance countdown state. Commit admission belongs to the one
	// outer-server-frame pass that follows all of them.
	size_t commit_attempts = 0;
	for (size_t substep = 0; substep < simulated_substeps; substep++)
		MM_CHECK_EQ(commit_attempts, 0u);

	std::array<size_t, capacity> selected_counts{};
	size_t cursor = capacity - 3;
	for (size_t server_frame = 0; server_frame < capacity; server_frame++) {
		const size_t selected =
			MM_GhostSelectDueRestoreCommit(due, capacity, cursor);
		commit_attempts++;
		MM_CHECK(selected < capacity);
		MM_CHECK(due[selected]);
		selected_counts[selected]++;
		due[selected] = false;
	}

	MM_CHECK_EQ(commit_attempts, capacity);
	for (const size_t count : selected_counts)
		MM_CHECK_EQ(count, 1u);
	MM_CHECK_EQ(MM_GhostSelectDueRestoreCommit(due, capacity, cursor),
		MM_GHOST_NO_CLIENT_INDEX);

	// Capacity is clamped to the production ceiling, and one call still admits
	// only one of several simultaneous due restores.
	due.fill(true);
	cursor = 0;
	const size_t selected = MM_GhostSelectDueRestoreCommit(
		due, capacity + 100, cursor);
	MM_CHECK_EQ(selected, 0u);
	MM_CHECK_EQ(cursor, 1u);
}

MM_TEST(ghost_skin_sync_discards_partially_drained_stale_work) {
	constexpr size_t capacity = MM_GHOST_MAX_CLIENT_CAPACITY;
	constexpr size_t restored = 5;
	const auto ready_slots = ReadyGhostSkinSyncSlots();
	const mm_ghost_skin_sync_context_t ready_context{
		capacity, 71, 13, true
	};

	const auto check_invalidation = [&](const auto &invalidate) {
		mm_ghost_skin_sync_scheduler_t<capacity> scheduler{};
		auto slots = ready_slots;
		auto context = ready_context;
		MM_CHECK(MM_GhostQueueSkinSync(scheduler, restored,
			slots[restored].spawn_count, context.round_epoch,
			context.world_epoch));

		const auto first = MM_GhostStepSkinSync(scheduler, slots, context);
		MM_CHECK_EQ(first.action,
			mm_ghost_skin_sync_action_t::PublishCanonical);
		MM_CHECK_EQ(first.restored_index, restored);
		MM_CHECK(scheduler.queues[restored].active);
		MM_CHECK_EQ(scheduler.queues[restored].next_operation, 1u);

		invalidate(slots, context);
		const auto stale = MM_GhostStepSkinSync(scheduler, slots, context);
		MM_CHECK_EQ(stale.action, mm_ghost_skin_sync_action_t::None);
		MM_CHECK_FALSE(scheduler.queues[restored].active);
		MM_CHECK_FALSE(scheduler.queues[restored].owns_pairs);
		MM_CHECK_EQ(scheduler.queues[restored].serial, 0u);
	};

	check_invalidation([](auto &, auto &context) { context.round_epoch++; });
	check_invalidation([](auto &, auto &context) { context.world_epoch++; });
	check_invalidation([](auto &slots, auto &) { slots[restored].spawn_count++; });
	check_invalidation([](auto &slots, auto &) { slots[restored].connected = false; });
	check_invalidation([](auto &slots, auto &) { slots[restored].spawned = false; });

	mm_ghost_skin_sync_scheduler_t<capacity> scheduler{};
	MM_CHECK(MM_GhostQueueSkinSync(scheduler, restored,
		ready_slots[restored].spawn_count, ready_context.round_epoch,
		ready_context.world_epoch));
	MM_CHECK(MM_GhostQueueSkinSync(scheduler, restored + 1,
		ready_slots[restored + 1].spawn_count, ready_context.round_epoch,
		ready_context.world_epoch));
	auto transition_context = ready_context;
	transition_context.presentation_allowed = false;
	MM_CHECK_EQ(MM_GhostStepSkinSync(
		scheduler, ready_slots, transition_context).action,
		mm_ghost_skin_sync_action_t::None);
	MM_CHECK_EQ(scheduler.last_serial, 0u);
	MM_CHECK_EQ(scheduler.cursor, 0u);
	for (const auto &queue : scheduler.queues) {
		MM_CHECK_FALSE(queue.active);
		MM_CHECK_FALSE(queue.owns_pairs);
		MM_CHECK_EQ(queue.serial, 0u);
	}
}

MM_TEST(ghost_skin_sync_false_sends_still_make_bounded_progress) {
	constexpr size_t capacity = MM_GHOST_MAX_CLIENT_CAPACITY;
	constexpr size_t restored = 0;
	auto slots = ReadyGhostSkinSyncSlots();
	const mm_ghost_skin_sync_context_t context{ capacity, 9, 4, true };
	mm_ghost_skin_sync_scheduler_t<capacity> scheduler{};
	MM_CHECK(MM_GhostQueueSkinSync(scheduler, restored,
		slots[restored].spawn_count, context.round_epoch, context.world_epoch));

	// The caller deliberately treats every action as an emission failure. Each
	// failed action is nevertheless consumed, so the drain cannot spin forever.
	size_t attempts = 0;
	for (; attempts < MM_GHOST_MAX_SKIN_SYNC_ACTIONS_PER_DRAIN; attempts++) {
		const auto step = MM_GhostStepSkinSync(scheduler, slots, context);
		if (step.action == mm_ghost_skin_sync_action_t::None)
			break;
	}
	MM_CHECK_EQ(attempts, MM_GHOST_MAX_SKIN_SYNC_ACTIONS_PER_QUEUE);
	MM_CHECK_EQ(MM_GhostStepSkinSync(scheduler, slots, context).action,
		mm_ghost_skin_sync_action_t::None);
}

MM_TEST(ghost_skin_sync_max_client_noop_wave_is_attempt_bounded_per_frame) {
	constexpr size_t capacity = MM_GHOST_MAX_CLIENT_CAPACITY;
	const auto slots = ReadyGhostSkinSyncSlots();
	const mm_ghost_skin_sync_context_t context{ capacity, 9, 4, true };
	mm_ghost_skin_sync_scheduler_t<capacity> scheduler{};

	for (size_t restored = 0; restored < capacity; restored++)
		MM_CHECK(MM_GhostQueueSkinSync(scheduler, restored,
			slots[restored].spawn_count, context.round_epoch, context.world_epoch));

	const size_t pending_before = MM_GhostPendingSkinSyncActionUpperBound(
		scheduler, context);
	size_t attempts = 0;
	for (; attempts < MM_GHOST_MAX_SKIN_SYNC_ACTIONS_PER_DRAIN; attempts++) {
		const auto step = MM_GhostStepSkinSync(scheduler, slots, context);
		if (step.action == mm_ghost_skin_sync_action_t::None)
			break;
	}
	const size_t pending_after = MM_GhostPendingSkinSyncActionUpperBound(
		scheduler, context);

	MM_CHECK_EQ(attempts, MM_GHOST_MAX_SKIN_SYNC_ACTIONS_PER_DRAIN);
	MM_CHECK(pending_after < pending_before);
	MM_CHECK(pending_after > 0);
	MM_CHECK(MM_GhostActiveSkinSyncQueueCount(scheduler) > 0);
}

MM_TEST(ghost_skin_sync_queue_diagnostics_bound_remaining_work) {
	constexpr size_t capacity = MM_GHOST_MAX_CLIENT_CAPACITY;
	const auto slots = ReadyGhostSkinSyncSlots();
	mm_ghost_skin_sync_scheduler_t<capacity> scheduler{};
	const mm_ghost_skin_sync_context_t full_context{ capacity, 9, 4, true };

	MM_CHECK(MM_GhostQueueSkinSync(scheduler, 1,
		slots[1].spawn_count, full_context.round_epoch,
		full_context.world_epoch));
	MM_CHECK(MM_GhostQueueSkinSync(scheduler, 5,
		slots[5].spawn_count, full_context.round_epoch,
		full_context.world_epoch));
	MM_CHECK_EQ(MM_GhostActiveSkinSyncQueueCount(scheduler), 2u);
	MM_CHECK_EQ(MM_GhostPendingSkinSyncActionUpperBound(
		scheduler, full_context),
		2 * MM_GHOST_MAX_SKIN_SYNC_ACTIONS_PER_QUEUE);

	const mm_ghost_skin_sync_context_t four_client_context{ 4, 9, 4, true };
	MM_CHECK_EQ(MM_GhostPendingSkinSyncActionUpperBound(
		scheduler, four_client_context), 7u);

	const auto first = MM_GhostStepSkinSync(scheduler, slots, full_context);
	MM_CHECK_EQ(first.action, mm_ghost_skin_sync_action_t::PublishCanonical);
	MM_CHECK_EQ(first.restored_index, 1u);
	MM_CHECK_EQ(MM_GhostPendingSkinSyncActionUpperBound(
		scheduler, full_context),
		2 * MM_GHOST_MAX_SKIN_SYNC_ACTIONS_PER_QUEUE - 1);

	MM_GhostCancelSkinSync(scheduler, 1);
	MM_CHECK_EQ(MM_GhostActiveSkinSyncQueueCount(scheduler), 1u);
	MM_CHECK_EQ(MM_GhostPendingSkinSyncActionUpperBound(
		scheduler, full_context),
		MM_GHOST_MAX_SKIN_SYNC_ACTIONS_PER_QUEUE);
}

MM_TEST(ghost_skin_sync_stress_bounds_repeated_max_client_reconnects) {
	constexpr size_t capacity = MM_GHOST_MAX_CLIENT_CAPACITY;
	constexpr size_t reconnect_waves = 4;
	constexpr size_t expected_pair_messages_per_wave =
		capacity * (capacity - 1);
	constexpr size_t expected_messages_per_wave =
		capacity + expected_pair_messages_per_wave;

	auto slots = ReadyGhostSkinSyncSlots();
	mm_ghost_skin_sync_scheduler_t<capacity> scheduler{};
	mm_ghost_skin_sync_context_t context{ capacity, 100, 20, true };

	for (size_t wave = 0; wave < reconnect_waves; wave++) {
		context.round_epoch = static_cast<uint32_t>(100 + wave);
		context.world_epoch = wave < reconnect_waves / 2 ? 20u : 21u;
		for (size_t restored = 0; restored < capacity; restored++)
			MM_CHECK(MM_GhostQueueSkinSync(scheduler, restored,
				slots[restored].spawn_count, context.round_epoch,
				context.world_epoch));

		std::array<std::array<size_t, capacity>, capacity> pair_counts{};
		std::array<std::array<size_t, capacity>, capacity> pair_owners{};
		for (auto &owners : pair_owners)
			owners.fill(MM_GHOST_NO_CLIENT_INDEX);
		std::array<size_t, capacity> canonical_counts{};
		size_t total_messages = 0;
		size_t frames = 0;

		for (;;) {
			mm_reliable_fanout_budget_t budget{
				MM_GHOST_POST_RESTORE_SKIN_MESSAGES_PER_FRAME,
				MM_GHOST_POST_RESTORE_SKIN_MESSAGES_PER_FRAME *
					MM_GHOST_MAX_SKIN_CONFIGSTRING_MESSAGE_BYTES
			};
			size_t sent_this_frame = 0;

			while (sent_this_frame <
					MM_GHOST_POST_RESTORE_SKIN_MESSAGES_PER_FRAME) {
				const auto step = MM_GhostStepSkinSync(scheduler, slots, context);
				if (step.action == mm_ghost_skin_sync_action_t::None)
					break;

				MM_CHECK(MM_ReliableFanoutTryReserve(
					budget, MM_GHOST_MAX_SKIN_CONFIGSTRING_MESSAGE_BYTES));
				if (step.action == mm_ghost_skin_sync_action_t::PublishCanonical) {
					MM_CHECK(step.restored_index < capacity);
					canonical_counts[step.restored_index]++;
				} else {
					MM_CHECK_EQ(step.action,
						mm_ghost_skin_sync_action_t::ReapplyOverride);
					MM_CHECK(step.pair.valid);
					MM_CHECK(step.pair.viewer_index < capacity);
					MM_CHECK(step.pair.target_index < capacity);
					MM_CHECK(step.pair.viewer_index != step.pair.target_index);
					MM_CHECK_EQ(canonical_counts[step.restored_index], 1u);
					pair_counts[step.pair.viewer_index][step.pair.target_index]++;
					pair_owners[step.pair.viewer_index][step.pair.target_index] =
						step.restored_index;
				}
				sent_this_frame++;
				total_messages++;
			}

			if (!sent_this_frame)
				break;

			MM_CHECK(sent_this_frame <=
				MM_GHOST_POST_RESTORE_SKIN_MESSAGES_PER_FRAME);
			MM_CHECK_EQ(budget.messages, sent_this_frame);
			MM_CHECK_EQ(budget.bytes,
				sent_this_frame * MM_GHOST_MAX_SKIN_CONFIGSTRING_MESSAGE_BYTES);
			frames++;
			MM_CHECK(frames <= expected_messages_per_wave);
		}

		MM_CHECK_EQ(total_messages, expected_messages_per_wave);
		MM_CHECK_EQ(frames,
			(expected_messages_per_wave +
				MM_GHOST_POST_RESTORE_SKIN_MESSAGES_PER_FRAME - 1) /
			MM_GHOST_POST_RESTORE_SKIN_MESSAGES_PER_FRAME);
		for (size_t target = 0; target < capacity; target++)
			MM_CHECK_EQ(canonical_counts[target], 1u);
		for (size_t viewer = 0; viewer < capacity; viewer++) {
			for (size_t target = 0; target < capacity; target++) {
				MM_CHECK_EQ(pair_counts[viewer][target],
					viewer == target ? 0u : 1u);
				if (viewer != target)
					MM_CHECK_EQ(pair_owners[viewer][target],
						std::max(viewer, target));
			}
		}
	}

	// A genuinely later restore supersedes the completed ownership watermark and
	// refreshes both directions involving that client, without touching peer pairs.
	constexpr size_t later_restored = capacity / 2;
	MM_CHECK(MM_GhostQueueSkinSync(scheduler, later_restored,
		slots[later_restored].spawn_count, context.round_epoch,
		context.world_epoch));
	std::array<std::array<size_t, capacity>, capacity> later_pair_counts{};
	size_t later_canonical_count = 0;
	for (;;) {
		const auto step = MM_GhostStepSkinSync(scheduler, slots, context);
		if (step.action == mm_ghost_skin_sync_action_t::None)
			break;
		if (step.action == mm_ghost_skin_sync_action_t::PublishCanonical) {
			MM_CHECK_EQ(step.restored_index, later_restored);
			later_canonical_count++;
			continue;
		}

		MM_CHECK_EQ(step.action,
			mm_ghost_skin_sync_action_t::ReapplyOverride);
		MM_CHECK_EQ(step.restored_index, later_restored);
		later_pair_counts[step.pair.viewer_index][step.pair.target_index]++;
	}
	MM_CHECK_EQ(later_canonical_count, 1u);
	for (size_t viewer = 0; viewer < capacity; viewer++) {
		for (size_t target = 0; target < capacity; target++) {
			const bool involves_later = viewer != target &&
				(viewer == later_restored || target == later_restored);
			MM_CHECK_EQ(later_pair_counts[viewer][target],
				involves_later ? 1u : 0u);
		}
	}
}

// --- Post-match awards ------------------------------------------------------

namespace awards_test {

// A match long enough and full enough to be judged, so each test only has to
// state the thing it is actually about.
mm_award_match_facts_t Match(size_t participants)
{
	mm_award_match_facts_t match;
	match.participants = participants;
	match.duration_msec = MM_AWARD_MIN_DURATION_MSEC;
	return match;
}

std::vector<mm_award_result_t> Select(std::vector<mm_award_player_facts_t> players,
	mm_award_match_facts_t match)
{
	match.participants = players.size();
	std::array<mm_award_result_t, MM_AWARDS_DISPLAY_LIMIT> reel {};
	const size_t count = MM_AwardsSelect(players.data(), players.size(), match,
		reel.data(), reel.size());
	return std::vector<mm_award_result_t>(reel.begin(), reel.begin() + count);
}

bool Holds(const std::vector<mm_award_result_t> &reel, mm_match_award_t award,
	size_t player_index)
{
	return std::any_of(reel.begin(), reel.end(),
		[&](const mm_award_result_t &result) {
			return result.award == award && result.player_index == player_index;
		});
}

bool Awarded(const std::vector<mm_award_result_t> &reel, mm_match_award_t award)
{
	return std::any_of(reel.begin(), reel.end(),
		[&](const mm_award_result_t &result) { return result.award == award; });
}

} // namespace awards_test

MM_TEST(awards_duration_reads_zero_and_negatives_as_disabled) {
	MM_CHECK_EQ(MM_AwardsDurationSeconds(0), 0);
	MM_CHECK_EQ(MM_AwardsDurationSeconds(-10), 0);
	MM_CHECK_EQ(MM_AwardsDurationSeconds(10), 10);
	MM_CHECK_EQ(MM_AwardsDurationSeconds(1), MM_AWARDS_MIN_SECONDS);
	MM_CHECK_EQ(MM_AwardsDurationSeconds(9999), MM_AWARDS_MAX_SECONDS);
}

MM_TEST(awards_skip_hold_always_leaves_room_to_skip) {
	// The hold must stay strictly inside the reel's own length, or a short reel
	// would refuse the key press for its entire duration and read as unskippable.
	for (int seconds = MM_AWARDS_MIN_SECONDS; seconds <= MM_AWARDS_MAX_SECONDS; seconds++) {
		const int hold = MM_AwardsSkipHoldSeconds(seconds);
		MM_CHECK(hold >= 1);
		MM_CHECK(hold < seconds);
		MM_CHECK(hold <= MM_AWARDS_MIN_HOLD_SECONDS);
	}

	// A disabled reel has no hold to serve.
	MM_CHECK_EQ(MM_AwardsSkipHoldSeconds(0), 0);
	MM_CHECK_EQ(MM_AwardsSkipHoldSeconds(-5), 0);

	// The default reel gives the full hold.
	MM_CHECK_EQ(MM_AwardsSkipHoldSeconds(10), MM_AWARDS_MIN_HOLD_SECONDS);
}

MM_TEST(awards_percent_is_division_safe_and_saturates) {
	MM_CHECK_EQ(MM_AwardPercent(1, 0), 0u);
	MM_CHECK_EQ(MM_AwardPercent(0, 0), 0u);
	MM_CHECK_EQ(MM_AwardPercent(1, 4), 25u);
	MM_CHECK_EQ(MM_AwardPercent(4, 4), 100u);
	// More pickups than spawns cannot happen, but it must not read as 400%.
	MM_CHECK_EQ(MM_AwardPercent(8, 4), 100u);
}

MM_TEST(awards_block_top_centres_the_reel_whatever_its_length) {
	// A single pair straddles the centre line; a full reel is symmetric about it.
	const int one = MM_AwardsBlockTop(1);
	MM_CHECK_EQ(one, MM_AWARDS_LAYOUT_CENTRE_Y - MM_AWARDS_TITLE_TO_NAME / 2);

	const size_t full = MM_AWARDS_DISPLAY_LIMIT;
	const int top = MM_AwardsBlockTop(full);
	const int bottom = top + static_cast<int>(full - 1) * MM_AWARDS_PAIR_PITCH +
		MM_AWARDS_TITLE_TO_NAME;
	MM_CHECK(top < MM_AWARDS_LAYOUT_CENTRE_Y);
	MM_CHECK(bottom > MM_AWARDS_LAYOUT_CENTRE_Y);
	MM_CHECK_EQ((top + bottom) / 2, MM_AWARDS_LAYOUT_CENTRE_Y);
}

MM_TEST(awards_display_name_strips_layout_hostile_bytes) {
	// A quote would close the layout token early and hand the rest of the name
	// to the parser as commands.
	MM_CHECK_EQ(MM_AwardsDisplayName("ra\"ge"), std::string("rage"));
	MM_CHECK_EQ(MM_AwardsDisplayName("ra\\ge"), std::string("rage"));
	MM_CHECK_EQ(MM_AwardsDisplayName("ra\nge"), std::string("rage"));
	MM_CHECK_EQ(MM_AwardsDisplayName("plain"), std::string("plain"));
}

MM_TEST(awards_display_name_truncation_never_splits_utf8) {
	const std::string wide(MM_AWARDS_MAX_NAME_CHARS + 8, 'x');
	const std::string clipped = MM_AwardsDisplayName(wide);
	MM_CHECK_EQ(clipped.size(), MM_AWARDS_MAX_NAME_CHARS + 3);
	MM_CHECK_EQ(clipped.substr(clipped.size() - 3), std::string("..."));

	// Two-byte sequences packed so the cut lands mid-character; the trailing
	// continuation byte must be dropped rather than emitted on its own.
	std::string multibyte;
	while (multibyte.size() < MM_AWARDS_MAX_NAME_CHARS + 6)
		multibyte += "\xC3\xA9";
	const std::string cut = MM_AwardsDisplayName(multibyte);
	const std::string body = cut.substr(0, cut.size() - 3);
	MM_CHECK_EQ(body.size() % 2, 0u);
}

MM_TEST(awards_short_or_thin_matches_produce_no_reel) {
	using namespace awards_test;
	mm_award_player_facts_t sheriff;
	sheriff.kills = 30;
	sheriff.shotgun_kills = 30;
	mm_award_player_facts_t victim;
	victim.kills = 1;

	// Long enough, but only one participant.
	mm_award_match_facts_t solo = Match(1);
	MM_CHECK(Select({ sheriff }, solo).empty());

	// Two participants, but over before it started.
	mm_award_match_facts_t brief = Match(2);
	brief.duration_msec = MM_AWARD_MIN_DURATION_MSEC - 1;
	MM_CHECK(Select({ sheriff, victim }, brief).empty());
}

MM_TEST(awards_shotgun_sheriff_needs_volume_share_and_a_strict_lead) {
	using namespace awards_test;
	mm_award_player_facts_t sheriff;
	sheriff.kills = 25;
	sheriff.shotgun_kills = 22;	// 88% share, clears the 20-kill floor
	mm_award_player_facts_t rival;
	rival.kills = 30;
	rival.shotgun_kills = 5;

	MM_CHECK(Holds(Select({ sheriff, rival }, Match(2)),
		mm_match_award_t::shotgun_sheriff, 0));

	// Same volume, but the shotgun is no longer most of their work.
	mm_award_player_facts_t dabbler = sheriff;
	dabbler.kills = 60;
	MM_CHECK_FALSE(Awarded(Select({ dabbler, rival }, Match(2)),
		mm_match_award_t::shotgun_sheriff));

	// Under the minimum body count, however pure the diet.
	mm_award_player_facts_t novice;
	novice.kills = 10;
	novice.shotgun_kills = 10;
	MM_CHECK_FALSE(Awarded(Select({ novice, rival }, Match(2)),
		mm_match_award_t::shotgun_sheriff));

	// A dead heat awards nobody rather than picking by slot number.
	MM_CHECK_FALSE(Awarded(Select({ sheriff, sheriff }, Match(2)),
		mm_match_award_t::shotgun_sheriff));
}

MM_TEST(awards_rail_slut_follows_the_same_signature_rules) {
	using namespace awards_test;
	mm_award_player_facts_t sniper;
	sniper.kills = 24;
	sniper.rail_kills = 21;
	mm_award_player_facts_t rival;
	rival.kills = 20;
	rival.rail_kills = 4;

	MM_CHECK(Holds(Select({ sniper, rival }, Match(2)),
		mm_match_award_t::rail_slut, 0));
}

MM_TEST(awards_quad_god_needs_control_and_conversion) {
	using namespace awards_test;
	mm_award_match_facts_t match = Match(2);
	match.quad_spawns = 10;

	mm_award_player_facts_t boss;
	boss.quad_pickups = 9;	// 90% control
	boss.kills = 20;
	boss.quad_kills = 12;	// 60% of kills quadded
	mm_award_player_facts_t rival;
	rival.quad_pickups = 1;
	rival.kills = 10;

	MM_CHECK(Holds(Select({ boss, rival }, match), mm_match_award_t::quad_god, 0));
	MM_CHECK_FALSE(Awarded(Select({ boss, rival }, match),
		mm_match_award_t::quad_dummy));
}

MM_TEST(awards_quad_control_without_conversion_is_the_dummy) {
	using namespace awards_test;
	mm_award_match_facts_t match = Match(2);
	match.quad_spawns = 10;

	mm_award_player_facts_t hoarder;
	hoarder.quad_pickups = 10;	// took every single one
	hoarder.kills = 20;
	hoarder.quad_kills = 2;		// and did nothing with them
	mm_award_player_facts_t rival;
	rival.kills = 10;

	const std::vector<mm_award_result_t> reel = Select({ hoarder, rival }, match);
	MM_CHECK(Holds(reel, mm_match_award_t::quad_dummy, 0));
	MM_CHECK_FALSE(Awarded(reel, mm_match_award_t::quad_god));
}

MM_TEST(awards_quad_titles_need_near_total_control_of_the_spawns) {
	using namespace awards_test;
	mm_award_match_facts_t match = Match(2);
	match.quad_spawns = 10;

	mm_award_player_facts_t sharer;
	sharer.quad_pickups = 6;	// 60% control -- the quad was contested
	sharer.kills = 20;
	sharer.quad_kills = 18;
	mm_award_player_facts_t rival;
	rival.quad_pickups = 4;
	rival.kills = 10;

	const std::vector<mm_award_result_t> reel = Select({ sharer, rival }, match);
	MM_CHECK_FALSE(Awarded(reel, mm_match_award_t::quad_god));
	MM_CHECK_FALSE(Awarded(reel, mm_match_award_t::quad_dummy));
}

MM_TEST(awards_map_with_no_quad_hands_out_no_quad_titles) {
	using namespace awards_test;
	mm_award_player_facts_t player;
	player.kills = 20;
	player.quad_kills = 20;
	mm_award_player_facts_t rival;
	rival.kills = 10;

	// quad_spawns stays 0: MM_AwardPercent would otherwise read as full control.
	const std::vector<mm_award_result_t> reel = Select({ player, rival }, Match(2));
	MM_CHECK_FALSE(Awarded(reel, mm_match_award_t::quad_god));
	MM_CHECK_FALSE(Awarded(reel, mm_match_award_t::quad_dummy));
}

MM_TEST(awards_aimbot_needs_a_real_sample_not_one_lucky_shot) {
	using namespace awards_test;
	mm_award_player_facts_t crack;
	crack.shots = MM_AWARD_ACCURACY_MIN_SHOTS;
	crack.hits = MM_AWARD_ACCURACY_MIN_SHOTS * 85 / 100;
	mm_award_player_facts_t sniper;
	sniper.shots = 2;
	sniper.hits = 2;	// 100%, and completely meaningless

	MM_CHECK(Holds(Select({ crack, sniper }, Match(2)),
		mm_match_award_t::aimbot, 0));

	mm_award_player_facts_t merely_good = crack;
	merely_good.hits = MM_AWARD_ACCURACY_MIN_SHOTS * 70 / 100;
	MM_CHECK_FALSE(Awarded(Select({ merely_good, sniper }, Match(2)),
		mm_match_award_t::aimbot));
}

MM_TEST(awards_punching_bag_needs_to_be_bad_on_both_axes) {
	using namespace awards_test;
	mm_award_player_facts_t bag;
	bag.kills = 2;
	bag.deaths = 20;			// K/D 0.10
	bag.damage_dealt = 400;
	bag.damage_received = 4000;	// ratio 0.10
	mm_award_player_facts_t winner;
	winner.kills = 20;
	winner.deaths = 2;
	winner.damage_dealt = 4000;
	winner.damage_received = 400;

	MM_CHECK(Holds(Select({ bag, winner }, Match(2)),
		mm_match_award_t::punching_bag, 0));

	// Trades badly but hits hard: having a rough match, not a punching bag.
	mm_award_player_facts_t brawler = bag;
	brawler.damage_dealt = 3800;
	MM_CHECK_FALSE(Awarded(Select({ brawler, winner }, Match(2)),
		mm_match_award_t::punching_bag));

	// Bad ratios but barely played.
	mm_award_player_facts_t latecomer = bag;
	latecomer.deaths = MM_AWARD_PUNCHING_BAG_DEATHS - 1;
	MM_CHECK_FALSE(Awarded(Select({ latecomer, winner }, Match(2)),
		mm_match_award_t::punching_bag));
}

MM_TEST(awards_camper_must_have_been_playing_not_parked) {
	using namespace awards_test;
	mm_award_player_facts_t camper;
	camper.camp_samples = 200;
	camper.camp_best_cell_samples = 150;	// 75% of the match in one cell
	camper.shots = 120;
	mm_award_player_facts_t rover;
	rover.camp_samples = 200;
	rover.camp_best_cell_samples = 40;
	rover.shots = 120;

	MM_CHECK(Holds(Select({ camper, rover }, Match(2)),
		mm_match_award_t::dirty_rotten_camper, 0));

	// Same dwell share, but the inactivity timer had them flagged: AFK, not
	// camping, which is exactly the case the award must not reward.
	mm_award_player_facts_t afk = camper;
	afk.camp_idle_samples = 120;
	MM_CHECK_FALSE(Awarded(Select({ afk, rover }, Match(2)),
		mm_match_award_t::dirty_rotten_camper));

	// Standing still all match without firing a shot is not camping either.
	mm_award_player_facts_t spectatorish = camper;
	spectatorish.shots = MM_AWARD_CAMP_MIN_SHOTS - 1;
	MM_CHECK_FALSE(Awarded(Select({ spectatorish, rover }, Match(2)),
		mm_match_award_t::dirty_rotten_camper));
}

MM_TEST(awards_spawn_fragger_needs_share_as_well_as_volume) {
	using namespace awards_test;
	mm_award_player_facts_t vulture;
	vulture.kills = 20;
	vulture.spawn_kills = 8;	// 40% of their kills
	mm_award_player_facts_t rival;
	rival.kills = 30;
	rival.spawn_kills = 2;

	MM_CHECK(Holds(Select({ vulture, rival }, Match(2)),
		mm_match_award_t::spawn_fragger, 0));

	// Same raw count, but incidental against a much bigger frag total.
	mm_award_player_facts_t busy = vulture;
	busy.kills = 100;
	MM_CHECK_FALSE(Awarded(Select({ busy, rival }, Match(2)),
		mm_match_award_t::spawn_fragger));
}

MM_TEST(awards_team_only_titles_stay_out_of_free_for_all) {
	using namespace awards_test;
	mm_award_player_facts_t traitor;
	traitor.kills = 10;
	traitor.team_kills = 6;
	mm_award_player_facts_t clean;
	clean.kills = 10;

	MM_CHECK_FALSE(Awarded(Select({ traitor, clean }, Match(2)),
		mm_match_award_t::benedict_arnold));

	mm_award_match_facts_t teams = Match(2);
	teams.team_mode = true;
	MM_CHECK(Holds(Select({ traitor, clean }, teams),
		mm_match_award_t::benedict_arnold, 0));
}

MM_TEST(awards_ctf_titles_stay_out_of_non_ctf_matches) {
	using namespace awards_test;
	mm_award_player_facts_t runner;
	runner.ctf_captures = 5;
	runner.kills = 10;
	mm_award_player_facts_t rival;
	rival.kills = 10;

	MM_CHECK_FALSE(Awarded(Select({ runner, rival }, Match(2)),
		mm_match_award_t::flag_runner));

	mm_award_match_facts_t ctf = Match(2);
	ctf.team_mode = true;
	ctf.ctf_mode = true;
	MM_CHECK(Holds(Select({ runner, rival }, ctf), mm_match_award_t::flag_runner, 0));
}

MM_TEST(awards_reel_is_capped_and_ordered_honours_first) {
	using namespace awards_test;
	// One player good at everything, plus a field for them to beat.
	mm_award_player_facts_t ace;
	ace.score = 50;
	ace.kills = 60;
	ace.deaths = 5;
	ace.first_frag_medals = 1;
	ace.shotgun_kills = 50;
	ace.shots = 400;
	ace.hits = 360;
	ace.damage_dealt = 40000;
	ace.excellent_medals = 20;
	ace.humiliation_medals = 10;
	ace.high_value_pickups = 40;
	ace.spawn_kills = 30;

	mm_award_player_facts_t filler;
	filler.score = 5;
	filler.kills = 5;
	filler.deaths = 40;

	mm_award_player_facts_t filler2;
	filler2.score = 3;
	filler2.kills = 3;
	filler2.deaths = 30;

	const std::vector<mm_award_result_t> reel =
		Select({ ace, filler, filler2 }, Match(3));

	MM_CHECK(reel.size() <= MM_AWARDS_DISPLAY_LIMIT);
	MM_CHECK(!reel.empty());
	// Honours lead the reel, so a truncated list drops the jokes, not the wins.
	MM_CHECK_EQ(static_cast<int>(reel.front().award),
		static_cast<int>(mm_match_award_t::top_dog));
	for (size_t i = 1; i < reel.size(); i++) {
		MM_CHECK(MM_AwardTier(reel[i - 1].award) <= MM_AwardTier(reel[i].award));
	}
}

MM_TEST(awards_soft_per_player_cap_spreads_a_short_reel) {
	using namespace awards_test;
	// Two players who between them qualify for well over the soft cap, so the
	// first pass has to move on rather than hand everything to slot zero.
	mm_award_player_facts_t ace;
	ace.score = 50;
	ace.kills = 60;
	ace.deaths = 5;
	ace.first_frag_medals = 1;
	ace.shotgun_kills = 50;
	ace.shots = 400;
	ace.hits = 360;
	ace.excellent_medals = 20;
	ace.humiliation_medals = 10;

	mm_award_player_facts_t goat;
	goat.score = 20;
	goat.kills = 3;
	goat.deaths = 30;
	goat.damage_dealt = 100;
	goat.damage_received = 9000;
	goat.suicides = 8;
	goat.environment_deaths = 9;
	goat.high_value_pickups = 40;

	mm_award_player_facts_t filler;
	filler.score = 1;
	filler.kills = 1;
	filler.deaths = 2;

	const std::vector<mm_award_result_t> reel =
		Select({ ace, goat, filler }, Match(3));

	size_t ace_titles = 0;
	size_t goat_titles = 0;
	for (const mm_award_result_t &result : reel) {
		if (result.player_index == 0)
			ace_titles++;
		else if (result.player_index == 1)
			goat_titles++;
	}

	// Between them the two qualify for more than the reel holds, so the cap is
	// what decides who gets squeezed out.
	MM_CHECK_EQ(reel.size(), MM_AWARDS_DISPLAY_LIMIT);
	MM_CHECK(ace_titles > 0);

	// The telling result: THE PUNCHING BAG is bottom-tier and belongs to the
	// player who did nothing all match, yet it survives a reel otherwise full of
	// the ace's higher-tier honours. Without the first-pass cap the ace's seven
	// titles would all be seated before any tier-5 entry was considered.
	MM_CHECK(Awarded(reel, mm_match_award_t::punching_bag));
	MM_CHECK(goat_titles >= MM_AWARDS_SOFT_PER_PLAYER_LIMIT);
}

MM_TEST(awards_damage_superlatives_are_actually_winnable) {
	using namespace awards_test;
	// Regression: routing this through MM_AwardPercent made both awards dead
	// code, because the leader's damage is always >= the average and the helper
	// saturates at 100 while the gate demanded 150.
	mm_award_player_facts_t monster;
	monster.kills = 20;
	monster.damage_dealt = 9000;
	monster.damage_received = 9000;
	mm_award_player_facts_t quiet;
	quiet.kills = 2;
	quiet.damage_dealt = 300;
	quiet.damage_received = 300;

	const std::vector<mm_award_result_t> reel =
		Select({ monster, quiet, quiet }, Match(3));
	MM_CHECK(Holds(reel, mm_match_award_t::wrecking_ball, 0));
	MM_CHECK(Holds(reel, mm_match_award_t::bullet_sponge, 0));

	// A leader who merely edged the field does not get either.
	mm_award_player_facts_t even = quiet;
	even.damage_dealt = 320;
	even.damage_received = 320;
	const std::vector<mm_award_result_t> close =
		Select({ even, quiet, quiet }, Match(3));
	MM_CHECK_FALSE(Awarded(close, mm_match_award_t::wrecking_ball));
	MM_CHECK_FALSE(Awarded(close, mm_match_award_t::bullet_sponge));
}

MM_TEST(awards_punching_bag_ignores_a_player_who_took_no_damage) {
	using namespace awards_test;
	// 0/0 reads as ratio 0, which would pass a "dealt almost nothing" test
	// rather than failing it.
	mm_award_player_facts_t ghost;
	ghost.deaths = 20;
	ghost.damage_received = 0;
	ghost.damage_dealt = 0;
	mm_award_player_facts_t winner;
	winner.kills = 20;
	winner.deaths = 2;
	winner.damage_dealt = 4000;
	winner.damage_received = 400;

	MM_CHECK_FALSE(Awarded(Select({ ghost, winner }, Match(2)),
		mm_match_award_t::punching_bag));
}

MM_TEST(awards_quad_titles_stand_down_when_pickups_exceed_spawns) {
	using namespace awards_test;
	// Hand-dropped Quads are picked up without a corresponding spawn, so the
	// share is not a share at all; MM_AwardPercent would saturate it to 100.
	mm_award_match_facts_t match = Match(2);
	match.quad_spawns = 4;

	mm_award_player_facts_t hoarder;
	hoarder.quad_pickups = 6;
	hoarder.kills = 20;
	hoarder.quad_kills = 15;
	mm_award_player_facts_t rival;
	rival.kills = 10;

	const std::vector<mm_award_result_t> reel = Select({ hoarder, rival }, match);
	MM_CHECK_FALSE(Awarded(reel, mm_match_award_t::quad_god));
	MM_CHECK_FALSE(Awarded(reel, mm_match_award_t::quad_dummy));
}

MM_TEST(camp_cells_keep_a_dominant_area_discovered_after_the_table_filled) {
	// Regression: a first-come table recorded the first sixteen cells a player
	// visited and then dropped everything afterwards, so somebody who wandered
	// at the start and then camped for eight minutes scored as a wanderer.
	std::array<mm_match_camp_cell_t, MM_MATCH_CAMP_CELL_SLOTS> cells {};
	const size_t slots = cells.size();

	// Fill every slot with a different transient cell, one sample each.
	for (size_t i = 0; i < slots; i++)
		MM_MatchStatsRecordCampCell(cells.data(), slots,
			static_cast<int16_t>(i), 0, 0);

	// Then park somewhere entirely new for the rest of the match.
	constexpr uint32_t kCampSamples = 400;
	for (uint32_t i = 0; i < kCampSamples; i++)
		MM_MatchStatsRecordCampCell(cells.data(), slots, 999, 999, 999);

	uint32_t busiest = 0;
	const mm_match_camp_cell_t *best = nullptr;
	for (const mm_match_camp_cell_t &cell : cells) {
		if (cell.samples > busiest) {
			busiest = cell.samples;
			best = &cell;
		}
	}

	MM_CHECK(best != nullptr);
	MM_CHECK_EQ(best->x, static_cast<int16_t>(999));
	// The count carries the evicted slot's total, so it may overstate by at most
	// that minimum -- here a single sample.
	MM_CHECK(busiest >= kCampSamples);
	MM_CHECK(busiest <= kCampSamples + 1);
}

MM_TEST(camp_cells_accumulate_repeat_visits_into_one_slot) {
	std::array<mm_match_camp_cell_t, MM_MATCH_CAMP_CELL_SLOTS> cells {};
	for (int i = 0; i < 5; i++)
		MM_MatchStatsRecordCampCell(cells.data(), cells.size(), 3, -4, 1);

	MM_CHECK_EQ(cells[0].samples, 5u);
	MM_CHECK_EQ(cells[0].x, static_cast<int16_t>(3));
	MM_CHECK_EQ(cells[0].y, static_cast<int16_t>(-4));
	MM_CHECK_EQ(cells[0].z, static_cast<int16_t>(1));
	for (size_t i = 1; i < cells.size(); i++)
		MM_CHECK_EQ(cells[i].samples, 0u);
}

MM_TEST(camp_cells_tolerate_a_degenerate_table) {
	// Never dereferences a null table or a zero-length one.
	MM_MatchStatsRecordCampCell(nullptr, 4, 1, 1, 1);
	std::array<mm_match_camp_cell_t, 1> single {};
	MM_MatchStatsRecordCampCell(single.data(), 0, 1, 1, 1);
	MM_CHECK_EQ(single[0].samples, 0u);

	// A one-slot table degenerates to "whatever was seen last", which is still
	// correct for a genuine majority holder.
	MM_MatchStatsRecordCampCell(single.data(), 1, 7, 7, 7);
	MM_CHECK_EQ(single[0].samples, 1u);
	MM_MatchStatsRecordCampCell(single.data(), 1, 7, 7, 7);
	MM_CHECK_EQ(single[0].samples, 2u);
}

MM_TEST(awards_every_catalog_entry_has_a_title_a_key_and_a_tier) {
	for (size_t i = 1; i < MM_MATCH_AWARD_COUNT; i++) {
		const mm_match_award_t award = static_cast<mm_match_award_t>(i);
		MM_CHECK(MM_AwardTitle(award)[0] != '\0');
		MM_CHECK(MM_AwardKey(award)[0] != '\0');
		MM_CHECK(MM_AwardTier(award) >= 0);
	}
	MM_CHECK_EQ(MM_AwardTitle(mm_match_award_t::none)[0], '\0');
	MM_CHECK_EQ(MM_AwardKey(mm_match_award_t::none)[0], '\0');
}

MM_TEST(awards_catalog_keys_are_unique) {
	// The keys are what career tallies accumulate under, so a duplicate would
	// silently merge two unrelated titles in every stored profile.
	std::vector<std::string> keys;
	for (size_t i = 1; i < MM_MATCH_AWARD_COUNT; i++)
		keys.emplace_back(MM_AwardKey(static_cast<mm_match_award_t>(i)));
	std::sort(keys.begin(), keys.end());
	MM_CHECK(std::adjacent_find(keys.begin(), keys.end()) == keys.end());
}

MM_TEST(awards_a_full_reel_fits_the_client_layout_budget) {
	// The reel is one svc_layout message. Measured the same way the module
	// builds it: one sticky "xv 0", a header, then a yv + centred string per
	// line, using the longest title in the catalog and a full-width name.
	size_t widest_title = 0;
	for (size_t i = 1; i < MM_MATCH_AWARD_COUNT; i++)
		widest_title = std::max(widest_title,
			std::strlen(MM_AwardTitle(static_cast<mm_match_award_t>(i))));

	const size_t name_chars = MM_AWARDS_MAX_NAME_CHARS + 3;	// ellipsised
	const size_t y_digits = 5;								// sign plus four
	const size_t title_line = 3 + y_digits + 1 + 9 + 1 + widest_title + 1 + 1;
	const size_t name_line = 3 + y_digits + 1 + 8 + 1 + name_chars + 1 + 1;
	const size_t header = 5 + 3 + y_digits + 1 + 9 + 1 + 12 + 1 + 1;

	const size_t total =
		header + MM_AWARDS_DISPLAY_LIMIT * (title_line + name_line);
	MM_CHECK(total <= MM_AWARDS_LAYOUT_BUDGET);
}

} // namespace

MM_TEST(gib_severity_tiers_scale_with_overkill_and_saturate) {
	// At or above the gib threshold there is no overkill to measure.
	MM_CHECK_EQ(MM_GibsSeverity(MM_GIBS_HEALTH_THRESHOLD), 1);
	MM_CHECK_EQ(MM_GibsSeverity(0), 1);
	MM_CHECK_EQ(MM_GibsSeverity(-41), 1);
	MM_CHECK_EQ(MM_GibsSeverity(-80), 2);
	MM_CHECK_EQ(MM_GibsSeverity(-120), 3);
	MM_CHECK_EQ(MM_GibsSeverity(-160), 4);
	// Saturates rather than growing without bound on an extreme overkill.
	MM_CHECK_EQ(MM_GibsSeverity(-100000), MM_GIBS_MAX_SEVERITY);
}

MM_TEST(gib_limb_budget_grows_with_severity_and_clamps_out_of_range_tiers) {
	const mm_gib_limb_budget_t low = MM_GibsLimbBudget(1);
	MM_CHECK_EQ(low.legs, 1);
	MM_CHECK_EQ(low.bones, 2);
	// A torso chunk only appears once enough of the player is missing.
	MM_CHECK_EQ(low.torsos, 0);

	const mm_gib_limb_budget_t high = MM_GibsLimbBudget(MM_GIBS_MAX_SEVERITY);
	MM_CHECK_EQ(high.legs, 2);
	MM_CHECK_EQ(high.bones, 4);
	MM_CHECK_EQ(high.forearms, 2);
	MM_CHECK_EQ(high.arms, 2);
	MM_CHECK_EQ(high.torsos, 1);
	MM_CHECK(MM_GibsLimbBudgetTotal(high) > MM_GibsLimbBudgetTotal(low));

	// Out-of-range tiers clamp instead of producing negative counts.
	MM_CHECK_EQ(MM_GibsLimbBudgetTotal(MM_GibsLimbBudget(0)), MM_GibsLimbBudgetTotal(low));
	MM_CHECK_EQ(MM_GibsLimbBudgetTotal(MM_GibsLimbBudget(99)), MM_GibsLimbBudgetTotal(high));
}

MM_TEST(gib_meat_count_stacks_deathmatch_tiers_only) {
	// Outside deathmatch the flat set is all a death throws.
	MM_CHECK_EQ(MM_GibsMeatCount(-1000, false), 8);
	MM_CHECK_EQ(MM_GibsMeatCount(-41, true), 8);
	MM_CHECK_EQ(MM_GibsMeatCount(-101, true), 18);
	MM_CHECK_EQ(MM_GibsMeatCount(-201, true), 30);
	MM_CHECK_EQ(MM_GibsMeatCount(-301, true), 46);
	// Thresholds are exclusive, so landing exactly on one does not pay for it.
	MM_CHECK_EQ(MM_GibsMeatCount(-100, true), 8);

	// The worst case has to stay inside the shipped g_gib_max default of 192,
	// otherwise one death can evict every gib thrown by the deaths around it.
	MM_CHECK_EQ(MM_GibsWorstCaseCount(), 46 + 11 + 1);
	MM_CHECK(MM_GibsWorstCaseCount() < 192);
}

MM_TEST(gib_launch_clip_preserves_bearing_and_bounds_vertical) {
	// A launch inside the ceiling is left alone apart from the vertical floor.
	const mm_gib_launch_t gentle = MM_GibsClipLaunch({ 100.0f, 0.0f, 300.0f }, mm_gib_launch_bounds_t{ 600.0f, 150.0f, 600.0f });
	MM_CHECK_EQ(gentle.x, 100.0f);
	MM_CHECK_EQ(gentle.y, 0.0f);
	MM_CHECK_EQ(gentle.z, 300.0f);

	// Over the ceiling, both axes scale together so the bearing survives. The
	// vanilla per-axis clamp would have returned (600, 600) and rotated this
	// 2:1 launch onto the diagonal.
	const mm_gib_launch_t fast = MM_GibsClipLaunch({ 2000.0f, 1000.0f, 400.0f }, mm_gib_launch_bounds_t{ 600.0f, 150.0f, 600.0f });
	MM_CHECK(std::abs(fast.x - (2.0f * fast.y)) < 0.01f);
	const float horizontal = std::sqrt((fast.x * fast.x) + (fast.y * fast.y));
	MM_CHECK(std::abs(horizontal - 600.0f) < 0.01f);

	// Vertical is bounded from both ends.
	MM_CHECK_EQ(MM_GibsClipLaunch({ 0.0f, 0.0f, -900.0f }, mm_gib_launch_bounds_t{ 600.0f, 150.0f, 600.0f }).z, 150.0f);
	MM_CHECK_EQ(MM_GibsClipLaunch({ 0.0f, 0.0f, 5000.0f }, mm_gib_launch_bounds_t{ 600.0f, 150.0f, 600.0f }).z, 600.0f);

	// A dead-stop launch must not divide by zero.
	const mm_gib_launch_t still = MM_GibsClipLaunch({ 0.0f, 0.0f, 0.0f }, mm_gib_launch_bounds_t{ 600.0f, 150.0f, 600.0f });
	MM_CHECK_EQ(still.x, 0.0f);
	MM_CHECK_EQ(still.y, 0.0f);
}

MM_TEST(gib_water_drag_is_tick_rate_independent) {
	// Applying the per-frame factor across a full second of frames has to land
	// on the per-second figure, whatever the tick rate.
	for (const float frame_time : { 0.1f, 0.025f, 0.0125f }) {
		const float per_frame = MM_GibsWaterDrag(0.15f, frame_time);
		float retained = 1.0f;
		for (int i = 0; i < static_cast<int>(1.0f / frame_time); ++i)
			retained *= per_frame;

		MM_CHECK(std::abs(retained - 0.15f) < 0.001f);
	}

	// Degenerate inputs stay in range rather than producing NaN.
	MM_CHECK_EQ(MM_GibsWaterDrag(0.0f, 0.025f), 0.0f);
	MM_CHECK_EQ(MM_GibsWaterDrag(1.0f, 0.025f), 1.0f);
	MM_CHECK_EQ(MM_GibsWaterDrag(-1.0f, 0.025f), 0.0f);
}

int main() {
	int failures = 0;

	for (const auto &test : tests()) {
		try {
			test.fn();
			std::printf("[PASS] %s\n", test.name);
		} catch (const std::exception &e) {
			failures++;
			std::fprintf(stderr, "[FAIL] %s: %s\n", test.name, e.what());
		} catch (...) {
			failures++;
			std::fprintf(stderr, "[FAIL] %s: unknown exception\n", test.name);
		}
	}

	std::printf("%zu tests, %d failures\n", tests().size(), failures);
	return failures == 0 ? 0 : 1;
}
