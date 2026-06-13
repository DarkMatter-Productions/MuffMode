// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstdint>

struct gentity_t;
struct gitem_t;
enum item_id_t : int32_t;

// [MuffMode] Per-ruleset weapon damage, spread, pickup, and projectile tuning.
int MM_Ruleset_SlugPickupQuantity();
int MM_Ruleset_MachinegunDamage();
void MM_Ruleset_MachinegunSpread(int &hspread, int &vspread);
int MM_Ruleset_ChaingunDamage(gentity_t *ent);
void MM_Ruleset_RocketLauncherDefaults(int &damage, int &splash_damage, float &splash_radius, int &speed, int &splash_knockback);
void MM_Ruleset_RocketLauncherDefaultsForCustomDamage(int damage, int &splash_damage, float &splash_radius, int &speed, int &splash_knockback);
int MM_Ruleset_HyperBlasterSpeed(bool hyper);
int MM_Ruleset_HyperBlasterDamage();
int MM_Ruleset_RailgunDamage();
int MM_Ruleset_RailgunKick(int damage);
void MM_Ruleset_BFGDefaults(int &damage, float &splash_radius, int &speed);
void MM_Ruleset_PlasmaBeamDefaults(int &damage, int &kick);
int MM_Ruleset_IonRipperDamage();
int MM_Ruleset_IonRipperSpeed();
void MM_Ruleset_GrenadeLauncherDefaults(int &damage, float &splash_radius, int &speed);
void MM_Ruleset_ChaingunSpreadDefaults(int &hspread, int &vspread, float &spread_offset);
int MM_Ruleset_ShotgunDamage();
int MM_Ruleset_ShotgunPelletCount();
bool MM_Ruleset_BFGUsesQ3Style();
int MM_Ruleset_BFGAmmoPerShot();
bool MM_Ruleset_Q3AHeavyAmmo(item_id_t ammo_id);
int MM_Ruleset_WeaponPickupAmmoQuantity(const gentity_t *ent, const gentity_t *other, const gitem_t *ammo);
int MM_Ruleset_WeaponDropAmmoQuantity(const gitem_t *item, const gitem_t *ammo);
