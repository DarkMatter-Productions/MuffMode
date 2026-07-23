// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_arena_rules.h"
#include "muffmode/mm_captain.h"
#include "muffmode/mm_chat.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_hud_stat_contracts.h"
#include "muffmode/mm_menu.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_red_rover_rules.h"
#include "muffmode/mm_scoring.h"
#include "muffmode/mm_team.h"
#include "muffmode/mm_time_format.h"
#include "muffmode/mm_util.h"
#include "muffmode/mm_vote.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

static void MM_Arena_OpenMenuPage(gentity_t *ent, int page, bool rooms);

namespace muffmode::arena {

constexpr int kMaxLogicalTeams = 255;
constexpr int kMaxConfigBytes = 1024 * 1024;
constexpr int kMaxConfigTokens = 32768;
constexpr int kMenuRows = 20;
constexpr uint8_t kInviteSpectator = 1u << 0;
constexpr uint8_t kInviteCoach = 1u << 1;
constexpr uint8_t kInviteMember = 1u << 2;

struct LogicalTeam {
	bool valid = false;
	bool fixed = false;
	uint16_t id = 0;
	int arena_id = 0;
	team_t side = TEAM_NONE;
	std::string name;
	std::string password;
	int captain = -1;
	bool locked = false;
	uint32_t queue_order = 0;
	int timeouts_used = 0;
	std::array<uint8_t, MAX_CLIENTS> invites {};
	bool chat_muted = false;
};

struct Ballot {
	bool active = false;
	int proposer = -1;
	int original_voters = 0;
	uint32_t setting_flag = 0;
	std::string description;
	std::string setting_key;
	std::string setting_value;
	gtime_t expires {};
	std::array<bool, MAX_CLIENTS> eligible {};
	std::array<int8_t, MAX_CLIENTS> votes {};
};

struct Arena {
	bool valid = false;
	int id = 0;
	std::string name;
	mm_arena_settings_t defaults {};
	mm_arena_settings_t settings {};
	mm_arena_state_t state = mm_arena_state_t::Empty;
	mm_arena_state_t state_before_pause = mm_arena_state_t::Empty;
	gtime_t state_timer {};
	gtime_t paused_remaining {};
	bool resume_countdown = false;
	uint16_t timeout_team = 0;
	uint16_t active_red = 0;
	uint16_t active_blue = 0;
	uint16_t fixed_red = 0;
	uint16_t fixed_blue = 0;
	uint16_t series_winner = 0;
	int red_score = 0;
	int blue_score = 0;
	int round = 0;
	uint32_t next_queue_order = 1;
	bool occupied = false;
	bool red_uses_odd_spawns = true;
	bool settings_pending = false;
	mm_arena_settings_t pending_settings {};
	std::array<uint8_t, MAX_CLIENTS> vote_tries_used {};
	Ballot ballot {};
};

struct ConfigOp {
	int specificity = 0; // built-in/cvars are applied before 0; config global/map/arena = 0/1/2.
	int arena_id = 0;
	std::string key;
	std::string value;
};

struct MenuState {
	std::array<int, kMenuRows> actions {};
	bool rooms = true;
	int page = 0;
};

std::array<Arena, MM_MAX_ARENAS + 1> s_arenas;
std::array<LogicalTeam, kMaxLogicalTeams + 1> s_teams;
std::array<team_t, MAX_CLIENTS> s_rover_pending_side {};
std::array<int16_t, MAX_CLIENTS> s_rover_pending_arena {};
std::array<bool, MAX_CLIENTS> s_rover_respawn_pending {};
std::array<int16_t, MAX_CLIENTS> s_rover_respawn_arena {};
std::array<bool, MAX_CLIENTS> s_practice_respawn_pending {};
std::array<int16_t, MAX_CLIENTS> s_practice_respawn_arena {};
int s_arena_count = 0;
bool s_classic_ra2_map = false;
bool s_map_active = false;
mm_arena_map_contract_t s_map_contract {};
mm_arena_map_validation_t s_map_validation {
	mm_arena_map_error_t::MissingArenaKey, 0
};
bool s_internal_team_change = false;
bool s_ended_round_this_frame = false;
bool s_had_human_participant = false;
gtime_t s_level_started {};

cvar_t *s_config;
cvar_t *s_type;
cvar_t *s_competition;
cvar_t *s_unbalanced;
cvar_t *s_health_protect;
cvar_t *s_armor_protect;
cvar_t *s_excessive;
cvar_t *s_grapple;
cvar_t *s_rocket_speed;
cvar_t *s_lock_arena;
cvar_t *s_lock_count;
cvar_t *s_max_players;
cvar_t *s_vote_seconds;
cvar_t *s_timeouts;

bool ParseFiniteOrigin(const char *text)
{
	if (!text || !*text)
		return false;
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	char trailing = '\0';
	if (std::sscanf(text, " %f %f %f %c", &x, &y, &z, &trailing) != 3)
		return false;
	return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

bool IsLobbyPointClass(const char *classname)
{
	return classname &&
		(!std::strcmp(classname, "misc_teleporter_dest") ||
		 !std::strcmp(classname, "info_player_start") ||
		 !std::strcmp(classname, "info_player_deathmatch"));
}

void AddContractEntity(mm_arena_map_contract_t &contract, bool first,
	const char *classname, int arena_id, bool arena_value_valid,
	int arena_key_count, const char *origin, const char *message)
{
	if (first) {
		contract.first_entity_is_worldspawn =
			classname && !std::strcmp(classname, "worldspawn");
		contract.world_arena_key_count = arena_key_count;
		contract.world_arena_value_valid =
			arena_key_count == 1 && arena_value_valid;
		contract.declared_rooms = arena_id;
		return;
	}

	if (!arena_value_valid || !ParseFiniteOrigin(origin))
		return;
	if (arena_id == 0 && IsLobbyPointClass(classname))
		contract.has_lobby_point = true;
	if (arena_id < 1 || arena_id > MM_ARENA_MAP_MAX_ROOMS)
		return;
	if (classname && !std::strcmp(classname, "info_player_deathmatch")) {
		uint16_t &count = contract.fighter_starts[arena_id];
		if (count != std::numeric_limits<uint16_t>::max())
			count++;
	} else if (classname &&
		!std::strcmp(classname, "info_player_intermission") &&
		!MM_TrimAsciiWhitespace(message ? std::string_view(message) :
			std::string_view {}).empty()) {
		contract.named_intermissions[arena_id] = true;
	}
}

mm_arena_map_contract_t ParseMapContract(const char *entity_lump)
{
	mm_arena_map_contract_t contract;
	if (!entity_lump)
		return contract;

	const char *cursor = entity_lump;
	int entity_index = 0;
	while (cursor) {
		char token[MAX_TOKEN_CHARS] {};
		COM_Parse(&cursor, token, sizeof(token));
		if (!cursor && !token[0])
			break;
		if (std::strcmp(token, "{"))
			return contract;

		char classname[MAX_TOKEN_CHARS] {};
		char origin[MAX_TOKEN_CHARS] {};
		char message[MAX_TOKEN_CHARS] {};
		int arena_id = 0;
		int arena_key_count = 0;
		bool arena_value_valid = true;
		bool closed = false;

		while (cursor) {
			char key[MAX_TOKEN_CHARS] {};
			COM_Parse(&cursor, key, sizeof(key));
			if (!std::strcmp(key, "}")) {
				closed = true;
				break;
			}
			if (!cursor || !key[0] || !std::strcmp(key, "{"))
				return contract;

			char value[MAX_TOKEN_CHARS] {};
			COM_Parse(&cursor, value, sizeof(value));
			if (!cursor || !std::strcmp(value, "}") ||
				!std::strcmp(value, "{"))
				return contract;

			if (!Q_strcasecmp(key, "classname"))
				Q_strlcpy(classname, value, sizeof(classname));
			else if (!Q_strcasecmp(key, "arena")) {
				arena_key_count++;
				const auto parsed = MM_ParseIntArg(value);
				arena_value_valid = parsed.has_value();
				if (parsed)
					arena_id = *parsed;
			} else if (!Q_strcasecmp(key, "origin"))
				Q_strlcpy(origin, value, sizeof(origin));
			else if (!Q_strcasecmp(key, "message"))
				Q_strlcpy(message, value, sizeof(message));
		}
		if (!closed)
			return contract;

		// Like ED_ParseField, duplicate non-world arena keys use the last value.
		// Worldspawn is intentionally stricter because it is the activation key.
		AddContractEntity(contract, entity_index == 0, classname, arena_id,
			arena_key_count == 0 || arena_value_valid, arena_key_count,
			origin, message);
		entity_index++;
	}
	contract.syntax_valid = entity_index > 0;
	return contract;
}

mm_arena_map_contract_t LiveMapContract()
{
	mm_arena_map_contract_t contract = s_map_contract;
	contract.has_lobby_point = false;
	contract.fighter_starts.fill(0);
	contract.named_intermissions.fill(false);

	for (size_t i = 1; i < globals.num_entities; i++) {
		const gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->classname ||
			!std::isfinite(ent->s.origin.x) ||
			!std::isfinite(ent->s.origin.y) ||
			!std::isfinite(ent->s.origin.z))
			continue;
		if (ent->arena == 0 && IsLobbyPointClass(ent->classname))
			contract.has_lobby_point = true;
		if (ent->arena < 1 || ent->arena > MM_ARENA_MAP_MAX_ROOMS)
			continue;
		if (!std::strcmp(ent->classname, "info_player_deathmatch")) {
			uint16_t &count = contract.fighter_starts[ent->arena];
			if (count != std::numeric_limits<uint16_t>::max())
				count++;
		} else if (!std::strcmp(ent->classname,
			"info_player_intermission") && ent->message &&
			!MM_TrimAsciiWhitespace(ent->message).empty()) {
			contract.named_intermissions[ent->arena] = true;
		}
	}
	return contract;
}

bool IsArenaGametype()
{
	return MM_Arena_Active();
}

bool IsConnected(const gentity_t *ent)
{
	return ent && ent->inuse && ent->client && ent->client->pers.connected;
}

int ClientNumber(const gclient_t *client)
{
	if (!client || client < game.clients ||
		client >= game.clients + static_cast<ptrdiff_t>(game.maxclients))
		return -1;
	return static_cast<int>(client - game.clients);
}

int ClientNumber(const gentity_t *ent)
{
	return ent && ent->client ? ClientNumber(ent->client) : -1;
}

gentity_t *ClientEntity(int client_num)
{
	if (client_num < 0 || client_num >= static_cast<int>(game.maxclients))
		return nullptr;
	return &g_entities[client_num + 1];
}

void ClearPendingClientEvents(int client_num)
{
	if (client_num < 0 || client_num >= MAX_CLIENTS)
		return;
	s_rover_pending_side[client_num] = TEAM_NONE;
	s_rover_pending_arena[client_num] = 0;
	s_rover_respawn_pending[client_num] = false;
	s_rover_respawn_arena[client_num] = 0;
	s_practice_respawn_pending[client_num] = false;
	s_practice_respawn_arena[client_num] = 0;
}

void ClearArenaPendingEvents(int arena_id)
{
	for (int client_num = 0; client_num < MAX_CLIENTS; client_num++) {
		if (s_rover_pending_arena[client_num] == arena_id) {
			s_rover_pending_side[client_num] = TEAM_NONE;
			s_rover_pending_arena[client_num] = 0;
		}
		if (s_rover_respawn_arena[client_num] == arena_id) {
			s_rover_respawn_pending[client_num] = false;
			s_rover_respawn_arena[client_num] = 0;
		}
		if (s_practice_respawn_arena[client_num] == arena_id) {
			s_practice_respawn_pending[client_num] = false;
			s_practice_respawn_arena[client_num] = 0;
		}
	}
}

void ClearClientSlotState(int client_num)
{
	if (client_num < 0 || client_num >= MAX_CLIENTS)
		return;
	ClearPendingClientEvents(client_num);
	for (LogicalTeam &team : s_teams)
		team.invites[client_num] = 0;
	for (Arena &arena : s_arenas) {
		arena.vote_tries_used[client_num] = 0;
		arena.ballot.eligible[client_num] = false;
		arena.ballot.votes[client_num] = 0;
	}
}

mm_arena_role_t Role(const gclient_t *client)
{
	if (!client)
		return mm_arena_role_t::Lobby;
	const uint8_t raw = client->resp.arena_role;
	if (raw > static_cast<uint8_t>(mm_arena_role_t::Coach))
		return mm_arena_role_t::Lobby;
	return static_cast<mm_arena_role_t>(raw);
}

void SetRoleField(gclient_t *client, mm_arena_role_t role)
{
	if (client)
		client->resp.arena_role = static_cast<uint8_t>(role);
}

Arena *FindArena(int id)
{
	if (id < 1 || id > s_arena_count || !s_arenas[id].valid)
		return nullptr;
	return &s_arenas[id];
}

const Arena *FindArena(int id, const int)
{
	return FindArena(id);
}

Arena *ArenaFor(const gclient_t *client)
{
	return client ? FindArena(client->resp.arena_id) : nullptr;
}

const Arena *ArenaForConst(const gclient_t *client)
{
	return client ? FindArena(client->resp.arena_id, 0) : nullptr;
}

LogicalTeam *FindTeam(uint16_t id)
{
	if (!id || id > kMaxLogicalTeams || !s_teams[id].valid)
		return nullptr;
	return &s_teams[id];
}

const LogicalTeam *FindTeam(uint16_t id, const int)
{
	return FindTeam(id);
}

bool IsTeamMember(const gclient_t *client, uint16_t team_id)
{
	if (!client || !team_id || client->resp.arena_team_id != team_id)
		return false;
	return Role(client) != mm_arena_role_t::Coach;
}

std::vector<gentity_t *> TeamMembers(uint16_t team_id, bool include_coaches = false)
{
	std::vector<gentity_t *> result;
	for (gentity_t *ent : active_clients()) {
		if (!IsConnected(ent) || ent->client->resp.arena_team_id != team_id)
			continue;
		if (!include_coaches && Role(ent->client) == mm_arena_role_t::Coach)
			continue;
		result.push_back(ent);
	}
	std::sort(result.begin(), result.end(), [](const gentity_t *a, const gentity_t *b) {
		return a->s.number < b->s.number;
	});
	return result;
}

int MemberCount(uint16_t team_id, bool line_only = false)
{
	int count = 0;
	for (gentity_t *ent : active_clients()) {
		if (!IsConnected(ent) || !IsTeamMember(ent->client, team_id))
			continue;
		if (line_only && !ent->client->resp.arena_line_enabled)
			continue;
		count++;
	}
	return count;
}

int CurrentRoundMemberCount(uint16_t team_id)
{
	int count = 0;
	for (gentity_t *ent : active_clients())
		if (IsConnected(ent) && IsTeamMember(ent->client, team_id) &&
			Role(ent->client) == mm_arena_role_t::Fighter)
			count++;
	return count;
}

int LivingCount(uint16_t team_id)
{
	int count = 0;
	for (gentity_t *ent : active_clients()) {
		if (!IsConnected(ent) || !IsTeamMember(ent->client, team_id) ||
			Role(ent->client) != mm_arena_role_t::Fighter)
			continue;
		if (!ent->client->eliminated && !ent->deadflag && ent->health > 0)
			count++;
	}
	return count;
}

int ArenaPopulation(int arena_id)
{
	int count = 0;
	for (gentity_t *ent : active_clients())
		if (IsConnected(ent) && ent->client->resp.arena_id == arena_id)
			count++;
	return count;
}

int LogicalTeamCount(int arena_id)
{
	int count = 0;
	for (uint16_t id = 1; id <= kMaxLogicalTeams; id++)
		if (s_teams[id].valid && !s_teams[id].fixed &&
			s_teams[id].arena_id == arena_id)
			count++;
	return count;
}

bool PingAllowed(const gentity_t *ent, const Arena &arena)
{
	if (!IsConnected(ent) || ent->client->sess.admin ||
		ent->client->sess.is_a_bot || (ent->svflags & SVF_BOT))
		return true;
	const int ping = std::max(0, ent->client->ping);
	return (!arena.settings.min_ping || ping >= arena.settings.min_ping) &&
		(!arena.settings.max_ping || ping <= arena.settings.max_ping);
}

void ResetPlayerScores(const Arena &arena)
{
	for (gentity_t *ent : active_clients())
		if (IsConnected(ent) && ent->client->resp.arena_id == arena.id)
			MM_ResetClientScoring(ent->client);
}

int ArenaFighters(int arena_id)
{
	int count = 0;
	for (gentity_t *ent : active_clients())
		if (IsConnected(ent) && ent->client->resp.arena_id == arena_id &&
			Role(ent->client) == mm_arena_role_t::Fighter)
			count++;
	return count;
}

std::string Lower(std::string_view input)
{
	std::string result(input);
	for (char &c : result)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return result;
}

bool ParseInt(std::string_view text, int &value)
{
	const auto parsed = MM_ParseIntText(text);
	if (!parsed)
		return false;
	value = *parsed;
	return true;
}

bool ParseBool(std::string_view text, bool &value)
{
	const auto parsed = MM_ParseBoolText(text);
	if (!parsed)
		return false;
	value = *parsed;
	return true;
}

bool ParseProtection(std::string_view value, mm_arena_protection_t &protection)
{
	if (value == "0" || value == "none" || value == "off")
		protection = mm_arena_protection_t::None;
	else if (value == "1" || value == "selfteam" || value == "all")
		protection = mm_arena_protection_t::SelfAndTeam;
	else if (value == "2" || value == "team")
		protection = mm_arena_protection_t::Team;
	else
		return false;
	return true;
}

std::string SafeText(std::string_view input, size_t max_length = 40)
{
	std::string result;
	result.reserve(std::min(input.size(), max_length));
	for (char c : input) {
		if (result.size() >= max_length)
			break;
		const unsigned char u = static_cast<unsigned char>(c);
		if (u < 32 || c == '"' || c == '\\')
			continue;
		result.push_back(c);
	}
	return result;
}

std::string JoinCommandArguments(int first)
{
	std::string result;
	for (int i = std::max(0, first); i < gi.argc(); i++) {
		if (!result.empty())
			result.push_back(' ');
		result += gi.argv(i);
	}
	return result;
}

const char *TypeName(mm_arena_type_t type)
{
	switch (type) {
	case mm_arena_type_t::RocketArena: return "Rocket Arena";
	case mm_arena_type_t::ClanArena: return "Clan Arena";
	case mm_arena_type_t::RedRover: return "Red Rover";
	case mm_arena_type_t::Practice: return "Practice";
	}
	return "Rocket Arena";
}

const char *StateName(mm_arena_state_t state)
{
	switch (state) {
	case mm_arena_state_t::Empty: return "EMPTY";
	case mm_arena_state_t::Warmup: return "WARMUP";
	case mm_arena_state_t::MatchCountdown: return "MATCH COUNTDOWN";
	case mm_arena_state_t::RoundCountdown: return "ROUND COUNTDOWN";
	case mm_arena_state_t::Running: return "FIGHT";
	case mm_arena_state_t::RoundOver: return "ROUND OVER";
	case mm_arena_state_t::MatchOver: return "MATCH OVER";
	case mm_arena_state_t::Paused: return "PAUSED";
	}
	return "";
}

bool IsActiveSeriesState(mm_arena_state_t state)
{
	return state == mm_arena_state_t::MatchCountdown ||
		state == mm_arena_state_t::RoundCountdown ||
		state == mm_arena_state_t::Running ||
		state == mm_arena_state_t::RoundOver ||
		state == mm_arena_state_t::Paused;
}

bool FighterRosterLocked(const gentity_t *ent)
{
	if (!IsConnected(ent))
		return false;
	const Arena *arena = ArenaForConst(ent->client);
	return arena && MM_ArenaFighterRosterLocked(
		arena->settings.type, arena->state,
		Role(ent->client) == mm_arena_role_t::Fighter,
		!ent->client->eliminated && !ent->deadflag &&
			ent->health > 0 && ent->takedamage);
}

uint32_t VoteFlagForSetting(std::string_view key)
{
	return MM_ArenaVoteFlagForSetting(key);
}

bool ApplySetting(mm_arena_settings_t &settings, std::string_view raw_key,
	std::string_view raw_value, int max_clients)
{
	const std::string key = Lower(raw_key);
	const std::string value = Lower(raw_value);
	int number = 0;

	if (key == "type" || key == "gametype") {
		if (value == "pickup") {
			settings.legacy_pickup = true;
			settings.type = settings.pickup_type;
			return true;
		}
		const auto parsed = MM_ArenaParseType(value, settings.type);
		if (parsed == settings.type && value != "0" && value != "rocket" &&
			value != "ra" && value != "rocketarena" && value != "1" &&
			value != "clan" && value != "ca" && value != "clanarena" &&
			value != "2" && value != "rover" &&
			value != "rr" && value != "redrover" && value != "3" &&
			value != "practice" && value != "practicearena")
			return false;
		settings.type = parsed;
		settings.legacy_pickup = false;
		return true;
	}
	if (key == "defpickup") {
		const auto parsed = MM_ArenaParseType(value, settings.pickup_type);
		const bool valid = value == "0" || value == "rocket" ||
			value == "ra" || value == "rocketarena" || value == "1" ||
			value == "clan" || value == "ca" || value == "clanarena" ||
			value == "2" || value == "rover" || value == "rr" ||
			value == "redrover" || value == "3" || value == "practice" ||
			value == "practicearena";
		if (!valid)
			return false;
		settings.pickup_type = parsed;
		return true;
	}
	if (key == "pickup") {
		if (!ParseBool(value, settings.legacy_pickup))
			return false;
		settings.type = settings.legacy_pickup
			? settings.pickup_type : mm_arena_type_t::RocketArena;
		return true;
	}
	if (key == "weapons" || key == "weaponmask") {
		if (key == "weaponmask") {
			if (!ParseInt(value, number) || number < 0)
				return false;
			settings.weapon_mask =
				MM_ArenaSanitizeWeaponMask(static_cast<uint32_t>(number));
			return true;
		}
		uint32_t mask = 0;
		bool saw_weapon_number = false;
		bool grapple = false;
		for (char c : value) {
			if (c >= '0' && c <= '9') {
				saw_weapon_number = true;
				if (c == '0')
					grapple = true;
				else
					mask |= MM_ArenaWeaponFlagForDigit(c);
			}
		}
		if (!saw_weapon_number)
			return false;
		settings.weapon_mask = MM_ArenaSanitizeWeaponMask(mask);
		settings.grapple = grapple;
		return true;
	}
	if (MM_ArenaSettingIsAmmo(key)) {
		if (!ParseInt(value, number))
			return false;
		return MM_ArenaApplyAmmoSetting(settings, key, number);
	}
	if (const uint32_t bit = MM_ArenaWeaponFlagForName(key)) {
		bool enabled = false;
		if (!ParseBool(value, enabled))
			return false;
		if (enabled)
			settings.weapon_mask |= bit;
		else
			settings.weapon_mask &= ~bit;
		return true;
	}
	if (key == "armor" && ParseInt(value, number)) {
		settings.armor = MM_ArenaClampStat(number);
		return true;
	}
	if (key == "health" && ParseInt(value, number)) {
		settings.health = std::clamp(number, 1, 999);
		return true;
	}
	if ((key == "playersperteam" || key == "ppt") && ParseInt(value, number)) {
		settings.players_per_team = MM_ArenaNormalizePlayersPerTeam(number, max_clients);
		return true;
	}
	if (key == "rounds" && ParseInt(value, number)) {
		settings.rounds = MM_ArenaNormalizeBestOf(number);
		return true;
	}
	if (key == "fastswitch") {
		return ParseBool(value, settings.fast_switch);
	}
	if (key == "grapple") {
		return ParseBool(value, settings.grapple);
	}
	if (key == "rocketspeed" && ParseInt(value, number)) {
		settings.rocket_speed = std::clamp(number, 1, 4000);
		return true;
	}
	if (key == "fallingdamage") {
		return ParseBool(value, settings.falling_damage);
	}
	if (key == "excessive") {
		return ParseBool(value, settings.excessive);
	}
	if (key == "damagescoring") {
		return ParseBool(value, settings.damage_scoring);
	}
	if (key == "lockarena") {
		return ParseBool(value, settings.lock_arena);
	}
	if (key == "competitionmode") {
		return ParseBool(value, settings.competition_mode);
	}
	if (key == "unbalanced") {
		return ParseBool(value, settings.unbalanced);
	}
	if (key == "lockcount" && ParseInt(value, number)) {
		settings.lock_count = std::clamp(number, 0, max_clients);
		return true;
	}
	if (key == "maxplayers" && ParseInt(value, number)) {
		settings.max_players = std::clamp(number, 0, max_clients);
		return true;
	}
	if (key == "minping" && ParseInt(value, number)) {
		settings.min_ping = std::clamp(number, 0, 999);
		return true;
	}
	if (key == "maxping" && ParseInt(value, number)) {
		settings.max_ping = std::clamp(number, 0, 999);
		return true;
	}
	if ((key == "maxteams" || key == "max_teams") &&
		ParseInt(value, number)) {
		settings.max_teams = std::clamp(number, 0, max_clients);
		return true;
	}
	if (key == "votetries" && ParseInt(value, number)) {
		settings.vote_tries = std::clamp(number, 0, 99);
		return true;
	}
	if (key == "armorprotect") {
		return ParseProtection(value, settings.armor_protect);
	}
	if (key == "healthprotect") {
		return ParseProtection(value, settings.health_protect);
	}

	constexpr std::array<std::pair<std::string_view, uint32_t>, 18> vote_keys = {{
		{ "allow_voting_gametype", MM_ARENA_VOTE_TYPE },
		{ "allow_voting_healtharmor", MM_ARENA_VOTE_HEALTH_ARMOR },
		{ "allow_voting_health", MM_ARENA_VOTE_HEALTH },
		{ "allow_voting_armor", MM_ARENA_VOTE_ARMOR },
		{ "allow_voting_playersperteam", MM_ARENA_VOTE_PLAYERS_PER_TEAM },
		{ "allow_voting_rounds", MM_ARENA_VOTE_ROUNDS },
		{ "allow_voting_protection", MM_ARENA_VOTE_PROTECTION },
		{ "allow_voting_armorprotect", MM_ARENA_VOTE_ARMOR_PROTECTION },
		{ "allow_voting_healthprotect", MM_ARENA_VOTE_HEALTH_PROTECTION },
		{ "allow_voting_weapons", MM_ARENA_VOTE_WEAPONS },
		{ "allow_voting_fallingdamage", MM_ARENA_VOTE_FALLING },
		{ "allow_voting_excessive", MM_ARENA_VOTE_EXCESSIVE },
		{ "allow_voting_lockarena", MM_ARENA_VOTE_LOCK },
		{ "allow_voting_competitionmode", MM_ARENA_VOTE_COMPETITION },
		{ "allow_voting_ping", MM_ARENA_VOTE_PING },
		{ "allow_voting_minping", MM_ARENA_VOTE_MIN_PING },
		{ "allow_voting_maxping", MM_ARENA_VOTE_MAX_PING },
		{ "allow_voting_maxteams", MM_ARENA_VOTE_MAX_TEAMS }
	}};
	for (const auto &[vote_key, flag] : vote_keys) {
		if (key != vote_key)
			continue;
		bool enabled = false;
		if (!ParseBool(value, enabled))
			return false;
		if (enabled)
			settings.vote_allow_mask |= flag;
		else
			settings.vote_allow_mask &= ~flag;
		return true;
	}
	uint32_t legacy_vote_flag = 0;
	if (key == "allowvotinggametype" || key == "allowvotingpickup")
		legacy_vote_flag = MM_ARENA_VOTE_TYPE;
	else if (key == "allowvotinghealtharmor")
		legacy_vote_flag = MM_ARENA_VOTE_HEALTH_ARMOR;
	else if (key == "allowvotinghealtharmorprotect")
		legacy_vote_flag = MM_ARENA_VOTE_PROTECTION;
	else if (key == "allowvotingarmor")
		legacy_vote_flag = MM_ARENA_VOTE_ARMOR;
	else if (key == "allowvotinghealth")
		legacy_vote_flag = MM_ARENA_VOTE_HEALTH;
	else if (key == "allowvotingplayersperteam")
		legacy_vote_flag = MM_ARENA_VOTE_PLAYERS_PER_TEAM;
	else if (key == "allowvotingrounds")
		legacy_vote_flag = MM_ARENA_VOTE_ROUNDS;
	else if (key == "allowvotingarmorprotect")
		legacy_vote_flag = MM_ARENA_VOTE_ARMOR_PROTECTION;
	else if (key == "allowvotinghealthprotect")
		legacy_vote_flag = MM_ARENA_VOTE_HEALTH_PROTECTION;
	else if (key == "allowvotingfallingdamage")
		legacy_vote_flag = MM_ARENA_VOTE_FALLING;
	else if (key == "allowvotingexcessive")
		legacy_vote_flag = MM_ARENA_VOTE_EXCESSIVE;
	else if (key == "allowvotinglockarena")
		legacy_vote_flag = MM_ARENA_VOTE_LOCK;
	else if (key == "allowvotingcompetitionmode")
		legacy_vote_flag = MM_ARENA_VOTE_COMPETITION;
	else if (key == "allowvotingping")
		legacy_vote_flag = MM_ARENA_VOTE_PING;
	else if (key == "allowvotingminping")
		legacy_vote_flag = MM_ARENA_VOTE_MIN_PING;
	else if (key == "allowvotingmaxping")
		legacy_vote_flag = MM_ARENA_VOTE_MAX_PING;
	else if (key == "allowvotingmaxteams")
		legacy_vote_flag = MM_ARENA_VOTE_MAX_TEAMS;
	else if (key == "allowvotingweapons")
		legacy_vote_flag = MM_ARENA_VOTE_WEAPONS;
	else if (key == "allowvotingshotgun")
		legacy_vote_flag = MM_ARENA_VOTE_SHOTGUN;
	else if (key == "allowvotingsupershotgun")
		legacy_vote_flag = MM_ARENA_VOTE_SUPER_SHOTGUN;
	else if (key == "allowvotingmachinegun")
		legacy_vote_flag = MM_ARENA_VOTE_MACHINEGUN;
	else if (key == "allowvotingchaingun")
		legacy_vote_flag = MM_ARENA_VOTE_CHAINGUN;
	else if (key == "allowvotinggrenadelauncher")
		legacy_vote_flag = MM_ARENA_VOTE_GRENADE_LAUNCHER;
	else if (key == "allowvotingrocketlauncher")
		legacy_vote_flag = MM_ARENA_VOTE_ROCKET_LAUNCHER;
	else if (key == "allowvotinghyperblaster")
		legacy_vote_flag = MM_ARENA_VOTE_HYPERBLASTER;
	else if (key == "allowvotingrailgun")
		legacy_vote_flag = MM_ARENA_VOTE_RAILGUN;
	else if (key == "allowvotingbfg")
		legacy_vote_flag = MM_ARENA_VOTE_BFG;
	if (legacy_vote_flag) {
		bool enabled = false;
		if (!ParseBool(value, enabled))
			return false;
		if (enabled)
			settings.vote_allow_mask |= legacy_vote_flag;
		else
			settings.vote_allow_mask &= ~legacy_vote_flag;
		return true;
	}
	if (key == "allow_voting_grapple" || key == "allow_voting_rocketspeed" ||
		key == "allowvotinggrapple" || key == "allowvotingrocketspeed") {
		const uint32_t flag =
			(key == "allow_voting_grapple" || key == "allowvotinggrapple")
			? MM_ARENA_VOTE_GRAPPLE : MM_ARENA_VOTE_ROCKET_SPEED;
		bool enabled = false;
		if (!ParseBool(value, enabled))
			return false;
		if (enabled)
			settings.vote_allow_mask |= flag;
		else
			settings.vote_allow_mask &= ~flag;
		return true;
	}

	return false;
}

void FinalizeSettings(mm_arena_settings_t &settings, int max_clients)
{
	if (!settings.legacy_pickup)
		return;
	settings.type = MM_ArenaResolvePickupType(
		settings.legacy_pickup, settings.type, settings.pickup_type);
	// RA2 writes 0x80 here: pickup teams are bounded by the server, not the
	// ordinary half-capacity per-team normalization used by RA3 settings.
	if (settings.type == mm_arena_type_t::ClanArena ||
		settings.type == mm_arena_type_t::RedRover) {
		settings.players_per_team = std::max(1, max_clients);
		settings.max_teams = 2;
		settings.unbalanced = true;
	}
}

bool IsConfigSettingName(std::string_view key)
{
	mm_arena_settings_t probe;
	return ApplySetting(probe, key, "0", MAX_CLIENTS);
}

mm_arena_settings_t CvarDefaults()
{
	mm_arena_settings_t result;
	const int max_clients = std::max(1, static_cast<int>(game.maxclients));
	result.type = MM_ArenaParseType(
		Lower(s_type ? s_type->string : "rocket"), result.type);
	result.players_per_team = MM_ArenaNormalizePlayersPerTeam(
		g_arena_players_per_team ? g_arena_players_per_team->integer : 1, max_clients);
	result.rounds = MM_ArenaNormalizeBestOf(
		g_arena_rounds ? g_arena_rounds->integer : 1);
	result.weapon_mask = MM_ArenaSanitizeWeaponMask(static_cast<uint32_t>(
		std::max(0, g_arena_weapon_mask ? g_arena_weapon_mask->integer :
			static_cast<int>(MM_ARENA_WEAPON_STANDARD))));
	result.armor = MM_ArenaClampStat(
		g_arena_start_armor ? g_arena_start_armor->integer : 100);
	result.health = std::clamp(
		g_arena_start_health ? g_arena_start_health->integer : 100, 1, 999);
	result.shells = MM_ArenaClampStat(
		g_arena_ammo_shells ? g_arena_ammo_shells->integer : 100);
	result.bullets = MM_ArenaClampStat(
		g_arena_ammo_bullets ? g_arena_ammo_bullets->integer : 200);
	result.grenades = MM_ArenaClampStat(
		g_arena_ammo_grenades ? g_arena_ammo_grenades->integer : 20);
	result.rockets = MM_ArenaClampStat(
		g_arena_ammo_rockets ? g_arena_ammo_rockets->integer : 50);
	result.cells = MM_ArenaClampStat(
		g_arena_ammo_cells ? g_arena_ammo_cells->integer : 150);
	result.slugs = MM_ArenaClampStat(
		g_arena_ammo_slugs ? g_arena_ammo_slugs->integer : 50);
	result.fast_switch = g_arena_fast_switch &&
		g_arena_fast_switch->integer != 0;
	result.competition_mode = s_competition && s_competition->integer != 0;
	result.unbalanced = s_unbalanced && s_unbalanced->integer != 0;
	result.excessive = s_excessive && s_excessive->integer != 0;
	result.grapple = s_grapple && s_grapple->integer != 0;
	result.rocket_speed = std::clamp(
		s_rocket_speed ? s_rocket_speed->integer : 900, 1, 4000);
	result.lock_arena = s_lock_arena && s_lock_arena->integer != 0;
	result.lock_count = std::clamp(s_lock_count ? s_lock_count->integer : 0, 0, max_clients);
	result.max_players = std::clamp(s_max_players ? s_max_players->integer : 0, 0, max_clients);
	result.falling_damage = g_arena_falling_damage &&
		g_arena_falling_damage->integer != 0;
	result.health_protect = MM_ArenaParseProtection(
		Lower(s_health_protect ? s_health_protect->string : "1"),
		mm_arena_protection_t::SelfAndTeam);
	result.armor_protect = MM_ArenaParseProtection(
		Lower(s_armor_protect ? s_armor_protect->string : "2"),
		mm_arena_protection_t::Team);
	return result;
}

bool IsSafeConfigName(const char *name)
{
	if (!name || !*name || std::strlen(name) >= MAX_QPATH)
		return false;
	for (const unsigned char *p = reinterpret_cast<const unsigned char *>(name); *p; p++) {
		if (*p <= ' ' || *p == '/' || *p == '\\' || *p == ':' ||
			*p == '"' || *p == ';')
			return false;
	}
	return std::strcmp(name, ".") != 0 && std::strcmp(name, "..") != 0;
}

std::vector<std::string> TokenizeConfig(const std::string &text)
{
	std::vector<std::string> tokens;
	tokens.reserve(std::min<size_t>(text.size() / 6, kMaxConfigTokens));

	for (size_t i = 0; i < text.size() && tokens.size() < kMaxConfigTokens;) {
		const unsigned char c = static_cast<unsigned char>(text[i]);
		if (std::isspace(c)) {
			i++;
			continue;
		}
		if (text[i] == '#' || (text[i] == '/' && i + 1 < text.size() &&
				text[i + 1] == '/')) {
			while (i < text.size() && text[i] != '\n')
				i++;
			continue;
		}
		if (text[i] == '{' || text[i] == '}' ||
			text[i] == ':' || text[i] == ';') {
			tokens.emplace_back(1, text[i++]);
			continue;
		}
		if (text[i] == '"') {
			i++;
			std::string token;
			while (i < text.size() && text[i] != '"') {
				if (text[i] == '\\' && i + 1 < text.size() &&
					(text[i + 1] == '"' || text[i + 1] == '\\'))
					i++;
				token.push_back(text[i++]);
			}
			if (i < text.size())
				i++;
			tokens.push_back(std::move(token));
			continue;
		}

		const size_t begin = i;
		while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i])) &&
			text[i] != '{' && text[i] != '}' && text[i] != ':' &&
			text[i] != ';' && text[i] != '#')
			i++;
		if (i > begin)
			tokens.emplace_back(text.substr(begin, i - begin));
	}
	return tokens;
}

