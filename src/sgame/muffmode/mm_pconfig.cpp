// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_announcer_rules.h"
#include "muffmode/mm_client_profile.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_pconfig.h"
#include "muffmode/mm_pconfig_rules.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_skin.h"
#include "muffmode/mm_util.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

//=======================================================================
// PLAYER CONFIGS
//=======================================================================

namespace muffmode::pconfig {

constexpr long k_max_player_config_file_length = 0x40000;
constexpr const char *k_player_config_dir = "baseq2/pcfg";
constexpr size_t k_max_player_config_stem_length = 251; // 255 minus ".cfg".

enum class config_line_result_t {
	ignored,
	applied,
	invalid
};

enum class save_result_t {
	saved,
	session_only,
	failed
};

struct config_load_stats_t {
	int applied = 0;
	int invalid = 0;
	uint32_t loaded_fields = 0;
};

constexpr uint32_t k_config_field_show_id = bit_v<0>;
constexpr uint32_t k_config_field_show_timer = bit_v<1>;
constexpr uint32_t k_config_field_show_fragmessages = bit_v<2>;
constexpr uint32_t k_config_field_announcer_enabled = bit_v<3>;
constexpr uint32_t k_config_field_killbeep = bit_v<4>;
constexpr uint32_t k_config_field_follow_killer = bit_v<5>;
constexpr uint32_t k_config_field_follow_leader = bit_v<6>;
constexpr uint32_t k_config_field_follow_powerup = bit_v<7>;
constexpr uint32_t k_config_field_enemy_skin = bit_v<8>;
constexpr uint32_t k_config_field_team_skin = bit_v<9>;
constexpr uint32_t k_config_field_follow_first_person = bit_v<10>;
constexpr uint32_t k_config_field_show_match_info = bit_v<11>;
int ClientSlotNumber(const gentity_t *ent)
{
	const uint32_t max_clients = static_cast<uint32_t>(game.maxclients);
	if (!ent || ent->s.number < 1 || ent->s.number > max_clients)
		return 0;

	return static_cast<int>(ent->s.number - 1);
}

const char *BoolText(bool value) noexcept
{
	return value ? "ON" : "OFF";
}

constexpr std::array<const char *, 5> k_kill_beep_names = { "off", "clang", "beep-boop", "insane", "tang-tang" };

bool RequireCommandArgc(gentity_t *ent, int min_expected, int max_expected, const char *usage)
{
	if (!ent || !ent->client)
		return false;

	if (MM_IsArgcInRangeValid(gi.argc(), min_expected, max_expected))
		return true;

	gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {}\n", usage);
	return false;
}

bool BeginPreferenceCommand(gentity_t *ent)
{
	return ent && ent->client && !CheckFlood(ent);
}

std::optional<bool> ReadOptionalBoolCommandArg(gentity_t *ent, bool current_value)
{
	if (!RequireCommandArgc(ent, 1, 2, G_Fmt("{} [0|1|on|off]", gi.argv(0)).data()))
		return std::nullopt;

	if (gi.argc() == 1)
		return !current_value;

	const auto parsed = ParseBoolToken(gi.argv(1));
	if (!parsed) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid {} value. Use on/off or 1/0.\n", gi.argv(0));
		return std::nullopt;
	}

	return parsed;
}

bool SetBoolPreference(gentity_t *ent, mm_pconfig_bool_setting_t setting, const char *label)
{
	const auto value = ReadOptionalBoolCommandArg(ent, MM_PConfigGetBool(ent, setting));
	if (!value)
		return false;

	MM_PConfigSetBool(ent, setting, *value);
	gi.LocClient_Print(ent, PRINT_HIGH, "{}: {}\n", label, BoolText(*value));
	return true;
}

