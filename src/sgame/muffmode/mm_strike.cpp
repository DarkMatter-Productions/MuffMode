// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_match.h"
#include "muffmode/mm_strike.h"

namespace {

constexpr int32_t STRIKE_TOUCH_POINTS = 1;
constexpr int32_t STRIKE_WIN_POINTS = 2;
constexpr int32_t STRIKE_MATCH_LIMIT = 15;

} // namespace

team_t MM_Strike_AttackingTeam()
{
	return level.strike_red_attacks ? TEAM_RED : TEAM_BLUE;
}

team_t MM_Strike_DefendingTeam()
{
	return level.strike_red_attacks ? TEAM_BLUE : TEAM_RED;
}

void MM_Strike_CheckMatchEnd()
{
	if (!GT(GT_STRIKE))
		return;

	const int scorelimit = GT_ScoreLimit();
	if (!scorelimit)
		return;

	if (level.team_scores[TEAM_RED] == level.team_scores[TEAM_BLUE])
		return;

	if (level.team_scores[TEAM_RED] >= scorelimit) {
		QueueIntermission(G_Fmt("{} WINS! (hit the {} limit)", Teams_TeamName(TEAM_RED), GT_ScoreLimitString()).data(), false, false);
		return;
	}
	if (level.team_scores[TEAM_BLUE] >= scorelimit) {
		QueueIntermission(G_Fmt("{} WINS! (hit the {} limit)", Teams_TeamName(TEAM_BLUE), GT_ScoreLimitString()).data(), false, false);
	}
}

void MM_Strike_AwardTouchBonus()
{
	if (!GT(GT_STRIKE) || level.strike_flag_touch)
		return;

	level.strike_flag_touch = true;
	G_AdjustTeamScore(MM_Strike_AttackingTeam(), STRIKE_TOUCH_POINTS);
	MM_Strike_CheckMatchEnd();
	if (level.match_state != matchst_t::MATCH_IN_PROGRESS)
		return;
	gi.LocBroadcast_Print(PRINT_CENTER, "Bonus point!\n{} reached the flag!\n",
		Teams_TeamName(MM_Strike_AttackingTeam()));
}

void MM_Strike_AwardTurnWin(bool captured)
{
	if (!GT(GT_STRIKE))
		return;

	const team_t attacker = MM_Strike_AttackingTeam();
	G_AdjustTeamScore(attacker, STRIKE_WIN_POINTS);

	if (captured) {
		gi.LocBroadcast_Print(PRINT_CENTER, "Flag captured!\n{} scores {} points!\n",
			Teams_TeamName(attacker), STRIKE_WIN_POINTS);
	} else {
		gi.LocBroadcast_Print(PRINT_CENTER, "Turn over!\n{} wiped out the defenders!\n",
			Teams_TeamName(attacker));
	}

	AnnouncerSound(world, attacker == TEAM_RED ? "red_wins_round" : "blue_wins_round", "ctf/flagcap.wav", attacker == TEAM_BLUE);
	Round_End();
	MM_Strike_CheckMatchEnd();
}

void MM_Strike_EndDefenseTurn(bool timeout)
{
	if (!GT(GT_STRIKE))
		return;

	const team_t defender = MM_Strike_DefendingTeam();

	if (timeout) {
		if (level.strike_flag_touch) {
			gi.LocBroadcast_Print(PRINT_CENTER, "Turn over!\n{} held the line!\n(partial offense credit awarded)\n",
				Teams_TeamName(defender));
		} else {
			gi.LocBroadcast_Print(PRINT_CENTER, "Turn over!\n{} successfully defended!\n",
				Teams_TeamName(defender));
		}
	} else {
		gi.LocBroadcast_Print(PRINT_CENTER, "Turn over!\n{} eliminated the attackers!\n",
			Teams_TeamName(defender));
	}

	AnnouncerSound(world, defender == TEAM_RED ? "red_wins_round" : "blue_wins_round", "ctf/flagcap.wav", defender == TEAM_BLUE);
	Round_End();
}

void MM_Strike_EndMutualElimination()
{
	if (!GT(GT_STRIKE))
		return;

	gi.LocBroadcast_Print(PRINT_CENTER, "Turn over!\nNo survivors!\n");
	AnnouncerSound(world, "round_won", "ctf/flagcap.wav", true);
	Round_End();
}

void MM_Strike_Init()
{
	if (notGT(GT_STRIKE))
		return;

	static bool strike_defaults_applied = false;
	if (strike_defaults_applied)
		return;
	strike_defaults_applied = true;

	bool changed = false;

	if (g_arena_start_health && g_arena_start_health->integer == 200) {
		gi.cvar_forceset("g_arena_start_health", "100");
		changed = true;
	}
	if (g_arena_start_armor && g_arena_start_armor->integer == 200) {
		gi.cvar_forceset("g_arena_start_armor", "100");
		changed = true;
	}
	if (capturelimit && capturelimit->integer == 8) {
		gi.cvar_forceset("capturelimit", G_Fmt("{}", STRIKE_MATCH_LIMIT).data());
		changed = true;
	}

	if (changed)
		gi.Com_Print("MM_Strike: applied Capture Strike defaults (100/100 spawn, capturelimit 15).\n");
}
