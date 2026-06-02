// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "g_debug_log.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_spawn_filter.h"
#include "muffmode/mm_statusbar.h"

struct spawn_t {
	const char *name;
	void (*spawn)(gentity_t *ent);
};

void SP_info_player_start(gentity_t *ent);
void SP_info_player_deathmatch(gentity_t *ent);
void SP_info_player_team_red(gentity_t *self);
void SP_info_player_team_blue(gentity_t *self);
void SP_info_player_coop(gentity_t *ent);
void SP_info_player_coop_lava(gentity_t *self);
void SP_info_player_intermission(gentity_t *ent);
void SP_info_teleport_destination(gentity_t *self);
void SP_info_ctf_teleport_destination(gentity_t *self);
void SP_info_landmark(gentity_t *self); // [Paril-KEX]
void SP_info_world_text(gentity_t *self);
void SP_info_nav_lock(gentity_t *self); // [Paril-KEX]

void SP_func_plat(gentity_t *ent);
void SP_func_plat2(gentity_t *ent);
void SP_func_rotating(gentity_t *ent);
void SP_func_button(gentity_t *ent);
void SP_func_door(gentity_t *ent);
void SP_func_door_secret(gentity_t *ent);
void SP_func_door_secret2(gentity_t *ent);
void SP_func_door_rotating(gentity_t *ent);
void SP_func_water(gentity_t *ent);
void SP_func_train(gentity_t *ent);
void SP_func_conveyor(gentity_t *self);
void SP_func_wall(gentity_t *self);
void SP_func_force_wall(gentity_t *ent);
void SP_func_object(gentity_t *self);
void SP_func_explosive(gentity_t *self);
void SP_func_timer(gentity_t *self);
void SP_func_areaportal(gentity_t *ent);
void SP_func_clock(gentity_t *ent);
void SP_func_killbox(gentity_t *ent);
void SP_func_eye(gentity_t *ent); // [Paril-KEX]
void SP_func_animation(gentity_t *ent); // [Paril-KEX]
void SP_func_spinning(gentity_t *ent); // [Paril-KEX]
void SP_object_repair(gentity_t *self);

void SP_trigger_always(gentity_t *ent);
void SP_trigger_once(gentity_t *ent);
void SP_trigger_multiple(gentity_t *ent);
void SP_trigger_relay(gentity_t *ent);
void SP_trigger_push(gentity_t *ent);
void SP_trigger_hurt(gentity_t *ent);
void SP_trigger_key(gentity_t *ent);
void SP_trigger_counter(gentity_t *ent);
void SP_trigger_elevator(gentity_t *ent);
void SP_trigger_gravity(gentity_t *ent);
void SP_trigger_monsterjump(gentity_t *ent);
void SP_trigger_flashlight(gentity_t *self); // [Paril-KEX]
void SP_trigger_fog(gentity_t *self); // [Paril-KEX]
void SP_trigger_coop_relay(gentity_t *self); // [Paril-KEX]
void SP_trigger_health_relay(gentity_t *self); // [Paril-KEX]
void SP_trigger_teleport(gentity_t *self);
void SP_trigger_ctf_teleport(gentity_t *self);
void SP_trigger_disguise(gentity_t *self);

void SP_trigger_deathcount(gentity_t *ent);	//mm
void SP_trigger_no_monsters(gentity_t *ent);	//mm
void SP_trigger_monsters(gentity_t *ent);	//mm

void SP_target_temp_entity(gentity_t *ent);
void SP_target_speaker(gentity_t *ent);
void SP_target_explosion(gentity_t *ent);
void SP_target_changelevel(gentity_t *ent);
void SP_target_secret(gentity_t *ent);
void SP_target_goal(gentity_t *ent);
void SP_target_splash(gentity_t *ent);
void SP_target_spawner(gentity_t *ent);
void SP_target_blaster(gentity_t *ent);
void SP_target_crosslevel_trigger(gentity_t *ent);
void SP_target_crosslevel_target(gentity_t *ent);
void SP_target_crossunit_trigger(gentity_t *ent); // [Paril-KEX]
void SP_target_crossunit_target(gentity_t *ent); // [Paril-KEX]
void SP_target_laser(gentity_t *self);
void SP_target_help(gentity_t *ent);
void SP_target_actor(gentity_t *ent);
void SP_target_lightramp(gentity_t *self);
void SP_target_earthquake(gentity_t *ent);
void SP_target_character(gentity_t *ent);
void SP_target_string(gentity_t *ent);
void SP_target_camera(gentity_t *self); // [Sam-KEX]
void SP_target_gravity(gentity_t *self); // [Sam-KEX]
void SP_target_soundfx(gentity_t *self); // [Sam-KEX]
void SP_target_light(gentity_t *self); // [Paril-KEX]
void SP_target_poi(gentity_t *ent); // [Paril-KEX]
void SP_target_music(gentity_t *ent);
void SP_target_healthbar(gentity_t *self); // [Paril-KEX]
void SP_target_autosave(gentity_t *self); // [Paril-KEX]
void SP_target_sky(gentity_t *self); // [Paril-KEX]
void SP_target_achievement(gentity_t *self); // [Paril-KEX]
void SP_target_story(gentity_t *self); // [Paril-KEX]
void SP_target_mal_laser(gentity_t *ent);
void SP_target_steam(gentity_t *self);
void SP_target_anger(gentity_t *self);
void SP_target_killplayers(gentity_t *self);
// PMM - still experimental!
void SP_target_blacklight(gentity_t *self);
void SP_target_orb(gentity_t *self);
// pmm
void SP_target_remove_powerups(gentity_t *ent);	//q3
void SP_target_give(gentity_t *ent);	//q3
void SP_target_delay(gentity_t *ent);	//q3
void SP_target_print(gentity_t *ent);	//q3
void SP_target_teleporter(gentity_t *ent);	//q3
void SP_target_kill(gentity_t *self);	//q3
void SP_target_cvar(gentity_t *ent);	//ql
void SP_target_setskill(gentity_t *ent);
void SP_target_score(gentity_t *ent);	//q3
void SP_target_remove_weapons(gentity_t *ent);

void SP_target_shooter_grenade(gentity_t *ent);
void SP_target_shooter_rocket(gentity_t *ent);
void SP_target_shooter_bfg(gentity_t *ent);
void SP_target_shooter_prox(gentity_t *ent);
void SP_target_shooter_ionripper(gentity_t *ent);
void SP_target_shooter_phalanx(gentity_t *ent);
void SP_target_shooter_flechette(gentity_t *ent);

void SP_target_push(gentity_t *ent);

void SP_worldspawn(gentity_t *ent);

void SP_dynamic_light(gentity_t *self);
void SP_rotating_light(gentity_t *self);
void SP_light(gentity_t *self);
void SP_light_mine1(gentity_t *ent);
void SP_light_mine2(gentity_t *ent);
void SP_info_null(gentity_t *self);
void SP_info_notnull(gentity_t *self);
void SP_misc_player_mannequin(gentity_t *self);
void SP_misc_model(gentity_t *self); // [Paril-KEX]
void SP_path_corner(gentity_t *self);
void SP_point_combat(gentity_t *self);

void SP_misc_explobox(gentity_t *self);
void SP_misc_banner(gentity_t *self);
void SP_misc_ctf_banner(gentity_t *ent);
void SP_misc_ctf_small_banner(gentity_t *ent);
void SP_misc_satellite_dish(gentity_t *self);
void SP_misc_actor(gentity_t *self);
void SP_misc_gib_arm(gentity_t *self);
void SP_misc_gib_leg(gentity_t *self);
void SP_misc_gib_head(gentity_t *self);
void SP_misc_insane(gentity_t *self);
void SP_misc_deadsoldier(gentity_t *self);
void SP_misc_viper(gentity_t *self);
void SP_misc_viper_bomb(gentity_t *self);
void SP_misc_bigviper(gentity_t *self);
void SP_misc_strogg_ship(gentity_t *self);
void SP_misc_teleporter(gentity_t *self);
void SP_misc_teleporter_dest(gentity_t *self);
void SP_misc_blackhole(gentity_t *self);
void SP_misc_eastertank(gentity_t *self);
void SP_misc_easterchick(gentity_t *self);
void SP_misc_easterchick2(gentity_t *self);
void SP_misc_crashviper(gentity_t *ent);
void SP_misc_viper_missile(gentity_t *self);
void SP_misc_amb4(gentity_t *ent);
void SP_misc_transport(gentity_t *ent);
void SP_misc_nuke(gentity_t *ent);
void SP_misc_flare(gentity_t *ent); // [Sam-KEX]
void SP_misc_hologram(gentity_t *ent);
void SP_misc_lavaball(gentity_t *ent);
void SP_misc_nuke_core(gentity_t *self);

void SP_monster_berserk(gentity_t *self);
void SP_monster_gladiator(gentity_t *self);
void SP_monster_gunner(gentity_t *self);
void SP_monster_infantry(gentity_t *self);
void SP_monster_soldier_light(gentity_t *self);
void SP_monster_soldier(gentity_t *self);
void SP_monster_soldier_ss(gentity_t *self);
void SP_monster_tank(gentity_t *self);
void SP_monster_medic(gentity_t *self);
void SP_monster_flipper(gentity_t *self);
void SP_monster_chick(gentity_t *self);
void SP_monster_parasite(gentity_t *self);
void SP_monster_flyer(gentity_t *self);
void SP_monster_brain(gentity_t *self);
void SP_monster_floater(gentity_t *self);
void SP_monster_hover(gentity_t *self);
void SP_monster_mutant(gentity_t *self);
void SP_monster_supertank(gentity_t *self);
void SP_monster_boss2(gentity_t *self);
void SP_monster_jorg(gentity_t *self);
void SP_monster_boss3_stand(gentity_t *self);
void SP_monster_makron(gentity_t *self);

void SP_monster_tank_stand(gentity_t *self);
void SP_monster_guardian(gentity_t *self);
void SP_monster_arachnid(gentity_t *self);
void SP_monster_guncmdr(gentity_t *self);

void SP_monster_commander_body(gentity_t *self);

void SP_turret_breach(gentity_t *self);
void SP_turret_base(gentity_t *self);
void SP_turret_driver(gentity_t *self);

