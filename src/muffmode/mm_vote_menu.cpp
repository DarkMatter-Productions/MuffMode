// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "g_debug_log.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_maps.h"
#include "muffmode/mm_menu.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_vote.h"
#include "muffmode/mm_vote_menu.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace {
static const int cvmenu_map = 3;

void G_Menu_CallVote_Map(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_NextMap(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_Restart(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_GameType(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_GameType_Update(gentity_t *ent);
void G_Menu_CallVote_GameType_Selection(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_Ruleset(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_Ruleset_Update(gentity_t *ent);
void G_Menu_CallVote_Ruleset_Selection(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_TimeLimit_Update(gentity_t *ent);
void G_Menu_CallVote_TimeLimit(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_TimeLimit_Selection(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_ScoreLimit_Update(gentity_t *ent);
void G_Menu_CallVote_ScoreLimit(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_ScoreLimit_Selection(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_ShuffleTeams(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_BalanceTeams(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_Powerups(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_Powerups_Update(gentity_t *ent);
void G_Menu_CallVote_Powerups_Selection(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_Techs(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_Techs_Update(gentity_t *ent);
void G_Menu_CallVote_Techs_Selection(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_FriendlyFire(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_FriendlyFire_Update(gentity_t *ent);
void G_Menu_CallVote_FriendlyFire_Selection(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_Cointoss(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_ReadyAll(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_Map_Selection(gentity_t *ent, menu_hnd_t *p);
void G_Menu_CallVote_Update(gentity_t *ent);

const menu_t pmcallvotemenu[] = {
	{ "Call a Vote", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "$g_pc_return", MENU_ALIGN_LEFT, G_Menu_ReturnToMain }
};

const menu_t pmcallvotemenu_map[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Map_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Map_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Map_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Map_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Map_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Map_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Map_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Map_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Map_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Map_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Map_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Map_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Map_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Map_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Map_Selection },
	{ "$g_pc_return", MENU_ALIGN_LEFT, G_Menu_ReturnToCallVote }
};

const menu_t pmcallvotemenu_gametype[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_GameType_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_GameType_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_GameType_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_GameType_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_GameType_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_GameType_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_GameType_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_GameType_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_GameType_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_GameType_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_GameType_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_GameType_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_GameType_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_GameType_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_GameType_Selection },
	{ "$g_pc_return", MENU_ALIGN_LEFT, G_Menu_ReturnToCallVote }
};

const menu_t pmcallvotemenu_ruleset[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Ruleset_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Ruleset_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Ruleset_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Ruleset_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Ruleset_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Ruleset_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Ruleset_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Ruleset_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Ruleset_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Ruleset_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Ruleset_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Ruleset_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Ruleset_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Ruleset_Selection },
	{ "", MENU_ALIGN_LEFT, G_Menu_CallVote_Ruleset_Selection },
	{ "$g_pc_return", MENU_ALIGN_LEFT, G_Menu_ReturnToCallVote }
};

const menu_t pmcallvotemenu_powerups[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "ON",  MENU_ALIGN_LEFT, G_Menu_CallVote_Powerups_Selection },
	{ "OFF", MENU_ALIGN_LEFT, G_Menu_CallVote_Powerups_Selection },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "$g_pc_return", MENU_ALIGN_LEFT, G_Menu_ReturnToCallVote }
};

const menu_t pmcallvotemenu_techs[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "ON",  MENU_ALIGN_LEFT, G_Menu_CallVote_Techs_Selection },
	{ "OFF", MENU_ALIGN_LEFT, G_Menu_CallVote_Techs_Selection },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "$g_pc_return", MENU_ALIGN_LEFT, G_Menu_ReturnToCallVote }
};

const menu_t pmcallvotemenu_friendlyfire[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "ON",  MENU_ALIGN_LEFT, G_Menu_CallVote_FriendlyFire_Selection },
	{ "OFF", MENU_ALIGN_LEFT, G_Menu_CallVote_FriendlyFire_Selection },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "$g_pc_return", MENU_ALIGN_LEFT, G_Menu_ReturnToCallVote }
};

const menu_t pmcallvotemenu_timelimit[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "0",  MENU_ALIGN_LEFT, G_Menu_CallVote_TimeLimit_Selection },
	{ "5",  MENU_ALIGN_LEFT, G_Menu_CallVote_TimeLimit_Selection },
	{ "10", MENU_ALIGN_LEFT, G_Menu_CallVote_TimeLimit_Selection },
	{ "15", MENU_ALIGN_LEFT, G_Menu_CallVote_TimeLimit_Selection },
	{ "20", MENU_ALIGN_LEFT, G_Menu_CallVote_TimeLimit_Selection },
	{ "25", MENU_ALIGN_LEFT, G_Menu_CallVote_TimeLimit_Selection },
	{ "30", MENU_ALIGN_LEFT, G_Menu_CallVote_TimeLimit_Selection },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "$g_pc_return", MENU_ALIGN_LEFT, G_Menu_ReturnToCallVote }
};

const menu_t pmcallvotemenu_scorelimit[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "0", MENU_ALIGN_LEFT, G_Menu_CallVote_ScoreLimit_Selection },
	{ "5", MENU_ALIGN_LEFT, G_Menu_CallVote_ScoreLimit_Selection },
	{ "10", MENU_ALIGN_LEFT, G_Menu_CallVote_ScoreLimit_Selection },
	{ "20", MENU_ALIGN_LEFT, G_Menu_CallVote_ScoreLimit_Selection },
	{ "30", MENU_ALIGN_LEFT, G_Menu_CallVote_ScoreLimit_Selection },
	{ "40", MENU_ALIGN_LEFT, G_Menu_CallVote_ScoreLimit_Selection },
	{ "50", MENU_ALIGN_LEFT, G_Menu_CallVote_ScoreLimit_Selection },
	{ "100", MENU_ALIGN_LEFT, G_Menu_CallVote_ScoreLimit_Selection },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "$g_pc_return", MENU_ALIGN_LEFT, G_Menu_ReturnToCallVote }
};

inline std::vector<std::string> str_split(const std::string_view &str, char by)
{
	std::vector<std::string> out;
	size_t start = 0;

	while (true)
	{
		start = str.find_first_not_of(by, start);
		if (start == std::string_view::npos)
			break;

		size_t end = str.find(by, start);
		if (end == std::string_view::npos)
		{
			out.emplace_back(str.substr(start));
			break;
		}

		out.emplace_back(str.substr(start, end - start));
		start = end + 1;
	}

	return out;
}

static bool MenuVote_ReadSelection(gentity_t *ent, menu_hnd_t *p, char *out, size_t out_size)
{
	if (!out || out_size == 0)
		return false;

	out[0] = '\0';

	if (!ent || !ent->client)
		return false;

	if (!p || !p->entries || p->cur < 0 || p->cur >= p->num)
		return false;

	UpdateFunc_t saved = p->UpdateFunc;
	p->UpdateFunc = nullptr;
	Q_strlcpy(out, p->entries[p->cur].text_arg1, out_size);
	p->UpdateFunc = saved;

	if (!out[0])
		return false;

	P_Menu_Close(ent);
	return true;
}

struct menu_vote_view_t {
	menu_t *entries = nullptr;
	int num = 0;
};

static bool MenuVote_View(gentity_t *ent, menu_vote_view_t &view)
{
	if (!ent || !ent->client || !ent->client->menu || !ent->client->menu->entries || ent->client->menu->num <= 0)
		return false;

	view.entries = ent->client->menu->entries;
	view.num = ent->client->menu->num;
	return true;
}

static menu_t *MenuVote_Entries(gentity_t *ent, int *num = nullptr)
{
	menu_vote_view_t view;
	if (!MenuVote_View(ent, view))
		return nullptr;

	if (num)
		*num = view.num;
	return view.entries;
}

static bool MenuVote_HasIndex(const menu_vote_view_t &view, int index)
{
	return index >= 0 && index < view.num;
}

static void MenuVote_ClearEntry(menu_t &entry)
{
	entry.text[0] = '\0';
	entry.text_arg1[0] = '\0';
	entry.SelectFunc = nullptr;
}

static void MenuVote_ClearRange(const menu_vote_view_t &view, int first, int last_exclusive)
{
	first = max(first, 0);
	last_exclusive = min(last_exclusive, view.num);
	for (int i = first; i < last_exclusive; ++i)
		MenuVote_ClearEntry(view.entries[i]);
}

static int MenuVote_ContentLimit(const menu_vote_view_t &view)
{
	// Reserve the final row for "return" entries in these menu definitions.
	return max(0, view.num - 1);
}

static menu_hnd_t *MenuVote_OpenMenu(gentity_t *ent, const menu_t *entries, int num, void *arg, UpdateFunc_t update)
{
	if (!ent || !ent->client || !entries || num <= 0)
		return nullptr;

	P_Menu_Close(ent);
	return P_Menu_Open(ent, entries, -1, num, arg, update);
}

const char *G_Menu_CallVoteCurrentRulesetName()
{
	const int ruleset = clamp((int)game.ruleset, (int)RS_NONE + 1, (int)RS_NUM_RULESETS - 1);
	return rs_long_name[ruleset];
}

static void MenuVote_Initiate(gentity_t *ent, const char *cmd_name, const char *arg)
{
	if (!ent || !ent->client)
		return;

	vcmds_t *cc = FindVoteCmdByName(cmd_name);
	if (!cc)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid vote command: {}\n", cmd_name ? cmd_name : "(null)");
		return;
	}
	if (level.vote_state.state != VoteState::IDLE)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "A vote is already in progress.\n");
		return;
	}
	if (!ValidateMenuVoteCommand(ent, cc, arg))
		return;
	level.vote_state.command = cc;
	level.vote_state.arg = arg ? arg : "";
	MM_VoteCommandStore(ent);
}

static void MenuVote_CloseAndInitiate(gentity_t *ent, const char *cmd_name, const char *arg)
{
	if (!ent || !ent->client)
		return;

	P_Menu_Close(ent);
	MenuVote_Initiate(ent, cmd_name, arg);
}

struct map_menu_page_t {
	int offset;
};

static constexpr int MAPS_PER_PAGE = 12;
static constexpr int MAP_MENU_FIRST_ITEM = 2;

static int MenuVote_MapPageSize(int menu_entries)
{
	const int content_limit = max(0, menu_entries - 1);
	const int prev_index = content_limit - 3;
	return min(MAPS_PER_PAGE, max(0, prev_index - MAP_MENU_FIRST_ITEM));
}

static int MenuVote_MapPrevIndex(int menu_entries)
{
	return max(MAP_MENU_FIRST_ITEM, max(0, menu_entries - 1) - 3);
}

static int MenuVote_MapNextIndex(int menu_entries)
{
	return max(MAP_MENU_FIRST_ITEM, max(0, menu_entries - 1) - 2);
}

void G_Menu_CallVote_Map_Selection(gentity_t *ent, menu_hnd_t *p)
{
	char value[MAX_QPATH];
	if (!MenuVote_ReadSelection(ent, p, value, sizeof(value)))
		return;

	if (!MM_IsSafeMapToken(value))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid characters in map name.\n");
		return;
	}

	MenuVote_Initiate(ent, "map", value);
}

void G_Menu_CallVote_Map_PrevPage(gentity_t *ent, menu_hnd_t *p)
{
	if (!ent || !ent->client || !p)
		return;

	const int page_size = MenuVote_MapPageSize(p->num);
	if (page_size <= 0)
		return;

	map_menu_page_t *page = (map_menu_page_t *)p->arg;
	if (page && page->offset > 0)
	{
		page->offset -= page_size;
		if (page->offset < 0)
			page->offset = 0;
	}
	P_Menu_Update(ent);
}

void G_Menu_CallVote_Map_NextPage(gentity_t *ent, menu_hnd_t *p)
{
	if (!ent || !ent->client || !p)
		return;

	const int page_size = MenuVote_MapPageSize(p->num);
	if (page_size <= 0)
		return;

	map_menu_page_t *page = (map_menu_page_t *)p->arg;
	if (page)
		page->offset += page_size;
	P_Menu_Update(ent);
}

void G_Menu_CallVote_Map_Update(gentity_t *ent)
{
	menu_vote_view_t view;
	if (!MenuVote_View(ent, view) || !MenuVote_HasIndex(view, 0) || !MenuVote_HasIndex(view, 1))
		return;

	menu_t *entries = view.entries;
	const int page_size = MenuVote_MapPageSize(view.num);
	const int prev_index = MenuVote_MapPrevIndex(view.num);
	const int next_index = MenuVote_MapNextIndex(view.num);
	const int content_limit = MenuVote_ContentLimit(view);
	if (page_size <= 0 || prev_index >= content_limit || next_index >= content_limit)
		return;

	MenuVote_ClearEntry(entries[1]);

	std::vector<std::string> values;
	auto map_exists = [&values](const std::string &map) -> bool
	{
		for (const auto &existing : values)
		{
			if (!Q_strcasecmp(existing.c_str(), map.c_str()))
				return true;
		}
		return false;
	};

	if (g_map_pool->string[0])
	{
		auto pool_values = str_split(g_map_pool->string, ' ');
		for (const auto &map : pool_values)
		{
			if (MM_IsSafeMapToken(map.c_str()) && !map_exists(map))
				values.push_back(map);
		}
	}

	if (g_map_list->string[0])
	{
		auto list_values = str_split(g_map_list->string, ' ');
		for (const auto &map : list_values)
		{
			if (MM_IsSafeMapToken(map.c_str()) && !map_exists(map))
				values.push_back(map);
		}
	}

	if (values.size() > 0)
	{
		std::sort(values.begin(), values.end(), [](const std::string &a, const std::string &b)
		{
			return Q_strcasecmp(a.c_str(), b.c_str()) < 0;
		});
	}

	map_menu_page_t *page = (map_menu_page_t *)ent->client->menu->arg;
	int offset = page ? page->offset : 0;
	int total_maps = (int)values.size();

	if (offset < 0)
		offset = 0;
	if (offset >= total_maps)
		offset = (total_maps > 0) ? ((total_maps - 1) / page_size) * page_size : 0;
	if (page)
		page->offset = offset;

	int total_pages = (total_maps + page_size - 1) / page_size;
	if (total_pages < 1)
		total_pages = 1;
	int current_page = (offset / page_size) + 1;

	if (total_pages > 1)
		Q_strlcpy(entries[0].text, G_Fmt("Choose a Map ({}/{})", current_page, total_pages).data(), sizeof(entries[0].text));
	else
		Q_strlcpy(entries[0].text, "Choose a Map", sizeof(entries[0].text));

	MenuVote_ClearRange(view, MAP_MENU_FIRST_ITEM, content_limit);

	int menu_index = MAP_MENU_FIRST_ITEM;
	for (int i = offset; i < total_maps && menu_index < (MAP_MENU_FIRST_ITEM + page_size); i++)
	{
		Q_strlcpy(entries[menu_index].text_arg1, values[i].c_str(), sizeof(entries[menu_index].text_arg1));
		Q_strlcpy(entries[menu_index].text, values[i].c_str(), sizeof(entries[menu_index].text));
		entries[menu_index].SelectFunc = G_Menu_CallVote_Map_Selection;
		menu_index++;
	}

	if (offset > 0)
	{
		Q_strlcpy(entries[prev_index].text, "< Prev Page", sizeof(entries[prev_index].text));
		entries[prev_index].SelectFunc = G_Menu_CallVote_Map_PrevPage;
	}

	if (offset + page_size < total_maps)
	{
		Q_strlcpy(entries[next_index].text, "> Next Page", sizeof(entries[next_index].text));
		entries[next_index].SelectFunc = G_Menu_CallVote_Map_NextPage;
	}
}

void G_Menu_CallVote_Map(gentity_t *ent, menu_hnd_t *p)
{
	if (!ent || !ent->client)
		return;

	map_menu_page_t *page = (map_menu_page_t *)gi.TagMalloc(sizeof(map_menu_page_t), TAG_LEVEL);
	page->offset = 0;
	MenuVote_OpenMenu(ent, pmcallvotemenu_map, (int)q_countof(pmcallvotemenu_map), page, G_Menu_CallVote_Map_Update);
}

void G_Menu_CallVote_NextMap(gentity_t *ent, menu_hnd_t *p)
{
	MenuVote_CloseAndInitiate(ent, "nextmap", nullptr);
}

void G_Menu_CallVote_Restart(gentity_t *ent, menu_hnd_t *p)
{
	MenuVote_CloseAndInitiate(ent, "restart", nullptr);
}

void G_Menu_CallVote_GameType_Update(gentity_t *ent)
{
	menu_vote_view_t view;
	if (!MenuVote_View(ent, view) || !MenuVote_HasIndex(view, 0))
		return;

	menu_t *entries = view.entries;
	const int content_limit = MenuVote_ContentLimit(view);
	if (content_limit <= 2)
		return;

	Q_strlcpy(entries[0].text, "Select Gametype", sizeof(entries[0].text));

	MenuVote_ClearRange(view, 2, content_limit);

	int menu_index = 2;
	for (int i = (int)GT_FIRST; i <= (int)GT_LAST && menu_index < content_limit; i++)
	{
		gametype_t gt = (gametype_t)i;
		if (gt == GT_NONE)
			continue;
		if (!MM_IsGametypeVotable(gt))
			continue;

		Q_strlcpy(entries[menu_index].text_arg1, gt_short_name[i], sizeof(entries[menu_index].text_arg1));
		Q_strlcpy(entries[menu_index].text, gt_long_name[i], sizeof(entries[menu_index].text));
		entries[menu_index].SelectFunc = G_Menu_CallVote_GameType_Selection;
		menu_index++;
	}
}

void G_Menu_CallVote_GameType_Selection(gentity_t *ent, menu_hnd_t *p)
{
	char value[64];
	if (!MenuVote_ReadSelection(ent, p, value, sizeof(value)))
		return;

	gametype_t gt = GT_IndexFromString(value);
	if (gt == GT_NONE)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid gametype selected.\n");
		return;
	}
	if (!MM_IsGametypeVotable(gt))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "This gametype is not available for voting.\n");
		return;
	}

	MenuVote_Initiate(ent, "gametype", value);
}

void G_Menu_CallVote_GameType(gentity_t *ent, menu_hnd_t *p)
{
	MenuVote_OpenMenu(ent, pmcallvotemenu_gametype, (int)q_countof(pmcallvotemenu_gametype), nullptr, G_Menu_CallVote_GameType_Update);
}

void G_Menu_CallVote_Ruleset_Update(gentity_t *ent)
{
	menu_vote_view_t view;
	if (!MenuVote_View(ent, view) || !MenuVote_HasIndex(view, 0))
		return;

	menu_t *entries = view.entries;
	const int content_limit = MenuVote_ContentLimit(view);
	if (content_limit <= 2)
		return;

	Q_strlcpy(entries[0].text, "Select Ruleset", sizeof(entries[0].text));

	MenuVote_ClearRange(view, 2, content_limit);

	int menu_index = 2;
	for (int i = (int)RS_NONE + 1; i < (int)RS_NUM_RULESETS && menu_index < content_limit; i++)
	{
		ruleset_t rs = (ruleset_t)i;
		if (rs == RS_NONE)
			continue;
		if (!MM_IsRulesetVotable(rs))
			continue;

		Q_strlcpy(entries[menu_index].text_arg1, rs_short_name[i], sizeof(entries[menu_index].text_arg1));
		Q_strlcpy(entries[menu_index].text, rs_long_name[i], sizeof(entries[menu_index].text));
		entries[menu_index].SelectFunc = G_Menu_CallVote_Ruleset_Selection;
		menu_index++;
	}
}

void G_Menu_CallVote_Ruleset_Selection(gentity_t *ent, menu_hnd_t *p)
{
	char value[64];
	if (!MenuVote_ReadSelection(ent, p, value, sizeof(value)))
		return;

	ruleset_t rs = RS_IndexFromString(value);
	if (rs == ruleset_t::RS_NONE)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid ruleset selected.\n");
		return;
	}
	if ((int)rs == game.ruleset)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Ruleset is already active.\n");
		return;
	}

	MenuVote_Initiate(ent, "ruleset", value);
}

