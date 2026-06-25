// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;

// [MuffMode] Admin command bodies. sgame/core/commands.cpp keeps command-table wrappers.
void MM_CmdDoctor(gentity_t *ent);
void MM_CmdGametype(gentity_t *ent);
void MM_CmdRuleset(gentity_t *ent);
void MM_CmdSetMap(gentity_t *ent);
void MM_CmdStartMatch(gentity_t *ent);
void MM_CmdEndMatch(gentity_t *ent);
void MM_CmdResetMatch(gentity_t *ent);
void MM_CmdForceVote(gentity_t *ent);
void MM_CmdMapRestart(gentity_t *ent);
void MM_CmdNextMap(gentity_t *ent);
void MM_CmdAdmin(gentity_t *ent);
