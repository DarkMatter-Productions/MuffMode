// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "g_debug_log.h"
#include "muffmode/mm_maps.h"
#include "bots/bot_includes.h"
#include "monsters/m_player.h"	// match starts

CHECK_GCLIENT_INTEGRITY;
CHECK_ENTITY_INTEGRITY;

constexpr int32_t DEFAULT_GRAPPLE_SPEED = 650; // speed of grapple in flight
constexpr float	  DEFAULT_GRAPPLE_PULL_SPEED = 650; // speed player is pulled at

std::mt19937 mt_rand;

game_locals_t  game;
level_locals_t level;

local_game_import_t  gi;

/*static*/ char local_game_import_t::print_buffer[0x10000];

/*static*/ std::array<char[MAX_INFO_STRING], MAX_LOCALIZATION_ARGS> local_game_import_t::buffers;
/*static*/ std::array<const char *, MAX_LOCALIZATION_ARGS> local_game_import_t::buffer_ptrs;

game_export_t  globals;
spawn_temp_t   st;

cached_modelindex		sm_meat_index;
cached_soundindex		snd_fry;

gentity_t *g_entities;

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
cvar_t *filterban;

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
cvar_t *g_dm_weapons_stay;
cvar_t *g_drop_cmds;
cvar_t *g_entity_override_dir;
cvar_t *g_entity_override_load;
cvar_t *g_entity_override_save;
cvar_t *g_eyecam;
cvar_t *g_fast_doors;
cvar_t *g_frag_messages;
cvar_t *g_frenzy;
cvar_t *g_friendly_fire;
cvar_t *g_frozen_time;
cvar_t *g_grapple_damage;
cvar_t *g_grapple_fly_speed;
cvar_t *g_grapple_offhand;
cvar_t *g_grapple_pull_speed;
cvar_t *g_gravity;
cvar_t *g_horde_starting_wave;
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
cvar_t *g_map_list;
cvar_t *g_map_list_shuffle;
cvar_t *g_map_pool;
cvar_t *g_votable_gametypes;
cvar_t *g_votable_rulesets;
cvar_t *g_match_lock;
cvar_t *g_matchstats;
cvar_t *g_maxvelocity;
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
	/* GT_FREEZE */ GTF_TEAMS | GTF_ELIMINATION,
	/* GT_STRIKE */ GTF_TEAMS | GTF_ARENA | GTF_ROUNDS | GTF_CTF | GTF_ELIMINATION,
	/* GT_RR */ GTF_TEAMS | GTF_ROUNDS | GTF_ARENA,
	/* GT_LMS */ GTF_ELIMINATION,
	/* GT_HORDE */ GTF_ROUNDS,
	/* GT_BALL */ 0,
	/* GT_INSTAGIB */ GTF_FRAGS,
	/* GT_NADEFEST */ GTF_FRAGS
};

// =================================================

static gentity_t *FindClosestPlayerToPoint(vec3_t point) {
	float	bestplayerdistance;
	vec3_t	v;
	float	playerdistance;
	gentity_t *closest = nullptr;

	bestplayerdistance = 9999999;

	for (auto ec : active_clients()) {
		if (ec->health <= 0 || ec->client->eliminated)
			continue;

		v = point - ec->s.origin;
		playerdistance = v.length();

		if (playerdistance < bestplayerdistance) {
			bestplayerdistance = playerdistance;
			closest = ec;
		}
	}

	return closest;
}

struct weighted_item_t;

using weight_adjust_func_t = void(*)(const weighted_item_t &item, float &weight);

void adjust_weight_health(const weighted_item_t &item, float &weight);
void adjust_weight_weapon(const weighted_item_t &item, float &weight);
void adjust_weight_ammo(const weighted_item_t &item, float &weight);
void adjust_weight_armor(const weighted_item_t &item, float &weight);

constexpr struct weighted_item_t {
	const char *classname;
	int32_t					min_level = -1, max_level = -1;
	float					weight = 1.0f;
	float					lvl_w_adjust = 0;
	int						flags;
	item_id_t				item[4];
	weight_adjust_func_t	adjust_weight = nullptr;
} items[] = {
	{ "item_health_small" },

	{ "item_health",				-1,	-1, 1.0f,	0,		0,		{ IT_NULL },					adjust_weight_health },
	{ "item_health_large",			-1,	-1, 0.85f,	0,		0,		{ IT_NULL },					adjust_weight_health },

	{ "item_armor_shard" },
	{ "item_armor_jacket",			-1,	4,	0.65f,	0,		0,		{ IT_NULL },					adjust_weight_armor },
	{ "item_armor_combat",			2,	-1, 0.62f,	0,		0,		{ IT_NULL },					adjust_weight_armor },
	{ "item_armor_body",			4,	-1, 0.35f,	0,		0,		{ IT_NULL },					adjust_weight_armor },

	{ "weapon_shotgun",				-1,	-1, 0.98f,	0,		0,		{ IT_NULL },					adjust_weight_weapon },
	{ "weapon_supershotgun",		2,	-1, 1.02f,	0,		0,		{ IT_NULL },					adjust_weight_weapon },
	{ "weapon_machinegun",			-1,	-1, 1.05f,	0,		0,		{ IT_NULL },					adjust_weight_weapon },
	{ "weapon_chaingun",			3,	-1, 1.01f,	0,		0,		{ IT_NULL },					adjust_weight_weapon },
	{ "weapon_grenadelauncher",		4,	-1, 0.75f,	0,		0,		{ IT_NULL },					adjust_weight_weapon },

	{ "ammo_shells",				-1,	-1,	1.25f,	0,		0,		{ IT_NULL },					adjust_weight_ammo },
	{ "ammo_bullets",				-1,	-1,	1.25f,	0,		0,		{ IT_NULL },					adjust_weight_ammo },
	{ "ammo_grenades",				2,	-1,	1.25f,	0,		0,		{ IT_NULL },					adjust_weight_ammo },
};

void adjust_weight_health(const weighted_item_t &item, float &weight) {}

void adjust_weight_weapon(const weighted_item_t &item, float &weight) {}

void adjust_weight_ammo(const weighted_item_t &item, float &weight) {}

void adjust_weight_armor(const weighted_item_t &item, float &weight) {}

//	classname,						min_level, max_level, weight, lvl_w_adjust, flags, items
constexpr weighted_item_t monsters[] = {
	{ "monster_soldier_light",		-1,	7,	1.50f,	-0.45f,		MF_GROUND,				{ IT_HEALTH_SMALL } },
	{ "monster_soldier",			-1,	7,	0.85f,	-0.25f,		MF_GROUND,				{ IT_AMMO_BULLETS_SMALL, IT_HEALTH_SMALL } },
	{ "monster_soldier_ss",			2,	7,	1.01f,	-0.125f,	MF_GROUND,				{ IT_AMMO_SHELLS_SMALL, IT_HEALTH_SMALL } },
	{ "monster_soldier_hypergun",	2,	9,	1.2f,	0.15f,		MF_GROUND,				{ IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL } },
	{ "monster_soldier_lasergun",	3,	9,	1.15f,	0.2f,		MF_GROUND,				{ IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL } },
	{ "monster_soldier_ripper",		3,	9,	1.25f,	0.25f,		MF_GROUND,				{ IT_AMMO_CELLS_SMALL, IT_HEALTH_SMALL } },
	{ "monster_infantry",			3,	16,	1.05f,	0.125f,		MF_GROUND,				{ IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS } },
	{ "monster_gunner",				4,	16,	1.08f,	0.5f,		MF_GROUND,				{ IT_AMMO_GRENADES, IT_AMMO_BULLETS_SMALL } },
	{ "monster_berserk",			4,	16,	1.05f,	0.1f,		MF_GROUND,				{ IT_ARMOR_SHARD } },
	//{ "monster_flipper",			4,	8,	0.85f,	-0.15f,		MF_WATER,				{ IT_NULL } },
	{ "monster_parasite",			5,	16,	1.04f,	-0.08f,		MF_GROUND,				{ IT_NULL } },
	{ "monster_gladiator",			5,	16,	1.07f,	0.3f,		MF_GROUND,				{ IT_AMMO_SLUGS } },
	{ "monster_gekk",				6,	16,	0.99f,	-0.15f,		MF_GROUND | MF_WATER,	{ IT_NULL } },
	{ "monster_brain",				6,	16,	0.95f,	0,			MF_GROUND,				{ IT_AMMO_CELLS_SMALL } },
	{ "monster_flyer",				6,	16,	0.92f,	0.15f,		MF_GROUND | MF_AIR,		{ IT_AMMO_CELLS_SMALL } },
	{ "monster_floater",			7,	16,	0.9f,	0,			MF_GROUND | MF_AIR,		{ IT_NULL } },
	{ "monster_mutant",				7,	16,	0.85f,	0,			MF_GROUND,				{ IT_NULL } },
	{ "monster_hover",				8,	16,	0.8f,	0,			MF_GROUND | MF_AIR,		{ IT_NULL } },
	{ "monster_guncmdr",			8,	-1,	0,		0.125f,		MF_GROUND | MF_MEDIUM,	{ IT_AMMO_GRENADES, IT_AMMO_BULLETS_SMALL, IT_AMMO_BULLETS, IT_AMMO_CELLS_SMALL } },
	{ "monster_chick",				9,	20,	1.01f,	-0.05f,		MF_GROUND,				{ IT_AMMO_ROCKETS_SMALL, IT_AMMO_ROCKETS } },
	{ "monster_daedalus",			9,	-1,	0.99f,	0.05f,		MF_GROUND | MF_AIR,		{ IT_AMMO_CELLS_SMALL } },
	{ "monster_medic",				10,	16,	0.95f,	-0.05f,		MF_GROUND,				{ IT_HEALTH_SMALL, IT_HEALTH_MEDIUM } },
	{ "monster_tank",				11,	-1,	0.85f,	0,			MF_GROUND | MF_MEDIUM,	{ IT_AMMO_ROCKETS } },
	{ "monster_chick_heat",			12,	-1,	0.87f,	0.065f,		MF_GROUND,				{ IT_AMMO_CELLS_SMALL, IT_AMMO_CELLS } },
	{ "monster_tank_commander",		12,	-1,	0.45f,	0.16f,		MF_GROUND | MF_MEDIUM,	{ IT_AMMO_ROCKETS_SMALL, IT_AMMO_BULLETS_SMALL, IT_AMMO_ROCKETS, IT_AMMO_BULLETS } },
	{ "monster_medic_commander",	13,	-1,	0.4f,	0.15f,		MF_GROUND | MF_MEDIUM,	{ IT_AMMO_CELLS_SMALL, IT_HEALTH_MEDIUM, IT_HEALTH_LARGE } },
	{ "monster_kamikaze",			13,	-1,	0.85f,	0.04f,		MF_GROUND | MF_AIR,		{ IT_NULL } },
	/*
	{ "monster_boss2",				0,	0,	0,		0,			MF_GROUND | MF_BOSS,	{ IT_HEALTH_MEGA, IT_AMMO_BULLETS_LARGE, IT_AMMO_ROCKETS } },	// hornet
	{ "monster_jorg",				0,	0,	0,		0,			MF_GROUND | MF_BOSS,	{ IT_HEALTH_MEGA, IT_AMMO_CELLS_LARGE } },
	{ "monster_makron",				0,	0,	0,		0,			MF_GROUND | MF_BOSS,	{ IT_HEALTH_MEGA, IT_AMMO_CELLS_LARGE } },
	{ "monster_guardian",			0,	0,	0,		0,			MF_GROUND | MF_BOSS,	{ IT_HEALTH_MEGA } },
	{ "monster_arachnid",			0,	0,	0,		0,			MF_GROUND | MF_BOSS,	{ IT_HEALTH_MEGA } },
	{ "monster_boss5",				0,	0,	0,		0,			MF_GROUND | MF_BOSS,	{ IT_HEALTH_MEGA, IT_AMMO_BULLETS_LARGE, IT_AMMO_CELLS, IT_AMMO_GRENADES } },	// super tank
	{ "monster_carrier",			0,	0,	0,		0,			MF_GROUND | MF_BOSS,	{ IT_AMMO_BULLETS_LARGE, IT_AMMO_SLUGS, IT_AMMO_GRENADES, IT_POWERUPS_QUAD } },
	{ "monster_widow",				0,	0,	0,		0,			MF_GROUND | MF_BOSS,	{ IT_HEALTH_MEGA, IT_AMMO_ROUNDS, IT_POWERUPS_QUAD } },
	{ "monster_widow2",				0,	0,	0,		0,			MF_GROUND | MF_BOSS,	{ IT_HEALTH_MEGA, IT_AMMO_ROUNDS, IT_POWERUPS_QUAD } },
	*/
};

