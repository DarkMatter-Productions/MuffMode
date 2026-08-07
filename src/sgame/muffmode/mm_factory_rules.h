// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

// [MuffMode] Factory document parsing and the safety predicates around it.
//
// A factory is a named preset: one base gametype plus an ordered set of cvar
// overrides and structured directives. It is *data* -- it never emits a console
// command, so there is no exec/alias/vstr expansion to inspect, every override
// is attributable, and switching factories restores exactly what the outgoing
// one changed. That is what let the mod retire the per-gametype config files
// this replaced, and the command-buffer filter they needed.
//
// Everything here is pure: no game headers, no cvars, no filesystem. The loader
// in mm_factory.cpp supplies the bytes; this decides what they mean.

#include "muffmode/mm_factory_table.h"
#include "muffmode/mm_map_pool.h"
#include "muffmode/mm_parse.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace muffmode {
namespace factory {

// Bounds. Exceeding any of these is a fatal document error rather than a
// partial load: a truncated registry is indistinguishable from an intended one.
inline constexpr size_t MM_FACTORY_MAX_FACTORIES = 192;
inline constexpr size_t MM_FACTORY_MAX_OVERRIDES = 64;
inline constexpr size_t MM_FACTORY_MAX_BYTES = 256 * 1024;
inline constexpr size_t MM_FACTORY_MAX_LINES = 4096;
inline constexpr size_t MM_FACTORY_MAX_LINE_LEN = 512;
inline constexpr size_t MM_FACTORY_ID_MAX = 31;
inline constexpr size_t MM_FACTORY_TITLE_MAX = 47;
inline constexpr size_t MM_FACTORY_DESC_MAX = 127;
inline constexpr size_t MM_FACTORY_VALUE_MAX = 63;
inline constexpr size_t MM_FACTORY_MAX_MAPS = 128;
// The engine's own ceiling; the game clamps maxclients to it at PreInit.
inline constexpr size_t MM_FACTORY_MAX_PLAYERS = 128;
// Matches the engine's MAX_QPATH. Spelled out rather than included so this
// header stays free of game headers and host-testable.
inline constexpr size_t MM_FACTORY_MAX_QPATH = 64;

// Map rotation order, mirroring g_map_list_shuffle. Named rather than numeric
// in a factory file because "2" tells an author nothing.
enum class mm_factory_rotation_t : uint8_t {
	Inherit = 0xff,			// not stated; keep whatever is configured
	Sequential = 0,			// walk the list in order
	ShuffleOnWrap = 1,		// reshuffle every time the list wraps
	ShufflePerGametype = 2	// shuffle once per gametype session
};

inline mm_factory_rotation_t MM_ParseFactoryRotation(std::string_view word) noexcept
{
	if (MM_EqualsAsciiI(word, "sequential"))
		return mm_factory_rotation_t::Sequential;
	if (MM_EqualsAsciiI(word, "shuffle-on-wrap"))
		return mm_factory_rotation_t::ShuffleOnWrap;
	if (MM_EqualsAsciiI(word, "shuffle-per-gametype"))
		return mm_factory_rotation_t::ShufflePerGametype;
	return mm_factory_rotation_t::Inherit;
}

// The spelling a factory file uses, so `factory info` echoes back the word an
// author would have written rather than the enum's number.
inline const char *MM_FactoryRotationName(mm_factory_rotation_t rotation) noexcept
{
	switch (rotation) {
	case mm_factory_rotation_t::Sequential:			return "sequential";
	case mm_factory_rotation_t::ShuffleOnWrap:		return "shuffle-on-wrap";
	case mm_factory_rotation_t::ShufflePerGametype:	return "shuffle-per-gametype";
	default:										return "inherit";
	}
}

// A map rotation and the player limits are not plain cvar overrides. A raw
// `set g_map_list "..."` could not work: the value is a whitespace list that
// routinely exceeds MM_FACTORY_VALUE_MAX, and nothing would validate the map
// tokens -- which are interpolated straight into a `gamemap` command. These
// directives carry the same structure the map system already validates, and the
// limits are range-checked against each other rather than written blind.
struct mm_factory_scope_t {
	std::vector<std::string>	maps;
	std::vector<std::string>	mappool;
	mm_factory_rotation_t		rotation = mm_factory_rotation_t::Inherit;
	int							min_players = -1;	// -1 = not stated
	int							max_players = -1;

