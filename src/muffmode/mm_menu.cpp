// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include "g_local.h"
#include "muffmode/mm_captain.h"
#include "muffmode/mm_match.h"
#include "muffmode/mm_menu.h"
#include "muffmode/mm_team.h"
#include "muffmode/mm_vote_menu.h"
#include "monsters/m_player.h"

#include <assert.h>
#include <string>

constexpr const char *BREAKER = "\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37";

static bool G_Menu_HasClient(gentity_t *ent)
{
	return ent && ent->client;
}

static bool G_Menu_GetEntries(gentity_t *ent, menu_t **entries, int *num = nullptr)
{
	if (!G_Menu_HasClient(ent) || !ent->client->menu || !ent->client->menu->entries || ent->client->menu->num <= 0)
		return false;

	if (entries)
		*entries = ent->client->menu->entries;
	if (num)
		*num = ent->client->menu->num;
	return true;
}

static int G_Menu_ContentLimit(int num_entries)
{
	return max(0, num_entries - 2);
}

static int G_Menu_CvarInteger(cvar_t *cvar)
{
	return cvar ? cvar->integer : 0;
}

static const char *G_Menu_CvarString(cvar_t *cvar)
{
	return (cvar && cvar->string) ? cvar->string : "";
}

static bool G_Menu_CvarEnabled(cvar_t *cvar)
{
	return G_Menu_CvarInteger(cvar) != 0;
}

static void G_Menu_SetText(menu_t *p, const char *text)
{
	if (!p)
		return;

	Q_strlcpy(p->text, text ? text : "", sizeof(p->text));
}

static bool G_Menu_CopyHostPlayerName(char *value, size_t value_size)
{
	if (!value || !value_size)
		return false;

	value[0] = '\0';
	if (!g_entities || game.maxclients <= 0 || !g_entities[1].client)
		return false;

	gi.Info_ValueForKey(g_entities[1].client->pers.userinfo, "name", value, value_size);
	return value[0] != '\0';
}

static int G_Menu_NormalizeTimelimit(int minutes)
{
	return clamp(minutes, 5, 60);
}

static void G_Menu_SetHostName(menu_t *p) {
	const char *name = G_Menu_CvarString(hostname);
	G_Menu_SetText(p, name[0] ? name : "MuffMode Server");
}

static void G_Menu_SetGamemodName(menu_t *p) {
	G_Menu_SetText(p, level.gamemod_name);
}

static void G_Menu_SetGametypeName(menu_t *p) {
	G_Menu_SetText(p, level.gametype_name);
}

static void G_Menu_SetLevelName(menu_t *p) {
	static char levelname[33];

	levelname[0] = '*';
	if (g_entities[0].message)
		Q_strlcpy(levelname + 1, g_entities[0].message, sizeof(levelname) - 1);
	else
		Q_strlcpy(levelname + 1, level.mapname, sizeof(levelname) - 1);
	levelname[sizeof(levelname) - 1] = 0;
	G_Menu_SetText(p, levelname);
}

static const char *G_Menu_CurrentRulesetName()
{
	const int ruleset = clamp((int)game.ruleset, (int)RS_NONE + 1, (int)RS_NUM_RULESETS - 1);
	return rs_long_name[ruleset];
}

/*----------------------------------------------------------------------------------*/
/* ADMIN */

void G_Menu_ReturnToMain(gentity_t *ent, menu_hnd_t *p);

struct admin_settings_t {
	int	 timelimit;
	bool weaponsstay;
	bool instantitems;
	bool pu_drop;
	bool instantweap;
	bool match_lock;
};

void G_Menu_Admin_UpdateSettings(gentity_t *ent, menu_hnd_t *setmenu);
void G_Menu_Admin(gentity_t *ent, menu_hnd_t *p);

static void G_Menu_Admin_SettingsApply(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent) || !ent->client->sess.admin || !p || !p->arg)
		return;

	admin_settings_t *settings = (admin_settings_t *)p->arg;
	settings->timelimit = G_Menu_NormalizeTimelimit(settings->timelimit);

	if (settings->timelimit != G_Menu_CvarInteger(timelimit)) {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} changed the timelimit to {} minutes.\n",
			ent->client->resp.netname, settings->timelimit);

		const std::string value = fmt::format("{}", settings->timelimit);
		gi.cvar_set("timelimit", value.c_str());
	}

	if (settings->weaponsstay != G_Menu_CvarEnabled(g_dm_weapons_stay)) {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} turned {} weapons stay.\n",
			ent->client->resp.netname, settings->weaponsstay ? "on" : "off");
		gi.cvar_set("g_dm_weapons_stay", settings->weaponsstay ? "1" : "0");
	}

	if (settings->instantitems != G_Menu_CvarEnabled(g_dm_instant_items)) {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} turned {} instant items.\n",
			ent->client->resp.netname, settings->instantitems ? "on" : "off");
		gi.cvar_set("g_dm_instant_items", settings->instantitems ? "1" : "0");
	}

	if (settings->pu_drop != G_Menu_CvarEnabled(g_dm_powerup_drop)) {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} turned {} powerup dropping.\n",
			ent->client->resp.netname, settings->pu_drop ? "on" : "off");
		gi.cvar_set("g_dm_powerup_drop", settings->pu_drop ? "1" : "0");
	}

	if (settings->instantweap != (G_Menu_CvarEnabled(g_instant_weapon_switch) || G_Menu_CvarEnabled(g_frenzy))) {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} turned {} instant weapon switch.\n",
			ent->client->resp.netname, settings->instantweap ? "on" : "off");
		gi.cvar_set("g_instant_weapon_switch", settings->instantweap ? "1" : "0");
	}

	if (settings->match_lock != G_Menu_CvarEnabled(g_match_lock)) {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} turned {} match lock.\n",
			ent->client->resp.netname, settings->match_lock ? "on" : "off");
		gi.cvar_set("g_match_lock", settings->match_lock ? "1" : "0");
	}

	P_Menu_Close(ent);
	G_Menu_Admin(ent, nullptr);
}

