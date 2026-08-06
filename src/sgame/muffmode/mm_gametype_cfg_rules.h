// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "muffmode/mm_parse.h"

#include <cstddef>
#include <string_view>

namespace muffmode::gametype {

inline constexpr bool MM_IsSlotCappedCapacity(
	int allocated_maxclients, int configured_maxclients, int slot_cap) noexcept
{
	const int authoritative_capacity = allocated_maxclients > 0
		? allocated_maxclients
		: configured_maxclients;
	return authoritative_capacity > 0 && authoritative_capacity <= slot_cap;
}

namespace cfg_detail {

struct cfg_token_t {
	std::string_view text;
	bool present = false;
	bool valid = true;
};

inline cfg_token_t NextCfgToken(std::string_view command, size_t &cursor) noexcept
{
	while (cursor < command.size() && MM_IsAsciiWhitespace(command[cursor]))
		cursor++;

	if (cursor >= command.size())
		return {};

	// COM_Parse treats // at a token boundary as the end of the command.
	if (command[cursor] == '/' && cursor + 1 < command.size() && command[cursor + 1] == '/') {
		cursor = command.size();
		return {};
	}

	if (command[cursor] == '"') {
		const size_t begin = ++cursor;
		while (cursor < command.size() && command[cursor] != '"')
			cursor++;

		if (cursor >= command.size())
			return { command.substr(begin), true, false };

		const std::string_view token = command.substr(begin, cursor - begin);
		cursor++;
		return { token, true, true };
	}

	const size_t begin = cursor;
	while (cursor < command.size() && !MM_IsAsciiWhitespace(command[cursor]))
		cursor++;

	return { command.substr(begin, cursor - begin), true, true };
}

inline bool IsMaxclientsSetter(std::string_view command) noexcept
{
	return MM_EqualsAsciiI(command, "set") ||
		MM_EqualsAsciiI(command, "seta") ||
		MM_EqualsAsciiI(command, "setu") ||
		MM_EqualsAsciiI(command, "sets") ||
		MM_EqualsAsciiI(command, "cvar_set") ||
		MM_EqualsAsciiI(command, "cvar_forceset");
}

inline bool IsTargetedCvarMutator(std::string_view command) noexcept
{
	return MM_EqualsAsciiI(command, "toggle") ||
		MM_EqualsAsciiI(command, "inc") ||
		MM_EqualsAsciiI(command, "add") ||
		MM_EqualsAsciiI(command, "reset") ||
		MM_EqualsAsciiI(command, "cvar_toggle") ||
		MM_EqualsAsciiI(command, "cvar_inc") ||
		MM_EqualsAsciiI(command, "cvar_add") ||
		MM_EqualsAsciiI(command, "cvar_reset");
}

inline bool IsUninspectableCommand(std::string_view command) noexcept
{
	// These commands can publish text that this one-line filter cannot inspect,
	// either immediately or on a later command-buffer tick. Keep this a narrow
	// denylist: packaged presets use direct set/seta commands, while ordinary
	// console commands, direct cvar assignments, and explicit mutations of
	// unrelated cvars remain compatible.
	return MM_EqualsAsciiI(command, "exec") ||
		MM_EqualsAsciiI(command, "execq") ||
		MM_EqualsAsciiI(command, "alias") ||
		MM_EqualsAsciiI(command, "vstr") ||
		MM_EqualsAsciiI(command, "if") ||
		MM_EqualsAsciiI(command, "ifnot") ||
		MM_EqualsAsciiI(command, "delay") ||
		MM_EqualsAsciiI(command, "defer") ||
		MM_EqualsAsciiI(command, "cvar_restart");
}

inline bool SegmentViolatesSlotCap(std::string_view segment, int slot_cap) noexcept
{
	size_t cursor = 0;
	const cfg_token_t command = NextCfgToken(segment, cursor);
	if (!command.present)
		return false;
	if (!command.valid)
		return true;

	// Lobby/setup command; never appropriate during an in-place gametype change.
	if (MM_EqualsAsciiI(command.text, "kexmultiplayer"))
		return true;
	if (IsUninspectableCommand(command.text))
		return true;

	const bool direct_assignment = MM_EqualsAsciiI(command.text, "maxclients");
	const bool setter = IsMaxclientsSetter(command.text);
	const bool mutator = IsTargetedCvarMutator(command.text);
	if (!direct_assignment && !setter && !mutator)
		return false;

	if (!direct_assignment) {
		const cfg_token_t target = NextCfgToken(segment, cursor);
		if (!target.present)
			return false;
		if (!target.valid)
			return true;
		if (!MM_EqualsAsciiI(target.text, "maxclients"))
			return false;
	}

	// Increment/toggle/reset commands do not have one statically knowable final
	// value, so any such mutation of maxclients is unsafe in a capped session.
	if (mutator)
		return true;

	const cfg_token_t value = NextCfgToken(segment, cursor);
	if (!value.present)
		return false; // A bare cvar command only prints its current value.

	const auto parsed = value.valid ? MM_ParseIntText(value.text) : std::nullopt;
	return !parsed || *parsed < 1 || *parsed > slot_cap;
}

} // namespace cfg_detail

// Match the engine command buffer: newlines always terminate a command and an
// unquoted semicolon separates commands. Every command segment is inspected so
// a safe prefix cannot hide a later maxclients mutation or dynamic expansion.
// Semicolons inside a quoted argument remain data and are not split.
inline bool MM_GtCfgLineViolatesSlotCap(std::string_view line, int slot_cap) noexcept
{
	size_t segment_begin = 0;
	bool in_quotes = false;

	for (size_t cursor = 0; cursor < line.size(); cursor++) {
		const char c = line[cursor];
		if (c == '\r' || c == '\n') {
			if (cfg_detail::SegmentViolatesSlotCap(
				line.substr(segment_begin, cursor - segment_begin), slot_cap))
				return true;
			segment_begin = cursor + 1;
			in_quotes = false;
			continue;
		}

		if (c == '"') {
			in_quotes = !in_quotes;
			continue;
		}

		if (c != ';' || in_quotes)
			continue;

		if (cfg_detail::SegmentViolatesSlotCap(
			line.substr(segment_begin, cursor - segment_begin), slot_cap))
			return true;
		segment_begin = cursor + 1;
	}

	return cfg_detail::SegmentViolatesSlotCap(line.substr(segment_begin), slot_cap);
}

} // namespace muffmode::gametype
