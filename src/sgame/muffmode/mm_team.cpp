// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_captain.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_duel.h"
#include "muffmode/mm_ghost.h"
#include "muffmode/mm_match.h"
#include "muffmode/mm_match_stats.h"
#include "muffmode/mm_player_stats.h"
#include "muffmode/mm_red_rover_rules.h"
#include "muffmode/mm_statusbar.h"
#include "muffmode/mm_team.h"
#include "muffmode/mm_util.h"
#include "muffmode/mm_vote.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string>
#include <vector>

/*
=================
PlayerSortByJoinTime
=================
*/
namespace muffmode::team {

bool IsClientIndexInRange(int client_num)
{
	return client_num >= 0 && static_cast<size_t>(client_num) < game.maxclients;
}

bool HumanPlayerLimitReached()
{
	const int limit = CvarInteger(maxplayers);
	return limit > 0 && level.num_playing_human_clients >= limit;
}

std::string DisplayName(gentity_t *ent)
{
	char name[MAX_INFO_VALUE] = { 0 };

	if (ent && ent->client)
		gi.Info_ValueForKey(ent->client->pers.userinfo, "name", name, sizeof(name));
	if (!name[0] && ent && ent->client)
		CopyString(name, ent->client->resp.netname);

	std::string display;
	for (const unsigned char *p = reinterpret_cast<const unsigned char *>(name); *p; p++) {
		if (*p < ' ' || *p == 0x7F || *p == '%')
			display += ' ';
		else
			display += static_cast<char>(*p);
	}

	while (!display.empty() && display.back() == ' ')
		display.pop_back();

	return display.empty() ? "player" : display;
}

int CompareJoinTimeDescending(int lhs, int rhs)
{
	const bool a_valid = IsClientIndexInRange(lhs);
	const bool b_valid = IsClientIndexInRange(rhs);
	if (!a_valid && !b_valid)
		return 0;
	if (!a_valid)
		return 1;
	if (!b_valid)
		return -1;

	const auto lhs_time = game.clients[lhs].sess.team_join_time.milliseconds();
	const auto rhs_time = game.clients[rhs].sess.team_join_time.milliseconds();

	if (lhs_time > rhs_time)
		return -1;
	if (lhs_time < rhs_time)
		return 1;
	return 0;
}

bool JoinedMoreRecently(int lhs, int rhs)
{
	return CompareJoinTimeDescending(lhs, rhs) < 0;
}

bool IsTeamInRange(team_t team)
{
	const int value = static_cast<int>(team);
	return value >= static_cast<int>(TEAM_NONE) && value < static_cast<int>(TEAM_NUM_TEAMS);
}

bool IsPlayingTeam(team_t team)
{
	return team == TEAM_RED || team == TEAM_BLUE;
}

} // namespace muffmode::team

int PlayerSortByJoinTime(const void *a, const void *b) {
	if (!a || !b)
		return 0;

	return muffmode::team::CompareJoinTimeDescending(
		*static_cast<const int *>(a),
		*static_cast<const int *>(b));
}

// =========================================
// TEAMPLAY - MOSTLY PORTED FROM QUAKE III
// =========================================

/*
================
PickTeam
================
*/
team_t PickTeam(int ignore_client_num) {
	if (!Teams())
		return TEAM_FREE;

	if (level.num_playing_blue > level.num_playing_red)
		return TEAM_RED;

	if (level.num_playing_red > level.num_playing_blue)
		return TEAM_BLUE;

	// equal team count, so join the team with the lowest score
	if (level.team_scores[TEAM_BLUE] > level.team_scores[TEAM_RED])
		return TEAM_RED;
	if (level.team_scores[TEAM_RED] > level.team_scores[TEAM_BLUE])
		return TEAM_BLUE;

	// equal team scores, so join team with lowest total individual scores
	// skip in tdm as it's redundant
	if (notGT(GT_TDM)) {
		int iscore_red = 0, iscore_blue = 0;

		for (size_t i = 0; i < game.maxclients; i++) {
			if (ignore_client_num >= 0 && i == static_cast<size_t>(ignore_client_num))
				continue;
			if (!game.clients[i].pers.connected)
				continue;

			if (game.clients[i].sess.team == TEAM_RED) {
				iscore_red += game.clients[i].resp.score;
				continue;
			}
			if (game.clients[i].sess.team == TEAM_BLUE) {
				iscore_blue += game.clients[i].resp.score;
				continue;
			}
		}

		if (iscore_blue > iscore_red)
			return TEAM_RED;
		if (iscore_red > iscore_blue)
			return TEAM_BLUE;
	}

	// otherwise just randomly select a team
	return brandom() ? TEAM_RED : TEAM_BLUE;
}