std::optional<std::filesystem::path> ConfigPathForClient(gentity_t *ent, bool log_failure)
{
	if (!ent || !ent->client || (ent->svflags & SVF_BOT))
		return std::nullopt;

	if (!ent->client->pers.social_id[0]) {
		if (log_failure)
			gi.Com_PrintFmt("{}: Cannot load player config for client {} without a stable social ID.\n", __FUNCTION__, ClientSlotNumber(ent));
		return std::nullopt;
	}

	const std::string_view social_id(ent->client->pers.social_id);
	const auto config_stem = EncodeSocialIdConfigStem(social_id, MAX_INFO_VALUE - 1, k_max_player_config_stem_length);
	if (!config_stem) {
		if (log_failure)
			gi.Com_PrintFmt("{}: Cannot load player config for unsupported social ID on client {}.\n", __FUNCTION__, ClientSlotNumber(ent));
		return std::nullopt;
	}

	return std::filesystem::path(k_player_config_dir) / (*config_stem + ".cfg");
}

std::optional<std::string> ReadConfigText(FILE *file)
{
	if (std::fseek(file, 0, SEEK_END) != 0)
		return std::nullopt;

	const long file_length = std::ftell(file);
	if (file_length < 0 || file_length > k_max_player_config_file_length)
		return std::nullopt;

	if (std::fseek(file, 0, SEEK_SET) != 0)
		return std::nullopt;

	std::string buffer(static_cast<size_t>(file_length), '\0');
	if (!buffer.empty()) {
		const size_t read_length = std::fread(&buffer[0], 1, buffer.size(), file);
		if (read_length != buffer.size())
			return std::nullopt;
	}

	return buffer;
}

config_line_result_t ApplyBoolLine(bool &field, std::string_view args, const char *name, int line_number)
{
	std::string_view value;
	if (!ReadSingleConfigArg(args, value)) {
		gi.Com_PrintFmt("{}:{}: expected one boolean value.\n", name, line_number);
		return config_line_result_t::invalid;
	}

	const auto parsed = ParseBoolToken(value);
	if (!parsed) {
		gi.Com_PrintFmt("{}:{}: invalid boolean value \"{}\".\n", name, line_number, std::string(value));
		return config_line_result_t::invalid;
	}

	field = *parsed;
	return config_line_result_t::applied;
}

config_line_result_t ApplyKillBeepLine(client_config_t &config, std::string_view args, const char *name, int line_number)
{
	std::string_view value;
	if (!ReadSingleConfigArg(args, value)) {
		gi.Com_PrintFmt("{}:{}: expected one kill beep value.\n", name, line_number);
		return config_line_result_t::invalid;
	}

	const auto parsed = ParseKillBeepToken(value);
	if (!parsed) {
		gi.Com_PrintFmt("{}:{}: invalid kill beep value \"{}\"; use 0-4 or a named beep.\n", name, line_number, std::string(value));
		return config_line_result_t::invalid;
	}

	config.killbeep_num = *parsed;
	return config_line_result_t::applied;
}

config_line_result_t ApplyFollowViewLine(client_config_t &config, std::string_view args, const char *name, int line_number)
{
	std::string_view value;
	if (!ReadSingleConfigArg(args, value)) {
		gi.Com_PrintFmt("{}:{}: expected one follow view value.\n", name, line_number);
		return config_line_result_t::invalid;
	}

	const auto parsed = ParseFollowViewToken(value);
	if (!parsed) {
		gi.Com_PrintFmt("{}:{}: invalid follow view value \"{}\"; use first or third.\n", name, line_number, std::string(value));
		return config_line_result_t::invalid;
	}

	config.follow_first_person = *parsed;
	return config_line_result_t::applied;
}

