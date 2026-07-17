// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Entity classname spawn registry.
#include <initializer_list>

#include "g_local.h"
#include "brush_misc.h"
#include "clock.h"
#include "shadow_lights.h"
#include "teleporter.h"
#include "world_text.h"

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
void SP_info_nav_lock(gentity_t *self); // [Paril-KEX]
void SP_info_horde_spawn(gentity_t *self);
void SP_info_horde_flying_spawn(gentity_t *self);
void SP_info_horde_water_spawn(gentity_t *self);
void SP_info_horde_boss_spawn(gentity_t *self);

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
void SP_func_force_wall(gentity_t *ent);
void SP_func_timer(gentity_t *self);
void SP_func_areaportal(gentity_t *ent);
void SP_func_killbox(gentity_t *ent);
void SP_func_eye(gentity_t *ent); // [Paril-KEX]
void SP_func_spinning(gentity_t *ent); // [Paril-KEX]
void SP_func_bobbing(gentity_t *ent);
void SP_func_pendulum(gentity_t *ent);
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

void SP_rotating_light(gentity_t *self);
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
	{ "info_horde_spawn", SP_info_horde_spawn },
	{ "info_horde_flying_spawn", SP_info_horde_flying_spawn },
	{ "info_horde_water_spawn", SP_info_horde_water_spawn },
	{ "info_horde_boss_spawn", SP_info_horde_boss_spawn },

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
	{ "func_bobbing", SP_func_bobbing },
	{ "func_pendulum", SP_func_pendulum },
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

bool G_CallSpawnClass(gentity_t *ent) {
	for (const spawn_t &s : spawns) {
		if (!strcmp(s.name, ent->classname)) {
			s.spawn(ent);

			// Paril: swap classname with stored constant if we didn't change it
			if (strcmp(ent->classname, s.name) == 0)
				ent->classname = s.name;
			return true;
		}
	}

	return false;
}
