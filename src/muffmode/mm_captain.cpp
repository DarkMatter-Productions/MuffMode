// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_captain.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_util.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace muffmode::captain {

bool RequireCommandArgc(gentity_t *ent, int min_expected, int max_expected, const char *usage)
{
	if (!ent || !ent->client)
		return false;

	if (MM_IsArgcInRangeValid(gi.argc(), min_expected, max_expected))
		return true;

	gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {}\n", usage);
	return false;
}

bool RequireNoCommandArgs(gentity_t *ent)
{
	return RequireCommandArgc(ent, 1, 1, gi.argv(0));
}

bool IsCaptainTeam(team_t team)
{
	return team == TEAM_RED || team == TEAM_BLUE;
}

bool IsBotClient(gentity_t *ent)
{
	return ent && ((ent->svflags & SVF_BOT) || (ent->client && ent->client->sess.is_a_bot));
}

bool IsValidCaptain(team_t team, gentity_t *ent)
{
	return IsCaptainTeam(team) && ent && ent->inuse && ent->client &&
		ent->client->pers.connected && ent->client->sess.team == team && !IsBotClient(ent);
}

} // namespace muffmode::captain

namespace captain = muffmode::captain;

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
	if (!captain::IsCaptainTeam(team))
		return;

	if (ent && !captain::IsValidCaptain(team, ent))
		return;

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
namespace muffmode::captain {

gentity_t *FindNewCaptain(team_t team, gentity_t *exclude = nullptr) {
	if (!IsCaptainTeam(team))
		return nullptr;

	gentity_t *best = nullptr;
	gtime_t earliest = {};

	for (auto ec : active_clients()) {
		if (!ec || !ec->client || !ec->client->pers.connected)
			continue;
		if (ec == exclude)
			continue;
		if (ec->client->sess.team != team)
			continue;
		if (IsBotClient(ec))
			continue;
		if (!best || ec->client->sess.team_join_time < earliest) {
			best = ec;
			earliest = ec->client->sess.team_join_time;
		}
	}

	return best;
}

} // namespace muffmode::captain

