// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "core/debug_log.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_maps.h"
#include "muffmode/mm_menu.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_vote.h"
#include "muffmode/mm_vote_menu.h"
#include "muffmode/mm_util.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace muffmode::vote_menu {
constexpr int cvmenu_map = 3;

void OpenMap(gentity_t *ent, menu_hnd_t *p);
void OpenGameType(gentity_t *ent, menu_hnd_t *p);
void UpdateGameType(gentity_t *ent);
void SelectGameType(gentity_t *ent, menu_hnd_t *p);
void OpenRuleset(gentity_t *ent, menu_hnd_t *p);
void UpdateRuleset(gentity_t *ent);
void SelectRuleset(gentity_t *ent, menu_hnd_t *p);
void UpdateTimeLimit(gentity_t *ent);
void OpenTimeLimit(gentity_t *ent, menu_hnd_t *p);
void SelectTimeLimit(gentity_t *ent, menu_hnd_t *p);
void UpdateScoreLimit(gentity_t *ent);
void OpenScoreLimit(gentity_t *ent, menu_hnd_t *p);
void SelectScoreLimit(gentity_t *ent, menu_hnd_t *p);
void StartShuffleTeamsVote(gentity_t *ent, menu_hnd_t *p);
void OpenPowerups(gentity_t *ent, menu_hnd_t *p);
void UpdatePowerups(gentity_t *ent);
void SelectPowerups(gentity_t *ent, menu_hnd_t *p);
void OpenTechs(gentity_t *ent, menu_hnd_t *p);
void UpdateTechs(gentity_t *ent);
void SelectTechs(gentity_t *ent, menu_hnd_t *p);
void OpenFriendlyFire(gentity_t *ent, menu_hnd_t *p);
void UpdateFriendlyFire(gentity_t *ent);
void SelectFriendlyFire(gentity_t *ent, menu_hnd_t *p);
void StartReadyAllVote(gentity_t *ent, menu_hnd_t *p);
void SelectMap(gentity_t *ent, menu_hnd_t *p);
void UpdateCallVote(gentity_t *ent);
void OpenCallVoteMenu(gentity_t *ent);
void MenuVote_Initiate(gentity_t *ent, const char *cmd_name, const char *arg);

void MenuVote_SetText(menu_t &entry, std::string_view text)
{
	P_Menu_SetText(&entry, text);
}

void MenuVote_SetArg(menu_t &entry, std::string_view arg)
{
	CopyString(entry.text_arg1, arg);
}

const menu_t kCallVoteMenuTemplate[] = {
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

const menu_t kMapMenuTemplate[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_LEFT, SelectMap },
	{ "", MENU_ALIGN_LEFT, SelectMap },
	{ "", MENU_ALIGN_LEFT, SelectMap },
	{ "", MENU_ALIGN_LEFT, SelectMap },
	{ "", MENU_ALIGN_LEFT, SelectMap },
	{ "", MENU_ALIGN_LEFT, SelectMap },
	{ "", MENU_ALIGN_LEFT, SelectMap },
	{ "", MENU_ALIGN_LEFT, SelectMap },
	{ "", MENU_ALIGN_LEFT, SelectMap },
	{ "", MENU_ALIGN_LEFT, SelectMap },
	{ "", MENU_ALIGN_LEFT, SelectMap },
	{ "", MENU_ALIGN_LEFT, SelectMap },
	{ "", MENU_ALIGN_LEFT, SelectMap },
	{ "", MENU_ALIGN_LEFT, SelectMap },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "$g_pc_return", MENU_ALIGN_LEFT, G_Menu_ReturnToCallVote }
};

const menu_t kGameTypeMenuTemplate[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_LEFT, SelectGameType },
	{ "", MENU_ALIGN_LEFT, SelectGameType },
	{ "", MENU_ALIGN_LEFT, SelectGameType },
	{ "", MENU_ALIGN_LEFT, SelectGameType },
	{ "", MENU_ALIGN_LEFT, SelectGameType },
	{ "", MENU_ALIGN_LEFT, SelectGameType },
	{ "", MENU_ALIGN_LEFT, SelectGameType },
	{ "", MENU_ALIGN_LEFT, SelectGameType },
	{ "", MENU_ALIGN_LEFT, SelectGameType },
	{ "", MENU_ALIGN_LEFT, SelectGameType },
	{ "", MENU_ALIGN_LEFT, SelectGameType },
	{ "", MENU_ALIGN_LEFT, SelectGameType },
	{ "", MENU_ALIGN_LEFT, SelectGameType },
	{ "", MENU_ALIGN_LEFT, SelectGameType },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "$g_pc_return", MENU_ALIGN_LEFT, G_Menu_ReturnToCallVote }
};

