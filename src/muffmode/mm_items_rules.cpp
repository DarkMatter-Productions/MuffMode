// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_items_rules.h"
#include "muffmode/mm_ruleset.h"

void MM_GetItemInhibitMode(item_flags_t flags, bool &add, bool &subtract)
{
	add = false;
	subtract = false;

	if (game.item_inhibit_pu && flags & (IF_POWERUP | IF_SPHERE)) {
		add = game.item_inhibit_pu > 0;
		subtract = game.item_inhibit_pu < 0;
	} else if (game.item_inhibit_pa && flags & IF_POWER_ARMOR) {
		add = game.item_inhibit_pa > 0;
		subtract = game.item_inhibit_pa < 0;
	} else if (game.item_inhibit_ht && flags & IF_HEALTH) {
		add = game.item_inhibit_ht > 0;
		subtract = game.item_inhibit_ht < 0;
	} else if (game.item_inhibit_ar && flags & IF_ARMOR) {
		add = game.item_inhibit_ar > 0;
		subtract = game.item_inhibit_ar < 0;
	} else if (game.item_inhibit_am && flags & IF_AMMO) {
		add = game.item_inhibit_am > 0;
		subtract = game.item_inhibit_am < 0;
	} else if (game.item_inhibit_wp && flags & IF_WEAPON) {
		add = game.item_inhibit_wp > 0;
		subtract = game.item_inhibit_wp < 0;
	}
}

void MM_ClearItemInhibitFlags()
{
	game.item_inhibit_pu = 0;
	game.item_inhibit_pa = 0;
	game.item_inhibit_ht = 0;
	game.item_inhibit_ar = 0;
	game.item_inhibit_am = 0;
	game.item_inhibit_wp = 0;
}

item_id_t MM_ClientArmorIndex(gentity_t *ent)
{
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

static bool MM_PickupArmorQ3(gentity_t *ent, gentity_t *other, int32_t base_count)
{
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

bool MM_PickupArmor(gentity_t *ent, gentity_t *other)
{
	item_id_t			 old_armor_index;
	const gitem_armor_t *oldinfo;
	const gitem_armor_t *newinfo;
	int					 newcount;
	float				 salvage;
	int					 salvagecount;

	newinfo = ent->item->armor_info;

	int32_t base_count = ent->count ? ent->count : newinfo ? (RS(RS_Q1) ? newinfo->max_count : newinfo->base_count) : 0;

	if (RS(RS_Q3A))
		return MM_PickupArmorQ3(ent, other, base_count);

	old_armor_index = MM_ClientArmorIndex(other);

	if (ent->item->id == IT_ARMOR_SHARD) {
		if (!old_armor_index)
			other->client->pers.inventory[IT_ARMOR_JACKET] = 2;
		else
			other->client->pers.inventory[old_armor_index] += 2;
	} else if (!old_armor_index) {
		other->client->pers.inventory[ent->item->id] = base_count;
	} else {
		if (old_armor_index == IT_ARMOR_JACKET)
			oldinfo = &jacketarmor_info;
		else if (old_armor_index == IT_ARMOR_COMBAT)
			oldinfo = &combatarmor_info;
		else
			oldinfo = &bodyarmor_info;

		if (newinfo->normal_protection > oldinfo->normal_protection) {
			salvage = oldinfo->normal_protection / newinfo->normal_protection;
			salvagecount = (int)(salvage * other->client->pers.inventory[old_armor_index]);
			newcount = base_count + salvagecount;
			if (newcount > newinfo->max_count)
				newcount = newinfo->max_count;

			other->client->pers.inventory[old_armor_index] = 0;
			other->client->pers.inventory[ent->item->id] = newcount;
		} else {
			salvage = newinfo->normal_protection / oldinfo->normal_protection;
			salvagecount = (int)(salvage * base_count);
			newcount = other->client->pers.inventory[old_armor_index] + salvagecount;
			if (newcount > oldinfo->max_count)
				newcount = oldinfo->max_count;

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
	if (!ent->client->pers.weapon)
		return true;

	switch (ent->client->pers.weapon->id) {
	case IT_WEAPON_CHAINFIST:
		return true;
	case IT_WEAPON_BLASTER:
		return item->id != IT_WEAPON_CHAINFIST;
	case IT_WEAPON_SHOTGUN:
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
	if (((RS(RS_MM) || RS(RS_Q3A)) && deathmatch->integer) || !is_dropped_from_death)
		return gtime_t::from_sec(ent->count);

	return ent->nextthink - level.time;
}

int MM_PowerupRespawnSeconds(gentity_t *ent)
{
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
	return RS(RS_Q3A) ? other->max_health * 2 : (MM_RulesetHealthArmorCap() ? MM_RULESET_HEALTH_CAP : 250);
}

int MM_HealthPickupAmount(gentity_t *ent, int quantity)
{
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
	return !(RS(RS_Q3A)) && (ent->item->tag & HEALTH_TIMED) && !Tech_HasRegeneration(other);
}

int MM_AmmoSlugPickupCount(int quantity)
{
	if (RS(RS_MM))
		return quantity;

	return quantity * 2;
}
