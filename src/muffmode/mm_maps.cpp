// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "g_debug_log.h"
#include "muffmode/mm_maps.h"

#include <algorithm>

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
		MM_ShuffleMapList();
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
					MM_ShuffleMapList();
				}
				else if (g_map_list_shuffle->integer == 2)
				{
					extern bool g_map_list_shuffled;
					if (!g_map_list_shuffled)
					{
						MM_ShuffleMapList();
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
		MM_ShuffleMapList();
		g_map_list_shuffled = true;
	}
}

namespace {

void MM_MQ_Clear()
{
	if (!deathmatch)
		return;

	game.mapqueue.clear();
}

bool MM_MQ_Update()
{
	if (!deathmatch)
		return false;

	if (!g_allow_mymap->integer)
		return false;

	if (!g_map_list->string[0] && game.mapqueue.size()) {
		MM_MQ_Clear();
		gi.Broadcast_Print(PRINT_HIGH, "Map queue has been cleared.\n");
		return false;
	}

	auto it = std::remove_if(game.mapqueue.begin(), game.mapqueue.end(),
		[](const std::string &s) { return s.empty(); });
	game.mapqueue.erase(it, game.mapqueue.end());

	return true;
}

void MM_MQ_PrintList(gentity_t *ent)
{
	std::string text;

	for (size_t i = 0; i < game.mapqueue.size(); i++) {
		if (!game.mapqueue[i].empty())
			text += game.mapqueue[i] + " ";
	}

	gi.LocClient_Print(ent, PRINT_HIGH, G_Fmt("{}\n", text).data());
}

constexpr size_t MAX_MAP_LIST_DISPLAY = 512;

void MM_PrintTruncatedMapList(gentity_t *ent)
{
	std::string map_list_display = g_map_list->string;
	if (map_list_display.length() > MAX_MAP_LIST_DISPLAY)
		map_list_display = map_list_display.substr(0, MAX_MAP_LIST_DISPLAY) + "...";
	gi.LocClient_Print(ent, PRINT_HIGH, "{}\n", map_list_display.c_str());
}

} // namespace

int MM_MQ_Count()
{
	if (!deathmatch)
		return 0;

	if (!g_allow_mymap->integer)
		return 0;

	if (!MM_MQ_Update())
		return 0;

	return (int)game.mapqueue.size();
}

bool MM_MQ_Add(gentity_t *ent, const char *mapname)
{
	if (!deathmatch)
		return false;

	if (!g_allow_mymap->integer)
		return false;

	if (!mapname[0]) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid map name.\n");
		return false;
	}

	if (!g_map_pool->string[0] && !g_map_list->string[0])
		return false;

	char *token;
	bool found = false;

	if (g_map_pool->string[0]) {
		const char *pool = g_map_pool->string;

		while ((token = COM_Parse(&pool)) && *token) {
			if (!Q_strcasecmp(token, mapname)) {
				found = true;
				break;
			}
		}
	}

	if (!found && g_map_list->string[0]) {
		const char *mlist = g_map_list->string;

		while ((token = COM_Parse(&mlist)) && *token) {
			if (!Q_strcasecmp(token, mapname)) {
				found = true;
				break;
			}
		}
	}

	if (!found) {
		gi.Client_Print(ent, PRINT_HIGH, "Selected map is either invalid or not in pool/list.\n");
		return false;
	}

	if (!MM_MQ_Update())
		return false;

	if (std::find(game.mapqueue.begin(), game.mapqueue.end(), mapname) != game.mapqueue.end()) {
		gi.Client_Print(ent, PRINT_HIGH, "Selected map is already in queue.\n");
		return false;
	}

	game.mapqueue.push_back(mapname);
	return true;
}

const char *MM_MQ_Go_Next()
{
	if (!deathmatch)
		return nullptr;

	if (!MM_MQ_Update())
		return nullptr;

	for (size_t i = 0; i < game.mapqueue.size(); i++) {
		if (game.mapqueue[i].empty())
			continue;
		const char *s = G_Fmt("{}", game.mapqueue[i]).data();
		game.mapqueue.erase(game.mapqueue.begin() + i);
		return s;
	}

	return nullptr;
}

void MM_CmdMapList(gentity_t *ent)
{
	if (g_map_list->string[0]) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Current map list:\n");
		MM_PrintTruncatedMapList(ent);
		if (MM_MQ_Count()) {
			gi.LocClient_Print(ent, PRINT_HIGH, "\nCurrent MyMap Queue:\n");
			MM_MQ_PrintList(ent);
		}
	} else {
		gi.LocClient_Print(ent, PRINT_HIGH, "No Map List set.\n");
	}
}

void MM_CmdMyMap(gentity_t *ent)
{
	if (!g_allow_mymap->integer) {
		gi.LocClient_Print(ent, PRINT_HIGH, "MyMap is disabled.\n");
		return;
	}

	if (!g_map_list->string[0]) {
		gi.LocClient_Print(ent, PRINT_HIGH, "No maps are queued as no map list is present.\n");
		return;
	}

	if (gi.argc() < 2) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Add a map to the MyMap Queue.\nRecognized maps are:\n");
		MM_PrintTruncatedMapList(ent);

		if (MM_MQ_Count()) {
			gi.LocClient_Print(ent, PRINT_HIGH, "MyMap Queue => ");
			MM_MQ_PrintList(ent);
		}
		return;
	}

	if (!strcmp(gi.argv(1), level.mapname)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Cannot add current map to MyMap Queue.\n");
		return;
	}

	if (!MM_MQ_Add(ent, gi.argv(1)))
		return;

	std::string text;

	for (size_t i = 0; i < game.mapqueue.size(); i++) {
		if (!game.mapqueue[i].empty()) {
			text += game.mapqueue[i];
			text += " ";
		}
	}

	game.item_inhibit_pu = 0;
	game.item_inhibit_pa = 0;
	game.item_inhibit_ht = 0;
	game.item_inhibit_ar = 0;
	game.item_inhibit_am = 0;
	game.item_inhibit_wp = 0;

	for (int i = 2; i < gi.argc(); i++) {
		const char *s = gi.argv(i);
		if (!s || !s[0])
			continue;
		int num = 0;
		if (s[0] == '+') { num = 1; s++; }
		else if (s[0] == '-') { num = -1; s++; }
		else continue;
		if (!Q_strcasecmp(s, "pu"))      game.item_inhibit_pu = num;
		else if (!Q_strcasecmp(s, "pa")) game.item_inhibit_pa = num;
		else if (!Q_strcasecmp(s, "ht")) game.item_inhibit_ht = num;
		else if (!Q_strcasecmp(s, "ar")) game.item_inhibit_ar = num;
		else if (!Q_strcasecmp(s, "am")) game.item_inhibit_am = num;
		else if (!Q_strcasecmp(s, "wp")) game.item_inhibit_wp = num;
	}

	if (text.size())
		gi.LocBroadcast_Print(PRINT_HIGH, "MyMap Queue => {}\n", text.data());
}
