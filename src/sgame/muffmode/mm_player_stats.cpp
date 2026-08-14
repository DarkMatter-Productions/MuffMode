// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_awards.h"
#include "muffmode/mm_client_profile.h"
#include "muffmode/mm_duel.h"
#include "muffmode/mm_ghost.h"
#include "muffmode/mm_match_stats.h"
#include "muffmode/mm_player_stats.h"
#include "muffmode/mm_util.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

struct player_stats_runtime_t {
	std::string social_id;
	int64_t accumulated_play_ms = 0;
	int32_t spawn_count = 0;
	bool initialized = false;
};

struct player_stats_participant_t {
	gentity_t *ent = nullptr;
	gclient_t *state = nullptr;
	std::string player_name;
	std::string recovery_preferences_json;
	float rating = MM_PLAYER_STATS_DEFAULT_RATING;
	int32_t score = 0;
	team_t team = TEAM_NONE;
	bool bot = false;
	bool profile_ready = false;
};

std::array<player_stats_runtime_t, MAX_LOBBY_PLAYERS> s_runtime;
uint32_t s_match_serial = 0;
bool s_match_open = false;

enum class player_stats_match_mode_t : uint8_t {
	ffa,
	duel,
	teams
};

std::string s_match_gametype;
player_stats_match_mode_t s_match_mode = player_stats_match_mode_t::ffa;
bool s_match_has_bot_participant = false;
bool s_match_has_unready_human_participant = false;
bool s_match_has_non_atomic_departure = false;

struct settled_player_t {
	float rating = MM_PLAYER_STATS_DEFAULT_RATING;
	int32_t change = 0;
	uint32_t serial = 0;
	mm_player_stats_outcome_t outcome = mm_player_stats_outcome_t::no_contest;
	bool profile_result_admitted = false;
};

using settled_gametype_map_t =
	std::unordered_map<std::string, settled_player_t>;
std::unordered_map<std::string, settled_gametype_map_t> s_settled_players;

struct pending_profile_settlement_t {
	uint32_t serial = 0;
	std::string social_id;
	std::string gametype;
	std::string player_name;
	std::string recovery_preferences_json;
	mm_client_profile_outcome_t outcome = mm_client_profile_outcome_t::draw;
	float rating = MM_PLAYER_STATS_DEFAULT_RATING;
	int32_t change = 0;
	int64_t duration_ms = 0;
	int64_t next_retry_ms = 0;
	int64_t retry_delay_ms = 0;
	// [MuffMode] Post-match award keys earned in this match. Owned here because
	// a settlement can be retried across many frames, long after the reel that
	// produced it has been torn down.
	std::vector<std::string> match_awards;
};

std::vector<pending_profile_settlement_t> s_pending_profile_settlements;
constexpr int64_t k_profile_retry_min_delay_ms = 5000;
constexpr int64_t k_profile_retry_max_delay_ms = 60000;
constexpr size_t k_profile_retry_attempts_per_frame = 1;
constexpr size_t k_profile_retry_candidates_per_frame = 64;
constexpr size_t k_profile_retry_attempts_on_shutdown =
	MM_PLAYER_STATS_PENDING_SETTLEMENT_LIMIT;
std::deque<std::string> s_profile_retry_round_robin;
size_t s_profile_io_attempts_since_run_frame = 0;

int64_t MonotonicMilliseconds() noexcept
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

size_t ClientSlotIndex(const gentity_t *ent) noexcept
{
	if (!g_entities)
		return MAX_LOBBY_PLAYERS;
	const size_t index = MM_GhostAddressIndex(
		reinterpret_cast<uintptr_t>(ent),
		reinterpret_cast<uintptr_t>(&g_entities[1]), sizeof(g_entities[0]),
		std::min(static_cast<size_t>(game.maxclients),
			static_cast<size_t>(MAX_LOBBY_PLAYERS)));
	return index == MM_GHOST_NO_CLIENT_INDEX ? MAX_LOBBY_PLAYERS : index;
}

bool IsPlayingTeam(team_t team) noexcept
{
	return team != TEAM_NONE && team != TEAM_SPECTATOR;
}

bool IsBot(const gentity_t *ent) noexcept
{
	return ent && ent->client &&
		(ent->client->sess.is_a_bot || (ent->svflags & SVF_BOT));
}

bool HasSocialId(const gentity_t *ent) noexcept
{
	return ent && ent->client && ent->client->pers.social_id[0] != '\0';
}

void EnsureRuntimeOwner(gentity_t *ent)
{
	const size_t index = ClientSlotIndex(ent);
	if (index >= s_runtime.size() || !ent || !ent->client)
		return;

	auto &runtime = s_runtime[index];
	const std::string_view social_id = ent->client->pers.social_id;
	if (MM_PlayerStats_RuntimeOwnerMatches(
			runtime.initialized, runtime.social_id, runtime.spawn_count,
			social_id, ent->spawn_count))
		return;

	runtime = {};
	runtime.social_id.assign(social_id);
	runtime.spawn_count = ent->spawn_count;
	runtime.initialized = true;
	ent->client->sess.stats_play_start_ms = 0;
	ent->client->sess.stats_saved_match_serial = 0;
	ent->client->sess.stats_saved_match_outcome =
		static_cast<uint8_t>(mm_player_stats_outcome_t::no_contest);
	ent->client->sess.stats_reconnect_suspended = false;
}

void BeginPlayTime(gentity_t *ent, int64_t now)
{
	const size_t index = ClientSlotIndex(ent);
	if (index >= s_runtime.size())
		return;

	EnsureRuntimeOwner(ent);
	if (ent->client->sess.stats_play_start_ms <= 0)
		ent->client->sess.stats_play_start_ms = now;
}

void AccumulatePlayTime(gentity_t *ent, int64_t now, bool continue_playing)
{
	const size_t index = ClientSlotIndex(ent);
	if (index >= s_runtime.size())
		return;

	EnsureRuntimeOwner(ent);
	auto &runtime = s_runtime[index];
	const int64_t play_start_ms = ent->client->sess.stats_play_start_ms;
	if (play_start_ms > 0 && now > play_start_ms) {
		const int64_t elapsed = now - play_start_ms;
		if (runtime.accumulated_play_ms >
			std::numeric_limits<int64_t>::max() - elapsed)
			runtime.accumulated_play_ms = std::numeric_limits<int64_t>::max();
		else
			runtime.accumulated_play_ms += elapsed;
	}

	ent->client->sess.stats_play_start_ms = continue_playing ? now : 0;
}

