// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_awards.h"
#include "muffmode/mm_duel.h"
#include "muffmode/mm_freezetag.h"
#include "muffmode/mm_match_stats.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_player_stats.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <json/json.h>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

cvar_t *g_statex_enabled = nullptr;
cvar_t *g_statex_humans_present = nullptr;
cvar_t *g_statex_export_html = nullptr;

namespace {

using json = Json::Value;

constexpr int k_match_schema_version = 1;
constexpr int k_match_artifact_version = 1;
constexpr const char *k_match_schema_name = "worr.match_stats";
constexpr const char *k_match_artifact_type = "match_stats";
constexpr int k_catalog_schema_version = 1;
constexpr int k_catalog_artifact_version = 1;
constexpr const char *k_catalog_schema_name = "worr.match_catalog";
constexpr const char *k_catalog_artifact_type = "match_catalog";
constexpr const char *k_catalog_file_name = "catalog.json";
constexpr size_t k_export_queue_capacity = 8;
constexpr size_t k_max_export_event_length = 1024;
constexpr size_t k_max_catalog_bytes = 16 * 1024 * 1024;
constexpr size_t k_max_catalog_entries = 4096;
constexpr size_t k_max_catalog_parse_entries = k_max_catalog_entries * 2;
constexpr size_t k_max_catalog_identity_length = 512;
constexpr int k_catalog_worker_lock_attempts = 500;
constexpr int k_catalog_game_thread_lock_attempts = 25;
constexpr auto k_catalog_lock_retry_delay = std::chrono::milliseconds(10);

constexpr std::array<const char *, MM_MATCH_WEAPON_COUNT> k_weapon_abbreviations = {
	"NONE", "GP", "BL", "CF", "SG", "SSG", "MG", "ETF", "CG", "HG",
	"TP", "TM", "GL", "PL", "RL", "HB", "IR", "PG", "PB", "TB",
	"RG", "PX", "BFG", "DTR"
};

constexpr std::array<const char *, MM_MATCH_MEDAL_COUNT> k_medal_names = {
	"None", "Excellent", "Humiliation", "Impressive", "Rampage",
	"First Frag", "Base Defense", "Carrier Assist", "Flag Capture",
	"Holy Shit!"
};

constexpr std::array<const char *, MM_MATCH_HIGH_VALUE_ITEM_COUNT>
	k_high_value_item_names = {
		"", "MegaHealth", "BodyArmor", "CombatArmor", "PowerShield",
		"PowerScreen", "Adrenaline", "QuadDamage", "DoubleDamage",
		"Invisibility", "Haste", "Regeneration", "BattleSuit",
		"EmpathyShield", "AmmoPack", "Bandolier"
	};


template<typename T, typename U>
void SaturatingAdd(T &target, U amount)
{
	static_assert(std::numeric_limits<T>::is_integer);
	if (amount <= 0)
		return;

	using target_limits = std::numeric_limits<T>;
	const uint64_t add = static_cast<uint64_t>(amount);
	const uint64_t current = static_cast<uint64_t>(target);
	const uint64_t maximum = static_cast<uint64_t>(target_limits::max());
	target = static_cast<T>(add > maximum - current ? maximum : current + add);
}

int64_t NowUnixMsec()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
}

int64_t MatchElapsedMsec()
{
	const int64_t elapsed =
		(level.time - level.match_start_time).milliseconds();
	return std::max<int64_t>(0, elapsed);
}

bool CvarEnabled(const cvar_t *value, bool default_value)
{
	return value ? value->integer != 0 : default_value;
}

bool ValidClient(const gclient_t *client)
{
	return client != nullptr && MM_MatchStatsAcceptsResultEvents(
		level.match.collecting, level.match.finalized,
		level.match.match_end_real_time_ms);
}

std::optional<size_t> CtfTeamIndex(team_t team)
{
	if (team == TEAM_RED)
		return 0;
	if (team == TEAM_BLUE)
		return 1;
	return std::nullopt;
}

uint8_t ModIndex(const mod_t &mod)
{
	return static_cast<uint8_t>(mod.id);
}

std::string PlayerName(const gclient_t *client)
{
	if (!client)
		return {};
	// pers.netname holds the client-resolved "##P<n>" cross-play token (see
	// EncodedPlayerName in userinfo.cpp) for every non-bot player -- it only
	// renders correctly through gi.LocClient_Print/LocBroadcast_Print's own
	// player-name substitution, not when copied into award/report/layout text
	// built server-side. resp.netname is the actual submitted name and is
	// always populated (for bots and humans alike), so it's the right value
	// for text that doesn't go through that substitution.
	if (client->resp.netname[0])
		return client->resp.netname;
	return client->pers.netname;
}

std::string PlayerSocialId(const gclient_t *client)
{
	return client && client->pers.social_id[0]
		? std::string(client->pers.social_id)
		: std::string();
}

std::string ServerHostName()
{
	if (CvarEnabled(g_dedicated, false) || !g_entities ||
		game.maxclients <= 0 || !g_entities[1].inuse ||
		!g_entities[1].client || !g_entities[1].client->pers.connected)
		return {};
	char value[MAX_INFO_VALUE]{};
	gi.Info_ValueForKey(g_entities[1].client->pers.userinfo,
		"name", value, sizeof(value));
	return value;
}

std::string HtmlEscape(std::string_view input)
{
	std::string output;
	output.reserve(input.size());
	for (const char value : input) {
		switch (value) {
		case '&': output.append("&amp;"); break;
		case '<': output.append("&lt;"); break;
		case '>': output.append("&gt;"); break;
		case '"': output.append("&quot;"); break;
		case '\'': output.append("&#39;"); break;
		default: output.push_back(value); break;
		}
	}
	return output;
}

std::string SanitizeFileStem(std::string_view input)
{
	std::string output;
	output.reserve(std::min<size_t>(input.size(), 96));
	for (const unsigned char value : input) {
		if (output.size() == 96)
			break;
		if (std::isalnum(value) || value == '-' || value == '_')
			output.push_back(static_cast<char>(value));
		else if (value == '.' && !output.empty())
			output.push_back('_');
	}
	return output.empty() ? "match" : output;
}

std::string FormatUtc(int64_t timestamp_msec)
{
	if (timestamp_msec <= 0)
		return "n/a";

	const std::time_t value = static_cast<std::time_t>(timestamp_msec / 1000);
	std::tm utc{};
#if defined(_WIN32)
	if (gmtime_s(&utc, &value) != 0)
		return "invalid";
#else
	if (!gmtime_r(&value, &utc))
		return "invalid";
#endif
	char buffer[32]{};
	if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S UTC", &utc))
		return buffer;
	return "invalid";
}

std::string FormatDurationMsec(uint64_t duration_msec)
{
	const uint64_t total_seconds = duration_msec / 1000;
	const uint64_t hours = total_seconds / 3600;
	const uint64_t minutes = (total_seconds / 60) % 60;
	const uint64_t seconds = total_seconds % 60;
	std::ostringstream stream;
	if (hours)
		stream << hours << 'h' << ' ';
	if (hours || minutes)
		stream << minutes << 'm' << ' ';
	stream << seconds << 's';
	return stream.str();
}

void AddArtifactMetadata(json &artifact, const char *schema_name,
	int schema_version, const char *artifact_type, int artifact_version)
{
	artifact["schemaName"] = schema_name;
	artifact["schemaVersion"] = schema_version;
	artifact["artifactType"] = artifact_type;
	artifact["artifactVersion"] = artifact_version;
}

struct FrozenPlayer {
	int client_num = -1;
	int32_t spawn_count = 0;
	std::string social_id;
	std::string player_name;
	team_t team = TEAM_NONE;
	int score = 0;
	bool bot = false;
	mm_match_player_outcome_t outcome = mm_match_player_outcome_t::loss;
	bool outcome_settled = false;
	mm_match_player_export_metadata_t metadata{};
	mm_match_player_stats_t stats{};
};

struct FrozenMatch {
	std::string match_id;
	std::string server_name;
	std::string server_host_name;
	std::string game_type;
	std::string rule_set;
	ruleset_t ruleset_id = RS_MM;
	std::string map_name;
	bool team_mode = false;
	bool ctf_mode = false;
	bool duel_mode = false;
	int64_t start_msec = 0;
	int64_t end_msec = 0;
	int64_t duration_msec = 0;
	int time_limit_seconds = 0;
	int score_limit = 0;
	int red_score = 0;
	int blue_score = 0;
	mm_match_overall_stats_t overall{};
	std::vector<FrozenPlayer> players;
	// [MuffMode] Post-match awards, decided once the participant set is final.
	// awards_ranked records whether the match cleared the ranked bar; an unranked
	// match still exports its (empty) award list so a reader can tell the
	// difference between "nobody qualified" and "awards were not offered".
	std::vector<mm_award_result_t> awards;
	bool awards_ranked = false;
	bool awards_decided = false;
};

// Players may leave the playing set before Match_End. Keep a game-thread-only
// snapshot so their contribution remains in the artifact even when their
// client slot is disconnected, reused, or moved to spectator.
std::vector<FrozenPlayer> g_departed_players;
std::optional<FrozenMatch> g_frozen_result;

bool SameParticipant(const FrozenPlayer &snapshot, const gentity_t *entity)
{
	if (!entity || !entity->client)
		return false;
	const std::string social_id = PlayerSocialId(entity->client);
	if (!snapshot.social_id.empty() || !social_id.empty())
		return !snapshot.social_id.empty() && snapshot.social_id == social_id;
	return snapshot.client_num == static_cast<int>(entity - g_entities - 1) &&
		snapshot.spawn_count == entity->spawn_count;
}

gentity_t *ResolveParticipantEntity(const FrozenPlayer &snapshot)
{
	if (!g_entities || game.maxclients <= 0 || snapshot.client_num < 0 ||
		static_cast<size_t>(snapshot.client_num) >= game.maxclients)
		return nullptr;

	gentity_t *entity = &g_entities[snapshot.client_num + 1];
	return entity->client && MM_MatchStatsCanDeliverFrozenResult(
		entity->inuse, entity->client->pers.connected,
		snapshot.spawn_count, entity->spawn_count) ? entity : nullptr;
}

mm_match_player_outcome_t MatchOutcome(mm_player_stats_outcome_t outcome)
{
	switch (outcome) {
	case mm_player_stats_outcome_t::win:
		return mm_match_player_outcome_t::win;
	case mm_player_stats_outcome_t::loss:
		return mm_match_player_outcome_t::loss;
	case mm_player_stats_outcome_t::draw:
		return mm_match_player_outcome_t::draw;
	case mm_player_stats_outcome_t::abandon:
		return mm_match_player_outcome_t::abandon;
	case mm_player_stats_outcome_t::no_contest:
		return mm_match_player_outcome_t::no_contest;
	}
	return mm_match_player_outcome_t::no_contest;
}

FrozenPlayer FreezePlayer(gentity_t *entity,
	const gclient_t *authoritative_profile_state = nullptr)
{
	FrozenPlayer player;
	player.client_num = static_cast<int>(entity - g_entities - 1);
	player.spawn_count = entity->spawn_count;
	player.social_id = PlayerSocialId(entity->client);
	player.player_name = PlayerName(entity->client);
	player.team = entity->client->sess.team;
	player.score = entity->client->resp.score;
	player.bot = entity->client->sess.is_a_bot ||
		(entity->svflags & SVF_BOT) != 0;
	player.stats = entity->client->pers.match;
	const gclient_t *profile_state = authoritative_profile_state
		? authoritative_profile_state : entity->client;
	const float skill_rating = profile_state->sess.skill_rating;
	const bool profile_ready = MM_MatchStatsEffectiveProfileReadiness(
		entity->client->sess.profile_persistence_ready,
		authoritative_profile_state != nullptr,
		profile_state->sess.profile_persistence_ready);
	player.metadata.has_skill_rating =
		profile_ready &&
		std::isfinite(skill_rating) &&
		skill_rating >= 0.0f;
	player.metadata.skill_rating = player.metadata.has_skill_rating
		? static_cast<int>(skill_rating) : 0;
	player.metadata.skill_rating_change =
		profile_state->sess.skill_rating_change;
	if (const auto outcome = MM_PlayerStats_CurrentOutcome(entity)) {
		player.outcome = MatchOutcome(*outcome);
		player.outcome_settled = true;
	}
	if (player.bot)
		player.metadata = {};
	return player;
}

void ArchivePlayer(gentity_t *entity)
{
	FrozenPlayer snapshot = FreezePlayer(entity);
	const auto existing = std::find_if(g_departed_players.begin(),
		g_departed_players.end(), [&](const FrozenPlayer &candidate) {
			return SameParticipant(candidate, entity);
		});
	if (existing == g_departed_players.end())
	{
		if (MM_MatchStatsLogHasCapacity(g_departed_players.size(),
			MM_MATCH_DEPARTED_PLAYER_LIMIT)) {
			g_departed_players.push_back(std::move(snapshot));
		} else if (!level.match.players_truncated) {
			level.match.players_truncated = true;
			gi.Com_PrintFmt(
				"MM_MatchStats: departed-player history truncated after {} participants\n",
				MM_MATCH_DEPARTED_PLAYER_LIMIT);
		}
	}
	else
		*existing = std::move(snapshot);
}

double Ratio(uint64_t numerator, uint64_t denominator)
{
	return denominator > 0
		? static_cast<double>(numerator) / static_cast<double>(denominator)
		: static_cast<double>(numerator);
}

double Percent(uint64_t numerator, uint64_t denominator)
{
	return denominator > 0
		? static_cast<double>(numerator) * 100.0 /
			static_cast<double>(denominator)
		: 0.0;
}

uint64_t PlayerPlayTime(const FrozenPlayer &player, int64_t match_end_msec)
{
	uint64_t total = player.stats.play_time_total_msec;
	const int64_t start = player.stats.play_start_real_time_ms;
	const int64_t end = player.stats.play_end_real_time_ms > 0
		? player.stats.play_end_real_time_ms
		: match_end_msec;
	if (start > 0 && end > start)
		SaturatingAdd(total, static_cast<uint64_t>(end - start));
	return total;
}

std::string TeamLabel(team_t team)
{
	switch (team) {
	case TEAM_RED: return "Red";
	case TEAM_BLUE: return "Blue";
	case TEAM_FREE: return "Free";
	case TEAM_SPECTATOR: return "Spectator";
	default: return "None";
	}
}

mm_match_player_outcome_t PlayerOutcome(const FrozenMatch &match,
	const FrozenPlayer &player)
{
	if (match.team_mode) {
		if (match.red_score == match.blue_score)
			return mm_match_player_outcome_t::draw;
		if (player.team == TEAM_RED)
			return match.red_score > match.blue_score
				? mm_match_player_outcome_t::win
				: mm_match_player_outcome_t::loss;
		if (player.team == TEAM_BLUE)
			return match.blue_score > match.red_score
				? mm_match_player_outcome_t::win
				: mm_match_player_outcome_t::loss;
		return mm_match_player_outcome_t::loss;
	}
	if (match.duel_mode) {
		switch (MM_Duel_FinalOutcome(
			player.client_num, player.social_id)) {
		case mm_duel_final_outcome_t::win:
			return mm_match_player_outcome_t::win;
		case mm_duel_final_outcome_t::loss:
			return mm_match_player_outcome_t::loss;
		case mm_duel_final_outcome_t::draw:
			return mm_match_player_outcome_t::draw;
		default:
			break;
		}
	}

	int highest = std::numeric_limits<int>::min();
	int leaders = 0;
	for (const FrozenPlayer &candidate : match.players) {
		if (candidate.score > highest) {
			highest = candidate.score;
			leaders = 1;
		} else if (candidate.score == highest) {
			leaders++;
		}
	}
	if (player.score != highest)
		return mm_match_player_outcome_t::loss;
	return leaders > 1
		? mm_match_player_outcome_t::draw
		: mm_match_player_outcome_t::win;
}

const char *OutcomeName(mm_match_player_outcome_t outcome)
{
	switch (outcome) {
	case mm_match_player_outcome_t::win: return "win";
	case mm_match_player_outcome_t::draw: return "draw";
	case mm_match_player_outcome_t::abandon: return "abandon";
	case mm_match_player_outcome_t::no_contest: return "no_contest";
	default: return "loss";
	}
}

const char *ModNameForRuleset(uint8_t mod, ruleset_t ruleset);

