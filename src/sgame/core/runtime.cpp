// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "debug_log.h"
#include "entities/shadow_lights.h"
#include "muffmode/mm_announcer.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_captain.h"
#include "muffmode/mm_client_profile.h"
#include "muffmode/mm_combat_heatmap.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_duel.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_ghost.h"
#include "muffmode/mm_horde.h"
#include "muffmode/mm_loc.h"
#include "muffmode/mm_map_pool.h"
#include "muffmode/mm_maps.h"
#include "muffmode/mm_match.h"
#include "muffmode/mm_match_stats.h"
#include "muffmode/mm_motd.h"
#include "muffmode/mm_player_stats.h"
#include "muffmode/mm_profile.h"
#include "muffmode/mm_red_rover_rules.h"
#include "muffmode/mm_team.h"
#include "muffmode/mm_util.h"
#include "muffmode/mm_vote.h"
#include "bots/bot_includes.h"
#include "monsters/m_player.h"	// match starts

#include <cmath>
#include <ctime>

CHECK_GCLIENT_INTEGRITY;
CHECK_ENTITY_INTEGRITY;

constexpr int32_t DEFAULT_GRAPPLE_SPEED = 650; // speed of grapple in flight
constexpr float	  DEFAULT_GRAPPLE_PULL_SPEED = 650; // speed player is pulled at

std::mt19937 mt_rand;

game_locals_t  game;
level_locals_t level;

local_game_import_t  gi;

game_export_t  globals;
spawn_temp_t   st;

cached_modelindex		sm_meat_index;
cached_soundindex		snd_fry;

gentity_t *g_entities;

static size_t G_AllocatedEntityCount() {
	return min(static_cast<size_t>(globals.num_entities), static_cast<size_t>(game.maxentities));
}

static size_t G_FrameEntityCount() {
	return min(
		static_cast<size_t>(game.maxentities),
		static_cast<size_t>(globals.num_entities) + 1 + static_cast<size_t>(game.maxclients) + BODY_QUEUE_SIZE);
}

static gentity_t *G_PrimaryClientEntity() {
	if (!g_entities || game.maxclients < 1 || G_AllocatedEntityCount() <= 1)
		return nullptr;

	gentity_t *ent = &g_entities[1];
	return (ent->inuse && ent->client) ? ent : nullptr;
}

static uint32_t G_ClampGameLimit(const char *name, int value, uint32_t min_value, uint32_t max_value) {
	const int64_t raw_value = value;
	const uint32_t clamped_value = static_cast<uint32_t>(clamp<int64_t>(raw_value, min_value, max_value));

	if (raw_value != clamped_value) {
		gi.Com_PrintFmt("{}: clamped {} from {} to {}\n", __FUNCTION__, name, raw_value, clamped_value);
	}

	return clamped_value;
}

cvar_t *hostname;

cvar_t *deathmatch;
cvar_t *ctf;
cvar_t *teamplay;
cvar_t *g_gametype;

cvar_t *coop;

cvar_t *skill;
cvar_t *fraglimit;
cvar_t *capturelimit;
cvar_t *timelimit;
cvar_t *roundlimit;
cvar_t *roundtimelimit;
cvar_t *mercylimit;
cvar_t *noplayerstime;

cvar_t *g_ruleset;

cvar_t *password;
cvar_t *spectator_password;
cvar_t *admin_password;
cvar_t *needpass;

static cvar_t *maxclients;
static cvar_t *maxentities;
cvar_t *maxplayers;
cvar_t *minplayers;

cvar_t *ai_allow_dm_spawn;
cvar_t *ai_damage_scale;
cvar_t *ai_model_scale;
cvar_t *ai_movement_disabled;
cvar_t *bob_pitch;
cvar_t *bob_roll;
cvar_t *bob_up;
cvar_t *bot_debug_follow_actor;
cvar_t *bot_debug_move_to_point;
cvar_t *flood_msgs;
cvar_t *flood_persecond;
cvar_t *flood_waitdelay;
cvar_t *gun_x, *gun_y, *gun_z;
cvar_t *run_pitch;
cvar_t *run_roll;

cvar_t *g_airaccelerate;
cvar_t *g_allow_admin;
cvar_t *g_allow_custom_skins;
cvar_t *g_allow_skin_overrides;
cvar_t *g_team_force_models;
cvar_t *g_team_red_model;
cvar_t *g_team_blue_model;
cvar_t *g_allow_forfeit;
cvar_t *g_allow_grapple;
cvar_t *g_allow_kill;
cvar_t *g_allow_mymap;
cvar_t *g_allow_spec_vote;
cvar_t *g_allow_techs;
cvar_t *g_allow_vote_midgame;
cvar_t *g_allow_voting;
cvar_t *g_arena_dmg_armor;
cvar_t *g_arena_start_armor;
cvar_t *g_arena_start_health;
cvar_t *g_arena_players_per_team;
cvar_t *g_arena_rounds;
cvar_t *g_arena_falling_damage;
cvar_t *g_arena_weapon_mask;
cvar_t *g_arena_ammo_shells;
cvar_t *g_arena_ammo_bullets;
cvar_t *g_arena_ammo_grenades;
cvar_t *g_arena_ammo_rockets;
cvar_t *g_arena_ammo_cells;
cvar_t *g_arena_ammo_slugs;
cvar_t *g_arena_config;
cvar_t *g_arena_legacy_idmap;
cvar_t *g_arena_default_type;
cvar_t *g_arena_health_protect;
cvar_t *g_arena_armor_protect;
cvar_t *g_arena_fast_switch;
cvar_t *g_arena_grapple;
cvar_t *g_arena_excessive;
cvar_t *g_arena_competition;
cvar_t *g_arena_unbalanced;
cvar_t *g_arena_lock;
cvar_t *g_arena_lock_count;
cvar_t *g_arena_max_players;
cvar_t *g_arena_vote_time;
cvar_t *g_arena_timeouts;
cvar_t *g_arena_rocket_speed;
cvar_t *g_auto_ghost_max;
cvar_t *g_auto_ghost_time;
cvar_t *g_auto_ghost_timeout;
cvar_t *g_cheats;
cvar_t *g_coop_enable_lives;
cvar_t *g_coop_health_scaling;
cvar_t *g_coop_instanced_items;
cvar_t *g_coop_num_lives;
cvar_t *g_coop_player_collision;
cvar_t *g_coop_squad_respawn;
cvar_t *g_corpse_sink_time;
cvar_t *g_damage_scale;
cvar_t *g_debug_monster_kills;
cvar_t *g_debug_monster_paths;
cvar_t *g_dedicated;
cvar_t *g_disable_player_collision;
cvar_t *g_dm_allow_exit;
cvar_t *g_dm_allow_no_humans;
cvar_t *g_dm_auto_join;
cvar_t *g_dm_crosshair_id;
cvar_t *g_dm_death_scoreboard;
cvar_t *g_dm_do_readyup;
cvar_t *g_dm_do_warmup;
cvar_t *g_dm_exec_level_cfg;
cvar_t *g_dm_force_join;
cvar_t *g_dm_force_respawn;
cvar_t *g_dm_force_respawn_time;
cvar_t *g_dm_holdable_adrenaline;
cvar_t *g_dm_instant_items;
cvar_t *g_dm_intermission_shots;
cvar_t *g_dm_item_respawn_rate;
cvar_t *g_dm_no_fall_damage;
cvar_t *g_dm_no_quad_drop;
cvar_t *g_dm_no_self_damage;
cvar_t *g_dm_no_stack_double;
cvar_t *g_dm_overtime;
cvar_t *g_dm_tie_max_time;
cvar_t *g_dm_powerup_drop;
cvar_t *g_dm_powerups_minplayers;
cvar_t *g_dm_random_items;
cvar_t *g_dm_respawn_delay_min;
cvar_t *g_dm_respawn_point_min_dist;
cvar_t *g_dm_respawn_point_min_dist_debug;
cvar_t *g_dm_same_level;
cvar_t *g_dm_spawn_farthest;
cvar_t *g_dm_spawnpads;
cvar_t *g_dm_strong_mines;
cvar_t *g_dm_timeout_length;
cvar_t *g_dm_timeout_resume_countdown;
cvar_t *g_dm_weapons_stay;
cvar_t *g_drop_cmds;
cvar_t *g_entity_override_dir;
cvar_t *g_entity_override_load;
cvar_t *g_entity_override_save;
cvar_t *g_fast_doors;
cvar_t *g_frag_messages;
cvar_t *g_freezetag_arena_loadout;
cvar_t *g_freezetag_auto_thaw_time;
cvar_t *g_freezetag_bot_rescue;
cvar_t *g_freezetag_frozen_knockback_scale;
cvar_t *g_freezetag_multi_thaw_scale;
cvar_t *g_freezetag_round_reset_alive_inventory;
cvar_t *g_freezetag_round_respawn_all;
cvar_t *g_freezetag_thaw_radius;
cvar_t *g_freezetag_thaw_respawn_at_location;
cvar_t *g_freezetag_thaw_time;
cvar_t *g_frenzy;
cvar_t *g_friendly_fire;
cvar_t *g_grapple_damage;
cvar_t *g_grapple_fly_speed;
cvar_t *g_grapple_offhand;
cvar_t *g_grapple_pull_speed;
cvar_t *g_gravity;
cvar_t *g_horde_starting_wave;
cvar_t *g_horde_points_base;
cvar_t *g_horde_points_per_wave;
cvar_t *g_horde_points_min;
cvar_t *g_horde_points_max;
cvar_t *g_horde_spawn_interval_min;
cvar_t *g_horde_spawn_interval_max;
cvar_t *g_horde_spawn_burst_count;
cvar_t *g_horde_spawn_burst_rest;
cvar_t *g_horde_warmup_cap;
cvar_t *g_horde_max_alive;
cvar_t *g_horde_wave_spawn_delay_ms;
cvar_t *g_horde_player_scale;
cvar_t *g_horde_player_scale_factor;
cvar_t *g_horde_player_scale_max;
cvar_t *g_horde_lives;
cvar_t *g_horde_featured_spawns;
cvar_t *g_horde_wave_type_ramp;
cvar_t *g_lms_lives;
cvar_t *g_horde_mark_monsters_threshold;
cvar_t *g_horde_mark_monsters_max;
cvar_t *g_horde_map_scale;
cvar_t *g_horde_map_scale_ref;
cvar_t *g_horde_map_scale_factor;
cvar_t *g_horde_champions;
cvar_t *g_horde_champion_max_per_run;
cvar_t *g_horde_champion_chance;
cvar_t *g_horde_champion_min_wave;
cvar_t *g_horde_champion_health_mult;
cvar_t *g_horde_champion_health_floor;
cvar_t *g_horde_champion_health_per_wave;
cvar_t *g_horde_champion_damage_mult;
cvar_t *g_horde_champion_speed_mult;
cvar_t *g_horde_champion_strong_ratio;
cvar_t *g_horde_champion_force; // DEBUG/TEST: force a champion every wave
cvar_t *g_horde_boss_waves;
cvar_t *g_horde_boss_min_wave;
cvar_t *g_horde_boss_interval;
cvar_t *g_horde_boss_budget_mult;
cvar_t *g_horde_boss_health_mult;
cvar_t *g_horde_boss_damage_mult;
cvar_t *g_horde_boss_tier_window;
cvar_t *g_horde_boss_powerup_chance;
cvar_t *g_horde_boss_machinegames;
cvar_t *g_horde_boss_pairs;
cvar_t *g_horde_boss_repeat_window;
cvar_t *g_horde_boss_force;
cvar_t *g_horde_boss_scale_limit;
cvar_t *g_horde_boss_health_per_wave;
cvar_t *g_horde_boss_damage_per_wave;
cvar_t *g_horde_boss_pair_health_mult;
cvar_t *g_horde_boss_armor_mult;
cvar_t *g_horde_themed_waves;
cvar_t *g_horde_theme_chance;
cvar_t *g_horde_theme_min_wave;
cvar_t *g_horde_wave_variety;
cvar_t *g_horde_wave_min_types;
cvar_t *g_horde_content_peak_wave;
cvar_t *g_horde_late_wave_factor;
cvar_t *g_horde_late_escalation;
cvar_t *g_horde_late_budget_factor;
cvar_t *g_horde_late_max_alive_per_wave;
cvar_t *g_horde_late_max_alive_cap;
cvar_t *g_horde_weight_floor;
cvar_t *g_horde_theme_min_monsters;
cvar_t *g_horde_monster_edge_drops;
cvar_t *g_horde_preset_allow_boss_waves;
cvar_t *g_horde_preset_chance;
cvar_t *g_horde_preset_weight_clone_army;
cvar_t *g_horde_preset_weight_funhouse_horde;
cvar_t *g_horde_preset_weight_get_over_here;
cvar_t *g_horde_preset_weight_giant_horde;
cvar_t *g_horde_preset_weight_glass_cannon;
cvar_t *g_horde_preset_weight_low_gravity;
cvar_t *g_horde_preset_weight_tiny_shamblers;
cvar_t *g_horde_preset_weight_tiny_terror;
cvar_t *g_horde_preset_weight_pinball_night;
cvar_t *g_horde_preset_weight_sawstorm;
cvar_t *g_horde_start_chainsaw;
cvar_t *g_horde_item_respawn_scale;
cvar_t *g_horde_tech_reset_each_wave;
cvar_t *g_horde_tech_relocate;
cvar_t *g_horde_tech_count;
cvar_t *g_horde_tech_drop_on_death;
cvar_t *g_horde_tech_spawn_anywhere;
cvar_t *g_horde_tech_duration;
cvar_t *g_horde_tech_unique;
cvar_t *g_horde_enhanced_ai;
cvar_t *g_horde_water_spawns;
cvar_t *g_horde_water_spawn_chance;
cvar_t *g_horde_water_max_alive;
cvar_t *g_horde_map_monster_spawns;
cvar_t *g_horde_map_spawn_chance;
cvar_t *g_horde_map_spawn_cooldown;
cvar_t *g_horde_map_spawn_min_dist;
cvar_t *g_horde_drop_chance;
cvar_t *g_horde_drop_profile_bias;
cvar_t *g_horde_champion_drop_chance;
cvar_t *g_horde_streak_step;
cvar_t *g_horde_streak_max_tier;
cvar_t *g_horde_streak_score_bonus;
cvar_t *g_horde_streak_drop_bonus;
cvar_t *g_horde_streak_upgrade_chance;
cvar_t *g_horde_momentum_messages;
cvar_t *g_horde_wave_survival_bonus;
cvar_t *g_horde_wave_flawless_message;
cvar_t *g_horde_reinforcement_kills;
cvar_t *g_horde_reinforcements_per_wave;
cvar_t *g_horde_reinforcement_protection;
cvar_t *g_horde_target_spread_weight;
cvar_t *g_horde_retarget_interval;
cvar_t *g_horde_stall_timeout;
cvar_t *g_huntercam;
cvar_t *g_inactivity;
cvar_t *g_infinite_ammo;
cvar_t *g_instagib;
cvar_t *g_instagib_splash;
cvar_t *g_instant_weapon_switch;
cvar_t *g_item_bobbing;
cvar_t *g_knockback_scale;
cvar_t *g_ladder_steps;
cvar_t *g_lag_compensation;
cvar_t *g_lag_compensation_enhanced;
cvar_t *g_map_list;
cvar_t *g_map_list_shuffle;
cvar_t *g_map_pool;
cvar_t *g_maps_pool_file;
cvar_t *g_maps_cycle_file;
cvar_t *g_maps_random;
cvar_t *g_maps_repeat_delay;
cvar_t *g_votable_gametypes;
cvar_t *g_votable_rulesets;
cvar_t *g_match_lock;
cvar_t *g_matchstats;
cvar_t *g_maxvelocity;
cvar_t *g_loc;
cvar_t *g_loc_items;
cvar_t *g_motd_filename;
cvar_t *g_mover_debug;
cvar_t *g_mover_speed_scale;
cvar_t *g_nadefest;
cvar_t *g_no_armor;
cvar_t *g_no_health;
cvar_t *g_no_items;
cvar_t *g_no_mines;
cvar_t *g_no_nukes;
cvar_t *g_no_powerups;
cvar_t *g_mapspawn_no_bfg;
cvar_t *g_mapspawn_no_plasmabeam;
cvar_t *g_no_spheres;
cvar_t *g_owner_auto_join;
cvar_t *g_owner_push_scores;
cvar_t *g_gametype_cfg;
cvar_t *g_quadhog;
cvar_t *g_quick_weapon_switch;
cvar_t *g_rollangle;
cvar_t *g_rollspeed;
cvar_t *g_round_countdown;
cvar_t *g_select_empty;
cvar_t *g_showhelp;
cvar_t *g_showmotd;
cvar_t *g_skip_view_modifiers;
cvar_t *g_start_items;
cvar_t *g_starting_health;
cvar_t *g_starting_health_bonus;
cvar_t *g_starting_armor;
cvar_t *g_stopspeed;
cvar_t *g_strict_saves;
cvar_t *g_teamplay_allow_team_pick;
cvar_t *g_teamplay_armor_protect;
cvar_t *g_teamplay_auto_balance;
cvar_t *g_teamplay_force_balance;
cvar_t *g_teamplay_item_drop_notice;
cvar_t *g_teleporter_freeze;
cvar_t *g_vampiric_damage;
cvar_t *g_vampiric_exp_min;
cvar_t *g_vampiric_health_max;
cvar_t *g_vampiric_percentile;
cvar_t *g_verbose;
cvar_t *g_vote_flags;
cvar_t *g_vote_limit;
cvar_t *g_muffmode_debug;
cvar_t *g_warmup_countdown;
cvar_t *g_warmup_ready_percentage;
cvar_t *g_weapon_projection;
cvar_t *g_weapon_respawn_time;
// Weapon balance cvars - declared in both builds, but only initialized in DEBUG
cvar_t *g_weapon_balance_dev;
cvar_t *g_chaingun_max_shots;
cvar_t *g_chaingun_damage;
	cvar_t *g_chaingun_hspread;
	cvar_t *g_chaingun_vspread;
	cvar_t *g_chaingun_spread_offset;
	cvar_t *g_machinegun_damage;
	cvar_t *g_machinegun_hspread;
	cvar_t *g_machinegun_vspread;
	cvar_t *g_hyperblaster_speed;
	cvar_t *g_railgun_damage;
	cvar_t *g_rocketlauncher_damage;
	cvar_t *g_rocketlauncher_speed;