static void G_Menu_Admin_SettingsCancel(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent))
		return;

	P_Menu_Close(ent);
	G_Menu_Admin(ent, nullptr);
}

static void G_Menu_Admin_ChangeMatchLen(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent) || !p || !p->arg)
		return;

	admin_settings_t *settings = (admin_settings_t *)p->arg;

	settings->timelimit = (settings->timelimit % 60) + 5;
	settings->timelimit = G_Menu_NormalizeTimelimit(settings->timelimit);

	G_Menu_Admin_UpdateSettings(ent, p);
}

static void G_Menu_Admin_ChangeMatchSetupLen(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent) || !p || !p->arg)
		return;

	G_Menu_Admin_UpdateSettings(ent, p);
}

static void G_Menu_Admin_ChangeMatchStartLen(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent) || !p || !p->arg)
		return;

	G_Menu_Admin_UpdateSettings(ent, p);
}

static void G_Menu_Admin_ChangeWeapStay(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent) || !p || !p->arg)
		return;

	admin_settings_t *settings = (admin_settings_t *)p->arg;

	settings->weaponsstay = !settings->weaponsstay;
	G_Menu_Admin_UpdateSettings(ent, p);
}

static void G_Menu_Admin_ChangeInstantItems(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent) || !p || !p->arg)
		return;

	admin_settings_t *settings = (admin_settings_t *)p->arg;

	settings->instantitems = !settings->instantitems;
	G_Menu_Admin_UpdateSettings(ent, p);
}

static void G_Menu_Admin_ChangePowerupDrop(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent) || !p || !p->arg)
		return;

	admin_settings_t *settings = (admin_settings_t *)p->arg;

	settings->pu_drop = !settings->pu_drop;
	G_Menu_Admin_UpdateSettings(ent, p);
}

static void G_Menu_Admin_ChangeInstantWeap(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent) || !p || !p->arg)
		return;

	admin_settings_t *settings = (admin_settings_t *)p->arg;

	settings->instantweap = !settings->instantweap;
	G_Menu_Admin_UpdateSettings(ent, p);
}

static void G_Menu_Admin_ChangeMatchLock(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent) || !p || !p->arg)
		return;

	admin_settings_t *settings = (admin_settings_t *)p->arg;

	settings->match_lock = !settings->match_lock;
	G_Menu_Admin_UpdateSettings(ent, p);
}

void G_Menu_Admin_UpdateSettings(gentity_t *ent, menu_hnd_t *setmenu) {
	if (!G_Menu_HasClient(ent) || !setmenu || !setmenu->entries || !setmenu->arg || setmenu->num < 8)
		return;

	int				  i = 2;
	admin_settings_t *settings = (admin_settings_t *)setmenu->arg;

	P_Menu_UpdateEntry(setmenu->entries + i, G_Fmt("time limit: {:2} mins", settings->timelimit).data(), MENU_ALIGN_LEFT, G_Menu_Admin_ChangeMatchLen);
	i++;

	P_Menu_UpdateEntry(setmenu->entries + i, G_Fmt("weapons stay: {}", settings->weaponsstay ? "Yes" : "No").data(), MENU_ALIGN_LEFT, G_Menu_Admin_ChangeWeapStay);
	i++;

	P_Menu_UpdateEntry(setmenu->entries + i, G_Fmt("instant items: {}", settings->instantitems ? "Yes" : "No").data(), MENU_ALIGN_LEFT, G_Menu_Admin_ChangeInstantItems);
	i++;

	P_Menu_UpdateEntry(setmenu->entries + i, G_Fmt("powerup drops: {}", settings->pu_drop ? "Yes" : "No").data(), MENU_ALIGN_LEFT, G_Menu_Admin_ChangePowerupDrop);
	i++;

	P_Menu_UpdateEntry(setmenu->entries + i, G_Fmt("instant weapon switch: {}", settings->instantweap ? "Yes" : "No").data(), MENU_ALIGN_LEFT, G_Menu_Admin_ChangeInstantWeap);
	i++;

	P_Menu_UpdateEntry(setmenu->entries + i, G_Fmt("match lock: {}", settings->match_lock ? "Yes" : "No").data(), MENU_ALIGN_LEFT, G_Menu_Admin_ChangeMatchLock);
	i++;

	P_Menu_Update(ent);
}

