// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

struct gentity_t;

namespace muffmode::maps {

inline bool IsSafeMapTokenText(std::string_view mapname, size_t max_qpath) noexcept
{
	if (mapname.empty() || mapname.size() >= max_qpath)
		return false;

	auto is_separator = [](char c) {
		return c == '/' || c == '\\';
	};

	auto is_safe_segment = [](std::string_view segment) {
		return !segment.empty() && segment != "." && segment != "..";
	};

	size_t segment_start = 0;
	for (size_t i = 0; i < mapname.size(); i++) {
		const unsigned char c = static_cast<unsigned char>(mapname[i]);

		if (c <= ' ' || c == 0x7F || c == '"' || c == '\'' || c == ';' || c == ':' ||
			c == '*' || c == '?' || c == '<' || c == '>' || c == '|') {
			return false;
		}

		if (mapname[i] == '.' && i + 1 < mapname.size() && mapname[i + 1] == '.')
			return false;

		if (!is_separator(mapname[i]))
			continue;

		if (!is_safe_segment(mapname.substr(segment_start, i - segment_start)))
			return false;
		segment_start = i + 1;
	}

	return is_safe_segment(mapname.substr(segment_start));
}

// Typed helpers for internal MuffMode systems that need the sanitized map pool/list.
std::vector<std::string> CollectConfiguredMaps();
bool ContainsConfiguredMap(const char *mapname);

} // namespace muffmode::maps

// [MuffMode] Map-list module entry points.
bool MM_IsSafeMapToken(const char *mapname);
void MM_ShuffleMapList();
void MM_GametypeChangeMapFirst();
bool MM_TryBeginIntermissionFromMapList();
void MM_HandleMapShuffleCvarChange();

// [MuffMode] MyMap queue (game.mapqueue state remains in game_locals_t).
int MM_MQ_Count();
bool MM_MQ_Add(gentity_t *ent, const char *mapname);
const char *MM_MQ_Go_Next();
void MM_CmdMapList(gentity_t *ent);
void MM_CmdMyMap(gentity_t *ent);
