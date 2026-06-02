// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_ruleset.h"

bool MM_RulesetHealthArmorCap()
{
	return game.ruleset == RS_VANILLA_PLUS || game.ruleset == RS_QC;
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