const menu_t def_setmenu[] = {
	{ "*Settings Menu", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr }, // int timelimit;
	{ "", MENU_ALIGN_LEFT, nullptr }, // bool weaponsstay;
	{ "", MENU_ALIGN_LEFT, nullptr }, // bool instantitems;
	{ "", MENU_ALIGN_LEFT, nullptr }, // bool pu_drop;
	{ "", MENU_ALIGN_LEFT, nullptr }, // bool instantweap;
	{ "", MENU_ALIGN_LEFT, nullptr }, // bool g_match_lock;
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

static void G_Menu_Admin_Settings(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent) || !ent->client->sess.admin)
		return;

	admin_settings_t *settings;
	menu_hnd_t *menu;

	P_Menu_Close(ent);

	settings = (admin_settings_t *)gi.TagMalloc(sizeof(*settings), TAG_LEVEL);
	if (!settings)
		return;

	settings->timelimit = G_Menu_NormalizeTimelimit(G_Menu_CvarInteger(timelimit));
	settings->weaponsstay = G_Menu_CvarEnabled(g_dm_weapons_stay);
	settings->instantitems = G_Menu_CvarEnabled(g_dm_instant_items);
	settings->pu_drop = G_Menu_CvarEnabled(g_dm_powerup_drop);
	settings->instantweap = G_Menu_CvarEnabled(g_instant_weapon_switch);
	settings->match_lock = G_Menu_CvarEnabled(g_match_lock);

	menu = P_Menu_Open(ent, def_setmenu, -1, sizeof(def_setmenu) / sizeof(menu_t), settings, nullptr);
	if (!menu) {
		gi.TagFree(settings);
		return;
	}
	G_Menu_Admin_UpdateSettings(ent, menu);
}

static void G_Menu_Admin_MatchSet(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent) || !ent->client->sess.admin)
		return;

	P_Menu_Close(ent);

	if (level.match_state <= matchst_t::MATCH_COUNTDOWN) {
		gi.LocBroadcast_Print(PRINT_CHAT, "Match has been forced to start.\n");
		Match_Start();
	} else if (level.match_state == matchst_t::MATCH_IN_PROGRESS) {
		gi.LocBroadcast_Print(PRINT_CHAT, "Match has been forced to terminate.\n");
		Match_Reset();
	}
}

static void G_Menu_Admin_Cancel(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent))
		return;

	P_Menu_Close(ent);
}

menu_t adminmenu[] = {
	{ "*Administration Menu", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "Settings", MENU_ALIGN_LEFT, G_Menu_Admin_Settings },
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

void G_Menu_Admin(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent) || !ent->client->sess.admin)
		return;

	adminmenu[3].text[0] = '\0';
	adminmenu[3].SelectFunc = nullptr;
	adminmenu[4].text[0] = '\0';
	adminmenu[4].SelectFunc = nullptr;

	if (level.match_state <= matchst_t::MATCH_COUNTDOWN) {
		Q_strlcpy(adminmenu[3].text, "Force start match", sizeof(adminmenu[3].text));
		adminmenu[3].SelectFunc = G_Menu_Admin_MatchSet;

	} else if (level.match_state == matchst_t::MATCH_IN_PROGRESS) {
		Q_strlcpy(adminmenu[3].text, "Reset match", sizeof(adminmenu[3].text));
		adminmenu[3].SelectFunc = G_Menu_Admin_MatchSet;
	}

	P_Menu_Close(ent);
	P_Menu_Open(ent, adminmenu, -1, sizeof(adminmenu) / sizeof(menu_t), nullptr, nullptr);
}

/*-----------------------------------------------------------------------*/

const menu_t pmstatsmenu[] = {
	{ "Player Match Stats", MENU_ALIGN_CENTER, nullptr },
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

static void G_Menu_PMStats_Update(gentity_t *ent) {
	menu_t *entries = nullptr;
	int num_entries = 0;

	if (!G_Menu_CvarEnabled(g_matchstats))
		return;
	if (!G_Menu_GetEntries(ent, &entries, &num_entries))
		return;

	client_match_stats_t *st = &ent->client->mstats;
	int i = 0;
	char value[MAX_INFO_VALUE] = { 0 };
	G_Menu_CopyHostPlayerName(value, sizeof(value));

	if (i < num_entries)
		Q_strlcpy(entries[i].text, "Player Stats for Match", sizeof(entries[i].text));
	i++;

	if (value[0]) {
		if (i < num_entries)
			Q_strlcpy(entries[i].text, G_Fmt("{}", value).data(), sizeof(entries[i].text));
		i++;
	}

	if (i < num_entries)
		Q_strlcpy(entries[i].text, BREAKER, sizeof(entries[i].text));
	i++;

	if (i < num_entries)
		Q_strlcpy(entries[i].text, G_Fmt("kills: {}", st->total_kills).data(), sizeof(entries[i].text));
	i++;
	if (i < num_entries)
		Q_strlcpy(entries[i].text, G_Fmt("deaths: {}", st->total_deaths).data(), sizeof(entries[i].text));
	i++;
	if (st->total_kills) {
		if (i < num_entries) {
			if (st->total_deaths > 0) {
				float val = (float)st->total_kills / (float)st->total_deaths;
				Q_strlcpy(entries[i].text, G_Fmt("k/d ratio: {:2}", val).data(), sizeof(entries[i].text));
			} else {
				Q_strlcpy(entries[i].text, "k/d ratio: N/A", sizeof(entries[i].text));
			}
		}
		i++;
	}
	if (i < num_entries)
		entries[i].text[0] = '\0';
	i++;
	if (i < num_entries)
		Q_strlcpy(entries[i].text, G_Fmt("dmg dealt: {}", st->total_dmg_dealt).data(), sizeof(entries[i].text));
	i++;
	if (i < num_entries)
		Q_strlcpy(entries[i].text, G_Fmt("dmg received: {}", st->total_dmg_received).data(), sizeof(entries[i].text));
	i++;
	if (st->total_dmg_dealt) {
		if (i < num_entries) {
			if (st->total_dmg_received > 0) {
				float val = (float)st->total_dmg_dealt / (float)st->total_dmg_received;
				Q_strlcpy(entries[i].text, G_Fmt("dmg ratio: {:02}", val).data(), sizeof(entries[i].text));
			} else {
				Q_strlcpy(entries[i].text, "dmg ratio: N/A", sizeof(entries[i].text));
			}
		}
		i++;
	}
	if (i < num_entries)
		entries[i].text[0] = '\0';
	i++;
	if (i < num_entries)
		Q_strlcpy(entries[i].text, G_Fmt("shots fired: {}", st->total_shots).data(), sizeof(entries[i].text));
	i++;
	if (i < num_entries)
		Q_strlcpy(entries[i].text, G_Fmt("shots on target: {}", st->total_hits).data(), sizeof(entries[i].text));
	i++;
	if (st->total_hits) {
		if (i < num_entries) {
			if (st->total_shots > 0) {
				int val = (int)(((float)st->total_hits / (float)st->total_shots) * 100.f);
				Q_strlcpy(entries[i].text, G_Fmt("total accuracy: {}%", val).data(), sizeof(entries[i].text));
			} else {
				Q_strlcpy(entries[i].text, "total accuracy: N/A", sizeof(entries[i].text));
			}
		}
		i++;
	}
}

static void G_Menu_PMStats(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent))
		return;

	P_Menu_Close(ent);
	P_Menu_Open(ent, pmstatsmenu, -1, sizeof(pmstatsmenu) / sizeof(menu_t), nullptr, G_Menu_PMStats_Update);
}

