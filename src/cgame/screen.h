// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "cgame_local.h"

void CG_InitScreen();
void CG_DrawHUD(int32_t isplit, const cg_server_data_t *data, vrect_t hud_vrect, vrect_t hud_safe, int32_t scale, int32_t playernum, const player_state_t *ps);
void CG_TouchPics();
layout_flags_t CG_LayoutFlags(const player_state_t *ps);