struct picked_item_t {
	const weighted_item_t *item;
	float weight;
};

static gitem_t *Horde_PickItem() {
	// collect valid items
	static std::array<picked_item_t, q_countof(items)> picked_items;
	static size_t num_picked_items;

	num_picked_items = 0;

	float total_weight = 0;
	
	for (auto &item : items) {
		if (item.min_level != -1 && level.round_number < item.min_level)
			continue;
		if (item.max_level != -1 && level.round_number > item.max_level)
			continue;

		float weight = item.weight + ((level.round_number - item.min_level) * item.lvl_w_adjust);

		if (item.adjust_weight)
			item.adjust_weight(item, weight);

		if (weight <= 0)
			continue;

		total_weight += weight;
		picked_items[num_picked_items++] = { &item, total_weight };
	}

	if (!total_weight)
		return nullptr;

	float r = frandom() * total_weight;

	for (size_t i = 0; i < num_picked_items; i++)
		if (r < picked_items[i].weight)
			return FindItemByClassname(picked_items[i].item->classname);

	return nullptr;
}

static const char *Horde_PickMonster() {
	// collect valid monsters
	static std::array<picked_item_t, q_countof(items)> picked_monsters;
	static size_t num_picked_monsters;

	num_picked_monsters = 0;

	float total_weight = 0;

	for (auto &monster : monsters) {
		if (monster.min_level != -1 && level.round_number < monster.min_level)
			continue;
		if (monster.max_level != -1 && level.round_number > monster.max_level)
			continue;
		
		float weight = monster.weight + ((level.round_number - monster.min_level) * monster.lvl_w_adjust);

		if (monster.adjust_weight)
			monster.adjust_weight(monster, weight);

		if (weight <= 0)
			continue;

		total_weight += weight;
		picked_monsters[num_picked_monsters++] = { &monster, total_weight };
	}

	if (!total_weight)
		return nullptr;

	float r = frandom() * total_weight;

	for (size_t i = 0; i < num_picked_monsters; i++)
		if (r < picked_monsters[i].weight)
			return picked_monsters[i].item->classname;

	return nullptr;
}

static void Horde_RunSpawning() {
	if (notGT(GT_HORDE))
		return;

	bool warmup = level.match_state == MATCH_WARMUP_DEFAULT || level.match_state == MATCH_WARMUP_READYUP;
	
	if (!warmup && level.round_state != ROUND_IN_PROGRESS)
		return;

	if (warmup && (level.total_monsters - level.killed_monsters >= 30))
		return;

	if (level.horde_all_spawned)
		return;

	if (level.horde_monster_spawn_time <= level.time) {
		gentity_t *e = G_Spawn();
		e->classname = Horde_PickMonster();
		select_spawn_result_t result = SelectDeathmatchSpawnPoint(nullptr, vec3_origin, SPAWN_FARTHEST, false, true, false, false);

		if (result.any_valid) {
			e->s.origin = result.spot->s.origin;
			e->s.angles = result.spot->s.angles;

			e->item = Horde_PickItem();
			ED_CallSpawn(e);
			level.horde_monster_spawn_time = warmup ? level.time + 5_sec : level.time + random_time(0.3_sec, 0.5_sec);

			e->enemy = FindClosestPlayerToPoint(e->s.origin);
			if (e->enemy)
				FoundTarget(e);

			if (!warmup) {
				level.horde_num_monsters_to_spawn--;

				if (!level.horde_num_monsters_to_spawn) {
					//gi.LocBroadcast_Print(PRINT_CENTER, "All monsters spawned.\nClean up time!");
					level.horde_all_spawned = true;
				}
			}
		} else
			level.horde_monster_spawn_time = warmup ? level.time + 5_sec : level.time + 1_sec;
	}
}

static void Horde_Init() {
	// this crashes the game
/*
	if (notGT(GT_HORDE))
		return;

	// precache all items
	for (auto &item : itemlist)
		PrecacheItem(&item);

	// all monsters too
	for (auto &monster : monsters) {
		gentity_t *e = G_Spawn();
		e->classname = monster.classname;
		ED_CallSpawn(e);
		G_FreeEntity(e);
	}
	*/
}

static bool Horde_AllMonstersDead() {
	for (size_t i = 0; i < globals.max_entities; i++) {
		if (!g_entities[i].inuse)
			continue;
		else if (g_entities[i].svflags & SVF_MONSTER) {
			if (!g_entities[i].deadflag && g_entities[i].health > 0)
				return false;
		}
	}

	return true;
}

// =================================================


void G_LoadMOTD() {
	// load up ent override
	const char *name = G_Fmt("baseq2/{}", g_motd_filename->string[0] ? g_motd_filename->string : "motd.txt").data();
	FILE *f = fopen(name, "rb");
	bool valid = true;
	if (f != NULL) {
		char *buffer = nullptr;
		size_t length;
		size_t read_length;

		fseek(f, 0, SEEK_END);
		length = ftell(f);
		fseek(f, 0, SEEK_SET);

		if (length > 0x40000) {
			gi.Com_PrintFmt("{}: MoTD file length exceeds maximum: \"{}\"\n", __FUNCTION__, name);
			valid = false;
		}
		if (valid) {
			buffer = (char *)gi.TagMalloc(length + 1, '\0');
			if (length) {
				read_length = fread(buffer, 1, length, f);

				if (length != read_length) {
					gi.Com_PrintFmt("{}: MoTD file read error: \"{}\"\n", __FUNCTION__, name);
					valid = false;
				}
			}
		}
		fclose(f);
		
		if (valid) {
			game.motd = (const char *)buffer;
			game.motd_mod_count++;
			if (g_verbose->integer)
				gi.Com_PrintFmt("{}: MotD file verified and loaded: \"{}\"\n", __FUNCTION__, name);
		} else {
			gi.Com_PrintFmt("{}: MotD file load error for \"{}\", discarding.\n", __FUNCTION__, name);
		}
	}
}

int check_ruleset = -1;
static void CheckRuleset() {
	if (game.ruleset && check_ruleset == g_ruleset->modified_count)
		return;

	game.ruleset = (ruleset_t)clamp(g_ruleset->integer, (int)RS_NONE + 1, (int)RS_NUM_RULESETS - 1);

	if ((int)game.ruleset != g_ruleset->integer)
		gi.cvar_forceset("g_ruleset", G_Fmt("{}", (int)game.ruleset).data());

	check_ruleset = g_ruleset->modified_count;

	gi.LocBroadcast_Print(PRINT_HIGH, "Ruleset: {}\n", rs_long_name[(int)game.ruleset]);
}

static void InitGametype() {
	constexpr const char *COOP = "coop";
	bool force_dm = false;

	if (g_gametype->integer < 0 || g_gametype->integer >= GT_NUM_GAMETYPES)
		gi.cvar_forceset("g_gametype", G_Fmt("{}", clamp(g_gametype->integer, (int)GT_FIRST, (int)GT_LAST)).data());
	
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

		if (pmax != floor(pmax / 2))
			gi.cvar_set("maxplayers", G_Fmt("{}", floor(pmax / 2) * 2).data());
	}
}

// Gametype tracking variables (used by both ChangeGametype and GT_Changes)
static int gt_teamplay = 0;
static int gt_ctf = 0;
static int gt_g_gametype = 0;
static bool gt_teams_on = false;
static gametype_t gt_check = GT_NONE;

void ChangeGametype(gametype_t gt) {
	switch (gt) {
	case gametype_t::GT_CTF:
		if (!ctf->integer)
			gi.cvar_forceset("ctf", "1");
		break;
	case gametype_t::GT_TDM:
		if (!teamplay->integer)
			gi.cvar_forceset("teamplay", "1");
		break;
	default:
		if (ctf->integer)
			gi.cvar_forceset("ctf", "0");
		if (teamplay->integer)
			gi.cvar_forceset("teamplay", "0");
		break;
	}

	if (!deathmatch->integer) {
		gi.Com_Print("Forcing deathmatch.\n");
		gi.cvar_forceset("deathmatch", "1");
	}

	if ((int)gt != g_gametype->integer) {
		MuffModeLog("GAMETYPE", "Changing gametype from %s (%d) to %s (%d)", 
		           gt_short_name[g_gametype->integer], g_gametype->integer,
		           gt_short_name[(int)gt], (int)gt);
		gi.cvar_forceset("g_gametype", G_Fmt("{}", (int)gt).data());

		// Force all human clients through explicit join flow after gametype change.
		// Without this, existing session state (team + initialised) can carry across
		// map reloads and spawn players directly in-game even when auto-join is off.
		for (auto ec : active_clients()) {
			if (!ec->client)
				continue;
			if (ec->client->sess.is_a_bot || (ec->svflags & SVF_BOT))
				continue;
			ec->client->sess.team = TEAM_NONE;
			ec->client->sess.duel_queued = false;
			ec->client->sess.initialised = false;
			ec->client->initial_menu_shown = false;
			ec->client->initial_menu_delay = level.time + 10_hz;
		}
		
		// Sync g_instagib cvar when switching to/from instagib gametype
		if (gt == gametype_t::GT_INSTAGIB) {
			if (!g_instagib->integer)
				gi.cvar_forceset("g_instagib", "1");
		} else if (g_gametype->integer == (int)gametype_t::GT_INSTAGIB) {
			// Switching away from instagib - clear the cvar
			if (g_instagib->integer)
				gi.cvar_forceset("g_instagib", "0");
		}
		
		// Sync g_nadefest cvar when switching to/from nadefest gametype
		if (gt == gametype_t::GT_NADEFEST) {
			if (!g_nadefest->integer)
				gi.cvar_forceset("g_nadefest", "1");
		} else if (g_gametype->integer == (int)gametype_t::GT_NADEFEST) {
			// Switching away from nadefest - clear the cvar
			if (g_nadefest->integer)
				gi.cvar_forceset("g_nadefest", "0");
		}
		
		// Clear duel handicap data when switching away from duel mode
		if (g_gametype->integer == (int)gametype_t::GT_DUEL && gt != gametype_t::GT_DUEL) {
			// Clear all handicap data for all clients
			for (auto ec : active_clients()) {
				if (!ec->client)
					continue;
				
				ec->client->handicap.restricted_weapons = 0;
				ec->client->handicap.damage_dealt_multiplier = 1.0f;
				ec->client->handicap.damage_received_multiplier = 1.0f;
				ec->client->handicap.health_multiplier = 1.0f;
			}
		}
		
		// Execute gametype-specific cfg when gametype actually changes
		// This ensures cfg runs on gametype change, not on every map load
		if (g_gametype_cfg->integer && deathmatch->integer) {
			gi.AddCommandString(G_Fmt("exec gt-{}.cfg\n", gt_short_name_upper[(int)gt]).data());
		}

		// Reset shuffle flag so the map list gets reshuffled for the new gametype
		extern bool g_map_list_shuffled;
		g_map_list_shuffled = false;
		
		// Update tracking vars to prevent GT_Changes() from triggering a redundant reload
		// when Vote_Pass_Gametype() has already handled the map change properly
		gt_g_gametype = g_gametype->modified_count;
		gt_check = (gametype_t)g_gametype->integer;
		gt_teamplay = teamplay->modified_count;
		gt_ctf = ctf->modified_count;
	}
}

