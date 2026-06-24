// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_ruleset_weapons.h"

#include <array>

namespace muffmode::ruleset_weapons {

struct PickupTuning {
	int slug_pickup_quantity = 0;
};

struct RocketTuning {
	int damage = 0;
	int splash_damage = 0;
	float splash_radius = 0.0f;
	int speed = 0;
	int splash_knockback = 0;
};

constexpr int k_q3_shotgun_spread = 700;
constexpr int k_q3_chaingun_spread = 600;
constexpr int k_q3_gauntlet_damage = 50;
constexpr int k_q3_lightning_damage = 8;
constexpr float k_q3_lightning_range = 768.0f;
constexpr int k_q3_nail_damage = 20;
constexpr int k_q3_nail_count = 15;
constexpr int k_q3_nail_spread = 500;
constexpr int k_q3_nail_min_speed = 555;
constexpr int k_q3_nail_speed_range = 1800;
// Q3 has a separate BFG ammo pool. In MuffMode Q3A it is cell-equivalent.
constexpr int k_q3_bfg_cell_ammo_per_shot = 10;
constexpr int k_q1_shotgun_spread = 328;
constexpr int k_q1_sshotgun_hspread = 1147;
constexpr int k_q1_sshotgun_vspread = 655;
constexpr int k_q1_lightning_damage = 30;
constexpr float k_q1_lightning_range = 600.0f;

constexpr int k_q2re_rocket_splash_knockback = 120;

constexpr std::array<PickupTuning, RS_NUM_RULESETS> k_pickup_tuning = {{
	/* RS_NONE */			{ 10 },
	/* RS_Q2RE */			{ 10 },
	/* RS_MM */				{ 5 },
	/* RS_Q3A */			{ 10 },
	/* RS_VANILLA_PLUS */	{ 10 },
	/* RS_Q1 */				{ 10 },
	/* RS_QC */				{ 10 },
}};

ruleset_t CurrentRulesetForWeaponTuning()
{
	return (ruleset_t)clamp((int)game.ruleset, (int)RS_NONE, (int)RS_NUM_RULESETS - 1);
}

RocketTuning RocketDefaults()
{
	switch (CurrentRulesetForWeaponTuning()) {
	case RS_MM:
		return { 100, 100, 120.0f, 650 };
	case RS_QC:
		return { 100, 100, 120.0f, 750 };
	case RS_VANILLA_PLUS:
		return { 100, 100, 120.0f, deathmatch->integer ? 750 : 650, k_q2re_rocket_splash_knockback };
	case RS_Q3A:
		return { 100, 100, 120.0f, 900 };
	case RS_Q1:
		return { irandom(100, 120), 120, 160.0f, 1000 };
	default:
		return { irandom(100, 120), 120, 120.0f, 650 };
	}
}

RocketTuning RocketDefaultsForCustomDamage(int damage)
{
	switch (CurrentRulesetForWeaponTuning()) {
	case RS_MM:
		return { damage, damage, 120.0f, 650 };
	case RS_QC:
		return { damage, damage, 120.0f, 750 };
	case RS_VANILLA_PLUS:
		return { damage, damage, 120.0f, deathmatch->integer ? 750 : 650, k_q2re_rocket_splash_knockback };
	case RS_Q3A:
		return { damage, damage, 120.0f, 900 };
	case RS_Q1:
		return { damage, 120, 160.0f, 1000 };
	default:
		return { damage, 120, 120.0f, 650 };
	}
}

int Q3AWeaponBaseQuantity(const gitem_t *weapon, const gitem_t *ammo)
{
	if (!weapon)
		return ammo ? ammo->quantity : 0;

	switch (weapon->id) {
	case IT_WEAPON_CHAINFIST:
		return 0;
	case IT_WEAPON_SHOTGUN:
		return 10;
	case IT_WEAPON_MACHINEGUN:
		return 40;
	case IT_WEAPON_CHAINGUN:
		return 80;
	case IT_WEAPON_GLAUNCHER:
	case IT_WEAPON_RLAUNCHER:
	case IT_WEAPON_RAILGUN:
	case IT_WEAPON_IONRIPPER:
		return 10;
	case IT_WEAPON_PLASMABEAM:
		return 100;
	case IT_WEAPON_HYPERBLASTER:
		return 50;
	case IT_WEAPON_BFG:
		return 20;
	default:
		return ammo ? ammo->quantity : 0;
	}
}

} // namespace muffmode::ruleset_weapons

