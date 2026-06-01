// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "g_debug_log.h"
#include "muffmode/mm_vote.h"

namespace {
bool MM_IsValidVoteTransition(VoteState from, VoteState to)
{
	switch (from)
	{
	case VoteState::IDLE:
		// Allow IDLE -> IDLE (harmless, happens during level init after memset).
		return to == VoteState::ACTIVE || to == VoteState::IDLE;

	case VoteState::ACTIVE:
		return to == VoteState::PASSED || to == VoteState::FAILED;

	case VoteState::PASSED:
		return to == VoteState::EXECUTING || to == VoteState::FAILED; // if caller disconnects

	case VoteState::EXECUTING:
		return to == VoteState::COMPLETE || to == VoteState::FAILED;

	case VoteState::FAILED:
	case VoteState::COMPLETE:
		return to == VoteState::IDLE;

	default:
		return false;
	}
}
} // namespace

void MM_TransitionVoteState(VoteState new_state)
{
	VoteState old_state = level.vote_state.state;

	if (old_state == new_state)
		return;

	if (!MM_IsValidVoteTransition(old_state, new_state))
	{
		MuffModeLog("VOTE", "Invalid state transition: %d -> %d", (int)old_state, (int)new_state);
		return;
	}

	level.vote_state.state = new_state;

	// Entry actions for new state.
	switch (new_state)
	{
	case VoteState::IDLE:
		level.vote_state.command = nullptr;
		level.vote_state.arg.clear();
		level.vote_state.caller = nullptr;
		level.vote_state.start_time = 0_sec;
		level.vote_state.execute_time = 0_sec;
		level.vote_state.yes_votes = 0;
		level.vote_state.no_votes = 0;
		level.vote_state.num_eligible = 0;
		break;

	case VoteState::PASSED:
		level.vote_state.execute_time = level.time + 3_sec;
		break;

	case VoteState::FAILED:
		level.vote_state.caller = nullptr;
		break;

	default:
		break;
	}
}

void MM_ClearVote()
{
	MM_TransitionVoteState(VoteState::IDLE);
}

void MM_VotePassed()
{
	if (!level.vote_state.command)
	{
		gi.LocBroadcast_Print(PRINT_HIGH, "Vote passed but command was lost.\n");
		MM_TransitionVoteState(VoteState::FAILED);
		return;
	}

	MuffModeLog("DEBUG", "Vote_Passed: executing command '%s'", level.vote_state.command->name);
	level.vote_state.command->func();
	MuffModeLog("DEBUG", "Vote_Passed: command executed, transitioning to COMPLETE");
	MM_TransitionVoteState(VoteState::COMPLETE);
	MuffModeLog("DEBUG", "Vote_Passed: done");
}

void MM_VotePassGametype()
{
	gametype_t gt = GT_IndexFromString(level.vote_state.arg.data());
	MuffModeLog("DEBUG", "Vote_Pass_Gametype: enter, arg='%s', gt=%d", level.vote_state.arg.data(), (int)gt);
	if (gt == GT_NONE)
	{
		MuffModeLog("DEBUG", "Vote_Pass_Gametype: GT_NONE, aborting");
		return;
	}

	// Re-check votability at execution time in case g_votable_gametypes changed
	// during the 3-second PASSED->EXECUTING window, or the vote arrived via the
	// menu path which does not run val_func.
	if (!MM_IsGametypeVotable(gt))
	{
		gi.LocBroadcast_Print(PRINT_HIGH, "Gametype vote rejected: gametype is no longer votable.\n");
		MuffModeLog("VOTE", "Vote_Pass_Gametype: gametype %d rejected by IsGametypeVotable at execution", (int)gt);
		return;
	}

	// Change the gametype (this sets cvars and queues config exec)
	MuffModeLog("DEBUG", "Vote_Pass_Gametype: calling ChangeGametype(%d)", (int)gt);
	ChangeGametype(gt);
	MuffModeLog("DEBUG", "Vote_Pass_Gametype: ChangeGametype returned, queuing sv gt_changemap_first");

	// Queue a special server command that will execute AFTER the gametype config
	// This command will read the NEW g_map_list and change to the first map in it
	// Note: "sv" prefix is required to invoke ServerCommand() handler
	gi.AddCommandString("sv gt_changemap_first\n");
	MuffModeLog("DEBUG", "Vote_Pass_Gametype: done");
}

bool MM_IsGametypeVotable(gametype_t gt)
{
	// If no votable list is set, allow all gametypes (backward compatible).
	if (!g_votable_gametypes->string[0])
		return true;

	// Check if the gametype's short name is in the votable list.
	const char *votable_list = g_votable_gametypes->string;
	char *token;

	while ((token = COM_Parse(&votable_list)) && *token)
	{
		if (!Q_strcasecmp(token, gt_short_name[(int)gt]))
			return true;
	}

	return false;
}