const menu_t kRulesetMenuTemplate[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_LEFT, SelectRuleset },
	{ "", MENU_ALIGN_LEFT, SelectRuleset },
	{ "", MENU_ALIGN_LEFT, SelectRuleset },
	{ "", MENU_ALIGN_LEFT, SelectRuleset },
	{ "", MENU_ALIGN_LEFT, SelectRuleset },
	{ "", MENU_ALIGN_LEFT, SelectRuleset },
	{ "", MENU_ALIGN_LEFT, SelectRuleset },
	{ "", MENU_ALIGN_LEFT, SelectRuleset },
	{ "", MENU_ALIGN_LEFT, SelectRuleset },
	{ "", MENU_ALIGN_LEFT, SelectRuleset },
	{ "", MENU_ALIGN_LEFT, SelectRuleset },
	{ "", MENU_ALIGN_LEFT, SelectRuleset },
	{ "", MENU_ALIGN_LEFT, SelectRuleset },
	{ "", MENU_ALIGN_LEFT, SelectRuleset },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "$g_pc_return", MENU_ALIGN_LEFT, G_Menu_ReturnToCallVote }
};

const menu_t kPowerupsMenuTemplate[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "ON",  MENU_ALIGN_LEFT, SelectPowerups },
	{ "OFF", MENU_ALIGN_LEFT, SelectPowerups },
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

const menu_t kTechsMenuTemplate[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "ON",  MENU_ALIGN_LEFT, SelectTechs },
	{ "OFF", MENU_ALIGN_LEFT, SelectTechs },
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

const menu_t kFriendlyFireMenuTemplate[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "ON",  MENU_ALIGN_LEFT, SelectFriendlyFire },
	{ "OFF", MENU_ALIGN_LEFT, SelectFriendlyFire },
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

const menu_t kTimeLimitMenuTemplate[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "0",  MENU_ALIGN_LEFT, SelectTimeLimit },
	{ "5",  MENU_ALIGN_LEFT, SelectTimeLimit },
	{ "10", MENU_ALIGN_LEFT, SelectTimeLimit },
	{ "15", MENU_ALIGN_LEFT, SelectTimeLimit },
	{ "20", MENU_ALIGN_LEFT, SelectTimeLimit },
	{ "25", MENU_ALIGN_LEFT, SelectTimeLimit },
	{ "30", MENU_ALIGN_LEFT, SelectTimeLimit },
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

const menu_t kScoreLimitMenuTemplate[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "0", MENU_ALIGN_LEFT, SelectScoreLimit },
	{ "5", MENU_ALIGN_LEFT, SelectScoreLimit },
	{ "10", MENU_ALIGN_LEFT, SelectScoreLimit },
	{ "20", MENU_ALIGN_LEFT, SelectScoreLimit },
	{ "30", MENU_ALIGN_LEFT, SelectScoreLimit },
	{ "40", MENU_ALIGN_LEFT, SelectScoreLimit },
	{ "50", MENU_ALIGN_LEFT, SelectScoreLimit },
	{ "100", MENU_ALIGN_LEFT, SelectScoreLimit },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "$g_pc_return", MENU_ALIGN_LEFT, G_Menu_ReturnToCallVote }
};

static_assert(std::size(kCallVoteMenuTemplate) == MENU_MAX_ROWS);
static_assert(std::size(kMapMenuTemplate) == MENU_MAX_ROWS);
static_assert(std::size(kGameTypeMenuTemplate) == MENU_MAX_ROWS);
static_assert(std::size(kRulesetMenuTemplate) == MENU_MAX_ROWS);
static_assert(std::size(kPowerupsMenuTemplate) == MENU_MAX_ROWS);
static_assert(std::size(kTechsMenuTemplate) == MENU_MAX_ROWS);
static_assert(std::size(kFriendlyFireMenuTemplate) == MENU_MAX_ROWS);
static_assert(std::size(kTimeLimitMenuTemplate) == MENU_MAX_ROWS);
static_assert(std::size(kScoreLimitMenuTemplate) == MENU_MAX_ROWS);

std::optional<std::string> MenuVote_ReadSelection(gentity_t *ent, menu_hnd_t *p)
{
	if (!ent || !ent->client)
		return std::nullopt;

	if (!p || !p->entries || p->cur < 0 || p->cur >= p->num)
		return std::nullopt;

	UpdateFunc_t saved = p->UpdateFunc;
	p->UpdateFunc = nullptr;
	const menu_t &selection = p->entries[p->cur];
	size_t value_length = 0;
	while (value_length < sizeof(selection.text_arg1) &&
		selection.text_arg1[value_length])
		value_length++;
	std::string value(selection.text_arg1, value_length);
	p->UpdateFunc = saved;

	if (value.empty())
		return std::nullopt;

	P_Menu_Close(ent);
	return value;
}