/*-----------------------------------------------------------------------*/

static void G_Menu_Join_Team_Free(gentity_t *ent, menu_hnd_t *p) {
	SetTeam(ent, TEAM_FREE, false, false, false);
}

static void G_Menu_Join_Team_Red(gentity_t *ent, menu_hnd_t *p) {
	SetTeam(ent, !G_Menu_CvarEnabled(g_teamplay_allow_team_pick) ? PickTeam(-1) : TEAM_RED, false, false, false);
}

static void G_Menu_Join_Team_Blue(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_CvarEnabled(g_teamplay_allow_team_pick))
		return;

	SetTeam(ent, TEAM_BLUE, false, false, false);
}

static void G_Menu_Join_Team_Spec(gentity_t *ent, menu_hnd_t *p) {
	SetTeam(ent, TEAM_SPECTATOR, false, false, false);
}

void G_Menu_ReturnToMain(gentity_t *ent, menu_hnd_t *p);
void G_Menu_ChaseCam(gentity_t *ent, menu_hnd_t *p);
void G_Menu_HostInfo(gentity_t *ent, menu_hnd_t *p);
void G_Menu_ServerInfo(gentity_t *ent, menu_hnd_t *p);

static void G_Menu_ReadyUp(gentity_t *ent, menu_hnd_t *p) {
	MM_CmdReadyUp(ent);
}

static const int jmenu_hostname = 0;
static const int jmenu_gametype = 1;
static const int jmenu_level = 2;
static const int jmenu_match = 3;

static const int jmenu_teams_join_red = 5;
static const int jmenu_teams_join_blue = 6;
static const int jmenu_teams_spec = 7;
static const int jmenu_teams_chase = 8;
static const int jmenu_teams_readyup = 9;
static const int jmenu_teams_hostinfo = 10;
static const int jmenu_teams_svinfo = 11;
static const int jmenu_teams_player = 12;
static const int jmenu_teams_callvote = 13;
static const int jmenu_teams_admin = 14;

static const int jmenu_free_join = 5;
static const int jmenu_free_spec = 7;
static const int jmenu_free_chase = 8;
static const int jmenu_free_readyup = 9;
static const int jmenu_free_hostinfo = 10;
static const int jmenu_free_svinfo = 11;
static const int jmenu_free_player = 12;
static const int jmenu_free_callvote = 13;
static const int jmenu_free_admin = 14;

static const int jmenu_gamemod = 16;
static const int jmenu_notice = 17;

const menu_t teams_join_menu[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "$g_pc_join_red_team", MENU_ALIGN_LEFT, G_Menu_Join_Team_Red },
	{ "$g_pc_join_blue_team", MENU_ALIGN_LEFT, G_Menu_Join_Team_Blue },
	{ "Spectate", MENU_ALIGN_LEFT, G_Menu_Join_Team_Spec },
	{ "$g_pc_chase_camera", MENU_ALIGN_LEFT, G_Menu_ChaseCam },
	{ "", MENU_ALIGN_LEFT, nullptr },  // Ready Up (set dynamically)
	{ "Host Info", MENU_ALIGN_LEFT, G_Menu_HostInfo },
	{ "Match Info", MENU_ALIGN_LEFT, G_Menu_ServerInfo },
	{ "Player Stats", MENU_ALIGN_LEFT, G_Menu_PMStats },
	{ "Call a Vote", MENU_ALIGN_LEFT, G_Menu_CallVote },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr }
};

