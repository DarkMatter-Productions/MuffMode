// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_spawn_filter.h"
#include "muffmode/mm_util.h"

#include <array>
#include <string_view>

namespace muffmode::spawn_filter {

struct ItemClassnameRemap {
	const char *from = nullptr;
	item_id_t item_id = IT_NULL;
	int default_count = 0;
};

struct LiteralClassnameRemap {
	const char *from = nullptr;
	const char *to = nullptr;
};

constexpr std::array<const char *, GT_NUM_GAMETYPES> k_gametype_spawn_tokens = {
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
	"ball",
	"instagib",
	"nadefest",
	"arena"
};

constexpr std::array<ItemClassnameRemap, 8> k_q3a_item_remaps = {{
	{ "weapon_gauntlet", IT_WEAPON_CHAINFIST },
	{ "weapon_lightning", IT_WEAPON_PLASMABEAM },
	{ "weapon_lightninggun", IT_WEAPON_PLASMABEAM },
	{ "weapon_plasmagun", IT_WEAPON_HYPERBLASTER },
	{ "weapon_plasma", IT_WEAPON_HYPERBLASTER },
	{ "weapon_nailgun", IT_WEAPON_IONRIPPER },
	{ "ammo_lightning", IT_AMMO_CELLS, 60 },
	{ "ammo_bfg", IT_AMMO_CELLS, 15 },
}};

constexpr std::array<ItemClassnameRemap, 2> k_q3a_ammo_remaps = {{
	{ "ammo_nails", IT_AMMO_CELLS, 20 },
	{ "ammo_belt", IT_AMMO_BULLETS, 100 },
}};

constexpr std::array<ItemClassnameRemap, 4> k_shared_item_remaps = {{
	{ "weapon_nailgun", IT_WEAPON_ETF_RIFLE },
	{ "ammo_nails", IT_AMMO_FLECHETTES },
	{ "weapon_heatbeam", IT_WEAPON_PLASMABEAM },
	{ "item_haste", IT_POWERUP_HASTE },
}};

constexpr std::array<LiteralClassnameRemap, 4> k_shared_literal_remaps = {{
	{ "info_player_team1", "info_player_team_red" },
	{ "info_player_team2", "info_player_team_blue" },
	{ "item_flag_team1", ITEM_CTF_FLAG_RED },
	{ "item_flag_team2", ITEM_CTF_FLAG_BLUE },
}};

constexpr std::array<ItemClassnameRemap, 6> k_q1_item_remaps = {{
	{ "weapon_machinegun", IT_WEAPON_ETF_RIFLE },
	{ "weapon_chaingun", IT_WEAPON_PLASMABEAM },
	{ "weapon_railgun", IT_WEAPON_HYPERBLASTER },
	{ "ammo_slugs", IT_AMMO_CELLS },
	{ "ammo_bullets", IT_AMMO_FLECHETTES },
	{ "ammo_grenades", IT_AMMO_ROCKETS_SMALL },
}};

// Whole-token membership test for the spawn-filter fields, which may be space- or
// comma-separated (see level-design-guide). Avoids substring collisions (e.g. the GT_CA
// token "ca" matching inside a "campaign"-tagged entity).
bool TokenInList(const char *list, const char *token)
{
	if (!list || !token || !*token)
		return false;

	const std::string_view token_view(token);
	auto is_delim = [](unsigned char c) { return c == '\0' || c == ',' || c <= ' '; };

	for (const char *p = list; *p; ) {
		while (*p && is_delim(static_cast<unsigned char>(*p)))
			p++;

		const char *start = p;
		while (*p && !is_delim(static_cast<unsigned char>(*p)))
			p++;

		const size_t len = static_cast<size_t>(p - start);
		if (len == token_view.size() && !Q_strncasecmp(start, token_view.data(), token_view.size()))
			return true;
	}

	return false;
}

const char *CurrentSpawnGametypeToken()
{
	const int gt = clamp(g_gametype ? MM_EFFECTIVE_GT : (int)GT_FFA,
		(int)GT_NONE, (int)GT_NUM_GAMETYPES - 1);
	return k_gametype_spawn_tokens[gt];
}

int CurrentSpawnGametypeFlags()
{
	const int gt = clamp(g_gametype ? MM_EFFECTIVE_GT : (int)GT_FFA,
		(int)GT_NONE, (int)GT_NUM_GAMETYPES - 1);
	return _gt[gt];
}

const char *CurrentSpawnRulesetToken()
{
	const int ruleset = clamp((int)game.ruleset, (int)RS_NONE + 1, (int)RS_NUM_RULESETS - 1);
	return rs_short_name[ruleset];
}

bool ClassnameMatches(const gentity_t *ent, const char *classname)
{
	return ent && muffmode::CStringEquals(ent->classname, classname);
}

bool IsItemClassname(const gentity_t *ent, item_id_t item_id)
{
	const gitem_t *item = GetItemByIndex(item_id);
	return item && ClassnameMatches(ent, item->classname);
}

void RemapToItem(gentity_t *ent, item_id_t item_id, int default_count)
{
	const gitem_t *item = GetItemByIndex(item_id);
	if (!ent || !item)
		return;

	ent->classname = item->classname;

	if (default_count > 0 && !ent->count)
		ent->count = default_count;
}

template <size_t Count>
bool ApplyItemRemap(gentity_t *ent, const std::array<ItemClassnameRemap, Count> &remaps)
{
	for (const auto &remap : remaps) {
		if (!ClassnameMatches(ent, remap.from))
			continue;

		RemapToItem(ent, remap.item_id, remap.default_count);
		return true;
	}

	return false;
}

template <size_t Count>
bool ApplyLiteralRemap(gentity_t *ent, const std::array<LiteralClassnameRemap, Count> &remaps)
{
	for (const auto &remap : remaps) {
		if (!ClassnameMatches(ent, remap.from))
			continue;

		ent->classname = remap.to;
		return true;
	}

	return false;
}

} // namespace muffmode::spawn_filter