int64_t AccumulatedPlayTime(const gentity_t *ent) noexcept
{
	const size_t index = ClientSlotIndex(ent);
	return index < s_runtime.size() ? s_runtime[index].accumulated_play_ms : 0;
}

float DefaultRating() noexcept
{
	const float profile_default = MM_ClientProfileDefaultRating();
	return std::isfinite(profile_default)
		? std::clamp(profile_default,
			MM_PLAYER_STATS_MIN_RATING, MM_PLAYER_STATS_MAX_RATING)
		: MM_PLAYER_STATS_DEFAULT_RATING;
}

float NormalizeClientRating(gclient_t *client) noexcept
{
	if (!client)
		return DefaultRating();

	if (!std::isfinite(client->sess.skill_rating) ||
		client->sess.skill_rating < MM_PLAYER_STATS_MIN_RATING)
		client->sess.skill_rating = DefaultRating();
	else
		client->sess.skill_rating = MM_PlayerStats_NormalizeRating(
			client->sess.skill_rating);

	return client->sess.skill_rating;
}

float ClientRating(const gclient_t *client) noexcept
{
	if (!client || !std::isfinite(client->sess.skill_rating) ||
		client->sess.skill_rating < MM_PLAYER_STATS_MIN_RATING) {
		return DefaultRating();
	}
	return MM_PlayerStats_NormalizeRating(client->sess.skill_rating);
}

std::string_view EffectiveGametypeName() noexcept
{
	const int gametype = MM_EFFECTIVE_GT;
	if (gametype < 0 || gametype >= GT_NUM_GAMETYPES)
		return "unknown";
	return gt_short_name[gametype];
}

std::string_view MatchGametypeName() noexcept
{
	return s_match_gametype.empty()
		? EffectiveGametypeName()
		: std::string_view(s_match_gametype);
}

std::string_view RecoveryGametypeName() noexcept
{
	return s_match_open ? MatchGametypeName() : EffectiveGametypeName();
}

bool AlreadySettled(const gentity_t *ent) noexcept;

std::string BoundedPlayerName(const gclient_t *client)
{
	if (!client)
		return {};
	const char *const end = std::find(
		client->pers.netname,
		client->pers.netname + MAX_NETNAME,
		'\0');
	return std::string(client->pers.netname, end);
}

std::vector<player_stats_participant_t> CollectParticipants()
{
	std::vector<player_stats_participant_t> participants;
	participants.reserve(game.maxclients);

	for (size_t i = 0; i < game.maxclients; ++i) {
		gentity_t *ent = &g_entities[i + 1];
		if (!ent->client)
			continue;

		// A reservation remains authoritative while disconnected and during
		// the reinstatement delay, when ClientConnect has initialized the live
		// slot with TEAM_NONE. Successful restore clears the snapshot.
		gclient_t *state = MM_Ghost_ReservedClientState(ent);
		if (!state) {
			if (!ent->inuse || !ent->client->pers.connected)
				continue;
			state = ent->client;
		}
		if (!IsPlayingTeam(state->sess.team))
			continue;
		const bool bot = state->sess.is_a_bot || (ent->svflags & SVF_BOT);
		if (bot)
			s_match_has_bot_participant = true;
		else if (!state->sess.profile_persistence_ready)
			s_match_has_unready_human_participant = true;
		if (AlreadySettled(ent))
			continue;
		const bool requires_profile_result = !bot &&
			state->sess.profile_persistence_ready &&
			state->pers.social_id[0] &&
			MM_ClientProfileCanPersistIdentity(state->pers.social_id);

		participants.push_back({
			ent,
			state,
			BoundedPlayerName(state),
			requires_profile_result
				? MM_ClientProfileSerializePreferences(state)
				: std::string(),
			state == ent->client
				? NormalizeClientRating(ent->client)
				: ClientRating(state),
			state->resp.score,
			state->sess.team,
			bot,
			state->sess.profile_persistence_ready
		});
	}

	return participants;
}

player_stats_participant_t *FindParticipant(
	std::vector<player_stats_participant_t> &participants, gentity_t *ent)
{
	for (auto &participant : participants)
		if (participant.ent == ent)
			return &participant;
	return nullptr;
}

bool RatingsAllowed(const std::vector<player_stats_participant_t> &participants)
{
	// Host opted out of ranking entirely: play still records outcomes, ratings simply never move.
	if (!muffmode::CvarEnabled(g_ranked))
		return false;

	return MM_PlayerStatsMatchCanBeRanked(
			s_match_has_bot_participant,
			s_match_has_unready_human_participant,
			s_match_has_non_atomic_departure) &&
		std::none_of(participants.begin(), participants.end(),
		[](const player_stats_participant_t &participant) {
			return participant.bot || !participant.profile_ready;
		});
}

mm_client_profile_outcome_t ProfileOutcome(mm_player_stats_outcome_t outcome)
{
	switch (outcome) {
	case mm_player_stats_outcome_t::win:
		return mm_client_profile_outcome_t::win;
	case mm_player_stats_outcome_t::loss:
		return mm_client_profile_outcome_t::loss;
	case mm_player_stats_outcome_t::draw:
		return mm_client_profile_outcome_t::draw;
	case mm_player_stats_outcome_t::abandon:
		return mm_client_profile_outcome_t::abandon;
	case mm_player_stats_outcome_t::no_contest:
		break;
	}

	return mm_client_profile_outcome_t::draw;
}

bool PersistPendingProfileSettlement(
	const pending_profile_settlement_t &pending)
{
	const mm_client_profile_match_result_t result{
		pending.social_id,
		pending.gametype,
		pending.outcome,
		pending.rating,
		pending.change,
		pending.duration_ms,
		pending.player_name,
		pending.recovery_preferences_json,
		&pending.match_awards
	};
	return MM_ClientProfilePersistMatchResult(result);
}

auto FirstPendingProfileSettlement(std::string_view social_id)
{
	return std::find_if(s_pending_profile_settlements.begin(),
		s_pending_profile_settlements.end(),
		[&](const pending_profile_settlement_t &pending) {
			return pending.social_id == social_id;
		});
}

void RegisterProfileRetryIdentity(std::string_view social_id)
{
	if (social_id.empty() || std::find_if(s_profile_retry_round_robin.begin(),
			s_profile_retry_round_robin.end(),
			[&](const std::string &candidate) {
				return candidate == social_id;
			}) !=
			s_profile_retry_round_robin.end()) {
		return;
	}
	s_profile_retry_round_robin.emplace_back(social_id);
}

