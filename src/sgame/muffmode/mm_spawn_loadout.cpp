// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_freezetag_rules.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_ruleset_weapons.h"
#include "muffmode/mm_spawn_loadout.h"

#include <algorithm>
#include <array>
#include <initializer_list>

namespace muffmode::spawn_loadout {

struct AmmoValue {
	int index = 0;
	int value = 0;
};

struct InventoryGrant {
	item_id_t item_id = IT_NULL;
	int32_t amount = 0;
};

enum class QcStartingWeapon {
	Shotgun,
	Machinegun,
	HyperBlaster,
};

item_id_t StartingArmorItemForAmount(int armor)
{
	if (armor > combatarmor_info.max_count)
		return IT_ARMOR_BODY;
	if (armor > jacketarmor_info.max_count)
		return IT_ARMOR_COMBAT;
	return IT_ARMOR_JACKET;
}

void ApplyAmmoCaps(gclient_t *client, int fill_value, std::initializer_list<AmmoValue> overrides)
{
	client->pers.max_ammo.fill(static_cast<int16_t>(fill_value));

	for (const auto &entry : overrides)
		client->pers.max_ammo[entry.index] = static_cast<int16_t>(entry.value);
}

void GrantInventory(gclient_t *client, std::initializer_list<InventoryGrant> grants)
{
	for (const auto &grant : grants)
		client->pers.inventory[grant.item_id] = grant.amount;
}

void ApplyArenaAmmo(gclient_t *client)
{
	ApplyAmmoCaps(client, 50, {
		{ AMMO_SHELLS, 50 },
		{ AMMO_BULLETS, 300 },
		{ AMMO_GRENADES, 50 },
		{ AMMO_ROCKETS, 50 },
		{ AMMO_CELLS, 200 },
		{ AMMO_SLUGS, 25 },
	});

	if (RS(RS_Q1)) {
		ApplyAmmoCaps(client, 50, {
			{ AMMO_SHELLS, 100 },
			{ AMMO_BULLETS, 300 },
			{ AMMO_GRENADES, 100 },
			{ AMMO_ROCKETS, 100 },
			{ AMMO_CELLS, 100 },
			{ AMMO_SLUGS, 25 },
		});
	}
}

void ApplyNonArenaAmmoCaps(gclient_t *client)
{
	if (RS(RS_Q3A)) {
		ApplyAmmoCaps(client, 200, {});
	} else if (RS(RS_Q1)) {
		ApplyAmmoCaps(client, 200, {
			{ AMMO_BULLETS, 200 },
			{ AMMO_SHELLS, 100 },
			{ AMMO_GRENADES, 100 },
			{ AMMO_ROCKETS, 100 },
			{ AMMO_CELLS, 100 },
			{ AMMO_TRAP, 200 },
			{ AMMO_FLECHETTES, 200 },
			{ AMMO_DISRUPTOR, 200 },
			{ AMMO_TESLA, 200 },
		});
	} else {
		ApplyAmmoCaps(client, 50, {
			{ AMMO_BULLETS, 200 },
			{ AMMO_SHELLS, 100 },
			{ AMMO_CELLS, 200 },
			{ AMMO_TRAP, 5 },
			{ AMMO_FLECHETTES, 200 },
			{ AMMO_DISRUPTOR, 12 },
			{ AMMO_TESLA, 5 },
		});
	}
}

void ApplyQcStartingWeapon(gclient_t *client)
{
	switch (static_cast<QcStartingWeapon>(irandom(3))) {
	case QcStartingWeapon::Shotgun:
		GrantInventory(client, { { IT_WEAPON_SHOTGUN, 1 }, { IT_AMMO_SHELLS, 50 } });
		break;
	case QcStartingWeapon::Machinegun:
		GrantInventory(client, { { IT_WEAPON_MACHINEGUN, 1 }, { IT_AMMO_BULLETS, 200 } });
		break;
	case QcStartingWeapon::HyperBlaster:
		GrantInventory(client, { { IT_WEAPON_HYPERBLASTER, 1 }, { IT_AMMO_CELLS, 200 } });
		break;
	}
}

void ApplyResolvedArenaLoadout(gentity_t *ent, gclient_t *client,
	const mm_arena_loadout_t &loadout)
{
	// RA3's weapon digit 0 is a normal selectable grapple. Keep MuffMode's
	// off-hand command as a convenience while preserving selectable behavior.
	client->pers.inventory[IT_WEAPON_GRAPPLE] =
		MM_Arena_GrappleEnabled(ent) ? 1 : 0;
	const std::array<item_id_t, 9> weapons = {
		IT_WEAPON_CHAINFIST, IT_WEAPON_MACHINEGUN, IT_WEAPON_SHOTGUN,
		IT_WEAPON_GLAUNCHER, IT_WEAPON_RLAUNCHER, IT_WEAPON_PLASMABEAM,
		IT_WEAPON_RAILGUN, IT_WEAPON_HYPERBLASTER, IT_WEAPON_BFG
	};
	bool granted_weapon = false;
	for (size_t i = 0; i < weapons.size(); i++)
		if ((client->pers.inventory[weapons[i]] =
			(loadout.weapon_mask & (uint32_t { 1 } << i)) ? 1 : 0))
			granted_weapon = true;
	// Keep a safe no-ammo fallback for an explicitly empty mask.
	if (!granted_weapon)
		client->pers.inventory[IT_WEAPON_CHAINFIST] = 1;

	const auto amount = [&loadout](int value) {
		return loadout.infinite_ammo ? AMMO_INFINITE :
			std::clamp(value, 0, 999);
	};
	const auto grant_ammo = [client, &amount](item_id_t item,
		int ammo_index, int value) {
		const int resolved = amount(value);
		client->pers.inventory[item] = resolved;
		client->pers.max_ammo[ammo_index] =
			static_cast<int16_t>(resolved);
	};
	client->pers.max_ammo.fill(0);
	grant_ammo(IT_AMMO_SHELLS, AMMO_SHELLS, loadout.shells);
	grant_ammo(IT_AMMO_BULLETS, AMMO_BULLETS, loadout.bullets);
	grant_ammo(IT_AMMO_GRENADES, AMMO_GRENADES, loadout.grenades);
	grant_ammo(IT_AMMO_ROCKETS, AMMO_ROCKETS, loadout.rockets);
	grant_ammo(IT_AMMO_CELLS, AMMO_CELLS, loadout.cells);
	grant_ammo(IT_AMMO_SLUGS, AMMO_SLUGS, loadout.slugs);
}

bool UsesArenaLoadout()
{
	const bool freeze_tag_arena_loadout = g_freezetag_arena_loadout && g_freezetag_arena_loadout->integer != 0;

	return MM_FreezeTagUsesArenaLoadout(
		MM_GametypeHasFlag(GTF_ARENA),
		GT(GT_FREEZE),
		freeze_tag_arena_loadout);
}

} // namespace muffmode::spawn_loadout

