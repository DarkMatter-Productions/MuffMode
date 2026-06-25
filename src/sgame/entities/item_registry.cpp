// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include "g_local.h"

bool Pickup_Ammo(gentity_t *ent, gentity_t *other);
bool Pickup_Armor(gentity_t *ent, gentity_t *other);
bool Pickup_Ball(gentity_t *ent, gentity_t *other);
bool Pickup_Bandolier(gentity_t *ent, gentity_t *other);
bool Pickup_Doppelganger(gentity_t *ent, gentity_t *other);
bool Pickup_General(gentity_t *ent, gentity_t *other);
bool Pickup_Health(gentity_t *ent, gentity_t *other);
bool Pickup_Key(gentity_t *ent, gentity_t *other);
bool Pickup_LegacyHead(gentity_t *ent, gentity_t *other);
bool Pickup_Nuke(gentity_t *ent, gentity_t *other);
bool Pickup_Pack(gentity_t *ent, gentity_t *other);
bool Pickup_PowerArmor(gentity_t *ent, gentity_t *other);
bool Pickup_Powerup(gentity_t *ent, gentity_t *other);
bool Pickup_Sphere(gentity_t *ent, gentity_t *other);
bool Pickup_Teleporter(gentity_t *ent, gentity_t *other);
bool Pickup_TimedItem(gentity_t *ent, gentity_t *other);
bool Pickup_Weapon(gentity_t *ent, gentity_t *other);
bool Tech_Pickup(gentity_t *ent, gentity_t *other);

void Drop_Ammo(gentity_t *ent, gitem_t *item);
void Drop_Ball(gentity_t *ent, gitem_t *item);
void Drop_General(gentity_t *ent, gitem_t *item);
void Drop_PowerArmor(gentity_t *ent, gitem_t *item);
void Drop_Weapon(gentity_t *ent, gitem_t *item);
void Tech_Drop(gentity_t *ent, gitem_t *item);

void Use_Adrenaline(gentity_t *ent, gitem_t *item);
void Use_Ball(gentity_t *ent, gitem_t *item);
void Use_Breather(gentity_t *ent, gitem_t *item);
void Use_Compass(gentity_t *ent, gitem_t *item);
void Use_Defender(gentity_t *ent, gitem_t *item);
void Use_Doppelganger(gentity_t *ent, gitem_t *item);
void Use_Double(gentity_t *ent, gitem_t *item);
void Use_Envirosuit(gentity_t *ent, gitem_t *item);
void Use_Flashlight(gentity_t *ent, gitem_t *item);
void Use_Haste(gentity_t *ent, gitem_t *item);
void Use_Hunter(gentity_t *ent, gitem_t *item);
void Use_Invisibility(gentity_t *ent, gitem_t *item);
void Use_IR(gentity_t *ent, gitem_t *item);
void Use_Nuke(gentity_t *ent, gitem_t *item);
void Use_PowerArmor(gentity_t *ent, gitem_t *item);
void Use_Protection(gentity_t *ent, gitem_t *item);
void Use_Quad(gentity_t *ent, gitem_t *item);
void Use_Regeneration(gentity_t *ent, gitem_t *item);
void Use_Silencer(gentity_t *ent, gitem_t *item);
void Use_Teleporter(gentity_t *ent, gitem_t *item);
void Use_Vengeance(gentity_t *ent, gitem_t *item);
void Use_Weapon(gentity_t *ent, gitem_t *item);

void Weapon_BFG(gentity_t *ent);
void Weapon_Blaster(gentity_t *ent);
void Weapon_ChainFist(gentity_t *ent);
void Weapon_Chaingun(gentity_t *ent);
void Weapon_Disruptor(gentity_t *ent);
void Weapon_ETF_Rifle(gentity_t *ent);
void Weapon_Grapple(gentity_t *ent);
void Weapon_GrenadeLauncher(gentity_t *ent);
void Weapon_HandGrenade(gentity_t *ent);
void Weapon_HyperBlaster(gentity_t *ent);
void Weapon_IonRipper(gentity_t *ent);
void Weapon_Machinegun(gentity_t *ent);
void Weapon_Phalanx(gentity_t *ent);
void Weapon_PlasmaBeam(gentity_t *ent);
void Weapon_ProxLauncher(gentity_t *ent);
void Weapon_Railgun(gentity_t *ent);
void Weapon_RocketLauncher(gentity_t *ent);
void Weapon_Shotgun(gentity_t *ent);
void Weapon_SuperShotgun(gentity_t *ent);
void Weapon_Tesla(gentity_t *ent);
void Weapon_Trap(gentity_t *ent);
// clang-format off
gitem_t	itemlist[] =
{
	{ },	// leave index 0 alone

	//
	// ARMOR
	//

/*QUAKED item_armor_body (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/armor/body/tris.md2"
*/
	{
		/* id */ IT_ARMOR_BODY,
		/* classname */ "item_armor_body",
		/* pickup */ Pickup_Armor,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/ar3_pkup.wav",
		/* world_model */ "models/items/armor/body/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_bodyarmor",
		/* use_name */   "Body Armor",
		/* pickup_name */   "$item_body_armor",
		/* pickup_name_definite */ "$item_body_armor_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR,
		/* vwep_model */ nullptr,
		/* armor_info */ &bodyarmor_info
	},

/*QUAKED item_armor_combat (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_ARMOR_COMBAT,
		/* classname */ "item_armor_combat",
		/* pickup */ Pickup_Armor,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/ar1_pkup.wav",
		/* world_model */ "models/items/armor/combat/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_combatarmor",
		/* use_name */  "Combat Armor",
		/* pickup_name */  "$item_combat_armor",
		/* pickup_name_definite */ "$item_combat_armor_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR,
		/* vwep_model */ nullptr,
		/* armor_info */ &combatarmor_info
	},

/*QUAKED item_armor_jacket (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_ARMOR_JACKET,
		/* classname */ "item_armor_jacket",
		/* pickup */ Pickup_Armor,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/ar1_pkup.wav",
		/* world_model */ "models/items/armor/jacket/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_jacketarmor",
		/* use_name */  "Jacket Armor",
		/* pickup_name */  "$item_jacket_armor",
		/* pickup_name_definite */ "$item_jacket_armor_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR,
		/* vwep_model */ nullptr,
		/* armor_info */ &jacketarmor_info
	},

/*QUAKED item_armor_shard (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_ARMOR_SHARD,
		/* classname */ "item_armor_shard",
		/* pickup */ Pickup_Armor,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/ar2_pkup.wav",
		/* world_model */ "models/items/armor/shard/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_armor_shard",
		/* use_name */  "Armor Shard",
		/* pickup_name */  "$item_armor_shard",
		/* pickup_name_definite */ "$item_armor_shard_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR
	},

/*QUAKED item_power_screen (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_POWER_SCREEN,
		/* classname */ "item_power_screen",
		/* pickup */ Pickup_PowerArmor,
		/* use */ Use_PowerArmor,
		/* drop */ Drop_PowerArmor,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/ar3_pkup.wav",
		/* world_model */ "models/items/armor/screen/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_powerscreen",
		/* use_name */  "Power Screen",
		/* pickup_name */  "$item_power_screen",
		/* pickup_name_definite */ "$item_power_screen_def",
		/* quantity */ 60,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_SCREEN,
		/* precaches */ "misc/power2.wav misc/power1.wav"
	},