void G_Menu_CallVote_Ruleset(gentity_t *ent, menu_hnd_t *p)
{
	MenuVote_OpenMenu(ent, pmcallvotemenu_ruleset, (int)q_countof(pmcallvotemenu_ruleset), nullptr, G_Menu_CallVote_Ruleset_Update);
}

void G_Menu_CallVote_TimeLimit_Update(gentity_t *ent)
{
	menu_vote_view_t view;
	if (!MenuVote_View(ent, view) || !MenuVote_HasIndex(view, 0))
		return;

	menu_t *entries = view.entries;
	const int content_limit = MenuVote_ContentLimit(view);
	Q_strlcpy(entries[0].text, "Select Time Limit (mins)", sizeof(entries[0].text));

	static const char *time_values[] = { "0", "5", "10", "15", "20", "25", "30" };
	const int first_index = 2;
	const int num_values = (int)(sizeof(time_values) / sizeof(time_values[0]));

	for (int i = 0; i < num_values; ++i)
	{
		int idx = first_index + i;
		if (idx >= content_limit)
			break;
		Q_strlcpy(entries[idx].text, time_values[i], sizeof(entries[idx].text));
		Q_strlcpy(entries[idx].text_arg1, time_values[i], sizeof(entries[idx].text_arg1));
	}
}