json PlayerToJson(const FrozenPlayer &player, int64_t match_end_msec,
	bool ctf_mode, ruleset_t ruleset)
{
	const mm_match_player_stats_t &stats = player.stats;
	json result(Json::objectValue);
	result["socialID"] = player.social_id;
	result["playerIdentifier"] = player.social_id.empty()
		? player.player_name : player.social_id;
	result["playerName"] = player.player_name;
	result["totalScore"] = player.score;
	result["outcome"] = OutcomeName(player.outcome);

	if (stats.total_kills) result["totalKills"] = stats.total_kills;
	if (stats.total_spawn_kills) result["totalSpawnKills"] = stats.total_spawn_kills;
	if (stats.total_team_kills) result["totalTeamKills"] = stats.total_team_kills;
	if (stats.total_deaths) result["totalDeaths"] = stats.total_deaths;
	if (stats.total_environment_deaths)
		result["totalEnvironmentDeaths"] = stats.total_environment_deaths;
	if (stats.total_spawn_deaths) result["totalSpawnDeaths"] = stats.total_spawn_deaths;
	if (stats.total_suicides) result["totalSuicides"] = stats.total_suicides;
	if (stats.quad_kills) result["totalQuadKills"] = stats.quad_kills;
	if (stats.total_kills || stats.total_deaths)
		result["totalKDR"] = Ratio(stats.total_kills, stats.total_deaths);
	// Position dwell, from which the camping award is derived. Exported as the
	// share rather than the raw histogram: the cell coordinates are an internal
	// bucketing detail, but "spent 71% of the match in one spot" is readable.
	if (stats.camp_samples) {
		uint32_t busiest = 0;
		for (const mm_match_camp_cell_t &cell : stats.camp_cells)
			busiest = std::max(busiest, cell.samples);
		result["dwellSamples"] = stats.camp_samples;
		result["dwellIdleSamples"] = stats.camp_idle_samples;
		result["dwellLargestAreaShare"] = Percent(busiest, stats.camp_samples);
	}
	if (stats.total_hits) result["totalHits"] = Json::UInt64(stats.total_hits);
	if (stats.total_shots) result["totalShots"] = Json::UInt64(stats.total_shots);
	if (stats.total_shots)
		result["totalAccuracy"] = Percent(stats.total_hits, stats.total_shots);
	if (stats.total_dmg_dealt)
		result["totalDmgDealt"] = Json::UInt64(stats.total_dmg_dealt);
	if (stats.total_dmg_received)
		result["totalDmgReceived"] = Json::UInt64(stats.total_dmg_received);

	const uint64_t play_time = PlayerPlayTime(player, match_end_msec);
	if (play_time) {
		result["playTime"] = Json::UInt64(play_time);
		result["killsPerMinute"] =
			stats.total_kills * 60000.0 / static_cast<double>(play_time);
	}
	if (!player.bot && player.metadata.has_skill_rating)
		result["skillRating"] = player.metadata.skill_rating;
	if (!player.bot && player.metadata.skill_rating_change)
		result["skillRatingChange"] = player.metadata.skill_rating_change;

	if (stats.completed_lives) {
		result["lifeAverageMsec"] = stats.life_average_msec;
		result["lifeLongestMsec"] = stats.life_longest_msec;
	}

	json shots(Json::objectValue);
	json hits(Json::objectValue);
	json accuracy(Json::objectValue);
	for (size_t index = 0; index < MM_MATCH_WEAPON_COUNT; index++) {
		const char *name = k_weapon_abbreviations[index];
		if (stats.total_shots_per_weapon[index])
			shots[name] = Json::UInt64(stats.total_shots_per_weapon[index]);
		if (stats.total_hits_per_weapon[index])
			hits[name] = Json::UInt64(stats.total_hits_per_weapon[index]);
		if (stats.total_shots_per_weapon[index])
			accuracy[name] = Percent(stats.total_hits_per_weapon[index],
				stats.total_shots_per_weapon[index]);
	}
	if (!shots.empty()) result["totalShotsPerWeapon"] = std::move(shots);
	if (!hits.empty()) result["totalHitsPerWeapon"] = std::move(hits);
	if (!accuracy.empty()) result["accuracyPerWeapon"] = std::move(accuracy);

	json mod_kills(Json::objectValue);
	json mod_deaths(Json::objectValue);
	json mod_kdr(Json::objectValue);
	json mod_damage_dealt(Json::objectValue);
	json mod_damage_received(Json::objectValue);
	for (size_t index = 0; index < MM_MATCH_MOD_COUNT; index++) {
		const bool has_data = stats.mod_total_kills[index] ||
			stats.mod_total_deaths[index] || stats.mod_total_dmg_dealt[index] ||
			stats.mod_total_dmg_received[index];
		if (!has_data)
			continue;
		const char *name = ModNameForRuleset(
			static_cast<uint8_t>(index), ruleset);
		if (stats.mod_total_kills[index])
			mod_kills[name] = stats.mod_total_kills[index];
		if (stats.mod_total_deaths[index])
			mod_deaths[name] = stats.mod_total_deaths[index];
		if (stats.mod_total_kills[index] || stats.mod_total_deaths[index])
			mod_kdr[name] = Ratio(stats.mod_total_kills[index],
				stats.mod_total_deaths[index]);
		if (stats.mod_total_dmg_dealt[index])
			mod_damage_dealt[name] = Json::UInt64(stats.mod_total_dmg_dealt[index]);
		if (stats.mod_total_dmg_received[index])
			mod_damage_received[name] = Json::UInt64(stats.mod_total_dmg_received[index]);
	}
	if (!mod_kills.empty()) result["totalKillsByMOD"] = std::move(mod_kills);
	if (!mod_deaths.empty()) result["totalDeathsByMOD"] = std::move(mod_deaths);
	if (!mod_kdr.empty()) result["totalKDRByMOD"] = std::move(mod_kdr);
	if (!mod_damage_dealt.empty()) result["totalDmgDByMOD"] = std::move(mod_damage_dealt);
	if (!mod_damage_received.empty()) result["totalDmgRByMOD"] = std::move(mod_damage_received);

	json awards(Json::objectValue);
	for (size_t index = 1; index < MM_MATCH_MEDAL_COUNT; index++)
		if (stats.medal_count[index])
			awards[k_medal_names[index]] = stats.medal_count[index];
	if (!awards.empty()) result["awards"] = std::move(awards);

	json pickups(Json::objectValue);
	json pickup_delays(Json::objectValue);
	json pickup_average_delays(Json::objectValue);
	for (size_t index = 1; index < MM_MATCH_HIGH_VALUE_ITEM_COUNT; index++) {
		const uint32_t count = stats.pickup_counts[index];
		const uint64_t total_delay = stats.pickup_delay_total_msec[index];
		if (count)
			pickups[k_high_value_item_names[index]] = count;
		if (total_delay) {
			pickup_delays[k_high_value_item_names[index]] = total_delay / 1000.0;
			if (count)
				pickup_average_delays[k_high_value_item_names[index]] =
					static_cast<double>(total_delay) / count;
		}
	}
	if (!pickups.empty()) result["pickupCounts"] = std::move(pickups);
	if (!pickup_delays.empty()) result["pickupDelays"] = std::move(pickup_delays);
	if (!pickup_average_delays.empty())
		result["pickupAverageDelayMsec"] = std::move(pickup_average_delays);

	if (ctf_mode) {
		if (stats.ctf_flag_pickups)
			result["ctfFlagPickups"] = stats.ctf_flag_pickups;
		if (stats.ctf_flag_drops)
			result["ctfFlagDrops"] = stats.ctf_flag_drops;
		if (stats.ctf_flag_returns)
			result["ctfFlagReturns"] = stats.ctf_flag_returns;
		if (stats.ctf_flag_assists)
			result["ctfFlagAssists"] = stats.ctf_flag_assists;
		if (stats.ctf_flag_defences)
			result["ctfFlagDefences"] = stats.ctf_flag_defences;
		if (stats.ctf_flag_captures)
			result["ctfFlagCaptures"] = stats.ctf_flag_captures;
		if (stats.ctf_flag_carrier_time_total_msec)
			result["ctfFlagCarrierTimeTotalMsec"] =
				Json::UInt64(stats.ctf_flag_carrier_time_total_msec);
		if (stats.ctf_flag_carrier_time_shortest_msec)
			result["ctfFlagCarrierTimeShortestMsec"] =
				stats.ctf_flag_carrier_time_shortest_msec;
		if (stats.ctf_flag_carrier_time_longest_msec)
			result["ctfFlagCarrierTimeLongestMsec"] =
				stats.ctf_flag_carrier_time_longest_msec;

		json ctf(Json::objectValue);
		if (stats.ctf_flag_pickups) ctf["flagPickups"] = stats.ctf_flag_pickups;
		if (stats.ctf_flag_drops) ctf["flagDrops"] = stats.ctf_flag_drops;
		if (stats.ctf_flag_returns) ctf["flagReturns"] = stats.ctf_flag_returns;
		if (stats.ctf_flag_assists) ctf["flagAssists"] = stats.ctf_flag_assists;
		if (stats.ctf_flag_defences) ctf["flagDefences"] = stats.ctf_flag_defences;
		if (stats.ctf_flag_captures) ctf["flagCaptures"] = stats.ctf_flag_captures;
		if (stats.ctf_flag_carrier_time_total_msec)
			ctf["flagCarrierTimeTotalMsec"] =
				Json::UInt64(stats.ctf_flag_carrier_time_total_msec);
		if (stats.ctf_flag_carrier_time_shortest_msec)
			ctf["flagCarrierTimeShortestMsec"] =
				stats.ctf_flag_carrier_time_shortest_msec;
		if (stats.ctf_flag_carrier_time_longest_msec)
			ctf["flagCarrierTimeLongestMsec"] =
				stats.ctf_flag_carrier_time_longest_msec;
		if (!ctf.empty())
			result["gametype"]["ctf"] = std::move(ctf);
	}

	return result;
}

json CtfTeamToJson(const mm_match_ctf_team_stats_t &stats)
{
	json result(Json::objectValue);
	result["flagsCaptured"] = stats.captures;
	result["flagAssists"] = stats.assists;
	result["flagDefends"] = stats.defences;
	result["flagPickups"] = stats.flag_pickups;
	result["flagDrops"] = stats.flag_drops;
	result["flagReturns"] = stats.flag_returns;
	result["flagHoldTimeTotalMsec"] = Json::UInt64(stats.flag_hold_time_total_msec);
	result["flagHoldTimeShortestMsec"] = stats.flag_hold_time_shortest_msec;
	result["flagHoldTimeLongestMsec"] = stats.flag_hold_time_longest_msec;
	return result;
}

json MatchToJson(const FrozenMatch &match)
{
	json result(Json::objectValue);
	AddArtifactMetadata(result, k_match_schema_name, k_match_schema_version,
		k_match_artifact_type, k_match_artifact_version);
	result["matchID"] = match.match_id;
	result["serverName"] = match.server_name;
	if (!match.server_host_name.empty())
		result["serverHostName"] = match.server_host_name;
	result["gameType"] = match.game_type;
	result["ruleSet"] = match.rule_set;
	result["mapName"] = match.map_name;
	result["matchRanked"] = false;
	result["totalKills"] = match.overall.total_kills;
	result["totalSpawnKills"] = match.overall.total_spawn_kills;
	result["totalTeamKills"] = match.overall.total_team_kills;
	result["totalDeaths"] = match.overall.total_deaths;
	result["totalSuicides"] = match.overall.total_suicides;
	result["avKillsPerMinute"] = match.duration_msec > 0
		? match.overall.total_kills * 60000.0 /
			static_cast<double>(match.duration_msec)
		: 0.0;
	result["matchStartMS"] = Json::Int64(match.start_msec);
	result["matchEndMS"] = Json::Int64(match.end_msec);
	result["matchTimeDuration"] = Json::Int64(match.duration_msec);
	result["timeLimitSeconds"] = match.time_limit_seconds;
	result["scoreLimit"] = match.score_limit;
	result["eventLogTruncated"] = match.overall.event_log_truncated;
	result["deathLogTruncated"] = match.overall.death_log_truncated;
	result["playersTruncated"] = match.overall.players_truncated;

	json total_mod_kills(Json::objectValue);
	json total_mod_deaths(Json::objectValue);
	json total_mod_kdr(Json::objectValue);
	for (size_t index = 0; index < MM_MATCH_MOD_COUNT; index++) {
		const uint32_t kills = match.overall.mod_kills[index];
		const uint32_t deaths = match.overall.mod_deaths[index];
		if (!kills && !deaths)
			continue;
		const char *name = ModNameForRuleset(
			static_cast<uint8_t>(index), match.ruleset_id);
		if (kills) total_mod_kills[name] = kills;
		if (deaths) total_mod_deaths[name] = deaths;
		total_mod_kdr[name] = Ratio(kills, deaths);
	}
	if (!total_mod_kills.empty()) result["totalKillsByMOD"] = std::move(total_mod_kills);
	if (!total_mod_deaths.empty()) result["totalDeathsByMOD"] = std::move(total_mod_deaths);
	if (!total_mod_kdr.empty()) result["totalKDRByMOD"] = std::move(total_mod_kdr);

	// [MuffMode] The post-match awards reel. Unlike the per-player medal tallies
	// next door, an award has exactly one holder, so this is a match-level list
	// rather than a per-player count. `value` is the number that won it.
	result["matchAwardsOffered"] = match.awards_ranked;
	result["totalQuadSpawns"] = match.overall.quad_spawns;
	json match_awards(Json::arrayValue);
	for (const mm_award_result_t &award : match.awards) {
		if (award.player_index >= match.players.size())
			continue;
		const FrozenPlayer &winner = match.players[award.player_index];
		json entry(Json::objectValue);
		entry["award"] = MM_AwardKey(award.award);
		entry["title"] = MM_AwardTitle(award.award);
		entry["playerName"] = winner.player_name;
		entry["playerIdentifier"] = winner.social_id.empty()
			? winner.player_name : winner.social_id;
		entry["value"] = award.value;
		match_awards.append(std::move(entry));
	}
	result["matchAwards"] = std::move(match_awards);

	// Awards are indexed against match.players, so a player's own titles are
	// resolved by their position in that vector rather than by name -- two
	// players can share a name, but not a slot.
	const auto award_titles_for = [&match](const FrozenPlayer &player) {
		json titles(Json::arrayValue);
		const size_t index =
			static_cast<size_t>(&player - match.players.data());
		for (const mm_award_result_t &award : match.awards)
			if (award.player_index == index)
				titles.append(MM_AwardTitle(award.award));
		return titles;
	};
	const auto player_json_with_awards = [&](const FrozenPlayer &player) {
		json entry = PlayerToJson(player, match.end_msec, match.ctf_mode,
			match.ruleset_id);
		json titles = award_titles_for(player);
		if (!titles.empty())
			entry["matchAwards"] = std::move(titles);
		return entry;
	};

	json all_players(Json::arrayValue);
	if (!match.team_mode)
		for (const FrozenPlayer &player : match.players)
			all_players.append(player_json_with_awards(player));

	result["players"] = std::move(all_players);
	if (match.team_mode) {
		result["teams"] = json(Json::arrayValue);
		for (const team_t team : { TEAM_RED, TEAM_BLUE }) {
			json team_json(Json::objectValue);
			team_json["teamName"] = TeamLabel(team);
			team_json["score"] = team == TEAM_RED ? match.red_score : match.blue_score;
			const mm_match_player_outcome_t outcome = match.red_score == match.blue_score
				? mm_match_player_outcome_t::draw
				: ((team == TEAM_RED) == (match.red_score > match.blue_score)
					? mm_match_player_outcome_t::win
					: mm_match_player_outcome_t::loss);
			team_json["outcome"] = OutcomeName(outcome);
			team_json["players"] = json(Json::arrayValue);
			for (const FrozenPlayer &player : match.players)
				if (player.team == team)
					team_json["players"].append(player_json_with_awards(player));
			result["teams"].append(std::move(team_json));
		}
	}

	if (match.ctf_mode) {
		const mm_match_ctf_team_stats_t &red = match.overall.ctf[0];
		const mm_match_ctf_team_stats_t &blue = match.overall.ctf[1];
		json &ctf = result["gametype"]["ctf"];
		ctf["totals"]["flagsCaptured"] = Json::UInt64(
			MM_MatchStatsWideCounterSum(red.captures, blue.captures));
		ctf["totals"]["flagAssists"] = Json::UInt64(
			MM_MatchStatsWideCounterSum(red.assists, blue.assists));
		ctf["totals"]["flagDefends"] = Json::UInt64(
			MM_MatchStatsWideCounterSum(red.defences, blue.defences));
		ctf["totals"]["flagPickups"] = Json::UInt64(
			MM_MatchStatsWideCounterSum(red.flag_pickups, blue.flag_pickups));
		ctf["totals"]["flagDrops"] = Json::UInt64(
			MM_MatchStatsWideCounterSum(red.flag_drops, blue.flag_drops));
		ctf["totals"]["flagReturns"] = Json::UInt64(
			MM_MatchStatsWideCounterSum(red.flag_returns, blue.flag_returns));
		ctf["totals"]["flagHoldTimeTotalMsec"] =
			Json::UInt64(MM_MatchStatsSaturatingDurationSum(
				red.flag_hold_time_total_msec,
				blue.flag_hold_time_total_msec));
		const uint32_t shortest = !red.flag_hold_time_shortest_msec
			? blue.flag_hold_time_shortest_msec
			: !blue.flag_hold_time_shortest_msec
				? red.flag_hold_time_shortest_msec
				: std::min(red.flag_hold_time_shortest_msec,
					blue.flag_hold_time_shortest_msec);
		ctf["totals"]["flagHoldTimeShortestMsec"] = shortest;
		ctf["totals"]["flagHoldTimeLongestMsec"] =
			std::max(red.flag_hold_time_longest_msec,
				blue.flag_hold_time_longest_msec);
		ctf["teams"]["red"] = CtfTeamToJson(red);
		ctf["teams"]["blue"] = CtfTeamToJson(blue);
		ctf["players"] = json(Json::arrayValue);
		for (const FrozenPlayer &player : match.players) {
			json player_json = PlayerToJson(
				player, match.end_msec, true, match.ruleset_id);
			if (!player_json.isMember("gametype") ||
				!player_json["gametype"].isMember("ctf"))
				continue;
			json entry(Json::objectValue);
			entry["socialID"] = player.social_id;
			entry["playerIdentifier"] = player.social_id.empty()
				? player.player_name : player.social_id;
			entry["playerName"] = player.player_name;
			entry["team"] = TeamLabel(player.team);
			entry["stats"] = player_json["gametype"]["ctf"];
			ctf["players"].append(std::move(entry));
		}
		result["totalFlagsCaptured"] = Json::UInt64(
			MM_MatchStatsWideCounterSum(red.captures, blue.captures));
		result["totalFlagAssists"] = Json::UInt64(
			MM_MatchStatsWideCounterSum(red.assists, blue.assists));
		result["totalFlagDefends"] = Json::UInt64(
			MM_MatchStatsWideCounterSum(red.defences, blue.defences));
	} else {
		result["totalFlagsCaptured"] = 0;
		result["totalFlagAssists"] = 0;
		result["totalFlagDefends"] = 0;
	}

	if (!match.overall.event_log.empty()) {
		result["eventLog"] = json(Json::arrayValue);
		for (const mm_match_event_t &event : match.overall.event_log) {
			json entry(Json::objectValue);
			entry["time"] = Json::Int64(event.time_msec / 1000);
			entry["timeMsec"] = Json::Int64(event.time_msec);
			entry["event"] = event.event;
			result["eventLog"].append(std::move(entry));
		}
	}

	if (!match.overall.death_log.empty()) {
		result["deathLog"] = json(Json::arrayValue);
		for (const mm_match_death_event_t &death : match.overall.death_log) {
			json entry(Json::objectValue);
			entry["time"] = Json::Int64(death.time_msec / 1000);
			entry["timeMsec"] = Json::Int64(death.time_msec);
			entry["victim"]["name"] = death.victim.name;
			entry["victim"]["id"] = death.victim.id;
			entry["attacker"]["name"] = death.attacker.name;
			entry["attacker"]["id"] = death.attacker.id;
			entry["mod"] = ModNameForRuleset(death.mod, match.ruleset_id);
			result["deathLog"].append(std::move(entry));
		}
	}

	return result;
}

void HtmlWritePlayerTable(std::ostringstream &html,
	const std::vector<const FrozenPlayer *> &players, int64_t match_end_msec)
{
	html << "<table><thead><tr><th>Player</th><th>Team</th><th>Outcome</th>"
		"<th>Score</th><th>SR</th><th>&Delta;SR</th><th>Kills</th>"
		"<th>Spawn Kills</th><th>Team Kills</th><th>Deaths</th><th>Suicides</th>"
		"<th>K/D</th><th>Damage +</th><th>Damage -</th>"
		"<th>Accuracy</th><th>Play Time</th></tr></thead><tbody>";
	for (const FrozenPlayer *player : players) {
		const mm_match_player_stats_t &stats = player->stats;
		html << "<tr><td>" << HtmlEscape(player->player_name) << "</td><td>"
			<< HtmlEscape(TeamLabel(player->team)) << "</td><td>"
			<< OutcomeName(player->outcome) << "</td><td>" << player->score
			<< "</td><td>";
		if (!player->bot && player->metadata.has_skill_rating)
			html << player->metadata.skill_rating;
		else
			html << "&mdash;";
		html << "</td><td>";
		if (!player->bot && player->metadata.has_skill_rating) {
			if (player->metadata.skill_rating_change >= 0)
				html << '+';
			html << player->metadata.skill_rating_change;
		} else {
			html << "&mdash;";
		}
		html << "</td><td>" << stats.total_kills
			<< "</td><td>" << stats.total_spawn_kills
			<< "</td><td>" << stats.total_team_kills
			<< "</td><td>" << stats.total_deaths
			<< "</td><td>" << stats.total_suicides << "</td><td>"
			<< std::fixed << std::setprecision(2)
			<< Ratio(stats.total_kills, stats.total_deaths) << "</td><td>"
			<< stats.total_dmg_dealt << "</td><td>"
			<< stats.total_dmg_received << "</td><td>"
			<< std::setprecision(1)
			<< Percent(stats.total_hits, stats.total_shots) << "%</td><td>"
			<< HtmlEscape(FormatDurationMsec(
				PlayerPlayTime(*player, match_end_msec))) << "</td></tr>";
	}
	html << "</tbody></table>";
}