/*QUAKED item_power_shield (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_POWER_SHIELD,
		/* classname */ "item_power_shield",
		/* pickup */ Pickup_PowerArmor,
		/* use */ Use_PowerArmor,
		/* drop */ Drop_PowerArmor,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/ar3_pkup.wav",
		/* world_model */ "models/items/armor/shield/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_powershield",
		/* use_name */  "Power Shield",
		/* pickup_name */  "$item_power_shield",
		/* pickup_name_definite */ "$item_power_shield_def",
		/* quantity */ 60,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_SHIELD,
		/* precaches */ "misc/power2.wav misc/power1.wav"
	},

	//
	// WEAPONS 
	//

/* weapon_grapple (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_WEAPON_GRAPPLE,
		/* classname */ "weapon_grapple",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Grapple,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_flareg/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/grapple/tris.md2",
		/* icon */ "w_grapple",
		/* use_name */  "Grapple",
		/* pickup_name */  "$item_grapple",
		/* pickup_name_definite */ "$item_grapple_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_WEAPON_BLASTER,
		/* flags */ IF_WEAPON | IF_NO_HASTE | IF_POWERUP_WHEEL | IF_NOT_RANDOM,
		/* vwep_model */ "#w_grapple.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/grapple/grfire.wav weapons/grapple/grpull.wav weapons/grapple/grhang.wav weapons/grapple/grreset.wav weapons/grapple/grhit.wav weapons/grapple/grfly.wav"
	},

/* weapon_blaster (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_WEAPON_BLASTER,
		/* classname */ "weapon_blaster",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Blaster,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_blast/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_blast/tris.md2",
		/* icon */ "w_blaster",
		/* use_name */  "Blaster",
		/* pickup_name */  "$item_blaster",
		/* pickup_name_definite */ "$item_blaster_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_WEAPON_BLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP | IF_NOT_RANDOM,
		/* vwep_model */ "#w_blaster.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/blastf1a.wav misc/lasfly.wav"
	},

/*QUAKED weapon_chainfist (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_chainf/tris.md2"
*/
	{
		/* id */ IT_WEAPON_CHAINFIST,
		/* classname */ "weapon_chainfist",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_ChainFist,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_chainf/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_chainf/tris.md2",
		/* icon */ "w_chainfist",
		/* use_name */  "Chainfist",
		/* pickup_name */  "$item_chainfist",
		/* pickup_name_definite */ "$item_chainfist_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_WEAPON_BLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP | IF_NO_HASTE,
		/* vwep_model */ "#w_chainfist.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/sawidle.wav weapons/sawhit.wav weapons/sawslice.wav",
	},

/*QUAKED weapon_shotgun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_shotg/tris.md2"
*/
	{
		/* id */ IT_WEAPON_SHOTGUN,
		/* classname */ "weapon_shotgun",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Shotgun,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_shotg/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_shotg/tris.md2",
		/* icon */ "w_shotgun",
		/* use_name */  "Shotgun",
		/* pickup_name */  "$item_shotgun",
		/* pickup_name_definite */ "$item_shotgun_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_SHELLS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_shotgun.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_SHELLS,
		/* precaches */ "weapons/shotgf1b.wav weapons/shotgr1b.wav"
	},

/*QUAKED weapon_supershotgun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_shotg2/tris.md2"
*/
	{
		/* id */ IT_WEAPON_SSHOTGUN,
		/* classname */ "weapon_supershotgun",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_SuperShotgun,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_shotg2/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_shotg2/tris.md2",
		/* icon */ "w_sshotgun",
		/* use_name */  "Super Shotgun",
		/* pickup_name */  "$item_super_shotgun",
		/* pickup_name_definite */ "$item_super_shotgun_def",
		/* quantity */ 2,
		/* ammo */ IT_AMMO_SHELLS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_sshotgun.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_SHELLS,
		/* precaches */ "weapons/sshotf1b.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 10
	},

/*QUAKED weapon_machinegun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_machn/tris.md2"
*/
	{
		/* id */ IT_WEAPON_MACHINEGUN,
		/* classname */ "weapon_machinegun",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Machinegun,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_machn/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_machn/tris.md2",
		/* icon */ "w_machinegun",
		/* use_name */  "Machinegun",
		/* pickup_name */  "$item_machinegun",
		/* pickup_name_definite */ "$item_machinegun_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_BULLETS,
		/* chain */ IT_WEAPON_MACHINEGUN,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_machinegun.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_BULLETS,
		/* precaches */ "weapons/machgf1b.wav weapons/machgf2b.wav weapons/machgf3b.wav weapons/machgf4b.wav weapons/machgf5b.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 30
	},

/*QUAKED weapon_etf_rifle (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_etf_rifle/tris.md2"
*/
	{
		/* id */ IT_WEAPON_ETF_RIFLE,
		/* classname */ "weapon_etf_rifle",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_ETF_Rifle,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_etf_rifle/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_etf_rifle/tris.md2",
		/* icon */ "w_etf_rifle",
		/* use_name */  "ETF Rifle",
		/* pickup_name */  "$item_etf_rifle",
		/* pickup_name_definite */ "$item_etf_rifle_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_FLECHETTES,
		/* chain */ IT_WEAPON_MACHINEGUN,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_etfrifle.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_FLECHETTES,
		/* precaches */ "weapons/nail1.wav models/proj/flechette/tris.md2",
		/* sort_id */ 0,
		/* quantity_warn */ 30
	},

/*QUAKED weapon_chaingun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_chain/tris.md2"
*/
	{
		/* id */ IT_WEAPON_CHAINGUN,
		/* classname */ "weapon_chaingun",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Chaingun,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_chain/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_chain/tris.md2",
		/* icon */ "w_chaingun",
		/* use_name */  "Chaingun",
		/* pickup_name */  "$item_chaingun",
		/* pickup_name_definite */ "$item_chaingun_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_BULLETS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_chaingun.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_BULLETS,
		/* precaches */ "weapons/chngnu1a.wav weapons/chngnl1a.wav weapons/machgf3b.wav weapons/chngnd1a.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 60
	},

/*QUAKED ammo_grenades (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_AMMO_GRENADES,
		/* classname */ "ammo_grenades",
		/* pickup */ Pickup_Ammo,
		/* use */ Use_Weapon,
		/* drop */ Drop_Ammo,
		/* weaponthink */ Weapon_HandGrenade,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/items/ammo/grenades/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ "models/weapons/v_handgr/tris.md2",
		/* icon */ "a_grenades",
		/* use_name */  "Grenades",
		/* pickup_name */  "$item_grenades",
		/* pickup_name_definite */ "$item_grenades_def",
		/* quantity */ 5,
		/* ammo */ IT_AMMO_GRENADES,
		/* chain */ IT_AMMO_GRENADES,
		/* flags */ IF_AMMO | IF_WEAPON,
		/* vwep_model */ "#a_grenades.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_GRENADES,
		/* precaches */ "weapons/hgrent1a.wav weapons/hgrena1b.wav weapons/hgrenc1b.wav weapons/hgrenb1a.wav weapons/hgrenb2a.wav models/objects/grenade3/tris.md2",
		/* sort_id */ 0,
		/* quantity_warn */ 2
	},

