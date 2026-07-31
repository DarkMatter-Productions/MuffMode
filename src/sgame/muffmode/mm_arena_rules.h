// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

enum class mm_arena_type_t : uint8_t {
	RocketArena,
	ClanArena,
	RedRover,
	Practice
};

enum class mm_arena_state_t : uint8_t {
	Empty,
	Warmup,
	MatchCountdown,
	RoundCountdown,
	Running,
	RoundOver,
	MatchOver,
	Paused
};

enum class mm_arena_role_t : uint8_t {
	Lobby,
	Observer,
	Queued,
	Fighter,
	Coach
};

enum class mm_arena_protection_t : uint8_t {
	None,
	SelfAndTeam,
	Team
};

enum class mm_arena_round_result_t {
	Ongoing,
	Draw,
	Red,
	Blue
};

// RA2 reserves arena zero for the lobby and stores the playable-room count on
// worldspawn. Keep this map contract independent from the game DLL so the
// fail-closed activation policy can be covered by host tests.
inline constexpr int MM_ARENA_MAP_MAX_ROOMS = 31;

inline constexpr int MM_ArenaEffectiveGametype(int requested,
	int arena_gametype, int fallback_gametype, bool map_active)
{
	return requested == arena_gametype && !map_active
		? fallback_gametype : requested;
}

inline constexpr bool MM_ArenaUsesGenericNoPlayersTimeout(bool arena_active)
{
	return !arena_active;
}

enum class mm_arena_map_error_t : uint8_t {
	None,
	MalformedEntityLump,
	MissingWorldspawn,
	MissingArenaKey,
	DuplicateArenaKey,
	InvalidArenaCount,
	MissingLobbyPoint,
	MissingFighterStarts
};

// TaggedMulti is the normal RA2 layout: every playable room has a positive
// entity key. LegacyIdmap mirrors RA2's explicit worldspawn "arena" "0"
// compatibility path, where the map supplies one shared set of DM starts.
enum class mm_arena_map_profile_t : uint8_t {
	None,
	TaggedMulti,
	LegacyIdmap
};

inline constexpr bool MM_ArenaMapRoomDeclared(
	mm_arena_map_profile_t profile, int declared_rooms, int room)
{
	return profile == mm_arena_map_profile_t::TaggedMulti && room >= 1 &&
		room <= declared_rooms;
}

struct mm_arena_map_contract_t {
	bool syntax_valid = false;
	bool first_entity_is_worldspawn = false;
	int world_arena_key_count = 0;
	bool world_arena_value_valid = false;
	int declared_rooms = 0;
	bool has_lobby_point = false;
	uint16_t idmap_fighter_starts = 0;
	std::array<uint16_t, MM_ARENA_MAP_MAX_ROOMS + 1> fighter_starts {};
	std::array<bool, MM_ARENA_MAP_MAX_ROOMS + 1> named_intermissions {};
};

struct mm_arena_map_validation_t {
	mm_arena_map_error_t error = mm_arena_map_error_t::None;
	int room = 0;
	mm_arena_map_profile_t profile = mm_arena_map_profile_t::None;

	constexpr explicit operator bool() const
	{
		return error == mm_arena_map_error_t::None;
	}
};

inline constexpr bool MM_ArenaMapUsesExplicitIdmap(
	const mm_arena_map_contract_t &contract)
{
	return contract.world_arena_key_count == 1 &&
		contract.world_arena_value_valid && contract.declared_rooms == 0;
}