cvar_t *bot_name_prefix;

static cvar_t *g_frames_per_frame;

int ii_duel_header;
int ii_highlight;
int ii_ctf_red_dropped;
int ii_ctf_blue_dropped;
int ii_ctf_red_taken;
int ii_ctf_blue_taken;
int ii_teams_red_default;
int ii_teams_blue_default;
int ii_teams_red_tiny;
int ii_teams_blue_tiny;
int ii_teams_header_red;
int ii_teams_header_blue;
int mi_ctf_red_flag, mi_ctf_blue_flag; // [Paril-KEX]

void ClientThink(gentity_t *ent, usercmd_t *cmd);
gentity_t *ClientChooseSlot(const char *userinfo, const char *social_id, bool is_bot, gentity_t **ignore, size_t num_ignore, bool cinematic);
bool ClientConnect(gentity_t *ent, char *userinfo, const char *social_id, bool is_bot);
char *WriteGameJson(bool autosave, size_t *out_size);
void ReadGameJson(const char *jsonString);
char *WriteLevelJson(bool transition, size_t *out_size);
void ReadLevelJson(const char *jsonString);
bool CanSave();
void ClientDisconnect(gentity_t *ent);
void ClientBegin(gentity_t *ent);
void ClientCommand(gentity_t *ent);
void G_RunFrame(bool main_loop);
void G_PrepFrame();
void InitSave();

#include <chrono>

int _gt[] = {
	/* GT_NONE */ 0,
	/* GT_FFA */ GTF_FRAGS,
	/* GT_DUEL */ GTF_FRAGS,
	/* GT_TDM */ GTF_TEAMS | GTF_FRAGS,
	/* GT_CTF */ GTF_TEAMS | GTF_CTF,
	/* GT_CA */ GTF_TEAMS | GTF_ARENA | GTF_ROUNDS | GTF_ELIMINATION,
	/* GT_FREEZE */ GTF_TEAMS | GTF_ROUNDS,
	/* GT_STRIKE */ GTF_TEAMS | GTF_ARENA | GTF_ROUNDS | GTF_CTF | GTF_ELIMINATION,
	/* GT_RR */ GTF_TEAMS | GTF_ARENA | GTF_ROUNDS | GTF_FRAGS,
	/* GT_LMS */ GTF_ARENA | GTF_ROUNDS | GTF_ELIMINATION,
	/* GT_HORDE */ GTF_ROUNDS,
	/* GT_BALL */ 0, // removed
	/* GT_INSTAGIB */ GTF_FRAGS,
	/* GT_NADEFEST */ GTF_FRAGS,
	/* GT_ARENA */ GTF_ARENA | GTF_MULTI_ARENA
};

// =================================================

static void CheckRuleset() {
	MM_CheckRuleset();
}

static void InitGametype() {
	constexpr const char *COOP = "coop";
	bool force_dm = false;

	MM_SanitizeCurrentGametype();

	if (ctf->integer) {
		force_dm = true;
		// force coop off
		if (coop->integer)
			gi.cvar_set(COOP, "0");
		// force tdm off
		if (teamplay->integer)
			gi.cvar_set("teamplay", "0");
	}
	if (teamplay->integer) {
		force_dm = true;
		// force coop off
		if (coop->integer)
			gi.cvar_set(COOP, "0");
	}

	if (force_dm && !deathmatch->integer) {
		gi.Com_Print("Forcing deathmatch.\n");
		gi.cvar_forceset("deathmatch", "1");
	}

	// force even maxplayers value during teamplay
	if (Teams()) {
		int pmax = maxplayers->integer;

		if (pmax > 0 && (pmax % 2) != 0)
			gi.cvar_set("maxplayers", G_Fmt("{}", pmax - 1).data());
	}
}

void ChangeGametype(gametype_t gt) {
	MM_ChangeGametype(gt);
}

void GT_Changes() {
	MM_GTChanges();
}

/*
============
PreInitGame

This will be called when the dll is first loaded, which
only happens when a new game is started or a save game
is loaded.
============
*/
static void PreInitGame() {
	maxclients = gi.cvar("maxclients", G_Fmt("{}", MAX_SPLIT_PLAYERS).data(), CVAR_SERVERINFO | CVAR_LATCH);
	const uint32_t supported_maxclients = MM_ClampLobbyPlayerCount(maxclients->integer);
	if (static_cast<int64_t>(maxclients->integer) != supported_maxclients) {
		gi.Com_PrintFmt("{}: clamped maxclients from {} to {}\n",
			__FUNCTION__, maxclients->integer, supported_maxclients);
		// The engine performs its final client-slab sizing after PreInit returns.
		// Force the latched cvar now so engine and game allocations cannot diverge.
		maxclients = gi.cvar_forceset(
			"maxclients", G_Fmt("{}", supported_maxclients).data());
	}
	minplayers = gi.cvar("minplayers", "2", CVAR_NOFLAGS);
	maxplayers = gi.cvar("maxplayers", "16", CVAR_NOFLAGS);

	deathmatch = gi.cvar("deathmatch", "1", CVAR_LATCH);
	teamplay = gi.cvar("teamplay", "0", CVAR_SERVERINFO);
	ctf = gi.cvar("ctf", "0", CVAR_SERVERINFO);
	g_gametype = gi.cvar("g_gametype", G_Fmt("{}", (int)GT_FFA).data(), CVAR_SERVERINFO);
	coop = gi.cvar("coop", "0", CVAR_LATCH);
	InitGametype();
}

// Tracks whether the map list has been shuffled for the current gametype session.
// Reset by ChangeGametype() so the list gets reshuffled on gametype change.
bool g_map_list_shuffled = false;