void G_Menu_CallVote_TimeLimit_Selection(gentity_t *ent, menu_hnd_t *p)
{
	char value[64];
	if (!MenuVote_ReadSelection(ent, p, value, sizeof(value)))
		return;
	MenuVote_Initiate(ent, "timelimit", value);
}

void G_Menu_CallVote_TimeLimit(gentity_t *ent, menu_hnd_t *p)
{
	MenuVote_OpenMenu(ent, pmcallvotemenu_timelimit, (int)q_countof(pmcallvotemenu_timelimit), nullptr, G_Menu_CallVote_TimeLimit_Update);
}

void G_Menu_CallVote_ScoreLimit_Update(gentity_t *ent)
{
	menu_vote_view_t view;
	if (!MenuVote_View(ent, view) || !MenuVote_HasIndex(view, 0))
		return;

	menu_t *entries = view.entries;
	const int content_limit = MenuVote_ContentLimit(view);
	Q_strlcpy(entries[0].text, "Select Score Limit", sizeof(entries[0].text));

	static const char *score_values[] = { "0", "5", "10", "20", "30", "40", "50", "100" };
	const int first_index = 2;
	const int num_values = (int)(sizeof(score_values) / sizeof(score_values[0]));

	for (int i = 0; i < num_values; ++i)
	{
		int idx = first_index + i;
		if (idx >= content_limit)
			break;
		Q_strlcpy(entries[idx].text, score_values[i], sizeof(entries[idx].text));
		Q_strlcpy(entries[idx].text_arg1, score_values[i], sizeof(entries[idx].text_arg1));
	}
}

