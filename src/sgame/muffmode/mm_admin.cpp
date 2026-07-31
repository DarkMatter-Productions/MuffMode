// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_admin.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_map_pool.h"
#include "muffmode/mm_maps.h"
#include "muffmode/mm_match.h"
#include "muffmode/mm_util.h"
#include "muffmode/mm_vote.h"

#include <string>

namespace muffmode::admin {

int MM_AdminCvarInteger(cvar_t *cvar)
{
	return muffmode::CvarInteger(cvar);
}

float MM_AdminCvarValue(cvar_t *cvar)
{
	return muffmode::CvarValue(cvar);
}

const char *MM_AdminCvarString(cvar_t *cvar)
{
	return muffmode::CvarString(cvar);
}

bool MM_AdminCvarEnabled(cvar_t *cvar)
{
	return muffmode::CvarEnabled(cvar);
}

void MM_AdminForceSetInteger(const char *name, int value)
{
	const std::string text = fmt::format("{}", value);
	gi.cvar_forceset(name, text.c_str());
}

bool MM_AdminQueueGamemap(const char *mapname)
{
	if (!MM_IsSafeMapToken(mapname))
		return false;

	const std::string command = fmt::format("gamemap \"{}\"\n", mapname);
	gi.AddCommandString(command.c_str());
	return true;
}

bool MM_AdminParseYesNo(const char *arg, bool *out)
{
	if (!arg || !out)
		return false;

	if (muffmode::CStringEqualsI(arg, "yes") || muffmode::CStringEqualsI(arg, "y") || muffmode::CStringEquals(arg, "1")) {
		*out = true;
		return true;
	}

	if (muffmode::CStringEqualsI(arg, "no") || muffmode::CStringEqualsI(arg, "n") || muffmode::CStringEquals(arg, "0")) {
		*out = false;
		return true;
	}

	return false;
}

bool MM_RequireExactCommandArgc(gentity_t *ent, int expected, const char *usage)
{
	if (!ent || !ent->client)
		return false;

	if (MM_IsExactArgcValid(gi.argc(), expected))
		return true;

	gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {}\n", usage);
	return false;
}

bool MM_RequireNoCommandArgs(gentity_t *ent)
{
	return MM_RequireExactCommandArgc(ent, 1, gi.argv(0));
}

gametype_t MM_CurrentGametypeForAdminDisplay()
{
	return (gametype_t)clamp(MM_AdminCvarInteger(g_gametype), (int)GT_FIRST, (int)GT_LAST);
}

ruleset_t MM_CurrentRulesetForAdminDisplay()
{
	return (ruleset_t)clamp((int)game.ruleset, (int)RS_NONE + 1, (int)RS_NUM_RULESETS - 1);
}

} // namespace muffmode::admin

namespace admin = muffmode::admin;