void MenuVote_InitiateSelection(gentity_t *ent, menu_hnd_t *p, const char *cmd_name)
{
	const auto value = MenuVote_ReadSelection(ent, p);
	if (!value)
		return;

	MenuVote_Initiate(ent, cmd_name, value->c_str());
}

struct MenuVoteView {
	menu_t *entries = nullptr;
	int num = 0;
};

bool MenuVote_View(gentity_t *ent, MenuVoteView &view)
{
	if (!ent || !ent->client || !ent->client->menu || !ent->client->menu->entries || ent->client->menu->num <= 0)
		return false;

	view.entries = ent->client->menu->entries;
	view.num = ent->client->menu->num;
	return true;
}

menu_t *MenuVote_Entries(gentity_t *ent, int *num = nullptr)
{
	MenuVoteView view;
	if (!MenuVote_View(ent, view))
		return nullptr;

	if (num)
		*num = view.num;
	return view.entries;
}

bool MenuVote_HasIndex(const MenuVoteView &view, int index)
{
	return index >= 0 && index < view.num;
}

void MenuVote_ClearEntry(menu_t &entry)
{
	MenuVote_SetText(entry, "");
	MenuVote_SetArg(entry, "");
	entry.SelectFunc = nullptr;
}

void MenuVote_ClearRange(const MenuVoteView &view, int first, int last_exclusive)
{
	const int begin = std::clamp(first, 0, view.num);
	const int end = std::clamp(last_exclusive, 0, view.num);
	for (int i = begin; i < end; ++i)
		MenuVote_ClearEntry(view.entries[i]);
}

void MenuVote_SetToggleChoices(menu_t *entries, std::string_view title)
{
	MenuVote_SetText(entries[0], title);
	MenuVote_SetText(entries[2], "ON");
	MenuVote_SetArg(entries[2], "1");
	MenuVote_SetText(entries[3], "OFF");
	MenuVote_SetArg(entries[3], "0");
}

struct ToggleVoteOption {
	const char *title;
	const char *command;
	const char *invalid_selection_message;
	const char *already_selected_message;
	bool (*is_enabled)();
};

bool PowerupsEnabled()
{
	return muffmode::CvarInteger(g_no_powerups) == 0;
}

bool TechsEnabled()
{
	return AllowTechs();
}

bool FriendlyFireEnabled()
{
	return muffmode::CvarEnabled(g_friendly_fire);
}

const ToggleVoteOption kPowerupsVote {
	"Powerups",
	"powerups",
	"Invalid powerups selection.\n",
	"Powerups are already {}.\n",
	PowerupsEnabled
};

const ToggleVoteOption kTechsVote {
	"Techs",
	"techs",
	"Invalid techs selection.\n",
	"Techs are already {}.\n",
	TechsEnabled
};

const ToggleVoteOption kFriendlyFireVote {
	"Friendly Fire",
	"friendlyfire",
	"Invalid friendly fire selection.\n",
	"Friendly fire is already {}.\n",
	FriendlyFireEnabled
};

void UpdateToggleVoteMenu(gentity_t *ent, const ToggleVoteOption &option)
{
	MenuVoteView view;
	if (!MenuVote_View(ent, view) || !MenuVote_HasIndex(view, 3))
		return;

	MenuVote_SetToggleChoices(view.entries, option.title);
}

void SelectToggleVote(gentity_t *ent, menu_hnd_t *p, const ToggleVoteOption &option)
{
	const auto value = MenuVote_ReadSelection(ent, p);
	if (!value)
		return;

	const auto parsed = MM_ParseNonNegativeIntArg(value->c_str());
	if (!parsed || (*parsed != 0 && *parsed != 1)) {
		gi.LocClient_Print(ent, PRINT_HIGH, option.invalid_selection_message);
		return;
	}

	const bool requested_enabled = *parsed == 1;
	if (option.is_enabled && option.is_enabled() == requested_enabled) {
		gi.LocClient_Print(ent, PRINT_HIGH, option.already_selected_message, requested_enabled ? "ENABLED" : "DISABLED");
		return;
	}

	MenuVote_Initiate(ent, option.command, value->c_str());
}

struct FixedChoiceVoteOption {
	const char *title;
	const char *command;
};

int MenuVote_ContentLimit(const MenuVoteView &view);

