// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Keep these types independent of g_local.h. They are embedded in
// client_persistant_t and level_locals_t, so g_local.h must be able to include
// this header before either structure is declared.

enum class mm_match_weapon_t : uint8_t {
	none,
	grappling_hook,
	blaster,
	chainfist,
	shotgun,
	super_shotgun,
	machinegun,
	etf_rifle,
	chaingun,
	hand_grenades,
	trap,
	tesla_mine,
	grenade_launcher,
	prox_launcher,
	rocket_launcher,
	hyperblaster,
	ion_ripper,
	plasma_gun,
	plasma_beam,
	thunderbolt,
	railgun,
	phalanx,
	bfg10k,
	disruptor,
	total
};

enum class mm_match_medal_t : uint8_t {
	none,
	excellent,
	humiliation,
	impressive,
	rampage,
	first_frag,
	defence,
	assist,
	capture,
	holy_shit,
	total
};

enum class mm_match_high_value_item_t : uint8_t {
	none,
	mega_health,
	body_armor,
	combat_armor,
	power_shield,
	power_screen,
	adrenaline,
	quad_damage,
	double_damage,
	invisibility,
	haste,
	regeneration,
	battle_suit,
	empathy_shield,
	ammo_pack,
	bandolier,
	total
};

constexpr size_t MM_MATCH_WEAPON_COUNT =
	static_cast<size_t>(mm_match_weapon_t::total);
constexpr size_t MM_MATCH_MEDAL_COUNT =
	static_cast<size_t>(mm_match_medal_t::total);
constexpr size_t MM_MATCH_HIGH_VALUE_ITEM_COUNT =
	static_cast<size_t>(mm_match_high_value_item_t::total);

// mod_id_t is an 8-bit engine enum without a terminal value. Indexing by the
// full underlying range keeps the exported taxonomy stable when new MODs are
// appended and avoids changing vanilla-facing enum declarations.
constexpr size_t MM_MATCH_MOD_COUNT = 256;
// These are hard per-match limits, not merely initial reservations. A hostile
// or pathological server session must not be able to grow either export log
// without bound.
constexpr size_t MM_MATCH_EVENT_LIMIT = 2048;
constexpr size_t MM_MATCH_DEATH_EVENT_LIMIT = 4096;
constexpr size_t MM_MATCH_DEPARTED_PLAYER_LIMIT = 1024;

struct mm_match_player_ref_t {
	std::string name;
	std::string id;
};

struct mm_match_event_t {
	int64_t time_msec = 0;
	std::string event;
};

struct mm_match_death_event_t {
	int64_t time_msec = 0;
	mm_match_player_ref_t victim;
	mm_match_player_ref_t attacker;
	uint8_t mod = 0;
};

struct mm_match_ctf_team_stats_t {
	uint64_t flag_hold_time_total_msec = 0;
	uint32_t flag_hold_time_shortest_msec = 0;
	uint32_t flag_hold_time_longest_msec = 0;
	uint32_t flag_pickups = 0;
	uint32_t flag_drops = 0;
	uint32_t flag_returns = 0;
	uint32_t captures = 0;
	uint32_t defences = 0;
	uint32_t assists = 0;
};

struct mm_match_player_stats_t {
	uint32_t life_average_msec = 0;
	uint32_t life_longest_msec = 0;
	uint64_t life_total_msec = 0;
	uint32_t completed_lives = 0;
	int64_t life_started_real_time_ms = 0;

	uint64_t total_dmg_dealt = 0;
	uint64_t total_dmg_received = 0;
	uint64_t total_shots = 0;
	uint64_t total_hits = 0;

	uint32_t total_kills = 0;
	uint32_t total_team_kills = 0;
	uint32_t total_spawn_kills = 0;
	uint32_t total_deaths = 0;
	uint32_t total_environment_deaths = 0;
	uint32_t total_spawn_deaths = 0;
	uint32_t total_suicides = 0;

	std::array<uint32_t, MM_MATCH_MOD_COUNT> mod_total_kills{};
	std::array<uint32_t, MM_MATCH_MOD_COUNT> mod_total_deaths{};
	std::array<uint64_t, MM_MATCH_MOD_COUNT> mod_total_dmg_dealt{};
	std::array<uint64_t, MM_MATCH_MOD_COUNT> mod_total_dmg_received{};
	std::array<uint64_t, MM_MATCH_WEAPON_COUNT> total_shots_per_weapon{};
	std::array<uint64_t, MM_MATCH_WEAPON_COUNT> total_hits_per_weapon{};
	std::array<uint32_t, MM_MATCH_MEDAL_COUNT> medal_count{};

	std::array<uint32_t, MM_MATCH_HIGH_VALUE_ITEM_COUNT> pickup_counts{};
	std::array<uint64_t, MM_MATCH_HIGH_VALUE_ITEM_COUNT> pickup_delay_total_msec{};

	uint32_t ctf_flag_pickups = 0;
	uint32_t ctf_flag_drops = 0;
	uint32_t ctf_flag_returns = 0;
	uint32_t ctf_flag_assists = 0;
	uint32_t ctf_flag_defences = 0;
	uint32_t ctf_flag_captures = 0;
	uint64_t ctf_flag_carrier_time_total_msec = 0;
	uint32_t ctf_flag_carrier_time_shortest_msec = 0;
	uint32_t ctf_flag_carrier_time_longest_msec = 0;

	uint64_t play_time_total_msec = 0;
	int64_t play_start_real_time_ms = 0;
	int64_t play_end_real_time_ms = 0;
};

struct mm_match_overall_stats_t {
	bool collecting = false;
	bool finalized = false;
	bool team_mode = false;
	bool ctf_mode = false;
	int64_t match_start_real_time_ms = 0;
	int64_t match_end_real_time_ms = 0;

	uint32_t total_kills = 0;
	uint32_t total_deaths = 0;
	uint32_t total_suicides = 0;
	uint32_t total_team_kills = 0;
	uint32_t total_spawn_kills = 0;
	std::array<uint32_t, MM_MATCH_MOD_COUNT> mod_kills{};
	std::array<uint32_t, MM_MATCH_MOD_COUNT> mod_deaths{};
	std::array<uint32_t, MM_MATCH_MEDAL_COUNT> medal_count{};
	std::array<uint32_t, MM_MATCH_HIGH_VALUE_ITEM_COUNT> pickup_counts{};
	std::array<uint64_t, MM_MATCH_HIGH_VALUE_ITEM_COUNT> pickup_delay_total_msec{};

	// Indexed 0 = red, 1 = blue. Flag hold/pickup/drop values describe the
	// named flag; capture/defence/assist values describe the named team.
	std::array<mm_match_ctf_team_stats_t, 2> ctf{};

	std::vector<mm_match_death_event_t> death_log{};
	std::vector<mm_match_event_t> event_log{};
	bool death_log_truncated = false;
	bool event_log_truncated = false;
	bool players_truncated = false;
};

static_assert(MM_MATCH_WEAPON_COUNT == 24,
	"The public match-stat weapon schema must remain stable");
static_assert(MM_MATCH_MEDAL_COUNT == 10,
	"The public match-stat medal schema must remain stable");
static_assert(MM_MATCH_HIGH_VALUE_ITEM_COUNT == 16,
	"The public match-stat pickup schema must remain stable");