void G_Menu_CallVote_ScoreLimit_Selection(gentity_t *ent, menu_hnd_t *p)
{
	char value[64];
	if (!MenuVote_ReadSelection(ent, p, value, sizeof(value)))
		return;
	MenuVote_Initiate(ent, "scorelimit", value);
}

void G_Menu_CallVote_ScoreLimit(gentity_t *ent, menu_hnd_t *p)
{
	MenuVote_OpenMenu(ent, pmcallvotemenu_scorelimit, (int)q_countof(pmcallvotemenu_scorelimit), nullptr, G_Menu_CallVote_ScoreLimit_Update);
}

void G_Menu_CallVote_ShuffleTeams(gentity_t *ent, menu_hnd_t *p)
{
	MenuVote_CloseAndInitiate(ent, "shuffle", nullptr);
}

void G_Menu_CallVote_BalanceTeams(gentity_t *ent, menu_hnd_t *p)
{
	MenuVote_CloseAndInitiate(ent, "balance", nullptr);
}

void G_Menu_CallVote_Cointoss(gentity_t *ent, menu_hnd_t *p)
{
	MenuVote_CloseAndInitiate(ent, "cointoss", nullptr);
}

void G_Menu_CallVote_ReadyAll(gentity_t *ent, menu_hnd_t *p)
{
	MenuVote_CloseAndInitiate(ent, "readyall", nullptr);
}