config_line_result_t ApplySkinLine(char (&store)[MAX_QPATH], std::string_view args, const char *name, int line_number)
{
	std::string_view value;
	if (!ReadSingleConfigArg(args, value)) {
		gi.Com_PrintFmt("{}:{}: expected one skin value.\n", name, line_number);
		return config_line_result_t::invalid;
	}

	if (IsDisableToken(value)) {
		store[0] = 0;
		return config_line_result_t::applied;
	}

	if (!IsSafeSkinPath(value)) {
		gi.Com_PrintFmt("{}:{}: invalid skin path \"{}\".\n", name, line_number, std::string(value));
		return config_line_result_t::invalid;
	}

	if (!IsStorableSkinPath(value, MAX_QPATH, MAX_NETNAME - 1, CS_SIZE(CS_PLAYERSKINS))) {
		gi.Com_PrintFmt("{}:{}: skin path \"{}\" is too long.\n", name, line_number, std::string(value));
		return config_line_result_t::invalid;
	}

	muffmode::CopyString(store, value);
	return config_line_result_t::applied;
}

config_line_result_t ApplyConfigLine(client_config_t &config, std::string_view raw_line, const char *name, int line_number, uint32_t &loaded_fields)
{
	const std::string_view line = StripConfigComment(raw_line);
	if (line.empty())
		return config_line_result_t::ignored;

	const size_t split = line.find_first_of(" \t\r\n\v\f");
	const std::string_view command = split == std::string_view::npos ? line : line.substr(0, split);
	const std::string_view args = split == std::string_view::npos ? std::string_view() : line.substr(split);
	const auto apply_known = [&loaded_fields](uint32_t field, config_line_result_t result) {
		if (result == config_line_result_t::applied)
			loaded_fields |= field;
		return result;
	};

	if (EqualsI(command, "id") || EqualsI(command, "crosshairid"))
		return apply_known(k_config_field_show_id, ApplyBoolLine(config.show_id, args, name, line_number));
	if (EqualsI(command, "timer"))
		return apply_known(k_config_field_show_timer, ApplyBoolLine(config.show_timer, args, name, line_number));
	if (EqualsI(command, "infohud") || EqualsI(command, "matchinfo"))
		return apply_known(k_config_field_show_match_info, ApplyBoolLine(config.show_match_info, args, name, line_number));
	if (EqualsI(command, "fm") || EqualsI(command, "fragmessages"))
		return apply_known(k_config_field_show_fragmessages, ApplyBoolLine(config.show_fragmessages, args, name, line_number));
	if (EqualsI(command, "announcer"))
		return apply_known(k_config_field_announcer_enabled, ApplyBoolLine(config.announcer_enabled, args, name, line_number));
	if (EqualsI(command, "followkiller"))
		return apply_known(k_config_field_follow_killer, ApplyBoolLine(config.follow_killer, args, name, line_number));
	if (EqualsI(command, "followleader"))
		return apply_known(k_config_field_follow_leader, ApplyBoolLine(config.follow_leader, args, name, line_number));
	if (EqualsI(command, "followpowerup"))
		return apply_known(k_config_field_follow_powerup, ApplyBoolLine(config.follow_powerup, args, name, line_number));
	if (EqualsI(command, "followview") || EqualsI(command, "followcamera") || EqualsI(command, "followfirstperson"))
		return apply_known(k_config_field_follow_first_person, ApplyFollowViewLine(config, args, name, line_number));
	if (EqualsI(command, "kb") || EqualsI(command, "killbeep"))
		return apply_known(k_config_field_killbeep, ApplyKillBeepLine(config, args, name, line_number));
	if (EqualsI(command, "eskin") || EqualsI(command, "enemyskin"))
		return apply_known(k_config_field_enemy_skin, ApplySkinLine(config.enemy_skin, args, name, line_number));
	if (EqualsI(command, "tskin") || EqualsI(command, "teamskin"))
		return apply_known(k_config_field_team_skin, ApplySkinLine(config.team_skin, args, name, line_number));

	gi.Com_PrintFmt("{}:{}: ignoring unknown player config command \"{}\".\n", name, line_number, std::string(command));
	return config_line_result_t::invalid;
}