void BackOffProfileSettlement(
	pending_profile_settlement_t &pending, int64_t now) noexcept
{
	pending.retry_delay_ms = std::clamp(pending.retry_delay_ms,
		k_profile_retry_min_delay_ms, k_profile_retry_max_delay_ms);
	pending.next_retry_ms = now + pending.retry_delay_ms;
	pending.retry_delay_ms = std::min(
		pending.retry_delay_ms * 2, k_profile_retry_max_delay_ms);
}

void RetryPendingProfileSettlements(
	size_t max_attempts, bool ignore_deadlines = false,
	bool ignore_frame_budget = false)
{
	// Normal frames make one fair pass over the identities present at entry.
	// Shutdown may make further passes to drain multiple ordered results for an
	// identity, but an identity that fails once is not hammered again during the
	// same bounded drain.
	std::vector<std::string> failed_identities;
	size_t attempts = 0;
	for (;;) {
		const size_t candidates = ignore_frame_budget
			? s_profile_retry_round_robin.size()
			: std::min(s_profile_retry_round_robin.size(),
				k_profile_retry_candidates_per_frame);
		if (!candidates || attempts >= max_attempts)
			break;

		bool pass_succeeded = false;
		bool pass_attempted = false;
		for (size_t visited = 0;
			visited < candidates && attempts < max_attempts; ++visited) {
			if (s_profile_retry_round_robin.empty())
				break;
			if (!ignore_frame_budget &&
				s_profile_io_attempts_since_run_frame >=
					k_profile_retry_attempts_per_frame) {
				break;
			}

			const std::string social_id =
				s_profile_retry_round_robin.front();
			s_profile_retry_round_robin.pop_front();
			auto pending = FirstPendingProfileSettlement(social_id);
			if (pending == s_pending_profile_settlements.end())
				continue;

			const bool failed_this_drain = ignore_frame_budget &&
				std::find(failed_identities.begin(), failed_identities.end(),
					social_id) != failed_identities.end();
			const int64_t now = MonotonicMilliseconds();
			if (!failed_this_drain &&
				(ignore_deadlines || pending->next_retry_ms <= now)) {
				pass_attempted = true;
				++attempts;
				++s_profile_io_attempts_since_run_frame;
				if (!PersistPendingProfileSettlement(*pending)) {
					BackOffProfileSettlement(*pending, now);
					if (ignore_frame_budget)
						failed_identities.push_back(social_id);
				} else {
					s_pending_profile_settlements.erase(pending);
					pass_succeeded = true;
				}
			}

			if (FirstPendingProfileSettlement(social_id) !=
				s_pending_profile_settlements.end()) {
				s_profile_retry_round_robin.push_back(social_id);
			}
		}

		if (!ignore_frame_budget || !pass_attempted || !pass_succeeded)
			break;
	}

	if (s_pending_profile_settlements.empty())
		s_profile_retry_round_robin.clear();
}

void PruneSettledPlayerHistory()
{
	for (auto identity = s_settled_players.begin();
		identity != s_settled_players.end();) {
		for (auto settled = identity->second.begin();
			settled != identity->second.end();) {
			const bool pending = std::any_of(
				s_pending_profile_settlements.begin(),
				s_pending_profile_settlements.end(),
				[&](const pending_profile_settlement_t &candidate) {
					return candidate.social_id == identity->first &&
						candidate.gametype == settled->first;
				});
			if (!pending)
				settled = identity->second.erase(settled);
			else
				++settled;
		}
		if (identity->second.empty())
			identity = s_settled_players.erase(identity);
		else
			++identity;
	}
}

// Award keys this identity took off the post-match reel. Matched on social id
// because that is the only stable handle a career profile is keyed by; a player
// with no social id has no profile to credit in the first place.
std::vector<std::string> CollectAwardKeys(std::string_view social_id)
{
	std::vector<std::string> keys;
	if (social_id.empty())
		return keys;

	// The reel is deliberately sticky so `awards` still works after the map
	// changes, and it carries no match identity of its own. Career tallies are
	// permanent, so credit it only while it belongs to the match settling now.
	if (!MM_Awards_BelongsToSettlingMatch())
		return keys;

	for (const mm_award_entry_t &entry : MM_Awards_Entries()) {
		if (entry.social_id != social_id)
			continue;
		const char *key = MM_AwardKey(entry.award);
		if (key && *key)
			keys.emplace_back(key);
	}

	return keys;
}

bool EnqueueProfileSettlement(
	const player_stats_participant_t &participant,
	mm_player_stats_outcome_t outcome,
	int64_t duration_ms)
{
	const gclient_t *client = participant.state;
	if (!client || !client->pers.social_id[0] || s_match_serial == 0)
		return false;
	const std::string_view social_id = client->pers.social_id;
	const auto duplicate = std::find_if(
		s_pending_profile_settlements.begin(),
		s_pending_profile_settlements.end(),
		[&](const pending_profile_settlement_t &pending) {
			return pending.serial == s_match_serial &&
				pending.social_id == social_id;
		});
	if (duplicate != s_pending_profile_settlements.end())
		return true;
	if (!MM_PlayerStatsPendingQueueHasCapacity(
		s_pending_profile_settlements.size())) {
		// Never evict an older exact result or force storage I/O from gameplay.
		// The bounded frame pump remains the only normal write path.
		gi.Com_PrintFmt(
			"MM_PlayerStats: pending profile queue full; cannot queue result from match {}\n",
			s_match_serial);
		s_match_has_unready_human_participant = true;
		return false;
	}
	if (participant.recovery_preferences_json.empty()) {
		gi.Com_PrintFmt(
			"MM_PlayerStats: cannot capture recovery preferences for result from match {}; settlement remains session-only and unranked.\n",
			s_match_serial);
		s_match_has_unready_human_participant = true;
		return false;
	}

	s_pending_profile_settlements.push_back({
		s_match_serial,
		std::string(social_id),
		std::string(MatchGametypeName()),
		participant.player_name,
		participant.recovery_preferences_json,
		ProfileOutcome(outcome),
		client->sess.skill_rating,
		client->sess.skill_rating_change,
		std::max<int64_t>(0, duration_ms)
	});
	// The awards reel is already decided by the time settlements are queued --
	// MM_MatchStats_FreezeResultTime runs before MM_PlayerStats_OnMatchEnd --
	// so the winner's titles can be attached to the career write here.
	s_pending_profile_settlements.back().match_awards =
		CollectAwardKeys(social_id);
	RegisterProfileRetryIdentity(social_id);
	return true;
}

