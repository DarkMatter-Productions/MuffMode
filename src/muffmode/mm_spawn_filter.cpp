// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_spawn_filter.h"

namespace {

constexpr const char *k_gt_spawn_string[GT_NUM_GAMETYPES] = {
	"campaign",
	"ffa",
	"tournament",
	"team",
	"ctf",
	"ca",
	"freeze",
	"strike",
	"rr",
	"lms",
	"horde",
	"ball"
};

// Whole-token membership test for the spawn-filter fields, which may be space- or
// comma-separated (see level-design-guide). Avoids substring collisions (e.g. the GT_CA
// token "ca" matching inside a "campaign"-tagged entity).
bool MM_TokenInList(const char *list, const char *token)
{
	if (!list || !token || !*token)
		return false;

	auto is_delim = [](char c) { return c == '\0' || c == ' ' || c == ','; };

	const size_t tlen = strlen(token);
	for (const char *p = list; (p = strstr(p, token)) != nullptr; p += 1)
	{
		const bool at_start = (p == list) || p[-1] == ' ' || p[-1] == ',';
		const bool at_end = is_delim(p[tlen]);
		if (at_start && at_end)
			return true;
	}
	return false;
}

} // namespace

void MM_RemapSpawnClassname(gentity_t *ent)
{
	if (!ent || !ent->classname)
		return;

	if (!strcmp(ent->classname, "weapon_nailgun"))
		ent->classname = GetItemByIndex(IT_WEAPON_ETF_RIFLE)->classname;
	else if (!strcmp(ent->classname, "ammo_nails"))
		ent->classname = GetItemByIndex(IT_AMMO_FLECHETTES)->classname;
	else if (!strcmp(ent->classname, "weapon_heatbeam"))
		ent->classname = GetItemByIndex(IT_WEAPON_PLASMABEAM)->classname;
	else if (!strcmp(ent->classname, "item_haste"))
		ent->classname = GetItemByIndex(IT_POWERUP_HASTE)->classname;
	else if (RS(RS_Q3A) && !strcmp(ent->classname, "weapon_supershotgun"))
		ent->classname = GetItemByIndex(IT_WEAPON_SHOTGUN)->classname;
	else if (!strcmp(ent->classname, "info_player_team1"))
		ent->classname = "info_player_team_red";
	else if (!strcmp(ent->classname, "info_player_team2"))
		ent->classname = "info_player_team_blue";
	else if (!strcmp(ent->classname, "item_flag_team1"))
		ent->classname = ITEM_CTF_FLAG_RED;
	else if (!strcmp(ent->classname, "item_flag_team2"))
		ent->classname = ITEM_CTF_FLAG_BLUE;

	if (RS(RS_Q1)) {
		if (!strcmp(ent->classname, "weapon_machinegun"))
			ent->classname = GetItemByIndex(IT_WEAPON_ETF_RIFLE)->classname;
		else if (!strcmp(ent->classname, "weapon_chaingun"))
			ent->classname = GetItemByIndex(IT_WEAPON_PLASMABEAM)->classname;
		else if (!strcmp(ent->classname, "weapon_railgun"))
			ent->classname = GetItemByIndex(IT_WEAPON_HYPERBLASTER)->classname;
		else if (!strcmp(ent->classname, "ammo_slugs"))
			ent->classname = GetItemByIndex(IT_AMMO_CELLS)->classname;
		else if (!strcmp(ent->classname, "ammo_bullets"))
			ent->classname = GetItemByIndex(IT_AMMO_FLECHETTES)->classname;
		else if (!strcmp(ent->classname, "ammo_grenades"))
			ent->classname = GetItemByIndex(IT_AMMO_ROCKETS_SMALL)->classname;
	}
}

bool MM_ShouldInhibitSpawnEntity(gentity_t *ent)
{
	if (!ent)
		return false;

	if (ent->gametype) {
		if (!MM_TokenInList(ent->gametype, k_gt_spawn_string[g_gametype->integer]))
			return true;
	}
	if (ent->not_gametype) {
		if (MM_TokenInList(ent->not_gametype, k_gt_spawn_string[g_gametype->integer]))
			return true;
	}

	if (ent->notq2 && (RS(RS_Q2RE) || RS(RS_MM)))
		return true;
	if (ent->notq3a && RS(RS_Q3A))
		return true;
	if (ent->notarena && (GTF(GTF_ARENA)))
		return true;

	if (ent->ruleset) {
		if (!MM_TokenInList(ent->ruleset, rs_short_name[game.ruleset]))
			return true;
	}
	if (ent->not_ruleset) {
		if (MM_TokenInList(ent->not_ruleset, rs_short_name[game.ruleset]))
			return true;
	}

	if (g_quadhog->integer && ent->classname && !strcmp(ent->classname, "item_quad"))
		return true;

	return false;
}
