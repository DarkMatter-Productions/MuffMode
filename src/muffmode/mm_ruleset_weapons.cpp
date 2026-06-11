// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_ruleset_weapons.h"

struct ruleset_pickup_tuning_t {
	int slug_pickup_quantity;
};

static constexpr ruleset_pickup_tuning_t k_ruleset_pickup_tuning[RS_NUM_RULESETS] = {
	/* RS_NONE */			{ 10 },
	/* RS_Q2RE */			{ 10 },
	/* RS_MM */				{ 5 },
	/* RS_Q3A */			{ 10 },
	/* RS_VANILLA_PLUS */	{ 10 },
	/* RS_Q1 */				{ 10 },
	/* RS_QC */				{ 10 },
};

int MM_Ruleset_SlugPickupQuantity()
{
	int ruleset = clamp((int)game.ruleset, 0, (int)RS_NUM_RULESETS - 1);
	return k_ruleset_pickup_tuning[ruleset].slug_pickup_quantity;
}

int MM_Ruleset_MachinegunDamage()
{
	if (RS(RS_Q3A))
		return GT(GT_TDM) ? 5 : 7;
	if (RS(RS_QC))
		return 6;
	if (RS(RS_VANILLA_PLUS) && deathmatch->integer)
		return 7;
	return 8;
}

void MM_Ruleset_MachinegunSpread(int &hspread, int &vspread)
{
	if (RS(RS_Q3A)) {
		hspread = 200;
		vspread = 200;
	} else {
		hspread = DEFAULT_BULLET_HSPREAD;
		vspread = DEFAULT_BULLET_VSPREAD;
	}
}

int MM_Ruleset_ChaingunDamage(gentity_t *ent)
{
	if (RS(RS_QC) && deathmatch->integer)
		return 4 + (ent->client->chaingun_shots++ & 1);
	if (RS(RS_VANILLA_PLUS) && deathmatch->integer)
		return 4 + (ent->client->chaingun_shots++ & 1);
	if (RS(RS_QC))
		return 6;
	return deathmatch->integer ? 6 : 8;
}

void MM_Ruleset_RocketLauncherDefaults(int &damage, int &splash_damage, float &splash_radius, int &speed)
{
	switch (game.ruleset) {
	case RS_MM:
		damage = 100;
		splash_radius = 120;
		splash_damage = damage;
		speed = 650;
		break;
	case RS_QC:
		damage = 100;
		splash_radius = 120;
		splash_damage = damage;
		speed = 750;
		break;
	case RS_VANILLA_PLUS:
		damage = 100;
		splash_radius = 120;
		splash_damage = damage;
		speed = (deathmatch->integer ? 750 : 650);
		break;
	case RS_Q3A:
		damage = 100;
		splash_radius = 120;
		splash_damage = 120;
		speed = 900;
		break;
	case RS_Q1:
		damage = irandom(100, 120);
		splash_radius = 120;
		splash_damage = 120;
		speed = 1000;
		break;
	default:
		damage = irandom(100, 120);
		splash_radius = 120;
		splash_damage = 120;
		speed = 650;
		break;
	}
}

void MM_Ruleset_RocketLauncherDefaultsForCustomDamage(int damage, int &splash_damage, float &splash_radius, int &speed)
{
	switch (game.ruleset) {
	case RS_MM:
		splash_radius = 120;
		splash_damage = damage;
		speed = 650;
		break;
	case RS_QC:
		splash_radius = 120;
		splash_damage = damage;
		speed = 750;
		break;
	case RS_VANILLA_PLUS:
		splash_radius = 120;
		splash_damage = damage;
		speed = (deathmatch->integer ? 750 : 650);
		break;
	case RS_Q3A:
		splash_radius = 120;
		splash_damage = 120;
		speed = 900;
		break;
	case RS_Q1:
		splash_radius = 120;
		splash_damage = 120;
		speed = 1000;
		break;
	default:
		splash_radius = 120;
		splash_damage = 120;
		speed = 650;
		break;
	}
}

int MM_Ruleset_HyperBlasterSpeed(bool hyper)
{
	if (RS(RS_Q3A))
		return hyper ? 2000 : 2500;
	if (RS(RS_VANILLA_PLUS))
		return hyper ? (deathmatch->integer ? 1100 : 1000) : (deathmatch->integer ? 1700 : 1500);
	return hyper ? 1000 : 1500;
}

