// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "mm_match_stats_types.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

struct cvar_t;
struct gclient_t;
struct gentity_t;
struct mod_t;
enum item_id_t : int32_t;
enum medal_t : uint8_t;
enum team_t;

inline constexpr char MM_MATCH_LOGGING_STATUS_API_V1[] =
	"MATCH_LOGGING_STATUS_API_V1";
inline constexpr size_t MM_MATCH_CATALOG_ARTIFACT_TYPE_LIMIT = 64;

struct mm_match_logging_status_api_v1_t {
	uint32_t version;
	int (*print_status)();
};

struct mm_match_player_export_metadata_t {
	int skill_rating = 0;
	int skill_rating_change = 0;
	bool has_skill_rating = false;
};

enum class mm_match_player_outcome_t : uint8_t {
	loss,
	win,
	draw,
	abandon,
	no_contest
};

extern cvar_t *g_statex_enabled;
extern cvar_t *g_statex_humans_present;
extern cvar_t *g_statex_export_html;

inline constexpr bool MM_MatchStatsAcceptsResultEvents(
	bool collecting, bool finalized, int64_t match_end_real_time_ms) noexcept
{
	return collecting && !finalized && match_end_real_time_ms == 0;
}

inline constexpr bool MM_MatchStatsHasRecordedParticipation(
	int64_t play_start_real_time_ms, uint64_t play_time_total_msec) noexcept
{
	return play_start_real_time_ms > 0 || play_time_total_msec > 0;
}

inline constexpr bool MM_MatchStatsCanRefreshArchivedParticipant(
	int64_t play_start_real_time_ms, uint64_t play_time_total_msec,
	int64_t play_end_real_time_ms) noexcept
{
	return MM_MatchStatsHasRecordedParticipation(
		play_start_real_time_ms, play_time_total_msec) ||
		play_end_real_time_ms > 0;
}

inline constexpr bool MM_MatchStatsCanDeliverFrozenResult(
	bool entity_inuse, bool client_connected, int32_t frozen_generation,
	int32_t current_generation) noexcept
{
	return entity_inuse && client_connected &&
		frozen_generation == current_generation;
}

inline constexpr bool MM_MatchStatsEffectiveProfileReadiness(
	bool live_readiness, bool has_authoritative_readiness,
	bool authoritative_readiness) noexcept
{
	return has_authoritative_readiness
		? authoritative_readiness : live_readiness;
}

inline int MM_MatchStatsTimeLimitSeconds(float configured_minutes) noexcept
{
	if (!(configured_minutes > 0.0f) || !std::isfinite(configured_minutes))
		return 0;
	const double seconds = static_cast<double>(configured_minutes) * 60.0;
	if (seconds >= static_cast<double>(std::numeric_limits<int>::max()))
		return std::numeric_limits<int>::max();
	// Match gtime_t::from_min(float): truncate the configured limit to whole
	// milliseconds first, then serialize the whole-second portion.
	const int64_t milliseconds = static_cast<int64_t>(
		configured_minutes * 60000.0f);
	return static_cast<int>(milliseconds / 1000);
}

inline constexpr bool MM_MatchStatsCatalogTypeCountValid(
	size_t distinct_type_count) noexcept
{
	return distinct_type_count <= MM_MATCH_CATALOG_ARTIFACT_TYPE_LIMIT;
}

inline constexpr uint64_t MM_MatchStatsWideCounterSum(
	uint32_t left, uint32_t right) noexcept
{
	return static_cast<uint64_t>(left) + static_cast<uint64_t>(right);
}

inline constexpr uint64_t MM_MatchStatsSaturatingDurationSum(
	uint64_t left, uint64_t right) noexcept
{
	return right > std::numeric_limits<uint64_t>::max() - left
		? std::numeric_limits<uint64_t>::max() : left + right;
}

inline constexpr bool MM_MatchStatsLogHasCapacity(
	size_t current_size, size_t limit) noexcept
{
	return current_size < limit;
}

