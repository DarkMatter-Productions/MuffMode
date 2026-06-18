// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "g_debug_log.h"
#include "muffmode/mm_command_contracts.h"
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

const char *MM_CvarString(cvar_t *cvar)
{
	return (cvar && cvar->string) ? cvar->string : "";
}

int MM_CvarInteger(cvar_t *cvar)
{
	return cvar ? cvar->integer : 0;
}

bool MM_IsSafeMapTokenImpl(const char *mapname)
{
	if (!mapname || !*mapname)
		return false;

	if (!strcmp(mapname, ".") || !strcmp(mapname, ".."))
		return false;

	// Subdirectory maps (e.g. "q64/outpost") are legitimate, so a path separator is allowed
	// mid-token - but never as a leading character, which would denote an absolute path or a
	// UNC share ("\\host\..", "/etc/..").
	if (mapname[0] == '/' || mapname[0] == '\\')
		return false;

	size_t length = 0;
	for (const unsigned char *p = reinterpret_cast<const unsigned char *>(mapname); *p; p++) {
		length++;
		if (length >= MAX_QPATH)
			return false;

		// Block control/space chars, quotes and ';' (console-command injection) and ':'
		// (drive letters / URL schemes). '/' and '\\' are permitted as subdirectory
		// separators; only ".." path traversal is rejected (checked below).
		if (*p <= ' ' || *p == '"' || *p == '\'' || *p == ';' || *p == ':')
			return false;

		if (*p == '.' && p[1] == '.')
			return false;
	}

	return true;
}

bool MM_ForEachConfiguredMap(bool (*visitor)(const char *map, const void *data), const void *data)
{
	if (!visitor)
		return false;

	const char *sources[] = { MM_CvarString(g_map_pool), MM_CvarString(g_map_list) };
	for (const char *source : sources) {
		if (!source[0])
			continue;

		const char *cursor = source;
		while (char *map = COM_Parse(&cursor)) {
			if (!*map)
				break;
			if (MM_IsSafeMapTokenImpl(map) && visitor(map, data))
				return true;
		}
	}

	return false;
}

bool MM_MapSourceContainsVisitor(const char *map, const void *data)
{
	const char *target = static_cast<const char *>(data);
	return target && !Q_strcasecmp(map, target);
}

bool MM_ConfiguredMapSourcesContain(const char *mapname)
{
	if (!MM_IsSafeMapTokenImpl(mapname))
		return false;

	return MM_ForEachConfiguredMap(MM_MapSourceContainsVisitor, mapname);
}

bool MM_HasConfiguredMapSource()
{
	return MM_CvarString(g_map_list)[0] || MM_CvarString(g_map_pool)[0];
}

char *MM_ParseNextSafeMapToken(const char **text)
{
	while (char *map = COM_ParseEx(text, " ")) {
		if (!*map)
			return nullptr;

		if (MM_IsSafeMapTokenImpl(map))
			return map;
	}

	return nullptr;
}
} // namespace

bool MM_IsSafeMapToken(const char *mapname)
{
	return MM_IsSafeMapTokenImpl(mapname);
}

void MM_ShuffleMapList()
{
	const char *map_list = MM_CvarString(g_map_list);
	if (!*map_list)
		return;

	auto values = MM_StrSplit(map_list, ' ');
	const size_t original_count = values.size();
	values.erase(std::remove_if(values.begin(), values.end(),
		[](const std::string &map) { return !MM_IsSafeMapToken(map.c_str()); }), values.end());

	if (values.empty()) {
		gi.cvar_forceset("g_map_list", "");
		gi.Com_PrintFmt("Map list shuffle skipped: no valid maps remain.\n");
		return;
	}

	if (values.size() <= 1) {
		if (values.size() != original_count)
			gi.cvar_forceset("g_map_list", join_strings(values, " ").data());
		return;
	}

	std::shuffle(values.begin(), values.end(), mt_rand);

	// If the current map ended up at the front, push it to the end.
	if (Q_strcasecmp(values[0].c_str(), level.mapname) == 0)
		std::swap(values[0], values[values.size() - 1]);

	gi.cvar_forceset("g_map_list", fmt::format("{}", join_strings(values, " ")).data());
	gi.Com_PrintFmt("Map list shuffled: {}\n", MM_CvarString(g_map_list));
}

