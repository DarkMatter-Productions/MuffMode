// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "g_debug_log.h"
#include "muffmode/mm_vote_menu.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

void G_Menu_ReturnToMain(gentity_t *ent, menu_hnd_t *p);

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

static void MenuVote_Initiate(gentity_t *ent, const char *cmd_name, const char *arg)
{
	vcmds_t *cc = FindVoteCmdByName(cmd_name);
	if (!cc)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid vote command: {}\n", cmd_name ? cmd_name : "(null)");
		return;
	}
	if (!ValidateMenuVoteCommand(ent, cc, arg))
		return;
	level.vote_state.command = cc;
	level.vote_state.arg = arg ? arg : "";
	VoteCommandStore(ent);
}

struct map_menu_page_t {
	int offset;
};

static constexpr int MAPS_PER_PAGE = 12;

void G_Menu_CallVote_Map_Selection(gentity_t *ent, menu_hnd_t *p)
{
	char value[64];
	if (!MenuVote_ReadSelection(ent, p, value, sizeof(value)))
		return;

	if (strstr(value, "..") || strpbrk(value, ":?*\"<>|"))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid characters in map name.\n");
		return;
	}

	MenuVote_Initiate(ent, "map", value);
}

void G_Menu_CallVote_Map_PrevPage(gentity_t *ent, menu_hnd_t *p)
{
	map_menu_page_t *page = (map_menu_page_t *)p->arg;
	if (page && page->offset > 0)
	{
		page->offset -= MAPS_PER_PAGE;
		if (page->offset < 0)
			page->offset = 0;
	}
	P_Menu_Update(ent);
}

void G_Menu_CallVote_Map_NextPage(gentity_t *ent, menu_hnd_t *p)
{
	map_menu_page_t *page = (map_menu_page_t *)p->arg;
	if (page)
		page->offset += MAPS_PER_PAGE;
	P_Menu_Update(ent);
}

void G_Menu_CallVote_Map_Update(gentity_t *ent)
{
	menu_t *entries = ent->client->menu->entries;

	entries[1].text[0] = '\0';
	entries[1].SelectFunc = nullptr;
	entries[1].text_arg1[0] = '\0';

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
			if (!map.empty() && !map_exists(map))
				values.push_back(map);
		}
	}

	if (g_map_list->string[0])
	{
		auto list_values = str_split(g_map_list->string, ' ');
		for (const auto &map : list_values)
		{
			if (!map.empty() && !map_exists(map))
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

	if (offset >= total_maps)
		offset = (total_maps > 0) ? ((total_maps - 1) / MAPS_PER_PAGE) * MAPS_PER_PAGE : 0;
	if (page)
		page->offset = offset;

	int total_pages = (total_maps + MAPS_PER_PAGE - 1) / MAPS_PER_PAGE;
	if (total_pages < 1)
		total_pages = 1;
	int current_page = (offset / MAPS_PER_PAGE) + 1;

	if (total_pages > 1)
		Q_strlcpy(entries[0].text, G_Fmt("Choose a Map ({}/{})", current_page, total_pages).data(), sizeof(entries[0].text));
	else
		Q_strlcpy(entries[0].text, "Choose a Map", sizeof(entries[0].text));

	for (int i = 2; i <= 16; i++)
	{
		entries[i].SelectFunc = nullptr;
		entries[i].text[0] = '\0';
		entries[i].text_arg1[0] = '\0';
	}

	int menu_index = 2;
	for (int i = offset; i < total_maps && menu_index < (2 + MAPS_PER_PAGE); i++)
	{
		Q_strlcpy(entries[menu_index].text_arg1, values[i].c_str(), sizeof(entries[menu_index].text_arg1));
		Q_strlcpy(entries[menu_index].text, values[i].c_str(), sizeof(entries[menu_index].text));
		entries[menu_index].SelectFunc = G_Menu_CallVote_Map_Selection;
		menu_index++;
	}

	if (offset > 0)
	{
		Q_strlcpy(entries[14].text, "< Prev Page", sizeof(entries[14].text));
		entries[14].SelectFunc = G_Menu_CallVote_Map_PrevPage;
	}

	if (offset + MAPS_PER_PAGE < total_maps)
	{
		Q_strlcpy(entries[15].text, "> Next Page", sizeof(entries[15].text));
		entries[15].SelectFunc = G_Menu_CallVote_Map_NextPage;
	}
}

void G_Menu_CallVote_Map(gentity_t *ent, menu_hnd_t *p)
{
	P_Menu_Close(ent);
	map_menu_page_t *page = (map_menu_page_t *)gi.TagMalloc(sizeof(map_menu_page_t), TAG_LEVEL);
	page->offset = 0;
	P_Menu_Open(ent, pmcallvotemenu_map, -1, sizeof(pmcallvotemenu_map) / sizeof(menu_t), page, G_Menu_CallVote_Map_Update);
}

void G_Menu_CallVote_NextMap(gentity_t *ent, menu_hnd_t *p)
{
	P_Menu_Close(ent);
	MenuVote_Initiate(ent, "nextmap", nullptr);
}

void G_Menu_CallVote_Restart(gentity_t *ent, menu_hnd_t *p)
{
	P_Menu_Close(ent);
	MenuVote_Initiate(ent, "restart", nullptr);
}

void G_Menu_CallVote_GameType_Update(gentity_t *ent)
{
	menu_t *entries = ent->client->menu->entries;
	Q_strlcpy(entries[0].text, "Select Gametype", sizeof(entries[0].text));

	for (int i = 2; i < 17; i++)
	{
		entries[i].SelectFunc = nullptr;
		entries[i].text[0] = '\0';
		entries[i].text_arg1[0] = '\0';
	}

	int menu_index = 2;
	for (int i = (int)GT_FIRST; i <= (int)GT_LAST && menu_index < 17; i++)
	{
		gametype_t gt = (gametype_t)i;
		if (gt == GT_NONE || gt == GT_STRIKE || gt == GT_RR || gt == GT_LMS || gt == GT_BALL)
			continue;
		if (!IsGametypeVotable(gt))
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
	if (!IsGametypeVotable(gt))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "This gametype is not available for voting.\n");
		return;
	}

	MenuVote_Initiate(ent, "gametype", value);
}