/*
=================
BroadcastTeamChange

Let everyone know about a team change
=================
*/
void BroadcastTeamChange(gentity_t *ent, int old_team, bool inactive, bool silent) {

	if (!muffmode::CvarEnabled(deathmatch))
		return;

	if (!ent || !ent->client)
		return;

	if (notGT(GT_DUEL) && ent->client->sess.team == old_team)
		return;

	if (silent)
		return;

	std::string s;
	std::string t;
	const std::string name = muffmode::team::DisplayName(ent);

	switch (ent->client->sess.team) {
	case TEAM_FREE:
		s = fmt::format("{} joined the battle.\n", name);
		//t = "%bind:inven:Toggles Menu%You have joined the game.";
		t = "You have joined the game.";
		break;
	case TEAM_SPECTATOR:
		if (inactive) {
			s = fmt::format("{} is inactive,\nmoved to spectators.\n", name);
			t = "You are inactive and have been\nmoved to spectators.";
		} else {
			if (GT(GT_DUEL) && ent->client->sess.duel_queued) {
				s = fmt::format("{} is in the queue to play.\n", name);
				t = "You are in the queue to play.";
			} else {
				s = fmt::format("{} joined the spectators.\n", name);
				t = "You are now spectating.";
			}
		}
		break;
	case TEAM_RED:
	case TEAM_BLUE:
		s = fmt::format("{} joined the {} Team.\n", name, Teams_TeamName(ent->client->sess.team));
		t = fmt::format("You have joined the {} Team.\n", Teams_TeamName(ent->client->sess.team));
		break;
	}

	if (!s.empty()) {
		for (auto ec : active_clients()) {
			if (ec == ent)
				continue;
			if (ec->svflags & SVF_BOT)
				continue;
			gi.LocClient_Print(ec, PRINT_CENTER, "{}", s.c_str());
		}
		//gi.Com_Print(s);
	}

	if (muffmode::CvarEnabled(g_dm_do_readyup) && level.match_state == matchst_t::MATCH_WARMUP_READYUP) {
		BroadcastReadyReminderMessage();
	} else if (!t.empty()) {
		gi.LocClient_Print(ent, PRINT_CENTER, "%bind:inven:Open menu%{}", t.c_str());
	}
}

/*
=================
AllowTeamSwitch
=================
*/
namespace muffmode::team {

bool AllowTeamSwitch(gentity_t *ent, team_t desired_team) {
	if (!ent || !ent->client || !IsTeamInRange(desired_team) || desired_team == TEAM_NONE)
		return false;

	// Red Rover: death is the only way to switch teams during a match, so block an
	// already-playing player from manually switching sides to dodge the defect mechanic.
	// Joining from spectator is allowed (otherwise nobody could enter a match in progress),
	// and leaving to spectator is allowed - that's quitting, not dodging a defect.
	if (MM_RedRoverBlocksManualTeamSwitch(ent->client->sess.team, desired_team, TEAM_SPECTATOR, GT(GT_RR), level.match_state == matchst_t::MATCH_IN_PROGRESS)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You cannot change teams during a Red Rover match.\n");
		return false;
	}
	if (desired_team != TEAM_SPECTATOR && HumanPlayerLimitReached()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Maximum player count has been reached.\n");
		return false; // ignore the request
	}

	if (level.locked[desired_team]) {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} is locked.\n", Teams_TeamName(desired_team));
		return false; // ignore the request
	}

