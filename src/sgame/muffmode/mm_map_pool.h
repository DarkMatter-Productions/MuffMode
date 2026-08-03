// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "muffmode/mm_maps.h"
#include "muffmode/mm_parse.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

struct gentity_t;

namespace muffmode::map_pool {

constexpr size_t MAX_POOL_FILE_BYTES = 4 * 1024 * 1024;
constexpr size_t MAX_CYCLE_FILE_BYTES = 256 * 1024;
constexpr size_t MAX_POOL_ENTRIES = 4096;
constexpr size_t MAX_CYCLE_TOKENS = 4096;
constexpr size_t MAX_TITLE_BYTES = 160;
constexpr size_t MAX_EPISODE_BYTES = 64;
constexpr size_t MAX_FILTER_BYTES = 160;

inline char AsciiLower(char value) noexcept
{
	if (value >= 'A' && value <= 'Z')
		return static_cast<char>(value + ('a' - 'A'));
	return value;
}

inline std::string AsciiFold(std::string_view value)
{
	std::string folded;
	folded.reserve(value.size());
	for (char ch : value)
		folded.push_back(AsciiLower(ch));
	return folded;
}

inline bool AsciiEqualsI(std::string_view lhs, std::string_view rhs) noexcept
{
	if (lhs.size() != rhs.size())
		return false;
	for (size_t i = 0; i < lhs.size(); i++)
		if (AsciiLower(lhs[i]) != AsciiLower(rhs[i]))
			return false;
	return true;
}

inline bool IsWellFormedUtf8(std::string_view value) noexcept
{
	return MM_IsWellFormedUtf8(value);
}

inline bool IsSafeDisplayText(
	std::string_view value,
	size_t max_bytes) noexcept
{
	if (value.size() > max_bytes || !IsWellFormedUtf8(value))
		return false;

	for (size_t i = 0; i < value.size(); i++) {
		const uint8_t byte = static_cast<uint8_t>(value[i]);
		if (byte < 0x20 || byte == 0x7f)
			return false;
		// Reject the UTF-8 encodings of C1 controls, line separators and bidi
		// controls so catalog metadata cannot inject or reorder terminal/UI text.
		if (byte == 0xc2 && i + 1 < value.size()) {
			const uint8_t next = static_cast<uint8_t>(value[i + 1]);
			if (next >= 0x80 && next <= 0x9f)
				return false;
		}
		if (byte == 0xe2 && i + 2 < value.size() &&
			static_cast<uint8_t>(value[i + 1]) == 0x80) {
			const uint8_t last = static_cast<uint8_t>(value[i + 2]);
			if ((last >= 0x8b && last <= 0x8f) ||
				(last >= 0xaa && last <= 0xae) ||
				last == 0xa8 || last == 0xa9)
				return false;
		}
		if (byte == 0xe2 && i + 2 < value.size() &&
			static_cast<uint8_t>(value[i + 1]) == 0x81) {
			const uint8_t last = static_cast<uint8_t>(value[i + 2]);
			if (last >= 0xa6 && last <= 0xa9)
				return false;
		}
		if (byte == 0xd8 && i + 1 < value.size() &&
			static_cast<uint8_t>(value[i + 1]) == 0x9c) {
			return false;
		}
		if (byte == 0xef && i + 2 < value.size() &&
			static_cast<uint8_t>(value[i + 1]) == 0xbb &&
			static_cast<uint8_t>(value[i + 2]) == 0xbf) {
			return false;
		}
	}
	return true;
}

inline std::string CanonicalMapKey(std::string_view value)
{
	std::string key;
	key.reserve(value.size());
	for (char ch : value)
		key.push_back(muffmode::maps::CanonicalMapTokenChar(ch));
	return key;
}

inline bool HasEngineMapStateExtension(std::string_view value) noexcept
{
	constexpr std::array<std::string_view, 5> EXTENSIONS = {
		".bsp", ".cin", ".dm2", ".pcx", ".png"
	};
	for (std::string_view extension : EXTENSIONS) {
		if (value.size() >= extension.size() &&
			AsciiEqualsI(value.substr(value.size() - extension.size()), extension)) {
			return true;
		}
	}
	return false;
}

inline bool IsWindowsReservedPathSegment(std::string_view segment) noexcept
{
	const std::string_view stem = segment.substr(0, segment.find('.'));
	if (AsciiEqualsI(stem, "con") || AsciiEqualsI(stem, "prn") ||
		AsciiEqualsI(stem, "aux") || AsciiEqualsI(stem, "nul")) {
		return true;
	}
	if (stem.size() != 4 || stem[3] < '1' || stem[3] > '9')
		return false;
	return AsciiEqualsI(stem.substr(0, 3), "com") ||
		AsciiEqualsI(stem.substr(0, 3), "lpt");
}

inline bool IsSafeStructuredMapIdentifier(
	std::string_view value,
	size_t max_qpath) noexcept
{
	if (!muffmode::maps::IsSafeMapTokenText(value, max_qpath) ||
		HasEngineMapStateExtension(value) || value.front() == '!' ||
		value.find('+') != std::string_view::npos ||
		value.find('$') != std::string_view::npos) {
		return false;
	}

	// Map-command identities are ASCII. Display metadata remains fully UTF-8.
	for (char ch : value) {
		const uint8_t byte = static_cast<uint8_t>(ch);
		if (byte < 0x21 || byte > 0x7e)
			return false;
	}

	if (value.size() > 5 && AsciiEqualsI(value.substr(0, 4), "maps") &&
		(value[4] == '/' || value[4] == '\\')) {
		return false;
	}

	size_t segment_start = 0;
	while (segment_start < value.size()) {
		const size_t separator = value.find_first_of("/\\", segment_start);
		const size_t segment_end = separator == std::string::npos
			? value.size()
			: separator;
		const std::string_view segment(
			value.data() + segment_start,
			segment_end - segment_start);
		if (segment.back() == '.' || IsWindowsReservedPathSegment(segment))
			return false;
		if (separator == std::string::npos)
			break;
		segment_start = separator + 1;
	}
	return true;
}

inline bool IsCanonicalStructuredMapIdentifier(
	std::string_view value,
	size_t max_qpath) noexcept
{
	if (!IsSafeStructuredMapIdentifier(value, max_qpath))
		return false;
	for (char ch : value)
		if (muffmode::maps::CanonicalMapTokenChar(ch) != ch)
			return false;
	return true;
}

inline bool IsSafeConfigLeaf(std::string_view value, size_t max_length = 128) noexcept
{
	if (value.empty() || value.size() >= max_length || value == "." || value == "..")
		return false;
	if (value.back() == '.' || value.find("..") != std::string_view::npos)
		return false;

	for (char ch : value) {
		if ((ch >= 'a' && ch <= 'z') ||
			(ch >= 'A' && ch <= 'Z') ||
			(ch >= '0' && ch <= '9')) {
			continue;
		}
		if (ch != '_' && ch != '-' && ch != '.')
			return false;
	}

	return !IsWindowsReservedPathSegment(value);
}

inline bool IsAsciiWhitespace(char value) noexcept
{
	return value == ' ' || value == '\t' || value == '\r' ||
		value == '\n' || value == '\v' || value == '\f';
}

struct cycle_parse_result_t {
	bool valid = false;
	std::vector<std::string> maps;
	size_t tokens_seen = 0;
	size_t invalid_tokens = 0;
	size_t duplicate_tokens = 0;
	std::string error;
};

// Parses the WORR-compatible whitespace list without whole-file regular
// expressions. Map order is retained and both // and /* */ comments work.
inline cycle_parse_result_t ParseCycleText(
	std::string_view text,
	size_t max_qpath,
	size_t max_tokens = MAX_CYCLE_TOKENS)
{
	cycle_parse_result_t result;
	if (text.size() >= 3 &&
		static_cast<uint8_t>(text[0]) == 0xef &&
		static_cast<uint8_t>(text[1]) == 0xbb &&
		static_cast<uint8_t>(text[2]) == 0xbf) {
		text.remove_prefix(3);
	}
	std::unordered_set<std::string> seen;
	std::string token;
	token.reserve(max_qpath);
	bool token_too_long = false;
	bool line_comment = false;
	bool block_comment = false;

	auto finish_token = [&]() -> bool {
		if (token.empty() && !token_too_long)
			return true;

		if (++result.tokens_seen > max_tokens) {
			result.error = "cycle contains too many tokens";
			return false;
		}

		if (token_too_long ||
			!IsSafeStructuredMapIdentifier(token, max_qpath)) {
			result.invalid_tokens++;
		} else {
			std::string key = CanonicalMapKey(token);
			if (seen.insert(std::move(key)).second)
				result.maps.push_back(token);
			else
				result.duplicate_tokens++;
		}

		token.clear();
		token_too_long = false;
		return true;
	};

	for (size_t i = 0; i < text.size(); i++) {
		const char ch = text[i];
		if (ch == '\0') {
			result.error = "cycle contains an embedded NUL byte";
			return result;
		}

		if (line_comment) {
			if (ch == '\n' || ch == '\r')
				line_comment = false;
			continue;
		}

		if (block_comment) {
			if (ch == '*' && i + 1 < text.size() && text[i + 1] == '/') {
				block_comment = false;
				i++;
			}
			continue;
		}

		if (ch == '/' && i + 1 < text.size()) {
			const char next = text[i + 1];
			if (next == '/' || next == '*') {
				if (!finish_token())
					return result;
				line_comment = next == '/';
				block_comment = next == '*';
				i++;
				continue;
			}
		}
		if (ch == '*' && i + 1 < text.size() && text[i + 1] == '/') {
			result.error = "cycle has an unexpected block-comment terminator";
			return result;
		}

		if (IsAsciiWhitespace(ch)) {
			if (!finish_token())
				return result;
			continue;
		}

		if (token.size() + 1 >= max_qpath) {
			token_too_long = true;
			continue;
		}

		token.push_back(ch);
	}

	if (block_comment) {
		result.error = "cycle has an unterminated block comment";
		return result;
	}
	if (!finish_token())
		return result;

	result.valid = true;
	return result;
}

enum map_mode_flags_t : uint8_t {
	MAP_MODE_NONE = 0,
	MAP_MODE_DM = 1 << 0,
	MAP_MODE_TDM = 1 << 1,
	MAP_MODE_CTF = 1 << 2,
	MAP_MODE_DUEL = 1 << 3,
	MAP_MODE_ARENA = 1 << 4
};

inline map_mode_flags_t operator|(map_mode_flags_t lhs, map_mode_flags_t rhs) noexcept
{
	return static_cast<map_mode_flags_t>(
		static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

inline map_mode_flags_t &operator|=(map_mode_flags_t &lhs, map_mode_flags_t rhs) noexcept
{
	lhs = lhs | rhs;
	return lhs;
}

inline bool HasMode(map_mode_flags_t flags, map_mode_flags_t flag) noexcept
{
	return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(flag)) != 0;
}

struct mode_selection_t {
	map_mode_flags_t preferred = MAP_MODE_DM;
	map_mode_flags_t fallback = MAP_MODE_NONE;
};

inline mode_selection_t ResolveModeSelection(
	bool arena,
	bool ctf,
	bool duel,
	bool teams,
	bool cycle_has_duel,
	bool cycle_has_tdm) noexcept
{
	if (arena)
		return { MAP_MODE_ARENA, MAP_MODE_NONE };
	if (ctf)
		return { MAP_MODE_CTF, MAP_MODE_NONE };
	if (duel && cycle_has_duel)
		return { MAP_MODE_DUEL, MAP_MODE_DM };
	if (teams && cycle_has_tdm)
		return { MAP_MODE_TDM, MAP_MODE_DM };
	return { MAP_MODE_DM, MAP_MODE_NONE };
}

struct selection_relaxation_t {
	bool enforce_player_bounds;
	bool enforce_cooldown;
};

constexpr std::array<selection_relaxation_t, 3> SELECTION_RELAXATIONS = {{
	{ true, true },
	{ true, false },
	{ false, false }
}};

enum class reload_action_t : uint8_t {
	none,
	pool,
	cycle
};

struct reload_context_t {
	bool pool_reload_pending = false;
	bool pool_disabled = false;
	bool cycle_disabled = false;
};

inline reload_action_t ResolveReloadAction(
	int pool_modified,
	int attempted_pool_modified,
	int cycle_modified,
	int attempted_cycle_modified,
	reload_context_t context = {}) noexcept
{
	// Disabling the complete structured system takes priority when both source
	// cvars are cleared together.
	if (pool_modified != attempted_pool_modified && context.pool_disabled)
		return reload_action_t::pool;
	// Clearing the cycle is generation-independent: it can safely disable an
	// LKG cycle even when a newer pool transaction is still invalid.
	if (cycle_modified != attempted_cycle_modified && context.cycle_disabled)
		return reload_action_t::cycle;
	if (pool_modified != attempted_pool_modified)
		return reload_action_t::pool;
	if (cycle_modified != attempted_cycle_modified)
		return context.pool_reload_pending
			? reload_action_t::pool
			: reload_action_t::cycle;
	return reload_action_t::none;
}

struct map_entry_t {
	std::string bsp;
	std::string canonical_bsp;
	std::string search_text_folded;
	std::string title;
	std::string episode;
	int min_players = 0;
	int max_players = 0;
	bool popular = false;
	bool custom = false;
	bool custom_textures = false;
	bool custom_sounds = false;
	map_mode_flags_t modes = MAP_MODE_NONE;
};

} // namespace muffmode::map_pool

// Structured map-pool lifecycle and queries.
void MM_InitMapPoolSystem();
void MM_ShutdownMapPoolSystem();
void MM_HandleMapPoolCvarChanges();
void MM_RecordStructuredMapPlayed();
bool MM_ReloadMapPool(gentity_t *ent);
bool MM_ReloadMapCycle(gentity_t *ent);

bool MM_StructuredMapPoolLoaded();
bool MM_StructuredMapCycleActive();
size_t MM_StructuredMapPoolCount();
size_t MM_StructuredMapCycleCount();
uint64_t MM_MapPoolRevision();
bool MM_StructuredMapPoolContains(const char *mapname);
bool MM_ResolveStructuredMapName(const char *mapname, std::string &resolved);
std::vector<std::string> MM_CollectStructuredMapPool();
std::vector<std::string> MM_CollectStructuredMapCycle();
bool MM_SelectStructuredNextMap(std::string &mapname);
bool MM_SelectStructuredCycleStartMap(std::string &mapname);

// Player/admin command bodies.
void MM_CmdMapPool(gentity_t *ent);
void MM_CmdMapCycle(gentity_t *ent);
void MM_CmdLoadMapPool(gentity_t *ent);
void MM_CmdLoadMapCycle(gentity_t *ent);
