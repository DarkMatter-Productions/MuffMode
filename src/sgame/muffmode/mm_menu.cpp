// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include "g_local.h"
#include "muffmode/mm_captain.h"
#include "muffmode/mm_match.h"
#include "muffmode/mm_menu.h"
#include "muffmode/mm_pconfig.h"
#include "muffmode/mm_skin.h"
#include "muffmode/mm_team.h"
#include "muffmode/mm_util.h"
#include "muffmode/mm_vote_menu.h"
#include "monsters/m_player.h"

#include <array>
#include <string>
#include <string_view>

namespace muffmode::menu {

constexpr const char *kBreaker = "\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37";

bool HasClient(gentity_t *ent)
{
	return ent && ent->client;
}

bool GetEntries(gentity_t *ent, menu_t **entries, int *num = nullptr)
{
	if (!HasClient(ent) || !ent->client->menu || !ent->client->menu->entries || ent->client->menu->num <= 0)
		return false;

	if (entries)
		*entries = ent->client->menu->entries;
	if (num)
		*num = ent->client->menu->num;
	return true;
}

int ContentLimit(int num_entries)
{
	return max(0, num_entries - 2);
}

void SetText(menu_t &entry, std::string_view text)
{
	muffmode::CopyString(entry.text, text);
}

void SetText(menu_t *entry, std::string_view text)
{
	if (entry)
		SetText(*entry, text);
}

bool CopyHostPlayerName(char *value, size_t value_size)
{
	if (!value || !value_size)
		return false;

	value[0] = '\0';
	if (!g_entities || game.maxclients <= 0 || !g_entities[1].client)
		return false;

	gi.Info_ValueForKey(g_entities[1].client->pers.userinfo, "name", value, value_size);
	return value[0] != '\0';
}

template <size_t N>
bool CopyHostPlayerName(std::array<char, N> &value)
{
	return CopyHostPlayerName(value.data(), value.size());
}

int NormalizeTimelimit(int minutes)
{
	return clamp(minutes, 5, 60);
}

void SetHostName(menu_t *p)
{
	const char *name = muffmode::CvarString(hostname);
	SetText(p, name[0] ? name : "MuffMode Server");
}

void SetGamemodName(menu_t *p)
{
	SetText(p, level.gamemod_name);
}

void SetGametypeName(menu_t *p)
{
	SetText(p, level.gametype_name);
}

void SetLevelName(menu_t *p)
{
	static char levelname[33];

	std::string value = "*";
	value += g_entities[0].message ? g_entities[0].message : level.mapname;
	muffmode::CopyString(levelname, value);
	SetText(p, levelname);
}

const char *CurrentRulesetName()
{
	const int ruleset = clamp(static_cast<int>(game.ruleset), static_cast<int>(RS_NONE) + 1, static_cast<int>(RS_NUM_RULESETS) - 1);
	return rs_long_name[ruleset];
}

} // namespace muffmode::menu

namespace menu = muffmode::menu;

/*----------------------------------------------------------------------------------*/
/* ADMIN */

namespace muffmode::menu::join {
void Open(gentity_t *ent);
}

namespace muffmode::menu::info {
void ReturnToMain(gentity_t *ent, menu_hnd_t *p);
void OpenFollowCamera(gentity_t *ent, menu_hnd_t *p);
void OpenHost(gentity_t *ent, menu_hnd_t *p);
void OpenServer(gentity_t *ent, menu_hnd_t *p);
}

namespace muffmode::menu::player_settings {
void Open(gentity_t *ent, menu_hnd_t *p);
}

namespace muffmode::menu::admin {

struct AdminSettings {
	int	 timelimit;
	bool weaponsstay;
	bool instantitems;
	bool pu_drop;
	bool instantweap;
	bool match_lock;
};

void UpdateSettings(gentity_t *ent, menu_hnd_t *settings_menu);
void Open(gentity_t *ent, menu_hnd_t *p);

void ApplySettings(gentity_t *ent, menu_hnd_t *p)
{
	if (!menu::HasClient(ent) || !ent->client->sess.admin || !p || !p->arg)
		return;

	auto *settings = static_cast<AdminSettings *>(p->arg);
	settings->timelimit = menu::NormalizeTimelimit(settings->timelimit);

	if (settings->timelimit != muffmode::CvarInteger(timelimit)) {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} changed the timelimit to {} minutes.\n",
			ent->client->resp.netname, settings->timelimit);

		const std::string value = fmt::format("{}", settings->timelimit);
		gi.cvar_set("timelimit", value.c_str());
	}

	if (settings->weaponsstay != muffmode::CvarEnabled(g_dm_weapons_stay)) {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} turned {} weapons stay.\n",
			ent->client->resp.netname, settings->weaponsstay ? "on" : "off");
		gi.cvar_set("g_dm_weapons_stay", settings->weaponsstay ? "1" : "0");
	}

	if (settings->instantitems != muffmode::CvarEnabled(g_dm_instant_items)) {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} turned {} instant items.\n",
			ent->client->resp.netname, settings->instantitems ? "on" : "off");
		gi.cvar_set("g_dm_instant_items", settings->instantitems ? "1" : "0");
	}

	if (settings->pu_drop != muffmode::CvarEnabled(g_dm_powerup_drop)) {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} turned {} powerup dropping.\n",
			ent->client->resp.netname, settings->pu_drop ? "on" : "off");
		gi.cvar_set("g_dm_powerup_drop", settings->pu_drop ? "1" : "0");
	}

	if (settings->instantweap != (muffmode::CvarEnabled(g_instant_weapon_switch) || muffmode::CvarEnabled(g_frenzy))) {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} turned {} instant weapon switch.\n",
			ent->client->resp.netname, settings->instantweap ? "on" : "off");
		gi.cvar_set("g_instant_weapon_switch", settings->instantweap ? "1" : "0");
	}

	if (settings->match_lock != muffmode::CvarEnabled(g_match_lock)) {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} turned {} match lock.\n",
			ent->client->resp.netname, settings->match_lock ? "on" : "off");
		gi.cvar_set("g_match_lock", settings->match_lock ? "1" : "0");
	}

	P_Menu_Close(ent);
	Open(ent, nullptr);
}

void CancelSettings(gentity_t *ent, menu_hnd_t *p)
{
	if (!menu::HasClient(ent))
		return;

	P_Menu_Close(ent);
	Open(ent, nullptr);
}