void GT_Changes() {
	if (!deathmatch->integer)
		return;

	// do these checks only once level has initialised
	if (!level.init)
		return;

	// If g_map_list_shuffle was changed via console, shuffle immediately
	extern bool g_map_list_shuffled;
	static int s_map_list_shuffle_modified = -1;
	if (s_map_list_shuffle_modified != g_map_list_shuffle->modified_count) {
		s_map_list_shuffle_modified = g_map_list_shuffle->modified_count;
		if (g_map_list_shuffle->integer == 2) {
			G_ShuffleMapList();
			g_map_list_shuffled = true;
		}
	}

	bool changed = false, team_reset = false;
	gametype_t gt = gametype_t::GT_NONE;

	if (gt_g_gametype != g_gametype->modified_count) {
		gt = (gametype_t)clamp(g_gametype->integer, (int)GT_FIRST, (int)GT_LAST);

		if (gt != gt_check) {
			switch (gt) {
			case gametype_t::GT_TDM:
				if (!teamplay->integer)
					gi.cvar_forceset("teamplay", "1");
				break;
			case gametype_t::GT_CTF:
				if (!ctf->integer)
					gi.cvar_forceset("ctf", "1");
				break;
			default:
				if (teamplay->integer)
					gi.cvar_forceset("teamplay", "0");
				if (ctf->integer)
					gi.cvar_forceset("ctf", "0");
				break;
			}
			gt_teamplay = teamplay->modified_count;
			gt_ctf = ctf->modified_count;
			changed = true;
		}
	}

	if (!changed) {
		if (gt_teamplay != teamplay->modified_count) {
			if (teamplay->integer) {
				gt = gametype_t::GT_TDM;
				if (!teamplay->integer)
					gi.cvar_forceset("teamplay", "1");
				if (ctf->integer)
					gi.cvar_forceset("ctf", "0");
			} else {
				gt = gametype_t::GT_FFA;
				if (teamplay->integer)
					gi.cvar_forceset("teamplay", "0");
				if (ctf->integer)
					gi.cvar_forceset("ctf", "0");
			}
			changed = true;
			gt_teamplay = teamplay->modified_count;
			gt_ctf = ctf->modified_count;
		}
		if (gt_ctf != ctf->modified_count) {
			if (ctf->integer) {
				gt = gametype_t::GT_CTF;
				if (teamplay->integer)
					gi.cvar_forceset("teamplay", "0");
				if (!ctf->integer)
					gi.cvar_forceset("ctf", "1");
			} else {
				gt = gametype_t::GT_TDM;
				if (!teamplay->integer)
					gi.cvar_forceset("teamplay", "1");
				if (ctf->integer)
					gi.cvar_forceset("ctf", "0");
			}
			changed = true;
			gt_teamplay = teamplay->modified_count;
			gt_ctf = ctf->modified_count;
		}
	}

	if (!changed || gt == gametype_t::GT_NONE)
		return;

	//gi.Com_PrintFmt("GAMETYPE = {}\n", (int)gt);
	
	if (gt_teams_on != Teams()) {
		team_reset = true;
		gt_teams_on = Teams();
	}

	if (team_reset) {
		// move all to spectator first
		for (auto ec : active_clients()) {
			SetIntermissionPoint();

			ec->s.origin = level.intermission_origin;
			ec->client->ps.pmove.origin = level.intermission_origin;
			ec->client->ps.viewangles = level.intermission_angle;

			ec->client->awaiting_respawn = true;
			ec->client->ps.pmove.pm_type = PM_FREEZE;
			ec->client->ps.rdflags = RDF_NONE;
			ec->deadflag = false;
			ec->solid = SOLID_NOT;
			ec->movetype = MOVETYPE_FREECAM;
			ec->s.modelindex = 0;
			ec->svflags |= SVF_NOCLIENT;
			gi.linkentity(ec);
		}

		// set to team and reset match
		for (auto ec : active_clients()) {
			if (!ClientIsPlaying(ec->client))
				continue;
			SetTeam(ec, PickTeam(-1), false, false, true);
		}
	}

	if ((int)gt != gt_check) {
		gi.cvar_forceset("g_gametype", G_Fmt("{}", (int)gt).data());
		gt_g_gametype = g_gametype->modified_count;
		gt_check = (gametype_t)g_gametype->integer;
	} else return;

	//TODO: save ent string so we can simply reload it and Match_Reset
	//gi.AddCommandString("map_restart");

	MuffModeLog("DEBUG", "GT_Changes: issuing gamemap '%s' (gt=%d gt_check=%d gt_g_gametype=%d g_gametype->modified_count=%d teamplay=%d ctf=%d in_frame=%d)",
		level.mapname, (int)gt, (int)gt_check, gt_g_gametype, g_gametype->modified_count,
		teamplay->integer, ctf->integer, level.in_frame);
	gi.AddCommandString(G_Fmt("gamemap {}\n", level.mapname).data());

	// Return immediately after queuing map change to avoid rendering state corruption.
	// GT_PrecacheAssets() and GT_SetLongName() will be called during normal map initialization
	// in SpawnEntities() -> PrecacheStartItems() -> PrecacheAssets() -> GT_PrecacheAssets()
	// and SpawnEntities() -> GT_SetLongName().
	return;
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

	g_horde_starting_wave = gi.cvar("g_horde_starting_wave", "1", CVAR_SERVERINFO | CVAR_LATCH);

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

	// freeze tag
	g_frozen_time = gi.cvar("g_frozen_time", "180", CVAR_NOFLAGS);

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
	filterban = gi.cvar("filterban", "1", CVAR_NOFLAGS);

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
	g_coop_health_scaling = gi.cvar("g_coop_health_scaling", "0", CVAR_LATCH);
	g_corpse_sink_time = gi.cvar("g_corpse_sink_time", "15", CVAR_NOFLAGS);
	g_damage_scale = gi.cvar("g_damage_scale", "1", CVAR_NOFLAGS);
	g_disable_player_collision = gi.cvar("g_disable_player_collision", "0", CVAR_NOFLAGS);
	g_dm_allow_exit = gi.cvar("g_dm_allow_exit", "0", CVAR_NOFLAGS);
	g_dm_allow_no_humans = gi.cvar("g_dm_allow_no_humans", "1", CVAR_NOFLAGS);
	g_dm_auto_join = gi.cvar("g_dm_auto_join", "0", CVAR_NOFLAGS);
	g_dm_crosshair_id = gi.cvar("g_dm_crosshair_id", "1", CVAR_NOFLAGS);
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
	g_dm_weapons_stay = gi.cvar("g_dm_weapons_stay", "0", CVAR_NOFLAGS);
	g_drop_cmds = gi.cvar("g_drop_cmds", "7", CVAR_NOFLAGS);
	g_entity_override_dir = gi.cvar("g_entity_override_dir", "maps", CVAR_NOFLAGS);
	g_entity_override_load = gi.cvar("g_entity_override_load", "1", CVAR_NOFLAGS);
	g_entity_override_save = gi.cvar("g_entity_override_save", "0", CVAR_NOFLAGS);
	g_eyecam = gi.cvar("g_eyecam", "1", CVAR_NOFLAGS);
	g_fast_doors = gi.cvar("g_fast_doors", "1", CVAR_NOFLAGS);
	g_frames_per_frame = gi.cvar("g_frames_per_frame", "1", CVAR_NOFLAGS);
	g_friendly_fire = gi.cvar("g_friendly_fire", "0", CVAR_NOFLAGS);
	g_inactivity = gi.cvar("g_inactivity", "120", CVAR_NOFLAGS);
	g_infinite_ammo = gi.cvar("g_infinite_ammo", "0", CVAR_LATCH);
	g_instant_weapon_switch = gi.cvar("g_instant_weapon_switch", "0", CVAR_NOFLAGS);
	g_item_bobbing = gi.cvar("g_item_bobbing", "1", CVAR_NOFLAGS);
	g_knockback_scale = gi.cvar("g_knockback_scale", "1.0", CVAR_NOFLAGS);
	g_ladder_steps = gi.cvar("g_ladder_steps", "1", CVAR_NOFLAGS);
	g_lag_compensation = gi.cvar("g_lag_compensation", "1", CVAR_NOFLAGS);
	g_map_list = gi.cvar("g_map_list", "", CVAR_NOFLAGS);
	g_map_list_shuffle = gi.cvar("g_map_list_shuffle", "1", CVAR_NOFLAGS);
	g_map_pool = gi.cvar("g_map_pool", "", CVAR_NOFLAGS);
	g_votable_gametypes = gi.cvar("g_votable_gametypes", "", CVAR_NOFLAGS);
	g_votable_rulesets = gi.cvar("g_votable_rulesets", "", CVAR_NOFLAGS);
	g_match_lock = gi.cvar("g_match_lock", "0", CVAR_SERVERINFO);
	g_matchstats = gi.cvar("g_matchstats", "0", CVAR_NOFLAGS);
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

	// initialize all entities for this game
	game.maxentities = maxentities->integer;
	g_entities = (gentity_t *)gi.TagMalloc(game.maxentities * sizeof(g_entities[0]), TAG_GAME);
	globals.gentities = g_entities;
	globals.max_entities = game.maxentities;

	// initialize all clients for this game
	game.maxclients = maxclients->integer;
	game.clients = (gclient_t *)gi.TagMalloc(game.maxclients * sizeof(game.clients[0]), TAG_GAME);
	globals.num_entities = game.maxclients + 1;

	// how far back we should support lag origins for
	game.max_lag_origins = 20 * (0.1f / gi.frame_time_s);
	game.lag_origins = (vec3_t *)gi.TagMalloc(game.maxclients * sizeof(vec3_t) * game.max_lag_origins, TAG_GAME);

	level.start_time = level.time;

	level.ready_to_exit = false;

	level.match_state = matchst_t::MATCH_WARMUP_DELAYED;
	level.match_state_timer = 0_sec;
	level.match_time = level.time;
	level.warmup_notice_time = level.time;

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

	gt_teamplay = teamplay->modified_count;
	gt_ctf = ctf->modified_count;
	gt_g_gametype = g_gametype->modified_count;
	gt_teams_on = Teams();

	Horde_Init();

	G_LoadMOTD();

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

static void Monsters_KillAll() {
	for (size_t i = 0; i < globals.max_entities; i++) {
		if (!g_entities[i].inuse)
			continue;
		if (g_entities[i].svflags & SVF_MONSTER)
			//if (g_entities[i].health <= 0 || g_entities[i].deadflag || (g_entities[i].svflags & SVF_DEADMONSTER))
			G_FreeEntity(&g_entities[i]);
		level.total_monsters = 0;
		level.killed_monsters = 0;
	}
}

static void Entities_ItemTeams_Reset() {
	gentity_t	*ent;
	size_t		i;

	gentity_t	*master;
	int			count, choice;

	for (ent = g_entities + 1, i = 1; i < globals.num_entities; i++, ent++) {
		if (!ent->inuse)
			continue;

		if (!ent->item)
			continue;

		if (!ent->team)
			continue;

		if (!ent->teammaster)
			continue;

		master = ent->teammaster;

		ent->svflags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;
		gi.linkentity(ent);

		for (count = 0, ent = master; ent; ent = ent->chain, count++)
			;

		choice = irandom(count);
		for (count = 0, ent = master; count < choice; ent = ent->chain, count++)
			;
	}
	/*
	for (ent = g_entities + 1, i = 1; i < globals.num_entities; i++, ent++) {
		if (!ent->inuse)
			continue;

		if (!ent->team)
			continue;

		if (!ent->item)
			continue;

		ent->flags &= ~FL_TEAMSLAVE;
		ent->chain = ent->teamchain;
		ent->teamchain = nullptr;

		ent->svflags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;

		if (ent == ent->teammaster) {
			ent->nextthink = level.time + 10_hz;
			if (!ent->think)
				ent->think = RespawnItem;
		} else
			ent->nextthink = 0_sec;
	}
	*/
}

/*
============
Entities_Reset

Reset clients and items
============
*/
static void Entities_Reset(bool reset_players, bool reset_ghost, bool reset_score) {
	gentity_t *ent;
	size_t	i;

	// reset the players
	if (reset_players) {
		for (auto ec : active_clients()) {
			// [muff] Store bot team before reset to preserve it
			team_t saved_bot_team = TEAM_NONE;
			bool is_bot = (ec->svflags & SVF_BOT) || ec->client->sess.is_a_bot;
			if (is_bot && ec->client && ClientIsPlaying(ec->client)) {
				saved_bot_team = ec->client->sess.team;
			}

			ec->client->resp.ctf_state = 0;
			if (reset_score)
				ec->client->resp.score = 0;
			if (reset_ghost)
				ec->client->resp.ghost = nullptr;

			if (ClientIsPlaying(ec->client)) {
				if (reset_ghost) {
					// make up a ghost code
					Match_Ghost_Assign(ec);
				}
				//if (!ec->client->eliminated) {
				Weapon_Grapple_DoReset(ec->client);
				ec->svflags = SVF_NOCLIENT;
				ec->flags &= ~FL_GODMODE;

				ec->client->eliminated = false;
				ec->client->pers.dmg_scorer = 0;
				ec->client->pers.dmg_team = 0;
				ec->client->respawn_time = level.time;	// +random_time(1_sec, 4_sec);
				ec->client->pers.last_spawn_time = level.time;
				ec->client->ps.pmove.pm_type = PM_DEAD;
				ec->client->anim_priority = ANIM_DEATH;
				ec->s.frame = FRAME_death308 - 1;
				ec->client->anim_end = FRAME_death308;
				ec->deadflag = true;
				ec->movetype = MOVETYPE_NOCLIP;
				ec->client->ps.gunindex = 0;
				ec->client->ps.gunskin = 0;
				gi.linkentity(ec);
				//}
			}

			// [muff] Restore bot team assignment to keep them playing
			if (is_bot && ec->client && saved_bot_team != TEAM_NONE) {
				ec->client->sess.team = saved_bot_team;
				// Ensure bot SVF flags are preserved
				if (ec->client->sess.is_a_bot)
					ec->svflags |= SVF_BOT;
			}
		}

		CalculateRanks();
	}
	
	// reset the level items
	Tech_Reset();
	CTF_ResetFlags();

	Monsters_KillAll();

	Entities_ItemTeams_Reset();

	// reset item spawns and gibs/corpses, remove dropped items and projectiles
	for (ent = g_entities + 1, i = 1; i < globals.num_entities; i++, ent++) {
		if (!ent->inuse)
			continue;

		if (Q_strcasecmp(ent->classname, "bodyque") == 0 || Q_strcasecmp(ent->classname, "gib") == 0) {
			ent->svflags = SVF_NOCLIENT;
			ent->takedamage = false;
			ent->solid = SOLID_NOT;
			gi.unlinkentity(ent);
			G_FreeEntity(ent);
		} else if ((ent->svflags & SVF_PROJECTILE) || (ent->clipmask & CONTENTS_PROJECTILECLIP)) {
			G_FreeEntity(ent);
		} else if (ent->item) {
			// already processed in CTF_ResetFlags()
			if (ent->item->id == IT_FLAG_RED || ent->item->id == IT_FLAG_BLUE)
				continue;

			if (ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER)) {
				//G_FreeEntity(ent);
				ent->nextthink = level.time;
			} else {
				// powerups don't spawn in for a while
				if (ent->item->flags & IF_POWERUP) {
					if (g_quadhog->integer && ent->item->id == IT_POWERUP_QUAD) {
						G_FreeEntity(ent);
						QuadHog_SetupSpawn(5_sec);
					} else if (RS(RS_MM) || RS(RS_Q3A)) {
						ent->svflags |= SVF_NOCLIENT;
						ent->solid = SOLID_NOT;

						ent->nextthink = level.time + gtime_t::from_sec(irandom(30, 60));
						//if (!ent->think)
							ent->think = RespawnItem;
					}
					continue;
				} else {
					if (ent->svflags & (SVF_NOCLIENT | SVF_RESPAWNING) || ent->solid == SOLID_NOT) {
						gtime_t t = 0_sec;
						if (ent->random) {
							t += gtime_t::from_ms((crandom() * ent->random) * 1000);
							if (t < FRAME_TIME_MS) {
								t = FRAME_TIME_MS;
							}
						}
						//if (ent->item->id == IT_HEALTH_MEGA)
							ent->think = RespawnItem;
						ent->nextthink = level.time + t;
					}
				}
			}
		}
	}

	// Reset trains that might be used by elevators
	// This fixes elevators breaking after match start when trains have nextthink set
	for (ent = g_entities + 1, i = 1; i < globals.num_entities; i++, ent++) {
		if (!ent->inuse)
			continue;

		// Reset trains that are not currently moving
		if (!Q_strcasecmp(ent->classname, "func_train")) {
			if (ent->movetype == MOVETYPE_PUSH && !ent->velocity) {
				// Clear nextthink to allow elevators to work after match start
				ent->nextthink = 0_ms;
				// Clear target_ent so train can be reinitialized by elevator trigger
				ent->target_ent = nullptr;
			}
		}
	}
}
#if 0
static int SortRoundScores(const void *a, const void *b) {
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
	if (ca->resp.score - ca->resp.old_score > cb->resp.score - cb->resp.old_score)
		return -1;
	if (ca->resp.score - ca->resp.old_score < cb->resp.score - cb->resp.old_score)
		return 1;

	return 0;
}