void MM_GametypeChangeMapFirst()
{
	MuffModeLog("DEBUG", "SVCmd_GametypeChangeMapFirst_f: enter, g_map_list='%s', g_map_list_shuffle=%d",
		MM_CvarString(g_map_list), MM_CvarInteger(g_map_list_shuffle));

	// This executes AFTER the gametype config has set the new g_map_list.
	// Shuffle the list if shuffle is enabled (mode 1 or 2).
	if (MM_CvarInteger(g_map_list_shuffle) >= 1)
	{
		MM_ShuffleMapList();
		if (MM_CvarInteger(g_map_list_shuffle) == 2)
		{
			extern bool g_map_list_shuffled;
			g_map_list_shuffled = true;
		}
	}

	const char *first_map = nullptr;

	// Try to get first map from g_map_list (now shuffled if enabled).
	if (MM_CvarString(g_map_list)[0])
	{
		const char *mlist = MM_CvarString(g_map_list);

		if (char *token = MM_ParseNextSafeMapToken(&mlist))
		{
			first_map = token;
			MuffModeLog("GAMETYPE", "SVCmd_GametypeChangeMapFirst_f: Found map '%s' from g_map_list", first_map);
		}
	}

	// If no map found in g_map_list, fall back to current map.
	if (!first_map || !first_map[0])
	{
		if (!MM_IsSafeMapToken(level.mapname)) {
			gi.Com_PrintFmt("ERROR: Current map name is unsafe: {}\n", level.mapname);
			MuffModeLog("GAMETYPE", "ERROR: Current map name is unsafe: %s", level.mapname);
			return;
		}

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
	if (!MM_CvarString(g_map_list)[0])
		return false;

	const char *str = MM_CvarString(g_map_list);
	char first_map[MAX_QPATH]{ 0 };
	char *map;

	while (1)
	{
		map = MM_ParseNextSafeMapToken(&str);

		if (!map || !*map)
			break;

		if (Q_strcasecmp(map, level.mapname) == 0)
		{
			// It's in the list, go to the next one.
			map = MM_ParseNextSafeMapToken(&str);
			if (!map || !*map)
			{
				// End of list, go to first one.
				if (!first_map[0])
				{
					if (!MM_IsSafeMapToken(level.mapname))
						return false;

					BeginIntermission(CreateTargetChangeLevel(level.mapname));
					return true;
				}

				// End of list wrap-around: shuffle if enabled.
				// g_map_list_shuffle 1 = shuffle every wrap-around
				// g_map_list_shuffle 2 = shuffle once per gametype (lazy)
				if (MM_CvarInteger(g_map_list_shuffle) == 1)
				{
					MM_ShuffleMapList();
				}
				else if (MM_CvarInteger(g_map_list_shuffle) == 2)
				{
					extern bool g_map_list_shuffled;
					if (!g_map_list_shuffled)
					{
						MM_ShuffleMapList();
						g_map_list_shuffled = true;
					}
				}

				// Re-read first map from (possibly shuffled) list.
				const char *reshuffled_str = MM_CvarString(g_map_list);
				char *reshuffled_first = MM_ParseNextSafeMapToken(&reshuffled_str);
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
	if (!g_map_list_shuffle)
		return;

	if (s_map_list_shuffle_modified == g_map_list_shuffle->modified_count)
		return;

	s_map_list_shuffle_modified = g_map_list_shuffle->modified_count;

	// Shuffle immediately in lazy mode when toggled/changed from console.
	if (MM_CvarInteger(g_map_list_shuffle) == 2)
	{
		extern bool g_map_list_shuffled;
		MM_ShuffleMapList();
		g_map_list_shuffled = true;
	}
}

namespace {

constexpr size_t MM_MAX_MAPQUEUE_ENTRIES = MAX_CLIENTS_KEX;

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

	if (!MM_CvarInteger(g_allow_mymap))
		return false;

	if (!MM_HasConfiguredMapSource() && game.mapqueue.size()) {
		MM_MQ_Clear();
		gi.Broadcast_Print(PRINT_HIGH, "Map queue has been cleared.\n");
		return false;
	}

	auto it = std::remove_if(game.mapqueue.begin(), game.mapqueue.end(),
		[](const std::string &s) { return !MM_IsSafeMapToken(s.c_str()); });
	game.mapqueue.erase(it, game.mapqueue.end());

	std::vector<std::string> clean_queue;
	clean_queue.reserve(std::min(game.mapqueue.size(), MM_MAX_MAPQUEUE_ENTRIES));

	for (const auto &queued_map : game.mapqueue) {
		if (clean_queue.size() >= MM_MAX_MAPQUEUE_ENTRIES)
			break;
		if (std::any_of(clean_queue.begin(), clean_queue.end(),
			[&queued_map](const std::string &existing) { return !Q_strcasecmp(existing.c_str(), queued_map.c_str()); }))
			continue;
		clean_queue.push_back(queued_map);
	}

	if (clean_queue.size() != game.mapqueue.size())
		game.mapqueue.swap(clean_queue);

	return true;
}

std::string MM_MQ_FormatList()
{
	std::string text;

	for (size_t i = 0; i < game.mapqueue.size(); i++) {
		if (game.mapqueue[i].empty())
			continue;
		if (!text.empty())
			text += " ";
		text += game.mapqueue[i];
	}

	return text;
}

void MM_MQ_PrintList(gentity_t *ent)
{
	std::string text = MM_MQ_FormatList();
	gi.LocClient_Print(ent, PRINT_HIGH, "{}\n", text.empty() ? "(empty)" : text.c_str());
}

constexpr size_t MAX_MAP_LIST_DISPLAY = 512;

void MM_PrintTruncatedMapList(gentity_t *ent)
{
	std::string map_list_display = MM_CvarString(g_map_list);
	if (map_list_display.length() > MAX_MAP_LIST_DISPLAY)
		map_list_display = map_list_display.substr(0, MAX_MAP_LIST_DISPLAY) + "...";
	gi.LocClient_Print(ent, PRINT_HIGH, "{}\n", map_list_display.c_str());
}

void MM_PrintTruncatedMapSource(gentity_t *ent)
{
	const char *maps = MM_CvarString(g_map_list)[0] ? MM_CvarString(g_map_list) : MM_CvarString(g_map_pool);
	std::string display = maps ? maps : "";
	if (display.length() > MAX_MAP_LIST_DISPLAY)
		display = display.substr(0, MAX_MAP_LIST_DISPLAY) + "...";
	gi.LocClient_Print(ent, PRINT_HIGH, "{}\n", display.c_str());
}

void MM_PrintTruncatedMapPool(gentity_t *ent)
{
	std::string display = MM_CvarString(g_map_pool);
	if (display.length() > MAX_MAP_LIST_DISPLAY)
		display = display.substr(0, MAX_MAP_LIST_DISPLAY) + "...";
	gi.LocClient_Print(ent, PRINT_HIGH, "{}\n", display.c_str());
}

bool MM_IsValidMyMapModifier(const char *modifier)
{
	if (!modifier || !modifier[0] || !modifier[1])
		return false;

	if (modifier[0] != '+' && modifier[0] != '-')
		return false;

	const char *name = modifier + 1;
	return !Q_strcasecmp(name, "pu") ||
		!Q_strcasecmp(name, "pa") ||
		!Q_strcasecmp(name, "ht") ||
		!Q_strcasecmp(name, "ar") ||
		!Q_strcasecmp(name, "am") ||
		!Q_strcasecmp(name, "wp");
}

bool MM_MapQueueContains(const char *mapname)
{
	for (const auto &queued_map : game.mapqueue) {
		if (!Q_strcasecmp(queued_map.c_str(), mapname))
			return true;
	}

	return false;
}

std::string s_next_mapqueue_return;

} // namespace

int MM_MQ_Count()
{
	if (!deathmatch)
		return 0;

	if (!MM_CvarInteger(g_allow_mymap))
		return 0;

	if (!MM_MQ_Update())
		return 0;

	return (int)game.mapqueue.size();
}

bool MM_MQ_Add(gentity_t *ent, const char *mapname)
{
	if (!ent || !ent->client)
		return false;

	if (!deathmatch)
		return false;

	if (!MM_CvarInteger(g_allow_mymap))
		return false;

	if (!MM_IsSafeMapToken(mapname)) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid map name.\n");
		return false;
	}

	if (!MM_HasConfiguredMapSource())
		return false;

	if (!MM_ConfiguredMapSourcesContain(mapname)) {
		gi.Client_Print(ent, PRINT_HIGH, "Selected map is either invalid or not in pool/list.\n");
		return false;
	}

	if (!MM_MQ_Update())
		return false;

	if (MM_MapQueueContains(mapname)) {
		gi.Client_Print(ent, PRINT_HIGH, "Selected map is already in queue.\n");
		return false;
	}

	if (game.mapqueue.size() >= MM_MAX_MAPQUEUE_ENTRIES) {
		gi.Client_Print(ent, PRINT_HIGH, "Map queue is full.\n");
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

	for (size_t i = 0; i < game.mapqueue.size();) {
		if (!MM_IsSafeMapToken(game.mapqueue[i].c_str())) {
			game.mapqueue.erase(game.mapqueue.begin() + i);
			continue;
		}

		s_next_mapqueue_return = game.mapqueue[i];
		game.mapqueue.erase(game.mapqueue.begin() + i);
		return s_next_mapqueue_return.c_str();
	}

	return nullptr;
}

void MM_CmdMapList(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	if (!MM_IsExactArgcValid(gi.argc(), 1)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {}\n", gi.argv(0));
		return;
	}

	if (!MM_HasConfiguredMapSource()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "No map list or pool set.\n");
		return;
	}

	const bool has_list = MM_CvarString(g_map_list)[0];
	const bool has_pool = MM_CvarString(g_map_pool)[0];

	// Show both the rotation list and the votable pool so players see every map
	// available on the server, not just whichever source happens to be set.
	if (has_list) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Current map list (rotation):\n");
		MM_PrintTruncatedMapList(ent);
	}

	if (has_pool) {
		gi.LocClient_Print(ent, PRINT_HIGH, has_list ? "\nMap pool (votable):\n" : "Current map pool:\n");
		MM_PrintTruncatedMapPool(ent);
	}

	if (MM_MQ_Count()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "\nCurrent MyMap Queue:\n");
		MM_MQ_PrintList(ent);
	}
}

void MM_CmdMyMap(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	if (!MM_CvarInteger(g_allow_mymap)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "MyMap is disabled.\n");
		return;
	}

	if (!MM_HasConfiguredMapSource()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "No maps are queued as no map list or pool is present.\n");
		return;
	}

	if (gi.argc() < 2) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Add a map to the MyMap Queue.\nRecognized maps are:\n");
		MM_PrintTruncatedMapSource(ent);

		if (MM_MQ_Count()) {
			gi.LocClient_Print(ent, PRINT_HIGH, "MyMap Queue => ");
			MM_MQ_PrintList(ent);
		}
		return;
	}

	for (int i = 2; i < gi.argc(); i++) {
		if (!MM_IsValidMyMapModifier(gi.argv(i))) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Invalid MyMap modifier: {}\n", gi.argv(i));
			return;
		}
	}

	if (!Q_strcasecmp(gi.argv(1), level.mapname)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Cannot add current map to MyMap Queue.\n");
		return;
	}

	if (!MM_MQ_Add(ent, gi.argv(1)))
		return;

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

	std::string text = MM_MQ_FormatList();
	if (text.size())
		gi.LocBroadcast_Print(PRINT_HIGH, "MyMap Queue => {}\n", text.data());
}