void ChangeMatchLength(gentity_t *ent, menu_hnd_t *p)
{
	if (!menu::HasClient(ent) || !p || !p->arg)
		return;

	auto *settings = static_cast<AdminSettings *>(p->arg);

	settings->timelimit = (settings->timelimit % 60) + 5;
	settings->timelimit = menu::NormalizeTimelimit(settings->timelimit);

	UpdateSettings(ent, p);
}

void ChangeMatchSetupLength(gentity_t *ent, menu_hnd_t *p)
{
	if (!menu::HasClient(ent) || !p || !p->arg)
		return;

	UpdateSettings(ent, p);
}

void ChangeMatchStartLength(gentity_t *ent, menu_hnd_t *p)
{
	if (!menu::HasClient(ent) || !p || !p->arg)
		return;

	UpdateSettings(ent, p);
}

void ToggleWeaponsStay(gentity_t *ent, menu_hnd_t *p)
{
	if (!menu::HasClient(ent) || !p || !p->arg)
		return;

	auto *settings = static_cast<AdminSettings *>(p->arg);

	settings->weaponsstay = !settings->weaponsstay;
	UpdateSettings(ent, p);
}

void ToggleInstantItems(gentity_t *ent, menu_hnd_t *p)
{
	if (!menu::HasClient(ent) || !p || !p->arg)
		return;

	auto *settings = static_cast<AdminSettings *>(p->arg);

	settings->instantitems = !settings->instantitems;
	UpdateSettings(ent, p);
}

void TogglePowerupDrop(gentity_t *ent, menu_hnd_t *p)
{
	if (!menu::HasClient(ent) || !p || !p->arg)
		return;

	auto *settings = static_cast<AdminSettings *>(p->arg);

	settings->pu_drop = !settings->pu_drop;
	UpdateSettings(ent, p);
}

void ToggleInstantWeapon(gentity_t *ent, menu_hnd_t *p)
{
	if (!menu::HasClient(ent) || !p || !p->arg)
		return;

	auto *settings = static_cast<AdminSettings *>(p->arg);

	settings->instantweap = !settings->instantweap;
	UpdateSettings(ent, p);
}

void ToggleMatchLock(gentity_t *ent, menu_hnd_t *p)
{
	if (!menu::HasClient(ent) || !p || !p->arg)
		return;

	auto *settings = static_cast<AdminSettings *>(p->arg);

	settings->match_lock = !settings->match_lock;
	UpdateSettings(ent, p);
}

void UpdateSettings(gentity_t *ent, menu_hnd_t *settings_menu)
{
	if (!menu::HasClient(ent) || !settings_menu || !settings_menu->entries || !settings_menu->arg || settings_menu->num < 8)
		return;

	int row = 2;
	auto *settings = static_cast<AdminSettings *>(settings_menu->arg);

	P_Menu_UpdateEntry(settings_menu->entries + row++, G_Fmt("time limit: {:2} mins", settings->timelimit).data(), MENU_ALIGN_LEFT, ChangeMatchLength);

	P_Menu_UpdateEntry(settings_menu->entries + row++, G_Fmt("weapons stay: {}", settings->weaponsstay ? "Yes" : "No").data(), MENU_ALIGN_LEFT, ToggleWeaponsStay);

	P_Menu_UpdateEntry(settings_menu->entries + row++, G_Fmt("instant items: {}", settings->instantitems ? "Yes" : "No").data(), MENU_ALIGN_LEFT, ToggleInstantItems);

	P_Menu_UpdateEntry(settings_menu->entries + row++, G_Fmt("powerup drops: {}", settings->pu_drop ? "Yes" : "No").data(), MENU_ALIGN_LEFT, TogglePowerupDrop);

	P_Menu_UpdateEntry(settings_menu->entries + row++, G_Fmt("instant weapon switch: {}", settings->instantweap ? "Yes" : "No").data(), MENU_ALIGN_LEFT, ToggleInstantWeapon);

	P_Menu_UpdateEntry(settings_menu->entries + row++, G_Fmt("match lock: {}", settings->match_lock ? "Yes" : "No").data(), MENU_ALIGN_LEFT, ToggleMatchLock);

	P_Menu_Update(ent);
}

const menu_t settings_menu_template[] = {
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
	{ "$g_pc_return", MENU_ALIGN_LEFT, menu::info::ReturnToMain }
};

void OpenSettings(gentity_t *ent, menu_hnd_t *p)
{
	if (!menu::HasClient(ent) || !ent->client->sess.admin)
		return;

	P_Menu_Close(ent);

	auto *settings = static_cast<AdminSettings *>(gi.TagMalloc(sizeof(AdminSettings), TAG_LEVEL));
	if (!settings)
		return;

	settings->timelimit = menu::NormalizeTimelimit(muffmode::CvarInteger(timelimit));
	settings->weaponsstay = muffmode::CvarEnabled(g_dm_weapons_stay);
	settings->instantitems = muffmode::CvarEnabled(g_dm_instant_items);
	settings->pu_drop = muffmode::CvarEnabled(g_dm_powerup_drop);
	settings->instantweap = muffmode::CvarEnabled(g_instant_weapon_switch);
	settings->match_lock = muffmode::CvarEnabled(g_match_lock);

	menu_hnd_t *settings_menu = P_Menu_Open(ent, settings_menu_template, -1, muffmode::CountAsInt(settings_menu_template), settings, nullptr);
	if (!settings_menu) {
		gi.TagFree(settings);
		return;
	}
	UpdateSettings(ent, settings_menu);
}

void ApplyMatchAction(gentity_t *ent, menu_hnd_t *p)
{
	if (!menu::HasClient(ent) || !ent->client->sess.admin)
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

void Cancel(gentity_t *ent, menu_hnd_t *p)
{
	if (!menu::HasClient(ent))
		return;

	P_Menu_Close(ent);
}

menu_t admin_menu[] = {
	{ "*Administration Menu", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "Settings", MENU_ALIGN_LEFT, OpenSettings },
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
	{ "$g_pc_return", MENU_ALIGN_LEFT, menu::info::ReturnToMain }
};

void Open(gentity_t *ent, menu_hnd_t *p)
{
	if (!menu::HasClient(ent) || !ent->client->sess.admin)
		return;

	menu::SetText(admin_menu[3], "");
	admin_menu[3].SelectFunc = nullptr;
	menu::SetText(admin_menu[4], "");
	admin_menu[4].SelectFunc = nullptr;

	if (level.match_state <= matchst_t::MATCH_COUNTDOWN) {
		menu::SetText(admin_menu[3], "Force start match");
		admin_menu[3].SelectFunc = ApplyMatchAction;

	} else if (level.match_state == matchst_t::MATCH_IN_PROGRESS) {
		menu::SetText(admin_menu[3], "Reset match");
		admin_menu[3].SelectFunc = ApplyMatchAction;
	}

	P_Menu_Close(ent);
	P_Menu_Open(ent, admin_menu, -1, muffmode::CountAsInt(admin_menu), nullptr, nullptr);
}

} // namespace muffmode::menu::admin