const menu_t free_join_menu[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "Join Game", MENU_ALIGN_LEFT, G_Menu_Join_Team_Free },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "Spectate", MENU_ALIGN_LEFT, G_Menu_Join_Team_Spec },
	{ "$g_pc_chase_camera", MENU_ALIGN_LEFT, G_Menu_ChaseCam },
	{ "", MENU_ALIGN_LEFT, nullptr },  // Ready Up (set dynamically)
	{ "Host Info", MENU_ALIGN_LEFT, G_Menu_HostInfo },
	{ "Match Info", MENU_ALIGN_LEFT, G_Menu_ServerInfo },
	{ "Player Stats", MENU_ALIGN_LEFT, G_Menu_PMStats },
	{ "Call a Vote", MENU_ALIGN_LEFT, G_Menu_CallVote },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr }
};

const menu_t nochasemenu[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "$g_pc_no_chase", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "$g_pc_return", MENU_ALIGN_LEFT, G_Menu_ReturnToMain }
};

const menu_t hostinfomenu[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "$g_pc_return", MENU_ALIGN_LEFT, G_Menu_ReturnToMain }
};

const menu_t svinfomenu[] = {
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
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "$g_pc_return", MENU_ALIGN_LEFT, G_Menu_ReturnToMain }
};

static void G_Menu_NoChaseCamUpdate(gentity_t *ent) {
	menu_t *entries = nullptr;
	int num_entries = 0;
	if (!G_Menu_GetEntries(ent, &entries, &num_entries) || G_Menu_ContentLimit(num_entries) < 3)
		return;

	G_Menu_SetGamemodName(&entries[0]);
	G_Menu_SetGametypeName(&entries[1]);
	G_Menu_SetLevelName(&entries[2]);
}

void G_Menu_ChaseCam(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent))
		return;

	SetTeam(ent, TEAM_SPECTATOR, false, false, false);

	if (ent->client->follow_target) {
		FreeFollower(ent);
		P_Menu_Close(ent);
		return;
	}

	GetFollowTarget(ent);

	P_Menu_Close(ent);
	P_Menu_Open(ent, nochasemenu, -1, sizeof(nochasemenu) / sizeof(menu_t), nullptr, G_Menu_NoChaseCamUpdate);
}

void G_Menu_ReturnToMain(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent))
		return;

	P_Menu_Close(ent);
	G_Menu_Join_Open(ent);
	gi.local_sound(ent, CHAN_AUTO, gi.soundindex("misc/menu3.wav"), 1, ATTN_NONE, 0);
}

static void G_Menu_HostInfo_Update(gentity_t *ent) {
	menu_t *entries = nullptr;
	int num_entries = 0;
	if (!G_Menu_GetEntries(ent, &entries, &num_entries))
		return;
	const int content_limit = G_Menu_ContentLimit(num_entries);

	int		i = 0;

	const char *server_name = G_Menu_CvarString(hostname);
	if (server_name[0] && i + 2 < content_limit) {
		Q_strlcpy(entries[i].text, "Server Name:", sizeof(entries[i].text));
		i++;
		Q_strlcpy(entries[i].text, server_name, sizeof(entries[i].text));
		i++;
		i++;
	}

	if (i + 2 < content_limit) {
		char value[MAX_INFO_VALUE] = { 0 };

		if (G_Menu_CopyHostPlayerName(value, sizeof(value))) {
			Q_strlcpy(entries[i].text, "Host:", sizeof(entries[i].text));
			i++;
			Q_strlcpy(entries[i].text, value, sizeof(entries[i].text));
			i++;
			i++;
		}
	}

	if (game.motd.size() && i < content_limit) {
		Q_strlcpy(entries[i].text, "Message of the Day:", sizeof(entries[i].text));
		i++;
		// 26 char line width
		// 9 lines
		// = 234

		if (i < content_limit)
			Q_strlcpy(entries[i].text, G_Fmt("{}", game.motd.c_str()).data(), sizeof(entries[i].text));
	}
}

void G_Menu_HostInfo(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent))
		return;

	P_Menu_Close(ent);
	P_Menu_Open(ent, hostinfomenu, -1, sizeof(hostinfomenu) / sizeof(menu_t), nullptr, G_Menu_HostInfo_Update);
}

