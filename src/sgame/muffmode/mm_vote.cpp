// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "core/debug_log.h"
#include "muffmode/mm_announcer.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_captain.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_maps.h"
#include "muffmode/mm_match.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_team.h"
#include "muffmode/mm_util.h"
#include "muffmode/mm_vote.h"
#include "muffmode/mm_vote_menu.h"

#include <algorithm>
#include <string>
#include <vector>

namespace muffmode::vote {
bool s_vote_validation_context_active = false;
vcmds_t *s_vote_validation_cmd = nullptr;
std::string s_vote_validation_arg;

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

int MM_VoteArgc()
{
	if (!s_vote_validation_context_active)
		return gi.argc();
	return s_vote_validation_arg.empty() ? 2 : 3;
}

const char *MM_VoteArgv(int index)
{
	if (!s_vote_validation_context_active)
		return gi.argv(index);
	if (index == 0)
		return "callvote";
	if (index == 1)
		return (s_vote_validation_cmd && s_vote_validation_cmd->name) ? s_vote_validation_cmd->name : "";
	if (index == 2)
		return s_vote_validation_arg.c_str();
	return "";
}

bool MM_ParseVoteNonNegativeInt(const char *text, int &out)
{
	const auto value = MM_ParseNonNegativeIntArg(text);
	if (!value)
		return false;

	out = *value;
	return true;
}

void MM_MarkExecutingVoteFailed()
{
	if (level.vote_state.state == VoteState::EXECUTING)
		MM_TransitionVoteState(VoteState::FAILED);
}

bool MM_ParseStoredVoteNonNegativeInt(int &out, const char *description)
{
	if (MM_ParseVoteNonNegativeInt(level.vote_state.arg.data(), out))
		return true;

	gi.LocBroadcast_Print(PRINT_HIGH, "{} vote failed: stored vote argument is invalid.\n", description);
	MM_MarkExecutingVoteFailed();
	return false;
}

bool MM_ParseStoredVoteBinaryInt(int &out, const char *description)
{
	if (!MM_ParseStoredVoteNonNegativeInt(out, description))
		return false;

	if (out == 0 || out == 1)
		return true;

	gi.LocBroadcast_Print(PRINT_HIGH, "{} vote failed: stored vote argument must be 0 or 1.\n", description);
	MM_MarkExecutingVoteFailed();
	return false;
}

bool MM_RestartCurrentMapForVote(const char *description)
{
	if (!MM_IsSafeMapToken(level.mapname)) {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} vote passed, but current map name is unsafe to restart.\n", description);
		return false;
	}

	gi.AddCommandString(G_Fmt("gamemap \"{}\"\n", level.mapname).data());
	return true;
}

bool MM_ParseVoteChoice(const char *arg, int &vote)
{
	if (!arg || !arg[0])
		return false;

	if (muffmode::CStringEqualsI(arg, "yes") || muffmode::CStringEqualsI(arg, "y") || muffmode::CStringEqualsI(arg, "1"))
	{
		vote = 1;
		return true;
	}

	if (muffmode::CStringEqualsI(arg, "no") || muffmode::CStringEqualsI(arg, "n") || muffmode::CStringEqualsI(arg, "0"))
	{
		vote = -1;
		return true;
	}

	return false;
}

bool MM_VoteValNone(gentity_t *ent)
{
	return true;
}

