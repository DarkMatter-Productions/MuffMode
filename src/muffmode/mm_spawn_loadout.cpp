// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_spawn_loadout.h"

void MM_ApplyStartingHealthArmor(gentity_t *ent, gclient_t *client)
{
	int health, armor;
	gitem_armor_t armor_type = jacketarmor_info;

	if (GTF(GTF_ARENA)) {
		health = clamp(g_arena_start_health->integer, 1, 9999);
		armor = clamp(g_arena_start_armor->integer, 0, 999);
	} else {
		health = clamp(g_starting_health->integer, 1, 9999);
		armor = clamp(g_starting_armor->integer, 0, 999);
	}

	if (armor > jacketarmor_info.max_count)
		if (armor > combatarmor_info.max_count)
			armor_type = bodyarmor_info;
		else
			armor_type = combatarmor_info;

	client->pers.health = client->pers.max_health = health;

	int bonus = RS(RS_Q3A) ? 25 : g_starting_health_bonus->integer;
	if (!(GTF(GTF_ARENA)) && bonus > 0) {
		client->pers.health += bonus;
		if (!(RS(RS_Q3A))) {
			client->pers.health_bonus = bonus;
			ent->client->pers.health_bonus_timer = level.time + 1_sec;
		}
		ent->client->time_residual = level.time;
	}

	if (armor_type.base_count == jacketarmor_info.base_count)
		client->pers.inventory[IT_ARMOR_JACKET] = armor;
	else if (armor_type.base_count == combatarmor_info.base_count)
		client->pers.inventory[IT_ARMOR_COMBAT] = armor;
	else if (armor_type.base_count == bodyarmor_info.base_count)
		client->pers.inventory[IT_ARMOR_BODY] = armor;
}