inline constexpr mm_arena_map_validation_t MM_ArenaValidateMapContract(
	const mm_arena_map_contract_t &contract,
	bool allow_untagged_idmap = false)
{
	if (!contract.syntax_valid)
		return { mm_arena_map_error_t::MalformedEntityLump, 0 };
	if (!contract.first_entity_is_worldspawn)
		return { mm_arena_map_error_t::MissingWorldspawn, 0 };
	if (contract.world_arena_key_count > 1)
		return { mm_arena_map_error_t::DuplicateArenaKey, 0 };
	if (contract.world_arena_key_count == 1 &&
		!contract.world_arena_value_valid)
		return { mm_arena_map_error_t::InvalidArenaCount, 0 };

	const bool legacy_idmap = MM_ArenaMapUsesExplicitIdmap(contract) ||
		(allow_untagged_idmap && contract.world_arena_key_count == 0);
	if (legacy_idmap) {
		// RA2 treats arena zero as its single-room idmap profile and ignores
		// per-entity room tags. This retains compatibility with idmaps that
		// reused a multi-room entity set without using its room separation.
		if (contract.idmap_fighter_starts < 2)
			return { mm_arena_map_error_t::MissingFighterStarts, 1 };
		return { mm_arena_map_error_t::None, 0,
			mm_arena_map_profile_t::LegacyIdmap };
	}

	if (contract.world_arena_key_count == 0)
		return { mm_arena_map_error_t::MissingArenaKey, 0 };
	if (contract.declared_rooms < 1 ||
		contract.declared_rooms > MM_ARENA_MAP_MAX_ROOMS)
		return { mm_arena_map_error_t::InvalidArenaCount, 0 };
	if (!contract.has_lobby_point)
		return { mm_arena_map_error_t::MissingLobbyPoint, 0 };
	for (int room = 1; room <= contract.declared_rooms; room++) {
		// Round setup respawns both active sides before the countdown. A single
		// shared point would make the second spawn telefrag the first.
		if (contract.fighter_starts[room] < 2)
			return { mm_arena_map_error_t::MissingFighterStarts, room };
	}
	return { mm_arena_map_error_t::None, 0,
		mm_arena_map_profile_t::TaggedMulti };
}

inline constexpr std::string_view MM_ArenaMapErrorText(
	mm_arena_map_error_t error)
{
	switch (error) {
	case mm_arena_map_error_t::None:
		return "valid";
	case mm_arena_map_error_t::MalformedEntityLump:
		return "malformed entity lump";
	case mm_arena_map_error_t::MissingWorldspawn:
		return "first entity is not worldspawn";
	case mm_arena_map_error_t::MissingArenaKey:
		return "worldspawn has no arena key";
	case mm_arena_map_error_t::DuplicateArenaKey:
		return "worldspawn has duplicate arena keys";
	case mm_arena_map_error_t::InvalidArenaCount:
		return "worldspawn arena count must be an integer from 0 to 31 (0 is legacy idmap)";
	case mm_arena_map_error_t::MissingLobbyPoint:
		return "arena 0 has no usable lobby point";
	case mm_arena_map_error_t::MissingFighterStarts:
		return "playable room has fewer than two usable fighter starts";
	}
	return "unknown validation error";
}

// RA3's `weapons:` list uses the stock Q3 number-row bindings. Keep that
// contract as the canonical mask even though the resolved items are Q2RE
// equivalents.
enum mm_arena_weapon_flag_t : std::uint32_t {
	MM_ARENA_WEAPON_GAUNTLET = 1u << 0,
	MM_ARENA_WEAPON_MACHINEGUN = 1u << 1,
	MM_ARENA_WEAPON_SHOTGUN = 1u << 2,
	MM_ARENA_WEAPON_GRENADE_LAUNCHER = 1u << 3,
	MM_ARENA_WEAPON_ROCKET_LAUNCHER = 1u << 4,
	MM_ARENA_WEAPON_LIGHTNING = 1u << 5,
	MM_ARENA_WEAPON_RAILGUN = 1u << 6,
	MM_ARENA_WEAPON_PLASMA = 1u << 7,
	MM_ARENA_WEAPON_BFG = 1u << 8,
	MM_ARENA_WEAPON_STANDARD = (1u << 8) - 1,
	MM_ARENA_WEAPON_ALL = (1u << 9) - 1
};