/*
============
InitGame

Called after PreInitGame when the game has set up cvars.
============
*/
static void InitGame() {
	gi.Com_Print("==== InitGame ====\n");

	InitSave();

	// seed RNG
	mt_rand.seed((uint32_t)std::chrono::system_clock::now().time_since_epoch().count());
	muffmode::combat_heatmap::Init();

	hostname = gi.cvar("hostname", "Welcome to Muff Mode!", CVAR_NOFLAGS);

	gun_x = gi.cvar("gun_x", "0", CVAR_NOFLAGS);
	gun_y = gi.cvar("gun_y", "0", CVAR_NOFLAGS);
	gun_z = gi.cvar("gun_z", "0", CVAR_NOFLAGS);

	g_rollspeed = gi.cvar("g_rollspeed", "200", CVAR_NOFLAGS);
	g_rollangle = gi.cvar("g_rollangle", "2", CVAR_NOFLAGS);
	g_maxvelocity = gi.cvar("g_maxvelocity", "2000", CVAR_NOFLAGS);
	g_gravity = gi.cvar("g_gravity", "800", CVAR_NOFLAGS);

	g_skip_view_modifiers = gi.cvar("g_skip_view_modifiers", "0", CVAR_NOSET);

	g_stopspeed = gi.cvar("g_stopspeed", "100", CVAR_NOFLAGS);

	// [MuffMode] Horde mode cvars (logic lives in muffmode/mm_horde)
	g_horde_starting_wave = gi.cvar("g_horde_starting_wave", "1", CVAR_SERVERINFO | CVAR_LATCH);
	g_horde_points_base = gi.cvar("g_horde_points_base", "15", CVAR_NOFLAGS);
	g_horde_points_per_wave = gi.cvar("g_horde_points_per_wave", "5", CVAR_NOFLAGS);
	g_horde_points_min = gi.cvar("g_horde_points_min", "0", CVAR_NOFLAGS);
	g_horde_points_max = gi.cvar("g_horde_points_max", "0", CVAR_NOFLAGS);
	g_horde_spawn_interval_min = gi.cvar("g_horde_spawn_interval_min", "0.3", CVAR_NOFLAGS);
	g_horde_spawn_interval_max = gi.cvar("g_horde_spawn_interval_max", "0.5", CVAR_NOFLAGS);
	g_horde_spawn_burst_count = gi.cvar("g_horde_spawn_burst_count", "6", CVAR_NOFLAGS);
	g_horde_spawn_burst_rest = gi.cvar("g_horde_spawn_burst_rest", "2.0", CVAR_NOFLAGS);
	g_horde_warmup_cap = gi.cvar("g_horde_warmup_cap", "30", CVAR_NOFLAGS);
	g_horde_max_alive = gi.cvar("g_horde_max_alive", "60", CVAR_NOFLAGS);
	g_horde_wave_spawn_delay_ms = gi.cvar("g_horde_wave_spawn_delay_ms", "500", CVAR_NOFLAGS);
	g_horde_player_scale = gi.cvar("g_horde_player_scale", "1", CVAR_NOFLAGS);
	g_horde_player_scale_factor = gi.cvar("g_horde_player_scale_factor", "0.4", CVAR_NOFLAGS);
	g_horde_player_scale_max = gi.cvar("g_horde_player_scale_max", "8", CVAR_NOFLAGS);
	g_horde_lives = gi.cvar("g_horde_lives", "1", CVAR_NOFLAGS);
	g_horde_featured_spawns = gi.cvar("g_horde_featured_spawns", "3", CVAR_NOFLAGS);
	g_horde_wave_type_ramp = gi.cvar("g_horde_wave_type_ramp", "3", CVAR_NOFLAGS);
	g_lms_lives = gi.cvar("g_lms_lives", "1", CVAR_NOFLAGS);
	g_horde_mark_monsters_threshold = gi.cvar("g_horde_mark_monsters_threshold", "3", CVAR_NOFLAGS);
	g_horde_mark_monsters_max = gi.cvar("g_horde_mark_monsters_max", "8", CVAR_NOFLAGS);
	g_horde_map_scale = gi.cvar("g_horde_map_scale", "1", CVAR_NOFLAGS);
	g_horde_map_scale_ref = gi.cvar("g_horde_map_scale_ref", "4000", CVAR_NOFLAGS);
	g_horde_map_scale_factor = gi.cvar("g_horde_map_scale_factor", "0.5", CVAR_NOFLAGS);
	g_horde_champions = gi.cvar("g_horde_champions", "1", CVAR_NOFLAGS);
	g_horde_champion_max_per_run = gi.cvar("g_horde_champion_max_per_run", "2", CVAR_NOFLAGS);
	g_horde_champion_chance = gi.cvar("g_horde_champion_chance", "0.6", CVAR_NOFLAGS);
	g_horde_champion_min_wave = gi.cvar("g_horde_champion_min_wave", "3", CVAR_NOFLAGS);
	g_horde_champion_health_mult = gi.cvar("g_horde_champion_health_mult", "3.0", CVAR_NOFLAGS);
	g_horde_champion_health_floor = gi.cvar("g_horde_champion_health_floor", "400", CVAR_NOFLAGS);
	g_horde_champion_health_per_wave = gi.cvar("g_horde_champion_health_per_wave", "25", CVAR_NOFLAGS);
	g_horde_champion_damage_mult = gi.cvar("g_horde_champion_damage_mult", "2.0", CVAR_NOFLAGS);
	g_horde_champion_speed_mult = gi.cvar("g_horde_champion_speed_mult", "1.25", CVAR_NOFLAGS);
	g_horde_champion_strong_ratio = gi.cvar("g_horde_champion_strong_ratio", "4.0", CVAR_NOFLAGS);
	g_horde_champion_force = gi.cvar("g_horde_champion_force", "0", CVAR_NOFLAGS); // DEBUG/TEST: 1 = champion every wave
	g_horde_boss_waves = gi.cvar("g_horde_boss_waves", "1", CVAR_NOFLAGS);
	g_horde_boss_min_wave = gi.cvar("g_horde_boss_min_wave", "6", CVAR_NOFLAGS);
	g_horde_boss_interval = gi.cvar("g_horde_boss_interval", "6", CVAR_NOFLAGS);
	g_horde_boss_budget_mult = gi.cvar("g_horde_boss_budget_mult", "0.8", CVAR_NOFLAGS);
	g_horde_boss_health_mult = gi.cvar("g_horde_boss_health_mult", "1.0", CVAR_NOFLAGS);
	g_horde_boss_damage_mult = gi.cvar("g_horde_boss_damage_mult", "1.15", CVAR_NOFLAGS);
	g_horde_boss_tier_window = gi.cvar("g_horde_boss_tier_window", "3", CVAR_NOFLAGS);
	g_horde_boss_powerup_chance = gi.cvar("g_horde_boss_powerup_chance", "0.35", CVAR_NOFLAGS);
	g_horde_boss_machinegames = gi.cvar("g_horde_boss_machinegames", "1", CVAR_NOFLAGS);
	g_horde_boss_pairs = gi.cvar("g_horde_boss_pairs", "1", CVAR_NOFLAGS);
	g_horde_boss_repeat_window = gi.cvar("g_horde_boss_repeat_window", "2", CVAR_NOFLAGS);
	g_horde_boss_force = gi.cvar("g_horde_boss_force", "", CVAR_NOFLAGS);
	g_horde_boss_scale_limit = gi.cvar("g_horde_boss_scale_limit", "2.5", CVAR_NOFLAGS);
	g_horde_boss_health_per_wave = gi.cvar("g_horde_boss_health_per_wave", "0.05", CVAR_NOFLAGS);
	g_horde_boss_damage_per_wave = gi.cvar("g_horde_boss_damage_per_wave", "0.01", CVAR_NOFLAGS);
	g_horde_boss_pair_health_mult = gi.cvar("g_horde_boss_pair_health_mult", "1.0", CVAR_NOFLAGS);
	g_horde_boss_armor_mult = gi.cvar("g_horde_boss_armor_mult", "1.0", CVAR_NOFLAGS);
	g_horde_themed_waves = gi.cvar("g_horde_themed_waves", "1", CVAR_NOFLAGS);
	g_horde_theme_chance = gi.cvar("g_horde_theme_chance", "0.20", CVAR_NOFLAGS);
	g_horde_theme_min_wave = gi.cvar("g_horde_theme_min_wave", "4", CVAR_NOFLAGS);
	g_horde_wave_variety = gi.cvar("g_horde_wave_variety", "1", CVAR_NOFLAGS);
	g_horde_wave_min_types = gi.cvar("g_horde_wave_min_types", "3", CVAR_NOFLAGS);
	g_horde_content_peak_wave = gi.cvar("g_horde_content_peak_wave", "12", CVAR_NOFLAGS);
	g_horde_late_wave_factor = gi.cvar("g_horde_late_wave_factor", "0.35", CVAR_NOFLAGS);
	g_horde_late_escalation = gi.cvar("g_horde_late_escalation", "1", CVAR_NOFLAGS);
	g_horde_late_budget_factor = gi.cvar("g_horde_late_budget_factor", "0.6", CVAR_NOFLAGS);
	g_horde_late_max_alive_per_wave = gi.cvar("g_horde_late_max_alive_per_wave", "2", CVAR_NOFLAGS);
	g_horde_late_max_alive_cap = gi.cvar("g_horde_late_max_alive_cap", "70", CVAR_NOFLAGS);
	g_horde_weight_floor = gi.cvar("g_horde_weight_floor", "0.12", CVAR_NOFLAGS);
	g_horde_theme_min_monsters = gi.cvar("g_horde_theme_min_monsters", "2", CVAR_NOFLAGS);
	g_horde_monster_edge_drops = gi.cvar("g_horde_monster_edge_drops", "1", CVAR_NOFLAGS);
	// Wildcard Waves are deliberately opt-in. Individual weights do nothing while chance is 0.
	g_horde_preset_allow_boss_waves = gi.cvar("g_horde_preset_allow_boss_waves", "0", CVAR_NOFLAGS);
	g_horde_preset_chance = gi.cvar("g_horde_preset_chance", "0", CVAR_NOFLAGS);
	g_horde_preset_weight_clone_army = gi.cvar("g_horde_preset_weight_clone_army", "7", CVAR_NOFLAGS);
	g_horde_preset_weight_funhouse_horde = gi.cvar("g_horde_preset_weight_funhouse_horde", "5", CVAR_NOFLAGS);
	g_horde_preset_weight_get_over_here = gi.cvar("g_horde_preset_weight_get_over_here", "4", CVAR_NOFLAGS);
	g_horde_preset_weight_giant_horde = gi.cvar("g_horde_preset_weight_giant_horde", "4", CVAR_NOFLAGS);
	g_horde_preset_weight_glass_cannon = gi.cvar("g_horde_preset_weight_glass_cannon", "8", CVAR_NOFLAGS);
	g_horde_preset_weight_low_gravity = gi.cvar("g_horde_preset_weight_low_gravity", "3", CVAR_NOFLAGS);
	g_horde_preset_weight_tiny_shamblers = gi.cvar("g_horde_preset_weight_tiny_shamblers", "4", CVAR_NOFLAGS);
	g_horde_preset_weight_tiny_terror = gi.cvar("g_horde_preset_weight_tiny_terror", "10", CVAR_NOFLAGS);
	g_horde_preset_weight_pinball_night = gi.cvar("g_horde_preset_weight_pinball_night", "4", CVAR_NOFLAGS);
	g_horde_preset_weight_sawstorm = gi.cvar("g_horde_preset_weight_sawstorm", "5", CVAR_NOFLAGS);
	g_horde_start_chainsaw = gi.cvar("g_horde_start_chainsaw", "1", CVAR_NOFLAGS);
	g_horde_item_respawn_scale = gi.cvar("g_horde_item_respawn_scale", "4", CVAR_NOFLAGS);
	g_horde_tech_reset_each_wave = gi.cvar("g_horde_tech_reset_each_wave", "1", CVAR_NOFLAGS);
	g_horde_tech_relocate = gi.cvar("g_horde_tech_relocate", "0", CVAR_NOFLAGS);
	g_horde_tech_count = gi.cvar("g_horde_tech_count", "0", CVAR_NOFLAGS);
	g_horde_tech_drop_on_death = gi.cvar("g_horde_tech_drop_on_death", "1", CVAR_NOFLAGS);
	g_horde_tech_spawn_anywhere = gi.cvar("g_horde_tech_spawn_anywhere", "1", CVAR_NOFLAGS);
	g_horde_tech_duration = gi.cvar("g_horde_tech_duration", "30", CVAR_NOFLAGS);
	g_horde_tech_unique = gi.cvar("g_horde_tech_unique", "0", CVAR_NOFLAGS);
	g_horde_enhanced_ai = gi.cvar("g_horde_enhanced_ai", "1", CVAR_NOFLAGS);
	g_horde_water_spawns = gi.cvar("g_horde_water_spawns", "1", CVAR_NOFLAGS);
	g_horde_water_spawn_chance = gi.cvar("g_horde_water_spawn_chance", "0.30", CVAR_NOFLAGS);
	g_horde_water_max_alive = gi.cvar("g_horde_water_max_alive", "4", CVAR_NOFLAGS);
	g_horde_map_monster_spawns = gi.cvar("g_horde_map_monster_spawns", "1", CVAR_NOFLAGS);
	g_horde_map_spawn_chance = gi.cvar("g_horde_map_spawn_chance", "0.75", CVAR_NOFLAGS);
	g_horde_map_spawn_cooldown = gi.cvar("g_horde_map_spawn_cooldown", "3.0", CVAR_NOFLAGS);
	g_horde_map_spawn_min_dist = gi.cvar("g_horde_map_spawn_min_dist", "192", CVAR_NOFLAGS);
	g_horde_drop_chance = gi.cvar("g_horde_drop_chance", "0.35", CVAR_NOFLAGS);
	g_horde_drop_profile_bias = gi.cvar("g_horde_drop_profile_bias", "0.85", CVAR_NOFLAGS);
	g_horde_champion_drop_chance = gi.cvar("g_horde_champion_drop_chance", "1.0", CVAR_NOFLAGS);
	g_horde_streak_step = gi.cvar("g_horde_streak_step", "5", CVAR_NOFLAGS);
	g_horde_streak_max_tier = gi.cvar("g_horde_streak_max_tier", "3", CVAR_NOFLAGS);
	g_horde_streak_score_bonus = gi.cvar("g_horde_streak_score_bonus", "1", CVAR_NOFLAGS);
	g_horde_streak_drop_bonus = gi.cvar("g_horde_streak_drop_bonus", "0.08", CVAR_NOFLAGS);
	g_horde_streak_upgrade_chance = gi.cvar("g_horde_streak_upgrade_chance", "0.20", CVAR_NOFLAGS);
	g_horde_momentum_messages = gi.cvar("g_horde_momentum_messages", "0", CVAR_NOFLAGS);
	g_horde_wave_survival_bonus = gi.cvar("g_horde_wave_survival_bonus", "2", CVAR_NOFLAGS);
	g_horde_wave_flawless_message = gi.cvar("g_horde_wave_flawless_message", "1", CVAR_NOFLAGS);
	g_horde_reinforcement_kills = gi.cvar("g_horde_reinforcement_kills", "12", CVAR_NOFLAGS);
	g_horde_reinforcements_per_wave = gi.cvar("g_horde_reinforcements_per_wave", "1", CVAR_NOFLAGS);
	g_horde_reinforcement_protection = gi.cvar("g_horde_reinforcement_protection", "2.0", CVAR_NOFLAGS);
	g_horde_target_spread_weight = gi.cvar("g_horde_target_spread_weight", "512", CVAR_NOFLAGS);
	g_horde_retarget_interval = gi.cvar("g_horde_retarget_interval", "8.0", CVAR_NOFLAGS);
	g_horde_stall_timeout = gi.cvar("g_horde_stall_timeout", "90", CVAR_NOFLAGS);

	g_huntercam = gi.cvar("g_huntercam", "1", CVAR_SERVERINFO | CVAR_LATCH);
	g_dm_strong_mines = gi.cvar("g_dm_strong_mines", "0", CVAR_NOFLAGS);
	g_dm_random_items = gi.cvar("g_dm_random_items", "0", CVAR_NOFLAGS);

	// game modifications
	g_instagib = gi.cvar("g_instagib", "0", CVAR_SERVERINFO | CVAR_LATCH);
	g_instagib_splash = gi.cvar("g_instagib_splash", "0", CVAR_NOFLAGS);
	g_owner_auto_join = gi.cvar("g_owner_auto_join", "1", CVAR_NOFLAGS);
	g_owner_push_scores = gi.cvar("g_owner_push_scores", "0", CVAR_NOFLAGS);
	g_gametype_cfg = gi.cvar("g_gametype_cfg", "1", CVAR_NOFLAGS);
	g_quadhog = gi.cvar("g_quadhog", "0", CVAR_SERVERINFO | CVAR_LATCH);
	g_nadefest = gi.cvar("g_nadefest", "0", CVAR_SERVERINFO | CVAR_LATCH);
	g_frenzy = gi.cvar("g_frenzy", "0", CVAR_SERVERINFO | CVAR_LATCH);
	g_vampiric_damage = gi.cvar("g_vampiric_damage", "0", CVAR_NOFLAGS);
	g_vampiric_exp_min = gi.cvar("g_vampiric_exp_min", "0", CVAR_NOFLAGS);
	g_vampiric_health_max = gi.cvar("g_vampiric_health_max", "9999", CVAR_NOFLAGS);
	g_vampiric_percentile = gi.cvar("g_vampiric_percentile", "0.67f", CVAR_NOFLAGS);

	// [Paril-KEX]
	g_coop_player_collision = gi.cvar("g_coop_player_collision", "0", CVAR_LATCH);
	g_coop_squad_respawn = gi.cvar("g_coop_squad_respawn", "1", CVAR_LATCH);
	g_coop_enable_lives = gi.cvar("g_coop_enable_lives", "0", CVAR_LATCH);
	g_coop_num_lives = gi.cvar("g_coop_num_lives", "2", CVAR_LATCH);
	g_coop_instanced_items = gi.cvar("g_coop_instanced_items", "1", CVAR_LATCH);
	g_allow_grapple = gi.cvar("g_allow_grapple", "auto", CVAR_NOFLAGS);
	g_allow_kill = gi.cvar("g_allow_kill", "1", CVAR_NOFLAGS);
	g_grapple_offhand = gi.cvar("g_grapple_offhand", "0", CVAR_NOFLAGS);
	g_grapple_fly_speed = gi.cvar("g_grapple_fly_speed", G_Fmt("{}", DEFAULT_GRAPPLE_SPEED).data(), CVAR_NOFLAGS);
	g_grapple_pull_speed = gi.cvar("g_grapple_pull_speed", G_Fmt("{}", DEFAULT_GRAPPLE_PULL_SPEED).data(), CVAR_NOFLAGS);
	g_grapple_damage = gi.cvar("g_grapple_damage", "10", CVAR_NOFLAGS);

	g_frag_messages = gi.cvar("g_frag_messages", "1", CVAR_NOFLAGS);

	g_debug_monster_paths = gi.cvar("g_debug_monster_paths", "0", CVAR_NOFLAGS);
	g_debug_monster_kills = gi.cvar("g_debug_monster_kills", "0", CVAR_LATCH);

	bot_debug_follow_actor = gi.cvar("bot_debug_follow_actor", "0", CVAR_NOFLAGS);
	bot_debug_move_to_point = gi.cvar("bot_debug_move_to_point", "0", CVAR_NOFLAGS);

	// noset vars
	g_dedicated = gi.cvar("dedicated", "0", CVAR_NOSET);

	// latched vars
	g_cheats = gi.cvar("cheats",
#if defined(_DEBUG)
		"1"
#else
		"0"
#endif
		, CVAR_SERVERINFO | CVAR_LATCH);
	gi.cvar("gamename", GAMEVERSION, CVAR_SERVERINFO | CVAR_LATCH);

	skill = gi.cvar("skill", "3", CVAR_LATCH);
	maxentities = gi.cvar("maxentities", G_Fmt("{}", MAX_ENTITIES).data(), CVAR_LATCH);

	// change anytime vars
	fraglimit = gi.cvar("fraglimit", "0", CVAR_SERVERINFO);
	timelimit = gi.cvar("timelimit", "0", CVAR_SERVERINFO);
	roundlimit = gi.cvar("roundlimit", "8", CVAR_SERVERINFO);
	roundtimelimit = gi.cvar("roundtimelimit", "2", CVAR_SERVERINFO);
	capturelimit = gi.cvar("capturelimit", "8", CVAR_SERVERINFO);
	mercylimit = gi.cvar("mercylimit", "0", CVAR_NOFLAGS);
	noplayerstime = gi.cvar("noplayerstime", "10", CVAR_NOFLAGS);

	g_ruleset = gi.cvar("g_ruleset", G_Fmt("{}", (int)RS_Q2RE).data(), CVAR_SERVERINFO);

	password = gi.cvar("password", "", CVAR_USERINFO);
	spectator_password = gi.cvar("spectator_password", "", CVAR_USERINFO);
	admin_password = gi.cvar("admin_password", "", CVAR_NOFLAGS);
	needpass = gi.cvar("needpass", "0", CVAR_SERVERINFO);

	run_pitch = gi.cvar("run_pitch", "0.002", CVAR_NOFLAGS);
	run_roll = gi.cvar("run_roll", "0.005", CVAR_NOFLAGS);
	bob_up = gi.cvar("bob_up", "0.005", CVAR_NOFLAGS);
	bob_pitch = gi.cvar("bob_pitch", "0.002", CVAR_NOFLAGS);
	bob_roll = gi.cvar("bob_roll", "0.002", CVAR_NOFLAGS);

	flood_msgs = gi.cvar("flood_msgs", "4", CVAR_NOFLAGS);
	flood_persecond = gi.cvar("flood_persecond", "4", CVAR_NOFLAGS);
	flood_waitdelay = gi.cvar("flood_waitdelay", "10", CVAR_NOFLAGS);

	ai_allow_dm_spawn = gi.cvar("ai_allow_dm_spawn", "0", CVAR_NOFLAGS);
	ai_damage_scale = gi.cvar("ai_damage_scale", "1", CVAR_NOFLAGS);
	ai_model_scale = gi.cvar("ai_model_scale", "0", CVAR_NOFLAGS);
	ai_movement_disabled = gi.cvar("ai_movement_disabled", "0", CVAR_NOFLAGS);

	g_airaccelerate = gi.cvar("g_airaccelerate", "0", CVAR_NOFLAGS);
	g_allow_admin = gi.cvar("g_allow_admin", "1", CVAR_NOFLAGS);
	g_allow_custom_skins = gi.cvar("g_allow_custom_skins", "1", CVAR_NOFLAGS);
	g_allow_skin_overrides = gi.cvar("g_allow_skin_overrides", "1", CVAR_NOFLAGS);
	g_team_force_models = gi.cvar("g_team_force_models", "0",           CVAR_NOFLAGS);
	g_team_red_model    = gi.cvar("g_team_red_model",   "male/ctf_r",  CVAR_NOFLAGS);
	g_team_blue_model   = gi.cvar("g_team_blue_model",  "female/ctf_b", CVAR_NOFLAGS);
	g_allow_forfeit = gi.cvar("g_allow_forfeit", "1", CVAR_NOFLAGS);
	g_allow_mymap = gi.cvar("g_allow_mymap", "1", CVAR_NOFLAGS);
	g_allow_spec_vote = gi.cvar("g_allow_spec_vote", "0", CVAR_NOFLAGS);
	g_allow_techs = gi.cvar("g_allow_techs", "auto", CVAR_NOFLAGS);
	g_allow_vote_midgame = gi.cvar("g_allow_vote_midgame", "0", CVAR_NOFLAGS);
	g_muffmode_debug = gi.cvar("g_muffmode_debug", "0", CVAR_NOFLAGS);
	g_allow_voting = gi.cvar("g_allow_voting", "1", CVAR_NOFLAGS);
	g_arena_dmg_armor = gi.cvar("g_arena_dmg_armor", "0", CVAR_NOFLAGS);
	g_arena_start_armor = gi.cvar("g_arena_start_armor", "200", CVAR_NOFLAGS);
	g_arena_start_health = gi.cvar("g_arena_start_health", "200", CVAR_NOFLAGS);
	g_arena_players_per_team = gi.cvar("g_arena_players_per_team", "1", CVAR_LATCH);
	g_arena_rounds = gi.cvar("g_arena_rounds", "1", CVAR_NOFLAGS);
	g_arena_falling_damage = gi.cvar("g_arena_falling_damage", "1", CVAR_NOFLAGS);
	g_arena_weapon_mask = gi.cvar("g_arena_weapon_mask", "255", CVAR_NOFLAGS);
	g_arena_ammo_shells = gi.cvar("g_arena_ammo_shells", "100", CVAR_NOFLAGS);
	g_arena_ammo_bullets = gi.cvar("g_arena_ammo_bullets", "200", CVAR_NOFLAGS);
	g_arena_ammo_grenades = gi.cvar("g_arena_ammo_grenades", "20", CVAR_NOFLAGS);
	g_arena_ammo_rockets = gi.cvar("g_arena_ammo_rockets", "50", CVAR_NOFLAGS);
	g_arena_ammo_cells = gi.cvar("g_arena_ammo_cells", "150", CVAR_NOFLAGS);
	g_arena_ammo_slugs = gi.cvar("g_arena_ammo_slugs", "50", CVAR_NOFLAGS);
	g_arena_config = gi.cvar("g_arena_config", "arena.cfg", CVAR_LATCH);
	g_arena_legacy_idmap = gi.cvar("g_arena_legacy_idmap", "0", CVAR_LATCH);
	g_arena_default_type = gi.cvar("g_arena_default_type", "rocket", CVAR_NOFLAGS);
	g_arena_health_protect = gi.cvar("g_arena_health_protect", "1", CVAR_NOFLAGS);
	g_arena_armor_protect = gi.cvar("g_arena_armor_protect", "2", CVAR_NOFLAGS);
	g_arena_fast_switch = gi.cvar("g_arena_fast_switch", "1", CVAR_NOFLAGS);
	g_arena_grapple = gi.cvar("g_arena_grapple", "0", CVAR_NOFLAGS);
	g_arena_excessive = gi.cvar("g_arena_excessive", "0", CVAR_NOFLAGS);
	g_arena_competition = gi.cvar("g_arena_competition", "0", CVAR_NOFLAGS);
	g_arena_unbalanced = gi.cvar("g_arena_unbalanced", "0", CVAR_NOFLAGS);
	g_arena_lock = gi.cvar("g_arena_lock", "0", CVAR_NOFLAGS);
	g_arena_lock_count = gi.cvar("g_arena_lock_count", "6", CVAR_NOFLAGS);
	g_arena_max_players = gi.cvar("g_arena_max_players", "0", CVAR_NOFLAGS);
	g_arena_vote_time = gi.cvar("g_arena_vote_time", "30", CVAR_NOFLAGS);
	g_arena_timeouts = gi.cvar("g_arena_timeouts", "3", CVAR_NOFLAGS);
	g_arena_rocket_speed = gi.cvar("g_arena_rocket_speed", "900", CVAR_NOFLAGS);
	g_auto_ghost_max = gi.cvar("g_auto_ghost_max", "3", CVAR_NOFLAGS);
	g_auto_ghost_time = gi.cvar("g_auto_ghost_time", "120", CVAR_NOFLAGS);
	g_auto_ghost_timeout = gi.cvar("g_auto_ghost_timeout", "0", CVAR_NOFLAGS);
	g_coop_health_scaling = gi.cvar("g_coop_health_scaling", "0", CVAR_LATCH);
	g_corpse_sink_time = gi.cvar("g_corpse_sink_time", "15", CVAR_NOFLAGS);
	g_damage_scale = gi.cvar("g_damage_scale", "1", CVAR_NOFLAGS);
	g_disable_player_collision = gi.cvar("g_disable_player_collision", "0", CVAR_NOFLAGS);
	g_dm_allow_exit = gi.cvar("g_dm_allow_exit", "0", CVAR_NOFLAGS);
	g_dm_allow_no_humans = gi.cvar("g_dm_allow_no_humans", "1", CVAR_NOFLAGS);
	g_dm_auto_join = gi.cvar("g_dm_auto_join", "0", CVAR_NOFLAGS);
	g_dm_crosshair_id = gi.cvar("g_dm_crosshair_id", "1", CVAR_NOFLAGS);
	g_dm_death_scoreboard = gi.cvar("g_dm_death_scoreboard", "1", CVAR_NOFLAGS);
	g_dm_do_readyup = gi.cvar("g_dm_do_readyup", "0", CVAR_NOFLAGS);
	g_dm_do_warmup = gi.cvar("g_dm_do_warmup", "1", CVAR_NOFLAGS);
	g_dm_exec_level_cfg = gi.cvar("g_dm_exec_level_cfg", "0", CVAR_NOFLAGS);
	g_dm_force_join = gi.cvar("g_dm_force_join", "0", CVAR_NOFLAGS);
	g_dm_force_respawn = gi.cvar("g_dm_force_respawn", "1", CVAR_NOFLAGS);
	g_dm_force_respawn_time = gi.cvar("g_dm_force_respawn_time", "3", CVAR_NOFLAGS);
	g_dm_holdable_adrenaline = gi.cvar("g_dm_holdable_adrenaline", "1", CVAR_NOFLAGS);
	g_dm_instant_items = gi.cvar("g_dm_instant_items", "1", CVAR_NOFLAGS);
	g_dm_intermission_shots = gi.cvar("g_dm_intermission_shots", "0", CVAR_NOFLAGS);
	g_dm_item_respawn_rate = gi.cvar("g_dm_item_respawn_rate", "1.0", CVAR_NOFLAGS);
	g_dm_no_fall_damage = gi.cvar("g_dm_no_fall_damage", "0", CVAR_NOFLAGS);
	g_dm_no_quad_drop = gi.cvar("g_dm_no_quad_drop", "0", CVAR_NOFLAGS);
	g_dm_no_self_damage = gi.cvar("g_dm_no_self_damage", "0", CVAR_NOFLAGS);
	g_dm_no_stack_double = gi.cvar("g_dm_no_stack_double", "0", CVAR_NOFLAGS);
	g_dm_overtime = gi.cvar("g_dm_overtime", "120", CVAR_NOFLAGS);
	g_dm_tie_max_time = gi.cvar("g_dm_tie_max_time", "1800", CVAR_NOFLAGS);
	g_dm_powerup_drop = gi.cvar("g_dm_powerup_drop", "1", CVAR_NOFLAGS);
	g_dm_powerups_minplayers = gi.cvar("g_dm_powerups_minplayers", "0", CVAR_NOFLAGS);
	g_dm_respawn_delay_min = gi.cvar("g_dm_respawn_delay_min", "1", CVAR_NOFLAGS);
	g_dm_respawn_point_min_dist = gi.cvar("g_dm_respawn_point_min_dist", "256", CVAR_NOFLAGS);
	g_dm_respawn_point_min_dist_debug = gi.cvar("g_dm_respawn_point_min_dist_debug", "0", CVAR_NOFLAGS);
	g_dm_same_level = gi.cvar("g_dm_same_level", "0", CVAR_NOFLAGS);
	g_dm_spawn_farthest = gi.cvar("g_dm_spawn_farthest", "1", CVAR_NOFLAGS);
	g_dm_spawnpads = gi.cvar("g_dm_spawnpads", "1", CVAR_NOFLAGS);
	g_dm_timeout_length = gi.cvar("g_dm_timeout_length", "120", CVAR_NOFLAGS);
	g_dm_timeout_resume_countdown = gi.cvar("g_dm_timeout_resume_countdown", "30", CVAR_NOFLAGS);
	g_dm_weapons_stay = gi.cvar("g_dm_weapons_stay", "0", CVAR_NOFLAGS);
	g_drop_cmds = gi.cvar("g_drop_cmds", "7", CVAR_NOFLAGS);
	g_entity_override_dir = gi.cvar("g_entity_override_dir", "maps", CVAR_NOFLAGS);
	g_entity_override_load = gi.cvar("g_entity_override_load", "1", CVAR_NOFLAGS);
	g_entity_override_save = gi.cvar("g_entity_override_save", "0", CVAR_NOFLAGS);
	g_fast_doors = gi.cvar("g_fast_doors", "1", CVAR_NOFLAGS);
	g_frames_per_frame = gi.cvar("g_frames_per_frame", "1", CVAR_NOFLAGS);
	g_freezetag_arena_loadout = gi.cvar("g_freezetag_arena_loadout", "0", CVAR_NOFLAGS);
	g_freezetag_auto_thaw_time = gi.cvar("g_freezetag_auto_thaw_time", "0", CVAR_NOFLAGS);
	g_freezetag_bot_rescue = gi.cvar("g_freezetag_bot_rescue", "1", CVAR_NOFLAGS);
	g_freezetag_frozen_knockback_scale = gi.cvar("g_freezetag_frozen_knockback_scale", "1.0", CVAR_NOFLAGS);
	g_freezetag_multi_thaw_scale = gi.cvar("g_freezetag_multi_thaw_scale", "0.5", CVAR_NOFLAGS);
	g_freezetag_round_reset_alive_inventory = gi.cvar("g_freezetag_round_reset_alive_inventory", "1", CVAR_NOFLAGS);
	g_freezetag_round_respawn_all = gi.cvar("g_freezetag_round_respawn_all", "1", CVAR_NOFLAGS);
	g_freezetag_thaw_radius = gi.cvar("g_freezetag_thaw_radius", "96", CVAR_NOFLAGS);
	g_freezetag_thaw_respawn_at_location = gi.cvar("g_freezetag_thaw_respawn_at_location", "0", CVAR_NOFLAGS);
	g_freezetag_thaw_time = gi.cvar("g_freezetag_thaw_time", "3", CVAR_NOFLAGS);
	g_friendly_fire = gi.cvar("g_friendly_fire", "0", CVAR_NOFLAGS);
	g_inactivity = gi.cvar("g_inactivity", "120", CVAR_NOFLAGS);
	g_infinite_ammo = gi.cvar("g_infinite_ammo", "0", CVAR_LATCH);
	g_instant_weapon_switch = gi.cvar("g_instant_weapon_switch", "0", CVAR_NOFLAGS);
	g_item_bobbing = gi.cvar("g_item_bobbing", "1", CVAR_NOFLAGS);
	g_knockback_scale = gi.cvar("g_knockback_scale", "1.0", CVAR_NOFLAGS);
	g_ladder_steps = gi.cvar("g_ladder_steps", "1", CVAR_NOFLAGS);
	g_lag_compensation = gi.cvar("g_lag_compensation", "1", CVAR_NOFLAGS);
	g_lag_compensation_enhanced = gi.cvar("g_lag_compensation_enhanced", "1", CVAR_NOFLAGS);
	g_map_list = gi.cvar("g_map_list", "", CVAR_NOFLAGS);
	g_map_list_shuffle = gi.cvar("g_map_list_shuffle", "1", CVAR_NOFLAGS);
	g_map_pool = gi.cvar("g_map_pool", "", CVAR_NOFLAGS);
	g_maps_pool_file = gi.cvar("g_maps_pool_file", "", CVAR_NOFLAGS);
	g_maps_cycle_file = gi.cvar("g_maps_cycle_file", "", CVAR_NOFLAGS);
	g_maps_random = gi.cvar("g_maps_random", "1", CVAR_NOFLAGS);
	g_maps_repeat_delay = gi.cvar("g_maps_repeat_delay", "1800", CVAR_NOFLAGS);
	g_votable_gametypes = gi.cvar("g_votable_gametypes", "", CVAR_NOFLAGS);
	g_votable_rulesets = gi.cvar("g_votable_rulesets", "", CVAR_NOFLAGS);
	g_match_lock = gi.cvar("g_match_lock", "0", CVAR_SERVERINFO);
	g_matchstats = gi.cvar("g_matchstats", "0", CVAR_NOFLAGS);
	MM_MatchStats_RegisterCvars();
	g_loc = gi.cvar("g_loc", "1", CVAR_NOFLAGS);
	g_loc_items = gi.cvar("g_loc_items", "1", CVAR_NOFLAGS);
	g_motd_filename = gi.cvar("g_motd_filename", "motd.txt", CVAR_NOFLAGS);
	g_mover_debug = gi.cvar("g_mover_debug", "0", CVAR_NOFLAGS);
	g_mover_speed_scale = gi.cvar("g_mover_speed_scale", "1.0f", CVAR_NOFLAGS);
	g_no_armor = gi.cvar("g_no_armor", "0", CVAR_NOFLAGS);
	g_no_health = gi.cvar("g_no_health", "0", CVAR_NOFLAGS);
	g_no_items = gi.cvar("g_no_items", "0", CVAR_NOFLAGS);
	g_no_mines = gi.cvar("g_no_mines", "0", CVAR_NOFLAGS);
	g_no_nukes = gi.cvar("g_no_nukes", "0", CVAR_NOFLAGS);
	g_no_powerups = gi.cvar("g_no_powerups", "0", CVAR_NOFLAGS);
	g_mapspawn_no_bfg = gi.cvar("g_no_bfg", "0", CVAR_NOFLAGS);
	g_mapspawn_no_plasmabeam = gi.cvar("g_no_plasmabeam", "0", CVAR_NOFLAGS);
	g_no_spheres = gi.cvar("g_no_spheres", "0", CVAR_NOFLAGS);
	g_quick_weapon_switch = gi.cvar("g_quick_weapon_switch", "1", CVAR_LATCH);
	g_round_countdown = gi.cvar("g_round_countdown", "10", CVAR_NOFLAGS);
	g_select_empty = gi.cvar("g_select_empty", "0", CVAR_ARCHIVE);
	g_showhelp = gi.cvar("g_showhelp", "1", CVAR_NOFLAGS);
	g_showmotd = gi.cvar("g_showmotd", "1", CVAR_NOFLAGS);
	g_start_items = gi.cvar("g_start_items", "", CVAR_NOFLAGS);
	g_starting_health = gi.cvar("g_starting_health", "100", CVAR_NOFLAGS);
	g_starting_health_bonus = gi.cvar("g_starting_health_bonus", "0", CVAR_NOFLAGS);
	g_starting_armor = gi.cvar("g_starting_armor", "0", CVAR_NOFLAGS);
	g_strict_saves = gi.cvar("g_strict_saves", "1", CVAR_NOFLAGS);
	g_teamplay_allow_team_pick = gi.cvar("g_teamplay_allow_team_pick", "0", CVAR_NOFLAGS);
	g_teamplay_armor_protect = gi.cvar("g_teamplay_armor_protect", "0", CVAR_NOFLAGS);
	g_teamplay_auto_balance = gi.cvar("g_teamplay_auto_balance", "1", CVAR_NOFLAGS);
	g_teamplay_force_balance = gi.cvar("g_teamplay_force_balance", "0", CVAR_NOFLAGS);
	g_teamplay_item_drop_notice = gi.cvar("g_teamplay_item_drop_notice", "1", CVAR_NOFLAGS);
	g_teleporter_freeze = gi.cvar("g_teleporter_freeze", "0", CVAR_NOFLAGS);
	g_verbose = gi.cvar("g_verbose", "0", CVAR_NOFLAGS);
	g_vote_flags = gi.cvar("g_vote_flags", "0", CVAR_NOFLAGS);
	g_vote_limit = gi.cvar("g_vote_limit", "3", CVAR_NOFLAGS);
	g_warmup_countdown = gi.cvar("g_warmup_countdown", "10", CVAR_NOFLAGS);
	g_warmup_ready_percentage = gi.cvar("g_warmup_ready_percentage", "0.51f", CVAR_NOFLAGS);
	g_weapon_projection = gi.cvar("g_weapon_projection", "0", CVAR_NOFLAGS);
	g_weapon_respawn_time = gi.cvar("g_weapon_respawn_time", "30", CVAR_NOFLAGS);
	
#ifdef _DEBUG
	// Weapon balance cvars (DEBUG ONLY - not available in release builds)
	g_weapon_balance_dev = gi.cvar("g_weapon_balance_dev", "0", CVAR_NOFLAGS);
	g_chaingun_max_shots = gi.cvar("g_chaingun_max_shots", "0", CVAR_NOFLAGS);
	g_chaingun_damage = gi.cvar("g_chaingun_damage", "0", CVAR_NOFLAGS);
	g_chaingun_hspread = gi.cvar("g_chaingun_hspread", "0", CVAR_NOFLAGS);
	g_chaingun_vspread = gi.cvar("g_chaingun_vspread", "0", CVAR_NOFLAGS);
	g_chaingun_spread_offset = gi.cvar("g_chaingun_spread_offset", "0", CVAR_NOFLAGS);
	g_machinegun_damage = gi.cvar("g_machinegun_damage", "0", CVAR_NOFLAGS);
	g_machinegun_hspread = gi.cvar("g_machinegun_hspread", "0", CVAR_NOFLAGS);
	g_machinegun_vspread = gi.cvar("g_machinegun_vspread", "0", CVAR_NOFLAGS);
	g_hyperblaster_speed = gi.cvar("g_hyperblaster_speed", "0", CVAR_NOFLAGS);
	g_railgun_damage = gi.cvar("g_railgun_damage", "0", CVAR_NOFLAGS);
	g_rocketlauncher_damage = gi.cvar("g_rocketlauncher_damage", "0", CVAR_NOFLAGS);
	g_rocketlauncher_speed = gi.cvar("g_rocketlauncher_speed", "0", CVAR_NOFLAGS);
#else
	// In release builds, set pointers to nullptr to prevent undefined references
	g_weapon_balance_dev = nullptr;
	g_chaingun_max_shots = nullptr;
	g_chaingun_damage = nullptr;
	g_chaingun_hspread = nullptr;
	g_chaingun_vspread = nullptr;
	g_chaingun_spread_offset = nullptr;
	g_machinegun_damage = nullptr;
	g_machinegun_hspread = nullptr;
	g_machinegun_vspread = nullptr;
	g_hyperblaster_speed = nullptr;
	g_railgun_damage = nullptr;
	g_rocketlauncher_damage = nullptr;
	g_rocketlauncher_speed = nullptr;
#endif

	bot_name_prefix = gi.cvar("bot_name_prefix", "B|", CVAR_NOFLAGS);

	// ruleset
	CheckRuleset();

	// items
	InitItems();

	game = {};
	// Static MyMap transition state must not survive a game-module lifecycle.
	MM_MQ_CancelPendingMapLoad();

	const uint32_t clamped_max_clients = G_ClampGameLimit(
		"maxclients", maxclients->integer, 1, static_cast<uint32_t>(MAX_LOBBY_PLAYERS));
	const uint32_t minimum_entities = clamped_max_clients + static_cast<uint32_t>(BODY_QUEUE_SIZE) + 1;
	const uint32_t clamped_max_entities = G_ClampGameLimit("maxentities", maxentities->integer, minimum_entities, MAX_ENTITIES);

	// initialize all entities for this game
	game.maxentities = clamped_max_entities;
	g_entities = (gentity_t *)gi.TagMalloc(game.maxentities * sizeof(g_entities[0]), TAG_GAME);
	globals.gentities = g_entities;
	globals.max_entities = game.maxentities;

	// initialize all clients for this game
	game.maxclients = clamped_max_clients;
	game.clients = (gclient_t *)gi.TagMalloc(game.maxclients * sizeof(game.clients[0]), TAG_GAME);
	memset(game.clients, 0, game.maxclients * sizeof(game.clients[0]));
	globals.num_entities = game.maxclients + 1;

	// how far back we should support lag compensation samples for
	game.max_lag_origins = clamp(static_cast<int32_t>(ceilf(2.0f / gi.frame_time_s)), 1, 65535);
	game.lag_samples = (lag_compensation_sample_t *)gi.TagMalloc(game.maxclients * sizeof(lag_compensation_sample_t) * game.max_lag_origins, TAG_GAME);

	level.start_time = level.time;

	level.ready_to_exit = false;

	level.match_state = matchst_t::MATCH_WARMUP_DELAYED;
	level.match_state_timer = 0_sec;
	level.match_time = level.time;
	level.warmup_notice_time = level.time;
	level.warmup_gametype_hud_time = level.time;

	level.locked[TEAM_SPECTATOR] = false;
	level.locked[TEAM_FREE] = false;
	level.locked[TEAM_RED] = false;
	level.locked[TEAM_BLUE] = false;

	level.captain[TEAM_RED] = nullptr;
	level.captain[TEAM_BLUE] = nullptr;

	*level.weapon_count = { 0 };

	ClearVote();

	MuffModeLog("DEBUG", "InitGame: vote state after ClearVote: state=%d, caller=%p, command=%p, arg_empty=%d",
	           (int)level.vote_state.state, (void*)level.vote_state.caller,
	           (void*)level.vote_state.command, (int)level.vote_state.arg.empty());

	// Dump client menu pointers to detect stale pointers after map load
	for (size_t i = 0; i < game.maxclients; i++) {
		if (game.clients[i].menu)
			MuffModeLog("DEBUG", "InitGame: WARNING client %d has stale menu pointer %p after map load",
			           (int)i, (void*)game.clients[i].menu);
	}

	level.total_player_deaths = 0;

	MM_SyncGametypeTracking();
	MM_SanitizeCurrentGametype();

	MM_LoadMOTD();
	MM_ClearLocCache();
	MM_InitMapPoolSystem();

	if (g_dm_exec_level_cfg->integer)
		gi.AddCommandString(G_Fmt("exec {}\n", level.mapname).data());

	// Note: Gametype cfg execution moved to ChangeGametype() so it only runs
	// when gametype actually changes, not on every map load. This prevents
	// cfg files from overriding player votes and map progression.

	// Note: Map list shuffling is handled lazily in Match_End().
	// ChangeGametype() resets the g_map_list_shuffled flag so the list
	// gets reshuffled when the next match ends.
}