bool ProfileSettlementRequiresQueue(
	const player_stats_participant_t &participant)
{
	return participant.state && !participant.bot && participant.profile_ready &&
		participant.state->pers.social_id[0] &&
		MM_ClientProfileCanPersistIdentity(
			participant.state->pers.social_id);
}

bool ProfileSettlementsCanBeQueued(
	const std::vector<player_stats_participant_t> &participants)
{
	const bool snapshots_ready = std::all_of(
		participants.begin(), participants.end(),
		[](const player_stats_participant_t &participant) {
			return !ProfileSettlementRequiresQueue(participant) ||
				!participant.recovery_preferences_json.empty();
		});
	const size_t required = static_cast<size_t>(std::count_if(
		participants.begin(), participants.end(),
		ProfileSettlementRequiresQueue));
	const bool admitted = snapshots_ready &&
		MM_PlayerStatsPendingQueueCanAdmit(
			s_pending_profile_settlements.size(), required);
	if (!admitted) {
		s_match_has_unready_human_participant = true;
		if (!snapshots_ready) {
			gi.Com_PrintFmt(
				"MM_PlayerStats: could not capture every recovery preference snapshot for match {}; settlement is session-only and unranked.\n",
				s_match_serial);
		} else {
			gi.Com_PrintFmt(
				"MM_PlayerStats: pending profile queue cannot admit {} result{} from match {}; settlement is session-only and unranked.\n",
				required, required == 1 ? "" : "s", s_match_serial);
		}
	}
	return admitted;
}

bool ProfileSettlementCanBeQueued(
	const player_stats_participant_t &participant)
{
	const size_t required = ProfileSettlementRequiresQueue(participant) ? 1 : 0;
	const bool admitted =
		(!required || !participant.recovery_preferences_json.empty()) &&
		MM_PlayerStatsPendingQueueCanAdmit(
			s_pending_profile_settlements.size(), required);
	if (!admitted) {
		s_match_has_unready_human_participant = true;
		if (required && participant.recovery_preferences_json.empty()) {
			gi.Com_PrintFmt(
				"MM_PlayerStats: could not capture recovery preferences for match {}; settlement is session-only and unranked.\n",
				s_match_serial);
		} else {
			gi.Com_PrintFmt(
				"MM_PlayerStats: pending profile queue cannot admit a result from match {}; settlement is session-only and unranked.\n",
				s_match_serial);
		}
	}
	return admitted;
}

bool AlreadySettled(const gentity_t *ent) noexcept
{
	if (!ent || !ent->client || s_match_serial == 0)
		return false;
	if (ent->client->sess.stats_saved_match_serial == s_match_serial)
		return true;
	if (!HasSocialId(ent))
		return false;
	const auto identity = s_settled_players.find(ent->client->pers.social_id);
	if (identity == s_settled_players.end())
		return false;
	const auto settled = identity->second.find(
		std::string(MatchGametypeName()));
	return settled != identity->second.end() &&
		settled->second.serial == s_match_serial;
}

bool ApplyRecordedSettlement(gclient_t *client, std::string_view gametype,
	bool allow_completed_current_match = false)
{
	if (!client || !client->pers.social_id[0] || gametype.empty())
		return false;
	const auto identity = s_settled_players.find(client->pers.social_id);
	if (identity == s_settled_players.end())
		return false;
	const auto settled = identity->second.find(
		std::string(gametype));
	if (settled == identity->second.end())
		return false;
	const bool exact_pending = std::any_of(
		s_pending_profile_settlements.begin(),
		s_pending_profile_settlements.end(),
		[&](const pending_profile_settlement_t &pending) {
			return pending.serial == settled->second.serial &&
				pending.social_id == client->pers.social_id &&
				pending.gametype == gametype;
		});
	if (!MM_PlayerStatsShouldApplyRecordedSettlement(
			settled->second.serial == s_match_serial &&
				(s_match_open || (allow_completed_current_match &&
					settled->second.profile_result_admitted)),
			exact_pending)) {
		return false;
	}
	client->sess.skill_rating = settled->second.rating;
	client->sess.skill_rating_change = settled->second.change;
	client->sess.stats_saved_match_serial = settled->second.serial;
	client->sess.stats_saved_match_outcome =
		static_cast<uint8_t>(settled->second.outcome);
	client->sess.stats_reconnect_suspended = false;
	return true;
}

void SettleParticipant(player_stats_participant_t &participant,
	mm_player_stats_outcome_t outcome,
	const mm_player_stats_rating_update_t &rating_update,
	bool profile_persistence_allowed)
{
	gentity_t *ent = participant.ent;
	gclient_t *client = participant.state;
	if (!ent || !ent->client || !client)
		return;
	if (AlreadySettled(ent)) {
		ApplyRecordedSettlement(client, MatchGametypeName());
		client->sess.stats_reconnect_suspended = false;
		return;
	}

	const float original_rating = participant.rating;
	const bool persist_profile = outcome !=
		mm_player_stats_outcome_t::no_contest &&
		ProfileSettlementRequiresQueue(participant) &&
		profile_persistence_allowed;
	client->sess.stats_reconnect_suspended = false;
	client->sess.skill_rating = MM_PlayerStats_NormalizeRating(
		profile_persistence_allowed ? rating_update.rating : original_rating);
	client->sess.skill_rating_change =
		profile_persistence_allowed ? rating_update.change : 0;
	participant.rating = client->sess.skill_rating;

	bool profile_result_admitted = false;
	if (persist_profile) {
		// Preserve the exact result before claiming the in-memory serial. Failed
		// atomic writes remain queued and are retried without recomputing Elo.
		profile_result_admitted = EnqueueProfileSettlement(
			participant, outcome, AccumulatedPlayTime(ent));
		if (!profile_result_admitted) {
			// Admission was preflighted on the game thread. Defensively fail closed
			// if an invariant changes instead of retaining an unpersistable Elo.
			client->sess.skill_rating = original_rating;
			client->sess.skill_rating_change = 0;
			participant.rating = original_rating;
		}
	}

	// Claim the serial before external persistence. The current-match ledger
	// prevents duplicate settlement while the owned queue makes failures retryable.
	client->sess.stats_saved_match_serial = s_match_serial;
	client->sess.stats_saved_match_outcome = static_cast<uint8_t>(outcome);
	if (client->pers.social_id[0])
		s_settled_players[client->pers.social_id]
			[std::string(MatchGametypeName())] = {
			client->sess.skill_rating,
			client->sess.skill_rating_change,
			s_match_serial,
			outcome,
			profile_result_admitted
		};
	MM_MatchStats_RefreshArchivedPlayerMetadata(
		ent, client);
}