void G_Menu_CallVote_Update(gentity_t *ent)
{
	menu_vote_view_t view;
	if (!MenuVote_View(ent, view))
		return;

	menu_t *entries = view.entries;
	const int content_limit = MenuVote_ContentLimit(view);
	const int readyall_index = 14;
	if (content_limit <= readyall_index)
		return;

	MenuVote_ClearRange(view, 0, content_limit);

	Q_strlcpy(entries[0].text, "Call a Vote", sizeof(entries[0].text));

	int gametype_index = cvmenu_map;
	entries[gametype_index].SelectFunc = G_Menu_CallVote_GameType;
	Q_strlcpy(entries[gametype_index].text, G_Fmt("Gametype: {}", level.gametype_name).data(), sizeof(entries[gametype_index].text));

	int ruleset_index = gametype_index + 1;
	entries[ruleset_index].SelectFunc = G_Menu_CallVote_Ruleset;
	Q_strlcpy(entries[ruleset_index].text, G_Fmt("Ruleset: {}", G_Menu_CallVoteCurrentRulesetName()).data(), sizeof(entries[ruleset_index].text));

	int map_index = ruleset_index + 1;
	entries[map_index].SelectFunc = G_Menu_CallVote_Map;
	if (level.mapname[0])
		Q_strlcpy(entries[map_index].text, G_Fmt("Map:\t\t {}", level.mapname).data(), sizeof(entries[map_index].text));
	else
		Q_strlcpy(entries[map_index].text, "Map", sizeof(entries[map_index].text));

	int blank1_index = map_index + 1;
	entries[blank1_index].SelectFunc = nullptr;
	entries[blank1_index].text[0] = '\0';

	int scorelimit_index = blank1_index + 1;
	entries[scorelimit_index].SelectFunc = G_Menu_CallVote_ScoreLimit;
	int current_scorelimit = GT_ScoreLimit();
	if (current_scorelimit > 0)
		Q_strlcpy(entries[scorelimit_index].text, G_Fmt("Scorelimit: {}", current_scorelimit).data(), sizeof(entries[scorelimit_index].text));
	else
		Q_strlcpy(entries[scorelimit_index].text, "Scorelimit: 0", sizeof(entries[scorelimit_index].text));

	int timelimit_index = scorelimit_index + 1;
	entries[timelimit_index].SelectFunc = G_Menu_CallVote_TimeLimit;
	int current_timelimit = (int)timelimit->value;
	if (current_timelimit > 0)
		Q_strlcpy(entries[timelimit_index].text, G_Fmt("Timelimit: {} min", current_timelimit).data(), sizeof(entries[timelimit_index].text));
	else
		Q_strlcpy(entries[timelimit_index].text, "Timelimit: 0", sizeof(entries[timelimit_index].text));

	int blank2_index = timelimit_index + 1;
	entries[blank2_index].SelectFunc = nullptr;
	entries[blank2_index].text[0] = '\0';

	int powerups_index = blank2_index + 1;
	entries[powerups_index].SelectFunc = G_Menu_CallVote_Powerups;
	bool powerups_enabled = g_no_powerups->integer == 0;
	Q_strlcpy(entries[powerups_index].text, G_Fmt("Powerups: {}", powerups_enabled ? "ON" : "OFF").data(), sizeof(entries[powerups_index].text));

	int techs_index = powerups_index + 1;
	if (GT(GT_FFA) || GT(GT_TDM) || GT(GT_CTF))
	{
		entries[techs_index].SelectFunc = G_Menu_CallVote_Techs;
		Q_strlcpy(entries[techs_index].text, G_Fmt("Techs: {}", AllowTechs() ? "ON" : "OFF").data(), sizeof(entries[techs_index].text));
	}
	else
	{
		entries[techs_index].SelectFunc = nullptr;
		Q_strlcpy(entries[techs_index].text, "Techs: N/A", sizeof(entries[techs_index].text));
	}

	int friendlyfire_index = techs_index + 1;
	if (Teams())
	{
		entries[friendlyfire_index].SelectFunc = G_Menu_CallVote_FriendlyFire;
		bool ff_enabled = g_friendly_fire->integer != 0;
		Q_strlcpy(entries[friendlyfire_index].text, G_Fmt("Friendly Fire: {}", ff_enabled ? "ON" : "OFF").data(), sizeof(entries[friendlyfire_index].text));
	}
	else
	{
		entries[friendlyfire_index].SelectFunc = nullptr;
		Q_strlcpy(entries[friendlyfire_index].text, "Friendly Fire: N/A", sizeof(entries[friendlyfire_index].text));
	}

	int shuffle_index = friendlyfire_index + 1;
	if (Teams())
	{
		entries[shuffle_index].SelectFunc = G_Menu_CallVote_ShuffleTeams;
		Q_strlcpy(entries[shuffle_index].text, "Shuffle Teams", sizeof(entries[shuffle_index].text));
	}
	else
	{
		entries[shuffle_index].SelectFunc = nullptr;
		Q_strlcpy(entries[shuffle_index].text, "Shuffle Teams: N/A", sizeof(entries[shuffle_index].text));
	}

	if (g_dm_do_readyup->integer && level.match_state == matchst_t::MATCH_WARMUP_READYUP)
	{
		entries[readyall_index].SelectFunc = G_Menu_CallVote_ReadyAll;
		Q_strlcpy(entries[readyall_index].text, "Ready All", sizeof(entries[readyall_index].text));
	}
	else
	{
		entries[readyall_index].SelectFunc = nullptr;
		Q_strlcpy(entries[readyall_index].text, "Ready All: N/A", sizeof(entries[readyall_index].text));
	}

	MenuVote_ClearRange(view, readyall_index + 1, content_limit);
}