	if (Teams()) {
		if (muffmode::CvarEnabled(g_teamplay_force_balance)) {
			// We allow a spread of two
			if ((desired_team == TEAM_RED && (level.num_playing_red - level.num_playing_blue > 1)) ||
				(desired_team == TEAM_BLUE && (level.num_playing_blue - level.num_playing_red > 1))) {
				gi.LocClient_Print(ent, PRINT_HIGH, "{} has too many players.\n", Teams_TeamName(desired_team));
				return false; // ignore the request
			}

			// It's ok, the team we are switching to has less or same number of players
		}
	}

	return true;
}

} // namespace muffmode::team

/*
=================
AllowClientTeamSwitch
=================
*/
bool AllowClientTeamSwitch(gentity_t *ent) {
	if (!muffmode::CvarEnabled(deathmatch))
		return false;

	if (!ent || !ent->client)
		return false;

	if (muffmode::CvarEnabled(g_dm_force_join) || !muffmode::CvarEnabled(g_teamplay_allow_team_pick)) {
		if (!(ent->svflags & SVF_BOT)) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Team picks are disabled.");
			return false;
		}
	}
	
	if (ent->client->resp.team_delay_time > level.time) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You may not switch teams more than once per 5 seconds.\n");
		return false;
	}

	return true;
}

/*
================
TeamBalance

Balance the teams without shuffling.
Switch last joined player(s) from stacked team.
================
*/
int TeamBalance(bool force) {
	if (GT(GT_ARENA))
		return 0;

	if (!Teams())
		return 0;

	if (GT(GT_RR))
		return 0;

	int logical_red = level.num_playing_red + static_cast<int>(
		MM_Ghost_ActivePlayingReservationCountForTeam(TEAM_RED));
	int logical_blue = level.num_playing_blue + static_cast<int>(
		MM_Ghost_ActivePlayingReservationCountForTeam(TEAM_BLUE));
	int delta = std::abs(logical_red - logical_blue);

	if (delta < 2)
		return 0;

	const team_t stack_team = logical_red > logical_blue ? TEAM_RED : TEAM_BLUE;

	std::array<int, MAX_LOBBY_PLAYERS> client_indices = {};
	size_t count = 0;

	// assemble list of client nums of everyone on stacked team
	for (auto ec : active_clients()) {
		if (count >= client_indices.size())
			break;
		if (ec->client->sess.team != stack_team)
			continue;
		// Store the client number (not the entity number); the switch loop below indexes
		// game.clients[] with this directly.
		client_indices[count] = static_cast<int>(ec - g_entities - 1);
		count++;
	}

	// sort client num list by join time
	std::sort(client_indices.begin(), client_indices.begin() + count, muffmode::team::JoinedMoreRecently);

	//run through sort list, switching from stack_team until teams are even
	int switched = 0;
	const team_t new_team = stack_team == TEAM_RED ? TEAM_BLUE : TEAM_RED;
	for (size_t i = 0; i < count && delta > 1; i++) {
		const int client_index = client_indices[i];
		gclient_t &client = game.clients[client_index];

		if (!client.pers.connected)
			continue;

		if (client.sess.team != stack_team)
			continue;

		// Route the switch through SetTeam (force) so CTF flag/skin/follower/captain
		// state is cleaned up; the old raw `sess.team = ...; ClientRespawn()` left that
		// state dangling and could crash on a CTF rebalance.
		gentity_t *sw_ent = &g_entities[client_index + 1];
		if (!SetTeam(sw_ent, new_team, false, force, false))
			continue;
		gi.LocClient_Print(sw_ent, PRINT_CENTER, "You have changed teams to rebalance the game.\n");

		if (stack_team == TEAM_RED) {
			logical_red--;
			logical_blue++;
		} else {
			logical_blue--;
			logical_red++;
		}
		delta = std::abs(logical_red - logical_blue);
		switched++;
	}

	if (switched) {
		gi.LocBroadcast_Print(PRINT_HIGH, "Teams have been balanced.\n");
		return switched;
	}

	return 0;
}