void ParseConfigBlock(const std::vector<std::string> &tokens, size_t &cursor,
	std::vector<ConfigOp> &ops, bool map_matches, int arena_id, int specificity)
{
	while (cursor < tokens.size()) {
		std::string token = tokens[cursor++];
		if (token == "}")
			return;
		const std::string lowered = Lower(token);

		if (lowered == "map" && cursor < tokens.size()) {
			const std::string map_name = tokens[cursor++];
			if (cursor < tokens.size() && tokens[cursor] == "{") {
				cursor++;
				ParseConfigBlock(tokens, cursor, ops,
					!Q_strcasecmp(map_name.c_str(), level.mapname), 0, 1);
			}
			continue;
		}

		if (lowered == "arena" && cursor + 1 < tokens.size()) {
			int selected = 0;
			if (ParseInt(tokens[cursor], selected) && tokens[cursor + 1] == "{") {
				cursor += 2;
				ParseConfigBlock(tokens, cursor, ops, map_matches, selected, 2);
				continue;
			}
		}

		if (cursor < tokens.size() && tokens[cursor] == "{") {
			cursor++;
			int numeric = 0;
			if (ParseInt(token, numeric))
				ParseConfigBlock(tokens, cursor, ops, map_matches, numeric, 2);
			else if (lowered == "global" || lowered == "default" ||
				lowered == "defaults")
				ParseConfigBlock(tokens, cursor, ops, true, 0, 0);
			else
				ParseConfigBlock(tokens, cursor, ops,
					!Q_strcasecmp(token.c_str(), level.mapname), 0, 1);
			continue;
		}

		if (cursor >= tokens.size() || tokens[cursor] == "}")
			continue;

		std::string value;
		std::string key = std::move(token);
		size_t colon = cursor;
		if (IsConfigSettingName(lowered) && tokens[cursor] != ":")
			colon = tokens.size();
		else
			while (colon < tokens.size() && tokens[colon] != ":" &&
				tokens[colon] != ";" && tokens[colon] != "}" &&
				tokens[colon] != "{")
				colon++;
		const bool colon_syntax = colon < tokens.size() && tokens[colon] == ":";
		if (colon_syntax) {
			// Classic RA2 treats every word before ':' as a possible key
			// alias. Arena display names are written directly before the
			// first setting, so the final alias is the actual setting name.
			if (colon > cursor)
				key = tokens[colon - 1];
			cursor = colon + 1;
			while (cursor < tokens.size() && tokens[cursor] != ";" &&
				tokens[cursor] != "}") {
				if (!value.empty())
					value.push_back(' ');
				value += tokens[cursor++];
			}
			if (cursor < tokens.size() && tokens[cursor] == ";")
				cursor++;
		} else {
			if (Lower(key) == "weapons") {
				while (cursor < tokens.size() && tokens[cursor] != ";" &&
					tokens[cursor] != "}" && tokens[cursor] != "{") {
					const std::string &part = tokens[cursor];
					if (part.empty() || !std::all_of(part.begin(), part.end(),
						[](unsigned char c) { return std::isdigit(c) != 0; }))
						break;
					if (!value.empty())
						value.push_back(' ');
					value += part;
					cursor++;
				}
			} else
				value = tokens[cursor++];
			if (cursor < tokens.size() && tokens[cursor] == ";")
				cursor++;
		}
		if (value.empty())
			continue;
		if (map_matches)
			ops.push_back({ specificity, arena_id, std::move(key), std::move(value) });
	}
}

void LoadArenaConfig(const mm_arena_settings_t &base)
{
	for (int id = 1; id <= s_arena_count; id++)
		s_arenas[id].defaults = s_arenas[id].settings = base;
	const auto finalize_defaults = [] {
		for (int id = 1; id <= s_arena_count; id++) {
			FinalizeSettings(s_arenas[id].defaults,
				static_cast<int>(game.maxclients));
			s_arenas[id].settings = s_arenas[id].defaults;
		}
	};

	const char *config_name = s_config ? s_config->string : "arena.cfg";
	if (!IsSafeConfigName(config_name)) {
		gi.Com_PrintFmt("Rocket Arena: rejecting unsafe config name \"{}\".\n",
			config_name ? config_name : "");
		finalize_defaults();
		return;
	}

	cvar_t *base_dir_cvar = gi.cvar("basedir", ".", CVAR_NOFLAGS);
	const char *base_dir = base_dir_cvar && base_dir_cvar->string &&
		*base_dir_cvar->string ? base_dir_cvar->string : ".";
	const std::string path =
		(std::filesystem::path(base_dir) / "baseq2" / config_name).string();
	FILE *file = std::fopen(path.c_str(), "rb");
	if (!file) {
		gi.Com_PrintFmt(
			"Rocket Arena: could not open config {}; using defaults.\n", path);
		finalize_defaults();
		return;
	}

	std::fseek(file, 0, SEEK_END);
	const long length = std::ftell(file);
	std::rewind(file);
	if (length <= 0 || length > kMaxConfigBytes) {
		std::fclose(file);
		gi.Com_PrintFmt("Rocket Arena: ignoring invalid config size for {}.\n", path);
		finalize_defaults();
		return;
	}

	std::string text(static_cast<size_t>(length), '\0');
	const size_t read = std::fread(text.data(), 1, text.size(), file);
	std::fclose(file);
	text.resize(read);

	const auto tokens = TokenizeConfig(text);
	if (!MM_ArenaConfigBracesBalanced(tokens)) {
		gi.Com_PrintFmt(
			"Rocket Arena: rejecting config with unbalanced braces: {}.\n",
			path);
		finalize_defaults();
		return;
	}
	std::vector<ConfigOp> ops;
	size_t cursor = 0;
	ParseConfigBlock(tokens, cursor, ops, true, 0, 0);

	for (int pass = 0; pass <= 2; pass++) {
		for (const ConfigOp &op : ops) {
			if (op.specificity != pass)
				continue;
			if (pass == 2) {
				Arena *arena = FindArena(op.arena_id);
				if (arena)
					ApplySetting(arena->defaults, op.key, op.value,
						static_cast<int>(game.maxclients));
				continue;
			}
			for (int id = 1; id <= s_arena_count; id++)
				ApplySetting(s_arenas[id].defaults, op.key, op.value,
					static_cast<int>(game.maxclients));
		}
	}

	finalize_defaults();
	gi.Com_PrintFmt("Rocket Arena: loaded {} layered settings from {}.\n",
		ops.size(), path);
}

void DiscoverArenas()
{
	s_arena_count = s_map_contract.declared_rooms;

	for (int id = 1; id <= s_arena_count; id++) {
		Arena &arena = s_arenas[id];
		arena.valid = true;
		arena.id = id;
		arena.name = fmt::format("Arena {}", id);
	}

	for (size_t i = 1; i < globals.num_entities; i++) {
		const gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->classname)
			continue;
		if (ent->arena > 0 &&
			!std::strcmp(ent->classname, "misc_teleporter"))
			s_classic_ra2_map = true;
		if (!ent->message ||
			std::strcmp(ent->classname, "info_player_intermission"))
			continue;
		const int id = ent->arena;
		Arena *arena = FindArena(id);
		if (!arena)
			continue;
		const std::string name = SafeText(ent->message, 56);
		if (!name.empty() && arena->name == fmt::format("Arena {}", id))
			arena->name = name;
	}
}

uint16_t AllocateTeam()
{
	for (uint16_t id = 1; id <= kMaxLogicalTeams; id++) {
		if (s_teams[id].valid)
			continue;
		s_teams[id] = {};
		s_teams[id].valid = true;
		s_teams[id].id = id;
		return id;
	}
	return 0;
}

uint32_t NextQueueOrder(Arena &arena)
{
	if (!arena.next_queue_order)
		arena.next_queue_order = 1;
	const uint32_t result = arena.next_queue_order++;
	if (!arena.next_queue_order)
		arena.next_queue_order = 1;
	return result;
}

uint16_t CreateLogicalTeam(Arena &arena, std::string_view requested_name,
	bool fixed, team_t side)
{
	const uint16_t id = AllocateTeam();
	if (!id)
		return 0;
	LogicalTeam &team = s_teams[id];
	team.arena_id = arena.id;
	team.fixed = fixed;
	team.side = side;
	team.name = SafeText(requested_name, 30);
	if (team.name.empty())
		team.name = fmt::format("Team {}", id);
	team.queue_order = NextQueueOrder(arena);
	return id;
}

void EnsureFixedTeams(Arena &arena)
{
	if (!MM_ArenaUsesFixedTeams(arena.settings.type)) {
		arena.fixed_red = arena.fixed_blue = 0;
		return;
	}
	if (!FindTeam(arena.fixed_red)) {
		arena.fixed_red = CreateLogicalTeam(arena, "Red", true, TEAM_RED);
		if (LogicalTeam *team = FindTeam(arena.fixed_red))
			team->captain = -1;
	}
	if (!FindTeam(arena.fixed_blue)) {
		arena.fixed_blue = CreateLogicalTeam(arena, "Blue", true, TEAM_BLUE);
		if (LogicalTeam *team = FindTeam(arena.fixed_blue))
			team->captain = -1;
	}
	arena.active_red = arena.fixed_red;
	arena.active_blue = arena.fixed_blue;
}

void ValidateCaptain(LogicalTeam &team)
{
	const auto members = TeamMembers(team.id);
	if (members.empty()) {
		team.captain = -1;
		return;
	}
	if (team.captain >= 0) {
		gentity_t *captain = ClientEntity(team.captain);
		if (IsConnected(captain) && IsTeamMember(captain->client, team.id) &&
			MM_CaptainEligible(captain))
			return;
	}
	team.captain = -1;
	for (gentity_t *member : members) {
		if (!MM_CaptainEligible(member))
			continue;
		team.captain = ClientNumber(member);
		break;
	}
}

void ClearSpectatorInvites(LogicalTeam &team)
{
	for (uint8_t &invite : team.invites)
		invite = MM_ArenaClearSpectatorInviteBits(invite);
}

void ProjectRole(gentity_t *ent, mm_arena_role_t role, team_t side,
	bool respawn_now);

void DestroyEmptyTeams(uint16_t transfer_destination = 0)
{
	for (uint16_t id = 1; id <= kMaxLogicalTeams; id++) {
		LogicalTeam &team = s_teams[id];
		if (!team.valid || team.fixed)
			continue;
		const int member_count = MemberCount(id);
		if (MM_ArenaShouldDestroyEmptyTeam(member_count,
			id == transfer_destination)) {
			for (gentity_t *ent : TeamMembers(id, true)) {
				ent->client->resp.arena_team_id = 0;
				ProjectRole(ent, mm_arena_role_t::Observer,
					TEAM_SPECTATOR, false);
			}
			for (int arena_id = 1; arena_id <= s_arena_count; arena_id++) {
				Arena &arena = s_arenas[arena_id];
				if (arena.active_red == id)
					arena.active_red = 0;
				if (arena.active_blue == id)
					arena.active_blue = 0;
				if (arena.series_winner == id)
					arena.series_winner = 0;
			}
			team = {};
		} else if (member_count > 0)
			ValidateCaptain(team);
	}
}

bool InternalSetTeam(gentity_t *ent, team_t side, bool silent = true)
{
	if (!IsConnected(ent))
		return false;
	if (ent->client->sess.team == side)
		return true;
	const bool previous = s_internal_team_change;
	s_internal_team_change = true;
	const bool result = SetTeam(ent, side, false, true, silent);
	s_internal_team_change = previous;
	return result;
}

void ProjectRole(gentity_t *ent, mm_arena_role_t role, team_t side,
	bool respawn_now)
{
	if (!IsConnected(ent))
		return;
	gclient_t *client = ent->client;
	SetRoleField(client, role);
	client->resp.arena_side = side;
	client->eliminated = role != mm_arena_role_t::Fighter;
	const team_t engine_team = role == mm_arena_role_t::Fighter ? side : TEAM_SPECTATOR;
	const bool changed = client->sess.team != engine_team;
	InternalSetTeam(ent, engine_team);
	// InternalSetTeam can return early when the projected side is unchanged.
	// Publish unconditionally so KEX presentation never retains a prior side.
	P_PublishEngineTeam(ent);
	if (respawn_now && !changed)
		ClientRespawn(ent);
}

void SetTeamRole(uint16_t team_id, mm_arena_role_t role, team_t side,
	bool respawn_now)
{
	const LogicalTeam *team = FindTeam(team_id, 0);
	const Arena *arena = team ? FindArena(team->arena_id, 0) : nullptr;
	for (gentity_t *ent : TeamMembers(team_id)) {
		if (role == mm_arena_role_t::Fighter &&
			ent->client->resp.arena_late_join)
			continue;
		if (role == mm_arena_role_t::Fighter && arena &&
			!PingAllowed(ent, *arena)) {
			ProjectRole(ent, mm_arena_role_t::Observer,
				TEAM_SPECTATOR, false);
			gi.Client_Print(ent, PRINT_HIGH,
				"Your current ping is outside this arena's limits.\n");
			continue;
		}
		if (!ent->client->resp.arena_line_enabled && role == mm_arena_role_t::Fighter)
			ProjectRole(ent, mm_arena_role_t::Observer, TEAM_SPECTATOR, false);
		else
			ProjectRole(ent, role, side, respawn_now);
	}
}

void SetQueuedRoles(Arena &arena)
{
	for (gentity_t *ent : active_clients()) {
		if (!IsConnected(ent) || ent->client->resp.arena_id != arena.id)
			continue;
		const uint16_t team_id = ent->client->resp.arena_team_id;
		if (!team_id) {
			if (Role(ent->client) != mm_arena_role_t::Coach)
				ProjectRole(ent, mm_arena_role_t::Observer, TEAM_SPECTATOR, false);
			continue;
		}
		if (Role(ent->client) == mm_arena_role_t::Coach)
			continue;
		ProjectRole(ent,
			ent->client->resp.arena_line_enabled
				? mm_arena_role_t::Queued : mm_arena_role_t::Observer,
			TEAM_SPECTATOR, false);
	}
}

bool EnrollPracticeFighter(gentity_t *ent, Arena &arena, bool respawn_now)
{
	if (!IsConnected(ent) ||
		ent->client->resp.arena_id != arena.id ||
		!MM_ArenaShouldAutoEnrollPractice(
			arena.settings.type, Role(ent->client)))
		return false;

	gclient_t *client = ent->client;
	const bool had_logical_team = client->resp.arena_team_id != 0;
	if (had_logical_team)
		client->resp.arena_team_id = 0;
	client->resp.arena_line_enabled = true;
	client->resp.arena_late_join = false;
	client->resp.ready = false;
	ClearPendingClientEvents(ClientNumber(ent));

	const bool needs_projection =
		Role(client) != mm_arena_role_t::Fighter ||
		client->sess.team != TEAM_FREE ||
		client->resp.arena_side != TEAM_FREE ||
		client->eliminated || ent->deadflag || ent->health <= 0;
	if (needs_projection || respawn_now)
		ProjectRole(ent, mm_arena_role_t::Fighter, TEAM_FREE, true);
	client->eliminated = false;

	if (had_logical_team)
		DestroyEmptyTeams();
	return true;
}

void ArenaPrint(const Arena &arena, print_type_t level_, std::string_view text,
	bool center = false)
{
	const std::string message(text);
	for (gentity_t *ent : active_clients()) {
		if (!IsConnected(ent) || ent->client->resp.arena_id != arena.id)
			continue;
		if (center)
			gi.Center_Print(ent, message.c_str());
		else
			gi.Client_Print(ent, level_, message.c_str());
	}
}

bool TeamPingsAllowed(const Arena &arena, uint16_t team_id)
{
	for (gentity_t *ent : TeamMembers(team_id))
		if (ent->client->resp.arena_line_enabled &&
			!PingAllowed(ent, arena))
			return false;
	return true;
}

int EligibleMemberCount(const Arena &arena, uint16_t team_id)
{
	const LogicalTeam *team = FindTeam(team_id, 0);
	if (!team || team->arena_id != arena.id)
		return 0;
	int count = 0;
	for (gentity_t *ent : TeamMembers(team_id)) {
		if (!ent->client->resp.arena_line_enabled ||
			ent->client->resp.arena_late_join)
			continue;
		count++;
	}
	return count;
}

bool TeamEligible(const Arena &arena, uint16_t team_id)
{
	if (!TeamPingsAllowed(arena, team_id))
		return false;
	const int count = EligibleMemberCount(arena, team_id);
	if (MM_ArenaUsesFixedTeams(arena.settings.type))
		return MM_ArenaTeamEligibleForType(arena.settings.type, count,
			arena.settings.players_per_team);
	if (arena.settings.type == mm_arena_type_t::Practice)
		return count > 0;
	return MM_ArenaTeamEligible(count, arena.settings.players_per_team);
}

uint16_t OldestEligibleTeam(const Arena &arena, uint16_t exclude_a = 0,
	uint16_t exclude_b = 0)
{
	const LogicalTeam *best = nullptr;
	for (uint16_t id = 1; id <= kMaxLogicalTeams; id++) {
		const LogicalTeam &team = s_teams[id];
		if (!team.valid || team.fixed || team.arena_id != arena.id ||
			id == exclude_a || id == exclude_b || !TeamEligible(arena, id))
			continue;
		if (!best || team.queue_order < best->queue_order ||
			(team.queue_order == best->queue_order && team.id < best->id))
			best = &team;
	}
	return best ? best->id : 0;
}

