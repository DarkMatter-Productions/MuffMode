// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;
enum team_t;

// [MuffMode] Captain, team lock and readyup systems. Implemented in muffmode/mm_captain.cpp.
void SetCaptain(team_t team, gentity_t *ent);
void VacateCaptain(team_t team, gentity_t *leaving);
void ValidateCaptains();
void ReadyAll();
void UnReadyAll();
void BroadcastReadyReminderMessage();
bool MM_CaptainEligible(const gentity_t *ent);
void MM_CmdCaptain(gentity_t *ent);
void MM_CmdLockTeam(gentity_t *ent);
void MM_CmdUnlockTeam(gentity_t *ent);
void MM_CmdReady(gentity_t *ent);
void MM_CmdNotReady(gentity_t *ent);
void MM_ToggleReadyUp(gentity_t *ent);
void MM_CmdReadyUp(gentity_t *ent);
void MM_CmdReadyAll(gentity_t *ent);
void MM_CmdUnReadyAll(gentity_t *ent);
void MM_CmdReadyTeam(gentity_t *ent);