bool MM_UsesArenaSpawnLoadout()
{
	return muffmode::spawn_loadout::UsesArenaLoadout();
}

void MM_ApplyStartingHealthArmor(gentity_t *ent, gclient_t *client)
{
	if (!ent || !client || ent->client != client)
		return;

	int health = 0;
	int armor = 0;
	const bool arena = muffmode::spawn_loadout::UsesArenaLoadout();
	mm_arena_loadout_t arena_loadout;
	const bool resolved_arena =
		MM_Arena_GetSpawnLoadout(client, arena_loadout);

	if (resolved_arena) {
		health = arena_loadout.health;
		armor = arena_loadout.armor;
	} else if (arena) {
		health = clamp(g_arena_start_health->integer, 1, 9999);
		armor = clamp(g_arena_start_armor->integer, 0, 999);
	} else {
		health = clamp(g_starting_health->integer, 1, 9999);
		armor = clamp(g_starting_armor->integer, 0, 999);
	}

	client->pers.health = client->pers.max_health = health;

	const int bonus = RS(RS_Q3A) ? 25 : g_starting_health_bonus->integer;
	if (!arena && bonus > 0) {
		client->pers.health += bonus;
		if (!(RS(RS_Q3A))) {
			client->pers.health_bonus = bonus;
			client->pers.health_bonus_timer = level.time + 1_sec;
		}
		client->time_residual = level.time;
	}

	if (resolved_arena)
		client->pers.inventory[IT_ARMOR_BODY] = armor;
	else
		client->pers.inventory[
			muffmode::spawn_loadout::StartingArmorItemForAmount(armor)] = armor;
}