void SP_monster_soldier_hypergun(gentity_t *self);
void SP_monster_soldier_lasergun(gentity_t *self);
void SP_monster_soldier_ripper(gentity_t *self);
void SP_monster_fixbot(gentity_t *self);
void SP_monster_gekk(gentity_t *self);
void SP_monster_chick_heat(gentity_t *self);
void SP_monster_gladb(gentity_t *self);
void SP_monster_boss5(gentity_t *self);

void SP_monster_stalker(gentity_t *self);
void SP_monster_turret(gentity_t *self);

void SP_hint_path(gentity_t *self);
void SP_monster_carrier(gentity_t *self);
void SP_monster_widow(gentity_t *self);
void SP_monster_widow2(gentity_t *self);
void SP_monster_kamikaze(gentity_t *self);
void SP_turret_invisible_brain(gentity_t *self);

void SP_monster_shambler(gentity_t *self);

// clang-format off
static const std::initializer_list<spawn_t> spawns = {
	{ "info_player_start", SP_info_player_start },
	{ "info_player_deathmatch", SP_info_player_deathmatch },
	{ "info_player_team_red", SP_info_player_team_red },
	{ "info_player_team_blue", SP_info_player_team_blue },
	{ "info_player_coop", SP_info_player_coop },
	{ "info_player_coop_lava", SP_info_player_coop_lava },
	{ "info_player_intermission", SP_info_player_intermission },
	{ "info_teleport_destination", SP_info_teleport_destination },
	{ "info_ctf_teleport_destination", SP_info_ctf_teleport_destination },
	{ "info_null", SP_info_null },
	{ "info_notnull", SP_info_notnull },
	{ "info_landmark", SP_info_landmark },
	{ "info_world_text", SP_info_world_text },
	{ "info_nav_lock", SP_info_nav_lock },

	{ "func_plat", SP_func_plat },
	{ "func_plat2", SP_func_plat2 },
	{ "func_button", SP_func_button },
	{ "func_door", SP_func_door },
	{ "func_door_secret", SP_func_door_secret },
	{ "func_door_secret2", SP_func_door_secret2 },
	{ "func_door_rotating", SP_func_door_rotating },
	{ "func_rotating", SP_func_rotating },
	{ "func_train", SP_func_train },
	{ "func_water", SP_func_water },
	{ "func_conveyor", SP_func_conveyor },
	{ "func_areaportal", SP_func_areaportal },
	{ "func_clock", SP_func_clock },
	{ "func_wall", SP_func_wall },
	{ "func_force_wall", SP_func_force_wall },
	{ "func_object", SP_func_object },
	{ "func_timer", SP_func_timer },
	{ "func_explosive", SP_func_explosive },
	{ "func_killbox", SP_func_killbox },
	{ "func_eye", SP_func_eye },
	{ "func_animation", SP_func_animation },
	{ "func_spinning", SP_func_spinning },
	{ "func_object_repair", SP_object_repair },

	{ "trigger_always", SP_trigger_always },
	{ "trigger_once", SP_trigger_once },
	{ "trigger_multiple", SP_trigger_multiple },
	{ "trigger_relay", SP_trigger_relay },
	{ "trigger_push", SP_trigger_push },
	{ "trigger_hurt", SP_trigger_hurt },
	{ "trigger_key", SP_trigger_key },
	{ "trigger_counter", SP_trigger_counter },
	{ "trigger_elevator", SP_trigger_elevator },
	{ "trigger_gravity", SP_trigger_gravity },
	{ "trigger_monsterjump", SP_trigger_monsterjump },
	{ "trigger_flashlight", SP_trigger_flashlight }, // [Paril-KEX]
	{ "trigger_fog", SP_trigger_fog }, // [Paril-KEX]
	{ "trigger_coop_relay", SP_trigger_coop_relay }, // [Paril-KEX]
	{ "trigger_health_relay", SP_trigger_health_relay }, // [Paril-KEX]
	{ "trigger_teleport", SP_trigger_teleport },
	{ "trigger_ctf_teleport", SP_trigger_ctf_teleport },
	{ "trigger_disguise", SP_trigger_disguise },
	{ "trigger_setskill", SP_target_setskill },

	{ "target_temp_entity", SP_target_temp_entity },
	{ "target_speaker", SP_target_speaker },
	{ "target_explosion", SP_target_explosion },
	{ "target_changelevel", SP_target_changelevel },
	{ "target_secret", SP_target_secret },
	{ "target_goal", SP_target_goal },
	{ "target_splash", SP_target_splash },
	{ "target_spawner", SP_target_spawner },
	{ "target_blaster", SP_target_blaster },
	{ "target_crosslevel_trigger", SP_target_crosslevel_trigger },
	{ "target_crosslevel_target", SP_target_crosslevel_target },
	{ "target_crossunit_trigger", SP_target_crossunit_trigger }, // [Paril-KEX]
	{ "target_crossunit_target", SP_target_crossunit_target }, // [Paril-KEX]
	{ "target_laser", SP_target_laser },
	{ "target_help", SP_target_help },
	{ "target_actor", SP_target_actor },
	{ "target_lightramp", SP_target_lightramp },
	{ "target_earthquake", SP_target_earthquake },
	{ "target_character", SP_target_character },
	{ "target_string", SP_target_string },
	{ "target_camera", SP_target_camera }, // [Sam-KEX]
	{ "target_gravity", SP_target_gravity }, // [Sam-KEX]
	{ "target_soundfx", SP_target_soundfx }, // [Sam-KEX]
	{ "target_light", SP_target_light }, // [Paril-KEX]
	{ "target_poi", SP_target_poi }, // [Paril-KEX]
	{ "target_music", SP_target_music },
	{ "target_healthbar", SP_target_healthbar }, // [Paril-KEX]
	{ "target_autosave", SP_target_autosave }, // [Paril-KEX]
	{ "target_sky", SP_target_sky }, // [Paril-KEX]
	{ "target_achievement", SP_target_achievement }, // [Paril-KEX]
	{ "target_story", SP_target_story }, // [Paril-KEX]
	{ "target_mal_laser", SP_target_mal_laser },
	{ "target_steam", SP_target_steam },
	{ "target_anger", SP_target_anger },
	{ "target_killplayers", SP_target_killplayers },
	// PMM - experiment
	{ "target_blacklight", SP_target_blacklight },
	{ "target_orb", SP_target_orb },
	// pmm
	{ "target_remove_powerups", SP_target_remove_powerups },
	{ "target_give", SP_target_give },
	{ "target_delay", SP_target_delay },
	{ "target_print", SP_target_print },
	{ "target_teleporter", SP_target_teleporter },
	{ "target_relay", SP_trigger_relay },
	{ "target_kill", SP_target_kill },
	{ "target_cvar", SP_target_cvar },
	{ "target_setskill", SP_target_setskill },
	{ "target_position", SP_info_notnull },

	{ "target_setskill", SP_target_setskill },
	{ "target_score", SP_target_score },
	{ "target_remove_weapons", SP_target_remove_weapons },

	{ "target_shooter_grenade", SP_target_shooter_grenade },
	{ "target_shooter_rocket", SP_target_shooter_rocket },
	{ "target_shooter_bfg", SP_target_shooter_bfg },
	{ "target_shooter_prox", SP_target_shooter_prox },
	{ "target_shooter_ionripper", SP_target_shooter_ionripper },
	{ "target_shooter_phalanx", SP_target_shooter_phalanx },
	{ "target_shooter_flechette", SP_target_shooter_flechette },
	{ "target_push", SP_target_push },

	{ "worldspawn", SP_worldspawn },

	{ "dynamic_light", SP_dynamic_light },
	{ "rotating_light", SP_rotating_light },
	{ "light", SP_light },
	{ "light_mine1", SP_light_mine1 },
	{ "light_mine2", SP_light_mine2 },
	{ "func_group", SP_info_null },
	{ "path_corner", SP_path_corner },
	{ "point_combat", SP_point_combat },

	{ "misc_explobox", SP_misc_explobox },
	{ "misc_banner", SP_misc_banner },
	{ "misc_ctf_banner", SP_misc_ctf_banner },
	{ "misc_ctf_small_banner", SP_misc_ctf_small_banner },
	{ "misc_satellite_dish", SP_misc_satellite_dish },
	{ "misc_actor", SP_misc_actor },
	{ "misc_player_mannequin", SP_misc_player_mannequin },
	{ "misc_model", SP_misc_model }, // [Paril-KEX]
	{ "misc_gib_arm", SP_misc_gib_arm },
	{ "misc_gib_leg", SP_misc_gib_leg },
	{ "misc_gib_head", SP_misc_gib_head },
	{ "misc_insane", SP_misc_insane },
	{ "misc_deadsoldier", SP_misc_deadsoldier },
	{ "misc_viper", SP_misc_viper },
	{ "misc_viper_bomb", SP_misc_viper_bomb },
	{ "misc_bigviper", SP_misc_bigviper },
	{ "misc_strogg_ship", SP_misc_strogg_ship },
	{ "misc_teleporter", SP_misc_teleporter },
	{ "misc_teleporter_dest", SP_misc_teleporter_dest },
	{ "misc_blackhole", SP_misc_blackhole },
	{ "misc_eastertank", SP_misc_eastertank },
	{ "misc_easterchick", SP_misc_easterchick },
	{ "misc_easterchick2", SP_misc_easterchick2 },
	{ "misc_flare", SP_misc_flare }, // [Sam-KEX]
	{ "misc_hologram", SP_misc_hologram }, // Paril
	{ "misc_lavaball", SP_misc_lavaball }, // Paril
	{ "misc_crashviper", SP_misc_crashviper },
	{ "misc_viper_missile", SP_misc_viper_missile },
	{ "misc_amb4", SP_misc_amb4 },
	{ "misc_transport", SP_misc_transport },
	{ "misc_nuke", SP_misc_nuke },
	{ "misc_nuke_core", SP_misc_nuke_core },

	{ "monster_berserk", SP_monster_berserk },
	{ "monster_gladiator", SP_monster_gladiator },
	{ "monster_gunner", SP_monster_gunner },
	{ "monster_infantry", SP_monster_infantry },
	{ "monster_soldier_light", SP_monster_soldier_light },
	{ "monster_soldier", SP_monster_soldier },
	{ "monster_soldier_ss", SP_monster_soldier_ss },
	{ "monster_tank", SP_monster_tank },
	{ "monster_tank_commander", SP_monster_tank },
	{ "monster_medic", SP_monster_medic },
	{ "monster_flipper", SP_monster_flipper },
	{ "monster_chick", SP_monster_chick },
	{ "monster_parasite", SP_monster_parasite },
	{ "monster_flyer", SP_monster_flyer },
	{ "monster_brain", SP_monster_brain },
	{ "monster_floater", SP_monster_floater },
	{ "monster_hover", SP_monster_hover },
	{ "monster_mutant", SP_monster_mutant },
	{ "monster_supertank", SP_monster_supertank },
	{ "monster_boss2", SP_monster_boss2 },
	{ "monster_boss3_stand", SP_monster_boss3_stand },
	{ "monster_jorg", SP_monster_jorg },
	{ "monster_makron", SP_monster_makron },
	{ "monster_tank_stand", SP_monster_tank_stand },
	{ "monster_guardian", SP_monster_guardian },
	{ "monster_arachnid", SP_monster_arachnid },
	{ "monster_guncmdr", SP_monster_guncmdr },

	{ "monster_commander_body", SP_monster_commander_body },

	{ "turret_breach", SP_turret_breach },
	{ "turret_base", SP_turret_base },
	{ "turret_driver", SP_turret_driver },

	{ "monster_soldier_hypergun", SP_monster_soldier_hypergun },
	{ "monster_soldier_lasergun", SP_monster_soldier_lasergun },
	{ "monster_soldier_ripper", SP_monster_soldier_ripper },
	{ "monster_fixbot", SP_monster_fixbot },
	{ "monster_gekk", SP_monster_gekk },
	{ "monster_chick_heat", SP_monster_chick_heat },
	{ "monster_gladb", SP_monster_gladb },
	{ "monster_boss5", SP_monster_boss5 },

	{ "monster_stalker", SP_monster_stalker },
	{ "monster_turret", SP_monster_turret },
	{ "monster_daedalus", SP_monster_hover },
	{ "hint_path", SP_hint_path },
	{ "monster_carrier", SP_monster_carrier },
	{ "monster_widow", SP_monster_widow },
	{ "monster_widow2", SP_monster_widow2 },
	{ "monster_medic_commander", SP_monster_medic },
	{ "monster_kamikaze", SP_monster_kamikaze },
	{ "turret_invisible_brain", SP_turret_invisible_brain },

	{ "monster_shambler", SP_monster_shambler }
};
// clang-format on