/*QUAKED ammo_trap (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/weapons/g_trap/tris.md2"
*/
	{
		/* id */ IT_AMMO_TRAP,
		/* classname */ "ammo_trap",
		/* pickup */ Pickup_Ammo,
		/* use */ Use_Weapon,
		/* drop */ Drop_Ammo,
		/* weaponthink */ Weapon_Trap,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/weapons/g_trap/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_trap/tris.md2",
		/* icon */ "a_trap",
		/* use_name */  "Trap",
		/* pickup_name */  "$item_trap",
		/* pickup_name_definite */ "$item_trap_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_TRAP,
		/* chain */ IT_AMMO_GRENADES,
		/* flags */ IF_AMMO | IF_WEAPON | IF_NO_INFINITE_AMMO,
		/* vwep_model */ "#a_trap.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_TRAP,
		/* precaches */ "misc/fhit3.wav weapons/trapcock.wav weapons/traploop.wav weapons/trapsuck.wav weapons/trapdown.wav items/s_health.wav items/n_health.wav items/l_health.wav items/m_health.wav models/weapons/z_trap/tris.md2",
		/* sort_id */ 0,
		/* quantity_warn */ 1
	},

/*QUAKED ammo_tesla (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/ammo/am_tesl/tris.md2"
*/
	{
		/* id */ IT_AMMO_TESLA,
		/* classname */ "ammo_tesla",
		/* pickup */ Pickup_Ammo,
		/* use */ Use_Weapon,
		/* drop */ Drop_Ammo,
		/* weaponthink */ Weapon_Tesla,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/ammo/am_tesl/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ "models/weapons/v_tesla/tris.md2",
		/* icon */ "a_tesla",
		/* use_name */  "Tesla",
		/* pickup_name */  "$item_tesla",
		/* pickup_name_definite */ "$item_tesla_def",
		/* quantity */ 3,
		/* ammo */ IT_AMMO_TESLA,
		/* chain */ IT_AMMO_GRENADES,
		/* flags */ IF_AMMO | IF_WEAPON | IF_NO_INFINITE_AMMO,
		/* vwep_model */ "#a_tesla.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_TESLA,
		/* precaches */ "weapons/teslaopen.wav weapons/hgrenb1a.wav weapons/hgrenb2a.wav models/weapons/g_tesla/tris.md2",
		/* sort_id */ 0,
		/* quantity_warn */ 1
	},

/*QUAKED weapon_grenadelauncher (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_launch/tris.md2"
*/
	{
		/* id */ IT_WEAPON_GLAUNCHER,
		/* classname */ "weapon_grenadelauncher",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_GrenadeLauncher,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_launch/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_launch/tris.md2",
		/* icon */ "w_glauncher",
		/* use_name */  "Grenade Launcher",
		/* pickup_name */  "$item_grenade_launcher",
		/* pickup_name_definite */ "$item_grenade_launcher_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_GRENADES,
		/* chain */ IT_WEAPON_GLAUNCHER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_glauncher.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_GRENADES,
		/* precaches */ "models/objects/grenade4/tris.md2 weapons/grenlf1a.wav weapons/grenlr1b.wav weapons/grenlb1b.wav"
	},

/*QUAKED weapon_proxlauncher (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_plaunch/tris.md2"
*/
	{
		/* id */ IT_WEAPON_PROXLAUNCHER,
		/* classname */ "weapon_proxlauncher",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_ProxLauncher,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_plaunch/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_plaunch/tris.md2",
		/* icon */ "w_proxlaunch",
		/* use_name */  "Prox Launcher",
		/* pickup_name */  "$item_prox_launcher",
		/* pickup_name_definite */ "$item_prox_launcher_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_PROX,
		/* chain */ IT_WEAPON_GLAUNCHER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_plauncher.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_PROX,
		/* precaches */ "weapons/grenlf1a.wav weapons/grenlr1b.wav weapons/grenlb1b.wav weapons/proxwarn.wav weapons/proxopen.wav",
	},

/*QUAKED weapon_rocketlauncher (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_rocket/tris.md2"
*/
	{
		/* id */ IT_WEAPON_RLAUNCHER,
		/* classname */ "weapon_rocketlauncher",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_RocketLauncher,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_rocket/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_rocket/tris.md2",
		/* icon */ "w_rlauncher",
		/* use_name */  "Rocket Launcher",
		/* pickup_name */  "$item_rocket_launcher",
		/* pickup_name_definite */ "$item_rocket_launcher_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_ROCKETS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_rlauncher.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_ROCKETS,
		/* precaches */ "models/objects/rocket/tris.md2 weapons/rockfly.wav weapons/rocklf1a.wav weapons/rocklr1b.wav models/objects/debris2/tris.md2"
	},

/*QUAKED weapon_hyperblaster (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_hyperb/tris.md2"
*/
	{
		/* id */ IT_WEAPON_HYPERBLASTER,
		/* classname */ "weapon_hyperblaster",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_HyperBlaster,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_hyperb/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_hyperb/tris.md2",
		/* icon */ "w_hyperblaster",
		/* use_name */  "HyperBlaster",
		/* pickup_name */  "$item_hyperblaster",
		/* pickup_name_definite */ "$item_hyperblaster_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_WEAPON_HYPERBLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_hyperblaster.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_CELLS,
		/* precaches */ "weapons/hyprbu1a.wav weapons/hyprbl1a.wav weapons/hyprbf1a.wav weapons/hyprbd1a.wav misc/lasfly.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 30
	},

/*QUAKED weapon_boomer (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_boom/tris.md2"
*/
	{
		/* id */ IT_WEAPON_IONRIPPER,
		/* classname */ "weapon_boomer",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_IonRipper,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_boom/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_boomer/tris.md2",
		/* icon */ "w_ripper",
		/* use_name */  "Ionripper",
		/* pickup_name */  "$item_ionripper",
		/* pickup_name_definite */ "$item_ionripper_def",
		/* quantity */ 2,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_WEAPON_HYPERBLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_ripper.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_CELLS,
		/* precaches */ "weapons/rippfire.wav models/objects/boomrang/tris.md2 misc/lasfly.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 30
	},

/*QUAKED weapon_plasmabeam (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_beamer/tris.md2"
*/
	{
		/* id */ IT_WEAPON_PLASMABEAM,
		/* classname */ "weapon_plasmabeam",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_PlasmaBeam,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_beamer/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_beamer/tris.md2",
		/* icon */ "w_heatbeam",
		/* use_name */  "Plasma Beam",
		/* pickup_name */  "$item_plasma_beam",
		/* pickup_name_definite */ "$item_plasma_beam_def",
		/* quantity */ 2,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_WEAPON_HYPERBLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_plasma.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_CELLS,
		/* precaches */ "weapons/bfg__l1a.wav weapons/bfg_hum.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 50
	},

/*QUAKED weapon_railgun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_rail/tris.md2"
*/
	{
		/* id */ IT_WEAPON_RAILGUN,
		/* classname */ "weapon_railgun",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Railgun,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_rail/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_rail/tris.md2",
		/* icon */ "w_railgun",
		/* use_name */  "Railgun",
		/* pickup_name */  "$item_railgun",
		/* pickup_name_definite */ "$item_railgun_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_SLUGS,
		/* chain */ IT_WEAPON_RAILGUN,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_railgun.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_SLUGS,
		/* precaches */ "weapons/rg_hum.wav"
	},