void MM_UpdateActiveVote()
{
	if (level.time - level.vote_state.start_time < 1_sec)
		return;

	MuffModeLog("DEBUG", "UpdateActiveVote: checking vote (state=%d, caller=%p, command=%p, arg_ptr=%p)",
		(int)level.vote_state.state, (void *)level.vote_state.caller,
		(void *)level.vote_state.command, (void *)level.vote_state.arg.c_str());

	if (level.time - level.vote_state.start_time >= 30_sec)
	{
		gi.LocBroadcast_Print(PRINT_HIGH, "Vote timed out.\n");
		MM_Announce(mm_announce_event_t::VoteFailed, world);
		MM_TransitionVoteState(VoteState::FAILED);
		return;
	}

	// Recount votes from client state each frame.
	level.vote_state.yes_votes = 0;
	level.vote_state.no_votes = 0;
	level.vote_state.num_eligible = 0;
	for (auto ec : active_clients())
	{
		if (ec->client->sess.is_a_bot)
			continue;
		if (!ClientCanVote(ec->client))
			continue;
		level.vote_state.num_eligible++;
		if (ec->client->pers.voted == 1)
			level.vote_state.yes_votes++;
		else if (ec->client->pers.voted == -1)
			level.vote_state.no_votes++;
	}

	int halfpoint = level.vote_state.num_eligible / 2;

	// Avoid 0 >= 0 turning an empty eligible-voter set into an instant failure.
	if (level.vote_state.num_eligible == 0)
		return;

	if (level.vote_state.yes_votes > halfpoint)
	{
		gi.LocBroadcast_Print(PRINT_HIGH, "Vote passed.\n");
		MM_Announce(mm_announce_event_t::VotePassed, world);
		MM_TransitionVoteState(VoteState::PASSED);
	}
	else if (level.vote_state.no_votes >= level.vote_state.num_eligible - halfpoint)
	{
		gi.LocBroadcast_Print(PRINT_HIGH, "Vote failed.\n");
		MM_Announce(mm_announce_event_t::VoteFailed, world);
		MM_TransitionVoteState(VoteState::FAILED);
	}
}

bool MM_IsMapValidImpl(const char *mapname)
{
	return muffmode::maps::ContainsConfiguredMap(mapname);
}

void MM_PrintAvailableMaps(gentity_t *ent)
{
	std::vector<std::string> all_maps = muffmode::maps::CollectConfiguredMaps();

	if (all_maps.empty())
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "No map list or pool configured.\n");
		return;
	}

	std::sort(all_maps.begin(), all_maps.end(), muffmode::CStringLessI);

	const std::string display = muffmode::TruncateWithEllipsis(join_strings(all_maps, " "), 256);
	gi.LocClient_Print(ent, PRINT_HIGH, "Valid maps are: {}\n", display.c_str());
}

} // namespace muffmode::vote

namespace vote = muffmode::vote;

bool MM_ParseVoteChoice(const char *arg, int &vote)
{
	return vote::MM_ParseVoteChoice(arg, vote);
}

bool MM_VoteClientEligible(const gentity_t *ent)
{
	return ent && ent->inuse && ent->client && ent->client->pers.connected &&
		!(ent->svflags & SVF_BOT) && !ent->client->sess.is_a_bot &&
		ClientCanVote(ent->client);
}

