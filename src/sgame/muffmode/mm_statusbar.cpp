// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "core/statusbar.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_hud_stat_contracts.h"
#include "muffmode/mm_statusbar.h"

#include <fstream>

namespace muffmode::statusbar {

// Most recently built CS_STATUSBAR layout string, kept for the hud_dump dev command.
std::string g_last_layout;

// JSON-escape the layout string (it contains quoted tokens like string2 "SPECTATOR MODE").
std::string JsonEscape(const std::string &in)
{
	std::string out;
	out.reserve(in.size() + 16);
	for (char c : in) {
		switch (c) {
		case '"':  out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:   out += c; break;
		}
	}
	return out;
}


int DigitChars(int value)
{
	if (value < 0)
		value = 0;
	if (value > 999)
		value = 999;

	return value > 99 ? 3 : value > 9 ? 2 : 1;
}

// Right HUD stack: num(2) at xr(-40) — field right edge sits 6px inside the screen edge.
constexpr int32_t kRightHudFieldRight = -6;
constexpr int32_t kRightHudNumFieldWidth = 2;

// Two-row miniscore geometry — see muffmode::hud in mm_hud_stat_contracts.h.
using muffmode::hud::kBottomMiniscoreRow1Yb;
using muffmode::hud::kBottomMiniscoreRow2Yb;
using muffmode::hud::kBottomMiniscoreMetaRow1Yb;
using muffmode::hud::kMiniscoreHighlightXr;
using muffmode::hud::kMiniscoreHighlightYInset;
using muffmode::hud::kMiniscoreNumFieldWidth;
using muffmode::hud::kMiniscoreNumXr;
using muffmode::hud::kMiniscorePicXr;
using muffmode::hud::kMiniscoreRowStep;

constexpr int32_t kHudDigitWidth = 16;
// Right HUD stack column right edge (text + digits). Test: -8 (miniscore score digits stay at -28).
constexpr int32_t kRightHudStackRightXr = -2;

enum class hud_vanchor_t {
	Top,
	Bottom,
};

int32_t RightHudNumXr(int field_width)
{
	return kRightHudFieldRight - (2 + field_width * kHudDigitWidth);
}

int32_t MiniscoreAlignedNumXr(int32_t field_width)
{
	return kRightHudStackRightXr - (2 + field_width * kHudDigitWidth);
}

// Right edge for loc_rstring / loc_stat_rstring — matches kRightHudStackRightXr.
constexpr int32_t kRightHudTextXr = kRightHudStackRightXr;

int32_t RightStackLabelXr(const char *label)
{
	int32_t label_xr = ((int32_t)strlen(label) * 8) / 2 - 22;
	if (label_xr > -4)
		label_xr = -4;
	return label_xr;
}

void EmitRightLabeledValue(statusbar_t &sb, hud_vanchor_t anchor, int32_t label_y, int32_t num_y, player_stat_t stat, const char *label, int field_width = kRightHudNumFieldWidth)
{
	sb.ifstat(stat).xr(RightStackLabelXr(label));

	if (anchor == hud_vanchor_t::Top)
		sb.yt(label_y).loc_rstring(label).xr(RightHudNumXr(field_width)).yt(num_y).num(field_width, stat).endifstat();
	else
		sb.yb(label_y).loc_rstring(label).xr(RightHudNumXr(field_width)).yb(num_y).num(field_width, stat).endifstat();
}

void EmitTeamMiniscoreRow(statusbar_t &sb, hud_vanchor_t anchor, int32_t y, player_stat_t pic_stat, player_stat_t num_stat, player_stat_t highlight_stat)
{
	sb.ifstat(pic_stat).xr(kMiniscorePicXr);

	if (anchor == hud_vanchor_t::Top)
		sb.yt(y).pic(pic_stat).xr(kMiniscoreNumXr).yt(y).num(kMiniscoreNumFieldWidth, num_stat).endifstat();
	else
		sb.yb(y).pic(pic_stat).xr(kMiniscoreNumXr).yb(y).num(kMiniscoreNumFieldWidth, num_stat).endifstat();

	sb.ifstat(highlight_stat).xr(kMiniscoreHighlightXr);

	if (anchor == hud_vanchor_t::Top)
		sb.yt(y + kMiniscoreHighlightYInset).pic(highlight_stat).endifstat();
	else
		sb.yb(y + kMiniscoreHighlightYInset).pic(highlight_stat).endifstat();
}

enum class round_counter_profile_t {
	CoopStack,
	DmCorner,
};

// Coop / horde right column: gap between stack entries, and label→number (tight like old num→label @ 26).
constexpr int32_t kCoopStackItemGap = 10;
constexpr int32_t kCoopStackLabelToNumGap = 12;
constexpr int32_t kCoopStackNumFieldHeight = 24;

void EmitCoopStackAdvancePastNum(int32_t *coop_y)
{
	*coop_y += kCoopStackNumFieldHeight;
}

const char *ScoreLimitLabel();

void EmitCoopStackLabeledNum(statusbar_t &sb, int32_t *coop_y, player_stat_t stat, const char *label, int field_width)
{
	sb.ifstat(stat).xr(RightStackLabelXr(label)).yt(*coop_y += kCoopStackItemGap).loc_rstring(label).xr(RightHudNumXr(field_width)).yt(*coop_y += kCoopStackLabelToNumGap).num(field_width, stat).endifstat();
	EmitCoopStackAdvancePastNum(coop_y);
}

void EmitCoopStackLives(statusbar_t &sb, int32_t *coop_y)
{
	sb.ifstat(STAT_LIVES)
		.xr(RightStackLabelXr("Lives"))
		.yt(*coop_y)
		.loc_rstring("$g_lives")
		.xr(RightHudNumXr(1))
		.yt(*coop_y += kCoopStackLabelToNumGap)
		.lives_num(STAT_LIVES)
		.endifstat();
	EmitCoopStackAdvancePastNum(coop_y);
}

// Big-digit lives stack (Horde/LMS): right-aligned num at yt, label below.
// Always emitted in the layout; hidden unless STAT_LIVES is non-zero.
void EmitRightLivesStack(statusbar_t &sb, int32_t yt)
{
	constexpr int32_t kLabelBelowNum = 26;

	sb.ifstat(STAT_LIVES)
		.xr(MiniscoreAlignedNumXr(1)).yt(yt).num(1, STAT_LIVES)
		.xr(kRightHudTextXr).yt(yt + kLabelBelowNum).loc_rstring("$g_lives")
		.endifstat();
}

void EmitCoopStackScoreLimit(statusbar_t &sb, int32_t *coop_y)
{
	EmitCoopStackLabeledNum(sb, coop_y, STAT_SCORELIMIT, ScoreLimitLabel(), kRightHudNumFieldWidth);
}

void EmitRoundCounter(statusbar_t &sb, round_counter_profile_t profile, const char *label, int32_t *coop_y = nullptr)
{
	if (!MM_GametypeHasFlag(GTF_ROUNDS))
		return;

	if (profile == round_counter_profile_t::CoopStack) {
		if (!coop_y)
			return;

		EmitCoopStackLabeledNum(sb, coop_y, STAT_ROUND_NUMBER, label, kRightHudNumFieldWidth);
		return;
	}

	EmitRightLabeledValue(sb, hud_vanchor_t::Top, 2, 22, STAT_ROUND_NUMBER, label);
}

void EmitHordeWaveAndRemainingStack(statusbar_t &sb)
{
	if (!GT(GT_HORDE))
		return;

	constexpr int32_t kLabelBelowNum = 26;
	int32_t y = 42;

	// Lives share yt 42 via EmitRightLivesStack (always in layout).
	if (g_horde_lives->integer > 0)
		y += kLabelBelowNum;

	sb.ifstat(STAT_HORDE_WAVE)
		.xr(MiniscoreAlignedNumXr(kMiniscoreNumFieldWidth)).yt(y += 10).num(kMiniscoreNumFieldWidth, STAT_HORDE_WAVE)
		.xr(kRightHudTextXr).yt(y += kLabelBelowNum).loc_rstring("Wave")
		.endifstat();

	sb.ifstat(STAT_HORDE_REMAINING)
		.xr(MiniscoreAlignedNumXr(kMiniscoreNumFieldWidth)).yt(y += 10).num(kMiniscoreNumFieldWidth, STAT_HORDE_REMAINING)
		.xr(kRightHudTextXr).yt(y += kLabelBelowNum).loc_rstring("Monsters")
		.endifstat();
}

// VANILLA_BASE — top-right text stack (gametype / ruleset / round progress).
constexpr int32_t kTopRightHudFirstYt = 2;
constexpr int32_t kTopRightHudLineSpacing = 10;
// Row 3: team badge / carried-flag icon (below up to two text lines).
constexpr int32_t kTopRightHudBadgeYt = kTopRightHudFirstYt + kTopRightHudLineSpacing * 2;

void EmitHordeCoopWaveHeader(statusbar_t &sb)
{
	sb.ifstat(STAT_GAMETYPE_HUD).xr(kRightHudTextXr).yt(kTopRightHudFirstYt).loc_stat_rstring(STAT_GAMETYPE_HUD).endifstat();
	sb.ifstat(STAT_RULESET_HUD).xr(kRightHudTextXr).yt(kTopRightHudFirstYt + kTopRightHudLineSpacing).loc_stat_rstring(STAT_RULESET_HUD).endifstat();
}

// VANILLA_BASE — top-right gametype, ruleset, round progress, Strike side role.
void EmitRedRoverTeamBadge(statusbar_t &sb)
{
	if (!GT(GT_RR))
		return;

	// Row 3 of the top-right stack (below gametype / ruleset text).
	sb.ifstat(STAT_CTF_FLAG_PIC).xr(kMiniscorePicXr).yt(kTopRightHudBadgeYt).pic(STAT_CTF_FLAG_PIC).endifstat();
}

void EmitTopRightMatchInfo(statusbar_t &sb)
{
	constexpr int32_t kGametypeYt = kTopRightHudFirstYt;
	constexpr int32_t kRulesetYt = kTopRightHudFirstYt + kTopRightHudLineSpacing;

	sb.ifstat(STAT_GAMETYPE_HUD).xr(kRightHudTextXr).yt(kGametypeYt).loc_stat_rstring(STAT_GAMETYPE_HUD).endifstat();
	if (GT(GT_STRIKE)) {
		sb.ifstat(STAT_ARENA_ROLE).xr(kRightHudTextXr).yt(kGametypeYt).loc_stat_rstring(STAT_ARENA_ROLE).endifstat();
	} else {
		sb.ifstat(STAT_RULESET_HUD).xr(kRightHudTextXr).yt(kRulesetYt).loc_stat_rstring(STAT_RULESET_HUD).endifstat();
	}

	EmitRedRoverTeamBadge(sb);
}

// VANILLA_BASE — centre eliminated/frozen label (CA/RR/Freeze team modes, not Strike).
void EmitCenterArenaRole(statusbar_t &sb, int32_t yt)
{
	if (!MM_GametypeHasFlag(GTF_ELIMINATION) && !GT(GT_FREEZE))
		return;

	sb.ifstat(STAT_ARENA_ROLE).xv(0).yt(yt).loc_stat_rstring(STAT_ARENA_ROLE).endifstat();
}

void EmitTeamHeader(statusbar_t &sb)
{
	if (!MM_GametypeHasFlag(GTF_TEAMS))
		return;

	if (MM_GametypeHasFlag(GTF_CTF))
		sb.ifstat(STAT_CTF_FLAG_PIC).xr(kMiniscorePicXr).yt(kTopRightHudBadgeYt).pic(STAT_CTF_FLAG_PIC).endifstat();

	sb.ifstat(STAT_TEAMPLAY_INFO).xl(0).yb(-88).stat_string(STAT_TEAMPLAY_INFO).endifstat();
}

const char *ScoreLimitLabel()
{
	if (GT(GT_STRIKE) || GT(GT_CTF))
		return "Captures";
	if (MM_GametypeHasFlag(GTF_ROUNDS))
		return "Rounds";
	return "Frags";
}

void EmitBottomTeamMiniscoreRows(statusbar_t &sb)
{
	EmitTeamMiniscoreRow(sb, hud_vanchor_t::Bottom, kBottomMiniscoreRow1Yb, STAT_MINISCORE_FIRST_PIC, STAT_MINISCORE_FIRST_SCORE, STAT_MINISCORE_FIRST_POS);
	EmitTeamMiniscoreRow(sb, hud_vanchor_t::Bottom, kBottomMiniscoreRow2Yb, STAT_MINISCORE_SECOND_PIC, STAT_MINISCORE_SECOND_SCORE, STAT_MINISCORE_SECOND_POS);
}

void EmitMiniscoreMetaRows(statusbar_t &sb)
{
	sb.ifstat(STAT_MINISCORE_FIRST_PIC)
		.ifstat(STAT_ROUND_NUMBER)
			.xr(MiniscoreAlignedNumXr(kMiniscoreNumFieldWidth))
			.yb(kBottomMiniscoreMetaRow1Yb)
			.num(kMiniscoreNumFieldWidth, STAT_ROUND_NUMBER)
		.endifstat()
		.endifstat();
}

void EmitStandardTeamMiniscore(statusbar_t &sb)
{
	EmitBottomTeamMiniscoreRows(sb);
	EmitMiniscoreMetaRows(sb);
}

} // namespace muffmode::statusbar