/*-----------------------------------------------------------------------*/

namespace muffmode::menu::stats {

struct MenuWriter {
	menu_t *entries = nullptr;
	int     num_entries = 0;
	int     row = 0;

	void Add(std::string_view text)
	{
		if (row < num_entries)
			menu::SetText(entries[row], text);
		row++;
	}
};

const menu_t match_stats_menu[] = {
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
	{ "$g_pc_return", MENU_ALIGN_LEFT, menu::info::ReturnToMain }
};

void Update(gentity_t *ent)
{
	menu_t *entries = nullptr;
	int num_entries = 0;

	if (!muffmode::CvarEnabled(g_matchstats))
		return;
	if (!menu::GetEntries(ent, &entries, &num_entries))
		return;

	// ent->client->mstats (client_match_stats_t) has no writers anywhere in the
	// tree, so this screen always rendered zeros. Source the live per-match
	// counters from resp.mstats[] via MS_Value. Field order matches the struct.
	const client_match_stats_t stats = {
		static_cast<uint32_t>(MS_Value(ent->client, MSTAT_DMG_DEALT)),
		static_cast<uint32_t>(MS_Value(ent->client, MSTAT_DMG_RECEIVED)),
		static_cast<uint32_t>(MS_Value(ent->client, MSTAT_SHOTS)),
		static_cast<uint32_t>(MS_Value(ent->client, MSTAT_HITS)),
		static_cast<uint32_t>(MS_Value(ent->client, MSTAT_KILLS_TOTAL)),
		static_cast<uint32_t>(MS_Value(ent->client, MSTAT_DEATHS_TOTAL)),
	};
	MenuWriter writer{ entries, num_entries };
	std::array<char, MAX_INFO_VALUE> value = {};
	menu::CopyHostPlayerName(value);

	writer.Add("Player Stats for Match");

	if (value[0])
		writer.Add(G_Fmt("{}", value.data()).data());

	writer.Add(menu::kBreaker);

	writer.Add(G_Fmt("kills: {}", stats.total_kills).data());
	writer.Add(G_Fmt("deaths: {}", stats.total_deaths).data());
	if (stats.total_kills) {
		if (stats.total_deaths > 0) {
			const float ratio = static_cast<float>(stats.total_kills) / static_cast<float>(stats.total_deaths);
			writer.Add(G_Fmt("k/d ratio: {:2}", ratio).data());
		} else {
			writer.Add("k/d ratio: N/A");
		}
	}

	writer.Add("");
	writer.Add(G_Fmt("dmg dealt: {}", stats.total_dmg_dealt).data());
	writer.Add(G_Fmt("dmg received: {}", stats.total_dmg_received).data());
	if (stats.total_dmg_dealt) {
		if (stats.total_dmg_received > 0) {
			const float ratio = static_cast<float>(stats.total_dmg_dealt) / static_cast<float>(stats.total_dmg_received);
			writer.Add(G_Fmt("dmg ratio: {:02}", ratio).data());
		} else {
			writer.Add("dmg ratio: N/A");
		}
	}

	writer.Add("");
	writer.Add(G_Fmt("shots fired: {}", stats.total_shots).data());
	writer.Add(G_Fmt("shots on target: {}", stats.total_hits).data());
	if (stats.total_hits) {
		if (stats.total_shots > 0) {
			const int accuracy = static_cast<int>((static_cast<float>(stats.total_hits) / static_cast<float>(stats.total_shots)) * 100.f);
			writer.Add(G_Fmt("total accuracy: {}%", accuracy).data());
		} else {
			writer.Add("total accuracy: N/A");
		}
	}
}

void Open(gentity_t *ent, menu_hnd_t *p)
{
	if (!menu::HasClient(ent))
		return;

	P_Menu_Close(ent);
	P_Menu_Open(ent, match_stats_menu, -1, muffmode::CountAsInt(match_stats_menu), nullptr, Update);
}

} // namespace muffmode::menu::stats

/*-----------------------------------------------------------------------*/

namespace muffmode::menu::player_settings {

void OpenDisplay(gentity_t *ent, menu_hnd_t *p);
void OpenSpectator(gentity_t *ent, menu_hnd_t *p);
void OpenSkins(gentity_t *ent, menu_hnd_t *p);
void ReturnToPlayerConfig(gentity_t *ent, menu_hnd_t *p);
void ToggleShowId(gentity_t *ent, menu_hnd_t *p);
void ToggleTimer(gentity_t *ent, menu_hnd_t *p);
void ToggleMatchInfo(gentity_t *ent, menu_hnd_t *p);
void ToggleFragMessages(gentity_t *ent, menu_hnd_t *p);
void ToggleAnnouncer(gentity_t *ent, menu_hnd_t *p);
void CycleKillBeep(gentity_t *ent, menu_hnd_t *p);
void ToggleFollowView(gentity_t *ent, menu_hnd_t *p);
void ToggleFollowKiller(gentity_t *ent, menu_hnd_t *p);
void ToggleFollowLeader(gentity_t *ent, menu_hnd_t *p);
void ToggleFollowPowerup(gentity_t *ent, menu_hnd_t *p);
void CycleEnemySkin(gentity_t *ent, menu_hnd_t *p);
void CycleTeamSkin(gentity_t *ent, menu_hnd_t *p);

constexpr std::array<const char *, 4> kSkinChoices = {
	"",
	"male/grunt",
	"female/athena",
	"cyborg/oni911"
};

const menu_t kPlayerConfigMenuTemplate[] = {
	{ "*Player Config", MENU_ALIGN_CENTER, nullptr },
	{ "\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37", MENU_ALIGN_CENTER, nullptr },
	{ "*Choose a section", MENU_ALIGN_LEFT, nullptr },
	{ "Display & Audio", MENU_ALIGN_LEFT, OpenDisplay },
	{ "Spectator & Follow", MENU_ALIGN_LEFT, OpenSpectator },
	{ "Skin Overrides", MENU_ALIGN_LEFT, OpenSkins },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "*Changes save automatically", MENU_ALIGN_CENTER, nullptr },
	{ "Move: navigate  Attack: select", MENU_ALIGN_CENTER, nullptr },
	{ "$g_pc_return", MENU_ALIGN_LEFT, menu::info::ReturnToMain }
};

