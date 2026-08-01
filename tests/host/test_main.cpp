// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "fake_game_import.h"
#include "muffmode/mm_announcer_rules.h"
#include "muffmode/mm_arena_rules.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_freezetag_rules.h"
#include "muffmode/mm_ghost.h"
#include "muffmode/mm_hud_stat_contracts.h"
#include "muffmode/mm_horde_ai_rules.h"
#include "muffmode/mm_items_rules.h"
#include "muffmode/mm_lms_rules.h"
#include "muffmode/mm_loc_parse.h"
#include "muffmode/mm_map_pool.h"
#include "muffmode/mm_maps.h"
#include "muffmode/mm_message_budget.h"
#include "muffmode/mm_motd.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_pconfig_rules.h"
#include "muffmode/mm_red_rover_rules.h"
#include "muffmode/mm_spawn_rules.h"
#include "muffmode/mm_time_format.h"
#include "muffmode/mm_util.h"
#include "muffmode/mm_vote_menu.h"

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
		mm_arena_type_t::ClanArena, 32, 1));
	MM_CHECK(MM_ArenaTeamEligibleForType(
		mm_arena_type_t::RedRover, 32, 1));
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

MM_TEST(ghost_restore_authority_comes_only_from_the_current_connection) {
	MM_CHECK_FALSE(MM_GhostRestoreAdminState(false, false));
	MM_CHECK(MM_GhostRestoreAdminState(true, false));
	MM_CHECK(MM_GhostRestoreAdminState(false, true));
	MM_CHECK(MM_GhostRestoreAdminState(true, true));
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

	// At the engine's 32-client ceiling, every saved hull may be occupied at
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