void EnsurePairing(Arena &arena)
{
	EnsureFixedTeams(arena);
	if (MM_ArenaUsesFixedTeams(arena.settings.type))
		return;
	if (arena.settings.type == mm_arena_type_t::Practice)
		return;

	if (!TeamEligible(arena, arena.active_red))
		arena.active_red = 0;
	if (!TeamEligible(arena, arena.active_blue))
		arena.active_blue = 0;
	if (!arena.settings.unbalanced) {
		const auto team_size = [&arena](uint16_t team_id) {
			return EligibleMemberCount(arena, team_id);
		};
		if (arena.active_red && arena.active_blue &&
			team_size(arena.active_red) == team_size(arena.active_blue))
			return;

		std::vector<const LogicalTeam *> eligible;
		for (uint16_t id = 1; id <= kMaxLogicalTeams; id++) {
			const LogicalTeam &team = s_teams[id];
			if (team.valid && !team.fixed && team.arena_id == arena.id &&
				TeamEligible(arena, id))
				eligible.push_back(&team);
		}
		std::sort(eligible.begin(), eligible.end(),
			[](const LogicalTeam *a, const LogicalTeam *b) {
				return a->queue_order < b->queue_order ||
					(a->queue_order == b->queue_order && a->id < b->id);
			});

		const auto compatible_with = [&](uint16_t anchor) -> uint16_t {
			if (!anchor)
				return 0;
			const int size = team_size(anchor);
			for (const LogicalTeam *team : eligible)
				if (team->id != anchor && team_size(team->id) == size)
					return team->id;
			return 0;
		};
		if (const uint16_t challenger = compatible_with(arena.active_red)) {
			arena.active_blue = challenger;
			return;
		}
		if (const uint16_t challenger = compatible_with(arena.active_blue)) {
			arena.active_red = arena.active_blue;
			arena.active_blue = challenger;
			return;
		}
		for (size_t i = 0; i < eligible.size(); i++) {
			for (size_t j = i + 1; j < eligible.size(); j++) {
				if (team_size(eligible[i]->id) != team_size(eligible[j]->id))
					continue;
				arena.active_red = eligible[i]->id;
				arena.active_blue = eligible[j]->id;
				return;
			}
		}
		arena.active_red = arena.active_blue = 0;
		return;
	}
	if (!arena.active_red)
		arena.active_red = OldestEligibleTeam(arena, arena.active_blue);
	if (!arena.active_blue)
		arena.active_blue = OldestEligibleTeam(arena, arena.active_red);
}

bool AllReady(const Arena &arena)
{
	if (!arena.settings.competition_mode)
		return true;
	if (!arena.active_red || !arena.active_blue)
		return false;
	for (const uint16_t team_id : { arena.active_red, arena.active_blue }) {
		bool found = false;
		for (gentity_t *ent : TeamMembers(team_id)) {
			if (!ent->client->resp.arena_line_enabled ||
				ent->client->resp.arena_late_join)
				continue;
			if (!PingAllowed(ent, arena))
				continue;
			found = true;
			if (!ent->client->sess.is_a_bot &&
				!(ent->svflags & SVF_BOT) &&
				!ent->client->resp.ready)
				return false;
		}
		if (!found)
			return false;
	}
	return true;
}

int CountdownSeconds()
{
	return MM_ArenaClampCountdown(g_round_countdown ? g_round_countdown->integer : 10);
}

void FreezeFighters(const Arena &arena, bool freeze)
{
	for (gentity_t *ent : active_clients()) {
		if (!IsConnected(ent) || ent->client->resp.arena_id != arena.id ||
			Role(ent->client) != mm_arena_role_t::Fighter)
			continue;
		if (freeze) {
			Weapon_CancelFiring(ent);
			ent->client->ps.pmove.pm_type = PM_FREEZE;
		} else if (!ent->deadflag)
			ent->client->ps.pmove.pm_type = PM_NORMAL;
	}
}

void PrepareRound(Arena &arena)
{
	arena.red_uses_odd_spawns = irandom(2) != 0;
	if (arena.settings.type == mm_arena_type_t::RedRover) {
		std::vector<gentity_t *> participants;
		for (gentity_t *ent : active_clients()) {
			if (IsConnected(ent) && ent->client->resp.arena_id == arena.id &&
				ent->client->resp.arena_team_id &&
				ent->client->resp.arena_line_enabled &&
				PingAllowed(ent, arena) &&
				Role(ent->client) != mm_arena_role_t::Coach)
				participants.push_back(ent);
		}
		for (size_t i = participants.size(); i > 1; i--)
			std::swap(participants[i - 1],
				participants[static_cast<size_t>(irandom(static_cast<int>(i)))]);
		for (size_t i = 0; i < participants.size(); i++) {
			const bool red = (i & 1u) == 0;
			participants[i]->client->resp.arena_team_id =
				red ? arena.fixed_red : arena.fixed_blue;
			participants[i]->client->resp.arena_late_join = false;
			MM_ResetClientScoring(participants[i]->client);
		}
		if (LogicalTeam *red = FindTeam(arena.fixed_red))
			ValidateCaptain(*red);
		if (LogicalTeam *blue = FindTeam(arena.fixed_blue))
			ValidateCaptain(*blue);
	}

	SetQueuedRoles(arena);
	SetTeamRole(arena.active_red, mm_arena_role_t::Fighter, TEAM_RED, true);
	SetTeamRole(arena.active_blue, mm_arena_role_t::Fighter, TEAM_BLUE, true);
	for (gentity_t *ent : active_clients()) {
		if (!IsConnected(ent) || ent->client->resp.arena_id != arena.id)
			continue;
		const int client_num = ClientNumber(ent);
		if (client_num >= 0 && client_num < MAX_CLIENTS) {
			s_rover_respawn_pending[client_num] = false;
			s_rover_respawn_arena[client_num] = 0;
		}
		if (Role(ent->client) == mm_arena_role_t::Fighter) {
			ent->client->resp.arena_late_join = false;
			ent->client->eliminated = false;
		}
	}
	FreezeFighters(arena, true);
}

void BeginCountdown(Arena &arena)
{
	if (arena.state == mm_arena_state_t::RoundOver &&
		arena.settings.type == mm_arena_type_t::ClanArena) {
		for (gentity_t *ent : active_clients())
			if (IsConnected(ent) && ent->client->resp.arena_id == arena.id)
				ent->client->resp.arena_late_join = false;
	}
	if (arena.settings.competition_mode) {
		if (LogicalTeam *team = FindTeam(arena.active_red))
			team->locked = true;
		if (LogicalTeam *team = FindTeam(arena.active_blue))
			team->locked = true;
	}
	PrepareRound(arena);
	if (arena.settings.competition_mode) {
		// Competition mode is eyecam-only. Reset nonfighters to the room's
		// neutral observer spawn before following selects an allowed target.
		for (gentity_t *ent : active_clients()) {
			if (!IsConnected(ent) ||
				ent->client->resp.arena_id != arena.id ||
				Role(ent->client) == mm_arena_role_t::Fighter)
				continue;
			ProjectRole(ent, Role(ent->client), TEAM_SPECTATOR, true);
		}
	}
	const bool first = arena.round == 0 && arena.red_score == 0 && arena.blue_score == 0;
	if (first)
		ResetPlayerScores(arena);
	arena.state = first ? mm_arena_state_t::MatchCountdown
		: mm_arena_state_t::RoundCountdown;
	arena.state_timer = level.time + gtime_t::from_sec(CountdownSeconds());
	ArenaPrint(arena, PRINT_CENTER,
		fmt::format("{}\nMatch begins in {}", arena.name, CountdownSeconds()), true);
}

void BeginFight(Arena &arena)
{
	arena.state = mm_arena_state_t::Running;
	arena.state_timer = roundtimelimit && roundtimelimit->value > 0
		? level.time + gtime_t::from_min(roundtimelimit->value) : gtime_t {};
	FreezeFighters(arena, false);
	ArenaPrint(arena, PRINT_CENTER,
		fmt::format("{}\nFIGHT!", arena.name), true);
}

void EndRound(Arena &arena, mm_arena_round_result_t result)
{
	s_ended_round_this_frame = true;
	FreezeFighters(arena, true);

	if (result == mm_arena_round_result_t::Draw) {
		arena.state = mm_arena_state_t::RoundOver;
		arena.state_timer = level.time + 3_sec;
		ArenaPrint(arena, PRINT_CENTER, "Tie round - replaying the same matchup.", true);
		return;
	}

	arena.round++;
	const bool red_won = result == mm_arena_round_result_t::Red;
	if (red_won)
		arena.red_score++;
	else
		arena.blue_score++;
	const uint16_t winning_team = red_won ? arena.active_red : arena.active_blue;
	const LogicalTeam *team = FindTeam(winning_team, 0);
	ArenaPrint(arena, PRINT_CENTER,
		fmt::format("{} wins round {}!\nScore {} - {}",
			team ? team->name : (red_won ? "Red" : "Blue"),
			arena.round, arena.red_score, arena.blue_score), true);

	const mm_arena_round_result_t series = MM_ArenaSeriesWinner(
		arena.red_score, arena.blue_score, arena.settings.rounds);
	if (series != mm_arena_round_result_t::Ongoing) {
		arena.series_winner = winning_team;
		arena.state = mm_arena_state_t::MatchOver;
		arena.state_timer = level.time + 5_sec;
		ArenaPrint(arena, PRINT_HIGH,
			fmt::format("{} wins the match in {}.\n",
				team ? team->name : "A team", arena.name));
	} else {
		arena.state = mm_arena_state_t::RoundOver;
		arena.state_timer = level.time + 3_sec;
	}
}

void EndRoverRound(Arena &arena, bool time_expired)
{
	s_ended_round_this_frame = true;
	FreezeFighters(arena, true);

	int best_score = std::numeric_limits<int>::min();
	std::vector<gentity_t *> leaders;
	for (gentity_t *ent : active_clients()) {
		if (!IsConnected(ent) || ent->client->resp.arena_id != arena.id ||
			!ent->client->resp.arena_line_enabled ||
			Role(ent->client) == mm_arena_role_t::Coach ||
			(ent->client->resp.arena_team_id != arena.fixed_red &&
			 ent->client->resp.arena_team_id != arena.fixed_blue))
			continue;
		const int score = ent->client->resp.score;
		if (score > best_score) {
			best_score = score;
			leaders.clear();
			leaders.push_back(ent);
		} else if (score == best_score) {
			leaders.push_back(ent);
		}
	}

	std::string names;
	for (gentity_t *leader : leaders) {
		const std::string name = SafeText(leader->client->resp.netname, 30);
		if (names.size() + name.size() + 2 > 500) {
			names += ", ...";
			break;
		}
		if (!names.empty())
			names += ", ";
		names += name;
	}
	if (!leaders.empty()) {
		ArenaPrint(arena, PRINT_CENTER,
			fmt::format("{}\nTop fragger{}: {}\n{} frag{}",
				time_expired ? "Round time expired." : "One side owns the arena!",
				leaders.size() == 1 ? "" : "s", names, best_score,
				best_score == 1 ? "" : "s"), true);
	} else {
		ArenaPrint(arena, PRINT_CENTER, "Red Rover round over.", true);
	}

	arena.round++;
	arena.red_score = arena.blue_score = 0;
	arena.series_winner = 0;
	for (gentity_t *ent : active_clients()) {
		if (!IsConnected(ent) || ent->client->resp.arena_id != arena.id)
			continue;
		MM_ResetClientScoring(ent->client);
	}
	arena.state = mm_arena_state_t::RoundOver;
	arena.state_timer = level.time + 3_sec;
}

void AdmitLateJoiners(Arena &arena)
{
	for (gentity_t *ent : active_clients()) {
		if (IsConnected(ent) && ent->client->resp.arena_id == arena.id) {
			ent->client->resp.arena_late_join = false;
			ent->client->resp.ready = false;
		}
	}
	for (LogicalTeam &team : s_teams)
		if (team.valid && team.arena_id == arena.id)
			team.timeouts_used = 0;
}

void RotateRocketArena(Arena &arena)
{
	AdmitLateJoiners(arena);
	const uint16_t winner = arena.series_winner;
	const uint16_t loser = winner == arena.active_red
		? arena.active_blue : arena.active_red;
	if (LogicalTeam *losing_team = FindTeam(loser)) {
		losing_team->queue_order = NextQueueOrder(arena);
	}
	arena.active_red = TeamEligible(arena, winner) ? winner : 0;
	arena.active_blue = OldestEligibleTeam(arena, arena.active_red);
	arena.red_score = arena.blue_score = arena.round = 0;
	arena.series_winner = 0;
	arena.vote_tries_used.fill(0);
	ResetPlayerScores(arena);
	SetQueuedRoles(arena);
}

void ResetFixedSeries(Arena &arena)
{
	AdmitLateJoiners(arena);
	arena.red_score = arena.blue_score = arena.round = 0;
	arena.series_winner = 0;
	arena.vote_tries_used.fill(0);
	ResetPlayerScores(arena);
	if (arena.settings.type == mm_arena_type_t::RedRover)
		arena.round = 0; // PrepareRound rebalances a fresh Red Rover series.
}

mm_arena_round_result_t RunningResult(Arena &arena)
{
	const int red_members = CurrentRoundMemberCount(arena.active_red);
	const int blue_members = CurrentRoundMemberCount(arena.active_blue);
	if (!red_members && blue_members)
		return mm_arena_round_result_t::Blue;
	if (red_members && !blue_members)
		return mm_arena_round_result_t::Red;
	if (!red_members && !blue_members)
		return mm_arena_round_result_t::Draw;

	bool expired = false;
	if (roundtimelimit && roundtimelimit->value > 0) {
		// A per-arena running deadline is stored in state_timer when configured.
		expired = arena.state_timer && level.time >= arena.state_timer;
	}
	return MM_ArenaResolveRound(
		LivingCount(arena.active_red), LivingCount(arena.active_blue), expired);
}

void ProcessRoverTransfers(Arena &arena)
{
	bool moved = false;
	for (int client_num = 0; client_num < static_cast<int>(game.maxclients) &&
		client_num < MAX_CLIENTS; client_num++) {
		const team_t side = s_rover_pending_side[client_num];
		if ((side != TEAM_RED && side != TEAM_BLUE) ||
			s_rover_pending_arena[client_num] != arena.id)
			continue;
		gentity_t *ent = ClientEntity(client_num);
		s_rover_pending_side[client_num] = TEAM_NONE;
		s_rover_pending_arena[client_num] = 0;
		if (!IsConnected(ent) || ent->client->resp.arena_id != arena.id)
			continue;
		ent->client->resp.arena_team_id =
			side == TEAM_RED ? arena.fixed_red : arena.fixed_blue;
		ent->client->resp.arena_side = side;
		SetRoleField(ent->client, mm_arena_role_t::Fighter);
		ent->client->resp.arena_late_join = false;
		P_PublishEngineTeam(ent);
		moved = true;
		s_rover_respawn_pending[client_num] = true;
		s_rover_respawn_arena[client_num] =
			static_cast<int16_t>(arena.id);
		gi.Client_Print(ent, PRINT_CENTER,
			side == TEAM_RED ? "You are now on the Red team!\n"
				: "You are now on the Blue team!\n");
	}
	if (moved) {
		if (LogicalTeam *red = FindTeam(arena.fixed_red))
			ValidateCaptain(*red);
		if (LogicalTeam *blue = FindTeam(arena.fixed_blue))
			ValidateCaptain(*blue);
	}
}

void RespawnRoverTransfers(Arena &arena)
{
	if (arena.state != mm_arena_state_t::Running)
		return;
	for (int client_num = 0; client_num < static_cast<int>(game.maxclients) &&
		client_num < MAX_CLIENTS; client_num++) {
		if (!s_rover_respawn_pending[client_num] ||
			s_rover_respawn_arena[client_num] != arena.id)
			continue;
		gentity_t *ent = ClientEntity(client_num);
		s_rover_respawn_pending[client_num] = false;
		s_rover_respawn_arena[client_num] = 0;
		if (!IsConnected(ent) || ent->client->resp.arena_id != arena.id)
			continue;
		const team_t side = ent->client->resp.arena_side;
		ProjectRole(ent, mm_arena_role_t::Fighter, side, true);
		ent->client->eliminated = false;
	}
}

void ResetEmptyArena(Arena &arena)
{
	ClearArenaPendingEvents(arena.id);
	arena.settings = arena.defaults;
	arena.state = mm_arena_state_t::Empty;
	arena.state_before_pause = mm_arena_state_t::Empty;
	arena.state_timer = {};
	arena.paused_remaining = {};
	arena.resume_countdown = false;
	arena.timeout_team = 0;
	arena.active_red = arena.active_blue = 0;
	arena.fixed_red = arena.fixed_blue = 0;
	arena.series_winner = 0;
	arena.red_score = arena.blue_score = arena.round = 0;
	arena.settings_pending = false;
	arena.vote_tries_used.fill(0);
	arena.ballot = {};
	arena.occupied = false;
	for (uint16_t id = 1; id <= kMaxLogicalTeams; id++) {
		if (s_teams[id].valid && s_teams[id].arena_id == arena.id)
			s_teams[id] = {};
	}
}

void ApplyPendingSettings(Arena &arena);

void ResetArenaSeries(Arena &arena)
{
	ClearArenaPendingEvents(arena.id);
	ApplyPendingSettings(arena);
	AdmitLateJoiners(arena);
	arena.red_score = arena.blue_score = arena.round = 0;
	arena.series_winner = 0;
	arena.state = ArenaPopulation(arena.id)
		? mm_arena_state_t::Warmup : mm_arena_state_t::Empty;
	arena.state_timer = {};
	arena.state_before_pause = arena.state;
	arena.paused_remaining = {};
	arena.resume_countdown = false;
	arena.timeout_team = 0;
	arena.vote_tries_used.fill(0);
	arena.ballot = {};
	ResetPlayerScores(arena);
	SetQueuedRoles(arena);
}

bool ForceStart(Arena &arena, gentity_t *admin)
{
	if (arena.settings.type == mm_arena_type_t::Practice) {
		gi.Client_Print(admin, PRINT_HIGH,
			"Practice arenas run continuously.\n");
		return false;
	}
	if (IsActiveSeriesState(arena.state) ||
		arena.state == mm_arena_state_t::MatchOver) {
		gi.Client_Print(admin, PRINT_HIGH,
			"This arena match has already started.\n");
		return false;
	}
	EnsurePairing(arena);
	const bool valid_pair = arena.active_red && arena.active_blue &&
		TeamEligible(arena, arena.active_red) &&
		TeamEligible(arena, arena.active_blue) &&
		(arena.settings.unbalanced ||
			EligibleMemberCount(arena, arena.active_red) ==
				EligibleMemberCount(arena, arena.active_blue));
	if (!valid_pair) {
		gi.Client_Print(admin, PRINT_HIGH,
			"This arena does not have a valid matchup to start.\n");
		return false;
	}
	BeginCountdown(arena);
	ArenaPrint(arena, PRINT_HIGH,
		fmt::format("[ADMIN]: {} forced this arena match to start.\n",
			admin->client->resp.netname));
	return true;
}

int EligibleVoters(const Arena &arena)
{
	int count = 0;
	for (gentity_t *ent : active_clients())
		if (IsConnected(ent) && ent->client->resp.arena_id == arena.id &&
			MM_VoteClientEligible(ent))
			count++;
	return count;
}

void ApplyResolvedSettings(Arena &arena, const mm_arena_settings_t &settings)
{
	const bool type_changed = arena.settings.type != settings.type;
	const bool competition_ended =
		arena.settings.competition_mode && !settings.competition_mode;
	arena.settings = settings;
	if (competition_ended) {
		for (LogicalTeam &team : s_teams) {
			if (!team.valid || team.arena_id != arena.id)
				continue;
			team.locked = false;
			team.password.clear();
			team.chat_muted = false;
			team.timeouts_used = 0;
			team.invites.fill(0);
		}
		for (gentity_t *ent : active_clients()) {
			if (!IsConnected(ent) ||
				ent->client->resp.arena_id != arena.id)
				continue;
			ent->client->resp.ready = false;
			if (Role(ent->client) != mm_arena_role_t::Coach)
				continue;
			ent->client->resp.arena_team_id = 0;
			ent->client->resp.arena_line_enabled = false;
			ProjectRole(ent, mm_arena_role_t::Observer,
				TEAM_SPECTATOR, false);
		}
	}
	if (!type_changed)
		return;

	ClearArenaPendingEvents(arena.id);
	for (gentity_t *ent : active_clients()) {
		if (!IsConnected(ent) || ent->client->resp.arena_id != arena.id)
			continue;
		ent->client->resp.arena_team_id = 0;
		ent->client->resp.ready = false;
		ent->client->resp.arena_late_join = false;
		ProjectRole(ent, mm_arena_role_t::Observer, TEAM_SPECTATOR, false);
	}
	for (uint16_t id = 1; id <= kMaxLogicalTeams; id++)
		if (s_teams[id].valid && s_teams[id].arena_id == arena.id)
			s_teams[id] = {};
	ResetPlayerScores(arena);
	arena.active_red = arena.active_blue = 0;
	arena.fixed_red = arena.fixed_blue = 0;
	arena.series_winner = 0;
	arena.red_score = arena.blue_score = arena.round = 0;
	arena.state = mm_arena_state_t::Warmup;
	arena.state_before_pause = mm_arena_state_t::Warmup;
	arena.state_timer = {};
	arena.paused_remaining = {};
	arena.resume_countdown = false;
	arena.timeout_team = 0;
	arena.settings_pending = false;
	arena.vote_tries_used.fill(0);
	arena.ballot = {};
	EnsureFixedTeams(arena);
}

void ApplyPendingSettings(Arena &arena)
{
	if (!arena.settings_pending)
		return;
	const mm_arena_settings_t settings = arena.pending_settings;
	arena.settings_pending = false;
	ApplyResolvedSettings(arena, settings);
	ArenaPrint(arena, PRINT_HIGH, "Deferred arena settings are now active.\n");
}

void FinishBallot(Arena &arena, bool passed)
{
	// RA3's per-player limit counts proposals made without a success. A
	// successful room vote restores every participant's allowance.
	MM_ArenaResolveVoteTries(arena.vote_tries_used, passed);
	if (passed) {
		ArenaPrint(arena, PRINT_HIGH,
			fmt::format("Arena vote passed: {}\n", arena.ballot.description));
		const bool combat_change =
			arena.ballot.setting_flag != MM_ARENA_VOTE_LOCK;
		const bool match_active =
			arena.state == mm_arena_state_t::MatchCountdown ||
			arena.state == mm_arena_state_t::RoundCountdown ||
			arena.state == mm_arena_state_t::Running ||
			arena.state == mm_arena_state_t::RoundOver ||
			arena.state == mm_arena_state_t::Paused;
		mm_arena_settings_t resolved = combat_change && match_active &&
			arena.settings.type != mm_arena_type_t::Practice
			? MM_ArenaProposalBase(arena.settings, arena.pending_settings,
				arena.settings_pending)
			: arena.settings;
		if (!ApplySetting(resolved, arena.ballot.setting_key,
			arena.ballot.setting_value, static_cast<int>(game.maxclients))) {
			ArenaPrint(arena, PRINT_HIGH,
				"Arena vote result could not be applied.\n");
			arena.ballot = {};
			return;
		}
		FinalizeSettings(resolved, static_cast<int>(game.maxclients));
		if (combat_change && match_active &&
			arena.settings.type != mm_arena_type_t::Practice) {
			arena.settings_pending = true;
			arena.pending_settings = resolved;
			ArenaPrint(arena, PRINT_HIGH,
				"The change will apply after the current match.\n");
		} else {
			ApplyResolvedSettings(arena, resolved);
		}
	} else {
		ArenaPrint(arena, PRINT_HIGH,
			fmt::format("Arena vote failed: {}\n", arena.ballot.description));
	}
	arena.ballot = {};
	P_Menu_Dirty();
}

void TickBallot(Arena &arena)
{
	if (!arena.ballot.active || level.time < arena.ballot.expires)
		return;
	int yes = 0;
	int no = 0;
	for (int client_num = 0; client_num < static_cast<int>(game.maxclients) &&
		client_num < MAX_CLIENTS; client_num++) {
		if (!arena.ballot.eligible[client_num])
			continue;
		const int vote = arena.ballot.votes[client_num];
		if (vote > 0)
			yes++;
		else if (vote < 0)
			no++;
	}
	const bool lock_vote = arena.ballot.setting_flag == MM_ARENA_VOTE_LOCK;
	const bool passed = lock_vote
		? MM_ArenaLockVotePassesAtExpiry(yes, no,
			arena.ballot.original_voters, arena.settings.lock_count)
		: MM_ArenaVotePassesAtExpiry(
			yes, no, arena.ballot.original_voters);
	FinishBallot(arena, passed);
}

void ResumeArena(Arena &arena)
{
	arena.state = arena.state_before_pause;
	arena.state_timer = arena.paused_remaining
		? level.time + arena.paused_remaining : gtime_t {};
	arena.resume_countdown = false;
	arena.timeout_team = 0;
	FreezeFighters(arena, arena.state != mm_arena_state_t::Running);
	ArenaPrint(arena, PRINT_CENTER, "FIGHT!", true);
}

void BeginResumeCountdown(Arena &arena);

void TickArena(Arena &arena)
{
	TickBallot(arena);
	const int population = ArenaPopulation(arena.id);
	if (!population) {
		if (arena.occupied)
			ResetEmptyArena(arena);
		return;
	}
	arena.occupied = true;
	if (arena.settings_pending &&
		(arena.state == mm_arena_state_t::Empty ||
		 arena.state == mm_arena_state_t::Warmup))
		ApplyPendingSettings(arena);
	DestroyEmptyTeams();
	EnsureFixedTeams(arena);

	if (arena.settings.type == mm_arena_type_t::Practice) {
		arena.state = mm_arena_state_t::Running;
		for (int client_num = 0;
			client_num < static_cast<int>(game.maxclients) &&
			client_num < MAX_CLIENTS; client_num++) {
			if (!s_practice_respawn_pending[client_num] ||
				s_practice_respawn_arena[client_num] != arena.id)
				continue;
			gentity_t *ent = ClientEntity(client_num);
			s_practice_respawn_pending[client_num] = false;
			s_practice_respawn_arena[client_num] = 0;
			if (!IsConnected(ent) || ent->client->resp.arena_id != arena.id ||
				Role(ent->client) != mm_arena_role_t::Fighter)
				continue;
			ProjectRole(ent, mm_arena_role_t::Fighter, TEAM_FREE, true);
			ent->client->eliminated = false;
		}
		for (gentity_t *ent : active_clients()) {
			if (!IsConnected(ent) || ent->client->resp.arena_id != arena.id ||
				Role(ent->client) == mm_arena_role_t::Coach)
				continue;
			EnrollPracticeFighter(ent, arena, false);
		}
		return;
	}

	if (arena.settings.type == mm_arena_type_t::RedRover)
		ProcessRoverTransfers(arena);
	if (arena.state == mm_arena_state_t::Empty ||
		arena.state == mm_arena_state_t::Warmup)
		EnsurePairing(arena);

	if (arena.state == mm_arena_state_t::Paused) {
		FreezeFighters(arena, true);
		if (level.time >= arena.state_timer) {
			if (arena.resume_countdown)
				ResumeArena(arena);
			else
				BeginResumeCountdown(arena);
		}
		return;
	}

	const bool pairing_phase = arena.state == mm_arena_state_t::Empty ||
		arena.state == mm_arena_state_t::Warmup ||
		arena.state == mm_arena_state_t::MatchCountdown ||
		arena.state == mm_arena_state_t::RoundCountdown;
	const bool rover_pairing = arena.settings.type == mm_arena_type_t::RedRover;
	const bool unequal_sides = pairing_phase && !rover_pairing &&
		!arena.settings.unbalanced &&
		arena.active_red && arena.active_blue &&
		EligibleMemberCount(arena, arena.active_red) !=
			EligibleMemberCount(arena, arena.active_blue);
	const bool rover_pair_unavailable = rover_pairing &&
		(!TeamPingsAllowed(arena, arena.active_red) ||
		 !TeamPingsAllowed(arena, arena.active_blue) ||
		 EligibleMemberCount(arena, arena.active_red) +
			EligibleMemberCount(arena, arena.active_blue) < 2);
	if (pairing_phase && (rover_pair_unavailable ||
		(!rover_pairing && (!arena.active_red || !arena.active_blue ||
			unequal_sides || !TeamEligible(arena, arena.active_red) ||
			!TeamEligible(arena, arena.active_blue))))) {
		if (arena.state == mm_arena_state_t::MatchCountdown ||
			arena.state == mm_arena_state_t::RoundCountdown) {
			AdmitLateJoiners(arena);
			arena.red_score = arena.blue_score = arena.round = 0;
			arena.series_winner = 0;
		}
		arena.state = mm_arena_state_t::Warmup;
		arena.state_timer = {};
		SetQueuedRoles(arena);
		return;
	}

	switch (arena.state) {
	case mm_arena_state_t::Empty:
	case mm_arena_state_t::Warmup:
		if (AllReady(arena))
			BeginCountdown(arena);
		break;
	case mm_arena_state_t::MatchCountdown:
	case mm_arena_state_t::RoundCountdown:
		FreezeFighters(arena, true);
		if (level.time >= arena.state_timer)
			BeginFight(arena);
		break;
	case mm_arena_state_t::Running:
	{
		if (arena.settings.type == mm_arena_type_t::RedRover) {
			const int red = CurrentRoundMemberCount(arena.active_red);
			const int blue = CurrentRoundMemberCount(arena.active_blue);
			if (!red && !blue) {
				ArenaPrint(arena, PRINT_HIGH,
					"Red Rover round cancelled: no fighters remain.\n");
				ResetPlayerScores(arena);
				arena.state = mm_arena_state_t::Warmup;
				arena.state_timer = {};
				SetQueuedRoles(arena);
				break;
			}
			const bool exhausted = !red || !blue;
			const bool expired = roundtimelimit && roundtimelimit->value > 0 &&
				arena.state_timer && level.time >= arena.state_timer;
			if (exhausted || expired)
				EndRoverRound(arena, expired);
			else
				RespawnRoverTransfers(arena);
			break;
		}
		const mm_arena_round_result_t result = RunningResult(arena);
		if (result != mm_arena_round_result_t::Ongoing)
			EndRound(arena, result);
		break;
	}
	case mm_arena_state_t::RoundOver:
		if (level.time >= arena.state_timer) {
			if (arena.settings.type == mm_arena_type_t::RedRover &&
				arena.settings_pending) {
				ApplyPendingSettings(arena);
				if (arena.settings.type != mm_arena_type_t::RedRover)
					break;
			}
			BeginCountdown(arena);
		}
		break;
	case mm_arena_state_t::MatchOver:
		if (level.time < arena.state_timer)
			break;
	{
		const mm_arena_type_t completed_type = arena.settings.type;
		ApplyPendingSettings(arena);
		if (arena.settings.type != completed_type)
			break;
		if (MM_ArenaWinnerStays(arena.settings.type))
			RotateRocketArena(arena);
		else
			ResetFixedSeries(arena);
		arena.state = mm_arena_state_t::Warmup;
		break;
	}
	case mm_arena_state_t::Paused:
		break;
	}
}