/*QUAKED weapon_phalanx (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_shotx/tris.md2"
*/
	{
		/* id */ IT_WEAPON_PHALANX,
		/* classname */ "weapon_phalanx",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Phalanx,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_shotx/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_shotx/tris.md2",
		/* icon */ "w_phallanx",
		/* use_name */  "Phalanx",
		/* pickup_name */  "$item_phalanx",
		/* pickup_name_definite */ "$item_phalanx_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_MAGSLUG,
		/* chain */ IT_WEAPON_RAILGUN,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_phalanx.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_MAGSLUG,
		/* precaches */ "weapons/plasshot.wav sprites/s_photon.sp2 weapons/rockfly.wav"
	},

/*QUAKED weapon_bfg (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_bfg/tris.md2"
*/
	{
		/* id */ IT_WEAPON_BFG,
		/* classname */ "weapon_bfg",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_BFG,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_bfg/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_bfg/tris.md2",
		/* icon */ "w_bfg",
		/* use_name */  "BFG10K",
		/* pickup_name */  "$item_bfg10k",
		/* pickup_name_definite */ "$item_bfg10k_def",
		/* quantity */ 50,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_WEAPON_BFG,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_bfg.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_CELLS,
		/* precaches */ "sprites/s_bfg1.sp2 sprites/s_bfg2.sp2 sprites/s_bfg3.sp2 weapons/bfg__f1y.wav weapons/bfg__l1a.wav weapons/bfg__x1b.wav weapons/bfg_hum.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 50
	},

/*QUAKED weapon_disintegrator (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_dist/tris.md2"
*/
	{
		/* id */ IT_WEAPON_DISRUPTOR,
		/* classname */ "weapon_disintegrator",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Disruptor,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_dist/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_dist/tris.md2",
		/* icon */ "w_disintegrator",
		/* use_name */  "Disruptor",
		/* pickup_name */  "$item_disruptor",
		/* pickup_name_definite */ "$item_disruptor_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_ROUNDS,
		/* chain */ IT_WEAPON_BFG,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_disrupt.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_DISRUPTOR,
		/* precaches */ "models/proj/disintegrator/tris.md2 weapons/disrupt.wav weapons/disint2.wav weapons/disrupthit.wav",
	},

	//
	// AMMO ITEMS
	//

/*QUAKED ammo_shells (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/ammo/shells/medium/tris.md2"
*/
	{
		/* id */ IT_AMMO_SHELLS,
		/* classname */ "ammo_shells",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/items/ammo/shells/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_shells",
		/* use_name */  "Shells",
		/* pickup_name */  "$item_shells",
		/* pickup_name_definite */ "$item_shells_def",
		/* quantity */ 10,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_SHELLS
	},

/*QUAKED ammo_bullets (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/ammo/bullets/medium/tris.md2"
*/
	{
		/* id */ IT_AMMO_BULLETS,
		/* classname */ "ammo_bullets",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/items/ammo/bullets/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_bullets",
		/* use_name */  "Bullets",
		/* pickup_name */  "$item_bullets",
		/* pickup_name_definite */ "$item_bullets_def",
		/* quantity */ 50,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_BULLETS
	},

/*QUAKED ammo_cells (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/ammo/cells/medium/tris.md2"
*/
	{
		/* id */ IT_AMMO_CELLS,
		/* classname */ "ammo_cells",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/items/ammo/cells/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_cells",
		/* use_name */  "Cells",
		/* pickup_name */  "$item_cells",
		/* pickup_name_definite */ "$item_cells_def",
		/* quantity */ 50,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_CELLS
	},

/*QUAKED ammo_rockets (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/ammo/rockets/medium/tris.md2"
*/
	{
		/* id */ IT_AMMO_ROCKETS,
		/* classname */ "ammo_rockets",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/items/ammo/rockets/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_rockets",
		/* use_name */  "Rockets",
		/* pickup_name */  "$item_rockets",
		/* pickup_name_definite */ "$item_rockets_def",
		/* quantity */ 5,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_ROCKETS
	},

/*QUAKED ammo_slugs (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/ammo/slugs/medium/tris.md2"
*/
	{
		/* id */ IT_AMMO_SLUGS,
		/* classname */ "ammo_slugs",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/items/ammo/slugs/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_slugs",
		/* use_name */  "Slugs",
		/* pickup_name */  "$item_slugs",
		/* pickup_name_definite */ "$item_slugs_def",
		/* quantity */ 5,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_SLUGS
	},

/*QUAKED ammo_magslug (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/objects/ammo/tris.md2"
*/
	{
		/* id */ IT_AMMO_MAGSLUG,
		/* classname */ "ammo_magslug",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/objects/ammo/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_mslugs",
		/* use_name */  "Mag Slug",
		/* pickup_name */  "$item_mag_slug",
		/* pickup_name_definite */ "$item_mag_slug_def",
		/* quantity */ 10,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_MAGSLUG
	},

/*QUAKED ammo_flechettes (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/ammo/am_flechette/tris.md2"
*/
	{
		/* id */ IT_AMMO_FLECHETTES,
		/* classname */ "ammo_flechettes",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/ammo/am_flechette/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_flechettes",
		/* use_name */  "Flechettes",
		/* pickup_name */  "$item_flechettes",
		/* pickup_name_definite */ "$item_flechettes_def",
		/* quantity */ 50,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_FLECHETTES
	},

/*QUAKED ammo_prox (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/ammo/am_prox/tris.md2"
*/
	{
		/* id */ IT_AMMO_PROX,
		/* classname */ "ammo_prox",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/ammo/am_prox/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_prox",
		/* use_name */  "Prox",
		/* pickup_name */  "$item_prox",
		/* pickup_name_definite */ "$item_prox_def",
		/* quantity */ 5,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_PROX,
		/* precaches */ "models/weapons/g_prox/tris.md2 weapons/proxwarn.wav"
	},

/*QUAKED ammo_nuke (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/ammo/g_nuke/tris.md2"
*/
	{
		/* id */ IT_AMMO_NUKE,
		/* classname */ "ammo_nuke",
		/* pickup */ Pickup_Nuke,
		/* use */ Use_Nuke,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/weapons/g_nuke/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_nuke",
		/* use_name */  "A-M Bomb",
		/* pickup_name */  "$item_am_bomb",
		/* pickup_name_definite */ "$item_am_bomb_def",
		/* quantity */ 300,
		/* ammo */ IT_AMMO_NUKE,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_AM_BOMB,
		/* precaches */ "weapons/nukewarn2.wav world/rumble.wav"
	},

/*QUAKED ammo_disruptor (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/ammo/am_disr/tris.md2"
*/
	{
		/* id */ IT_AMMO_ROUNDS,
		/* classname */ "ammo_disruptor",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/ammo/am_disr/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_disruptor",
		/* use_name */  "Rounds",
		/* pickup_name */  "$item_rounds",
		/* pickup_name_definite */ "$item_rounds_def",
		/* quantity */ 3,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_DISRUPTOR
	},

//
// POWERUP ITEMS
//
/*QUAKED item_quad (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/quaddama/tris.md2"
*/
	{
		/* id */ IT_POWERUP_QUAD,
		/* classname */ "item_quad",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Quad,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/quaddama/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_quad",
		/* use_name */  "Quad Damage",
		/* pickup_name */  "$item_quad_damage",
		/* pickup_name_definite */ "$item_quad_damage_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_QUAD,
		/* precaches */ "items/damage.wav items/damage2.wav items/damage3.wav ctf/tech2x.wav"
	},

