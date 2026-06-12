// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_captain.h"

/*----------------------------------------------------------------*/
/* CAPTAINS AND TEAM LOCKS                                        */
/*----------------------------------------------------------------*/

/*
=================
SetCaptain

Sets ent as captain of team. Pass nullptr to remove captain.
=================
*/
void SetCaptain(team_t team, gentity_t *ent) {
	level.captain[team] = ent;

	if (ent) {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} became captain of {}.\n",
			ent->client->resp.netname, Teams_TeamName(team));
	}
}

/*
=================
FindNewCaptain

Finds the longest-tenured teammate to auto-promote as captain.
Returns nullptr if no eligible player found.
=================
*/
static gentity_t *FindNewCaptain(team_t team, gentity_t *exclude = nullptr) {
	gentity_t *best = nullptr;
	gtime_t earliest = {};

	for (auto ec : active_clients()) {
		if (ec == exclude)
			continue;
		if (ec->client->sess.team != team)
			continue;
		if (ec->svflags & SVF_BOT)
			continue;
		if (!best || ec->client->sess.team_join_time < earliest) {
			best = ec;
			earliest = ec->client->sess.team_join_time;
		}
	}

	return best;
}

/*
=================
VacateCaptain

Called when a captain leaves their team. Auto-promotes the
longest-tenured teammate, or clears captain if team is empty.
=================
*/
void VacateCaptain(team_t team, gentity_t *leaving) {
	level.captain[team] = nullptr;

	gentity_t *replacement = FindNewCaptain(team, leaving);
	if (replacement)
		SetCaptain(team, replacement);
}

/*
=================
ValidateCaptains

Called after match start / reset to ensure captain pointers
are still valid. Keeps existing captains if valid, otherwise
auto-promotes the longest-tenured teammate.
=================
*/
void ValidateCaptains() {
	if (!Teams())
		return;

	for (team_t t : { TEAM_RED, TEAM_BLUE }) {
		gentity_t *cap = level.captain[t];
		if (cap && cap->inuse && cap->client && cap->client->pers.connected && cap->client->sess.team == t)
			continue; // captain is still valid
		level.captain[t] = nullptr;
		gentity_t *replacement = FindNewCaptain(t);
		if (replacement)
			SetCaptain(t, replacement);
	}
}

/*
=================
IsCaptainOrAdmin

Returns true if ent is captain of the given team, or is an admin.
=================
*/
static bool IsCaptainOrAdmin(gentity_t *ent, team_t team) {
	if (ent->client->sess.admin)
		return true;
	if (level.captain[team] == ent)
		return true;
	return false;
}

/*
=================
MM_CmdCaptain

Usage:
  captain          - claim captain (if none) or show current captain
  captain <player> - transfer captain to a teammate (must be captain)
=================
*/
void MM_CmdCaptain(gentity_t *ent) {
	if (!Teams()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Captain is only available in team modes.\n");
		return;
	}

	team_t team = ent->client->sess.team;

	if (team != TEAM_RED && team != TEAM_BLUE) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You must be on a team to use this command.\n");
		return;
	}

	if (gi.argc() == 1) {
		if (level.captain[team] == ent) {
			gi.LocClient_Print(ent, PRINT_HIGH, "You are the captain of {}.\n", Teams_TeamName(team));
		} else if (level.captain[team]) {
			gi.LocClient_Print(ent, PRINT_HIGH, "{} is the captain of {}.\n",
				level.captain[team]->client->resp.netname, Teams_TeamName(team));
		} else {
			SetCaptain(team, ent);
		}
		return;
	}

	// transfer captain to another player
	if (level.captain[team] != ent) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You must be captain to transfer it to another player.\n");
		return;
	}

	const char *args = gi.args();
	size_t args_len = strlen(args);
	char name_buf[MAX_NETNAME];
	if (args_len >= 2 && args[0] == '"' && args[args_len - 1] == '"') {
		size_t copy_len = std::min(args_len - 2, sizeof(name_buf) - 1);
		memcpy(name_buf, args + 1, copy_len);
		name_buf[copy_len] = '\0';
		args = name_buf;
	}

	gentity_t *target = ClientEntFromString(args);

	if (!target || !target->inuse || !target->client) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid player.\n");
		return;
	}

	if (target == ent) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You can't transfer captain to yourself.\n");
		return;
	}

	if (target->client->sess.team != team) {
		gi.LocClient_Print(ent, PRINT_HIGH, "{} is not on your team.\n", target->client->resp.netname);
		return;
	}

	gi.LocClient_Print(target, PRINT_HIGH, "{} transferred captain status to you.\n",
		ent->client->resp.netname);
	SetCaptain(team, target);
}

