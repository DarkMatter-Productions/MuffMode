// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_items_rules.h"
#include "muffmode/mm_ruleset.h"

#include <algorithm>
#include <array>

namespace muffmode::items {

struct ItemInhibitRule {
	int8_t game_locals_t::*setting = nullptr;
	item_flags_t flags = IF_NONE;
};

struct ArmorRule {
	item_id_t item_id = IT_NULL;
	const gitem_armor_t *info = nullptr;
	int32_t q1_count = 0;
};

constexpr std::array<ItemInhibitRule, 6> k_item_inhibit_rules = {{
	{ &game_locals_t::item_inhibit_pu, IF_POWERUP | IF_SPHERE },
	{ &game_locals_t::item_inhibit_pa, IF_POWER_ARMOR },
	{ &game_locals_t::item_inhibit_ht, IF_HEALTH },
	{ &game_locals_t::item_inhibit_ar, IF_ARMOR },
	{ &game_locals_t::item_inhibit_am, IF_AMMO },
	{ &game_locals_t::item_inhibit_wp, IF_WEAPON },
}};

constexpr std::array<ArmorRule, 3> k_armor_rules = {{
	{ IT_ARMOR_JACKET, &jacketarmor_info, 100 },
	{ IT_ARMOR_COMBAT, &combatarmor_info, 150 },
	{ IT_ARMOR_BODY, &bodyarmor_info, 200 },
}};

const ArmorRule *ArmorRuleForItem(item_id_t armor_id)
{
	for (const auto &rule : k_armor_rules) {
		if (rule.item_id == armor_id)
			return &rule;
	}

	return nullptr;
}

int32_t Q1ArmorCount(item_id_t armor_id, const gitem_armor_t *info)
{
	if (const ArmorRule *rule = ArmorRuleForItem(armor_id))
		return rule->q1_count;

	return info ? info->max_count : 0;
}

int32_t RulesetArmorBaseCount(item_id_t armor_id, const gitem_armor_t *info)
{
	if (!info)
		return 0;

	return RS(RS_Q1) ? Q1ArmorCount(armor_id, info) : info->base_count;
}

int32_t RulesetArmorMaxCount(item_id_t armor_id, const gitem_armor_t *info)
{
	if (!info)
		return 0;

	return RS(RS_Q1) ? Q1ArmorCount(armor_id, info) : info->max_count;
}

const gitem_armor_t *ArmorInfoForItem(item_id_t armor_id)
{
	if (const ArmorRule *rule = ArmorRuleForItem(armor_id))
		return rule->info;

	return nullptr;
}

bool PickupArmorQ3(gentity_t *ent, gentity_t *other, int32_t base_count)
{
	if (!ent || !ent->item || !other || !other->client)
		return false;

	if (other->client->pers.inventory[IT_ARMOR_COMBAT] >= other->client->pers.max_health * 2)
		return false;

	if (ent->item->id == IT_ARMOR_SHARD && !ent->count)
		base_count = 5;

	other->client->pers.inventory[IT_ARMOR_COMBAT] += base_count;
	if (other->client->pers.inventory[IT_ARMOR_COMBAT] > other->client->pers.max_health * 2)
		other->client->pers.inventory[IT_ARMOR_COMBAT] = other->client->pers.max_health * 2;

	other->client->pers.inventory[IT_ARMOR_SHARD] = 0;
	other->client->pers.inventory[IT_ARMOR_JACKET] = 0;
	other->client->pers.inventory[IT_ARMOR_BODY] = 0;

	SetRespawn(ent, 25_sec);

	return true;
}

bool PickupArmorQ1(gentity_t *ent, gentity_t *other, const gitem_armor_t *newinfo, int32_t base_count)
{
	if (!ent || !ent->item || !other || !other->client || !newinfo)
		return false;

	const item_id_t old_armor_index = MM_ClientArmorIndex(other);
	if (old_armor_index != IT_NULL) {
		const gitem_armor_t *oldinfo = ArmorInfoForItem(old_armor_index);
		if (!oldinfo)
			return false;

		if (other->client->pers.inventory[old_armor_index] * oldinfo->normal_protection >= base_count * newinfo->normal_protection)
			return false;

		other->client->pers.inventory[old_armor_index] = 0;
	}

	other->client->pers.inventory[ent->item->id] = min(base_count, RulesetArmorMaxCount(ent->item->id, newinfo));
	SetRespawn(ent, 20_sec);
	return true;
}

} // namespace muffmode::items