int MM_Ruleset_SlugPickupQuantity()
{
	const int ruleset = (int)muffmode::ruleset_weapons::CurrentRulesetForWeaponTuning();
	return muffmode::ruleset_weapons::k_pickup_tuning[ruleset].slug_pickup_quantity;
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
	if (RS(RS_Q3A))
		return 7;
	if ((RS(RS_QC) || RS(RS_VANILLA_PLUS)) && deathmatch->integer) {
		if (!ent || !ent->client)
			return 4;
		return 4 + (ent->client->chaingun_shots++ & 1);
	}
	if (RS(RS_QC))
		return 6;
	return deathmatch->integer ? 6 : 8;
}

void MM_Ruleset_RocketLauncherDefaults(int &damage, int &splash_damage, float &splash_radius, int &speed, int &splash_knockback)
{
	const auto tuning = muffmode::ruleset_weapons::RocketDefaults();
	damage = tuning.damage;
	splash_damage = tuning.splash_damage;
	splash_radius = tuning.splash_radius;
	speed = tuning.speed;
	splash_knockback = tuning.splash_knockback;
}

void MM_Ruleset_RocketLauncherDefaultsForCustomDamage(int damage, int &splash_damage, float &splash_radius, int &speed, int &splash_knockback)
{
	const auto tuning = muffmode::ruleset_weapons::RocketDefaultsForCustomDamage(damage);
	splash_damage = tuning.splash_damage;
	splash_radius = tuning.splash_radius;
	speed = tuning.speed;
	splash_knockback = tuning.splash_knockback;
}

int MM_Ruleset_HyperBlasterSpeed(bool hyper)
{
	if (RS(RS_Q3A))
		return 2000;
	if (RS(RS_VANILLA_PLUS))
		return hyper ? (deathmatch->integer ? 1100 : 1000) : (deathmatch->integer ? 1700 : 1500);
	return hyper ? 1000 : 1500;
}

int MM_Ruleset_HyperBlasterDamage()
{
	if (RS(RS_Q3A))
		return 20;
	if (RS(RS_QC))
		return 12;
	return deathmatch->integer ? 15 : 20;
}

int MM_Ruleset_RailgunDamage()
{
	if (RS(RS_Q3A))
		return 100;
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
		speed = 2000;
	} else {
		damage = deathmatch->integer ? 200 : 500;
		splash_radius = 1000;
		speed = 400;
	}
}