//===================================================================

#if 0
static void ClearBodyQue(void) {
	int	i;
	gentity_t *ent;

	for (i = 0; i < BODY_QUEUE_SIZE; i++) {
		ent = level.bodyQue[i];
		if (ent->linked) {
			gi.unlinkentity(ent);
		}
	}
}
#endif

/*
==================
FindIntermissionPoint

This is also used for spectator spawns
==================
*/
static bool IntermissionEntityUsable(const gentity_t *ent) {
	return ent && ent->inuse && std::isfinite(ent->s.origin.x) && std::isfinite(ent->s.origin.y) && std::isfinite(ent->s.origin.z);
}

static gentity_t *FindUsableIntermissionEntity(gentity_t *from) {
	gentity_t *ent = from;
	while ((ent = G_FindByString<&gentity_t::classname>(ent, "info_player_intermission")) != nullptr) {
		if (IntermissionEntityUsable(ent))
			return ent;
	}

	return nullptr;
}

static gentity_t *FindUsableSpawnEntity(const char *classname) {
	gentity_t *ent = nullptr;
	while ((ent = G_FindByString<&gentity_t::classname>(ent, classname)) != nullptr) {
		if (IntermissionEntityUsable(ent))
			return ent;
	}

	return nullptr;
}

static bool SetIntermissionAngleTowardTarget(gentity_t *ent) {
	if (!ent || !ent->target)
		return false;

	gentity_t *target = G_PickTarget(ent->target);
	if (!IntermissionEntityUsable(target))
		return false;

	const vec3_t delta = target->s.origin - ent->s.origin;
	if (!delta)
		return false;

	level.intermission_angle = vectoangles(delta);
	return true;
}