void MM_CmdDoctor(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	if (!admin::MM_RequireNoCommandArgs(ent))
		return;

	int errors = 0;
	int warnings = 0;
	int infos = 0;

	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "\n[MuffMode Doctor] Running diagnostics...\n");

	auto report = [&](const char *severity, const char *problem, const char *fix) {
		if (muffmode::CStringEqualsI(severity, "ERROR"))
			errors++;
		else if (muffmode::CStringEqualsI(severity, "WARN"))
			warnings++;
		else
			infos++;

		gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "[{}] {}\n", severity, problem);
		if (fix && fix[0])
			gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "      Suggestion: {}\n", fix);
	};

	const bool readyup_enabled = admin::MM_AdminCvarEnabled(g_dm_do_readyup);
	const float warmup_ready_percentage = admin::MM_AdminCvarValue(g_warmup_ready_percentage);
	const int min_players = admin::MM_AdminCvarInteger(minplayers);
	const int max_players = admin::MM_AdminCvarInteger(maxplayers);
	const bool voting_enabled = admin::MM_AdminCvarEnabled(g_allow_voting);
	const int vote_limit = admin::MM_AdminCvarInteger(g_vote_limit);
	const int round_limit = admin::MM_AdminCvarInteger(roundlimit);
	const int round_countdown = admin::MM_AdminCvarInteger(g_round_countdown);
	const float round_time_limit = admin::MM_AdminCvarValue(roundtimelimit);

	if (readyup_enabled && !admin::MM_AdminCvarEnabled(g_dm_do_warmup))
	{
		report("ERROR",
			"g_dm_do_readyup is enabled while g_dm_do_warmup is disabled.",
			"set g_dm_do_warmup 1");
	}

	if (readyup_enabled && (warmup_ready_percentage <= 0.f || warmup_ready_percentage > 1.f))
	{
		report("ERROR",
			"g_warmup_ready_percentage must be in range (0.0, 1.0] when readyup is enabled.",
			"set g_warmup_ready_percentage 0.51");
	}

	if (min_players > 0 && max_players > 0 && min_players > max_players)
	{
		report("ERROR",
			"minplayers is greater than maxplayers.",
			"set minplayers <= maxplayers");
	}

	if (maxplayers && max_players <= 0)
	{
		report("ERROR",
			"maxplayers must be greater than zero.",
			"set maxplayers 2");
	}

	if (vote_limit < 0)
	{
		report("ERROR",
			"g_vote_limit cannot be negative.",
			"set g_vote_limit 0");
	}

	if (!voting_enabled && admin::MM_AdminCvarEnabled(g_allow_spec_vote))
	{
		report("WARN",
			"g_allow_spec_vote is enabled while voting is globally disabled.",
			"set g_allow_voting 1 or set g_allow_spec_vote 0");
	}

	if (!voting_enabled && admin::MM_AdminCvarEnabled(g_allow_vote_midgame))
	{
		report("WARN",
			"g_allow_vote_midgame is enabled while voting is globally disabled.",
			"set g_allow_voting 1 or set g_allow_vote_midgame 0");
	}

	if (readyup_enabled && warmup_ready_percentage >= 0.99f)
	{
		report("WARN",
			"g_warmup_ready_percentage is very high; matches may stall waiting for nearly all players.",
			"set g_warmup_ready_percentage between 0.50 and 0.80");
	}

	if (admin::MM_AdminCvarInteger(g_dm_overtime) > 0 && (GT(GT_DUEL) == 0))
	{
		report("INFO",
			"g_dm_overtime is set but currently only applies to Duel.",
			"Switch to Duel or leave as a preset for later.");
	}

	if (!MM_GametypeHasFlag(GTF_ROUNDS))
	{
		if (round_limit != 8)
		{
			report("INFO",
				"roundlimit is non-default in a non-round gametype.",
				"No action needed unless this was unintended.");
		}

		if (round_time_limit != 2.f)
		{
			report("INFO",
				"roundtimelimit is non-default in a non-round gametype.",
				"No action needed unless this was unintended.");
		}

		if (round_countdown != 10)
		{
			report("INFO",
				"g_round_countdown is non-default in a non-round gametype.",
				"No action needed unless this was unintended.");
		}
	}
	else
	{
		if (round_limit <= 0)
		{
			if (GT(GT_HORDE))
			{
				report("INFO",
					"roundlimit <= 0 in Horde runs endless waves.",
					"Intentional for endless Horde; set a positive roundlimit to cap the run.");
			}
			else if (GT(GT_STRIKE))
			{
				report("INFO",
					"roundlimit <= 0 in Capture Strike; match length uses capturelimit.",
					"Intentional for Strike; set capturelimit to control match length.");
			}
			else
			{
				report("WARN",
					"roundlimit is <= 0 in a round-based gametype.",
					"set roundlimit 8");
			}
		}

		if (round_time_limit <= 0.f)
		{
			report("WARN",
				"roundtimelimit is <= 0 in a round-based gametype.",
				"set roundtimelimit 2");
		}

		if (round_countdown < 0)
		{
			report("WARN",
				"g_round_countdown is negative.",
				"set g_round_countdown 10");
		}
	}

	if (!voting_enabled && (admin::MM_AdminCvarInteger(g_vote_flags) != 0 || vote_limit != 3))
	{
		report("INFO",
			"Vote restriction cvars are customized while voting is disabled.",
			"No action needed unless you expected votes to be active.");
	}

	if (!errors && !warnings && !infos)
		gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "[OK] No issues found.\n");

	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY,
		"[MuffMode Doctor] Summary: {} error(s), {} warning(s), {} info message(s).\n\n",
		errors, warnings, infos);
}

