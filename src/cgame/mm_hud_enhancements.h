// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "cgame_local.h"

// MM client HUD hook after notify. Layout parse is authoritative for stock/MM parity.
void CG_DrawMuffModeHudEnhancements(const player_state_t *ps, vrect_t hud_vrect, vrect_t hud_safe, int32_t scale, int32_t playernum);
