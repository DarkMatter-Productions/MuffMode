// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "g_debug_log.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_items_rules.h"
#include "muffmode/mm_maps.h"
#include "muffmode/mm_util.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace muffmode::maps {
int s_map_list_shuffle_modified = -1;

bool IsSafeMapToken(std::string_view mapname)
{
	if (mapname.empty())
		return false;

	if (mapname == "." || mapname == "..")
		return false;

	if (mapname.size() >= MAX_QPATH)
		return false;

	for (const unsigned char c : mapname) {
		if (c <= ' ' || c == '"' || c == '\'' || c == ';' || c == '/' || c == '\\')
			return false;
	}

	return true;
}

bool IsSafeMapToken(const char *mapname)
{
	return mapname && IsSafeMapToken(std::string_view(mapname));
}

template <typename Visitor>
bool ForEachConfiguredMap(Visitor &&visitor)
{
	const std::array<std::string_view, 2> sources = { CvarStringView(g_map_pool), CvarStringView(g_map_list) };
	for (std::string_view source : sources) {
		if (source.empty())
			continue;

		const char *cursor = source.data();
		while (char *map = COM_Parse(&cursor)) {
			if (!*map)
				break;
			if (IsSafeMapToken(map) && visitor(map))
				return true;
		}
	}

	return false;
}

bool ContainsConfiguredMap(const char *mapname)
{
	if (!IsSafeMapToken(mapname))
		return false;

	return ForEachConfiguredMap([mapname](const char *map) {
		return CStringEqualsI(map, mapname);
	});
}

std::vector<std::string> CollectConfiguredMaps()
{
	std::vector<std::string> maps;

	ForEachConfiguredMap([&maps](const char *map) {
		const bool already_listed = std::any_of(maps.begin(), maps.end(), [map](const std::string &existing) {
			return CStringEqualsI(existing.c_str(), map);
		});

		if (!already_listed)
			maps.emplace_back(map);

		return false;
	});

	return maps;
}

bool HasConfiguredMapSource()
{
	return CvarString(g_map_list)[0] || CvarString(g_map_pool)[0];
}

const char *ParseNextSafeMapToken(const char **text)
{
	while (char *map = COM_ParseEx(text, " ")) {
		if (!*map)
			return nullptr;

		if (IsSafeMapToken(map))
			return map;
	}

	return nullptr;
}
} // namespace muffmode::maps

bool MM_IsSafeMapToken(const char *mapname)
{
	return muffmode::maps::IsSafeMapToken(mapname);
}

void MM_ShuffleMapList()
{
	const char *map_list = muffmode::CvarString(g_map_list);
	if (!*map_list)
		return;

	auto values = muffmode::SplitTokens(map_list, ' ');
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
		{
			const std::string joined = join_strings(values, " ");
			gi.cvar_forceset("g_map_list", joined.c_str());
		}
		return;
	}

	std::shuffle(values.begin(), values.end(), mt_rand);

	// If the current map ended up at the front, push it to the end.
	if (muffmode::CStringEqualsI(values[0].c_str(), level.mapname))
		std::swap(values[0], values[values.size() - 1]);

	const std::string joined = join_strings(values, " ");
	gi.cvar_forceset("g_map_list", joined.c_str());
	gi.Com_PrintFmt("Map list shuffled: {}\n", muffmode::CvarString(g_map_list));
}

