// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;
enum team_t;

// [MuffMode] Team management core. Implemented in muffmode/mm_team.cpp.
// Vanilla adapter names retained for high-fan-in call sites.
team_t PickTeam(int ignore_client_num);
void BroadcastTeamChange(gentity_t *ent, int old_team, bool inactive, bool silent);
bool AllowClientTeamSwitch(gentity_t *ent);
int TeamBalance(bool force);
bool TeamShuffle();
bool SetTeam(gentity_t *ent, team_t desired_team, bool inactive, bool force, bool silent);
int PlayerSortByJoinTime(const void *a, const void *b);
void MM_CmdTeam(gentity_t *ent);
void MM_CmdSetTeam(gentity_t *ent);
void MM_CmdShuffle(gentity_t *ent);
void MM_CmdBalanceTeams(gentity_t *ent);
