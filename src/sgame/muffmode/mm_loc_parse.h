// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "mm_parse.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

// Parse one line of a Quake .loc file of the form:
//
//     <x> <y> <z> <label...>
//
// This is the de-facto community format (R1Q2/q2pro and the loc proxies all
// emit three coordinates followed by the place name). On success fills out_xyz
// with the world position and out_label with the whitespace-trimmed place name.
inline bool MM_ParseLocLine(const char *line, float out_xyz[3], std::string &out_label)
{
	if (!line)
		return false;

	const char *p = line;

	float vals[3];
	for (int i = 0; i < 3; i++) {
		while (MM_IsAsciiWhitespace(*p))
			p++;
		if (!*p)
			return false;

		char *end = nullptr;
		const float v = std::strtof(p, &end);
		if (end == p || !std::isfinite(v))
			return false;
		if (*end && !MM_IsAsciiWhitespace(*end))
			return false;

		vals[i] = v;
		p = end;
	}

	while (MM_IsAsciiWhitespace(*p))
		p++;

	const char *label_begin = p;
	const char *label_end = p + std::strlen(p);
	while (label_end > label_begin && MM_IsAsciiWhitespace(label_end[-1]))
		label_end--;

	if (label_begin == label_end)
		return false;

	out_xyz[0] = vals[0];
	out_xyz[1] = vals[1];
	out_xyz[2] = vals[2];
	out_label.assign(label_begin, static_cast<size_t>(label_end - label_begin));
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

inline bool MM_LocBodyHasMacro(const std::string &body)
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
	const std::string repl = std::string("[") + (label ? label : "") + "]";

	std::string body = (args && *args) ? args : "";

	if (body.size() >= 2 && body.front() == '"' && body.back() == '"')
		body = body.substr(1, body.size() - 2);

	if (!MM_LocBodyHasMacro(body))
		return std::string();

	for (const char *tok : MM_LOC_LOCATION_TOKENS) {
		size_t pos = 0;
		while ((pos = body.find(tok, pos)) != std::string::npos) {
			body.replace(pos, 2, repl);
			pos += repl.size();
		}
	}

	if (body.size() > MM_MAX_LOC_MSG)
		body.resize(MM_MAX_LOC_MSG);

	return body;
}