static void G_Menu_ServerInfo_Update(gentity_t *ent) {
	menu_t *entries = nullptr;
	int num_entries = 0;
	if (!G_Menu_GetEntries(ent, &entries, &num_entries))
		return;
	const int content_limit = G_Menu_ContentLimit(num_entries);
	if (content_limit < 3)
		return;

	int		i = 0;
	bool	limits = false;
	bool	infiniteammo = InfiniteAmmoOn(nullptr);
	bool	items = ItemSpawnsEnabled();
	int		scorelimit = GT_ScoreLimit();
	
	Q_strlcpy(entries[i].text, "Match Info", sizeof(entries[i].text));
	i++;

	Q_strlcpy(entries[i].text, BREAKER, sizeof(entries[i].text));
	i++;

	Q_strlcpy(entries[i].text, level.gametype_name, sizeof(entries[i].text));
	i++;
	
	if (level.level_name[0]) {
		Q_strlcpy(entries[i].text, G_Fmt("map: {}", level.level_name).data(), sizeof(entries[i].text));
		i++;
	}
	if (level.mapname[0]) {
		Q_strlcpy(entries[i].text, G_Fmt("mapname: {}", level.mapname).data(), sizeof(entries[i].text));
		i++;
	}
	if (level.author[0]) {
		Q_strlcpy(entries[i].text, G_Fmt("author: {}", level.author).data(), sizeof(entries[i].text));
		i++;
	}
	if (level.author2[0] && level.author[0]) {
		Q_strlcpy(entries[i].text, G_Fmt("      {}", level.author2).data(), sizeof(entries[i].text));
		i++;
	}

	Q_strlcpy(entries[i].text, G_Fmt("ruleset: {}", G_Menu_CurrentRulesetName()).data(), sizeof(entries[i].text));
	i++;

	if (scorelimit) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, G_Fmt("{} limit: {}", GT_ScoreLimitString(), scorelimit).data(), sizeof(entries[i].text));
		i++;
		limits = true;
	}

	if (timelimit->value > 0) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, G_Fmt("time limit: {}", G_TimeString(timelimit->value * 60000, false)).data(), sizeof(entries[i].text));
		i++;
		limits = true;
	}

	if (limits) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, BREAKER, sizeof(entries[i].text));
		i++;
	}

	if (g_instagib->integer || GT(GT_INSTAGIB)) {
		if (i >= content_limit) return;
		if (g_instagib_splash->integer) {
			Q_strlcpy(entries[i].text, "InstaGib + Rail Splash", sizeof(entries[i].text));
		} else {
			Q_strlcpy(entries[i].text, "InstaGib", sizeof(entries[i].text));
		}
		i++;
	}
	if (g_vampiric_damage->integer) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, "Vampiric Damage", sizeof(entries[i].text));
		i++;
	}
	if (g_frenzy->integer) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, "Weapons Frenzy", sizeof(entries[i].text));
		i++;
	}
	if (g_nadefest->integer || GT(GT_NADEFEST)) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, "Nade Fest", sizeof(entries[i].text));
		i++;
	}
	if (g_quadhog->integer) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, "Quad Hog", sizeof(entries[i].text));
		i++;
	}

	if (i >= content_limit) return;
	Q_strlcpy(entries[i].text, BREAKER, sizeof(entries[i].text));
	i++;

	if (items) {
		if (g_dm_weapons_stay->integer) {
			if (i >= content_limit) return;
			Q_strlcpy(entries[i].text, "weapons stay", sizeof(entries[i].text));
			i++;
		} else {
			if (g_weapon_respawn_time->integer != 30) {
				if (i >= content_limit) return;
				Q_strlcpy(entries[i].text, G_Fmt("weapon respawn delay: {}", g_weapon_respawn_time->integer).data(), sizeof(entries[i].text));
				i++;
			}
		}
	}

	if (g_infinite_ammo->integer && !infiniteammo) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, "infinite ammo", sizeof(entries[i].text));
		i++;
	}
	if (Teams() && g_friendly_fire->integer) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, "friendly fire", sizeof(entries[i].text));
		i++;
	}

	if (g_allow_grapple->integer) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, G_Fmt("{}grapple enabled", g_grapple_offhand->integer ? "off-hand " : "").data(), sizeof(entries[i].text));
		i++;
	}

	if (g_inactivity->integer > 0) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, G_Fmt("inactivity timer: {} sec", g_inactivity->integer).data(), sizeof(entries[i].text));
		i++;
	}

	if (g_teleporter_freeze->integer) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, "teleporter freeze", sizeof(entries[i].text));
		i++;
	}

	if (Teams() && g_teamplay_force_balance->integer && notGT(GT_RR)) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, "forced team balancing", sizeof(entries[i].text));
		i++;
	}

	if (g_dm_random_items->integer && items) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, "random items", sizeof(entries[i].text));
		i++;
	}

	if (g_dm_force_join->integer) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, "forced game joining", sizeof(entries[i].text));
		i++;
	}

	if (!g_dm_powerup_drop->integer) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, "no powerup drops", sizeof(entries[i].text));
		i++;
	}

	if (g_knockback_scale->value != 1) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, G_Fmt("knockback scale: {}", g_knockback_scale->value).data(), sizeof(entries[i].text));
		i++;
	}

	if (g_dm_no_self_damage->integer) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, "no self-damage", sizeof(entries[i].text));
		i++;
	}

	if (g_dm_no_fall_damage->integer) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, "no falling damage", sizeof(entries[i].text));
		i++;
	}

	if (!g_dm_instant_items->integer) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, "no instant items", sizeof(entries[i].text));
		i++;
	}

	if (items) {
		if (i >= content_limit) return;
		if (g_no_items->integer) {
			Q_strlcpy(entries[i].text, "no items", sizeof(entries[i].text));
			i++;
		} else {
			if (i >= content_limit) return;
			if (g_no_health->integer) {
				Q_strlcpy(entries[i].text, "no health spawns", sizeof(entries[i].text));
				i++;
			}

			if (i >= content_limit) return;
			if (g_no_armor->integer) {
				Q_strlcpy(entries[i].text, "no armor spawns", sizeof(entries[i].text));
				i++;
			}

			if (i >= content_limit) return;
			if (g_no_mines->integer) {
				Q_strlcpy(entries[i].text, "no mines", sizeof(entries[i].text));
				i++;
			}
		}
	}

	if (g_dm_allow_exit->integer) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, "allow exiting", sizeof(entries[i].text));
		i++;
	}

	if (g_mover_speed_scale->value != 1.0f) {
		if (i >= content_limit) return;
		Q_strlcpy(entries[i].text, G_Fmt("mover speed scale: {}", g_mover_speed_scale->value).data(), sizeof(entries[i].text));
		i++;
	}

}

void G_Menu_ServerInfo(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent))
		return;

	P_Menu_Close(ent);
	P_Menu_Open(ent, svinfomenu, -1, sizeof(svinfomenu) / sizeof(menu_t), nullptr, G_Menu_ServerInfo_Update);
}