constexpr int kFixedChoiceFirstIndex = 2;

const FixedChoiceVoteOption kTimeLimitVote {
	"Select Time Limit (mins)",
	"timelimit"
};

const FixedChoiceVoteOption kScoreLimitVote {
	"Select Score Limit",
	"scorelimit"
};

template <size_t N>
void UpdateFixedChoiceVoteMenu(gentity_t *ent, const FixedChoiceVoteOption &option, const std::array<const char *, N> &values)
{
	MenuVoteView view;
	if (!MenuVote_View(ent, view) || !MenuVote_HasIndex(view, 0))
		return;

	menu_t *entries = view.entries;
	const int content_limit = MenuVote_ContentLimit(view);
	MenuVote_SetText(entries[0], option.title);

	for (int i = 0; i < static_cast<int>(values.size()); ++i) {
		const int index = kFixedChoiceFirstIndex + i;
		if (index >= content_limit)
			break;
		MenuVote_SetText(entries[index], values[i]);
		MenuVote_SetArg(entries[index], values[i]);
	}
}

void SelectFixedChoiceVote(gentity_t *ent, menu_hnd_t *p, const FixedChoiceVoteOption &option)
{
	MenuVote_InitiateSelection(ent, p, option.command);
}

int MenuVote_ContentLimit(const MenuVoteView &view)
{
	// Reserve the final row for "return" entries in these menu definitions.
	return max(0, view.num - 1);
}

menu_hnd_t *MenuVote_OpenMenu(gentity_t *ent, const menu_t *entries, int num, void *arg, UpdateFunc_t update)
{
	if (!ent || !ent->client || !entries || num <= 0)
		return nullptr;

	P_Menu_Close(ent);
	return P_Menu_Open(ent, entries, -1, num, arg, update);
}

const char *CurrentRulesetName()
{
	const int ruleset = clamp((int)game.ruleset, (int)RS_NONE + 1, (int)RS_NUM_RULESETS - 1);
	return rs_long_name[ruleset];
}

void MenuVote_Initiate(gentity_t *ent, const char *cmd_name, const char *arg)
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

void MenuVote_CloseAndInitiate(gentity_t *ent, const char *cmd_name, const char *arg)
{
	if (!ent || !ent->client)
		return;

	P_Menu_Close(ent);
	MenuVote_Initiate(ent, cmd_name, arg);
}

struct MapMenuPage {
	int offset = 0;
};

struct MapMenuSnapshot {
	bool initialized = false;
	MapMenuSourceRevision revision;
	std::vector<std::string> values;
};

// Every menu copies these owned strings into its rows. Sharing the source
// snapshot avoids repeating map parsing, case-folded deduplication and sorting.
static MapMenuSnapshot s_map_menu_snapshot;

constexpr int kMapsPerPage = 12;
constexpr int kMapMenuFirstItem = 2;

static MapMenuSourceRevision CurrentMapMenuSourceRevision()
{
	return {
		g_map_list,
		g_map_list ? g_map_list->modified_count : 0,
		g_map_pool,
		g_map_pool ? g_map_pool->modified_count : 0
	};
}

static const std::vector<std::string> &MapMenuValues()
{
	const MapMenuSourceRevision current = CurrentMapMenuSourceRevision();
	if (!MapMenuSnapshotNeedsRefresh(
			s_map_menu_snapshot.initialized,
			s_map_menu_snapshot.revision,
			current)) {
		return s_map_menu_snapshot.values;
	}

	std::vector<std::string> values = muffmode::maps::CollectConfiguredMaps();
	if (values.size() > 1)
		std::sort(values.begin(), values.end(), muffmode::CStringLessI);

	s_map_menu_snapshot.values = std::move(values);
	s_map_menu_snapshot.revision = current;
	s_map_menu_snapshot.initialized = true;
	return s_map_menu_snapshot.values;
}

int MapPageSize(int menu_entries)
{
	const int content_limit = max(0, menu_entries - 1);
	const int prev_index = content_limit - 3;
	return min(kMapsPerPage, max(0, prev_index - kMapMenuFirstItem));
}

int MapPrevIndex(int menu_entries)
{
	return max(kMapMenuFirstItem, max(0, menu_entries - 1) - 3);
}

int MapNextIndex(int menu_entries)
{
	return max(kMapMenuFirstItem, max(0, menu_entries - 1) - 2);
}

int ClampMapPageOffset(int offset, int total_maps, int page_size)
{
	if (offset < 0)
		return 0;
	if (offset < total_maps)
		return offset;
	return total_maps > 0 ? ((total_maps - 1) / page_size) * page_size : 0;
}