void LeaveTeam(gentity_t *ent, bool to_lobby, bool silent,
	uint16_t transfer_destination = 0)
{
	if (!IsConnected(ent))
		return;
	gclient_t *client = ent->client;
	ClearPendingClientEvents(ClientNumber(ent));
	const uint16_t old_team = client->resp.arena_team_id;
	client->resp.arena_team_id = 0;
	client->resp.ready = false;
	client->resp.arena_late_join = false;
	client->resp.arena_side = TEAM_SPECTATOR;
	if (to_lobby)
		client->resp.arena_id = 0;
	MM_ResetClientScoring(client);
	ProjectRole(ent, to_lobby ? mm_arena_role_t::Lobby : mm_arena_role_t::Observer,
		TEAM_SPECTATOR, true);
	if (LogicalTeam *team = FindTeam(old_team)) {
		ClearSpectatorInvites(*team);
		if (team->captain == ClientNumber(ent))
			team->captain = -1;
		ValidateCaptain(*team);
	}
	DestroyEmptyTeams(transfer_destination);
	if (!silent)
		gi.Client_Print(ent, PRINT_HIGH,
			to_lobby ? "You left Rocket Arena.\n" : "You left your Arena team.\n");
	P_Menu_Dirty();
}

bool HasArenaInvite(const gentity_t *ent, const Arena &arena);
bool ArenaEntryLocked(const gentity_t *ent, const Arena &arena);

bool JoinLogicalTeam(gentity_t *ent, LogicalTeam &team, const char *password,
	bool coach = false, bool force = false)
{
	if (!IsConnected(ent))
		return false;
	Arena *arena = FindArena(team.arena_id);
	if (!arena)
		return false;
	if (!MM_ArenaUsesLogicalTeams(arena->settings.type)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Practice arenas have no teams; entrants fight independently.\n");
		return false;
	}
	if (!force && FighterRosterLocked(ent) &&
		(ent->client->resp.arena_team_id != team.id || coach)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"You cannot change Arena teams during an active match.\n");
		return false;
	}
	if (!force && !PingAllowed(ent, *arena)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Your ping does not meet that arena's limits.\n");
		return false;
	}
	const int client_num = ClientNumber(ent);
	if (client_num < 0 || client_num >= MAX_CLIENTS)
		return false;
	if (!coach && ent->client->resp.arena_team_id == team.id &&
		Role(ent->client) != mm_arena_role_t::Coach)
		return true;
	if (!force && ent->client->resp.arena_team_id != team.id &&
		ArenaEntryLocked(ent, *arena)) {
		gi.Client_Print(ent, PRINT_HIGH, "That arena is locked.\n");
		return false;
	}
	if (!force && arena->settings.max_players > 0 &&
		ArenaPopulation(arena->id) >= arena->settings.max_players &&
		ent->client->resp.arena_id != arena->id) {
		gi.Client_Print(ent, PRINT_HIGH, "That arena is full.\n");
		return false;
	}
	const bool coach_invited = coach &&
		(team.invites[client_num] & kInviteCoach);
	const bool member_invited = !coach &&
		(team.invites[client_num] & kInviteMember);
	if (!force && team.locked && !coach_invited && !member_invited &&
		(team.password.empty() || !password || team.password != password)) {
		gi.Client_Print(ent, PRINT_HIGH, "That team is locked.\n");
		return false;
	}
	if (!force && !coach &&
		!MM_ArenaTeamSizeIsUnlimited(arena->settings.type) &&
		MemberCount(team.id) >= arena->settings.players_per_team) {
		gi.Client_Print(ent, PRINT_HIGH, "That team is full.\n");
		return false;
	}

	if (ent->client->resp.arena_team_id != team.id)
		LeaveTeam(ent, false, true, team.id);
	ent->client->resp.arena_id = static_cast<int16_t>(arena->id);
	ent->client->resp.arena_team_id = team.id;
	ent->client->resp.arena_line_enabled = !coach;
	ent->client->resp.ready = false;
	const bool series_active = IsActiveSeriesState(arena->state);
	const bool rover_immediate = !coach &&
		arena->settings.type == mm_arena_type_t::RedRover &&
		series_active && arena->state != mm_arena_state_t::RoundOver;
	ent->client->resp.arena_late_join = !coach && series_active &&
		arena->settings.type != mm_arena_type_t::RedRover &&
		(team.id == arena->active_red || team.id == arena->active_blue);
	if (coach) {
		MM_ResetClientScoring(ent->client);
		SetRoleField(ent->client, mm_arena_role_t::Coach);
		ProjectRole(ent, mm_arena_role_t::Coach, TEAM_SPECTATOR, false);
	} else if (rover_immediate) {
		MM_ResetClientScoring(ent->client);
		ProjectRole(ent, mm_arena_role_t::Fighter, team.side, true);
		ent->client->eliminated = false;
		if (arena->state != mm_arena_state_t::Running)
			ent->client->ps.pmove.pm_type = PM_FREEZE;
	} else {
		SetRoleField(ent->client, mm_arena_role_t::Queued);
		ProjectRole(ent, mm_arena_role_t::Queued, TEAM_SPECTATOR, false);
	}
	if (team.captain < 0 && !coach && MM_CaptainEligible(ent))
		team.captain = client_num;
	P_Menu_Dirty();
	return true;
}

bool CreatePlayerTeam(gentity_t *ent, std::string_view requested_name,
	bool force = false)
{
	if (!IsConnected(ent))
		return false;
	if (!force && FighterRosterLocked(ent)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"You cannot change Arena teams during an active match.\n");
		return false;
	}
	Arena *arena = ArenaFor(ent->client);
	if (!arena) {
		gi.Client_Print(ent, PRINT_HIGH, "Enter an arena first.\n");
		return false;
	}
	if (!force && !PingAllowed(ent, *arena)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Your ping does not meet this arena's limits.\n");
		return false;
	}
	if (MM_ArenaUsesFixedTeams(arena->settings.type)) {
		gi.Client_Print(ent, PRINT_HIGH, "Choose Red or Blue in this arena.\n");
		return false;
	}
	if (!MM_ArenaUsesLogicalTeams(arena->settings.type)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Practice arenas have no teams; entrants fight independently.\n");
		return false;
	}
	if (!force && arena->settings.lock_arena) {
		gi.Client_Print(ent, PRINT_HIGH, "This arena is locked.\n");
		return false;
	}
	if (!force && arena->settings.max_teams > 0 &&
		LogicalTeamCount(arena->id) >= arena->settings.max_teams) {
		gi.Client_Print(ent, PRINT_HIGH,
			"This arena has reached its team limit.\n");
		return false;
	}
	if (ent->client->resp.arena_team_id)
		LeaveTeam(ent, false, true);
	std::string name = SafeText(requested_name, 30);
	if (name.empty())
		name = fmt::format("{}'s team", ent->client->resp.netname);
	const uint16_t id = CreateLogicalTeam(*arena, name, false, TEAM_NONE);
	LogicalTeam *team = FindTeam(id);
	if (!team) {
		gi.Client_Print(ent, PRINT_HIGH, "No Arena team slots are available.\n");
		return false;
	}
	team->captain = MM_CaptainEligible(ent) ? ClientNumber(ent) : -1;
	return JoinLogicalTeam(ent, *team, nullptr, false, force);
}

LogicalTeam *FindJoinTarget(gentity_t *ent, std::string_view token)
{
	int id = 0;
	if (ParseInt(token, id) && id > 0 && id <= kMaxLogicalTeams) {
		LogicalTeam *team = FindTeam(static_cast<uint16_t>(id));
		if (team && (!ent || !ent->client ||
			team->arena_id == ent->client->resp.arena_id))
			return team;
	}
	gentity_t *target = ClientEntFromString(std::string(token).c_str());
	if (!IsConnected(target))
		return nullptr;
	return FindTeam(target->client->resp.arena_team_id);
}

bool IsCaptain(const gentity_t *ent, const LogicalTeam *team)
{
	return IsConnected(ent) && team && team->captain == ClientNumber(ent);
}

bool RequireCompetitionCommand(gentity_t *ent, const Arena *arena,
	const char *operation, bool allow_admin_override = false)
{
	const bool admin = ent && ent->client && ent->client->sess.admin;
	if (arena && MM_ArenaCompetitionCommandAllowed(
		arena->settings.competition_mode, admin, allow_admin_override))
		return true;
	gi.Client_Print(ent, PRINT_HIGH,
		fmt::format("{} is only available in competition mode.\n",
			operation).c_str());
	return false;
}

bool TransferCaptain(gentity_t *ent, LogicalTeam &team, gentity_t *target)
{
	if (!IsCaptain(ent, &team)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"You must be captain to transfer it to another player.\n");
		return false;
	}
	if (!IsConnected(target)) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid player.\n");
		return false;
	}
	if (target == ent) {
		gi.Client_Print(ent, PRINT_HIGH,
			"You can't transfer captain to yourself.\n");
		return false;
	}
	if (!IsTeamMember(target->client, team.id)) {
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("{} is not on your Arena team.\n",
				target->client->resp.netname).c_str());
		return false;
	}
	if (!MM_CaptainEligible(target)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Bots cannot be team captain.\n");
		return false;
	}
	Arena *a = FindArena(team.arena_id);
	if (a && IsActiveSeriesState(a->state) &&
		(team.id == a->active_red || team.id == a->active_blue) &&
		Role(target->client) != mm_arena_role_t::Fighter) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Captaincy can only pass to an active fighter during a match.\n");
		return false;
	}
	team.captain = ClientNumber(target);
	if (a)
		ArenaPrint(*a, PRINT_HIGH,
			fmt::format("{} is now captain of {}.\n",
				target->client->resp.netname, team.name));
	return true;
}

void CaptainCommand(gentity_t *ent, const char *target_name)
{
	LogicalTeam *team = IsConnected(ent)
		? FindTeam(ent->client->resp.arena_team_id) : nullptr;
	if (!team || Role(ent->client) == mm_arena_role_t::Coach) {
		gi.Client_Print(ent, PRINT_HIGH,
			"You must be on an Arena team to use this command.\n");
		return;
	}
	Arena *arena = FindArena(team->arena_id);
	if (!RequireCompetitionCommand(ent, arena, "Team captain control"))
		return;
	ValidateCaptain(*team);
	if (!target_name || !*target_name) {
		if (team->captain == ClientNumber(ent))
			gi.Client_Print(ent, PRINT_HIGH,
				"You are your Arena team captain.\n");
		else if (gentity_t *captain = ClientEntity(team->captain);
			IsConnected(captain))
			gi.Client_Print(ent, PRINT_HIGH,
				fmt::format("{} is captain of {}.\n",
					captain->client->resp.netname, team->name).c_str());
		else if (!MM_CaptainEligible(ent))
			gi.Client_Print(ent, PRINT_HIGH,
				"Bots cannot be team captain.\n");
		else {
			team->captain = ClientNumber(ent);
			if (Arena *a = FindArena(team->arena_id))
				ArenaPrint(*a, PRINT_HIGH,
					fmt::format("{} became captain of {}.\n",
						ent->client->resp.netname, team->name));
		}
		return;
	}
	TransferCaptain(ent, *team, ClientEntFromString(target_name));
}

bool SetTeamLocked(gentity_t *ent, LogicalTeam &team, bool locked,
	std::string_view password)
{
	Arena *arena = FindArena(team.arena_id);
	if (!RequireCompetitionCommand(ent, arena, "Team locking", true))
		return false;
	if (!IsCaptain(ent, &team) &&
		(!ent || !ent->client || !ent->client->sess.admin)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Only team captains or admins can lock Arena teams.\n");
		return false;
	}
	if (team.locked == locked) {
		gi.Client_Print(ent, PRINT_HIGH,
			locked ? "Your Arena team is already locked.\n" :
				"Your Arena team is already unlocked.\n");
		return false;
	}
	team.locked = locked;
	team.password = locked ? SafeText(password, 30) : "";
	if (arena)
		ArenaPrint(*arena, PRINT_HIGH,
			fmt::format("{} has been {}.\n", team.name,
				locked ? "locked" : "unlocked"));
	P_Menu_Dirty();
	return true;
}

int ReadyLogicalTeam(gentity_t *ent, LogicalTeam &team, bool ready)
{
	Arena *a = FindArena(team.arena_id);
	if (!RequireCompetitionCommand(ent, a, "Team ready control", true))
		return -1;
	if (!IsCaptain(ent, &team) &&
		(!ent || !ent->client || !ent->client->sess.admin)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Only team captains or admins can ready the Arena team.\n");
		return -1;
	}
	if (!a || (a->state != mm_arena_state_t::Empty &&
		a->state != mm_arena_state_t::Warmup)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Ready status can only change between Arena matches.\n");
		return -1;
	}
	int changed = 0;
	for (gentity_t *member : TeamMembers(team.id)) {
		if (member->client->resp.ready == ready)
			continue;
		member->client->resp.ready = ready;
		changed++;
	}
	if (changed)
		ArenaPrint(*a, PRINT_HIGH,
			fmt::format("{} marked {} {} ({} player{}).\n",
				ent->client->resp.netname, team.name,
				ready ? "ready" : "not ready", changed,
				changed == 1 ? "" : "s"));
	else
		gi.Client_Print(ent, PRINT_HIGH,
			ready ? "All players on that Arena team are already ready.\n" :
				"All players on that Arena team are already not ready.\n");
	return changed;
}

void TeamNameCommand(gentity_t *ent, std::string_view requested_name)
{
	LogicalTeam *team = IsConnected(ent)
		? FindTeam(ent->client->resp.arena_team_id) : nullptr;
	if (!team || Role(ent->client) == mm_arena_role_t::Coach) {
		gi.Client_Print(ent, PRINT_HIGH,
			"You must be on an Arena team to use this command.\n");
		return;
	}
	Arena *a = FindArena(team->arena_id);
	if (!RequireCompetitionCommand(ent, a, "Team naming"))
		return;
	if (!IsCaptain(ent, team)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Only the team captain can change the team name.\n");
		return;
	}
	if (requested_name.empty()) {
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("Current team name: {}.\n", team->name).c_str());
		return;
	}
	const std::string name = SafeText(requested_name, 30);
	if (name.empty()) {
		gi.Client_Print(ent, PRINT_HIGH, "The team name cannot be empty.\n");
		return;
	}
	team->name = name;
	ArenaPrint(*a, PRINT_HIGH,
		fmt::format("{} renamed the team to {}.\n",
			ent->client->resp.netname, team->name));
	P_Menu_Dirty();
}

void PrintTeamKickList(gentity_t *ent, const LogicalTeam &team)
{
	gi.Client_Print(ent, PRINT_HIGH,
		fmt::format("Players on {}:\n", team.name).c_str());
	bool found = false;
	for (gentity_t *member : TeamMembers(team.id, true)) {
		const int client_num = ClientNumber(member);
		if (client_num < 0)
			continue;
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("  {}: {}{}\n", client_num,
				member->client->resp.netname,
				Role(member->client) == mm_arena_role_t::Coach
					? " (coach)" : "").c_str());
		found = true;
	}
	if (!found)
		gi.Client_Print(ent, PRINT_HIGH, "  none\n");
}

void TeamKickCommand(gentity_t *ent, const char *target_name)
{
	LogicalTeam *team = IsConnected(ent)
		? FindTeam(ent->client->resp.arena_team_id) : nullptr;
	if (!team || Role(ent->client) == mm_arena_role_t::Coach) {
		gi.Client_Print(ent, PRINT_HIGH,
			"You must be on an Arena team to use this command.\n");
		return;
	}
	Arena *a = FindArena(team->arena_id);
	if (!RequireCompetitionCommand(ent, a, "Team kick"))
		return;
	if (!IsCaptain(ent, team)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Only the team captain can kick a team member.\n");
		return;
	}
	if (!target_name || !*target_name) {
		PrintTeamKickList(ent, *team);
		return;
	}
	gentity_t *target = ClientEntFromString(target_name);
	if (!IsConnected(target)) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid player.\n");
		return;
	}
	if (target == ent) {
		gi.Client_Print(ent, PRINT_HIGH,
			"You cannot kick yourself from the team.\n");
		return;
	}
	if (!IsTeamMember(target->client, team->id)) {
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("{} is not on your Arena team.\n",
				target->client->resp.netname).c_str());
		return;
	}
	const std::string target_name_copy = target->client->resp.netname;
	LeaveTeam(target, false, true);
	gi.Client_Print(target, PRINT_HIGH,
		"You were kicked from your Arena team.\n");
	ArenaPrint(*a, PRINT_HIGH,
		fmt::format("{} kicked {} from {}.\n",
			ent->client->resp.netname, target_name_copy, team->name));
}

void TeamMuteCommand(gentity_t *ent, bool muted)
{
	LogicalTeam *team = IsConnected(ent)
		? FindTeam(ent->client->resp.arena_team_id) : nullptr;
	if (!team || Role(ent->client) == mm_arena_role_t::Coach) {
		gi.Client_Print(ent, PRINT_HIGH,
			"You must be on an Arena team to use this command.\n");
		return;
	}
	Arena *a = FindArena(team->arena_id);
	if (!RequireCompetitionCommand(ent, a, "Team mute"))
		return;
	if (!IsCaptain(ent, team)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Only the team captain can mute or unmute the team.\n");
		return;
	}
	if (team->chat_muted == muted) {
		gi.Client_Print(ent, PRINT_HIGH,
			muted ? "Your Arena team is already muted.\n" :
				"Your Arena team is already unmuted.\n");
		return;
	}
	team->chat_muted = muted;
	ArenaPrint(*a, PRINT_HIGH,
		muted
			? "Non-captains are now limited to team chat.\n"
			: "Arena and world chat are enabled for the team.\n");
}

enum class TeamInviteAction {
	InviteMember,
	RevokeMember,
	InviteSpectator,
	RevokeSpectator
};

void TeamInviteCommand(gentity_t *ent, TeamInviteAction action,
	const char *target_name, bool coach)
{
	LogicalTeam *team = IsConnected(ent)
		? FindTeam(ent->client->resp.arena_team_id) : nullptr;
	if (!team || Role(ent->client) == mm_arena_role_t::Coach) {
		gi.Client_Print(ent, PRINT_HIGH,
			"You must be on an Arena team to use this command.\n");
		return;
	}
	Arena *a = FindArena(team->arena_id);
	const bool spectator_action =
		action == TeamInviteAction::InviteSpectator ||
		action == TeamInviteAction::RevokeSpectator;
	if (!RequireCompetitionCommand(ent, a,
		spectator_action ? "Spectator invitations" : "Team invitations"))
		return;
	if (!spectator_action && !IsCaptain(ent, team)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Only the team captain can manage team invitations.\n");
		return;
	}
	gentity_t *target = target_name && *target_name
		? ClientEntFromString(target_name) : nullptr;
	if (!IsConnected(target)) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid player.\n");
		return;
	}
	if (target == ent) {
		gi.Client_Print(ent, PRINT_HIGH, "You cannot invite or revoke yourself.\n");
		return;
	}
	if (spectator_action && target->client->resp.arena_id != a->id) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Spectator invitations only apply to players in this arena.\n");
		return;
	}

	const int target_num = ClientNumber(target);
	if (target_num < 0 || target_num >= MAX_CLIENTS) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid player.\n");
		return;
	}
	switch (action) {
	case TeamInviteAction::InviteMember:
		if (IsTeamMember(target->client, team->id)) {
			gi.Client_Print(ent, PRINT_HIGH,
				"That player is already on your Arena team.\n");
			return;
		}
		team->invites[target_num] |= kInviteMember;
		gi.Client_Print(ent, PRINT_HIGH, "Team invitation granted.\n");
		break;
	case TeamInviteAction::RevokeMember:
		team->invites[target_num] &= ~kInviteMember;
		gi.Client_Print(ent, PRINT_HIGH, "Team invitation revoked.\n");
		break;
	case TeamInviteAction::InviteSpectator:
		team->invites[target_num] |= kInviteSpectator;
		if (coach)
			team->invites[target_num] |= kInviteCoach;
		gi.Client_Print(ent, PRINT_HIGH,
			coach ? "Coach invitation granted.\n" :
				"Spectator invitation granted.\n");
		break;
	case TeamInviteAction::RevokeSpectator:
		team->invites[target_num] &=
			~(kInviteSpectator | kInviteCoach);
		if (target->client->resp.arena_team_id == team->id &&
			Role(target->client) == mm_arena_role_t::Coach)
			LeaveTeam(target, false, false);
		gi.Client_Print(ent, PRINT_HIGH, "Spectator invitation revoked.\n");
		break;
	}
}

void SpecWhoCommand(gentity_t *ent)
{
	LogicalTeam *team = IsConnected(ent)
		? FindTeam(ent->client->resp.arena_team_id) : nullptr;
	if (!team || Role(ent->client) == mm_arena_role_t::Coach) {
		gi.Client_Print(ent, PRINT_HIGH,
			"You must be on an Arena team to use this command.\n");
		return;
	}
	Arena *a = FindArena(team->arena_id);
	if (!RequireCompetitionCommand(ent, a, "Spectator invitations"))
		return;

	gi.Client_Print(ent, PRINT_HIGH, "Team spectators/coaches:\n");
	bool found = false;
	for (int i = 0; i < static_cast<int>(game.maxclients) && i < MAX_CLIENTS; i++) {
		gentity_t *viewer = ClientEntity(i);
		if (!IsConnected(viewer))
			continue;
		const bool coaching =
			viewer->client->resp.arena_team_id == team->id &&
			Role(viewer->client) == mm_arena_role_t::Coach;
		const uint8_t invite = team->invites[i];
		const uint8_t spectator_invite =
			invite & (kInviteSpectator | kInviteCoach);
		if (!coaching && !spectator_invite)
			continue;
		const char *label = coaching ? "current coach" :
			((invite & kInviteCoach) ? "invited coach" : "invited observer");
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("  {} ({})\n", game.clients[i].resp.netname,
				label).c_str());
		found = true;
	}
	if (!found)
		gi.Client_Print(ent, PRINT_HIGH, "  none\n");
}

void CoachCommand(gentity_t *ent, const char *team_name)
{
	LogicalTeam *target_team = team_name && *team_name
		? FindJoinTarget(ent, team_name) : nullptr;
	if (!target_team ||
		target_team->arena_id != ent->client->resp.arena_id) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid Arena team.\n");
		return;
	}
	Arena *a = FindArena(target_team->arena_id);
	if (!RequireCompetitionCommand(ent, a, "Coaching"))
		return;
	const int client_num = ClientNumber(ent);
	if (client_num < 0 || client_num >= MAX_CLIENTS ||
		!(target_team->invites[client_num] & kInviteCoach)) {
		gi.Client_Print(ent, PRINT_HIGH, "You do not have a coach invite.\n");
		return;
	}
	JoinLogicalTeam(ent, *target_team, nullptr, true);
}

bool HasArenaInvite(const gentity_t *ent, const Arena &arena)
{
	const int client_num = ClientNumber(ent);
	if (client_num < 0 || client_num >= MAX_CLIENTS)
		return false;
	for (uint16_t id = 1; id <= kMaxLogicalTeams; id++)
		if (s_teams[id].valid && s_teams[id].arena_id == arena.id &&
			s_teams[id].invites[client_num])
			return true;
	return false;
}

bool ArenaEntryLocked(const gentity_t *ent, const Arena &arena)
{
	if (IsConnected(ent) && ent->client->sess.admin)
		return false;
	if (IsConnected(ent) && ent->client->resp.arena_id == arena.id)
		return false;
	if (HasArenaInvite(ent, arena))
		return false;
	return arena.settings.lock_arena;
}

void PrintArenaList(gentity_t *ent)
{
	gi.Client_Print(ent, PRINT_HIGH, "Rocket Arena arenas:\n");
	for (int id = 1; id <= s_arena_count; id++) {
		const Arena &arena = s_arenas[id];
		const std::string line = fmt::format(
			"  {}: {} [{} | {}] - {} player{}\n", id, arena.name,
			TypeName(arena.settings.type), StateName(arena.state),
			ArenaPopulation(id),
			ArenaPopulation(id) == 1 ? "" : "s");
		gi.Client_Print(ent, PRINT_HIGH, line.c_str());
	}
}