void MM_CmdGametype(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	if (!admin::MM_AdminCvarEnabled(deathmatch))
		return;

	if (!MM_IsExactArgcValid(gi.argc(), 2))
	{
		const gametype_t current_gt = admin::MM_CurrentGametypeForAdminDisplay();
		const std::string enabled_list = MM_GetEnabledGametypesList();
		if (!enabled_list.empty())
			gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <{}>\nChanges current gametype. Current gametype is {} ({}).\n", gi.argv(0), enabled_list.c_str(), gt_long_name[(int)current_gt], gt_short_name[(int)current_gt]);
		else
			gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <gametype>\nChanges current gametype. Current gametype is {} ({}).\n", gi.argv(0), gt_long_name[(int)current_gt], gt_short_name[(int)current_gt]);
		return;
	}

	// Listing the available gametypes (the no-argument path above) is open to everyone so
	// players can see what's on offer; actually changing the gametype stays admin-only.
	if (!admin::MM_AdminCvarEnabled(g_allow_admin) || !ent->client->sess.admin)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Only admins can change the gametype.\n");
		return;
	}

	gametype_t gt = GT_IndexFromString(gi.argv(1));
	if (gt == GT_NONE)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid gametype.\n");
		const std::string enabled_list = MM_GetEnabledGametypesList();
		if (!enabled_list.empty())
			gi.LocClient_Print(ent, PRINT_HIGH, "Valid gametypes are: {}\n", enabled_list.c_str());
		return;
	}

	if (!MM_IsGametypeEnabled(gt)) {
		const gametype_avail_t avail = MM_GetGametypeAvailability(gt);
		const char *reason = avail == gametype_avail_t::Disabled ? "disabled" : "removed";
		gi.LocClient_Print(ent, PRINT_HIGH, "Gametype {} ({}) is {} and cannot be selected.\n",
			gt_short_name[(int)gt], gt_long_name[(int)gt], reason);
		return;
	}

	if (admin::MM_AdminCvarString(g_votable_gametypes)[0] && !MM_IsGametypeVotable(gt))
		gi.LocClient_Print(ent, PRINT_HIGH, "Warning: This gametype is not in the votable list, but setting it anyway (admin override).\n");

	// force_cfg = true: admin gametype changes always (re-)exec the gt-cfg, even
	// when selecting the gametype that's already active, so admins get a reliable
	// "apply this preset" command. (Player votes go through ChangeGametype() with
	// force_cfg = false and never re-exec on a same-gametype vote.)
	MM_ChangeGametype(gt, true);
	gi.AddCommandString("sv gt_changemap_first\n");
}

void MM_CmdRuleset(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	if (!admin::MM_AdminCvarEnabled(deathmatch))
		return;

	if (!MM_IsExactArgcValid(gi.argc(), 2))
	{
		const ruleset_t current_rs = admin::MM_CurrentRulesetForAdminDisplay();
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <q2re|mm|q3a|q2reb|q|qc>\nChanges current ruleset. Current ruleset is {} ({}).\n", gi.argv(0), rs_long_name[(int)current_rs], (int)current_rs);
		return;
	}

	ruleset_t rs = RS_IndexFromString(gi.argv(1));
	if (rs == RS_NONE)
	{
		gi.Client_Print(ent, PRINT_HIGH, "Invalid ruleset.\n");
		return;
	}

	admin::MM_AdminForceSetInteger("g_ruleset", (int)rs);
}

void MM_CmdSetMap(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	if (!MM_IsExactArgcValid(gi.argc(), 2))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <mapname>\nChanges to a map within the map pool or list.\n", gi.argv(0));
		return;
	}

	MM_HandleMapPoolCvarChanges();
	std::string resolved_map;
	if (!muffmode::maps::ResolveConfiguredMap(gi.argv(1), resolved_map))
	{
		gi.Client_Print(ent, PRINT_HIGH, "Map name is not valid.\n");
		return;
	}

	if (!admin::MM_AdminQueueGamemap(resolved_map.c_str())) {
		gi.Client_Print(ent, PRINT_HIGH, "Map name is not safe to queue.\n");
		return;
	}

	gi.LocBroadcast_Print(
		PRINT_HIGH, "[ADMIN]: Changing map to {}\n", resolved_map.c_str());
}

