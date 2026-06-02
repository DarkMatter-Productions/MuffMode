// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_debug.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_maps.h"

namespace {

int s_check_ruleset = -1;

// Gametype tracking variables used by both MM_ChangeGametype() and MM_GTChanges().
int s_gt_teamplay = 0;
int s_gt_ctf = 0;
int s_gt_g_gametype = 0;
bool s_gt_teams_on = false;
gametype_t s_gt_check = GT_NONE;

} // namespace

void MM_CheckRuleset()
{
	if (game.ruleset && s_check_ruleset == g_ruleset->modified_count)
		return;

	game.ruleset = (ruleset_t)clamp(g_ruleset->integer, (int)RS_NONE + 1, (int)RS_NUM_RULESETS - 1);

	if ((int)game.ruleset != g_ruleset->integer)
		gi.cvar_forceset("g_ruleset", G_Fmt("{}", (int)game.ruleset).data());

	s_check_ruleset = g_ruleset->modified_count;

	gi.LocBroadcast_Print(PRINT_HIGH, "Ruleset: {}\n", rs_long_name[(int)game.ruleset]);
}

void MM_ChangeGametype(gametype_t gt)
{
	switch (gt)
	{
	case gametype_t::GT_CTF:
		if (!ctf->integer)
			gi.cvar_forceset("ctf", "1");
		break;
	case gametype_t::GT_TDM:
		if (!teamplay->integer)
			gi.cvar_forceset("teamplay", "1");
		break;
	default:
		if (ctf->integer)
			gi.cvar_forceset("ctf", "0");
		if (teamplay->integer)
			gi.cvar_forceset("teamplay", "0");
		break;
	}

	if (!deathmatch->integer)
	{
		gi.Com_Print("Forcing deathmatch.\n");
		gi.cvar_forceset("deathmatch", "1");
	}

	if ((int)gt != g_gametype->integer)
	{
		MuffModeLog("GAMETYPE", "Changing gametype from %s (%d) to %s (%d)",
			gt_short_name[g_gametype->integer], g_gametype->integer,
			gt_short_name[(int)gt], (int)gt);
		gi.cvar_forceset("g_gametype", G_Fmt("{}", (int)gt).data());

		// Force all human clients through explicit join flow after gametype change.
		for (auto ec : active_clients())
		{
			if (!ec->client)
				continue;
			if (ec->client->sess.is_a_bot || (ec->svflags & SVF_BOT))
				continue;
			ec->client->sess.team = TEAM_NONE;
			ec->client->sess.duel_queued = false;
			ec->client->sess.initialised = false;
			ec->client->initial_menu_shown = false;
			ec->client->initial_menu_delay = level.time + 10_hz;
		}

		if (gt == gametype_t::GT_INSTAGIB)
		{
			if (!g_instagib->integer)
				gi.cvar_forceset("g_instagib", "1");
		}
		else if (g_gametype->integer == (int)gametype_t::GT_INSTAGIB)
		{
			if (g_instagib->integer)
				gi.cvar_forceset("g_instagib", "0");
		}

		if (gt == gametype_t::GT_NADEFEST)
		{
			if (!g_nadefest->integer)
				gi.cvar_forceset("g_nadefest", "1");
		}
		else if (g_gametype->integer == (int)gametype_t::GT_NADEFEST)
		{
			if (g_nadefest->integer)
				gi.cvar_forceset("g_nadefest", "0");
		}

		if (g_gametype_cfg->integer && deathmatch->integer)
			gi.AddCommandString(G_Fmt("exec gt-{}.cfg\n", gt_short_name_upper[(int)gt]).data());

		extern bool g_map_list_shuffled;
		g_map_list_shuffled = false;

		// Avoid GT_Changes() issuing a redundant reload for an explicit gametype change.
		s_gt_g_gametype = g_gametype->modified_count;
		s_gt_check = (gametype_t)g_gametype->integer;
		s_gt_teamplay = teamplay->modified_count;
		s_gt_ctf = ctf->modified_count;
	}
}