void G_Menu_CallVote_GameType(gentity_t *ent, menu_hnd_t *p)
{
	P_Menu_Close(ent);
	P_Menu_Open(ent, pmcallvotemenu_gametype, -1, sizeof(pmcallvotemenu_gametype) / sizeof(menu_t), nullptr, G_Menu_CallVote_GameType_Update);
}

void G_Menu_CallVote_Ruleset_Update(gentity_t *ent)
{
	menu_t *entries = ent->client->menu->entries;
	Q_strlcpy(entries[0].text, "Select Ruleset", sizeof(entries[0].text));

	for (int i = 2; i < 17; i++)
	{
		entries[i].SelectFunc = nullptr;
		entries[i].text[0] = '\0';
		entries[i].text_arg1[0] = '\0';
	}

	int menu_index = 2;
	for (int i = (int)RS_NONE + 1; i < (int)RS_NUM_RULESETS && menu_index < 17; i++)
	{
		ruleset_t rs = (ruleset_t)i;
		if (rs == RS_NONE)
			continue;
		if (!IsRulesetVotable(rs))
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
	P_Menu_Close(ent);
	P_Menu_Open(ent, pmcallvotemenu_ruleset, -1, sizeof(pmcallvotemenu_ruleset) / sizeof(menu_t), nullptr, G_Menu_CallVote_Ruleset_Update);
}

void G_Menu_CallVote_TimeLimit_Update(gentity_t *ent)
{
	menu_t *entries = ent->client->menu->entries;
	Q_strlcpy(entries[0].text, "Select Time Limit (mins)", sizeof(entries[0].text));

	static const char *time_values[] = { "0", "5", "10", "15", "20", "25", "30" };
	const int first_index = 2;
	const int num_values = (int)(sizeof(time_values) / sizeof(time_values[0]));

	for (int i = 0; i < num_values; ++i)
	{
		int idx = first_index + i;
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
	P_Menu_Close(ent);
	P_Menu_Open(ent, pmcallvotemenu_timelimit, -1, sizeof(pmcallvotemenu_timelimit) / sizeof(menu_t), nullptr, G_Menu_CallVote_TimeLimit_Update);
}

void G_Menu_CallVote_ScoreLimit_Update(gentity_t *ent)
{
	menu_t *entries = ent->client->menu->entries;
	Q_strlcpy(entries[0].text, "Select Score Limit", sizeof(entries[0].text));

	static const char *score_values[] = { "0", "5", "10", "20", "30", "40", "50", "100" };
	const int first_index = 2;
	const int num_values = (int)(sizeof(score_values) / sizeof(score_values[0]));

	for (int i = 0; i < num_values; ++i)
	{
		int idx = first_index + i;
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
	P_Menu_Close(ent);
	P_Menu_Open(ent, pmcallvotemenu_scorelimit, -1, sizeof(pmcallvotemenu_scorelimit) / sizeof(menu_t), nullptr, G_Menu_CallVote_ScoreLimit_Update);
}

void G_Menu_CallVote_ShuffleTeams(gentity_t *ent, menu_hnd_t *p)
{
	P_Menu_Close(ent);
	MenuVote_Initiate(ent, "shuffle", nullptr);
}

void G_Menu_CallVote_BalanceTeams(gentity_t *ent, menu_hnd_t *p)
{
	P_Menu_Close(ent);
	MenuVote_Initiate(ent, "balance", nullptr);
}

void G_Menu_CallVote_Cointoss(gentity_t *ent, menu_hnd_t *p)
{
	P_Menu_Close(ent);
	MenuVote_Initiate(ent, "cointoss", nullptr);
}

void G_Menu_CallVote_ReadyAll(gentity_t *ent, menu_hnd_t *p)
{
	P_Menu_Close(ent);
	MenuVote_Initiate(ent, "readyall", nullptr);
}

void G_Menu_CallVote_Update(gentity_t *ent)
{
	menu_t *entries = ent->client->menu->entries;

	for (int i = 0; i < 15; i++)
	{
		entries[i].SelectFunc = nullptr;
		entries[i].text[0] = '\0';
	}

	Q_strlcpy(entries[0].text, "Call a Vote", sizeof(entries[0].text));

	int gametype_index = cvmenu_map;
	entries[gametype_index].SelectFunc = G_Menu_CallVote_GameType;
	Q_strlcpy(entries[gametype_index].text, G_Fmt("Gametype: {}", level.gametype_name).data(), sizeof(entries[gametype_index].text));

	int ruleset_index = gametype_index + 1;
	entries[ruleset_index].SelectFunc = G_Menu_CallVote_Ruleset;
	Q_strlcpy(entries[ruleset_index].text, G_Fmt("Ruleset: {}", rs_long_name[(int)game.ruleset]).data(), sizeof(entries[ruleset_index].text));

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

	int friendlyfire_index = powerups_index + 1;
	if (GT(GT_TDM) || GT(GT_CTF))
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

	int readyall_index = shuffle_index + 1;
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

	for (int i = readyall_index + 1; i < 15; i++)
	{
		entries[i].SelectFunc = nullptr;
		entries[i].text[0] = '\0';
	}
}

void G_Menu_CallVote_Powerups_Update(gentity_t *ent)
{
	menu_t *entries = ent->client->menu->entries;
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

	int v = strtoul(value, nullptr, 10);
	bool currently_enabled = g_no_powerups->integer == 0;
	if (currently_enabled == (v == 1))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Powerups are already {}.\n", v ? "ENABLED" : "DISABLED");
		return;
	}

	MenuVote_Initiate(ent, "powerups", value);
}

void G_Menu_CallVote_Powerups(gentity_t *ent, menu_hnd_t *p)
{
	P_Menu_Close(ent);
	P_Menu_Open(ent, pmcallvotemenu_powerups, -1, sizeof(pmcallvotemenu_powerups) / sizeof(menu_t), nullptr, G_Menu_CallVote_Powerups_Update);
}

void G_Menu_CallVote_FriendlyFire_Update(gentity_t *ent)
{
	menu_t *entries = ent->client->menu->entries;
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

	int v = strtoul(value, nullptr, 10);
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
	P_Menu_Close(ent);
	P_Menu_Open(ent, pmcallvotemenu_friendlyfire, -1, sizeof(pmcallvotemenu_friendlyfire) / sizeof(menu_t), nullptr, G_Menu_CallVote_FriendlyFire_Update);
}

void G_Menu_Vote_Yes(gentity_t *ent, menu_hnd_t *p)
{
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
	int ci = ent->client ? (int)(ent->client - game.clients) : -1;
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

	menu_t *entries = ent->client->menu->entries;
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
	if (!ClientCanVote(ent->client))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "You are not allowed to call a vote as a spectator.\n");
		return;
	}

	P_Menu_Close(ent);
	P_Menu_Open(ent, pmcallvotemenu, -1, sizeof(pmcallvotemenu) / sizeof(menu_t), nullptr, G_Menu_CallVote_Update);
}

void G_Menu_ReturnToCallVote(gentity_t *ent, menu_hnd_t *p)
{
	P_Menu_Close(ent);
	P_Menu_Open(ent, pmcallvotemenu, -1, sizeof(pmcallvotemenu) / sizeof(menu_t), nullptr, G_Menu_CallVote_Update);
	gi.local_sound(ent, CHAN_AUTO, gi.soundindex("misc/menu3.wav"), 1, ATTN_NONE, 0);
}

void G_Menu_Vote_Open(gentity_t *ent)
{
	P_Menu_Open(ent, votemenu, -1, sizeof(votemenu) / sizeof(menu_t), nullptr, G_Menu_Vote_Update);
}
