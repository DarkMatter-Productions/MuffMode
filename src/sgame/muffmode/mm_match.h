// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;

// [MuffMode] Match/round state machine and timeout system. Implemented in muffmode/mm_match.cpp.
// Vanilla adapter names retained for high-fan-in call sites.
void Match_Start();
void Match_Reset();
void Round_End();
void MM_Match_RunFrame();
void TimeoutEnd();
void MM_TimeoutBeginResumeCountdown();
void MM_CmdTimeOut(gentity_t *ent);
void MM_CmdTimeIn(gentity_t *ent);

namespace muffmode::match {
void GetWarmupReadyCounts(int &ready_humans, int &playing_humans);
void SendWarmupReadyReminder(gentity_t *ent);
void BroadcastWarmupWaitNotice();
} // namespace muffmode::match
