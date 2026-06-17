// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_captain.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_ghost.h"
#include "muffmode/mm_match.h"
#include "muffmode/mm_red_rover_rules.h"
#include "muffmode/mm_team.h"
#include "muffmode/mm_vote.h"

#include <string>

/*
=================
PlayerSortByJoinTime
=================
*/
static bool MM_IsClientIndexInRange(int client_num)
{
	return client_num >= 0 && (size_t)client_num < game.maxclients;
}

static int MM_TeamCvarInteger(cvar_t *cvar)
{
	return cvar ? cvar->integer : 0;
}

static bool MM_TeamCvarEnabled(cvar_t *cvar)
{
	return MM_TeamCvarInteger(cvar) != 0;
}

static bool MM_HumanPlayerLimitReached()
{
	const int limit = MM_TeamCvarInteger(maxplayers);
	return limit > 0 && level.num_playing_human_clients >= limit;
}

static std::string MM_TeamDisplayName(gentity_t *ent)
{
	char name[MAX_INFO_VALUE] = { 0 };

	if (ent && ent->client)
		gi.Info_ValueForKey(ent->client->pers.userinfo, "name", name, sizeof(name));
	if (!name[0] && ent && ent->client)
		Q_strlcpy(name, ent->client->resp.netname, sizeof(name));

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

int PlayerSortByJoinTime(const void *a, const void *b) {
	int anum, bnum;

	anum = *(const int *)a;
	bnum = *(const int *)b;

	const bool a_valid = MM_IsClientIndexInRange(anum);
	const bool b_valid = MM_IsClientIndexInRange(bnum);
	if (!a_valid && !b_valid)
		return 0;
	if (!a_valid)
		return 1;
	if (!b_valid)
		return -1;

	anum = game.clients[anum].sess.team_join_time.milliseconds();
	bnum = game.clients[bnum].sess.team_join_time.milliseconds();

	if (anum > bnum)
		return -1;
	if (anum < bnum)
		return 1;
	return 0;
}

// =========================================
// TEAMPLAY - MOSTLY PORTED FROM QUAKE III
// =========================================

static bool MM_IsTeamInRange(team_t team)
{
	const int value = (int)team;
	return value >= (int)TEAM_NONE && value < (int)TEAM_NUM_TEAMS;
}

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
			if (ignore_client_num >= 0 && i == (size_t)ignore_client_num)
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

	if (!MM_TeamCvarEnabled(deathmatch))
		return;

	if (!ent || !ent->client)
		return;

	if (notGT(GT_DUEL) && ent->client->sess.team == old_team)
		return;

	if (silent)
		return;

	std::string s;
	std::string t;
	const std::string name = MM_TeamDisplayName(ent);

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

	if (MM_TeamCvarEnabled(g_dm_do_readyup) && level.match_state == matchst_t::MATCH_WARMUP_READYUP) {
		BroadcastReadyReminderMessage();
	} else if (!t.empty()) {
		gi.LocClient_Print(ent, PRINT_CENTER, "%bind:inven:Toggles Menu%{}", t.c_str());
	}
}

