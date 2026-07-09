// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "fake_game_import.h"
#include "muffmode/mm_announcer_rules.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_freezetag_rules.h"
#include "muffmode/mm_hud_stat_contracts.h"
#include "muffmode/mm_horde_ai_rules.h"
#include "muffmode/mm_lms_rules.h"
#include "muffmode/mm_loc_parse.h"
#include "muffmode/mm_maps.h"
#include "muffmode/mm_motd.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_pconfig_rules.h"
#include "muffmode/mm_red_rover_rules.h"
#include "muffmode/mm_time_format.h"
#include "muffmode/mm_util.h"

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

MM_TEST(parse_uint32_rejects_signed_malformed_and_overflow_values) {
	MM_CHECK_EQ(*MM_ParseUInt32Arg("0"), 0u);
	MM_CHECK_EQ(*MM_ParseUInt32Arg("4294967295"), std::numeric_limits<uint32_t>::max());
	MM_CHECK_FALSE(MM_ParseUInt32Arg(""));
	MM_CHECK_FALSE(MM_ParseUInt32Arg("-1"));
	MM_CHECK_FALSE(MM_ParseUInt32Arg("+1"));
	MM_CHECK_FALSE(MM_ParseUInt32Arg(" 1"));
	MM_CHECK_FALSE(MM_ParseUInt32Arg("1x"));
	MM_CHECK_FALSE(MM_ParseUInt32Arg("4294967296"));

	const char embedded_null[] = { '1', '\0', '2' };
	MM_CHECK_FALSE(MM_ParseUInt32Text(std::string_view(embedded_null, sizeof(embedded_null))));
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

MM_TEST(scoreboard_footer_reserve_keeps_layout_room_available) {
	MM_CHECK_EQ(MM_ScoreboardFooterReserve(false), 96u);
	MM_CHECK_EQ(MM_ScoreboardFooterReserve(true), 320u);
	MM_CHECK(MM_ScoreboardCanAppend(0, 1, 1400, false));
	MM_CHECK(MM_ScoreboardCanAppend(1304, 0, 1400, false));
	MM_CHECK_FALSE(MM_ScoreboardCanAppend(1304, 1, 1400, false));
	MM_CHECK_FALSE(MM_ScoreboardCanAppend(1080, 1, 1400, true));
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

MM_TEST(horde_adaptive_spawn_mult_bounds_and_direction) {
	MM_CHECK_EQ(MM_Horde_ClampAdaptiveSpawnMult(2.f), 1.35f);
	MM_CHECK_EQ(MM_Horde_ClampAdaptiveSpawnMult(0.1f), 0.65f);

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
		"if 264 xr -24 yt 68 loc_rstring 0 Monsters endif "
		"if 18 xl 0 yb -60 if 18 pic 18 endif endif";
	MM_CHECK_FALSE(MM_StatusbarLayoutContainsBannedToken(sample));
	MM_CHECK(MM_StatusbarLayoutUsesOnlyVanillaTokens(sample));

	MM_CHECK(MM_StatusbarLayoutUsesOnlyVanillaTokens("if 1 string2 \"USE VIEW JUMP\" endif "));
	MM_CHECK(MM_StatusbarLayoutUsesOnlyVanillaTokens("if 1 loc_rstring 0 Monsters endif "));
	MM_CHECK_FALSE(MM_StatusbarLayoutUsesOnlyVanillaTokens("if 1 unknown_token 2 endif "));
	MM_CHECK_FALSE(MM_StatusbarLayoutUsesOnlyVanillaTokens("if not_a_stat endif "));
	MM_CHECK_FALSE(MM_StatusbarLayoutUsesOnlyVanillaTokens("string2 \"unterminated "));
	MM_CHECK_FALSE(MM_StatusbarLayoutUsesOnlyVanillaTokens("xv string_operand "));
}

MM_TEST(hud_stat_count_within_max_stats) {
	MM_CHECK((int)STAT_LAST <= (int)MAX_STATS);
	MM_CHECK((int)STAT_ARENA_ROLE < (int)MAX_STATS);
}

MM_TEST(hud_pov_configstring_lanes_do_not_overlap_global_countdown_or_each_other) {
	constexpr size_t kTypicalMaxClients = 32;
	MM_CHECK(CONFIG_COUNTDOWN_HEADER < CONFIG_POV_CENTER_POOL);

	const int primary = MM_PovConfigStringForClient(1, kTypicalMaxClients, mm_pov_configstring_lane_t::Primary);
	const int secondary = MM_PovConfigStringForClient(1, kTypicalMaxClients, mm_pov_configstring_lane_t::Secondary);

	MM_CHECK(primary != 0);
	MM_CHECK(secondary != 0);
	MM_CHECK(primary != secondary);
	MM_CHECK(primary != CONFIG_COUNTDOWN_HEADER);
	MM_CHECK(secondary != CONFIG_COUNTDOWN_HEADER);
	MM_CHECK_EQ(MM_PovConfigStringForClient(CONFIG_POV_CENTER_POOL_SLOTS, kTypicalMaxClients, mm_pov_configstring_lane_t::Primary), 0);
	MM_CHECK_EQ(MM_PovConfigStringForClient(0, CONFIG_POV_CENTER_POOL_SLOTS, mm_pov_configstring_lane_t::Secondary), 0);
}

MM_TEST(hud_stat_contract_miniscore_val_visibility) {
	MM_CHECK_FALSE(MM_MiniscoreValVisible(0));
	MM_CHECK(MM_MiniscoreValVisible(1));
	MM_CHECK(MM_StatusbarLayoutLengthWithinBudget(MM_STATUSBAR_LAYOUT_MAX_CHARS));
	MM_CHECK_FALSE(MM_StatusbarLayoutLengthWithinBudget(MM_STATUSBAR_LAYOUT_MAX_CHARS + 1));
}

MM_TEST(announcer_decision_off_with_backup_plays_backup_only) {
	const announce_action_t action = MM_AnnounceDecision(false, true, false, true, false);
	MM_CHECK_FALSE(action.play_vo);
	MM_CHECK(action.play_backup);
	MM_CHECK_FALSE(action.play_sting);
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

} // namespace

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