void MM_TransitionVoteState(VoteState new_state)
{
	VoteState old_state = level.vote_state.state;

	if (old_state == new_state)
		return;

	if (!vote::MM_IsValidVoteTransition(old_state, new_state))
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

void MM_CheckVote()
{
	if (!deathmatch->integer)
		return;

	if (level.vote_state.state != VoteState::IDLE)
		MuffModeLog("DEBUG", "CheckVote: state=%d, caller=%p, command=%p",
			(int)level.vote_state.state, (void *)level.vote_state.caller, (void *)level.vote_state.command);

	switch (level.vote_state.state)
	{
	case VoteState::IDLE:
		return;

	case VoteState::ACTIVE:
		if (!level.vote_state.command || !level.vote_state.caller)
		{
			gi.LocBroadcast_Print(PRINT_HIGH, "Vote cancelled: invalid state.\n");
			MM_TransitionVoteState(VoteState::FAILED);
			return;
		}
		vote::MM_UpdateActiveVote();
		break;

	case VoteState::PASSED:
		if (level.time >= level.vote_state.execute_time)
			MM_TransitionVoteState(VoteState::EXECUTING);
		break;

	case VoteState::EXECUTING:
		MM_VotePassed();
		break;

	case VoteState::FAILED:
	case VoteState::COMPLETE:
		MM_TransitionVoteState(VoteState::IDLE);
		break;
	}
}

void MM_BeginVoteValidationContext(vcmds_t *cc, const char *arg)
{
	vote::s_vote_validation_context_active = true;
	vote::s_vote_validation_cmd = cc;
	vote::s_vote_validation_arg = arg ? arg : "";
}

void MM_EndVoteValidationContext()
{
	vote::s_vote_validation_context_active = false;
	vote::s_vote_validation_cmd = nullptr;
	vote::s_vote_validation_arg.clear();
}

void MM_VotePassed()
{
	if (!level.vote_state.command || !level.vote_state.command->func)
	{
		gi.LocBroadcast_Print(PRINT_HIGH, "Vote passed but command was lost.\n");
		MM_TransitionVoteState(VoteState::FAILED);
		return;
	}

	MuffModeLog("DEBUG", "Vote_Passed: executing command '%s'", level.vote_state.command->name);
	level.vote_state.command->func();
	if (level.vote_state.state != VoteState::EXECUTING)
	{
		MuffModeLog("DEBUG", "Vote_Passed: command changed state to %d, skipping COMPLETE", (int)level.vote_state.state);
		return;
	}

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
		gi.LocBroadcast_Print(PRINT_HIGH, "Gametype vote failed: stored vote argument is invalid.\n");
		vote::MM_MarkExecutingVoteFailed();
		return;
	}

	// Re-check votability at execution time in case g_votable_gametypes changed
	// during the 3-second PASSED->EXECUTING window (the value is validated at
	// callvote time, but could have changed by the time the vote executes).
	if (!MM_IsGametypeVotable(gt))
	{
		gi.LocBroadcast_Print(PRINT_HIGH, "Gametype vote rejected: gametype is no longer votable.\n");
		MuffModeLog("VOTE", "Vote_Pass_Gametype: gametype %d rejected by IsGametypeVotable at execution", (int)gt);
		vote::MM_MarkExecutingVoteFailed();
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

bool MM_VoteValGametype(gentity_t *ent)
{
	// Ensure exactly 3 arguments: callvote, gametype, <gametype_name>.
	if (vote::MM_VoteArgc() != 3)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: callvote gametype <gametype_name>\n");
		std::string votable_list = MM_GetVotableGametypesList();
		if (!votable_list.empty())
			gi.LocClient_Print(ent, PRINT_HIGH, "Valid gametypes are: {}\n", votable_list.c_str());
		return false;
	}

	gametype_t gt = GT_IndexFromString(vote::MM_VoteArgv(2));

	if (gt == GT_NONE)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid gametype: '{}'\n", vote::MM_VoteArgv(2));
		std::string votable_list = MM_GetVotableGametypesList();
		if (!votable_list.empty())
			gi.LocClient_Print(ent, PRINT_HIGH, "Valid gametypes are: {}\n", votable_list.c_str());
		return false;
	}

	// Check if gametype is votable.
	if (!MM_IsGametypeVotable(gt))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "This gametype is not available for voting.\n");
		std::string votable_list = MM_GetVotableGametypesList();
		if (!votable_list.empty())
			gi.LocClient_Print(ent, PRINT_HIGH, "Valid gametypes are: {}\n", votable_list.c_str());
		return false;
	}

	return true;
}

bool MM_IsGametypeVotable(gametype_t gt)
{
	if (!MM_IsGametypeEnabled(gt))
		return false;

	// If no votable list is set, allow all enabled gametypes.
	if (!g_votable_gametypes->string[0])
		return true;

	// Check if the gametype's short name is in the votable list.
	const char *votable_list = g_votable_gametypes->string;
	char *token;

	while ((token = COM_Parse(&votable_list)) && *token)
	{
		if (muffmode::CStringEqualsI(token, gt_short_name[(int)gt]))
			return true;
	}

	return false;
}

std::string MM_GetVotableGametypesList()
{
	std::string result;

	if (!g_votable_gametypes->string[0])
	{
		return MM_GetEnabledGametypesList();
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
	const int ruleset = (int)rs;
	if (ruleset <= (int)RS_NONE || ruleset >= (int)RS_NUM_RULESETS)
		return false;

	// If no votable list is set, allow all rulesets (backward compatible).
	if (!g_votable_rulesets->string[0])
		return true;

	// Check if the ruleset's short name is in the votable list.
	const char *votable_list = g_votable_rulesets->string;
	char *token;

	while ((token = COM_Parse(&votable_list)) && *token)
	{
		if (muffmode::CStringEqualsI(token, rs_short_name[ruleset]))
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
	{
		gi.LocBroadcast_Print(PRINT_HIGH, "Ruleset vote failed: stored vote argument is invalid.\n");
		vote::MM_MarkExecutingVoteFailed();
		return;
	}

	// Re-check votability at execution time in case g_votable_rulesets changed
	// during the 3-second PASSED->EXECUTING window (the value is validated at
	// callvote time, but could have changed by the time the vote executes).
	if (!MM_IsRulesetVotable(rs))
	{
		gi.LocBroadcast_Print(PRINT_HIGH, "Ruleset vote rejected: ruleset is no longer votable.\n");
		MuffModeLog("VOTE", "Vote_Pass_Ruleset: ruleset %d rejected by IsRulesetVotable at execution", (int)rs);
		vote::MM_MarkExecutingVoteFailed();
		return;
	}

	gi.cvar_forceset("g_ruleset", G_Fmt("{}", (int)rs).data());
}

bool MM_VoteValRuleset(gentity_t *ent)
{
	ruleset_t desired_rs = RS_IndexFromString(vote::MM_VoteArgv(2));
	if (desired_rs == ruleset_t::RS_NONE)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid ruleset: '{}'\n", vote::MM_VoteArgv(2));
		std::string votable_list = MM_GetVotableRulesetsList();
		if (!votable_list.empty())
			gi.LocClient_Print(ent, PRINT_HIGH, "Valid rulesets are: {}\n", votable_list.c_str());
		return false;
	}
	if ((int)desired_rs == game.ruleset)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Ruleset currently active.\n");
		return false;
	}

	// Check if ruleset is votable.
	if (!MM_IsRulesetVotable(desired_rs))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "This ruleset is not available for voting.\n");
		std::string votable_list = MM_GetVotableRulesetsList();
		if (!votable_list.empty())
			gi.LocClient_Print(ent, PRINT_HIGH, "Valid rulesets are: {}\n", votable_list.c_str());
		return false;
	}

	return true;
}