void G_Menu_CallVote_Powerups_Update(gentity_t *ent)
{
	menu_vote_view_t view;
	if (!MenuVote_View(ent, view) || !MenuVote_HasIndex(view, 3))
		return;

	menu_t *entries = view.entries;
	Q_strlcpy(entries[0].text, "Powerups", sizeof(entries[0].text));
	Q_strlcpy(entries[2].text, "ON", sizeof(entries[2].text));
	Q_strlcpy(entries[2].text_arg1, "1", sizeof(entries[2].text_arg1));
	Q_strlcpy(entries[3].text, "OFF", sizeof(entries[3].text));
	Q_strlcpy(entries[3].text_arg1, "0", sizeof(entries[3].text_arg1));
}

void G_Menu_CallVote_Powerups_Selection(gentity_t *ent, menu_hnd_t *p)
{
	char value[64];
	if (!MenuVote_ReadSelection(ent, p, value, sizeof(value)))
		return;

	const auto parsed = MM_ParseNonNegativeIntArg(value);
	if (!parsed || (*parsed != 0 && *parsed != 1)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid powerups selection.\n");
		return;
	}

	const int v = *parsed;
	bool currently_enabled = g_no_powerups->integer == 0;
	if (currently_enabled == (v == 1))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Powerups are already {}.\n", v ? "ENABLED" : "DISABLED");
		return;
	}

	MenuVote_Initiate(ent, "powerups", value);
}