void FindIntermissionPoint(void) {
	gentity_t *ent;
	bool	is_landmark = false;

	if (level.intermission_spot) // search only once
		return;

	gi.Com_Print("FindIntermissionPoint\n");

	// find the intermission spot
	ent = level.spawn_spots[SPAWN_SPOT_INTERMISSION];

	if (!IntermissionEntityUsable(ent)) { // the map creator forgot to put in an intermission point...
		SelectSpawnPoint(NULL, level.intermission_origin, level.intermission_angle, false, is_landmark);
	} else {
		level.intermission_origin = ent->s.origin;
		level.intermission_angle = ent->s.angles;

		// ugly hax!
		if (!Q_strncasecmp(level.mapname, "campgrounds", 11)) {
			gvec3_t v = { -320, -96, 503 };
			if (ent->s.origin == v)
				level.intermission_angle[PITCH] = -30;
		} else if (!Q_strncasecmp(level.mapname, "rdm10", 5)) {
			gvec3_t v = { -1256, -1672, -136 };
			if (ent->s.origin == v)
				level.intermission_angle = { 15, 135, 0 };
		}

		// if it has a target, look towards it
		if (ent->target) {
			gi.Com_Print("FindIntermissionPoint target\n");
			if (SetIntermissionAngleTowardTarget(ent)) {
				gi.Com_Print("FindIntermissionPoint target 2\n");
			}
		}
	}

	level.intermission_spot = true;
}

/*
==================
SetIntermissionPoint
==================
*/
void SetIntermissionPoint(void) {
	if (level.level_intermission_set)
		return;

	//FindIntermissionPoint();
	//gi.Com_Print("SetIntermissionPoint\n");

	gentity_t *ent;
	// find an intermission spot
	ent = FindUsableIntermissionEntity(nullptr);
	if (!ent) { // the map creator forgot to put in an intermission point...
		ent = FindUsableSpawnEntity("info_player_start");
		if (!ent)
			ent = FindUsableSpawnEntity("info_player_deathmatch");
	} else { // choose one of four spots
		int32_t i = irandom(4);
		while (i--) {
			ent = FindUsableIntermissionEntity(ent);
			if (!ent) // wrap around the list
				ent = FindUsableIntermissionEntity(nullptr);
		}
	}

	if (ent) {
		level.intermission_origin = ent->s.origin;
		level.spawn_spots[SPAWN_SPOT_INTERMISSION] = ent;
		level.intermission_angle = ent->s.angles;
	}
	
	// ugly hax!
	if (ent && !Q_strncasecmp(level.mapname, "campgrounds", 11)) {
		gvec3_t v = { -320, -96, 503 };
		if (ent->s.origin == v)
			level.intermission_angle[PITCH] = -30;
	} else if (ent && !Q_strncasecmp(level.mapname, "rdm10", 5)) {
		gvec3_t v = { -1256, -1672, -136 };
		if (ent->s.origin == v)
			level.intermission_angle = { 15, 135, 0 };
	} else {
		// if it has a target, look towards it
		if (ent && !SetIntermissionAngleTowardTarget(ent) && !level.intermission_angle)
			level.intermission_angle = ent->s.angles;
	}
	
	//gi.Com_PrintFmt("{}: origin={} angles={}\n", __FUNCTION__, level.intermission_origin, level.intermission_angle);
}

/*
=================
CheckDMIntermissionExit

The level will stay at the intermission for a minimum of 5 seconds
If all players wish to continue, the level will then exit.
If one or more players have not acknowledged the continue, the game will
wait 10 seconds before going on.

Adapted from Quake III
=================
*/

static void CheckDMIntermissionExit(void) {
	int ready, not_ready;

	// see which players are ready
	ready = not_ready = 0;
	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client))
			continue;

		if (ec->client->sess.is_a_bot)
			ec->client->ready_to_exit = true;

		if (ec->client->ready_to_exit)
			ready++;
		else
			not_ready++;
	}

	// vote in progress
	if (level.vote_state.state != VoteState::IDLE) {
		ready = 0;
		not_ready = 1;
	}

	// never exit in less than five seconds
	if (level.time < level.intermission_time + 5_sec && !level.exit_time)
		return;

	// if nobody wants to go, clear timer
	// skip this if no players present
	if (!ready && not_ready) {
		level.ready_to_exit = false;
		return;
	}

	// if everyone wants to go, go now
	if (!not_ready) {
		ExitLevel();
		return;
	}

	// the first person to ready starts the ten second timeout
	if (ready && !level.ready_to_exit) {
		level.ready_to_exit = true;
		level.exit_time = level.time + 10_sec;
	}

	// if we have waited ten seconds since at least one player
	// wanted to exit, go ahead
	if (level.time < level.exit_time)
		return;

	ExitLevel();
}

/*
=============
ScoreIsTied

Adapted from Quake III
=============
*/
static bool ClientIndexIsValid(int client_index) {
	return client_index >= 0 && static_cast<size_t>(client_index) < game.maxclients;
}

static gclient_t *ClientFromSortedSlot(size_t slot) {
	if (slot >= q_countof(level.sorted_clients))
		return nullptr;

	const int client_index = level.sorted_clients[slot];
	if (!ClientIndexIsValid(client_index))
		return nullptr;

	return &game.clients[client_index];
}

struct logical_individual_score_view_t {
	const gclient_t *leader = nullptr;
	const gclient_t *runner_up = nullptr;
	int leader_client_num = -1;
	int runner_up_client_num = -1;
	size_t participant_count = 0;
};

static bool LogicalScoreRanksBefore(
	const gclient_t *candidate, int candidate_num,
	const gclient_t *current, int current_num) {
	return !current || candidate->resp.score > current->resp.score ||
		(candidate->resp.score == current->resp.score &&
			candidate_num < current_num);
}

static logical_individual_score_view_t LogicalIndividualScoreView() {
	logical_individual_score_view_t view;
	for (size_t i = 0; i < game.maxclients; ++i) {
		gentity_t *ent = &g_entities[i + 1];
		if (!ent->client)
			continue;

		const gclient_t *state = MM_Ghost_ReservedClientState(ent);
		if (!state) {
			if (!ent->inuse || !ent->client->pers.connected ||
				!ClientIsPlaying(ent->client))
				continue;
			state = ent->client;
		} else if (state->sess.team == TEAM_NONE ||
			state->sess.team == TEAM_SPECTATOR) {
			continue;
		}

		const int client_num = static_cast<int>(i);
		if (LogicalScoreRanksBefore(state, client_num,
				view.leader, view.leader_client_num)) {
			view.runner_up = view.leader;
			view.runner_up_client_num = view.leader_client_num;
			view.leader = state;
			view.leader_client_num = client_num;
		} else if (LogicalScoreRanksBefore(state, client_num,
				view.runner_up, view.runner_up_client_num)) {
			view.runner_up = state;
			view.runner_up_client_num = client_num;
		}
		++view.participant_count;
	}
	return view;
}

static bool ScoreIsTied(void) {
	if (Teams() && notGT(GT_RR))
		return level.team_scores[TEAM_RED] == level.team_scores[TEAM_BLUE];

	const logical_individual_score_view_t scores = LogicalIndividualScoreView();
	return scores.participant_count >= 2 && scores.leader && scores.runner_up &&
		scores.leader->resp.score == scores.runner_up->resp.score;
}

/*
=============
SortRanks

Adapted from Quake III
=============
*/
static int SortRanks(const void *a, const void *b) {
	gclient_t *ca, *cb;

	ca = &game.clients[*(int *)a];
	cb = &game.clients[*(int *)b];

	// sort special clients last
	if (ca->sess.spectator_client < 0)
		return 1;
	if (cb->sess.spectator_client < 0)
		return -1;

	// then connecting clients
	if (!ca->pers.connected)
		return 1;
	if (!cb->pers.connected)
		return -1;
	
	// then spectators
	if (!ClientIsPlaying(ca) && !ClientIsPlaying(cb)) {
		if (ca->sess.duel_queued && cb->sess.duel_queued) {
			if (ca->sess.team_join_time > cb->sess.team_join_time)
				return -1;
			if (ca->sess.team_join_time < cb->sess.team_join_time)
				return 1;
		}
		if (ca->sess.duel_queued)
			return -1;
		if (cb->sess.duel_queued)
			return 1;
		if (ca->sess.team_join_time > cb->sess.team_join_time)
			return -1;
		if (ca->sess.team_join_time < cb->sess.team_join_time)
			return 1;
		return 0;
	}
	if (!ClientIsPlaying(ca))
		return 1;
	if (!ClientIsPlaying(cb))
		return -1;

	// then sort by score
	if (false) { // Race mode removed
		if (ca->resp.score > 0 && (ca->resp.score < cb->resp.score))
			return -1;
		if (cb->resp.score > 0 && (ca->resp.score > cb->resp.score))
			return 1;
	} else {
		if (ca->resp.score > cb->resp.score)
			return -1;
		if (ca->resp.score < cb->resp.score)
			return 1;
	}

	// then sort by time
	if (ca->sess.team_join_time < cb->sess.team_join_time)
		return -1;
	if (ca->sess.team_join_time > cb->sess.team_join_time)
		return 1;

	return 0;
}

