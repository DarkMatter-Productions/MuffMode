// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "cgame_local.h"

void CG_InitMessages();

void CG_DrawNotify(int32_t isplit, vrect_t hud_vrect, vrect_t hud_safe, int32_t scale, const player_state_t *ps);
void CG_CheckDrawCenterString(const player_state_t *ps, const vrect_t &hud_vrect, const vrect_t &hud_safe, int isplit, int scale);

void CG_ClearCenterprint(int32_t isplit);
void CG_ClearNotify(int32_t isplit);
void CG_ParseCenterPrint(const char *str, int isplit, bool instant);
void CG_NotifyMessage(int32_t isplit, const char *msg, bool is_chat);