void MM_GetItemInhibitMode(item_flags_t flags, bool &add, bool &subtract)
{
	add = false;
	subtract = false;

	for (const auto &rule : muffmode::items::k_item_inhibit_rules) {
		const int8_t mode = game.*(rule.setting);
		if (!mode || !(flags & rule.flags))
			continue;

		add = mode > 0;
		subtract = mode < 0;
		return;
	}
}

void MM_ClearItemInhibitFlags()
{
	for (const auto &rule : muffmode::items::k_item_inhibit_rules)
		game.*(rule.setting) = 0;
}

item_id_t MM_ClientArmorIndex(gentity_t *ent)
{
	if (!ent || !ent->client)
		return IT_NULL;

	if (RS(RS_Q3A)) {
		if (ent->client->pers.inventory[IT_ARMOR_JACKET] > 0 ||
			ent->client->pers.inventory[IT_ARMOR_COMBAT] > 0 ||
			ent->client->pers.inventory[IT_ARMOR_BODY] > 0)
			return IT_ARMOR_COMBAT;
	} else {
		if (ent->client->pers.inventory[IT_ARMOR_JACKET] > 0)
			return IT_ARMOR_JACKET;
		else if (ent->client->pers.inventory[IT_ARMOR_COMBAT] > 0)
			return IT_ARMOR_COMBAT;
		else if (ent->client->pers.inventory[IT_ARMOR_BODY] > 0)
			return IT_ARMOR_BODY;
	}

	return IT_NULL;
}

bool MM_PickupArmor(gentity_t *ent, gentity_t *other)
{
	if (!ent || !ent->item || !other || !other->client)
		return false;

	const gitem_armor_t *newinfo = ent->item->armor_info;
	if (!newinfo && ent->item->id != IT_ARMOR_SHARD)
		return false;

	const int32_t base_count = ent->count ? ent->count : newinfo ? muffmode::items::RulesetArmorBaseCount(ent->item->id, newinfo) : 0;

	if (RS(RS_Q3A))
		return muffmode::items::PickupArmorQ3(ent, other, base_count);

	if (RS(RS_Q1) && ent->item->id != IT_ARMOR_SHARD)
		return muffmode::items::PickupArmorQ1(ent, other, newinfo, base_count);

	const item_id_t old_armor_index = MM_ClientArmorIndex(other);

	if (ent->item->id == IT_ARMOR_SHARD) {
		if (!old_armor_index)
			other->client->pers.inventory[IT_ARMOR_JACKET] = 2;
		else
			other->client->pers.inventory[old_armor_index] += 2;
	} else if (!old_armor_index) {
		other->client->pers.inventory[ent->item->id] = base_count;
	} else {
		const gitem_armor_t *oldinfo = muffmode::items::ArmorInfoForItem(old_armor_index);
		if (!oldinfo)
			return false;

		if (!newinfo)
			return false;

		if (newinfo->normal_protection > oldinfo->normal_protection) {
			const float salvage = oldinfo->normal_protection / newinfo->normal_protection;
			const int salvagecount = (int)(salvage * other->client->pers.inventory[old_armor_index]);
			int newcount = base_count + salvagecount;
			if (newcount > muffmode::items::RulesetArmorMaxCount(ent->item->id, newinfo))
				newcount = muffmode::items::RulesetArmorMaxCount(ent->item->id, newinfo);

			other->client->pers.inventory[old_armor_index] = 0;
			other->client->pers.inventory[ent->item->id] = newcount;
		} else {
			const float salvage = newinfo->normal_protection / oldinfo->normal_protection;
			const int salvagecount = (int)(salvage * base_count);
			int newcount = other->client->pers.inventory[old_armor_index] + salvagecount;
			if (newcount > muffmode::items::RulesetArmorMaxCount(old_armor_index, oldinfo))
				newcount = muffmode::items::RulesetArmorMaxCount(old_armor_index, oldinfo);

			if (RS(RS_Q1) && other->client->pers.inventory[old_armor_index] * oldinfo->normal_protection >= newcount * newinfo->normal_protection)
				return false;

			if (other->client->pers.inventory[old_armor_index] >= newcount)
				return false;

			other->client->pers.inventory[old_armor_index] = newcount;
		}
	}

	if (MM_RulesetHealthArmorCap()) {
		item_id_t cap_idx = MM_ClientArmorIndex(other);
		if (cap_idx != IT_NULL && other->client->pers.inventory[cap_idx] > MM_RULESET_ARMOR_CAP)
			other->client->pers.inventory[cap_idx] = MM_RULESET_ARMOR_CAP;
	}

	SetRespawn(ent, 20_sec);

	return true;
}

