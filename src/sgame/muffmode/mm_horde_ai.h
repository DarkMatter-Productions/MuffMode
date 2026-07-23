// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "g_local.h"

namespace muffmode::horde {

void ResetRuntimeState();
void RefreshTargetLoadCache();
float AdaptivePressureMult();
void Adaptive_BeginWave();
void Adaptive_RecordWaveEnd();
void Adaptive_RecordPlayerDeath();

select_spawn_result_t SelectSpawnPoint(vec3_t avoid_point, const vec3_t &check_mins,
	const vec3_t &check_maxs, gentity_t *const *allowed_spots = nullptr,
	size_t allowed_count = 0, bool restrict_to_allowed = false);
void ApplySpawnRoleTuning(gentity_t *ent, const char *classname);

gentity_t *PickTarget(gentity_t *from);
bool MaybeRetarget(gentity_t *monster);

} // namespace muffmode::horde
