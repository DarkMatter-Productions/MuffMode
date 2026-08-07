// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

// [MuffMode] The closed set of cvars a factory may override.
//
// This is an allowlist rather than a denylist on purpose. A factory is
// operator-authored data, and the override ledger can only restore exactly what
// it knows how to name -- so bounding the set is what makes "switching factories
// restores everything the outgoing one changed" a provable property rather than
// a hand-maintained list that drifts.
//
// It also fixes the ownership question. Three groups are deliberately absent:
//
//   * The ruleset's numbers -- weapon damage, spread, pellet counts, projectile
//     speed (mm_ruleset_weapons.cpp). A factory selects a ruleset by name.
//   * Server identity, the client slab, authentication, and the sv_/cl_/fs_/
//     net_/in_/bot_ namespaces. Those are the operator's, in server-base.cfg.
//   * Anything with structure a bare string cannot carry safely -- the map
//     rotation and the player limits. Those are first-class factory directives
//     (`maps`, `mappool`, `rotation`, `players`) with their own validation.

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace muffmode {
namespace factory {

enum class mm_factory_apply_t : uint8_t {
	Immediate,	// plain cvar, read fresh at the point of use
	MapLoad,	// captured in the world spawn profile or read once at spawn
	Latched		// CVAR_LATCH: takes effect at the next map load
};

struct mm_factory_cvar_t {
	const char			*name;
	mm_factory_apply_t	apply;
};

// Sorted by name; the sortedness is asserted below and relied on by the lookup.
inline constexpr mm_factory_cvar_t k_factory_cvars[] = {
	// Sorted by name; the sortedness is asserted below and relied on by the
	// binary-search lookup. Apply class is decided first by the registration
	// flags -- CVAR_LATCH is always Latched -- and only then by the read site.
	{ "capturelimit",                             mm_factory_apply_t::Immediate },
	{ "fraglimit",                                mm_factory_apply_t::Immediate },
	{ "g_allow_grapple",                          mm_factory_apply_t::MapLoad },
	{ "g_allow_techs",                            mm_factory_apply_t::MapLoad },
	{ "g_arena_ammo_bullets",                     mm_factory_apply_t::Immediate },
	{ "g_arena_ammo_cells",                       mm_factory_apply_t::Immediate },
	{ "g_arena_ammo_grenades",                    mm_factory_apply_t::Immediate },
	{ "g_arena_ammo_rockets",                     mm_factory_apply_t::Immediate },
	{ "g_arena_ammo_shells",                      mm_factory_apply_t::Immediate },
	{ "g_arena_ammo_slugs",                       mm_factory_apply_t::Immediate },
	{ "g_arena_armor_protect",                    mm_factory_apply_t::Immediate },
	{ "g_arena_competition",                      mm_factory_apply_t::Immediate },
	{ "g_arena_config",                           mm_factory_apply_t::Latched },
	{ "g_arena_default_type",                     mm_factory_apply_t::Immediate },
	{ "g_arena_dmg_armor",                        mm_factory_apply_t::Immediate },
	{ "g_arena_excessive",                        mm_factory_apply_t::Immediate },
	{ "g_arena_falling_damage",                   mm_factory_apply_t::Immediate },
	{ "g_arena_fast_switch",                      mm_factory_apply_t::Immediate },
	{ "g_arena_grapple",                          mm_factory_apply_t::Immediate },
	{ "g_arena_health_protect",                   mm_factory_apply_t::Immediate },
	{ "g_arena_legacy_idmap",                     mm_factory_apply_t::Latched },
	{ "g_arena_lock",                             mm_factory_apply_t::Immediate },
	{ "g_arena_lock_count",                       mm_factory_apply_t::Immediate },
	{ "g_arena_max_players",                      mm_factory_apply_t::Immediate },
	{ "g_arena_players_per_team",                 mm_factory_apply_t::Latched },
	{ "g_arena_rocket_speed",                     mm_factory_apply_t::Immediate },
	{ "g_arena_rounds",                           mm_factory_apply_t::Immediate },
	{ "g_arena_start_armor",                      mm_factory_apply_t::MapLoad },
	{ "g_arena_start_health",                     mm_factory_apply_t::MapLoad },
	{ "g_arena_timeouts",                         mm_factory_apply_t::Immediate },
	{ "g_arena_unbalanced",                       mm_factory_apply_t::Immediate },
	{ "g_arena_vote_time",                        mm_factory_apply_t::Immediate },
	{ "g_arena_warmup_readyup",                   mm_factory_apply_t::Immediate },
	{ "g_arena_weapon_mask",                      mm_factory_apply_t::Immediate },
	{ "g_corpse_sink_time",                       mm_factory_apply_t::Immediate },
	{ "g_damage_scale",                           mm_factory_apply_t::Immediate },
	{ "g_dm_allow_no_humans",                     mm_factory_apply_t::Immediate },
	{ "g_dm_auto_join",                           mm_factory_apply_t::Immediate },
	{ "g_dm_crosshair_id",                        mm_factory_apply_t::Immediate },
	{ "g_dm_do_readyup",                          mm_factory_apply_t::Immediate },
	{ "g_dm_do_warmup",                           mm_factory_apply_t::Immediate },
	{ "g_dm_force_join",                          mm_factory_apply_t::Immediate },
	{ "g_dm_holdable_adrenaline",                 mm_factory_apply_t::Immediate },
	{ "g_dm_no_fall_damage",                      mm_factory_apply_t::Immediate },
	{ "g_dm_no_self_damage",                      mm_factory_apply_t::Immediate },
	{ "g_dm_overtime",                            mm_factory_apply_t::Immediate },
	{ "g_dm_powerup_drop",                        mm_factory_apply_t::Immediate },
	{ "g_dm_random_items",                        mm_factory_apply_t::MapLoad },
	{ "g_dm_respawn_delay_min",                   mm_factory_apply_t::Immediate },
	{ "g_dm_respawn_point_min_dist",              mm_factory_apply_t::Immediate },
	{ "g_dm_spawn_farthest",                      mm_factory_apply_t::Immediate },
	{ "g_dm_spawnpads",                           mm_factory_apply_t::MapLoad },
	{ "g_dm_timeout_length",                      mm_factory_apply_t::Immediate },
	{ "g_dm_timeout_resume_countdown",            mm_factory_apply_t::Immediate },
	{ "g_dm_weapons_stay",                        mm_factory_apply_t::Immediate },
	{ "g_frag_messages",                          mm_factory_apply_t::Immediate },
	{ "g_freezetag_arena_loadout",                mm_factory_apply_t::Immediate },
	{ "g_freezetag_auto_thaw_time",               mm_factory_apply_t::Immediate },
	{ "g_freezetag_bot_rescue",                   mm_factory_apply_t::Immediate },
	{ "g_freezetag_frozen_hazard_release_time",   mm_factory_apply_t::Immediate },
	{ "g_freezetag_frozen_knockback_scale",       mm_factory_apply_t::Immediate },
	{ "g_freezetag_frozen_shove_lift",            mm_factory_apply_t::Immediate },
	{ "g_freezetag_frozen_shove_max_speed",       mm_factory_apply_t::Immediate },
	{ "g_freezetag_frozen_slide_friction",        mm_factory_apply_t::Immediate },
	{ "g_freezetag_multi_thaw_scale",             mm_factory_apply_t::Immediate },
	{ "g_freezetag_round_reset_alive_inventory",  mm_factory_apply_t::Immediate },
	{ "g_freezetag_round_respawn_all",            mm_factory_apply_t::Immediate },
	{ "g_freezetag_thaw_radius",                  mm_factory_apply_t::Immediate },
	{ "g_freezetag_thaw_respawn_at_location",     mm_factory_apply_t::Immediate },
	{ "g_freezetag_thaw_time",                    mm_factory_apply_t::Immediate },
	{ "g_frenzy",                                 mm_factory_apply_t::Latched },
	{ "g_friendly_fire",                          mm_factory_apply_t::Immediate },
	{ "g_grapple_damage",                         mm_factory_apply_t::Immediate },
	{ "g_grapple_offhand",                        mm_factory_apply_t::Immediate },
	{ "g_horde_boss_armor_mult",                  mm_factory_apply_t::Immediate },
	{ "g_horde_boss_budget_mult",                 mm_factory_apply_t::Immediate },
	{ "g_horde_boss_damage_mult",                 mm_factory_apply_t::Immediate },
	{ "g_horde_boss_damage_per_wave",             mm_factory_apply_t::Immediate },
	{ "g_horde_boss_force",                       mm_factory_apply_t::Immediate },
	{ "g_horde_boss_health_mult",                 mm_factory_apply_t::Immediate },
	{ "g_horde_boss_health_per_wave",             mm_factory_apply_t::Immediate },
	{ "g_horde_boss_interval",                    mm_factory_apply_t::Immediate },
	{ "g_horde_boss_machinegames",                mm_factory_apply_t::Immediate },
	{ "g_horde_boss_min_wave",                    mm_factory_apply_t::Immediate },
	{ "g_horde_boss_pair_health_mult",            mm_factory_apply_t::Immediate },
	{ "g_horde_boss_pairs",                       mm_factory_apply_t::Immediate },
	{ "g_horde_boss_powerup_chance",              mm_factory_apply_t::Immediate },
	{ "g_horde_boss_repeat_window",               mm_factory_apply_t::Immediate },
	{ "g_horde_boss_scale_limit",                 mm_factory_apply_t::Immediate },
	{ "g_horde_boss_tier_window",                 mm_factory_apply_t::Immediate },
	{ "g_horde_boss_waves",                       mm_factory_apply_t::Immediate },
	{ "g_horde_champion_chance",                  mm_factory_apply_t::Immediate },
	{ "g_horde_champion_damage_mult",             mm_factory_apply_t::Immediate },
	{ "g_horde_champion_drop_chance",             mm_factory_apply_t::Immediate },
	{ "g_horde_champion_force",                   mm_factory_apply_t::Immediate },
	{ "g_horde_champion_health_floor",            mm_factory_apply_t::Immediate },
	{ "g_horde_champion_health_mult",             mm_factory_apply_t::Immediate },
	{ "g_horde_champion_health_per_wave",         mm_factory_apply_t::Immediate },
	{ "g_horde_champion_max_per_run",             mm_factory_apply_t::Immediate },
	{ "g_horde_champion_min_wave",                mm_factory_apply_t::Immediate },
	{ "g_horde_champion_speed_mult",              mm_factory_apply_t::Immediate },
	{ "g_horde_champion_strong_ratio",            mm_factory_apply_t::Immediate },
	{ "g_horde_champions",                        mm_factory_apply_t::Immediate },
	{ "g_horde_content_peak_wave",                mm_factory_apply_t::Immediate },
	{ "g_horde_drop_chance",                      mm_factory_apply_t::Immediate },
	{ "g_horde_drop_profile_bias",                mm_factory_apply_t::Immediate },
	{ "g_horde_enhanced_ai",                      mm_factory_apply_t::Immediate },
	{ "g_horde_featured_spawns",                  mm_factory_apply_t::Immediate },
	{ "g_horde_item_respawn_scale",               mm_factory_apply_t::Immediate },
	{ "g_horde_late_budget_factor",               mm_factory_apply_t::Immediate },
	{ "g_horde_late_escalation",                  mm_factory_apply_t::Immediate },
	{ "g_horde_late_max_alive_cap",               mm_factory_apply_t::Immediate },
	{ "g_horde_late_max_alive_per_wave",          mm_factory_apply_t::Immediate },
	{ "g_horde_late_wave_factor",                 mm_factory_apply_t::Immediate },
	{ "g_horde_lives",                            mm_factory_apply_t::Immediate },
	{ "g_horde_map_monster_spawns",               mm_factory_apply_t::Immediate },
	{ "g_horde_map_scale",                        mm_factory_apply_t::Immediate },
	{ "g_horde_map_scale_factor",                 mm_factory_apply_t::Immediate },
	{ "g_horde_map_scale_ref",                    mm_factory_apply_t::Immediate },
	{ "g_horde_map_spawn_chance",                 mm_factory_apply_t::Immediate },
	{ "g_horde_map_spawn_cooldown",               mm_factory_apply_t::Immediate },
	{ "g_horde_map_spawn_min_dist",               mm_factory_apply_t::Immediate },
	{ "g_horde_mark_monsters_max",                mm_factory_apply_t::Immediate },
	{ "g_horde_mark_monsters_threshold",          mm_factory_apply_t::Immediate },
	{ "g_horde_max_alive",                        mm_factory_apply_t::Immediate },
	{ "g_horde_momentum_messages",                mm_factory_apply_t::Immediate },
	{ "g_horde_monster_edge_drops",               mm_factory_apply_t::Immediate },
	{ "g_horde_player_scale",                     mm_factory_apply_t::Immediate },
	{ "g_horde_player_scale_factor",              mm_factory_apply_t::Immediate },
	{ "g_horde_player_scale_max",                 mm_factory_apply_t::Immediate },
	{ "g_horde_points_base",                      mm_factory_apply_t::Immediate },
	{ "g_horde_points_max",                       mm_factory_apply_t::Immediate },
	{ "g_horde_points_min",                       mm_factory_apply_t::Immediate },
	{ "g_horde_points_per_wave",                  mm_factory_apply_t::Immediate },
	{ "g_horde_preset_allow_boss_waves",          mm_factory_apply_t::Immediate },
	{ "g_horde_preset_chance",                    mm_factory_apply_t::Immediate },
	{ "g_horde_preset_weight_clone_army",         mm_factory_apply_t::Immediate },
	{ "g_horde_preset_weight_funhouse_horde",     mm_factory_apply_t::Immediate },
	{ "g_horde_preset_weight_get_over_here",      mm_factory_apply_t::Immediate },
	{ "g_horde_preset_weight_giant_horde",        mm_factory_apply_t::Immediate },
	{ "g_horde_preset_weight_glass_cannon",       mm_factory_apply_t::Immediate },
	{ "g_horde_preset_weight_low_gravity",        mm_factory_apply_t::Immediate },
	{ "g_horde_preset_weight_pinball_night",      mm_factory_apply_t::Immediate },
	{ "g_horde_preset_weight_sawstorm",           mm_factory_apply_t::Immediate },
	{ "g_horde_preset_weight_tiny_shamblers",     mm_factory_apply_t::Immediate },
	{ "g_horde_preset_weight_tiny_terror",        mm_factory_apply_t::Immediate },
	{ "g_horde_pursuit",                          mm_factory_apply_t::Immediate },
	{ "g_horde_pursuit_repath_time",              mm_factory_apply_t::Immediate },
	{ "g_horde_reach_probe_budget",               mm_factory_apply_t::Immediate },
	{ "g_horde_reinforcement_kills",              mm_factory_apply_t::Immediate },
	{ "g_horde_reinforcement_protection",         mm_factory_apply_t::Immediate },
	{ "g_horde_reinforcements_per_wave",          mm_factory_apply_t::Immediate },
	{ "g_horde_retarget_interval",                mm_factory_apply_t::Immediate },
	{ "g_horde_spawn_burst_count",                mm_factory_apply_t::Immediate },
	{ "g_horde_spawn_burst_rest",                 mm_factory_apply_t::Immediate },
	{ "g_horde_spawn_interval_max",               mm_factory_apply_t::Immediate },
	{ "g_horde_spawn_interval_min",               mm_factory_apply_t::Immediate },
	{ "g_horde_stall_timeout",                    mm_factory_apply_t::Immediate },
	{ "g_horde_start_chainsaw",                   mm_factory_apply_t::Immediate },
	{ "g_horde_starting_wave",                    mm_factory_apply_t::Latched },
	{ "g_horde_streak_drop_bonus",                mm_factory_apply_t::Immediate },
	{ "g_horde_streak_max_tier",                  mm_factory_apply_t::Immediate },
	{ "g_horde_streak_score_bonus",               mm_factory_apply_t::Immediate },
	{ "g_horde_streak_step",                      mm_factory_apply_t::Immediate },
	{ "g_horde_streak_upgrade_chance",            mm_factory_apply_t::Immediate },
	{ "g_horde_target_aggression",                mm_factory_apply_t::Immediate },
	{ "g_horde_target_model",                     mm_factory_apply_t::Immediate },
	{ "g_horde_target_opportunism",               mm_factory_apply_t::Immediate },
	{ "g_horde_target_spread_weight",             mm_factory_apply_t::Immediate },
	{ "g_horde_tech_count",                       mm_factory_apply_t::Immediate },
	{ "g_horde_tech_drop_on_death",               mm_factory_apply_t::Immediate },
	{ "g_horde_tech_duration",                    mm_factory_apply_t::Immediate },
	{ "g_horde_tech_relocate",                    mm_factory_apply_t::Immediate },
	{ "g_horde_tech_reset_each_wave",             mm_factory_apply_t::Immediate },
	{ "g_horde_tech_spawn_anywhere",              mm_factory_apply_t::Immediate },
	{ "g_horde_tech_unique",                      mm_factory_apply_t::Immediate },
	{ "g_horde_theme_chance",                     mm_factory_apply_t::Immediate },
	{ "g_horde_theme_min_monsters",               mm_factory_apply_t::Immediate },
	{ "g_horde_theme_min_wave",                   mm_factory_apply_t::Immediate },
	{ "g_horde_themed_waves",                     mm_factory_apply_t::Immediate },
	{ "g_horde_warmup_cap",                       mm_factory_apply_t::Immediate },
	{ "g_horde_water_max_alive",                  mm_factory_apply_t::Immediate },
	{ "g_horde_water_spawn_chance",               mm_factory_apply_t::Immediate },
	{ "g_horde_water_spawns",                     mm_factory_apply_t::Immediate },
	{ "g_horde_wave_flawless_message",            mm_factory_apply_t::Immediate },
	{ "g_horde_wave_min_types",                   mm_factory_apply_t::Immediate },
	{ "g_horde_wave_spawn_delay_ms",              mm_factory_apply_t::Immediate },
	{ "g_horde_wave_survival_bonus",              mm_factory_apply_t::Immediate },
	{ "g_horde_wave_type_ramp",                   mm_factory_apply_t::Immediate },
	{ "g_horde_wave_variety",                     mm_factory_apply_t::Immediate },
	{ "g_horde_weight_floor",                     mm_factory_apply_t::Immediate },
	{ "g_infinite_ammo",                          mm_factory_apply_t::Latched },
	{ "g_instagib",                               mm_factory_apply_t::Latched },
	{ "g_instagib_splash",                        mm_factory_apply_t::Immediate },
	{ "g_instant_weapon_switch",                  mm_factory_apply_t::Immediate },
	{ "g_knockback_scale",                        mm_factory_apply_t::Immediate },
	{ "g_lms_lives",                              mm_factory_apply_t::Immediate },
	{ "g_match_lock",                             mm_factory_apply_t::Immediate },
	{ "g_mover_speed_scale",                      mm_factory_apply_t::MapLoad },
	{ "g_nadefest",                               mm_factory_apply_t::Latched },
	{ "g_no_powerups",                            mm_factory_apply_t::MapLoad },
	{ "g_owner_auto_join",                        mm_factory_apply_t::Immediate },
	{ "g_quadhog",                                mm_factory_apply_t::Latched },
	{ "g_quick_weapon_switch",                    mm_factory_apply_t::Latched },
	{ "g_round_countdown",                        mm_factory_apply_t::Immediate },
	{ "g_start_items",                            mm_factory_apply_t::MapLoad },
	{ "g_starting_health_bonus",                  mm_factory_apply_t::Immediate },
	{ "g_teamplay_allow_team_pick",               mm_factory_apply_t::Immediate },
	{ "g_teamplay_force_balance",                 mm_factory_apply_t::Immediate },
	{ "g_teleporter_freeze",                      mm_factory_apply_t::Immediate },
	{ "g_vampiric_damage",                        mm_factory_apply_t::Immediate },
	{ "g_vampiric_exp_min",                       mm_factory_apply_t::Immediate },
	{ "g_vampiric_health_max",                    mm_factory_apply_t::Immediate },
	{ "g_vampiric_percentile",                    mm_factory_apply_t::Immediate },
	{ "g_warmup_countdown",                       mm_factory_apply_t::Immediate },
	{ "g_warmup_ready_percentage",                mm_factory_apply_t::Immediate },
	{ "g_weapon_respawn_time",                    mm_factory_apply_t::Immediate },
	{ "mercylimit",                               mm_factory_apply_t::Immediate },
	{ "roundlimit",                               mm_factory_apply_t::Immediate },
	{ "roundtimelimit",                           mm_factory_apply_t::Immediate },
	{ "timelimit",                                mm_factory_apply_t::Immediate },
};

inline constexpr size_t k_factory_cvar_count =
	sizeof(k_factory_cvars) / sizeof(k_factory_cvars[0]);

inline constexpr bool MM_FactoryCvarsSortedUnique() noexcept
{
	for (size_t i = 1; i < k_factory_cvar_count; i++) {
		const std::string_view previous(k_factory_cvars[i - 1].name);
		const std::string_view current(k_factory_cvars[i].name);
		if (!(previous < current))
			return false;
	}
	return true;
}

namespace table_detail {

inline constexpr bool HasPrefix(std::string_view name, std::string_view prefix) noexcept
{
	return name.size() >= prefix.size() && name.substr(0, prefix.size()) == prefix;
}

} // namespace table_detail

// Server identity, the client slab, authentication and every engine-owned
// namespace must be unreachable from a factory file.
inline constexpr bool MM_FactoryCvarsExcludeProtected() noexcept
{
	constexpr std::string_view protected_names[] = {
		"maxclients", "maxentities", "maxplayers", "minplayers",
		"deathmatch", "coop", "teamplay", "ctf", "g_gametype",
		"cheats", "gamename", "basedir", "skill",
		"password", "spectator_password", "needpass", "rcon_password",
		"hostname", "g_factory", "g_factory_title", "g_factory_file",
		"g_votable_factories", "g_ruleset", "g_map_list", "g_map_pool",
	};
	constexpr std::string_view protected_prefixes[] = {
		"sv_", "cl_", "fs_", "net_", "in_",
	};

	for (size_t i = 0; i < k_factory_cvar_count; i++) {
		const std::string_view name(k_factory_cvars[i].name);
		if (name.empty())
			return false;
		for (const std::string_view reserved : protected_names)
			if (name == reserved)
				return false;
		for (const std::string_view prefix : protected_prefixes)
			if (table_detail::HasPrefix(name, prefix))
				return false;
	}
	return true;
}

static_assert(MM_FactoryCvarsSortedUnique());
static_assert(MM_FactoryCvarsExcludeProtected());

// Exact, case-insensitive by way of the caller lowering first; cvar names in the
// tree are all lowercase, so the binary search compares directly.
inline constexpr const mm_factory_cvar_t *MM_FindFactoryCvar(
	std::string_view name) noexcept
{
	size_t low = 0;
	size_t high = k_factory_cvar_count;
	while (low < high) {
		const size_t middle = low + (high - low) / 2;
		const std::string_view candidate(k_factory_cvars[middle].name);
		if (candidate < name)
			low = middle + 1;
		else if (name < candidate)
			high = middle;
		else
			return &k_factory_cvars[middle];
	}
	return nullptr;
}

static_assert(MM_FindFactoryCvar("g_instagib") != nullptr);
static_assert(MM_FindFactoryCvar("timelimit") != nullptr);
static_assert(MM_FindFactoryCvar("maxclients") == nullptr);
static_assert(MM_FindFactoryCvar("g_gametype") == nullptr);
static_assert(MM_FindFactoryCvar("") == nullptr);

} // namespace factory
} // namespace muffmode