void PrintSettings(gentity_t *ent, const Arena &arena)
{
	const mm_arena_settings_t &s = arena.settings;
	const bool practice = s.type == mm_arena_type_t::Practice;
	const std::string state_line = practice
		? fmt::format("State: {}  active players {}",
			StateName(arena.state), ArenaPopulation(arena.id))
		: fmt::format("State: {}  score {}-{}  round {}",
			StateName(arena.state), arena.red_score, arena.blue_score,
			arena.round);
	const std::string roster_rule = practice
		? std::string("free-for-all")
		: (MM_ArenaTeamSizeIsUnlimited(s.type)
			? std::string("unlimited teams")
			: fmt::format("{}v{}", s.players_per_team, s.players_per_team));
	const std::string round_rule = s.type == mm_arena_type_t::RedRover
		? fmt::format("continuous rover, round {}", arena.round + 1)
		: (practice
			? std::string("continuous practice, no teams or deaths")
			: fmt::format("best-of {}, round {}", s.rounds, arena.round));
	const std::string line = fmt::format(
		"{} ({})\n{}\n"
		"{}  {}  health {} armor {} weapons 0x{:03x}\n"
		"ammo S{} B{} G{} R{} C{} Sl{} plasma{} bfg{}\n"
		"protection H{} A{}  falling {}  excessive {}  damage-score {}\n"
		"fast-switch {}  grapple {}  rocket-speed {}  competition {}\n"
		"arena-lock {}  lock-voters {}  max-players {}  max-teams {}  ping {}-{}\n"
		"unbalanced {}  vote-tries {}  vote-mask 0x{:03x}\n",
		arena.name, TypeName(s.type), state_line, roster_rule, round_rule,
		s.health, s.armor, s.weapon_mask,
		s.shells, s.bullets, s.grenades, s.rockets, s.cells, s.slugs,
		s.plasma_ammo, s.bfg_ammo,
		static_cast<int>(s.health_protect), static_cast<int>(s.armor_protect),
		s.falling_damage ? "on" : "off", s.excessive ? "on" : "off",
		s.damage_scoring ? "on" : "off",
		s.fast_switch ? "on" : "off", s.grapple ? "on" : "off",
		s.rocket_speed, s.competition_mode ? "on" : "off",
		s.lock_arena ? "on" : "off", s.lock_count > 0 ? s.lock_count : 6,
		s.max_players, s.max_teams, s.min_ping, s.max_ping,
		s.unbalanced ? "on" : "off", s.vote_tries, s.vote_allow_mask);
	gi.Client_Print(ent, PRINT_HIGH, line.c_str());
}

void PrintLine(gentity_t *ent, const Arena &arena)
{
	if (arena.settings.type == mm_arena_type_t::Practice) {
		int fighters = 0;
		for (gentity_t *candidate : active_clients())
			if (IsConnected(candidate) &&
				candidate->client->resp.arena_id == arena.id &&
				Role(candidate->client) == mm_arena_role_t::Fighter)
				fighters++;
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("{} is continuous free practice: {} active fighter{}, "
				"no team line.\n", arena.name, fighters,
				fighters == 1 ? "" : "s").c_str());
		return;
	}

	gi.Client_Print(ent, PRINT_HIGH,
		fmt::format("{} line:\n", arena.name).c_str());
	std::vector<const LogicalTeam *> teams;
	for (uint16_t id = 1; id <= kMaxLogicalTeams; id++) {
		const LogicalTeam &team = s_teams[id];
		if (team.valid && team.arena_id == arena.id && !team.fixed)
			teams.push_back(&team);
	}
	std::sort(teams.begin(), teams.end(), [](const LogicalTeam *a, const LogicalTeam *b) {
		return a->queue_order < b->queue_order;
	});
	int queue = 0;
	for (const LogicalTeam *team : teams) {
		const char *position = "";
		std::string dynamic_position;
		if (team->id == arena.active_red)
			position = "Champion";
		else if (team->id == arena.active_blue)
			position = "Challenger";
		else {
			dynamic_position = fmt::format("#{}", ++queue);
			position = dynamic_position.c_str();
		}
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("  {} team {} \"{}\" ({}/{})\n", position, team->id,
				team->name, MemberCount(team->id, true),
				arena.settings.players_per_team).c_str());
	}
}

bool Propose(gentity_t *ent, std::string_view key, std::string_view value)
{
	Arena *arena = ArenaFor(ent->client);
	if (!arena)
		return false;
	if (!MM_VoteClientEligible(ent)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Only human arena participants can propose settings.\n");
		return false;
	}
	if (!g_allow_voting || !g_allow_voting->integer) {
		gi.Client_Print(ent, PRINT_HIGH, "Voting is disabled.\n");
		return false;
	}
	if (arena->ballot.active) {
		gi.Client_Print(ent, PRINT_HIGH, "A vote is already in progress here.\n");
		return false;
	}
	const int proposer_num = ClientNumber(ent);
	if (proposer_num < 0 || proposer_num >= MAX_CLIENTS ||
		arena->settings.vote_tries <= 0 ||
		arena->vote_tries_used[proposer_num] >= arena->settings.vote_tries) {
		gi.Client_Print(ent, PRINT_HIGH,
			"You have no arena proposal attempts remaining this match.\n");
		return false;
	}
	const uint32_t flag = VoteFlagForSetting(Lower(key));
	if (!flag ||
		(arena->settings.vote_allow_mask & flag) != flag) {
		gi.Client_Print(ent, PRINT_HIGH, "That arena setting cannot be voted.\n");
		return false;
	}
	const int lock_voters = arena->settings.lock_count > 0
		? arena->settings.lock_count : 6;
	if (flag == MM_ARENA_VOTE_LOCK &&
		EligibleVoters(*arena) < lock_voters) {
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("Lock votes require at least {} players in this arena.\n",
				lock_voters).c_str());
		return false;
	}
	mm_arena_settings_t proposed = MM_ArenaProposalBase(
		arena->settings, arena->pending_settings, arena->settings_pending);
	if (!ApplySetting(proposed, key, value, static_cast<int>(game.maxclients))) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid arena setting or value.\n");
		return false;
	}
	FinalizeSettings(proposed, static_cast<int>(game.maxclients));
	arena->ballot.active = true;
	arena->vote_tries_used[proposer_num]++;
	arena->ballot.proposer = proposer_num;
	arena->ballot.original_voters = 0;
	for (gentity_t *voter : active_clients()) {
		if (!IsConnected(voter) ||
			voter->client->resp.arena_id != arena->id ||
			!MM_VoteClientEligible(voter))
			continue;
		const int voter_num = ClientNumber(voter);
		if (voter_num < 0 || voter_num >= MAX_CLIENTS)
			continue;
		arena->ballot.eligible[voter_num] = true;
		arena->ballot.original_voters++;
	}
	arena->ballot.setting_flag = flag;
	arena->ballot.setting_key = std::string(key);
	arena->ballot.setting_value = std::string(value);
	arena->ballot.description = fmt::format("{} {}", SafeText(key), SafeText(value));
	arena->ballot.expires = level.time +
		gtime_t::from_sec(std::clamp(s_vote_seconds ? s_vote_seconds->integer : 30, 10, 120));
	arena->ballot.votes[proposer_num] = 1;
	ArenaPrint(*arena, PRINT_HIGH,
		fmt::format("{} proposed {}. Use `arena vote yes` or `arena vote no`.\n",
			ent->client->resp.netname, arena->ballot.description));
	return true;
}

void CastVote(gentity_t *ent, bool yes)
{
	Arena *arena = ArenaFor(ent->client);
	if (!arena || !arena->ballot.active) {
		gi.Client_Print(ent, PRINT_HIGH, "There is no arena vote in progress.\n");
		return;
	}
	const int client_num = ClientNumber(ent);
	if (client_num < 0 || client_num >= MAX_CLIENTS ||
		!arena->ballot.eligible[client_num]) {
		gi.Client_Print(ent, PRINT_HIGH,
			"You were not eligible when this arena vote began.\n");
		return;
	}
	arena->ballot.votes[client_num] = yes ? 1 : -1;
	gi.Client_Print(ent, PRINT_HIGH, yes ? "You voted yes.\n" : "You voted no.\n");
	TickBallot(*arena);
}

void BeginTimeout(gentity_t *ent)
{
	Arena *arena = ArenaFor(ent->client);
	LogicalTeam *team = FindTeam(ent->client->resp.arena_team_id);
	const bool active_side = arena && team &&
		(team->id == arena->active_red || team->id == arena->active_blue);
	const bool team_member = team &&
		Role(ent->client) != mm_arena_role_t::Coach;
	if (!arena || !MM_ArenaCanCallTimeout(arena->state, active_side,
		team_member)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Only a member of an active team can call timeout.\n");
		return;
	}
	if (arena->settings.type == mm_arena_type_t::Practice) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Practice arenas do not use competition timeouts.\n");
		return;
	}
	if (!arena->settings.competition_mode) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Timeouts are only available in competition mode.\n");
		return;
	}
	const int timeout_seconds = MM_ClampTimeoutSeconds(
		g_dm_timeout_length ? g_dm_timeout_length->integer : 120);
	if (timeout_seconds <= 0) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Server has disabled timeouts.\n");
		return;
	}
	const int allowance = std::clamp(s_timeouts ? s_timeouts->integer : 3, 0, 9);
	if (team->timeouts_used >= allowance) {
		gi.Client_Print(ent, PRINT_HIGH, "Your team has no timeouts left.\n");
		return;
	}
	team->timeouts_used++;
	arena->timeout_team = team->id;
	arena->state_before_pause = arena->state;
	arena->paused_remaining = arena->state_timer && arena->state_timer > level.time
		? arena->state_timer - level.time : gtime_t {};
	arena->state = mm_arena_state_t::Paused;
	arena->state_timer = level.time +
		gtime_t::from_sec(timeout_seconds);
	FreezeFighters(*arena, true);
	ArenaPrint(*arena, PRINT_CENTER,
		fmt::format("TIMEOUT\nCalled by {}", team->name), true);
}

void BeginResumeCountdown(Arena &arena)
{
	const int resume_seconds = MM_ClampResumeCountdownSeconds(
		g_dm_timeout_resume_countdown
			? g_dm_timeout_resume_countdown->integer : 30);
	if (resume_seconds <= 0) {
		ResumeArena(arena);
		return;
	}
	arena.resume_countdown = true;
	arena.state_timer = level.time + gtime_t::from_sec(resume_seconds);
	ArenaPrint(arena, PRINT_CENTER,
		fmt::format("Match resumes in {} seconds.", resume_seconds), true);
}

void BeginTimein(gentity_t *ent)
{
	Arena *arena = ArenaFor(ent->client);
	LogicalTeam *team = FindTeam(ent->client->resp.arena_team_id);
	const bool active_side = arena && team &&
		(team->id == arena->active_red || team->id == arena->active_blue);
	const bool team_member = team &&
		Role(ent->client) != mm_arena_role_t::Coach;
	if (arena && arena->state == mm_arena_state_t::Paused &&
		arena->resume_countdown) {
		gi.Client_Print(ent, PRINT_HIGH,
			"This arena is already counting back in.\n");
		return;
	}
	if (!arena || !MM_ArenaCanCallTimein(arena->state, active_side,
		team_member,
		arena->resume_countdown, team ? team->id : 0,
		arena->timeout_team)) {
		if (arena && team && active_side &&
			arena->state == mm_arena_state_t::Paused &&
			arena->timeout_team && team->id != arena->timeout_team)
			gi.Client_Print(ent, PRINT_HIGH,
				"Only the team that called timeout can call timein.\n");
		else
			gi.Client_Print(ent, PRINT_HIGH,
				"Only a member of an active team can resume this arena.\n");
		return;
	}
	BeginResumeCountdown(*arena);
}

const gentity_t *SpatialOwner(const gentity_t *ent)
{
	const gentity_t *current = ent;
	for (int depth = 0; current && !current->client && depth < 8; depth++) {
		const gentity_t *next = nullptr;
		if (current->owner && current->owner != current)
			next = current->owner;
		else if (current->teammaster && current->teammaster != current &&
			current->teammaster->client)
			next = current->teammaster;
		if (!next || next == ent)
			break;
		current = next;
	}
	return current;
}

int EffectiveEntityArena(const gentity_t *ent)
{
	if (!ent || ent == world || ent == g_entities)
		return 0;
	if (ent->client)
		return ent->client->resp.arena_id;
	if (ent->arena != 0)
		return ent->arena;
	ent = SpatialOwner(ent);
	if (!ent || ent == world || ent == g_entities)
		return 0;
	if (ent->client)
		return ent->client->resp.arena_id;
	if (ent->arena != 0)
		return ent->arena;
	return 0;
}

gentity_t *RandomPoint(const std::vector<gentity_t *> &points)
{
	if (points.empty())
		return nullptr;
	return points[irandom(static_cast<int>(points.size()))];
}

gentity_t *FarthestPoint(const std::vector<gentity_t *> &points,
	const gentity_t *player)
{
	if (points.empty())
		return nullptr;
	std::vector<const gentity_t *> fighters;
	const int arena_id = player && player->client
		? player->client->resp.arena_id : 0;
	for (gentity_t *ent : active_clients()) {
		if (!IsConnected(ent) || ent == player ||
			ent->client->resp.arena_id != arena_id ||
			Role(ent->client) != mm_arena_role_t::Fighter ||
			ent->client->eliminated || ent->deadflag || ent->health <= 0)
			continue;
		fighters.push_back(ent);
	}
	if (fighters.empty())
		return RandomPoint(points);

	gentity_t *best = nullptr;
	float best_nearest = -1.0f;
	for (gentity_t *spot : points) {
		float nearest = std::numeric_limits<float>::max();
		for (const gentity_t *fighter : fighters)
			nearest = std::min(nearest,
				(spot->s.origin - fighter->s.origin).lengthSquared());
		if (!best || nearest > best_nearest) {
			best = spot;
			best_nearest = nearest;
		}
	}
	return best;
}

void AppendLayout(std::string &layout, std::string_view text)
{
	if (!MM_ScoreboardCanAppend(layout.size(), text.size(),
		MAX_STRING_CHARS, false))
		return;
	layout.append(text);
}

const char *RoleLabel(const gclient_t *client)
{
	switch (Role(client)) {
	case mm_arena_role_t::Lobby: return "LOBBY";
	case mm_arena_role_t::Observer: return "OBSERVER";
	case mm_arena_role_t::Queued: return "IN LINE";
	case mm_arena_role_t::Fighter:
		return client && client->eliminated ? "ELIMINATED" : "FIGHTER";
	case mm_arena_role_t::Coach: return "COACH";
	}
	return "";
}

void ReturnToJoinMenu(gentity_t *ent, menu_hnd_t *)
{
	if (!ent || !ent->client)
		return;
	P_Menu_Close(ent);
	G_Menu_Join_Open(ent);
}

const char *TypeTag(mm_arena_type_t type)
{
	switch (type) {
	case mm_arena_type_t::RocketArena: return "RA";
	case mm_arena_type_t::ClanArena: return "CA";
	case mm_arena_type_t::RedRover: return "RR";
	case mm_arena_type_t::Practice: return "PR";
	}
	return "RA";
}

bool TeamMenuLocked(const gentity_t *ent, const LogicalTeam &team)
{
	if (!team.locked || !IsConnected(ent) ||
		ent->client->resp.arena_team_id == team.id)
		return false;
	const int client_num = ClientNumber(ent);
	return client_num < 0 || client_num >= MAX_CLIENTS ||
		!(team.invites[client_num] & kInviteMember);
}

bool TeamMenuFull(const gentity_t *ent, const Arena &arena,
	const LogicalTeam &team)
{
	return IsConnected(ent) && ent->client->resp.arena_team_id != team.id &&
		!MM_ArenaTeamSizeIsUnlimited(arena.settings.type) &&
		MemberCount(team.id) >= arena.settings.players_per_team;
}

void ToggleLineFromMenu(gentity_t *ent)
{
	Arena *arena = IsConnected(ent) ? ArenaFor(ent->client) : nullptr;
	if (!arena || arena->settings.type == mm_arena_type_t::Practice ||
		!ent->client->resp.arena_team_id)
		return;
	if (Role(ent->client) == mm_arena_role_t::Coach) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Leave coaching before joining a team line.\n");
		return;
	}
	if (FighterRosterLocked(ent)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"You cannot change line status during an active match.\n");
		return;
	}
	ent->client->resp.arena_line_enabled =
		!ent->client->resp.arena_line_enabled;
	ent->client->resp.ready = false;
	ProjectRole(ent, ent->client->resp.arena_line_enabled
		? mm_arena_role_t::Queued : mm_arena_role_t::Observer,
		TEAM_SPECTATOR, false);
	gi.Client_Print(ent, PRINT_HIGH,
		ent->client->resp.arena_line_enabled
			? "You entered the arena line.\n"
			: "You left the arena line.\n");
	P_Menu_Dirty();
}

void SelectMenu(gentity_t *ent, menu_hnd_t *menu)
{
	if (!IsConnected(ent) || !menu || !menu->arg ||
		menu->cur < 0 || menu->cur >= kMenuRows)
		return;
	const auto *state = static_cast<MenuState *>(menu->arg);
	const bool rooms = state->rooms;
	const int action = state->actions[menu->cur];
	if (action <= -100) {
		const int page = -100 - action;
		P_Menu_Close(ent);
		MM_Arena_OpenMenuPage(ent, page, rooms);
		return;
	}
	bool return_to_main = false;
	bool return_to_rooms = false;
	if (action >= 1000) {
		if (LogicalTeam *team = FindTeam(static_cast<uint16_t>(action - 1000)))
			JoinLogicalTeam(ent, *team, nullptr);
	} else if (action > 0) {
		const Arena *destination = FindArena(action);
		return_to_rooms = !MM_Arena_MoveTo(ent, action, true);
		if (!return_to_rooms && destination &&
			destination->settings.type == mm_arena_type_t::Practice)
			return_to_main = true;
	} else if (action == -1)
		CreatePlayerTeam(ent, {});
	else if (action == -2) {
		if (FighterRosterLocked(ent))
			gi.Client_Print(ent, PRINT_HIGH,
				"You cannot leave an active Arena match.\n");
		else
			LeaveTeam(ent, false, false);
	} else if (action == -3 && ent->client->resp.arena_id) {
		if (FighterRosterLocked(ent))
			gi.Client_Print(ent, PRINT_HIGH,
				"You cannot leave an active Arena match.\n");
		else {
			LeaveTeam(ent, true, false);
			return_to_main = true;
		}
	} else if (action == -4) {
		ToggleLineFromMenu(ent);
	}
	P_Menu_Close(ent);
	if (return_to_main)
		G_Menu_Join_Open(ent);
	else
		MM_Arena_OpenMenuPage(ent, 0,
			return_to_rooms || (rooms && action <= 0));
}

std::string CompactMenuLabel(std::string_view prefix,
	std::string_view name, std::string_view suffix)
{
	constexpr size_t max_length = 26;
	const std::string clean_prefix = SafeText(prefix, max_length);
	const std::string clean_suffix = SafeText(suffix, max_length);
	const size_t fixed_length = clean_prefix.size() + clean_suffix.size();
	if (fixed_length >= max_length)
		return SafeText(clean_prefix + clean_suffix, max_length);

	const std::string clean_name = SafeText(name);
	return clean_prefix +
		muffmode::TruncateWithEllipsis(
			clean_name, max_length - fixed_length) +
		clean_suffix;
}

void SetMenuEntry(menu_t &entry, std::string_view text, int align,
	void (*select)(gentity_t *, menu_hnd_t *) = nullptr)
{
	Q_strlcpy(entry.text, SafeText(text, 26).c_str(), sizeof(entry.text));
	entry.align = align;
	entry.SelectFunc = select;
}

void PopulateMenu(gentity_t *ent, menu_t *entries, MenuState &state)
{
	if (!entries)
		return;
	for (int row = 0; row < kMenuRows; row++) {
		entries[row] = {};
		state.actions[row] = 0;
	}

	SetMenuEntry(entries[0], "*Rocket Arena", MENU_ALIGN_CENTER);
	Q_strlcpy(entries[1].text,
		"\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
		"\36\36\36\36\36\36\36\36\37", sizeof(entries[1].text));
	entries[1].align = MENU_ALIGN_CENTER;

	if (state.rooms) {
		constexpr int page_size = 13;
		const mm_arena_page_range_t range = MM_ArenaPageRange(
			s_arena_count, state.page, page_size);
		state.page = range.page;
		SetMenuEntry(entries[2],
			fmt::format("Arenas {}-{} / {}", range.first + 1,
				range.last, s_arena_count), MENU_ALIGN_CENTER);

		const int current_arena = IsConnected(ent)
			? ent->client->resp.arena_id : 0;
		const bool roster_locked = FighterRosterLocked(ent);
		int row = 3;
		for (int id = range.first + 1; id <= range.last && row <= 15;
			id++, row++) {
			const Arena &arena = s_arenas[id];
			const int population = ArenaPopulation(id);
			const bool current = id == current_arena;
			const bool locked = ArenaEntryLocked(ent, arena);
			const bool full = arena.settings.max_players > 0 &&
				population >= arena.settings.max_players &&
				(!IsConnected(ent) ||
				 ent->client->resp.arena_id != arena.id);
			const bool ping_blocked = !PingAllowed(ent, arena);
			const std::string prefix =
				fmt::format("{} [{}] ", id, TypeTag(arena.settings.type));
			const std::string suffix = current ? " (current)" :
				(roster_locked ? " (in match)" :
				 (locked ? " LOCKED" :
				  (full ? " FULL" :
				   (ping_blocked ? " PING" :
					fmt::format(" ({})", population)))));
			const std::string label =
				CompactMenuLabel(prefix, arena.name, suffix);
			const bool disabled = current || roster_locked || locked ||
				full || ping_blocked;
			SetMenuEntry(entries[row], label, MENU_ALIGN_LEFT,
				disabled ? nullptr : SelectMenu);
			if (!disabled)
				state.actions[row] = id;
		}
		if (range.page > 0) {
			SetMenuEntry(entries[16], "< Previous", MENU_ALIGN_LEFT,
				SelectMenu);
			state.actions[16] = -100 - (range.page - 1);
		}
		if (range.page + 1 < range.page_count) {
			SetMenuEntry(entries[17], "Next >", MENU_ALIGN_LEFT,
				SelectMenu);
			state.actions[17] = -100 - (range.page + 1);
		}
	} else {
		Arena *arena = IsConnected(ent) ? ArenaFor(ent->client) : nullptr;
		if (!arena) {
			state.rooms = true;
			state.page = 0;
			PopulateMenu(ent, entries, state);
			return;
		}

		const bool practice =
			arena->settings.type == mm_arena_type_t::Practice;
		std::vector<uint16_t> team_ids;
		if (!practice) {
			EnsureFixedTeams(*arena);
			for (uint16_t id = 1; id <= kMaxLogicalTeams; id++) {
				const LogicalTeam &team = s_teams[id];
				if (team.valid && team.arena_id == arena->id &&
					(team.fixed ||
					 !MM_ArenaUsesFixedTeams(arena->settings.type)))
					team_ids.push_back(id);
			}
		}

		constexpr int team_page_size = 10;
		const mm_arena_page_range_t range = MM_ArenaPageRange(
			static_cast<int>(team_ids.size()), state.page, team_page_size);
		state.page = range.page;
		const std::string header_suffix = range.page_count > 1
			? fmt::format(" [{}] {}/{}", TypeTag(arena->settings.type),
				range.page + 1, range.page_count)
			: fmt::format(" [{}]", TypeTag(arena->settings.type));
		SetMenuEntry(entries[2],
			CompactMenuLabel({}, arena->name, header_suffix),
			MENU_ALIGN_CENTER);
		const bool roster_locked = FighterRosterLocked(ent);
		int row = 3;
		if (practice) {
			SetMenuEntry(entries[row++], "Practice: no teams",
				MENU_ALIGN_LEFT);
		} else {
			for (int index = range.first; index < range.last && row <= 12;
				index++, row++) {
				const LogicalTeam &team =
					s_teams[team_ids[static_cast<size_t>(index)]];
				const int members = MemberCount(team.id);
				const bool current = IsConnected(ent) &&
					ent->client->resp.arena_team_id == team.id;
				const bool coaching = current &&
					Role(ent->client) == mm_arena_role_t::Coach;
				const bool locked = TeamMenuLocked(ent, team);
				const bool full = TeamMenuFull(ent, *arena, team);
				std::string label;
				if (coaching)
					label = CompactMenuLabel({}, team.name,
						" (coaching)");
				else if (current)
					label = CompactMenuLabel({}, team.name,
						" (current)");
				else if (roster_locked)
					label = CompactMenuLabel({}, team.name,
						" (in match)");
				else if (locked)
					label = CompactMenuLabel({}, team.name,
						" LOCKED");
				else if (full)
					label = CompactMenuLabel({}, team.name,
						" FULL");
				else if (MM_ArenaTeamSizeIsUnlimited(
					arena->settings.type))
					label = CompactMenuLabel("Join ", team.name,
						fmt::format(" ({})", members));
				else
					label = CompactMenuLabel("Join ", team.name,
						fmt::format(" ({}/{})", members,
							arena->settings.players_per_team));
				const bool disabled =
					current || roster_locked || locked || full;
				SetMenuEntry(entries[row], label, MENU_ALIGN_LEFT,
					disabled ? nullptr : SelectMenu);
				if (!disabled)
					state.actions[row] = 1000 + team.id;
			}
		}

		const bool has_team = IsConnected(ent) &&
			ent->client->resp.arena_team_id != 0;
		const bool coaching = has_team &&
			Role(ent->client) == mm_arena_role_t::Coach;
		if (!practice && !MM_ArenaUsesFixedTeams(arena->settings.type)) {
			SetMenuEntry(entries[13],
				coaching ? "Stop Coaching" :
					(has_team ? "Leave Team" :
						(roster_locked ? "Create Team (locked)" :
							"Create Team")),
				MENU_ALIGN_LEFT, roster_locked ? nullptr : SelectMenu);
			if (!roster_locked)
				state.actions[13] = has_team ? -2 : -1;
		} else if (!practice && has_team) {
			SetMenuEntry(entries[13],
				coaching ? "Stop Coaching" :
					(roster_locked ? "Leave Team (locked)" : "Leave Team"),
				MENU_ALIGN_LEFT, roster_locked ? nullptr : SelectMenu);
			if (!roster_locked)
				state.actions[13] = -2;
		}
		if (!practice && has_team &&
			Role(ent->client) != mm_arena_role_t::Coach) {
			SetMenuEntry(entries[14],
				roster_locked ? "Team Line (locked)" :
					(ent->client->resp.arena_line_enabled
						? "Leave Line" : "Join Line"),
				MENU_ALIGN_LEFT, roster_locked ? nullptr : SelectMenu);
			if (!roster_locked)
				state.actions[14] = -4;
		}
		SetMenuEntry(entries[15],
			roster_locked ? "Return Lobby (locked)" : "Return to Lobby",
			MENU_ALIGN_LEFT, roster_locked ? nullptr : SelectMenu);
		if (!roster_locked)
			state.actions[15] = -3;
		if (range.page > 0) {
			SetMenuEntry(entries[16], "< Previous", MENU_ALIGN_LEFT,
				SelectMenu);
			state.actions[16] = -100 - (range.page - 1);
		}
		if (range.page + 1 < range.page_count) {
			SetMenuEntry(entries[17], "Next >", MENU_ALIGN_LEFT,
				SelectMenu);
			state.actions[17] = -100 - (range.page + 1);
		}
	}

	SetMenuEntry(entries[18], "Move: navigate  Attack: select",
		MENU_ALIGN_CENTER);
	Q_strlcpy(entries[19].text, "$g_pc_return",
		sizeof(entries[19].text));
	entries[19].align = MENU_ALIGN_LEFT;
	entries[19].SelectFunc = ReturnToJoinMenu;
}

} // namespace muffmode::arena

namespace arena = muffmode::arena;