void MM_Ruleset_PlasmaBeamDefaults(int &damage, int &kick)
{
	switch (muffmode::ruleset_weapons::CurrentRulesetForWeaponTuning()) {
	case RS_MM:
	case RS_VANILLA_PLUS:
	case RS_QC:
		damage = deathmatch->integer ? 10 : 15;
		kick = damage;
		break;
	case RS_Q3A:
		damage = muffmode::ruleset_weapons::k_q3_lightning_damage;
		kick = damage;
		break;
	case RS_Q1:
		damage = muffmode::ruleset_weapons::k_q1_lightning_damage;
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
	if (RS(RS_Q3A))
		return muffmode::ruleset_weapons::k_q3_nail_damage;
	if (deathmatch->integer)
		return RS(RS_MM) ? 20 : 30;
	return 50;
}

int MM_Ruleset_IonRipperSpeed()
{
	if (RS(RS_Q3A))
		return muffmode::ruleset_weapons::k_q3_nail_min_speed;
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
	if (RS(RS_Q3A)) {
		hspread = muffmode::ruleset_weapons::k_q3_chaingun_spread;
		vspread = muffmode::ruleset_weapons::k_q3_chaingun_spread;
		spread_offset = 0.0f;
		return;
	}

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
	if (RS(RS_Q1))
		return 6;
	return RS(RS_Q3A) ? 11 : 12;
}

int MM_Ruleset_ShotgunSpread()
{
	if (RS(RS_Q1))
		return muffmode::ruleset_weapons::k_q1_shotgun_spread;
	return RS(RS_Q3A) ? muffmode::ruleset_weapons::k_q3_shotgun_spread : 500;
}

int MM_Ruleset_SuperShotgunDamage()
{
	return RS(RS_Q1) ? 4 : 6;
}

int MM_Ruleset_SuperShotgunPelletCount()
{
	return RS(RS_Q1) ? 14 : DEFAULT_SSHOTGUN_COUNT;
}

void MM_Ruleset_SuperShotgunSpread(int &hspread, int &vspread)
{
	if (RS(RS_Q1)) {
		hspread = muffmode::ruleset_weapons::k_q1_sshotgun_hspread;
		vspread = muffmode::ruleset_weapons::k_q1_sshotgun_vspread;
		return;
	}

	hspread = DEFAULT_SHOTGUN_HSPREAD;
	vspread = DEFAULT_SHOTGUN_VSPREAD;
}

bool MM_Ruleset_SuperShotgunFallsBackToSingleShell()
{
	return RS(RS_Q1);
}

item_id_t MM_Ruleset_WeaponAmmoId(const gitem_t *item)
{
	if (!item)
		return IT_NULL;

	if (RS(RS_Q1) && item->id == IT_WEAPON_GLAUNCHER)
		return IT_AMMO_ROCKETS;

	return item->ammo;
}

int MM_Ruleset_WeaponAmmoRequired(const gitem_t *item)
{
	if (!item)
		return 0;

	if (!RS(RS_Q3A) && !RS(RS_Q1))
		return item->quantity;

	switch (item->id) {
	case IT_WEAPON_IONRIPPER:
	case IT_WEAPON_PLASMABEAM:
		return 1;
	case IT_WEAPON_BFG:
		return MM_Ruleset_BFGAmmoPerShot();
	default:
		return item->quantity;
	}
}

int MM_Ruleset_ChainfistDamage()
{
	return RS(RS_Q3A) ? muffmode::ruleset_weapons::k_q3_gauntlet_damage : (deathmatch->integer ? 15 : 7);
}

int MM_Ruleset_ChainfistKick()
{
	return RS(RS_Q3A) ? 0 : 100;
}

int MM_Ruleset_PlasmaBeamAmmoPerShot()
{
	return (RS(RS_Q3A) || RS(RS_Q1)) ? 1 : 2;
}

float MM_Ruleset_PlasmaBeamRange()
{
	if (RS(RS_Q1))
		return muffmode::ruleset_weapons::k_q1_lightning_range;

	return (RS(RS_MM) || RS(RS_VANILLA_PLUS) || RS(RS_QC) || RS(RS_Q3A)) ? muffmode::ruleset_weapons::k_q3_lightning_range : 8192.0f;
}

int MM_Ruleset_IonRipperProjectileCount()
{
	return RS(RS_Q3A) ? muffmode::ruleset_weapons::k_q3_nail_count : 1;
}

int MM_Ruleset_IonRipperSpread()
{
	return RS(RS_Q3A) ? muffmode::ruleset_weapons::k_q3_nail_spread : 0;
}

int MM_Ruleset_IonRipperMinSpeed()
{
	return RS(RS_Q3A) ? muffmode::ruleset_weapons::k_q3_nail_min_speed : MM_Ruleset_IonRipperSpeed();
}

int MM_Ruleset_IonRipperSpeedRange()
{
	return RS(RS_Q3A) ? muffmode::ruleset_weapons::k_q3_nail_speed_range : 0;
}

bool MM_Ruleset_BFGUsesQ3Style()
{
	return RS(RS_Q3A);
}

int MM_Ruleset_BFGAmmoPerShot()
{
	return MM_Ruleset_BFGUsesQ3Style() ? muffmode::ruleset_weapons::k_q3_bfg_cell_ammo_per_shot : 50;
}

bool MM_Ruleset_Q3AHeavyAmmo(item_id_t ammo_id)
{
	return ammo_id == IT_AMMO_GRENADES || ammo_id == IT_AMMO_ROCKETS || ammo_id == IT_AMMO_SLUGS;
}

int MM_Ruleset_WeaponPickupAmmoQuantity(const gentity_t *ent, const gentity_t *other, const gitem_t *ammo)
{
	if (!ammo)
		return 0;

	if (ent && ent->count < 0)
		return 0;

	if (ent && ent->count > 0)
		return ent->count;

	if (RS(RS_Q3A)) {
		const int quantity = muffmode::ruleset_weapons::Q3AWeaponBaseQuantity(ent ? ent->item : nullptr, ammo);
		const bool dropped = ent && !!(ent->spawnflags & (SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER));

		if (dropped || GT(GT_TDM) || !other || !other->client)
			return quantity;

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
	if (!item || !ammo)
		return 0;

	if (RS(RS_Q3A))
		return muffmode::ruleset_weapons::Q3AWeaponBaseQuantity(item, ammo);

	if (item->id == IT_WEAPON_RAILGUN)
		return MM_Ruleset_SlugPickupQuantity();

	return ammo->quantity;
}
