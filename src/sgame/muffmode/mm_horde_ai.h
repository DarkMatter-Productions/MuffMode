// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "g_local.h"

namespace muffmode::horde {

void ResetRuntimeState();
void RefreshTargetLoadCache();
void HarvestReachEvidence(gentity_t *monster);
// True when origin is in the PHS of at least one living, non-eliminated fighter
// (respects closed area portals). Defined in mm_horde.cpp.
bool OriginSharesFighterPHS(const vec3_t &origin);
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
void DrivePursuit(gentity_t *monster);

} // namespace muffmode::horde