void SelectMap(gentity_t *ent, menu_hnd_t *p)
{
	const auto value = MenuVote_ReadSelection(ent, p);
	if (!value)
		return;

	if (!MM_IsSafeMapToken(value->c_str()))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid characters in map name.\n");
		return;
	}

	MenuVote_Initiate(ent, "map", value->c_str());
}

void PreviousMapPage(gentity_t *ent, menu_hnd_t *p)
{
	if (!ent || !ent->client || !p)
		return;

	const int page_size = MapPageSize(p->num);
	if (page_size <= 0)
		return;

	MapMenuPage *page = static_cast<MapMenuPage *>(p->arg);
	if (page && page->offset > 0)
	{
		page->offset -= page_size;
		page->offset = max(0, page->offset);
	}
	P_Menu_Update(ent);
}

void NextMapPage(gentity_t *ent, menu_hnd_t *p)
{
	if (!ent || !ent->client || !p)
		return;

	const int page_size = MapPageSize(p->num);
	if (page_size <= 0)
		return;

	MapMenuPage *page = static_cast<MapMenuPage *>(p->arg);
	if (page)
		page->offset += page_size;
	P_Menu_Update(ent);
}

void UpdateMap(gentity_t *ent)
{
	MenuVoteView view;
	if (!MenuVote_View(ent, view) || !MenuVote_HasIndex(view, 0) || !MenuVote_HasIndex(view, 1))
		return;

	menu_t *entries = view.entries;
	const int page_size = MapPageSize(view.num);
	const int prev_index = MapPrevIndex(view.num);
	const int next_index = MapNextIndex(view.num);
	const int content_limit = MenuVote_ContentLimit(view);
	if (page_size <= 0 || prev_index >= content_limit || next_index >= content_limit)
		return;

	MenuVote_ClearEntry(entries[1]);

	const std::vector<std::string> &values = MapMenuValues();

	MapMenuPage *page = static_cast<MapMenuPage *>(ent->client->menu->arg);
	int offset = page ? page->offset : 0;
	const int total_maps = static_cast<int>(values.size());

	offset = ClampMapPageOffset(offset, total_maps, page_size);
	if (page)
		page->offset = offset;

	int total_pages = (total_maps + page_size - 1) / page_size;
	if (total_pages < 1)
		total_pages = 1;
	const int current_page = (offset / page_size) + 1;

	if (total_pages > 1)
		MenuVote_SetText(entries[0], G_Fmt("Choose a Map ({}/{})", current_page, total_pages).data());
	else
		MenuVote_SetText(entries[0], "Choose a Map");

	MenuVote_ClearRange(view, kMapMenuFirstItem, content_limit);

	int menu_index = kMapMenuFirstItem;
	for (int i = offset; i < total_maps && menu_index < (kMapMenuFirstItem + page_size); i++)
	{
		MenuVote_SetArg(entries[menu_index], values[i]);
		MenuVote_SetText(entries[menu_index], values[i]);
		entries[menu_index].SelectFunc = SelectMap;
		menu_index++;
	}

	if (offset > 0)
	{
		MenuVote_SetText(entries[prev_index], "< Prev Page");
		entries[prev_index].SelectFunc = PreviousMapPage;
	}

	if (offset + page_size < total_maps)
	{
		MenuVote_SetText(entries[next_index], "> Next Page");
		entries[next_index].SelectFunc = NextMapPage;
	}
}

void OpenMap(gentity_t *ent, menu_hnd_t *)
{
	if (!ent || !ent->client)
		return;

	MapMenuPage *page = static_cast<MapMenuPage *>(gi.TagMalloc(sizeof(MapMenuPage), TAG_LEVEL));
	if (!page)
		return;

	*page = {};
	if (!MenuVote_OpenMenu(ent, kMapMenuTemplate,
		muffmode::CountAsInt(kMapMenuTemplate), page, UpdateMap))
		gi.TagFree(page);
}

void UpdateGameType(gentity_t *ent)
{
	MenuVoteView view;
	if (!MenuVote_View(ent, view) || !MenuVote_HasIndex(view, 0))
		return;

	menu_t *entries = view.entries;
	const int content_limit = MenuVote_ContentLimit(view);
	if (content_limit <= kFixedChoiceFirstIndex)
		return;

	MenuVote_SetText(entries[0], "Select Gametype");

	MenuVote_ClearRange(view, kFixedChoiceFirstIndex, content_limit);

	int menu_index = kFixedChoiceFirstIndex;
	for (int i = static_cast<int>(GT_FIRST); i <= static_cast<int>(GT_LAST) && menu_index < content_limit; i++)
	{
		const gametype_t gt = static_cast<gametype_t>(i);
		if (gt == GT_NONE)
			continue;
		if (!MM_IsGametypeVotable(gt))
			continue;

		MenuVote_SetArg(entries[menu_index], gt_short_name[i]);
		MenuVote_SetText(entries[menu_index], gt_long_name[i]);
		entries[menu_index].SelectFunc = SelectGameType;
		menu_index++;
	}
}