/*
=================
VacateCaptain

Called when a captain leaves their team. Auto-promotes the
longest-tenured teammate, or clears captain if team is empty.
=================
*/
void VacateCaptain(team_t team, gentity_t *leaving) {
	if (!captain::IsCaptainTeam(team))
		return;

	level.captain[team] = nullptr;

	gentity_t *replacement = captain::FindNewCaptain(team, leaving);
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
		if (captain::IsValidCaptain(t, cap))
			continue; // captain is still valid
		level.captain[t] = nullptr;
		gentity_t *replacement = captain::FindNewCaptain(t);
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
namespace muffmode::captain {

bool IsCaptainOrAdmin(gentity_t *ent, team_t team) {
	if (!ent || !ent->client || !IsCaptainTeam(team))
		return false;

	if (ent->client->sess.admin)
		return true;
	if (level.captain[team] == ent)
		return true;
	return false;
}

} // namespace muffmode::captain

/*
=================
MM_CmdCaptain

Usage:
  captain          - claim captain (if none) or show current captain
  captain <player> - transfer captain to a teammate (must be captain)
=================
*/
void MM_CmdCaptain(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (!Teams()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Captain is only available in team modes.\n");
		return;
	}

	const team_t team = ent->client->sess.team;

	if (!captain::IsCaptainTeam(team)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You must be on a team to use this command.\n");
		return;
	}

	if (gi.argc() == 1) {
		if (level.captain[team] == ent) {
			gi.LocClient_Print(ent, PRINT_HIGH, "You are the captain of {}.\n", Teams_TeamName(team));
		} else if (captain::IsValidCaptain(team, level.captain[team])) {
			gi.LocClient_Print(ent, PRINT_HIGH, "{} is the captain of {}.\n",
				level.captain[team]->client->resp.netname, Teams_TeamName(team));
		} else {
			level.captain[team] = nullptr;
			SetCaptain(team, ent);
		}
		return;
	}

	// transfer captain to another player
	if (!captain::IsValidCaptain(team, level.captain[team]))
		level.captain[team] = nullptr;

	if (level.captain[team] != ent) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You must be captain to transfer it to another player.\n");
		return;
	}

	const char *args = gi.args();
	if (!args || !*args) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid player.\n");
		return;
	}

	std::string quoted_name;
	const std::string_view arg_view(args);
	if (arg_view.size() >= 2 && arg_view.front() == '"' && arg_view.back() == '"') {
		const size_t copy_len = std::min(arg_view.size() - 2, static_cast<size_t>(MAX_NETNAME - 1));
		quoted_name.assign(arg_view.substr(1, copy_len));
		args = quoted_name.c_str();
	}

	gentity_t *target = ClientEntFromString(args);

	if (!target || !target->inuse || !target->client) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid player.\n");
		return;
	}

	if (captain::IsBotClient(target)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Bots cannot be team captain.\n");
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
	if (!ent || !ent->client)
		return;

	if (!Teams()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Team lock is only available in team modes.\n");
		return;
	}

	if (ent->client->sess.admin) {
		if (!captain::RequireCommandArgc(ent, 1, 2, G_Fmt("{} [red|blue]", gi.argv(0)).data()))
			return;
	} else if (!captain::RequireNoCommandArgs(ent)) {
		return;
	}

	const team_t team = ent->client->sess.admin && gi.argc() == 2
		? StringToTeamNum(gi.argv(1))
		: ent->client->sess.team;

	if (!captain::IsCaptainTeam(team)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid team.\n");
		return;
	}

	if (!captain::IsCaptainOrAdmin(ent, team)) {
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
	if (!ent || !ent->client)
		return;

	if (!Teams()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Team unlock is only available in team modes.\n");
		return;
	}

	if (ent->client->sess.admin) {
		if (!captain::RequireCommandArgc(ent, 1, 2, G_Fmt("{} [red|blue]", gi.argv(0)).data()))
			return;
	} else if (!captain::RequireNoCommandArgs(ent)) {
		return;
	}

	const team_t team = ent->client->sess.admin && gi.argc() == 2
		? StringToTeamNum(gi.argv(1))
		: ent->client->sess.team;

	if (!captain::IsCaptainTeam(team)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid team.\n");
		return;
	}

	if (!captain::IsCaptainOrAdmin(ent, team)) {
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
		if (!ec || !ec->client)
			continue;
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
		if (!ec || !ec->client)
			continue;
		if (!ClientIsPlaying(ec->client))
			continue;
		ec->client->resp.ready = false;
	}
}

void BroadcastReadyReminderMessage() {
	for (auto ec : active_players()) {
		if (!ec || !ec->client)
			continue;
		if (!ClientIsPlaying(ec->client))
			continue;
		if (ec->client->sess.is_a_bot)
			continue;
		if (ec->client->resp.ready)
			continue;
		gi.LocCenter_Print(ec, "%bind:+wheel2:Use Compass to toggle your ready status.%MATCH IS IN WARMUP\nYou are NOT ready.");
	}
}

namespace muffmode::captain {

bool ReadyConditions(gentity_t *ent, bool desired_status, bool admin_cmd) {
	if (!ent || !ent->client)
		return false;

	if (!admin_cmd && !ClientIsPlaying(ent->client)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You must be playing to change your ready status.\n");
		return false;
	}

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
		const int minp = GT(GT_DUEL) ? 2 : std::max(1, muffmode::CvarInteger(minplayers));
		const int req = std::max(1, minp - level.num_playing_clients);
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

} // namespace muffmode::captain

void MM_CmdReadyAll(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (!captain::RequireNoCommandArgs(ent))
		return;

	if (!captain::ReadyConditions(ent, true, true))
		return;

	ReadyAll();

	gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Forced all players to ready status\n");
}

void MM_CmdUnReadyAll(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (!captain::RequireNoCommandArgs(ent))
		return;

	if (!captain::ReadyConditions(ent, false, true))
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
	if (!ent || !ent->client)
		return;

	if (!captain::RequireNoCommandArgs(ent))
		return;

	if (!Teams()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "This command is only available in team modes.\n");
		return;
	}

	if (!captain::ReadyConditions(ent, true, false))
		return;

	const team_t team = ent->client->sess.team;

	if (!captain::IsCaptainTeam(team)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You must be on a team to use this command.\n");
		return;
	}

	if (!captain::IsCaptainOrAdmin(ent, team)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Only team captains or admins can ready the team.\n");
		return;
	}

	int count = 0;
	for (auto ec : active_clients()) {
		if (!ec || !ec->client)
			continue;
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

namespace muffmode::captain {

void BroadcastReadyStatus(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	gi.LocBroadcast_Print(PRINT_CENTER, "%bind:+wheel2:Use Compass to toggle your ready status.%MATCH IS IN WARMUP\n{} is {}ready.", ent->client->resp.netname, ent->client->resp.ready ? "" : "NOT ");
}

} // namespace muffmode::captain

void MM_CmdReady(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (!captain::RequireNoCommandArgs(ent))
		return;

	if (!captain::ReadyConditions(ent, true, false))
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
	captain::BroadcastReadyStatus(ent);
}

void MM_CmdNotReady(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (!captain::RequireNoCommandArgs(ent))
		return;

	if (!captain::ReadyConditions(ent, false, false))
		return;

	if (!ent->client->resp.ready) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You haven't committed.\n");
		return;
	}

	ent->client->resp.ready = false;
	captain::BroadcastReadyStatus(ent);
}

void MM_CmdReadyUp(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (!captain::RequireNoCommandArgs(ent))
		return;

	if (!captain::ReadyConditions(ent, !ent->client->resp.ready, false))
		return;

	ent->client->resp.ready ^= true;
	captain::BroadcastReadyStatus(ent);
}