std::string MatchToHtml(const FrozenMatch &match)
{
	std::ostringstream html;
	html << "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">"
		"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
		"<title>Match " << HtmlEscape(match.match_id) << " - "
		<< HtmlEscape(match.game_type) << "</title><style>"
		":root{color-scheme:dark}body{font-family:system-ui,-apple-system,Segoe UI,"
		"sans-serif;background:#11151b;color:#e9edf2;margin:0;padding:24px}"
		"main{max-width:1200px;margin:auto}h1,h2,h3{color:#fff}"
		"section{background:#1b222c;border:1px solid #303b49;border-radius:10px;"
		"padding:18px;margin:16px 0;overflow:auto}.meta{display:grid;"
		"grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:10px}"
		".card{background:#151b23;border-radius:7px;padding:10px}"
		"table{border-collapse:collapse;width:100%;min-width:650px}"
		"th,td{border-bottom:1px solid #303b49;padding:8px;text-align:left}"
		"th{color:#9fc7ff}.red{color:#ff8e8e}.blue{color:#86b8ff}"
		".muted{color:#9aa7b5}code{color:#c9e1ff}</style></head><body><main>";
	html << "<h1>Match Report</h1><section><div class=\"meta\">";
	auto card = [&](std::string_view label, std::string_view value) {
		html << "<div class=\"card\"><div class=\"muted\">"
			<< HtmlEscape(label) << "</div><strong>" << HtmlEscape(value)
			<< "</strong></div>";
	};
	card("Match ID", match.match_id);
	card("Server", match.server_name);
	if (!match.server_host_name.empty())
		card("Host", match.server_host_name);
	card("Gametype", match.game_type);
	card("Ruleset", match.rule_set);
	card("Map", match.map_name);
	card("Started", FormatUtc(match.start_msec));
	card("Ended", FormatUtc(match.end_msec));
	card("Duration", FormatDurationMsec(
		static_cast<uint64_t>(std::max<int64_t>(0, match.duration_msec))));
	card("Kills", std::to_string(match.overall.total_kills));
	card("Deaths", std::to_string(match.overall.total_deaths));
	card("Spawn Kills", std::to_string(match.overall.total_spawn_kills));
	card("Team Kills", std::to_string(match.overall.total_team_kills));
	card("Suicides", std::to_string(match.overall.total_suicides));
	html << "</div></section>";

	if (match.team_mode) {
		html << "<section><h2>Team Result</h2><table><thead><tr><th>Team</th>"
			"<th>Score</th><th>Outcome</th></tr></thead><tbody>";
		const bool draw = match.red_score == match.blue_score;
		html << "<tr><td class=\"red\">Red</td><td>" << match.red_score
			<< "</td><td>" << (draw ? "draw" : match.red_score > match.blue_score
				? "win" : "loss") << "</td></tr>";
		html << "<tr><td class=\"blue\">Blue</td><td>" << match.blue_score
			<< "</td><td>" << (draw ? "draw" : match.blue_score > match.red_score
				? "win" : "loss") << "</td></tr></tbody></table></section>";
	}

	std::vector<const FrozenPlayer *> ordered;
	ordered.reserve(match.players.size());
	for (const FrozenPlayer &player : match.players)
		ordered.push_back(&player);
	std::sort(ordered.begin(), ordered.end(), [](const FrozenPlayer *left,
		const FrozenPlayer *right) { return left->score > right->score; });
	html << "<section><h2>Overall Scores</h2>";
	HtmlWritePlayerTable(html, ordered, match.end_msec);
	if (match.overall.players_truncated)
		html << "<p class=\"muted\">Departed-player history was truncated after "
			<< MM_MATCH_DEPARTED_PLAYER_LIMIT << " participants.</p>";
	html << "</section>";

	if (match.ctf_mode) {
		html << "<section><h2>CTF Summary</h2><table><thead><tr><th>Team / Flag</th>"
			"<th>Captures</th><th>Assists</th><th>Defences</th><th>Pickups</th>"
			"<th>Drops</th><th>Returns</th><th>Hold Time</th></tr></thead><tbody>";
		for (size_t index = 0; index < 2; index++) {
			const mm_match_ctf_team_stats_t &ctf = match.overall.ctf[index];
			html << "<tr><td class=\"" << (index == 0 ? "red\">Red" : "blue\">Blue")
				<< "</td><td>" << ctf.captures << "</td><td>" << ctf.assists
				<< "</td><td>" << ctf.defences << "</td><td>" << ctf.flag_pickups
				<< "</td><td>" << ctf.flag_drops << "</td><td>" << ctf.flag_returns
				<< "</td><td>" << HtmlEscape(FormatDurationMsec(
					ctf.flag_hold_time_total_msec)) << "</td></tr>";
		}
		html << "</tbody></table></section>";
	}

	// [MuffMode] The post-match awards reel, in the order it was shown.
	html << "<section><h2>Match Awards</h2><table><thead><tr><th>Award</th>"
		"<th>Player</th><th>Value</th></tr></thead><tbody>";
	for (const mm_award_result_t &award : match.awards) {
		if (award.player_index >= match.players.size())
			continue;
		html << "<tr><td>" << HtmlEscape(MM_AwardTitle(award.award))
			<< "</td><td>"
			<< HtmlEscape(match.players[award.player_index].player_name)
			<< "</td><td>" << award.value << "</td></tr>";
	}
	if (match.awards.empty()) {
		html << "<tr><td colspan=\"3\">"
			<< (match.awards_ranked
				? "No awards were earned."
				: "Awards are only offered in ranked matches.")
			<< "</td></tr>";
	}
	html << "</tbody></table></section>";

	html << "<section><h2>High-value Pickups</h2><table><thead><tr><th>Item</th>"
		"<th>Pickups</th><th>Average Delay</th></tr></thead><tbody>";
	bool wrote_pickup = false;
	for (size_t index = 1; index < MM_MATCH_HIGH_VALUE_ITEM_COUNT; index++) {
		const uint32_t count = match.overall.pickup_counts[index];
		if (!count)
			continue;
		wrote_pickup = true;
		html << "<tr><td>" << HtmlEscape(k_high_value_item_names[index])
			<< "</td><td>" << count << "</td><td>"
			<< HtmlEscape(FormatDurationMsec(
				match.overall.pickup_delay_total_msec[index] / count))
			<< "</td></tr>";
	}
	if (!wrote_pickup)
		html << "<tr><td colspan=\"3\">No high-value pickups recorded.</td></tr>";
	html << "</tbody></table></section>";

	html << "<section><h2>Means of Death</h2><table><thead><tr><th>MOD</th>"
		"<th>Kills</th><th>Deaths</th><th>K/D</th></tr></thead><tbody>";
	bool wrote_mod = false;
	for (size_t index = 0; index < MM_MATCH_MOD_COUNT; index++) {
		const uint32_t kills = match.overall.mod_kills[index];
		const uint32_t deaths = match.overall.mod_deaths[index];
		if (!kills && !deaths)
			continue;
		wrote_mod = true;
		html << "<tr><td>" << HtmlEscape(ModNameForRuleset(
			static_cast<uint8_t>(index), match.ruleset_id)) << "</td><td>" << kills
			<< "</td><td>" << deaths << "</td><td>" << std::fixed
			<< std::setprecision(2) << Ratio(kills, deaths) << "</td></tr>";
	}
	if (!wrote_mod)
		html << "<tr><td colspan=\"4\">No deaths recorded.</td></tr>";
	html << "</tbody></table></section>";

	if (!match.overall.event_log.empty()) {
		html << "<section><h2>Event Log</h2><table><thead><tr><th>Time</th>"
			"<th>Event</th></tr></thead><tbody>";
		for (const mm_match_event_t &event : match.overall.event_log)
			html << "<tr><td>" << HtmlEscape(FormatDurationMsec(
				static_cast<uint64_t>(std::max<int64_t>(0, event.time_msec))))
				<< "</td><td>" << HtmlEscape(event.event) << "</td></tr>";
		html << "</tbody></table>";
		if (match.overall.event_log_truncated)
			html << "<p class=\"muted\">Event log truncated after "
				<< MM_MATCH_EVENT_LIMIT << " entries.</p>";
		html << "</section>";
	}

	if (!match.overall.death_log.empty()) {
		html << "<section><h2>Death Log</h2><table><thead><tr><th>Time</th>"
			"<th>Attacker</th><th>Victim</th><th>MOD</th></tr></thead><tbody>";
		for (const mm_match_death_event_t &death : match.overall.death_log)
			html << "<tr><td>" << HtmlEscape(FormatDurationMsec(
				static_cast<uint64_t>(std::max<int64_t>(0, death.time_msec))))
				<< "</td><td>" << HtmlEscape(death.attacker.name)
				<< "</td><td>" << HtmlEscape(death.victim.name)
				<< "</td><td>" << HtmlEscape(
					ModNameForRuleset(death.mod, match.ruleset_id))
				<< "</td></tr>";
		html << "</tbody></table>";
		if (match.overall.death_log_truncated)
			html << "<p class=\"muted\">Death log truncated after "
				<< MM_MATCH_DEATH_EVENT_LIMIT << " entries.</p>";
		html << "</section>";
	}

	for (const FrozenPlayer *player : ordered) {
		const mm_match_player_stats_t &stats = player->stats;
		html << "<section><h2>" << HtmlEscape(player->player_name)
			<< "</h2><p>Team: " << HtmlEscape(TeamLabel(player->team))
			<< " | Outcome: " << OutcomeName(player->outcome)
			<< " | Score: " << player->score
			<< " | Kills: " << stats.total_kills
			<< " | Spawn Kills: " << stats.total_spawn_kills
			<< " | Team Kills: " << stats.total_team_kills
			<< " | Deaths: " << stats.total_deaths
			<< " | Suicides: " << stats.total_suicides
			<< " | Damage +: " << stats.total_dmg_dealt
			<< " | Damage -: " << stats.total_dmg_received << "</p>";

		if (match.ctf_mode) {
			html << "<h3>CTF Performance</h3><table><thead><tr>"
				"<th>Pickups</th><th>Drops</th><th>Returns</th><th>Assists</th>"
				"<th>Defences</th><th>Captures</th><th>Total Carry</th>"
				"<th>Shortest Carry</th><th>Longest Carry</th></tr></thead><tbody><tr><td>"
				<< stats.ctf_flag_pickups << "</td><td>" << stats.ctf_flag_drops
				<< "</td><td>" << stats.ctf_flag_returns << "</td><td>"
				<< stats.ctf_flag_assists << "</td><td>" << stats.ctf_flag_defences
				<< "</td><td>" << stats.ctf_flag_captures << "</td><td>"
				<< HtmlEscape(FormatDurationMsec(
					stats.ctf_flag_carrier_time_total_msec)) << "</td><td>"
				<< HtmlEscape(FormatDurationMsec(
					stats.ctf_flag_carrier_time_shortest_msec)) << "</td><td>"
				<< HtmlEscape(FormatDurationMsec(
					stats.ctf_flag_carrier_time_longest_msec))
				<< "</td></tr></tbody></table>";
		}

		html << "<h3>High-value Pickups</h3><table><thead><tr><th>Item</th>"
			"<th>Pickups</th><th>Average Delay</th></tr></thead><tbody>";
		bool wrote_player_pickup = false;
		for (size_t index = 1; index < MM_MATCH_HIGH_VALUE_ITEM_COUNT; index++) {
			const uint32_t count = stats.pickup_counts[index];
			if (!count)
				continue;
			wrote_player_pickup = true;
			html << "<tr><td>" << HtmlEscape(k_high_value_item_names[index])
				<< "</td><td>" << count << "</td><td>"
				<< HtmlEscape(FormatDurationMsec(
					stats.pickup_delay_total_msec[index] / count))
				<< "</td></tr>";
		}
		if (!wrote_player_pickup)
			html << "<tr><td colspan=\"3\">No high-value pickups recorded.</td></tr>";
		html << "</tbody></table><h3>Weapon Stats</h3><table><thead><tr><th>Weapon</th>"
			"<th>Shots</th><th>Hits</th><th>Accuracy</th></tr></thead><tbody>";
		bool wrote_weapon = false;
		for (size_t index = 1; index < MM_MATCH_WEAPON_COUNT; index++) {
			const uint64_t shots = stats.total_shots_per_weapon[index];
			const uint64_t hits = stats.total_hits_per_weapon[index];
			if (!shots && !hits)
				continue;
			wrote_weapon = true;
			html << "<tr><td>" << k_weapon_abbreviations[index]
				<< "</td><td>" << shots << "</td><td>" << hits
				<< "</td><td>" << std::fixed << std::setprecision(1)
				<< Percent(hits, shots) << "%</td></tr>";
		}
		if (!wrote_weapon)
			html << "<tr><td colspan=\"4\">No weapon usage recorded.</td></tr>";
		html << "</tbody></table><h3>Means-of-Death Stats</h3><table><thead><tr>"
			"<th>MOD</th><th>Kills</th><th>Deaths</th><th>K/D</th>"
			"<th>Damage +</th><th>Damage -</th></tr></thead><tbody>";
		bool wrote_player_mod = false;
		for (size_t index = 0; index < MM_MATCH_MOD_COUNT; index++) {
			const uint32_t kills = stats.mod_total_kills[index];
			const uint32_t deaths = stats.mod_total_deaths[index];
			const uint64_t damage_dealt = stats.mod_total_dmg_dealt[index];
			const uint64_t damage_received = stats.mod_total_dmg_received[index];
			if (!kills && !deaths && !damage_dealt && !damage_received)
				continue;
			wrote_player_mod = true;
			html << "<tr><td>" << HtmlEscape(ModNameForRuleset(
				static_cast<uint8_t>(index), match.ruleset_id)) << "</td><td>" << kills
				<< "</td><td>" << deaths << "</td><td>" << std::fixed
				<< std::setprecision(2) << Ratio(kills, deaths) << "</td><td>"
				<< damage_dealt << "</td><td>" << damage_received << "</td></tr>";
		}
		if (!wrote_player_mod)
			html << "<tr><td colspan=\"6\">No means-of-death data recorded.</td></tr>";
		html << "</tbody></table><h3>Medals</h3><table><thead><tr><th>Medal</th>"
			"<th>Count</th></tr></thead><tbody>";
		bool wrote_award = false;
		for (size_t index = 1; index < MM_MATCH_MEDAL_COUNT; index++) {
			if (!stats.medal_count[index])
				continue;
			wrote_award = true;
			html << "<tr><td>" << HtmlEscape(k_medal_names[index])
				<< "</td><td>" << stats.medal_count[index] << "</td></tr>";
		}
		if (!wrote_award)
			html << "<tr><td colspan=\"2\">No medals recorded.</td></tr>";
		html << "</tbody></table>";

		// [MuffMode] The titles this player took off the post-match reel. The
		// section list is sorted, so the award index is resolved against the
		// underlying participant vector the pointers came from.
		const size_t player_index =
			static_cast<size_t>(player - match.players.data());
		bool wrote_match_award = false;
		for (const mm_award_result_t &award : match.awards) {
			if (award.player_index != player_index)
				continue;
			if (!wrote_match_award) {
				html << "<h3>Match Awards</h3><table><thead><tr><th>Award</th>"
					"<th>Value</th></tr></thead><tbody>";
				wrote_match_award = true;
			}
			html << "<tr><td>" << HtmlEscape(MM_AwardTitle(award.award))
				<< "</td><td>" << award.value << "</td></tr>";
		}
		if (wrote_match_award)
			html << "</tbody></table>";

		html << "</section>";
	}

	html << "<p class=\"muted\">Compiled by " << HtmlEscape(GAMEMOD_TITLE)
		<< ' ' << HtmlEscape(GAMEMOD_VERSION) << "</p></main></body></html>";
	return html.str();
}

uint64_t MixEntropy(uint64_t value) noexcept
{
	value += 0x9e3779b97f4a7c15ULL;
	value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
	value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
	return value ^ (value >> 31);
}

uint64_t ExportProcessEntropy() noexcept
{
	static const uint64_t entropy = []() noexcept {
#ifdef _WIN32
		const uint64_t process_id = static_cast<uint64_t>(GetCurrentProcessId());
#else
		const uint64_t process_id = static_cast<uint64_t>(getpid());
#endif
		const uint64_t system_ticks = static_cast<uint64_t>(
			std::chrono::system_clock::now().time_since_epoch().count());
		const uint64_t steady_ticks = static_cast<uint64_t>(
			std::chrono::steady_clock::now().time_since_epoch().count());
		const uint64_t stack_entropy = static_cast<uint64_t>(
			reinterpret_cast<uintptr_t>(&process_id));
		return MixEntropy(process_id ^ MixEntropy(system_ticks) ^
			MixEntropy(steady_ticks) ^ stack_entropy);
	}();
	return entropy;
}

std::filesystem::path UniqueSiblingPath(const std::filesystem::path &path,
	std::string_view purpose)
{
	static std::atomic<uint64_t> sequence{1};
	const uint64_t token = sequence.fetch_add(1);
	std::ostringstream suffix;
	suffix << '.' << purpose << '.' << std::hex << ExportProcessEntropy()
		<< '.' << token;
	std::filesystem::path result = path;
	result += suffix.str();
	return result;
}

void RemoveFileNoThrow(const std::filesystem::path &path) noexcept
{
	std::error_code ignored;
	std::filesystem::remove(path, ignored);
}

bool WriteUniqueTemporaryFile(const std::filesystem::path &final_path,
	std::string_view contents, std::filesystem::path &temporary_path,
	std::string &error)
{
	for (int attempt = 0; attempt < 32; ++attempt) {
		std::filesystem::path candidate = UniqueSiblingPath(final_path, "tmp");
#ifdef _WIN32
		HANDLE file = CreateFileW(candidate.c_str(), GENERIC_WRITE, 0, nullptr,
			CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE) {
			const DWORD create_error = GetLastError();
			if (create_error == ERROR_FILE_EXISTS ||
				create_error == ERROR_ALREADY_EXISTS)
				continue;
			error = "temporary file creation: " +
				std::error_code(static_cast<int>(create_error),
					std::system_category()).message();
			return false;
		}

		bool write_ok = true;
		DWORD write_error = ERROR_SUCCESS;
		size_t offset = 0;
		while (offset < contents.size()) {
			const DWORD requested = static_cast<DWORD>(std::min<size_t>(
				contents.size() - offset,
				static_cast<size_t>(std::numeric_limits<DWORD>::max())));
			DWORD written = 0;
			const BOOL write_succeeded = WriteFile(file,
				contents.data() + offset, requested, &written, nullptr);
			if (!write_succeeded || written != requested) {
				write_error = write_succeeded ? ERROR_WRITE_FAULT : GetLastError();
				write_ok = false;
				break;
			}
			offset += written;
		}
		if (write_ok && !FlushFileBuffers(file)) {
			write_error = GetLastError();
			write_ok = false;
		}
		CloseHandle(file);
		if (!write_ok) {
			RemoveFileNoThrow(candidate);
			error = "temporary file write/flush: " +
				std::error_code(static_cast<int>(write_error),
					std::system_category()).message();
			return false;
		}
#else
		int descriptor = open(candidate.c_str(),
			O_WRONLY | O_CREAT | O_EXCL
#ifdef O_CLOEXEC
			| O_CLOEXEC
#endif
			, 0644);
		if (descriptor < 0) {
			const int create_error = errno;
			if (create_error == EEXIST)
				continue;
			error = "temporary file creation: " +
				std::error_code(create_error, std::generic_category()).message();
			return false;
		}

		bool write_ok = true;
		int write_error = 0;
		size_t offset = 0;
		while (offset < contents.size()) {
			const ssize_t written = write(descriptor, contents.data() + offset,
				contents.size() - offset);
			if (written < 0 && errno == EINTR)
				continue;
			if (written <= 0) {
				write_error = written < 0 ? errno : EIO;
				write_ok = false;
				break;
			}
			offset += static_cast<size_t>(written);
		}
		while (write_ok && fsync(descriptor) != 0) {
			if (errno == EINTR)
				continue;
			write_error = errno;
			write_ok = false;
		}
		if (close(descriptor) != 0 && write_ok) {
			write_error = errno;
			write_ok = false;
		}
		if (!write_ok) {
			RemoveFileNoThrow(candidate);
			error = "temporary file write/flush: " +
				std::error_code(write_error, std::generic_category()).message();
			return false;
		}
#endif
		temporary_path = std::move(candidate);
		return true;
	}

	error = "could not allocate a unique temporary file name";
	return false;
}