// [MuffMode] Admin match-control and session command bodies (moved from sgame/core/commands.cpp).
/*
=================
MM_CmdStartMatch
=================
*/
void MM_CmdStartMatch(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (!admin::MM_RequireNoCommandArgs(ent))
		return;

	if (MM_Arena_AdminStart(ent))
		return;

	if (level.match_state > matchst_t::MATCH_WARMUP_READYUP) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Match has already started.\n");
		return;
	}

	gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Forced match start.\n");
	Match_Start();
}

/*
=================
MM_CmdEndMatch
=================
*/
void MM_CmdEndMatch(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (!admin::MM_RequireNoCommandArgs(ent))
		return;

	if (MM_Arena_AdminEnd(ent))
		return;

	if (level.match_state < matchst_t::MATCH_IN_PROGRESS) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Match has not yet begun.\n");
		return;
	}
	if (level.intermission_time) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Match has already ended.\n");
		return;
	}
	QueueIntermission("[ADMIN]: Forced match end.", true, false);
}

/*
=================
MM_CmdResetMatch
=================
*/
void MM_CmdResetMatch(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (!admin::MM_RequireNoCommandArgs(ent))
		return;

	if (MM_Arena_AdminReset(ent))
		return;

	if (level.match_state < matchst_t::MATCH_IN_PROGRESS) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Match has not yet begun.\n");
		return;
	}
	if (level.intermission_time) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Match has already ended.\n");
		return;
	}
	
	gi.LocBroadcast_Print(PRINT_HIGH, "[ADMIN]: Forced match reset.\n");
	Match_Reset();
}

/*
=================
MM_CmdForceVote
=================
*/
void MM_CmdForceVote(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (!admin::MM_AdminCvarEnabled(deathmatch))
		return;

	if (!admin::MM_RequireExactCommandArgc(ent, 2, G_Fmt("{} <yes|no>", gi.argv(0)).data()))
		return;

	if (level.vote_state.state == VoteState::IDLE) {
		gi.LocClient_Print(ent, PRINT_HIGH, "No vote in progress.\n");
		return;
	}

	bool force_yes = false;
	if (!admin::MM_AdminParseYesNo(gi.argv(1), &force_yes)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <yes|no>\n", gi.argv(0));
		return;
	}

	if (force_yes) {
		gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Passed the vote.\n");
		if (level.vote_state.state == VoteState::ACTIVE) {
			TransitionVoteState(VoteState::PASSED);
		} else if (level.vote_state.state == VoteState::PASSED) {
			// Already passed, just execute immediately
			TransitionVoteState(VoteState::EXECUTING);
		}
	} else {
		gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Failed the vote.\n");
		TransitionVoteState(VoteState::FAILED);
	}
}

void MM_CmdMapRestart(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (!admin::MM_RequireNoCommandArgs(ent))
		return;

	if (!MM_IsSafeMapToken(level.mapname)) {
		gi.Client_Print(ent, PRINT_HIGH, "Current map name is not safe to restart.\n");
		return;
	}

	gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Session reset.\n");

	admin::MM_AdminQueueGamemap(level.mapname);
}

void MM_CmdNextMap(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (!admin::MM_RequireNoCommandArgs(ent))
		return;

	gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Changing to next map.\n");
	Match_End();
	level.intermission_exit = true;
}

void MM_CmdAdmin(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (!admin::MM_AdminCvarEnabled(g_allow_admin)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Administration is disabled\n");
		return;
	}

	if (ent->client->sess.admin) {
		gi.Client_Print(ent, PRINT_HIGH, "You already have administrative rights.\n");
		return;
	}

	const char *password = admin::MM_AdminCvarString(admin_password);
	if (!password[0]) {
		gi.Client_Print(ent, PRINT_HIGH, "Administration password is not configured.\n");
		return;
	}

	if (!admin::MM_RequireExactCommandArgc(ent, 2, G_Fmt("{} <password>", gi.argv(0)).data()))
		return;

	if (!muffmode::CStringEqualsI(password, gi.argv(1))) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid administration password.\n");
		return;
	}

	ent->client->sess.admin = true;
	gi.LocBroadcast_Print(PRINT_HIGH, "{} has become an admin.\n", ent->client->resp.netname);
}