const menu_t kDisplayMenuTemplate[] = {
	{ "*Display & Audio", MENU_ALIGN_CENTER, nullptr },
	{ "\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_LEFT, ToggleShowId },
	{ "", MENU_ALIGN_LEFT, ToggleTimer },
	{ "", MENU_ALIGN_LEFT, ToggleMatchInfo },
	{ "", MENU_ALIGN_LEFT, ToggleFragMessages },
	{ "", MENU_ALIGN_LEFT, ToggleAnnouncer },
	{ "", MENU_ALIGN_LEFT, CycleKillBeep },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "*Voice pack is opt-in", MENU_ALIGN_CENTER, nullptr },
	{ "Changes save automatically", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "$g_pc_return", MENU_ALIGN_LEFT, ReturnToPlayerConfig }
};

const menu_t kSpectatorMenuTemplate[] = {
	{ "*Spectator & Follow", MENU_ALIGN_CENTER, nullptr },
	{ "\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_LEFT, ToggleFollowView },
	{ "", MENU_ALIGN_LEFT, ToggleFollowKiller },
	{ "", MENU_ALIGN_LEFT, ToggleFollowLeader },
	{ "", MENU_ALIGN_LEFT, ToggleFollowPowerup },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "*Used while spectating", MENU_ALIGN_CENTER, nullptr },
	{ "Changes save automatically", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "$g_pc_return", MENU_ALIGN_LEFT, ReturnToPlayerConfig }
};

const menu_t kSkinMenuTemplate[] = {
	{ "*Skin Overrides", MENU_ALIGN_CENTER, nullptr },
	{ "\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_LEFT, CycleEnemySkin },
	{ "", MENU_ALIGN_LEFT, CycleTeamSkin },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "*Custom paths: eskin / tskin", MENU_ALIGN_CENTER, nullptr },
	{ "Changes save automatically", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "$g_pc_return", MENU_ALIGN_LEFT, ReturnToPlayerConfig }
};

int SkinChoiceIndex(std::string_view skin)
{
	for (int i = 0; i < static_cast<int>(kSkinChoices.size()); i++) {
		if (skin == kSkinChoices[i])
			return i;
	}

	return -1;
}

const char *NextSkinChoice(std::string_view skin)
{
	const int index = SkinChoiceIndex(skin);
	if (index < 0)
		return kSkinChoices[0];

	return kSkinChoices[(index + 1) % kSkinChoices.size()];
}

std::string SkinValueText(const char *skin)
{
	if (!skin || !skin[0])
		return "normal";

	if (SkinChoiceIndex(skin) < 0)
		return "custom";

	return skin;
}

void ToggleBool(gentity_t *ent, menu_hnd_t *, mm_pconfig_bool_setting_t setting)
{
	if (!menu::HasClient(ent))
		return;

	if (MM_PConfigToggleBool(ent, setting))
		P_Menu_Update(ent);
}

void ToggleShowId(gentity_t *ent, menu_hnd_t *p)
{
	ToggleBool(ent, p, mm_pconfig_bool_setting_t::show_id);
}

void ToggleTimer(gentity_t *ent, menu_hnd_t *p)
{
	ToggleBool(ent, p, mm_pconfig_bool_setting_t::show_timer);
}

void ToggleMatchInfo(gentity_t *ent, menu_hnd_t *p)
{
	ToggleBool(ent, p, mm_pconfig_bool_setting_t::show_match_info);
}

void ToggleFragMessages(gentity_t *ent, menu_hnd_t *p)
{
	ToggleBool(ent, p, mm_pconfig_bool_setting_t::show_fragmessages);
}

void ToggleAnnouncer(gentity_t *ent, menu_hnd_t *p)
{
	ToggleBool(ent, p, mm_pconfig_bool_setting_t::announcer_enabled);
}

void CycleKillBeep(gentity_t *ent, menu_hnd_t *)
{
	if (!menu::HasClient(ent))
		return;

	if (MM_PConfigCycleKillBeep(ent))
		P_Menu_Update(ent);
}

void ToggleFollowView(gentity_t *ent, menu_hnd_t *p)
{
	ToggleBool(ent, p, mm_pconfig_bool_setting_t::follow_first_person);
}

void ToggleFollowKiller(gentity_t *ent, menu_hnd_t *p)
{
	ToggleBool(ent, p, mm_pconfig_bool_setting_t::follow_killer);
}

void ToggleFollowLeader(gentity_t *ent, menu_hnd_t *p)
{
	ToggleBool(ent, p, mm_pconfig_bool_setting_t::follow_leader);
}

void ToggleFollowPowerup(gentity_t *ent, menu_hnd_t *p)
{
	ToggleBool(ent, p, mm_pconfig_bool_setting_t::follow_powerup);
}

void SetSkinChoice(gentity_t *ent, bool enemy, const char *skin)
{
	if (!menu::HasClient(ent))
		return;

	char *store = enemy ? ent->client->sess.pc.enemy_skin : ent->client->sess.pc.team_skin;
	if (skin && skin[0])
		Q_strlcpy(store, skin, MAX_QPATH);
	else
		store[0] = 0;

	MM_ClientSavePConfigOrWarn(ent);
	MM_RefreshSkinOverridesForViewer(ent);
	P_Menu_Update(ent);
}

void CycleEnemySkin(gentity_t *ent, menu_hnd_t *)
{
	if (!menu::HasClient(ent) || !muffmode::CvarEnabled(g_allow_skin_overrides))
		return;

	SetSkinChoice(ent, true, NextSkinChoice(ent->client->sess.pc.enemy_skin));
}

void CycleTeamSkin(gentity_t *ent, menu_hnd_t *)
{
	if (!menu::HasClient(ent) || !muffmode::CvarEnabled(g_allow_skin_overrides) || GT(GT_DUEL))
		return;

	SetSkinChoice(ent, false, NextSkinChoice(ent->client->sess.pc.team_skin));
}

void SetBoolRow(menu_t *entry, const char *label, mm_pconfig_bool_setting_t setting, SelectFunc_t select, gentity_t *ent)
{
	P_Menu_UpdateEntry(entry, G_Fmt("{}: {}", label, MM_PConfigBoolText(MM_PConfigGetBool(ent, setting))).data(), MENU_ALIGN_LEFT, select);
}