void MM_VotePassMap()
{
	MuffModeLog("DEBUG", "Vote_Pass_Map: enter, arg='%s' (len=%d, ptr=%p)",
		level.vote_state.arg.c_str(), (int)level.vote_state.arg.length(),
		(void *)level.vote_state.arg.c_str());

	if (!MM_IsSafeMapToken(level.vote_state.arg.c_str()) || level.vote_state.arg.length() >= sizeof(level.nextmap))
	{
		gi.LocBroadcast_Print(PRINT_HIGH, "Map vote failed: invalid map name.\n");
		vote::MM_MarkExecutingVoteFailed();
		return;
	}

	muffmode::CopyString(level.nextmap, level.vote_state.arg);
	MuffModeLog("DEBUG", "Vote_Pass_Map: queuing gamemap for '%s'", level.nextmap);
	gi.AddCommandString(G_Fmt("gamemap \"{}\"\n", level.nextmap).data());
}

bool MM_VoteValMap(gentity_t *ent)
{
	if (vote::MM_VoteArgc() < 3 || !vote::MM_VoteArgv(2)[0])
	{
		vote::MM_PrintAvailableMaps(ent);
		return false;
	}

	if (!vote::MM_IsMapValidImpl(vote::MM_VoteArgv(2)))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Unknown map.\n");
		vote::MM_PrintAvailableMaps(ent);
		return false;
	}

	return true;
}

bool MM_IsMapValid(const char *mapname)
{
	return vote::MM_IsMapValidImpl(mapname);
}

void MM_VotePassRestartMatch()
{
	Match_Reset();
}

void MM_VotePassNextMap()
{
	Match_End();
	level.intermission_exit = true;
}

bool MM_VoteValRandom(gentity_t *ent)
{
	int arg = 0;

	if (!vote::MM_ParseVoteNonNegativeInt(vote::MM_VoteArgv(2), arg) || arg > 100 || arg < 2)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid random value. Use a number from 2 to 100.\n");
		return false;
	}

	return true;
}

void MM_VotePassCointoss()
{
	gi.LocBroadcast_Print(PRINT_HIGH, "The coin is: {}\n", brandom() ? "HEADS" : "TAILS");
}

void MM_VotePassRandom()
{
	int arg = 0;
	if (!vote::MM_ParseStoredVoteNonNegativeInt(arg, "Random"))
		return;
	if (arg < 2 || arg > 100)
	{
		gi.LocBroadcast_Print(PRINT_HIGH, "Random vote failed: stored vote argument is out of range.\n");
		vote::MM_MarkExecutingVoteFailed();
		return;
	}

	gi.LocBroadcast_Print(PRINT_HIGH, "The random number is: {}\n", irandom(2, arg + 1));
}

void MM_VotePassUnlagged()
{
	int argi = 0;
	if (!vote::MM_ParseStoredVoteBinaryInt(argi, "Unlagged"))
		return;

	gi.LocBroadcast_Print(PRINT_HIGH, "Lag compensation has been {}.\n", argi ? "ENABLED" : "DISABLED");

	gi.cvar_forceset("g_lag_compensation", argi ? "1" : "0");
}

