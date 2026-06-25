// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstddef>

struct gentity_t;

// [MuffMode] Match ghost system: rejoin a match in progress with state intact.
void MM_Ghost_ClearAll();
void MM_Ghost_Assign(gentity_t *ent);
void MM_Ghost_DoAssign(gentity_t *ent);
bool MM_Ghost_CaptureDisconnect(gentity_t *ent);
void MM_Ghost_MakeDisconnectPlaceholder(gentity_t *ent);
gentity_t *MM_Ghost_ChooseReconnectSlot(const char *social_id, gentity_t **ignore, size_t num_ignore);
bool MM_Ghost_IsReservedSlot(gentity_t *slot);
bool MM_Ghost_HasActiveReservations();
bool MM_Ghost_TryRestore(gentity_t *ent);
void MM_Ghost_DropTimedOutFlags();
void MM_Ghost_RunFrame();
void MM_CmdGhost(gentity_t *ent);
