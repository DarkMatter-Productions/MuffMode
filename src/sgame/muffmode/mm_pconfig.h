// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "mm_client_profile.h"

struct gentity_t;
struct client_config_t;

enum class mm_pconfig_bool_setting_t {
	show_id,
	show_timer,
	show_match_info,
	show_fragmessages,
	announcer_enabled,
	follow_first_person,
	follow_killer,
	follow_leader,
	follow_powerup
};

// [MuffMode] Per-player config persistence and preference commands.
client_config_t MM_DefaultClientConfig();
void MM_ClientInitPConfig(gentity_t *ent);
bool MM_ClientSavePConfig(gentity_t *ent,
	mm_client_profile_preference_mask_t dirty_mask =
		MM_CLIENT_PROFILE_PREFERENCE_ALL);
void MM_ClientSavePConfigOrWarn(gentity_t *ent,
	mm_client_profile_preference_mask_t dirty_mask =
		MM_CLIENT_PROFILE_PREFERENCE_ALL);
bool MM_PConfigGetBool(const gentity_t *ent, mm_pconfig_bool_setting_t setting);
bool MM_PConfigSetBool(gentity_t *ent, mm_pconfig_bool_setting_t setting, bool value);
bool MM_PConfigToggleBool(gentity_t *ent, mm_pconfig_bool_setting_t setting);
const char *MM_PConfigBoolText(bool value);
const char *MM_PConfigFollowViewName(bool first_person);
const char *MM_PConfigKillBeepName(int value);
int MM_PConfigKillBeepCount();
bool MM_PConfigSetKillBeep(gentity_t *ent, int value);
bool MM_PConfigCycleKillBeep(gentity_t *ent);
void MM_CmdCrosshairID(gentity_t *ent);
void MM_CmdTimer(gentity_t *ent);
void MM_CmdInfoHud(gentity_t *ent);
void MM_CmdFragMessages(gentity_t *ent);
void MM_CmdAnnouncer(gentity_t *ent);
void MM_CmdKillBeep(gentity_t *ent);
void MM_CmdFollowView(gentity_t *ent);
void MM_CmdFollowKiller(gentity_t *ent);
void MM_CmdFollowLeader(gentity_t *ent);
void MM_CmdFollowPowerup(gentity_t *ent);