/*
============
CalculateRanks

Recalculates the score ranks of all players
This will be called on every client connect, begin, disconnect, death,
and team change.

Adapted from Quake III
============
*/
void CalculateRanks() {
	if (level.restarted)
		return;

	gclient_t	*cl;
	bool		teams = Teams();

	level.num_connected_clients = 0;
	level.num_nonspectator_clients = 0;
	level.num_playing_clients = 0;
	level.num_playing_human_clients = 0;
	level.num_eliminated_red = 0;
	level.num_eliminated_blue = 0;
	level.num_living_red = 0;
	level.num_living_blue = 0;
	level.num_living_free = 0;
	level.num_playing_red = 0;
	level.num_playing_blue = 0;
	level.follow1 = UINT8_MAX;
	level.follow2 = UINT8_MAX;

	//memset(level.sorted_clients, -1, sizeof(level.sorted_clients));
	for (size_t i = 0; i < MAX_CLIENTS; i++)
		level.sorted_clients[i] = -1;

	for (auto ec : active_clients()) {
		cl = ec->client;

		level.sorted_clients[level.num_connected_clients] = ec->client - game.clients;
		level.num_connected_clients++;

		if (!ClientIsPlaying(cl)) {
			continue;
		}

		level.num_nonspectator_clients++;

		// decide if this should be auto-followed
		level.num_playing_clients++;
		if (!cl->sess.is_a_bot) {
			level.num_playing_human_clients++;
		}
		if (level.follow1 == UINT8_MAX)
			level.follow1 = static_cast<uint8_t>(ec->client - game.clients);
		else if (level.follow2 == UINT8_MAX)
			level.follow2 = static_cast<uint8_t>(ec->client - game.clients);

		if (teams) {
			if (cl->sess.team == TEAM_RED) {
				level.num_playing_red++;
				if (!cl->eliminated && cl->pers.health > 0)
					level.num_living_red++;
				else if (cl->eliminated)
					level.num_eliminated_red++;
			} else {
				level.num_playing_blue++;
				if (!cl->eliminated && cl->pers.health > 0)
					level.num_living_blue++;
				else if (cl->eliminated)
					level.num_eliminated_blue++;
			}
		} else if (!cl->eliminated && cl->pers.health > 0) {
			level.num_living_free++;
		}
	}

	for (size_t i = 0; i < level.num_connected_clients; i++) {
		gclient_t *sorted_client = ClientFromSortedSlot(i);
		if (sorted_client && sorted_client->pers.connected) {
			sorted_client->resp.old_rank = sorted_client->resp.rank;
		}
	}

	qsort(level.sorted_clients, level.num_connected_clients, sizeof(level.sorted_clients[0]), SortRanks);

	if (level.sorted_clients[0] >= 0) {
		// set the rank value for all clients that are connected and not spectators
		if (GT(GT_ARENA)) {
			// Rank each fighter against only the fighters in the same room.
			// The global sorted list is still maintained for generic client
			// bookkeeping, but never defines a cross-arena winner.
			for (gentity_t *player : active_clients()) {
				gclient_t *client = player->client;
				const int arena_id = MM_Arena_Id(player);
				if (!MM_Arena_IsFighter(client) || arena_id <= 0) {
					client->resp.rank = 0;
					continue;
				}
				int ahead = 0;
				int tied = 0;
				for (gentity_t *other : active_clients()) {
					if (!MM_Arena_IsFighter(other->client) ||
						MM_Arena_Id(other) != arena_id)
						continue;
					if (other->client->resp.score > client->resp.score)
						ahead++;
					else if (other->client->resp.score == client->resp.score)
						tied++;
				}
				client->resp.old_score = client->resp.score;
				client->resp.rank = ahead |
					(tied > 1 ? RANK_TIED_FLAG : 0);
			}
		} else if (teams && notGT(GT_RR)) {
			// in team games, rank is just the order of the teams, 0=red, 1=blue, 2=tied
			for (size_t i = 0; i < level.num_connected_clients; i++) {
				cl = ClientFromSortedSlot(i);
				if (!cl)
					continue;

				if (level.team_scores[TEAM_RED] == level.team_scores[TEAM_BLUE]) {
					cl->resp.rank = 2;
				}
				else if (level.team_scores[TEAM_RED] > level.team_scores[TEAM_BLUE]) {
					cl->resp.rank = 0;
				}
				else {
					cl->resp.rank = 1;
				}
			}
		}
		else {
			int score = 0, new_score, rank = 0;

			for (size_t i = 0; i < level.num_playing_clients; i++) {
				cl = ClientFromSortedSlot(i);
				if (cl && cl->pers.connected && ClientIsPlaying(cl)) {
					cl->resp.old_score = cl->resp.score;
					new_score = cl->resp.score;
					if (i == 0 || new_score != score) {
						rank = i;
						// assume we aren't tied until the next client is checked
						cl->resp.rank = rank;
					}
					else {
						// we are tied with the previous client
						gclient_t *previous = ClientFromSortedSlot(i - 1);
						if (previous)
							previous->resp.rank = rank | RANK_TIED_FLAG;
						cl->resp.rank = rank | RANK_TIED_FLAG;
					}
					score = new_score;
				}
			}
		}
	}

	if (!level.num_playing_clients && !level.no_players_time)
		level.no_players_time = level.time;
	else if (level.num_playing_clients)
		level.no_players_time = 0_sec;

	if (level.match_state == MATCH_IN_PROGRESS) {
		// Red Rover is decided by the round/time limits, not a frag target, so it skips the
		// "N frags to win" countdown even though it tracks individual frags (GTF_FRAGS).
		if (GTF(GTF_FRAGS) && notGT(GT_RR)) {
			//gi.Com_PrintFmt("new={} old={}\n", game.clients[level.sorted_clients[0]].resp.score, old_first_score);
			// frag_warning has 3 entries (1/2/3 frags to go). Once the leader reaches
			// the limit score_diff is <= 0, so guard the lower bound or score_diff-1
			// indexes frag_warning[-1] and corrupts the adjacent field (crash on match end).
			gclient_t *leader = ClientFromSortedSlot(0);
			if (leader) {
				if (const auto warning_index = MM_FragWarningIndex(fraglimit->integer, leader->resp.score)) {
					const int score_diff = static_cast<int>(*warning_index) + 1;
					if (!level.frag_warning[*warning_index]) {
						const mm_announce_event_t frag_events[] = {
							mm_announce_event_t::OneFrag,
							mm_announce_event_t::TwoFrags,
							mm_announce_event_t::ThreeFrags,
						};
						MM_Announce(frag_events[score_diff - 1], world);
						level.frag_warning[*warning_index] = true;
						CheckDMExitRules();
						return;
					}
				}
			}
		}
		gclient_t *leader = ClientFromSortedSlot(0);
		if ((!Teams() || GT(GT_RR)) && leader && leader->resp.score > 0) {
			// check changes in rank to trigger sounds
			// (RR is a team mode but scores individually, so it uses the FFA lead announcer)
			int new_rank = 0, old_rank = 0;
			bool new_tied = false, old_tied = false;
			for (auto ec : active_players()) {
				new_rank = ec->client->resp.rank;
				old_rank = ec->client->resp.old_rank;

				//if (ec == world + 1)
				//	gi.Com_PrintFmt("new_rank={} old_rank={}\n", new_rank, old_rank);

				if (new_rank == old_rank)
					continue;

				if (new_rank & RANK_TIED_FLAG) {
					new_rank &= ~RANK_TIED_FLAG;
					new_tied = true;
				} else {
					new_tied = false;
				}
				if (old_rank & RANK_TIED_FLAG) {
					old_rank &= ~RANK_TIED_FLAG;
					old_tied = true;
				} else {
					old_tied = false;
				}

				//if (ec == world + 1)
				//	gi.Com_PrintFmt("new_rank2={} old_rank2={}\n", new_rank, old_rank);

				const bool entered_tied_for_first = (new_rank == 0 && new_tied && old_rank != 0);
				const bool tied_state_changed_at_first = (new_rank == 0 && old_tied != new_tied);

				if (entered_tied_for_first || tied_state_changed_at_first) {
					MM_Announce((new_tied || entered_tied_for_first) ? mm_announce_event_t::LeadTied : mm_announce_event_t::LeadTaken, ec);

					// find and update all spectators who want to follow leader
					for (auto ec2 : active_clients()) {
						if ((!ClientIsPlaying(ec2->client) || ec2->client->eliminated) && ec2->client->sess.pc.follow_leader && ec2->client->follow_target != ec) {
							ec2->client->follow_queued_target = ec;
							ec2->client->follow_queued_time = level.time;
						}
					}
				} else if (new_rank != 0 && old_rank == 0) {
					MM_Announce(mm_announce_event_t::LeadLost, ec);
				}
			}
		} else if (Teams() && notGT(GT_RR) && GTF(GTF_FRAGS)) {
			int new_rank, old_rank;

			if (level.team_old_scores[TEAM_RED] == level.team_old_scores[TEAM_BLUE]) {
				old_rank = 2;
			} else if (level.team_old_scores[TEAM_RED] > level.team_old_scores[TEAM_BLUE]) {
				old_rank = 0;
			} else {
				old_rank = 1;
			}
			if (level.team_scores[TEAM_RED] == level.team_scores[TEAM_BLUE]) {
				new_rank = 2;
			} else if (level.team_scores[TEAM_RED] > level.team_scores[TEAM_BLUE]) {
				new_rank = 0;
			} else {
				new_rank = 1;
			}

			if (old_rank == 2 && new_rank != 2) {
				//a team just took the lead
				MM_Announce(new_rank ? mm_announce_event_t::BlueLeads : mm_announce_event_t::RedLeads, world);
			} else if (old_rank != 2 && new_rank == 2) {
				//teams just tied
				MM_Announce(mm_announce_event_t::TeamsTied, world);
			}
			/*
			else if ((GTF(GTF_CTF)) && new_rank != 2) {
				//a team has scored
				AnnouncerSound(world, new_rank ? "blue_scores" : "red_scores", nullptr, false);
			}
			*/
			level.team_old_scores[TEAM_RED] = level.team_scores[TEAM_RED];
			level.team_old_scores[TEAM_BLUE] = level.team_scores[TEAM_BLUE];
		}
	}

	// see if it is time to end the level
	CheckDMExitRules();

}

static void ShutdownGame() {
	gi.Com_Print("==== ShutdownGame ====\n");

	// [MuffMode] Give any exact, already-computed profile settlements one final
	// persistence attempt while the game imports are still available.
	MM_PlayerStats_Shutdown();
	MM_ClientProfile_Shutdown();
	// [MuffMode] Drain and join the asynchronous stats writer while game state
	// and import callbacks are still valid.
	MM_MatchStats_Shutdown();
	MM_ShutdownMapPoolSystem();
	gi.FreeTags(TAG_LEVEL);
	gi.FreeTags(TAG_GAME);
}

static void *G_GetExtension(const char *name) {
	if (void *extension = MM_MatchStats_GetExtension(name))
		return extension;

	return nullptr;
}

gtime_t FRAME_TIME_S;
gtime_t FRAME_TIME_MS;

/*
=================
GetGameAPI

Returns a pointer to the structure with all entry points
and global variables
=================
*/
Q2GAME_API game_export_t * GetGameAPI(game_import_t * import) {
	gi = *import;

	FRAME_TIME_S = FRAME_TIME_MS = gtime_t::from_ms(gi.frame_time_ms);

	globals.apiversion = GAME_API_VERSION;
	globals.PreInit = PreInitGame;
	globals.Init = InitGame;
	globals.Shutdown = ShutdownGame;
	globals.SpawnEntities = SpawnEntities;

	globals.WriteGameJson = WriteGameJson;
	globals.ReadGameJson = ReadGameJson;
	globals.WriteLevelJson = WriteLevelJson;
	globals.ReadLevelJson = ReadLevelJson;
	globals.CanSave = CanSave;

	globals.Pmove = Pmove;

	globals.GetExtension = G_GetExtension;

	globals.ClientChooseSlot = ClientChooseSlot;
	globals.ClientThink = ClientThink;
	globals.ClientConnect = ClientConnect;
	globals.ClientUserinfoChanged = ClientUserinfoChanged;
	globals.ClientDisconnect = ClientDisconnect;
	globals.ClientBegin = ClientBegin;
	globals.ClientCommand = ClientCommand;

	globals.RunFrame = G_RunFrame;
	globals.PrepFrame = G_PrepFrame;

	globals.ServerCommand = ServerCommand;
	globals.Bot_SetWeapon = Bot_SetWeapon;
	globals.Bot_TriggerEntity = Bot_TriggerEntity;
	globals.Bot_GetItemID = Bot_GetItemID;
	globals.Bot_UseItem = Bot_UseItem;
	globals.Entity_ForceLookAtPoint = Entity_ForceLookAtPoint;
	globals.Bot_PickedUpItem = Bot_PickedUpItem;

	globals.Entity_IsVisibleToPlayer = Entity_IsVisibleToPlayer;
	globals.GetShadowLightData = GetShadowLightData;

	globals.gentity_size = sizeof(gentity_t);

	return &globals;
}

//======================================================================

/*
=================
ClientEndServerFrames
=================
*/
static void ClientEndServerFrames() {
	// calc the player views now that all pushing
	// and damage has been added
	for (auto ec : active_clients())
		ClientEndServerFrame(ec);
}

/*
=================
CreateTargetChangeLevel

Returns the created target changelevel
=================
*/
gentity_t *CreateTargetChangeLevel(const char *map) {
	gentity_t *ent;

	ent = G_Spawn();
	ent->classname = "target_changelevel";
	Q_strlcpy(level.nextmap, map, sizeof(level.nextmap));
	ent->map = level.nextmap;
	return ent;
}

/*
=================
Match_End

An end of match condition has been reached
=================
*/
void Match_End() {
	gentity_t *ent;
	// [MuffMode] Publish or reject pending map-source changes before the
	// higher-priority MyMap queue is validated and consumed.
	MM_HandleMapPoolCvarChanges();

	level.match_state = matchst_t::MATCH_ENDED;
	level.match_state_timer = 0_sec;

	// [MuffMode] Freeze every result consumer before settlement so direct admin
	// and vote endings share the same reservation-aware path as QueueIntermission.
	MM_Duel_MatchEnd_AdjustScores();
	MM_MatchStats_FreezeResultTime();
	MM_PlayerStats_OnMatchEnd();
	MM_MatchStats_End();

	// see if there is a queued map to go to
	if (const char *queued_map = MM_MQ_Go_Next(); queued_map && *queued_map) {
		BeginIntermission(CreateTargetChangeLevel(queued_map));
		return;
	}
	
	// stay on same level flag
	if (g_dm_same_level->integer) {
		BeginIntermission(CreateTargetChangeLevel(level.mapname));
		return;
	}

	if (*level.forcemap) {
		BeginIntermission(CreateTargetChangeLevel(level.forcemap));
		return;
	}

	// [MuffMode] Thin vanilla hook for map-list rotation selection.
	if (MM_TryBeginIntermissionFromMapList())
		return;

	if (level.nextmap[0]) // go to a specific map
	{
		BeginIntermission(CreateTargetChangeLevel(level.nextmap));
		return;
	}

	// search for a changelevel
	ent = G_FindByString<&gentity_t::classname>(nullptr, "target_changelevel");

	if (!ent) { // the map designer didn't include a changelevel,
		// so create a fake ent that goes back to the same level
		BeginIntermission(CreateTargetChangeLevel(level.mapname));
		return;
	}

	//MS_EndMatchExport();

	BeginIntermission(ent);
}

/*
=================
CheckNeedPass
=================
*/
static void CheckNeedPass() {
	int need;
	static int32_t password_modified, spectator_password_modified;

	// if password or spectator_password has changed, update needpass
	// as needed
	if (Cvar_WasModified(password, password_modified) || Cvar_WasModified(spectator_password, spectator_password_modified)) {
		need = 0;

		if (*password->string && Q_strcasecmp(password->string, "none"))
			need |= 1;
		if (*spectator_password->string && Q_strcasecmp(spectator_password->string, "none"))
			need |= 2;

		gi.cvar_set("needpass", G_Fmt("{}", need).data());
	}
}

void QueueIntermission(const char *msg, bool boo, bool reset) {
	// [MuffMode] GT_ARENA deliberately keeps the singleton match state in
	// warmup while its rooms run independent state machines. Its session
	// timelimit/no-human exit still needs the shared intermission path.
	if (level.intermission_queued ||
		(notGT(GT_ARENA) && level.match_state < matchst_t::MATCH_IN_PROGRESS))
		return;

	level.tied_overtime_start = 0_sec;

	Q_strlcpy(level.intermission_victor_msg, msg, sizeof(level.intermission_victor_msg));

	//gi.LocBroadcast_Print(PRINT_CHAT, "MATCH END: {}\n", level.intermission_victor_msg[0] ? level.intermission_victor_msg : "Unknown Reason");
	gi.Com_PrintFmt("MATCH END: {}\n", level.intermission_victor_msg[0] ? level.intermission_victor_msg : "Unknown Reason");
	gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex(boo ? "insane/insane4.wav" : "world/xian1.wav"), 1, ATTN_NONE, 0);

	if (reset) {
		Match_Reset();
	} else {
		// Freeze every result consumer while reconnect reservations still expose
		// their authoritative saved score. Match_End later performs export and
		// enters the intermission, but these calls are exact-once.
		MM_Duel_MatchEnd_AdjustScores();
		MM_MatchStats_FreezeResultTime();
		MM_PlayerStats_OnMatchEnd();

		level.match_state = matchst_t::MATCH_ENDED;
		level.match_state_timer = 0_sec;
		level.match_time = level.time;
		level.intermission_queued = level.time;

		gi.configstring(CS_CDTRACK, "0");
	}
}

int GT_ScoreLimit() {
	if (GT(GT_ARENA))
		return 0; // each RA2/RA3 arena owns its own series limit
	if (GT(GT_STRIKE))
		return capturelimit->integer;
	if (GTF(GTF_ROUNDS))
		return roundlimit->integer;
	if (GT(GT_CTF))
		return capturelimit->integer;
	return fraglimit->integer;
}

const char *GT_ScoreLimitString() {
	if (GT(GT_STRIKE) || GT(GT_CTF))
		return "capture";
	if (GTF(GTF_ROUNDS))
		return "round";
	return "frag";
}