void MM_InitStatusbar()
{
	statusbar_t sb;
	bool minhud = (g_instagib->integer || GT(GT_INSTAGIB)) || (g_nadefest->integer || GT(GT_NADEFEST));

	sb.yb(-24);

	sb.ifstat(STAT_SHOW_STATUSBAR).ifstat(STAT_HEALTH_ICON).xv(minhud ? 100 : 0).hnum().xv(minhud ? 150 : 50).pic(STAT_HEALTH_ICON).endifstat().endifstat();
	if (!minhud) {
		sb.ifstat(STAT_SHOW_STATUSBAR).ifstat(STAT_AMMO_ICON).xv(100).anum().xv(150).pic(STAT_AMMO_ICON).endifstat().endifstat();
		sb.ifstat(STAT_SHOW_STATUSBAR).ifstat(STAT_ARMOR_ICON).xv(200).rnum().xv(250).pic(STAT_ARMOR_ICON).endifstat().endifstat();
		sb.ifstat(STAT_SHOW_STATUSBAR).ifstat(STAT_SELECTED_ICON).xv(296).pic(STAT_SELECTED_ICON).endifstat().endifstat();

		sb.yb(-50);

		sb.ifstat(STAT_SHOW_STATUSBAR).ifstat(STAT_PICKUP_ICON).xv(0).pic(STAT_PICKUP_ICON).xv(26).yb(-42).loc_stat_string(STAT_PICKUP_STRING).yb(-50).endifstat().endifstat();
		sb.ifstat(STAT_SHOW_STATUSBAR).ifstat(STAT_SELECTED_ITEM_NAME).yb(-34).xv(319).loc_stat_rstring(STAT_SELECTED_ITEM_NAME).yb(-58).endifstat().endifstat();
	}

	sb.ifstat(STAT_SHOW_STATUSBAR).ifstat(STAT_POWERUP_ICON).xv(262).num(2, STAT_POWERUP_TIME).xv(296).pic(STAT_POWERUP_ICON).endifstat().endifstat();
	sb.ifstat(STAT_SHOW_STATUSBAR).ifstat(STAT_TECH).yb(-137).xr(-26).pic(STAT_TECH).endifstat().endifstat();

	sb.yb(-50);
	if (!minhud) {
		sb.ifstat(STAT_SHOW_STATUSBAR).ifstat(STAT_HELPICON).xv(150).pic(STAT_HELPICON).endifstat().endifstat();
	}

	// Horde/LMS lives digit (yt 42) — not gated on gametype at layout build time.
	muffmode::statusbar::EmitRightLivesStack(sb, 42);

	if (InCoopStyle()) {
		int32_t y = 2;

		sb.ifstat(STAT_COOP_RESPAWN).xv(0).yt(0).loc_stat_cstring2(STAT_COOP_RESPAWN).endifstat();

		if (g_coop_enable_lives->integer && g_coop_num_lives->integer > 0 && notGT(GT_HORDE))
			muffmode::statusbar::EmitCoopStackLives(sb, &y);

		if (GT(GT_HORDE) && !deathmatch->integer)
			muffmode::statusbar::EmitHordeCoopWaveHeader(sb);

		if (GT(GT_HORDE))
			muffmode::statusbar::EmitHordeWaveAndRemainingStack(sb);
	}
	if (!deathmatch->integer) {
		sb.ifstat(STAT_POWERUP_ICON).yb(-76).endifstat();
		sb.ifstat(STAT_SELECTED_ITEM_NAME)
			.yb(-58)
			.ifstat(STAT_POWERUP_ICON)
			.yb(-84)
			.endifstat()
			.endifstat();
		sb.ifstat(STAT_KEY_A).xv(296).pic(STAT_KEY_A).endifstat();
		sb.ifstat(STAT_KEY_B).xv(272).pic(STAT_KEY_B).endifstat();
		sb.ifstat(STAT_KEY_C).xv(248).pic(STAT_KEY_C).endifstat();

		sb.ifstat(STAT_HEALTH_BARS).yt(24).health_bars().endifstat();

		sb.story();
	} else {
		sb.ifstat(STAT_SHOW_STATUSBAR);

		if (Teams())
			muffmode::statusbar::EmitTeamHeader(sb);

		muffmode::statusbar::EmitTopRightMatchInfo(sb);

		if (MM_GametypeHasFlag(GTF_ELIMINATION) && MM_GametypeHasFlag(GTF_TEAMS)) {
			if (!GT(GT_STRIKE))
				muffmode::statusbar::EmitCenterArenaRole(sb, 48);
		}

		// Vanilla num(3) right-aligns in a 50px field (2 + 3*16). Digit centre: xv + 50 - 8*l.
		// hx=160 → l=1 needs xv(118), l=2 needs xv(126). Use 118 for 3…2…1 (brief "10" sits ~8px left).
		sb.ifstat(STAT_COUNTDOWN).xv(118).yb(-256).num(3, STAT_COUNTDOWN).endifstat();
		// Match timer / warmup — bottom-centre (xv 0 yb -78, classic MuffMode placement).
		sb.ifstat(STAT_MATCH_STATE).xv(0).yb(-78).stat_string(STAT_MATCH_STATE).endifstat();
		if (GT(GT_LMS))
			sb.ifstat(STAT_CENTER_LINE).xv(0).yt(26).stat_string(STAT_CENTER_LINE).endifstat();
		sb.ifstat(STAT_FOLLOW).xv(0).yb(-68).string("FOLLOWING").xv(80).stat_string(STAT_FOLLOW).endifstat();
		sb.ifstat(STAT_SPECTATOR).xv(0).yb(-58).stat_string2(STAT_SPECTATOR).endifstat();

		sb.ifstat(STAT_CROSSHAIR_ID_VIEW).xv(122).yb(-128).stat_pname(STAT_CROSSHAIR_ID_VIEW).endifstat();
		sb.ifstat(STAT_CROSSHAIR_ID_VIEW_COLOR).xv(156).yb(-118).pic(STAT_CROSSHAIR_ID_VIEW_COLOR).endifstat();

		sb.endifstat();

		muffmode::statusbar::EmitStandardTeamMiniscore(sb);
	}

	const std::string layout = sb.sb.str();
	if (!MM_StatusbarLayoutLengthWithinBudget(layout.length()))
		gi.Com_PrintFmt("WARNING: CS_STATUSBAR length {} exceeds budget {}\n", layout.length(), MM_STATUSBAR_LAYOUT_MAX_CHARS);

	if (MM_StatusbarLayoutContainsBannedToken(layout))
		gi.Com_Error("CS_STATUSBAR layout contains banned token (ifbit)");

	if (!MM_StatusbarLayoutUsesOnlyVanillaTokens(layout))
		gi.Com_Error("CS_STATUSBAR layout contains non-vanilla token");

	muffmode::statusbar::g_last_layout = layout;

	gi.configstring(CS_STATUSBAR, layout.c_str());
}