/*QUAKED item_quadfire (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/quadfire/tris.md2"
*/
	{
		/* id */ IT_POWERUP_HASTE,
		/* classname */ "item_quadfire",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Haste,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/quadfire/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_quadfire",
		/* use_name */  "Haste",
		/* pickup_name */  "Haste",
		/* pickup_name_definite */ "Haste",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_HASTE,
		/* precaches */ "items/quadfire1.wav items/quadfire2.wav items/quadfire3.wav"
	},

/*QUAKED item_invulnerability (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/invulner/tris.md2"
*/
	{
		/* id */ IT_POWERUP_PROTECTION,
		/* classname */ "item_invulnerability",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Protection,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/invulner/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_invulnerability",
		/* use_name */  "Protection",
		/* pickup_name */  "Protection",
		/* pickup_name_definite */ "Protection",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_PROTECTION,
		/* precaches */ "items/protect.wav items/protect2.wav items/protect4.wav"
	},

/*QUAKED item_invisibility (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/cloaker/tris.md2"
*/
	{
		/* id */ IT_POWERUP_INVISIBILITY,
		/* classname */ "item_invisibility",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Invisibility,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/cloaker/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_cloaker",
		/* use_name */  "Invisibility",
		/* pickup_name */  "$item_invisibility",
		/* pickup_name_definite */ "$item_invisibility_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_INVISIBILITY,
	},

/*QUAKED item_silencer (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/silencer/tris.md2"
*/
	{
		/* id */ IT_POWERUP_SILENCER,
		/* classname */ "item_silencer",
		/* pickup */ Pickup_TimedItem,
		/* use */ Use_Silencer,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/silencer/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_silencer",
		/* use_name */  "Silencer",
		/* pickup_name */  "$item_silencer",
		/* pickup_name_definite */ "$item_silencer_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_SILENCER,
	},

/*QUAKED item_breather (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/breather/tris.md2"
*/
	{
		/* id */ IT_POWERUP_REBREATHER,
		/* classname */ "item_breather",
		/* pickup */ Pickup_TimedItem,
		/* use */ Use_Breather,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/breather/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_rebreather",
		/* use_name */  "Rebreather",
		/* pickup_name */  "$item_rebreather",
		/* pickup_name_definite */ "$item_rebreather_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_TIMED | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_REBREATHER,
		/* precaches */ "items/airout.wav"
	},

/*QUAKED item_enviro (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/enviro/tris.md2"
*/
	{
		/* id */ IT_POWERUP_ENVIROSUIT,
		/* classname */ "item_enviro",
		/* pickup */ Pickup_TimedItem,
		/* use */ Use_Envirosuit,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/enviro/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_envirosuit",
		/* use_name */  "Environment Suit",
		/* pickup_name */  "$item_environment_suit",
		/* pickup_name_definite */ "$item_environment_suit_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_TIMED | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_ENVIROSUIT,
		/* precaches */ "items/airout.wav"
	},

/*QUAKED item_ancient_head (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Special item that gives +2 to maximum health
model="models/items/c_head/tris.md2"
*/
	{
		/* id */ IT_ANCIENT_HEAD,
		/* classname */ "item_ancient_head",
		/* pickup */ Pickup_LegacyHead,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/c_head/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_fixme",
		/* use_name */  "Ancient Head",
		/* pickup_name */  "$item_ancient_head",
		/* pickup_name_definite */ "$item_ancient_head_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH | IF_NOT_RANDOM,
	},

/*QUAKED item_legacy_head (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Special item that gives +5 to maximum health.
model="models/items/legacyhead/tris.md2"
*/
	{
		/* id */ IT_LEGACY_HEAD,
		/* classname */ "item_legacy_head",
		/* pickup */ Pickup_LegacyHead,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/legacyhead/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_fixme",
		/* use_name */  "Ranger's Head",
		/* pickup_name */  "Ranger's Head",
		/* pickup_name_definite */ "Ranger's Head",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH | IF_NOT_RANDOM,
	},

/*QUAKED item_adrenaline (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Gives +1 to maximum health, +5 in deathmatch.
model="models/items/adrenal/tris.md2"
*/
	{
		/* id */ IT_ADRENALINE,
		/* classname */ "item_adrenaline",
		/* pickup */ Pickup_TimedItem,
		/* use */ Use_Adrenaline,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/adrenal/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_adrenaline",
		/* use_name */  "Adrenaline",
		/* pickup_name */  "$item_adrenaline",
		/* pickup_name_definite */ "$item_adrenaline_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_ADRENALINE,
		/* precache */ "items/n_health.wav"
	},

/*QUAKED item_bandolier (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/band/tris.md2"
*/
	{
		/* id */ IT_BANDOLIER,
		/* classname */ "item_bandolier",
		/* pickup */ Pickup_Bandolier,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/band/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_bandolier",
		/* use_name */  "Bandolier",
		/* pickup_name */  "$item_bandolier",
		/* pickup_name_definite */ "$item_bandolier_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED
	},

/*QUAKED item_pack (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/pack/tris.md2"
*/
	{
		/* id */ IT_PACK,
		/* classname */ "item_pack",
		/* pickup */ Pickup_Pack,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/pack/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_pack",
		/* use_name */  "Ammo Pack",
		/* pickup_name */  "$item_ammo_pack",
		/* pickup_name_definite */ "$item_ammo_pack_def",
		/* quantity */ 180,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED
	},

/*QUAKED item_ir_goggles (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Infrared vision.
model="models/items/goggles/tris.md2"
*/
	{
		/* id */ IT_IR_GOGGLES,
		/* classname */ "item_ir_goggles",
		/* pickup */ Pickup_TimedItem,
		/* use */ Use_IR,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/goggles/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_ir",
		/* use_name */  "IR Goggles",
		/* pickup_name */  "$item_ir_goggles",
		/* pickup_name_definite */ "$item_ir_goggles_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_IR_GOGGLES,
		/* precaches */ "misc/ir_start.wav"
	},

/*QUAKED item_double (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/ddamage/tris.md2"
*/
	{
		/* id */ IT_POWERUP_DOUBLE,
		/* classname */ "item_double",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Double,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/ddamage/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_double",
		/* use_name */  "Double Damage",
		/* pickup_name */  "$item_double_damage",
		/* pickup_name_definite */ "$item_double_damage_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_DOUBLE,
		/* precaches */ "misc/ddamage1.wav misc/ddamage2.wav misc/ddamage3.wav ctf/tech2x.wav"
	},

/*QUAKED item_sphere_vengeance (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/vengnce/tris.md2"
*/
	{
		/* id */ IT_POWERUP_SPHERE_VENGEANCE,
		/* classname */ "item_sphere_vengeance",
		/* pickup */ Pickup_Sphere,
		/* use */ Use_Vengeance,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/vengnce/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_vengeance",
		/* use_name */  "vengeance sphere",
		/* pickup_name */  "$item_vengeance_sphere",
		/* pickup_name_definite */ "$item_vengeance_sphere_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_SPHERE | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_SPHERE_VENGEANCE,
		/* precaches */ "spheres/v_idle.wav"
	},

