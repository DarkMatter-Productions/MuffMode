// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Player-affecting target entities: inventory, teleport, score and cvar helpers.
#include <array>
#include <cerrno>
#include <limits>

#include "g_local.h"

namespace {

struct target_ammo_limit_t {
	item_id_t item_id;
	ammo_t ammo_id;
	int16_t max_amount;
};

static constexpr std::array<target_ammo_limit_t, 10> TARGET_POWERUP_AMMO_LIMITS = { {
	{ IT_AMMO_SHELLS, AMMO_SHELLS, 50 },
	{ IT_AMMO_BULLETS, AMMO_BULLETS, 300 },
	{ IT_AMMO_GRENADES, AMMO_GRENADES, 50 },
	{ IT_AMMO_ROCKETS, AMMO_ROCKETS, 50 },
	{ IT_AMMO_CELLS, AMMO_CELLS, 200 },
	{ IT_AMMO_SLUGS, AMMO_SLUGS, 25 },
	{ IT_AMMO_TRAP, AMMO_TRAP, 5 },
	{ IT_AMMO_FLECHETTES, AMMO_FLECHETTES, 200 },
	{ IT_AMMO_ROUNDS, AMMO_DISRUPTOR, 12 },
	{ IT_AMMO_TESLA, AMMO_TESLA, 5 },
} };

void ResetTargetPowerupAmmoLimits(gclient_t *client) {
	client->pers.max_ammo.fill(50);

	for (const target_ammo_limit_t &limit : TARGET_POWERUP_AMMO_LIMITS) {
		client->pers.max_ammo[limit.ammo_id] = limit.max_amount;
	}
}

void ClampTargetPowerupAmmoInventory(gclient_t *client) {
	for (const target_ammo_limit_t &limit : TARGET_POWERUP_AMMO_LIMITS) {
		int32_t &inventory = client->pers.inventory[limit.item_id];
		const int16_t max_amount = client->pers.max_ammo[limit.ammo_id];

		if (inventory > max_amount) {
			inventory = max_amount;
		}
	}
}

bool TargetPlayerConsumesWholeToken(const char *token, const char *end) {
	return token != nullptr && token[0] != '\0' && end != nullptr && end != token && *end == '\0';
}

bool TargetPlayerParseInt32(const char *token, int32_t &out) {
	if (token == nullptr || token[0] == '\0') {
		return false;
	}

	char *end = nullptr;
	errno = 0;
	const long value = strtol(token, &end, 10);
	if (!TargetPlayerConsumesWholeToken(token, end) || errno == ERANGE ||
		value < static_cast<long>(std::numeric_limits<int32_t>::min()) ||
		value > static_cast<long>(std::numeric_limits<int32_t>::max())) {
		return false;
	}

	out = static_cast<int32_t>(value);
	return true;
}

/*QUAKED target_remove_powerups (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Takes away all the activator's powerups, techs, held items, keys and CTF flags.
*/
static USE(target_remove_powerups_use) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	if (!activator->client)
		return;

	activator->client->pu_time_quad = 0_sec;
	activator->client->pu_time_haste = 0_sec;
	activator->client->pu_time_double = 0_sec;
	activator->client->pu_time_protection = 0_sec;
	activator->client->pu_time_invisibility = 0_sec;
	activator->client->pu_time_regeneration = 0_sec;
	activator->client->pu_time_rebreather = 0_sec;
	activator->client->pu_time_enviro = 0_sec;

	ResetTargetPowerupAmmoLimits(activator->client);
	ClampTargetPowerupAmmoInventory(activator->client);

