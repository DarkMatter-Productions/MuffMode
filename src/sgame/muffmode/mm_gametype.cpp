// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_debug.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_maps.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_team.h"
#include "muffmode/mm_util.h"

#include <cstdio>
#include <string>
#include <string_view>

namespace muffmode::gametype {

int s_check_ruleset = -1;

// Gametype tracking variables used by both MM_ChangeGametype() and MM_GTChanges().
int s_gt_teamplay = 0;
int s_gt_ctf = 0;
int s_gt_g_gametype = 0;
bool s_gt_teams_on = false;
gametype_t s_gt_check = GT_NONE;

constexpr int MM_MAX_GAMETYPE_CFG_LINES = 4096;

constexpr gametype_avail_t k_gametype_availability[GT_NUM_GAMETYPES] = {
	/* GT_NONE */ gametype_avail_t::Removed,
	/* GT_FFA */ gametype_avail_t::Enabled,
	/* GT_DUEL */ gametype_avail_t::Enabled,
	/* GT_TDM */ gametype_avail_t::Enabled,
	/* GT_CTF */ gametype_avail_t::Enabled,
	/* GT_CA */ gametype_avail_t::Enabled,
	/* GT_FREEZE */ gametype_avail_t::Enabled,
	/* GT_STRIKE */ gametype_avail_t::Enabled,
	/* GT_RR */ gametype_avail_t::Enabled,
	/* GT_LMS */ gametype_avail_t::Enabled,
	/* GT_HORDE */ gametype_avail_t::Enabled,
	/* GT_BALL */ gametype_avail_t::Removed,
	/* GT_INSTAGIB */ gametype_avail_t::Enabled,
	/* GT_NADEFEST */ gametype_avail_t::Enabled,
	/* GT_ARENA */ gametype_avail_t::Enabled,
};

// Sessions running at or below the splitscreen player cap cannot safely
// apply gt-cfg lines that raise maxclients above that cap mid-match.
bool MM_IsSlotCappedSession()
{
	cvar_t *mc = gi.cvar("maxclients", nullptr, CVAR_NOFLAGS);
	return mc && mc->integer <= (int)MAX_SPLIT_PLAYERS;
}

bool MM_IsValidGametypeIndex(gametype_t gt)
{
	const int index = static_cast<int>(gt);
	return index >= 0 && index < GT_NUM_GAMETYPES;
}

std::string MM_GametypeCfgPath(const char *cfg_name)
{
	std::string path = "baseq2/gt-";
	path += cfg_name ? cfg_name : "";
	path += ".cfg";
	return path;
}

const char *MM_SkipCfgWhitespace(const char *p)
{
	if (!p)
		return "";

	while (MM_IsAsciiWhitespace(*p))
		p++;

	return p;
}

bool MM_ConsumeCfgCommand(const char *&p, std::string_view command)
{
	const size_t len = command.size();
	if (Q_strncasecmp(p, command.data(), len))
		return false;

	if (p[len] && !MM_IsAsciiWhitespace(p[len]))
		return false;

	p += len;
	return true;
}

bool MM_ConsumeCfgTargetName(const char *&p, std::string_view name)
{
	p = MM_SkipCfgWhitespace(p);

	const size_t len = name.size();
	if (*p == '"')
	{
		const char *quoted = p + 1;
		if (Q_strncasecmp(quoted, name.data(), len) || quoted[len] != '"')
			return false;

		p = quoted + len + 1;
		if (*p && !MM_IsAsciiWhitespace(*p))
			return false;
		return true;
	}

	if (Q_strncasecmp(p, name.data(), len))
		return false;

	if (p[len] && !MM_IsAsciiWhitespace(p[len]))
		return false;

	p += len;
	return true;
}

const char *MM_FindCfgCommentStart(const char *p)
{
	bool in_quotes = false;

	for (const char *cursor = p; *cursor; cursor++)
	{
		if (*cursor == '"')
		{
			in_quotes = !in_quotes;
			continue;
		}

		if (in_quotes)
			continue;

		const bool is_comment = *cursor == '#' || (cursor[0] == '/' && cursor[1] == '/');
		if (!is_comment)
			continue;

		const char *end = cursor;
		while (end > p && MM_IsAsciiWhitespace(end[-1]))
			end--;

		if (end == p || end < cursor)
			return end;
	}

	return nullptr;
}

bool MM_IsCommentOrEmptyCfgLine(const char *line)
{
	line = MM_SkipCfgWhitespace(line);
	return !*line || *line == '#' || (line[0] == '/' && line[1] == '/');
}

std::optional<int32_t> MM_ParseCfgIntValue(const char *p)
{
	p = MM_SkipCfgWhitespace(p);

	const char *comment = MM_FindCfgCommentStart(p);
	if (!comment)
		return MM_ParseCfgIntArg(p);

	std::string value(p, comment - p);
	return MM_ParseCfgIntArg(value.c_str());
}

bool MM_TryParseMaxclientsTarget(const char *line, int *out_target)
{
	const char *p = MM_SkipCfgWhitespace(line);

	if (MM_ConsumeCfgCommand(p, "seta") ||
		MM_ConsumeCfgCommand(p, "set") ||
		MM_ConsumeCfgCommand(p, "sets") ||
		MM_ConsumeCfgCommand(p, "cvar_forceset") ||
		MM_ConsumeCfgCommand(p, "cvar_set"))
	{
		p = MM_SkipCfgWhitespace(p);
	}
	else
	{
		return false;
	}

	if (!MM_ConsumeCfgTargetName(p, "maxclients"))
		return false;
	const auto target = MM_ParseCfgIntValue(p);
	if (!target)
		return false;

	*out_target = *target;
	return true;
}

bool MM_ShouldSkipGtCfgLine(const char *line)
{
	if (!MM_IsSlotCappedSession())
		return false;

	const char *p = MM_SkipCfgWhitespace(line);

	// Lobby/setup command; never appropriate during gametype change.
	if (MM_ConsumeCfgCommand(p, "kexmultiplayer"))
		return true;

	int target = 0;
	if (MM_TryParseMaxclientsTarget(line, &target))
		return target < 1 || target > (int)MAX_SPLIT_PLAYERS;

	return false;
}

bool MM_IsOverlongCfgLine(FILE *f, std::string_view line)
{
	if (line.find('\n') != std::string_view::npos || std::feof(f))
		return false;

	int ch = 0;
	while ((ch = std::fgetc(f)) != '\n' && ch != EOF) {
	}

	return true;
}

void MM_TrimCfgLineEnd(char *line)
{
	if (!line)
		return;

	const std::string_view text(line);
	const size_t newline = text.find_first_of("\r\n");
	if (newline != std::string_view::npos)
		line[newline] = '\0';
}

void MM_ExecGametypeCfg(gametype_t gt)
{
	if (!MM_IsValidGametypeIndex(gt)) {
		gi.Com_PrintFmt("WARNING: Refusing to load gametype cfg for invalid gametype {}.\n", static_cast<int>(gt));
		return;
	}

	const char *cfg_name = gt_short_name_upper[(int)gt];
	if (!cfg_name || !*cfg_name) {
		gi.Com_PrintFmt("WARNING: Refusing to load gametype cfg for unnamed gametype {}.\n", static_cast<int>(gt));
		return;
	}

	if (!MM_IsSlotCappedSession())
	{
		std::string command = std::string(G_Fmt("exec gt-{}.cfg\n", cfg_name));
		gi.AddCommandString(command.c_str());
		return;
	}

	const std::string path = MM_GametypeCfgPath(cfg_name);
	auto file = muffmode::OpenFile(path.c_str(), "rb");
	if (!file)
	{
		gi.Com_PrintFmt("WARNING: Could not open {} for filtered load; skipping gametype cfg.\n", path.c_str());
		return;
	}

	gi.Com_PrintFmt("Loading gametype cfg (filtered for {} maxclients: skipping kexmultiplayer and maxclients > {}).\n",
		MAX_SPLIT_PLAYERS, MAX_SPLIT_PLAYERS);

	int skipped = 0;
	int executed = 0;
	int lines_read = 0;
	char line_buf[1024];

	while (std::fgets(line_buf, sizeof(line_buf), file.get()))
	{
		lines_read++;
		if (lines_read > MM_MAX_GAMETYPE_CFG_LINES)
		{
			gi.Com_PrintFmt("WARNING: Stopping filtered gametype cfg load after {} lines: {}\n",
				MM_MAX_GAMETYPE_CFG_LINES, path.c_str());
			break;
		}

		if (MM_IsOverlongCfgLine(file.get(), line_buf))
		{
			skipped++;
			continue;
		}

		MM_TrimCfgLineEnd(line_buf);

		if (MM_IsCommentOrEmptyCfgLine(line_buf))
			continue;

		if (MM_ShouldSkipGtCfgLine(line_buf))
		{
			skipped++;
			continue;
		}

		gi.AddCommandString(G_Fmt("{}\n", line_buf).data());
		executed++;
	}

	gi.Com_PrintFmt("Filtered gametype cfg complete: executed {} line{}, skipped {} line{}.\n",
		executed, executed == 1 ? "" : "s", skipped, skipped == 1 ? "" : "s");
}

} // namespace muffmode::gametype