config_load_stats_t ApplyConfigText(client_config_t &config, std::string_view text, const char *name)
{
	config_load_stats_t stats;
	int line_number = 1;
	size_t line_start = 0;

	while (line_start <= text.size()) {
		size_t line_end = text.find('\n', line_start);
		std::string_view line = line_end == std::string_view::npos
			? text.substr(line_start)
			: text.substr(line_start, line_end - line_start);
		if (!line.empty() && line.back() == '\r')
			line.remove_suffix(1);

		const config_line_result_t result = ApplyConfigLine(config, line, name, line_number, stats.loaded_fields);
		if (result == config_line_result_t::applied)
			stats.applied++;
		else if (result == config_line_result_t::invalid)
			stats.invalid++;

		if (line_end == std::string_view::npos)
			break;

		line_start = line_end + 1;
		line_number++;
	}

	if (stats.invalid > 0) {
		gi.Com_PrintFmt("{}: Loaded {} player config setting{} from \"{}\" with {} invalid line{}.\n",
			__FUNCTION__, stats.applied, stats.applied == 1 ? "" : "s", name, stats.invalid, stats.invalid == 1 ? "" : "s");
	}

	return stats;
}

void ClearQueuedFollowTarget(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	ent->client->follow_queued_target = nullptr;
	ent->client->follow_queued_time = 0_sec;
}

void WarnSaveResult(gentity_t *ent, save_result_t result)
{
	if (!ent || !ent->client)
		return;

	switch (result) {
	case save_result_t::session_only:
		gi.LocClient_Print(ent, PRINT_HIGH, "Preference changed for this session only.\n");
		break;
	case save_result_t::failed:
		gi.LocClient_Print(ent, PRINT_HIGH, "Preference changed for this session only; it could not be saved.\n");
		break;
	default:
		break;
	}
}

gentity_t *ClientEntityForClientIndex(int client_index)
{
	if (client_index < 0 || static_cast<size_t>(client_index) >= static_cast<size_t>(game.maxclients))
		return nullptr;

	const size_t entity_index = static_cast<size_t>(client_index) + 1;
	const size_t client_entity_count = std::min(static_cast<size_t>(globals.num_entities), static_cast<size_t>(game.maxclients) + 1);
	if (entity_index >= client_entity_count)
		return nullptr;

	return &g_entities[entity_index];
}

void ApplyLoadedConfigEffects(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	if (ent->client->sess.pc.follow_leader && (!ClientIsPlaying(ent->client) || ent->client->eliminated)) {
		gentity_t *leader = level.num_playing_clients > 0 ? ClientEntityForClientIndex(level.sorted_clients[0]) : nullptr;
		if (leader && leader->inuse && leader->client && ClientIsPlaying(leader->client)) {
			ent->client->follow_queued_target = leader;
			ent->client->follow_queued_time = level.time;
		}
	}

	MM_RefreshSkinOverridesForViewer(ent);
}

} // namespace muffmode::pconfig