void SelectGameType(gentity_t *ent, menu_hnd_t *p)
{
	const auto value = MenuVote_ReadSelection(ent, p);
	if (!value)
		return;

	const gametype_t gt = GT_IndexFromString(value->c_str());
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

	MenuVote_Initiate(ent, "gametype", value->c_str());
}

void OpenGameType(gentity_t *ent, menu_hnd_t *)
{
	MenuVote_OpenMenu(ent, kGameTypeMenuTemplate, muffmode::CountAsInt(kGameTypeMenuTemplate), nullptr, UpdateGameType);
}

void UpdateRuleset(gentity_t *ent)
{
	MenuVoteView view;
	if (!MenuVote_View(ent, view) || !MenuVote_HasIndex(view, 0))
		return;

	menu_t *entries = view.entries;
	const int content_limit = MenuVote_ContentLimit(view);
	if (content_limit <= kFixedChoiceFirstIndex)
		return;

	MenuVote_SetText(entries[0], "Select Ruleset");

	MenuVote_ClearRange(view, kFixedChoiceFirstIndex, content_limit);

	int menu_index = kFixedChoiceFirstIndex;
	for (int i = static_cast<int>(RS_NONE) + 1; i < static_cast<int>(RS_NUM_RULESETS) && menu_index < content_limit; i++)
	{
		const ruleset_t rs = static_cast<ruleset_t>(i);
		if (rs == RS_NONE)
			continue;
		if (!MM_IsRulesetVotable(rs))
			continue;

		MenuVote_SetArg(entries[menu_index], rs_short_name[i]);
		MenuVote_SetText(entries[menu_index], rs_long_name[i]);
		entries[menu_index].SelectFunc = SelectRuleset;
		menu_index++;
	}
}

void SelectRuleset(gentity_t *ent, menu_hnd_t *p)
{
	const auto value = MenuVote_ReadSelection(ent, p);
	if (!value)
		return;

	const ruleset_t rs = RS_IndexFromString(value->c_str());
	if (rs == ruleset_t::RS_NONE)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid ruleset selected.\n");
		return;
	}
	if (static_cast<int>(rs) == game.ruleset)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Ruleset is already active.\n");
		return;
	}

	MenuVote_Initiate(ent, "ruleset", value->c_str());
}

void OpenRuleset(gentity_t *ent, menu_hnd_t *)
{
	MenuVote_OpenMenu(ent, kRulesetMenuTemplate, muffmode::CountAsInt(kRulesetMenuTemplate), nullptr, UpdateRuleset);
}

void UpdateTimeLimit(gentity_t *ent)
{
	static constexpr std::array<const char *, 7> time_values = { "0", "5", "10", "15", "20", "25", "30" };
	UpdateFixedChoiceVoteMenu(ent, kTimeLimitVote, time_values);
}

void SelectTimeLimit(gentity_t *ent, menu_hnd_t *p)
{
	SelectFixedChoiceVote(ent, p, kTimeLimitVote);
}

void OpenTimeLimit(gentity_t *ent, menu_hnd_t *)
{
	MenuVote_OpenMenu(ent, kTimeLimitMenuTemplate, muffmode::CountAsInt(kTimeLimitMenuTemplate), nullptr, UpdateTimeLimit);
}

void UpdateScoreLimit(gentity_t *ent)
{
	static constexpr std::array<const char *, 8> score_values = { "0", "5", "10", "20", "30", "40", "50", "100" };
	UpdateFixedChoiceVoteMenu(ent, kScoreLimitVote, score_values);
}

void SelectScoreLimit(gentity_t *ent, menu_hnd_t *p)
{
	SelectFixedChoiceVote(ent, p, kScoreLimitVote);
}

void OpenScoreLimit(gentity_t *ent, menu_hnd_t *)
{
	MenuVote_OpenMenu(ent, kScoreLimitMenuTemplate, muffmode::CountAsInt(kScoreLimitMenuTemplate), nullptr, UpdateScoreLimit);
}

void StartShuffleTeamsVote(gentity_t *ent, menu_hnd_t *)
{
	MenuVote_CloseAndInitiate(ent, "shuffle", nullptr);
}

