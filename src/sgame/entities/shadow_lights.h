// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "g_local.h"

const shadow_light_data_t *GetShadowLightData(int32_t entity_number);
void setup_shadow_lights();
void G_LoadShadowLights();

void SP_dynamic_light(gentity_t *self);
void SP_light(gentity_t *self);