void UpdateDisplay(gentity_t *ent)
{
	menu_t *entries = nullptr;
	int num_entries = 0;
	if (!menu::GetEntries(ent, &entries, &num_entries) || num_entries < muffmode::CountAsInt(kDisplayMenuTemplate))
		return;

	SetBoolRow(entries + 2, "ID display", mm_pconfig_bool_setting_t::show_id, ToggleShowId, ent);
	SetBoolRow(entries + 3, "Match timer", mm_pconfig_bool_setting_t::show_timer, ToggleTimer, ent);
	SetBoolRow(entries + 4, "Match info HUD", mm_pconfig_bool_setting_t::show_match_info, ToggleMatchInfo, ent);
	SetBoolRow(entries + 5, "Frag messages", mm_pconfig_bool_setting_t::show_fragmessages, ToggleFragMessages, ent);
	SetBoolRow(entries + 6, "Voice announcer", mm_pconfig_bool_setting_t::announcer_enabled, ToggleAnnouncer, ent);
	P_Menu_UpdateEntry(entries + 7, G_Fmt("Kill beep: {}", MM_PConfigKillBeepName(ent->client->sess.pc.killbeep_num)).data(), MENU_ALIGN_LEFT, CycleKillBeep);
}

void UpdateSpectator(gentity_t *ent)
{
	menu_t *entries = nullptr;
	int num_entries = 0;
	if (!menu::GetEntries(ent, &entries, &num_entries) || num_entries < muffmode::CountAsInt(kSpectatorMenuTemplate))
		return;

	P_Menu_UpdateEntry(entries + 2,
		G_Fmt("Follow view: {}", MM_PConfigFollowViewName(ent->client->sess.pc.follow_first_person)).data(),
		MENU_ALIGN_LEFT,
		ToggleFollowView);
	SetBoolRow(entries + 3, "Follow killer", mm_pconfig_bool_setting_t::follow_killer, ToggleFollowKiller, ent);
	SetBoolRow(entries + 4, "Follow leader", mm_pconfig_bool_setting_t::follow_leader, ToggleFollowLeader, ent);
	SetBoolRow(entries + 5, "Follow powerup", mm_pconfig_bool_setting_t::follow_powerup, ToggleFollowPowerup, ent);
}

void UpdateSkins(gentity_t *ent)
{
	menu_t *entries = nullptr;
	int num_entries = 0;
	if (!menu::GetEntries(ent, &entries, &num_entries) || num_entries < muffmode::CountAsInt(kSkinMenuTemplate))
		return;

	if (!muffmode::CvarEnabled(g_allow_skin_overrides)) {
		P_Menu_UpdateEntry(entries + 2, "Skin overrides: disabled", MENU_ALIGN_LEFT, nullptr);
		P_Menu_UpdateEntry(entries + 3, "", MENU_ALIGN_LEFT, nullptr);
		return;
	}

	P_Menu_UpdateEntry(entries + 2,
		G_Fmt("{} skin: {}", GT(GT_DUEL) ? "Opp" : "Enemy", SkinValueText(ent->client->sess.pc.enemy_skin)).data(),
		MENU_ALIGN_LEFT,
		CycleEnemySkin);

	if (GT(GT_DUEL)) {
		P_Menu_UpdateEntry(entries + 3, "Team skin: unavailable", MENU_ALIGN_LEFT, nullptr);
	} else {
		P_Menu_UpdateEntry(entries + 3,
			G_Fmt("Team skin: {}", SkinValueText(ent->client->sess.pc.team_skin)).data(),
			MENU_ALIGN_LEFT,
			CycleTeamSkin);
	}
}

void OpenDisplay(gentity_t *ent, menu_hnd_t *)
{
	if (!menu::HasClient(ent))
		return;

	P_Menu_Close(ent);
	P_Menu_Open(ent, kDisplayMenuTemplate, -1, muffmode::CountAsInt(kDisplayMenuTemplate), nullptr, UpdateDisplay);
}

void OpenSpectator(gentity_t *ent, menu_hnd_t *)
{
	if (!menu::HasClient(ent))
		return;

	P_Menu_Close(ent);
	P_Menu_Open(ent, kSpectatorMenuTemplate, -1, muffmode::CountAsInt(kSpectatorMenuTemplate), nullptr, UpdateSpectator);
}

void OpenSkins(gentity_t *ent, menu_hnd_t *)
{
	if (!menu::HasClient(ent))
		return;

	P_Menu_Close(ent);
	P_Menu_Open(ent, kSkinMenuTemplate, -1, muffmode::CountAsInt(kSkinMenuTemplate), nullptr, UpdateSkins);
}

void Open(gentity_t *ent, menu_hnd_t *)
{
	if (!menu::HasClient(ent))
		return;

	P_Menu_Close(ent);
	P_Menu_Open(ent, kPlayerConfigMenuTemplate, -1, muffmode::CountAsInt(kPlayerConfigMenuTemplate), nullptr, nullptr);
}

void ReturnToPlayerConfig(gentity_t *ent, menu_hnd_t *)
{
	Open(ent, nullptr);
	gi.local_sound(ent, CHAN_AUTO, gi.soundindex("misc/menu3.wav"), 1, ATTN_NONE, 0);
}

} // namespace muffmode::menu::player_settings

/*-----------------------------------------------------------------------*/