static void SpawnEnt_MapFixes(gentity_t *ent) {
	if (!Q_strcasecmp(level.mapname, "bunk1")) {
		if (!Q_strcasecmp(ent->classname, "func_button") && !Q_strcasecmp(ent->model, "*36")) {
			ent->wait = -1;
		}
		return;
	}
	if (!Q_strcasecmp(ent->classname, "item_health_mega")) {
		if (!Q_strcasecmp(level.mapname, "q2dm1")) {
			if (ent->s.origin == vec3_t{ 480, 1376, 912 }) {
				ent->s.angles = { 0, -45, 0 };
			}
			return;
		}
		if (!Q_strcasecmp(level.mapname, "q2dm8")) {
			if (ent->s.origin == vec3_t{ -832, 192, -232 }) {
				ent->s.angles = { 0, 90, 0 };
			}
			return;
		}
		if (!Q_strcasecmp(level.mapname, "fact3")) {
			if (ent->s.origin == vec3_t{ -80, 568, 144 }) {
				ent->s.angles = { 0, -90, 0 };
			}
			return;
		}
	}
}

// ----------

/*
===============
ED_CallSpawn

Finds the spawn function for the entity and calls it
===============
*/
void ED_CallSpawn(gentity_t *ent) {
	gitem_t	*item;
	int		 i;

	if (!ent->classname) {
		gi.Com_PrintFmt("{}: nullptr classname\n", __FUNCTION__);
		G_FreeEntity(ent);
		return;
	}

	// do this before calling the spawn function so it can be overridden.
	ent->gravityVector[0] = 0.0;
	ent->gravityVector[1] = 0.0;
	ent->gravityVector[2] = -1.0;

	ent->sv.init = false;

#if 0
	if (GT(GT_HORDE)) {
		// remove monsters from map, we will spawn them in during wave starts
		if (!strnicmp(ent->classname, "monster_", 8)) {
			G_FreeEntity(ent);
			return;
		}
	}
#endif
	MM_RemapSpawnClassname(ent);

	SpawnEnt_MapFixes(ent);

	// check item spawn functions
	for (i = 0, item = itemlist; i < IT_TOTAL; i++, item++) {
		if (!item->classname)
			continue;
		if (!strcmp(item->classname, ent->classname)) {
			// found it
			// before spawning, pick random item replacement
			if (g_dm_random_items->integer) {
				ent->item = item;
				item_id_t new_item = DoRandomRespawn(ent);

				if (new_item) {
					item = GetItemByIndex(new_item);
					ent->classname = item->classname;
				}
			}

			SpawnItem(ent, item);
			return;
		}
	}

	// check normal spawn functions
	for (auto &s : spawns) {
		if (!strcmp(s.name, ent->classname)) { // found it
			s.spawn(ent);
			//gi.Com_PrintFmt("{}: found {}\n", __FUNCTION__, *ent);

			// Paril: swap classname with stored constant if we didn't change it
			if (strcmp(ent->classname, s.name) == 0)
				ent->classname = s.name;
			return;
		}
	}

	if (!strcmp(ent->classname, "item_ball")) {
		if (GT(GT_BALL)) {
			ent->s.effects |= EF_COLOR_SHELL;
			ent->s.renderfx |= RF_SHELL_RED | RF_SHELL_GREEN;
		} else {
			G_FreeEntity(ent);
		}
		return;
	}

	gi.Com_PrintFmt("{}: {} doesn't have a spawn function.\n", __FUNCTION__, *ent);
	G_FreeEntity(ent);
}

/*
=============
ED_NewString
=============
*/
char *ED_NewString(const char *string) {
	char *newb, *new_p;
	int		i;
	size_t	l;

	l = strlen(string) + 1;

	newb = (char *)gi.TagMalloc(l, TAG_LEVEL);

	new_p = newb;

	for (i = 0; i < l; i++) {
		if (string[i] == '\\' && i < l - 1) {
			i++;
			if (string[i] == 'n')
				*new_p++ = '\n';
			else
				*new_p++ = '\\';
		} else
			*new_p++ = string[i];
	}

	return newb;
}

//
// fields are used for spawning from the entity string
//

struct field_t {
	const char *name;
	void (*load_func) (gentity_t *e, const char *s) = nullptr;
};

// utility template for getting the type of a field
template<typename>
struct member_object_container_type {};
template<typename T1, typename T2>
struct member_object_container_type<T1 T2:: *> { using type = T2; };
template<typename T>
using member_object_container_type_t = typename member_object_container_type<std::remove_cv_t<T>>::type;

struct type_loaders_t {
	template<typename T, std::enable_if_t<std::is_same_v<T, const char *>, int> = 0>
	static T load(const char *s) {
		return ED_NewString(s);
	}

	template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
	static T load(const char *s) {
		return atoi(s);
	}

	template<typename T, std::enable_if_t<std::is_same_v<T, spawnflags_t>, int> = 0>
	static T load(const char *s) {
		return spawnflags_t(atoi(s));
	}

	template<typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
	static T load(const char *s) {
		return atof(s);
	}

	template<typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
	static T load(const char *s) {
		if constexpr (sizeof(T) > 4)
			return static_cast<T>(atoll(s));
		else
			return static_cast<T>(atoi(s));
	}

	template<typename T, std::enable_if_t<std::is_same_v<T, vec3_t>, int> = 0>
	static T load(const char *s) {
		vec3_t vec;
		static char vec_buffer[32];
		const char *token = COM_Parse(&s, vec_buffer, sizeof(vec_buffer));
		vec.x = atof(token);
		token = COM_Parse(&s);
		vec.y = atof(token);
		token = COM_Parse(&s);
		vec.z = atof(token);
		return vec;
	}
};

#define AUTO_LOADER_FUNC(M) \
	[](gentity_t *e, const char *s) { \
		e->M = type_loaders_t::load<decltype(e->M)>(s); \
	}

static int32_t ED_LoadColor(const char *value) {
	// space means rgba as values
	if (strchr(value, ' ')) {
		static char color_buffer[32];
		std::array<float, 4> raw_values{ 0, 0, 0, 1.0f };
		bool is_float = true;

		for (auto &v : raw_values) {
			const char *token = COM_Parse(&value, color_buffer, sizeof(color_buffer));

			if (*token) {
				v = atof(token);

				if (v > 1.0f)
					is_float = false;
			}
		}

		if (is_float)
			for (auto &v : raw_values)
				v *= 255.f;

		return ((int32_t)raw_values[3]) | (((int32_t)raw_values[2]) << 8) | (((int32_t)raw_values[1]) << 16) | (((int32_t)raw_values[0]) << 24);
	}

	// integral
	return atoi(value);
}

#define FIELD_COLOR(n, x) \
	{ n, [](gentity_t *e, const char *s) { \
		e->x = ED_LoadColor(s); \
	} }

