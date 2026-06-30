// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "fake_game_import.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_hud_stat_contracts.h"
#include "muffmode/mm_horde_ai_rules.h"
#include "muffmode/mm_lms_rules.h"
#include "muffmode/mm_loc_parse.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_pconfig_rules.h"
#include "muffmode/mm_red_rover_rules.h"
#include "muffmode/mm_time_format.h"

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
}

MM_TEST(loc_line_parser_rejects_malformed_lines) {
	float xyz[3] = { 0.f, 0.f, 0.f };
	std::string label;

	MM_CHECK_FALSE(MM_ParseLocLine(nullptr, xyz, label));
	MM_CHECK_FALSE(MM_ParseLocLine("", xyz, label));
	MM_CHECK_FALSE(MM_ParseLocLine("   \r\n", xyz, label));
	MM_CHECK_FALSE(MM_ParseLocLine("1 2", xyz, label));
	MM_CHECK_FALSE(MM_ParseLocLine("1 2 3", xyz, label));
	MM_CHECK_FALSE(MM_ParseLocLine("1 2 x LABEL", xyz, label));
	MM_CHECK_FALSE(MM_ParseLocLine("1 2 3x LABEL", xyz, label));
	MM_CHECK_FALSE(MM_ParseLocLine("1 2 nan LABEL", xyz, label));
	MM_CHECK_FALSE(MM_ParseLocLine("1 2 inf LABEL", xyz, label));
	MM_CHECK_FALSE(MM_ParseLocLine("1 2 1e1000 LABEL", xyz, label));
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

	MM_CHECK_FALSE(MM_BuildLocBody("%h %a", "WATER").empty());
	MM_CHECK_EQ(MM_BuildLocBody("at %l %h", "MEGA"), std::string("at [MEGA] %h"));
	MM_CHECK(MM_LocBodyHasMacro("need %w now"));
	MM_CHECK_FALSE(MM_LocBodyHasMacro("just plain text"));
}

MM_TEST(loc_body_caps_length) {
	std::string long_text = "%l " + std::string(400, 'x');
	MM_CHECK(MM_BuildLocBody(long_text.c_str(), "WATER").size() <= 150);
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
}

MM_TEST(hud_stat_count_within_max_stats) {
	MM_CHECK((int)STAT_LAST <= (int)MAX_STATS);
	MM_CHECK((int)STAT_ARENA_ROLE < (int)MAX_STATS);
}

MM_TEST(hud_stat_contract_miniscore_val_visibility) {
	MM_CHECK_FALSE(MM_MiniscoreValVisible(0));
	MM_CHECK(MM_MiniscoreValVisible(1));
	MM_CHECK(MM_StatusbarLayoutLengthWithinBudget(MM_STATUSBAR_LAYOUT_MAX_CHARS));
	MM_CHECK_FALSE(MM_StatusbarLayoutLengthWithinBudget(MM_STATUSBAR_LAYOUT_MAX_CHARS + 1));
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
