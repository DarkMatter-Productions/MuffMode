// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

// Host-testable helpers for map-load ownership of non-trivial level state.
// SpawnEntities must destroy prior std::string members on every gamemap; memset
// alone leaks their heap blocks and is undefined for non-trivial types.

struct mm_level_cpp_state_t {
	std::string vote_arg;
	std::string entstring;
	std::string match_id;
};

// Destroy any prior dynamic members, then adopt a single owned entity-lump copy.
inline void MM_ResetLevelCppState(mm_level_cpp_state_t &state, std::string &&entity_string)
{
	state = {};
	state.entstring = std::move(entity_string);
}

inline const char *MM_EntityStringCStr(const mm_level_cpp_state_t &state) noexcept
{
	return state.entstring.c_str();
}

// Entity lifetimes use (slot, generation) pairs. Advancing through the signed
// range explicitly avoids undefined overflow while ensuring a slot recycled by
// a world reload can never satisfy a handle captured before that reload.
constexpr int32_t MM_NextEntityGeneration(int32_t generation) noexcept
{
	return generation == std::numeric_limits<int32_t>::max()
		? std::numeric_limits<int32_t>::min()
		: generation + 1;
}

struct mm_entity_lump_validation_t {
	bool valid = false;
	size_t entity_count = 0;
	const char *error = "empty entity string";
};

namespace muffmode::spawn_rules::detail {

constexpr size_t ENTITY_TOKEN_CAPACITY = 512;

struct entity_token_t {
	std::string_view text;
	bool present = false;
	bool quoted = false;
	bool valid = true;
};

inline bool IsEntityWhitespace(char c) noexcept
{
	return c == '\r' || c == '\n' || c == '\t' || c == ' ';
}

inline bool EqualsAsciiI(std::string_view lhs, std::string_view rhs) noexcept
{
	if (lhs.size() != rhs.size())
		return false;

	for (size_t i = 0; i < lhs.size(); i++) {
		char a = lhs[i];
		char b = rhs[i];
		if (a >= 'A' && a <= 'Z')
			a = static_cast<char>(a - 'A' + 'a');
		if (b >= 'A' && b <= 'Z')
			b = static_cast<char>(b - 'A' + 'a');
		if (a != b)
			return false;
	}
	return true;
}

inline entity_token_t NextEntityToken(std::string_view input, size_t &cursor) noexcept
{
	for (;;) {
		while (cursor < input.size() && IsEntityWhitespace(input[cursor]))
			cursor++;

		if (cursor >= input.size())
			return {};

		if (input[cursor] == '/' && cursor + 1 < input.size() && input[cursor + 1] == '/') {
			cursor += 2;
			while (cursor < input.size() && input[cursor] != '\n')
				cursor++;
			continue;
		}
		break;
	}

	const bool quoted = input[cursor] == '"';
	const size_t begin = cursor + (quoted ? 1 : 0);
	if (quoted) {
		cursor++;
		while (cursor < input.size() && input[cursor] != '"') {
			if (input[cursor] == '\0')
				return { {}, true, true, false };
			cursor++;
		}
		if (cursor >= input.size())
			return { {}, true, true, false };

		const size_t length = cursor - begin;
		cursor++;
		return {
			input.substr(begin, length),
			true,
			true,
			length < ENTITY_TOKEN_CAPACITY
		};
	}

	while (cursor < input.size() && !IsEntityWhitespace(input[cursor])) {
		if (input[cursor] == '\0')
			return { {}, true, false, false };
		cursor++;
	}

	const size_t length = cursor - begin;
	return {
		input.substr(begin, length),
		true,
		false,
		length < ENTITY_TOKEN_CAPACITY
	};
}

} // namespace muffmode::spawn_rules::detail

