// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;

// [MuffMode] Admin command bodies. g_cmds.cpp keeps command-table wrappers.
void MM_CmdDoctor(gentity_t *ent);
void MM_CmdGametype(gentity_t *ent);
void MM_CmdRuleset(gentity_t *ent);
void MM_CmdSetMap(gentity_t *ent);
void MM_CmdHandicap(gentity_t *ent);
void MM_CmdHandicapClear(gentity_t *ent);