/*
=================
AllowTeamSwitch
=================
*/
static bool AllowTeamSwitch(gentity_t *ent, team_t desired_team) {
	if (!ent || !ent->client || !MM_IsTeamInRange(desired_team) || desired_team == TEAM_NONE)
		return false;

	// Red Rover: death is the only way to switch teams during a match, so block an
	// already-playing player from manually switching sides to dodge the defect mechanic.
	// Joining from spectator is allowed (otherwise nobody could enter a match in progress),
	// and leaving to spectator is allowed - that's quitting, not dodging a defect.
	if (MM_RedRoverBlocksManualTeamSwitch(ent->client->sess.team, desired_team, TEAM_SPECTATOR, GT(GT_RR), level.match_state == matchst_t::MATCH_IN_PROGRESS)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You cannot change teams during a Red Rover match.\n");
		return false;
	}
	if (desired_team != TEAM_SPECTATOR && MM_HumanPlayerLimitReached()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Maximum player count has been reached.\n");
		return false; // ignore the request
	}

	if (level.locked[desired_team]) {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} is locked.\n", Teams_TeamName(desired_team));
		return false; // ignore the request
	}

	if (Teams()) {
		if (MM_TeamCvarEnabled(g_teamplay_force_balance)) {
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

/*
=================
AllowClientTeamSwitch
=================
*/
bool AllowClientTeamSwitch(gentity_t *ent) {
	if (!MM_TeamCvarEnabled(deathmatch))
		return false;

	if (!ent || !ent->client)
		return false;

	if (MM_TeamCvarEnabled(g_dm_force_join) || !MM_TeamCvarEnabled(g_teamplay_allow_team_pick)) {
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
	if (!Teams())
		return 0;

	if (GT(GT_RR))
		return 0;

	int delta = abs(level.num_playing_red - level.num_playing_blue);

	if (delta < 2)
		return level.num_playing_red - level.num_playing_blue;

	team_t stack_team = level.num_playing_red > level.num_playing_blue ? TEAM_RED : TEAM_BLUE;

	size_t	count = 0;
	int		index[MAX_CLIENTS_KEX];
	memset(index, 0, sizeof(index));

	// assemble list of client nums of everyone on stacked team
	for (auto ec : active_clients()) {
		if (count >= q_countof(index))
			break;
		if (ec->client->sess.team != stack_team)
			continue;
		// store the client number (not the entity number); PlayerSortByJoinTime and
		// the switch loop below index game.clients[] with this directly.
		index[count] = ec - g_entities - 1;
		count++;
	}

	// sort client num list by join time
	qsort(index, count, sizeof(index[0]), PlayerSortByJoinTime);

	//run through sort list, switching from stack_team until teams are even
	if (count) {
		size_t	i;
		int switched = 0;
		gclient_t *cl = nullptr;
		const team_t new_team = stack_team == TEAM_RED ? TEAM_BLUE : TEAM_RED;
		for (i = 0; i < count && delta > 1; i++) {
			cl = &game.clients[index[i]];

			if (!cl->pers.connected)
				continue;

			if (cl->sess.team != stack_team)
				continue;

			// Route the switch through SetTeam (force) so CTF flag/skin/follower/captain
			// state is cleaned up; the old raw `sess.team = ...; ClientRespawn()` left that
			// state dangling and could crash on a CTF rebalance.
			gentity_t *sw_ent = &g_entities[index[i] + 1];
			SetTeam(sw_ent, new_team, false, true, false);
			gi.LocClient_Print(sw_ent, PRINT_CENTER, "You have changed teams to rebalance the game.\n");

			delta--;
			switched++;
		}

		if (switched) {
			gi.LocBroadcast_Print(PRINT_HIGH, "Teams have been balanced.\n");
			return switched;
		}
	}
	return 0;
}

/*
================
TeamShuffle

Randomly shuffles all players in teamplay
================
*/
bool TeamShuffle() {
	if (!Teams())
		return false;
	/*
	if (level.num_playing_clients < 3)
		return false;
		*/
	bool join_red = brandom();
	gentity_t *ent;
	int32_t index[MAX_CLIENTS_KEX] = { 0 };

	for (int32_t i = 0; i < MAX_CLIENTS_KEX; i++)
		index[i] = i;

	for (int32_t i = MAX_CLIENTS_KEX - 1; i > 0; i--) {
		const int32_t j = irandom(i + 1);
		const int32_t tmp = index[i];
		index[i] = index[j];
		index[j] = tmp;
	}

	// determine max team size based from active players
	int maxteam = (level.num_playing_clients + 1) / 2;
	int count_red = 0, count_blue = 0;
	team_t setteam = join_red ? TEAM_RED : TEAM_BLUE;
	
#if 0
	for (size_t i = 0; i < MAX_CLIENTS_KEX; i++) {
		gi.Com_PrintFmt("{}={}\n", i, index[i]);
	}
#endif

	// set teams
	for (size_t i = 0; i < MAX_CLIENTS_KEX; i++) {
		// index[] holds client numbers (0..MAX_CLIENTS_KEX-1); client N is entity N+1.
		ent = &g_entities[index[i] + 1];
		if (!ent->inuse)
			continue;
		if (!ent->client)
			continue;
		if (!ent->client->pers.connected)
			continue;
		if (!ClientIsPlaying(ent->client))
			continue;

		if (count_red >= maxteam || count_red > count_blue)
			setteam = TEAM_BLUE;
		else if (count_blue >= maxteam || count_blue > count_red)
			setteam = TEAM_RED;
		
		if (!SetTeam(ent, setteam, false, true, true))
			continue;

		if (setteam == TEAM_RED)
			count_red++;
		else count_blue++;

		join_red ^= true;
		setteam = join_red ? TEAM_RED : TEAM_BLUE;
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

	if (!MM_IsTeamInRange(desired_team))
		return false;

	if (desired_team == TEAM_NONE && !(force && GT(GT_DUEL)))
		return false;

	team_t old_team = ent->client->sess.team;
	bool queue = false;
	
	if (!force) {
		// Check if this would be a duel queue join (spectator with queue flag)
		bool would_be_duel_queue = GT(GT_DUEL) && desired_team != TEAM_SPECTATOR && level.num_playing_clients >= 2;
		
		if (!ClientIsPlaying(ent->client) && desired_team != TEAM_SPECTATOR) {
			bool revoke = false;
			// Check if the desired team is locked (covers both captain lock and g_match_lock)
			if (level.locked[desired_team] && !would_be_duel_queue) {
				gi.LocClient_Print(ent, PRINT_HIGH, "{} is locked.\n", Teams_TeamName(desired_team));
				revoke = true;
			} else if (MM_HumanPlayerLimitReached()) {
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
			if (desired_team != TEAM_SPECTATOR && level.num_playing_clients >= 2) {
				desired_team = TEAM_SPECTATOR;
				queue = true;
				P_Menu_Close(ent);
			}
		}

		if (!AllowTeamSwitch(ent, desired_team))
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

	// vacate captain if leaving a team
	if ((old_team == TEAM_RED || old_team == TEAM_BLUE) && old_team != desired_team) {
		if (level.captain[old_team] == ent)
			VacateCaptain(old_team, ent);
	}

	// start as spectator
	if (ent->movetype == MOVETYPE_NOCLIP)
		Weapon_Grapple_DoReset(ent->client);

	CTF_DeadDropFlag(ent);
	Tech_DeadDrop(ent);

	FreeFollower(ent);

	ent->svflags &= ~SVF_NOCLIENT;
	ent->client->resp.score = 0;
	ent->client->sess.team = desired_team;
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
		if (Teams())
			G_AssignPlayerSkin(ent, ent->client->pers.skin);

		MM_RevertVote(ent->client);

		// assign a ghost code
		MM_Ghost_DoAssign(ent);

		// free any followers
		FreeClientFollowers(ent);

		// auto-assign captain if team has none
		if ((desired_team == TEAM_RED || desired_team == TEAM_BLUE) && !level.captain[desired_team])
			SetCaptain(desired_team, ent);
	}

	ent->client->sess.initialised = true;

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
	team_t team = StringToTeamNum(s);
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

	team_t team = StringToTeamNum(gi.argv(2));
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
