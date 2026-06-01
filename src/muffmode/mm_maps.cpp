// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "g_debug_log.h"
#include "muffmode/mm_maps.h"

namespace {
std::vector<std::string> MM_StrSplit(const std::string_view &str, char by)
{
	std::vector<std::string> out;
	size_t start, end = 0;

	while ((start = str.find_first_not_of(by, end)) != std::string_view::npos)
	{
		end = str.find(by, start);
		out.push_back(std::string{ str.substr(start, end - start) });
	}

	return out;
}

int s_map_list_shuffle_modified = -1;
} // namespace

void MM_ShuffleMapList()
{
	if (!*g_map_list->string)
		return;

	auto values = MM_StrSplit(g_map_list->string, ' ');

	if (values.size() <= 1)
		return;

	std::shuffle(values.begin(), values.end(), mt_rand);

	// If the current map ended up at the front, push it to the end.
	if (Q_strcasecmp(values[0].c_str(), level.mapname) == 0)
		std::swap(values[0], values[values.size() - 1]);

	gi.cvar_forceset("g_map_list", fmt::format("{}", join_strings(values, " ")).data());
	gi.Com_PrintFmt("Map list shuffled: {}\n", g_map_list->string);
}

void MM_GametypeChangeMapFirst()
{
	MuffModeLog("DEBUG", "SVCmd_GametypeChangeMapFirst_f: enter, g_map_list='%s', g_map_list_shuffle=%d",
		g_map_list->string, g_map_list_shuffle->integer);

	// This executes AFTER the gametype config has set the new g_map_list.
	// Shuffle the list if shuffle is enabled (mode 1 or 2).
	if (g_map_list_shuffle->integer >= 1)
	{
		G_ShuffleMapList();
		if (g_map_list_shuffle->integer == 2)
		{
			extern bool g_map_list_shuffled;
			g_map_list_shuffled = true;
		}
	}

	const char *first_map = nullptr;

	// Try to get first map from g_map_list (now shuffled if enabled).
	if (g_map_list->string[0])
	{
		const char *mlist = g_map_list->string;
		char *token;

		if ((token = COM_Parse(&mlist)) && *token)
		{
			first_map = token;
			MuffModeLog("GAMETYPE", "SVCmd_GametypeChangeMapFirst_f: Found map '%s' from g_map_list", first_map);
		}
	}

	// If no map found in g_map_list, fall back to current map.
	if (!first_map || !first_map[0])
	{
		first_map = level.mapname;
		MuffModeLog("GAMETYPE", "SVCmd_GametypeChangeMapFirst_f: No map in g_map_list, reloading current map '%s'", first_map);
	}

	// Store in safe storage.
	if (strlen(first_map) >= sizeof(level.nextmap))
	{
		gi.Com_PrintFmt("ERROR: Map name too long: {}\n", first_map);
		MuffModeLog("GAMETYPE", "ERROR: Map name too long: %s", first_map);
		return;
	}

	MuffModeLog("GAMETYPE", "Gametype change complete, loading map: %s", first_map);

	// Issue gamemap directly instead of ExitLevel() -- ExitLevel does too much
	// (screenshots, ClientEndServerFrames, Duel_RemoveLoser) that assumes
	// intermission context and causes crashes when called outside normal match flow.
	gi.AddCommandString(G_Fmt("gamemap \"{}\"\n", first_map).data());
}

bool MM_TryBeginIntermissionFromMapList()
{
	if (!*g_map_list->string)
		return false;

	const char *str = g_map_list->string;
	char first_map[MAX_QPATH]{ 0 };
	char *map;

	while (1)
	{
		map = COM_ParseEx(&str, " ");

		if (!*map)
			break;

		if (Q_strcasecmp(map, level.mapname) == 0)
		{
			// It's in the list, go to the next one.
			map = COM_ParseEx(&str, " ");
			if (!*map)
			{
				// End of list, go to first one.
				if (!first_map[0])
				{
					BeginIntermission(CreateTargetChangeLevel(level.mapname));
					return true;
				}

				// End of list wrap-around: shuffle if enabled.
				// g_map_list_shuffle 1 = shuffle every wrap-around
				// g_map_list_shuffle 2 = shuffle once per gametype (lazy)
				if (g_map_list_shuffle->integer == 1)
				{
					G_ShuffleMapList();
				}
				else if (g_map_list_shuffle->integer == 2)
				{
					extern bool g_map_list_shuffled;
					if (!g_map_list_shuffled)
					{
						G_ShuffleMapList();
						g_map_list_shuffled = true;
					}
				}

				// Re-read first map from (possibly shuffled) list.
				const char *reshuffled_str = g_map_list->string;
				char *reshuffled_first = COM_ParseEx(&reshuffled_str, " ");
				if (reshuffled_first && *reshuffled_first)
					BeginIntermission(CreateTargetChangeLevel(reshuffled_first));
				else
					BeginIntermission(CreateTargetChangeLevel(first_map));
				return true;
			}

			BeginIntermission(CreateTargetChangeLevel(map));
			return true;
		}

		if (!first_map[0])
			Q_strlcpy(first_map, map, sizeof(first_map));
	}

	// Current map not in g_map_list (e.g. voted from pool) - rejoin rotation at first map.
	if (first_map[0])
	{
		BeginIntermission(CreateTargetChangeLevel(first_map));
		return true;
	}

	return false;
}

void MM_HandleMapShuffleCvarChange()
{
	if (s_map_list_shuffle_modified == g_map_list_shuffle->modified_count)
		return;

	s_map_list_shuffle_modified = g_map_list_shuffle->modified_count;

	// Shuffle immediately in lazy mode when toggled/changed from console.
	if (g_map_list_shuffle->integer == 2)
	{
		extern bool g_map_list_shuffled;
		G_ShuffleMapList();
		g_map_list_shuffled = true;
	}
}