namespace muffmode::menu::join {

void JoinFree(gentity_t *ent, menu_hnd_t *)
{
	SetTeam(ent, TEAM_FREE, false, false, false);
}

void JoinRed(gentity_t *ent, menu_hnd_t *)
{
	SetTeam(ent, !muffmode::CvarEnabled(g_teamplay_allow_team_pick) ? PickTeam(-1) : TEAM_RED, false, false, false);
}

void JoinBlue(gentity_t *ent, menu_hnd_t *)
{
	if (!muffmode::CvarEnabled(g_teamplay_allow_team_pick))
		return;

	SetTeam(ent, TEAM_BLUE, false, false, false);
}

void JoinSpectator(gentity_t *ent, menu_hnd_t *)
{
	SetTeam(ent, TEAM_SPECTATOR, false, false, false);
}

void ToggleReady(gentity_t *ent, menu_hnd_t *)
{
	MM_ToggleReadyUp(ent);
}

constexpr int kGametypeName = 0;
constexpr int kLevel = 1;
constexpr int kMatch = 2;

constexpr int kTeamsJoinRed = 4;
constexpr int kTeamsJoinBlue = 5;
constexpr int kTeamsFollow = 8;
constexpr int kTeamsReadyUp = 9;
constexpr int kTeamsPlayerSettings = 11;
constexpr int kTeamsCallVote = 12;
constexpr int kTeamsPlayerStats = 13;
constexpr int kTeamsAdmin = 16;

constexpr int kFreeJoin = 4;
constexpr int kFreeFollow = 8;
constexpr int kFreeReadyUp = 9;
constexpr int kFreePlayerSettings = 11;
constexpr int kFreeCallVote = 12;
constexpr int kFreePlayerStats = 13;
constexpr int kFreeAdmin = 16;

constexpr int kGameMod = 18;
constexpr int kNotice = 19;

const menu_t kTeamsMenuTemplate[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37", MENU_ALIGN_CENTER, nullptr },
	{ "$g_pc_join_red_team", MENU_ALIGN_LEFT, JoinRed },
	{ "$g_pc_join_blue_team", MENU_ALIGN_LEFT, JoinBlue },
	{ "Spectate", MENU_ALIGN_LEFT, JoinSpectator },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "Follow Player", MENU_ALIGN_LEFT, menu::info::OpenFollowCamera },
	{ "", MENU_ALIGN_LEFT, nullptr },  // Ready Up (set dynamically)
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "Player Config", MENU_ALIGN_LEFT, menu::player_settings::Open },
	{ "Call a Vote", MENU_ALIGN_LEFT, ::G_Menu_CallVote },
	{ "Player Stats", MENU_ALIGN_LEFT, menu::stats::Open },
	{ "Server Info", MENU_ALIGN_LEFT, menu::info::OpenHost },
	{ "Match Info", MENU_ALIGN_LEFT, menu::info::OpenServer },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "Move: navigate  Attack: select", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr }
};

const menu_t kFreeMenuTemplate[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37", MENU_ALIGN_CENTER, nullptr },
	{ "Join Game", MENU_ALIGN_LEFT, JoinFree },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "Spectate", MENU_ALIGN_LEFT, JoinSpectator },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "Follow Player", MENU_ALIGN_LEFT, menu::info::OpenFollowCamera },
	{ "", MENU_ALIGN_LEFT, nullptr },  // Ready Up (set dynamically)
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "Player Config", MENU_ALIGN_LEFT, menu::player_settings::Open },
	{ "Call a Vote", MENU_ALIGN_LEFT, ::G_Menu_CallVote },
	{ "Player Stats", MENU_ALIGN_LEFT, menu::stats::Open },
	{ "Server Info", MENU_ALIGN_LEFT, menu::info::OpenHost },
	{ "Match Info", MENU_ALIGN_LEFT, menu::info::OpenServer },
	{ "", MENU_ALIGN_LEFT, nullptr },
	{ "Move: navigate  Attack: select", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr }
};

static_assert(std::size(kTeamsMenuTemplate) == 20);
static_assert(std::size(kFreeMenuTemplate) == 20);

} // namespace muffmode::menu::join

namespace muffmode::menu::info {

const menu_t kNoFollowMenuTemplate[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "No players to follow", MENU_ALIGN_LEFT, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "$g_pc_return", MENU_ALIGN_LEFT, ReturnToMain }
};

const menu_t kHostMenuTemplate[] = {
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
	{ "$g_pc_return", MENU_ALIGN_LEFT, ReturnToMain }
};

const menu_t kServerMenuTemplate[] = {
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
	{ "$g_pc_return", MENU_ALIGN_LEFT, ReturnToMain }
};

void UpdateNoFollow(gentity_t *ent)
{
	menu_t *entries = nullptr;
	int num_entries = 0;
	if (!menu::GetEntries(ent, &entries, &num_entries) || menu::ContentLimit(num_entries) < 3)
		return;

	menu::SetGamemodName(&entries[0]);
	menu::SetGametypeName(&entries[1]);
	menu::SetLevelName(&entries[2]);
}

void OpenFollowCamera(gentity_t *ent, menu_hnd_t *)
{
	if (!menu::HasClient(ent))
		return;

	SetTeam(ent, TEAM_SPECTATOR, false, false, false);

	if (ent->client->follow_target) {
		FreeFollower(ent);
		P_Menu_Close(ent);
		return;
	}

	GetFollowTarget(ent);

	P_Menu_Close(ent);
	if (ent->client->follow_target)
		return;

	P_Menu_Open(ent, kNoFollowMenuTemplate, -1, muffmode::CountAsInt(kNoFollowMenuTemplate), nullptr, UpdateNoFollow);
}

void ReturnToMain(gentity_t *ent, menu_hnd_t *)
{
	if (!menu::HasClient(ent))
		return;

	P_Menu_Close(ent);
	menu::join::Open(ent);
	gi.local_sound(ent, CHAN_AUTO, gi.soundindex("misc/menu3.wav"), 1, ATTN_NONE, 0);
}

void UpdateHost(gentity_t *ent)
{
	menu_t *entries = nullptr;
	int num_entries = 0;
	if (!menu::GetEntries(ent, &entries, &num_entries))
		return;
	const int content_limit = menu::ContentLimit(num_entries);

	int		i = 0;

	const char *server_name = muffmode::CvarString(hostname);
	if (server_name[0] && i + 2 < content_limit) {
		menu::SetText(entries[i], "Server Name:");
		i++;
		menu::SetText(entries[i], server_name);
		i++;
		i++;
	}

	if (i + 2 < content_limit) {
		std::array<char, MAX_INFO_VALUE> value = {};

		if (menu::CopyHostPlayerName(value)) {
			menu::SetText(entries[i], "Host:");
			i++;
			menu::SetText(entries[i], value.data());
			i++;
			i++;
		}
	}

	if (game.motd.size() && i < content_limit) {
		menu::SetText(entries[i], "Message of the Day:");
		i++;
		// 26 char line width
		// 9 lines
		// = 234

		if (i < content_limit)
			menu::SetText(entries[i], G_Fmt("{}", game.motd.c_str()).data());
	}
}

void OpenHost(gentity_t *ent, menu_hnd_t *)
{
	if (!menu::HasClient(ent))
		return;

	P_Menu_Close(ent);
	P_Menu_Open(ent, kHostMenuTemplate, -1, muffmode::CountAsInt(kHostMenuTemplate), nullptr, UpdateHost);
}