bool MM_DumpStatusbar(std::string *out_path)
{
	constexpr const char *filename = "hud_dump.json";

	std::ofstream f(filename, std::ios::trunc);
	if (!f.is_open())
		return false;

	const int gt = clamp((int)MM_CurrentGametype(), 0, (int)GT_NUM_GAMETYPES - 1);

	f << "{\n";
	f << "  \"gametype\": \"" << gt_short_name[gt] << "\",\n";
	f << "  \"flags\": " << MM_CurrentGametypeFlags() << ",\n";

	// Emit name->index map so the previewer can label stats regardless of enum packing.
	// X-macro covers every stat the statusbar layout references.
	f << "  \"stat_names\": {\n";
#define MM_HUD_STAT(s) f << "    \"" << (int)(s) << "\": \"" #s "\",\n";
	MM_HUD_STAT(STAT_SHOW_STATUSBAR)
	MM_HUD_STAT(STAT_HEALTH_ICON) MM_HUD_STAT(STAT_HEALTH)
	MM_HUD_STAT(STAT_AMMO_ICON) MM_HUD_STAT(STAT_AMMO)
	MM_HUD_STAT(STAT_ARMOR_ICON) MM_HUD_STAT(STAT_ARMOR)
	MM_HUD_STAT(STAT_SELECTED_ICON)
	MM_HUD_STAT(STAT_PICKUP_ICON) MM_HUD_STAT(STAT_PICKUP_STRING)
	MM_HUD_STAT(STAT_SELECTED_ITEM_NAME)
	MM_HUD_STAT(STAT_POWERUP_ICON) MM_HUD_STAT(STAT_POWERUP_TIME)
	MM_HUD_STAT(STAT_TECH) MM_HUD_STAT(STAT_HELPICON)
	MM_HUD_STAT(STAT_COOP_RESPAWN) MM_HUD_STAT(STAT_LIVES)
	MM_HUD_STAT(STAT_GAMETYPE_HUD) MM_HUD_STAT(STAT_RULESET_HUD)
	MM_HUD_STAT(STAT_ROUND_NUMBER) MM_HUD_STAT(STAT_HORDE_REMAINING) MM_HUD_STAT(STAT_HORDE_WAVE)
	MM_HUD_STAT(STAT_ARENA_ROLE)
	MM_HUD_STAT(STAT_SCORELIMIT)
	MM_HUD_STAT(STAT_KEY_A) MM_HUD_STAT(STAT_KEY_B) MM_HUD_STAT(STAT_KEY_C)
	MM_HUD_STAT(STAT_HEALTH_BARS)
	MM_HUD_STAT(STAT_CTF_FLAG_PIC) MM_HUD_STAT(STAT_TEAMPLAY_INFO)
	MM_HUD_STAT(STAT_MINISCORE_FIRST_PIC) MM_HUD_STAT(STAT_MINISCORE_FIRST_SCORE)
	MM_HUD_STAT(STAT_MINISCORE_FIRST_POS)
	MM_HUD_STAT(STAT_MINISCORE_SECOND_PIC) MM_HUD_STAT(STAT_MINISCORE_SECOND_SCORE)
	MM_HUD_STAT(STAT_MINISCORE_SECOND_POS)
	MM_HUD_STAT(STAT_COUNTDOWN) MM_HUD_STAT(STAT_MATCH_STATE) MM_HUD_STAT(STAT_CENTER_LINE)
	MM_HUD_STAT(STAT_FOLLOW) MM_HUD_STAT(STAT_SPECTATOR)
	MM_HUD_STAT(STAT_CROSSHAIR_ID_VIEW) MM_HUD_STAT(STAT_CROSSHAIR_ID_VIEW_COLOR)
#undef MM_HUD_STAT
	// trailing dummy key keeps the JSON valid despite the trailing commas above
	f << "    \"-1\": \"\"\n";
	f << "  },\n";

	f << "  \"layout\": \"" << muffmode::statusbar::JsonEscape(muffmode::statusbar::g_last_layout) << "\"\n";
	f << "}\n";

	if (!f.good())
		return false;

	if (out_path)
		*out_path = filename;
	return true;
}