void StartReadyAllVote(gentity_t *ent, menu_hnd_t *)
{
	MenuVote_CloseAndInitiate(ent, "readyall", nullptr);
}

void OpenCallVoteMenu(gentity_t *ent)
{
	MenuVote_OpenMenu(ent, kCallVoteMenuTemplate, muffmode::CountAsInt(kCallVoteMenuTemplate), nullptr, UpdateCallVote);
}

void UpdateCallVote(gentity_t *ent)
{
	MenuVoteView view;
	if (!MenuVote_View(ent, view))
		return;

	menu_t *entries = view.entries;
	const int content_limit = MenuVote_ContentLimit(view);
	const int readyall_index = 14;
	if (content_limit <= readyall_index)
		return;

	MenuVote_ClearRange(view, 0, content_limit);

	MenuVote_SetText(entries[0], "Call a Vote");

	int gametype_index = cvmenu_map;
	entries[gametype_index].SelectFunc = OpenGameType;
	MenuVote_SetText(entries[gametype_index], G_Fmt("Gametype: {}", level.gametype_name).data());

	int ruleset_index = gametype_index + 1;
	entries[ruleset_index].SelectFunc = OpenRuleset;
	MenuVote_SetText(entries[ruleset_index], G_Fmt("Ruleset: {}", CurrentRulesetName()).data());

	int map_index = ruleset_index + 1;
	entries[map_index].SelectFunc = OpenMap;
	if (level.mapname[0])
		MenuVote_SetText(entries[map_index], G_Fmt("Map:\t\t {}", level.mapname).data());
	else
		MenuVote_SetText(entries[map_index], "Map");

	int blank1_index = map_index + 1;
	entries[blank1_index].SelectFunc = nullptr;
	MenuVote_SetText(entries[blank1_index], "");

	int scorelimit_index = blank1_index + 1;
	entries[scorelimit_index].SelectFunc = OpenScoreLimit;
	int current_scorelimit = GT_ScoreLimit();
	if (current_scorelimit > 0)
		MenuVote_SetText(entries[scorelimit_index], G_Fmt("Scorelimit: {}", current_scorelimit).data());
	else
		MenuVote_SetText(entries[scorelimit_index], "Scorelimit: 0");

	int timelimit_index = scorelimit_index + 1;
	entries[timelimit_index].SelectFunc = OpenTimeLimit;
	int current_timelimit = (int)timelimit->value;
	if (current_timelimit > 0)
		MenuVote_SetText(entries[timelimit_index], G_Fmt("Timelimit: {} min", current_timelimit).data());
	else
		MenuVote_SetText(entries[timelimit_index], "Timelimit: 0");

	int blank2_index = timelimit_index + 1;
	entries[blank2_index].SelectFunc = nullptr;
	MenuVote_SetText(entries[blank2_index], "");

	int powerups_index = blank2_index + 1;
	entries[powerups_index].SelectFunc = OpenPowerups;
	MenuVote_SetText(entries[powerups_index], G_Fmt("Powerups: {}", PowerupsEnabled() ? "ON" : "OFF").data());

	int techs_index = powerups_index + 1;
	if (GT(GT_FFA) || GT(GT_TDM) || GT(GT_CTF) || GT(GT_HORDE))
	{
		entries[techs_index].SelectFunc = OpenTechs;
		MenuVote_SetText(entries[techs_index], G_Fmt("Techs: {}", TechsEnabled() ? "ON" : "OFF").data());
	}
	else
	{
		entries[techs_index].SelectFunc = nullptr;
		MenuVote_SetText(entries[techs_index], "Techs: N/A");
	}

	int friendlyfire_index = techs_index + 1;
	if (Teams())
	{
		entries[friendlyfire_index].SelectFunc = OpenFriendlyFire;
		MenuVote_SetText(entries[friendlyfire_index], G_Fmt("Friendly Fire: {}", FriendlyFireEnabled() ? "ON" : "OFF").data());
	}
	else
	{
		entries[friendlyfire_index].SelectFunc = nullptr;
		MenuVote_SetText(entries[friendlyfire_index], "Friendly Fire: N/A");
	}

	int shuffle_index = friendlyfire_index + 1;
	if (Teams())
	{
		entries[shuffle_index].SelectFunc = StartShuffleTeamsVote;
		MenuVote_SetText(entries[shuffle_index], "Shuffle Teams");
	}
	else
	{
		entries[shuffle_index].SelectFunc = nullptr;
		MenuVote_SetText(entries[shuffle_index], "Shuffle Teams: N/A");
	}

	if (g_dm_do_readyup->integer && level.match_state == matchst_t::MATCH_WARMUP_READYUP)
	{
		entries[readyall_index].SelectFunc = StartReadyAllVote;
		MenuVote_SetText(entries[readyall_index], "Ready All");
	}
	else
	{
		entries[readyall_index].SelectFunc = nullptr;
		MenuVote_SetText(entries[readyall_index], "Ready All: N/A");
	}

	MenuVote_ClearRange(view, readyall_index + 1, content_limit);
}