	for (size_t i = 0; i < IT_TOTAL; i++) {
		if (!activator->client->pers.inventory[i])
			continue;
		
		if (itemlist[i].flags & IF_KEY | IF_POWERUP | IF_TIMED | IF_SPHERE | IF_TECH) {
			if (itemlist[i].id == IT_POWERUP_QUAD && g_quadhog->integer) {
				// spawn quad
				
			}
			activator->client->pers.inventory[i] = 0;
		} else if (itemlist[i].flags & IF_POWER_ARMOR) {
			activator->client->pers.inventory[i] = 0;
			G_CheckPowerArmor(activator);
		} else if (itemlist[i].flags & IF_TECH) {
			activator->client->pers.inventory[i] = 0;
			Tech_DeadDrop(activator);
		} else if (itemlist[i].id == IT_FLAG_BLUE) {
			activator->client->pers.inventory[i] = 0;
			CTF_ResetTeamFlag(TEAM_BLUE);
		} else if (itemlist[i].id == IT_FLAG_RED) {
			activator->client->pers.inventory[i] = 0;
			CTF_ResetTeamFlag(TEAM_RED);
		}
	}
}

/*QUAKED target_remove_weapons (1 0 0) (-8 -8 -8) (8 8 8) BLASTER x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Takes away all the activator's weapons and ammo (except blaster).
BLASTER : also remove blaster
*/
static USE(target_remove_weapons_use) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	if (!activator->client)
		return;
	
	for (size_t i = 0; i < IT_TOTAL; i++) {
		if (!activator->client->pers.inventory[i])
			continue;

		if (itemlist[i].flags & IF_WEAPON | IF_AMMO && itemlist[i].id != IT_WEAPON_BLASTER)
			activator->client->pers.inventory[i] = 0;
	}

	NoAmmoWeaponChange(ent, false);

	activator->client->pers.weapon = activator->client->newweapon;
	if (activator->client->newweapon)
		activator->client->pers.selected_item = activator->client->newweapon->id;
	activator->client->newweapon = nullptr;
	activator->client->pers.lastweapon = activator->client->pers.weapon;
}

/*QUAKED target_give (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Gives the activator the targetted item.
*/
static USE(target_give_use) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	if (!activator->client)
		return;

	ent->item->pickup(ent, other);
}

/*QUAKED target_delay (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Sets a delay before firing its targets.
"wait" seconds to pause before firing targets.
"random" delay variance, total delay = delay +/- random seconds
*/
static THINK(target_delay_think) (gentity_t *ent) -> void {
	G_UseTargets(ent, ent->activator);
}

static USE(target_delay_use) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	ent->nextthink = gtime_t::from_ms(level.time.milliseconds() + (ent->wait + ent->random * crandom()) * 1000);
	ent->think = target_delay_think;
	ent->activator = activator;
}

/*QUAKED target_print (1 0 0) (-8 -8 -8) (8 8 8) REDTEAM BLUETEAM PRIVATE x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Sends a center-printed message to clients.
"message"	text to print
If "private", only the activator gets the message. If no checks, all clients get the message.
*/
static USE(target_print_use) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	if (activator && activator->client && ent->spawnflags.has(4_spawnflag)) {
		gi.LocClient_Print(activator, PRINT_CENTER, "{}", ent->message);
		return;
	}

	if (ent->spawnflags.has(3_spawnflag)) {
		if (ent->spawnflags.has(1_spawnflag))
			BroadcastTeamMessage(TEAM_RED, PRINT_CENTER, G_Fmt("{}", ent->message).data());
		if (ent->spawnflags.has(2_spawnflag))
			BroadcastTeamMessage(TEAM_BLUE, PRINT_CENTER, G_Fmt("{}", ent->message).data());
		return;
	}

	gi.LocBroadcast_Print(PRINT_CENTER, "{}", ent->message);
}

/*QUAKED target_teleporter (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
The activator will be teleported to the targetted destination.
If no target set, it will find a player spawn point instead.
*/
static USE(target_teleporter_use) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	if (!activator || !activator->client)
		return;

	// no target point to teleport to, teleport to a spawn point
	if (!ent->target_ent) {
		TeleportPlayerToRandomSpawnPoint(activator, true);
		return;
	}

	TeleportPlayer(activator, ent->target_ent->s.origin, ent->target_ent->s.angles);
}

/*QUAKED target_kill (.5 .5 .5) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Kills the activator.
*/
static USE(target_kill_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	if (!activator)
		return;
	T_Damage(activator, self, self, vec3_origin, self->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MOD_UNKNOWN);

}