enum mm_arena_vote_flag_t : std::uint32_t {
	MM_ARENA_VOTE_TYPE = 1u << 0,
	MM_ARENA_VOTE_HEALTH = 1u << 1,
	MM_ARENA_VOTE_ARMOR = 1u << 2,
	MM_ARENA_VOTE_PLAYERS_PER_TEAM = 1u << 3,
	MM_ARENA_VOTE_ROUNDS = 1u << 4,
	MM_ARENA_VOTE_ARMOR_PROTECTION = 1u << 5,
	MM_ARENA_VOTE_HEALTH_PROTECTION = 1u << 6,
	MM_ARENA_VOTE_SHOTGUN = 1u << 7,
	MM_ARENA_VOTE_SUPER_SHOTGUN = 1u << 8,
	MM_ARENA_VOTE_MACHINEGUN = 1u << 9,
	MM_ARENA_VOTE_CHAINGUN = 1u << 10,
	MM_ARENA_VOTE_GRENADE_LAUNCHER = 1u << 11,
	MM_ARENA_VOTE_ROCKET_LAUNCHER = 1u << 12,
	MM_ARENA_VOTE_HYPERBLASTER = 1u << 13,
	MM_ARENA_VOTE_RAILGUN = 1u << 14,
	MM_ARENA_VOTE_BFG = 1u << 15,
	MM_ARENA_VOTE_FALLING = 1u << 16,
	MM_ARENA_VOTE_EXCESSIVE = 1u << 17,
	MM_ARENA_VOTE_LOCK = 1u << 18,
	MM_ARENA_VOTE_COMPETITION = 1u << 19,
	MM_ARENA_VOTE_MIN_PING = 1u << 20,
	MM_ARENA_VOTE_MAX_PING = 1u << 21,
	MM_ARENA_VOTE_MAX_TEAMS = 1u << 22,
	MM_ARENA_VOTE_GRAPPLE = 1u << 23,
	MM_ARENA_VOTE_ROCKET_SPEED = 1u << 24,
	MM_ARENA_VOTE_HEALTH_ARMOR =
		MM_ARENA_VOTE_HEALTH | MM_ARENA_VOTE_ARMOR,
	MM_ARENA_VOTE_PROTECTION =
		MM_ARENA_VOTE_ARMOR_PROTECTION | MM_ARENA_VOTE_HEALTH_PROTECTION,
	MM_ARENA_VOTE_WEAPONS =
		MM_ARENA_VOTE_SHOTGUN | MM_ARENA_VOTE_SUPER_SHOTGUN |
		MM_ARENA_VOTE_MACHINEGUN | MM_ARENA_VOTE_CHAINGUN |
		MM_ARENA_VOTE_GRENADE_LAUNCHER | MM_ARENA_VOTE_ROCKET_LAUNCHER |
		MM_ARENA_VOTE_HYPERBLASTER | MM_ARENA_VOTE_RAILGUN |
		MM_ARENA_VOTE_BFG,
	MM_ARENA_VOTE_PING = MM_ARENA_VOTE_MIN_PING | MM_ARENA_VOTE_MAX_PING,
	MM_ARENA_VOTE_ALL = (1u << 25) - 1
};

inline constexpr std::uint32_t MM_ARENA_DEFAULT_VOTE_ALLOW_MASK =
	MM_ARENA_VOTE_ALL &
	~(MM_ARENA_VOTE_EXCESSIVE | MM_ARENA_VOTE_GRAPPLE);

// This structure is intentionally independent from the game DLL.  It is used
// by defaults, config layers and per-arena ballots, making rule resolution
// host-testable without pulling in g_local.h.
struct mm_arena_settings_t {
	mm_arena_type_t type = mm_arena_type_t::RocketArena;
	mm_arena_type_t pickup_type = mm_arena_type_t::ClanArena;
	int players_per_team = 1;
	int rounds = 1;
	std::uint32_t weapon_mask = MM_ARENA_WEAPON_STANDARD;
	int armor = 100;
	int health = 100;
	int shells = 100;
	int bullets = 200;
	int slugs = 50;
	int grenades = 20;
	int rockets = 50;
	int cells = 150;
	int plasma_ammo = 100;
	int bfg_ammo = 20;
	int rocket_speed = 900;
	bool fast_switch = true;
	bool grapple = false;
	bool falling_damage = true;
	bool excessive = false;
	bool damage_scoring = false;
	bool lock_arena = false;
	bool competition_mode = false;
	bool unbalanced = false;
	// RA3 `gametype: pickup` and RA2's boolean pickup switch are finalized
	// after all layers so a later defpickup or pickup:0 remains authoritative.
	bool legacy_pickup = false;
	int lock_count = 6;
	int max_players = 0;
	int min_ping = 0;
	int max_ping = 0;
	int max_teams = 0;
	int vote_tries = 2;
	mm_arena_protection_t armor_protect = mm_arena_protection_t::Team;
	mm_arena_protection_t health_protect = mm_arena_protection_t::SelfAndTeam;
	std::uint32_t vote_allow_mask = MM_ARENA_DEFAULT_VOTE_ALLOW_MASK;
};

inline mm_arena_settings_t MM_ArenaProposalBase(
	const mm_arena_settings_t &current,
	const mm_arena_settings_t &pending, bool has_pending)
{
	return has_pending ? pending : current;
}

