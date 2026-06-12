// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;

// [MuffMode] GT_DUEL queue and match-end handling.
bool MM_Duel_AddPlayer();
void MM_Duel_RemoveLoser();
void MM_Duel_MatchEnd_AdjustScores();
void MM_Duel_QueueSpectatorBots();
void MM_Duel_ScoreboardMessage(gentity_t *ent, gentity_t *killer);
void MM_Duel_CmdForfeit(gentity_t *ent);