bool WriteTextFileAtomically(const std::filesystem::path &final_path,
	std::string_view contents, std::string &error)
{
	std::filesystem::path temporary_path;
	if (!WriteUniqueTemporaryFile(
			final_path, contents, temporary_path, error))
		return false;

#ifdef _WIN32
	if (!MoveFileExW(temporary_path.c_str(), final_path.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		const DWORD replace_error = GetLastError();
		RemoveFileNoThrow(temporary_path);
		error = "atomic replace: " +
			std::error_code(static_cast<int>(replace_error),
				std::system_category()).message();
		return false;
	}
#else
	std::error_code filesystem_error;
	std::filesystem::rename(temporary_path, final_path, filesystem_error);
	if (filesystem_error) {
		RemoveFileNoThrow(temporary_path);
		error = "atomic replace: " + filesystem_error.message();
		return false;
	}

	const std::filesystem::path parent = final_path.parent_path().empty()
		? std::filesystem::path(".") : final_path.parent_path();
	int directory = open(parent.c_str(), O_RDONLY
#ifdef O_DIRECTORY
		| O_DIRECTORY
#endif
		);
	if (directory < 0) {
		error = "directory flush open: " +
			std::error_code(errno, std::generic_category()).message();
		return false;
	}
	int flush_result;
	do {
		flush_result = fsync(directory);
	} while (flush_result != 0 && errno == EINTR);
	const int flush_error = flush_result == 0 ? 0 : errno;
	close(directory);
	if (flush_error) {
		error = "directory flush: " +
			std::error_code(flush_error, std::generic_category()).message();
		return false;
	}
#endif
	return true;
}

void InitializeCatalog(json &catalog)
{
	catalog = json(Json::objectValue);
	AddArtifactMetadata(catalog, k_catalog_schema_name, k_catalog_schema_version,
		k_catalog_artifact_type, k_catalog_artifact_version);
	catalog["generatedAtMS"] = Json::Int64(NowUnixMsec());
	catalog["artifacts"] = json(Json::arrayValue);
	catalog["latest"] = json(Json::objectValue);
	catalog["artifactCount"] = 0;
}

bool CatalogMetadataValid(const json &catalog)
{
	return catalog.isObject() && catalog["schemaName"].isString() &&
		catalog["schemaName"].asString() == k_catalog_schema_name &&
		catalog["schemaVersion"].isInt() &&
		catalog["schemaVersion"].asInt() == k_catalog_schema_version &&
		catalog["artifactType"].isString() &&
		catalog["artifactType"].asString() == k_catalog_artifact_type &&
		catalog["artifactVersion"].isInt() &&
		catalog["artifactVersion"].asInt() == k_catalog_artifact_version;
}

enum class CatalogReadStatus : uint8_t {
	valid,
	missing,
	corrupt,
	io_error
};

struct CatalogReadResult {
	CatalogReadStatus status = CatalogReadStatus::io_error;
	json catalog;
	std::string error;
};

bool CatalogStructureValid(const json &catalog, std::string &error)
{
	if (!CatalogMetadataValid(catalog)) {
		error = "catalog metadata is invalid";
		return false;
	}
	const json &generated_at = catalog["generatedAtMS"];
	if (!generated_at.isUInt64() &&
		(!generated_at.isInt64() || generated_at.asInt64() < 0)) {
		error = "generatedAtMS must be a non-negative integer";
		return false;
	}
	if (!catalog["artifacts"].isArray()) {
		error = "artifacts must be an array";
		return false;
	}
	if (catalog["artifacts"].size() > k_max_catalog_parse_entries) {
		error = "artifact array exceeds the parse limit";
		return false;
	}
	if (!catalog["latest"].isObject()) {
		error = "latest must be an object";
		return false;
	}
	const json &artifact_count = catalog["artifactCount"];
	const bool signed_count_valid = artifact_count.isInt64() &&
		artifact_count.asInt64() >= 0;
	const uint64_t artifact_count_value = artifact_count.isUInt64()
		? artifact_count.asUInt64()
		: (signed_count_valid
			? static_cast<uint64_t>(artifact_count.asInt64())
			: std::numeric_limits<uint64_t>::max());
	if ((!signed_count_valid && !artifact_count.isUInt64()) ||
		artifact_count_value != catalog["artifacts"].size()) {
		error = "artifactCount does not match artifacts";
		return false;
	}

	std::unordered_map<std::string, std::unordered_set<std::string>>
		artifact_identities;
	for (const json &artifact : catalog["artifacts"]) {
		if (!artifact.isObject() || !artifact["artifactType"].isString() ||
			!artifact["id"].isString() ||
			artifact["artifactType"].asString().empty() ||
			artifact["id"].asString().empty()) {
			error = "artifact identity is invalid";
			return false;
		}
		const std::vector<std::string> members = artifact.getMemberNames();
		if (members.size() > 32 ||
			artifact["artifactType"].asString().size() >
				k_max_catalog_identity_length ||
			artifact["id"].asString().size() > k_max_catalog_identity_length) {
			error = "artifact identity exceeds its limit";
			return false;
		}
		const std::string artifact_type = artifact["artifactType"].asString();
		const std::string artifact_id = artifact["id"].asString();
		if (!MM_IsWellFormedUtf8(artifact_type) ||
			!MM_IsWellFormedUtf8(artifact_id)) {
			error = "artifact identity is not valid UTF-8";
			return false;
		}
		auto type = artifact_identities.find(artifact_type);
		if (type == artifact_identities.end()) {
			if (!MM_MatchStatsCatalogTypeCountValid(
					artifact_identities.size() + 1)) {
				error = "artifact types exceed the catalog limit";
				return false;
			}
			type = artifact_identities.emplace(artifact_type,
				std::unordered_set<std::string>{}).first;
		}
		if (!type->second.insert(artifact_id).second) {
			error = "artifact identity is duplicated";
			return false;
		}
		for (const std::string &member : members) {
			const json &value = artifact[member];
			if (member.size() > 64 || !MM_IsWellFormedUtf8(member) ||
				value.isArray() || value.isObject() ||
				(value.isString() && value.asString().size() >
					k_max_catalog_identity_length)) {
				error = "artifact contains an unsupported or oversized field";
				return false;
			}
			if (value.isString() &&
				!MM_IsWellFormedUtf8(value.asString())) {
				error = "artifact contains a string that is not valid UTF-8";
				return false;
			}
		}
	}

	const std::vector<std::string> latest_members =
		catalog["latest"].getMemberNames();
	if (!MM_MatchStatsCatalogTypeCountValid(latest_members.size())) {
		error = "latest contains too many artifact types";
		return false;
	}
	for (const std::string &member : latest_members) {
		const json &value = catalog["latest"][member];
		if (member.empty() || member.size() > k_max_catalog_identity_length ||
			!MM_IsWellFormedUtf8(member) ||
			!value.isString() || value.asString().empty() ||
			value.asString().size() > k_max_catalog_identity_length ||
			!MM_IsWellFormedUtf8(value.asString())) {
			error = "latest contains an invalid identity";
			return false;
		}
	}
	return true;
}

bool CatalogLatestValid(const json &catalog, std::string &error);

CatalogReadResult ReadCatalog(const std::filesystem::path &path)
{
	CatalogReadResult result;
	std::error_code filesystem_error;
	const bool exists = std::filesystem::exists(path, filesystem_error);
	if (filesystem_error) {
		result.error = "existence check: " + filesystem_error.message();
		return result;
	}
	if (!exists) {
		result.status = CatalogReadStatus::missing;
		InitializeCatalog(result.catalog);
		return result;
	}

	const uintmax_t length = std::filesystem::file_size(path, filesystem_error);
	if (filesystem_error) {
		result.error = "file size: " + filesystem_error.message();
		return result;
	}
	if (length > k_max_catalog_bytes) {
		result.status = CatalogReadStatus::corrupt;
		result.error = "catalog exceeds the byte limit";
		return result;
	}

	std::ifstream file(path, std::ios::binary);
	if (!file.is_open()) {
		result.error = "file could not be opened";
		return result;
	}
	std::string text;
	try {
		text.resize(static_cast<size_t>(length));
	} catch (const std::exception &exception) {
		result.error = std::string("read buffer allocation: ") + exception.what();
		return result;
	}
	if (!text.empty()) {
		file.read(text.data(), static_cast<std::streamsize>(text.size()));
		if (file.gcount() != static_cast<std::streamsize>(text.size())) {
			result.error = "file could not be read completely";
			return result;
		}
	}
	char extra = 0;
	if (file.get(extra)) {
		result.status = CatalogReadStatus::corrupt;
		result.error = "catalog changed while it was being read";
		return result;
	}
	if (file.bad()) {
		result.error = "file read failed";
		return result;
	}
	if (text.find('\0') != std::string::npos) {
		result.status = CatalogReadStatus::corrupt;
		result.error = "catalog contains an embedded NUL byte";
		return result;
	}

	Json::CharReaderBuilder builder;
	builder["collectComments"] = false;
	builder["allowComments"] = false;
	builder["allowTrailingCommas"] = false;
	builder["strictRoot"] = true;
	builder["allowDroppedNullPlaceholders"] = false;
	builder["allowNumericKeys"] = false;
	builder["allowSingleQuotes"] = false;
	builder["stackLimit"] = 64;
	builder["failIfExtra"] = true;
	builder["rejectDupKeys"] = true;
	builder["allowSpecialFloats"] = false;
	builder["skipBom"] = true;

	std::string parse_error;
	try {
		const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		if (!reader || !reader->parse(text.data(), text.data() + text.size(),
				&result.catalog, &parse_error)) {
			result.status = CatalogReadStatus::corrupt;
			result.error = "JSON parse failed: " + parse_error;
			return result;
		}
	} catch (const std::exception &exception) {
		result.status = CatalogReadStatus::corrupt;
		result.error = std::string("JSON parse failed: ") + exception.what();
		return result;
	}

	if (!CatalogStructureValid(result.catalog, result.error) ||
		!CatalogLatestValid(result.catalog, result.error)) {
		result.status = CatalogReadStatus::corrupt;
		return result;
	}
	result.status = CatalogReadStatus::valid;
	return result;
}

bool SameCatalogArtifact(const json &left, const json &right)
{
	return left.isObject() && right.isObject() &&
		left["artifactType"].isString() &&
		right["artifactType"].isString() && left["id"].isString() &&
		right["id"].isString() &&
		left["artifactType"].asString() == right["artifactType"].asString() &&
		left["id"].asString() == right["id"].asString();
}

std::optional<int64_t> CatalogTimestamp(const json &entry, const char *key)
{
	const json &value = entry[key];
	if (value.isInt64())
		return value.asInt64();
	if (value.isUInt64())
		return static_cast<int64_t>(std::min<Json::UInt64>(
			value.asUInt64(), static_cast<Json::UInt64>(
				std::numeric_limits<int64_t>::max())));
	return std::nullopt;
}

bool CatalogArtifactIsNewer(const json &candidate, const json &current)
{
	const std::optional<int64_t> candidate_end =
		CatalogTimestamp(candidate, "matchEndMS");
	const std::optional<int64_t> current_end =
		CatalogTimestamp(current, "matchEndMS");
	if (candidate_end != current_end) {
		if (!candidate_end)
			return false;
		if (!current_end)
			return true;
		return *candidate_end > *current_end;
	}

	const std::optional<int64_t> candidate_start =
		CatalogTimestamp(candidate, "matchStartMS");
	const std::optional<int64_t> current_start =
		CatalogTimestamp(current, "matchStartMS");
	if (candidate_start != current_start) {
		if (!candidate_start)
			return false;
		if (!current_start)
			return true;
		return *candidate_start > *current_start;
	}

	// Timestamped match artifacts use the ID as a deterministic final tie-break.
	// Other artifact types retain catalog append-order semantics.
	if (candidate_end || candidate_start)
		return candidate["id"].asString() > current["id"].asString();
	return true;
}

bool CatalogLatestValid(const json &catalog, std::string &error)
{
	std::unordered_map<std::string, const json *> expected_latest;
	for (const json &artifact : catalog["artifacts"]) {
		const std::string type = artifact["artifactType"].asString();
		const auto existing = expected_latest.find(type);
		if (existing == expected_latest.end() ||
			CatalogArtifactIsNewer(artifact, *existing->second))
			expected_latest[type] = &artifact;
	}
	if (expected_latest.size() != catalog["latest"].size()) {
		error = "latest does not cover the catalog artifact types";
		return false;
	}
	for (const auto &[type, artifact] : expected_latest) {
		const json &latest_id = catalog["latest"][type];
		if (!latest_id.isString() ||
			latest_id.asString() != (*artifact)["id"].asString()) {
			error = "latest does not identify the newest artifact";
			return false;
		}
	}
	return true;
}

std::mutex g_catalog_mutex;

class CatalogProcessLock {
public:
	CatalogProcessLock() = default;
	CatalogProcessLock(const CatalogProcessLock &) = delete;
	CatalogProcessLock &operator=(const CatalogProcessLock &) = delete;

	~CatalogProcessLock()
	{
#ifdef _WIN32
		if (handle_ != INVALID_HANDLE_VALUE)
			CloseHandle(handle_);
#else
		if (descriptor_ >= 0) {
			flock(descriptor_, LOCK_UN);
			close(descriptor_);
		}
#endif
	}

	bool Acquire(const std::filesystem::path &path, int attempts,
		std::string &error)
	{
#ifdef _WIN32
		for (int attempt = 0; attempt < attempts; ++attempt) {
			handle_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
				0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (handle_ != INVALID_HANDLE_VALUE)
				return true;
			const DWORD lock_error = GetLastError();
			if (lock_error != ERROR_SHARING_VIOLATION &&
				lock_error != ERROR_LOCK_VIOLATION) {
				error = "catalog lock: " +
					std::error_code(static_cast<int>(lock_error),
						std::system_category()).message();
				return false;
			}
			std::this_thread::sleep_for(k_catalog_lock_retry_delay);
		}
		error = "catalog lock timed out";
		return false;
#else
		descriptor_ = open(path.c_str(), O_RDWR | O_CREAT
#ifdef O_CLOEXEC
			| O_CLOEXEC
#endif
			, 0644);
		if (descriptor_ < 0) {
			error = "catalog lock open: " +
				std::error_code(errno, std::generic_category()).message();
			return false;
		}
		for (int attempt = 0; attempt < attempts; ++attempt) {
			if (flock(descriptor_, LOCK_EX | LOCK_NB) == 0)
				return true;
			const int lock_error = errno;
			if (lock_error != EWOULDBLOCK && lock_error != EAGAIN &&
				lock_error != EINTR) {
				error = "catalog lock: " +
					std::error_code(lock_error, std::generic_category()).message();
				return false;
			}
			std::this_thread::sleep_for(k_catalog_lock_retry_delay);
		}
		error = "catalog lock timed out";
		return false;
#endif
	}

private:
#ifdef _WIN32
	HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
	int descriptor_ = -1;
#endif
};

bool QuarantineCatalog(const std::filesystem::path &path,
	std::filesystem::path &quarantined_path, std::string &error)
{
	std::error_code filesystem_error;
	const std::filesystem::file_status status =
		std::filesystem::symlink_status(path, filesystem_error);
	if (filesystem_error || status.type() != std::filesystem::file_type::regular) {
		error = filesystem_error
			? "catalog quarantine status: " + filesystem_error.message()
			: "catalog quarantine refused a non-regular file";
		return false;
	}

	const std::filesystem::path quarantine_directory =
		path.parent_path() / "quarantine";
	std::filesystem::create_directories(quarantine_directory, filesystem_error);
	if (filesystem_error) {
		error = "catalog quarantine directory: " + filesystem_error.message();
		return false;
	}

	for (int attempt = 0; attempt < 32; ++attempt) {
		const std::filesystem::path candidate = UniqueSiblingPath(
			quarantine_directory / path.filename(), "invalid");
		if (std::filesystem::exists(candidate, filesystem_error)) {
			if (filesystem_error) {
				error = "catalog quarantine existence check: " +
					filesystem_error.message();
				return false;
			}
			continue;
		}
		std::filesystem::rename(path, candidate, filesystem_error);
		if (!filesystem_error) {
			quarantined_path = candidate;
			return true;
		}
		error = "catalog quarantine rename: " + filesystem_error.message();
		return false;
	}

	error = "could not allocate a unique catalog quarantine name";
	return false;
}

void AppendExportMessage(std::string &target, std::string message)
{
	if (message.empty())
		return;
	if (!target.empty())
		target.append("; ");
	target.append(std::move(message));
}

bool UpdateCatalog(const std::filesystem::path &directory, json entry,
	bool html_written, int lock_attempts, std::string &error,
	std::string &warning)
{
	std::lock_guard<std::mutex> lock(g_catalog_mutex);
	const std::filesystem::path path = directory / k_catalog_file_name;
	CatalogProcessLock process_lock;
	if (!process_lock.Acquire(directory / "catalog.lock", lock_attempts, error))
		return false;

	CatalogReadResult read = ReadCatalog(path);
	if (read.status == CatalogReadStatus::io_error) {
		error = "catalog read: " + read.error;
		return false;
	}
	if (read.status == CatalogReadStatus::corrupt) {
		std::filesystem::path quarantined;
		std::string quarantine_error;
		if (!QuarantineCatalog(path, quarantined, quarantine_error)) {
			error = read.error + "; " + quarantine_error;
			return false;
		}
		AppendExportMessage(warning, "invalid catalog quarantined as " +
			quarantined.filename().string() + " (" + read.error + ")");
		InitializeCatalog(read.catalog);
	}
	json &catalog = read.catalog;

	if (!html_written)
		entry.removeMember("htmlPath");

	std::vector<json> artifacts;
	artifacts.reserve(std::min<size_t>(
		static_cast<size_t>(catalog["artifacts"].size()) + 1,
		k_max_catalog_parse_entries + 1));
	for (const json &existing : catalog["artifacts"])
		if (!SameCatalogArtifact(existing, entry))
			artifacts.push_back(existing);
	artifacts.push_back(entry);

	const auto timestamp_or_min = [](const json &artifact, const char *key) {
		return CatalogTimestamp(artifact, key).value_or(
			std::numeric_limits<int64_t>::min());
	};
	std::stable_sort(artifacts.begin(), artifacts.end(),
		[&](const json &left, const json &right) {
			const bool left_timestamped =
				CatalogTimestamp(left, "matchEndMS").has_value() ||
				CatalogTimestamp(left, "matchStartMS").has_value();
			const bool right_timestamped =
				CatalogTimestamp(right, "matchEndMS").has_value() ||
				CatalogTimestamp(right, "matchStartMS").has_value();
			if (left_timestamped != right_timestamped)
				return !left_timestamped;
			if (!left_timestamped) {
				// Untimestamped artifacts use append order for both retention and
				// latest-version authority. The stable sort preserves that order.
				return false;
			}
			const int64_t left_end = timestamp_or_min(left, "matchEndMS");
			const int64_t right_end = timestamp_or_min(right, "matchEndMS");
			if (left_end != right_end)
				return left_end < right_end;
			const int64_t left_start = timestamp_or_min(left, "matchStartMS");
			const int64_t right_start = timestamp_or_min(right, "matchStartMS");
			if (left_start != right_start)
				return left_start < right_start;
			if (left["artifactType"].asString() !=
				right["artifactType"].asString())
				return left["artifactType"].asString() <
					right["artifactType"].asString();
			return left["id"].asString() < right["id"].asString();
		});

	const auto remove_oldest = [&]() {
		if (artifacts.empty())
			return false;
		artifacts.erase(artifacts.begin());
		return true;
	};
	while (artifacts.size() > k_max_catalog_entries)
		if (!remove_oldest())
			break;
	const auto distinct_artifact_type_count = [&]() {
		std::unordered_set<std::string> types;
		for (const json &artifact : artifacts)
			types.insert(artifact["artifactType"].asString());
		return types.size();
	};
	while (!MM_MatchStatsCatalogTypeCountValid(
			distinct_artifact_type_count())) {
		if (!remove_oldest()) {
			error = "catalog artifact types exceed the retention limit";
			return false;
		}
	}

	const auto rebuild_catalog = [&]() {
		InitializeCatalog(catalog);
		catalog["artifacts"] = json(Json::arrayValue);
		std::unordered_map<std::string, const json *> latest;
		for (const json &artifact : artifacts) {
			catalog["artifacts"].append(artifact);
			const std::string type = artifact["artifactType"].asString();
			const auto existing = latest.find(type);
			if (existing == latest.end() ||
				CatalogArtifactIsNewer(artifact, *existing->second))
				latest[type] = &artifact;
		}
		for (const auto &[type, artifact] : latest)
			catalog["latest"][type] = (*artifact)["id"].asString();
		catalog["artifactCount"] =
			static_cast<Json::UInt64>(artifacts.size());
	};

	Json::StreamWriterBuilder writer;
	writer["indentation"] = "    ";
	std::string catalog_text;
	for (;;) {
		rebuild_catalog();
		std::string validation_error;
		if (!CatalogStructureValid(catalog, validation_error) ||
			!CatalogLatestValid(catalog, validation_error)) {
			error = "rebuilt catalog is invalid: " + validation_error;
			return false;
		}
		catalog_text = Json::writeString(writer, catalog);
		if (catalog_text.size() <= k_max_catalog_bytes)
			break;
		const size_t prune_count = std::max<size_t>(1, artifacts.size() / 8);
		bool removed = false;
		for (size_t index = 0; index < prune_count; ++index) {
			if (!remove_oldest())
				break;
			removed = true;
		}
		if (!removed) {
			error = "serialized catalog exceeds the byte limit";
			return false;
		}
	}
	return WriteTextFileAtomically(path, catalog_text, error);
}

struct ExportJob {
	uint64_t id = 0;
	std::filesystem::path directory;
	std::string file_stem;
	FrozenMatch match;
	bool write_html = false;
};

struct ExportResult {
	bool success = false;
	std::string error;
	std::string warning;
};

struct ExportCompletionNotice {
	uint64_t job_id = 0;
	bool success = false;
	std::string error;
	std::string warning;
};

json BuildCatalogEntry(const FrozenMatch &match, std::string_view file_stem);

ExportResult ExecuteExportJob(const ExportJob &job, int catalog_lock_attempts)
{
	ExportResult result;
	try {
		Json::StreamWriterBuilder writer;
		writer["indentation"] = "    ";
		std::string json_text;
		json catalog_entry;
		try {
			json_text = Json::writeString(writer, MatchToJson(job.match));
			catalog_entry = BuildCatalogEntry(job.match, job.file_stem);
		} catch (const std::exception &exception) {
			result.error = std::string("JSON/catalog serialization: ") +
				exception.what();
			return result;
		}

		bool html_available = job.write_html;
		std::string html_text;
		if (html_available) {
			try {
				html_text = MatchToHtml(job.match);
			} catch (const std::exception &exception) {
				html_available = false;
				AppendExportMessage(result.warning,
					std::string("HTML serialization: ") + exception.what());
			} catch (...) {
				html_available = false;
				AppendExportMessage(result.warning,
					"HTML serialization: unknown exception");
			}
		}

		std::error_code directory_error;
		std::filesystem::create_directories(job.directory, directory_error);
		if (directory_error) {
			result.error = "directory: " + directory_error.message();
			return result;
		}

		const std::filesystem::path base = job.directory / job.file_stem;
		std::filesystem::path json_path = base;
		json_path += ".json";
		std::filesystem::path html_path = base;
		html_path += ".html";
		std::string error;
		if (!WriteTextFileAtomically(json_path, json_text, error)) {
			result.error = "JSON: " + error;
			return result;
		}

		bool html_written = false;
		if (html_available) {
			if (!WriteTextFileAtomically(html_path, html_text, error)) {
				AppendExportMessage(result.warning, "HTML: " + error);
			} else {
				html_written = true;
			}
		}

		std::string catalog_warning;
		if (!UpdateCatalog(job.directory, std::move(catalog_entry), html_written,
				catalog_lock_attempts, error, catalog_warning)) {
			result.error = "catalog: " + error;
			return result;
		}
		AppendExportMessage(result.warning, std::move(catalog_warning));

		result.success = true;
	} catch (const std::exception &exception) {
		result.error = std::string("exception: ") + exception.what();
	} catch (...) {
		result.error = "unknown exception";
	}
	return result;
}

class MatchExportWorker {
public:
	~MatchExportWorker()
	{
		Shutdown();
	}

	bool Enqueue(std::unique_ptr<ExportJob> &job) noexcept
	{
		if (!job)
			return false;
		try {
			std::lock_guard<std::mutex> lock(mutex_);
			if (stopping_ || queue_.size() >= k_export_queue_capacity)
				return false;
			if (!thread_.joinable()) {
				stopping_ = false;
				thread_ = std::thread(&MatchExportWorker::Run, this);
			}
			// Allocate the queue node before transferring ownership. If allocation
			// fails, the caller retains the complete job for synchronous fallback.
			queue_.emplace_back();
			queue_.back().swap(job);
			pending_.fetch_add(1);
			submitted_.fetch_add(1);
			condition_.notify_one();
			return true;
		} catch (...) {
			return false;
		}
	}

	bool ExecuteFallback(std::unique_ptr<ExportJob> job)
	{
		if (!job)
			return false;
		submitted_.fetch_add(1);
		pending_.fetch_add(1);
		const uint64_t job_id = job->id;
		ExportResult result = ExecuteExportJob(
			*job, k_catalog_game_thread_lock_attempts);
		const bool success = result.success;
		Complete(job_id, std::move(result));
		return success;
	}

	void Shutdown()
	{
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (!thread_.joinable())
				return;
			stopping_ = true;
			condition_.notify_all();
		}
		thread_.join();
		{
			std::lock_guard<std::mutex> lock(mutex_);
			stopping_ = false;
		}
	}

	uint64_t Submitted() const { return submitted_.load(); }
	uint64_t Pending() const { return pending_.load(); }
	uint64_t Completed() const { return completed_.load(); }
	uint64_t Failed() const { return failed_.load(); }

	void DrainNotices()
	{
		std::deque<ExportCompletionNotice> notices;
		{
			std::lock_guard<std::mutex> lock(notice_mutex_);
			notices.swap(notices_);
		}
		for (const ExportCompletionNotice &notice : notices) {
			if (notice.success) {
				if (notice.warning.empty()) {
					gi.Com_PrintFmt("MM_MatchStats: export job {} succeeded (pending {}, completed {}, failed {})\n",
						notice.job_id, Pending(), Completed(), Failed());
				} else {
					gi.Com_PrintFmt("MM_MatchStats: export job {} succeeded with warning: {} (pending {}, completed {}, failed {})\n",
						notice.job_id, notice.warning, Pending(), Completed(), Failed());
				}
			} else {
				gi.Com_PrintFmt("MM_MatchStats: export job {} failed: {} (pending {}, completed {}, failed {})\n",
					notice.job_id, notice.error, Pending(), Completed(), Failed());
			}
		}
	}

	std::string LastError() const
	{
		std::lock_guard<std::mutex> lock(error_mutex_);
		return last_error_;
	}

private:
	void Complete(uint64_t job_id, ExportResult result) noexcept
	{
		pending_.fetch_sub(1);
		if (result.success) {
			completed_.fetch_add(1);
		} else {
			failed_.fetch_add(1);
			try {
				std::lock_guard<std::mutex> lock(error_mutex_);
				last_error_ = result.error;
			} catch (...) {
			}
		}
		try {
			std::lock_guard<std::mutex> lock(notice_mutex_);
			notices_.push_back({job_id, result.success, std::move(result.error),
				std::move(result.warning)});
		} catch (...) {
		}
	}

	void Run()
	{
		for (;;) {
			std::unique_ptr<ExportJob> job;
			{
				std::unique_lock<std::mutex> lock(mutex_);
				condition_.wait(lock, [&] { return stopping_ || !queue_.empty(); });
				if (queue_.empty()) {
					if (stopping_)
						return;
					continue;
				}
				job = std::move(queue_.front());
				queue_.pop_front();
			}
			if (!job)
				continue;
			const uint64_t job_id = job->id;
			Complete(job_id, ExecuteExportJob(
				*job, k_catalog_worker_lock_attempts));
		}
	}

	mutable std::mutex mutex_;
	std::condition_variable condition_;
	std::deque<std::unique_ptr<ExportJob>> queue_;
	std::thread thread_;
	bool stopping_ = false;
	std::atomic<uint64_t> submitted_{0};
	std::atomic<uint64_t> pending_{0};
	std::atomic<uint64_t> completed_{0};
	std::atomic<uint64_t> failed_{0};
	mutable std::mutex error_mutex_;
	std::string last_error_;
	mutable std::mutex notice_mutex_;
	std::deque<ExportCompletionNotice> notices_;
};