/*
================
TeamShuffle

Distributes active players by descending skill rating.
================
*/
bool TeamShuffle() {
	if (GT(GT_ARENA))
		return false;

	if (!Teams())
		return false;
	std::vector<int32_t> client_indices;
	client_indices.reserve(static_cast<size_t>(level.num_playing_clients));
	for (size_t client_index = 0; client_index < game.maxclients; ++client_index) {
		// client_indices holds client numbers (0..MAX_LOBBY_PLAYERS-1); client N is entity N+1.
		gentity_t *ent = &g_entities[client_index + 1];
		if (ent->inuse && ent->client && ent->client->pers.connected &&
			ClientIsPlaying(ent->client))
			client_indices.push_back(static_cast<int32_t>(client_index));
	}
	if (client_indices.size() < 2)
		return false;

	std::sort(client_indices.begin(), client_indices.end(),
		[](int32_t lhs, int32_t rhs) {
			const float lhs_rating = MM_PlayerStats_Rating(&g_entities[lhs + 1]);
			const float rhs_rating = MM_PlayerStats_Rating(&g_entities[rhs + 1]);
			if (lhs_rating != rhs_rating)
				return lhs_rating > rhs_rating;
			return lhs < rhs;
		});

	team_t target_team = TEAM_RED;
	for (const int32_t client_index : client_indices) {
		gentity_t *ent = &g_entities[client_index + 1];
		if (!ent->inuse || !ent->client || !ent->client->pers.connected ||
			!ClientIsPlaying(ent->client))
			continue;
		if (ent->client->sess.team != target_team)
			SetTeam(ent, target_team, false, true, true);
		target_team = target_team == TEAM_RED ? TEAM_BLUE : TEAM_RED;
	}

	return true;
}