/*
=================
MM_CmdLockTeam

Locks a team. Captains lock their own team; admins can specify a team.
=================
*/
void MM_CmdLockTeam(gentity_t *ent) {
	if (!Teams()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Team lock is only available in team modes.\n");
		return;
	}

	team_t team;

	if (ent->client->sess.admin && gi.argc() >= 2) {
		team = StringToTeamNum(gi.argv(1));
	} else {
		team = ent->client->sess.team;
	}

	if (team != TEAM_RED && team != TEAM_BLUE) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid team.\n");
		return;
	}

	if (!IsCaptainOrAdmin(ent, team)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Only team captains or admins can lock teams.\n");
		return;
	}

	if (level.locked[team]) {
		gi.LocClient_Print(ent, PRINT_HIGH, "{} is already locked.\n", Teams_TeamName(team));
		return;
	}

	level.locked[team] = true;
	gi.LocBroadcast_Print(PRINT_HIGH, "{} has been locked.\n", Teams_TeamName(team));
	P_Menu_Dirty();
}

/*
=================
MM_CmdUnlockTeam

Unlocks a team. Captains unlock their own team; admins can specify a team.
=================
*/
void MM_CmdUnlockTeam(gentity_t *ent) {
	if (!Teams()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Team unlock is only available in team modes.\n");
		return;
	}

	team_t team;

	if (ent->client->sess.admin && gi.argc() >= 2) {
		team = StringToTeamNum(gi.argv(1));
	} else {
		team = ent->client->sess.team;
	}

	if (team != TEAM_RED && team != TEAM_BLUE) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid team.\n");
		return;
	}

	if (!IsCaptainOrAdmin(ent, team)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Only team captains or admins can unlock teams.\n");
		return;
	}

	if (!level.locked[team]) {
		gi.LocClient_Print(ent, PRINT_HIGH, "{} is already unlocked.\n", Teams_TeamName(team));
		return;
	}

	level.locked[team] = false;
	gi.LocBroadcast_Print(PRINT_HIGH, "{} has been unlocked.\n", Teams_TeamName(team));
	P_Menu_Dirty();
}

/*----------------------------------------------------------------*/
/* READYUP                                                        */
/*----------------------------------------------------------------*/

/*
=============
ReadyAll
=============
*/
void ReadyAll() {
	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client))
			continue;
		ec->client->resp.ready = true;
	}
}

/*
=============
UnReadyAll
=============
*/
void UnReadyAll() {
	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client))
			continue;
		ec->client->resp.ready = false;
	}
}

void BroadcastReadyReminderMessage() {
	for (auto ec : active_players()) {
		if (!ClientIsPlaying(ec->client))
			continue;
		if (ec->client->sess.is_a_bot)
			continue;
		if (ec->client->resp.ready)
			continue;
		gi.LocCenter_Print(ec, "%bind:+wheel2:Use Compass to toggle your ready status.%MATCH IS IN WARMUP\nYou are NOT ready.");
	}
}