bool MM_VoteValUnlagged(gentity_t *ent)
{
	int arg = 0;

	if (!vote::MM_ParseVoteNonNegativeInt(vote::MM_VoteArgv(2), arg) || (arg != 0 && arg != 1))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid argument. Use 0 to disable or 1 to enable lag compensation.\n");
		return false;
	}

	if ((g_lag_compensation->integer && arg)
		|| (!g_lag_compensation->integer && !arg))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Lag compensation is already {}.\n", arg ? "ENABLED" : "DISABLED");
		return false;
	}

	return true;
}

void MM_VotePassTimelimit()
{
	int argi = 0;
	if (!vote::MM_ParseStoredVoteNonNegativeInt(argi, "Time limit"))
		return;
	if (argi > 1440)
	{
		gi.LocBroadcast_Print(PRINT_HIGH, "Time limit vote failed: stored vote argument is out of range.\n");
		vote::MM_MarkExecutingVoteFailed();
		return;
	}

	if (!argi)
		gi.LocBroadcast_Print(PRINT_HIGH, "Time limit has been DISABLED.\n");
	else
		gi.LocBroadcast_Print(PRINT_HIGH, "Time limit has been set to {}.\n", G_TimeString(argi * 60000, false));

	gi.cvar_forceset("timelimit", G_Fmt("{}", argi).data());
}

bool MM_VoteValTimelimit(gentity_t *ent)
{
	int argi = 0;

	if (!vote::MM_ParseVoteNonNegativeInt(vote::MM_VoteArgv(2), argi) || argi > 1440)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid time limit value.\n");
		return false;
	}

	if (argi == timelimit->integer)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Time limit is already set to {}.\n", G_TimeString(argi * 60000, false));
		return false;
	}
	return true;
}

void MM_VotePassScorelimit()
{
	int argi = 0;
	if (!vote::MM_ParseStoredVoteNonNegativeInt(argi, "Score limit"))
		return;

	if (argi)
		gi.LocBroadcast_Print(PRINT_HIGH, "Score limit has been set to {}.\n", argi);
	else
		gi.LocBroadcast_Print(PRINT_HIGH, "Score limit has been DISABLED.\n");

	gi.cvar_forceset(G_Fmt("{}limit", GT_ScoreLimitString()).data(), G_Fmt("{}", argi).data());
}

bool MM_VoteValScorelimit(gentity_t *ent)
{
	int argi = 0;

	if (!vote::MM_ParseVoteNonNegativeInt(vote::MM_VoteArgv(2), argi))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid score limit value.\n");
		return false;
	}

	if (argi == GT_ScoreLimit())
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Score limit is already set to {}.\n", argi);
		return false;
	}

	return true;
}

void MM_VotePassPowerups()
{
	int argi = 0;
	if (!vote::MM_ParseStoredVoteBinaryInt(argi, "Powerups"))
		return;

	gi.LocBroadcast_Print(PRINT_HIGH, "Powerups have been {}.\n", argi ? "ENABLED" : "DISABLED");

	gi.cvar_forceset("g_no_powerups", argi ? "0" : "1");

	// Restart the map so powerup changes take effect immediately.
	vote::MM_RestartCurrentMapForVote("Powerups");
}

bool MM_VoteValPowerups(gentity_t *ent)
{
	int arg = 0;

	if (!vote::MM_ParseVoteNonNegativeInt(vote::MM_VoteArgv(2), arg) || (arg != 0 && arg != 1))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid argument. Use 0 to disable or 1 to enable powerups.\n");
		return false;
	}

	bool currently_disabled = g_no_powerups->integer != 0;
	bool will_be_disabled = (arg == 0);

	if (currently_disabled == will_be_disabled)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Powerups are already {}.\n", will_be_disabled ? "DISABLED" : "ENABLED");
		return false;
	}

	return true;
}

void MM_VotePassTechs()
{
	int argi = 0;
	if (!vote::MM_ParseStoredVoteBinaryInt(argi, "Techs"))
		return;

	gi.LocBroadcast_Print(PRINT_HIGH, "Techs have been {}.\n", argi ? "ENABLED" : "DISABLED");

	gi.cvar_forceset("g_allow_techs", argi ? "1" : "0");

	// Restart the map so tech changes take effect immediately.
	vote::MM_RestartCurrentMapForVote("Techs");
}