void MM_Arena_PreflightMap(const char *mapname, const char *entity_lump)
{
	arena::s_map_active = false;
	arena::s_map_contract = {};
	arena::s_map_validation = {
		mm_arena_map_error_t::MissingArenaKey, 0
	};

	if (!g_gametype || !GT_RAW(GT_ARENA))
		return;

	arena::s_map_contract = arena::ParseMapContract(entity_lump);
	arena::s_map_validation =
		MM_ArenaValidateMapContract(arena::s_map_contract);
	arena::s_map_active = static_cast<bool>(arena::s_map_validation);

	if (arena::s_map_active) {
		gi.Com_PrintFmt(
			"Rocket Arena: validated {} with {} playable arena{}.\n",
			mapname ? mapname : "<unnamed>",
			arena::s_map_contract.declared_rooms,
			arena::s_map_contract.declared_rooms == 1 ? "" : "s");
		return;
	}

	const int room = arena::s_map_validation.room;
	gi.Com_PrintFmt(
		"Rocket Arena inactive on {}: {}{}. "
		"Load an RA2-compatible map with an explicit worldspawn arena key.\n",
		mapname ? mapname : "<unnamed>",
		MM_ArenaMapErrorText(arena::s_map_validation.error),
		room > 0 ? fmt::format(" (arena {})", room) : std::string {});
}

bool MM_Arena_Active()
{
	return g_gametype && GT_RAW(GT_ARENA) && arena::s_map_active;
}

void MM_Arena_Init()
{
	arena::s_arenas = {};
	arena::s_teams = {};
	arena::s_rover_pending_side.fill(TEAM_NONE);
	arena::s_rover_pending_arena.fill(0);
	arena::s_rover_respawn_pending.fill(false);
	arena::s_rover_respawn_arena.fill(0);
	arena::s_practice_respawn_pending.fill(false);
	arena::s_practice_respawn_arena.fill(0);
	arena::s_arena_count = 0;
	arena::s_classic_ra2_map = false;
	arena::s_internal_team_change = false;
	arena::s_ended_round_this_frame = false;
	arena::s_had_human_participant = false;
	arena::s_level_started = level.time;

	if (!arena::IsArenaGametype())
		return;

	if (!world || world->arena != arena::s_map_contract.declared_rooms) {
		arena::s_map_active = false;
		gi.Com_ErrorFmt(
			"Rocket Arena: live worldspawn disagrees with the validated map "
			"contract for {}.\n", level.mapname);
		return;
	}
	const mm_arena_map_validation_t live_validation =
		MM_ArenaValidateMapContract(arena::LiveMapContract());
	if (!live_validation) {
		arena::s_map_active = false;
		gi.Com_ErrorFmt(
			"Rocket Arena: live entity validation failed on {}: {}{}.\n",
			level.mapname, MM_ArenaMapErrorText(live_validation.error),
			live_validation.room > 0
				? fmt::format(" (arena {})", live_validation.room)
				: std::string {});
		return;
	}

	arena::s_config = g_arena_config;
	arena::s_type = g_arena_default_type;
	arena::s_competition = g_arena_competition;
	arena::s_unbalanced = g_arena_unbalanced;
	arena::s_health_protect = g_arena_health_protect;
	arena::s_armor_protect = g_arena_armor_protect;
	arena::s_excessive = g_arena_excessive;
	arena::s_grapple = g_arena_grapple;
	arena::s_rocket_speed = g_arena_rocket_speed;
	arena::s_lock_arena = g_arena_lock;
	arena::s_lock_count = g_arena_lock_count;
	arena::s_max_players = g_arena_max_players;
	arena::s_vote_seconds = g_arena_vote_time;
	arena::s_timeouts = g_arena_timeouts;

	arena::DiscoverArenas();
	mm_arena_settings_t defaults = arena::CvarDefaults();
	arena::LoadArenaConfig(defaults);

	for (size_t i = 0; i < game.maxclients; i++) {
		gclient_t &client = game.clients[i];
		MM_ResetClientScoring(&client);
		client.resp.arena_id = 0;
		client.resp.arena_team_id = 0;
		client.resp.arena_role = static_cast<uint8_t>(mm_arena_role_t::Lobby);
		client.resp.arena_side = TEAM_SPECTATOR;
		client.resp.ready = false;
		client.resp.arena_line_enabled = true;
		client.resp.arena_late_join = false;
	}

	const std::array<item_id_t, 9> weapons = {
		IT_WEAPON_CHAINFIST, IT_WEAPON_MACHINEGUN, IT_WEAPON_SHOTGUN,
		IT_WEAPON_GLAUNCHER, IT_WEAPON_RLAUNCHER, IT_WEAPON_PLASMABEAM,
		IT_WEAPON_RAILGUN, IT_WEAPON_HYPERBLASTER, IT_WEAPON_BFG
	};
	for (item_id_t item : weapons)
		PrecacheItem(GetItemByIndex(item));

	gi.Com_PrintFmt("Rocket Arena: initialized {} arena{}.\n",
		arena::s_arena_count, arena::s_arena_count == 1 ? "" : "s");
}

void MM_Arena_OnMatchStart()
{
	if (!arena::IsArenaGametype())
		return;
	for (int id = 1; id <= arena::s_arena_count; id++) {
		arena::Arena &a = arena::s_arenas[id];
		if (arena::ArenaPopulation(id))
			a.state = mm_arena_state_t::Warmup;
	}
}

void MM_Arena_OnMatchReset()
{
	if (!arena::IsArenaGametype())
		return;
	arena::s_rover_pending_side.fill(TEAM_NONE);
	arena::s_rover_pending_arena.fill(0);
	arena::s_rover_respawn_pending.fill(false);
	arena::s_rover_respawn_arena.fill(0);
	arena::s_practice_respawn_pending.fill(false);
	arena::s_practice_respawn_arena.fill(0);
	for (int id = 1; id <= arena::s_arena_count; id++)
		arena::ResetArenaSeries(arena::s_arenas[id]);
}

void MM_Arena_OnClientBegin(gentity_t *ent)
{
	if (!arena::IsArenaGametype() || !arena::IsConnected(ent))
		return;
	const int client_num = arena::ClientNumber(ent);
	arena::ClearClientSlotState(client_num);
	ent->client->resp.arena_id = 0;
	ent->client->resp.arena_team_id = 0;
	ent->client->resp.arena_side = TEAM_SPECTATOR;
	MM_ResetClientScoring(ent->client);
	ent->client->resp.ready = false;
	ent->client->resp.arena_line_enabled = true;
	ent->client->resp.arena_late_join = false;
	arena::SetRoleField(ent->client, mm_arena_role_t::Lobby);
	ent->client->eliminated = true;
	// ClientBeginDeathmatch calls ClientSpawn immediately after this hook.
	// Project the fresh lobby session directly to avoid SetTeam spawning once
	// here and then spawning the same client again in the caller.
	ent->client->sess.team = TEAM_SPECTATOR;
	P_PublishEngineTeam(ent);
	if (!(ent->svflags & SVF_BOT) && !ent->client->sess.is_a_bot)
		arena::s_had_human_participant = true;
	if (ent->client->sess.is_a_bot || (ent->svflags & SVF_BOT)) {
		int best = 1;
		for (int id = 2; id <= arena::s_arena_count; id++)
			if (arena::ArenaPopulation(id) < arena::ArenaPopulation(best))
				best = id;
		MM_Arena_MoveTo(ent, best, false);
	}
}

void MM_Arena_OnClientDisconnect(gentity_t *ent)
{
	if (!arena::IsArenaGametype() || !ent || !ent->client)
		return;
	const uint16_t old_team = ent->client->resp.arena_team_id;
	const int client_num = arena::ClientNumber(ent);
	arena::ClearClientSlotState(client_num);
	ent->client->resp.arena_team_id = 0;
	ent->client->resp.arena_id = 0;
	arena::SetRoleField(ent->client, mm_arena_role_t::Lobby);
	if (arena::LogicalTeam *team = arena::FindTeam(old_team)) {
		arena::ClearSpectatorInvites(*team);
		if (team->captain == client_num)
			team->captain = -1;
		arena::ValidateCaptain(*team);
	}
	arena::DestroyEmptyTeams();
	P_Menu_Dirty();
}

void MM_Arena_OnDeath(gentity_t *victim, gentity_t *attacker)
{
	if (!arena::IsArenaGametype() || !arena::IsConnected(victim) ||
		!MM_Arena_IsFighter(victim->client))
		return;
	arena::Arena *a = arena::ArenaFor(victim->client);
	if (!a || a->state != mm_arena_state_t::Running)
		return;
	const int client_num = arena::ClientNumber(victim);
	if (a->settings.type == mm_arena_type_t::Practice) {
		victim->client->eliminated = false;
		if (client_num >= 0 && client_num < MAX_CLIENTS)
			arena::s_practice_respawn_pending[client_num] = true;
		if (client_num >= 0 && client_num < MAX_CLIENTS)
			arena::s_practice_respawn_arena[client_num] =
				static_cast<int16_t>(a->id);
		return;
	}

	const gentity_t *source = arena::SpatialOwner(attacker);
	const bool valid_player_source = source && source->client &&
		source->client->resp.arena_id == a->id &&
		MM_Arena_IsFighter(source->client) &&
		source->client->resp.arena_team_id &&
		victim->client->resp.arena_team_id;
	const bool other_player = valid_player_source && source != victim;
	const bool enemy_kill = other_player &&
		!MM_Arena_SameTeam(source->client, victim->client);

	if (a->settings.type != mm_arena_type_t::RedRover) {
		// Damage scoring already awards points as damage is dealt. Classic
		// frag scoring instead gives an enemy kill to the attacker and charges
		// suicides, environmental deaths and team kills to the responsible
		// player. Do this before elimination so G_AdjustPlayerScore accepts the
		// victim for self/world penalties.
		if (!a->settings.damage_scoring) {
			if (enemy_kill)
				G_AdjustPlayerScore(source->client, 1, false, 0);
			else
				G_AdjustPlayerScore(valid_player_source
					? source->client : victim->client, -1, false, 0);
		}
		if (enemy_kill) {
			MS_Adjust(source->client, MSTAT_KILLS_TOTAL, 1);
			if (1_sec > (level.time - victim->client->respawn_time))
				MS_Adjust(source->client, MSTAT_KILLS_SPAWN, 1);
		}
		victim->client->eliminated = true;
		return;
	}

	victim->client->eliminated = true;
	const bool rover_player = other_player &&
		(source->client->resp.arena_side == TEAM_RED ||
		 source->client->resp.arena_side == TEAM_BLUE);
	const bool rover_enemy_kill = rover_player &&
		source->client->resp.arena_side != victim->client->resp.arena_side;
	const bool friendly_kill = rover_player && !rover_enemy_kill;
	if (rover_enemy_kill) {
		MS_Adjust(source->client, MSTAT_KILLS_TOTAL, 1);
		if (1_sec > (level.time - victim->client->respawn_time))
			MS_Adjust(source->client, MSTAT_KILLS_SPAWN, 1);
	}
	const auto score_delta = MM_ArenaRedRoverScoreDelta(rover_enemy_kill);
	if (rover_enemy_kill) {
		G_AdjustPlayerScore(source->client, score_delta.killer, false, 0);
		G_AdjustPlayerScore(victim->client, score_delta.victim, false, 0);
	} else if (friendly_kill)
		G_AdjustPlayerScore(source->client, -1, false, 0);
	else
		G_AdjustPlayerScore(victim->client, score_delta.victim, false, 0);
	const bool destination_red = MM_ArenaRedRoverDestinationIsRed(
		victim->client->resp.arena_side == TEAM_RED, rover_enemy_kill,
		rover_enemy_kill && source->client->resp.arena_side == TEAM_RED);
	const team_t new_side = destination_red ? TEAM_RED : TEAM_BLUE;
	if (client_num >= 0 && client_num < MAX_CLIENTS) {
		arena::s_rover_pending_side[client_num] = new_side;
		arena::s_rover_pending_arena[client_num] =
			static_cast<int16_t>(a->id);
	}
}

bool MM_Arena_RunFrame()
{
	if (!arena::IsArenaGametype())
		return true;
	arena::s_ended_round_this_frame = false;
	bool runnable = false;
	for (int id = 1; id <= arena::s_arena_count; id++) {
		arena::TickArena(arena::s_arenas[id]);
		const mm_arena_state_t state = arena::s_arenas[id].state;
		runnable = runnable || state == mm_arena_state_t::MatchCountdown ||
			state == mm_arena_state_t::RoundCountdown ||
			state == mm_arena_state_t::Running ||
			state == mm_arena_state_t::Paused;
	}
	return runnable;
}

bool MM_Arena_UpdateRound()
{
	return arena::IsArenaGametype() && arena::s_ended_round_this_frame;
}

bool MM_Arena_CheckExitRules()
{
	if (!arena::IsArenaGametype())
		return false;
	if (level.intermission_time)
		return true;
	if (timelimit && timelimit->value > 0 &&
		level.time >= arena::s_level_started + gtime_t::from_min(timelimit->value)) {
		QueueIntermission("Rocket Arena timelimit hit.", false, false);
		return true;
	}
	if (arena::s_had_human_participant &&
		g_dm_allow_no_humans && !g_dm_allow_no_humans->integer) {
		bool human = false;
		for (gentity_t *ent : active_clients()) {
			if (!arena::IsConnected(ent))
				continue;
			if (!(ent->svflags & SVF_BOT) && !ent->client->sess.is_a_bot) {
				human = true;
				break;
			}
		}
		if (!human)
			QueueIntermission("No human Arena players remaining.", true, false);
	}
	return true;
}

int MM_Arena_Count()
{
	return arena::IsArenaGametype() ? arena::s_arena_count : 0;
}

int MM_Arena_Id(const gentity_t *ent)
{
	return arena::IsArenaGametype() ? arena::EffectiveEntityArena(ent) : 0;
}

const char *MM_Arena_Name(int arena_id)
{
	const arena::Arena *a = arena::FindArena(arena_id, 0);
	return a ? a->name.c_str() : "";
}

bool MM_Arena_ValidId(int arena_id)
{
	return arena::IsArenaGametype() && arena::FindArena(arena_id) != nullptr;
}

bool MM_Arena_IsRunning(int arena_id)
{
	const arena::Arena *a = arena::FindArena(arena_id, 0);
	return arena::IsArenaGametype() && a &&
		a->state == mm_arena_state_t::Running;
}

bool MM_Arena_IsPaused(int arena_id)
{
	const arena::Arena *a = arena::FindArena(arena_id, 0);
	return arena::IsArenaGametype() && a &&
		a->state == mm_arena_state_t::Paused;
}

bool MM_Arena_OrdnanceActive(int arena_id)
{
	return MM_Arena_IsRunning(arena_id) || MM_Arena_IsPaused(arena_id);
}

bool MM_Arena_SameArena(const gentity_t *first, const gentity_t *second)
{
	if (!arena::IsArenaGametype())
		return true;
	const int a = MM_Arena_Id(first);
	const int b = MM_Arena_Id(second);
	return a < 0 || b < 0 || a == b;
}

bool MM_Arena_SpawnAllowed(const gentity_t *player, const gentity_t *spot)
{
	if (!arena::IsArenaGametype())
		return true;
	if (!player || !player->client || !spot)
		return false;
	const int desired = player->client->resp.arena_id;
	const int spot_arena = arena::EffectiveEntityArena(spot);
	if (desired <= 0)
		return spot_arena == 0;
	return desired == spot_arena;
}

gentity_t *MM_Arena_SelectSpawnPoint(gentity_t *player, bool spectator)
{
	if (!arena::IsArenaGametype() || !player || !player->client)
		return nullptr;
	std::vector<gentity_t *> primary;
	std::vector<gentity_t *> secondary;
	std::vector<gentity_t *> fallback;
	for (size_t i = 1; i < globals.num_entities; i++) {
		gentity_t *spot = &g_entities[i];
		if (!spot->inuse || !spot->classname)
			continue;
		if (!MM_Arena_SpawnAllowed(player, spot))
			continue;
		if (!spectator) {
			const bool red_start =
				!std::strcmp(spot->classname, "info_player_team_red");
			const bool blue_start =
				!std::strcmp(spot->classname, "info_player_team_blue");
			if ((red_start && player->client->resp.arena_side == TEAM_RED) ||
				(blue_start && player->client->resp.arena_side == TEAM_BLUE)) {
				primary.push_back(spot);
				continue;
			}
		}
		if (spectator && !std::strcmp(spot->classname, "misc_teleporter_dest"))
			primary.push_back(spot); // RA2 observer destinations.
		else if (spectator &&
			!std::strcmp(spot->classname, "info_player_intermission")) {
			if (arena::s_classic_ra2_map)
				fallback.push_back(spot);
			else
				secondary.push_back(spot); // RA3 observer cameras.
		}
		else if (!std::strcmp(spot->classname, "info_player_deathmatch")) {
			if (!spectator)
				secondary.push_back(spot);
			else if (arena::s_classic_ra2_map)
				secondary.push_back(spot); // RA2 idarena fallback.
			else
				fallback.push_back(spot);
		} else if (spectator &&
			!std::strcmp(spot->classname, "info_player_start"))
			secondary.push_back(spot); // Minimal validated arena-0 lobby.
	}
	if (!spectator) {
		if (gentity_t *point = arena::FarthestPoint(primary, player))
			return point;
		const arena::Arena *a = arena::ArenaForConst(player->client);
		if (a && MM_ArenaUsesFixedTeams(a->settings.type) && secondary.size() > 1) {
			const bool wants_odd = player->client->resp.arena_side == TEAM_RED
				? a->red_uses_odd_spawns : !a->red_uses_odd_spawns;
			std::vector<gentity_t *> side_points;
			for (size_t i = 0; i < secondary.size(); i++)
				if (((i & 1u) == 0) == wants_odd)
					side_points.push_back(secondary[i]);
			if (gentity_t *point = arena::FarthestPoint(side_points, player))
				return point;
		}
	}
	if (gentity_t *point = arena::FarthestPoint(primary, player))
		return point;
	if (gentity_t *point = arena::FarthestPoint(secondary, player))
		return point;
	return arena::FarthestPoint(fallback, player);
}

bool MM_Arena_CanUseEntity(const gentity_t *player, const gentity_t *target)
{
	if (!arena::IsArenaGametype())
		return true;
	if (!player || !target)
		return false;
	const int player_arena = arena::EffectiveEntityArena(player);
	if (player->client && player_arena > 0 &&
		arena::Role(player->client) == mm_arena_role_t::Fighter &&
		MM_Arena_IsPaused(player_arena))
		return false;
	const int target_arena = arena::EffectiveEntityArena(target);
	if (target_arena <= 0 || player_arena <= 0)
		return true;
	return player_arena == target_arena;
}

bool MM_Arena_CanFollow(const gentity_t *viewer, const gentity_t *target)
{
	if (!arena::IsArenaGametype())
		return true;
	if (!arena::IsConnected(viewer) || !arena::IsConnected(target) ||
		viewer == target || viewer->client->resp.arena_id <= 0 ||
		viewer->client->resp.arena_id != target->client->resp.arena_id)
		return false;
	const arena::LogicalTeam *team =
		arena::FindTeam(target->client->resp.arena_team_id, 0);
	const arena::Arena *a = arena::ArenaForConst(target->client);
	const bool same_squad = MM_Arena_SameSquad(viewer->client, target->client);
	const int viewer_num = arena::ClientNumber(viewer);
	const bool invited = team && viewer_num >= 0 &&
		(team->invites[viewer_num] & arena::kInviteSpectator);
	return MM_ArenaFollowAllowedByCompetition(
		a && a->settings.competition_mode,
		arena::Role(viewer->client) == mm_arena_role_t::Coach,
		same_squad, invited);
}

bool MM_Arena_CanInteract(const gentity_t *first, const gentity_t *second)
{
	if (!arena::IsArenaGametype())
		return true;
	if (!first || !second)
		return false;
	const int a = arena::EffectiveEntityArena(first);
	const int b = arena::EffectiveEntityArena(second);
	if (a <= 0 || b <= 0)
		return true;
	return a == b;
}

bool MM_Arena_FilterDamage(gentity_t *target, gentity_t *attacker,
	int &damage, int &knockback)
{
	if (!arena::IsArenaGametype())
		return true;
	if (!target || !target->client)
		return MM_Arena_CanInteract(target, attacker);
	const gentity_t *source = arena::SpatialOwner(attacker);
	const int target_arena = target->client->resp.arena_id;
	const int source_arena = arena::EffectiveEntityArena(attacker);
	if (target_arena <= 0 || !MM_Arena_IsFighter(target->client) ||
		(source_arena > 0 && source_arena != target_arena) ||
		(source && source->client && !MM_Arena_IsFighter(source->client))) {
		damage = 0;
		knockback = 0;
		return false;
	}
	arena::Arena *a = arena::FindArena(target_arena);
	if (!a || a->state != mm_arena_state_t::Running) {
		damage = 0;
		knockback = 0;
		return false;
	}
	if (a->settings.type == mm_arena_type_t::Practice) {
		if (source && source->client) {
			if (source != target) {
				MM_AwardDamageScore(source->client, std::max(0, damage));
			}
			// Player weapons remain non-lethal while preserving knockback. World,
			// trigger and other environmental damage proceeds normally.
			damage = 0;
		}
		return true;
	}
	if (a->settings.type == mm_arena_type_t::RedRover && source &&
		source->client && source != target && !MM_Arena_SameTeam(source, target)) {
		const int dealt = std::max(0, damage);
		source->client->resp.round_dmg += dealt;
	}
	return true;
}

void MM_Arena_AdjustProtection(gentity_t *target, gentity_t *attacker,
	int &health_damage, int &armor_damage)
{
	if (!arena::IsArenaGametype() || !target || !target->client)
		return;
	const gentity_t *source = arena::SpatialOwner(attacker);
	if (!source || !source->client)
		return;
	arena::Arena *a = arena::ArenaFor(target->client);
	if (!a || source->client->resp.arena_id != a->id)
		return;
	const bool same_team = MM_Arena_SameTeam(target, source);
	const bool self = target == source;
	if (MM_ArenaProtectionBlocks(a->settings.armor_protect, same_team, self))
		armor_damage = 0;
	if (MM_ArenaProtectionBlocks(a->settings.health_protect, same_team, self))
		health_damage = 0;
}

bool MM_Arena_SameTeam(const gclient_t *first, const gclient_t *second)
{
	return arena::IsArenaGametype() && first && second &&
		first->resp.arena_id > 0 &&
		first->resp.arena_id == second->resp.arena_id &&
		first->resp.arena_team_id != 0 &&
		first->resp.arena_team_id == second->resp.arena_team_id;
}

bool MM_Arena_SameTeam(const gentity_t *first, const gentity_t *second)
{
	return first && second && MM_Arena_SameTeam(first->client, second->client);
}

bool MM_Arena_IsFighter(const gclient_t *client)
{
	return arena::IsArenaGametype() && arena::Role(client) == mm_arena_role_t::Fighter;
}

bool MM_Arena_IsEliminated(const gclient_t *client)
{
	return MM_Arena_IsFighter(client) && client->eliminated;
}

bool MM_Arena_CombatEnabled(const gentity_t *ent)
{
	if (!arena::IsConnected(ent) || !MM_Arena_IsFighter(ent->client) ||
		ent->client->eliminated)
		return false;
	const arena::Arena *a = arena::ArenaForConst(ent->client);
	return a && a->state == mm_arena_state_t::Running;
}

bool MM_Arena_SeriesActive(const gentity_t *ent)
{
	if (!arena::IsArenaGametype() || !arena::IsConnected(ent))
		return false;
	const arena::Arena *a = arena::ArenaForConst(ent->client);
	if (!a)
		return false;
	switch (a->state) {
	case mm_arena_state_t::MatchCountdown:
	case mm_arena_state_t::RoundCountdown:
	case mm_arena_state_t::Running:
	case mm_arena_state_t::Paused:
	case mm_arena_state_t::RoundOver:
	case mm_arena_state_t::MatchOver:
		return true;
	case mm_arena_state_t::Empty:
	case mm_arena_state_t::Warmup:
		return false;
	}
	return false;
}

bool MM_Arena_GlobalVoteBlocked(const gentity_t *ent)
{
	(void) ent;
	if (!arena::IsArenaGametype())
		return false;

	// Global votes can change the map or server-wide rules. A caller waiting in
	// the lobby (or in an idle room) must not bypass another room's live series.
	for (int id = 1; id <= arena::s_arena_count; id++) {
		switch (arena::s_arenas[id].state) {
		case mm_arena_state_t::MatchCountdown:
		case mm_arena_state_t::RoundCountdown:
		case mm_arena_state_t::Running:
		case mm_arena_state_t::Paused:
		case mm_arena_state_t::RoundOver:
		case mm_arena_state_t::MatchOver:
			return true;
		case mm_arena_state_t::Empty:
		case mm_arena_state_t::Warmup:
			break;
		}
	}
	return false;
}

bool MM_Arena_HandleTeamRequest(gentity_t *ent, team_t desired_team, bool,
	bool force, bool, bool &result)
{
	if (!arena::IsArenaGametype() || arena::s_internal_team_change)
		return false;
	if (!arena::IsConnected(ent)) {
		result = false;
		return true;
	}
	if (desired_team == TEAM_SPECTATOR) {
		if (!force && arena::FighterRosterLocked(ent)) {
			gi.Client_Print(ent, PRINT_HIGH,
				"You cannot leave an active Arena match.\n");
			result = false;
			return true;
		}
		arena::LeaveTeam(ent, true, false);
		result = true;
		return true;
	}
	if (desired_team == TEAM_RED || desired_team == TEAM_BLUE) {
		const int arena_id = ent->client->resp.arena_id > 0
			? ent->client->resp.arena_id : 1;
		if (!arena::ArenaFor(ent->client) &&
			!MM_Arena_MoveTo(ent, arena_id, true, force)) {
			result = false;
			return true;
		}
		arena::Arena *a = arena::ArenaFor(ent->client);
		if (!a || !MM_ArenaUsesFixedTeams(a->settings.type)) {
			gi.Client_Print(ent, PRINT_HIGH,
				"Red/Blue assignment is only valid in fixed-team arenas.\n");
			result = false;
			return true;
		}
		arena::EnsureFixedTeams(*a);
		arena::LogicalTeam *team = arena::FindTeam(
			desired_team == TEAM_RED ? a->fixed_red : a->fixed_blue);
		result = team &&
			arena::JoinLogicalTeam(ent, *team, nullptr, false, force);
		return true;
	}
	if (ent->client->resp.arena_team_id) {
		result = true;
		return true;
	}
	const int arena_id = ent->client->resp.arena_id > 0
		? ent->client->resp.arena_id : 1;
	result = MM_Arena_MoveTo(ent, arena_id, !force, force);
	return true;
}