/*
=================
CheckDMExitRules

There will be a delay between the time the exit is qualified for
and the time everyone is moved to the intermission spot, so you
can see the last frag/capture.
=================
*/
void CheckDMExitRules() {

	// if at the intermission, wait for all non-bots to
	// signal ready, then go to next level
	if (level.intermission_time) {
		CheckDMIntermissionExit();
		return;
	}

	if (MM_ArenaUsesGenericNoPlayersTimeout(GT(GT_ARENA)) &&
		!level.num_playing_clients && noplayerstime->integer &&
		level.time > level.no_players_time +
			gtime_t::from_min(noplayerstime->integer)) {
		Match_End();
		return;
	}

	if (level.intermission_queued) {
		if (level.time - level.intermission_queued >= 1_sec) {
			level.intermission_queued = 0_ms;
			Match_End();
		}
		return;
	}

	if (GT(GT_ARENA)) {
		// [MuffMode] Multi-arena map exit handling cannot depend on the
		// singleton match/round state below.
		MM_Arena_CheckExitRules();
		return;
	}

	if (level.match_state < matchst_t::MATCH_IN_PROGRESS)
		return;
	
	if (level.time - level.match_time <= FRAME_TIME_MS)
		return;

	if (MM_Horde_CheckAllFightersLost())
		return;

	if (GTF(GTF_ROUNDS) && level.round_state != roundst_t::ROUND_ENDED)
		return;

	if (MM_Horde_CheckMatchEnd())
		return;

	const size_t logical_human_players =
		(level.num_playing_human_clients > 0
			? static_cast<size_t>(level.num_playing_human_clients) : 0) +
		MM_Ghost_ActiveHumanPlayingReservationCount();
	const size_t logical_players =
		(level.num_playing_clients > 0
			? static_cast<size_t>(level.num_playing_clients) : 0) +
		MM_Ghost_ActivePlayingReservationCount();

	if (!g_dm_allow_no_humans->integer && logical_human_players == 0) {
		QueueIntermission("No human players remaining.", true, false);
		return;
	}
	
	if (minplayers->integer > 0 &&
		logical_players < static_cast<size_t>(minplayers->integer)) {
		QueueIntermission("Not enough players remaining.", true, false);
		return;
	}

	bool teams = MM_UseTeamScoreLimit(Teams(), GT(GT_RR));
	
	if (teams && g_teamplay_force_balance->integer) {
		const int logical_red = level.num_playing_red + static_cast<int>(
			MM_Ghost_ActivePlayingReservationCountForTeam(TEAM_RED));
		const int logical_blue = level.num_playing_blue + static_cast<int>(
			MM_Ghost_ActivePlayingReservationCountForTeam(TEAM_BLUE));
		if (abs(logical_red - logical_blue) > 1) {
			if (g_teamplay_auto_balance->integer) {
				TeamBalance(true);
				const int balanced_red = level.num_playing_red +
					static_cast<int>(
						MM_Ghost_ActivePlayingReservationCountForTeam(TEAM_RED));
				const int balanced_blue = level.num_playing_blue +
					static_cast<int>(
						MM_Ghost_ActivePlayingReservationCountForTeam(TEAM_BLUE));
				if (abs(balanced_red - balanced_blue) > 1)
					QueueIntermission("Teams are imbalanced.", true, true);
			} else {
				QueueIntermission("Teams are imbalanced.", true, true);
			}
			return;
		}
	}

	if (timelimit->value) {
		if (!(GTF(GTF_ROUNDS)) || level.round_state == roundst_t::ROUND_ENDED) {
			if (level.time >= level.match_time + gtime_t::from_min(timelimit->value) + level.overtime) {
				// check for overtime
				if (ScoreIsTied()) {
					if (g_dm_tie_max_time->integer > 0) {
						if (!level.tied_overtime_start) {
							level.tied_overtime_start = level.time;
						} else if (level.time - level.tied_overtime_start >= gtime_t::from_sec(g_dm_tie_max_time->integer)) {
							QueueIntermission("Tie timeout reached. Match ends in a draw.", false, false);
							return;
						}
					}

					if (GT(GT_DUEL) && g_dm_overtime->integer > 0) {
						level.overtime += gtime_t::from_sec(g_dm_overtime->integer);
						gi.LocBroadcast_Print(PRINT_CENTER, "Overtime!\n{} added", G_TimeString(g_dm_overtime->integer * 1000, false));
						MM_Announce(mm_announce_event_t::Overtime, world);
					} else if (!level.suddendeath) {
						gi.LocBroadcast_Print(PRINT_CENTER, "Sudden Death!");
						MM_Announce(mm_announce_event_t::SuddenDeath, world);
						level.suddendeath = true;
					}

					return;
				}

				level.tied_overtime_start = 0_sec;

				// find the winner and broadcast it
				if (teams) {
					if (level.team_scores[TEAM_RED] > level.team_scores[TEAM_BLUE]) {
						QueueIntermission(G_Fmt("{} Team WINS with a final score of {} to {}.\n", Teams_TeamName(TEAM_RED), level.team_scores[TEAM_RED], level.team_scores[TEAM_BLUE]).data(), false, false);
						return;
					}
					if (level.team_scores[TEAM_BLUE] > level.team_scores[TEAM_RED]) {
						QueueIntermission(G_Fmt("{} Team WINS with a final score of {} to {}.\n", Teams_TeamName(TEAM_BLUE), level.team_scores[TEAM_BLUE], level.team_scores[TEAM_RED]).data(), false, false);
						return;
					}
				} else {
					const gclient_t *leader = LogicalIndividualScoreView().leader;
					if (leader)
						QueueIntermission(G_Fmt("{} WINS with a final score of {}.", leader->resp.netname, leader->resp.score).data(), false, false);
					else
						QueueIntermission("Timelimit hit.", false, false);
					return;
				}

				QueueIntermission("Timelimit hit.", false, false);
				return;
			}
		}
	}
	
	if (mercylimit->integer > 0) {
		if (teams) {
			if (level.team_scores[TEAM_RED] >= level.team_scores[TEAM_BLUE] + mercylimit->integer) {
				QueueIntermission(G_Fmt("{} hit the mercylimit ({}).", Teams_TeamName(TEAM_RED), mercylimit->integer).data(), true, false);
				return;
			}
			if (level.team_scores[TEAM_BLUE] >= level.team_scores[TEAM_RED] + mercylimit->integer) {
				QueueIntermission(G_Fmt("{} hit the mercylimit ({}).", Teams_TeamName(TEAM_BLUE), mercylimit->integer).data(), true, false);
				return;
			}
		} else if (!MM_Horde_SkipMercyLimit() && notGT(GT_RR)) {
			const logical_individual_score_view_t scores =
				LogicalIndividualScoreView();
			const gclient_t *cl1 = scores.leader;
			const gclient_t *cl2 = scores.runner_up;
			if (cl1 && cl2) {
				if (cl1->resp.score >= cl2->resp.score + mercylimit->integer) {
					QueueIntermission(G_Fmt("{} hit the mercylimit ({}).", cl1->resp.netname, mercylimit->integer).data(), true, false);
					return;
				}
			}
		}
	}

	// check for sudden death
	if (ScoreIsTied())
		return;

	if (MM_Horde_SkipFragScoreLimit())
		return;

	// no score limit in race
	if (false) // Race mode removed
		return;

	int	scorelimit = GT_ScoreLimit();
	if (!scorelimit) return;

	// Red Rover is an arena mode like CA: the match ends after `roundlimit` rounds (or the
	// timelimit backstop above), never on a frag target. But unlike CA it scores individual
	// frags rather than team round-wins, so the winner is the frag leader after that many
	// rounds. GT_ScoreLimit() returns roundlimit here (GTF_ROUNDS), and level.round_number
	// is the round just finished (we only reach this at ROUND_ENDED).
	if (GT(GT_RR)) {
		if (level.round_number >= scorelimit) {
			const gclient_t *leader = LogicalIndividualScoreView().leader;
			QueueIntermission(leader
				? G_Fmt("{} WINS! (most frags after {} rounds)", leader->resp.netname, scorelimit).data()
				: "Round limit hit.", false, false);
		}
		return;
	}

	if (teams) {
		if (level.team_scores[TEAM_RED] >= scorelimit) {
			QueueIntermission(G_Fmt("{} WINS! (hit the {} limit)", Teams_TeamName(TEAM_RED), GT_ScoreLimitString()).data(), false, false);
			return;
		}
		if (level.team_scores[TEAM_BLUE] >= scorelimit) {
			QueueIntermission(G_Fmt("{} WINS! (hit the {} limit)", Teams_TeamName(TEAM_BLUE), GT_ScoreLimitString()).data(), false, false);
			return;
		}
	} else {
		const logical_individual_score_view_t scores =
			LogicalIndividualScoreView();
		if (scores.leader && scores.leader->resp.score >= scorelimit) {
			QueueIntermission(G_Fmt("{} WINS! (hit the {} limit)",
				scores.leader->resp.netname, GT_ScoreLimitString()).data(),
				false, false);
			return;
		}
	}
}

static bool Match_NextMap() {
	if (level.match_state == matchst_t::MATCH_ENDED) {
		level.match_state = matchst_t::MATCH_WARMUP_DELAYED;
		level.warmup_notice_time = level.time;
		level.warmup_gametype_hud_time = level.time;
		Match_Reset();
		return true;
	}
	return false;
}

/*
============
Teams_CalcRankings

End game rankings
============
*/
void Teams_CalcRankings(std::array<uint32_t, MAX_CLIENTS> &player_ranks) {
	if (!Teams())
		return;

	// we're all winners.. or losers. whatever
	if (level.team_scores[TEAM_RED] == level.team_scores[TEAM_BLUE]) {
		player_ranks.fill(1);
		return;
	}

	team_t winning_team = (level.team_scores[TEAM_RED] > level.team_scores[TEAM_BLUE]) ? TEAM_RED : TEAM_BLUE;

	for (auto player : active_clients())
		if (player->client->pers.spawned && ClientIsPlaying(player->client))
			player_ranks[player->s.number - 1] = player->client->sess.team == winning_team ? 1 : 2;
}

/*
=============
BeginIntermission
=============
*/
void BeginIntermission(gentity_t *targ) {
	if (level.intermission_time)
		return; // already activated

	// [MuffMode] Every path into intermission, including a map's direct
	// target_changelevel exit, must settle and export the live result. Each hook
	// is internally exact-once, so the normal Match_End path safely reaches this
	// same boundary after already freezing the result.
	MM_Duel_MatchEnd_AdjustScores();
	MM_MatchStats_FreezeResultTime();
	MM_PlayerStats_OnMatchEnd();
	MM_MatchStats_End();

	game.autosaved = false;

	level.intermission_time = level.time;

	// respawn any dead clients
	for (auto ec : active_clients()) {
		if (ec->health <= 0 || ec->client->eliminated) {
			ec->health = 1;
			// give us our max health back since it will reset
			// to pers.health; in instanced items we'd lose the items
			// we touched so we always want to respawn with our max.
			if (P_UseCoopInstancedItems())
				ec->client->pers.health = ec->client->pers.max_health = ec->max_health;

			ClientRespawn(ec);
		}
	}

	level.intermission_server_frame = gi.ServerFrame();
	level.changemap = targ->map;
	level.intermission_clear = targ->spawnflags.has(SPAWNFLAG_CHANGELEVEL_CLEAR_INVENTORY);
	level.intermission_eou = false;
	level.intermission_fade = targ->spawnflags.has(SPAWNFLAG_CHANGELEVEL_FADE_OUT);

	// destroy all player trails
	PlayerTrail_Destroy(nullptr);
	
	// Clean up any dangling entity references before map transition
	for (auto ec : active_clients()) {
		// Clear follow target if entity is not in use OR if client pointer is invalid
		if (ec->client->follow_target && (!ec->client->follow_target->inuse || !ec->client->follow_target->client)) {
			SetFollowTarget(ec, nullptr);
		}
		// Clear any bot-specific entity references
		if (ec->svflags & SVF_BOT) {
			// Clear any dangling bot entity references
			ec->goalentity = nullptr;
			ec->movetarget = nullptr;
		}
	}

	// [Paril-KEX] update game level entry
	G_UpdateLevelEntry();

	// A leading '*' is the engine's end-of-unit marker. Do not scan an
	// untrusted map string without a bound before ExitLevel validates it.
	if (G_IsValidStringPtr(level.changemap) && level.changemap[0] == '*') {
		if (coop->integer) {
			for (auto ec : active_clients()) {
				// strip players of all keys between units
				for (uint8_t n = 0; n < IT_TOTAL; n++)
					if (itemlist[n].flags & IF_KEY)
						ec->client->pers.inventory[n] = 0;
			}
		}

		if (level.achievement && level.achievement[0]) {
			gi.WriteByte(svc_achievement);
			gi.WriteString(level.achievement);
			gi.multicast(vec3_origin, MULTICAST_ALL, true);
		}

		level.intermission_eou = true;

		// "no end of unit" maps handle intermission differently
		if (!targ->spawnflags.has(SPAWNFLAG_CHANGELEVEL_NO_END_OF_UNIT))
			G_EndOfUnitMessage();
		else if (targ->spawnflags.has(SPAWNFLAG_CHANGELEVEL_IMMEDIATE_LEAVE) && !deathmatch->integer) {
			// Need to call this now
			G_ReportMatchDetails(true);
			level.intermission_exit = true; // go immediately to the next level
			return;
		}
	} else {
		if (!deathmatch->integer) {
			level.intermission_exit = true; // go immediately to the next level
			return;
		}
	}

	// Call while intermission is running
	G_ReportMatchDetails(true);

	level.intermission_exit = false;
	const logical_individual_score_view_t individual_scores =
		!Teams() && notGT(GT_ARENA)
			? LogicalIndividualScoreView()
			: logical_individual_score_view_t{};
	const bool individual_draw = individual_scores.participant_count >= 2 &&
		individual_scores.leader && individual_scores.runner_up &&
		individual_scores.leader->resp.score ==
			individual_scores.runner_up->resp.score;

	//SetIntermissionPoint();

	// move all clients to the intermission point
	for (auto ec : active_clients()) {
		MoveClientToIntermission(ec);
		if (GT(GT_ARENA))
			continue;
		if (Teams()) {
			if (level.team_scores[TEAM_RED] > level.team_scores[TEAM_BLUE])
				MM_Announce(mm_announce_event_t::RedWins, ec);
			else if (level.team_scores[TEAM_BLUE] > level.team_scores[TEAM_RED])
				MM_Announce(mm_announce_event_t::BlueWins, ec);
			continue;
		}
		if (GT(GT_DUEL)) {
			switch (MM_Duel_FinalOutcomeForClient(ec)) {
			case mm_duel_final_outcome_t::win:
				MM_Announce(mm_announce_event_t::YouWin, ec);
				break;
			case mm_duel_final_outcome_t::loss:
				MM_Announce(mm_announce_event_t::YouLose, ec);
				break;
			default:
				break;
			}
			continue;
		}
		if (individual_draw)
			continue;
		const int client_num = static_cast<int>(ec - g_entities - 1);
		MM_Announce(client_num == individual_scores.leader_client_num
			? mm_announce_event_t::YouWin
			: mm_announce_event_t::YouLose, ec);
	}

}

/*
=============
ExitLevel
=============
*/
void ExitLevel() {
	MuffModeLog("DEBUG", "ExitLevel: entry - fading=%d exit=%d fade_time=%.2f level_time=%.2f in_frame=%d",
		level.intermission_fading, level.intermission_exit,
		level.intermission_fade_time.seconds(), level.time.seconds(),
		level.in_frame);
	const char* next_map = level.changemap ? level.changemap : level.nextmap;
	if (next_map && next_map[0]) {
		MuffModeLog("MAP", "Exiting level '%s', next map: '%s'", level.mapname, next_map);
	} else {
		MuffModeLog("MAP", "Exiting level '%s' (no next map specified)", level.mapname);
	}
	
	MuffModeLog("DEBUG", "ExitLevel: screenshot check - dm=%d, shots=%d, humans=%d, playing=%d",
		deathmatch->integer, g_dm_intermission_shots->integer, level.num_playing_human_clients, level.num_playing_clients);

	// Take screenshot at match end (when exiting level)
	if (deathmatch->integer && g_dm_intermission_shots->integer && level.num_playing_human_clients > 0) {
		std::tm local_time = {};
		const std::time_t now = std::time(nullptr);
		std::string timestamp = "unknown-time";
		if (muffmode::LocalTimeSnapshot(now, local_time)) {
			timestamp = std::string(G_Fmt("{}_{:02}_{:02}-{:02}_{:02}_{:02}",
				1900 + local_time.tm_year, local_time.tm_mon + 1, local_time.tm_mday,
				local_time.tm_hour, local_time.tm_min, local_time.tm_sec));
		} else {
			MuffModeLog("WARN", "ExitLevel: unable to resolve local time for screenshot filename");
		}

		// Helper lambda to sanitize filename components (replace spaces and invalid chars)
		auto sanitize_name = [](const char *name, const char *fallback = "player") -> std::string {
			std::string result;
			for (const char *p = name; *p; p++) {
				char c = *p;
				// Replace spaces/path separators with underscores, remove other invalid filename chars
				if (c == ' ') {
					result += '_';
				} else if (c == '/' || c == '\\' || c == ':') {
					result += '_';
				} else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || 
				           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
					result += c;
				}
				// Skip other characters
			}
			return result.empty() ? fallback : result;
		};

		const std::string safe_mapname = sanitize_name(level.mapname, "map");
		std::string screenshot_cmd;
		constexpr size_t MAX_SCREENSHOT_FILENAME = 120;

		if (GT(GT_DUEL) && level.num_playing_clients >= 2) {
			MuffModeLog("DEBUG", "ExitLevel: duel screenshot - sorted[0]=%d, sorted[1]=%d",
				level.sorted_clients[0], level.sorted_clients[1]);

			const int c1 = level.sorted_clients[0];
			const int c2 = level.sorted_clients[1];
			const bool duel_indices_valid = (c1 >= 0 && c1 < MAX_CLIENTS && c2 >= 0 && c2 < MAX_CLIENTS);

			gentity_t *e1 = duel_indices_valid ? &g_entities[c1 + 1] : nullptr;
			gentity_t *e2 = duel_indices_valid ? &g_entities[c2 + 1] : nullptr;

			if (!duel_indices_valid) {
				MuffModeLog("DEBUG", "ExitLevel: invalid duel sorted client indices (%d, %d), using fallback names", c1, c2);
			}

			MuffModeLog("DEBUG", "ExitLevel: e1->client=%p, e2->client=%p",
				(void *)(e1 ? e1->client : nullptr), (void *)(e2 ? e2->client : nullptr));
			std::string n1 = sanitize_name((e1 && e1->client && e1->inuse) ? e1->client->resp.netname : "", "player1");
			std::string n2 = sanitize_name((e2 && e2->client && e2->inuse) ? e2->client->resp.netname : "", "player2");

			std::string filename = std::string(G_Fmt("{}-vs-{}-{}-{}",
				n1.c_str(), n2.c_str(), safe_mapname.c_str(), timestamp.c_str()));
			if (filename.length() > MAX_SCREENSHOT_FILENAME)
				filename.resize(MAX_SCREENSHOT_FILENAME);
			screenshot_cmd = std::string(G_Fmt("screenshot {}\n", filename));
			gi.Com_PrintFmt("Screenshot saved: {}\n", filename.c_str());
		} else {
			gentity_t *ent = G_PrimaryClientEntity();
			const char *raw_name = "player";
			if (ent) {
				gentity_t *follow = ent->client->follow_target;
				raw_name = (follow && follow->inuse && follow->client)
					? follow->client->resp.netname
					: ent->client->resp.netname;
			}
			std::string name = sanitize_name(raw_name);

			const int gametype_index = static_cast<int>(MM_CurrentGametype());
			std::string filename = std::string(G_Fmt("{}-{}-{}-{}", gt_short_name_upper[gametype_index],
				name.c_str(), safe_mapname.c_str(), timestamp.c_str()));
			if (filename.length() > MAX_SCREENSHOT_FILENAME)
				filename.resize(MAX_SCREENSHOT_FILENAME);
			screenshot_cmd = std::string(G_Fmt("screenshot {}\n", filename));
			gi.Com_PrintFmt("Screenshot saved: {}\n", filename.c_str());
		}
		MuffModeLog("DEBUG", "ExitLevel: screenshot command='%s' (raw_mapname='%s', safe_mapname='%s')",
			screenshot_cmd.c_str(), level.mapname, safe_mapname.c_str());
		if (!screenshot_cmd.empty())
			gi.AddCommandString(screenshot_cmd.c_str());
	}

	// [Paril-KEX] N64 fade
	if (level.intermission_fade) {
		level.intermission_fade_time = level.time + 1.3_sec;
		level.intermission_fading = true;
		return;
	}

	MuffModeLog("DEBUG", "ExitLevel: calling ClientEndServerFrames");
	ClientEndServerFrames();

	// Prevent repeated ExitLevel calls on subsequent frames before the map change executes.
	level.intermission_exit = false;

	MuffModeLog("DEBUG", "ExitLevel: ClientEndServerFrames done, checking Duel_RemoveLoser");
	// if we are running a duel, kick the loser to queue,
	// which will automatically grab the next queued player and restart
	if (deathmatch->integer && GT(GT_DUEL))
		MM_Duel_RemoveLoser();

	level.intermission_time = 0_ms;

	// [Paril-KEX] support for intermission completely wiping players
	// back to default stuff
	if (level.intermission_clear) {
		level.intermission_clear = false;

		for (auto ec : active_clients()) {
			// [Kex] Maintain user info to keep the player skin. 
			char userinfo[MAX_INFO_STRING];
			memcpy(userinfo, ec->client->pers.userinfo, sizeof(userinfo));

			ec->client->pers = ec->client->resp.coop_respawn = {};
			ec->health = 0; // this should trip the power armor, etc to reset as well

			memcpy(ec->client->pers.userinfo, userinfo, sizeof(userinfo));
			memcpy(ec->client->resp.coop_respawn.userinfo, userinfo, sizeof(userinfo));
		}
	}

	// [Paril-KEX] end of unit, so clear level trackers
	if (level.intermission_eou) {
		game.level_entries = {};

		// give all players their lives back
		if (g_coop_enable_lives->integer)
			for (auto player : active_clients())
				player->client->pers.lives = g_coop_num_lives->integer + 1;
	}

	// For duel mode, if changemap is null, restart on the same map
	if (GT(GT_DUEL) && level.changemap == nullptr) {
		level.changemap = level.mapname;
	}

	if (level.changemap == nullptr) {
		gi.Com_Error("Got null changemap when trying to exit level. Was a trigger_changelevel configured correctly?");
		return;
	}
	
	// changemap comes from owned level/map state. Reject null-page sentinels,
	// then keep every read inside the fixed engine path contract.
	if (!G_IsValidStringPtr(level.changemap)) {
		gi.Com_Error("Got invalid changemap pointer when trying to exit level.");
		return;
	}

	size_t map_len = 0;
	while (map_len < MAX_QPATH && level.changemap[map_len])
		map_len++;
	if (map_len == 0 || map_len == MAX_QPATH) {
		gi.Com_ErrorFmt("Got invalid changemap length ({}) when trying to exit level.", map_len);
		return;
	}
	const std::string_view changemap(level.changemap, map_len);
	if (!muffmode::maps::IsSafeChangeLevelTokenText(changemap, MAX_QPATH)) {
		gi.Com_ErrorFmt("Got unsafe changemap token when trying to exit level: {}", changemap);
		return;
	}

	// for N64 mainly, but if we're directly changing to "victorXXX.pcx" then
	// end game
	size_t start_offset = changemap.front() == '*' ? 1 : 0;

	MuffModeLog("DEBUG", "ExitLevel: issuing gamemap command for '%s'", level.changemap);
	if (map_len > (6 + start_offset) &&
		!Q_strncasecmp(changemap.data() + start_offset, "victor", 6) &&
		!Q_strncasecmp(changemap.data() + map_len - 4, ".pcx", 4))
		gi.AddCommandString(G_Fmt("endgame \"{}\"\n", changemap.substr(start_offset)).data());
	else
		gi.AddCommandString(G_Fmt("gamemap \"{}\"\n", changemap).data());

	MuffModeLog("DEBUG", "ExitLevel: complete");
	level.changemap = nullptr;
}