// Bounded, non-allocating preflight for the entity grammar consumed by
// COM_Parse/ED_ParseEntity. Unlike the engine parser, this reports malformed
// data without calling Com_Error, so a live match can retain its current world
// and fall back to the legacy reset before any destructive work begins.
inline mm_entity_lump_validation_t MM_ValidateEntityLump(
	std::string_view entity_lump, size_t max_entity_definitions) noexcept
{
	using namespace muffmode::spawn_rules::detail;

	mm_entity_lump_validation_t result;
	if (entity_lump.empty())
		return result;
	if (entity_lump.find('\0') != std::string_view::npos) {
		result.error = "embedded NUL byte";
		return result;
	}
	if (max_entity_definitions == 0) {
		result.error = "no entity capacity";
		return result;
	}

	size_t cursor = 0;
	for (;;) {
		const entity_token_t opening = NextEntityToken(entity_lump, cursor);
		if (!opening.valid) {
			result.error = "unterminated quote or oversized token";
			return result;
		}
		if (!opening.present)
			break;
		if (opening.quoted || opening.text != "{") {
			result.error = "expected opening brace";
			return result;
		}

		result.entity_count++;
		if (result.entity_count > max_entity_definitions) {
			result.error = "entity count exceeds capacity";
			return result;
		}

		bool found_closing_brace = false;
		bool found_classname = false;
		bool is_worldspawn = false;
		for (;;) {
			const entity_token_t key = NextEntityToken(entity_lump, cursor);
			if (!key.valid) {
				result.error = "unterminated quote or oversized token";
				return result;
			}
			if (!key.present) {
				result.error = "EOF without closing brace";
				return result;
			}
			// ED_ParseEntity treats any token beginning with '}' as a closing
			// brace, including quoted tokens. Match that first-byte behavior so
			// malformed data cannot pass preflight and fail after teardown.
			if (!key.text.empty() && key.text.front() == '}') {
				found_closing_brace = true;
				break;
			}
			if (key.text == "{") {
				result.error = "unexpected opening brace";
				return result;
			}

			const entity_token_t value = NextEntityToken(entity_lump, cursor);
			if (!value.valid) {
				result.error = "unterminated quote or oversized token";
				return result;
			}
			if (!value.present) {
				result.error = "missing value";
				return result;
			}
			if (value.text == "{" ||
				(!value.text.empty() && value.text.front() == '}')) {
				result.error = "structural token used as value";
				return result;
			}

			if (EqualsAsciiI(key.text, "classname")) {
				found_classname = true;
				// Spawn dispatch is case-sensitive. Requiring the canonical
				// spelling prevents a lookalike world entity from occupying the
				// special slot without ever running SP_worldspawn.
				is_worldspawn = value.text == "worldspawn";
			}
		}

		if (!found_closing_brace) {
			result.error = "EOF without closing brace";
			return result;
		}
		if (result.entity_count == 1 && (!found_classname || !is_worldspawn)) {
			result.error = "first entity is not worldspawn";
			return result;
		}
		if (result.entity_count > 1 && !found_classname) {
			result.error = "entity is missing classname";
			return result;
		}
		if (result.entity_count > 1 && found_classname && is_worldspawn) {
			result.error = "duplicate worldspawn";
			return result;
		}
	}

	if (result.entity_count == 0)
		return result;

	result.valid = true;
	result.error = nullptr;
	return result;
}

// Replace the final path component of an absolute/relative file path in-place.
// Returns false when the result would not fit in `capacity` (including NUL).
inline bool MM_ReplacePathFilename(char *path, size_t capacity, const char *filename) noexcept
{
	if (!path || !filename || capacity == 0)
		return false;

	char *slash = nullptr;
	for (char *p = path; *p; ++p) {
		if (*p == '\\' || *p == '/')
			slash = p;
	}

	const size_t dir_len = slash ? static_cast<size_t>(slash - path + 1) : 0;
	const size_t file_len = std::strlen(filename);
	if (dir_len + file_len + 1 > capacity)
		return false;

	std::memcpy(path + dir_len, filename, file_len + 1);
	return true;
}

// Join a directory and filename into `out`. Appends a separator when needed.
inline bool MM_JoinDirectoryFile(char *out, size_t capacity, const char *directory,
	const char *filename) noexcept
{
	if (!out || !directory || !filename || capacity == 0)
		return false;

	const size_t dir_len = std::strlen(directory);
	const size_t file_len = std::strlen(filename);
	const bool needs_sep = dir_len > 0 && directory[dir_len - 1] != '\\' &&
		directory[dir_len - 1] != '/';
	const size_t sep_len = needs_sep ? 1u : 0u;

	if (dir_len + sep_len + file_len + 1 > capacity)
		return false;

	std::memcpy(out, directory, dir_len);
	size_t cursor = dir_len;
	if (needs_sep)
		out[cursor++] = '\\';
	std::memcpy(out + cursor, filename, file_len + 1);
	return true;
}