namespace {

bool *MM_PConfigBoolField(gentity_t *ent, mm_pconfig_bool_setting_t setting)
{
	if (!ent || !ent->client)
		return nullptr;

	client_config_t &config = ent->client->sess.pc;
	switch (setting) {
	case mm_pconfig_bool_setting_t::show_id:
		return &config.show_id;
	case mm_pconfig_bool_setting_t::show_timer:
		return &config.show_timer;
	case mm_pconfig_bool_setting_t::show_match_info:
		return &config.show_match_info;
	case mm_pconfig_bool_setting_t::show_fragmessages:
		return &config.show_fragmessages;
	case mm_pconfig_bool_setting_t::announcer_enabled:
		return &config.announcer_enabled;
	case mm_pconfig_bool_setting_t::follow_first_person:
		return &config.follow_first_person;
	case mm_pconfig_bool_setting_t::follow_killer:
		return &config.follow_killer;
	case mm_pconfig_bool_setting_t::follow_leader:
		return &config.follow_leader;
	case mm_pconfig_bool_setting_t::follow_powerup:
		return &config.follow_powerup;
	}

	return nullptr;
}

const bool *MM_PConfigBoolField(const gentity_t *ent, mm_pconfig_bool_setting_t setting)
{
	return MM_PConfigBoolField(const_cast<gentity_t *>(ent), setting);
}

mm_client_profile_preference_mask_t MM_PConfigBoolPreferenceMask(
	mm_pconfig_bool_setting_t setting)
{
	switch (setting) {
	case mm_pconfig_bool_setting_t::show_id:
		return MM_CLIENT_PROFILE_PREFERENCE_SHOW_ID;
	case mm_pconfig_bool_setting_t::show_timer:
		return MM_CLIENT_PROFILE_PREFERENCE_SHOW_TIMER;
	case mm_pconfig_bool_setting_t::show_match_info:
		return MM_CLIENT_PROFILE_PREFERENCE_SHOW_MATCH_INFO;
	case mm_pconfig_bool_setting_t::show_fragmessages:
		return MM_CLIENT_PROFILE_PREFERENCE_SHOW_FRAG_MESSAGES;
	case mm_pconfig_bool_setting_t::announcer_enabled:
		return MM_CLIENT_PROFILE_PREFERENCE_ANNOUNCER;
	case mm_pconfig_bool_setting_t::follow_first_person:
		return MM_CLIENT_PROFILE_PREFERENCE_FOLLOW_VIEW;
	case mm_pconfig_bool_setting_t::follow_killer:
		return MM_CLIENT_PROFILE_PREFERENCE_FOLLOW_KILLER;
	case mm_pconfig_bool_setting_t::follow_leader:
		return MM_CLIENT_PROFILE_PREFERENCE_FOLLOW_LEADER;
	case mm_pconfig_bool_setting_t::follow_powerup:
		return MM_CLIENT_PROFILE_PREFERENCE_FOLLOW_POWERUP;
	}

	return 0;
}

gentity_t *MM_PConfigLeader()
{
	return level.num_playing_clients > 0 ? muffmode::pconfig::ClientEntityForClientIndex(level.sorted_clients[0]) : nullptr;
}

void MM_PConfigApplyBoolEffects(gentity_t *ent, mm_pconfig_bool_setting_t setting, bool value)
{
	if (!ent || !ent->client)
		return;

	if (setting == mm_pconfig_bool_setting_t::follow_first_person) {
		if (ent->client->follow_target)
			UpdateFollowCamera(ent);
		return;
	}

	if (setting == mm_pconfig_bool_setting_t::follow_killer && !value) {
		muffmode::pconfig::ClearQueuedFollowTarget(ent);
		return;
	}

	if (setting != mm_pconfig_bool_setting_t::follow_leader)
		return;

	if (!value) {
		muffmode::pconfig::ClearQueuedFollowTarget(ent);
		return;
	}

	gentity_t *leader = MM_PConfigLeader();
	if (!leader || !leader->inuse || !leader->client || !ClientIsPlaying(leader->client))
		return;

	if (!ClientIsPlaying(ent->client) && ent->client->follow_target != leader) {
		SetFollowTarget(ent, leader);
		UpdateFollowCamera(ent);
	} else if (ent->client->eliminated && ent->client->follow_target != leader) {
		ent->client->follow_queued_target = leader;
		ent->client->follow_queued_time = level.time;
	}
}

} // namespace

client_config_t MM_DefaultClientConfig()
{
	client_config_t config{};
	config.show_id = true;
	config.show_timer = true;
	config.show_match_info = true;
	config.show_fragmessages = true;
	config.killbeep_num = 1;
	config.follow_killer = false;
	config.follow_leader = false;
	config.follow_powerup = false;
	config.follow_first_person = true;
	config.announcer_enabled = MM_ANNOUNCER_DEFAULT_ENABLED;
	config.enemy_skin[0] = 0;
	config.team_skin[0] = 0;
	return config;
}

