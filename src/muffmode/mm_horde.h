// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstdint>

struct gclient_t;

// [MuffMode] GT_HORDE wave spawning, scoring, and round/match orchestration.
void MM_Horde_Init();
void MM_Horde_RunSpawning();
void MM_Horde_BeginWave();
void MM_Horde_AdjustPlayerScore(gclient_t *cl, int32_t offset);

bool MM_Horde_ShouldSkipEntitiesReset();
int  MM_Horde_CountdownWaveNumber();
void MM_Horde_AdvanceRoundNumber();
void MM_Horde_OnRoundStarted();
void MM_Horde_OnRoundEnd();

// Returns true when the wave is cleared (caller should invoke Round_End).
bool MM_Horde_UpdateRoundInProgress();

// CheckDMExitRules hooks; return true when intermission was queued.
bool MM_Horde_CheckOverrun();
bool MM_Horde_CheckMatchEnd();

bool MM_Horde_SkipFragScoreLimit();
bool MM_Horde_SkipMercyLimit();