void UpdateServer(gentity_t *ent)
{
	menu_t *entries = nullptr;
	int num_entries = 0;
	if (!menu::GetEntries(ent, &entries, &num_entries))
		return;
	const int content_limit = menu::ContentLimit(num_entries);
	if (content_limit < 3)
		return;

	int		i = 0;
	bool	limits = false;
	bool	infiniteammo = InfiniteAmmoOn(nullptr);
	bool	items = ItemSpawnsEnabled();
	int		scorelimit = GT_ScoreLimit();
	
	menu::SetText(entries[i], "Match Info");
	i++;

	menu::SetText(entries[i], menu::kBreaker);
	i++;

	menu::SetText(entries[i], level.gametype_name);
	i++;
	
	if (level.level_name[0]) {
		menu::SetText(entries[i], G_Fmt("map: {}", level.level_name).data());
		i++;
	}
	if (level.mapname[0]) {
		menu::SetText(entries[i], G_Fmt("mapname: {}", level.mapname).data());
		i++;
	}
	if (level.author[0]) {
		menu::SetText(entries[i], G_Fmt("author: {}", level.author).data());
		i++;
	}
	if (level.author2[0] && level.author[0]) {
		menu::SetText(entries[i], G_Fmt("      {}", level.author2).data());
		i++;
	}

	menu::SetText(entries[i], G_Fmt("ruleset: {}", menu::CurrentRulesetName()).data());
	i++;

	if (scorelimit) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], G_Fmt("{} limit: {}", GT_ScoreLimitString(), scorelimit).data());
		i++;
		limits = true;
	}

	if (timelimit->value > 0) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], G_Fmt("time limit: {}", G_TimeString(timelimit->value * 60000, false)).data());
		i++;
		limits = true;
	}

	if (limits) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], menu::kBreaker);
		i++;
	}

	if (g_instagib->integer || GT(GT_INSTAGIB)) {
		if (i >= content_limit) return;
		if (g_instagib_splash->integer) {
			menu::SetText(entries[i], "InstaGib + Rail Splash");
		} else {
			menu::SetText(entries[i], "InstaGib");
		}
		i++;
	}
	if (g_vampiric_damage->integer) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], "Vampiric Damage");
		i++;
	}
	if (g_frenzy->integer) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], "Weapons Frenzy");
		i++;
	}
	if (g_nadefest->integer || GT(GT_NADEFEST)) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], "Nade Fest");
		i++;
	}
	if (g_quadhog->integer) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], "Quad Hog");
		i++;
	}

	if (i >= content_limit) return;
	menu::SetText(entries[i], menu::kBreaker);
	i++;

	if (items) {
		if (g_dm_weapons_stay->integer) {
			if (i >= content_limit) return;
			menu::SetText(entries[i], "weapons stay");
			i++;
		} else {
			if (g_weapon_respawn_time->integer != 30) {
				if (i >= content_limit) return;
				menu::SetText(entries[i], G_Fmt("weapon respawn delay: {}", g_weapon_respawn_time->integer).data());
				i++;
			}
		}
	}

	if (g_infinite_ammo->integer && !infiniteammo) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], "infinite ammo");
		i++;
	}
	if (Teams() && g_friendly_fire->integer) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], "friendly fire");
		i++;
	}

	if (g_allow_grapple->integer) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], G_Fmt("{}grapple enabled", g_grapple_offhand->integer ? "off-hand " : "").data());
		i++;
	}

	if (g_inactivity->integer > 0) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], G_Fmt("inactivity timer: {} sec", g_inactivity->integer).data());
		i++;
	}

	if (g_teleporter_freeze->integer) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], "teleporter freeze");
		i++;
	}

	if (Teams() && g_teamplay_force_balance->integer && notGT(GT_RR)) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], "forced team balancing");
		i++;
	}

	if (g_dm_random_items->integer && items) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], "random items");
		i++;
	}

	if (g_dm_force_join->integer) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], "forced game joining");
		i++;
	}

	if (!g_dm_powerup_drop->integer) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], "no powerup drops");
		i++;
	}

	if (g_knockback_scale->value != 1) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], G_Fmt("knockback scale: {}", g_knockback_scale->value).data());
		i++;
	}

	if (g_dm_no_self_damage->integer) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], "no self-damage");
		i++;
	}

	if (g_dm_no_fall_damage->integer) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], "no falling damage");
		i++;
	}

	if (!g_dm_instant_items->integer) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], "no instant items");
		i++;
	}

	if (items) {
		if (i >= content_limit) return;
		if (g_no_items->integer) {
			menu::SetText(entries[i], "no items");
			i++;
		} else {
			if (i >= content_limit) return;
			if (g_no_health->integer) {
				menu::SetText(entries[i], "no health spawns");
				i++;
			}

			if (i >= content_limit) return;
			if (g_no_armor->integer) {
				menu::SetText(entries[i], "no armor spawns");
				i++;
			}

			if (i >= content_limit) return;
			if (g_no_mines->integer) {
				menu::SetText(entries[i], "no mines");
				i++;
			}
		}
	}

	if (g_dm_allow_exit->integer) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], "allow exiting");
		i++;
	}

	if (g_mover_speed_scale->value != 1.0f) {
		if (i >= content_limit) return;
		menu::SetText(entries[i], G_Fmt("mover speed scale: {}", g_mover_speed_scale->value).data());
		i++;
	}

}

void OpenServer(gentity_t *ent, menu_hnd_t *)
{
	if (!menu::HasClient(ent))
		return;

	P_Menu_Close(ent);
	P_Menu_Open(ent, kServerMenuTemplate, -1, muffmode::CountAsInt(kServerMenuTemplate), nullptr, UpdateServer);
}

void UpdateGameRules(gentity_t *ent)
{
	menu_t *entries = nullptr;
	int num_entries = 0;
	if (!menu::GetEntries(ent, &entries, &num_entries) || menu::ContentLimit(num_entries) < 3)
		return;

	int		i = 0;

	menu::SetText(entries[i], "Game Rules"); i++;
	menu::SetText(entries[i], menu::kBreaker); i++;
	menu::SetText(entries[i], G_Fmt("{}", level.gametype_name).data()); i++;
}

void OpenGameRules(gentity_t *ent, menu_hnd_t *)
{
	if (!menu::HasClient(ent))
		return;

	P_Menu_Close(ent);
	P_Menu_Open(ent, kServerMenuTemplate, -1, muffmode::CountAsInt(kServerMenuTemplate), nullptr, UpdateGameRules);
}

} // namespace muffmode::menu::info

void G_Menu_ReturnToMain(gentity_t *ent, menu_hnd_t *p)
{
	menu::info::ReturnToMain(ent, p);
}