// clang-format off
// fields that get copied directly to gentity_t
#define FIELD_AUTO(x) \
	{ #x, AUTO_LOADER_FUNC(x) }

#define FIELD_AUTO_NAMED(n, x) \
	{ n, AUTO_LOADER_FUNC(x) }

static const std::initializer_list<field_t> entity_fields = {
	FIELD_AUTO(classname),
	FIELD_AUTO(model),
	FIELD_AUTO(spawnflags),
	FIELD_AUTO(speed),
	FIELD_AUTO(accel),
	FIELD_AUTO(decel),
	FIELD_AUTO(target),
	FIELD_AUTO(targetname),
	FIELD_AUTO(pathtarget),
	FIELD_AUTO(deathtarget),
	FIELD_AUTO(healthtarget),
	FIELD_AUTO(itemtarget),
	FIELD_AUTO(killtarget),
	FIELD_AUTO(combattarget),
	FIELD_AUTO(message),
	FIELD_AUTO(team),
	FIELD_AUTO(wait),
	FIELD_AUTO(delay),
	FIELD_AUTO(random),
	FIELD_AUTO(move_origin),
	FIELD_AUTO(move_angles),
	FIELD_AUTO(style),
	FIELD_AUTO(style_on),
	FIELD_AUTO(style_off),
	FIELD_AUTO(crosslevel_flags),
	FIELD_AUTO(count),
	FIELD_AUTO(health),
	FIELD_AUTO(sounds),
	{ "light" },
	FIELD_AUTO(dmg),
	FIELD_AUTO(mass),
	FIELD_AUTO(volume),
	FIELD_AUTO(attenuation),
	FIELD_AUTO(map),
	FIELD_AUTO_NAMED("origin", s.origin),
	FIELD_AUTO_NAMED("angles", s.angles),
	{ "angle", [](gentity_t *e, const char *value) {
		e->s.angles = {};
		e->s.angles[YAW] = atof(value);
	} },
	FIELD_COLOR("rgba", s.skinnum), // [Sam-KEX]
	FIELD_AUTO(hackflags), // [Paril-KEX] n64
	FIELD_AUTO_NAMED("alpha", s.alpha), // [Paril-KEX]
	FIELD_AUTO_NAMED("scale", s.scale), // [Paril-KEX]
	{ "mangle" }, // editor field
	FIELD_AUTO_NAMED("dead_frame", monsterinfo.start_frame), // [Paril-KEX]
	FIELD_AUTO_NAMED("frame", s.frame),
	FIELD_AUTO_NAMED("effects", s.effects),
	FIELD_AUTO_NAMED("renderfx", s.renderfx),

	// [Paril-KEX] fog keys
	FIELD_AUTO_NAMED("fog_color", fog.color),
	FIELD_AUTO_NAMED("fog_color_off", fog.color_off),
	FIELD_AUTO_NAMED("fog_density", fog.density),
	FIELD_AUTO_NAMED("fog_density_off", fog.density_off),
	FIELD_AUTO_NAMED("fog_sky_factor", fog.sky_factor),
	FIELD_AUTO_NAMED("fog_sky_factor_off", fog.sky_factor_off),

	FIELD_AUTO_NAMED("heightfog_falloff", heightfog.falloff),
	FIELD_AUTO_NAMED("heightfog_density", heightfog.density),
	FIELD_AUTO_NAMED("heightfog_start_color", heightfog.start_color),
	FIELD_AUTO_NAMED("heightfog_start_dist", heightfog.start_dist),
	FIELD_AUTO_NAMED("heightfog_end_color", heightfog.end_color),
	FIELD_AUTO_NAMED("heightfog_end_dist", heightfog.end_dist),

	FIELD_AUTO_NAMED("heightfog_falloff_off", heightfog.falloff_off),
	FIELD_AUTO_NAMED("heightfog_density_off", heightfog.density_off),
	FIELD_AUTO_NAMED("heightfog_start_color_off", heightfog.start_color_off),
	FIELD_AUTO_NAMED("heightfog_start_dist_off", heightfog.start_dist_off),
	FIELD_AUTO_NAMED("heightfog_end_color_off", heightfog.end_color_off),
	FIELD_AUTO_NAMED("heightfog_end_dist_off", heightfog.end_dist_off),

	// [Paril-KEX] func_eye stuff
	FIELD_AUTO_NAMED("eye_position", move_origin),
	FIELD_AUTO_NAMED("vision_cone", yaw_speed),

	// [Paril-KEX] for trigger_coop_relay
	FIELD_AUTO_NAMED("message2", map),
	FIELD_AUTO(mins),
	FIELD_AUTO(maxs),

	// [Paril-KEX] customizable bmodel animations
	FIELD_AUTO_NAMED("bmodel_anim_start", bmodel_anim.start),
	FIELD_AUTO_NAMED("bmodel_anim_end", bmodel_anim.end),
	FIELD_AUTO_NAMED("bmodel_anim_style", bmodel_anim.style),
	FIELD_AUTO_NAMED("bmodel_anim_speed", bmodel_anim.speed),
	FIELD_AUTO_NAMED("bmodel_anim_nowrap", bmodel_anim.nowrap),

	FIELD_AUTO_NAMED("bmodel_anim_alt_start", bmodel_anim.alt_start),
	FIELD_AUTO_NAMED("bmodel_anim_alt_end", bmodel_anim.alt_end),
	FIELD_AUTO_NAMED("bmodel_anim_alt_style", bmodel_anim.alt_style),
	FIELD_AUTO_NAMED("bmodel_anim_alt_speed", bmodel_anim.alt_speed),
	FIELD_AUTO_NAMED("bmodel_anim_alt_nowrap", bmodel_anim.alt_nowrap),

	// [Paril-KEX] customizable power armor stuff
	FIELD_AUTO_NAMED("power_armor_power", monsterinfo.power_armor_power),
	{ "power_armor_type", [](gentity_t *s, const char *v) {
			int32_t type = atoi(v);

			if (type == 0)
				s->monsterinfo.power_armor_type = IT_NULL;
			else if (type == 1)
				s->monsterinfo.power_armor_type = IT_POWER_SCREEN;
			else
				s->monsterinfo.power_armor_type = IT_POWER_SHIELD;
		}
	},

//muff
	FIELD_AUTO(gametype),
	FIELD_AUTO(not_gametype),
	FIELD_AUTO(notteam),
	FIELD_AUTO(notfree),
	FIELD_AUTO(notq2),
	FIELD_AUTO(notq3a),
	FIELD_AUTO(notarena),
	FIELD_AUTO(ruleset),
	FIELD_AUTO(not_ruleset),
	FIELD_AUTO(powerups_on),
	FIELD_AUTO(powerups_off),
	FIELD_AUTO(bfg_on),
	FIELD_AUTO(bfg_off),
	FIELD_AUTO(plasmabeam_on),
	FIELD_AUTO(plasmabeam_off),
//-muff

	FIELD_AUTO_NAMED("monster_slots", monsterinfo.monster_slots)
};

#undef AUTO_LOADER_FUNC

#define AUTO_LOADER_FUNC(M) \
	[](spawn_temp_t *e, const char *s) { \
		e->M = type_loaders_t::load<decltype(e->M)>(s); \
	}

struct temp_field_t {
	const char *name;
	void (*load_func) (spawn_temp_t *e, const char *s) = nullptr;
};

// temp spawn vars -- only valid when the spawn function is called
// (copied to `st`)
static const std::initializer_list<temp_field_t> temp_fields = {
	FIELD_AUTO(lip),
	FIELD_AUTO(distance),
	FIELD_AUTO(height),
	FIELD_AUTO(noise),
	FIELD_AUTO(pausetime),
	FIELD_AUTO(item),

	FIELD_AUTO(gravity),
	FIELD_AUTO(sky),
	FIELD_AUTO(skyrotate),
	FIELD_AUTO(skyaxis),
	FIELD_AUTO(skyautorotate),
	FIELD_AUTO(minyaw),
	FIELD_AUTO(maxyaw),
	FIELD_AUTO(minpitch),
	FIELD_AUTO(maxpitch),
	FIELD_AUTO(nextmap),
	FIELD_AUTO(music),  // [Edward-KEX]
	FIELD_AUTO(instantitems),
	FIELD_AUTO(radius), // [Paril-KEX]
	FIELD_AUTO(hub_map),
	FIELD_AUTO(achievement),

	FIELD_AUTO_NAMED("shadowlightradius", sl.data.radius),
	FIELD_AUTO_NAMED("shadowlightresolution", sl.data.resolution),
	FIELD_AUTO_NAMED("shadowlightintensity", sl.data.intensity),
	FIELD_AUTO_NAMED("shadowlightstartfadedistance", sl.data.fade_start),
	FIELD_AUTO_NAMED("shadowlightendfadedistance", sl.data.fade_end),
	FIELD_AUTO_NAMED("shadowlightstyle", sl.data.lightstyle),
	FIELD_AUTO_NAMED("shadowlightconeangle", sl.data.coneangle),
	FIELD_AUTO_NAMED("shadowlightstyletarget", sl.lightstyletarget),

	FIELD_AUTO(goals),

	FIELD_AUTO(image),

	FIELD_AUTO(fade_start_dist),
	FIELD_AUTO(fade_end_dist),
	FIELD_AUTO(start_items),
	FIELD_AUTO(no_grapple),
	FIELD_AUTO(no_dm_spawnpads),
	FIELD_AUTO(health_multiplier),

	FIELD_AUTO(reinforcements),
	FIELD_AUTO(noise_start),
	FIELD_AUTO(noise_middle),
	FIELD_AUTO(noise_end),

	FIELD_AUTO(loop_count),

	FIELD_AUTO(cvar),
	FIELD_AUTO(cvarvalue),
	FIELD_AUTO(author),
	FIELD_AUTO(author2),

	FIELD_AUTO(ruleset),

	FIELD_AUTO(nobots),
	FIELD_AUTO(nohumans),

};
// clang-format on

/*
===============
ED_ParseField

Takes a key/value pair and sets the binary values
in an entity
===============
*/
void ED_ParseField(const char *key, const char *value, gentity_t *ent) {

	// check st first
	for (auto &f : temp_fields) {
		if (Q_strcasecmp(f.name, key))
			continue;

		st.keys_specified.emplace(f.name);

		// found it
		if (f.load_func)
			f.load_func(&st, value);

		return;
	}

	// now entity
	for (auto &f : entity_fields) {
		if (Q_strcasecmp(f.name, key))
			continue;

		st.keys_specified.emplace(f.name);

		// [Paril-KEX]
		if (!strcmp(f.name, "bmodel_anim_start") || !strcmp(f.name, "bmodel_anim_end"))
			ent->bmodel_anim.enabled = true;

		// found it
		if (f.load_func)
			f.load_func(ent, value);

		return;
	}

	//gi.Com_PrintFmt("{} is not a valid field\n", key);
}

/*
====================
ED_ParseEntity

Parses an entity out of the given string, returning the new position
ed should be a properly initialized empty entity.
====================
*/
static const char *ED_ParseEntity(const char *data, gentity_t *ent) {
	bool  init;
	char  keyname[256];
	const char *com_token;
	char  last_keyname[256] = "";

	init = false;
	st = {};

	// go through all the dictionary pairs
	while (1) {
		// parse key
		com_token = COM_Parse(&data);
		if (com_token[0] == '}')
			break;
		if (!data) {
			MuffModeLog("DEBUG", "ED_ParseEntity: EOF parsing key, last_key='%s', ent classname='%s'",
			           last_keyname, ent->classname ? ent->classname : "(null)");
			gi.Com_Error("ED_ParseEntity: EOF without closing brace");
		}

		Q_strlcpy(keyname, com_token, sizeof(keyname));
		Q_strlcpy(last_keyname, keyname, sizeof(last_keyname));

		// parse value
		com_token = COM_Parse(&data);
		if (!data) {
			MuffModeLog("DEBUG", "ED_ParseEntity: EOF parsing value for key='%s', ent classname='%s'",
			           keyname, ent->classname ? ent->classname : "(null)");
			gi.Com_Error("ED_ParseEntity: EOF without closing brace");
		}

		if (com_token[0] == '}')
			gi.Com_Error("ED_ParseEntity: closing brace without data");

		init = true;

		// keynames with a leading underscore are used for utility comments,
		// and are immediately discarded by quake
		if (keyname[0] == '_') {
			// [Sam-KEX] Hack for setting RGBA for shadow-casting lights
			if (!strcmp(keyname, "_color"))
				ent->s.skinnum = ED_LoadColor(com_token);

			continue;
		}

		ED_ParseField(keyname, com_token, ent);
	}

	if (!init)
		memset(ent, 0, sizeof(*ent));

	return data;
}

/*
================
G_FindTeams

Chain together all entities with a matching team field.

All but the first will have the FL_TEAMSLAVE flag set.
All but the last will have the teamchain field set to the next one
================
*/

// adjusts teams so that trains that move their children
// are in the front of the team
static void G_FixTeams() {
	gentity_t *e, *e2, *chain;
	uint32_t i, j;
	uint32_t c;

	c = 0;
	for (i = 1, e = g_entities + i; i < globals.num_entities; i++, e++) {
		if (!e->inuse)
			continue;
		if (!e->team)
			continue;
		if (!strcmp(e->classname, "func_train") && e->spawnflags.has(SPAWNFLAG_TRAIN_MOVE_TEAMCHAIN)) {
			if (e->flags & FL_TEAMSLAVE) {
				chain = e;
				e->teammaster = e;
				e->teamchain = nullptr;
				e->flags &= ~FL_TEAMSLAVE;
				e->flags |= FL_TEAMMASTER;
				c++;
				for (j = 1, e2 = g_entities + j; j < globals.num_entities; j++, e2++) {
					if (e2 == e)
						continue;
					if (!e2->inuse)
						continue;
					if (!e2->team)
						continue;
					if (!strcmp(e->team, e2->team)) {
						chain->teamchain = e2;
						e2->teammaster = e;
						e2->teamchain = nullptr;
						chain = e2;
						e2->flags |= FL_TEAMSLAVE;
						e2->flags &= ~FL_TEAMMASTER;
						e2->movetype = MOVETYPE_PUSH;
						e2->speed = e->speed;
					}
				}
			}
		}
	}

	if (c)
		gi.Com_PrintFmt("{}: {} entity team{} repaired.\n", __FUNCTION__, c, c != 1 ? "s" : "");
}

static void G_FindTeams() {
	gentity_t *e1, *e2, *chain;
	uint32_t i, j;
	uint32_t c1, c2;

	c1 = 0;
	c2 = 0;
	for (i = 1, e1 = g_entities + i; i < globals.num_entities; i++, e1++) {
		if (!e1->inuse)
			continue;
		if (!e1->team)
			continue;
		if (e1->flags & FL_TEAMSLAVE)
			continue;
		chain = e1;
		e1->teammaster = e1;
		e1->flags |= FL_TEAMMASTER;
		c1++;
		c2++;
		for (j = i + 1, e2 = e1 + 1; j < globals.num_entities; j++, e2++) {
			if (!e2->inuse)
				continue;
			if (!e2->team)
				continue;
			if (e2->flags & FL_TEAMSLAVE)
				continue;
			if (!strcmp(e1->team, e2->team)) {
				c2++;
				chain->teamchain = e2;
				e2->teammaster = e1;
				chain = e2;
				e2->flags |= FL_TEAMSLAVE;
			}
		}
	}

	G_FixTeams();

	if (c1 && g_verbose->integer)
		gi.Com_PrintFmt("{}: {} entity team{} found with a total of {} entit{}.\n", __FUNCTION__, c1, c1 != 1 ? "s" : "", c2, c2 != 1 ? "ies" : "y");
}

// inhibit entities from game based on cvars & spawnflags
static inline bool G_InhibitEntity(gentity_t *ent) {
	if (MM_ShouldInhibitSpawnEntity(ent))
		return true;

	if (ent->notteam && Teams())
		return true;
	if (ent->notfree && !Teams())
		return true;

	if (ent->powerups_on && g_no_powerups->integer)
		return true;
	if (ent->powerups_off && !g_no_powerups->integer)
		return true;

	if (ent->bfg_on && g_mapspawn_no_bfg->integer)
		return true;
	if (ent->bfg_off && !g_mapspawn_no_bfg->integer)
		return true;

	if (ent->plasmabeam_on && g_mapspawn_no_plasmabeam->integer)
		return true;
	if (ent->plasmabeam_off && !g_mapspawn_no_plasmabeam->integer)
		return true;

	// dm-only
	if (deathmatch->integer)
		return ent->spawnflags.has(SPAWNFLAG_NOT_DEATHMATCH);

	// coop flags
	if (coop->integer && ent->spawnflags.has(SPAWNFLAG_NOT_COOP))
		return true;
	else if (!coop->integer && ent->spawnflags.has(SPAWNFLAG_COOP_ONLY))
		return true;

	// skill
	return ((skill->integer == 0) && ent->spawnflags.has(SPAWNFLAG_NOT_EASY)) ||
		((skill->integer == 1) && ent->spawnflags.has(SPAWNFLAG_NOT_MEDIUM)) ||
		((skill->integer >= 2) && ent->spawnflags.has(SPAWNFLAG_NOT_HARD));
}

void setup_shadow_lights();

// [Paril-KEX]
void PrecacheInventoryItems() {
	if (deathmatch->integer)
		return;

	for (auto ce : active_clients()) {
		for (item_id_t id = IT_NULL; id != IT_TOTAL; id = static_cast<item_id_t>(id + 1))
			if (ce->client->pers.inventory[id])
				PrecacheItem(GetItemByIndex(id));
	}
}

static void PrecacheStartItems() {
	if (!*g_start_items->string)
		return;

	char token_copy[MAX_TOKEN_CHARS];
	const char *token;
	const char *ptr = g_start_items->string;

	while ((token = COM_ParseEx(&ptr, ";")) && *token) {
		Q_strlcpy(token_copy, token, sizeof(token_copy));
		const char *ptr_copy = token_copy;

		const char *item_name = COM_Parse(&ptr_copy);
		gitem_t *item = FindItemByClassname(item_name);

		if (!item || !item->pickup)
			gi.Com_ErrorFmt("Invalid g_start_item entry: {}\n", item_name);

		if (*ptr_copy)
			COM_Parse(&ptr_copy);

		PrecacheItem(item);
	}
}

static void PrecachePlayerSounds() {

	gi.soundindex("player/lava1.wav");
	gi.soundindex("player/lava2.wav");

	gi.soundindex("player/gasp1.wav"); // gasping for air
	gi.soundindex("player/gasp2.wav"); // head breaking surface, not gasping

	gi.soundindex("player/watr_in.wav");  // feet hitting water
	gi.soundindex("player/watr_out.wav"); // feet leaving water

	gi.soundindex("player/watr_un.wav"); // head going underwater

	gi.soundindex("player/u_breath1.wav");
	gi.soundindex("player/u_breath2.wav");

	gi.soundindex("player/wade1.wav");
	gi.soundindex("player/wade2.wav");
	gi.soundindex("player/wade3.wav");
	gi.soundindex("misc/talk1.wav");

	gi.soundindex("world/land.wav");   // landing thud
	gi.soundindex("misc/h2ohit1.wav"); // landing splash

	// gibs
	gi.soundindex("misc/udeath.wav");

	gi.soundindex("items/respawn1.wav");
	gi.soundindex("misc/mon_power2.wav");

	// sexed sounds
	gi.soundindex("*death1.wav");
	gi.soundindex("*death2.wav");
	gi.soundindex("*death3.wav");
	gi.soundindex("*death4.wav");
	gi.soundindex("*fall1.wav");
	gi.soundindex("*fall2.wav");
	gi.soundindex("*gurp1.wav"); // drowning damage
	gi.soundindex("*gurp2.wav");
	gi.soundindex("*jump1.wav"); // player jump
	gi.soundindex("*pain25_1.wav");
	gi.soundindex("*pain25_2.wav");
	gi.soundindex("*pain50_1.wav");
	gi.soundindex("*pain50_2.wav");
	gi.soundindex("*pain75_1.wav");
	gi.soundindex("*pain75_2.wav");
	gi.soundindex("*pain100_1.wav");
	gi.soundindex("*pain100_2.wav");
	gi.soundindex("*drown1.wav"); // [Paril-KEX]
}

void GT_PrecacheAssets() {
	if (Teams()) {
		if (notGT(GT_RR)) {
			ii_teams_header_red = gi.imageindex("tag4");
			ii_teams_header_blue = gi.imageindex("tag5");
		}
		ii_teams_red_default = gi.imageindex("i_ctf1");
		ii_teams_blue_default = gi.imageindex("i_ctf2");
		ii_teams_red_tiny = gi.imageindex("sbfctf1");
		ii_teams_blue_tiny = gi.imageindex("sbfctf2");
	}

	if (GT(GT_DUEL))
		ii_duel_header = gi.imageindex("/tags/default");

	if (GTF(GTF_CTF)) {
		ii_ctf_red_dropped = gi.imageindex("i_ctf1d");
		ii_ctf_blue_dropped = gi.imageindex("i_ctf2d");
		ii_ctf_red_taken = gi.imageindex("i_ctf1t");
		ii_ctf_blue_taken = gi.imageindex("i_ctf2t");
		mi_ctf_red_flag = gi.modelindex("players/male/flag1.md2");
		mi_ctf_blue_flag = gi.modelindex("players/male/flag2.md2");
	}
}

// [Paril-KEX]
static void PrecacheAssets() {
	if (!deathmatch->integer) {
		gi.soundindex("infantry/inflies1.wav");

		// help icon for statusbar
		gi.imageindex("i_help");
		gi.imageindex("help");
		gi.soundindex("misc/pc_up.wav");
	}

	level.pic_ping = gi.imageindex("loc_ping");

	level.pic_health = gi.imageindex("i_health");
	gi.imageindex("field_3");

	gi.soundindex("items/pkup.wav");   // bonus item pickup

	//gi.soundindex("items/damage.wav");
	//gi.soundindex("items/protect.wav");
	//gi.soundindex("items/protect4.wav");
	gi.soundindex("weapons/noammo.wav");
	gi.soundindex("weapons/lowammo.wav");
	gi.soundindex("weapons/change.wav");

	// gibs
	sm_meat_index.assign("models/objects/gibs/sm_meat/tris.md2");
	gi.modelindex("models/objects/gibs/arm/tris.md2");
	gi.modelindex("models/objects/gibs/bone/tris.md2");
	gi.modelindex("models/objects/gibs/bone2/tris.md2");
	gi.modelindex("models/objects/gibs/chest/tris.md2");
	gi.modelindex("models/objects/gibs/skull/tris.md2");
	gi.modelindex("models/objects/gibs/head2/tris.md2");
	gi.modelindex("models/objects/gibs/sm_metal/tris.md2");

	ii_highlight = gi.imageindex("i_ctfj");

	GT_PrecacheAssets();
}

#define	MAX_READ	0x10000		// read in blocks of 64k
static void FS_Read(void *buffer, int len, FILE *f) {
	int		block, remaining;
	int		read;
	byte *buf;
	int		tries;

	buf = (byte *)buffer;

	remaining = len;
	tries = 0;
	while (remaining) {
		block = remaining;
		if (block > MAX_READ)
			block = MAX_READ;
		read = fread(buf, 1, block, f);
		if (read == 0) {
			if (!tries) {
				tries = 1;
			} else
				gi.Com_Error("FS_Read: 0 bytes read");
		}

		if (read == -1)
			gi.Com_Error("FS_Read: -1 bytes read");

		remaining -= read;
		buf += read;
	}
}


/*
==============
VerifyEntityString
==============
*/
static bool VerifyEntityString(const char *entities) {
	const char *or_token;
	gentity_t *or_ent = nullptr;
	const char *or_buf = entities;
	bool		or_error = false;

	while (1) {
		// parse the opening brace
		or_token = COM_Parse(&or_buf);
		if (!or_buf)
			break;
		if (or_token[0] != '{') {
			gi.Com_PrintFmt("{}: Found \"{}\" when expecting {{ in override.\n", __FUNCTION__, or_token);
			return false;
		}

		while (1) {
			// parse key
			or_token = COM_Parse(&or_buf);
			if (or_token[0] == '}')
				break;
			if (!or_buf) {
				gi.Com_ErrorFmt("{}: EOF without closing brace.\n", __FUNCTION__);
				return false;
			}
			// parse value
			or_token = COM_Parse(&or_buf);
			if (!or_buf) {
				gi.Com_ErrorFmt("{}: EOF without closing brace.\n", __FUNCTION__);
				return false;
			}
			if (or_token[0] == '}') {
				gi.Com_ErrorFmt("{}: Closing brace without data.\n", __FUNCTION__);
				return false;
			}
		}

	}
	return true;
}

static void PrecacheForRandomRespawn() {
	gitem_t *it;
	int		 i;
	int		 itflags;

	it = itemlist;
	for (i = 0; i < IT_TOTAL; i++, it++) {
		itflags = it->flags;

		if (!itflags || (itflags & (IF_NOT_GIVEABLE | IF_TECH | IF_NOT_RANDOM)) || !it->pickup || !it->world_model)
			continue;

		PrecacheItem(it);
	}
}

static void G_LocateSpawnSpots(void) {
	gentity_t *ent;
	int			n;
	const char *s = nullptr;
	size_t		sl = 0;

	level.spawn_spots[SPAWN_SPOT_INTERMISSION] = nullptr;
	level.num_spawn_spots_free = 0;
	level.num_spawn_spots_team = 0;

	// locate all spawn spots
	n = 0;
	for (ent = g_entities; ent < &g_entities[globals.num_entities]; ent++) {

		if (!ent->inuse || !ent->classname)
			continue;

		s = "info_player_";
		sl = strlen(s);

		if (Q_strncasecmp(ent->classname, s, sl))
			continue;

		// intermission/ffa spots
		if (!Q_strncasecmp(ent->classname, s, sl)) {
			if (!Q_strcasecmp(ent->classname + sl, "intermission")) {
				if (level.spawn_spots[SPAWN_SPOT_INTERMISSION] == NULL) {
					level.spawn_spots[SPAWN_SPOT_INTERMISSION] = ent; // put in the last slot
					ent->fteam = TEAM_FREE;

					// if it has a target, look towards it
					if (ent->target) {
						gentity_t *target = G_PickTarget(ent->target);

						if (target) {
							vec3_t	dir = (target->s.origin - level.intermission_origin).normalized();
							AngleVectors(dir);
							level.intermission_angle = dir;
							return;
						}
					}
					level.intermission_angle = ent->s.angles;
				}
				continue;
			}
			if (!Q_strcasecmp(ent->classname + sl, "deathmatch")) {
				level.spawn_spots[n] = ent; n++;
				level.num_spawn_spots_free++;
				ent->fteam = TEAM_FREE;
				ent->count = 1; // means its not initial spawn point
				continue;
			}
			if (!Q_strcasecmp(ent->classname + sl, "team_red")) {
				level.spawn_spots[n] = ent; n++;
				level.num_spawn_spots_team++;
				ent->fteam = TEAM_RED;
				ent->count = 1; // means its not initial spawn point
				continue;
			}
			if (!Q_strcasecmp(ent->classname + sl, "team_blue")) {
				level.spawn_spots[n] = ent; n++;
				level.num_spawn_spots_team++;
				ent->fteam = TEAM_BLUE;
				ent->count = 1; // means its not initial spawn point
				continue;
			}
			continue;
		}
	}

	level.num_spawn_spots = n;
}

static void ParseWorldEntityString(const char *mapname, bool try_q3) {
	bool	ent_file_exists = false, ent_valid = true;
	const char *entities = level.entstring.c_str();

	// load up ent override
	const char *name = G_Fmt("baseq2/{}/{}.ent", g_entity_override_dir->string[0] ? g_entity_override_dir->string : "maps", mapname).data();
	FILE *f = fopen(name, "rb");
	if (f != NULL) {
		char *buffer = nullptr;
		size_t length;
		size_t read_length;

		fseek(f, 0, SEEK_END);
		length = ftell(f);
		fseek(f, 0, SEEK_SET);

		if (length > 0x40000) {
			//gi.Com_PrintFmt("{}: Entities override file length exceeds maximum: \"{}\"\n", __FUNCTION__, name);
			ent_valid = false;
		}
		if (ent_valid) {
			buffer = (char *)gi.TagMalloc(length + 1, TAG_LEVEL);
			if (length) {
				read_length = fread(buffer, 1, length, f);

				if (length != read_length) {
					//gi.Com_PrintFmt("{}: Entities override file read error: \"{}\"\n", __FUNCTION__, name);
					ent_valid = false;
				}
			}
		}
		ent_file_exists = true;
		fclose(f);

		if (ent_valid) {
			if (g_entity_override_load->integer && !strstr(level.mapname, ".dm2")) {

				if (VerifyEntityString((const char *)buffer)) {
					entities = (const char *)buffer;
					//gi.Com_PrintFmt("Entities override: \"{}\"\n", name);
				}
			}
		} else {
			gi.Com_PrintFmt("{}: Entities override file load error for \"{}\", discarding.\n", __FUNCTION__, name);
		}
	}

	// save ent override
	if (g_entity_override_save->integer && !strstr(level.mapname, ".dm2")) {
		if (!ent_file_exists) {
			f = fopen(name, "w");
			if (f) {
				fwrite(entities, 1, strlen(entities), f);
				if (g_verbose->integer)
					gi.Com_PrintFmt("{}: Entities override file written to: \"{}\"\n", __FUNCTION__, name);
				fclose(f);
			}
		} else {
			if (g_verbose->integer)
				gi.Com_PrintFmt("{}: Entities override file not saved as file already exists: \"{}\"\n", __FUNCTION__, name);
		}
	}
	level.entstring = entities;
}

static void ParseWorldEntities() {
	gentity_t		*ent = nullptr;
	int			inhibit = 0;
	const char	*com_token;
	const char	*entities = level.entstring.c_str();

	// parse entities
	while (1) {
		// parse the opening brace
		com_token = COM_Parse(&entities);
		if (!entities)
			break;
		if (com_token[0] != '{')
			gi.Com_ErrorFmt("{}: Found \"{}\" when expecting {{ in entity string.", __FUNCTION__, com_token);

		if (!ent)
			ent = g_entities;
		else
			ent = G_Spawn();
		entities = ED_ParseEntity(entities, ent);

		// nasty hacks time!
		if (!strcmp(level.mapname, "bunk1")) {
			if (!strcmp(ent->classname, "func_button") && !Q_strcasecmp(ent->model, "*36")) {
				ent->wait = -1;
			}
		}

		// remove things (except the world) from different skill levels or deathmatch
		if (ent != g_entities) {
			if (G_InhibitEntity(ent)) {
				G_FreeEntity(ent);
				inhibit++;
				continue;
			}

			ent->spawnflags &= ~SPAWNFLAG_EDITOR_MASK;
		}

		if (!ent)
			gi.Com_ErrorFmt("{}: Invalid or empty entity string.", __FUNCTION__);

		// do this before calling the spawn function so it can be overridden.
		ent->gravityVector = { 0.0, 0.0, -1.0 };

		ED_CallSpawn(ent);

		ent->s.renderfx |= RF_IR_VISIBLE;
	}

	if (inhibit && g_verbose->integer)
		gi.Com_PrintFmt("{} entities inhibited.\n", inhibit);
}

void ClearWorldEntities() {
	gentity_t *ent = nullptr;
	//memset(g_entities, 0, game.maxentities * sizeof(g_entities[0]));

	for (size_t i = MAX_CLIENTS; i < game.maxentities; i++) {
		ent = &g_entities[i];

		if (!ent || !ent->inuse || ent->client)
			continue;

		memset(&g_entities[i], 0, sizeof(g_entities[i]));
	}
}

/*
==============
SpawnEntities

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.
==============
*/
void SpawnEntities(const char *mapname, const char *entities, const char *spawnpoint) {
	MuffModeLog("MAP", "Loading map: '%s' (spawnpoint: '%s')", mapname, spawnpoint ? spawnpoint : "(none)");
	
	bool		ent_file_exists = false, ent_valid = true;
	//const char	*entities = level.entstring.c_str();
//#if 0
	// load up ent override
	//const char *name = G_Fmt("baseq2/maps/{}.ent", mapname).data();
	const char *name = G_Fmt("baseq2/{}/{}.ent", g_entity_override_dir->string[0] ? g_entity_override_dir->string : "maps", mapname).data();
	FILE *f = fopen(name, "rb");
	if (f != NULL) {
		char *buffer = nullptr;
		size_t length;
		size_t read_length;

		fseek(f, 0, SEEK_END);
		length = ftell(f);
		fseek(f, 0, SEEK_SET);

		if (length > 0x40000) {
			//gi.Com_PrintFmt("{}: Entities override file length exceeds maximum: \"{}\"\n", __FUNCTION__, name);
			ent_valid = false;
		}
		if (ent_valid) {
			buffer = (char *)gi.TagMalloc(length + 1, TAG_LEVEL);
			if (length) {
				read_length = fread(buffer, 1, length, f);

				if (length != read_length) {
					//gi.Com_PrintFmt("{}: Entities override file read error: \"{}\"\n", __FUNCTION__, name);
					ent_valid = false;
				}
			}
		}
		ent_file_exists = true;
		fclose(f);

		if (ent_valid) {
			if (g_entity_override_load->integer && !strstr(level.mapname, ".dm2")) {

				if (VerifyEntityString((const char *)buffer)) {
					entities = (const char *)buffer;
					if (g_verbose->integer)
						gi.Com_PrintFmt("{}: Entities override file verified and loaded: \"{}\"\n", __FUNCTION__, name);
				}
			}
		} else {
			gi.Com_PrintFmt("{}: Entities override file load error for \"{}\", discarding.\n", __FUNCTION__, name);
		}
	}

	// save ent override
	if (g_entity_override_save->integer && !strstr(level.mapname, ".dm2")) {
		if (!ent_file_exists) {
			f = fopen(name, "w");
			if (f) {
				fwrite(entities, 1, strlen(entities), f);
				if (g_verbose->integer)
					gi.Com_PrintFmt("{}: Entities override file written to: \"{}\"\n", __FUNCTION__, name);
				fclose(f);
			}
		} else {
			//gi.Com_PrintFmt("{}: Entities override file not saved as file already exists: \"{}\"\n", __FUNCTION__, name);
		}
	}
	// Save entity string into a std::string BEFORE FreeTags, because if an .ent
	// override file was loaded, 'entities' points to TAG_LEVEL memory that will
	// be freed. This copy ensures we have a safe copy for parsing later.
	std::string saved_entstring(entities);
	level.entstring = entities;
//#endif
	//ParseWorldEntityString(mapname, RS(RS_Q3A));

	// clear cached indices
	cached_soundindex::clear_all();
	cached_modelindex::clear_all();
	cached_imageindex::clear_all();

	int skill_level = clamp(skill->integer, 0, 4);
	if (skill->integer != skill_level)
		gi.cvar_forceset("skill", G_Fmt("{}", skill_level).data());

	SaveClientData();

	// Dump client menu pointers BEFORE FreeTags to detect what will become stale
	for (size_t dbg_i = 0; dbg_i < game.maxclients; dbg_i++) {
		if (game.clients[dbg_i].menu)
			MuffModeLog("DEBUG", "SpawnEntities: client %d has menu=%p BEFORE FreeTags",
			           (int)dbg_i, (void*)game.clients[dbg_i].menu);
	}

	gi.FreeTags(TAG_LEVEL);

	// After FreeTags, any TAG_LEVEL pointers in game.clients are now dangling.
	// Null them out to prevent use-after-free.
	for (size_t dbg_i = 0; dbg_i < game.maxclients; dbg_i++) {
		if (game.clients[dbg_i].menu) {
			MuffModeLog("DEBUG", "SpawnEntities: nulling stale menu pointer for client %d (was %p)",
			           (int)dbg_i, (void*)game.clients[dbg_i].menu);
			game.clients[dbg_i].menu = nullptr;
			game.clients[dbg_i].inmenu = false;
		}
	}

	// After FreeTags, 'entities' may be a dangling pointer if it was from an .ent file.
	// Redirect it to our saved copy.
	entities = saved_entstring.c_str();

	memset(&level, 0, sizeof(level));
	
	// CRITICAL: Reinitialize all C++ objects after memset
	// memset corrupts C++ objects like std::string, so we must reconstruct them
	new (&level.vote_state) VoteStateData();
	new (&level.entstring) std::string(saved_entstring);
	new (&level.match_id) std::string();
	
	memset(g_entities, 0, game.maxentities * sizeof(g_entities[0]));
	
	// all other flags are not important atm
	globals.server_flags &= SERVER_FLAG_LOADING;

	Q_strlcpy(level.mapname, mapname, sizeof(level.mapname));
	// Paril: fixes a bug where autosaves will start you at
	// the wrong spawnpoint if they happen to be non-empty
	// (mine2 -> mine3)
	if (!game.autosaved)
		Q_strlcpy(game.spawnpoint, spawnpoint, sizeof(game.spawnpoint));

	level.is_n64 = strncmp(level.mapname, "q64/", 4) == 0;
	
	MuffModeLog("MAP", "Map name set: '%s' (is_n64=%d, gametype=%s)", 
	           level.mapname, level.is_n64 ? 1 : 0, gt_short_name[g_gametype->integer]);

	level.coop_scale_players = 0;
	level.coop_health_scaling = clamp(g_coop_health_scaling->value, 0.f, 1.f);

	// set client fields on player entities
	for (size_t i = 0; i < game.maxclients; i++) {
		g_entities[i + 1].client = game.clients + i;

		// "disconnect" all players since the level is switching
		game.clients[i].pers.connected = false;
		game.clients[i].pers.spawned = false;
	}

	// reserve some spots for dead player bodies for coop / deathmatch
	InitBodyQue();

	gentity_t *ent = nullptr;
	int			inhibit = 0;
	const char *com_token;
	//const char *entities = level.entstring.c_str();

	// Log entity string state before parsing
	size_t ent_str_len = entities ? strlen(entities) : 0;
	MuffModeLog("DEBUG", "SpawnEntities: entity string ptr=%p, len=%zu, first_32='%.32s'",
	           (void*)entities, ent_str_len, entities ? entities : "(null)");
	if (ent_str_len > 0) {
		// Log last 64 chars to see if string is truncated
		size_t tail_start = ent_str_len > 64 ? ent_str_len - 64 : 0;
		MuffModeLog("DEBUG", "SpawnEntities: entity string tail='%s'", entities + tail_start);
	}

	int ent_count = 0;

	// parse entities
	while (1) {
		// parse the opening brace
		com_token = COM_Parse(&entities);
		if (!entities)
			break;
		if (com_token[0] != '{')
			gi.Com_ErrorFmt("{}: Found \"{}\" when expecting {{ in entity string.\n", __FUNCTION__, com_token);

		ent_count++;
		if (!ent)
			ent = g_entities;
		else
			ent = G_Spawn();
		entities = ED_ParseEntity(entities, ent);

		// nasty hacks time!
		if (!strcmp(level.mapname, "bunk1")) {
			if (!strcmp(ent->classname, "func_button") && !Q_strcasecmp(ent->model, "*36")) {
				ent->wait = -1;
			}
		}

		// remove things (except the world) from different skill levels or deathmatch
		if (ent != g_entities) {
			if (G_InhibitEntity(ent)) {
				G_FreeEntity(ent);
				inhibit++;
				continue;
			}

			ent->spawnflags &= ~SPAWNFLAG_EDITOR_MASK;
		}

		if (!ent)
			gi.Com_ErrorFmt("{}: Invalid or empty entity string.", __FUNCTION__);

		// do this before calling the spawn function so it can be overridden.
		ent->gravityVector = { 0.0, 0.0, -1.0 };

		ED_CallSpawn(ent);

		ent->s.renderfx |= RF_IR_VISIBLE;
	}

	if (inhibit && g_verbose->integer)
		gi.Com_PrintFmt("{} entities inhibited.\n", inhibit);

	// precache start_items
	PrecacheStartItems();

	// precache player inventory items
	PrecacheInventoryItems();

	G_FindTeams();

	QuadHog_SetupSpawn(5_sec);
	Tech_SetupSpawn();

	if (deathmatch->integer) {
		if (g_dm_random_items->integer)
			PrecacheForRandomRespawn();

		game.item_inhibit_pu = 0;
		game.item_inhibit_pa = 0;
		game.item_inhibit_ht = 0;
		game.item_inhibit_ar = 0;
		game.item_inhibit_am = 0;
		game.item_inhibit_wp = 0;
	} else {
		InitHintPaths(); // if there aren't hintpaths on this map, enable quick aborts
	}

	G_LocateSpawnSpots();

	SetIntermissionPoint();

	setup_shadow_lights();

	level.init = true;
	
	MuffModeLog("MAP", "Map '%s' loaded successfully (entities spawned, init=true)", level.mapname);
	MuffModeLog_Separator();
}

//===================================================================

void GT_SetLongName(void) {
	MM_GTSetLongName();
}

/*QUAKED worldspawn (0 0 0) ?

Only used for the world.
"sky"				environment map name
"skyaxis"			vector axis for rotating sky
"skyrotate"			speed of rotation in degrees/second
"sounds"			music cd track number
"music"				specific music file to play, overrides "sounds"
"gravity"			800 is default gravity
"hub_map"			in campaigns, sets as hub map
"message"			sets long level name
"author"			sets level author name
"author2"			sets another level author name
"start_items"		give players these items on spawn
"no_grapple"		disables grappling hook
"no_dm_spawnpads"	disables spawn pads in deathmatch
"ruleset"			overrides gameplay ruleset (q2re/mm/q3a/q2reb/qc)
*/
void SP_worldspawn(gentity_t *ent) {
	Q_strlcpy(level.gamemod_name, G_Fmt("{} v{}", GAMEMOD_TITLE, GAMEMOD_VERSION).data(), sizeof(level.gamemod_name));

	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_BSP;
	ent->inuse = true; // since the world doesn't use G_Spawn()
	ent->s.modelindex = MODELINDEX_WORLD;
	ent->gravity = 1.0f;

	if (st.hub_map) {
		level.hub_map = true;

		// clear helps
		game.help1changed = game.help2changed = 0;
		*game.helpmessage1 = *game.helpmessage2 = '\0';

		for (auto ec : active_clients()) {
			ec->client->pers.game_help1changed = ec->client->pers.game_help2changed = 0;
			ec->client->resp.coop_respawn.game_help1changed = ec->client->resp.coop_respawn.game_help2changed = 0;
		}
	}

	if (st.achievement && st.achievement[0])
		level.achievement = st.achievement;

	//---------------

	// set configstrings for items
	SetItemNames();

	if (st.nextmap)
		Q_strlcpy(level.nextmap, st.nextmap, sizeof(level.nextmap));
	else
		level.nextmap[0] = '\0'; // Clear stale value when map has no worldspawn nextmap (e.g. vote)

	// make some data visible to the server

	if (ent->message && ent->message[0]) {
		gi.configstring(CS_NAME, ent->message);
		Q_strlcpy(level.level_name, ent->message, sizeof(level.level_name));
	} else
		Q_strlcpy(level.level_name, level.mapname, sizeof(level.level_name));

	if (st.author && st.author[0])
		Q_strlcpy(level.author, st.author, sizeof(level.author));
	if (st.author2 && st.author2[0])
		Q_strlcpy(level.author2, st.author2, sizeof(level.author2));

	if (st.ruleset && st.ruleset[0]) {
		game.ruleset = RS_IndexFromString(st.ruleset);
		gi.Com_PrintFmt("st={} game={}\n", st.ruleset, rs_long_name[(int)game.ruleset]);
		if (!game.ruleset)
			game.ruleset = (ruleset_t)clamp(g_ruleset->integer, 1, (int)RS_NUM_RULESETS);
	} else
		if ((int)game.ruleset != g_ruleset->integer)
			game.ruleset = (ruleset_t)clamp(g_ruleset->integer, 1, (int)RS_NUM_RULESETS);

	if (st.sky && st.sky[0])
		gi.configstring(CS_SKY, st.sky);
	else
		gi.configstring(CS_SKY, "unit1_");

	gi.configstring(CS_SKYROTATE, G_Fmt("{} {}", st.skyrotate, st.skyautorotate).data());

	gi.configstring(CS_SKYAXIS, G_Fmt("{}", st.skyaxis).data());

	if (st.music && st.music[0]) {
		gi.configstring(CS_CDTRACK, st.music);
	} else {
		gi.configstring(CS_CDTRACK, G_Fmt("{}", ent->sounds).data());
	}

	if (level.is_n64)
		gi.configstring(CS_CD_LOOP_COUNT, "0");
	else if (st.was_key_specified("loop_count"))
		gi.configstring(CS_CD_LOOP_COUNT, G_Fmt("{}", st.loop_count).data());
	else
		gi.configstring(CS_CD_LOOP_COUNT, "");

	if (st.instantitems > 0 || level.is_n64)
		level.instantitems = true;

	// [Paril-KEX]
	if (!deathmatch->integer)
		gi.configstring(CS_GAME_STYLE, G_Fmt("{}", (int32_t)game_style_t::GAME_STYLE_PVE).data());
	else if (Teams() && notGT(GT_RR))
		gi.configstring(CS_GAME_STYLE, G_Fmt("{}", (int32_t)game_style_t::GAME_STYLE_TDM).data());
	else
		gi.configstring(CS_GAME_STYLE, G_Fmt("{}", (int32_t)game_style_t::GAME_STYLE_FFA).data());

	// [Paril-KEX]
	if (st.goals) {
		level.goals = st.goals;
		game.help1changed++;
	}

	if (st.start_items)
		level.start_items = st.start_items;

	if (st.no_grapple)
		level.no_grapple = true;

	if (st.no_dm_spawnpads)
		level.no_dm_spawnpads = true;

	gi.configstring(CS_MAXCLIENTS, G_Fmt("{}", game.maxclients).data());

	if (level.is_n64 && !deathmatch->integer) {
		gi.configstring(CONFIG_N64_PHYSICS, "1");
		pm_config.n64_physics = true;
	}

	// statusbar prog
	MM_InitStatusbar();

	// [Paril-KEX] air accel handled by game DLL now, and allow
	// it to be changed in sp/coop
	gi.configstring(CS_AIRACCEL, G_Fmt("{}", g_airaccelerate->integer).data());
	pm_config.airaccel = g_airaccelerate->integer;

	game.airacceleration_modified = g_airaccelerate->modified_count;

	//---------------

	if (!st.gravity) {
		level.gravity = 800.f;
		gi.cvar_set("g_gravity", "800");
	} else {
		level.gravity = atof(st.gravity);
		gi.cvar_set("g_gravity", st.gravity);
	}

	snd_fry.assign("player/fry.wav"); // standing in lava / slime

	PrecacheItem(GetItemByIndex(IT_COMPASS));

	if (!(g_instagib->integer || GT(GT_INSTAGIB)) && !(g_nadefest->integer || GT(GT_NADEFEST)) && notGT(GT_BALL))
		PrecacheItem(GetItemByIndex(IT_WEAPON_BLASTER));

	if (GT(GT_BALL))
		PrecacheItem(GetItemByIndex(IT_BALL));

	if ((!strcmp(g_allow_grapple->string, "auto")) ?
		(GTF(GTF_CTF) ? !level.no_grapple : 0) :
		g_allow_grapple->integer) {
		PrecacheItem(GetItemByIndex(IT_WEAPON_GRAPPLE));
	}

	if (g_dm_random_items->integer) {
		for (item_id_t i = static_cast<item_id_t>(IT_NULL + 1); i < IT_TOTAL; i = static_cast<item_id_t>(i + 1))
			PrecacheItem(GetItemByIndex(i));
	}

	PrecachePlayerSounds();

	// sexed models
	for (auto &item : itemlist)
		item.vwep_index = 0;

	for (auto &item : itemlist) {
		if (!item.vwep_model)
			continue;

		for (auto &check : itemlist) {
			if (check.vwep_model && !Q_strcasecmp(item.vwep_model, check.vwep_model) && check.vwep_index) {
				item.vwep_index = check.vwep_index;
				break;
			}
		}

		if (item.vwep_index)
			continue;

		item.vwep_index = gi.modelindex(item.vwep_model);

		if (!level.vwep_offset)
			level.vwep_offset = item.vwep_index;
	}

	PrecacheAssets();

	GT_SetLongName();

	//
	// Setup light animation tables. 'a' is total darkness, 'z' is doublebright.
	//

	// 0 normal
	gi.configstring(CS_LIGHTS + 0, "m");

	// 1 FLICKER (first variety)
	gi.configstring(CS_LIGHTS + 1, "mmnmmommommnonmmonqnmmo");

	// 2 SLOW STRONG PULSE
	gi.configstring(CS_LIGHTS + 2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");

	// 3 CANDLE (first variety)
	gi.configstring(CS_LIGHTS + 3, "mmmmmaaaaammmmmaaaaaabcdefgabcdefg");

	// 4 FAST STROBE
	gi.configstring(CS_LIGHTS + 4, "mamamamamama");

	// 5 GENTLE PULSE 1
	gi.configstring(CS_LIGHTS + 5, "jklmnopqrstuvwxyzyxwvutsrqponmlkj");

	// 6 FLICKER (second variety)
	gi.configstring(CS_LIGHTS + 6, "nmonqnmomnmomomno");

	// 7 CANDLE (second variety)`map
	gi.configstring(CS_LIGHTS + 7, "mmmaaaabcdefgmmmmaaaammmaamm");

	// 8 CANDLE (third variety)
	gi.configstring(CS_LIGHTS + 8, "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa");

	// 9 SLOW STROBE (fourth variety)
	gi.configstring(CS_LIGHTS + 9, "aaaaaaaazzzzzzzz");

	// 10 FLUORESCENT FLICKER
	gi.configstring(CS_LIGHTS + 10, "mmamammmmammamamaaamammma");

	// 11 SLOW PULSE NOT FADE TO BLACK
	gi.configstring(CS_LIGHTS + 11, "abcdefghijklmnopqrrqponmlkjihgfedcba");

	// [Paril-KEX] 12 N64's 2 (fast strobe)
	gi.configstring(CS_LIGHTS + 12, "zzazazzzzazzazazaaazazzza");

	// [Paril-KEX] 13 N64's 3 (half of strong pulse)
	gi.configstring(CS_LIGHTS + 13, "abcdefghijklmnopqrstuvwxyz");

	// [Paril-KEX] 14 N64's 4 (fast strobe)
	gi.configstring(CS_LIGHTS + 14, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");

	// styles 32-62 are assigned by the light program for switchable lights

	// 63 testing
	gi.configstring(CS_LIGHTS + 63, "a");

	// coop respawn strings
	if (InCoopStyle()) {
		gi.configstring(CONFIG_COOP_RESPAWN_STRING + 0, "$g_coop_respawn_in_combat");
		gi.configstring(CONFIG_COOP_RESPAWN_STRING + 1, "$g_coop_respawn_bad_area");
		gi.configstring(CONFIG_COOP_RESPAWN_STRING + 2, "$g_coop_respawn_blocked");
		gi.configstring(CONFIG_COOP_RESPAWN_STRING + 3, "$g_coop_respawn_waiting");
		gi.configstring(CONFIG_COOP_RESPAWN_STRING + 4, "$g_coop_respawn_no_lives");
	}
}