int MM_Ruleset_HyperBlasterDamage()
{
	if (RS(RS_Q3A))
		return deathmatch->integer ? 20 : 25;
	if (RS(RS_QC))
		return 12;
	return deathmatch->integer ? 15 : 20;
}

int MM_Ruleset_RailgunDamage()
{
	if (RS(RS_QC) || RS(RS_VANILLA_PLUS))
		return 90;
	return deathmatch->integer ? 100 : 150;
}

int MM_Ruleset_RailgunKick(int damage)
{
	if (RS(RS_MM))
		return damage * 2;
	return deathmatch->integer ? 200 : 225;
}

void MM_Ruleset_BFGDefaults(int &damage, float &splash_radius, int &speed)
{
	if (RS(RS_Q3A)) {
		damage = 100;
		splash_radius = 120;
		speed = 1000;
	} else {
		damage = deathmatch->integer ? 200 : 500;
		splash_radius = 1000;
		speed = 400;
	}
}

void MM_Ruleset_PlasmaBeamDefaults(int &damage, int &kick)
{
	switch (game.ruleset) {
	case RS_MM:
	case RS_VANILLA_PLUS:
	case RS_QC:
		damage = deathmatch->integer ? 10 : 15;
		kick = damage;
		break;
	case RS_Q3A:
		damage = deathmatch->integer ? 8 : 15;
		kick = damage;
		break;
	default:
		damage = 15;
		kick = deathmatch->integer ? 75 : 30;
		break;
	}
}

int MM_Ruleset_IonRipperDamage()
{
	if (deathmatch->integer)
		return RS(RS_MM) ? 20 : 30;
	return 50;
}

int MM_Ruleset_IonRipperSpeed()
{
	return RS(RS_MM) ? 800 : 500;
}

void MM_Ruleset_GrenadeLauncherDefaults(int &damage, float &splash_radius, int &speed)
{
	if (RS(RS_Q3A)) {
		damage = 100;
		splash_radius = 150;
		speed = 700;
	} else {
		damage = 120;
		splash_radius = (float)(damage + 40);
		speed = 600;
	}
}

void MM_Ruleset_ChaingunSpreadDefaults(int &hspread, int &vspread, float &spread_offset)
{
	hspread = DEFAULT_BULLET_HSPREAD;
	vspread = DEFAULT_BULLET_VSPREAD;
	spread_offset = 4.0f;
}

int MM_Ruleset_ShotgunDamage()
{
	return RS(RS_Q3A) ? 10 : 4;
}

int MM_Ruleset_ShotgunPelletCount()
{
	return RS(RS_Q3A) ? 11 : 12;
}

bool MM_Ruleset_BFGUsesQ3Style()
{
	return RS(RS_Q3A);
}

int MM_Ruleset_BFGAmmoPerShot()
{
	return MM_Ruleset_BFGUsesQ3Style() ? 10 : 50;
}

bool MM_Ruleset_Q3AHeavyAmmo(item_id_t ammo_id)
{
	return ammo_id == IT_AMMO_GRENADES || ammo_id == IT_AMMO_ROCKETS || ammo_id == IT_AMMO_SLUGS;
}

int MM_Ruleset_WeaponPickupAmmoQuantity(const gentity_t *ent, const gentity_t *other, const gitem_t *ammo)
{
	if (ent->count)
		return ent->count;

	if (RS(RS_Q3A)) {
		int quantity = MM_Ruleset_Q3AHeavyAmmo(ammo->id) ? 10 : ammo->quantity;

		if (other->client->pers.inventory[ammo->id] < quantity)
			return quantity - other->client->pers.inventory[ammo->id];

		return 1;
	}

	if (ammo->id == IT_AMMO_SLUGS)
		return MM_Ruleset_SlugPickupQuantity();

	return ammo->quantity;
}

int MM_Ruleset_WeaponDropAmmoQuantity(const gitem_t *item, const gitem_t *ammo)
{
	if (item->id == IT_WEAPON_RAILGUN)
		return MM_Ruleset_SlugPickupQuantity();

	return ammo->quantity;
}