/*QUAKED item_sphere_hunter (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/hunter/tris.md2"
*/
	{
		/* id */ IT_POWERUP_SPHERE_HUNTER,
		/* classname */ "item_sphere_hunter",
		/* pickup */ Pickup_Sphere,
		/* use */ Use_Hunter,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/hunter/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_hunter",
		/* use_name */  "hunter sphere",
		/* pickup_name */  "$item_hunter_sphere",
		/* pickup_name_definite */ "$item_hunter_sphere_def",
		/* quantity */ 120,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_SPHERE | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_SPHERE_HUNTER,
		/* precaches */ "spheres/h_idle.wav spheres/h_active.wav spheres/h_lurk.wav"
	},

/*QUAKED item_sphere_defender (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/defender/tris.md2"
*/
	{
		/* id */ IT_POWERUP_SPHERE_DEFENDER,
		/* classname */ "item_sphere_defender",
		/* pickup */ Pickup_Sphere,
		/* use */ Use_Defender,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/defender/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_defender",
		/* use_name */  "defender sphere",
		/* pickup_name */  "$item_defender_sphere",
		/* pickup_name_definite */ "$item_defender_sphere_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_SPHERE | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_SPHERE_DEFENDER,
		/* precaches */ "models/objects/laser/tris.md2 models/items/shell/tris.md2 spheres/d_idle.wav"
	},

/*QUAKED item_doppleganger (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/dopple/tris.md2"
*/
	{
		/* id */ IT_DOPPELGANGER,
		/* classname */ "item_doppleganger",
		/* pickup */ Pickup_Doppelganger,
		/* use */ Use_Doppelganger,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/dopple/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_doppleganger",
		/* use_name */  "Doppelganger",
		/* pickup_name */  "$item_doppleganger",
		/* pickup_name_definite */ "$item_doppleganger_def",
		/* quantity */ 90,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_DOPPELGANGER,
		/* precaches */ "models/objects/dopplebase/tris.md2 models/items/spawngro3/tris.md2 medic_commander/monsterspawn1.wav models/items/hunter/tris.md2 models/items/vengnce/tris.md2",
	},

/* Tag Token */
	{
		/* id */ IT_TAG_TOKEN,
		/* classname */ nullptr,
		/* pickup */ nullptr,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/tagtoken/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB | EF_TAGTRAIL,
		/* view_model */ nullptr,
		/* icon */ "i_tagtoken",
		/* use_name */  "Tag Token",
		/* pickup_name */  "$item_tag_token",
		/* pickup_name_definite */ "$item_tag_token_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED | IF_NOT_GIVEABLE
	},

	//
	// KEYS
	//
