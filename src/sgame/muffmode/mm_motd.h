// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;

// [MuffMode] Message-of-the-day load and client commands.
void MM_LoadMOTD();
void MM_CmdLoadMotd(gentity_t *ent);
void MM_CmdMotd(gentity_t *ent);
