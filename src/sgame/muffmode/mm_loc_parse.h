// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "mm_parse.h"
#include "mm_util.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

inline constexpr size_t MM_MAX_LOC_LABEL_CHARS = 64;

inline std::string MM_SanitizeLocLabel(std::string_view label)
{
	std::string out;
	out.reserve(std::min(label.size(), MM_MAX_LOC_LABEL_CHARS));

	bool last_was_space = false;
	for (const unsigned char ch : label) {
		if (out.size() >= MM_MAX_LOC_LABEL_CHARS)
			break;

		if (ch < ' ' || ch == 0x7F || MM_IsAsciiWhitespace(static_cast<char>(ch))) {
			if (!out.empty() && !last_was_space) {
				out += ' ';
				last_was_space = true;
			}
			continue;
		}

		out += static_cast<char>(ch);
		last_was_space = false;
	}

	while (!out.empty() && out.back() == ' ')
		out.pop_back();

	return out;
}

// Parse one line of a Quake .loc file of the form:
//
//     <x> <y> <z> <label...>
//
// This is the de-facto community format (R1Q2/q2pro and the loc proxies all
// emit three coordinates followed by the place name). On success fills out_xyz
// with the world position and out_label with the whitespace-trimmed place name.
inline bool MM_ParseLocLine(const char *line, float out_xyz[3], std::string &out_label)
{
	if (!line || !out_xyz)
		return false;

	const char *p = line;

	float vals[3];
	for (int i = 0; i < 3; i++) {
		while (MM_IsAsciiWhitespace(*p))
			p++;
		if (!*p)
			return false;

		const char *token_begin = p;
		while (*p && !MM_IsAsciiWhitespace(*p))
			p++;

		const auto parsed = MM_ParseFloatArg(std::string_view(token_begin, static_cast<size_t>(p - token_begin)));
		if (!parsed)
			return false;

		vals[i] = *parsed;
	}

	while (MM_IsAsciiWhitespace(*p))
		p++;

	const char *label_begin = p;
	const char *label_end = p + std::strlen(p);
	while (label_end > label_begin && MM_IsAsciiWhitespace(label_end[-1]))
		label_end--;

	if (label_begin == label_end)
		return false;

	std::string label = MM_SanitizeLocLabel(std::string_view(label_begin, static_cast<size_t>(label_end - label_begin)));
	if (label.empty())
		return false;

	out_xyz[0] = vals[0];
	out_xyz[1] = vals[1];
	out_xyz[2] = vals[2];
	out_label = std::move(label);
	return true;
}

// Keep this token list in sync with MM_ExpandStatusMacros in mm_loc.cpp.
inline constexpr const char *MM_LOC_MACRO_TOKENS[] = {
	"%l", "%L", // location
	"%h",       // health
	"%a",       // armor
	"%w",       // weapon + ammo
	"%m",       // current ammo name
	"%n",       // nearby teammates
	"%N",       // nearby players
};

inline bool MM_LocBodyHasMacro(std::string_view body)
{
	for (const char *tok : MM_LOC_MACRO_TOKENS) {
		if (body.find(tok) != std::string::npos)
			return true;
	}
	return false;
}

// Build a loc callout body from the player's command text. "%l"/"%L" expand to
// the bracketed location; live status tokens stay intact for game-side expansion.
inline std::string MM_BuildLocBody(const char *args, const char *label)
{
	constexpr size_t MM_MAX_LOC_MSG = 150;
	constexpr const char *MM_LOC_LOCATION_TOKENS[] = { "%l", "%L" };
	const std::string safe_label = MM_SanitizeLocLabel(label ? std::string_view(label) : std::string_view());
	const std::string repl = std::string("[") + safe_label + "]";

	std::string body(MM_TrimAsciiWhitespace((args && *args) ? std::string_view(args) : std::string_view()));

	if (body.size() >= 2 && body.front() == '"' && body.back() == '"') {
		const std::string_view inner(body.data() + 1, body.size() - 2);
		body = std::string(MM_TrimAsciiWhitespace(inner));
	}

	if (!MM_LocBodyHasMacro(body))
		return std::string();

	for (const char *tok : MM_LOC_LOCATION_TOKENS) {
		size_t pos = 0;
		while ((pos = body.find(tok, pos)) != std::string::npos) {
			body.replace(pos, 2, repl);
			pos += repl.size();
		}
	}

	return muffmode::TruncateWithEllipsis(body, MM_MAX_LOC_MSG);
}