void SettleNoContest(std::vector<player_stats_participant_t> &participants)
{
	for (auto &participant : participants) {
		const mm_player_stats_rating_update_t unchanged{
			participant.rating,
			0
		};
		SettleParticipant(participant,
			mm_player_stats_outcome_t::no_contest, unchanged, false);
	}
}

void AccumulateParticipantTimes(
	const std::vector<player_stats_participant_t> &participants, int64_t now)
{
	for (const auto &participant : participants)
		AccumulatePlayTime(participant.ent, now, false);
}

void SettleDuel(std::vector<player_stats_participant_t> &participants,
	bool ratings_allowed)
{
	if (participants.size() != 2) {
		SettleNoContest(participants);
		return;
	}

	auto &a = participants[0];
	auto &b = participants[1];
	const bool profile_persistence_allowed =
		ProfileSettlementsCanBeQueued(participants);
	ratings_allowed = ratings_allowed && profile_persistence_allowed;
	const mm_duel_final_outcome_t final_a =
		MM_Duel_FinalOutcomeForClient(a.ent);
	const mm_duel_final_outcome_t final_b =
		MM_Duel_FinalOutcomeForClient(b.ent);
	const bool frozen_draw = final_a == mm_duel_final_outcome_t::draw &&
		final_b == mm_duel_final_outcome_t::draw;
	const bool frozen_a_win = final_a == mm_duel_final_outcome_t::win &&
		final_b == mm_duel_final_outcome_t::loss;
	const bool frozen_b_win = final_a == mm_duel_final_outcome_t::loss &&
		final_b == mm_duel_final_outcome_t::win;
	const bool has_frozen_result = frozen_draw || frozen_a_win || frozen_b_win;
	const bool draw = has_frozen_result ? frozen_draw : a.score == b.score;
	const bool a_won = has_frozen_result ? frozen_a_win : a.score > b.score;
	const float actual_a = draw ? 0.5f : (a_won ? 1.0f : 0.0f);
	const float expected_a = MM_PlayerStats_EloExpected(a.rating, b.rating);

	const mm_player_stats_rating_update_t update_a = ratings_allowed
		? MM_PlayerStats_ApplyRating(a.rating, actual_a, expected_a)
		: mm_player_stats_rating_update_t{ a.rating, 0 };
	const mm_player_stats_rating_update_t update_b = ratings_allowed
		? MM_PlayerStats_ApplyRating(b.rating, 1.0f - actual_a, 1.0f - expected_a)
		: mm_player_stats_rating_update_t{ b.rating, 0 };

	SettleParticipant(a,
		draw ? mm_player_stats_outcome_t::draw :
			(a_won ? mm_player_stats_outcome_t::win : mm_player_stats_outcome_t::loss),
		update_a, profile_persistence_allowed);
	SettleParticipant(b,
		draw ? mm_player_stats_outcome_t::draw :
			(a_won ? mm_player_stats_outcome_t::loss : mm_player_stats_outcome_t::win),
		update_b, profile_persistence_allowed);
}

void SettleTeams(std::vector<player_stats_participant_t> &participants,
	bool ratings_allowed)
{
	float red_total = 0.0f;
	float blue_total = 0.0f;
	size_t red_count = 0;
	size_t blue_count = 0;

	for (const auto &participant : participants) {
		if (participant.team == TEAM_RED) {
			red_total += participant.rating;
			++red_count;
		} else if (participant.team == TEAM_BLUE) {
			blue_total += participant.rating;
			++blue_count;
		}
	}

	if (red_count == 0 || blue_count == 0 ||
		red_count + blue_count != participants.size()) {
		SettleNoContest(participants);
		return;
	}
	const bool profile_persistence_allowed =
		ProfileSettlementsCanBeQueued(participants);
	ratings_allowed = ratings_allowed && profile_persistence_allowed;

	const float red_rating = red_total / static_cast<float>(red_count);
	const float blue_rating = blue_total / static_cast<float>(blue_count);
	const float expected_red = MM_PlayerStats_EloExpected(red_rating, blue_rating);
	const bool draw = level.team_scores[TEAM_RED] == level.team_scores[TEAM_BLUE];
	const bool red_won = level.team_scores[TEAM_RED] > level.team_scores[TEAM_BLUE];

	for (auto &participant : participants) {
		const bool red = participant.team == TEAM_RED;
		const float actual = draw ? 0.5f :
			(red == red_won ? 1.0f : 0.0f);
		const float expected = red ? expected_red : 1.0f - expected_red;
		const auto update = ratings_allowed
			? MM_PlayerStats_ApplyRating(participant.rating, actual, expected)
			: mm_player_stats_rating_update_t{ participant.rating, 0 };
		const auto outcome = draw ? mm_player_stats_outcome_t::draw :
			(actual > 0.5f ? mm_player_stats_outcome_t::win :
				mm_player_stats_outcome_t::loss);
		SettleParticipant(participant, outcome, update,
			profile_persistence_allowed);
	}
}

void SettleFfa(std::vector<player_stats_participant_t> &participants,
	bool ratings_allowed)
{
	if (participants.size() < 2) {
		SettleNoContest(participants);
		return;
	}
	const bool profile_persistence_allowed =
		ProfileSettlementsCanBeQueued(participants);
	ratings_allowed = ratings_allowed && profile_persistence_allowed;

	std::vector<int32_t> scores;
	std::vector<float> ratings;
	scores.reserve(participants.size());
	ratings.reserve(participants.size());
	for (const auto &participant : participants) {
		scores.push_back(participant.score);
		ratings.push_back(participant.rating);
	}

	const int32_t top_score = *std::max_element(scores.begin(), scores.end());
	const size_t top_count = static_cast<size_t>(std::count(
		scores.begin(), scores.end(), top_score));

	for (size_t i = 0; i < participants.size(); ++i) {
		auto &participant = participants[i];
		const float actual = MM_PlayerStats_FfaActualScore(
			scores.data(), scores.size(), i);
		const float expected = MM_PlayerStats_FfaExpectedScore(
			ratings.data(), ratings.size(), i);
		const auto update = ratings_allowed
			? MM_PlayerStats_ApplyRating(participant.rating, actual, expected)
			: mm_player_stats_rating_update_t{ participant.rating, 0 };

		mm_player_stats_outcome_t outcome = mm_player_stats_outcome_t::loss;
		if (participant.score == top_score)
			outcome = top_count == 1 ? mm_player_stats_outcome_t::win :
				mm_player_stats_outcome_t::draw;
		SettleParticipant(participant, outcome, update,
			profile_persistence_allowed);
	}
}

