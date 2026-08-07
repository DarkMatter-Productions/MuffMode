// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_debug.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_gt_session.h"
#include "muffmode/mm_maps.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_statusbar.h"
#include "muffmode/mm_team.h"
#include "muffmode/mm_util.h"

#include <cstring>
#include <string>
#include <string_view>

namespace muffmode::gametype {

namespace {

int s_check_ruleset = -1;

} // namespace

bool MM_IsValidGametypeIndex(gametype_t gt)
{
	return MM_IsGametypeIndex(static_cast<int>(gt));
}

} // namespace muffmode::gametype

gametype_avail_t MM_GetGametypeAvailability(gametype_t gt)
{
	const int i = (int)gt;
	if (!MM_IsGametypeIndex(i))
		return gametype_avail_t::Removed;
	return MM_GT_TABLE[(size_t)i].availability;
}

bool MM_IsGametypeEnabled(gametype_t gt)
{
	return MM_GetGametypeAvailability(gt) == gametype_avail_t::Enabled;
}

gametype_t MM_CurrentGametype()
{
	return static_cast<gametype_t>(muffmode::gametype::g_gt_live.effective);
}

int MM_CurrentGametypeFlags()
{
	return muffmode::gametype::g_gt_live.flags;
}

bool MM_GametypeHasFlag(int flag)
{
	return (muffmode::gametype::g_gt_live.flags & flag) != 0;
}

void MM_SanitizeCurrentGametype()
{
	if (!g_gametype)
		return;

	const int requested = g_gametype->integer;
	const mm_gametype_resolution_t resolved =
		MM_ResolveGametypeState(requested, MM_Arena_Active());

	// Compare against the actual cvar integer, not a bounded endpoint.
	// Otherwise -1 while Deathmatch is active, or INT_MAX while Arena is
	// active, survives forever.
	if (requested == resolved.configured) {
		muffmode::gametype::MM_GT_PublishLive();
		return;
	}

	if (!resolved.requested_in_range) {
		gi.Com_PrintFmt("g_gametype {} is outside the supported range {}-{}; using {} ({}).\n",
			requested, (int)GT_FIRST, (int)GT_LAST,
			resolved.configured, gt_long_name[resolved.configured]);
	} else {
		const gametype_t bounded = (gametype_t)requested;
		const char *reason = MM_GetGametypeAvailability(bounded) ==
			gametype_avail_t::Disabled ? "disabled" : "removed";
		gi.Com_PrintFmt("g_gametype {} ({}) is {}; using {} ({}).\n",
			requested, gt_long_name[requested], reason,
			resolved.configured, gt_long_name[resolved.configured]);
	}

	gi.cvar_forceset("g_gametype", G_Fmt("{}", resolved.configured).data());
	muffmode::gametype::MM_GT_PublishLive();
}

std::string MM_GetEnabledGametypesList()
{
	std::string result;

	for (int i = (int)GT_FIRST; i <= (int)GT_LAST; i++) {
		const gametype_t gt = (gametype_t)i;
		if (!MM_IsGametypeEnabled(gt))
			continue;
		if (!result.empty())
			result += "|";
		result += gt_short_name[i];
	}

	return result;
}

void MM_CheckRuleset()
{
	if (game.ruleset && muffmode::gametype::s_check_ruleset == g_ruleset->modified_count)
		return;

	game.ruleset = (ruleset_t)clamp(g_ruleset->integer, (int)RS_NONE + 1, (int)RS_NUM_RULESETS - 1);

	if ((int)game.ruleset != g_ruleset->integer)
		gi.cvar_forceset("g_ruleset", G_Fmt("{}", (int)game.ruleset).data());

	muffmode::gametype::s_check_ruleset = g_ruleset->modified_count;

	// re-arm the top-right match info notice for everyone already in the match
	MM_MatchInfoHud_ShowAll();

	gi.LocBroadcast_Print(PRINT_HIGH, "Ruleset: {}\n", rs_long_name[(int)game.ruleset]);
}

// [MuffMode] Thin adapters. Every gametype mutation is now one transaction
// owned by muffmode::gametype::MM_GT_Submit; these keep the historical entry
// points and signatures so callers do not have to know that.
void MM_ChangeGametype(gametype_t gt, bool force_cfg)
{
	namespace session = muffmode::gametype;

	if (!MM_IsGametypeEnabled(gt)) {
		const gametype_avail_t avail = MM_GetGametypeAvailability(gt);
		const char *reason = avail == gametype_avail_t::Disabled ? "disabled" : "removed";
		if (session::MM_IsValidGametypeIndex(gt)) {
			gi.Com_PrintFmt("Gametype {} ({}) is {} and cannot be selected.\n",
				gt_short_name[(int)gt], gt_long_name[(int)gt], reason);
		} else {
			gi.Com_PrintFmt("Gametype index {} is {} and cannot be selected.\n",
				(int)gt, reason);
		}
		return;
	}

	session::mm_gt_request_t request;
	request.source = session::gt_source_t::Admin;
	request.gametype = (int)gt;
	// Selecting a gametype directly drops a factory that belongs to another
	// mode; Resolve does that. Selecting the same gametype keeps it.
	request.force_reapply = force_cfg;

	const session::gt_reject_t reject = session::MM_GT_Submit(request);
	if (reject != session::gt_reject_t::None &&
		reject != session::gt_reject_t::NoChange)
		gi.Com_PrintFmt("Gametype change refused: {}.\n",
			session::MM_GT_RejectText(reject));
}

void MM_GTSetLongName()
{
	// A fresh level: force the recompose even if nothing the fingerprint covers
	// has moved, because level.gametype_name itself was just reset.
	muffmode::gametype::MM_GT_MarkNameDirty();
	muffmode::gametype::MM_GT_RefreshName();
}
