// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "shared/q_vec3.h"

namespace muffmode::combat_heatmap {

void Init();
void ResetForNewLevel();
void RunFrame();
void AddEvent(const vec3_t &origin, float amount);
float Query(const vec3_t &origin, float radius);
float DangerAt(const vec3_t &origin);

} // namespace muffmode::combat_heatmap