inline constexpr int MM_ArenaNormalizePlayersPerTeam(int value, int max_clients)
{
	const int maximum = std::max(1, max_clients / 2);
	return std::clamp(value, 1, maximum);
}

struct mm_arena_page_range_t {
	int page = 0;
	int page_count = 1;
	int first = 0;
	int last = 0; // Exclusive.
};

inline constexpr mm_arena_page_range_t MM_ArenaPageRange(
	int item_count, int requested_page, int page_size)
{
	const int count = std::max(0, item_count);
	const int size = std::max(1, page_size);
	const int page_count = count > 0 ? 1 + ((count - 1) / size) : 1;
	const int page = std::clamp(requested_page, 0, page_count - 1);
	const int first = page * size;
	const int last = first + std::min(size, count - first);
	return { page, page_count, first, last };
}

inline constexpr int MM_ArenaNormalizeBestOf(int value)
{
	int normalized = std::clamp(value, 1, 99);
	if ((normalized % 2) == 0 && normalized < 99)
		normalized++;
	return normalized;
}

inline constexpr int MM_ArenaWinsNeeded(int best_of)
{
	return (MM_ArenaNormalizeBestOf(best_of) / 2) + 1;
}

inline constexpr bool MM_ArenaQueueKeyPrecedes(std::uint32_t first_order,
	std::uint16_t first_id, std::uint32_t second_order,
	std::uint16_t second_id)
{
	return first_order < second_order ||
		(first_order == second_order && first_id < second_id);
}

inline constexpr std::uint32_t MM_ArenaSanitizeWeaponMask(std::uint32_t mask)
{
	return mask & MM_ARENA_WEAPON_ALL;
}

inline constexpr std::uint32_t MM_ArenaWeaponFlagForDigit(char digit)
{
	return digit >= '1' && digit <= '9'
		? std::uint32_t { 1 } << (digit - '1') : 0;
}

// RA2's `weapons:` list is Q2's weapon row, not RA3's Q3-style number row.
// The two compact Q2RE roles intentionally combine SG/SSG and MG/Chaingun,
// while preserving the meaningful launcher, HyperBlaster, rail, and BFG
// choices from an imported RA2 config.
inline constexpr std::uint32_t MM_ArenaWeaponFlagForRa2Digit(char digit)
{
	switch (digit) {
	case '2':
	case '3':
		return MM_ARENA_WEAPON_SHOTGUN;
	case '4':
	case '5':
		return MM_ARENA_WEAPON_MACHINEGUN;
	case '6':
		return MM_ARENA_WEAPON_GRENADE_LAUNCHER;
	case '7':
		return MM_ARENA_WEAPON_ROCKET_LAUNCHER;
	case '8':
		return MM_ARENA_WEAPON_PLASMA;
	case '9':
		return MM_ARENA_WEAPON_RAILGUN;
	case '0':
		return MM_ARENA_WEAPON_BFG;
	default:
		return 0;
	}
}

enum class mm_arena_weapon_list_format_t : uint8_t {
	RA3,
	RA2
};

// A top-level-only `arena.cfg` is an RA2 convention and has no structural
// marker of its own. Keep that compatibility default; native files can make
// their format explicit, while `map`/`room` scopes identify themselves.
inline constexpr mm_arena_weapon_list_format_t
MM_ArenaResolveConfigWeaponListFormat(bool has_legacy_syntax,
	bool has_native_syntax, std::string_view explicit_format)
{
	if (explicit_format == "ra2")
		return mm_arena_weapon_list_format_t::RA2;
	if (explicit_format == "ra3" || explicit_format == "native" ||
		explicit_format == "muffmode")
		return mm_arena_weapon_list_format_t::RA3;
	if (has_legacy_syntax)
		return mm_arena_weapon_list_format_t::RA2;
	if (has_native_syntax)
		return mm_arena_weapon_list_format_t::RA3;
	return mm_arena_weapon_list_format_t::RA2;
}

struct mm_arena_weapon_list_t {
	std::uint32_t mask = 0;
	bool grapple = false;
	bool valid = false;
};