MatchExportWorker &ExportWorker()
{
	static MatchExportWorker worker;
	return worker;
}

json BuildCatalogEntry(const FrozenMatch &match, std::string_view file_stem)
{
	json entry(Json::objectValue);
	entry["artifactType"] = k_match_artifact_type;
	entry["schemaName"] = k_match_schema_name;
	entry["schemaVersion"] = k_match_schema_version;
	entry["artifactVersion"] = k_match_artifact_version;
	entry["id"] = match.match_id;
	entry["gameType"] = match.game_type;
	entry["ruleSet"] = match.rule_set;
	entry["mapName"] = match.map_name;
	entry["matchStartMS"] = Json::Int64(match.start_msec);
	entry["matchEndMS"] = Json::Int64(match.end_msec);
	entry["durationMS"] = Json::Int64(match.duration_msec);
	entry["playerCount"] = static_cast<int>(match.players.size());
	entry["teamCount"] = match.team_mode ? 2 : 0;
	entry["jsonPath"] = std::string(file_stem) + ".json";
	entry["htmlPath"] = std::string(file_stem) + ".html";
	return entry;
}

std::unique_ptr<ExportJob> BuildExportJob(FrozenMatch &&match, uint64_t job_id,
	bool write_html)
{
	const std::filesystem::path directory =
		std::filesystem::path(GAMEVERSION) / "matches";
	const std::string file_stem = SanitizeFileStem(match.match_id);
	auto job = std::make_unique<ExportJob>();
	job->id = job_id;
	job->directory = directory;
	job->file_stem = file_stem;
	job->match = std::move(match);
	job->write_html = write_html;
	return job;
}

FrozenMatch FreezeCurrentMatch()
{
	FrozenMatch match;
	match.match_id = level.match_id.empty()
		? std::string(gt_short_name[MM_EFFECTIVE_GT]) + "_" +
			std::to_string(level.match.match_start_real_time_ms)
		: level.match_id;
	match.server_name = hostname && hostname->string ? hostname->string : "";
	match.server_host_name = ServerHostName();
	const int gametype = clamp(MM_EFFECTIVE_GT, static_cast<int>(GT_NONE),
		static_cast<int>(GT_NUM_GAMETYPES) - 1);
	match.game_type = gt_short_name_upper[gametype];
	const size_t ruleset = std::min<size_t>(static_cast<size_t>(game.ruleset),
		RS_NUM_RULESETS - 1);
	match.ruleset_id = static_cast<ruleset_t>(ruleset);
	match.rule_set = rs_long_name[ruleset];
	match.map_name = level.mapname;
	match.team_mode = level.match.team_mode;
	match.ctf_mode = level.match.ctf_mode;
	match.duel_mode = gametype == GT_DUEL;
	match.start_msec = level.match.match_start_real_time_ms;
	match.end_msec = level.match.match_end_real_time_ms;
	match.duration_msec = match.end_msec > match.start_msec
		? match.end_msec - match.start_msec : 0;
	match.time_limit_seconds = MM_MatchStatsTimeLimitSeconds(
		timelimit ? timelimit->value : 0.0f);
	match.score_limit = GT_ScoreLimit();
	match.red_score = level.team_scores[TEAM_RED];
	match.blue_score = level.team_scores[TEAM_BLUE];
	match.overall = level.match;
	match.players = g_departed_players;
	match.players.reserve(match.players.size() + level.num_playing_clients);

	for (gentity_t *entity : active_players()) {
		FrozenPlayer player = FreezePlayer(entity);
		if (!player.stats.play_end_real_time_ms)
			player.stats.play_end_real_time_ms = match.end_msec;
		const auto existing = std::find_if(match.players.begin(),
			match.players.end(), [&](const FrozenPlayer &candidate) {
				return SameParticipant(candidate, entity);
			});
		if (existing == match.players.end())
			match.players.push_back(std::move(player));
		else
			*existing = std::move(player);
	}
	for (FrozenPlayer &player : match.players) {
		if (!player.outcome_settled)
			player.outcome = PlayerOutcome(match, player);
	}
	return match;
}

void SendMiniSummaries(const FrozenMatch &match)
{
	for (const FrozenPlayer &player : match.players) {
		gentity_t *entity = ResolveParticipantEntity(player);
		if (!entity || !entity->client)
			continue;
		const size_t player_index =
			static_cast<size_t>(&player - match.players.data());
		std::ostringstream message;
		message << ":: Match Summary ::\n" << player.player_name
			<< " - Result: " << OutcomeName(player.outcome)
			<< " | Kills: " << player.stats.total_kills
			<< " | Deaths: " << player.stats.total_deaths
			<< " | K/D Ratio: " << std::fixed << std::setprecision(2)
			<< Ratio(player.stats.total_kills, player.stats.total_deaths);
		if (!player.bot && player.metadata.has_skill_rating) {
			message << " | SR: " << player.metadata.skill_rating << " (";
			if (player.metadata.skill_rating_change >= 0)
				message << '+';
			message << player.metadata.skill_rating_change << ')';
		}
		message << '\n';

		// [MuffMode] Whatever this player took off the post-match awards reel.
		// The reel itself is gone once the map changes, so the summary is the
		// only lasting record a player sees without opening the export.
		bool wrote_award = false;
		for (const mm_award_result_t &award : match.awards) {
			if (award.player_index != player_index)
				continue;
			message << (wrote_award ? ", " : "Awards: ")
				<< MM_AwardTitle(award.award);
			wrote_award = true;
		}
		if (wrote_award)
			message << '\n';

		gi.LocClient_Print(entity, PRINT_HIGH, "{}", message.str().c_str());
	}
}

bool HasHumanPlayer(const FrozenMatch &match)
{
	return std::any_of(match.players.begin(), match.players.end(),
		[](const FrozenPlayer &player) { return !player.bot; });
}

void ValidateModTotals(const FrozenMatch &match)
{
	uint64_t kills = 0;
	uint64_t deaths = 0;
	for (size_t index = 0; index < MM_MATCH_MOD_COUNT; index++) {
		kills += match.overall.mod_kills[index];
		deaths += match.overall.mod_deaths[index];
	}
	if (kills != match.overall.total_kills)
		gi.Com_PrintFmt("MM_MatchStats: totalKillsByMOD mismatch ({} != {})\n",
			kills, match.overall.total_kills);
	if (deaths != match.overall.total_deaths)
		gi.Com_PrintFmt("MM_MatchStats: totalDeathsByMOD mismatch ({} != {})\n",
			deaths, match.overall.total_deaths);
}

void RecordMappedMedal(gclient_t *client, mm_match_medal_t medal,
	uint32_t count)
{
	if (!ValidClient(client) || medal <= mm_match_medal_t::none ||
		medal >= mm_match_medal_t::total || !count)
		return;
	const size_t index = static_cast<size_t>(medal);
	SaturatingAdd(client->pers.match.medal_count[index], count);
	SaturatingAdd(level.match.medal_count[index], count);
}

void RecordCarrierDuration(gclient_t *client, size_t flag_index,
	uint64_t duration_msec)
{
	if (!ValidClient(client) || flag_index >= level.match.ctf.size() ||
		!duration_msec)
		return;

	mm_match_player_stats_t &player = client->pers.match;
	mm_match_ctf_team_stats_t &flag = level.match.ctf[flag_index];
	SaturatingAdd(player.ctf_flag_carrier_time_total_msec, duration_msec);
	SaturatingAdd(flag.flag_hold_time_total_msec, duration_msec);
	const uint32_t clamped = static_cast<uint32_t>(std::min<uint64_t>(
		duration_msec, std::numeric_limits<uint32_t>::max()));
	if (!player.ctf_flag_carrier_time_shortest_msec ||
		clamped < player.ctf_flag_carrier_time_shortest_msec)
		player.ctf_flag_carrier_time_shortest_msec = clamped;
	player.ctf_flag_carrier_time_longest_msec = std::max(
		player.ctf_flag_carrier_time_longest_msec, clamped);
	if (!flag.flag_hold_time_shortest_msec ||
		clamped < flag.flag_hold_time_shortest_msec)
		flag.flag_hold_time_shortest_msec = clamped;
	flag.flag_hold_time_longest_msec = std::max(
		flag.flag_hold_time_longest_msec, clamped);
}

// --- Post-match awards ------------------------------------------------------
// The awards reel is decided here rather than in mm_awards because the frozen
// participant set -- including everyone who left mid-match -- only exists on this
// side of the anonymous namespace. mm_awards owns presentation; the catalog
// itself is pure arithmetic in mm_awards_rules.h.

// One position sample per player per second. Anything finer just multiplies the
// sample count without changing the share the camping award actually measures.
constexpr gtime_t kCampSampleInterval = 1_sec;
gtime_t g_next_camp_sample = 0_ms;

// Buckets a live origin into the player's dwell table. An idle sample is counted
// toward the total but never toward a cell, so a player the inactivity timer has
// flagged can only ever dilute their own camping share. The table's eviction
// policy lives with the table itself, in MM_MatchStatsRecordCampCell.
void RecordCampSample(mm_match_player_stats_t &stats, const vec3_t &origin,
	bool idle)
{
	SaturatingAdd(stats.camp_samples, 1u);
	if (idle) {
		SaturatingAdd(stats.camp_idle_samples, 1u);
		return;
	}

	const auto quantize = [](float value) {
		const float cell = std::floor(value / MM_MATCH_CAMP_CELL_SIZE);
		return static_cast<int16_t>(std::clamp(cell, -32768.0f, 32767.0f));
	};
	MM_MatchStatsRecordCampCell(stats.camp_cells.data(), stats.camp_cells.size(),
		quantize(origin[0]), quantize(origin[1]), quantize(origin[2]));
}