static void G_Menu_GameRules_Update(gentity_t *ent) {
	menu_t *entries = nullptr;
	int num_entries = 0;
	if (!G_Menu_GetEntries(ent, &entries, &num_entries) || G_Menu_ContentLimit(num_entries) < 3)
		return;

	int		i = 0;

	Q_strlcpy(entries[i].text, "Game Rules", sizeof(entries[i].text)); i++;
	Q_strlcpy(entries[i].text, BREAKER, sizeof(entries[i].text)); i++;
	Q_strlcpy(entries[i].text, G_Fmt("{}", level.gametype_name).data(), sizeof(entries[i].text)); i++;
}

static void G_Menu_GameRules(gentity_t *ent, menu_hnd_t *p) {
	if (!G_Menu_HasClient(ent))
		return;

	P_Menu_Close(ent);
	P_Menu_Open(ent, svinfomenu, -1, sizeof(svinfomenu) / sizeof(menu_t), nullptr, G_Menu_GameRules_Update);
}

static void G_Menu_Join_Update(gentity_t *ent) {
	menu_t *entries = nullptr;
	int num_entries = 0;
	if (!G_Menu_GetEntries(ent, &entries, &num_entries) || num_entries <= jmenu_notice)
		return;

	int		pmax = G_Menu_CvarInteger(maxplayers);
	int		num_red = 0, num_blue = 0, num_free = 0, num_queue = 0;

	for (auto ec : active_clients()) {
		if (GT(GT_DUEL) && ec->client->sess.team == TEAM_SPECTATOR && ec->client->sess.duel_queued) {
			num_queue++;
		} else {
			switch (ec->client->sess.team) {
			case TEAM_FREE:
				num_free++;
				break;
			case TEAM_RED:
				num_red++;
				break;
			case TEAM_BLUE:
				num_blue++;
				break;
			}
		}
	}

	if (pmax < 1) pmax = 1;

	if (Teams()) {
		if (!G_Menu_CvarEnabled(g_teamplay_allow_team_pick) && !level.locked[TEAM_RED] && !level.locked[TEAM_BLUE]) {
			Q_strlcpy(entries[jmenu_teams_join_red].text, G_Fmt("Join a Team ({}/{})", num_red + num_blue, pmax).data(), sizeof(entries[jmenu_teams_join_red].text));
			Q_strlcpy(entries[jmenu_teams_join_blue].text, "", sizeof(entries[jmenu_teams_join_blue].text));

			entries[jmenu_teams_join_red].SelectFunc = G_Menu_Join_Team_Red;
			entries[jmenu_teams_join_blue].SelectFunc = nullptr;
		} else {
			if (level.locked[TEAM_RED]) {
				Q_strlcpy(entries[jmenu_teams_join_red].text, G_Fmt("{} is LOCKED", Teams_TeamName(TEAM_RED)).data(), sizeof(entries[jmenu_teams_join_red].text));
				entries[jmenu_teams_join_red].SelectFunc = nullptr;
			} else {
				Q_strlcpy(entries[jmenu_teams_join_red].text, G_Fmt("Join {} ({}/{})", Teams_TeamName(TEAM_RED), num_red, pmax / 2).data(), sizeof(entries[jmenu_teams_join_red].text));
				entries[jmenu_teams_join_red].SelectFunc = G_Menu_Join_Team_Red;
			}
			if (level.locked[TEAM_BLUE]) {
				Q_strlcpy(entries[jmenu_teams_join_blue].text, G_Fmt("{} is LOCKED", Teams_TeamName(TEAM_BLUE)).data(), sizeof(entries[jmenu_teams_join_blue].text));
				entries[jmenu_teams_join_blue].SelectFunc = nullptr;
			} else {
				Q_strlcpy(entries[jmenu_teams_join_blue].text, G_Fmt("Join {} ({}/{})", Teams_TeamName(TEAM_BLUE), num_blue, pmax / 2).data(), sizeof(entries[jmenu_teams_join_blue].text));
				entries[jmenu_teams_join_blue].SelectFunc = G_Menu_Join_Team_Blue;
			}

		}
	} else {
		// Allow duel queue joining even during match lock (queue joining doesn't affect active match)
		bool is_duel_queue_join = GT(GT_DUEL) && level.num_playing_clients == 2;
		if (level.locked[TEAM_FREE] && !is_duel_queue_join) {
			Q_strlcpy(entries[jmenu_free_join].text, "Match LOCKED", sizeof(entries[jmenu_free_join].text));
			entries[jmenu_free_join].SelectFunc = nullptr;
		} else if (GT(GT_DUEL) && level.num_playing_clients == 2) {
			Q_strlcpy(entries[jmenu_free_join].text, G_Fmt("Join Queue to Play ({}/{})", num_queue, max(0, pmax - 2)).data(), sizeof(entries[jmenu_free_join].text));
			entries[jmenu_free_join].SelectFunc = G_Menu_Join_Team_Free;
		} else {
			Q_strlcpy(entries[jmenu_free_join].text, G_Fmt("Join Match ({}/{})", num_free, GT(GT_DUEL) ? 2 : pmax).data(), sizeof(entries[jmenu_free_join].text));
			entries[jmenu_free_join].SelectFunc = G_Menu_Join_Team_Free;
		}
	}

	if (!G_Menu_CvarEnabled(g_matchstats)) {
		int index = Teams() ? jmenu_teams_player : jmenu_free_player;
		Q_strlcpy(entries[index].text, "", sizeof(entries[index].text));
		entries[index].SelectFunc = nullptr;
	} else {
		int index = Teams() ? jmenu_teams_player : jmenu_free_player;
		Q_strlcpy(entries[index].text, "Player Stats", sizeof(entries[index].text));
		entries[index].SelectFunc = G_Menu_PMStats;
	}

	if (!G_Menu_CvarEnabled(g_allow_voting)) {
		int index = Teams() ? jmenu_teams_callvote : jmenu_free_callvote;
		Q_strlcpy(entries[index].text, "", sizeof(entries[index].text));
		entries[index].SelectFunc = nullptr;
	} else {
		int index = Teams() ? jmenu_teams_callvote : jmenu_free_callvote;
		Q_strlcpy(entries[index].text, "Call a Vote", sizeof(entries[index].text));
		entries[index].SelectFunc = G_Menu_CallVote;
	}

	const char *force_join = G_Menu_CvarString(g_dm_force_join);
	if (force_join[0]) {
		if (Teams()) {
			if (Q_strcasecmp(force_join, "red") == 0) {
				entries[jmenu_teams_join_blue].text[0] = '\0';
				entries[jmenu_teams_join_blue].SelectFunc = nullptr;
			} else if (Q_strcasecmp(force_join, "blue") == 0) {
				entries[jmenu_teams_join_red].text[0] = '\0';
				entries[jmenu_teams_join_red].SelectFunc = nullptr;
			}
		}
	}

	int index = Teams() ? jmenu_teams_chase : jmenu_free_chase;
	if (ent->client->follow_target)
		Q_strlcpy(entries[index].text, "$g_pc_leave_chase_camera", sizeof(entries[index].text));
	else
		Q_strlcpy(entries[index].text, "$g_pc_chase_camera", sizeof(entries[index].text));

	index = Teams() ? jmenu_teams_readyup : jmenu_free_readyup;
	if (G_Menu_CvarEnabled(g_dm_do_readyup) && level.match_state == matchst_t::MATCH_WARMUP_READYUP) {
		Q_strlcpy(entries[index].text, ent->client->resp.ready ? "Not Ready" : "Ready Up", sizeof(entries[index].text));
		entries[index].SelectFunc = G_Menu_ReadyUp;
	} else {
		entries[index].text[0] = '\0';
		entries[index].SelectFunc = nullptr;
	}

	G_Menu_SetHostName(entries + jmenu_hostname);
	G_Menu_SetGametypeName(entries + jmenu_gametype);
	G_Menu_SetLevelName(entries + jmenu_level);

	G_Menu_SetGamemodName(entries + jmenu_gamemod);

	switch (level.match_state) {
	case matchst_t::MATCH_NONE:
		entries[jmenu_match].text[0] = '\0';
		break;

	case matchst_t::MATCH_WARMUP_DELAYED:
	case matchst_t::MATCH_WARMUP_DEFAULT:
	case matchst_t::MATCH_WARMUP_READYUP:
		Q_strlcpy(entries[jmenu_match].text, "*MATCH WARMUP", sizeof(entries[jmenu_match].text));
		break;

	case matchst_t::MATCH_COUNTDOWN:
		Q_strlcpy(entries[jmenu_match].text, "*MATCH IS STARTING", sizeof(entries[jmenu_match].text));
		break;

	case matchst_t::MATCH_IN_PROGRESS:
		Q_strlcpy(entries[jmenu_match].text, "*MATCH IN PROGRESS", sizeof(entries[jmenu_match].text));
		break;

	default:
		Q_strlcpy(entries[jmenu_match].text, BREAKER, sizeof(entries[jmenu_match].text));
		break;
	}

	int admin_index = Teams() ? jmenu_teams_admin : jmenu_free_admin;
	if (ent->client->sess.admin) {
		Q_strlcpy(entries[admin_index].text, "Admin", sizeof(entries[admin_index].text));
		entries[admin_index].align = MENU_ALIGN_LEFT;
		entries[admin_index].SelectFunc = G_Menu_Admin;
	} else {
		Q_strlcpy(entries[admin_index].text, "", sizeof(entries[admin_index].text));
		entries[admin_index].SelectFunc = nullptr;
	}

	Q_strlcpy(entries[jmenu_notice].text, "github.com/themuffinator", sizeof(entries[jmenu_notice].text));
}

void G_Menu_Join_Open(gentity_t *ent) {
	if (!G_Menu_HasClient(ent))
		return;

	if (Vote_Menu_Active(ent))
		return;

	if (Teams()) {
		team_t team = TEAM_SPECTATOR;
		int num_red = 0, num_blue = 0;

		for (auto ec : active_clients()) {
			switch (ec->client->sess.team) {
			case TEAM_RED:
				num_red++;
				break;
			case TEAM_BLUE:
				num_blue++;
				break;
			}
		}

		if (num_red > num_blue)
			team = TEAM_RED;
		else if (num_blue > num_red)
			team = TEAM_BLUE;
		else
			team = brandom() ? TEAM_RED : TEAM_BLUE;

		P_Menu_Open(ent, teams_join_menu, team, sizeof(teams_join_menu) / sizeof(menu_t), nullptr, G_Menu_Join_Update);
	} else {
		P_Menu_Open(ent, free_join_menu, TEAM_FREE, sizeof(free_join_menu) / sizeof(menu_t), nullptr, G_Menu_Join_Update);
	}
}