bool MM_AllowSmartAutoSwitch(gentity_t *ent, gitem_t *item)
{
	if (!ent || !ent->client || !item)
		return false;

	if (!ent->client->pers.weapon)
		return true;

	switch (ent->client->pers.weapon->id) {
	case IT_WEAPON_CHAINFIST:
		return true;
	case IT_WEAPON_BLASTER:
		return item->id != IT_WEAPON_CHAINFIST;
	case IT_WEAPON_SHOTGUN:
		if (RS(RS_Q3A))
			return false;
		return item->id == IT_WEAPON_SSHOTGUN;
	case IT_WEAPON_MACHINEGUN:
		if (RS(RS_Q3A))
			return true;
		return item->id == IT_WEAPON_CHAINGUN;
	default:
		return false;
	}
}

gtime_t MM_PowerupInstantPickupTimeout(gentity_t *ent, bool is_dropped_from_death)
{
	if (!ent)
		return 0_sec;

	if (((RS(RS_MM) || RS(RS_Q3A)) && deathmatch->integer) || !is_dropped_from_death)
		return gtime_t::from_sec(ent->count);

	return ent->nextthink - level.time;
}

int MM_PowerupRespawnSeconds(gentity_t *ent)
{
	if (!ent || !ent->item)
		return 0;

	int count = 0;

	if (deathmatch->integer && (RS(RS_MM) || RS(RS_Q3A)) && !ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER))
		count = 120;

	if (RS(RS_Q2RE) && (ent->item->id == IT_POWERUP_PROTECTION || ent->item->id == IT_POWERUP_INVISIBILITY))
		count = 300;

	if (ent->item->quantity)
		count = ent->item->quantity;

	return count;
}

bool MM_DeferInitialPowerupSpawn(gentity_t *ent)
{
	if (!ent || !ent->item)
		return false;

	if (!(RS(RS_MM) || RS(RS_Q3A)) || !deathmatch->integer || !(ent->item->flags & IF_POWERUP))
		return false;

	int32_t r = RS(RS_MM) ? 30 : irandom(30, 60);

	ent->svflags |= SVF_NOCLIENT;
	ent->solid = SOLID_NOT;

	ent->nextthink = level.time + gtime_t::from_sec(r);
	ent->think = RespawnItem;
	return true;
}

int MM_HealthPickupCap(gentity_t *other)
{
	if (!other)
		return 0;

	return RS(RS_Q3A) ? other->max_health * 2 : (MM_RulesetHealthArmorCap() ? MM_RULESET_HEALTH_CAP : 250);
}

int MM_HealthPickupAmount(gentity_t *ent, int quantity)
{
	if (!ent || !ent->item)
		return quantity;

	if (RS(RS_Q3A) && !ent->count) {
		switch (ent->item->id) {
		case IT_HEALTH_SMALL:
			return 5;
		case IT_HEALTH_MEDIUM:
			return 25;
		case IT_HEALTH_LARGE:
			return 50;
		default:
			break;
		}
	}

	return quantity;
}

gtime_t MM_HealthPickupRespawnDelay()
{
	return RS(RS_Q3A) ? 60_sec : 30_sec;
}

bool MM_HealthPickupUsesMegaThink(gentity_t *ent, gentity_t *other)
{
	if (!ent || !ent->item)
		return false;

	return !(RS(RS_Q3A)) && (ent->item->tag & HEALTH_TIMED) && !Tech_HasRegeneration(other);
}