void G_Menu_CallVote_Techs_Update(gentity_t *ent)
{
	menu_vote_view_t view;
	if (!MenuVote_View(ent, view) || !MenuVote_HasIndex(view, 3))
		return;

	menu_t *entries = view.entries;
	Q_strlcpy(entries[0].text, "Techs", sizeof(entries[0].text));
	Q_strlcpy(entries[2].text, "ON", sizeof(entries[2].text));
	Q_strlcpy(entries[2].text_arg1, "1", sizeof(entries[2].text_arg1));
	Q_strlcpy(entries[3].text, "OFF", sizeof(entries[3].text));
	Q_strlcpy(entries[3].text_arg1, "0", sizeof(entries[3].text_arg1));
}

void G_Menu_CallVote_Techs_Selection(gentity_t *ent, menu_hnd_t *p)
{
	char value[64];
	if (!MenuVote_ReadSelection(ent, p, value, sizeof(value)))
		return;

	const auto parsed = MM_ParseNonNegativeIntArg(value);
	if (!parsed || (*parsed != 0 && *parsed != 1)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid techs selection.\n");
		return;
	}

	const int v = *parsed;
	bool currently_enabled = AllowTechs();
	if (currently_enabled == (v == 1))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Techs are already {}.\n", v ? "ENABLED" : "DISABLED");
		return;
	}

	MenuVote_Initiate(ent, "techs", value);
}

void G_Menu_CallVote_Techs(gentity_t *ent, menu_hnd_t *p)
{
	MenuVote_OpenMenu(ent, pmcallvotemenu_techs, (int)q_countof(pmcallvotemenu_techs), nullptr, G_Menu_CallVote_Techs_Update);
}

void G_Menu_CallVote_Powerups(gentity_t *ent, menu_hnd_t *p)
{
	MenuVote_OpenMenu(ent, pmcallvotemenu_powerups, (int)q_countof(pmcallvotemenu_powerups), nullptr, G_Menu_CallVote_Powerups_Update);
}

void G_Menu_CallVote_FriendlyFire_Update(gentity_t *ent)
{
	menu_vote_view_t view;
	if (!MenuVote_View(ent, view) || !MenuVote_HasIndex(view, 3))
		return;

	menu_t *entries = view.entries;
	Q_strlcpy(entries[0].text, "Friendly Fire", sizeof(entries[0].text));
	Q_strlcpy(entries[2].text, "ON", sizeof(entries[2].text));
	Q_strlcpy(entries[2].text_arg1, "1", sizeof(entries[2].text_arg1));
	Q_strlcpy(entries[3].text, "OFF", sizeof(entries[3].text));
	Q_strlcpy(entries[3].text_arg1, "0", sizeof(entries[3].text_arg1));
}

void G_Menu_CallVote_FriendlyFire_Selection(gentity_t *ent, menu_hnd_t *p)
{
	char value[64];
	if (!MenuVote_ReadSelection(ent, p, value, sizeof(value)))
		return;

	const auto parsed = MM_ParseNonNegativeIntArg(value);
	if (!parsed || (*parsed != 0 && *parsed != 1)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid friendly fire selection.\n");
		return;
	}

	const int v = *parsed;
	bool currently_enabled = g_friendly_fire->integer != 0;
	if (currently_enabled == (v == 1))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Friendly fire is already {}.\n", v ? "ENABLED" : "DISABLED");
		return;
	}

	MenuVote_Initiate(ent, "friendlyfire", value);
}

void G_Menu_CallVote_FriendlyFire(gentity_t *ent, menu_hnd_t *p)
{
	MenuVote_OpenMenu(ent, pmcallvotemenu_friendlyfire, (int)q_countof(pmcallvotemenu_friendlyfire), nullptr, G_Menu_CallVote_FriendlyFire_Update);
}

void G_Menu_Vote_Yes(gentity_t *ent, menu_hnd_t *p)
{
	if (!ent || !ent->client)
		return;

	if (level.vote_state.state != VoteState::ACTIVE)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "No vote in progress.\n");
		P_Menu_Close(ent);
		return;
	}

	if (ent->client->pers.voted != 0)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Vote already cast.\n");
		return;
	}

	ent->client->pers.voted = 1;
	gi.LocClient_Print(ent, PRINT_HIGH, "Vote cast.\n");
	P_Menu_Close(ent);
}