static bool ReadyConditions(gentity_t *ent, bool desired_status, bool admin_cmd) {
	if (level.match_state == matchst_t::MATCH_WARMUP_READYUP)
		return true;

	const char *s = nullptr;
	if (admin_cmd) {
		s = "You cannot force ready status until ";
	} else {
		s = "You cannot change your ready status until ";
	}

	switch (level.warmup_requisite) {
	case warmupreq_t::WARMUP_REQ_MORE_PLAYERS:
	{
		int minp = GT(GT_DUEL) ? 2 : minplayers->integer;
		int req = minp - level.num_playing_clients;
		gi.LocClient_Print(ent, PRINT_HIGH, "{}{} more player{} present.\n", s, req, req > 1 ? "s are" : " is");
		break;
	}
	case warmupreq_t::WARMUP_REQ_BALANCE:
		gi.LocClient_Print(ent, PRINT_HIGH, "{}teams are balanced.\n", s);
		break;
	default:
		gi.LocClient_Print(ent, PRINT_HIGH, "You cannot {}ready at this stage of the match.\n", desired_status ? "" : "un");
		break;
	}
	return false;
}

void MM_CmdReadyAll(gentity_t *ent) {
	if (!ReadyConditions(ent, true, true))
		return;

	ReadyAll();

	gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Forced all players to ready status\n");
}

void MM_CmdUnReadyAll(gentity_t *ent) {
	if (!ReadyConditions(ent, false, true))
		return;

	UnReadyAll();

	gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Forced all players to NOT ready status\n");
}

/*
=================
MM_CmdReadyTeam

Captain readies up all players on their team.
=================
*/
void MM_CmdReadyTeam(gentity_t *ent) {
	if (!Teams()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "This command is only available in team modes.\n");
		return;
	}

	if (!ReadyConditions(ent, true, false))
		return;

	team_t team = ent->client->sess.team;

	if (team != TEAM_RED && team != TEAM_BLUE) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You must be on a team to use this command.\n");
		return;
	}

	if (!IsCaptainOrAdmin(ent, team)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Only team captains or admins can ready the team.\n");
		return;
	}

	int count = 0;
	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client))
			continue;
		if (ec->client->sess.team != team)
			continue;
		if (!ec->client->resp.ready) {
			ec->client->resp.ready = true;
			count++;
		}
	}

	if (count > 0)
		gi.LocBroadcast_Print(PRINT_HIGH, "{} readied up {} ({} player{}).\n",
			ent->client->resp.netname, Teams_TeamName(team), count, count > 1 ? "s" : "");
	else
		gi.LocClient_Print(ent, PRINT_HIGH, "All players on {} are already ready.\n", Teams_TeamName(team));
}

static void BroadcastReadyStatus(gentity_t *ent) {
	gi.LocBroadcast_Print(PRINT_CENTER, "%bind:+wheel2:Use Compass to toggle your ready status.%MATCH IS IN WARMUP\n{} is {}ready.", ent->client->resp.netname, ent->client->resp.ready ? "" : "NOT ");
}

void MM_CmdReady(gentity_t *ent) {
	if (!ReadyConditions(ent, true, false))
		return;

	if (level.match_state != matchst_t::MATCH_WARMUP_READYUP) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You cannot ready at this stage of the match.\n");
		return;
	}

	if (ent->client->resp.ready) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You have already committed.\n");
		return;
	}

	ent->client->resp.ready = true;
	BroadcastReadyStatus(ent);
}

void MM_CmdNotReady(gentity_t *ent) {
	if (!ReadyConditions(ent, false, false))
		return;

	if (!ent->client->resp.ready) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You haven't committed.\n");
		return;
	}

	ent->client->resp.ready = false;
	BroadcastReadyStatus(ent);
}

void MM_CmdReadyUp(gentity_t *ent) {
	if (!ReadyConditions(ent, !ent->client->resp.ready, false))
		return;

	ent->client->resp.ready ^= true;
	BroadcastReadyStatus(ent);
}