inline constexpr bool MM_MatchStatsShouldCountHit(
	bool target_alive, bool same_team, bool count_once, bool has_inflictor,
	bool inflictor_already_counted) noexcept
{
	return target_alive && !same_team &&
		(!count_once || !has_inflictor || !inflictor_already_counted);
}

// Registration and lifecycle. Collection is independent of g_matchstats;
// that cvar controls only the live pmenu.
void MM_MatchStats_RegisterCvars();
void MM_MatchStats_Init();
void MM_MatchStats_RunFrame();
void MM_MatchStats_FreezeResultTime();
void MM_MatchStats_End();
void MM_MatchStats_Reset();
void MM_MatchStats_Shutdown();
bool MM_MatchStats_IsCollecting();

// Connection/play-time lifecycle. Call Begin when a client starts playing and
// End before disconnecting or becoming a spectator.
void MM_MatchStats_ClientBegin(gentity_t *player);
void MM_MatchStats_ClientEnd(gentity_t *player);
void MM_MatchStats_RefreshArchivedPlayerMetadata(
	gentity_t *player, const gclient_t *authoritative_profile_state);
void MM_MatchStats_RecordSpawn(gclient_t *client);

// Stable taxonomy helpers used by thin instrumentation hooks.
mm_match_weapon_t MM_MatchStats_WeaponForItem(item_id_t item);
mm_match_weapon_t MM_MatchStats_WeaponForMod(const mod_t &mod);
mm_match_high_value_item_t MM_MatchStats_HighValueForItem(item_id_t item);
mm_match_medal_t MM_MatchStats_MedalForNative(medal_t medal);
const char *MM_MatchStats_WeaponAbbreviation(mm_match_weapon_t weapon);
const char *MM_MatchStats_ModName(uint8_t mod);
const char *MM_MatchStats_MedalName(mm_match_medal_t medal);
const char *MM_MatchStats_HighValueItemName(mm_match_high_value_item_t item);

// Combat and pickup instrumentation.
void MM_MatchStats_RecordShot(gclient_t *client, mm_match_weapon_t weapon,
	uint32_t count = 1);
void MM_MatchStats_RecordHit(gclient_t *client, const mod_t &mod,
	uint32_t count = 1);
void MM_MatchStats_RecordDamage(gclient_t *attacker, gclient_t *victim,
	const mod_t &mod, uint32_t amount);
void MM_MatchStats_RecordDeath(gentity_t *victim, gentity_t *attacker,
	const mod_t &mod, bool spawn_death, bool team_kill);
void MM_MatchStats_RecordMedal(gclient_t *client, medal_t medal,
	uint32_t count = 1);
void MM_MatchStats_RecordPickup(gclient_t *client, item_id_t item,
	uint64_t pickup_delay_msec);
// Notes that a map-placed item has returned to the floor. Only the Quad is
// counted today: the post-match Quad awards are a share of how many times it was
// available, which a pickup total on its own cannot express.
void MM_MatchStats_RecordItemAvailable(const gentity_t *ent);

// CTF instrumentation. Flag-team counters describe the flag being carried;
// scoring-team counters describe captures, defences, and assists.
void MM_MatchStats_RecordCtfFlagPickup(gclient_t *client, team_t flag_team);
void MM_MatchStats_RecordCtfFlagDrop(gclient_t *client, team_t flag_team,
	uint64_t carrier_time_msec);
void MM_MatchStats_RecordCtfFlagReturn(gclient_t *client, team_t returning_team);
void MM_MatchStats_RecordCtfFlagAssist(gclient_t *client, team_t scoring_team);
void MM_MatchStats_RecordCtfFlagDefence(gclient_t *client, team_t defending_team);
void MM_MatchStats_RecordCtfFlagCapture(gclient_t *client, team_t scoring_team,
	team_t carried_flag_team, uint64_t carrier_time_msec);
void MM_MatchStats_RecordCtfCarrierTime(gclient_t *client, team_t flag_team,
	uint64_t carrier_time_msec);

void MM_MatchStats_LogEvent(std::string_view event);

int MM_MatchStats_PrintSchemaStatus();
const mm_match_logging_status_api_v1_t *MM_MatchStats_StatusAPI();
void *MM_MatchStats_GetExtension(const char *name);
