// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

enum team_t;

// [MuffMode] GT_STRIKE Threewave-style team scoring and gametype defaults.
void MM_Strike_Init();

team_t MM_Strike_AttackingTeam();
team_t MM_Strike_DefendingTeam();

void MM_Strike_AwardTouchBonus();
void MM_Strike_AwardTurnWin(bool captured);
void MM_Strike_EndDefenseTurn(bool timeout);
void MM_Strike_EndMutualElimination();
void MM_Strike_CheckMatchEnd();