std::string MM_GetVotableGametypesList()
{
	std::string result;

	if (!g_votable_gametypes->string[0])
	{
		// If no restriction, show all implemented gametypes.
		for (int i = (int)GT_FIRST; i <= (int)GT_LAST; i++)
		{
			// Skip GT_NONE, GT_STRIKE, GT_RR, GT_LMS, GT_BALL (not fully implemented).
			if (i == GT_NONE || i == GT_STRIKE || i == GT_RR || i == GT_LMS || i == GT_BALL)
				continue;
			if (!result.empty())
				result += "|";
			result += gt_short_name[i];
		}
	}
	else
	{
		// Show only votable gametypes.
		const char *votable_list = g_votable_gametypes->string;
		char *token;
		bool first = true;

		while ((token = COM_Parse(&votable_list)) && *token)
		{
			if (!first)
				result += "|";
			result += token;
			first = false;
		}
	}

	return result;
}

bool MM_IsRulesetVotable(ruleset_t rs)
{
	// If no votable list is set, allow all rulesets (backward compatible).
	if (!g_votable_rulesets->string[0])
		return true;

	// Check if the ruleset's short name is in the votable list.
	const char *votable_list = g_votable_rulesets->string;
	char *token;

	while ((token = COM_Parse(&votable_list)) && *token)
	{
		if (!Q_strcasecmp(token, rs_short_name[(int)rs]))
			return true;
	}

	return false;
}

std::string MM_GetVotableRulesetsList()
{
	std::string result;

	if (!g_votable_rulesets->string[0])
	{
		// If no restriction, show all implemented rulesets (skip RS_NONE).
		for (int i = (int)RS_NONE + 1; i < (int)RS_NUM_RULESETS; i++)
		{
			if (i == RS_NONE)
				continue;
			if (!result.empty())
				result += "|";
			result += rs_short_name[i];
		}
	}
	else
	{
		// Show only votable rulesets.
		const char *votable_list = g_votable_rulesets->string;
		char *token;
		bool first = true;

		while ((token = COM_Parse(&votable_list)) && *token)
		{
			if (!first)
				result += "|";
			result += token;
			first = false;
		}
	}

	return result;
}

void MM_VotePassRuleset()
{
	ruleset_t rs = RS_IndexFromString(level.vote_state.arg.data());
	if (rs == ruleset_t::RS_NONE)
		return;

	// Re-check votability at execution time in case g_votable_rulesets changed
	// during the 3-second PASSED->EXECUTING window, or the vote arrived via the
	// menu path which does not run val_func.
	if (!MM_IsRulesetVotable(rs))
	{
		gi.LocBroadcast_Print(PRINT_HIGH, "Ruleset vote rejected: ruleset is no longer votable.\n");
		MuffModeLog("VOTE", "Vote_Pass_Ruleset: ruleset %d rejected by IsRulesetVotable at execution", (int)rs);
		return;
	}

	gi.cvar_forceset("g_ruleset", G_Fmt("{}", (int)rs).data());
}

bool MM_ValidVoteCommand(gentity_t *ent)
{
	if (!ent->client)
		return false;

	MuffModeLog("DEBUG", "ValidVoteCommand: enter, argv(1)=%s, argc=%d", gi.argv(1), gi.argc());

	level.vote_state.command = nullptr;

	vcmds_t *cc = FindVoteCmdByName(gi.argv(1));
	if (!cc)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid vote command: {}\n", gi.argv(1));
		return false;
	}

	if (cc->args && gi.argc() < (1 + cc->min_args))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "{}: {}\nUsage: {} {}\n", cc->name, cc->help, cc->name, cc->args);
		return false;
	}

	if (!cc->val_func(ent))
		return false;

	level.vote_state.command = cc;

	// Build the vote argument string.
	std::string raw_arg;
	if (!Q_strcasecmp(cc->name, "handicap") && gi.argc() >= 5)
	{
		std::string player_name = gi.argv(2);
		if (player_name.find(' ') != std::string::npos)
			raw_arg = "\"" + player_name + "\" " + std::string(gi.argv(3)) + " " + std::string(gi.argv(4));
		else
			raw_arg = player_name + " " + std::string(gi.argv(3)) + " " + std::string(gi.argv(4));
	}
	else
	{
		raw_arg = gi.argc() > 2 ? gi.argv(2) : "";
	}

	constexpr size_t MAX_VOTE_ARG_LENGTH = 128;
	if (raw_arg.length() > MAX_VOTE_ARG_LENGTH)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Vote argument too long (max {} characters).\n", MAX_VOTE_ARG_LENGTH);
		return false;
	}
	level.vote_state.arg = raw_arg;
	MuffModeLog("DEBUG", "ValidVoteCommand: success, cmd=%s, arg='%s' (len=%d, ptr=%p)",
		cc->name, level.vote_state.arg.c_str(), (int)level.vote_state.arg.length(),
		(void *)level.vote_state.arg.c_str());
	return true;
}

