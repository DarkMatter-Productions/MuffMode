// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstddef>

struct gentity_t;
enum team_t;

constexpr int MM_EffectiveMatchMinPlayers(
	bool duel, int configured_minimum) noexcept
{
	return duel ? 2 : (configured_minimum > 1 ? configured_minimum : 1);
}

constexpr bool MM_SingletonMatchRosterCanStart(
	bool duel, size_t occupied_slots) noexcept
{
	return duel ? occupied_slots == 2 : occupied_slots > 0;
}

// [MuffMode] Match/round state machine and timeout system. Implemented in muffmode/mm_match.cpp.
// Vanilla adapter names retained for high-fan-in call sites.
bool Match_Start();
void Match_Reset();
void Round_End();
void MM_Match_RunFrame();
void TimeoutEnd();
void MM_TimeoutBeginResumeCountdown();
void MM_CmdTimeOut(gentity_t *ent);
void MM_CmdTimeIn(gentity_t *ent);

namespace muffmode::match {
int EffectiveMinPlayers();
bool IsTeamLocked(team_t team);
bool IsTeamManuallyLocked(team_t team);
bool IsTeamAutomaticallyLocked(team_t team);
void SetManualTeamLocked(team_t team, bool locked);
void SetAutomaticTeamLocked(team_t team, bool locked);
void ReconcileAutomaticMatchLocks();
void GetWarmupReadyCounts(int &ready_humans, int &playing_humans);
void SendWarmupReadyReminder(gentity_t *ent);
void BroadcastWarmupWaitNotice();
} // namespace muffmode::match