void SampleCampPositions()
{
	if (!MM_MatchStats_IsCollecting())
		return;
	if (level.match_state != match_state_t::MATCH_IN_PROGRESS)
		return;
	if (level.time < g_next_camp_sample)
		return;

	g_next_camp_sample = level.time + kCampSampleInterval;

	for (gentity_t *entity : active_players()) {
		if (!entity->client || !entity->client->pers.spawned)
			continue;
		// A dead, eliminated or frozen player is not choosing where to stand. A
		// Freeze Tag block in particular keeps health at 1 and stays on its team,
		// so it passes every other test here while being the clearest case of
		// somebody standing still against their will.
		if (entity->health <= 0 || entity->client->eliminated ||
			MM_FreezeTag_IsFrozen(entity))
			continue;

		RecordCampSample(entity->client->pers.match, entity->s.origin,
			entity->client->sess.inactive);
	}
}

// Quads already lying on the floor at the moment the match opens.
//
// This is the exact complement of MM_MatchStats_RecordItemAvailable, not a
// duplicate of it: that hook counts a Quad BECOMING available, this counts one
// that ALREADY is, and the two sets cannot overlap. On the normal reset the
// entity lump has only just been re-parsed and every item is still SOLID_NOT
// with its FinishSpawningItem think pending, so this finds nothing and the hook
// does all the work. On the legacy reset a non-teamed Quad resting on the floor
// is deliberately left untouched under rulesets other than RS_MM/RS_Q3A, so no
// think will ever fire for that appearance and this is the only thing that
// counts it. Missing it would not merely lose a spawn: the pickup of that same
// Quad is still recorded, so the control percentage the Quad awards are gated on
// would read high against a short denominator.
uint32_t CountAvailableQuads()
{
	// QuadHog keeps one Quad permanently in circulation rather than respawning
	// it on the floor, so there are no spawns to take a share of.
	if (g_quadhog && g_quadhog->integer)
		return 0;

	uint32_t available = 0;
	for (size_t i = 1; i < globals.num_entities; i++) {
		const gentity_t *entity = &g_entities[i];
		if (!entity->inuse || entity->client || !entity->item)
			continue;
		if (entity->item->id != IT_POWERUP_QUAD)
			continue;
		if (entity->solid != SOLID_TRIGGER || (entity->svflags & SVF_NOCLIENT))
			continue;
		if (entity->spawnflags.has(SPAWNFLAG_ITEM_DROPPED_PLAYER) &&
			!entity->spawnflags.has(SPAWNFLAG_ITEM_DROPPED))
			continue;
		available++;
	}

	return available;
}

// Folds one participant's raw counters into the flat snapshot the catalog reads.
// Kill counts arrive per means-of-death; the catalog thinks in weapon families,
// so the existing MOD-to-weapon bridge does the collapsing rather than a second
// hand-written MOD table that could drift away from it.
mm_award_player_facts_t AwardFacts(const FrozenPlayer &player,
	int64_t match_end_msec)
{
	const mm_match_player_stats_t &stats = player.stats;
	mm_award_player_facts_t facts;

	facts.kills = stats.total_kills;
	facts.deaths = stats.total_deaths;
	facts.suicides = stats.total_suicides;
	facts.team_kills = stats.total_team_kills;
	facts.spawn_kills = stats.total_spawn_kills;
	facts.environment_deaths = stats.total_environment_deaths;
	facts.shots = stats.total_shots;
	facts.hits = stats.total_hits;
	facts.damage_dealt = stats.total_dmg_dealt;
	facts.damage_received = stats.total_dmg_received;
	facts.quad_pickups =
		stats.pickup_counts[static_cast<size_t>(mm_match_high_value_item_t::quad_damage)];
	facts.quad_kills = stats.quad_kills;
	facts.excellent_medals =
		stats.medal_count[static_cast<size_t>(mm_match_medal_t::excellent)];
	facts.humiliation_medals =
		stats.medal_count[static_cast<size_t>(mm_match_medal_t::humiliation)];
	facts.rampage_medals =
		stats.medal_count[static_cast<size_t>(mm_match_medal_t::rampage)];
	facts.first_frag_medals =
		stats.medal_count[static_cast<size_t>(mm_match_medal_t::first_frag)];
	facts.ctf_captures = stats.ctf_flag_captures;
	facts.ctf_defences = stats.ctf_flag_defences;
	facts.ctf_returns = stats.ctf_flag_returns;
	facts.ctf_carrier_time_msec = stats.ctf_flag_carrier_time_total_msec;
	facts.completed_lives = stats.completed_lives;
	facts.life_average_msec = stats.life_average_msec;
	facts.camp_samples = stats.camp_samples;
	facts.camp_idle_samples = stats.camp_idle_samples;
	facts.play_time_msec = PlayerPlayTime(player, match_end_msec);
	facts.score = player.score;
	facts.bot = player.bot;

	for (size_t index = 1; index < MM_MATCH_HIGH_VALUE_ITEM_COUNT; index++)
		SaturatingAdd(facts.high_value_pickups, stats.pickup_counts[index]);

	for (size_t index = 0; index < MM_MATCH_MOD_COUNT; index++) {
		const uint32_t kills = stats.mod_total_kills[index];
		if (!kills)
			continue;

		switch (MM_MatchStats_WeaponForMod(
			mod_t(static_cast<mod_id_t>(index)))) {
		case mm_match_weapon_t::shotgun:
		case mm_match_weapon_t::super_shotgun:
			SaturatingAdd(facts.shotgun_kills, kills);
			break;
		case mm_match_weapon_t::railgun:
			SaturatingAdd(facts.rail_kills, kills);
			break;
		case mm_match_weapon_t::rocket_launcher:
			SaturatingAdd(facts.rocket_kills, kills);
			break;
		case mm_match_weapon_t::machinegun:
		case mm_match_weapon_t::chaingun:
			SaturatingAdd(facts.bullet_kills, kills);
			break;
		case mm_match_weapon_t::hand_grenades:
			SaturatingAdd(facts.grenade_kills, kills);
			break;
		case mm_match_weapon_t::bfg10k:
			SaturatingAdd(facts.bfg_kills, kills);
			break;
		default:
			break;
		}
	}

	for (const mm_match_camp_cell_t &cell : stats.camp_cells)
		facts.camp_best_cell_samples =
			std::max(facts.camp_best_cell_samples, cell.samples);

	return facts;
}

// A match earns awards when it was a real ranked contest. Bots never carry a
// profile and a padded match is not a ranking, so one bot participant disarms
// the reel entirely -- the same bar MM_PlayerStatsMatchCanBeRanked applies to
// rating settlement, evaluated here against the frozen participant set so it
// does not depend on which module ran first. Arena Rooms and Horde are
// excluded from rating settlement for the same reason (see
// MM_PlayerStats_OnMatchStart), so awards stay off for both too.
bool AwardsMatchIsRanked(const FrozenMatch &match)
{
	if (!CvarEnabled(g_ranked, true) || GT(GT_ARENA) || GT(GT_HORDE))
		return false;

	size_t humans = 0;
	for (const FrozenPlayer &player : match.players) {
		if (player.bot)
			return false;
		humans++;
	}

	return humans >= MM_AWARD_MIN_PARTICIPANTS;
}

std::vector<mm_award_result_t> DecideAwards(const FrozenMatch &match)
{
	std::vector<mm_award_result_t> decided;
	if (match.players.empty())
		return decided;

	std::vector<mm_award_player_facts_t> facts;
	facts.reserve(match.players.size());
	for (const FrozenPlayer &player : match.players) {
		// A bot keeps its slot so award indices still line up with the
		// participant list, but contributes no counters: every catalog entry has
		// a positive floor, so an all-zero entrant cannot win anything. The
		// ranked gate already excludes bot matches outright; this is what makes
		// that a belt-and-braces guarantee rather than the only guard.
		if (player.bot) {
			mm_award_player_facts_t bot_facts;
			bot_facts.bot = true;
			facts.push_back(bot_facts);
			continue;
		}
		facts.push_back(AwardFacts(player, match.end_msec));
	}

	mm_award_match_facts_t match_facts;
	match_facts.quad_spawns = match.overall.quad_spawns;
	match_facts.total_kills = match.overall.total_kills;
	match_facts.duration_msec = match.duration_msec > 0
		? static_cast<uint64_t>(match.duration_msec) : 0;
	match_facts.participants = facts.size();
	match_facts.team_mode = match.team_mode;
	match_facts.ctf_mode = match.ctf_mode;

	std::array<mm_award_result_t, MM_AWARDS_DISPLAY_LIMIT> reel {};
	const size_t count = MM_AwardsSelect(facts.data(), facts.size(), match_facts,
		reel.data(), reel.size());

	decided.assign(reel.begin(),
		reel.begin() + static_cast<ptrdiff_t>(count));
	return decided;
}

// Resolves the decided reel against the participant list and hands it to
// mm_awards, which owns everything the players actually see.
void PublishAwards(const FrozenMatch &match,
	const std::vector<mm_award_result_t> &decided)
{
	std::vector<mm_award_entry_t> entries;
	entries.reserve(decided.size());
	for (const mm_award_result_t &result : decided) {
		if (result.player_index >= match.players.size())
			continue;
		const FrozenPlayer &winner = match.players[result.player_index];
		mm_award_entry_t entry;
		entry.award = result.award;
		entry.player_name = winner.player_name;
		entry.social_id = winner.social_id;
		entry.value = result.value;
		entries.push_back(std::move(entry));
	}

	MM_Awards_Publish(std::move(entries), match.awards_ranked);
}

// Idempotent: the result is frozen once by QueueIntermission and consumed later
// by Match_End, but a direct target_changelevel exit can reach the freeze and the
// end in the same call, and the fallback path in MM_MatchStats_End may re-freeze.
void FinalizeAwards(FrozenMatch &match)
{
	if (match.awards_decided)
		return;

	match.awards_decided = true;
	match.awards_ranked = AwardsMatchIsRanked(match);
	// Awards are a ranked-match feature all the way down, not merely a reel that
	// is hidden. An unranked match decides nothing, so it exports an empty list,
	// says nothing in the end-of-match summaries, and writes no career tallies.
	if (match.awards_ranked)
		match.awards = DecideAwards(match);
	// Published either way: an empty publish is what retires the previous
	// match's reel, so an unranked match cannot leave the last ranked one armed.
	PublishAwards(match, match.awards);
}

} // namespace

void MM_MatchStats_RegisterCvars()
{
	g_statex_enabled = gi.cvar("g_statex_enabled", "1", CVAR_NOFLAGS);
	g_statex_humans_present = gi.cvar("g_statex_humans_present", "1", CVAR_NOFLAGS);
	g_statex_export_html = gi.cvar("g_statex_export_html", "1", CVAR_NOFLAGS);
}

bool MM_MatchStats_IsCollecting()
{
	// g_ranked 0 turns the whole statistics system off, so no Record* call does any work.
	return CvarEnabled(g_ranked, true) &&
		deathmatch && deathmatch->integer && level.match.collecting &&
		!level.match.finalized;
}

void MM_MatchStats_Reset()
{
	level.match = {};
	g_departed_players.clear();
	g_frozen_result.reset();
	g_next_camp_sample = 0_ms;
	for (gentity_t *entity : active_clients())
		if (entity && entity->client)
			entity->client->pers.match = {};
}

void MM_MatchStats_ClientBegin(gentity_t *player)
{
	if (!player || !ValidClient(player->client) ||
		!ClientIsPlaying(player->client))
		return;
	if (player->client->pers.match.play_start_real_time_ms &&
		!player->client->pers.match.play_end_real_time_ms)
		return;

	const auto archived = std::find_if(g_departed_players.begin(),
		g_departed_players.end(), [&](const FrozenPlayer &candidate) {
			return SameParticipant(candidate, player);
		});
	if (archived != g_departed_players.end()) {
		player->client->pers.match = archived->stats;
		g_departed_players.erase(archived);
	}

	mm_match_player_stats_t &stats = player->client->pers.match;
	stats.play_start_real_time_ms = NowUnixMsec();
	stats.play_end_real_time_ms = 0;
	if (!stats.life_started_real_time_ms)
		stats.life_started_real_time_ms = NowUnixMsec();
}

void MM_MatchStats_ClientEnd(gentity_t *player)
{
	if (!player || !player->client || !level.match.collecting ||
		level.match.finalized)
		return;
	mm_match_player_stats_t &stats = player->client->pers.match;
	const auto archived = std::find_if(g_departed_players.begin(),
		g_departed_players.end(), [&](const FrozenPlayer &candidate) {
			return SameParticipant(candidate, player);
		});
	if ((!stats.play_start_real_time_ms || stats.play_end_real_time_ms) &&
		archived != g_departed_players.end()) {
		// Late departure work (notably dropping a carried CTF flag) can mutate
		// the stopped player's counters. Refresh without charging more play/life
		// time, but do not let a fresh spectator connection with the same social
		// ID replace the real participant snapshot with empty counters.
		if (MM_MatchStatsCanRefreshArchivedParticipant(
			stats.play_start_real_time_ms, stats.play_time_total_msec,
			stats.play_end_real_time_ms))
			ArchivePlayer(player);
		return;
	}
	if (archived == g_departed_players.end() &&
		!MM_MatchStatsHasRecordedParticipation(
			stats.play_start_real_time_ms, stats.play_time_total_msec)) {
		// Disconnect and team-transition paths also run for spectators. Do not
		// turn a client who never entered play into an exported participant.
		return;
	}

	const int64_t now = level.match.match_end_real_time_ms > 0
		? level.match.match_end_real_time_ms
		: NowUnixMsec();
	if (stats.play_start_real_time_ms && !stats.play_end_real_time_ms &&
		now > stats.play_start_real_time_ms) {
		SaturatingAdd(stats.play_time_total_msec,
			static_cast<uint64_t>(now - stats.play_start_real_time_ms));
	}
	stats.play_start_real_time_ms = 0;
	stats.play_end_real_time_ms = now;
	if (stats.life_started_real_time_ms > 0 &&
		now > stats.life_started_real_time_ms) {
		const uint64_t life = static_cast<uint64_t>(
			now - stats.life_started_real_time_ms);
		SaturatingAdd(stats.life_total_msec, life);
		SaturatingAdd(stats.completed_lives, 1);
		stats.life_average_msec = static_cast<uint32_t>(std::min<uint64_t>(
			stats.life_total_msec / stats.completed_lives,
			std::numeric_limits<uint32_t>::max()));
		stats.life_longest_msec = std::max(stats.life_longest_msec,
			static_cast<uint32_t>(std::min<uint64_t>(life,
				std::numeric_limits<uint32_t>::max())));
	}
	stats.life_started_real_time_ms = 0;
	ArchivePlayer(player);
}

void MM_MatchStats_RefreshArchivedPlayerMetadata(
	gentity_t *player, const gclient_t *authoritative_profile_state)
{
	if (!player || !player->client || !level.match.collecting ||
		level.match.finalized)
		return;
	const auto archived = std::find_if(g_departed_players.begin(),
		g_departed_players.end(), [&](const FrozenPlayer &candidate) {
			return SameParticipant(candidate, player);
		});
	const FrozenPlayer current = FreezePlayer(
		player, authoritative_profile_state);
	auto refresh = [&](FrozenPlayer &snapshot) {
		snapshot.metadata = current.metadata;
		if (current.outcome_settled) {
			snapshot.outcome = current.outcome;
			snapshot.outcome_settled = true;
		}
	};
	if (archived != g_departed_players.end())
		refresh(*archived);
	if (g_frozen_result) {
		const auto frozen = std::find_if(g_frozen_result->players.begin(),
			g_frozen_result->players.end(), [&](const FrozenPlayer &candidate) {
				return SameParticipant(candidate, player);
			});
		if (frozen != g_frozen_result->players.end())
			refresh(*frozen);
	}
}

void MM_MatchStats_RecordSpawn(gclient_t *client)
{
	if (!ValidClient(client) || !ClientIsPlaying(client))
		return;
	client->pers.match.life_started_real_time_ms = NowUnixMsec();
}

void MM_MatchStats_Init()
{
	// Arena Rooms run independent simultaneous series; this exporter owns one
	// singleton match context and must stay out of their room-local lifecycle.
	if (!deathmatch || !deathmatch->integer || GT(GT_ARENA))
		return;

	// Host opted out of ranking and statistics; never open a match context to collect into.
	if (!CvarEnabled(g_ranked, true))
		return;

	MM_MatchStats_Reset();
	level.match.collecting = true;
	level.match.finalized = false;
	level.match.team_mode = Teams();
	level.match.ctf_mode = GTF(GTF_CTF);
	level.match.match_start_real_time_ms = NowUnixMsec();
	// Seeds the Quad spawn total with whatever is already on the floor; every
	// later appearance arrives through MM_MatchStats_RecordItemAvailable.
	level.match.quad_spawns = CountAvailableQuads();
	level.match.event_log.reserve(MM_MATCH_EVENT_LIMIT);
	level.match.death_log.reserve(MM_MATCH_DEATH_EVENT_LIMIT);
	for (gentity_t *entity : active_players())
		MM_MatchStats_ClientBegin(entity);
	MM_MatchStats_LogEvent("MATCH START");
	gi.Com_PrintFmt("MM_MatchStats: started match {}\n",
		level.match_id.empty() ? "(pending ID)" : level.match_id.c_str());
}

void MM_MatchStats_RunFrame()
{
	ExportWorker().DrainNotices();
	SampleCampPositions();
}

void MM_MatchStats_FreezeResultTime()
{
	if (!MM_MatchStats_IsCollecting() || level.match.match_end_real_time_ms > 0)
		return;
	MM_MatchStats_LogEvent("MATCH END");
	level.match.match_end_real_time_ms = NowUnixMsec();
	// Materialize the complete result now. QueueIntermission deliberately leaves
	// a short handoff before Match_End; joins, team changes, and disconnects in
	// that window must not alter the already-settled participant set or scores.
	for (gentity_t *entity : active_players())
		MM_MatchStats_ClientEnd(entity);
	g_frozen_result = FreezeCurrentMatch();
	// The complete participant set exists exactly here, which is also early
	// enough for the reel to be published before intermission begins.
	FinalizeAwards(*g_frozen_result);
}

void MM_MatchStats_End()
{
	if (!deathmatch || !deathmatch->integer || !level.match.collecting ||
		level.match.finalized)
		return;

	MM_MatchStats_FreezeResultTime();
	level.match.collecting = false;
	level.match.finalized = true;

	FrozenMatch match = g_frozen_result
		? std::move(*g_frozen_result)
		: FreezeCurrentMatch();
	g_frozen_result.reset();
	FinalizeAwards(match);
	ValidateModTotals(match);
	try {
		SendMiniSummaries(match);
	} catch (const std::exception &exception) {
		gi.Com_PrintFmt("MM_MatchStats: could not prepare match summary: {}\n",
			exception.what());
	} catch (...) {
		gi.Com_Print("MM_MatchStats: could not prepare match summary: unknown exception.\n");
	}

	if (!CvarEnabled(g_statex_enabled, true)) {
		gi.Com_Print("MM_MatchStats: export disabled by g_statex_enabled.\n");
		return;
	}
	if (CvarEnabled(g_statex_humans_present, true) && !HasHumanPlayer(match)) {
		gi.Com_Print("MM_MatchStats: export skipped because no human player participated.\n");
		return;
	}

	try {
		static std::atomic<uint64_t> next_job_id{1};
		const uint64_t job_id = next_job_id.fetch_add(1);
		std::unique_ptr<ExportJob> job = BuildExportJob(std::move(match), job_id,
			CvarEnabled(g_statex_export_html, true));
		if (!ExportWorker().Enqueue(job)) {
			gi.Com_Print("MM_MatchStats: export worker unavailable or queue full; writing synchronously.\n");
			ExportWorker().ExecuteFallback(std::move(job));
			ExportWorker().DrainNotices();
			return;
		}
		gi.Com_PrintFmt("MM_MatchStats: queued export {} (pending {}, completed {}, failed {})\n",
			job_id, ExportWorker().Pending(), ExportWorker().Completed(),
			ExportWorker().Failed());
	} catch (const std::exception &exception) {
		gi.Com_PrintFmt("MM_MatchStats: could not prepare match export: {}\n",
			exception.what());
	} catch (...) {
		gi.Com_Print("MM_MatchStats: could not prepare match export: unknown exception.\n");
	}
}