gametype_avail_t MM_GetGametypeAvailability(gametype_t gt)
{
	const int i = (int)gt;
	if (i < 0 || i >= GT_NUM_GAMETYPES)
		return gametype_avail_t::Removed;
	return muffmode::gametype::k_gametype_availability[i];
}

bool MM_IsGametypeEnabled(gametype_t gt)
{
	return MM_GetGametypeAvailability(gt) == gametype_avail_t::Enabled;
}

gametype_t MM_CurrentGametype()
{
	const int current = g_gametype ? MM_EFFECTIVE_GT : (int)GT_FFA;
	return (gametype_t)clamp(current, (int)GT_NONE,
		(int)GT_NUM_GAMETYPES - 1);
}

int MM_CurrentGametypeFlags()
{
	return _gt[(int)MM_CurrentGametype()];
}

bool MM_GametypeHasFlag(int flag)
{
	return (MM_CurrentGametypeFlags() & flag) != 0;
}

gametype_t MM_SanitizeGametype(gametype_t gt)
{
	if (MM_IsGametypeEnabled(gt))
		return gt;
	return GT_FFA;
}

void MM_SanitizeCurrentGametype()
{
	const gametype_t raw = (gametype_t)clamp(g_gametype->integer, (int)GT_FIRST, (int)GT_LAST);
	const gametype_t sane = MM_SanitizeGametype(raw);

	if (sane == raw)
		return;

	const char *reason = MM_GetGametypeAvailability(raw) == gametype_avail_t::Disabled ? "disabled" : "removed";
	gi.Com_PrintFmt("g_gametype {} ({}) is {}; using {} ({}).\n",
		(int)raw, gt_long_name[(int)raw], reason, (int)sane, gt_long_name[(int)sane]);
	gi.cvar_forceset("g_gametype", G_Fmt("{}", (int)sane).data());
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

	gi.LocBroadcast_Print(PRINT_HIGH, "Ruleset: {}\n", rs_long_name[(int)game.ruleset]);
}