void MM_ApplySpawnLoadout(gentity_t *ent, gclient_t *client, bool taken_loadout)
{
	if (!ent || !client || ent->client != client)
		return;

	if (!taken_loadout) {
		const bool arena = muffmode::spawn_loadout::UsesArenaLoadout();
		mm_arena_loadout_t arena_loadout;
		const bool resolved_arena =
			MM_Arena_GetSpawnLoadout(client, arena_loadout);

		if (notGT(GT_ARENA) && (g_instagib->integer || GT(GT_INSTAGIB))) {
			client->pers.inventory[IT_WEAPON_RAILGUN] = 1;
			client->pers.inventory[IT_AMMO_SLUGS] = AMMO_INFINITE;
		} else if (notGT(GT_ARENA) && (g_nadefest->integer || GT(GT_NADEFEST))) {
			client->pers.inventory[IT_AMMO_GRENADES] = AMMO_INFINITE;
		} else if (resolved_arena) {
			muffmode::spawn_loadout::ApplyResolvedArenaLoadout(
				ent, client, arena_loadout);
		} else if (arena) {
			muffmode::spawn_loadout::ApplyArenaAmmo(client);
			muffmode::spawn_loadout::GrantInventory(client, {
				{ IT_AMMO_SHELLS, 50 },
				{ IT_AMMO_ROCKETS, 50 },
				{ IT_AMMO_CELLS, RS(RS_Q1) ? 100 : 200 },
			});

			if (!(RS(RS_Q1))) {
				muffmode::spawn_loadout::GrantInventory(client, {
					{ IT_AMMO_BULLETS, 200 },
					{ IT_AMMO_GRENADES, 50 },
					{ IT_AMMO_SLUGS, 50 },
				});
			}

			if (RS(RS_Q3A)) {
				muffmode::spawn_loadout::GrantInventory(client, {
					{ IT_WEAPON_CHAINFIST, 1 },
					{ IT_WEAPON_SHOTGUN, 1 },
				});
			} else {
				muffmode::spawn_loadout::GrantInventory(client, {
					{ IT_WEAPON_BLASTER, 1 },
					{ IT_WEAPON_SHOTGUN, 1 },
					{ IT_WEAPON_SSHOTGUN, 1 },
				});
			}
			if (!(RS(RS_Q1))) {
				muffmode::spawn_loadout::GrantInventory(client, {
					{ IT_WEAPON_MACHINEGUN, 1 },
					{ IT_WEAPON_CHAINGUN, 1 },
				});
			}
			muffmode::spawn_loadout::GrantInventory(client, {
				{ IT_WEAPON_GLAUNCHER, 1 },
				{ IT_WEAPON_RLAUNCHER, 1 },
				{ IT_WEAPON_HYPERBLASTER, 1 },
			});
			if (!(RS(RS_Q1)))
				muffmode::spawn_loadout::GrantInventory(client, { { IT_WEAPON_RAILGUN, 1 } });
		} else {
			muffmode::spawn_loadout::ApplyNonArenaAmmoCaps(client);

			if (RS(RS_Q3A)) {
				muffmode::spawn_loadout::GrantInventory(client, {
					{ IT_WEAPON_CHAINFIST, 1 },
					{ IT_WEAPON_MACHINEGUN, 1 },
					{ IT_AMMO_BULLETS, GT(GT_TDM) ? 50 : 100 },
				});
			} else if (RS(RS_Q1)) {
				muffmode::spawn_loadout::GrantInventory(client, {
					{ IT_WEAPON_CHAINFIST, 1 },
					{ IT_WEAPON_SHOTGUN, 1 },
					{ IT_AMMO_SHELLS, 10 },
				});
			} else if (RS(RS_QC)) {
				muffmode::spawn_loadout::ApplyQcStartingWeapon(client);
			} else {
				// Horde can start players on the chainfist instead of the blaster.
				if (GT(GT_HORDE) && g_horde_start_chainsaw->integer)
					muffmode::spawn_loadout::GrantInventory(client, { { IT_WEAPON_CHAINFIST, 1 } });
				else
					muffmode::spawn_loadout::GrantInventory(client, { { IT_WEAPON_BLASTER, 1 } });
			}

			if (deathmatch->integer && game.ruleset != RS_QC) {
				if (level.match_state < matchst_t::MATCH_IN_PROGRESS) {
					for (int i = FIRST_WEAPON; i < LAST_WEAPON; i++) {
						if (!level.weapon_count[i - FIRST_WEAPON])
							continue;

						const item_id_t ammo_id = MM_Ruleset_WeaponAmmoId(&itemlist[i]);
						if (!ammo_id)
							continue;

						client->pers.inventory[i] = 1;

						gitem_t *ammo = GetItemByIndex(ammo_id);
						if (ammo)
							Add_Ammo(ent, ammo,
								(InfiniteAmmoOn(ammo) || MM_Arena_InfiniteAmmoEnabled(ent))
									? AMMO_INFINITE
									: ammo->quantity * 2);
					}
				}
			}
		}
	}
}