void MM_MatchStats_Shutdown()
{
	ExportWorker().Shutdown();
	ExportWorker().DrainNotices();
	gi.Com_PrintFmt("MM_MatchStats: export worker drained (submitted {}, completed {}, failed {})\n",
		ExportWorker().Submitted(), ExportWorker().Completed(),
		ExportWorker().Failed());
}

mm_match_weapon_t MM_MatchStats_WeaponForItem(item_id_t item)
{
	switch (item) {
	case IT_WEAPON_GRAPPLE: return mm_match_weapon_t::grappling_hook;
	case IT_WEAPON_BLASTER: return mm_match_weapon_t::blaster;
	case IT_WEAPON_CHAINFIST: return mm_match_weapon_t::chainfist;
	case IT_WEAPON_SHOTGUN: return mm_match_weapon_t::shotgun;
	case IT_WEAPON_SSHOTGUN: return mm_match_weapon_t::super_shotgun;
	case IT_WEAPON_MACHINEGUN: return mm_match_weapon_t::machinegun;
	case IT_WEAPON_ETF_RIFLE: return mm_match_weapon_t::etf_rifle;
	case IT_WEAPON_CHAINGUN: return mm_match_weapon_t::chaingun;
	case IT_AMMO_GRENADES: return mm_match_weapon_t::hand_grenades;
	case IT_AMMO_TRAP: return mm_match_weapon_t::trap;
	case IT_AMMO_TESLA: return mm_match_weapon_t::tesla_mine;
	case IT_WEAPON_GLAUNCHER: return mm_match_weapon_t::grenade_launcher;
	case IT_WEAPON_PROXLAUNCHER: return mm_match_weapon_t::prox_launcher;
	case IT_WEAPON_RLAUNCHER: return mm_match_weapon_t::rocket_launcher;
	case IT_WEAPON_HYPERBLASTER:
		return RS(RS_Q3A) ? mm_match_weapon_t::plasma_gun
			: mm_match_weapon_t::hyperblaster;
	case IT_WEAPON_IONRIPPER: return mm_match_weapon_t::ion_ripper;
	case IT_WEAPON_PLASMABEAM:
		return RS(RS_Q1) ? mm_match_weapon_t::thunderbolt
			: mm_match_weapon_t::plasma_beam;
	case IT_WEAPON_RAILGUN: return mm_match_weapon_t::railgun;
	case IT_WEAPON_PHALANX: return mm_match_weapon_t::phalanx;
	case IT_WEAPON_BFG: return mm_match_weapon_t::bfg10k;
	case IT_WEAPON_DISRUPTOR: return mm_match_weapon_t::disruptor;
	default: return mm_match_weapon_t::none;
	}
}

mm_match_weapon_t MM_MatchStats_WeaponForMod(const mod_t &mod)
{
	switch (mod.id) {
	case MOD_BLASTER: return mm_match_weapon_t::blaster;
	case MOD_SHOTGUN: return mm_match_weapon_t::shotgun;
	case MOD_SSHOTGUN: return mm_match_weapon_t::super_shotgun;
	case MOD_MACHINEGUN: return mm_match_weapon_t::machinegun;
	case MOD_CHAINGUN: return mm_match_weapon_t::chaingun;
	case MOD_GRENADE:
	case MOD_G_SPLASH: return mm_match_weapon_t::grenade_launcher;
	case MOD_ROCKET:
	case MOD_R_SPLASH: return mm_match_weapon_t::rocket_launcher;
	case MOD_HYPERBLASTER:
		return RS(RS_Q3A) ? mm_match_weapon_t::plasma_gun
			: mm_match_weapon_t::hyperblaster;
	case MOD_RAILGUN:
	case MOD_RAILGUN_SPLASH: return mm_match_weapon_t::railgun;
	case MOD_BFG_LASER:
	case MOD_BFG_BLAST:
	case MOD_BFG_EFFECT: return mm_match_weapon_t::bfg10k;
	case MOD_HANDGRENADE:
	case MOD_HG_SPLASH: return mm_match_weapon_t::hand_grenades;
	case MOD_HELD_GRENADE: return mm_match_weapon_t::none;
	case MOD_RIPPER: return mm_match_weapon_t::ion_ripper;
	case MOD_PHALANX: return mm_match_weapon_t::phalanx;
	case MOD_TRAP: return mm_match_weapon_t::trap;
	case MOD_CHAINFIST: return mm_match_weapon_t::chainfist;
	case MOD_DISINTEGRATOR: return mm_match_weapon_t::disruptor;
	case MOD_ETF_RIFLE: return mm_match_weapon_t::etf_rifle;
	case MOD_PLASMABEAM:
		return RS(RS_Q1) ? mm_match_weapon_t::thunderbolt
			: mm_match_weapon_t::plasma_beam;
	case MOD_TESLA: return mm_match_weapon_t::tesla_mine;
	case MOD_PROX: return mm_match_weapon_t::prox_launcher;
	case MOD_GRAPPLE: return mm_match_weapon_t::grappling_hook;
	default: return mm_match_weapon_t::none;
	}
}

mm_match_high_value_item_t MM_MatchStats_HighValueForItem(item_id_t item)
{
	switch (item) {
	case IT_HEALTH_MEGA: return mm_match_high_value_item_t::mega_health;
	case IT_ARMOR_BODY: return mm_match_high_value_item_t::body_armor;
	case IT_ARMOR_COMBAT: return mm_match_high_value_item_t::combat_armor;
	case IT_POWER_SHIELD: return mm_match_high_value_item_t::power_shield;
	case IT_POWER_SCREEN: return mm_match_high_value_item_t::power_screen;
	case IT_ADRENALINE: return mm_match_high_value_item_t::adrenaline;
	case IT_POWERUP_QUAD: return mm_match_high_value_item_t::quad_damage;
	case IT_POWERUP_DOUBLE: return mm_match_high_value_item_t::double_damage;
	case IT_POWERUP_INVISIBILITY: return mm_match_high_value_item_t::invisibility;
	case IT_POWERUP_HASTE: return mm_match_high_value_item_t::haste;
	case IT_POWERUP_REGEN: return mm_match_high_value_item_t::regeneration;
	case IT_POWERUP_PROTECTION: return mm_match_high_value_item_t::battle_suit;
	case IT_PACK: return mm_match_high_value_item_t::ammo_pack;
	case IT_BANDOLIER: return mm_match_high_value_item_t::bandolier;
	default: return mm_match_high_value_item_t::none;
	}
}

mm_match_medal_t MM_MatchStats_MedalForNative(medal_t medal)
{
	switch (medal) {
	case MEDAL_EXCELLENT: return mm_match_medal_t::excellent;
	case MEDAL_HUMILIATION: return mm_match_medal_t::humiliation;
	case MEDAL_IMPRESSIVE: return mm_match_medal_t::impressive;
	case MEDAL_RAMPAGE: return mm_match_medal_t::rampage;
	case MEDAL_DEFENCE: return mm_match_medal_t::defence;
	case MEDAL_ASSIST: return mm_match_medal_t::assist;
	case MEDAL_CAPTURE: return mm_match_medal_t::capture;
	default: return mm_match_medal_t::none;
	}
}

const char *MM_MatchStats_WeaponAbbreviation(mm_match_weapon_t weapon)
{
	const size_t index = static_cast<size_t>(weapon);
	return index < k_weapon_abbreviations.size()
		? k_weapon_abbreviations[index] : k_weapon_abbreviations[0];
}

const char *MM_MatchStats_MedalName(mm_match_medal_t medal)
{
	const size_t index = static_cast<size_t>(medal);
	return index < k_medal_names.size() ? k_medal_names[index] : k_medal_names[0];
}

const char *MM_MatchStats_HighValueItemName(mm_match_high_value_item_t item)
{
	const size_t index = static_cast<size_t>(item);
	return index < k_high_value_item_names.size()
		? k_high_value_item_names[index] : k_high_value_item_names[0];
}

namespace {

const char *ModNameForRuleset(uint8_t mod, ruleset_t ruleset)
{
	switch (static_cast<mod_id_t>(mod)) {
	case MOD_UNKNOWN: return "Unknown";
	case MOD_BLASTER: return "Blaster";
	case MOD_SHOTGUN: return "Shotgun";
	case MOD_SSHOTGUN: return "Super Shotgun";
	case MOD_MACHINEGUN: return "Machinegun";
	case MOD_CHAINGUN: return "Chaingun";
	case MOD_GRENADE: return "Grenade Impact";
	case MOD_G_SPLASH: return "Grenade Splash";
	case MOD_ROCKET: return "Rocket Impact";
	case MOD_R_SPLASH: return "Rocket Splash";
	case MOD_HYPERBLASTER:
		return ruleset == RS_Q3A ? "Plasma Gun" : "HyperBlaster";
	case MOD_RAILGUN: return "Railgun";
	case MOD_BFG_LASER: return "BFG Laser";
	case MOD_BFG_BLAST: return "BFG Blast";
	case MOD_BFG_EFFECT: return "BFG Core";
	case MOD_HANDGRENADE: return "Hand Grenade Impact";
	case MOD_HG_SPLASH: return "Hand Grenade Splash";
	case MOD_WATER: return "Drowning";
	case MOD_SLIME: return "Slime";
	case MOD_LAVA: return "Lava";
	case MOD_CRUSH: return "Crushed";
	case MOD_TELEFRAG: return "Telefrag";
	case MOD_TELEFRAG_SPAWN: return "Telefrag (Spawn)";
	case MOD_FALLING: return "Falling";
	case MOD_SUICIDE: return "Suicide";
	case MOD_HELD_GRENADE: return "Held Grenade Explosion";
	case MOD_EXPLOSIVE: return "Explosion";
	case MOD_BARREL: return "Barrel";
	case MOD_BOMB: return "Bomb";
	case MOD_EXIT: return "Exit";
	case MOD_SPLASH: return "Splash Damage";
	case MOD_TARGET_LASER: return "Target Laser";
	case MOD_TRIGGER_HURT: return "Trigger Hurt";
	case MOD_HIT: return "Hit";
	case MOD_TARGET_BLASTER: return "Target Blaster";
	case MOD_RIPPER: return "Ion Ripper";
	case MOD_PHALANX: return "Phalanx";
	case MOD_BRAINTENTACLE: return "Brain Tentacle";
	case MOD_BLASTOFF: return "Blast Off";
	case MOD_GEKK: return "Gekk";
	case MOD_TRAP: return "Trap";
	case MOD_CHAINFIST: return "Chainfist";
	case MOD_DISINTEGRATOR: return "Disruptor";
	case MOD_ETF_RIFLE: return "ETF Rifle";
	case MOD_BLASTER2: return "Blaster 2";
	case MOD_PLASMABEAM:
		return ruleset == RS_Q1 ? "Thunderbolt" : "Plasma Beam";
	case MOD_TESLA: return "Tesla";
	case MOD_PROX: return "Proximity Mine";
	case MOD_NUKE: return "Nuke";
	case MOD_VENGEANCE_SPHERE: return "Vengeance Sphere";
	case MOD_HUNTER_SPHERE: return "Hunter Sphere";
	case MOD_DEFENDER_SPHERE: return "Defender Sphere";
	case MOD_TRACKER: return "Tracker";
	case MOD_DOPPEL_EXPLODE: return "Doppelganger Explosion";
	case MOD_DOPPEL_VENGEANCE: return "Doppelganger Vengeance";
	case MOD_DOPPEL_HUNTER: return "Doppelganger Hunter";
	case MOD_GRAPPLE: return "Grapple";
	case MOD_BLUEBLASTER: return "Blue Blaster";
	case MOD_RAILGUN_SPLASH: return "Railgun Splash";
	case MOD_EXPIRE: return "Expire";
	case MOD_CHANGE_TEAM: return "Change Team";
	default: {
		thread_local char name[16];
		std::snprintf(name, sizeof(name), "MOD_%u", static_cast<unsigned>(mod));
		return name;
	}
	}
}

} // namespace

const char *MM_MatchStats_ModName(uint8_t mod)
{
	return ModNameForRuleset(mod, game.ruleset);
}

void MM_MatchStats_RecordShot(gclient_t *client, mm_match_weapon_t weapon,
	uint32_t count)
{
	if (!ValidClient(client) || !count)
		return;
	SaturatingAdd(client->pers.match.total_shots, count);
	const size_t index = static_cast<size_t>(weapon);
	if (index < MM_MATCH_WEAPON_COUNT)
		SaturatingAdd(client->pers.match.total_shots_per_weapon[index], count);
}

void MM_MatchStats_RecordHit(gclient_t *client, const mod_t &mod,
	uint32_t count)
{
	if (!ValidClient(client) || !count)
		return;
	SaturatingAdd(client->pers.match.total_hits, count);
	const size_t weapon = static_cast<size_t>(MM_MatchStats_WeaponForMod(mod));
	if (weapon < MM_MATCH_WEAPON_COUNT)
		SaturatingAdd(client->pers.match.total_hits_per_weapon[weapon], count);
}

void MM_MatchStats_RecordDamage(gclient_t *attacker, gclient_t *victim,
	const mod_t &mod, uint32_t amount)
{
	if (!MM_MatchStatsAcceptsResultEvents(
			level.match.collecting, level.match.finalized,
			level.match.match_end_real_time_ms) || !amount ||
		attacker == victim)
		return;
	const size_t mod_index = ModIndex(mod);
	if (attacker) {
		SaturatingAdd(attacker->pers.match.total_dmg_dealt, amount);
		SaturatingAdd(attacker->pers.match.mod_total_dmg_dealt[mod_index], amount);
	}
	if (victim) {
		SaturatingAdd(victim->pers.match.total_dmg_received, amount);
		SaturatingAdd(victim->pers.match.mod_total_dmg_received[mod_index], amount);
	}
}

void MM_MatchStats_RecordDeath(gentity_t *victim, gentity_t *attacker,
	const mod_t &mod, bool spawn_death, bool team_kill)
{
	if (!victim || !ValidClient(victim->client))
		return;

	const uint8_t mod_index = ModIndex(mod);
	const bool suicide = attacker == victim;
	const bool player_attacker = attacker && attacker->client;
	const bool friendly = team_kill || mod.friendly_fire;
	const bool valid_kill = player_attacker && !suicide && !friendly;
	mm_match_player_stats_t &victim_stats = victim->client->pers.match;

	if (valid_kill) {
		if (!level.match.total_kills)
			RecordMappedMedal(attacker->client, mm_match_medal_t::first_frag, 1);
		SaturatingAdd(attacker->client->pers.match.total_kills, 1);
		SaturatingAdd(attacker->client->pers.match.mod_total_kills[mod_index], 1);
		SaturatingAdd(level.match.total_kills, 1);
		SaturatingAdd(level.match.mod_kills[mod_index], 1);
		// [MuffMode] Whether the Quad was still running decides which of the two
		// Quad awards its owner walks away with. Sampled here rather than passed
		// in, because the kill and the damage that caused it share a frame.
		if (attacker->client->pu_time_quad > level.time)
			SaturatingAdd(attacker->client->pers.match.quad_kills, 1);
		if (spawn_death) {
			SaturatingAdd(attacker->client->pers.match.total_spawn_kills, 1);
			SaturatingAdd(level.match.total_spawn_kills, 1);
		}
	} else if (player_attacker && !suicide && friendly) {
		SaturatingAdd(attacker->client->pers.match.total_team_kills, 1);
		SaturatingAdd(level.match.total_team_kills, 1);
	}

	SaturatingAdd(victim_stats.total_deaths, 1);
	SaturatingAdd(victim_stats.mod_total_deaths[mod_index], 1);
	SaturatingAdd(level.match.total_deaths, 1);
	SaturatingAdd(level.match.mod_deaths[mod_index], 1);
	if (suicide) {
		SaturatingAdd(victim_stats.total_suicides, 1);
		SaturatingAdd(level.match.total_suicides, 1);
	} else if (!player_attacker) {
		SaturatingAdd(victim_stats.total_environment_deaths, 1);
	}
	if (spawn_death && !suicide)
		SaturatingAdd(victim_stats.total_spawn_deaths, 1);

	const int64_t now = NowUnixMsec();
	if (victim_stats.life_started_real_time_ms > 0 &&
		now > victim_stats.life_started_real_time_ms) {
		const uint64_t life = static_cast<uint64_t>(
			now - victim_stats.life_started_real_time_ms);
		SaturatingAdd(victim_stats.life_total_msec, life);
		SaturatingAdd(victim_stats.completed_lives, 1);
		victim_stats.life_average_msec = static_cast<uint32_t>(std::min<uint64_t>(
			victim_stats.life_total_msec / victim_stats.completed_lives,
			std::numeric_limits<uint32_t>::max()));
		victim_stats.life_longest_msec = std::max(victim_stats.life_longest_msec,
			static_cast<uint32_t>(std::min<uint64_t>(life,
				std::numeric_limits<uint32_t>::max())));
	}
	victim_stats.life_started_real_time_ms = 0;

	mm_match_death_event_t event;
	event.time_msec = MatchElapsedMsec();
	event.victim.name = PlayerName(victim->client);
	event.victim.id = PlayerSocialId(victim->client);
	if (player_attacker) {
		event.attacker.name = PlayerName(attacker->client);
		event.attacker.id = PlayerSocialId(attacker->client);
	} else {
		event.attacker.name = "Environment";
		event.attacker.id = "0";
	}
	event.mod = mod_index;
	if (MM_MatchStatsLogHasCapacity(
		level.match.death_log.size(), MM_MATCH_DEATH_EVENT_LIMIT)) {
		level.match.death_log.push_back(std::move(event));
	} else if (!level.match.death_log_truncated) {
		level.match.death_log_truncated = true;
		gi.Com_PrintFmt("MM_MatchStats: death log truncated after {} entries\n",
			MM_MATCH_DEATH_EVENT_LIMIT);
	}
}

void MM_MatchStats_RecordMedal(gclient_t *client, medal_t medal, uint32_t count)
{
	RecordMappedMedal(client, MM_MatchStats_MedalForNative(medal), count);
}

void MM_MatchStats_RecordItemAvailable(const gentity_t *ent)
{
	// Quad control is measured as a share of the times the Quad was there to be
	// taken, so this is called from both places that make one takeable: the
	// initial FinishSpawningItem and every later RespawnItem. Counting the
	// transition rather than scanning the world at match start is deliberate --
	// at that moment the entity lump has only just been re-parsed and no item
	// think has run yet, so every item is still SOLID_NOT and a scan sees none
	// of them.
	if (!ent || !ent->item || ent->item->id != IT_POWERUP_QUAD)
		return;
	if (!MM_MatchStats_IsCollecting())
		return;

	// Only an item that is actually on the floor and visible counts. Team slaves
	// and deferred powerup spawns leave FinishSpawningItem hidden and come back
	// through RespawnItem; a trigger-spawned item comes back through
	// Item_TriggeredSpawn, which clears SVF_NOCLIENT and re-enters
	// FinishSpawningItem. Either way each appearance passes here exactly once.
	if (ent->solid != SOLID_TRIGGER || (ent->svflags & SVF_NOCLIENT))
		return;

	// Mirrors the exclusions at the MM_MatchStats_RecordPickup call site, so the
	// spawn count and the pickup count it is divided into describe the same set
	// of Quads. Dropped items never reach either caller, but keeping the test
	// identical stops the two from drifting apart.
	if (ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED_PLAYER) &&
		!ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED))
		return;
	if (g_quadhog && g_quadhog->integer)
		return;

	SaturatingAdd(level.match.quad_spawns, 1u);
}