bool MM_VoteValTechs(gentity_t *ent)
{
	if (notGT(GT_FFA) && notGT(GT_TDM) && notGT(GT_CTF) && notGT(GT_HORDE))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Techs can only be changed in FFA, TDM, CTF or Horde gametypes.\n");
		return false;
	}

	int arg = 0;

	if (!vote::MM_ParseVoteNonNegativeInt(vote::MM_VoteArgv(2), arg) || (arg != 0 && arg != 1))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid argument. Use 0 to disable or 1 to enable techs.\n");
		return false;
	}

	bool currently_enabled = AllowTechs();
	bool will_be_enabled = (arg == 1);

	if (currently_enabled == will_be_enabled)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Techs are already {}.\n", will_be_enabled ? "ENABLED" : "DISABLED");
		return false;
	}

	return true;
}

void MM_VotePassFriendlyFire()
{
	int argi = 0;
	if (!vote::MM_ParseStoredVoteBinaryInt(argi, "Friendly fire"))
		return;

	gi.LocBroadcast_Print(PRINT_HIGH, "Friendly fire has been {}.\n", argi ? "ENABLED" : "DISABLED");

	gi.cvar_forceset("g_friendly_fire", argi ? "1" : "0");
}

bool MM_VoteValFriendlyFire(gentity_t *ent)
{
	if (!Teams())
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Friendly fire can only be changed in team gametypes.\n");
		return false;
	}

	int arg = 0;

	if (!vote::MM_ParseVoteNonNegativeInt(vote::MM_VoteArgv(2), arg) || (arg != 0 && arg != 1))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid argument. Use 0 to disable or 1 to enable friendly fire.\n");
		return false;
	}

	bool currently_enabled = g_friendly_fire->integer != 0;
	bool will_be_enabled = (arg == 1);

	if (currently_enabled == will_be_enabled)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Friendly fire is already {}.\n", will_be_enabled ? "ENABLED" : "DISABLED");
		return false;
	}

	return true;
}

void MM_VotePassShuffleTeams()
{
	if (!Teams())
	{
		gi.LocBroadcast_Print(PRINT_HIGH, "Shuffle vote failed: not a team gametype.\n");
		return;
	}

	if (!TeamShuffle())
	{
		gi.LocBroadcast_Print(PRINT_HIGH, "Shuffle vote failed: teams could not be shuffled.\n");
		return;
	}

	Match_Reset();
	gi.LocBroadcast_Print(PRINT_HIGH, "Teams have been shuffled.\n");
}

bool MM_VoteValShuffleTeams(gentity_t *ent)
{
	if (!Teams())
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Shuffle teams is only available in team gametypes.\n");
		return false;
	}
	if (level.num_playing_clients < 2)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Not enough players to shuffle teams.\n");
		return false;
	}
	return true;
}

void MM_VotePassBalanceTeams()
{
	if (!Teams())
	{
		gi.LocBroadcast_Print(PRINT_HIGH, "Balance vote failed: not a team gametype.\n");
		return;
	}

	const int moved = TeamBalance(true);
	if (moved <= 0)
		gi.LocBroadcast_Print(PRINT_HIGH, "Teams are already balanced.\n");
}

bool MM_VoteValBalanceTeams(gentity_t *ent)
{
	if (!Teams())
		return false;

	return true;
}

void MM_VotePassReadyAll()
{
	if (!g_dm_do_readyup->integer || level.match_state != matchst_t::MATCH_WARMUP_READYUP)
	{
		gi.LocBroadcast_Print(PRINT_HIGH, "Ready all vote failed: not in ready-up warmup.\n");
		return;
	}
	ReadyAll();
	gi.LocBroadcast_Print(PRINT_HIGH, "All players have been readied.\n");
}

bool MM_VoteValReadyAll(gentity_t *ent)
{
	if (!g_dm_do_readyup->integer || level.match_state != matchst_t::MATCH_WARMUP_READYUP)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Ready all is only available during ready-up warmup.\n");
		return false;
	}
	return true;
}

