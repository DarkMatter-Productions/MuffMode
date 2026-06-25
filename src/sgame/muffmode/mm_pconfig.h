// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;

// [MuffMode] Per-player config persistence and preference commands.
void MM_ClientInitPConfig(gentity_t *ent);
void MM_CmdCrosshairID(gentity_t *ent);
void MM_CmdTimer(gentity_t *ent);
void MM_CmdFragMessages(gentity_t *ent);
void MM_CmdAnnouncer(gentity_t *ent);
void MM_CmdKillBeep(gentity_t *ent);