/*QUAKED key_data_cd (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Key for computer centers.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/data_cd/tris.md2"
*/
	{
		/* id */ IT_KEY_DATA_CD,
		/* classname */ "key_data_cd",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/data_cd/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_datacd",
		/* use_name */  "Data CD",
		/* pickup_name */  "$item_data_cd",
		/* pickup_name_definite */ "$item_data_cd_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_power_cube (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN NO_TOUCH x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Power Cubes for warehouse.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/power/tris.md2"
*/
	{
		/* id */ IT_KEY_POWER_CUBE,
		/* classname */ "key_power_cube",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/power/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_powercube",
		/* use_name */  "Power Cube",
		/* pickup_name */  "$item_power_cube",
		/* pickup_name_definite */ "$item_power_cube_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_explosive_charges (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN NO_TOUCH x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Explosive Charges - for N64.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/n64/charge/tris.md2"
*/
	{
		/* id */ IT_KEY_EXPLOSIVE_CHARGES,
		/* classname */ "key_explosive_charges",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/n64/charge/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "n64/i_charges",
		/* use_name */  "Explosive Charges",
		/* pickup_name */  "$item_explosive_charges",
		/* pickup_name_definite */ "$item_explosive_charges_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_yellow_key (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Normal door key - Yellow - for N64.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/n64/yellow_key/tris.md2"
*/
	{
		/* id */ IT_KEY_YELLOW,
		/* classname */ "key_yellow_key",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/n64/yellow_key/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "n64/i_yellow_key",
		/* use_name */  "Yellow Key",
		/* pickup_name */  "$item_yellow_key",
		/* pickup_name_definite */ "$item_yellow_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_power_core (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Power Core key - for N64.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/n64/power_core/tris.md2"
*/
	{
		/* id */ IT_KEY_POWER_CORE,
		/* classname */ "key_power_core",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/n64/power_core/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_pyramid",
		/* use_name */  "Power Core",
		/* pickup_name */  "$item_power_core",
		/* pickup_name_definite */ "$item_power_core_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_pyramid (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Key for the entrance of jail3.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/pyramid/tris.md2"
*/
	{
		/* id */ IT_KEY_PYRAMID,
		/* classname */ "key_pyramid",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/pyramid/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_pyramid",
		/* use_name */  "Pyramid Key",
		/* pickup_name */  "$item_pyramid_key",
		/* pickup_name_definite */ "$item_pyramid_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_data_spinner (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Key for the city computer.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/spinner/tris.md2"
*/
	{
		/* id */ IT_KEY_DATA_SPINNER,
		/* classname */ "key_data_spinner",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/spinner/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_dataspin",
		/* use_name */  "Data Spinner",
		/* pickup_name */  "$item_data_spinner",
		/* pickup_name_definite */ "$item_data_spinner_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_pass (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Security pass for the security level.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/pass/tris.md2"
*/
	{
		/* id */ IT_KEY_PASS,
		/* classname */ "key_pass",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/pass/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_security",
		/* use_name */  "Security Pass",
		/* pickup_name */  "$item_security_pass",
		/* pickup_name_definite */ "$item_security_pass_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_blue_key (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Normal door key - Blue.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/key/tris.md2"
*/
	{
		/* id */ IT_KEY_BLUE_KEY,
		/* classname */ "key_blue_key",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/key/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_bluekey",
		/* use_name */  "Blue Key",
		/* pickup_name */  "$item_blue_key",
		/* pickup_name_definite */ "$item_blue_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_red_key (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Normal door key - Red.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/red_key/tris.md2"
*/
	{
		/* id */ IT_KEY_RED_KEY,
		/* classname */ "key_red_key",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/red_key/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_redkey",
		/* use_name */  "Red Key",
		/* pickup_name */  "$item_red_key",
		/* pickup_name_definite */ "$item_red_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_green_key (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Normal door key - Green.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/green_key/tris.md2"
*/
	{
		/* id */ IT_KEY_GREEN_KEY,
		/* classname */ "key_green_key",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/green_key/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_green",
		/* use_name */  "Green Key",
		/* pickup_name */  "$item_green_key",
		/* pickup_name_definite */ "$item_green_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_commander_head (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Key - Tank Commander's Head.
model="models/monsters/commandr/head/tris.md2"
*/
	{
		/* id */ IT_KEY_COMMANDER_HEAD,
		/* classname */ "key_commander_head",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/monsters/commandr/head/tris.md2",
		/* world_model_flags */ EF_GIB,
		/* view_model */ nullptr,
		/* icon */ "k_comhead",
		/* use_name */  "Commander's Head",
		/* pickup_name */  "$item_commanders_head",
		/* pickup_name_definite */ "$item_commanders_head_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_airstrike_target (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Key - Airstrike Target for strike.
model="models/items/keys/target/tris.md2"
*/
	{
		/* id */ IT_KEY_AIRSTRIKE,
		/* classname */ "key_airstrike_target",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/target/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_airstrike",
		/* use_name */  "Airstrike Marker",
		/* pickup_name */  "$item_airstrike_marker",
		/* pickup_name_definite */ "$item_airstrike_marker_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_nuke_container (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_nuke/tris.md2"
*/
	{
		/* id */ IT_KEY_NUKE_CONTAINER,
		/* classname */ "key_nuke_container",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/weapons/g_nuke/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_contain",
		/* use_name */  "Antimatter Pod",
		/* pickup_name */  "$item_antimatter_pod",
		/* pickup_name_definite */ "$item_antimatter_pod_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY,
	},

/*QUAKED key_nuke (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_nuke/tris.md2"
*/
	{
		/* id */ IT_KEY_NUKE,
		/* classname */ "key_nuke",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/weapons/g_nuke/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_nuke",
		/* use_name */  "Antimatter Bomb",
		/* pickup_name */  "$item_antimatter_bomb",
		/* pickup_name_definite */ "$item_antimatter_bomb_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY,
	},

/*QUAKED item_health_small (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Health - Stimpack.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/healing/stimpack/tris.md2"
*/
	// Paril: split the healths up so they are always valid classnames
	{
		/* id */ IT_HEALTH_SMALL,
		/* classname */ "item_health_small",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/s_health.wav",
		/* world_model */ "models/items/healing/stimpack/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "i_health",
		/* use_name */  "Health",
		/* pickup_name */  "$item_stimpack",
		/* pickup_name_definite */ "$item_stimpack_def",
		/* quantity */ 2,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ HEALTH_IGNORE_MAX
	},

/*QUAKED item_health (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Health - First Aid.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/healing/medium/tris.md2"
*/
	{
		/* id */ IT_HEALTH_MEDIUM,
		/* classname */ "item_health",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/n_health.wav",
		/* world_model */ "models/items/healing/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "i_health",
		/* use_name */  "Health",
		/* pickup_name */  "$item_small_medkit",
		/* pickup_name_definite */ "$item_small_medkit_def",
		/* quantity */ 10,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH
	},

/*QUAKED item_health_large (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Health - Medkit.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/healing/large/tris.md2"
*/
	{
		/* id */ IT_HEALTH_LARGE,
		/* classname */ "item_health_large",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/l_health.wav",
		/* world_model */ "models/items/healing/large/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "i_health",
		/* use_name */  "Health",
		/* pickup_name */  "$item_large_medkit",
		/* pickup_name_definite */ "$item_large_medkit",
		/* quantity */ 25,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH
	},

/*QUAKED item_health_mega (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Health - Mega Health.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/mega_h/tris.md2"
*/
	{
		/* id */ IT_HEALTH_MEGA,
		/* classname */ "item_health_mega",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/m_health.wav",
		/* world_model */ "models/items/mega_h/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "p_megahealth",
		/* use_name */  "Mega Health",
		/* pickup_name */  "$item_mega_health",
		/* pickup_name_definite */ "$item_mega_health_def",
		/* quantity */ 100,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ HEALTH_IGNORE_MAX | HEALTH_TIMED
	},

/*QUAKED item_flag_team_red (1 0.2 0) (-16 -16 -24) (16 16 32) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Red Flag for CTF.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="players/male/flag1.md2"
*/
	{
		/* id */ IT_FLAG_RED,
		/* classname */ ITEM_CTF_FLAG_RED,
		/* pickup */ CTF_PickupFlag,
		/* use */ nullptr,
		/* drop */ CTF_DropFlag, //Should this be null if we don't want players to drop it manually?
		/* weaponthink */ nullptr,
		/* pickup_sound */ "ctf/flagtk.wav",
		/* world_model */ "players/male/flag1.md2",
		/* world_model_flags */ EF_FLAG_RED,
		/* view_model */ nullptr,
		/* icon */ "i_ctf1",
		/* use_name */  "Red Flag",
		/* pickup_name */  "$item_red_flag",
		/* pickup_name_definite */ "$item_red_flag_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_NONE,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "ctf/flagcap.wav"
	},

/*QUAKED item_flag_team_blue (1 0.2 0) (-16 -16 -24) (16 16 32) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Blue Flag for CTF.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="players/male/flag2.md2"
*/
	{
		/* id */ IT_FLAG_BLUE,
		/* classname */ ITEM_CTF_FLAG_BLUE,
		/* pickup */ CTF_PickupFlag,
		/* use */ nullptr,
		/* drop */ CTF_DropFlag,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "ctf/flagtk.wav",
		/* world_model */ "players/male/flag2.md2",
		/* world_model_flags */ EF_FLAG_BLUE,
		/* view_model */ nullptr,
		/* icon */ "i_ctf2",
		/* use_name */  "Blue Flag",
		/* pickup_name */  "$item_blue_flag",
		/* pickup_name_definite */ "$item_blue_flag_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_NONE,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "ctf/flagcap.wav"
	},

/* Disruptor Shield Tech */
	{
		/* id */ IT_TECH_DISRUPTOR_SHIELD,
		/* classname */ "item_tech1",
		/* pickup */ Tech_Pickup,
		/* use */ nullptr,
		/* drop */ Tech_Drop,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/ctf/resistance/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "tech1",
		/* use_name */  "Disruptor Shield",
		/* pickup_name */  "$item_disruptor_shield",
		/* pickup_name_definite */ "$item_disruptor_shield_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TECH | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_TECH_DISRUPTOR_SHIELD,
		/* precaches */ "ctf/tech1.wav"
	},

/* Power Amplifier Tech */
	{
		/* id */ IT_TECH_POWER_AMP,
		/* classname */ "item_tech2",
		/* pickup */ Tech_Pickup,
		/* use */ nullptr,
		/* drop */ Tech_Drop,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/ctf/strength/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "tech2",
		/* use_name */  "Power Amplifier",
		/* pickup_name */  "$item_power_amplifier",
		/* pickup_name_definite */ "$item_power_amplifier_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TECH | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_TECH_POWER_AMP,
		/* precaches */ "ctf/tech2.wav ctf/tech2x.wav"
	},

/* Time Accel Tech */
	{
		/* id */ IT_TECH_TIME_ACCEL,
		/* classname */ "item_tech3",
		/* pickup */ Tech_Pickup,
		/* use */ nullptr,
		/* drop */ Tech_Drop,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/ctf/haste/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "tech3",
		/* use_name */  "Time Accel",
		/* pickup_name */  "$item_time_accel",
		/* pickup_name_definite */ "$item_time_accel_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TECH | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_TECH_TIME_ACCEL,
		/* precaches */ "ctf/tech3.wav"
	},

/* AutoDoc Tech */
	{
		/* id */ IT_TECH_AUTODOC,
		/* classname */ "item_tech4",
		/* pickup */ Tech_Pickup,
		/* use */ nullptr,
		/* drop */ Tech_Drop,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/ctf/regeneration/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "tech4",
		/* use_name */  "AutoDoc",
		/* pickup_name */  "$item_autodoc",
		/* pickup_name_definite */ "$item_autodoc_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TECH | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_TECH_AUTODOC,
		/* precaches */ "ctf/tech4.wav"
	},

/*QUAKED ammo_shells_large (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/shells/large/tris.md2"
*/
	{
		/* id */ IT_AMMO_SHELLS_LARGE ,
		/* classname */ "ammo_shells_large",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/vault/items/ammo/shells/large/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_shells",
		/* use_name */  "Large Shells",
		/* pickup_name */  "Large Shells",
		/* pickup_name_definite */ "Large Shells",
		/* quantity */ 20,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_SHELLS
	},

/*QUAKED ammo_shells_small (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/shells/small/tris.md2"
*/
	{
		/* id */ IT_AMMO_SHELLS_SMALL,
		/* classname */ "ammo_shells_small",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/vault/items/ammo/shells/small/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_shells",
		/* use_name */  "Small Shells",
		/* pickup_name */  "Small Shells",
		/* pickup_name_definite */ "Small Shells",
		/* quantity */ 6,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_SHELLS
	},

/*QUAKED ammo_bullets_large (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/bullets/large/tris.md2"
*/
	{
		/* id */ IT_AMMO_BULLETS_LARGE,
		/* classname */ "ammo_bullets_large",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/vault/items/ammo/bullets/large/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_bullets",
		/* use_name */  "Large Bullets",
		/* pickup_name */  "Large Bullets",
		/* pickup_name_definite */ "Large Bullets",
		/* quantity */ 100,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_BULLETS
	},

/*QUAKED ammo_bullets_small (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/bullets/small/tris.md2"
*/
	{
		/* id */ IT_AMMO_BULLETS_SMALL,
		/* classname */ "ammo_bullets_small",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/vault/items/ammo/bullets/small/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_bullets",
		/* use_name */  "Small Bullets",
		/* pickup_name */  "Small Bullets",
		/* pickup_name_definite */ "Small Bullets",
		/* quantity */ 16,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_BULLETS
	},

/*QUAKED ammo_cells_large (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/cells/large/tris.md2"
*/
	{
		/* id */ IT_AMMO_CELLS_LARGE,
		/* classname */ "ammo_cells_large",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/vault/items/ammo/cells/large/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_cells",
		/* use_name */  "Large Cells",
		/* pickup_name */  "Large Cells",
		/* pickup_name_definite */ "Large Cells",
		/* quantity */ 100,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_CELLS
	},

/*QUAKED ammo_cells_small (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/cells/small/tris.md2"
*/
	{
		/* id */ IT_AMMO_CELLS_SMALL,
		/* classname */ "ammo_cells_small",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/vault/items/ammo/cells/small/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_cells",
		/* use_name */  "Small Cells",
		/* pickup_name */  "Small Cells",
		/* pickup_name_definite */ "Small Cells",
		/* quantity */ 20,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_CELLS
	},

/*QUAKED ammo_rockets_small (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/rockets/small/tris.md2"
*/
	{
		/* id */ IT_AMMO_ROCKETS_SMALL,
		/* classname */ "ammo_rockets_small",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/vault/items/ammo/rockets/small/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_rockets",
		/* use_name */  "Small Rockets",
		/* pickup_name */  "Small Rockets",
		/* pickup_name_definite */ "Small Rockets",
		/* quantity */ 2,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_ROCKETS
	},

/*QUAKED ammo_slugs_large (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/slugs/large/tris.md2"
*/
	{
		/* id */ IT_AMMO_SLUGS_LARGE,
		/* classname */ "ammo_slugs_large",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/vault/items/ammo/slugs/large/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_slugs",
		/* use_name */  "Large Slugs",
		/* pickup_name */  "Large Slugs",
		/* pickup_name_definite */ "Large Slugs",
		/* quantity */ 20,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_SLUGS
	},

/*QUAKED ammo_slugs_small (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/slugs/small/tris.md2"
*/
	{
		/* id */ IT_AMMO_SLUGS_SMALL,
		/* classname */ "ammo_slugs_small",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/vault/items/ammo/slugs/small/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_slugs",
		/* use_name */  "Small Slugs",
		/* pickup_name */  "Small Slugs",
		/* pickup_name_definite */ "Small Slugs",
		/* quantity */ 3,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_SLUGS
	},

/*QUAKED item_teleporter (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/nuke/tris.md2"
*/
	{
		/* id */ IT_TELEPORTER,
		/* classname */ "item_teleporter",
		/* pickup */ Pickup_Teleporter,
		/* use */ Use_Teleporter,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/vault/items/ammo/nuke/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_fixme",
		/* use_name */  "Personal Teleporter",
		/* pickup_name */  "Personal Teleporter",
		/* pickup_name_definite */ "Personal Teleporter",
		/* quantity */ 120,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF
	},

/*QUAKED item_regen (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/invulner/tris.md2"
*/
	{
		/* id */ IT_POWERUP_REGEN,
		/* classname */ "item_regen",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Regeneration,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/invulner/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_fixme",
		/* use_name */  "Regeneration",
		/* pickup_name */  "Regeneration",
		/* pickup_name_definite */ "Regeneration",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_REGEN,
		/* precaches */ "items/protect.wav"
	},

/*QUAKED item_foodcube (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Meaty cube o' health
model="models/objects/trapfx/tris.md2"
*/
	{
		/* id */ IT_FOODCUBE,
		/* classname */ "item_foodcube",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/n_health.wav",
		/* world_model */ "models/objects/trapfx/tris.md2",
		/* world_model_flags */ EF_GIB,
		/* view_model */ nullptr,
		/* icon */ "i_health",
		/* use_name */  "Meaty Cube",
		/* pickup_name */  "Meaty Cube",
		/* pickup_name_definite */ "Meaty Cube",
		/* quantity */ 50,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ HEALTH_IGNORE_MAX
	},

/*QUAKED item_ball (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Big ol' ball
models/items/ammo/grenades/medium/tris.md2"
*/
	{
		/* id */ IT_BALL,
		/* classname */ "item_ball",
		/* pickup */ Pickup_Ball,
		/* use */ Use_Ball,
		/* drop */ Drop_Ball,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/ammo/grenades/medium/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_help",
		/* use_name */  "Ball",
		/* pickup_name */  "Ball",
		/* pickup_name_definite */ "Ball",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_POWERUP| IF_POWERUP_WHEEL | IF_NOT_RANDOM,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_BALL,
		/* precaches */ "",
		/* sort_id */ -1
	},
	
/* Flashlight */
	{
		/* id */ IT_FLASHLIGHT,
		/* classname */ "item_flashlight",
		/* pickup */ Pickup_General,
		/* use */ Use_Flashlight,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/flashlight/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_torch",
		/* use_name */  "Flashlight",
		/* pickup_name */  "$item_flashlight",
		/* pickup_name_definite */ "$item_flashlight_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF | IF_NOT_RANDOM,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_FLASHLIGHT,
		/* precaches */ "items/flashlight_on.wav items/flashlight_off.wav",
		/* sort_id */ -1
	},

/* Compass */
	{
		/* id */ IT_COMPASS,
		/* classname */ "item_compass",
		/* pickup */ nullptr,
		/* use */ Use_Compass,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ nullptr,
		/* world_model */ nullptr,
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "p_compass",
		/* use_name */  "Compass",
		/* pickup_name */  "$item_compass",
		/* pickup_name_definite */ "$item_compass_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_COMPASS,
		/* precaches */ "misc/help_marker.wav",
		/* sort_id */ -2
	},
};
// clang-format on