bool MM_ValidVoteCommand(gentity_t *ent)
{
	if (!ent || !ent->client)
		return false;

	MuffModeLog("DEBUG", "ValidVoteCommand: enter, argv(1)=%s, argc=%d", gi.argv(1), gi.argc());

	level.vote_state.command = nullptr;

	vcmds_t *cc = FindVoteCmdByName(gi.argv(1));
	if (!cc)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid vote command: {}\n", gi.argv(1));
		return false;
	}

	if (!cc->val_func || !cc->func)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Vote command is not available.\n");
		return false;
	}

	if (cc->args && gi.argc() < (1 + cc->min_args))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "{}: {}\nUsage: {} {}\n", cc->name, cc->help, cc->name, cc->args);
		return false;
	}

	const int expected_argc = 1 + cc->min_args;
	if (gi.argc() > expected_argc)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Too many arguments for vote command.\nUsage: {} {}\n", cc->name, cc->args ? cc->args : "");
		return false;
	}

	if (!cc->val_func(ent))
		return false;

	level.vote_state.command = cc;

	std::string raw_arg = gi.argc() > 2 ? gi.argv(2) : "";

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
	auto clear_idle_staged_vote = []() {
		if (level.vote_state.state != VoteState::IDLE)
			return;

		level.vote_state.command = nullptr;
		level.vote_state.arg.clear();
		level.vote_state.caller = nullptr;
	};

	if (!ent || !ent->client)
	{
		clear_idle_staged_vote();
		return;
	}

	if (!level.vote_state.command)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Internal error: vote command was lost.\n");
		clear_idle_staged_vote();
		return;
	}

	if (!g_allow_voting->integer)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Voting not allowed here.\n");
		clear_idle_staged_vote();
		return;
	}

	if (g_vote_flags->integer & level.vote_state.command->flag)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "This vote type is not allowed.\n");
		clear_idle_staged_vote();
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
		clear_idle_staged_vote();
		return;
	}

	if (g_vote_limit->integer && ent->client->pers.vote_count >= g_vote_limit->integer)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "You have called the maximum number of votes ({}).\n", g_vote_limit->integer);
		clear_idle_staged_vote();
		return;
	}

	if (!g_allow_vote_midgame->integer &&
		(level.match_state >= matchst_t::MATCH_COUNTDOWN ||
		 MM_Arena_GlobalVoteBlocked(ent)))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Voting is only allowed during the warm up period.\n");
		clear_idle_staged_vote();
		return;
	}

	// Initialize vote state.
	level.vote_state.caller = ent->client;
	level.vote_state.start_time = level.time;
	level.vote_state.yes_votes = 1;
	level.vote_state.no_votes = 0;

	// Count eligible voters (non-bot humans who can vote).
	level.vote_state.num_eligible = 0;
	for (auto ec : active_clients())
	{
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
	MM_Announce(mm_announce_event_t::VoteNow, world);

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
		ec->client->ps.stats[STAT_SHOW_STATUSBAR] = (e && e->client && ClientIsPlaying(e->client)) ? 1 : 0;
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

	if (!ent || !ent->client)
		return;

	MuffModeLog("DEBUG", "Cmd_CallVote_f: enter, ent=%p, client=%p, argc=%d",
		(void *)ent, (void *)ent->client, gi.argc());
	for (int i = 0; i < gi.argc(); i++)
		MuffModeLog("DEBUG", "Cmd_CallVote_f: argv(%d)=%s", i, gi.argv(i));

	// Formulate list of allowed voting commands.
	constexpr size_t MAX_VOTE_COMMAND_LIST = 1024;
	std::string vote_command_list = " ";
	vote_command_list.reserve(128);
	for (vcmds_t *cc = vote_cmds; cc->name; ++cc)
	{
		if (g_vote_flags->integer & cc->flag)
			continue;

		const std::string option = G_Fmt("{} ", cc->name).data();
		if (vote_command_list.size() + option.size() >= MAX_VOTE_COMMAND_LIST)
			break;

		vote_command_list += option;
	}

	if (!g_allow_voting->integer || vote_command_list.size() <= 1)
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
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <command> <params>\nValid Voting Commands:{}\n", gi.argv(0), vote_command_list.c_str());
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

	if (!ent || !ent->client)
		return;

	if (MM_Arena_CastVote(ent, gi.argc() == 2 ? gi.argv(1) : nullptr))
		return;

	if (!ClientCanVote(ent->client))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Not allowed to vote as spectator.\n");
		return;
	}

	if (gi.argc() != 2)
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

	int vote = 0;
	if (!MM_ParseVoteChoice(gi.argv(1), vote))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid vote. Use yes or no.\n");
		return;
	}

	ent->client->pers.voted = vote;

	gi.LocClient_Print(ent, PRINT_HIGH, "Vote cast.\n");

	// A majority will be determined in CheckVote, which will also account
	// for players entering or leaving.
}