/*
=================
SetTeam
=================
*/
bool SetTeam(gentity_t *ent, team_t desired_team, bool inactive, bool force, bool silent) {
	if (!ent || !ent->client)
		return false;

	bool arena_result = false;
	if (MM_Arena_HandleTeamRequest(ent, desired_team, inactive, force, silent, arena_result))
		return arena_result;

	if (!muffmode::team::IsTeamInRange(desired_team))
		return false;

	if (desired_team == TEAM_NONE && !(force && GT(GT_DUEL)))
		return false;

	const team_t old_team = ent->client->sess.team;
	bool queue = false;

	if (desired_team != TEAM_SPECTATOR && ent->client->sess.inactive && MM_Ghost_TryRestore(ent))
		return true;
	
	if (!force) {
		// Check if this would be a duel queue join (spectator with queue flag)
		const bool would_be_duel_queue = GT(GT_DUEL) &&
			desired_team != TEAM_SPECTATOR && MM_Duel_OccupiedSlots() >= 2;
		
		if (!ClientIsPlaying(ent->client) && desired_team != TEAM_SPECTATOR) {
			bool revoke = false;
			// Check if the desired team is locked (covers both captain lock and g_match_lock)
			if (level.locked[desired_team] && !would_be_duel_queue) {
				gi.LocClient_Print(ent, PRINT_HIGH, "{} is locked.\n", Teams_TeamName(desired_team));
				revoke = true;
			} else if (muffmode::team::HumanPlayerLimitReached()) {
				gi.LocClient_Print(ent, PRINT_HIGH, "Maximum player load reached.\n");
				revoke = true;
			}
			if (revoke) {
				P_Menu_Close(ent);
				return false;
			}
		}

		if (desired_team != TEAM_SPECTATOR && desired_team == ent->client->sess.team) {
			P_Menu_Close(ent);
			return false;
		}

		if (GT(GT_DUEL)) {
			if (desired_team != TEAM_SPECTATOR && MM_Duel_OccupiedSlots() >= 2) {
				desired_team = TEAM_SPECTATOR;
				queue = true;
				P_Menu_Close(ent);
			}
		}

		if (!muffmode::team::AllowTeamSwitch(ent, desired_team))
			return false;

		// Don't rate limit switching TO spectator - allow players to spectate immediately
		// Only rate limit switches between playing teams
		if (!inactive && desired_team != TEAM_SPECTATOR && ent->client->resp.team_delay_time > level.time) {
			gi.LocClient_Print(ent, PRINT_HIGH, "You may not switch teams more than once per 5 seconds.\n");
			P_Menu_Close(ent);
			return false;
		}
	} else {
		if (GT(GT_DUEL)) {
			if (desired_team == TEAM_NONE) {
				desired_team = TEAM_SPECTATOR;
				queue = true;
			}
		}
	}

	// allow the change...

	P_Menu_Close(ent);
	const bool was_match_player = muffmode::team::IsPlayingTeam(old_team);
	const bool will_be_match_player = muffmode::team::IsPlayingTeam(desired_team);
	bool inactive_reserved = false;
	if (old_team != desired_team) {
		const bool capture_inactive = inactive &&
			desired_team == TEAM_SPECTATOR && was_match_player &&
			!will_be_match_player;
		if (capture_inactive) {
			// Pause and archive before copying the reconnect state. A successful
			// inactivity reservation remains an unsettled logical participant;
			// a failed capture falls through to ordinary abandonment settlement.
			MM_PlayerStats_OnClientPause(ent);
			MM_MatchStats_ClientEnd(ent);
			inactive_reserved = MM_Ghost_CaptureInactive(ent);
		}
		if (!inactive_reserved) {
			MM_PlayerStats_OnTeamTransition(ent, old_team, desired_team);
			if (was_match_player && !will_be_match_player && !capture_inactive)
				MM_MatchStats_ClientEnd(ent);
		}
	}

	// vacate captain if leaving a team
	if (notGT(GT_ARENA) &&
		muffmode::team::IsPlayingTeam(old_team) && old_team != desired_team) {
		if (level.captain[old_team] == ent)
			VacateCaptain(old_team, ent);
	}

	// start as spectator
	if (ent->movetype == MOVETYPE_NOCLIP)
		Weapon_Grapple_DoReset(ent->client);

	CTF_DeadDropFlag(ent);
	Tech_DeadDrop(ent);
	if (old_team != desired_team && was_match_player && !will_be_match_player)
		MM_MatchStats_ClientEnd(ent);

	FreeFollower(ent);

	ent->svflags &= ~SVF_NOCLIENT;
	// Red Rover and multi-arena role projection change engine teams during play.
	// Their own match controllers reset scores at real series boundaries.
	if (notGT(GT_RR) && notGT(GT_ARENA))
		ent->client->resp.score = 0;
	ent->client->sess.team = desired_team;
	P_PublishEngineTeam(ent);
	if (old_team != desired_team && !was_match_player && will_be_match_player)
		MM_MatchStats_ClientBegin(ent);
	if (desired_team == TEAM_SPECTATOR)
		ent->client->eliminated = false;
	ent->client->resp.ctf_state = 0;
	ent->client->sess.inactive = inactive;
	ent->client->sess.inactivity_time = level.time + 1_min;
	// If queued for duel, record when they joined the queue for proper ordering.
	// Otherwise, non-queued spectators get 0_sec, playing players get current time.
	if (desired_team == TEAM_SPECTATOR) {
		ent->client->sess.team_join_time = queue ? level.time : 0_sec;
	} else {
		ent->client->sess.team_join_time = level.time;
	}
	ent->client->resp.team_delay_time = force || !ent->client->sess.initialised ? level.time : level.time + 5_sec;
	ent->client->sess.spectator_state = desired_team == TEAM_SPECTATOR ? SPECTATOR_FREE : SPECTATOR_NOT;
	ent->client->sess.spectator_client = 0;
	ent->client->sess.duel_queued = queue;

	if (desired_team != TEAM_SPECTATOR) {
		if (Teams() || GT(GT_ARENA))
			G_AssignPlayerSkin(ent, ent->client->pers.skin);

		if (notGT(GT_ARENA)) {
			MM_RevertVote(ent->client);

			// assign a ghost code
			MM_Ghost_DoAssign(ent);
		}

		// free any followers
		FreeClientFollowers(ent);

		// auto-assign captain if team has none
		if (notGT(GT_ARENA) &&
			muffmode::team::IsPlayingTeam(desired_team) && !level.captain[desired_team])
			SetCaptain(desired_team, ent);
	} else if (!inactive && notGT(GT_ARENA)) {
		MM_Ghost_ClearClient(ent);
	}

	ent->client->sess.initialised = true;

	// joining the match re-arms the top-right gametype/ruleset notice
	if (will_be_match_player)
		MM_MatchInfoHud_Show(ent);

	// if they are playing a duel, count as a loss
	if (GT(GT_DUEL) && old_team == TEAM_FREE)
		ent->client->sess.losses++;

	ClientSpawn(ent);
	G_PostRespawn(ent);

	BroadcastTeamChange(ent, old_team, inactive, silent);

	ent->client->ps.stats[STAT_SHOW_STATUSBAR] = desired_team == TEAM_SPECTATOR || ent->client->eliminated ? 0 : 1;

	// if anybody has a menu open, update it immediately
	P_Menu_Dirty();

	return true;
}