void SettleDeparture(gentity_t *ent)
{
	if (!s_match_open || !ent || !ent->client || AlreadySettled(ent))
		return;

	auto participants = CollectParticipants();
	auto *quitter = FindParticipant(participants, ent);
	const bool atomic_duel_departure = quitter &&
		s_match_mode == player_stats_match_mode_t::duel &&
		participants.size() == 2;
	if (!quitter) {
		// Disconnect is also called for never-playing spectators. They have no
		// match result to settle, and claiming the serial here would suppress a
		// legitimate result if the same identity reconnects and joins later.
		if (!IsPlayingTeam(ent->client->sess.team))
			return;
		s_match_has_non_atomic_departure = true;
		player_stats_participant_t participant{
			ent,
			ent->client,
			BoundedPlayerName(ent->client),
			MM_ClientProfileSerializePreferences(ent->client),
			NormalizeClientRating(ent->client),
			ent->client->resp.score,
			ent->client->sess.team,
			IsBot(ent),
			ent->client->sess.profile_persistence_ready
		};
		const mm_player_stats_rating_update_t unchanged{
			participant.rating,
			0
		};
		SettleParticipant(participant,
			mm_player_stats_outcome_t::no_contest, unchanged, false);
		return;
	}
	if (!atomic_duel_departure)
		s_match_has_non_atomic_departure = true;

	bool ratings_allowed = RatingsAllowed(participants);
	if (participants.size() < 2) {
		const mm_player_stats_rating_update_t unchanged{ quitter->rating, 0 };
		SettleParticipant(*quitter,
			mm_player_stats_outcome_t::no_contest, unchanged, false);
		return;
	}

	if (s_match_mode == player_stats_match_mode_t::duel &&
		participants.size() == 2) {
		const bool profile_persistence_allowed =
			ProfileSettlementsCanBeQueued(participants);
		const bool duel_ratings_allowed =
			ratings_allowed && profile_persistence_allowed;
		// Freeze the logical Duel result while the departing player (or reconnect
		// reservation) is still present in the shared score view.
		const bool duel_result_committed =
			MM_Duel_CommitDepartureForfeit(ent);
		if (duel_result_committed) {
			auto *opponent = participants[0].ent == ent ?
				&participants[1] : &participants[0];
			AccumulatePlayTime(opponent->ent, MonotonicMilliseconds(), false);
			const float expected_quitter = MM_PlayerStats_EloExpected(
				quitter->rating, opponent->rating);
			const auto quitter_update = duel_ratings_allowed
				? MM_PlayerStats_ApplyRating(quitter->rating, 0.0f,
					expected_quitter)
				: mm_player_stats_rating_update_t{ quitter->rating, 0 };
			const auto opponent_update = duel_ratings_allowed
				? MM_PlayerStats_ApplyRating(opponent->rating, 1.0f,
					1.0f - expected_quitter)
				: mm_player_stats_rating_update_t{ opponent->rating, 0 };
			SettleParticipant(*quitter,
				mm_player_stats_outcome_t::abandon, quitter_update,
				profile_persistence_allowed);
			SettleParticipant(*opponent,
				mm_player_stats_outcome_t::win, opponent_update,
				profile_persistence_allowed);
			// Do not leave a settled Duel live with one vacant slot: a manual team
			// request can otherwise admit a replacement before the next rules tick.
			// QueueIntermission snapshots the two original players before the caller
			// mutates or releases the departing client state. Its player-stats hook is
			// exact-once and simply closes this already-settled lifecycle.
			QueueIntermission("Duel ended by player departure.", true, false);
			return;
		}

		// A Duel departure that cannot produce the exact two-player forfeit is
		// no longer atomic. Fail the remaining lifecycle closed instead of
		// falling through to an individually rated FFA-style abandonment.
		s_match_has_non_atomic_departure = true;
		ratings_allowed = false;
	}

	// WORR rates Red Rover as an individual free-for-all because team
	// membership changes as part of the mode's core scoring loop.
	if (s_match_mode == player_stats_match_mode_t::teams) {
		float red_total = 0.0f;
		float blue_total = 0.0f;
		size_t red_count = 0;
		size_t blue_count = 0;
		for (const auto &participant : participants) {
			if (participant.team == TEAM_RED) {
				red_total += participant.rating;
				++red_count;
			} else if (participant.team == TEAM_BLUE) {
				blue_total += participant.rating;
				++blue_count;
			}
		}

		if (red_count == 0 || blue_count == 0 ||
			(quitter->team != TEAM_RED && quitter->team != TEAM_BLUE)) {
			const mm_player_stats_rating_update_t unchanged{ quitter->rating, 0 };
			SettleParticipant(*quitter,
				mm_player_stats_outcome_t::no_contest, unchanged, false);
			return;
		}
		const bool profile_persistence_allowed =
			ProfileSettlementCanBeQueued(*quitter);
		const bool departure_ratings_allowed =
			ratings_allowed && profile_persistence_allowed;

		const float red_rating = red_total / static_cast<float>(red_count);
		const float blue_rating = blue_total / static_cast<float>(blue_count);
		const float expected_red = MM_PlayerStats_EloExpected(
			red_rating, blue_rating);
		const float expected = quitter->team == TEAM_RED ?
			expected_red : 1.0f - expected_red;
		const auto update = departure_ratings_allowed
			? MM_PlayerStats_ApplyRating(quitter->rating, 0.0f, expected)
			: mm_player_stats_rating_update_t{ quitter->rating, 0 };
		SettleParticipant(*quitter,
			mm_player_stats_outcome_t::abandon, update,
			profile_persistence_allowed);
		return;
	}

	float expected = 0.0f;
	for (const auto &participant : participants)
		if (participant.ent != ent)
			expected += MM_PlayerStats_EloExpected(
				quitter->rating, participant.rating);
	expected /= static_cast<float>(participants.size() - 1);
	const bool profile_persistence_allowed =
		ProfileSettlementCanBeQueued(*quitter);
	const bool departure_ratings_allowed =
		ratings_allowed && profile_persistence_allowed;
	const auto update = departure_ratings_allowed
		? MM_PlayerStats_ApplyRating(quitter->rating, 0.0f, expected)
		: mm_player_stats_rating_update_t{ quitter->rating, 0 };
	SettleParticipant(*quitter,
		mm_player_stats_outcome_t::abandon, update,
		profile_persistence_allowed);
}

} // namespace