/*QUAKED target_cvar (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
When targetted sets a cvar to a value.
"cvar" : name of cvar to set
"cvarValue" : value to set cvar to
*/
static USE(target_cvar_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	if (!activator || !activator->client)
		return;

	gi.cvar_set(st.cvar, st.cvarvalue);
}

/*QUAKED target_setskill (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Set skill level.
"message" : skill level to set to (0-3)

Skill levels are:
0 = Easy
1 = Medium
2 = Hard
3 = Nightmare/Hard+
*/
static USE(target_setskill_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	if (!activator || !activator->client)
		return;
	
	gi.cvar_set("skill", G_Fmt("{}", self->count).data());
}

/*QUAKED target_score (1 0 0) (-8 -8 -8) (8 8 8) TEAM x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
"count" number of points to adjust by, default 1

The activator is given this many points.

TEAM : also adjust team score
*/
static USE(target_score_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	if (!activator || !activator->client)
		return;

	G_AdjustPlayerScore(activator->client, self->count, GT(GT_TDM) || self->spawnflags.has(1_spawnflag), self->count);
}

} // namespace

void SP_target_remove_powerups(gentity_t *ent) {
	ent->use = target_remove_powerups_use;
}

void SP_target_remove_weapons(gentity_t *ent) {
	ent->use = target_remove_weapons_use;
}

void SP_target_give(gentity_t *ent) {
	gentity_t *target_ent = G_PickTarget(ent->target);
	if (!target_ent || !target_ent->classname[0]) {
		gi.Com_PrintFmt("{}: Invalid target entity, removing.\n", *ent);
		G_FreeEntity(ent);
		return;
	}

	gitem_t *it = FindItemByClassname(target_ent->classname);
	if (!it || !it->pickup) {
		gi.Com_PrintFmt("{}: Targetted entity is not an item, removing.\n", *ent);
		G_FreeEntity(ent);
		return;
	}
	
	ent->item = it;
	ent->use = target_give_use;
	ent->svflags = SVF_NOCLIENT;
}

void SP_target_delay(gentity_t *ent) {
	if (!ent->wait)
		ent->wait = 1;
	ent->use = target_delay_use;
	ent->svflags = SVF_NOCLIENT;
}

void SP_target_print(gentity_t *ent) {
	if (!ent->message[0]) {
		gi.Com_PrintFmt("{}: No message, removing.\n", *ent);
		G_FreeEntity(ent);
		return;
	}
	ent->use = target_print_use;
	ent->svflags = SVF_NOCLIENT;
}

void SP_target_teleporter(gentity_t *ent) {
	if (ent->target && ent->target[0]) {
		ent->target_ent = G_PickTarget(ent->target);
		if (!ent->target_ent) {
			gi.Com_PrintFmt("{}: Couldn't find teleporter destination, removing.\n", *ent);
			G_FreeEntity(ent);
			return;
		}
	}

	ent->use = target_teleporter_use;
}

void SP_target_kill(gentity_t *self) {
	self->use = target_kill_use;
	self->svflags = SVF_NOCLIENT;
}

void SP_target_cvar(gentity_t *ent) {
	if (!st.cvar[0] || !st.cvarvalue[0]) {
		G_FreeEntity(ent);
		return;
	}

	ent->use = target_cvar_use;
}

void SP_target_setskill(gentity_t *ent) {
	int32_t skill_level = 0;

	if (!ent->message || !ent->message[0]) {
		gi.Com_PrintFmt("{}: No message key set, removing.\n", *ent);
		G_FreeEntity(ent);
		return;
	}

	if (!TargetPlayerParseInt32(ent->message, skill_level)) {
		gi.Com_PrintFmt("{}: Invalid skill '{}', removing.\n", *ent, ent->message);
		G_FreeEntity(ent);
		return;
	}

	ent->count = clamp(skill_level, 0, 4);
	ent->use = target_setskill_use;
}

void SP_target_score(gentity_t *ent) {
	if (!ent->count)
		ent->count = 1;

	ent->use = target_score_use;
}