void MM_GTChanges()
{
	if (!deathmatch->integer)
		return;

	if (!level.init)
		return;

	MM_HandleMapShuffleCvarChange();

	bool changed = false;
	bool team_reset = false;
	gametype_t gt = gametype_t::GT_NONE;

	if (s_gt_g_gametype != g_gametype->modified_count)
	{
		gt = (gametype_t)clamp(g_gametype->integer, (int)GT_FIRST, (int)GT_LAST);

		if (gt != s_gt_check)
		{
			switch (gt)
			{
			case gametype_t::GT_TDM:
				if (!teamplay->integer)
					gi.cvar_forceset("teamplay", "1");
				break;
			case gametype_t::GT_CTF:
				if (!ctf->integer)
					gi.cvar_forceset("ctf", "1");
				break;
			default:
				if (teamplay->integer)
					gi.cvar_forceset("teamplay", "0");
				if (ctf->integer)
					gi.cvar_forceset("ctf", "0");
				break;
			}
			s_gt_teamplay = teamplay->modified_count;
			s_gt_ctf = ctf->modified_count;
			changed = true;
		}
	}

	if (!changed)
	{
		if (s_gt_teamplay != teamplay->modified_count)
		{
			if (teamplay->integer)
			{
				gt = gametype_t::GT_TDM;
				if (!teamplay->integer)
					gi.cvar_forceset("teamplay", "1");
				if (ctf->integer)
					gi.cvar_forceset("ctf", "0");
			}
			else
			{
				gt = gametype_t::GT_FFA;
				if (teamplay->integer)
					gi.cvar_forceset("teamplay", "0");
				if (ctf->integer)
					gi.cvar_forceset("ctf", "0");
			}
			changed = true;
			s_gt_teamplay = teamplay->modified_count;
			s_gt_ctf = ctf->modified_count;
		}

		if (s_gt_ctf != ctf->modified_count)
		{
			if (ctf->integer)
			{
				gt = gametype_t::GT_CTF;
				if (teamplay->integer)
					gi.cvar_forceset("teamplay", "0");
				if (!ctf->integer)
					gi.cvar_forceset("ctf", "1");
			}
			else
			{
				gt = gametype_t::GT_TDM;
				if (!teamplay->integer)
					gi.cvar_forceset("teamplay", "1");
				if (ctf->integer)
					gi.cvar_forceset("ctf", "0");
			}
			changed = true;
			s_gt_teamplay = teamplay->modified_count;
			s_gt_ctf = ctf->modified_count;
		}
	}

	if (!changed || gt == gametype_t::GT_NONE)
		return;

	if (s_gt_teams_on != Teams())
	{
		team_reset = true;
		s_gt_teams_on = Teams();
	}

	if (team_reset)
	{
		for (auto ec : active_clients())
		{
			SetIntermissionPoint();

			ec->s.origin = level.intermission_origin;
			ec->client->ps.pmove.origin = level.intermission_origin;
			ec->client->ps.viewangles = level.intermission_angle;

			ec->client->awaiting_respawn = true;
			ec->client->ps.pmove.pm_type = PM_FREEZE;
			ec->client->ps.rdflags = RDF_NONE;
			ec->deadflag = false;
			ec->solid = SOLID_NOT;
			ec->movetype = MOVETYPE_FREECAM;
			ec->s.modelindex = 0;
			ec->svflags |= SVF_NOCLIENT;
			gi.linkentity(ec);
		}

		for (auto ec : active_clients())
		{
			if (!ClientIsPlaying(ec->client))
				continue;
			SetTeam(ec, PickTeam(-1), false, false, true);
		}
	}

	if ((int)gt != s_gt_check)
	{
		gi.cvar_forceset("g_gametype", G_Fmt("{}", (int)gt).data());
		s_gt_g_gametype = g_gametype->modified_count;
		s_gt_check = (gametype_t)g_gametype->integer;
	}
	else
	{
		return;
	}

	MuffModeLog("DEBUG", "GT_Changes: issuing gamemap '%s' (gt=%d gt_check=%d gt_g_gametype=%d g_gametype->modified_count=%d teamplay=%d ctf=%d in_frame=%d)",
		level.mapname, (int)gt, (int)s_gt_check, s_gt_g_gametype, g_gametype->modified_count,
		teamplay->integer, ctf->integer, level.in_frame);
	gi.AddCommandString(G_Fmt("gamemap {}\n", level.mapname).data());
}

void MM_SyncGametypeTracking()
{
	s_gt_teamplay = teamplay->modified_count;
	s_gt_ctf = ctf->modified_count;
	s_gt_g_gametype = g_gametype->modified_count;
	s_gt_teams_on = Teams();
}