bool MM_Arena_MoveTo(gentity_t *ent, int arena_id, bool observe, bool force)
{
	if (!arena::IsArenaGametype() || !arena::IsConnected(ent))
		return false;
	if (!force && arena::FighterRosterLocked(ent)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"You cannot leave an active Arena match.\n");
		return false;
	}
	arena::Arena *a = arena::FindArena(arena_id);
	if (!a) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid arena number.\n");
		return false;
	}
	if (!force && !arena::PingAllowed(ent, *a)) {
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("Your ping does not meet this arena's {}-{} range.\n",
				a->settings.min_ping, a->settings.max_ping).c_str());
		return false;
	}
	if (!force && arena::ArenaEntryLocked(ent, *a)) {
		gi.Client_Print(ent, PRINT_HIGH, "That arena is locked.\n");
		return false;
	}
	if (!force && a->settings.max_players > 0 &&
		arena::ArenaPopulation(arena_id) >= a->settings.max_players &&
		ent->client->resp.arena_id != arena_id) {
		gi.Client_Print(ent, PRINT_HIGH, "That arena is full.\n");
		return false;
	}
	const bool changed_arena = ent->client->resp.arena_id != arena_id;
	if (!force && !changed_arena) {
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("You are already in {}.\n", a->name).c_str());
		return true;
	}
	const bool preserve_coach = !changed_arena &&
		arena::Role(ent->client) == mm_arena_role_t::Coach;
	arena::ClearPendingClientEvents(arena::ClientNumber(ent));
	if (ent->client->resp.arena_team_id) {
		const arena::LogicalTeam *team =
			arena::FindTeam(ent->client->resp.arena_team_id, 0);
		if (!team || team->arena_id != arena_id)
			arena::LeaveTeam(ent, false, true);
	}
	ent->client->resp.arena_id = static_cast<int16_t>(arena_id);
	if (changed_arena)
		MM_ResetClientScoring(ent->client);
	ent->client->resp.ready = false;
	if (!preserve_coach)
		ent->client->resp.arena_line_enabled = true;
	if (!preserve_coach &&
		MM_ArenaShouldAutoEnrollPractice(
			a->settings.type, arena::Role(ent->client))) {
		arena::EnrollPracticeFighter(ent, *a, true);
	} else if (ent->client->resp.arena_team_id) {
		arena::ProjectRole(ent,
			preserve_coach ? mm_arena_role_t::Coach : mm_arena_role_t::Queued,
			TEAM_SPECTATOR, true);
	} else {
		arena::ProjectRole(ent, mm_arena_role_t::Observer, TEAM_SPECTATOR, true);
		if (!observe) {
			arena::EnsureFixedTeams(*a);
			if (MM_ArenaUsesFixedTeams(a->settings.type)) {
				const uint16_t team_id =
					arena::MemberCount(a->fixed_red) <= arena::MemberCount(a->fixed_blue)
					? a->fixed_red : a->fixed_blue;
				if (arena::LogicalTeam *team = arena::FindTeam(team_id))
					arena::JoinLogicalTeam(ent, *team, nullptr, false, force);
			} else {
				arena::CreatePlayerTeam(ent, {}, force);
			}
		}
	}
	gi.Client_Print(ent, PRINT_HIGH,
		fmt::format("Entered {}.\n", a->name).c_str());
	P_Menu_Dirty();
	return true;
}

bool MM_Arena_RosterLocked(const gentity_t *ent)
{
	return arena::IsArenaGametype() && arena::FighterRosterLocked(ent);
}

bool MM_Arena_UsesTeams(const gentity_t *ent)
{
	if (!arena::IsArenaGametype() || !ent || !ent->client)
		return false;
	const arena::Arena *a = arena::ArenaForConst(ent->client);
	return a && MM_ArenaUsesLogicalTeams(a->settings.type);
}

bool MM_Arena_CanToggleReady(const gentity_t *ent)
{
	if (!arena::IsArenaGametype() || !arena::IsConnected(ent))
		return false;
	const arena::Arena *a = arena::ArenaForConst(ent->client);
	return a && a->settings.competition_mode &&
		a->settings.type != mm_arena_type_t::Practice &&
		ent->client->resp.arena_team_id &&
		arena::Role(ent->client) != mm_arena_role_t::Coach &&
		(a->state == mm_arena_state_t::Empty ||
		 a->state == mm_arena_state_t::Warmup);
}

bool MM_Arena_TeamCommand(gentity_t *ent)
{
	if (!arena::IsArenaGametype())
		return false;
	if (!arena::IsConnected(ent))
		return true;
	if (gi.argc() < 1 || gi.argc() > 2) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Usage: team [auto|red|blue|spectator]\n");
		return true;
	}
	if (gi.argc() == 1) {
		const arena::Arena *a = arena::ArenaForConst(ent->client);
		const arena::LogicalTeam *team =
			arena::FindTeam(ent->client->resp.arena_team_id, 0);
		if (!a)
			gi.Client_Print(ent, PRINT_HIGH,
				"You are in the Rocket Arena lobby.\n");
		else if (team)
			gi.Client_Print(ent, PRINT_HIGH,
				fmt::format("Arena {}: {} | Team: {} | {}\n",
					a->id, a->name, team->name,
					arena::RoleLabel(ent->client)).c_str());
		else
			gi.Client_Print(ent, PRINT_HIGH,
				fmt::format("Arena {}: {} | {}\n", a->id, a->name,
					arena::RoleLabel(ent->client)).c_str());
		return true;
	}

	const std::string selection = arena::Lower(gi.argv(1));
	if (selection == "spectator" || selection == "spec" || selection == "s") {
		if (arena::FighterRosterLocked(ent))
			gi.Client_Print(ent, PRINT_HIGH,
				"You cannot leave an active Arena match.\n");
		else
			arena::LeaveTeam(ent, true, false);
		return true;
	}
	if (!arena::ArenaFor(ent->client) && !MM_Arena_MoveTo(ent, 1, true))
		return true;
	arena::Arena *a = arena::ArenaFor(ent->client);
	if (!a)
		return true;

	arena::EnsureFixedTeams(*a);
	const bool choose_red = selection == "red" || selection == "r";
	const bool choose_blue = selection == "blue" || selection == "b";
	const bool choose_auto = selection == "auto" || selection == "a" ||
		selection == "free" || selection == "f";
	if (a->settings.type == mm_arena_type_t::Practice) {
		if (choose_red || choose_blue)
			gi.Client_Print(ent, PRINT_HIGH,
				"Practice has no Red or Blue teams.\n");
		else if (!choose_auto)
			gi.Client_Print(ent, PRINT_HIGH,
				"Usage: team [auto|spectator]\n");
		else {
			arena::EnrollPracticeFighter(ent, *a, false);
			gi.Client_Print(ent, PRINT_HIGH,
				"Practice is free-for-all; you are already active.\n");
		}
		return true;
	}
	if (choose_red || choose_blue) {
		if (!MM_ArenaUsesFixedTeams(a->settings.type)) {
			gi.Client_Print(ent, PRINT_HIGH,
				"Red and Blue are match sides here; use `arena create` or "
				"`arena join` for Rocket Arena teams.\n");
			return true;
		}
		if (arena::LogicalTeam *team = arena::FindTeam(
			choose_red ? a->fixed_red : a->fixed_blue))
			arena::JoinLogicalTeam(ent, *team, nullptr);
		return true;
	}
	if (!choose_auto) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Usage: team [auto|red|blue|spectator]\n");
		return true;
	}
	if (MM_ArenaUsesFixedTeams(a->settings.type)) {
		const uint16_t team_id =
			arena::MemberCount(a->fixed_red) <= arena::MemberCount(a->fixed_blue)
			? a->fixed_red : a->fixed_blue;
		if (arena::LogicalTeam *team = arena::FindTeam(team_id))
			arena::JoinLogicalTeam(ent, *team, nullptr);
	} else if (!ent->client->resp.arena_team_id) {
		arena::CreatePlayerTeam(ent, {});
	}
	return true;
}

bool MM_Arena_SetTeamCommand(gentity_t *ent)
{
	if (!arena::IsArenaGametype())
		return false;
	if (!arena::IsConnected(ent))
		return true;
	if (gi.argc() < 2 || gi.argc() > 3) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Usage: setteam <client name/num> [auto|red|blue|spectator]\n");
		return true;
	}
	gentity_t *target = ClientEntFromString(gi.argv(1));
	if (!arena::IsConnected(target)) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid client name or number.\n");
		return true;
	}
	if (gi.argc() == 2) {
		const arena::Arena *a = arena::ArenaForConst(target->client);
		const arena::LogicalTeam *team =
			arena::FindTeam(target->client->resp.arena_team_id, 0);
		if (!a)
			gi.Client_Print(ent, PRINT_HIGH,
				fmt::format("{} is in the Rocket Arena lobby.\n",
					target->client->resp.netname).c_str());
		else
			gi.Client_Print(ent, PRINT_HIGH,
				fmt::format("{} is in Arena {} ({}){}{}.\n",
					target->client->resp.netname, a->id, a->name,
					team ? ", team " : "",
					team ? team->name : "").c_str());
		return true;
	}

	const std::string selection = arena::Lower(gi.argv(2));
	const bool choose_red = selection == "red" || selection == "r";
	const bool choose_blue = selection == "blue" || selection == "b";
	const bool choose_auto = selection == "auto" || selection == "a" ||
		selection == "free" || selection == "f";
	const bool choose_spectator = selection == "spectator" ||
		selection == "spec" || selection == "s";
	if (!choose_red && !choose_blue && !choose_auto && !choose_spectator) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Usage: setteam <client name/num> "
			"[auto|red|blue|spectator]\n");
		return true;
	}

	bool moved = false;
	if (choose_spectator) {
		arena::LeaveTeam(target, true, true);
		moved = true;
	} else {
		arena::Arena *a = arena::ArenaFor(target->client);
		if (!a)
			a = arena::FindArena(1);
		if (!a)
			return true;
		if ((choose_red || choose_blue) &&
			!MM_ArenaUsesFixedTeams(a->settings.type)) {
			gi.Client_Print(ent, PRINT_HIGH,
				"Red/Blue assignment is only valid in fixed-team arenas.\n");
			return true;
		}
		if (!arena::ArenaFor(target->client) &&
			!MM_Arena_MoveTo(target, 1, true, true))
			return true;
		a = arena::ArenaFor(target->client);
		if (!a)
			return true;
		arena::EnsureFixedTeams(*a);
		if (choose_red || choose_blue) {
			if (arena::LogicalTeam *team = arena::FindTeam(
				choose_red ? a->fixed_red : a->fixed_blue))
				moved = arena::JoinLogicalTeam(
					target, *team, nullptr, false, true);
		} else {
			if (a->settings.type == mm_arena_type_t::Practice) {
				moved = arena::EnrollPracticeFighter(target, *a, true);
			} else if (MM_ArenaUsesFixedTeams(a->settings.type)) {
				const uint16_t team_id =
					arena::MemberCount(a->fixed_red) <=
						arena::MemberCount(a->fixed_blue)
					? a->fixed_red : a->fixed_blue;
				if (arena::LogicalTeam *team = arena::FindTeam(team_id))
					moved = arena::JoinLogicalTeam(
						target, *team, nullptr, false, true);
			} else if (target->client->resp.arena_team_id)
				moved = true;
			else
				moved = arena::CreatePlayerTeam(target, {}, true);
		}
	}

	if (moved) {
		const arena::Arena *a = arena::ArenaForConst(target->client);
		const arena::LogicalTeam *team =
			arena::FindTeam(target->client->resp.arena_team_id, 0);
		gi.LocBroadcast_Print(PRINT_HIGH,
			"[ADMIN]: Moved {} to {}.\n", target->client->resp.netname,
			a ? (team ? team->name.c_str() : a->name.c_str()) :
				"the Rocket Arena lobby");
	}
	return true;
}

bool MM_Arena_CaptainCommand(gentity_t *ent)
{
	if (!arena::IsArenaGametype())
		return false;
	if (!arena::IsConnected(ent))
		return true;
	if (gi.argc() < 1 || gi.argc() > 2) {
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("Usage: {} [player]\n", gi.argv(0)).c_str());
		return true;
	}
	arena::CaptainCommand(ent, gi.argc() == 2 ? gi.argv(1) : nullptr);
	return true;
}

bool MM_Arena_LockTeamCommand(gentity_t *ent, bool locked)
{
	if (!arena::IsArenaGametype())
		return false;
	if (!arena::IsConnected(ent))
		return true;
	if (gi.argc() < 1 || gi.argc() > 2) {
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("Usage: {} {}\n", gi.argv(0),
				locked ? "[password|red|blue]" : "[red|blue]").c_str());
		return true;
	}
	arena::LogicalTeam *team =
		arena::FindTeam(ent->client->resp.arena_team_id);
	bool side_argument = false;
	if (ent->client->sess.admin && gi.argc() == 2) {
		const std::string selection = arena::Lower(gi.argv(1));
		arena::Arena *a = arena::ArenaFor(ent->client);
		if (a && MM_ArenaUsesFixedTeams(a->settings.type) &&
			(selection == "red" || selection == "r" ||
				selection == "blue" || selection == "b")) {
			arena::EnsureFixedTeams(*a);
			side_argument = true;
			team = arena::FindTeam(
				selection == "red" || selection == "r"
					? a->fixed_red : a->fixed_blue);
		}
	}
	if (!team || (arena::Role(ent->client) == mm_arena_role_t::Coach &&
		!ent->client->sess.admin)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"You must be on an Arena team to use this command.\n");
		return true;
	}
	arena::SetTeamLocked(ent, *team, locked,
		locked && gi.argc() == 2 && !side_argument ? gi.argv(1) : "");
	return true;
}

bool MM_Arena_TeamNameCommand(gentity_t *ent)
{
	if (!arena::IsArenaGametype())
		return false;
	if (!arena::IsConnected(ent))
		return true;
	arena::TeamNameCommand(ent, arena::JoinCommandArguments(1));
	return true;
}

bool MM_Arena_TeamKickCommand(gentity_t *ent)
{
	if (!arena::IsArenaGametype())
		return false;
	if (!arena::IsConnected(ent))
		return true;
	if (gi.argc() < 1 || gi.argc() > 2) {
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("Usage: {} [player]\n", gi.argv(0)).c_str());
		return true;
	}
	arena::TeamKickCommand(ent, gi.argc() == 2 ? gi.argv(1) : nullptr);
	return true;
}

bool MM_Arena_TeamMuteCommand(gentity_t *ent, bool muted)
{
	if (!arena::IsArenaGametype())
		return false;
	if (!arena::IsConnected(ent))
		return true;
	if (gi.argc() != 1) {
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("Usage: {}\n", gi.argv(0)).c_str());
		return true;
	}
	arena::TeamMuteCommand(ent, muted);
	return true;
}

bool MM_Arena_SpectatorInviteCommand(gentity_t *ent, bool invited)
{
	if (!arena::IsArenaGametype())
		return false;
	if (!arena::IsConnected(ent))
		return true;
	const bool coach = invited && gi.argc() == 3 &&
		!Q_strcasecmp(gi.argv(2), "coach");
	if ((!invited && gi.argc() != 2) ||
		(invited && (gi.argc() < 2 || gi.argc() > 3 ||
			(gi.argc() == 3 && !coach)))) {
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("Usage: {} <player>{}\n", gi.argv(0),
				invited ? " [coach]" : "").c_str());
		return true;
	}
	arena::TeamInviteCommand(ent,
		invited ? arena::TeamInviteAction::InviteSpectator :
			arena::TeamInviteAction::RevokeSpectator,
		gi.argv(1), coach);
	return true;
}

bool MM_Arena_SpecWhoCommand(gentity_t *ent)
{
	if (!arena::IsArenaGametype())
		return false;
	if (!arena::IsConnected(ent))
		return true;
	if (gi.argc() != 1) {
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("Usage: {}\n", gi.argv(0)).c_str());
		return true;
	}
	arena::SpecWhoCommand(ent);
	return true;
}

bool MM_Arena_ReadyTeamCommand(gentity_t *ent)
{
	if (!arena::IsArenaGametype())
		return false;
	if (!arena::IsConnected(ent))
		return true;
	if (gi.argc() != 1) {
		gi.Client_Print(ent, PRINT_HIGH, "Usage: readyteam\n");
		return true;
	}
	arena::LogicalTeam *team =
		arena::FindTeam(ent->client->resp.arena_team_id);
	if (!team || arena::Role(ent->client) == mm_arena_role_t::Coach) {
		gi.Client_Print(ent, PRINT_HIGH,
			"You must be on an Arena team to use this command.\n");
		return true;
	}
	arena::ReadyLogicalTeam(ent, *team, true);
	return true;
}

bool MM_Arena_ReadyAllCommand(gentity_t *ent, bool ready)
{
	if (!arena::IsArenaGametype())
		return false;
	if (!arena::IsConnected(ent))
		return true;
	if (gi.argc() != 1) {
		gi.Client_Print(ent, PRINT_HIGH,
			ready ? "Usage: readyall\n" : "Usage: unreadyall\n");
		return true;
	}
	arena::Arena *a = arena::ArenaFor(ent->client);
	if (!a) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Enter an arena before changing its ready state.\n");
		return true;
	}
	if (a->state != mm_arena_state_t::Empty &&
		a->state != mm_arena_state_t::Warmup) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Ready status can only change between Arena matches.\n");
		return true;
	}
	int changed = 0;
	for (gentity_t *target : active_clients()) {
		if (!arena::IsConnected(target) ||
			target->client->resp.arena_id != a->id ||
			!target->client->resp.arena_team_id ||
			arena::Role(target->client) == mm_arena_role_t::Coach ||
			target->client->resp.ready == ready)
			continue;
		target->client->resp.ready = ready;
		changed++;
	}
	arena::ArenaPrint(*a, PRINT_HIGH,
		fmt::format("[ADMIN]: Marked {} player{} {}.\n", changed,
			changed == 1 ? "" : "s", ready ? "ready" : "not ready"));
	return true;
}

bool MM_Arena_AdminStart(gentity_t *ent)
{
	if (!arena::IsArenaGametype())
		return false;
	if (!arena::IsConnected(ent))
		return true;
	arena::Arena *a = arena::ArenaFor(ent->client);
	if (!a)
		gi.Client_Print(ent, PRINT_HIGH,
			"Enter an arena before forcing its match to start.\n");
	else
		arena::ForceStart(*a, ent);
	return true;
}

bool MM_Arena_AdminReset(gentity_t *ent)
{
	if (!arena::IsArenaGametype())
		return false;
	if (!arena::IsConnected(ent))
		return true;
	arena::Arena *a = arena::ArenaFor(ent->client);
	if (!a) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Enter an arena before resetting its match.\n");
		return true;
	}
	arena::ResetArenaSeries(*a);
	arena::ArenaPrint(*a, PRINT_HIGH,
		fmt::format("[ADMIN]: {} reset this arena match.\n",
			ent->client->resp.netname));
	return true;
}

bool MM_Arena_AdminEnd(gentity_t *ent)
{
	if (!arena::IsArenaGametype())
		return false;
	if (!arena::IsConnected(ent))
		return true;
	arena::Arena *a = arena::ArenaFor(ent->client);
	if (!a) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Enter an arena before ending its match.\n");
		return true;
	}
	if (!arena::IsActiveSeriesState(a->state) &&
		a->state != mm_arena_state_t::MatchOver) {
		gi.Client_Print(ent, PRINT_HIGH,
			"This arena match has not begun.\n");
		return true;
	}
	arena::ResetArenaSeries(*a);
	arena::ArenaPrint(*a, PRINT_HIGH,
		fmt::format("[ADMIN]: {} ended this arena match.\n",
			ent->client->resp.netname));
	return true;
}

bool MM_Arena_ChatRecipient(const gentity_t *sender, const gentity_t *recipient,
	mm_arena_chat_scope_t scope)
{
	if (!arena::IsConnected(sender) || !arena::IsConnected(recipient))
		return false;
	if (scope == mm_arena_chat_scope_t::World)
		return true;
	if (scope == mm_arena_chat_scope_t::Arena &&
		sender->client->resp.arena_id <= 0)
		return true;
	if (sender->client->resp.arena_id != recipient->client->resp.arena_id)
		return false;
	if (scope == mm_arena_chat_scope_t::Arena)
		return true;
	const arena::Arena *room = arena::ArenaForConst(sender->client);
	if (room && MM_ArenaTeamChatUsesArenaScope(room->settings.type))
		return true;
	if (!MM_Arena_SameSquad(sender->client, recipient->client))
		return false;
	const mm_arena_role_t a = arena::Role(sender->client);
	const mm_arena_role_t b = arena::Role(recipient->client);
	const bool sender_playing = a == mm_arena_role_t::Fighter &&
		!sender->client->eliminated && !sender->deadflag && sender->health > 0;
	const bool recipient_playing = b == mm_arena_role_t::Fighter &&
		!recipient->client->eliminated && !recipient->deadflag &&
		recipient->health > 0;
	return MM_ArenaTeamChatStatesMatch(sender_playing, recipient_playing,
		a == mm_arena_role_t::Coach, b == mm_arena_role_t::Coach);
}

bool MM_Arena_CanSendChat(gentity_t *sender, mm_arena_chat_scope_t scope)
{
	if (!arena::IsArenaGametype() || !arena::IsConnected(sender))
		return true;
	// RA3 team mute preserves tactical team chat while keeping non-captains
	// from speaking to the whole arena/world during competition play.
	if (scope == mm_arena_chat_scope_t::Team)
		return true;
	const arena::LogicalTeam *team =
		arena::FindTeam(sender->client->resp.arena_team_id, 0);
	const arena::Arena *a = arena::ArenaForConst(sender->client);
	if (!team || !a || !a->settings.competition_mode || !team->chat_muted ||
		arena::IsCaptain(sender, team))
		return true;
	gi.Client_Print(sender, PRINT_HIGH,
		"Your captain limited you to team chat during competition play.\n");
	return false;
}

bool MM_Arena_BypassChatFlood(const gentity_t *sender,
	mm_arena_chat_scope_t scope)
{
	if (!arena::IsArenaGametype() || !arena::IsConnected(sender))
		return false;
	const arena::Arena *a = arena::ArenaForConst(sender->client);
	return a && MM_ArenaTeamChatBypassesFlood(
		a->settings.type, a->settings.competition_mode,
		scope == mm_arena_chat_scope_t::Team);
}

bool MM_Arena_ReadyCommand(gentity_t *ent)
{
	if (!arena::IsArenaGametype())
		return false;
	if (!arena::IsConnected(ent))
		return true;
	if (gi.argc() < 1 || gi.argc() > 2) {
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("Usage: {} [0|1]\n", gi.argv(0)).c_str());
		return true;
	}
	bool ready = false;
	if (!MM_ArenaReadyCommandValue(
		gi.argc() == 2 ? std::string_view(gi.argv(1)) : std::string_view {},
		ent->client->resp.ready, ready)) {
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("Usage: {} [0|1]\n", gi.argv(0)).c_str());
		return true;
	}
	return MM_Arena_SetReady(ent, ready);
}

bool MM_Arena_SetReady(gentity_t *ent, bool ready)
{
	if (!arena::IsArenaGametype())
		return false;
	if (!arena::IsConnected(ent))
		return true;
	arena::Arena *a = arena::ArenaFor(ent->client);
	if (a && a->settings.type == mm_arena_type_t::Practice) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Practice runs continuously and does not use ready status.\n");
		return true;
	}
	if (!a || !ent->client->resp.arena_team_id ||
		arena::Role(ent->client) == mm_arena_role_t::Coach) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Join an Arena team before changing ready status.\n");
		return true;
	}
	if (!arena::RequireCompetitionCommand(ent, a, "Ready"))
		return true;
	if (a->state != mm_arena_state_t::Empty &&
		a->state != mm_arena_state_t::Warmup) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Ready status can only change between matches.\n");
		return true;
	}
	ent->client->resp.ready = ready;
	arena::ArenaPrint(*a, PRINT_HIGH,
		fmt::format("{} is {}ready.\n", ent->client->resp.netname,
			ready ? "" : "not "));
	return true;
}

bool MM_Arena_ToggleReady(gentity_t *ent)
{
	if (!arena::IsArenaGametype())
		return false;
	const bool ready = arena::IsConnected(ent) &&
		!ent->client->resp.ready;
	return MM_Arena_SetReady(ent, ready);
}

bool MM_Arena_CallTimeout(gentity_t *ent)
{
	if (!arena::IsArenaGametype())
		return false;
	if (!arena::IsConnected(ent))
		return true;
	if (gi.argc() != 1) {
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("Usage: {}\n", gi.argv(0)).c_str());
		return true;
	}
	arena::BeginTimeout(ent);
	return true;
}

bool MM_Arena_CallTimein(gentity_t *ent)
{
	if (!arena::IsArenaGametype())
		return false;
	if (!arena::IsConnected(ent))
		return true;
	if (gi.argc() != 1) {
		gi.Client_Print(ent, PRINT_HIGH,
			fmt::format("Usage: {}\n", gi.argv(0)).c_str());
		return true;
	}
	arena::BeginTimein(ent);
	return true;
}

bool MM_Arena_CastVote(gentity_t *ent, const char *choice)
{
	if (!arena::IsArenaGametype() || !arena::IsConnected(ent))
		return false;
	arena::Arena *a = arena::ArenaFor(ent->client);
	if (!a || !a->ballot.active)
		return false;
	int vote = 0;
	if (!MM_ParseVoteChoice(choice, vote)) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid vote. Use yes or no.\n");
		return true;
	}
	arena::CastVote(ent, vote > 0);
	return true;
}