/*
=============
CheckPowerups
=============
*/
static int powerup_minplayers_mod_count = -1;
static int numplayers_check = -1;

static void CheckPowerups() {
	bool docheck = false;

	if (powerup_minplayers_mod_count != g_dm_powerups_minplayers->integer) {
		powerup_minplayers_mod_count = g_dm_powerups_minplayers->integer;
		docheck = true;
	}

	if (numplayers_check != level.num_playing_clients) {
		numplayers_check = level.num_playing_clients;
		docheck = true;
	}

	if (!docheck)
		return;

	bool	disable = g_dm_powerups_minplayers->integer > 0 && (level.num_playing_clients < g_dm_powerups_minplayers->integer);
	gentity_t	*ent = nullptr;
	size_t	i;
	const size_t entity_count = G_AllocatedEntityCount();
	for (ent = g_entities + 1, i = 1; i < entity_count; i++, ent++) {
		if (!ent->inuse || !ent->item)
			continue;

		if (!(ent->item->flags & IF_POWERUP))
			continue;
		/*
		if (!(ent->svflags & SVF_NOCLIENT))
			continue;
		*/
		if (g_quadhog->integer && ent->item->id == IT_POWERUP_QUAD)
			return;

		if (disable) {
			ent->s.renderfx |= (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE);
			ent->s.effects |= EF_COLOR_SHELL;
		} else {
			ent->s.renderfx &= ~(RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE);
			ent->s.effects &= ~EF_COLOR_SHELL;
		}
	}
}

/*
=============
CheckMinMaxPlayers
=============
*/
static int minplayers_mod_count = -1;
static int maxplayers_mod_count = -1;

static void CheckMinMaxPlayers() {

	if (!deathmatch->integer)
		return;

	if (minplayers_mod_count == minplayers->modified_count &&
			maxplayers_mod_count == maxplayers->modified_count)
		return;

	const int client_capacity = static_cast<int>(game.maxclients);

	// Set active-player limits from the client array actually allocated by the
	// game, not from a stale or externally clamped cvar string.
	if (minplayers->integer < 1) {
		gi.Com_PrintFmt("minplayers must be at least 1; clamped to 1.\n");
		gi.cvar_set("minplayers", "1");
	}
	else if (minplayers->integer > client_capacity)
		gi.cvar_set("minplayers", G_Fmt("{}", client_capacity).data());
	if (maxplayers->integer < 0)
		gi.cvar_set("maxplayers", G_Fmt("{}", client_capacity).data());
	if (maxplayers->integer > client_capacity)
		gi.cvar_set("maxplayers", G_Fmt("{}", client_capacity).data());
	else if (maxplayers->integer < minplayers->integer) gi.cvar_set("maxplayers", minplayers->string);

	minplayers_mod_count = minplayers->modified_count;
	maxplayers_mod_count = maxplayers->modified_count;
}

static void CheckCvars() {
	if (Cvar_WasModified(g_airaccelerate, game.airacceleration_modified)) {
		// [Paril-KEX] air accel handled by game DLL now, and allow
		// it to be changed in sp/coop
		gi.configstring(CS_AIRACCEL, G_Fmt("{}", g_airaccelerate->integer).data());
		pm_config.airaccel = g_airaccelerate->integer;
	}

	if (Cvar_WasModified(g_gravity, game.gravity_modified))
		level.gravity = g_gravity->value;

	CheckMinMaxPlayers();
}

static bool G_AnyDeadPlayersWithoutLives() {
	for (auto player : active_clients())
		if (player->health <= 0 && (!player->client->pers.lives || player->client->eliminated))
			return true;

	return false;
}

/*
================
CheckDMEndFrame
================
*/
static void CheckDMEndFrame() {
	if (!deathmatch->integer)
		return;

	// [MuffMode] Match/round state machine lives in muffmode/mm_match
	MM_Match_RunFrame();

	// see if it is time to end a deathmatch
	CheckDMExitRules();
}

/*
================
G_RunFrame

Advances the world by 0.1 seconds
================
*/
static inline void G_RunFrame_(bool main_loop) {
	MM_PROFILE_ZONE("G_RunFrame_");
	MM_PROFILE_INC(frame_runs);

	if (main_loop)
		MM_PROFILE_INC(frame_main_loop_runs);
	else
		MM_PROFILE_INC(frame_non_main_loop_runs);

	if (level.in_frame)
		MuffModeLog("ERROR", "G_RunFrame_: re-entrant call detected! (main_loop=%d)", main_loop);
	level.in_frame = true;

	muffmode::combat_heatmap::RunFrame();
	MM_Ghost_RunFrame();
	MM_PlayerStats_RunFrame();
	MM_ClientProfile_RunFrame();
	MM_MatchStats_RunFrame();
	// [MuffMode] Keep structured map sources current even while timeout logic
	// skips GT_Changes and the rest of the normal cvar polling path.
	MM_HandleMapPoolCvarChanges();

	if (level.timeout_in_place > 0_ms) {
		int t = (level.timeout_in_place).seconds<int>() + 1;

		if (!level.countdown_check || level.countdown_check.seconds<int>() > t) {
			if (!(t % 10) || t < 10)
				gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex(G_Fmt("world/{}{}.wav", t, t >= 20 ? "sec" : "").data()), 1, ATTN_NONE, 0);
			level.countdown_check = gtime_t::from_sec(t);
		}

		level.timeout_in_place -= FRAME_TIME_MS;
		if (level.timeout_in_place <= 0_ms)
			TimeoutEnd();

		ClientEndServerFrames();
		level.in_frame = false;
		return;
	} else {
		// track gametype changes and update accordingly
		GT_Changes();

		// [MuffMode] Thin vanilla hook for vote lifecycle ticking.
		MM_CheckVote();

		// for tracking changes
		CheckCvars();

		CheckPowerups();

		CheckRuleset();

		Bot_UpdateDebug();

		level.time += FRAME_TIME_MS;

		if (level.intermission_fading) {
			if (level.intermission_fade_time > level.time) {
				float alpha = clamp(1.0f - (level.intermission_fade_time - level.time - 300_ms).seconds(), 0.f, 1.f);

				for (auto player : active_clients())
					player->client->ps.screen_blend = { 0, 0, 0, alpha };
			} else {
				level.intermission_fade = level.intermission_fading = false;
				ExitLevel();
			}

			level.in_frame = false;

			return;
		}

		// exit intermissions

		if (level.intermission_exit) {
			ExitLevel();
			level.in_frame = false;
			return;
		}
	}

	// reload the map start save if restart time is set (all players are dead)
	if (level.coop_level_restart_time > 0_ms && level.time > level.coop_level_restart_time) {
		ClientEndServerFrames();
		gi.AddCommandString("restart_level\n");
	}

	// clear client coop respawn states; this is done
	// early since it may be set multiple times for different
	// players
	if (InCoopStyle() && (g_coop_enable_lives->integer || g_coop_squad_respawn->integer)) {
		for (auto player : active_clients()) {
			if (player->client->respawn_time >= level.time)
				player->client->coop_respawn_state = COOP_RESPAWN_WAITING;
			else if (g_coop_enable_lives->integer && player->health <= 0 && player->client->pers.lives == 0)
				player->client->coop_respawn_state = COOP_RESPAWN_NO_LIVES;
			else if (g_coop_enable_lives->integer && G_AnyDeadPlayersWithoutLives())
				player->client->coop_respawn_state = COOP_RESPAWN_NO_LIVES;
			else
				player->client->coop_respawn_state = COOP_RESPAWN_NONE;
		}
	}

	//
	// treat each object in turn
	// even the world gets a chance to think
	//
	gentity_t *ent = &g_entities[0];
	const size_t frame_entity_count = G_AllocatedEntityCount();
	for (size_t i = 0; i < frame_entity_count; i++, ent++) {
		MM_PROFILE_INC(frame_entities_visited);

		if (!ent->inuse) {
			// defer removing client info so that disconnected, etc works
			if (i > 0 && i <= game.maxclients) {
				if (ent->timestamp && level.time < ent->timestamp) {
					int32_t playernum = ent - g_entities - 1;
					gi.configstring(CS_PLAYERSKINS + playernum, "");
					ent->timestamp = 0_ms;
				}
			}
			continue;
		}

		level.current_entity = ent;

		// Paril: RF_BEAM entities update their old_origin by hand.
		if (!(ent->s.renderfx & RF_BEAM))
			ent->s.old_origin = ent->s.origin;

		if (i > 0 && i <= game.maxclients && ent->client && !ent->client->pers.connected) {
			MM_PROFILE_INC(frame_clients_visited);
			continue;
		}

		// if the ground entity moved, make sure we are still on it
		if ((ent->groundentity) && (ent->groundentity->linkcount != ent->groundentity_linkcount)) {
			contents_t mask = G_GetClipMask(ent);

			if (!(ent->flags & (FL_SWIM | FL_FLY)) && (ent->svflags & SVF_MONSTER)) {
				ent->groundentity = nullptr;
				M_CheckGround(ent, mask);
			} else {
				// if it's still 1 point below us, we're good
				trace_t tr = gi.trace(ent->s.origin, ent->mins, ent->maxs, ent->s.origin + ent->gravityVector, ent,
					mask);

				if (tr.startsolid || tr.allsolid || tr.ent != ent->groundentity)
					ent->groundentity = nullptr;
				else
					ent->groundentity_linkcount = ent->groundentity->linkcount;
			}
		}

		Entity_UpdateState(ent);

		if (i > 0 && i <= game.maxclients) {
			MM_PROFILE_INC(frame_clients_visited);
			ClientBeginServerFrame(ent);
			continue;
		}

		MM_PROFILE_INC(frame_nonclients_visited);
		G_RunEntity(ent);
	}

	CheckDMEndFrame();

	// see if needpass needs updated
	CheckNeedPass();

	if (InCoopStyle() && (g_coop_enable_lives->integer || g_coop_squad_respawn->integer)) {
		// rarely, we can see a flash of text if all players respawned
		// on some other player, so if everybody is now alive we'll reset
		// back to empty
		bool reset_coop_respawn = true;

		for (auto player : active_clients()) {
			if (player->health > 0) {	//muff: changed from >= to >
				reset_coop_respawn = false;
				break;
			}
		}

		if (reset_coop_respawn) {
			for (auto player : active_clients())
				player->client->coop_respawn_state = COOP_RESPAWN_NONE;
		}
	}

	// build the playerstate_t structures for all players
	ClientEndServerFrames();

	// [Paril-KEX] if not in intermission and player 1 is loaded in
	// the game as an entity, increase timer on current entry
	gentity_t *primary_client = G_PrimaryClientEntity();
	if (level.entry && !level.intermission_time && primary_client && primary_client->client->pers.connected)
		level.entry->time += FRAME_TIME_S;

	// [Paril-KEX] run monster pains now
	const size_t pain_entity_count = G_FrameEntityCount();
	for (size_t i = 0; i < pain_entity_count; i++) {
		gentity_t *e = &g_entities[i];

		if (!e->inuse || !(e->svflags & SVF_MONSTER))
			continue;

		M_ProcessPain(e);
	}

	level.in_frame = false;
}

static inline bool G_AnyClientsSpawned() {
	for (auto player : active_clients())
		if (player->client && player->client->pers.spawned)
			return true;

	return false;
}

void G_RunFrame(bool main_loop) {
	MM_PROFILE_ZONE("G_RunFrame");

	const bool any_clients_spawned = G_AnyClientsSpawned();

	if (main_loop && !any_clients_spawned && !level.timeout_in_place && !MM_Ghost_HasActiveReservations()) {
		MM_PROFILE_INC(frame_no_client_skips);
		return;
	}

	const int configured_frames_per_frame = g_frames_per_frame->integer;
	const int frames_per_frame = MM_ClampFramesPerServerFrame(configured_frames_per_frame);
	if (frames_per_frame != configured_frames_per_frame) {
		const std::string normalized_value = fmt::format("{}", frames_per_frame);
		gi.cvar_forceset("g_frames_per_frame", normalized_value.c_str());
	}

	for (int i = 0; i < frames_per_frame; i++)
		G_RunFrame_(main_loop);

	// [MuffMode] Simulation substeps may advance many times per engine frame,
	// but restore commits and their reliable presentation fan-out get one shared
	// scheduling opportunity for the actual server frame.
	MM_Ghost_RunServerFrame();

	// match details.. only bother if there's at least 1 player in-game
	// and not already end of game
	if (main_loop && any_clients_spawned && !level.intermission_time) {
		constexpr gtime_t report_time = 45_sec;

		if (level.time - level.next_match_report > report_time) {
			level.next_match_report = level.time + report_time;
			G_ReportMatchDetails(false);
		}
	}

	MM_PROFILE_FRAME_MARK("G_RunFrame");
}

/*
================
G_PrepFrame

This has to be done before the world logic, because
player processing happens outside RunFrame
================
*/
void G_PrepFrame() {
	const size_t entity_count = G_AllocatedEntityCount();
	for (size_t i = 0; i < entity_count; i++)
		g_entities[i].s.event = EV_NONE;

	for (auto player : active_clients())
		player->client->ps.stats[STAT_HIT_MARKER] = 0;

	globals.server_flags &= ~SERVER_FLAG_INTERMISSION;

	if (level.intermission_time) {
		globals.server_flags |= SERVER_FLAG_INTERMISSION;
	}
}