void MM_RevertVote(gclient_t *client)
{
	if (!client)
		return;

	if (level.vote_state.state != VoteState::ACTIVE)
		return;

	if (!level.vote_state.caller)
		return;

	if (client->pers.voted != 0)
		client->pers.voted = 0;
}

bool ValidateMenuVoteCommand(gentity_t *ent, vcmds_t *cc, const char *arg)
{
	if (!ent || !ent->client || !cc || !cc->val_func || !cc->func)
		return false;

	const char *menu_arg = arg ? arg : "";
	const int provided_argc = menu_arg[0] ? 3 : 2;

	if (cc->args && provided_argc < (1 + cc->min_args))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "{}: {}\nUsage: {} {}\n", cc->name, cc->help, cc->name, cc->args);
		return false;
	}

	MM_BeginVoteValidationContext(cc, menu_arg);
	const bool ok = cc->val_func(ent);
	MM_EndVoteValidationContext();
	return ok;
}

vcmds_t vote_cmds[] = {
	{"map",					MM_VoteValMap,				MM_VotePassMap,				1,		2,	"[mapname]",						"changes to the specified map"},
	{"nextmap",				vote::MM_VoteValNone,		MM_VotePassNextMap,			2,		1,	"",									"move to the next map in the rotation"},
	{"restart",				vote::MM_VoteValNone,		MM_VotePassRestartMatch,	4,		1,	"",									"restarts the current match"},
	{"gametype",			MM_VoteValGametype,			MM_VotePassGametype,		8,		2,	"<gametype>",						"changes the current gametype"},
	{"timelimit",			MM_VoteValTimelimit,		MM_VotePassTimelimit,		16,		2,	"<0..$>",							"alters the match time limit, 0 for no time limit"},
	{"scorelimit",			MM_VoteValScorelimit,		MM_VotePassScorelimit,		32,		2,	"<0..$>",							"alters the match score limit, 0 for no score limit"},
	{"fraglimit",			MM_VoteValScorelimit,		MM_VotePassScorelimit,		32,		2,	"<0..$>",							"alters the match score limit, 0 for no score limit (alias for scorelimit)"},
	{"shuffle",				MM_VoteValShuffleTeams,		MM_VotePassShuffleTeams,	64,		1,	"",									"shuffles teams"},
	{"unlagged",			MM_VoteValUnlagged,			MM_VotePassUnlagged,		128,	2,	"<0/1>",							"enables or disables lag compensation"},
	{"cointoss",			vote::MM_VoteValNone,		MM_VotePassCointoss,		256,	1,	"",									"invokes a HEADS or TAILS cointoss"},
	{"random",				MM_VoteValRandom,			MM_VotePassRandom,			512,	2,	"<2-100>",							"randomly selects a number from 2 to specified value"},
	{"balance",				MM_VoteValBalanceTeams,		MM_VotePassBalanceTeams,	1024,	1,	"",									"balance teams without shuffling"},
	{"ruleset",				MM_VoteValRuleset,			MM_VotePassRuleset,			2048,	2,	"<q2re|mm|q3a|q2reb|q|qc>",			"changes the current ruleset"},
	{"powerups",			MM_VoteValPowerups,			MM_VotePassPowerups,		4096,	2,	"<0/1>",							"enables or disables powerups"},
	{"friendlyfire",		MM_VoteValFriendlyFire,		MM_VotePassFriendlyFire,	8192,	2,	"<0/1>",							"enables or disables friendly fire (team modes only)"},
	{"techs",				MM_VoteValTechs,			MM_VotePassTechs,			65536,	2,	"<0/1>",							"enables or disables techs (FFA/TDM/CTF/Horde only)"},
	{"readyall",			MM_VoteValReadyAll,			MM_VotePassReadyAll,		32768,	1,	"",									"ready all players (during ready-up warmup)"},
	{nullptr,				nullptr,					nullptr,					0,		0,	nullptr,								nullptr},
};

vcmds_t *FindVoteCmdByName(const char *name)
{
	if (!name || !name[0])
		return nullptr;

	for (vcmds_t *cc = vote_cmds; cc->name; ++cc)
	{
		if (muffmode::CStringEqualsI(cc->name, name))
			return cc;
	}

	return nullptr;
}
