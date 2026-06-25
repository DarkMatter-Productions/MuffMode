// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_ruleset.h"

bool MM_RulesetHealthArmorCap()
{
	return game.ruleset == RS_VANILLA_PLUS || game.ruleset == RS_QC;
}

// [MuffMode] q2re's DualFire powerup is fire-rate only; the +movement-speed buff is a
// MuffMode/arcade addition. Keep it off for the q2re-faithful rulesets.
bool MM_RulesetHasteBoostsMovement()
{
	return !(game.ruleset == RS_Q2RE || game.ruleset == RS_VANILLA_PLUS);
}

void MM_ClampClientPersistHealthArmor(gclient_t *client)
{
	if (!client || !MM_RulesetHealthArmorCap())
		return;

	client->pers.max_health = min(client->pers.max_health, (int)MM_RULESET_HEALTH_CAP);
	client->pers.health = min(client->pers.health, (int)MM_RULESET_HEALTH_CAP);

	for (item_id_t aid : { IT_ARMOR_JACKET, IT_ARMOR_COMBAT, IT_ARMOR_BODY }) {
		if (client->pers.inventory[aid] > MM_RULESET_ARMOR_CAP)
			client->pers.inventory[aid] = MM_RULESET_ARMOR_CAP;
	}
}

void MM_RulesetQ3AHealthArmorDecay(gentity_t *ent)
{
	if (!ent || !ent->client || game.ruleset != RS_Q3A)
		return;

	if (ent->health > ent->client->pers.max_health)
		ent->health--;

	if (ent->client->pers.inventory[IT_ARMOR_COMBAT] > ent->client->pers.max_health)
		ent->client->pers.inventory[IT_ARMOR_COMBAT]--;
}

void MM_ClampEntityHealthArmor(gentity_t *ent)
{
	if (!ent || !ent->client || !MM_RulesetHealthArmorCap())
		return;

	if (ent->max_health > MM_RULESET_HEALTH_CAP)
		ent->max_health = MM_RULESET_HEALTH_CAP;
	if (ent->client->pers.max_health > MM_RULESET_HEALTH_CAP)
		ent->client->pers.max_health = MM_RULESET_HEALTH_CAP;
	if (ent->health > MM_RULESET_HEALTH_CAP)
		ent->health = MM_RULESET_HEALTH_CAP;

	for (item_id_t aid : { IT_ARMOR_JACKET, IT_ARMOR_COMBAT, IT_ARMOR_BODY }) {
		if (ent->client->pers.inventory[aid] > MM_RULESET_ARMOR_CAP)
			ent->client->pers.inventory[aid] = MM_RULESET_ARMOR_CAP;
	}
}

// [MuffMode] Ruleset-specific falling damage tuning (moved from sgame/client/lifecycle.cpp).
// Q3A uses higher medium/far thresholds and fixed 5/10 damage values.
int MM_RulesetFallDamageMedMin()
{
	return RS(RS_Q3A) ? 40 : 30;
}

int MM_RulesetFallDamageFarMin()
{
	return RS(RS_Q3A) ? 61 : 55;
}

int MM_RulesetFallDamage(bool far_fall, float delta)
{
	if (RS(RS_Q3A))
		return far_fall ? 10 : 5;

	int damage = (int)((delta - 30) / 2);
	if (damage < 1)
		damage = 1;
	return damage;
}