void UpdatePowerups(gentity_t *ent)
{
	UpdateToggleVoteMenu(ent, kPowerupsVote);
}

void SelectPowerups(gentity_t *ent, menu_hnd_t *p)
{
	SelectToggleVote(ent, p, kPowerupsVote);
}

void UpdateTechs(gentity_t *ent)
{
	UpdateToggleVoteMenu(ent, kTechsVote);
}

void SelectTechs(gentity_t *ent, menu_hnd_t *p)
{
	SelectToggleVote(ent, p, kTechsVote);
}

void OpenTechs(gentity_t *ent, menu_hnd_t *)
{
	MenuVote_OpenMenu(ent, kTechsMenuTemplate, muffmode::CountAsInt(kTechsMenuTemplate), nullptr, UpdateTechs);
}

void OpenPowerups(gentity_t *ent, menu_hnd_t *)
{
	MenuVote_OpenMenu(ent, kPowerupsMenuTemplate, muffmode::CountAsInt(kPowerupsMenuTemplate), nullptr, UpdatePowerups);
}

void UpdateFriendlyFire(gentity_t *ent)
{
	UpdateToggleVoteMenu(ent, kFriendlyFireVote);
}

void SelectFriendlyFire(gentity_t *ent, menu_hnd_t *p)
{
	SelectToggleVote(ent, p, kFriendlyFireVote);
}

void OpenFriendlyFire(gentity_t *ent, menu_hnd_t *)
{
	MenuVote_OpenMenu(ent, kFriendlyFireMenuTemplate, muffmode::CountAsInt(kFriendlyFireMenuTemplate), nullptr, UpdateFriendlyFire);
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
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "30", MENU_ALIGN_CENTER, nullptr }
};

static_assert(std::size(votemenu) == MENU_MAX_ROWS);
constexpr int kVoteTimeoutRow = MENU_MAX_ROWS - 1;

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
	if (!entries || num_entries <= kVoteTimeoutRow)
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
	MenuVote_SetText(entries[i], G_Fmt("{} called a vote:", level.vote_state.caller->resp.netname).data());

	i = 4;
	if (!level.vote_state.command)
	{
		P_Menu_Close(ent);
		return;
	}
	MuffModeLog("DEBUG", "G_Menu_Vote_Update: writing command '%s' arg '%s' (arg_ptr=%p)",
		level.vote_state.command->name, level.vote_state.arg.c_str(), (void *)level.vote_state.arg.c_str());
	MenuVote_SetText(entries[i], G_Fmt("{} {}", level.vote_state.command->name, level.vote_state.arg).data());

	if (level.vote_state.start_time + 3_sec > level.time)
	{
		i = 7;
		MenuVote_SetText(entries[i], "GET READY TO VOTE!");
		entries[i].SelectFunc = nullptr;

		i = 8;
		int time = 3 - (level.time - level.vote_state.start_time).seconds<int>();
		MenuVote_SetText(entries[i], G_Fmt("{}...", time).data());
		entries[i].SelectFunc = nullptr;
		return;
	}

	i = 7;
	MenuVote_SetText(entries[i], "[ YES ]");
	entries[i].SelectFunc = G_Menu_Vote_Yes;
	i = 8;
	MenuVote_SetText(entries[i], "[ NO ]");
	entries[i].SelectFunc = G_Menu_Vote_No;

	i = kVoteTimeoutRow;
	MenuVote_SetText(entries[i], G_Fmt("{}", timeout).data());
}
} // namespace muffmode::vote_menu

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

	muffmode::vote_menu::OpenCallVoteMenu(ent);
}

void G_Menu_ReturnToCallVote(gentity_t *ent, menu_hnd_t *p)
{
	if (!ent || !ent->client)
		return;

	muffmode::vote_menu::OpenCallVoteMenu(ent);
	gi.local_sound(ent, CHAN_AUTO, gi.soundindex("misc/menu3.wav"), 1, ATTN_NONE, 0);
}

void G_Menu_Vote_Open(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	P_Menu_Open(ent, muffmode::vote_menu::votemenu, -1, muffmode::CountAsInt(muffmode::vote_menu::votemenu),
		nullptr, muffmode::vote_menu::G_Menu_Vote_Update);
}
