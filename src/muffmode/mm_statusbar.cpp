// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "g_statusbar.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_statusbar.h"

void MM_InitStatusbar()
{
	statusbar_t sb;
	bool minhud = (g_instagib->integer || GT(GT_INSTAGIB)) || (g_nadefest->integer || GT(GT_NADEFEST));

	sb.yb(-24);

	sb.ifstat(STAT_SHOW_STATUSBAR).xv(minhud ? 100 : 0).hnum().xv(minhud ? 150 : 50).pic(STAT_HEALTH_ICON).endifstat();
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

	if (InCoopStyle()) {
		int32_t y = 2;
		const int32_t text_adj = 26;

		sb.ifstat(STAT_COOP_RESPAWN).xv(0).yt(0).loc_stat_cstring2(STAT_COOP_RESPAWN).endifstat();

		if (g_coop_enable_lives->integer && g_coop_num_lives->integer > 0 && notGT(GT_HORDE))
			sb.ifstat(STAT_LIVES).xr(-16).yt(y = 2).lives_num(STAT_LIVES).xr(0).yt(y += text_adj).loc_rstring("$g_lives").endifstat();

		if (GT(GT_HORDE) && g_horde_lives->integer > 0)
			sb.ifstat(STAT_LIVES).xr(-16).yt(y = 2).lives_num(STAT_LIVES).xr(0).yt(y += text_adj).loc_rstring("$g_lives").endifstat();

		if (GT(GT_HORDE)) {
			int num, chars;

			num = level.round_number;
			chars = num > 99 ? 3 : num > 9 ? 2 : 1;
			sb.ifstat(STAT_ROUND_NUMBER).xr(-32 - (16 * chars)).yt(y += 10).num(3, STAT_ROUND_NUMBER).xr(0).yt(y += text_adj).loc_rstring("Wave").endifstat();

			num = level.total_monsters - level.killed_monsters;
			chars = num > 99 ? 3 : num > 9 ? 2 : 1;
			sb.ifstat(STAT_MONSTER_COUNT).xr(-32 - (16 * chars)).yt(y += 10).num(3, STAT_MONSTER_COUNT).xr(0).yt(y += text_adj).loc_rstring("Monsters").endifstat();
		}
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
		if (Teams()) {
			if (MM_GametypeHasFlag(GTF_CTF))
				sb.ifstat(STAT_CTF_FLAG_PIC).xr(-24).yt(26).pic(STAT_CTF_FLAG_PIC).endifstat();

			// Red Rover: teams flip on death, so show the current team's logo prominently
			// at top-centre. Reuses the (CTF-only) flag pic slot, fed by G_SetStats.
			if (GT(GT_RR))
				sb.ifstat(STAT_CTF_FLAG_PIC).xv(144).yt(2).pic(STAT_CTF_FLAG_PIC).endifstat();

			sb.ifstat(STAT_TEAMPLAY_INFO).xl(0).yb(-88).stat_string(STAT_TEAMPLAY_INFO).endifstat();
		}

		// CaptureStrike: show the current round number (top-right, like Horde's wave counter)
		// plus this player's attack/defend role, unpacked from STAT_MONSTER_COUNT via ifbit.
		if (GT(GT_STRIKE)) {
			int num = level.round_number;
			int chars = num > 99 ? 3 : num > 9 ? 2 : 1;
			sb.ifstat(STAT_ROUND_NUMBER).xr(-32 - (16 * chars)).yt(2).num(3, STAT_ROUND_NUMBER).xr(0).yt(2 + 26).loc_rstring("Round").endifstat();
			sb.ifbit(STAT_MONSTER_COUNT, STRIKE_HUD_ATTACKING).xr(0).yt(2 + 48).loc_rstring("ATTACK").endifstat();
			sb.ifbit(STAT_MONSTER_COUNT, STRIKE_HUD_DEFENDING).xr(0).yt(2 + 48).loc_rstring("DEFEND").endifstat();
		}

		sb.ifstat(STAT_COUNTDOWN).yb(-256).num(3, STAT_COUNTDOWN).endifstat();
		sb.ifstat(STAT_MATCH_STATE).xv(0).yb(-78).stat_string(STAT_MATCH_STATE).endifstat();
		sb.ifstat(STAT_CHASE).xv(0).yb(-68).string("FOLLOWING").xv(80).stat_string(STAT_CHASE).endifstat();
		sb.ifstat(STAT_SPECTATOR).xv(0).yb(-58).string2("SPECTATOR MODE").endifstat();

		sb.ifstat(STAT_MINISCORE_FIRST_PIC).xr(-26).yb(-110).pic(STAT_MINISCORE_FIRST_PIC).xr(-78).num(3, STAT_MINISCORE_FIRST_SCORE).ifstat(STAT_MINISCORE_FIRST_VAL).xr(-24).yb(-94).stat_string(STAT_MINISCORE_FIRST_VAL).endifstat().endifstat();
		sb.ifstat(STAT_MINISCORE_FIRST_POS).xr(-28).yb(-112).pic(STAT_MINISCORE_FIRST_POS).endifstat();
		sb.ifstat(STAT_MINISCORE_SECOND_PIC).xr(-26).yb(-83).pic(STAT_MINISCORE_SECOND_PIC).xr(-78).num(3, STAT_MINISCORE_SECOND_SCORE).ifstat(STAT_MINISCORE_SECOND_VAL).xr(-24).yb(-68).stat_string(STAT_MINISCORE_SECOND_VAL).endifstat().endifstat();
		sb.ifstat(STAT_MINISCORE_SECOND_POS).xr(-28).yb(-85).pic(STAT_MINISCORE_SECOND_POS).endifstat();
		sb.ifstat(STAT_MINISCORE_FIRST_PIC).ifstat(STAT_SCORELIMIT).xr(-40).yb(-57).num(2, STAT_SCORELIMIT).endifstat().endifstat();

		sb.ifstat(STAT_CROSSHAIR_ID_VIEW).xv(122).yb(-128).stat_pname(STAT_CROSSHAIR_ID_VIEW).endifstat();
		sb.ifstat(STAT_CROSSHAIR_ID_VIEW_COLOR).xv(156).yb(-118).pic(STAT_CROSSHAIR_ID_VIEW_COLOR).endifstat();
	}

	gi.configstring(CS_STATUSBAR, sb.sb.str().c_str());
}
