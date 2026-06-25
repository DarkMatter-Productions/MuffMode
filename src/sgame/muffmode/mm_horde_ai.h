// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "g_local.h"

namespace muffmode::horde {

void RefreshTargetLoadCache();
float AdaptivePressureMult();
void Adaptive_BeginWave();
void Adaptive_RecordWaveEnd();
void Adaptive_RecordPlayerDeath();

select_spawn_result_t SelectSpawnPoint(vec3_t avoid_point);
void ApplySpawnRoleTuning(gentity_t *ent, const char *classname);

gentity_t *PickTarget(gentity_t *from);

} // namespace muffmode::horde