void MM_VoteCommandStore(gentity_t *ent)
{
	if (!level.vote_state.command)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Internal error: vote command was lost.\n");
		return;
	}

	if (!g_allow_voting->integer)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Voting not allowed here.\n");
		return;
	}

	if (g_vote_flags->integer & level.vote_state.command->flag)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "This vote type is not allowed.\n");
		return;
	}

	if (level.vote_state.state != VoteState::IDLE)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "A vote is already in progress.\n");
		return;
	}

	if (!ClientCanVote(ent->client))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "You are not allowed to call a vote as a spectator.\n");
		return;
	}

	if (g_vote_limit->integer && ent->client->pers.vote_count >= g_vote_limit->integer)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "You have called the maximum number of votes ({}).\n", g_vote_limit->integer);
		return;
	}

	if (!g_allow_vote_midgame->integer && level.match_state >= matchst_t::MATCH_COUNTDOWN)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Voting is only allowed during the warm up period.\n");
		return;
	}

	// Initialize vote state.
	level.vote_state.caller = ent->client;
	level.vote_state.start_time = level.time;
	level.vote_state.yes_votes = 1;
	level.vote_state.no_votes = 0;

	// Count eligible voters (non-bot humans who can vote).
	// Diagnostic: iterate ALL client slots to reveal why clients may be invisible.
	level.vote_state.num_eligible = 0;
	MuffModeLog("DEBUG", "VoteEligibility: maxclients=%d, scanning all slots...", (int)game.maxclients);
	for (uint32_t ve_i = 0; ve_i < (uint32_t)game.maxclients; ve_i++)
	{
		gentity_t *ec = &g_entities[1 + ve_i];
		bool has_client = ec->client != nullptr;
		MuffModeLog("DEBUG", "VoteEligibility: slot %d, inuse=%d, client=%p, connected=%d, is_bot=%d, svflags_bot=%d, team=%d, duel_queued=%d, name='%s'",
			(int)ve_i, (int)ec->inuse, (void *)ec->client,
			has_client ? (int)ec->client->pers.connected : -1,
			has_client ? (int)ec->client->sess.is_a_bot : -1,
			(int)((ec->svflags & SVF_BOT) != 0),
			has_client ? (int)ec->client->sess.team : -1,
			has_client ? (int)ec->client->sess.duel_queued : -1,
			has_client ? ec->client->resp.netname : "(no client)");
		if (!ec->inuse || !has_client || !ec->client->pers.connected)
			continue;
		if (ec->client->sess.is_a_bot)
			continue;
		if (!ClientCanVote(ec->client))
			continue;
		level.vote_state.num_eligible++;
	}

	MuffModeLog("VOTE", "Vote started: %s %s by %s (%d eligible)",
		level.vote_state.command->name,
		level.vote_state.arg.empty() ? "" : level.vote_state.arg.c_str(),
		ent->client->resp.netname,
		level.vote_state.num_eligible);

	MuffModeLog("DEBUG", "VoteCommandStore: about to broadcast (arg_empty=%d, arg_len=%d, arg_ptr=%p, cmd_name=%s, netname=%s)",
		(int)level.vote_state.arg.empty(), (int)level.vote_state.arg.length(),
		(void *)level.vote_state.arg.c_str(), level.vote_state.command->name, ent->client->resp.netname);

	// Broadcast.
	if (level.vote_state.arg.empty())
		gi.LocBroadcast_Print(PRINT_CENTER, "{} called a vote:\n{}\n", ent->client->resp.netname, level.vote_state.command->name);
	else
		gi.LocBroadcast_Print(PRINT_CENTER, "{} called a vote:\n{} {}\n", ent->client->resp.netname, level.vote_state.command->name, level.vote_state.arg.c_str());

	MuffModeLog("DEBUG", "VoteCommandStore: broadcast done, resetting votes");

	// Caller auto-votes yes, everyone else reset.
	for (auto ec : active_clients())
		ec->client->pers.voted = ec == ent ? 1 : 0;

	ent->client->pers.vote_count++;

	MuffModeLog("DEBUG", "VoteCommandStore: votes reset, playing announcer sound");
	AnnouncerSound(world, "vote_now", "misc/pc_up.wav", true);

	MuffModeLog("DEBUG", "VoteCommandStore: announcer done, transitioning to ACTIVE");
	MM_TransitionVoteState(VoteState::ACTIVE);

	MuffModeLog("DEBUG", "VoteCommandStore: state=ACTIVE, opening vote menus for non-callers");

	// Open vote menu for eligible non-caller clients.
	for (auto ec : active_clients())
	{
		if (ec->svflags & SVF_BOT || ec->client->sess.is_a_bot)
			continue;
		if (ec->client == level.vote_state.caller)
			continue;
		if (!ClientCanVote(ec->client))
			continue;

		int ci = (int)(ec->client - game.clients);
		MuffModeLog("DEBUG", "VoteCommandStore: opening vote menu for client %d (%s), menu=%p, inmenu=%d",
			ci, ec->client->resp.netname, (void *)ec->client->menu, (int)ec->client->inmenu);

		ec->client->showinventory = false;
		ec->client->showhelp = false;
		ec->client->showscores = false;
		gentity_t *e = ec->client->follow_target ? ec->client->follow_target : ec;
		ec->client->ps.stats[STAT_SHOW_STATUSBAR] = !ClientIsPlaying(e->client) ? 0 : 1;
		P_Menu_Close(ec);
		G_Menu_Vote_Open(ec);

		MuffModeLog("DEBUG", "VoteCommandStore: vote menu opened for client %d", ci);
	}

	MuffModeLog("DEBUG", "VoteCommandStore: complete");
}

