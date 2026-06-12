// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;

// [MuffMode] Match ghost system: rejoin a match in progress with score intact.
void MM_Ghost_Assign(gentity_t *ent);
void MM_Ghost_DoAssign(gentity_t *ent);
void MM_CmdGhost(gentity_t *ent);
