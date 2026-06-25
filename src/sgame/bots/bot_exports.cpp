// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "bot_exports.h"

namespace {

bool Entity_IsUsable(const gentity_t *entity) {
	return entity != nullptr && entity->inuse;
}

bool Entity_IsUsableBot(const gentity_t *entity) {
	return Entity_IsUsable(entity) && (entity->svflags & SVF_BOT) != 0;
}

gclient_t *Bot_GetClient(gentity_t *bot) {
	if (!Entity_IsUsableBot(bot)) {
		return nullptr;
	}

	return bot->client;
}

bool ItemId_IsUsable(const int32_t itemID) {
	return itemID > IT_NULL && itemID < IT_TOTAL;
}

} // namespace

/*
================
Bot_SetWeapon
================
*/
void Bot_SetWeapon(gentity_t *bot, const int weaponIndex, const bool instantSwitch) {
	if (!ItemId_IsUsable(weaponIndex)) {
		return;
	}

	gclient_t *client = Bot_GetClient(bot);
	if (client == nullptr || !client->pers.inventory[weaponIndex]) {
		return;
	}

	const item_id_t weaponItemID = static_cast<item_id_t>(weaponIndex);

	const gitem_t *currentGun = client->pers.weapon;
	if (currentGun != nullptr) {
		if (currentGun->id == weaponItemID) {
			return;
		} // already have the gun in hand.
	}

	const gitem_t *pendingGun = client->newweapon;
	if (pendingGun != nullptr) {
		if (pendingGun->id == weaponItemID) {
			return;
		} // already in the process of switching to that gun, just be patient!
	}

	gitem_t *item = &itemlist[weaponIndex];
	if ((item->flags & IF_WEAPON) == 0) {
		return;
	}

	if (item->use == nullptr) {
		return;
	}

	bot->client->no_weapon_chains = true;
	item->use(bot, item);

	if (instantSwitch) {
		// FIXME: ugly, maybe store in client later
		const int temp_instant_weapon = g_instant_weapon_switch->integer || !!g_frenzy->integer;
		g_instant_weapon_switch->integer = 1;
		Change_Weapon(bot);
		g_instant_weapon_switch->integer = temp_instant_weapon;
	}
}

/*
================
Bot_TriggerEntity
================
*/
void Bot_TriggerEntity(gentity_t *bot, gentity_t *entity) {
	if (!Entity_IsUsableBot(bot) || !Entity_IsUsable(entity)) {
		return;
	}

	if (entity->use) {
		entity->use(entity, bot, bot);
	}

	trace_t unUsed{};
	if (entity->touch) {
		entity->touch(entity, bot, unUsed, true);
	}
}

/*
================
Bot_UseItem
================
*/
void Bot_UseItem(gentity_t *bot, const int32_t itemID) {
	gclient_t *client = Bot_GetClient(bot);
	if (client == nullptr || !ItemId_IsUsable(itemID)) {
		return;
	}

	const item_id_t desiredItemID = item_id_t(itemID);
	client->pers.selected_item = desiredItemID;

	ValidateSelectedItem(bot);

	if (client->pers.selected_item == IT_NULL) {
		return;
	}

	if (client->pers.selected_item != desiredItemID) {
		return;
	} // the itemID changed on us - don't use it!

	gitem_t *item = &itemlist[client->pers.selected_item];
	client->pers.selected_item = IT_NULL;

	if (item->use == nullptr) {
		return;
	}

	client->no_weapon_chains = true;
	item->use(bot, item);
}

/*
================
Bot_GetItemID
================
*/
int32_t Bot_GetItemID(const char *classname) {
	if (classname == nullptr || classname[0] == '\0') {
		return Item_Invalid;
	}

	if (Q_strcasecmp(classname, "none") == 0) {
		return Item_Null;
	}

	for (int i = 0; i < IT_TOTAL; ++i) {
		const gitem_t *item = itemlist + i;
		if (item->classname == nullptr || item->classname[0] == '\0') {
			continue;
		}

		if (Q_strcasecmp(item->classname, classname) == 0) {
			return item->id;
		}
	}

	return Item_Invalid;
}

/*
================
Entity_ForceLookAtPoint
================
*/
void Entity_ForceLookAtPoint(gentity_t *entity, gvec3_cref_t point) {
	if (!Entity_IsUsable(entity)) {
		return;
	}

	vec3_t viewOrigin = entity->s.origin;
	if (entity->client != nullptr) {
		viewOrigin += entity->client->ps.viewoffset;
	}

	const vec3_t ideal = (point - viewOrigin).normalized();

	vec3_t viewAngles = vectoangles(ideal);
	if (viewAngles.x < -180.0f) {
		viewAngles.x = anglemod(viewAngles.x + 360.0f);
	}

	if (entity->client != nullptr) {
		entity->client->ps.pmove.delta_angles = (viewAngles - entity->client->resp.cmd_angles);
		entity->client->ps.viewangles = {};
		entity->client->v_angle = {};
		entity->s.angles = {};
	}
}

/*
================
Bot_PickedUpItem

Check if the given bot has picked up the given item or not.
================
*/
bool Bot_PickedUpItem(gentity_t *bot, gentity_t *item) {
	if (!Entity_IsUsableBot(bot) || !Entity_IsUsable(item) || item->item == nullptr) {
		return false;
	}

	if (bot->s.number == 0 || bot->s.number > MAX_CLIENTS) {
		return false;
	}

	return item->item_picked_up_by[bot->s.number - 1];
}