gclient_t *Round_SaveOldPlayerScore() {
	gclient_t *cl = nullptr;
	int high = 0;
	for (auto ec : active_clients()) {

		ec->client->resp.old_score = ec->client->resp.score;
	}
}
#endif
/*
=============
Round_StartNew
=============
*/
static bool Round_StartNew() {
	if (!(GTF(GTF_ROUNDS))) {
		level.round_state = roundst_t::ROUND_NONE;
		level.round_state_timer = 0_sec;
		return false;
	}

	bool horde = GT(GT_HORDE);

	level.round_state = roundst_t::ROUND_COUNTDOWN;
	level.round_state_timer = level.time + gtime_t::from_sec(g_round_countdown->integer);
	level.countdown_check = 0_sec;

	if (!horde)
		Entities_Reset(!horde, false, false);

	if (GT(GT_STRIKE)) {
		level.strike_red_attacks ^= true;
		level.strike_flag_touch = false;

		int round_num;
		if (level.round_number && (!level.strike_turn_red && level.strike_turn_blue ||
			level.strike_turn_red && !level.strike_turn_blue))
			round_num = level.round_number;
		else {
			round_num = level.round_number + 1;
		}
		BroadcastTeamMessage(TEAM_RED, PRINT_CENTER, G_Fmt("Your team is on {}!\nRound {} - Begins in...", level.strike_red_attacks ? "OFFENSE" : "DEFENSE", round_num).data());
		BroadcastTeamMessage(TEAM_BLUE, PRINT_CENTER, G_Fmt("Your team is on {}!\nRound {} - Begins in...", !level.strike_red_attacks ? "OFFENSE" : "DEFENSE", round_num).data());
	} else {
		int round_num;

		if (horde && !level.round_number && g_horde_starting_wave->integer > 0)
			round_num = g_horde_starting_wave->integer;
		else
			round_num = level.round_number + 1;

		if (GT(GT_RR) && roundlimit->integer) {
			gi.LocBroadcast_Print(PRINT_CENTER, "{} {} of {}\nBegins in...", horde ? "Wave" : "Round", round_num, roundlimit->integer);
		} else
			gi.LocBroadcast_Print(PRINT_CENTER, "{} {}\nBegins in...", horde ? "Wave" : "Round", round_num);
	}

	AnnouncerSound(world, "round_begins_in", nullptr, false);

	return true;
}

/*
=============
Round_End
=============
*/
void Round_End() {
	// reset if not round based
	if (!(GTF(GTF_ROUNDS))) {
		level.round_state = roundst_t::ROUND_NONE;
		level.round_state_timer = 0_sec;
		return;
	}

	// there must be a round to end
	if (level.round_state != ROUND_IN_PROGRESS)
		return;

	level.round_state = roundst_t::ROUND_ENDED;
	level.round_state_timer = level.time + 3_sec;
	level.horde_all_spawned = false;
}

/*
=============
SetMatchID
=============
*/
static void SetMatchID() {
	//level.match_id = gt_short_name_upper[g_gametype->integer];
	//level.match_id += "-";
	level.match_id = stime();
	level.match_id = stime();
}

/*
============
Match_Start

Starts a match
============
*/
void Match_Start() {
	if (!deathmatch->integer)
		return;

	level.match_time = level.time;
	level.match_start_time = level.time;
	level.overtime = 0_sec;
	level.tied_overtime_start = 0_sec;

	const char *s = G_TimeString(timelimit->value ? timelimit->value * 1000 : 0, true);
	gi.configstring(CONFIG_MATCH_STATE, s);

	level.match_state = matchst_t::MATCH_IN_PROGRESS;
	level.match_state_timer = level.time;
	level.warmup_requisite = warmupreq_t::WARMUP_REQ_NONE;
	level.warmup_notice_time = 0_sec;

	level.team_scores[TEAM_RED] = level.team_scores[TEAM_BLUE] = 0;

	level.total_player_deaths = 0;

	memset(level.ghosts, 0, sizeof(level.ghosts));

	Entities_Reset(true, true, true);
	UnReadyAll();
	ValidateCaptains();

	// g_match_lock: lock teams via level.locked so unlockteam can override per-team
	if (g_match_lock->integer && Teams()) {
		level.locked[TEAM_RED] = true;
		level.locked[TEAM_BLUE] = true;
	} else if (g_match_lock->integer) {
		level.locked[TEAM_FREE] = true;
	}

	SetMatchID();

	gi.LocBroadcast_Print(PRINT_TTS, "Match ID: {}\n", level.match_id.c_str());

	if (GT(GT_STRIKE))
		level.strike_red_attacks = brandom();

	if (Round_StartNew())
		return;

	gi.LocBroadcast_Print(PRINT_CENTER, "FIGHT!");
	//gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex("misc/tele_up.wav"), 1, ATTN_NONE, 0);
	AnnouncerSound(world, "fight", "misc/tele_up.wav", true);
}

/*
============
Match_Reset
============
*/
void Match_Reset() {
	//if (!g_dm_do_warmup->integer) {
	//	Match_Start();
	//	return;
	//}

	Entities_Reset(true, true, true);
	UnReadyAll();
	ValidateCaptains();

	// clear any team locks (g_match_lock or captain locks) on reset
	level.locked[TEAM_SPECTATOR] = level.locked[TEAM_FREE] = level.locked[TEAM_RED] = level.locked[TEAM_BLUE] = false;

	level.match_time = level.time;
	level.match_state = matchst_t::MATCH_WARMUP_DEFAULT;
	level.warmup_requisite = warmupreq_t::WARMUP_REQ_NONE;
	level.warmup_notice_time = 0_sec;
	level.match_state_timer = 0_sec;
	level.intermission_queued = 0_sec;
	level.intermission_exit = false;
	level.intermission_time = 0_sec;
	level.tied_overtime_start = 0_sec;

	CalculateRanks();

	gi.LocBroadcast_Print(PRINT_TTS, "The match has been reset.\n");
}

/*
=============
Duel_AddPlayer

If there are less than two players in the arena, place the
next queued player in the game and restart
=============
*/
static bool Duel_AddPlayer(void) {
	if (notGT(GT_DUEL))
		return false;

	if (level.num_playing_clients >= 2)
		return false;

	// never change during intermission or outside of warmup (allow during readyup to refill if player disconnects)
	if (level.match_state > matchst_t::MATCH_WARMUP_READYUP || level.intermission_time || level.intermission_queued)
		return false;

	gclient_t *next_in_line = nullptr;
	
	for (auto ec : active_clients()) {
		if (ClientIsPlaying(ec->client))
			continue;

		//gi.Com_PrintFmt("Duel: {}, join time={}\n", ec->client->resp.netname, ec->client->sess.team_join_time.milliseconds());

		if (!ec->client->sess.duel_queued)
			continue;

		if (!next_in_line || ec->client->sess.team_join_time < next_in_line->sess.team_join_time) {
			//gi.Com_PrintFmt("Duel: A next-in-line considered: {}, join time={}\n", ec->client->resp.netname, ec->client->sess.team_join_time.milliseconds());
			next_in_line = ec->client;
		}
	}

	if (!next_in_line)
		return false;
	/*
	level.match_state_timer = 0_sec;
	level.match_state = matchst_t::MATCH_WARMUP_DEFAULT;
	level.warmup_requisite = warmupreq_t::WARMUP_REQ_NONE;
	level.warmup_notice_time = 0_sec;
	*/
	// set them to free-for-all team
	SetTeam(&g_entities[next_in_line - game.clients + 1], TEAM_FREE, false, true, false);

	return true;
}

/*
=======================
Duel_RemoveLoser

Make the loser a queued player at the back of the line
=======================
*/
static void Duel_RemoveLoser(void) {
	if (level.num_playing_clients != 2)
		return;
	
	gentity_t *ent = &g_entities[level.sorted_clients[1] + 1];

	if (!ent || !ent->client || !ent->client->pers.connected)
		return;

	if (g_verbose->integer)
		gi.Com_PrintFmt( "Duel: Moving the loser, {}, to end of queue.\n", ent->client->resp.netname);

	// make them a queued player
	SetTeam(ent, TEAM_NONE, false, true, false);
}

/*
=======================
Duel_MatchEnd_AdjustScores
=======================
*/
static void Duel_MatchEnd_AdjustScores(void) {
	if (notGT(GT_DUEL))
		return;

	int client_num;

	client_num = level.sorted_clients[0];
	if (game.clients[client_num].pers.connected) {
		game.clients[client_num].sess.wins++;
		//ClientUserinfoChanged(&g_entities[client_num], g_entities[client_num].client->pers.userinfo);
	}

	client_num = level.sorted_clients[1];
	if (game.clients[client_num].pers.connected) {
		// handled in SetTeam
		//game.clients[client_num].sess.losses++;
		//ClientUserinfoChanged(&g_entities[client_num], g_entities[client_num].client->pers.userinfo);
	}
}

/*
=============
ReadyAll
=============
*/
void ReadyAll() {
	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client))
			continue;
		ec->client->resp.ready = true;
	}
}

/*
=============
UnReadyAll
=============
*/
void UnReadyAll() {
	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client))
			continue;
		ec->client->resp.ready = false;
	}
}