void MM_GametypeChangeMapFirst()
{
	MuffModeLog("DEBUG", "SVCmd_GametypeChangeMapFirst_f: enter, g_map_list='%s', g_map_list_shuffle=%d",
		muffmode::CvarString(g_map_list), muffmode::CvarInteger(g_map_list_shuffle));

	// This executes AFTER the gametype config has set the new g_map_list.
	// Shuffle the list if shuffle is enabled (mode 1 or 2).
	if (muffmode::CvarInteger(g_map_list_shuffle) >= 1)
	{
		MM_ShuffleMapList();
		if (muffmode::CvarInteger(g_map_list_shuffle) == 2)
		{
			extern bool g_map_list_shuffled;
			g_map_list_shuffled = true;
		}
	}

	const char *first_map = nullptr;

	// Try to get first map from g_map_list (now shuffled if enabled).
	if (muffmode::CvarString(g_map_list)[0])
	{
		const char *mlist = muffmode::CvarString(g_map_list);

		if (const char *token = muffmode::maps::ParseNextSafeMapToken(&mlist))
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
	if (std::string_view(first_map).size() >= sizeof(level.nextmap))
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
	if (!muffmode::CvarString(g_map_list)[0])
		return false;

	const char *str = muffmode::CvarString(g_map_list);
	std::string first_map;
	const char *map = nullptr;

	while (1)
	{
		map = muffmode::maps::ParseNextSafeMapToken(&str);

		if (!map || !*map)
			break;

		if (muffmode::CStringEqualsI(map, level.mapname))
		{
			// It's in the list, go to the next one.
			map = muffmode::maps::ParseNextSafeMapToken(&str);
			if (!map || !*map)
			{
				// End of list, go to first one.
				if (first_map.empty())
				{
					if (!MM_IsSafeMapToken(level.mapname))
						return false;

					BeginIntermission(CreateTargetChangeLevel(level.mapname));
					return true;
				}

				// End of list wrap-around: shuffle if enabled.
				// g_map_list_shuffle 1 = shuffle every wrap-around
				// g_map_list_shuffle 2 = shuffle once per gametype (lazy)
				if (muffmode::CvarInteger(g_map_list_shuffle) == 1)
				{
					MM_ShuffleMapList();
				}
				else if (muffmode::CvarInteger(g_map_list_shuffle) == 2)
				{
					extern bool g_map_list_shuffled;
					if (!g_map_list_shuffled)
					{
						MM_ShuffleMapList();
						g_map_list_shuffled = true;
					}
				}

				// Re-read first map from (possibly shuffled) list.
				const char *reshuffled_str = muffmode::CvarString(g_map_list);
				const char *reshuffled_first = muffmode::maps::ParseNextSafeMapToken(&reshuffled_str);
				if (reshuffled_first && *reshuffled_first)
					BeginIntermission(CreateTargetChangeLevel(reshuffled_first));
				else
					BeginIntermission(CreateTargetChangeLevel(first_map.c_str()));
				return true;
			}

			BeginIntermission(CreateTargetChangeLevel(map));
			return true;
		}

		if (first_map.empty())
			first_map = map;
	}

	// Current map not in g_map_list (e.g. voted from pool) - rejoin rotation at first map.
	if (!first_map.empty())
	{
		BeginIntermission(CreateTargetChangeLevel(first_map.c_str()));
		return true;
	}

	return false;
}

void MM_HandleMapShuffleCvarChange()
{
	if (!g_map_list_shuffle)
		return;

	if (muffmode::maps::s_map_list_shuffle_modified == g_map_list_shuffle->modified_count)
		return;

	muffmode::maps::s_map_list_shuffle_modified = g_map_list_shuffle->modified_count;

	// Shuffle immediately in lazy mode when toggled/changed from console.
	if (muffmode::CvarInteger(g_map_list_shuffle) == 2)
	{
		extern bool g_map_list_shuffled;
		MM_ShuffleMapList();
		g_map_list_shuffled = true;
	}
}

namespace muffmode::maps::queue {

constexpr size_t MM_MAX_MAPQUEUE_ENTRIES = MAX_CLIENTS_KEX;

struct mymap_modifier_t {
	const char *name = nullptr;
	int8_t game_locals_t::*setting = nullptr;
};

constexpr std::array<mymap_modifier_t, 6> k_mymap_modifiers = {{
	{ "pu", &game_locals_t::item_inhibit_pu },
	{ "pa", &game_locals_t::item_inhibit_pa },
	{ "ht", &game_locals_t::item_inhibit_ht },
	{ "ar", &game_locals_t::item_inhibit_ar },
	{ "am", &game_locals_t::item_inhibit_am },
	{ "wp", &game_locals_t::item_inhibit_wp },
}};

const mymap_modifier_t *MM_FindMyMapModifier(const char *name)
{
	if (!name || !*name)
		return nullptr;

	for (const auto &modifier : k_mymap_modifiers) {
		if (muffmode::CStringEqualsI(name, modifier.name))
			return &modifier;
	}

	return nullptr;
}

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

	if (!muffmode::CvarInteger(g_allow_mymap))
		return false;

	if (!muffmode::maps::HasConfiguredMapSource() && game.mapqueue.size()) {
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
			[&queued_map](const std::string &existing) { return muffmode::CStringEqualsI(existing.c_str(), queued_map.c_str()); }))
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
	std::string map_list_display = muffmode::CvarString(g_map_list);
	if (map_list_display.length() > MAX_MAP_LIST_DISPLAY)
		map_list_display = map_list_display.substr(0, MAX_MAP_LIST_DISPLAY) + "...";
	gi.LocClient_Print(ent, PRINT_HIGH, "{}\n", map_list_display.c_str());
}

void MM_PrintTruncatedMapSource(gentity_t *ent)
{
	const char *maps = muffmode::CvarString(g_map_list)[0] ? muffmode::CvarString(g_map_list) : muffmode::CvarString(g_map_pool);
	std::string display = maps ? maps : "";
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
	return MM_FindMyMapModifier(name) != nullptr;
}