inline constexpr mm_arena_weapon_list_t MM_ArenaParseWeaponList(
	std::string_view value, mm_arena_weapon_list_format_t format)
{
	mm_arena_weapon_list_t result;
	if (format == mm_arena_weapon_list_format_t::RA2) {
		// RA2 tokenizes its row on whitespace, so `100` is not three weapon
		// selectors. This matters when accepting real-world legacy configs that
		// omitted a semicolon before the following `armor: 100` setting.
		for (size_t begin = 0; begin < value.size();) {
			while (begin < value.size() &&
				(value[begin] == ' ' || value[begin] == '\t' ||
					value[begin] == '\r' || value[begin] == '\n'))
				begin++;
			const size_t end = value.find_first_of(" \t\r\n", begin);
			const size_t length = (end == std::string_view::npos
				? value.size() : end) - begin;
			if (length == 1) {
				const std::uint32_t flag =
					MM_ArenaWeaponFlagForRa2Digit(value[begin]);
				if (flag) {
					result.mask |= flag;
					result.valid = true;
				}
			}
			if (end == std::string_view::npos)
				break;
			begin = end + 1;
		}
		return result;
	}
	for (const char digit : value) {
		if (digit < '0' || digit > '9')
			continue;
		if (digit == '0') {
			result.grapple = true;
			result.valid = true;
			continue;
		}
		const std::uint32_t flag = MM_ArenaWeaponFlagForDigit(digit);
		if (!flag)
			continue;
		result.mask |= flag;
		result.valid = true;
	}
	return result;
}

inline constexpr std::uint32_t MM_ArenaWeaponFlagForName(
	std::string_view name)
{
	if (name == "gauntlet" || name == "chainfist" || name == "melee")
		return MM_ARENA_WEAPON_GAUNTLET;
	if (name == "machinegun" || name == "chaingun")
		return MM_ARENA_WEAPON_MACHINEGUN;
	if (name == "shotgun" || name == "supershotgun")
		return MM_ARENA_WEAPON_SHOTGUN;
	if (name == "grenadelauncher" || name == "grenade")
		return MM_ARENA_WEAPON_GRENADE_LAUNCHER;
	if (name == "rocketlauncher" || name == "rocket")
		return MM_ARENA_WEAPON_ROCKET_LAUNCHER;
	if (name == "lightning" || name == "lightninggun" ||
		name == "plasmabeam")
		return MM_ARENA_WEAPON_LIGHTNING;
	if (name == "railgun" || name == "rail")
		return MM_ARENA_WEAPON_RAILGUN;
	if (name == "plasma" || name == "plasmagun" ||
		name == "hyperblaster")
		return MM_ARENA_WEAPON_PLASMA;
	if (name == "bfg" || name == "bfg10k")
		return MM_ARENA_WEAPON_BFG;
	return 0;
}

inline constexpr bool MM_ArenaSettingIsAmmo(std::string_view name)
{
	return name == "shells" || name == "bullets" || name == "slugs" ||
		name == "grenades" || name == "rockets" || name == "cells" ||
		name == "plasma" || name == "bfgammo";
}

inline constexpr std::uint32_t MM_ArenaVoteFlagForSetting(
	std::string_view key)
{
	if (key == "type" || key == "gametype" || key == "pickup")
		return MM_ARENA_VOTE_TYPE;
	if (key == "health")
		return MM_ARENA_VOTE_HEALTH;
	if (key == "armor")
		return MM_ARENA_VOTE_ARMOR;
	if (key == "playersperteam" || key == "ppt")
		return MM_ARENA_VOTE_PLAYERS_PER_TEAM;
	if (key == "rounds")
		return MM_ARENA_VOTE_ROUNDS;
	if (key == "healthprotect")
		return MM_ARENA_VOTE_HEALTH_PROTECTION;
	if (key == "armorprotect")
		return MM_ARENA_VOTE_ARMOR_PROTECTION;
	if (MM_ArenaSettingIsAmmo(key))
		return 0;
	if (key == "weapons" || key == "weaponmask")
		return MM_ARENA_VOTE_WEAPONS;
	const std::uint32_t weapon = MM_ArenaWeaponFlagForName(key);
	if (weapon == MM_ARENA_WEAPON_GAUNTLET)
		return MM_ARENA_VOTE_WEAPONS;
	if (weapon == MM_ARENA_WEAPON_MACHINEGUN)
		return MM_ARENA_VOTE_MACHINEGUN;
	if (weapon == MM_ARENA_WEAPON_SHOTGUN)
		return MM_ARENA_VOTE_SHOTGUN;
	if (weapon == MM_ARENA_WEAPON_GRENADE_LAUNCHER)
		return MM_ARENA_VOTE_GRENADE_LAUNCHER;
	if (weapon == MM_ARENA_WEAPON_ROCKET_LAUNCHER)
		return MM_ARENA_VOTE_ROCKET_LAUNCHER;
	if (weapon == MM_ARENA_WEAPON_LIGHTNING)
		return MM_ARENA_VOTE_CHAINGUN;
	if (weapon == MM_ARENA_WEAPON_PLASMA)
		return MM_ARENA_VOTE_HYPERBLASTER;
	if (weapon == MM_ARENA_WEAPON_RAILGUN)
		return MM_ARENA_VOTE_RAILGUN;
	if (weapon == MM_ARENA_WEAPON_BFG)
		return MM_ARENA_VOTE_BFG;
	if (key == "grapple")
		return MM_ARENA_VOTE_GRAPPLE;
	if (key == "rocketspeed")
		return MM_ARENA_VOTE_ROCKET_SPEED;
	if (key == "fallingdamage")
		return MM_ARENA_VOTE_FALLING;
	if (key == "excessive")
		return MM_ARENA_VOTE_EXCESSIVE;
	if (key == "lockarena" || key == "lockcount")
		return MM_ARENA_VOTE_LOCK;
	if (key == "competitionmode")
		return MM_ARENA_VOTE_COMPETITION;
	if (key == "minping")
		return MM_ARENA_VOTE_MIN_PING;
	if (key == "maxping")
		return MM_ARENA_VOTE_MAX_PING;
	if (key == "maxteams" || key == "max_teams")
		return MM_ARENA_VOTE_MAX_TEAMS;
	return 0;
}