/*
=============
CheckReady
=============
*/
static bool CheckReady() {
	if (!g_dm_do_readyup->integer)
		return true;

	uint8_t count_ready, count_humans, count_bots;

	count_ready = count_humans = count_bots = 0;
	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client))
			continue;
		if (ec->svflags & SVF_BOT || ec->client->sess.is_a_bot) {
			count_bots++;
			continue;
		}

		if (ec->client->resp.ready)
			count_ready++;
		count_humans++;
	}

	// wait if no players at all
	if (!count_humans && !count_bots)
		return true;

	// wait if below minimum players
	if (minplayers->integer > 0 && (count_humans + count_bots) < minplayers->integer)
		return false;

	// start if only bots
	if (!count_humans && count_bots && g_dm_allow_no_humans->integer)
		return true;

	// wait if no ready humans
	if (!count_ready)
		return false;

	// start if over min ready percentile
	if (((float)count_ready / (float)count_humans) * 100.0f >= g_warmup_ready_percentage->value * 100.0f)
		return true;

	return false;
}

/*
=============
CheckDMRoundState
=============
*/
static void CheckDMRoundState(void) {
	if (!(GTF(GTF_ROUNDS)))
		return;

	if (level.match_state != matchst_t::MATCH_IN_PROGRESS)
		return;

	// initiate round
	if (level.round_state == roundst_t::ROUND_NONE || level.round_state == roundst_t::ROUND_ENDED) {
		if (level.round_state_timer > level.time)
			return;

		if (GT(GT_RR) && level.round_state == roundst_t::ROUND_ENDED) {
			TeamShuffle();
		}

		Round_StartNew();
		return;
	}

	// start round
	if (level.round_state == roundst_t::ROUND_COUNTDOWN) {
		if (level.time >= level.round_state_timer) {
			for (auto ec : active_clients())
				ec->client->latched_buttons = BUTTON_NONE;

			level.round_state = roundst_t::ROUND_IN_PROGRESS;
			level.round_state_timer = level.time + gtime_t::from_min(roundtimelimit->value);

			bool turn = false;
			if (GT(GT_STRIKE)) {
				if (!level.strike_turn_red && !level.strike_turn_blue) {
					level.strike_turn_red = level.strike_red_attacks;
					level.strike_turn_blue = !level.strike_red_attacks;
				} else if (!level.strike_turn_red && level.strike_red_attacks) {
					level.strike_turn_red = true;
					turn = true;
				} else if (!level.strike_turn_blue && !level.strike_red_attacks) {
					level.strike_turn_blue = true;
					turn = true;
				} else {
					level.strike_turn_red = level.strike_red_attacks;
					level.strike_turn_blue = !level.strike_red_attacks;
				}
			}
			if (!turn) {
				if (GT(GT_HORDE) && !level.round_number && g_horde_starting_wave->integer > 0)
					level.round_number = g_horde_starting_wave->integer;
				else
					level.round_number++;
			}
			if (GT(GT_STRIKE)) {
				gi.LocBroadcast_Print(PRINT_CHAT, "Round {}: {} is attacking!\n", level.round_number, Teams_TeamName(level.strike_red_attacks ? TEAM_RED : TEAM_BLUE));
				const char *msg[2] = { "DEFEND", "CAPTURE" };
				BroadcastTeamMessage(TEAM_RED, PRINT_CENTER, G_Fmt("Round {} has begun!\n{} THE FLAG!", level.round_number, msg[level.strike_red_attacks]).data());
				BroadcastTeamMessage(TEAM_BLUE, PRINT_CENTER, G_Fmt("Round {} has begun!\n{} THE FLAG!", level.round_number, msg[!level.strike_red_attacks]).data());
			} else {
				bool horde = GT(GT_HORDE);
				gi.LocBroadcast_Print(PRINT_CHAT, "{} {} has begun!\n", horde ? "Wave" : "Round", level.round_number);
				gi.LocBroadcast_Print(PRINT_CENTER, horde ? (brandom() ? "INCOMING!" : "LOCK AND LOAD!") : "FIGHT!");
				AnnouncerSound(world, "fight", nullptr, false);

				if (horde) {
					level.horde_num_monsters_to_spawn = clamp(15 + (level.round_number * 5), 20, 80);
					level.horde_monster_spawn_time = level.time + 500_ms;	// random_time(1_sec, 2_sec);
				}
			}
		}
		return;
	}

	// end round
	if (level.round_state == roundst_t::ROUND_IN_PROGRESS) {
		auto is_living_round_player = [](gentity_t *ent) {
			return ent->client && ClientIsPlaying(ent->client) &&
				!ent->client->eliminated && ent->health > 0;
		};

		switch (g_gametype->integer) {
		case GT_CA:
		case GT_STRIKE:
		{
			int count_living_red = 0, count_living_blue = 0;

			for (auto ec : active_clients()) {
				switch (ec->client->sess.team) {
				case TEAM_RED:
					if (is_living_round_player(ec))
						count_living_red++;
					break;
				case TEAM_BLUE:
					if (is_living_round_player(ec))
						count_living_blue++;
					break;
				}
			}

			// check eliminated first
			if (!count_living_red && count_living_blue) {
				int points = GT(GT_STRIKE) ? (level.strike_red_attacks ? 0 : 2) : 1;
				G_AdjustTeamScore(TEAM_BLUE, points);
				if (GT(GT_STRIKE))
					gi.LocBroadcast_Print(PRINT_CENTER, "Turn has ended.\n{} successfully {}!\n", Teams_TeamName(TEAM_BLUE), points ? "attacked" : "defended");
				else {
					gi.LocBroadcast_Print(PRINT_CENTER, "{} wins the round!\n(eliminated {})\n", Teams_TeamName(TEAM_BLUE), Teams_TeamName(TEAM_RED));
					AnnouncerSound(world, "blue_wins_round", "ctf/flagcap.wav", true);
				}
				//gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex("ctf/flagcap.wav"), 1, ATTN_NONE, 0);
				Round_End();
				return;
			}
			if (!count_living_blue && count_living_red) {
				int points = GT(GT_STRIKE) ? (!level.strike_red_attacks ? 0 : 2) : 1;
				G_AdjustTeamScore(TEAM_RED, points);
				if (GT(GT_STRIKE)) {
					gi.LocBroadcast_Print(PRINT_CENTER, "Turn has ended.\n{} successfully {}!\n", Teams_TeamName(TEAM_RED), points ? "attacked" : "defended");
				} else {
					gi.LocBroadcast_Print(PRINT_CENTER, "{} wins the round!\n(eliminated {})\n", Teams_TeamName(TEAM_RED), Teams_TeamName(TEAM_BLUE));
					AnnouncerSound(world, "red_wins_round", "ctf/flagcap.wav", false);
				}
				//gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex("ctf/flagcap.wav"), 1, ATTN_NONE, 0);
				Round_End();
				return;
			}
			break;
		}
		case GT_HORDE:
			Horde_RunSpawning();
			//if (level.horde_all_spawned && Horde_AllMonstersDead()) {
			if (level.horde_all_spawned && !(level.total_monsters - level.killed_monsters)) {
				gi.LocBroadcast_Print(PRINT_CENTER, "Monsters eliminated!\n");
				gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex("ctf/flagcap.wav"), 1, ATTN_NONE, 0);
				Round_End();
				return;
			}
			break;

		case GT_RR:
			if (!level.num_playing_red || !level.num_playing_blue) {
				gclient_t *cl = &game.clients[level.sorted_clients[0]];

				gi.Broadcast_Print(PRINT_CENTER, "Round Ends!\n");

				gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex("ctf/flagcap.wav"), 1, ATTN_NONE, 0);

				if (level.round_number + 1 >= roundlimit->integer) {
					QueueIntermission("MATCH ENDED", false, false);
				} else
					Round_End();
				return;
			}
			break;

		}

		// hit the round time limit, check any other winning conditions
		if (level.time >= level.round_state_timer) {
			// highest number of players remaining or highest total health wins
			if (GT(GT_CA)) {
				int living_red = 0, living_blue = 0;

				for (auto ec : active_clients()) {
					if (!is_living_round_player(ec))
						continue;

					switch (ec->client->sess.team) {
					case TEAM_RED:
						living_red++;
						break;
					case TEAM_BLUE:
						living_blue++;
						break;
					}
				}

				if (living_red > living_blue) {
					G_AdjustTeamScore(TEAM_RED, 1);
					gi.LocBroadcast_Print(PRINT_CENTER, "{} wins the round!\n(players remaining: {} vs {})\n", Teams_TeamName(TEAM_RED), living_red, living_blue);
					//gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex("ctf/flagcap.wav"), 1, ATTN_NONE, 0);
					AnnouncerSound(world, "red_wins_round", "ctf/flagcap.wav", false);
				} else if (living_blue > living_red) {
					G_AdjustTeamScore(TEAM_BLUE, 1);
					gi.LocBroadcast_Print(PRINT_CENTER, "{} wins the round!\n(players remaining: {} vs {})\n", Teams_TeamName(TEAM_BLUE), living_blue, living_red);
					//gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex("ctf/flagcap.wav"), 1, ATTN_NONE, 0);
					AnnouncerSound(world, "blue_wins_round", "ctf/flagcap.wav", true);
				} else {
					int total_health_red = 0, total_health_blue = 0;

					for (auto ec : active_players()) {
						if (!is_living_round_player(ec))
							continue;
						switch (ec->client->sess.team) {
						case TEAM_RED:
							total_health_red += ec->health;
							break;
						case TEAM_BLUE:
							total_health_blue += ec->health;
							break;
						}
					}

					if (total_health_red > total_health_blue) {
						G_AdjustTeamScore(TEAM_RED, 1);
						gi.LocBroadcast_Print(PRINT_CENTER, "{} wins the round!\n(total health: {} vs {})\n", Teams_TeamName(TEAM_RED), total_health_red, total_health_blue);
						//gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex("ctf/flagcap.wav"), 1, ATTN_NONE, 0);
						AnnouncerSound(world, "red_wins_round", "ctf/flagcap.wav", false);
					} else if (total_health_blue > total_health_red) {
						G_AdjustTeamScore(TEAM_BLUE, 1);
						gi.LocBroadcast_Print(PRINT_CENTER, "{} wins the round!\n(total health: {} vs {})\n", Teams_TeamName(TEAM_BLUE), total_health_blue, total_health_red);
						//gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex("ctf/flagcap.wav"), 1, ATTN_NONE, 0);
						AnnouncerSound(world, "blue_wins_round", "ctf/flagcap.wav", true);
					} else {
						gi.LocBroadcast_Print(PRINT_CENTER, "Round draw!");
					}
				}
			} else {
				if (GT(GT_STRIKE)) {
					if (level.strike_flag_touch)
						gi.LocBroadcast_Print(PRINT_CENTER, "Turn has ended.\n{} scored a point!\n", Teams_TeamName(level.strike_red_attacks ? TEAM_RED : TEAM_BLUE));
					else
						gi.LocBroadcast_Print(PRINT_CENTER, "Turn has ended.\n{} successfully defended!", Teams_TeamName(!level.strike_red_attacks ? TEAM_RED : TEAM_BLUE));
				}
			}
			//gi.LocBroadcast_Print(PRINT_CENTER, "{} wins the round!\n", Teams_TeamName(TEAM_BLUE));
			Round_End();
			return;
		}
	}
}

/*
=============
CheckDMCountdown
=============
*/
static void CheckDMCountdown(void) {
	if ((level.match_state != matchst_t::MATCH_COUNTDOWN && level.round_state != roundst_t::ROUND_COUNTDOWN) || level.intermission_time) {
		if (level.countdown_check)
			level.countdown_check = 0_sec;
		return;
	}

	gtime_t base = (level.round_state == roundst_t::ROUND_COUNTDOWN) ? level.round_state_timer : level.match_state_timer;
	int t = (base + 1_sec - level.time).seconds<int>();

	if (!level.countdown_check || level.countdown_check.seconds<int>() > t) {
		if (!(t % 10) || t < 10) {
			AnnouncerSound(world, nullptr, G_Fmt("world/{}{}.wav", t, t >= 20 ? "sec" : "").data(), false);
			//gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex(G_Fmt("world/{}{}.wav", t, t >= 20 ? "sec" : "").data()), 1, ATTN_NONE, 0);
			if (t <= 3) {
				const char *s[3] = { "one", "two", "three" };
				AnnouncerSound(world, G_Fmt("{}", s[t-1]).data(), nullptr, false);
			}
		}
		level.countdown_check = gtime_t::from_sec(t);
	}
}

