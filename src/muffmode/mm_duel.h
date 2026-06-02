// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

// [MuffMode] GT_DUEL queue and match-end handling.
bool MM_Duel_AddPlayer();
void MM_Duel_RemoveLoser();
void MM_Duel_MatchEnd_AdjustScores();
void MM_Duel_QueueSpectatorBots();