void MM_ApplySpawnLoadout(gentity_t *ent, gclient_t *client, bool taken_loadout)
{
	if (!taken_loadout) {
		if (g_instagib->integer || GT(GT_INSTAGIB)) {
			client->pers.inventory[IT_WEAPON_RAILGUN] = 1;
			client->pers.inventory[IT_AMMO_SLUGS] = AMMO_INFINITE;
		} else if (g_nadefest->integer || GT(GT_NADEFEST)) {
			client->pers.inventory[IT_AMMO_GRENADES] = AMMO_INFINITE;
		} else if (GTF(GTF_ARENA)) {
			client->pers.max_ammo.fill(50);
			client->pers.max_ammo[AMMO_SHELLS] = 50;
			client->pers.max_ammo[AMMO_BULLETS] = 300;
			client->pers.max_ammo[AMMO_GRENADES] = 50;
			client->pers.max_ammo[AMMO_ROCKETS] = 50;
			client->pers.max_ammo[AMMO_CELLS] = 200;
			client->pers.max_ammo[AMMO_SLUGS] = 25;

			client->pers.inventory[IT_AMMO_SHELLS] = 50;
			if (!(RS(RS_Q1))) {
				client->pers.inventory[IT_AMMO_BULLETS] = 200;
				client->pers.inventory[IT_AMMO_GRENADES] = 50;
			}
			client->pers.inventory[IT_AMMO_ROCKETS] = 50;
			client->pers.inventory[IT_AMMO_CELLS] = 200;
			if (!(RS(RS_Q1)))
				client->pers.inventory[IT_AMMO_SLUGS] = 50;

			client->pers.inventory[IT_WEAPON_BLASTER] = 1;
			client->pers.inventory[IT_WEAPON_SHOTGUN] = 1;
			if (!(RS(RS_Q3A)))
				client->pers.inventory[IT_WEAPON_SSHOTGUN] = 1;
			if (!(RS(RS_Q1))) {
				client->pers.inventory[IT_WEAPON_MACHINEGUN] = 1;
				client->pers.inventory[IT_WEAPON_CHAINGUN] = 1;
			}
			client->pers.inventory[IT_WEAPON_GLAUNCHER] = 1;
			client->pers.inventory[IT_WEAPON_RLAUNCHER] = 1;
			client->pers.inventory[IT_WEAPON_HYPERBLASTER] = 1;
			if (!(RS(RS_Q1)))
				client->pers.inventory[IT_WEAPON_RAILGUN] = 1;
		} else {
			if (RS(RS_Q3A)) {
				client->pers.max_ammo.fill(200);
				client->pers.max_ammo[AMMO_BULLETS] = 200;
				client->pers.max_ammo[AMMO_SHELLS] = 200;
				client->pers.max_ammo[AMMO_CELLS] = 200;

				client->pers.max_ammo[AMMO_TRAP] = 200;
				client->pers.max_ammo[AMMO_FLECHETTES] = 200;
				client->pers.max_ammo[AMMO_DISRUPTOR] = 200;
				client->pers.max_ammo[AMMO_TESLA] = 200;

				client->pers.inventory[IT_WEAPON_CHAINFIST] = 1;
				client->pers.inventory[IT_WEAPON_MACHINEGUN] = 1;
				client->pers.inventory[IT_AMMO_BULLETS] = (GT(GT_TDM)) ? 50 : 100;
			} else if (RS(RS_Q1)) {
				client->pers.max_ammo.fill(200);
				client->pers.max_ammo[AMMO_BULLETS] = 200;
				client->pers.max_ammo[AMMO_SHELLS] = 200;
				client->pers.max_ammo[AMMO_CELLS] = 200;

				client->pers.max_ammo[AMMO_TRAP] = 200;
				client->pers.max_ammo[AMMO_FLECHETTES] = 200;
				client->pers.max_ammo[AMMO_DISRUPTOR] = 200;
				client->pers.max_ammo[AMMO_TESLA] = 200;

				client->pers.inventory[IT_WEAPON_CHAINFIST] = 1;
				client->pers.inventory[IT_WEAPON_SHOTGUN] = 1;
				client->pers.inventory[IT_AMMO_SHELLS] = 10;
			} else if (RS(RS_QC)) {
				client->pers.max_ammo.fill(50);
				client->pers.max_ammo[AMMO_BULLETS] = 200;
				client->pers.max_ammo[AMMO_SHELLS] = 100;
				client->pers.max_ammo[AMMO_CELLS] = 200;

				client->pers.max_ammo[AMMO_TRAP] = 5;
				client->pers.max_ammo[AMMO_FLECHETTES] = 200;
				client->pers.max_ammo[AMMO_DISRUPTOR] = 12;
				client->pers.max_ammo[AMMO_TESLA] = 5;

				int weapon_choice = irandom(3);
				if (weapon_choice == 0) {
					client->pers.inventory[IT_WEAPON_SHOTGUN] = 1;
					client->pers.inventory[IT_AMMO_SHELLS] = 50;
				} else if (weapon_choice == 1) {
					client->pers.inventory[IT_WEAPON_MACHINEGUN] = 1;
					client->pers.inventory[IT_AMMO_BULLETS] = 200;
				} else {
					client->pers.inventory[IT_WEAPON_HYPERBLASTER] = 1;
					client->pers.inventory[IT_AMMO_CELLS] = 200;
				}
			} else {
				client->pers.max_ammo.fill(50);
				client->pers.max_ammo[AMMO_BULLETS] = 200;
				client->pers.max_ammo[AMMO_SHELLS] = 100;
				client->pers.max_ammo[AMMO_CELLS] = 200;

				client->pers.max_ammo[AMMO_TRAP] = 5;
				client->pers.max_ammo[AMMO_FLECHETTES] = 200;
				client->pers.max_ammo[AMMO_DISRUPTOR] = 12;
				client->pers.max_ammo[AMMO_TESLA] = 5;

				client->pers.inventory[IT_WEAPON_BLASTER] = 1;
			}

			if (deathmatch->integer && game.ruleset != RS_QC) {
				if (level.match_state < matchst_t::MATCH_IN_PROGRESS) {
					for (size_t i = FIRST_WEAPON; i < LAST_WEAPON; i++) {
						if (!level.weapon_count[i - FIRST_WEAPON])
							continue;

						if (!itemlist[i].ammo)
							continue;

						client->pers.inventory[i] = 1;

						gitem_t *ammo = GetItemByIndex(itemlist[i].ammo);
						if (ammo)
							Add_Ammo(&g_entities[client - game.clients + 1], ammo, InfiniteAmmoOn(ammo) ? AMMO_INFINITE : ammo->quantity * 2);
					}
				}
			}
		}
	}
}