namespace muffmode::menu::join {

void Update(gentity_t *ent)
{
	menu_t *entries = nullptr;
	int num_entries = 0;
	if (!menu::GetEntries(ent, &entries, &num_entries) || num_entries <= kNotice)
		return;

	const int max_players = max(1, muffmode::CvarInteger(maxplayers));
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

	const bool teams = Teams();
	if (teams) {
		if (!muffmode::CvarEnabled(g_teamplay_allow_team_pick) && !level.locked[TEAM_RED] && !level.locked[TEAM_BLUE]) {
			menu::SetText(entries[kTeamsJoinRed], G_Fmt("Join a Team ({}/{})", num_red + num_blue, max_players).data());
			menu::SetText(entries[kTeamsJoinBlue], "");

			entries[kTeamsJoinRed].SelectFunc = JoinRed;
			entries[kTeamsJoinBlue].SelectFunc = nullptr;
		} else {
			if (level.locked[TEAM_RED]) {
				menu::SetText(entries[kTeamsJoinRed], G_Fmt("{} is LOCKED", Teams_TeamName(TEAM_RED)).data());
				entries[kTeamsJoinRed].SelectFunc = nullptr;
			} else {
				menu::SetText(entries[kTeamsJoinRed], G_Fmt("Join {} ({}/{})", Teams_TeamName(TEAM_RED), num_red, max_players / 2).data());
				entries[kTeamsJoinRed].SelectFunc = JoinRed;
			}
			if (level.locked[TEAM_BLUE]) {
				menu::SetText(entries[kTeamsJoinBlue], G_Fmt("{} is LOCKED", Teams_TeamName(TEAM_BLUE)).data());
				entries[kTeamsJoinBlue].SelectFunc = nullptr;
			} else {
				menu::SetText(entries[kTeamsJoinBlue], G_Fmt("Join {} ({}/{})", Teams_TeamName(TEAM_BLUE), num_blue, max_players / 2).data());
				entries[kTeamsJoinBlue].SelectFunc = JoinBlue;
			}

		}
	} else {
		// Allow duel queue joining even during match lock (queue joining doesn't affect active match)
		const bool is_duel_queue_join = GT(GT_DUEL) && level.num_playing_clients == 2;
		if (level.locked[TEAM_FREE] && !is_duel_queue_join) {
			menu::SetText(entries[kFreeJoin], "Match LOCKED");
			entries[kFreeJoin].SelectFunc = nullptr;
		} else if (GT(GT_DUEL) && level.num_playing_clients == 2) {
			menu::SetText(entries[kFreeJoin], G_Fmt("Join Queue to Play ({}/{})", num_queue, max(0, max_players - 2)).data());
			entries[kFreeJoin].SelectFunc = JoinFree;
		} else {
			menu::SetText(entries[kFreeJoin], G_Fmt("Join Match ({}/{})", num_free, GT(GT_DUEL) ? 2 : max_players).data());
			entries[kFreeJoin].SelectFunc = JoinFree;
		}
	}

	if (!muffmode::CvarEnabled(g_matchstats)) {
		int index = teams ? kTeamsPlayerStats : kFreePlayerStats;
		menu::SetText(entries[index], "");
		entries[index].SelectFunc = nullptr;
	} else {
		int index = teams ? kTeamsPlayerStats : kFreePlayerStats;
		menu::SetText(entries[index], "Player Stats");
		entries[index].SelectFunc = menu::stats::Open;
	}

	if (!muffmode::CvarEnabled(g_allow_voting)) {
		int index = teams ? kTeamsCallVote : kFreeCallVote;
		menu::SetText(entries[index], "");
		entries[index].SelectFunc = nullptr;
	} else {
		int index = teams ? kTeamsCallVote : kFreeCallVote;
		menu::SetText(entries[index], "Call a Vote");
		entries[index].SelectFunc = ::G_Menu_CallVote;
	}

	const char *force_join = muffmode::CvarString(g_dm_force_join);
	if (force_join[0]) {
		if (teams) {
			if (muffmode::CStringEqualsI(force_join, "red")) {
				menu::SetText(entries[kTeamsJoinBlue], "");
				entries[kTeamsJoinBlue].SelectFunc = nullptr;
			} else if (muffmode::CStringEqualsI(force_join, "blue")) {
				menu::SetText(entries[kTeamsJoinRed], "");
				entries[kTeamsJoinRed].SelectFunc = nullptr;
			}
		}
	}

	int index = teams ? kTeamsFollow : kFreeFollow;
	if (ent->client->follow_target)
		menu::SetText(entries[index], "Stop Following");
	else
		menu::SetText(entries[index], "Follow Player");

	index = teams ? kTeamsReadyUp : kFreeReadyUp;
	if (muffmode::CvarEnabled(g_dm_do_readyup) && level.match_state == matchst_t::MATCH_WARMUP_READYUP) {
		menu::SetText(entries[index], ent->client->resp.ready ? "Not Ready" : "Ready Up");
		entries[index].SelectFunc = ToggleReady;
	} else {
		menu::SetText(entries[index], "");
		entries[index].SelectFunc = nullptr;
	}

	menu::SetGametypeName(entries + kGametypeName);
	menu::SetLevelName(entries + kLevel);

	// Match status text removed from this menu -- crowded the join options below it.
	menu::SetText(entries[kMatch], "");

	menu::SetGamemodName(entries + kGameMod);

	int player_settings_index = teams ? kTeamsPlayerSettings : kFreePlayerSettings;
	menu::SetText(entries[player_settings_index], "Player Config");
	entries[player_settings_index].align = MENU_ALIGN_LEFT;
	entries[player_settings_index].SelectFunc = menu::player_settings::Open;

	int admin_index = teams ? kTeamsAdmin : kFreeAdmin;
	if (ent->client->sess.admin) {
		menu::SetText(entries[admin_index], "Admin");
		entries[admin_index].align = MENU_ALIGN_LEFT;
		entries[admin_index].SelectFunc = menu::admin::Open;
	} else {
		menu::SetText(entries[admin_index], "");
		entries[admin_index].SelectFunc = nullptr;
	}

	menu::SetText(entries[kNotice], "www.darkmatter-quake.com");
}

void Open(gentity_t *ent)
{
	if (!menu::HasClient(ent))
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

		P_Menu_Open(ent, kTeamsMenuTemplate, team, muffmode::CountAsInt(kTeamsMenuTemplate), nullptr, Update);
	} else {
		P_Menu_Open(ent, kFreeMenuTemplate, TEAM_FREE, muffmode::CountAsInt(kFreeMenuTemplate), nullptr, Update);
	}
}

} // namespace muffmode::menu::join

void G_Menu_Join_Open(gentity_t *ent)
{
	menu::join::Open(ent);
}