void MM_MatchStats_RecordPickup(gclient_t *client, item_id_t item,
	uint64_t pickup_delay_msec)
{
	if (!ValidClient(client))
		return;
	const mm_match_high_value_item_t mapped =
		MM_MatchStats_HighValueForItem(item);
	if (mapped <= mm_match_high_value_item_t::none ||
		mapped >= mm_match_high_value_item_t::total)
		return;
	const size_t index = static_cast<size_t>(mapped);
	SaturatingAdd(client->pers.match.pickup_counts[index], 1);
	SaturatingAdd(client->pers.match.pickup_delay_total_msec[index],
		pickup_delay_msec);
	SaturatingAdd(level.match.pickup_counts[index], 1);
	SaturatingAdd(level.match.pickup_delay_total_msec[index], pickup_delay_msec);
}

void MM_MatchStats_RecordCtfFlagPickup(gclient_t *client, team_t flag_team)
{
	const auto index = CtfTeamIndex(flag_team);
	if (!ValidClient(client) || !index)
		return;
	SaturatingAdd(client->pers.match.ctf_flag_pickups, 1);
	SaturatingAdd(level.match.ctf[*index].flag_pickups, 1);
}

void MM_MatchStats_RecordCtfCarrierTime(gclient_t *client, team_t flag_team,
	uint64_t carrier_time_msec)
{
	const auto index = CtfTeamIndex(flag_team);
	if (index)
		RecordCarrierDuration(client, *index, carrier_time_msec);
}

void MM_MatchStats_RecordCtfFlagDrop(gclient_t *client, team_t flag_team,
	uint64_t carrier_time_msec)
{
	const auto index = CtfTeamIndex(flag_team);
	if (!ValidClient(client) || !index)
		return;
	SaturatingAdd(client->pers.match.ctf_flag_drops, 1);
	SaturatingAdd(level.match.ctf[*index].flag_drops, 1);
	RecordCarrierDuration(client, *index, carrier_time_msec);
}

void MM_MatchStats_RecordCtfFlagReturn(gclient_t *client, team_t returning_team)
{
	const auto index = CtfTeamIndex(returning_team);
	if (!ValidClient(client) || !index)
		return;
	SaturatingAdd(client->pers.match.ctf_flag_returns, 1);
	SaturatingAdd(level.match.ctf[*index].flag_returns, 1);
}

void MM_MatchStats_RecordCtfFlagAssist(gclient_t *client, team_t scoring_team)
{
	const auto index = CtfTeamIndex(scoring_team);
	if (!ValidClient(client) || !index)
		return;
	SaturatingAdd(client->pers.match.ctf_flag_assists, 1);
	SaturatingAdd(level.match.ctf[*index].assists, 1);
}

void MM_MatchStats_RecordCtfFlagDefence(gclient_t *client, team_t defending_team)
{
	const auto index = CtfTeamIndex(defending_team);
	if (!ValidClient(client) || !index)
		return;
	SaturatingAdd(client->pers.match.ctf_flag_defences, 1);
	SaturatingAdd(level.match.ctf[*index].defences, 1);
}

void MM_MatchStats_RecordCtfFlagCapture(gclient_t *client, team_t scoring_team,
	team_t carried_flag_team, uint64_t carrier_time_msec)
{
	const auto scoring_index = CtfTeamIndex(scoring_team);
	const auto flag_index = CtfTeamIndex(carried_flag_team);
	if (!ValidClient(client) || !scoring_index)
		return;
	SaturatingAdd(client->pers.match.ctf_flag_captures, 1);
	SaturatingAdd(level.match.ctf[*scoring_index].captures, 1);
	if (flag_index)
		RecordCarrierDuration(client, *flag_index, carrier_time_msec);
}

void MM_MatchStats_LogEvent(std::string_view event)
{
	if (!MM_MatchStatsAcceptsResultEvents(
			level.match.collecting, level.match.finalized,
			level.match.match_end_real_time_ms) || event.empty())
		return;
	mm_match_event_t entry;
	entry.time_msec = MatchElapsedMsec();
	entry.event.assign(event.data(), std::min(event.size(),
		k_max_export_event_length));
	if (MM_MatchStatsLogHasCapacity(
		level.match.event_log.size(), MM_MATCH_EVENT_LIMIT)) {
		level.match.event_log.push_back(std::move(entry));
	} else if (!level.match.event_log_truncated) {
		level.match.event_log_truncated = true;
		gi.Com_PrintFmt("MM_MatchStats: event log truncated after {} entries\n",
			MM_MATCH_EVENT_LIMIT);
	}
}

int MM_MatchStats_PrintSchemaStatus()
{
	ExportWorker().DrainNotices();
	FrozenMatch sample;
	sample.match_id = "schema-smoke-match";
	sample.server_name = "schema-smoke-server";
	sample.server_host_name = "schema-smoke-host";
	sample.game_type = "FFA";
	sample.rule_set = rs_long_name[RS_Q3A];
	sample.ruleset_id = RS_Q3A;
	sample.map_name = "schema-smoke-map";
	sample.start_msec = 1000;
	sample.end_msec = 16000;
	sample.duration_msec = 15000;
	sample.time_limit_seconds = 600;
	sample.score_limit = 30;
	sample.overall.event_log_truncated = true;
	sample.overall.death_log_truncated = true;
	sample.overall.players_truncated = true;
	sample.overall.event_log.push_back({1000, "SCHEMA_SMOKE"});
	sample.overall.death_log.push_back({2000,
		{"Schema Victim", "victim-id"},
		{"Schema Attacker", "attacker-id"}, MOD_HYPERBLASTER});
	FrozenPlayer sample_player;
	sample_player.player_name = "Schema Player";
	sample_player.team = TEAM_FREE;
	sample_player.score = 30;
	sample_player.outcome = mm_match_player_outcome_t::win;
	sample_player.metadata.has_skill_rating = true;
	sample_player.metadata.skill_rating = 1516;
	sample_player.metadata.skill_rating_change = 16;
	sample.players.push_back(std::move(sample_player));
	const json match_json = MatchToJson(sample);
	FrozenMatch ctf_sample = sample;
	ctf_sample.ctf_mode = true;
	ctf_sample.overall.ctf[0].captures =
		std::numeric_limits<uint32_t>::max();
	ctf_sample.overall.ctf[1].captures =
		std::numeric_limits<uint32_t>::max();
	ctf_sample.overall.ctf[0].flag_hold_time_total_msec =
		std::numeric_limits<uint64_t>::max();
	ctf_sample.overall.ctf[1].flag_hold_time_total_msec = 1;
	const json ctf_json = MatchToJson(ctf_sample);
	const bool schema_ok = match_json["schemaName"].asString() ==
		k_match_schema_name && match_json["schemaVersion"].asInt() ==
		k_match_schema_version && match_json["artifactType"].asString() ==
		k_match_artifact_type && match_json["artifactVersion"].asInt() ==
		k_match_artifact_version &&
		match_json["serverHostName"].asString() == sample.server_host_name &&
		match_json["players"].isArray() &&
		match_json["players"].size() == 1 &&
		match_json["players"][0]["outcome"].asString() == "win" &&
		match_json["players"][0]["skillRating"].asInt() == 1516 &&
		match_json["eventLog"].isArray() &&
		match_json["deathLog"].isArray() &&
		match_json["eventLogTruncated"].asBool() &&
		match_json["deathLogTruncated"].asBool() &&
		match_json["playersTruncated"].asBool() &&
		match_json["deathLog"][0]["mod"].asString() == "Plasma Gun" &&
		ctf_json["totalFlagsCaptured"].asUInt64() ==
			static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) * 2 &&
		ctf_json["gametype"]["ctf"]["totals"]
			["flagHoldTimeTotalMsec"].asUInt64() ==
			std::numeric_limits<uint64_t>::max();

	json catalog;
	InitializeCatalog(catalog);
	json entry = BuildCatalogEntry(sample, sample.match_id);
	catalog["artifacts"].append(entry);
	catalog["latest"][k_match_artifact_type] = sample.match_id;
	catalog["artifactCount"] = 1;
	json malformed_catalog = catalog;
	malformed_catalog["schemaVersion"] = json(Json::objectValue);
	json older_entry = entry;
	older_entry["id"] = "older";
	older_entry["matchStartMS"] = Json::Int64(500);
	older_entry["matchEndMS"] = Json::Int64(15000);
	json generic_z(Json::objectValue);
	generic_z["artifactType"] = "schema-smoke-generic";
	generic_z["id"] = "z";
	json generic_a = generic_z;
	generic_a["id"] = "a";
	json boundary_types;
	InitializeCatalog(boundary_types);
	for (size_t index = 0;
		index < MM_MATCH_CATALOG_ARTIFACT_TYPE_LIMIT; ++index) {
		json boundary_entry(Json::objectValue);
		const std::string type = "type-" + std::to_string(index);
		const std::string id = "id-" + std::to_string(index);
		boundary_entry["artifactType"] = type;
		boundary_entry["id"] = id;
		boundary_types["artifacts"].append(std::move(boundary_entry));
		boundary_types["latest"][type] = id;
	}
	boundary_types["artifactCount"] = static_cast<Json::UInt64>(
		boundary_types["artifacts"].size());
	json excessive_types = boundary_types;
	json excessive_entry(Json::objectValue);
	excessive_entry["artifactType"] = "type-over-limit";
	excessive_entry["id"] = "id-over-limit";
	excessive_types["artifacts"].append(std::move(excessive_entry));
	excessive_types["latest"]["type-over-limit"] = "id-over-limit";
	excessive_types["artifactCount"] = static_cast<Json::UInt64>(
		excessive_types["artifacts"].size());
	json inconsistent_latest = catalog;
	inconsistent_latest["latest"][k_match_artifact_type] = "missing";
	json duplicate_identity = catalog;
	duplicate_identity["artifacts"].append(entry);
	duplicate_identity["artifactCount"] = 2;
	json invalid_utf8 = catalog;
	const std::string invalid_utf8_id("\x80", 1);
	invalid_utf8["artifacts"][0]["id"] = invalid_utf8_id;
	invalid_utf8["latest"][k_match_artifact_type] = invalid_utf8_id;
	json invalid_member_name = catalog;
	invalid_member_name["artifacts"][0][invalid_utf8_id] = 1;
	json invalid_latest_name = catalog;
	invalid_latest_name["latest"].removeMember(k_match_artifact_type);
	invalid_latest_name["latest"][invalid_utf8_id] = sample.match_id;
	std::string catalog_validation_error;
	const bool catalog_ok = CatalogMetadataValid(catalog) &&
		!CatalogMetadataValid(malformed_catalog) &&
		CatalogStructureValid(catalog, catalog_validation_error) &&
		CatalogLatestValid(catalog, catalog_validation_error) &&
		CatalogStructureValid(boundary_types, catalog_validation_error) &&
		CatalogLatestValid(boundary_types, catalog_validation_error) &&
		!CatalogStructureValid(excessive_types, catalog_validation_error) &&
		!CatalogStructureValid(duplicate_identity, catalog_validation_error) &&
		!CatalogStructureValid(invalid_utf8, catalog_validation_error) &&
		!CatalogStructureValid(invalid_member_name,
			catalog_validation_error) &&
		!CatalogStructureValid(invalid_latest_name,
			catalog_validation_error) &&
		!CatalogLatestValid(inconsistent_latest, catalog_validation_error) &&
		CatalogArtifactIsNewer(entry, older_entry) &&
		!CatalogArtifactIsNewer(older_entry, entry) &&
		catalog["artifacts"].isArray() && catalog["artifacts"].size() == 1 &&
		catalog["latest"][k_match_artifact_type].asString() == sample.match_id;
	int catalog_write_pass = 0;
	int catalog_write_artifact_count = 0;
	try {
		const std::filesystem::path smoke_directory =
			std::filesystem::path(".tmp") / "match_logging_catalog_smoke";
		std::error_code filesystem_error;
		std::filesystem::create_directories(smoke_directory, filesystem_error);
		if (!filesystem_error) {
			const std::filesystem::path smoke_catalog =
				smoke_directory / k_catalog_file_name;
			std::filesystem::remove(smoke_catalog, filesystem_error);
			if (!filesystem_error)
				std::filesystem::remove(
					smoke_catalog.string() + ".tmp", filesystem_error);

			std::string write_error;
			std::string write_warning;
			Json::StreamWriterBuilder boundary_writer;
			boundary_writer["indentation"] = "";
			const bool boundary_write_ok =
				WriteTextFileAtomically(smoke_catalog,
					Json::writeString(boundary_writer, boundary_types),
					write_error) &&
				UpdateCatalog(smoke_directory, entry, false,
					k_catalog_game_thread_lock_attempts, write_error,
					write_warning);
			CatalogReadResult boundary_written = boundary_write_ok
				? ReadCatalog(smoke_catalog) : CatalogReadResult{};
			const bool boundary_write_pass = boundary_write_ok &&
				boundary_written.status == CatalogReadStatus::valid &&
				boundary_written.catalog["artifacts"].size() ==
					MM_MATCH_CATALOG_ARTIFACT_TYPE_LIMIT &&
				boundary_written.catalog["latest"]
					[k_match_artifact_type].asString() == sample.match_id;
			std::filesystem::remove(smoke_catalog, filesystem_error);
			if (!filesystem_error)
				std::filesystem::remove(
					smoke_catalog.string() + ".tmp", filesystem_error);
			write_error.clear();
			write_warning.clear();

			json retention_boundary;
			InitializeCatalog(retention_boundary);
			std::string newest_retained_id;
			for (size_t index = 0; index < k_max_catalog_entries; ++index) {
				json retained = entry;
				newest_retained_id = "retained-" + std::to_string(index);
				retained["id"] = newest_retained_id;
				retained["matchStartMS"] = Json::Int64(20000 + index);
				retained["matchEndMS"] = Json::Int64(30000 + index);
				retention_boundary["artifacts"].append(std::move(retained));
			}
			retention_boundary["artifactCount"] =
				static_cast<Json::UInt64>(k_max_catalog_entries);
			retention_boundary["latest"][k_match_artifact_type] =
				newest_retained_id;
			const bool retention_write_ok =
				WriteTextFileAtomically(smoke_catalog,
					Json::writeString(boundary_writer, retention_boundary),
					write_error) &&
				UpdateCatalog(smoke_directory, older_entry, false,
					k_catalog_game_thread_lock_attempts, write_error,
					write_warning);
			CatalogReadResult retention_written = retention_write_ok
				? ReadCatalog(smoke_catalog) : CatalogReadResult{};
			bool out_of_window_entry_retained = false;
			if (retention_written.status == CatalogReadStatus::valid) {
				for (const json &artifact :
					retention_written.catalog["artifacts"])
					if (SameCatalogArtifact(artifact, older_entry)) {
						out_of_window_entry_retained = true;
						break;
					}
			}
			const bool retention_write_pass = retention_write_ok &&
				retention_written.status == CatalogReadStatus::valid &&
				retention_written.catalog["artifacts"].size() ==
					k_max_catalog_entries &&
				!out_of_window_entry_retained &&
				retention_written.catalog["latest"]
					[k_match_artifact_type].asString() == newest_retained_id;
			std::filesystem::remove(smoke_catalog, filesystem_error);
			if (!filesystem_error)
				std::filesystem::remove(
					smoke_catalog.string() + ".tmp", filesystem_error);
			write_error.clear();
			write_warning.clear();
			// Complete the newer artifact first, then the older one. The on-disk
			// smoke covers both atomic I/O and out-of-order worker completion.
			const bool write_ok = boundary_write_pass && retention_write_pass &&
				!filesystem_error &&
				UpdateCatalog(smoke_directory, generic_z, false,
					k_catalog_game_thread_lock_attempts, write_error,
					write_warning) &&
				UpdateCatalog(smoke_directory, generic_a, false,
					k_catalog_game_thread_lock_attempts, write_error,
					write_warning) &&
				UpdateCatalog(smoke_directory, entry, false,
					k_catalog_game_thread_lock_attempts, write_error,
					write_warning) &&
				UpdateCatalog(smoke_directory, older_entry, false,
					k_catalog_game_thread_lock_attempts, write_error,
					write_warning);
			CatalogReadResult written = write_ok
				? ReadCatalog(smoke_catalog) : CatalogReadResult{};
			json &written_catalog = written.catalog;
			const bool read_ok = write_ok &&
				written.status == CatalogReadStatus::valid;
			if (read_ok && written_catalog["artifacts"].isArray())
				catalog_write_artifact_count = static_cast<int>(
					written_catalog["artifacts"].size());
			catalog_write_pass = read_ok &&
				CatalogMetadataValid(written_catalog) &&
				catalog_write_artifact_count == 4 &&
				written_catalog["latest"][k_match_artifact_type].isString() &&
				written_catalog["latest"][k_match_artifact_type].asString() ==
					sample.match_id &&
				written_catalog["latest"]["schema-smoke-generic"].isString() &&
				written_catalog["latest"]["schema-smoke-generic"].asString() ==
					"a";
		}
	} catch (...) {
		catalog_write_pass = 0;
	}

	gi.Com_PrintFmt("q3a_match_logging_schema attempted=1 match_schema_name={} match_schema_version={} match_artifact_type={} match_artifact_version={} match_has_players_array={} match_has_event_log_array={} pass={}\n",
		match_json["schemaName"].asString(),
		match_json["schemaVersion"].asInt(),
		match_json["artifactType"].asString(),
		match_json["artifactVersion"].asInt(),
		match_json["players"].isArray() ? 1 : 0,
		match_json["eventLog"].isArray() ? 1 : 0,
		schema_ok ? 1 : 0);
	gi.Com_PrintFmt("q3a_match_logging_catalog attempted=1 catalog_schema_name={} catalog_schema_version={} catalog_artifact_type={} catalog_artifact_version={} catalog_artifact_count={} latest_match_stats={} catalog_write_pass={} catalog_write_artifact_count={} pass={}\n",
		catalog["schemaName"].asString(), catalog["schemaVersion"].asInt(),
		catalog["artifactType"].asString(), catalog["artifactVersion"].asInt(),
		catalog["artifactCount"].asInt(),
		catalog["latest"][k_match_artifact_type].asString(),
		catalog_write_pass, catalog_write_artifact_count,
		catalog_ok && catalog_write_pass ? 1 : 0);
	gi.Com_PrintFmt("q3a_match_logging_worker submitted={} pending={} completed={} failed={} last_error=\"{}\"\n",
		ExportWorker().Submitted(), ExportWorker().Pending(),
		ExportWorker().Completed(), ExportWorker().Failed(),
		ExportWorker().LastError());
	return schema_ok && catalog_ok && catalog_write_pass ? 1 : 0;
}

const mm_match_logging_status_api_v1_t *MM_MatchStats_StatusAPI()
{
	static const mm_match_logging_status_api_v1_t api{
		1, MM_MatchStats_PrintSchemaStatus
	};
	return &api;
}

void *MM_MatchStats_GetExtension(const char *name)
{
	if (!name || std::strcmp(name, MM_MATCH_LOGGING_STATUS_API_V1) != 0)
		return nullptr;
	return const_cast<mm_match_logging_status_api_v1_t *>(
		MM_MatchStats_StatusAPI());
}
