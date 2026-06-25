// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "g_local.h"

void TeleportPlayer(gentity_t *player, vec3_t origin, vec3_t angles);

void SP_misc_teleporter(gentity_t *ent);
void SP_misc_teleporter_dest(gentity_t *ent);