/*
=================
MM_CmdTeam
=================
*/
void MM_CmdTeam(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (MM_Arena_TeamCommand(ent))
		return;

	if (!MM_IsArgcInRangeValid(gi.argc(), 1, 2)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} [team]\n", gi.argv(0));
		return;
	}

	if (gi.argc() == 1) {
		switch (ent->client->sess.team) {
		case TEAM_SPECTATOR:
			gi.LocClient_Print(ent, PRINT_HIGH, "You are spectating.\n");
			break;
		case TEAM_FREE:
			gi.LocClient_Print(ent, PRINT_HIGH, "You are in the match.\n");
			break;
		case TEAM_RED:
		case TEAM_BLUE:
			gi.LocClient_Print(ent, PRINT_HIGH, "Your team: {}\n", Teams_TeamName(ent->client->sess.team));
			break;
		default:
			break;
		}
		return;
	}

	const char *s = gi.argv(1);
	const team_t team = StringToTeamNum(s);
	if (team == TEAM_NONE) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid team.\n");
		return;
	}

	SetTeam(ent, team, false, false, false);
}

/*
=================
MM_CmdSetTeam
=================
*/
void MM_CmdSetTeam(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (MM_Arena_SetTeamCommand(ent))
		return;

	if (!MM_IsArgcInRangeValid(gi.argc(), 2, 3)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} [client name/num] [team]\n", gi.argv(0));
		return;
	}

	gentity_t *targ = ClientEntFromString(gi.argv(1));

	if (!targ || !targ->inuse || !targ->client) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid client name or number.\n");
		return;
	}

	if (gi.argc() == 2) {
		gi.LocClient_Print(ent, PRINT_HIGH, "{} is on {} team.\n", targ->client->resp.netname, Teams_TeamName(targ->client->sess.team));
		return;
	}

	const team_t team = StringToTeamNum(gi.argv(2));
	if (team == TEAM_NONE) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid team.\n");
		return;
	}

	if (targ->client->sess.team == team) {
		gi.LocClient_Print(ent, PRINT_HIGH, "{} is already on {} team.\n", targ->client->resp.netname, Teams_TeamName(team));
		return;
	}

	if ((Teams() && team == TEAM_FREE) || (!Teams() && team != TEAM_SPECTATOR && team != TEAM_FREE)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid team.\n");
		return;
	}

	if (!SetTeam(targ, team, false, true, false)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Could not move {} to {} team.\n", targ->client->resp.netname, Teams_TeamName(team));
		return;
	}

	gi.LocBroadcast_Print(PRINT_HIGH, "[ADMIN]: Moved {} to {} team.\n", targ->client->resp.netname, Teams_TeamName(team));
}

/*
=================
MM_CmdShuffle
=================
*/
void MM_CmdShuffle(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (!MM_IsExactArgcValid(gi.argc(), 1)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {}\n", gi.argv(0));
		return;
	}

	if (!Teams()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Team shuffle is only available in team modes.\n");
		return;
	}

	if (level.num_playing_clients < 2) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Not enough players to shuffle teams.\n");
		return;
	}

	if (!TeamShuffle()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Teams could not be shuffled.\n");
		return;
	}

	gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Forced team shuffle.\n");
	Match_Reset();
}

/*
=================
MM_CmdBalanceTeams
=================
*/
void MM_CmdBalanceTeams(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (!MM_IsExactArgcValid(gi.argc(), 1)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {}\n", gi.argv(0));
		return;
	}

	if (!Teams()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Team balancing is only available in team modes.\n");
		return;
	}

	const int moved = TeamBalance(true);
	if (moved > 0)
		gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Forced team balancing.\n");
	else
		gi.LocClient_Print(ent, PRINT_HIGH, "Teams are already balanced.\n");
}