void MM_Arena_Cmd(gentity_t *ent)
{
	if (!arena::IsConnected(ent))
		return;
	if (!arena::IsArenaGametype()) {
		if (g_gametype && GT_RAW(GT_ARENA))
			gi.Client_Print(ent, PRINT_HIGH,
				"Rocket Arena is inactive: this map is not "
				"RA2-compatible. Load a map with a valid worldspawn "
				"arena contract.\n");
		else
			gi.Client_Print(ent, PRINT_HIGH,
				"The arena command is only available in Rocket Arena.\n");
		return;
	}
	const int argc = gi.argc();
	const std::string sub = arena::Lower(argc >= 2 ? gi.argv(1) : "status");
	arena::Arena *a = arena::ArenaFor(ent->client);

	if (sub == "list" || sub == "arenas") {
		arena::PrintArenaList(ent);
		return;
	}
	if (sub == "go" || sub == "move" || sub == "moveto") {
		int id = 0;
		if (argc < 3 || !arena::ParseInt(gi.argv(2), id))
			gi.Client_Print(ent, PRINT_HIGH, "Usage: arena go <number>\n");
		else
			MM_Arena_MoveTo(ent, id, true);
		return;
	}
	if (sub == "leave" || sub == "leavearena") {
		if (arena::FighterRosterLocked(ent))
			gi.Client_Print(ent, PRINT_HIGH,
				"You cannot leave an active Arena match.\n");
		else
			arena::LeaveTeam(ent, true, false);
		return;
	}
	if (sub == "status" || sub == "settings") {
		if (a)
			arena::PrintSettings(ent, *a);
		else
			arena::PrintArenaList(ent);
		return;
	}
	if (sub == "line" && a) {
		if (a->settings.type == mm_arena_type_t::Practice) {
			gi.Client_Print(ent, PRINT_HIGH,
				"Practice is always active and has no team line.\n");
			return;
		}
		if (arena::Role(ent->client) == mm_arena_role_t::Coach) {
			gi.Client_Print(ent, PRINT_HIGH,
				"Coaches must leave coaching before joining a team line.\n");
			return;
		}
		if (arena::FighterRosterLocked(ent)) {
			gi.Client_Print(ent, PRINT_HIGH,
				"You cannot change line status during an active match.\n");
			return;
		}
		if (argc >= 3) {
			const auto enabled = MM_ParseBoolArg(gi.argv(2));
			if (!enabled) {
				gi.Client_Print(ent, PRINT_HIGH,
					"Usage: arena line [on|off]\n");
				return;
			}
			ent->client->resp.arena_line_enabled = *enabled;
		} else
			ent->client->resp.arena_line_enabled =
				!ent->client->resp.arena_line_enabled;
		arena::ProjectRole(ent,
			ent->client->resp.arena_line_enabled && ent->client->resp.arena_team_id
				? mm_arena_role_t::Queued : mm_arena_role_t::Observer,
			TEAM_SPECTATOR, false);
		gi.Client_Print(ent, PRINT_HIGH,
			ent->client->resp.arena_line_enabled
				? "You entered the arena line.\n" : "You left the arena line.\n");
		return;
	}
	if (sub == "queue" || sub == "teams") {
		if (a)
			arena::PrintLine(ent, *a);
		return;
	}
	if (sub == "create") {
		arena::CreatePlayerTeam(ent, argc >= 3 ? gi.argv(2) : "");
		return;
	}
	if (sub == "teamleave") {
		if (a && a->settings.type == mm_arena_type_t::Practice)
			gi.Client_Print(ent, PRINT_HIGH,
				"Practice has no team to leave; use `arena leave` for the lobby.\n");
		else if (arena::FighterRosterLocked(ent))
			gi.Client_Print(ent, PRINT_HIGH,
				"You cannot leave an active Arena match.\n");
		else
			arena::LeaveTeam(ent, false, false);
		return;
	}
	if (sub == "join") {
		if (!a) {
			gi.Client_Print(ent, PRINT_HIGH, "Enter an arena first.\n");
			return;
		}
		arena::EnsureFixedTeams(*a);
		const std::string target = arena::Lower(argc >= 3 ? gi.argv(2) : "");
		arena::LogicalTeam *team = nullptr;
		if (target == "red")
			team = arena::FindTeam(a->fixed_red);
		else if (target == "blue")
			team = arena::FindTeam(a->fixed_blue);
		else if (argc >= 3)
			team = arena::FindJoinTarget(ent, gi.argv(2));
		if (!team) {
			gi.Client_Print(ent, PRINT_HIGH,
				"Usage: arena join <team-id|player|red|blue> [password]\n");
			return;
		}
		arena::JoinLogicalTeam(ent, *team, argc >= 4 ? gi.argv(3) : nullptr);
		return;
	}
	if (sub == "ready") {
		bool ready = false;
		if (argc < 2 || argc > 3 ||
			!MM_ArenaReadyCommandValue(
				argc == 3 ? std::string_view(gi.argv(2)) : std::string_view {},
				ent->client->resp.ready, ready)) {
			gi.Client_Print(ent, PRINT_HIGH,
				"Usage: arena ready [0|1]\n");
		} else
			MM_Arena_SetReady(ent, ready);
		return;
	}
	if (sub == "propose") {
		if (argc < 4)
			gi.Client_Print(ent, PRINT_HIGH,
				"Usage: arena propose <setting> <value>\n");
		else
			arena::Propose(ent, gi.argv(2), gi.argv(3));
		return;
	}
	if (sub == "vote") {
		if (argc != 3) {
			gi.Client_Print(ent, PRINT_HIGH,
				"Usage: arena vote <yes|no>\n");
			return;
		}
		if (!MM_Arena_CastVote(ent, gi.argv(2)))
			gi.Client_Print(ent, PRINT_HIGH,
				"There is no arena vote in progress.\n");
		return;
	}
	if (sub == "say" || sub == "say_team" || sub == "say_world") {
		if (argc < 3) {
			gi.Client_Print(ent, PRINT_HIGH, "Usage: arena say <message>\n");
			return;
		}
		std::string message;
		for (int i = 2; i < argc; i++) {
			if (!message.empty())
				message.push_back(' ');
			message += gi.argv(i);
		}
		MM_SendScopedChat(ent,
			sub == "say_world" ? mm_arena_chat_scope_t::World :
			(sub == "say_team" ? mm_arena_chat_scope_t::Team :
				mm_arena_chat_scope_t::Arena),
			message, sub == "say_team");
		return;
	}
	if (sub == "timeout") {
		if (argc != 2)
			gi.Client_Print(ent, PRINT_HIGH, "Usage: arena timeout\n");
		else
			arena::BeginTimeout(ent);
		return;
	}
	if (sub == "timein") {
		if (argc != 2)
			gi.Client_Print(ent, PRINT_HIGH, "Usage: arena timein\n");
		else
			arena::BeginTimein(ent);
		return;
	}

	if (sub == "captain") {
		if (argc > 3)
			gi.Client_Print(ent, PRINT_HIGH,
				"Usage: arena captain [player]\n");
		else
			arena::CaptainCommand(ent, argc == 3 ? gi.argv(2) : nullptr);
		return;
	}
	if (sub == "name") {
		arena::TeamNameCommand(ent, arena::JoinCommandArguments(2));
		return;
	}
	if (sub == "lock" || sub == "unlock") {
		arena::LogicalTeam *team =
			arena::FindTeam(ent->client->resp.arena_team_id);
		if (!MM_ArenaTeamLockArgumentsValid(sub == "lock", argc))
			gi.Client_Print(ent, PRINT_HIGH,
				sub == "lock" ? "Usage: arena lock [password]\n" :
					"Usage: arena unlock\n");
		else if (!team)
			gi.Client_Print(ent, PRINT_HIGH,
				"Join an Arena team before changing its lock.\n");
		else
			arena::SetTeamLocked(ent, *team, sub == "lock",
				sub == "lock" && argc >= 3 ? gi.argv(2) : "");
		return;
	}
	if (sub == "teammute" || sub == "teamunmute" || sub == "mute" ||
		sub == "unmute") {
		if (argc != 2)
			gi.Client_Print(ent, PRINT_HIGH,
				"Usage: arena teammute|teamunmute\n");
		else
			arena::TeamMuteCommand(ent,
				sub == "teammute" || sub == "mute");
		return;
	}
	if (sub == "kick") {
		if (argc > 3)
			gi.Client_Print(ent, PRINT_HIGH,
				"Usage: arena kick [player]\n");
		else
			arena::TeamKickCommand(ent, argc == 3 ? gi.argv(2) : nullptr);
		return;
	}
	if (sub == "invite" || sub == "revoke") {
		if (argc != 3)
			gi.Client_Print(ent, PRINT_HIGH,
				sub == "invite" ? "Usage: arena invite <player>\n" :
					"Usage: arena revoke <player>\n");
		else
			arena::TeamInviteCommand(ent,
				sub == "invite"
					? arena::TeamInviteAction::InviteMember
					: arena::TeamInviteAction::RevokeMember,
				gi.argv(2), false);
		return;
	}
	if (sub == "specinvite") {
		const bool coach = argc == 4 &&
			!Q_strcasecmp(gi.argv(3), "coach");
		if (argc < 3 || argc > 4 || (argc == 4 && !coach))
			gi.Client_Print(ent, PRINT_HIGH,
				"Usage: arena specinvite <player> [coach]\n");
		else
			arena::TeamInviteCommand(ent,
				arena::TeamInviteAction::InviteSpectator,
				gi.argv(2), coach);
		return;
	}
	if (sub == "specrevoke") {
		if (argc != 3)
			gi.Client_Print(ent, PRINT_HIGH,
				"Usage: arena specrevoke <player>\n");
		else
			arena::TeamInviteCommand(ent,
				arena::TeamInviteAction::RevokeSpectator,
				gi.argv(2), false);
		return;
	}
	if (sub == "coach") {
		if (argc != 3)
			gi.Client_Print(ent, PRINT_HIGH,
				"Usage: arena coach <team|player>\n");
		else
			arena::CoachCommand(ent, gi.argv(2));
		return;
	}
	if (sub == "specwho") {
		if (argc != 2)
			gi.Client_Print(ent, PRINT_HIGH,
				"Usage: arena specwho\n");
		else
			arena::SpecWhoCommand(ent);
		return;
	}
	if (sub == "admin" && ent->client->sess.admin) {
		int id = 0;
		if (argc < 4 || !arena::ParseInt(gi.argv(2), id) ||
			!(a = arena::FindArena(id))) {
			gi.Client_Print(ent, PRINT_HIGH,
				"Usage: arena admin <arena> <setting|reset|start|abort> [value]\n");
			return;
		}
		const std::string action = arena::Lower(gi.argv(3));
		if (action == "reset") {
			arena::ApplyResolvedSettings(*a, a->defaults);
		} else if (action == "start") {
			arena::ForceStart(*a, ent);
		} else if (action == "abort") {
			arena::ResetArenaSeries(*a);
			arena::ArenaPrint(*a, PRINT_HIGH,
				fmt::format("[ADMIN]: {} aborted this arena match.\n",
					ent->client->resp.netname));
		} else {
			mm_arena_settings_t proposed = a->settings;
			if (argc >= 5 && arena::ApplySetting(proposed, gi.argv(3), gi.argv(4),
				static_cast<int>(game.maxclients)))
				arena::ApplyResolvedSettings(*a, proposed);
			else
				gi.Client_Print(ent, PRINT_HIGH, "Invalid arena admin setting.\n");
		}
		return;
	}

	gi.Client_Print(ent, PRINT_HIGH,
		"arena: list, go, status, create, join, teamleave, line, leave\n"
		"match: ready, propose, vote, timeout, timein\n"
		"team: name, lock, captain, invite, coach, kick\n");
}

static void MM_Arena_UpdateMenu(gentity_t *ent)
{
	if (!arena::IsConnected(ent) || !ent->client->menu ||
		!ent->client->menu->arg)
		return;
	auto *state =
		static_cast<arena::MenuState *>(ent->client->menu->arg);
	arena::PopulateMenu(ent, ent->client->menu->entries, *state);

	if (ent->client->menu->cur >= 0 &&
		ent->client->menu->cur < ent->client->menu->num &&
		!ent->client->menu->entries[ent->client->menu->cur].SelectFunc) {
		ent->client->menu->cur = -1;
		for (int row = 0; row < ent->client->menu->num; row++) {
			if (!ent->client->menu->entries[row].SelectFunc)
				continue;
			ent->client->menu->cur = row;
			break;
		}
	}
}

static void MM_Arena_OpenMenuPage(gentity_t *ent, int page, bool rooms)
{
	if (!arena::IsArenaGametype() || !arena::IsConnected(ent))
		return;
	std::array<menu_t, arena::kMenuRows> entries {};
	auto *state = static_cast<arena::MenuState *>(
		gi.TagMalloc(sizeof(arena::MenuState), TAG_LEVEL));
	if (!state)
		return;
	new (state) arena::MenuState {};
	state->rooms = rooms;
	state->page = page;
	arena::PopulateMenu(ent, entries.data(), *state);

	P_Menu_Close(ent);
	if (!P_Menu_Open(ent, entries.data(), -1,
		static_cast<int>(entries.size()), state, MM_Arena_UpdateMenu))
		gi.TagFree(state);
}

void MM_Arena_OpenJoinMenu(gentity_t *ent, menu_hnd_t *)
{
	const arena::Arena *current = arena::IsConnected(ent)
		? arena::ArenaFor(ent->client) : nullptr;
	const bool rooms = !current ||
		current->settings.type == mm_arena_type_t::Practice;
	MM_Arena_OpenMenuPage(ent, 0, rooms);
}

void MM_Arena_OpenRoomMenu(gentity_t *ent, menu_hnd_t *)
{
	MM_Arena_OpenMenuPage(ent, 0, true);
}

void MM_Arena_ScoreboardMessage(gentity_t *viewer, gentity_t *)
{
	std::string layout;
	const arena::Arena *a = viewer && viewer->client
		? arena::ArenaForConst(viewer->client) : nullptr;
	if (!a) {
		arena::AppendLayout(layout,
			fmt::format("xv 0 yv -48 cstring2 \"Rocket Arena on {}\" "
				"xv 0 yv -36 cstring \"Select an arena\" ",
				arena::SafeText(level.level_name)));
		int y = -18;
		for (int id = 1; id <= arena::s_arena_count && id <= 16; id++, y += 9) {
			const arena::Arena &item = arena::s_arenas[id];
			arena::AppendLayout(layout,
				fmt::format("xv -120 yv {} string2 \"{}: {}\" "
					"xv 120 yv {} string \"{} {}\" ",
					y, id, arena::SafeText(item.name, 28), y,
					arena::ArenaPopulation(id), arena::StateName(item.state)));
		}
		if (arena::s_arena_count > 16)
			arena::AppendLayout(layout,
				fmt::format("xv 0 yv 132 cstring \"...and {} more; use the Join menu\" ",
					arena::s_arena_count - 16));
	} else {
		const bool practice = a->settings.type == mm_arena_type_t::Practice;
		const std::string progress =
			a->settings.type == mm_arena_type_t::RedRover
				? fmt::format("continuous rover | round {}", a->round + 1)
				: (practice
					? std::string("continuous free practice")
					: fmt::format("best of {} | round {}",
						a->settings.rounds, a->round));
		arena::AppendLayout(layout,
			fmt::format("xv 0 yv -48 cstring2 \"{} - {}\" "
				"xv 0 yv -38 cstring \"{} | {}\" ",
				arena::SafeText(a->name), arena::TypeName(a->settings.type),
				arena::StateName(a->state), progress));
		if (practice)
			arena::AppendLayout(layout,
				"xv 0 yv -22 cstring2 \"PLAYERS\" ");
		else
			arena::AppendLayout(layout,
				fmt::format("xv -40 yv -22 string2 \"RED {}\" "
					"xv 200 yv -22 string2 \"BLUE {}\" ",
					a->red_score, a->blue_score));
		int red_row = 0;
		int blue_row = 0;
		int red_total = 0;
		int blue_total = 0;
		for (gentity_t *ent : active_clients()) {
			if (!arena::IsConnected(ent) ||
				ent->client->resp.arena_id != a->id ||
				arena::Role(ent->client) != mm_arena_role_t::Fighter)
				continue;
			const bool red = practice
				? ((red_total + blue_total) & 1) == 0
				: ent->client->resp.arena_side == TEAM_RED;
			if (red)
				red_total++;
			else
				blue_total++;
			int &row = red ? red_row : blue_row;
			if (row >= 8)
				continue;
			arena::AppendLayout(layout, fmt::format("ctf {} {} {} {} {} \"\" ",
				red ? -40 : 200, -10 + row * 9, ent->s.number - 1,
				ent->client->resp.score, std::min(ent->client->ping, 999)));
			row++;
		}
		int y = 66;
		int rows = 0;
		int waiting_total = 0;
		arena::AppendLayout(layout, "xv 0 yv 54 cstring2 \"WAITING / OBSERVING\" ");
		for (gentity_t *ent : active_clients()) {
			if (!arena::IsConnected(ent) ||
				ent->client->resp.arena_id != a->id ||
				arena::Role(ent->client) == mm_arena_role_t::Fighter)
				continue;
			waiting_total++;
			if (rows >= 10)
				continue;
			const int x = (rows & 1) ? 200 : -40;
			const int row_y = y + (rows / 2) * 9;
			arena::AppendLayout(layout, fmt::format("ctf {} {} {} {} {} \"\" ",
				x, row_y, ent->s.number - 1, ent->client->resp.score,
				std::min(ent->client->ping, 999)));
			rows++;
		}
		const int hidden_red = std::max(0, red_total - red_row);
		const int hidden_blue = std::max(0, blue_total - blue_row);
		const int hidden_waiting = std::max(0, waiting_total - rows);
		if (hidden_red || hidden_blue || hidden_waiting) {
			if (practice)
				arena::AppendLayout(layout,
					fmt::format("xv 0 yv 120 cstring \"Not shown: {} players, {} waiting\" ",
						hidden_red + hidden_blue, hidden_waiting));
			else
				arena::AppendLayout(layout,
					fmt::format("xv 0 yv 120 cstring \"Not shown: {} Red, {} Blue, {} waiting\" ",
						hidden_red, hidden_blue, hidden_waiting));
		}
	}
	gi.WriteByte(svc_layout);
	gi.WriteString(layout.c_str());
}

void MM_Arena_SetHudStats(gentity_t *ent)
{
	if (!arena::IsArenaGametype() || !arena::IsConnected(ent))
		return;
	const arena::Arena *a = arena::ArenaForConst(ent->client);
	std::string text;
	if (!a) {
		text = "ROCKET ARENA LOBBY";
		ent->client->ps.stats[STAT_MINISCORE_FIRST_SCORE] = -999;
		ent->client->ps.stats[STAT_MINISCORE_SECOND_SCORE] = -999;
		ent->client->ps.stats[STAT_MINISCORE_FIRST_PIC] = 0;
		ent->client->ps.stats[STAT_MINISCORE_SECOND_PIC] = 0;
		ent->client->ps.stats[STAT_MINISCORE_FIRST_POS] = 0;
		ent->client->ps.stats[STAT_MINISCORE_SECOND_POS] = 0;
	} else {
		text = fmt::format("A{} {} | {}", a->id, arena::SafeText(a->name, 22),
			arena::RoleLabel(ent->client));
		if (arena::Role(ent->client) == mm_arena_role_t::Queued)
			text += fmt::format(" #{}", MM_Arena_QueuePosition(ent->client));
		if (a->settings.type == mm_arena_type_t::RedRover ||
			a->settings.type == mm_arena_type_t::Practice) {
			std::vector<gentity_t *> leaders;
			for (gentity_t *candidate : active_clients())
				if (arena::IsConnected(candidate) &&
					candidate->client->resp.arena_id == a->id &&
					arena::Role(candidate->client) == mm_arena_role_t::Fighter)
					leaders.push_back(candidate);
			std::sort(leaders.begin(), leaders.end(),
				[](const gentity_t *first, const gentity_t *second) {
					if (first->client->resp.score != second->client->resp.score)
						return first->client->resp.score >
							second->client->resp.score;
					return first->s.number < second->s.number;
				});
			gentity_t *first = leaders.empty() ? nullptr : leaders[0];
			gentity_t *second = leaders.size() < 2 ? nullptr : leaders[1];
			ent->client->ps.stats[STAT_MINISCORE_FIRST_SCORE] =
				first ? first->client->resp.score : -999;
			ent->client->ps.stats[STAT_MINISCORE_SECOND_SCORE] =
				second ? second->client->resp.score : -999;
			ent->client->ps.stats[STAT_MINISCORE_FIRST_PIC] =
				first ? first->client->pers.skin_icon_index : 0;
			ent->client->ps.stats[STAT_MINISCORE_SECOND_PIC] =
				second ? second->client->pers.skin_icon_index : 0;
			ent->client->ps.stats[STAT_MINISCORE_FIRST_POS] =
				first == ent ? ii_highlight : 0;
			ent->client->ps.stats[STAT_MINISCORE_SECOND_POS] =
				second == ent ? ii_highlight : 0;
		} else {
			ent->client->ps.stats[STAT_MINISCORE_FIRST_SCORE] = a->red_score;
			ent->client->ps.stats[STAT_MINISCORE_SECOND_SCORE] = a->blue_score;
			ent->client->ps.stats[STAT_MINISCORE_FIRST_PIC] = ii_teams_red_default;
			ent->client->ps.stats[STAT_MINISCORE_SECOND_PIC] = ii_teams_blue_default;
			ent->client->ps.stats[STAT_MINISCORE_FIRST_POS] =
				ent->client->resp.arena_side == TEAM_RED ? ii_highlight : 0;
			ent->client->ps.stats[STAT_MINISCORE_SECOND_POS] =
				ent->client->resp.arena_side == TEAM_BLUE ? ii_highlight : 0;
		}
	}
	const int client_num = arena::ClientNumber(ent);
	const int cs = client_num >= 0 ? MM_PovConfigStringForClient(
		static_cast<size_t>(client_num), game.maxclients,
		mm_pov_configstring_lane_t::Primary) : 0;
	if (cs) {
		gi.configstring(cs, text.c_str());
		ent->client->ps.stats[STAT_ARENA_ROLE] = cs;
	}
}

void MM_Arena_SetTimerHudStats(gentity_t *ent)
{
	if (!arena::IsArenaGametype() || !arena::IsConnected(ent))
		return;

	ent->client->ps.stats[STAT_COUNTDOWN] = 0;
	const int client_num = arena::ClientNumber(ent);
	const int cs = client_num >= 0 ? MM_PovConfigStringForClient(
		static_cast<size_t>(client_num), game.maxclients,
		mm_pov_configstring_lane_t::Secondary) : 0;
	const arena::Arena *a = arena::ArenaForConst(ent->client);
	if (!a || !cs) {
		ent->client->ps.stats[STAT_MATCH_STATE] = 0;
		return;
	}

	const auto remaining_seconds = [](gtime_t deadline) {
		if (!deadline || deadline <= level.time)
			return 0;
		const int64_t milliseconds =
			(deadline - level.time).milliseconds();
		return static_cast<int>(
			std::max<int64_t>(0, milliseconds + 999) / 1000);
	};
	std::string text;
	switch (a->state) {
	case mm_arena_state_t::Empty:
	case mm_arena_state_t::Warmup:
		text = "WARMUP";
		break;
	case mm_arena_state_t::MatchCountdown:
	case mm_arena_state_t::RoundCountdown:
	{
		const int seconds = remaining_seconds(a->state_timer);
		ent->client->ps.stats[STAT_COUNTDOWN] = seconds;
		text = fmt::format("COUNTDOWN ({})", seconds);
		break;
	}
	case mm_arena_state_t::Running:
		if (a->settings.type == mm_arena_type_t::Practice)
			text = "PRACTICE";
		else if (roundtimelimit && roundtimelimit->value > 0 &&
			a->state_timer)
			text = fmt::format("ROUND {}", MM_FormatMatchTime(
				remaining_seconds(a->state_timer) * 1000));
		else
			text = "FIGHT";
		break;
	case mm_arena_state_t::RoundOver:
		text = "ROUND OVER";
		break;
	case mm_arena_state_t::MatchOver:
		text = "MATCH OVER";
		break;
	case mm_arena_state_t::Paused:
	{
		const int seconds = remaining_seconds(a->state_timer);
		if (a->resume_countdown)
			ent->client->ps.stats[STAT_COUNTDOWN] = seconds;
		text = fmt::format("{} ({})",
			a->resume_countdown ? "TIME-IN" : "TIMEOUT",
			MM_FormatMatchTime(seconds * 1000));
		break;
	}
	}

	if (!ent->client->sess.pc.show_timer) {
		ent->client->ps.stats[STAT_MATCH_STATE] = 0;
		return;
	}
	gi.configstring(cs, text.c_str());
	ent->client->ps.stats[STAT_MATCH_STATE] = cs;
}

bool MM_Arena_ClientCanVote(const gclient_t *client)
{
	return arena::IsArenaGametype() && client && client->resp.arena_id > 0;
}

bool MM_Arena_SameSquad(const gclient_t *first, const gclient_t *second)
{
	return MM_Arena_SameTeam(first, second);
}

int MM_Arena_QueuePosition(const gclient_t *client)
{
	const arena::Arena *a = arena::ArenaForConst(client);
	const arena::LogicalTeam *own =
		client ? arena::FindTeam(client->resp.arena_team_id, 0) : nullptr;
	if (!a || !own || own->id == a->active_red || own->id == a->active_blue)
		return 0;
	std::vector<const arena::LogicalTeam *> teams;
	for (uint16_t id = 1; id <= arena::kMaxLogicalTeams; id++) {
		const arena::LogicalTeam &team = arena::s_teams[id];
		if (team.valid && !team.fixed && team.arena_id == a->id &&
			team.id != a->active_red && team.id != a->active_blue &&
			arena::TeamEligible(*a, team.id))
			teams.push_back(&team);
	}
	std::sort(teams.begin(), teams.end(),
		[](const arena::LogicalTeam *x, const arena::LogicalTeam *y) {
			return x->queue_order < y->queue_order;
		});
	for (size_t i = 0; i < teams.size(); i++)
		if (teams[i]->id == own->id)
			return static_cast<int>(i + 1);
	return 0;
}

bool MM_Arena_GetSpawnLoadout(const gclient_t *client,
	mm_arena_loadout_t &loadout)
{
	const arena::Arena *a = arena::ArenaForConst(client);
	if (!arena::IsArenaGametype() || !client || !a)
		return false;
	const mm_arena_settings_t &s = a->settings;
	// RA3 has distinct plasma/BFG reserves; Q2 shares cells. Preserve each knob
	// as a minimum shared-cell reserve while its corresponding weapon is enabled.
	int cell_reserve = s.cells;
	if (s.weapon_mask & MM_ARENA_WEAPON_PLASMA)
		cell_reserve = std::max(cell_reserve, s.plasma_ammo);
	if (s.weapon_mask & MM_ARENA_WEAPON_BFG)
		cell_reserve = std::max(cell_reserve, s.bfg_ammo);
	loadout.health = s.health;
	loadout.armor = s.armor;
	loadout.weapon_mask = s.weapon_mask;
	loadout.shells = s.shells;
	loadout.bullets = s.bullets;
	loadout.grenades = s.grenades;
	loadout.rockets = s.rockets;
	loadout.cells = cell_reserve;
	loadout.slugs = s.slugs;
	loadout.infinite_ammo =
		s.excessive || s.type == mm_arena_type_t::Practice;
	return true;
}

bool MM_Arena_FallingDamageEnabled(const gentity_t *ent)
{
	if (!arena::IsArenaGametype())
		return true;
	const arena::Arena *a = ent && ent->client
		? arena::ArenaForConst(ent->client) : nullptr;
	return a ? a->settings.falling_damage :
		(g_arena_falling_damage && g_arena_falling_damage->integer != 0);
}

bool MM_Arena_FallingDamageEnabled()
{
	if (!arena::IsArenaGametype())
		return false;
	return g_arena_falling_damage && g_arena_falling_damage->integer != 0;
}

bool MM_Arena_ExcessiveEnabled(const gentity_t *ent)
{
	if (!arena::IsArenaGametype() || !ent || !ent->client)
		return false;
	const arena::Arena *a = arena::ArenaForConst(ent->client);
	return a && a->settings.excessive;
}

bool MM_Arena_InfiniteAmmoEnabled(const gentity_t *ent)
{
	if (!arena::IsArenaGametype() || !ent || !ent->client)
		return false;
	const arena::Arena *a = arena::ArenaForConst(ent->client);
	return a && (a->settings.excessive ||
		a->settings.type == mm_arena_type_t::Practice);
}

bool MM_Arena_DamageScoringEnabled(const gentity_t *attacker)
{
	if (!arena::IsArenaGametype() || !attacker || !attacker->client)
		return false;
	const arena::Arena *a = arena::ArenaForConst(attacker->client);
	return a && a->settings.damage_scoring &&
		a->settings.type != mm_arena_type_t::Practice &&
		a->settings.type != mm_arena_type_t::RedRover;
}

bool MM_Arena_GrappleEnabled(const gentity_t *ent)
{
	if (!arena::IsArenaGametype() || !ent || !ent->client)
		return false;
	const arena::Arena *a = arena::ArenaForConst(ent->client);
	return a && a->settings.grapple;
}

bool MM_Arena_FastSwitchEnabled(const gentity_t *ent)
{
	if (!arena::IsArenaGametype() || !ent || !ent->client)
		return false;
	const arena::Arena *a = arena::ArenaForConst(ent->client);
	return a && a->settings.fast_switch;
}

int MM_Arena_RocketSpeed(const gentity_t *ent, int fallback)
{
	if (!arena::IsArenaGametype() || !ent || !ent->client)
		return fallback;
	const arena::Arena *a = arena::ArenaForConst(ent->client);
	return a ? a->settings.rocket_speed : fallback;
}

float MM_Arena_WeaponFireRateScale(const gentity_t *ent)
{
	// RA3 excessive halves refire delays.  The weapon layer applies the scale
	// to timing only; projectile speed and ammo policy use their own hooks.
	return MM_Arena_ExcessiveEnabled(ent) ? 0.5f : 1.0f;
}

bool MM_Arena_ForceFirstPerson(const gentity_t *viewer)
{
	if (!arena::IsArenaGametype() || !viewer || !viewer->client ||
		viewer->client->resp.arena_id <= 0)
		return false;
	const arena::Arena *a = arena::ArenaForConst(viewer->client);
	return a && a->settings.competition_mode &&
		(arena::Role(viewer->client) != mm_arena_role_t::Fighter ||
		 viewer->client->eliminated || viewer->deadflag);
}

bool MM_Arena_FreecamAllowed(const gentity_t *viewer)
{
	if (!arena::IsArenaGametype() || !viewer || !viewer->client ||
		viewer->client->resp.arena_id <= 0)
		return true;
	const arena::Arena *a = arena::ArenaForConst(viewer->client);
	const bool competition_observer = a && a->settings.competition_mode &&
		(arena::Role(viewer->client) != mm_arena_role_t::Fighter ||
		 viewer->client->eliminated || viewer->deadflag);
	const gentity_t *target = viewer->client->follow_target;
	const bool has_permitted_target =
		arena::IsConnected(target) && ClientIsPlaying(target->client) &&
		!target->client->eliminated && !target->deadflag &&
		MM_Arena_CanFollow(viewer, target);
	return MM_ArenaFreecamAllowedByCompetition(
		competition_observer, has_permitted_target);
}