/*
=============
CheckDMMatchEndWarning
=============
*/
static void CheckDMMatchEndWarning(void) {
	if (GTF(GTF_ROUNDS))
		return;

	if (level.match_state != matchst_t::MATCH_IN_PROGRESS || !timelimit->value) {
		if (level.matchendwarn_check)
			level.matchendwarn_check = 0_sec;
		return;
	}

	int t = (level.match_time + gtime_t::from_min(timelimit->value) - level.time).seconds<int>();	// +1;

	if (!level.matchendwarn_check || level.matchendwarn_check.seconds<int>() > t) {
		if (t && (t == 30 || t == 20 || t <= 10)) {
			AnnouncerSound(world, nullptr, G_Fmt("world/{}{}.wav", t, t >= 20 ? "sec" : "").data(), false);
			//gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex(G_Fmt("world/{}{}.wav", t, t >= 20 ? "sec" : "").data()), 1, ATTN_NONE, 0);
			if (t >= 10)
				gi.LocBroadcast_Print(PRINT_HIGH, "{} second warning!\n", t);
		} else if (t == 300 || t == 60) {
			AnnouncerSound(world, G_Fmt("{}_minute", t == 300 ? 5 : 1).data(), nullptr, false);
		}
		level.matchendwarn_check = gtime_t::from_sec(t);
	}
}

/*
=============
CheckDMWarmupState

Once a frame, check for changes in match state during warmup
=============
*/
static void CheckDMWarmupState(void) {
	uint8_t min_players;

	if (!level.num_playing_clients) {
		if (level.match_state != matchst_t::MATCH_NONE) {
			level.match_state = matchst_t::MATCH_NONE;
			level.match_state_timer = 0_sec;
			level.warmup_requisite = warmupreq_t::WARMUP_REQ_NONE;
			level.warmup_notice_time = 0_sec;
			return;
		}
		// pull in any spectating bots
		for (auto ec : active_clients())
			if (!ClientIsPlaying(ec->client) && (ec->client->sess.is_a_bot || ec->svflags & SVF_BOT))
				SetTeam(ec, PickTeam(-1), false, false, false);
		return;
	}

	// duel: pull in a queued spectator if needed
	if (Duel_AddPlayer())
		return;

	// duel: automatically queue spectating bots (they'll be pulled in when needed)
	// Queue bots regardless of match state - Duel_AddPlayer will only pull them during warmup
	if (GT(GT_DUEL) && !level.intermission_time && !level.intermission_queued) {
		for (auto ec : active_clients()) {
			if (ClientIsPlaying(ec->client))
				continue;
			if (!(ec->client->sess.is_a_bot || ec->svflags & SVF_BOT))
				continue;
			if (ec->client->sess.duel_queued)
				continue; // already queued
			// queue this bot so it's ready for future matches
			SetTeam(ec, TEAM_SPECTATOR, false, true, false);
		}
	}

	min_players = GT(GT_DUEL) ? 2 : minplayers->integer;
	if (level.match_state < matchst_t::MATCH_COUNTDOWN && !g_dm_do_warmup->integer && level.num_playing_clients >= min_players) {
		Match_Start();
		return;
	}

	// check because we run 3 game frames before calling Connect and/or ClientBegin
	// for clients on a map_restart
	if (level.match_state == matchst_t::MATCH_NONE) {
		level.match_state = matchst_t::MATCH_WARMUP_DELAYED;
		level.match_state_timer = level.time + 5_sec;
		level.warmup_requisite = warmupreq_t::WARMUP_REQ_NONE;
		level.warmup_notice_time = level.time;
		return;
	}

	if (level.match_state == matchst_t::MATCH_WARMUP_DELAYED && level.match_state_timer > level.time)
		return;

	if (level.match_state == matchst_t::MATCH_WARMUP_DEFAULT || level.match_state == matchst_t::MATCH_WARMUP_READYUP)
		Horde_RunSpawning();

	bool not_enough = false;
	bool teams_imba = false;

	if (Teams()) {
		if (g_teamplay_force_balance->integer && abs(level.num_playing_red - level.num_playing_blue) > 1) {
			teams_imba = true;
		} else if (level.num_playing_red < 1 || level.num_playing_blue < 1 || level.num_playing_clients < min_players) {
			not_enough = true;
		}
	} else if (GT(GT_DUEL)) {
		if (level.num_playing_clients != 2)
			not_enough = true;
	} else if (level.num_playing_clients < min_players) {
		not_enough = true;
	}

	if (notGT(GT_DUEL)) {
		// pull in any spectating bots
		for (auto ec : active_clients())
			if (!ClientIsPlaying(ec->client) && (ec->client->sess.is_a_bot || ec->svflags & SVF_BOT))
				SetTeam(ec, PickTeam(-1), false, false, false);
	}

	if (!g_dm_allow_no_humans->integer && !level.num_playing_human_clients)
		not_enough = true;

	if (teams_imba) {
		// Cancel immediately for team imbalance
		level.match_cancel_delay_timer = 0_ms;
		if (level.match_state <= matchst_t::MATCH_COUNTDOWN) {
			if (level.match_state == matchst_t::MATCH_WARMUP_READYUP)
				UnReadyAll();
			else if (level.match_state == matchst_t::MATCH_COUNTDOWN) {
				gi.LocBroadcast_Print(PRINT_CENTER, "Countdown cancelled: teams are imbalanced\n");
				// clear locks set by g_match_lock at countdown start
				level.locked[TEAM_RED] = level.locked[TEAM_BLUE] = level.locked[TEAM_FREE] = false;
			}

			if (level.match_state != matchst_t::MATCH_WARMUP_DEFAULT) {
				level.match_state = matchst_t::MATCH_WARMUP_DEFAULT;
				level.match_state_timer = 0_sec;
				level.warmup_requisite = warmupreq_t::WARMUP_REQ_BALANCE;
				level.warmup_notice_time = level.time;
			}
		}
		return; // still waiting for players
	}

	if (not_enough) {
		// Add 300ms delay before cancelling to allow bot_minclients to add bots
		if (level.match_cancel_delay_timer == 0_ms) {
			level.match_cancel_delay_timer = level.time + 300_ms;
		}

		// Only cancel after delay has passed
		if (level.time >= level.match_cancel_delay_timer) {
			if (level.match_state <= matchst_t::MATCH_COUNTDOWN) {
				if (level.match_state == matchst_t::MATCH_WARMUP_READYUP)
					UnReadyAll();
				else if (level.match_state == matchst_t::MATCH_COUNTDOWN) {
					gi.LocBroadcast_Print(PRINT_CENTER, "Countdown cancelled: not enough players\n");
					// clear locks set by g_match_lock at countdown start
					level.locked[TEAM_RED] = level.locked[TEAM_BLUE] = level.locked[TEAM_FREE] = false;
				}

				if (level.match_state != matchst_t::MATCH_WARMUP_DEFAULT) {
					level.match_state = matchst_t::MATCH_WARMUP_DEFAULT;
					level.match_state_timer = 0_sec;
					level.warmup_requisite = warmupreq_t::WARMUP_REQ_MORE_PLAYERS;
					level.warmup_notice_time = level.time;
				}
			}
			level.match_cancel_delay_timer = 0_ms; // reset
			return; // still waiting for players
		} else {
			// Still waiting for delay, don't cancel yet
			return;
		}
	}

	// We have enough players - clear the cancellation timer
	level.match_cancel_delay_timer = 0_ms;

	if (level.match_state == matchst_t::MATCH_WARMUP_DEFAULT) {
		if (!g_dm_do_readyup->integer)
			goto countdown;
		level.match_state = matchst_t::MATCH_WARMUP_READYUP;
		level.match_state_timer = 0_sec;
		level.warmup_requisite = warmupreq_t::WARMUP_REQ_READYUP;
		level.warmup_notice_time = level.time;

		BroadcastReadyReminderMessage();
		return;
	}

	if (level.match_state > matchst_t::MATCH_COUNTDOWN)
		return;

	// if the warmup is changed at the console, restart it
	if (g_warmup_countdown->modified_count != level.warmup_modification_count) {
		level.warmup_modification_count = g_warmup_countdown->modified_count;
		level.match_state_timer = 0_sec;
		level.match_state = matchst_t::MATCH_WARMUP_DEFAULT;
		level.warmup_requisite = warmupreq_t::WARMUP_REQ_NONE;
		level.warmup_notice_time = 0_sec;
		level.prepare_to_fight = false;
		return;
	}

	// if sufficient number of players are ready, start countdown
	if (level.match_state == matchst_t::MATCH_WARMUP_READYUP) {
		if (CheckReady()) {
countdown:
			level.match_state = matchst_t::MATCH_COUNTDOWN;
			level.warmup_requisite = warmupreq_t::WARMUP_REQ_NONE;
			level.warmup_notice_time = 0_sec;
			Monsters_KillAll();

			// lock teams on countdown if g_match_lock is enabled
			if (g_match_lock->integer) {
				if (Teams()) {
					level.locked[TEAM_RED] = true;
					level.locked[TEAM_BLUE] = true;
				} else {
					level.locked[TEAM_FREE] = true;
				}
			}

			if (g_warmup_countdown->integer > 0) {
				level.match_state_timer = level.time + gtime_t::from_sec(g_warmup_countdown->integer);

				// announce it
				if ((GT(GT_DUEL) || (level.num_playing_clients == 2 && g_match_lock->integer)) &&
						&game.clients[level.sorted_clients[0]] && &game.clients[level.sorted_clients[1]])
					gi.LocBroadcast_Print(PRINT_CENTER, "{} vs {}\nBegins in...", game.clients[level.sorted_clients[0]].resp.netname, game.clients[level.sorted_clients[1]].resp.netname);
				else
					gi.LocBroadcast_Print(PRINT_CENTER, "{}\nBegins in...", level.gametype_name);

				//gi.LocBroadcast_Print(PRINT_HIGH, "{}Match {} starting...\n", g_match_lock->integer ? "TEAMS LOCKED! " : "", level.match_id.data());
				if (!level.prepare_to_fight) {
					AnnouncerSound(world, (Teams() && level.num_playing_clients >= 4) ? "prepare_your_team" : "prepare_to_fight", nullptr, false);
					level.prepare_to_fight = true;
				}
			} else {
				level.match_state_timer = 0_ms;
				goto start;
			}
		}
		return;
	}

	// if the warmup time has counted down, start the match
	if (level.match_state == matchst_t::MATCH_COUNTDOWN && level.time.seconds() >= level.match_state_timer.seconds()) {
start:
		Match_Start();
		return;
	}
}

/*
==================
CheckVote
==================
*/
static void UpdateActiveVote() {
	if (level.time - level.vote_state.start_time < 1_sec)
		return;

	MuffModeLog("DEBUG", "UpdateActiveVote: checking vote (state=%d, caller=%p, command=%p, arg_ptr=%p)",
	           (int)level.vote_state.state, (void*)level.vote_state.caller,
	           (void*)level.vote_state.command, (void*)level.vote_state.arg.c_str());

	if (level.time - level.vote_state.start_time >= 30_sec) {
		gi.LocBroadcast_Print(PRINT_HIGH, "Vote timed out.\n");
		AnnouncerSound(world, "vote_failed", nullptr, false);
		TransitionVoteState(VoteState::FAILED);
		return;
	}

	// Recount votes from client state each frame
	level.vote_state.yes_votes = 0;
	level.vote_state.no_votes = 0;
	level.vote_state.num_eligible = 0;
	for (auto ec : active_clients()) {
		if (ec->client->sess.is_a_bot)
			continue;
		if (!ClientCanVote(ec->client))
			continue;
		level.vote_state.num_eligible++;
		if (ec->client->pers.voted == 1)
			level.vote_state.yes_votes++;
		else if (ec->client->pers.voted == -1)
			level.vote_state.no_votes++;
	}

	int halfpoint = level.vote_state.num_eligible / 2;

	// Guard: if no eligible voters found, don't instantly pass/fail - wait for timeout
	// This prevents 0 >= 0 from triggering an instant fail
	if (level.vote_state.num_eligible == 0)
		return;

	if (level.vote_state.yes_votes > halfpoint) {
		gi.LocBroadcast_Print(PRINT_HIGH, "Vote passed.\n");
		AnnouncerSound(world, "vote_passed", nullptr, false);
		TransitionVoteState(VoteState::PASSED);
	} else if (level.vote_state.no_votes >= halfpoint) {
		gi.LocBroadcast_Print(PRINT_HIGH, "Vote failed.\n");
		AnnouncerSound(world, "vote_failed", nullptr, false);
		TransitionVoteState(VoteState::FAILED);
	}
}