uint32_t MM_PlayerStats_CurrentMatchSerial() noexcept
{
	return s_match_serial;
}

bool MM_PlayerStats_IsMatchOpen() noexcept
{
	return s_match_open;
}

void MM_PlayerStats_RunFrame()
{
	s_profile_io_attempts_since_run_frame = 0;
	if (!s_pending_profile_settlements.empty())
		RetryPendingProfileSettlements(k_profile_retry_attempts_per_frame);
}

void MM_PlayerStats_Shutdown()
{
	if (s_pending_profile_settlements.empty())
		return;
	RetryPendingProfileSettlements(
		k_profile_retry_attempts_on_shutdown, true, true);
	if (!s_pending_profile_settlements.empty()) {
		gi.Com_PrintFmt(
			"MM_PlayerStats: {} profile settlement{} could not be persisted before shutdown.\n",
			s_pending_profile_settlements.size(),
			s_pending_profile_settlements.size() == 1 ? "" : "s");
	}
}

void MM_PlayerStats_OnProfileLoaded(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;
	EnsureRuntimeOwner(ent);
	NormalizeClientRating(ent->client);
	ApplyRecordedSettlement(ent->client, RecoveryGametypeName());
}

void MM_PlayerStats_OnMatchStart()
{
	// Arena Rooms are simultaneous independent series and do not have a single
	// global result that this rating lifecycle can settle safely.
	if (GT(GT_ARENA)) {
		s_match_open = false;
		s_match_gametype.clear();
		s_match_has_bot_participant = false;
		s_match_has_unready_human_participant = false;
		s_match_has_non_atomic_departure = false;
		return;
	}
	PruneSettledPlayerHistory();

	if (++s_match_serial == 0)
		++s_match_serial;
	s_match_gametype.assign(EffectiveGametypeName());
	s_match_mode = GT(GT_DUEL)
		? player_stats_match_mode_t::duel
		: (Teams() && notGT(GT_RR)
			? player_stats_match_mode_t::teams
			: player_stats_match_mode_t::ffa);
	s_match_has_bot_participant = false;
	s_match_has_unready_human_participant = false;
	s_match_has_non_atomic_departure = false;
	s_match_open = true;
	// Keep only identities with pending writes available across map loads so a
	// stale profile read cannot roll back an in-memory rating awaiting retry.
	for (auto &runtime : s_runtime)
		runtime = {};

	const int64_t now = MonotonicMilliseconds();
	for (size_t i = 0; i < game.maxclients; ++i) {
		gentity_t *ent = &g_entities[i + 1];
		if (!ent->client)
			continue;
		ent->client->sess.skill_rating_change = 0;
		ent->client->sess.stats_play_start_ms = 0;
		ent->client->sess.stats_saved_match_outcome =
			static_cast<uint8_t>(mm_player_stats_outcome_t::no_contest);
		ent->client->sess.stats_reconnect_suspended = false;
		NormalizeClientRating(ent->client);
		if (ent->inuse && ent->client->pers.connected &&
			IsPlayingTeam(ent->client->sess.team)) {
			if (IsBot(ent))
				s_match_has_bot_participant = true;
			else if (!ent->client->sess.profile_persistence_ready)
				s_match_has_unready_human_participant = true;
			BeginPlayTime(ent, now);
		}
	}
}

void MM_PlayerStats_OnMatchEnd(bool no_contest)
{
	if (!s_match_open || s_match_serial == 0)
		return;

	auto participants = CollectParticipants();
	AccumulateParticipantTimes(participants, MonotonicMilliseconds());
	if (no_contest) {
		SettleNoContest(participants);
		s_match_open = false;
		return;
	}
	if (participants.empty()) {
		s_match_open = false;
		return;
	}
	if (participants.size() == 1) {
		auto &participant = participants.front();
		const mm_player_stats_rating_update_t unchanged{
			participant.rating,
			0
		};
		SettleParticipant(participant,
			mm_player_stats_outcome_t::win, unchanged,
			ProfileSettlementCanBeQueued(participant));
		s_match_open = false;
		return;
	}

	const bool ratings_allowed = RatingsAllowed(participants);
	if (s_match_mode == player_stats_match_mode_t::duel)
		SettleDuel(participants, ratings_allowed);
	else if (s_match_mode == player_stats_match_mode_t::teams)
		SettleTeams(participants, ratings_allowed);
	else
		SettleFfa(participants, ratings_allowed);

	s_match_open = false;
}

void MM_PlayerStats_OnMatchAbort()
{
	MM_PlayerStats_OnMatchEnd(true);
}

void MM_PlayerStats_OnClientDisconnect(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	MM_PlayerStats_OnClientPause(ent);
	SettleDeparture(ent);
}

void MM_PlayerStats_ReconcileSettledReservation(
	gentity_t *slot, const gclient_t *saved_client)
{
	if (!slot || !slot->client || !saved_client ||
		!slot->client->pers.connected || !s_match_serial ||
		saved_client->sess.stats_saved_match_serial != s_match_serial ||
		!muffmode::CStringEquals(
			slot->client->pers.social_id, saved_client->pers.social_id)) {
		return;
	}

	// Keep connection membership and preferences, but do not let the settled
	// participant result disappear with its inactivity/pending snapshot.
	slot->client->sess.wins = saved_client->sess.wins;
	slot->client->sess.losses = saved_client->sess.losses;
	slot->client->sess.skill_rating = saved_client->sess.skill_rating;
	slot->client->sess.skill_rating_change =
		saved_client->sess.skill_rating_change;
	slot->client->sess.stats_saved_match_serial =
		saved_client->sess.stats_saved_match_serial;
	slot->client->sess.stats_saved_match_outcome =
		saved_client->sess.stats_saved_match_outcome;
	slot->client->sess.stats_reconnect_suspended =
		saved_client->sess.stats_reconnect_suspended;
	slot->client->pers.vote_count = std::max(
		slot->client->pers.vote_count, saved_client->pers.vote_count);
	slot->client->pers.timeout_used =
		slot->client->pers.timeout_used || saved_client->pers.timeout_used;
}