/*
=============
MM_ClientInitPConfig

Load or create the player's server-side configuration file on connect or when
a retained client begins a new level/gametype.
=============
*/
void MM_ClientInitPConfig(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	if ((ent->svflags & SVF_BOT) || ent->client->sess.is_a_bot) {
		ent->client->sess.pc = MM_DefaultClientConfig();
		ent->client->sess.profile_persistence_ready = false;
		ent->client->sess.skill_rating = MM_ClientProfileDefaultRating();
		ent->client->sess.skill_rating_change = 0;
		MM_ClientProfileClearWeaponPreferences(ent->client);
		return;
	}

	// Import the old line-based .cfg as the seed only when a canonical JSON
	// profile does not exist. The profile loader owns persistence after that.
	client_config_t legacy_seed = MM_DefaultClientConfig();
	const client_config_t *missing_profile_seed = nullptr;
	if (!MM_ClientProfileCanonicalExists(ent->client->pers.social_id)) {
		missing_profile_seed = &legacy_seed;
		const auto path = muffmode::pconfig::ConfigPathForClient(ent, true);
		if (path) {
			const std::string name = path->string();
			auto existing_file = muffmode::OpenFile(name.c_str(), "rb");
			if (existing_file) {
				const auto config_text = muffmode::pconfig::ReadConfigText(existing_file.get());
				if (!config_text) {
					gi.Com_PrintFmt("{}: Player config load error for \"{}\"; using defaults.\n", __FUNCTION__, name);
				} else {
					muffmode::pconfig::ApplyConfigText(
						legacy_seed, *config_text, name.c_str());
				}
			}
		}
	}

	const int gametype = clamp(static_cast<int>(MM_CurrentGametype()),
		0, static_cast<int>(GT_NUM_GAMETYPES) - 1);
	ent->client->sess.profile_persistence_ready = false;
	const mm_client_profile_load_status_t profile_status = MM_ClientProfileLoad(
		ent->client,
		ent->client->pers.social_id,
		ent->client->pers.netname,
		gt_short_name_upper[gametype],
		missing_profile_seed);
	ent->client->sess.profile_persistence_ready =
		profile_status == mm_client_profile_load_status_t::created ||
		profile_status == mm_client_profile_load_status_t::loaded ||
		profile_status == mm_client_profile_load_status_t::repaired ||
		profile_status == mm_client_profile_load_status_t::recovered;
	muffmode::pconfig::ApplyLoadedConfigEffects(ent);
}

bool MM_ClientSavePConfig(
	gentity_t *ent,
	mm_client_profile_preference_mask_t dirty_mask)
{
	return ent && ent->client &&
		MM_ClientProfileSavePreferences(
			ent->client, ent->client->pers.social_id, dirty_mask);
}

void MM_ClientSavePConfigOrWarn(
	gentity_t *ent,
	mm_client_profile_preference_mask_t dirty_mask)
{
	if (!ent || !ent->client || (ent->svflags & SVF_BOT) ||
		ent->client->sess.is_a_bot)
		return;
	if (!MM_ClientProfileCanPersistIdentity(ent->client->pers.social_id)) {
		muffmode::pconfig::WarnSaveResult(ent,
			muffmode::pconfig::save_result_t::session_only);
		return;
	}
	muffmode::pconfig::WarnSaveResult(ent,
		MM_ClientSavePConfig(ent, dirty_mask)
			? muffmode::pconfig::save_result_t::saved
			: muffmode::pconfig::save_result_t::failed);
}

bool MM_PConfigGetBool(const gentity_t *ent, mm_pconfig_bool_setting_t setting)
{
	const bool *field = MM_PConfigBoolField(ent, setting);
	return field ? *field : false;
}