bool MM_MapQueueContains(const char *mapname)
{
	return std::any_of(game.mapqueue.begin(), game.mapqueue.end(), [mapname](const std::string &queued_map) {
		return muffmode::CStringEqualsI(queued_map.c_str(), mapname);
	});
}

std::string s_next_mapqueue_return;

} // namespace muffmode::maps::queue

namespace map_queue = muffmode::maps::queue;

int MM_MQ_Count()
{
	if (!deathmatch)
		return 0;

	if (!muffmode::CvarInteger(g_allow_mymap))
		return 0;

	if (!map_queue::MM_MQ_Update())
		return 0;

	return static_cast<int>(game.mapqueue.size());
}

bool MM_MQ_Add(gentity_t *ent, const char *mapname)
{
	if (!ent || !ent->client)
		return false;

	if (!deathmatch)
		return false;

	if (!muffmode::CvarInteger(g_allow_mymap))
		return false;

	if (!MM_IsSafeMapToken(mapname)) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid map name.\n");
		return false;
	}

	if (!muffmode::maps::HasConfiguredMapSource())
		return false;

	if (!muffmode::maps::ContainsConfiguredMap(mapname)) {
		gi.Client_Print(ent, PRINT_HIGH, "Selected map is either invalid or not in pool/list.\n");
		return false;
	}

	if (!map_queue::MM_MQ_Update())
		return false;

	if (map_queue::MM_MapQueueContains(mapname)) {
		gi.Client_Print(ent, PRINT_HIGH, "Selected map is already in queue.\n");
		return false;
	}

	if (game.mapqueue.size() >= map_queue::MM_MAX_MAPQUEUE_ENTRIES) {
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

	if (!map_queue::MM_MQ_Update())
		return nullptr;

	for (size_t i = 0; i < game.mapqueue.size();) {
		if (!MM_IsSafeMapToken(game.mapqueue[i].c_str())) {
			game.mapqueue.erase(game.mapqueue.begin() + i);
			continue;
		}

		map_queue::s_next_mapqueue_return = game.mapqueue[i];
		game.mapqueue.erase(game.mapqueue.begin() + i);
		return map_queue::s_next_mapqueue_return.c_str();
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

	if (muffmode::maps::HasConfiguredMapSource()) {
		gi.LocClient_Print(ent, PRINT_HIGH, muffmode::CvarString(g_map_list)[0] ? "Current map list:\n" : "Current map pool:\n");
		if (muffmode::CvarString(g_map_list)[0])
			map_queue::MM_PrintTruncatedMapList(ent);
		else
			map_queue::MM_PrintTruncatedMapSource(ent);
		if (MM_MQ_Count()) {
			gi.LocClient_Print(ent, PRINT_HIGH, "\nCurrent MyMap Queue:\n");
			map_queue::MM_MQ_PrintList(ent);
		}
	} else {
		gi.LocClient_Print(ent, PRINT_HIGH, "No map list or pool set.\n");
	}
}

void MM_CmdMyMap(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	if (!muffmode::CvarInteger(g_allow_mymap)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "MyMap is disabled.\n");
		return;
	}

	if (!muffmode::maps::HasConfiguredMapSource()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "No maps are queued as no map list or pool is present.\n");
		return;
	}

	if (gi.argc() < 2) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Add a map to the MyMap Queue.\nRecognized maps are:\n");
		map_queue::MM_PrintTruncatedMapSource(ent);

		if (MM_MQ_Count()) {
			gi.LocClient_Print(ent, PRINT_HIGH, "MyMap Queue => ");
			map_queue::MM_MQ_PrintList(ent);
		}
		return;
	}

	for (int i = 2; i < gi.argc(); i++) {
		if (!map_queue::MM_IsValidMyMapModifier(gi.argv(i))) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Invalid MyMap modifier: {}\n", gi.argv(i));
			return;
		}
	}

	if (muffmode::CStringEqualsI(gi.argv(1), level.mapname)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Cannot add current map to MyMap Queue.\n");
		return;
	}

	if (!MM_MQ_Add(ent, gi.argv(1)))
		return;

	MM_ClearItemInhibitFlags();

	for (int i = 2; i < gi.argc(); i++) {
		const char *s = gi.argv(i);
		if (!s || !s[0])
			continue;
		int num = 0;
		if (s[0] == '+') { num = 1; s++; }
		else if (s[0] == '-') { num = -1; s++; }
		else continue;
		if (const auto *modifier = map_queue::MM_FindMyMapModifier(s))
			game.*(modifier->setting) = static_cast<int8_t>(num);
	}

	std::string text = map_queue::MM_MQ_FormatList();
	if (!text.empty())
		gi.LocBroadcast_Print(PRINT_HIGH, "MyMap Queue => {}\n", text.c_str());
}