void MM_CmdCallVote(gentity_t *ent)
{
	if (!deathmatch->integer)
		return;

	MuffModeLog("DEBUG", "Cmd_CallVote_f: enter, ent=%p, client=%p, argc=%d",
		(void *)ent, (void *)ent->client, gi.argc());
	for (int i = 0; i < gi.argc(); i++)
		MuffModeLog("DEBUG", "Cmd_CallVote_f: argv(%d)=%s", i, gi.argv(i));

	// Formulate list of allowed voting commands.
	char vstr[1024] = " ";
	for (vcmds_t *cc = vote_cmds; cc->name; ++cc)
	{
		if (g_vote_flags->integer & cc->flag)
			continue;

		std::string option = std::string(G_Fmt("{} ", cc->name));
		if (Q_strlcat(vstr, option.c_str(), sizeof(vstr)) >= sizeof(vstr))
		{
			vstr[sizeof(vstr) - 1] = '\0';
			break;
		}
	}

	if (!g_allow_voting->integer || strlen(vstr) <= 1)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Voting not allowed here.\n");
		return;
	}

	// Note: g_allow_vote_midgame check is in MM_VoteCommandStore()
	// to apply to both console and menu voting.

	if (level.vote_state.state != VoteState::IDLE)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "A vote is already in progress.\n");
		return;
	}

	// If there is still a vote to be executed.
	if (level.restarted)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Previous vote command is still awaiting execution.\n");
		return;
	}

	if (!ClientCanVote(ent->client))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "You are not allowed to call a vote as a spectator.\n");
		return;
	}

	if (g_vote_limit->integer && ent->client->pers.vote_count >= g_vote_limit->integer)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "You have called the maximum number of votes ({}).\n", g_vote_limit->integer);
		return;
	}

	if (gi.argc() < 2)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <command> <params>\nValid Voting Commands:{}\n", gi.argv(0), vstr);
		return;
	}

	// Make sure it is a valid command to vote on.
	if (!MM_ValidVoteCommand(ent))
		return;

	MM_VoteCommandStore(ent);
}

void MM_CmdVote(gentity_t *ent)
{
	if (!deathmatch->integer)
		return;

	if (!ClientCanVote(ent->client))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Not allowed to vote as spectator.\n");
		return;
	}

	if (gi.argc() < 2)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} [yes/no]\nCasts your vote in current voting session.\n", gi.argv(0));
		return;
	}

	if (level.vote_state.state != VoteState::ACTIVE)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "No vote in progress.\n");
		return;
	}

	if (ent->client->pers.voted != 0)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Vote already cast.\n");
		return;
	}

	const char *arg = gi.argv(1);

	if (arg[0] == 'y' || arg[0] == 'Y' || arg[0] == '1')
		ent->client->pers.voted = 1;
	else
		ent->client->pers.voted = -1;

	gi.LocClient_Print(ent, PRINT_HIGH, "Vote cast.\n");

	// A majority will be determined in CheckVote, which will also account
	// for players entering or leaving.
}