void MM_PlayerStats_OnReservedClientExpired(
	gentity_t *slot, gclient_t *saved_client)
{
	if (!slot || !slot->client || !saved_client || !s_match_open)
		return;
	ApplyRecordedSettlement(saved_client, MatchGametypeName());
	if (saved_client->sess.stats_saved_match_serial == s_match_serial) {
		MM_PlayerStats_ReconcileSettledReservation(slot, saved_client);
		return;
	}

	// A reconnect attempt can already have initialized this slot with its new
	// session. Settle against the reservation copy, then restore that live state.
	const auto live_client = std::make_unique<gclient_t>(*slot->client);
	*slot->client = *saved_client;
	MM_PlayerStats_OnClientDisconnect(slot);
	*slot->client = *live_client;
	MM_PlayerStats_ReconcileSettledReservation(slot, saved_client);
}

void MM_PlayerStats_OnClientPause(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	AccumulatePlayTime(ent, MonotonicMilliseconds(), false);
	if (s_match_open && !AlreadySettled(ent) &&
		IsPlayingTeam(ent->client->sess.team))
		ent->client->sess.stats_reconnect_suspended = true;
}

void MM_PlayerStats_OnClientResume(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;
	ApplyRecordedSettlement(ent->client, MatchGametypeName(), true);
	if (!s_match_open || AlreadySettled(ent) ||
		!ent->client->sess.stats_reconnect_suspended ||
		!IsPlayingTeam(ent->client->sess.team))
		return;

	ent->client->sess.stats_reconnect_suspended = false;
	BeginPlayTime(ent, MonotonicMilliseconds());
}

void MM_PlayerStats_OnTeamTransition(
	gentity_t *ent, team_t old_team, team_t new_team)
{
	if (!ent || !ent->client || !s_match_open)
		return;

	const bool was_playing = IsPlayingTeam(old_team);
	const bool will_play = IsPlayingTeam(new_team);
	const int64_t now = MonotonicMilliseconds();
	if (!was_playing && will_play) {
		if (IsBot(ent))
			s_match_has_bot_participant = true;
		else if (!ent->client->sess.profile_persistence_ready)
			s_match_has_unready_human_participant = true;
		if (AlreadySettled(ent)) {
			ent->client->sess.stats_reconnect_suspended = false;
			return;
		}
		BeginPlayTime(ent, now);
		return;
	}
	if (was_playing && !will_play) {
		AccumulatePlayTime(ent, now, false);
		SettleDeparture(ent);
	}
}

float MM_PlayerStats_Rating(const gentity_t *ent) noexcept
{
	if (!ent || !ent->client)
		return DefaultRating();
	const float rating = ent->client->sess.skill_rating;
	return !std::isfinite(rating) || rating < MM_PLAYER_STATS_MIN_RATING
		? DefaultRating()
		: MM_PlayerStats_NormalizeRating(rating);
}

bool MM_PlayerStats_SessionIsRanked(const gentity_t *ent) noexcept
{
	if (!ent || !ent->client)
		return false;
	if (!muffmode::CvarEnabled(g_ranked))
		return false;
	if (GT(GT_ARENA))
		return false;
	if (IsBot(ent) || ent->client->sess.is_a_bot)
		return false;

	return ent->client->sess.profile_persistence_ready;
}

int32_t MM_PlayerStats_RatingChange(const gentity_t *ent) noexcept
{
	return ent && ent->client ? ent->client->sess.skill_rating_change : 0;
}

std::optional<mm_player_stats_outcome_t> MM_PlayerStats_CurrentOutcome(
	const gentity_t *ent) noexcept
{
	if (!ent || !ent->client || s_match_serial == 0)
		return std::nullopt;

	if (ent->client->sess.stats_saved_match_serial == s_match_serial) {
		const auto outcome = static_cast<mm_player_stats_outcome_t>(
			ent->client->sess.stats_saved_match_outcome);
		switch (outcome) {
		case mm_player_stats_outcome_t::no_contest:
		case mm_player_stats_outcome_t::win:
		case mm_player_stats_outcome_t::loss:
		case mm_player_stats_outcome_t::draw:
		case mm_player_stats_outcome_t::abandon:
			return outcome;
		}
		return std::nullopt;
	}

	if (!HasSocialId(ent))
		return std::nullopt;
	const auto identity = s_settled_players.find(ent->client->pers.social_id);
	if (identity == s_settled_players.end())
		return std::nullopt;
	const auto settled = identity->second.find(
		std::string(MatchGametypeName()));
	if (settled == identity->second.end() ||
		settled->second.serial != s_match_serial)
		return std::nullopt;
	return settled->second.outcome;
}

const char *MM_PlayerStats_OutcomeName(mm_player_stats_outcome_t outcome) noexcept
{
	switch (outcome) {
	case mm_player_stats_outcome_t::no_contest:
		return "no contest";
	case mm_player_stats_outcome_t::win:
		return "win";
	case mm_player_stats_outcome_t::loss:
		return "loss";
	case mm_player_stats_outcome_t::draw:
		return "draw";
	case mm_player_stats_outcome_t::abandon:
		return "abandon";
	}
	return "unknown";
}

void MM_CmdSkillRating(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;
	if (CheckFlood(ent))
		return;
	if (!muffmode::CvarEnabled(g_ranked)) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"This server is unranked; skill ratings and match statistics are disabled.\n");
		return;
	}

	if (GT(GT_ARENA)) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"Arena Rooms are unranked; live room statistics remain available in Player Stats.\n");
		return;
	}

	int64_t total = 0;
	size_t count = 0;
	for (auto player : active_players()) {
		if (!player || !player->client || IsBot(player) ||
			!player->client->sess.profile_persistence_ready)
			continue;
		total += MM_PlayerStats_DisplayRating(MM_PlayerStats_Rating(player));
		++count;
	}

	const int32_t average = count
		? static_cast<int32_t>(total / static_cast<int64_t>(count))
		: 0;
	if (!ent->client->sess.profile_persistence_ready) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"Persistent profile unavailable; this session is unranked.\n");
	}
	gi.LocClient_Print(ent, PRINT_HIGH,
		"{} skill rating: {} ({:+d}); active-player average: {}.\n",
		level.gametype_name,
		MM_PlayerStats_DisplayRating(MM_PlayerStats_Rating(ent)),
		MM_PlayerStats_RatingChange(ent),
		average);
}