void G_Menu_Vote_No(gentity_t *ent, menu_hnd_t *p)
{
	if (!ent || !ent->client)
		return;

	if (level.vote_state.state != VoteState::ACTIVE)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "No vote in progress.\n");
		P_Menu_Close(ent);
		return;
	}

	if (ent->client->pers.voted != 0)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Vote already cast.\n");
		return;
	}

	ent->client->pers.voted = -1;
	gi.LocClient_Print(ent, PRINT_HIGH, "Vote cast.\n");
	P_Menu_Close(ent);
}

const menu_t votemenu[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "Voting Menu", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "none", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "[ YES ]", MENU_ALIGN_CENTER, G_Menu_Vote_Yes },
	{ "[ NO ]", MENU_ALIGN_CENTER, G_Menu_Vote_No },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "30", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr }
};

void G_Menu_Vote_Update(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	int ci = (int)(ent->client - game.clients);
	MuffModeLog("DEBUG", "G_Menu_Vote_Update: enter for client %d, menu=%p", ci, (void *)ent->client->menu);

	if (!Vote_Menu_Active(ent))
	{
		P_Menu_Close(ent);
		return;
	}

	int timeout = 30 - (level.time - level.vote_state.start_time).seconds<int>();
	if (timeout <= 0)
	{
		P_Menu_Close(ent);
		return;
	}

	int num_entries = 0;
	menu_t *entries = MenuVote_Entries(ent, &num_entries);
	if (!entries || num_entries <= 16)
		return;

	MuffModeLog("DEBUG", "G_Menu_Vote_Update: entries=%p, caller=%p, command=%p",
		(void *)entries, (void *)level.vote_state.caller, (void *)level.vote_state.command);

	int i = 2;
	if (!level.vote_state.caller)
	{
		P_Menu_Close(ent);
		return;
	}
	MuffModeLog("DEBUG", "G_Menu_Vote_Update: writing caller name '%s'", level.vote_state.caller->resp.netname);
	Q_strlcpy(entries[i].text, G_Fmt("{} called a vote:", level.vote_state.caller->resp.netname).data(), sizeof(entries[i].text));

	i = 4;
	if (!level.vote_state.command)
	{
		P_Menu_Close(ent);
		return;
	}
	MuffModeLog("DEBUG", "G_Menu_Vote_Update: writing command '%s' arg '%s' (arg_ptr=%p)",
		level.vote_state.command->name, level.vote_state.arg.c_str(), (void *)level.vote_state.arg.c_str());
	Q_strlcpy(entries[i].text, G_Fmt("{} {}", level.vote_state.command->name, level.vote_state.arg).data(), sizeof(entries[i].text));

	if (level.vote_state.start_time + 3_sec > level.time)
	{
		i = 7;
		Q_strlcpy(entries[i].text, "GET READY TO VOTE!", sizeof(entries[i].text));
		entries[i].SelectFunc = nullptr;

		i = 8;
		int time = 3 - (level.time - level.vote_state.start_time).seconds<int>();
		Q_strlcpy(entries[i].text, G_Fmt("{}...", time).data(), sizeof(entries[i].text));
		entries[i].SelectFunc = nullptr;
		return;
	}

	i = 7;
	Q_strlcpy(entries[i].text, "[ YES ]", sizeof(entries[i].text));
	entries[i].SelectFunc = G_Menu_Vote_Yes;
	i = 8;
	Q_strlcpy(entries[i].text, "[ NO ]", sizeof(entries[i].text));
	entries[i].SelectFunc = G_Menu_Vote_No;

	i = 16;
	Q_strlcpy(entries[i].text, G_Fmt("{}", timeout).data(), sizeof(entries[i].text));
}
} // namespace

bool Vote_Menu_Active(gentity_t *ent)
{
	if (!ent || !ent->client)
		return false;

	if (level.vote_state.state != VoteState::ACTIVE)
		return false;

	if (!level.vote_state.caller)
		return false;

	if (ent->client->pers.voted)
		return false;

	return true;
}

void G_Menu_CallVote(gentity_t *ent, menu_hnd_t *p)
{
	if (!ent || !ent->client)
		return;

	if (!ClientCanVote(ent->client))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "You are not allowed to call a vote as a spectator.\n");
		return;
	}

	MenuVote_OpenMenu(ent, pmcallvotemenu, (int)q_countof(pmcallvotemenu), nullptr, G_Menu_CallVote_Update);
}

void G_Menu_ReturnToCallVote(gentity_t *ent, menu_hnd_t *p)
{
	if (!ent || !ent->client)
		return;

	MenuVote_OpenMenu(ent, pmcallvotemenu, (int)q_countof(pmcallvotemenu), nullptr, G_Menu_CallVote_Update);
	gi.local_sound(ent, CHAN_AUTO, gi.soundindex("misc/menu3.wav"), 1, ATTN_NONE, 0);
}

void G_Menu_Vote_Open(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	P_Menu_Open(ent, votemenu, -1, sizeof(votemenu) / sizeof(menu_t), nullptr, G_Menu_Vote_Update);
}