inline constexpr bool MM_ArenaFighterRosterLocked(
	mm_arena_type_t type, mm_arena_state_t state,
	bool fighter, bool live_damageable)
{
	return type != mm_arena_type_t::Practice && fighter && live_damageable &&
		(state == mm_arena_state_t::Running ||
		 state == mm_arena_state_t::Paused);
}

inline constexpr bool MM_ArenaCompetitionCommandAllowed(
	bool competition_mode, bool admin, bool allow_admin_override)
{
	return competition_mode || (allow_admin_override && admin);
}

inline constexpr bool MM_ArenaTeamLockArgumentsValid(bool locking, int argc)
{
	return locking ? argc == 2 || argc == 3 : argc == 2;
}

inline constexpr bool MM_ArenaReadyCommandValue(
	std::string_view value, bool current, bool &ready)
{
	if (value.empty()) {
		ready = !current;
		return true;
	}
	if (value == "0") {
		ready = false;
		return true;
	}
	if (value == "1") {
		ready = true;
		return true;
	}
	return false;
}

inline constexpr bool MM_ArenaCanCallTimeout(
	mm_arena_state_t state, bool active_side, bool team_member)
{
	return state == mm_arena_state_t::Running && active_side && team_member;
}

inline constexpr bool MM_ArenaCanCallTimein(
	mm_arena_state_t state, bool active_side, bool team_member,
	bool resume_countdown, std::uint16_t caller_team,
	std::uint16_t timeout_team)
{
	return state == mm_arena_state_t::Paused && active_side && team_member &&
		!resume_countdown && caller_team != 0 && caller_team == timeout_team;
}

inline constexpr bool MM_ArenaSpectatorInviteCommandAllowed(
	bool competition_mode)
{
	return competition_mode;
}

inline constexpr std::uint8_t MM_ArenaClearSpectatorInviteBits(
	std::uint8_t invites)
{
	return static_cast<std::uint8_t>(invites & ~std::uint8_t { 0x03 });
}

inline constexpr std::uint8_t MM_ArenaInvitesAfterMemberTransfer(
	std::uint8_t invites, std::uint16_t old_team, std::uint16_t new_team)
{
	return old_team != 0 && old_team != new_team
		? MM_ArenaClearSpectatorInviteBits(invites) : invites;
}

inline constexpr bool MM_ArenaDelayedActivatorValid(bool inuse,
	bool connected, std::int32_t scheduled_spawn_count,
	std::int32_t current_spawn_count, int source_room, int current_room)
{
	return inuse && connected &&
		scheduled_spawn_count == current_spawn_count &&
		(source_room < 0 || source_room == current_room);
}

inline constexpr bool MM_ArenaConfigStartsColonSetting(
	bool known_setting, std::string_view key, std::string_view next_token)
{
	return next_token == ":" &&
		(known_setting || key == "format" || key == "maploop");
}