bool MM_PConfigSetBool(gentity_t *ent, mm_pconfig_bool_setting_t setting, bool value)
{
	bool *field = MM_PConfigBoolField(ent, setting);
	if (!field)
		return false;

	const bool changed = *field != value;
	*field = value;
	if (changed)
		MM_ClientSavePConfigOrWarn(ent, MM_PConfigBoolPreferenceMask(setting));
	MM_PConfigApplyBoolEffects(ent, setting, value);
	return true;
}

bool MM_PConfigToggleBool(gentity_t *ent, mm_pconfig_bool_setting_t setting)
{
	bool *field = MM_PConfigBoolField(ent, setting);
	return field && MM_PConfigSetBool(ent, setting, !*field);
}

const char *MM_PConfigBoolText(bool value)
{
	return muffmode::pconfig::BoolText(value);
}

const char *MM_PConfigFollowViewName(bool first_person)
{
	return first_person ? "first-person" : "third-person";
}

const char *MM_PConfigKillBeepName(int value)
{
	if (value < 0 || value >= static_cast<int>(muffmode::pconfig::k_kill_beep_names.size()))
		return "unknown";

	return muffmode::pconfig::k_kill_beep_names[value];
}

int MM_PConfigKillBeepCount()
{
	return static_cast<int>(muffmode::pconfig::k_kill_beep_names.size());
}

bool MM_PConfigSetKillBeep(gentity_t *ent, int value)
{
	if (!ent || !ent->client || value < 0 || value >= MM_PConfigKillBeepCount())
		return false;

	if (ent->client->sess.pc.killbeep_num == value)
		return true;
	ent->client->sess.pc.killbeep_num = value;
	MM_ClientSavePConfigOrWarn(ent, MM_CLIENT_PROFILE_PREFERENCE_KILL_BEEP);
	return true;
}

bool MM_PConfigCycleKillBeep(gentity_t *ent)
{
	if (!ent || !ent->client)
		return false;

	return MM_PConfigSetKillBeep(ent, (ent->client->sess.pc.killbeep_num + 1) % MM_PConfigKillBeepCount());
}

/*
=================
MM_CmdCrosshairID
=================
*/
void MM_CmdCrosshairID(gentity_t *ent)
{
	if (!muffmode::pconfig::BeginPreferenceCommand(ent))
		return;

	muffmode::pconfig::SetBoolPreference(ent, mm_pconfig_bool_setting_t::show_id, "Player identification display");
}

/*
=================
MM_CmdTimer
=================
*/
void MM_CmdTimer(gentity_t *ent)
{
	if (!muffmode::pconfig::BeginPreferenceCommand(ent))
		return;

	muffmode::pconfig::SetBoolPreference(ent, mm_pconfig_bool_setting_t::show_timer, "Match timer display");
}

/*
=================
MM_CmdInfoHud
=================
*/
void MM_CmdInfoHud(gentity_t *ent)
{
	if (!muffmode::pconfig::BeginPreferenceCommand(ent))
		return;

	muffmode::pconfig::SetBoolPreference(ent, mm_pconfig_bool_setting_t::show_match_info, "Match info HUD");
}

/*
=================
MM_CmdFragMessages
=================
*/
void MM_CmdFragMessages(gentity_t *ent)
{
	if (!muffmode::pconfig::BeginPreferenceCommand(ent))
		return;

	muffmode::pconfig::SetBoolPreference(ent, mm_pconfig_bool_setting_t::show_fragmessages, "Frag messages");
}

/*
=================
MM_CmdAnnouncer
=================
*/
void MM_CmdAnnouncer(gentity_t *ent)
{
	if (!muffmode::pconfig::BeginPreferenceCommand(ent))
		return;

	muffmode::pconfig::SetBoolPreference(ent, mm_pconfig_bool_setting_t::announcer_enabled, "Match announcer");
}