static void CheckVote(void) {
	if (!deathmatch->integer)
		return;

	if (level.vote_state.state != VoteState::IDLE)
		MuffModeLog("DEBUG", "CheckVote: state=%d, caller=%p, command=%p",
		           (int)level.vote_state.state, (void*)level.vote_state.caller, (void*)level.vote_state.command);

	switch (level.vote_state.state) {
		case VoteState::IDLE:
			return;

		case VoteState::ACTIVE:
			if (!level.vote_state.command || !level.vote_state.caller) {
				gi.LocBroadcast_Print(PRINT_HIGH, "Vote cancelled: invalid state.\n");
				TransitionVoteState(VoteState::FAILED);
				return;
			}
			UpdateActiveVote();
			break;

		case VoteState::PASSED:
			if (level.time >= level.vote_state.execute_time)
				TransitionVoteState(VoteState::EXECUTING);
			break;

		case VoteState::EXECUTING:
			Vote_Passed();
			break;

		case VoteState::FAILED:
		case VoteState::COMPLETE:
			TransitionVoteState(VoteState::IDLE);
			break;
	}
}

// ----------------

/*
==================
FindIntermissionPoint

This is also used for spectator spawns
==================
*/
void FindIntermissionPoint(void) {
	gentity_t *ent, *target;
	vec3_t	dir;
	bool	is_landmark = false;

	if (level.intermission_spot) // search only once
		return;

	gi.Com_Print("FindIntermissionPoint\n");

	// find the intermission spot
	ent = level.spawn_spots[SPAWN_SPOT_INTERMISSION];

	if (!ent) { // the map creator forgot to put in an intermission point...
		SelectSpawnPoint(NULL, level.intermission_origin, level.intermission_angle, false, is_landmark);
	} else {
		level.intermission_origin = ent->s.origin;

		// ugly hax!
		if (!Q_strncasecmp(level.mapname, "campgrounds", 11)) {
			gvec3_t v = { -320, -96, 503 };
			if (ent->s.origin == v)
				level.intermission_angle[PITCH] = -30;
		} else if (!Q_strncasecmp(level.mapname, "rdm10", 5)) {
			gvec3_t v = { -1256, -1672, -136 };
			if (ent->s.origin == v)
				level.intermission_angle = { 15, 135, 0 };
		} else {
			level.intermission_angle = ent->s.angles;
		}

		// if it has a target, look towards it
		if (ent->target) {
			gi.Com_Print("FindIntermissionPoint target\n");
			target = G_PickTarget(ent->target);
			if (target) {
				gi.Com_Print("FindIntermissionPoint target 2\n");
				dir = (target->s.origin - level.intermission_origin).normalized();
				AngleVectors(dir);
				level.intermission_angle = dir;
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
	ent = G_FindByString<&gentity_t::classname>(nullptr, "info_player_intermission");
	if (!ent) { // the map creator forgot to put in an intermission point...
		ent = G_FindByString<&gentity_t::classname>(nullptr, "info_player_start");
		if (!ent)
			ent = G_FindByString<&gentity_t::classname>(nullptr, "info_player_deathmatch");
	} else { // choose one of four spots
		int32_t i = irandom(4);
		while (i--) {
			ent = G_FindByString<&gentity_t::classname>(ent, "info_player_intermission");
			if (!ent) // wrap around the list
				ent = G_FindByString<&gentity_t::classname>(ent, "info_player_intermission");
		}
	}

	if (ent) {
		level.intermission_origin = ent->s.origin;
		level.spawn_spots[SPAWN_SPOT_INTERMISSION] = ent;
	}
	
	// ugly hax!
	if (!Q_strncasecmp(level.mapname, "campgrounds", 11)) {
		gvec3_t v = { -320, -96, 503 };
		if (ent->s.origin == v)
			level.intermission_angle[PITCH] = -30;
	} else if (!Q_strncasecmp(level.mapname, "rdm10", 5)) {
		gvec3_t v = { -1256, -1672, -136 };
		if (ent->s.origin == v)
			level.intermission_angle = { 15, 135, 0 };
	} else {
		// if it has a target, look towards it
		if (ent && ent->target) {
			gentity_t *target = G_PickTarget(ent->target);

			if (target) {
				//gi.Com_Print("HAS TARGET\n");
				vec3_t	dir = (target->s.origin - level.intermission_origin).normalized();
				AngleVectors(dir);
				level.intermission_angle = dir;
			}
		}
		if (ent && !level.intermission_angle)
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
static bool ScoreIsTied(void) {
	if (level.num_playing_clients < 2)
		return false;

	if (Teams() && notGT(GT_RR))
		return level.team_scores[TEAM_RED] == level.team_scores[TEAM_BLUE];

	return game.clients[level.sorted_clients[0]].resp.score == game.clients[level.sorted_clients[1]].resp.score;
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
	level.num_playing_red = 0;
	level.num_playing_blue = 0;
	level.num_voting_clients = 0;

	//memset(level.sorted_clients, -1, sizeof(level.sorted_clients));
	for (size_t i = 0; i < MAX_CLIENTS; i++)
		level.sorted_clients[i] = -1;

	for (auto ec : active_clients()) {
		cl = ec->client;

		level.sorted_clients[level.num_connected_clients] = ec->client - game.clients;
		level.num_connected_clients++;

		if (!ClientIsPlaying(cl)) {
			if (ClientCanVote(cl))
				level.num_voting_clients++;
			continue;
		}

		level.num_nonspectator_clients++;

		// decide if this should be auto-followed
		level.num_playing_clients++;
		if (!cl->sess.is_a_bot) {
			level.num_playing_human_clients++;
			level.num_voting_clients++;
		}
		if (level.follow1 == -1)
			level.follow1 = ec->client - game.clients;
		else if (level.follow2 == -1)
			level.follow2 = ec->client - game.clients;

		if (teams) {
			if (cl->sess.team == TEAM_RED) {
				level.num_playing_red++;
				if (cl->pers.health > 0)
					level.num_living_red++;
				else if (cl->eliminated)
					level.num_eliminated_red++;
			} else {
				level.num_playing_blue++;
				if (cl->pers.health > 0)
					level.num_living_blue++;
				else if (cl->eliminated)
					level.num_eliminated_blue++;
			}
		}
	}

	for (size_t i = 0; i < level.num_playing_clients; i++) {
		if (game.clients[i].pers.connected) {
			game.clients[level.sorted_clients[i]].resp.old_rank = game.clients[level.sorted_clients[i]].resp.rank;
		}
	}

	qsort(level.sorted_clients, level.num_connected_clients, sizeof(level.sorted_clients[0]), SortRanks);

	if (level.sorted_clients[0] >= 0) {
		// set the rank value for all clients that are connected and not spectators
		if (teams && notGT(GT_RR)) {
			// in team games, rank is just the order of the teams, 0=red, 1=blue, 2=tied
			for (size_t i = 0; i < level.num_connected_clients; i++) {
				cl = &game.clients[level.sorted_clients[i]];
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
			int score = 0, new_score, rank;

			for (size_t i = 0; i < level.num_playing_clients; i++) {
				if (game.clients[i].pers.connected) {
					cl = &game.clients[level.sorted_clients[i]];
					cl->resp.old_score = cl->resp.score;
					new_score = cl->resp.score;
					if (i == 0 || new_score != score) {
						rank = i;
						// assume we aren't tied until the next client is checked
						game.clients[level.sorted_clients[i]].resp.rank = rank;
					}
					else {
						// we are tied with the previous client
						game.clients[level.sorted_clients[i - 1]].resp.rank = rank | RANK_TIED_FLAG;
						game.clients[level.sorted_clients[i]].resp.rank = rank | RANK_TIED_FLAG;
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
	
	level.warmup_notice_time = level.time;

	if (level.match_state == MATCH_IN_PROGRESS) {
		if (GTF(GTF_FRAGS)) {
			//gi.Com_PrintFmt("new={} old={}\n", game.clients[level.sorted_clients[0]].resp.score, old_first_score);
			if (fraglimit->integer > 3) {
				int score_diff = fraglimit->integer - game.clients[level.sorted_clients[0]].resp.score;
				if (score_diff <= 3 && !level.frag_warning[score_diff - 1]) {
					AnnouncerSound(world, G_Fmt("{}_frag{}", score_diff, score_diff > 1 ? "s" : "").data(), nullptr, false);
					level.frag_warning[score_diff - 1] = true;
					CheckDMExitRules();
					return;
				}
			}
		}
		if (!Teams() && game.clients[level.sorted_clients[0]].resp.score > 0) {
			// check changes in rank to trigger sounds
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

				if (new_rank == 0 && old_tied != new_tied) {
					AnnouncerSound(ec, new_tied ? "lead_tied" : "lead_taken", nullptr, false);

					// find and update all spectators who want to follow leader
					for (auto ec2 : active_clients()) {
						if ((!ClientIsPlaying(ec2->client) || ec2->client->eliminated) && ec2->client->sess.pc.follow_leader && ec2->client->follow_target != ec) {
							ec2->client->follow_queued_target = ec;
							ec2->client->follow_queued_time = level.time;
						}
					}
				} else if (new_rank != 0 && old_rank == 0) {
					AnnouncerSound(ec, "lead_lost", nullptr, false);
				}
			}
		} else if (Teams() && GTF(GTF_FRAGS)) {
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
				AnnouncerSound(world, new_rank ? "blue_leads" : "red_leads", nullptr, false);
			} else if (old_rank != 2 && new_rank == 2) {
				//teams just tied
				AnnouncerSound(world, "teams_tied", nullptr, false);
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

//===================================================================
// MAP QUEUE SYSTEM
//===================================================================

static void MQ_Clear() {
	if (!deathmatch)
		return;

	game.mapqueue.clear();
}

static bool MQ_Update() {
	if (!deathmatch)
		return false;

	if (!g_allow_mymap->integer)
		return false;

	if (!g_map_list->string[0] && game.mapqueue.size()) {
		MQ_Clear();
		gi.Broadcast_Print(PRINT_HIGH, "Map queue has been cleared.\n");
		return false;
	}

	// Remove empty elements from the queue
	auto it = std::remove_if(game.mapqueue.begin(), game.mapqueue.end(), 
		[](const std::string& s) { return s.empty(); });
	game.mapqueue.erase(it, game.mapqueue.end());

	return true;
}

bool MQ_Add(gentity_t *ent, const char *mapname) {
	if (!deathmatch)
		return false;

	if (!g_allow_mymap->integer)
		return false;

	if (!mapname[0]) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid map name.\n");
		return false;
	}

	// Check if either pool or list exists
	if (!g_map_pool->string[0] && !g_map_list->string[0])
		return false;

	// Validate map against pool first, then list
	char *token;
	bool found = false;

	// First check g_map_pool if it exists and is non-empty
	if (g_map_pool->string[0]) {
		const char *pool = g_map_pool->string;

		while ((token = COM_Parse(&pool)) && *token) {
			if (!Q_strcasecmp(token, mapname)) {
				found = true;
				break;
			}
		}
	}

	// Fall back to g_map_list if pool didn't have it (or pool was empty)
	if (!found && g_map_list->string[0]) {
		const char *mlist = g_map_list->string;

		while ((token = COM_Parse(&mlist)) && *token) {
			if (!Q_strcasecmp(token, mapname)) {
				found = true;
				break;
			}
		}
	}

	if (!found) {
		gi.Client_Print(ent, PRINT_HIGH, "Selected map is either invalid or not in pool/list.\n");
		return false;
	}

	if (!MQ_Update())
		return false;

	// ensure map isn't already in the queue
	if (std::find(game.mapqueue.begin(), game.mapqueue.end(), mapname) != game.mapqueue.end()) {
		gi.Client_Print(ent, PRINT_HIGH, "Selected map is already in queue.\n");
		return false;
	}

	// add it!
	game.mapqueue.push_back(mapname);

	return true;
}

static void MQ_Remove_Index(gentity_t *ent, int num) {
	if (!deathmatch)
		return;

	if (!MQ_Update())
		return;

	if (num >= 0 && num < game.mapqueue.size()) {
		game.mapqueue.erase(game.mapqueue.begin() + num);
	}
}

static const char *MQ_Go_Next() {
	if (!deathmatch)
		return nullptr;

	if (!MQ_Update())
		return nullptr;

	for (size_t i = 0; i < game.mapqueue.size(); i++) {
		if (game.mapqueue[i].empty())
			continue;
		const char *s = G_Fmt("{}", game.mapqueue[i]).data();
		game.mapqueue.erase(game.mapqueue.begin() + i);
		return s;
	}
	return nullptr;
}

int MQ_Count() {
	if (!deathmatch)
		return 0;

	if (!g_allow_mymap->integer)
		return 0;

	if (!MQ_Update())
		return 0;
	//gi.Com_PrintFmt("AAAAAAAAAAAAAAA size={}\n", game.mapqueue.size());
	return game.mapqueue.size();
}

//===================================================================

static void ShutdownGame() {
	gi.Com_Print("==== ShutdownGame ====\n");

	gi.FreeTags(TAG_LEVEL);
	gi.FreeTags(TAG_GAME);
}

static void *G_GetExtension(const char *name) {
	return nullptr;
}

const shadow_light_data_t *GetShadowLightData(int32_t entity_number);

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
G_ShuffleMapList

Shuffle the map list in place, avoiding the current map at the front.
=================
*/
void G_ShuffleMapList() {
	// [MuffMode] Thin vanilla hook; implementation lives in muffmode/mm_maps.cpp.
	MM_ShuffleMapList();
}

/*
=================
Match_End

An end of match condition has been reached
=================
*/
void Match_End() {
	gentity_t *ent;

	level.match_state = matchst_t::MATCH_ENDED;
	level.match_state_timer = 0_sec;

	// see if there is a queued map to go to
	if (MQ_Count()) {
		BeginIntermission(CreateTargetChangeLevel(MQ_Go_Next()));
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
	if (level.intermission_queued || level.match_state < matchst_t::MATCH_IN_PROGRESS)
		return;

	level.tied_overtime_start = 0_sec;

	Q_strlcpy(level.intermission_victor_msg, msg, sizeof(level.intermission_victor_msg));

	//gi.LocBroadcast_Print(PRINT_CHAT, "MATCH END: {}\n", level.intermission_victor_msg[0] ? level.intermission_victor_msg : "Unknown Reason");
	gi.Com_PrintFmt("MATCH END: {}\n", level.intermission_victor_msg[0] ? level.intermission_victor_msg : "Unknown Reason");
	gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex(boo ? "insane/insane4.wav" : "world/xian1.wav"), 1, ATTN_NONE, 0);

	if (reset) {
		Match_Reset();
	} else {
		level.match_state = matchst_t::MATCH_ENDED;
		level.match_state_timer = 0_sec;
		level.match_time = level.time;
		level.intermission_queued = level.time;

		gi.configstring(CS_CDTRACK, "0");
	}
}

int GT_ScoreLimit() {
	if (GTF(GTF_ROUNDS))
		return roundlimit->integer;
	if (GT(GT_CTF))
		return capturelimit->integer;
	return fraglimit->integer;
}

const char *GT_ScoreLimitString() {
	if (GT(GT_CTF))
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

	if (!level.num_playing_clients && noplayerstime->integer && level.time > level.no_players_time + gtime_t::from_min(noplayerstime->integer)) {
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

	if (level.match_state < matchst_t::MATCH_IN_PROGRESS)
		return;
	
	if (level.time - level.match_time <= FRAME_TIME_MS)
		return;

	if (GT(GT_HORDE)) {
		if ((level.total_monsters - level.killed_monsters) >= 100) {
			gi.Broadcast_Print(PRINT_CENTER, "DEFEATED!");
			QueueIntermission("OVERRUN BY MONSTERS!", true, false);
			return;
		}
	}

	if (GTF(GTF_ROUNDS) && level.round_state != roundst_t::ROUND_ENDED)
		return;

	if (GT(GT_HORDE)) {
		if (roundlimit->integer > 0 && level.round_number >= roundlimit->integer) {
			QueueIntermission(G_Fmt("{} WINS with a final score of {}.", game.clients[level.sorted_clients[0]].resp.netname, game.clients[level.sorted_clients[0]].resp.score).data(), false, false);
			return;
		}
	}

	if (!g_dm_allow_no_humans->integer && !level.num_playing_human_clients) {
		QueueIntermission("No human players remaining.", true, false);
		return;
	}
	
	if (minplayers->integer > 0 && level.num_playing_clients < minplayers->integer) {
		QueueIntermission("Not enough players remaining.", true, false);
		return;
	}

	bool teams = Teams() && notGT(GT_RR);
	
	if (teams && g_teamplay_force_balance->integer) {
		if (abs(level.num_playing_red - level.num_playing_blue) > 1) {
			if (g_teamplay_auto_balance->integer) {
				TeamBalance(true);
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
						AnnouncerSound(world, "overtime", "world/klaxon2.wav", true);
					} else if (!level.suddendeath) {
						gi.LocBroadcast_Print(PRINT_CENTER, "Sudden Death!");
						AnnouncerSound(world, "sudden_death", "world/klaxon2.wav", true);
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
					QueueIntermission(G_Fmt("{} WINS with a final score of {}.", game.clients[level.sorted_clients[0]].resp.netname, game.clients[level.sorted_clients[0]].resp.score).data(), false, false);
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
		} else {
			if (notGT(GT_HORDE)) {
				gclient_t *cl1, *cl2;

				cl1 = &game.clients[level.sorted_clients[0]];
				cl2 = &game.clients[level.sorted_clients[1]];
				if (cl1 && cl2) {
					if (cl1->resp.score >= cl2->resp.score + mercylimit->integer) {
						QueueIntermission(G_Fmt("{} hit the mercylimit ({}).", cl1->resp.netname, mercylimit->integer).data(), true, false);
						return;
					}
				}
			}
		}
	}

	// check for sudden death
	if (ScoreIsTied())
		return;

	// no score limit in horde
	if (GT(GT_HORDE))
		return;

	// no score limit in race
	if (false) // Race mode removed
		return;
	
	int	scorelimit = GT_ScoreLimit();
	if (!scorelimit) return;

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
		for (auto ec : active_clients()) {
			if (ec->client->sess.team != TEAM_FREE)
				continue;

			if (ec->client->resp.score >= scorelimit) {
				QueueIntermission(G_Fmt("{} WINS! (hit the {} limit)", ec->client->resp.netname, GT_ScoreLimitString()).data(), false, false);
				return;
			}
		}
	}
}

static bool Match_NextMap() {
	if (level.match_state == matchst_t::MATCH_ENDED) {
		level.match_state = matchst_t::MATCH_WARMUP_DELAYED;
		level.warmup_notice_time = level.time;
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

	// if in a duel, change the wins / losses
	Duel_MatchEnd_AdjustScores();

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
		// Clear follow_target if entity is not in use OR if client pointer is invalid
		if (ec->client->follow_target && (!ec->client->follow_target->inuse || !ec->client->follow_target->client)) {
			ec->client->follow_target = nullptr;
		}
		if (ec->client->viewed && !ec->client->viewed->inuse) {
			ec->client->viewed = nullptr;
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

	if (strstr(level.changemap, "*")) {
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

	//SetIntermissionPoint();

	// move all clients to the intermission point
	for (auto ec : active_clients()) {
		MoveClientToIntermission(ec);
		if (Teams())
			AnnouncerSound(ec, level.team_scores[TEAM_RED] > level.team_scores[TEAM_BLUE] ? "red_wins" : "blue_wins", nullptr, false);
		else
			AnnouncerSound(ec, ec->client->resp.rank == 0 ? "you_win" : "you_lose", nullptr, false);
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
		struct tm *ltime;
		time_t gmtime;

		time(&gmtime);
		ltime = localtime(&gmtime);
		time(&gmtime);
		ltime = localtime(&gmtime);

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

			std::string filename = std::string(G_Fmt("{}-vs-{}-{}-{}_{:02}_{:02}-{:02}_{:02}_{:02}",
				n1.c_str(), n2.c_str(), safe_mapname.c_str(), 1900 + ltime->tm_year, ltime->tm_mon + 1, ltime->tm_mday, ltime->tm_hour, ltime->tm_min, ltime->tm_sec));
			if (filename.length() > MAX_SCREENSHOT_FILENAME)
				filename.resize(MAX_SCREENSHOT_FILENAME);
			screenshot_cmd = std::string(G_Fmt("screenshot {}\n", filename));
			gi.Com_PrintFmt("Screenshot saved: {}\n", filename.c_str());
		} else {
			gentity_t *ent = &g_entities[1];
			const char *raw_name = "player";
			if (ent && ent->inuse && ent->client) {
				gentity_t *follow = ent->client->follow_target;
				raw_name = (follow && follow->inuse && follow->client)
					? follow->client->resp.netname
					: ent->client->resp.netname;
			}
			std::string name = sanitize_name(raw_name);

			std::string filename = std::string(G_Fmt("{}-{}-{}-{}_{:02}_{:02}-{:02}_{:02}_{:02}", gt_short_name_upper[g_gametype->integer],
				name.c_str(), safe_mapname.c_str(), 1900 + ltime->tm_year, ltime->tm_mon + 1, ltime->tm_mday, ltime->tm_hour, ltime->tm_min, ltime->tm_sec));
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
		Duel_RemoveLoser();

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
	
	// Additional safety check: validate the pointer points to valid memory
	// Check if it's a reasonable address (not perfect, but helps catch obvious corruption)
	uintptr_t ptr_val = reinterpret_cast<uintptr_t>(level.changemap);
	if (ptr_val < 0x1000 || ptr_val > 0x7FFFFFFFFFFF) {
		gi.Com_ErrorFmt("Got invalid changemap pointer ({:#x}) when trying to exit level.", ptr_val);
		return;
	}
	
	// Additional safety check for invalid map names - use strlen safely
	size_t map_len = strlen(level.changemap);
	if (map_len == 0 || map_len >= MAX_QPATH) {
		gi.Com_ErrorFmt("Got invalid changemap length ({}) when trying to exit level.", map_len);
		return;
	}

	// for N64 mainly, but if we're directly changing to "victorXXX.pcx" then
	// end game
	size_t start_offset = (level.changemap[0] == '*' ? 1 : 0);

	if (GT(GT_RR) && level.num_playing_clients > 1 && (!level.num_playing_red || !level.num_playing_blue))
		TeamShuffle();

	MuffModeLog("DEBUG", "ExitLevel: issuing gamemap command for '%s'", level.changemap);
	if (map_len > (6 + start_offset) &&
		!Q_strncasecmp(level.changemap + start_offset, "victor", 6) &&
		!Q_strncasecmp(level.changemap + map_len - 4, ".pcx", 4))
		gi.AddCommandString(G_Fmt("endgame \"{}\"\n", level.changemap + start_offset).data());
	else
		gi.AddCommandString(G_Fmt("gamemap \"{}\"\n", level.changemap).data());

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
	for (ent = g_entities + 1, i = 1; i < globals.num_entities; i++, ent++) {
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

	// set min/maxplayer limits
	if (minplayers->integer < 2) gi.cvar_set("minplayers", "2");
	else if (minplayers->integer > maxclients->integer) gi.cvar_set("minplayers", maxclients->string);
	if (maxplayers->integer < 0) gi.cvar_set("maxplayers", maxclients->string);
	if (maxplayers->integer > maxclients->integer) gi.cvar_set("maxplayers", maxclients->string);
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

	// see if it is time to do a match restart
	CheckDMWarmupState();
	CheckDMRoundState();
	CheckDMCountdown();
	CheckDMMatchEndWarning();

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
	if (level.in_frame)
		MuffModeLog("ERROR", "G_RunFrame_: re-entrant call detected! (main_loop=%d)", main_loop);
	level.in_frame = true;

	if (level.timeout_in_place > 0_ms && level.timeout_ent) {
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
		return;
	} else {
		// track gametype changes and update accordingly
		GT_Changes();

		// cancel vote if timed out
		CheckVote();

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
	for (size_t i = 0; i < globals.num_entities; i++, ent++) {
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
			ClientBeginServerFrame(ent);
			continue;
		}

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
	if (level.entry && !level.intermission_time && g_entities[1].inuse && g_entities[1].client->pers.connected)
		level.entry->time += FRAME_TIME_S;

	// [Paril-KEX] run monster pains now
	for (size_t i = 0; i < globals.num_entities + 1 + game.maxclients + BODY_QUEUE_SIZE; i++) {
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
	if (main_loop && !G_AnyClientsSpawned())
		return;

	for (size_t i = 0; i < g_frames_per_frame->integer; i++)
		G_RunFrame_(main_loop);

	// match details.. only bother if there's at least 1 player in-game
	// and not already end of game
	if (G_AnyClientsSpawned() && !level.intermission_time) {
		constexpr gtime_t report_time = 45_sec;

		if (level.time - level.next_match_report > report_time) {
			level.next_match_report = level.time + report_time;
			G_ReportMatchDetails(false);
		}
	}
}

/*
================
G_PrepFrame

This has to be done before the world logic, because
player processing happens outside RunFrame
================
*/
void G_PrepFrame() {
	for (size_t i = 0; i < globals.num_entities; i++)
		g_entities[i].s.event = EV_NONE;

	for (auto player : active_clients())
		player->client->ps.stats[STAT_HIT_MARKER] = 0;

	globals.server_flags &= ~SERVER_FLAG_INTERMISSION;

	if (level.intermission_time) {
		globals.server_flags |= SERVER_FLAG_INTERMISSION;
	}
}