template <typename TokenRange>
inline bool MM_ArenaConfigBracesBalanced(const TokenRange &tokens)
{
	int depth = 0;
	for (const auto &token : tokens) {
		const std::string_view view(token.data(), token.size());
		if (view == "{")
			depth++;
		else if (view == "}") {
			if (depth == 0)
				return false;
			depth--;
		}
	}
	return depth == 0;
}

inline constexpr bool MM_ArenaTeamEligible(int member_count, int players_per_team)
{
	return players_per_team > 0 && member_count > 0 &&
		member_count <= players_per_team;
}

inline constexpr bool MM_ArenaTeamSizeIsUnlimited(mm_arena_type_t type)
{
	// RA3's fixed Clan Arena and Red Rover sides have no players-per-team
	// ceiling. The setting remains a queue size only for Rocket Arena teams.
	return type == mm_arena_type_t::ClanArena ||
		type == mm_arena_type_t::RedRover;
}

inline constexpr bool MM_ArenaUsesLogicalTeams(mm_arena_type_t type)
{
	return type != mm_arena_type_t::Practice;
}

inline constexpr bool MM_ArenaShouldAutoEnrollPractice(
	mm_arena_type_t type, mm_arena_role_t role)
{
	return type == mm_arena_type_t::Practice &&
		role != mm_arena_role_t::Coach;
}

inline constexpr bool MM_ArenaTeamEligibleForType(mm_arena_type_t type,
	int member_count, int players_per_team)
{
	return MM_ArenaTeamSizeIsUnlimited(type)
		? member_count > 0
		: MM_ArenaTeamEligible(member_count, players_per_team);
}

inline constexpr bool MM_ArenaShouldDestroyEmptyTeam(int member_count,
	bool transfer_destination)
{
	return member_count == 0 && !transfer_destination;
}

inline constexpr mm_arena_round_result_t MM_ArenaResolveRound(
	int red_living, int blue_living, bool time_expired)
{
	const bool red_alive = red_living > 0;
	const bool blue_alive = blue_living > 0;

	if (!red_alive && !blue_alive)
		return mm_arena_round_result_t::Draw;
	if (red_alive && !blue_alive)
		return mm_arena_round_result_t::Red;
	if (!red_alive && blue_alive)
		return mm_arena_round_result_t::Blue;
	if (time_expired)
		return mm_arena_round_result_t::Draw;
	return mm_arena_round_result_t::Ongoing;
}

inline constexpr mm_arena_round_result_t MM_ArenaSeriesWinner(
	int red_wins, int blue_wins, int best_of)
{
	const int wins_needed = MM_ArenaWinsNeeded(best_of);
	const bool red_won = red_wins >= wins_needed;
	const bool blue_won = blue_wins >= wins_needed;

	if (red_won && blue_won)
		return mm_arena_round_result_t::Draw;
	if (red_won)
		return mm_arena_round_result_t::Red;
	if (blue_won)
		return mm_arena_round_result_t::Blue;
	return mm_arena_round_result_t::Ongoing;
}

inline constexpr bool MM_ArenaProtectionBlocks(
	mm_arena_protection_t mode, bool same_team, bool self)
{
	if (!same_team)
		return false;
	switch (mode) {
	case mm_arena_protection_t::None:
		return false;
	case mm_arena_protection_t::SelfAndTeam:
		return true;
	case mm_arena_protection_t::Team:
		return !self;
	}
	return false;
}

inline constexpr bool MM_ArenaVotePassesAtExpiry(
	int yes, int no, int original_voters)
{
	return original_voters > 0 &&
		3 * (yes - no) > original_voters;
}

inline constexpr bool MM_ArenaLockVotePassesAtExpiry(
	int yes, int no, int original_voters, int lock_count)
{
	const int required_voters = lock_count > 0 ? lock_count : 6;
	return original_voters >= required_voters &&
		yes == original_voters && no == 0;
}

template <std::size_t N>
inline void MM_ArenaResolveVoteTries(
	std::array<std::uint8_t, N> &tries_used, bool passed)
{
	if (passed)
		tries_used.fill(0);
}

inline constexpr bool MM_ArenaTeamChatBypassesFlood(
	mm_arena_type_t type, bool competition_mode, bool team_scope)
{
	return type == mm_arena_type_t::ClanArena &&
		competition_mode && team_scope;
}

inline constexpr bool MM_ArenaTeamChatUsesArenaScope(mm_arena_type_t type)
{
	return type == mm_arena_type_t::Practice;
}