	bool Empty() const noexcept
	{
		return maps.empty() && mappool.empty() &&
			rotation == mm_factory_rotation_t::Inherit &&
			min_players < 0 && max_players < 0;
	}
};

// A child's list REPLACES its parent's rather than appending: a variant that
// names its own rotation means "play these", not "these as well as those".
// Scalars the child leaves unstated inherit.
inline mm_factory_scope_t MM_MergeScope(
	const mm_factory_scope_t &parent, const mm_factory_scope_t &child)
{
	mm_factory_scope_t merged = parent;
	if (!child.maps.empty())
		merged.maps = child.maps;
	if (!child.mappool.empty())
		merged.mappool = child.mappool;
	if (child.rotation != mm_factory_rotation_t::Inherit)
		merged.rotation = child.rotation;
	if (child.min_players >= 0)
		merged.min_players = child.min_players;
	if (child.max_players >= 0)
		merged.max_players = child.max_players;
	return merged;
}

// ---------------------------------------------------------------------------
// Safety predicates.
// ---------------------------------------------------------------------------

// An id is a console command argument, a vote argument and a serverinfo value at
// once, so it stays a single unambiguous lowercase token.
inline bool MM_IsSafeFactoryId(std::string_view id) noexcept
{
	if (id.empty() || id.size() > MM_FACTORY_ID_MAX)
		return false;
	for (const char c : id) {
		const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
			c == '_' || c == '-';
		if (!ok)
			return false;
	}
	return true;
}

// `factory <id>` shares its argument slot with the subcommands, so an id that
// spells one is a factory nobody can ever select: `factory none` would clear
// instead of selecting it, and `factory list` would list. Reject it at parse
// time with a reason rather than registering something silently unreachable.
inline bool MM_IsReservedFactoryId(std::string_view id) noexcept
{
	for (const std::string_view reserved : { std::string_view("all"),
			std::string_view("clear"), std::string_view("cvars"),
			std::string_view("diag"), std::string_view("info"),
			std::string_view("list"), std::string_view("none"),
			std::string_view("reload") }) {
		if (id == reserved)
			return true;
	}
	return false;
}

// A leading underscore hides a factory from listings and from the vote menu,
// without hiding it from an operator who names it explicitly.
inline bool MM_IsHiddenFactoryId(std::string_view id) noexcept
{
	return !id.empty() && id.front() == '_';
}

// The exact byte set the engine's info-string validation silently drops before
// it ever reaches the FROM_CODE branch of a cvar write. Rejecting these at parse
// time turns a runtime no-op nobody can see into a named config error.
inline bool MM_IsInfoStringSafe(std::string_view value, size_t max_length) noexcept
{
	if (value.size() > max_length)
		return false;
	for (const char c : value) {
		const unsigned char byte = static_cast<unsigned char>(c);
		if (byte < 0x20 || byte == 0x7f)
			return false;
		if (c == '\\' || c == ';' || c == '"')
			return false;
	}
	return true;
}

inline bool MM_IsSafeFactoryValue(std::string_view value) noexcept
{
	// An empty value is legitimate (clearing g_start_items, for instance).
	return MM_IsInfoStringSafe(value, MM_FACTORY_VALUE_MAX);
}

inline bool MM_IsSafeFactoryTitle(std::string_view title) noexcept
{
	return !title.empty() &&
		MM_IsInfoStringSafe(title, MM_FACTORY_TITLE_MAX) &&
		MM_IsWellFormedUtf8(title);
}

// A description is only ever printed to a client, never published in an info
// string, so it is held to the display rules rather than the info-string ones:
// no control bytes and valid UTF-8, but punctuation such as `;` is fine.
inline bool MM_IsSafeFactoryDesc(std::string_view desc) noexcept
{
	if (desc.size() > MM_FACTORY_DESC_MAX || !MM_IsWellFormedUtf8(desc))
		return false;
	for (const char c : desc) {
		const unsigned char byte = static_cast<unsigned char>(c);
		if (byte < 0x20 || byte == 0x7f)
			return false;
		// A quote would have terminated the token before reaching here; reject
		// it explicitly so an unquoted description cannot smuggle one in.
		if (c == '"')
			return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// Tokenizer. Quote-aware, comment-aware, allocation-free.
// ---------------------------------------------------------------------------

struct factory_token_t {
	std::string_view	text;
	bool				quoted = false;
};

enum class factory_tokenize_t : uint8_t {
	Ok,
	UnterminatedQuote,
	TooManyTokens
};

// A `maps` line carries a rotation, so it needs far more room than a `set`.
inline constexpr size_t MM_FACTORY_MAX_TOKENS_PER_LINE = 32;

// `//` and `#` start a comment outside quotes. Inside quotes every byte but the
// closing quote is data, so a title may contain `#` or `//`.
inline factory_tokenize_t MM_TokenizeFactoryLine(
	std::string_view line, std::vector<factory_token_t> &out)
{
	out.clear();

	size_t cursor = 0;
	while (cursor < line.size()) {
		while (cursor < line.size() && MM_IsAsciiWhitespace(line[cursor]))
			cursor++;
		if (cursor >= line.size())
			break;

		if (line[cursor] == '#')
			break;
		if (line[cursor] == '/' && cursor + 1 < line.size() &&
			line[cursor + 1] == '/')
			break;

		if (out.size() >= MM_FACTORY_MAX_TOKENS_PER_LINE)
			return factory_tokenize_t::TooManyTokens;

		if (line[cursor] == '"') {
			const size_t begin = ++cursor;
			while (cursor < line.size() && line[cursor] != '"')
				cursor++;
			if (cursor >= line.size())
				return factory_tokenize_t::UnterminatedQuote;
			out.push_back({ line.substr(begin, cursor - begin), true });
			cursor++;
			continue;
		}

		const size_t begin = cursor;
		while (cursor < line.size() && !MM_IsAsciiWhitespace(line[cursor]))
			cursor++;
		out.push_back({ line.substr(begin, cursor - begin), false });
	}

	return factory_tokenize_t::Ok;
}

// The line with any unquoted `//` or `#` comment removed. Values taken as a
// raw tail need this: without it, `set timelimit 20 // ten minutes` would set
// timelimit to the whole comment.
inline std::string_view MM_StripFactoryComment(std::string_view line) noexcept
{
	bool in_quotes = false;
	for (size_t i = 0; i < line.size(); i++) {
		const char c = line[i];
		if (c == '"') {
			in_quotes = !in_quotes;
			continue;
		}
		if (in_quotes)
			continue;
		if (c == '#')
			return line.substr(0, i);
		if (c == '/' && i + 1 < line.size() && line[i + 1] == '/')
			return line.substr(0, i);
	}
	return line;
}

// A `set` directive's value is everything after the cvar name, so an unquoted
// multi-word value (`set g_start_items item_bandolier item_pack`) is preserved
// verbatim rather than silently truncated to its first word. Pass the
// comment-stripped line.
inline std::string_view MM_FactoryTailAfterToken(
	std::string_view line, std::string_view token) noexcept
{
	const char *line_end = line.data() + line.size();
	const char *token_end = token.data() + token.size();
	if (token_end < line.data() || token_end > line_end)
		return {};
	return MM_TrimAsciiWhitespace(
		std::string_view(token_end, static_cast<size_t>(line_end - token_end)));
}

// ---------------------------------------------------------------------------
// Parsed document.
// ---------------------------------------------------------------------------

struct parsed_override_t {
	std::string name;
	std::string value;
};

struct parsed_factory_t {
	std::string						id;
	std::string						title;
	std::string						desc;
	std::string						inherit;
	std::string						base_token;
	std::string						ruleset_token;
	std::vector<parsed_override_t>	overrides;
	mm_factory_scope_t				scope;
	size_t							line = 0;
	bool							hidden = false;
};

struct factory_parse_result_t {
	// False only when the document itself is unusable (bounds exceeded,
	// unbalanced braces). Individual bad blocks are rejected and counted
	// without condemning the file.
	bool							valid = false;
	std::vector<parsed_factory_t>	factories;
	std::vector<std::string>		errors;
	size_t							rejected_blocks = 0;
};

namespace parse_detail {

inline std::string Where(std::string_view filename, size_t line)
{
	std::string out(filename);
	out += ':';
	out += std::to_string(line);
	out += ": ";
	return out;
}

inline void Fail(factory_parse_result_t &result, std::string_view filename,
	size_t line, std::string_view message)
{
	if (result.errors.size() < MM_FACTORY_MAX_FACTORIES * 2) {
		std::string text = Where(filename, line);
		text.append(message);
		result.errors.push_back(std::move(text));
	}
}

inline std::string LowerCopy(std::string_view value)
{
	std::string out;
	out.reserve(value.size());
	for (const char c : value)
		out.push_back(MM_ToAsciiLower(c));
	return out;
}

} // namespace parse_detail

inline const parsed_override_t *MM_FindOverride(
	const std::vector<parsed_override_t> &set, std::string_view name) noexcept
{
	for (const parsed_override_t &entry : set)
		if (entry.name == name)
			return &entry;
	return nullptr;
}

// Child overrides win. The parent's ordering is preserved ahead of the child's
// additions, so an operator reading the flattened set reads it in the order the
// layers were declared.
inline std::vector<parsed_override_t> MM_MergeOverrides(
	const std::vector<parsed_override_t> &parent,
	const std::vector<parsed_override_t> &child)
{
	std::vector<parsed_override_t> merged;
	merged.reserve(parent.size() + child.size());

	for (const parsed_override_t &entry : parent) {
		const parsed_override_t *replacement = MM_FindOverride(child, entry.name);
		merged.push_back({ entry.name, replacement ? replacement->value : entry.value });
	}

	for (const parsed_override_t &entry : child)
		if (!MM_FindOverride(parent, entry.name))
			merged.push_back(entry);

	return merged;
}

// Parses a whole document. Fail-closed and bounded; every rejection names the
// file, the line and the reason.
inline factory_parse_result_t MM_ParseFactoryDocument(
	std::string_view text, std::string_view filename)
{
	factory_parse_result_t result;

	if (text.size() > MM_FACTORY_MAX_BYTES) {
		parse_detail::Fail(result, filename, 0,
			"document exceeds the " +
			std::to_string(MM_FACTORY_MAX_BYTES) + " byte limit");
		return result;
	}

	// Strip a UTF-8 BOM so the first directive is not mistaken for garbage.
	if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xef &&
		static_cast<unsigned char>(text[1]) == 0xbb &&
		static_cast<unsigned char>(text[2]) == 0xbf) {
		text.remove_prefix(3);
	}

	std::vector<factory_token_t> tokens;
	parsed_factory_t current;
	bool in_block = false;
	bool block_failed = false;
	size_t block_line = 0;
	size_t line_number = 0;
	size_t cursor = 0;
	bool document_failed = false;

	const auto reject_block = [&](const std::string &message) {
		if (!block_failed) {
			parse_detail::Fail(result, filename, line_number, message);
			result.rejected_blocks++;
			block_failed = true;
		}
	};

	// Opens a new block. Returns false when the document cannot continue.
	const auto begin_block = [&]() {
		if (result.factories.size() >= MM_FACTORY_MAX_FACTORIES) {
			parse_detail::Fail(result, filename, line_number,
				"more than " + std::to_string(MM_FACTORY_MAX_FACTORIES) +
				" factories in one document");
			document_failed = true;
			return;
		}

		current = parsed_factory_t{};
		current.line = line_number;
		in_block = true;
		block_failed = false;
		block_line = line_number;

		if (tokens.size() < 2) {
			reject_block("`factory` requires an id");
			return;
		}

		const std::string id = parse_detail::LowerCopy(tokens[1].text);
		if (!MM_IsSafeFactoryId(id)) {
			reject_block("invalid factory id `" + std::string(tokens[1].text) +
				"` (use 1-31 characters of a-z, 0-9, _ or -)");
			return;
		}
		if (MM_IsReservedFactoryId(id)) {
			reject_block("`" + id + "` is a `factory` subcommand, so a factory "
				"with that id could never be selected");
			return;
		}

		current.id = id;
		current.hidden = MM_IsHiddenFactoryId(id);

		// `{` may trail the factory line or stand alone on the next line.
		if (tokens.size() >= 3 && tokens[2].text != "{")
			reject_block("expected `{` after the factory id");
	};

	while (cursor < text.size() && !document_failed) {
		const size_t newline = text.find('\n', cursor);
		const size_t end = newline == std::string_view::npos ? text.size() : newline;
		std::string_view line = text.substr(cursor, end - cursor);
		if (!line.empty() && line.back() == '\r')
			line.remove_suffix(1);
		cursor = end == text.size() ? end : end + 1;
		line_number++;

		if (line_number > MM_FACTORY_MAX_LINES) {
			parse_detail::Fail(result, filename, line_number,
				"document exceeds the " +
				std::to_string(MM_FACTORY_MAX_LINES) + " line limit");
			return result;
		}
		if (line.size() > MM_FACTORY_MAX_LINE_LEN) {
			parse_detail::Fail(result, filename, line_number,
				"line exceeds the " +
				std::to_string(MM_FACTORY_MAX_LINE_LEN) + " character limit");
			return result;
		}

		// Values taken as a raw tail must come from the comment-stripped line.
		const std::string_view content = MM_StripFactoryComment(line);
		const factory_tokenize_t tokenized = MM_TokenizeFactoryLine(line, tokens);
		if (tokenized == factory_tokenize_t::UnterminatedQuote) {
			parse_detail::Fail(result, filename, line_number,
				"unterminated quoted string");
			return result;
		}
		if (tokenized == factory_tokenize_t::TooManyTokens) {
			reject_block("too many tokens on one line");
			continue;
		}
		if (tokens.empty())
			continue;

		const std::string keyword = parse_detail::LowerCopy(tokens[0].text);

		// A `factory` line always starts a block. Meeting one inside an open
		// block means the previous block lost its `}`; report that and recover
		// here rather than swallowing every subsequent definition.
		if (keyword == "factory") {
			if (in_block)
				reject_block("factory block starting at line " +
					std::to_string(block_line) +
					" is missing its closing `}`");
			begin_block();
			continue;
		}

		if (!in_block) {
			parse_detail::Fail(result, filename, line_number,
				"expected a `factory <id> {` block, found `" +
				std::string(tokens[0].text) + "`");
			return result;
		}

		if (keyword == "{")
			continue;

		if (keyword == "}") {
			if (!block_failed) {
				if (current.title.empty())
					reject_block("factory `" + current.id +
						"` is missing a title");
				else if (current.base_token.empty())
					reject_block("factory `" + current.id +
						"` is missing a base gametype");
			}
			if (!block_failed)
				result.factories.push_back(std::move(current));
			in_block = false;
			block_failed = false;
			current = parsed_factory_t{};
			continue;
		}

		if (keyword == "title" || keyword == "desc" || keyword == "description") {
			if (tokens.size() < 2) {
				reject_block("`" + keyword + "` requires a value");
			} else {
				const std::string_view value = tokens[1].quoted
					? tokens[1].text
					: MM_FactoryTailAfterToken(content, tokens[0].text);
				if (keyword == "title") {
					if (!MM_IsSafeFactoryTitle(value))
						reject_block("factory `" + current.id +
							"` has an unusable title (max " +
							std::to_string(MM_FACTORY_TITLE_MAX) +
							" characters, no backslash, semicolon or quote)");
					else
						current.title.assign(value);
				} else {
					if (!MM_IsSafeFactoryDesc(value))
						reject_block("factory `" + current.id +
							"` has an unusable description");
					else
						current.desc.assign(value);
				}
			}
		} else if (keyword == "base" || keyword == "basegt") {
			if (tokens.size() < 2)
				reject_block("`base` requires a gametype short name");
			else
				current.base_token = parse_detail::LowerCopy(tokens[1].text);
		} else if (keyword == "ruleset") {
			if (tokens.size() < 2)
				reject_block("`ruleset` requires a ruleset short name");
			else
				current.ruleset_token = parse_detail::LowerCopy(tokens[1].text);
		} else if (keyword == "inherit") {
			if (tokens.size() < 2)
				reject_block("`inherit` requires a factory id");
			else {
				const std::string parent = parse_detail::LowerCopy(tokens[1].text);
				if (!MM_IsSafeFactoryId(parent))
					reject_block("`inherit` names an invalid factory id `" +
						std::string(tokens[1].text) + "`");
				else if (parent == current.id)
					reject_block("factory `" + current.id +
						"` cannot inherit from itself");
				else
					current.inherit = parent;
			}
		} else if (keyword == "maps" || keyword == "mappool") {
			// Accumulates, so a long rotation can be split over several lines
			// rather than fighting the line-length bound.
			std::vector<std::string> &target = keyword == "maps"
				? current.scope.maps : current.scope.mappool;
			if (tokens.size() < 2) {
				reject_block("`" + keyword + "` requires at least one map");
			} else {
				for (size_t i = 1; i < tokens.size(); i++) {
					const std::string_view token = tokens[i].text;
					if (!muffmode::map_pool::IsSafeStructuredMapIdentifier(
						token, MM_FACTORY_MAX_QPATH)) {
						reject_block("`" + keyword + "` names an unusable map `" +
							std::string(token) + "`");
						break;
					}
					if (target.size() >= MM_FACTORY_MAX_MAPS) {
						reject_block("`" + keyword + "` lists more than " +
							std::to_string(MM_FACTORY_MAX_MAPS) + " maps");
						break;
					}
					target.emplace_back(token);
				}
			}
		} else if (keyword == "rotation") {
			if (tokens.size() < 2) {
				reject_block("`rotation` requires sequential, shuffle-on-wrap "
					"or shuffle-per-gametype");
			} else {
				const mm_factory_rotation_t rotation =
					MM_ParseFactoryRotation(tokens[1].text);
				if (rotation == mm_factory_rotation_t::Inherit)
					reject_block("`rotation` does not accept `" +
						std::string(tokens[1].text) +
						"`; use sequential, shuffle-on-wrap or "
						"shuffle-per-gametype");
				else
					current.scope.rotation = rotation;
			}
		} else if (keyword == "players") {
			const auto low = tokens.size() > 1
				? MM_ParseIntText(tokens[1].text) : std::nullopt;
			const auto high = tokens.size() > 2
				? MM_ParseIntText(tokens[2].text) : std::nullopt;
			if (tokens.size() < 3 || !low || !high) {
				reject_block("`players` requires a minimum and a maximum, "
					"for example `players 2 10`");
			} else if (*low < 1 || *high < *low ||
				static_cast<size_t>(*high) > MM_FACTORY_MAX_PLAYERS) {
				reject_block("`players " + std::to_string(*low) + " " +
					std::to_string(*high) + "` is not a usable range (1 <= "
					"minimum <= maximum <= " +
					std::to_string(MM_FACTORY_MAX_PLAYERS) + ")");
			} else {
				current.scope.min_players = *low;
				current.scope.max_players = *high;
			}
		} else if (keyword == "set") {
			if (tokens.size() < 2) {
				reject_block("`set` requires a cvar name");
			} else if (tokens.size() < 3) {
				// A value that is missing, or swallowed whole by a comment, is
				// a typo rather than a request to clear -- and clearing a
				// numeric cvar reads as 0 everywhere. `set <cvar> ""` says
				// "clear this" unambiguously; a bare `set <cvar>` does not.
				reject_block("`set " + parse_detail::LowerCopy(tokens[1].text) +
					"` requires a value (use \"\" to clear the cvar)");
			} else {
				const std::string name = parse_detail::LowerCopy(tokens[1].text);
				const std::string_view value = tokens.size() >= 3 && tokens[2].quoted
					? tokens[2].text
					: MM_FactoryTailAfterToken(content, tokens[1].text);

				if (MM_FindFactoryCvar(name) == nullptr) {
					reject_block("unknown cvar `" + name +
						"` (factories may only set allowlisted cvars; see "
						"docs/configuration-reference.md)");
				} else if (!MM_IsSafeFactoryValue(value)) {
					reject_block("value for `" + name +
						"` is unusable (max " +
						std::to_string(MM_FACTORY_VALUE_MAX) +
						" characters, no backslash, semicolon or quote)");
				} else if (current.overrides.size() >= MM_FACTORY_MAX_OVERRIDES) {
					reject_block("factory `" + current.id + "` sets more than " +
						std::to_string(MM_FACTORY_MAX_OVERRIDES) + " cvars");
				} else {
					bool replaced = false;
					for (parsed_override_t &existing : current.overrides) {
						if (existing.name == name) {
							existing.value.assign(value);
							replaced = true;
							break;
						}
					}
					if (!replaced)
						current.overrides.push_back({ name, std::string(value) });
				}
			}
		} else {
			reject_block("unknown directive `" + std::string(tokens[0].text) + "`");
		}
	}

	if (document_failed)
		return result;

	if (in_block && !block_failed) {
		parse_detail::Fail(result, filename, block_line,
			"factory block is missing its closing `}`");
		result.rejected_blocks++;
	}

	result.valid = true;
	return result;
}

} // namespace factory
} // namespace muffmode
