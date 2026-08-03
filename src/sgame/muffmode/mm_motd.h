// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstddef>
#include <string_view>

struct gentity_t;

namespace muffmode::motd {

inline bool IsSafeFilenameText(std::string_view filename, size_t max_qpath) noexcept
{
	if (filename.empty() || filename.size() >= max_qpath)
		return false;

	if (filename == "." || filename == ".." || filename.back() == '.')
		return false;

	for (const unsigned char c : filename) {
		if (c <= ' ' || c == 0x7F || c == '/' || c == '\\' || c == ':' || c == '"' || c == ';' ||
			c == '*' || c == '?' || c == '<' || c == '>' || c == '|') {
			return false;
		}
	}

	return true;
}

} // namespace muffmode::motd

// [MuffMode] Message-of-the-day load and client commands.
void MM_LoadMOTD();
void MM_ShowAutomaticMOTD(gentity_t *ent);
void MM_CmdLoadMotd(gentity_t *ent);
void MM_CmdMotd(gentity_t *ent);