/*
=================
MM_CmdKillBeep
=================
*/
void MM_CmdKillBeep(gentity_t *ent)
{
	if (!muffmode::pconfig::BeginPreferenceCommand(ent))
		return;

	if (!muffmode::pconfig::RequireCommandArgc(ent, 1, 2, G_Fmt("{} [0-4|off|clang|beep-boop|insane|tang-tang]", gi.argv(0)).data()))
		return;

	int num = 0;
	if (gi.argc() > 1) {
		const auto parsed = muffmode::pconfig::ParseKillBeepToken(gi.argv(1));
		if (!parsed) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Invalid kill beep value. Use 0 through 4, off, clang, beep-boop, insane, or tang-tang.\n");
			return;
		}
		num = *parsed;
	} else {
		num = (ent->client->sess.pc.killbeep_num + 1) % MM_PConfigKillBeepCount();
	}

	MM_PConfigSetKillBeep(ent, num);
	gi.LocClient_Print(ent, PRINT_HIGH, "Kill beep changed to: {}\n", MM_PConfigKillBeepName(num));
}

/*
=================
MM_CmdFollowView
=================
*/
void MM_CmdFollowView(gentity_t *ent)
{
	if (!muffmode::pconfig::BeginPreferenceCommand(ent))
		return;

	if (!muffmode::pconfig::RequireCommandArgc(ent, 1, 2, G_Fmt("{} [first|third]", gi.argv(0)).data()))
		return;

	bool first_person = !ent->client->sess.pc.follow_first_person;
	if (gi.argc() > 1) {
		const auto parsed = muffmode::pconfig::ParseFollowViewToken(gi.argv(1));
		if (!parsed) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Invalid follow view. Use first or third.\n");
			return;
		}
		first_person = *parsed;
	}

	MM_PConfigSetBool(ent, mm_pconfig_bool_setting_t::follow_first_person, first_person);
	gi.LocClient_Print(ent, PRINT_HIGH, "Follow view: {}\n", MM_PConfigFollowViewName(first_person));
}

/*
=================
MM_CmdFollowKiller
=================
*/
void MM_CmdFollowKiller(gentity_t *ent)
{
	if (!muffmode::pconfig::BeginPreferenceCommand(ent))
		return;

	muffmode::pconfig::SetBoolPreference(ent, mm_pconfig_bool_setting_t::follow_killer, "Auto-follow killer");
}

/*
=================
MM_CmdFollowLeader
=================
*/
void MM_CmdFollowLeader(gentity_t *ent)
{
	if (!muffmode::pconfig::BeginPreferenceCommand(ent))
		return;

	const auto value = muffmode::pconfig::ReadOptionalBoolCommandArg(ent, ent->client->sess.pc.follow_leader);
	if (!value)
		return;

	gentity_t *leader = nullptr;
	if (*value) {
		leader = level.num_playing_clients > 0 ? muffmode::pconfig::ClientEntityForClientIndex(level.sorted_clients[0]) : nullptr;
	}

	MM_PConfigSetBool(ent, mm_pconfig_bool_setting_t::follow_leader, *value);

	if (!*value) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Auto-follow leader: OFF\n");
		return;
	}

	if (ent->client->sess.pc.follow_leader && (!leader || !leader->inuse || !leader->client || !ClientIsPlaying(leader->client))) {
		gi.Client_Print(ent, PRINT_HIGH, "No leader available to follow yet.\n");
		gi.LocClient_Print(ent, PRINT_HIGH, "Auto-follow leader: ON\n");
		return;
	}

	gi.LocClient_Print(ent, PRINT_HIGH, "Auto-follow leader: ON\n");
}

/*
=================
MM_CmdFollowPowerup
=================
*/
void MM_CmdFollowPowerup(gentity_t *ent)
{
	if (!muffmode::pconfig::BeginPreferenceCommand(ent))
		return;

	muffmode::pconfig::SetBoolPreference(ent, mm_pconfig_bool_setting_t::follow_powerup, "Auto-follow powerup pick-ups");
}