void MM_RemapSpawnClassname(gentity_t *ent)
{
	if (!ent || !ent->classname)
		return;

	if (RS(RS_Q3A)) {
		if (muffmode::spawn_filter::ApplyItemRemap(ent, muffmode::spawn_filter::k_q3a_item_remaps) ||
			muffmode::spawn_filter::ApplyItemRemap(ent, muffmode::spawn_filter::k_q3a_ammo_remaps))
			return;
	}

	if (muffmode::spawn_filter::ApplyItemRemap(ent, muffmode::spawn_filter::k_shared_item_remaps))
		return;

	if (muffmode::spawn_filter::ApplyLiteralRemap(ent, muffmode::spawn_filter::k_shared_literal_remaps))
		return;

	if (RS(RS_Q1))
		muffmode::spawn_filter::ApplyItemRemap(ent, muffmode::spawn_filter::k_q1_item_remaps);
}

bool MM_ShouldInhibitSpawnEntity(gentity_t *ent)
{
	if (!ent)
		return false;

	const char *gametype_token = muffmode::spawn_filter::CurrentSpawnGametypeToken();
	if (ent->gametype) {
		if (!muffmode::spawn_filter::TokenInList(ent->gametype, gametype_token))
			return true;
	}
	if (ent->not_gametype) {
		if (muffmode::spawn_filter::TokenInList(ent->not_gametype, gametype_token))
			return true;
	}

	if (ent->notq2 && (RS(RS_Q2RE) || RS(RS_MM)))
		return true;
	if (ent->notq3a && RS(RS_Q3A))
		return true;
	if (RS(RS_Q3A) && muffmode::spawn_filter::IsItemClassname(ent, IT_WEAPON_SSHOTGUN))
		return true;
	if (ent->notarena && (muffmode::spawn_filter::CurrentSpawnGametypeFlags() & GTF_ARENA))
		return true;

	const char *ruleset_token = muffmode::spawn_filter::CurrentSpawnRulesetToken();
	if (ent->ruleset) {
		if (!muffmode::spawn_filter::TokenInList(ent->ruleset, ruleset_token))
			return true;
	}
	if (ent->not_ruleset) {
		if (muffmode::spawn_filter::TokenInList(ent->not_ruleset, ruleset_token))
			return true;
	}

	if (g_quadhog->integer && muffmode::spawn_filter::ClassnameMatches(ent, "item_quad"))
		return true;

	return false;
}