void MM_ChangeGametype(gametype_t gt, bool force_cfg)
{
	if (!MM_IsGametypeEnabled(gt)) {
		const gametype_avail_t avail = MM_GetGametypeAvailability(gt);
		const char *reason = avail == gametype_avail_t::Disabled ? "disabled" : "removed";
		gi.Com_PrintFmt("Gametype {} ({}) is {} and cannot be selected.\n",
			gt_short_name[(int)gt], gt_long_name[(int)gt], reason);
		return;
	}

	switch (gt)
	{
	case gametype_t::GT_CTF:
		if (!ctf->integer)
			gi.cvar_forceset("ctf", "1");
		if (teamplay->integer)
			gi.cvar_forceset("teamplay", "0");
		break;
	case gametype_t::GT_TDM:
	case gametype_t::GT_FREEZE:
		if (ctf->integer)
			gi.cvar_forceset("ctf", "0");
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
		const gametype_t old_gt = (gametype_t)g_gametype->integer;
		MuffModeLog("GAMETYPE", "Changing gametype from %s (%d) to %s (%d)",
			gt_short_name[(int)old_gt], (int)old_gt,
			gt_short_name[(int)gt], (int)gt);
		gi.cvar_forceset("g_gametype", G_Fmt("{}", (int)gt).data());

		// Force all clients through proper join flow after gametype change.
		for (auto ec : active_clients())
		{
			if (!ec->client)
				continue;
			if (ec->client->sess.is_a_bot || (ec->svflags & SVF_BOT)) {
				// Reset bots fully so they go through ClientConnect's spectator
				// initialization path and are re-assigned by CheckDMWarmupState
				// rather than inheriting TEAM_FREE from the previous gametype (e.g. horde).
				ec->client->sess.team = TEAM_NONE;
				P_PublishEngineTeam(ec);
				ec->client->sess.initialised = false;
				continue;
			}
			ec->client->sess.team = TEAM_NONE;
			P_PublishEngineTeam(ec);
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
		else if (old_gt == gametype_t::GT_INSTAGIB)
		{
			if (g_instagib->integer)
				gi.cvar_forceset("g_instagib", "0");
		}

		if (gt == gametype_t::GT_NADEFEST)
		{
			if (!g_nadefest->integer)
				gi.cvar_forceset("g_nadefest", "1");
		}
		else if (old_gt == gametype_t::GT_NADEFEST)
		{
			if (g_nadefest->integer)
				gi.cvar_forceset("g_nadefest", "0");
		}

		// Rocket Arena's RA3-compatible preset enables self-armor damage. Other
		// arena modes historically default to protected armor, so do not leak the
		// Arena preset into CA/Strike/Red Rover when changing modes in-place.
		if (old_gt == gametype_t::GT_ARENA && gt != gametype_t::GT_ARENA &&
			g_arena_dmg_armor->integer)
			gi.cvar_forceset("g_arena_dmg_armor", "0");

		if (g_gametype_cfg->integer && deathmatch->integer)
			muffmode::gametype::MM_ExecGametypeCfg(gt);

		MM_GTSetLongName();

		extern bool g_map_list_shuffled;
		g_map_list_shuffled = false;

		// Avoid GT_Changes() issuing a redundant reload for an explicit gametype change.
		muffmode::gametype::s_gt_g_gametype = g_gametype->modified_count;
		muffmode::gametype::s_gt_check = (gametype_t)g_gametype->integer;
		muffmode::gametype::s_gt_teamplay = teamplay->modified_count;
		muffmode::gametype::s_gt_ctf = ctf->modified_count;
	}
	else if (force_cfg && g_gametype_cfg->integer && deathmatch->integer)
	{
		// Admin re-selected the gametype that's already active: re-exec its cfg to
		// reapply the preset in place (no mode switch, so none of the change-only
		// side effects above run). Player votes pass force_cfg = false and never
		// reach here, so a same-gametype vote can't clobber settings players have
		// voted on.
		muffmode::gametype::MM_ExecGametypeCfg(gt);
		MM_GTSetLongName();
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

	if (muffmode::gametype::s_gt_g_gametype != g_gametype->modified_count)
	{
		const gametype_t raw = (gametype_t)clamp(g_gametype->integer, (int)GT_FIRST, (int)GT_LAST);
		gt = MM_SanitizeGametype(raw);

		if (gt != raw)
			gi.cvar_forceset("g_gametype", G_Fmt("{}", (int)gt).data());

		if (gt != muffmode::gametype::s_gt_check)
		{
			switch (gt)
			{
			case gametype_t::GT_TDM:
			case gametype_t::GT_FREEZE:
				if (ctf->integer)
					gi.cvar_forceset("ctf", "0");
				if (!teamplay->integer)
					gi.cvar_forceset("teamplay", "1");
				break;
			case gametype_t::GT_CTF:
				if (!ctf->integer)
					gi.cvar_forceset("ctf", "1");
				if (teamplay->integer)
					gi.cvar_forceset("teamplay", "0");
				break;
			default:
				if (teamplay->integer)
					gi.cvar_forceset("teamplay", "0");
				if (ctf->integer)
					gi.cvar_forceset("ctf", "0");
				break;
			}
			muffmode::gametype::s_gt_teamplay = teamplay->modified_count;
			muffmode::gametype::s_gt_ctf = ctf->modified_count;
			changed = true;
		}
	}

	if (!changed)
	{
		if (muffmode::gametype::s_gt_teamplay != teamplay->modified_count)
		{
			if (teamplay->integer)
			{
				gt = gametype_t::GT_TDM;
				if (ctf->integer)
					gi.cvar_forceset("ctf", "0");
			}
			else
			{
				gt = gametype_t::GT_FFA;
				if (ctf->integer)
					gi.cvar_forceset("ctf", "0");
			}
			changed = true;
			muffmode::gametype::s_gt_teamplay = teamplay->modified_count;
			muffmode::gametype::s_gt_ctf = ctf->modified_count;
		}

		if (muffmode::gametype::s_gt_ctf != ctf->modified_count)
		{
			if (ctf->integer)
			{
				gt = gametype_t::GT_CTF;
				if (teamplay->integer)
					gi.cvar_forceset("teamplay", "0");
			}
			else
			{
				gt = gametype_t::GT_TDM;
				if (!teamplay->integer)
					gi.cvar_forceset("teamplay", "1");
			}
			changed = true;
			muffmode::gametype::s_gt_teamplay = teamplay->modified_count;
			muffmode::gametype::s_gt_ctf = ctf->modified_count;
		}
	}

	if (!changed || gt == gametype_t::GT_NONE)
		return;

	if (muffmode::gametype::s_gt_teams_on != Teams())
	{
		team_reset = true;
		muffmode::gametype::s_gt_teams_on = Teams();
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

	if ((int)gt != muffmode::gametype::s_gt_check)
	{
		gi.cvar_forceset("g_gametype", G_Fmt("{}", (int)gt).data());
		muffmode::gametype::s_gt_g_gametype = g_gametype->modified_count;
		muffmode::gametype::s_gt_check = (gametype_t)g_gametype->integer;
	}
	else
	{
		return;
	}

	MuffModeLog("DEBUG", "GT_Changes: issuing gamemap '%s' (gt=%d gt_check=%d gt_g_gametype=%d g_gametype->modified_count=%d teamplay=%d ctf=%d in_frame=%d)",
		level.mapname, (int)gt, (int)muffmode::gametype::s_gt_check, muffmode::gametype::s_gt_g_gametype, g_gametype->modified_count,
		teamplay->integer, ctf->integer, level.in_frame);
	if (!MM_IsSafeMapToken(level.mapname)) {
		gi.Com_PrintFmt("ERROR: Current map name is unsafe: {}\n", level.mapname);
		return;
	}

	gi.AddCommandString(G_Fmt("gamemap \"{}\"\n", level.mapname).data());
}

void MM_SyncGametypeTracking()
{
	muffmode::gametype::s_gt_teamplay = teamplay->modified_count;
	muffmode::gametype::s_gt_ctf = ctf->modified_count;
	muffmode::gametype::s_gt_g_gametype = g_gametype->modified_count;
	muffmode::gametype::s_gt_teams_on = Teams();
}

void MM_GTSetLongName()
{
	const char *s;
	if (deathmatch->integer) {
		if (GT(GT_INSTAGIB)) {
			s = gt_long_name[GT_INSTAGIB];
		} else if (GT(GT_NADEFEST)) {
			s = gt_long_name[GT_NADEFEST];
		} else if (GT_RAW(GT_ARENA)) {
			// Individual arenas own their mutators; global modification cvars
			// must not rename the whole multi-arena session.
			s = MM_Arena_Active() ? gt_long_name[GT_ARENA] : "Rocket Arena (inactive)";
		} else if (GT(GT_CTF)) {
			if (g_instagib->integer) {
				s = "Insta-CTF";
			} else if (g_vampiric_damage->integer) {
				s = "Vampiric CTF";
			} else if (g_frenzy->integer) {
				s = "Frenzy CTF";
			} else if (g_nadefest->integer) {
				s = "NadeFest CTF";
			} else if (g_quadhog->integer) {
				s = "Quad Hog CTF";
			} else {
				s = gt_long_name[GT_CTF];
			}
		} else if (GT(GT_CA)) {
			if (g_instagib->integer) {
				s = "Insta-CA";
			} else if (g_vampiric_damage->integer) {
				s = "Vampiric CA";
			} else if (g_frenzy->integer) {
				s = "Frenzy CA";
			} else if (g_nadefest->integer) {
				s = "NadeFest CA";
			} else if (g_quadhog->integer) {
				s = "Quad Hog CA";
			} else {
				s = gt_long_name[GT_CA];
			}
		} else if (GT(GT_RR)) {
			if (g_instagib->integer) {
				s = "Insta-RR";
			} else if (g_vampiric_damage->integer) {
				s = "Vampiric RR";
			} else if (g_frenzy->integer) {
				s = "Frenzy RR";
			} else if (g_nadefest->integer) {
				s = "NadeFest RR";
			} else if (g_quadhog->integer) {
				s = "Quad Hog RR";
			} else {
				s = gt_long_name[GT_RR];
			}
		} else if (GT(GT_LMS)) {
			if (g_instagib->integer) {
				s = "Insta-LMS";
			} else if (g_vampiric_damage->integer) {
				s = "Vampiric LMS";
			} else if (g_frenzy->integer) {
				s = "Frenzy LMS";
			} else if (g_nadefest->integer) {
				s = "NadeFest LMS";
			} else if (g_quadhog->integer) {
				s = "Quad Hog LMS";
			} else {
				s = gt_long_name[GT_LMS];
			}
		} else if (GT(GT_STRIKE)) {
			if (g_instagib->integer) {
				s = "Insta-Strike";
			} else if (g_vampiric_damage->integer) {
				s = "Vampiric Strike";
			} else if (g_frenzy->integer) {
				s = "Frenzy Strike";
			} else if (g_nadefest->integer) {
				s = "NadeFest Strike";
			} else if (g_quadhog->integer) {
				s = "Quad Hog Strike";
			} else {
				s = gt_long_name[GT_STRIKE];
			}
		} else if (GT(GT_TDM)) {
			if (g_instagib->integer) {
				s = "Insta-TDM";
			} else if (g_vampiric_damage->integer) {
				s = "Vampiric TDM";
			} else if (g_frenzy->integer) {
				s = "Frenzy TDM";
			} else if (g_nadefest->integer) {
				s = "NadeFest TDM";
			} else if (g_quadhog->integer) {
				s = "Quad Hog TDM";
			} else {
				s = gt_long_name[GT_TDM];
			}
		} else if (GT(GT_DUEL)) {
			if (g_instagib->integer) {
				s = "Insta-Duel";
			} else if (g_vampiric_damage->integer) {
				s = "Vampiric Duel";
			} else if (g_frenzy->integer) {
				s = "Frenzy Duel";
			} else if (g_nadefest->integer) {
				s = "NadeFest Duel";
			} else if (g_quadhog->integer) {
				s = "Quad Hog Duel";
			} else {
				s = gt_long_name[GT_DUEL];
			}
		} else if (GT(GT_HORDE)) {
			if (g_instagib->integer) {
				s = "Insta-Horde";
			} else if (g_vampiric_damage->integer) {
				s = "Vampiric Horde";
			} else if (g_frenzy->integer) {
				s = "Frenzy Horde";
			} else if (g_nadefest->integer) {
				s = "NadeFest Horde";
			} else if (g_quadhog->integer) {
				s = "Quad Hog Horde";
			} else {
				s = gt_long_name[GT_HORDE];
			}
		} else {
			const gametype_t gt = (gametype_t)clamp(g_gametype->integer, (int)GT_FIRST, (int)GT_LAST);
			if (g_instagib->integer) {
				s = "InstaGib";
			} else if (g_vampiric_damage->integer) {
				s = G_Fmt("Vampiric {}", gt_long_name[(int)gt]).data();
			} else if (g_frenzy->integer) {
				s = G_Fmt("Frenzy {}", gt_long_name[(int)gt]).data();
			} else if (g_nadefest->integer) {
				s = gt_long_name[GT_NADEFEST];
			} else if (g_quadhog->integer) {
				s = G_Fmt("Quad Hog {}", gt_long_name[(int)gt]).data();
			} else {
				s = gt_long_name[(int)gt];
			}
		}
	} else {
		if (coop->integer) {
			s = "Co-op";
		} else {
			s = "Single Player";
		}
	}
	if (s)
		muffmode::CopyString(level.gametype_name, s);
}