inline constexpr bool MM_ArenaTeamChatStatesMatch(bool first_playing,
	bool second_playing, bool first_coach, bool second_coach)
{
	return first_coach || second_coach || first_playing == second_playing;
}

inline constexpr bool MM_ArenaFollowAllowedByCompetition(
	bool competition_mode, bool coach, bool same_squad, bool invited)
{
	if (!competition_mode)
		return true;
	return same_squad || (!coach && invited);
}

inline constexpr bool MM_ArenaFreecamAllowedByCompetition(
	bool competition_mode, bool has_permitted_target)
{
	return !competition_mode || has_permitted_target;
}

struct mm_arena_rover_score_delta_t {
	int killer = 0;
	int victim = 0;
};

inline constexpr mm_arena_rover_score_delta_t
MM_ArenaRedRoverScoreDelta(bool player_kill)
{
	return player_kill
		? mm_arena_rover_score_delta_t { 1, -1 }
		: mm_arena_rover_score_delta_t { 0, -1 };
}

inline constexpr bool MM_ArenaRedRoverDestinationIsRed(
	bool victim_is_red, bool player_kill, bool killer_is_red)
{
	if (player_kill && killer_is_red != victim_is_red)
		return killer_is_red;
	return !victim_is_red;
}

inline constexpr bool MM_ArenaRoundEliminates(mm_arena_type_t type)
{
	return type != mm_arena_type_t::Practice;
}

inline constexpr bool MM_ArenaUsesFixedTeams(mm_arena_type_t type)
{
	return type == mm_arena_type_t::ClanArena ||
		type == mm_arena_type_t::RedRover;
}

inline constexpr bool MM_ArenaWinnerStays(mm_arena_type_t type)
{
	return type == mm_arena_type_t::RocketArena;
}

inline constexpr mm_arena_type_t MM_ArenaResolvePickupType(
	bool pickup, mm_arena_type_t selected, mm_arena_type_t pickup_type)
{
	return pickup ? pickup_type : selected;
}

inline constexpr int MM_ArenaClampStat(int value)
{
	return std::clamp(value, 0, 999);
}

inline bool MM_ArenaApplyAmmoSetting(mm_arena_settings_t &settings,
	std::string_view name, int value)
{
	const int clamped = MM_ArenaClampStat(value);
	if (name == "shells")
		settings.shells = clamped;
	else if (name == "bullets")
		settings.bullets = clamped;
	else if (name == "slugs")
		settings.slugs = clamped;
	else if (name == "grenades")
		settings.grenades = clamped;
	else if (name == "rockets")
		settings.rockets = clamped;
	else if (name == "cells")
		settings.cells = clamped;
	else if (name == "plasma")
		settings.plasma_ammo = clamped;
	else if (name == "bfgammo")
		settings.bfg_ammo = clamped;
	else
		return false;
	return true;
}

inline constexpr int MM_ArenaClampCountdown(int value)
{
	return std::clamp(value, 1, 30);
}

inline constexpr bool MM_ArenaParseBool(std::string_view value, bool fallback)
{
	if (value == "1" || value == "on" || value == "yes" || value == "true")
		return true;
	if (value == "0" || value == "off" || value == "no" || value == "false")
		return false;
	return fallback;
}

inline constexpr mm_arena_type_t MM_ArenaParseType(
	std::string_view value, mm_arena_type_t fallback)
{
	if (value == "0" || value == "rocket" || value == "ra" ||
		value == "rocketarena")
		return mm_arena_type_t::RocketArena;
	if (value == "1" || value == "clan" || value == "ca" ||
		value == "clanarena" || value == "pickup")
		return mm_arena_type_t::ClanArena;
	if (value == "2" || value == "rover" || value == "rr" ||
		value == "redrover")
		return mm_arena_type_t::RedRover;
	if (value == "3" || value == "practice" || value == "practicearena")
		return mm_arena_type_t::Practice;
	return fallback;
}

inline constexpr mm_arena_protection_t MM_ArenaParseProtection(
	std::string_view value, mm_arena_protection_t fallback)
{
	if (value == "0" || value == "none" || value == "off")
		return mm_arena_protection_t::None;
	if (value == "1" || value == "selfteam" || value == "all")
		return mm_arena_protection_t::SelfAndTeam;
	if (value == "2" || value == "team")
		return mm_arena_protection_t::Team;
	return fallback;
}