int MM_AmmoPickupCount(gentity_t *ent, int quantity)
{
	if (!ent || !ent->item)
		return quantity;

	if (RS(RS_Q3A) && !ent->count) {
		switch (ent->item->id) {
		case IT_AMMO_CELLS:
			return 30;
		case IT_AMMO_SLUGS:
			return 10;
		default:
			break;
		}
	}

	if (ent->item->id == IT_AMMO_SLUGS)
		return MM_AmmoSlugPickupCount(quantity);

	return quantity;
}

int MM_AmmoSlugPickupCount(int quantity)
{
	if (RS(RS_MM))
		return quantity;

	return quantity * 2;
}

int MM_PickRespawnItemTeamIndex(int current_index, int count)
{
	if (count <= 0)
		return 0;

	if (RS(RS_MM))
		return (current_index + 1) % count;

	return irandom(count);
}

void MM_OnPowerupItemRespawned(gentity_t *ent)
{
	if (!ent)
		return;

	if (!deathmatch->integer || !(RS(RS_MM) || RS(RS_Q3A)))
		return;

	if (!ent->item || !(ent->item->flags & IF_POWERUP))
		return;

	QLSound(ent, "items/poweruprespawn", "misc/alarm.wav", true);
}

bool MM_ShouldAnnouncePowerupUse()
{
	// RS_Q2RE was omitted here, so on the default ruleset Use_Powerup_BroadcastMsg
	// played no powerup activation sound at all -- e.g. the quad damage use sound
	// (items/damage.wav) was silent on Q2RE deathmatch servers.
	return RS(RS_Q2RE) || RS(RS_MM) || RS(RS_Q3A) || RS(RS_VANILLA_PLUS);
}

// [MuffMode] AutoDoc tech regen with per-ruleset/per-mod tuning (moved from sgame/entities/items.cpp).
void Tech_ApplyAutoDoc(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	gclient_t *const client = ent->client;
	if (ent->health <= 0 || client->eliminated)
		return;

	const bool mod_regen = (g_instagib->integer || GT(GT_INSTAGIB)) || (g_nadefest->integer || GT(GT_NADEFEST));
	const bool no_health = mod_regen || MM_GametypeHasFlag(GTF_ARENA) || g_no_health->integer;
	const int max_value = g_vampiric_damage->integer ? (g_vampiric_health_max->integer + 1) / 2 : (mod_regen ? 100 : 150);
	const float volume = client->silencer_shots ? 0.2f : 1.0f;

	if (mod_regen && !client->tech_regen_time) {
		client->tech_regen_time = level.time;
		return;
	}

	if (!client->pers.inventory[IT_TECH_AUTODOC] && !mod_regen)
		return;

	if (client->tech_regen_time >= level.time)
		return;

	bool played_noise = false;
	const bool mm_ruleset = RS(RS_MM);
	const gtime_t delay = mm_ruleset ? 1_sec : 500_ms;

	client->tech_regen_time = level.time;

	if (!g_vampiric_damage->integer && ent->health < max_value) {
		ent->health = std::min(ent->health + 5, max_value);
		client->tech_regen_time += delay;
		played_noise = true;
	}

	// MuffMode does not regenerate armor on the same tick as health.
	if (!no_health && (!mm_ruleset || !played_noise)) {
		const int armor_index = ArmorIndex(ent);
		if (armor_index && client->pers.inventory[armor_index] < max_value) {
			const int armor_step = g_vampiric_damage->integer ? 10 : 5;
			client->pers.inventory[armor_index] = std::min(client->pers.inventory[armor_index] + armor_step, max_value);
			client->tech_regen_time += delay;
			played_noise = true;
		}
	}

	if (played_noise && client->tech_sound_time < level.time) {
		client->tech_sound_time = level.time + 1_sec;
		gi.sound(ent, CHAN_AUX, gi.soundindex("ctf/tech4.wav"), volume, ATTN_NORM, 0);
	}
}

bool Tech_HasRegeneration(gentity_t *ent)
{
	if (!ent || !ent->client)
		return false;

	return ent->client->pers.inventory[IT_TECH_AUTODOC] ||
		g_instagib->integer || GT(GT_INSTAGIB) ||
		g_nadefest->integer || GT(GT_NADEFEST);
}
