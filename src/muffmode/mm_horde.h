// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstdint>

struct gclient_t;

// [MuffMode] GT_HORDE wave spawning and scoring.
void MM_Horde_Init();
void MM_Horde_RunSpawning();
void MM_Horde_BeginWave();
void MM_Horde_AdjustPlayerScore(gclient_t *cl, int32_t offset);
