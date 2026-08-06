// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_freezetag.h"
#include "muffmode/mm_ghost.h"
#include "muffmode/mm_match.h"
#include "muffmode/mm_match_stats.h"
#include "muffmode/mm_message_budget.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_pconfig.h"
#include "muffmode/mm_player_stats.h"
#include "muffmode/mm_skin.h"
#include "muffmode/mm_spawn_rules.h"
#include "muffmode/mm_util.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

extern cvar_t *g_auto_ghost_max;
extern cvar_t *g_auto_ghost_time;
extern cvar_t *g_auto_ghost_timeout;

namespace muffmode::ghost {
constexpr int GHOST_CODE_MIN = 10000;
constexpr int GHOST_CODE_MAX_EXCLUSIVE = 100000;
constexpr int GHOST_CODE_MAX = GHOST_CODE_MAX_EXCLUSIVE - 1;
constexpr int AUTO_GHOST_DEFAULT_REJOIN_SECONDS = 120;
constexpr int AUTO_GHOST_MAX_REJOIN_SECONDS = 3600;
constexpr int AUTO_GHOST_DEFAULT_MAX_RESERVATIONS = 3;
constexpr float AUTO_GHOST_PLACEHOLDER_ALPHA = 0.25f;
constexpr gtime_t AUTO_GHOST_FADE_OUT_TIME = 700_ms;
constexpr gtime_t AUTO_GHOST_FADE_IN_TIME = 500_ms;
constexpr gtime_t AUTO_GHOST_REINSTATE_DELAY = 3_sec;
constexpr gtime_t AUTO_GHOST_PLACEMENT_RETRY_TIME = 3_sec;
constexpr gtime_t AUTO_GHOST_EXPIRE_PULSE_TIME = 700_ms;
} // namespace muffmode::ghost

static_assert(MM_GHOST_MAX_CLIENT_CAPACITY == MAX_LOBBY_PLAYERS);
static_assert(MM_GHOST_PLAYER_SKIN_CONFIGSTRING_VALUE_BYTES == CS_MAX_STRING_LENGTH);

namespace muffmode::ghost::snapshot {

enum class AutoGhostPhase : uint8_t {
	Reserved,
	Reinstating,
	Expiring
};

struct SavedProfileAuthority {
	float rating = MM_PLAYER_STATS_DEFAULT_RATING;
	int32_t rating_change = 0;
	uint32_t stats_serial = 0;
	uint8_t stats_outcome = 0;
	bool stats_suspended = false;
	bool persistence_ready = false;
	client_config_t config{};
	std::array<item_id_t, IT_TOTAL> weapon_prefs{};
	uint8_t weapon_pref_count = 0;
};

struct EntityLifeSnapshot {
	entity_state_t s{};
	svflags_t svflags = SVF_NONE;
	vec3_t mins{};
	vec3_t maxs{};
	vec3_t velocity{};
	vec3_t avelocity{};
	vec3_t gravityVector{ 0.0f, 0.0f, -1.0f };
	solid_t solid = SOLID_NOT;
	contents_t clipmask = CONTENTS_NONE;
	movetype_t movetype = MOVETYPE_NONE;
	ent_flags_t flags = FL_NONE;
	gtime_t timestamp;
	gtime_t air_finished;
	gtime_t powerarmor_time;
	gtime_t teleport_time;
	water_level_t waterlevel = WATER_NONE;
	contents_t watertype = CONTENTS_NONE;
	int32_t mass = 0;
	float gravity = 1.0f;
	int32_t health = 0;
	int32_t max_health = 0;
	int32_t gib_health = 0;
	int32_t viewheight = 0;
	int32_t dmg = 0;
	bool deadflag = false;
	bool takedamage = false;
	int32_t groundentity_number = -1;
	int32_t groundentity_spawn_count = 0;
	int32_t groundentity_linkcount = 0;
};

struct AutoGhostSnapshot {
	bool valid = false;
	char social_id[MAX_INFO_VALUE]{};
	std::string match_id;
	uint32_t round_epoch = 0;
	uint32_t world_epoch = 0;
	gtime_t remaining;
	gclient_t client{};
	EntityLifeSnapshot entity{};
	AutoGhostPhase phase = AutoGhostPhase::Reserved;
	gtime_t phase_elapsed = 0_ms;
	gtime_t reinstate_remaining = 0_ms;
	int32_t pending_entnum = -1;
	int32_t pending_spawn_count = 0;
	bool pending_manual = false;
	bool inventory_released = false;
};

static void ResetAutoGhostSnapshot(AutoGhostSnapshot &snapshot)
{
	static const AutoGhostSnapshot empty_snapshot;
	snapshot = empty_snapshot;
}

std::array<AutoGhostSnapshot, MM_GHOST_MAX_CLIENT_CAPACITY> auto_ghosts;

struct ReconnectClientState {
	char userinfo[MAX_INFO_STRING]{};
	char social_id[MAX_INFO_VALUE]{};
	client_config_t config{};
	std::array<item_id_t, IT_TOTAL> weapon_prefs{};
	uint8_t weapon_pref_count = 0;
	bool profile_persistence_ready = false;
	bool admin = false;
	int32_t ping = 0;
	gtime_t inactivity_time;
	bool inactivity_warning = false;
	int voted = 0;
	vec3_t cmd_angles{};
	gtime_t flood_locktill;
	std::array<gtime_t, 10> flood_when{};
	int32_t flood_whenhead = 0;
	gtime_t last_banned_message_time;
	gtime_t follow_msg_time;
};

static_assert(q_countof(gclient_t::flood_when) ==
	std::tuple_size_v<decltype(ReconnectClientState::flood_when)>);

struct RestorePlacement {
	bool fallback = false;
	vec3_t origin{};
	vec3_t angles{};
};

mm_ghost_skin_sync_scheduler_t<MM_GHOST_MAX_CLIENT_CAPACITY> deferred_skin_sync;
std::array<bool, MM_GHOST_MAX_CLIENT_CAPACITY> deferred_abort_spawns{};

struct DeferredProfileReconciliation {
	bool valid = false;
	int32_t spawn_count = 0;
	uint32_t match_serial = 0;
	char social_id[MAX_INFO_VALUE]{};
	SavedProfileAuthority reconnect_profile{};
	SavedProfileAuthority installed_profile{};
};

std::array<DeferredProfileReconciliation, MM_GHOST_MAX_CLIENT_CAPACITY>
	deferred_profile_reconciliations{};
size_t pending_restore_commit_cursor = 0;

struct GhostDiagnostics {
	uint64_t capture_attempts = 0;
	uint64_t capture_successes = 0;
	uint64_t capture_rejected_client_state = 0;
	uint64_t capture_rejected_intermission = 0;
	uint64_t capture_rejected_social_id = 0;
	uint64_t capture_rejected_match_id = 0;
	uint64_t capture_rejected_disabled = 0;
	uint64_t capture_rejected_capacity = 0;
	uint64_t capture_rejected_no_slot = 0;
	uint64_t restore_attempts = 0;
	uint64_t restore_successes = 0;
	uint64_t restore_retries = 0;
	uint64_t restore_invalid = 0;
	uint64_t restore_aborts = 0;
	uint64_t restore_cancellations = 0;
	uint64_t saved_placements = 0;
	uint64_t fallback_placements = 0;
	uint64_t skin_queues_queued = 0;
	uint64_t skin_actions_attempted = 0;
	uint64_t skin_messages_emitted = 0;
	uint64_t skin_actions_without_message = 0;
	size_t peak_active_skin_queues = 0;
};

GhostDiagnostics diagnostics{};

void IncrementDiagnostic(uint64_t &value)
{
	if (value != std::numeric_limits<uint64_t>::max())
		value++;
}

void ClearTransientClientReferences(gclient_t &client)
{
	client.resp.ghost = nullptr;
	client.oldgroundentity = nullptr;
	client.follow_queued_target = nullptr;
	client.follow_queued_time = 0_ms;
	client.follow_target = nullptr;
	client.follow_update = false;
	client.owned_sphere = nullptr;
	client.owned_sphere_generation = 0;
	client.tracker_pain_time = 0_ms;
	client.inmenu = false;
	client.menu = nullptr;
	client.menudirty = false;
	client.grapple_ent = nullptr;
	client.grapple_state = GRAPPLE_STATE_FLY;
	client.trail_head = nullptr;
	client.trail_tail = nullptr;
	client.landmark_free_fall = false;
	client.landmark_name = nullptr;
	client.landmark_rel_pos = {};
	client.sight_entity = nullptr;
	client.sound_entity = nullptr;
	client.sound2_entity = nullptr;
	client.num_lag_origins = 0;
	client.next_lag_origin = 0;
	client.is_lag_compensated = false;
	client.lag_restore = {};
}

size_t GhostSlotCapacity()
{
	const size_t configured = game.maxclients
		? static_cast<size_t>(game.maxclients)
		: auto_ghosts.size();
	return std::min(configured, auto_ghosts.size());
}

int AutoGhostDurationSeconds()
{
	if (!g_auto_ghost_time)
		return muffmode::ghost::AUTO_GHOST_DEFAULT_REJOIN_SECONDS;

	return clamp(g_auto_ghost_time->integer, 0, muffmode::ghost::AUTO_GHOST_MAX_REJOIN_SECONDS);
}

int AutoGhostMaxReservations()
{
	const int capacity = static_cast<int>(GhostSlotCapacity());
	if (!g_auto_ghost_max)
		return std::min(muffmode::ghost::AUTO_GHOST_DEFAULT_MAX_RESERVATIONS, capacity);

	return clamp(g_auto_ghost_max->integer, 0, capacity);
}

bool IsSlotIgnored(gentity_t *slot, gentity_t **ignore, size_t num_ignore)
{
	for (size_t i = 0; i < num_ignore; i++)
		if (slot == ignore[i])
			return true;

	return false;
}

int GhostIndex(const ghost_t *ghost)
{
	if (!ghost)
		return -1;

	const ptrdiff_t index = ghost - level.ghosts;
	if (index < 0 || index >= static_cast<ptrdiff_t>(auto_ghosts.size()))
		return -1;

	return static_cast<int>(index);
}

bool HasUsableSocialId(const char *social_id)
{
	return social_id && social_id[0];
}

bool SnapshotBelongsToCurrentMatchId(const AutoGhostSnapshot &snapshot)
{
	return MM_GhostSessionBelongsToMatch(snapshot.valid,
		!snapshot.match_id.empty() && snapshot.match_id == level.match_id);
}

bool SnapshotBelongsToCurrentWorld(const AutoGhostSnapshot &snapshot)
{
	return SnapshotBelongsToCurrentMatchId(snapshot) &&
		MM_GhostSnapshotBelongsToWorld(
			true,
			snapshot.world_epoch,
			level.world_epoch);
}

bool SnapshotMatchesCurrentMatch(const AutoGhostSnapshot &snapshot)
{
	return SnapshotBelongsToCurrentWorld(snapshot) &&
		MM_GhostRestoreEpochMatches(snapshot.round_epoch, level.round_epoch) &&
		(snapshot.remaining > 0_ms ||
			snapshot.phase == AutoGhostPhase::Reinstating ||
			snapshot.phase == AutoGhostPhase::Expiring) &&
		level.match_state == match_state_t::MATCH_IN_PROGRESS &&
		!level.intermission_time &&
		!level.intermission_queued;
}

int ActiveSnapshotCount(int except_index = -1)
{
	int count = 0;

	for (size_t i = 0; i < GhostSlotCapacity(); i++) {
		if (static_cast<int>(i) == except_index)
			continue;
		if (SnapshotMatchesCurrentMatch(auto_ghosts[i]))
			count++;
	}

	return count;
}

bool SnapshotOwnsSocialId(const AutoGhostSnapshot &snapshot, const char *social_id)
{
	return snapshot.valid && HasUsableSocialId(social_id) &&
		muffmode::CStringEquals(snapshot.social_id, social_id);
}

bool SnapshotMatchesSocialId(const AutoGhostSnapshot &snapshot, const char *social_id)
{
	return SnapshotMatchesCurrentMatch(snapshot) &&
		snapshot.phase != AutoGhostPhase::Expiring &&
		SnapshotOwnsSocialId(snapshot, social_id);
}

bool IsClientSlot(gentity_t *ent)
{
	return ent && ent >= g_entities + 1 && ent < g_entities + game.maxclients + 1;
}

bool DeferredAbortSpawnIsPending(gentity_t *ent)
{
	if (!IsClientSlot(ent))
		return false;

	const size_t client_index = static_cast<size_t>(ent - g_entities - 1);
	return client_index < deferred_abort_spawns.size() &&
		deferred_abort_spawns[client_index];
}

void SetDeferredAbortSpawnPending(gentity_t *ent, bool pending)
{
	if (!IsClientSlot(ent))
		return;

	const size_t client_index = static_cast<size_t>(ent - g_entities - 1);
	if (client_index < deferred_abort_spawns.size())
		deferred_abort_spawns[client_index] = pending;
}

bool IsAutoGhostPlaceholder(const gentity_t *ent)
{
	return ent && ent->client &&
		(muffmode::CStringEquals(ent->classname, "auto_ghost") ||
			muffmode::CStringEquals(ent->classname, "auto_ghost_reinstating") ||
			muffmode::CStringEquals(ent->classname, "auto_ghost_expiring"));
}

float TimeFraction(gtime_t elapsed, gtime_t duration)
{
	if (duration <= 0_ms)
		return 1.0f;

	return clamp(elapsed.seconds() / duration.seconds(), 0.0f, 1.0f);
}

void SetPlaceholderOpacity(gentity_t *ent, float alpha)
{
	if (!ent)
		return;

	alpha = clamp(alpha, 0.0f, 1.0f);
	ent->s.alpha = alpha;
	ent->s.renderfx |= RF_IR_VISIBLE;
	if (alpha < 0.999f)
		ent->s.renderfx |= RF_TRANSLUCENT;
	else
		ent->s.renderfx &= ~RF_TRANSLUCENT;
}

void CancelDeferredSkinSync(gentity_t *ent)
{
	if (!IsClientSlot(ent))
		return;

	const size_t client_index = static_cast<size_t>(ent - g_entities - 1);
	MM_GhostCancelSkinSync(deferred_skin_sync, client_index);
}

void RemovePlaceholder(size_t index)
{
	if (index >= auto_ghosts.size())
		return;

	gentity_t *ent = level.ghosts[index].ent;
	if (!IsClientSlot(ent))
		return;
	if (!ent->client || ent->client->pers.connected)
		return;
	if (!auto_ghosts[index].valid && !IsAutoGhostPlaceholder(ent))
		return;

	if (ent->linked)
		gi.unlinkentity(ent);

	CancelDeferredSkinSync(ent);
	MM_FreezeTag_ClearClient(ent);

	ent->s.modelindex = 0;
	ent->s.modelindex2 = 0;
	ent->s.modelindex3 = 0;
	ent->s.modelindex4 = 0;
	ent->s.effects = EF_NONE;
	ent->s.event = EV_NONE;
	ent->s.sound = 0;
	ent->s.loop_attenuation = 0;
	ent->s.loop_volume = 0;
	ent->s.renderfx = RF_NONE;
	ent->s.alpha = 0.0f;
	ent->solid = SOLID_NOT;
	ent->clipmask = CONTENTS_NONE;
	// Client slots do not pass through G_FreeEntity. End this placeholder
	// lifetime explicitly so delayed callbacks cannot bind to a later user.
	ent->spawn_count = MM_NextEntityGeneration(ent->spawn_count);
	ent->inuse = false;
	ent->sv.init = false;
	ent->classname = "disconnected";
	ent->client->resp.ghost = nullptr;
	ent->client->pers.connected = false;
	ent->client->pers.spawned = false;
	ent->client->pers.ingame = false;
	ent->client->awaiting_respawn = false;
	ent->client->respawn_timeout = 0_ms;
	ent->timestamp = level.time + 1_sec;
}

void ReleaseExpiredMatchItems(const AutoGhostSnapshot &snapshot);
void AbortPendingRestore(size_t index, const char *reason,
	bool release_match_items = true, bool resume_auto_timeout = true);

void ClearSnapshot(size_t index, bool release_match_items = false)
{
	if (index >= auto_ghosts.size())
		return;

	if (release_match_items)
		ReleaseExpiredMatchItems(auto_ghosts[index]);

	RemovePlaceholder(index);
	ResetAutoGhostSnapshot(auto_ghosts[index]);
}

void SettleSnapshotDeparture(size_t index)
{
	if (index >= auto_ghosts.size())
		return;

	auto &snapshot = auto_ghosts[index];
	if (!snapshot.valid || !SnapshotBelongsToCurrentMatchId(snapshot))
		return;

	gentity_t *slot = level.ghosts[index].ent;
	if (IsClientSlot(slot) && slot->client)
		MM_PlayerStats_OnReservedClientExpired(slot, &snapshot.client);
}

void ReleaseExpiredMatchItems(const AutoGhostSnapshot &snapshot)
{
	if (!snapshot.valid || snapshot.inventory_released)
		return;

	if (snapshot.client.pers.inventory[IT_FLAG_RED])
		CTF_ResetTeamFlag(TEAM_RED);
	if (snapshot.client.pers.inventory[IT_FLAG_BLUE])
		CTF_ResetTeamFlag(TEAM_BLUE);

	// Reset-each-wave Horde has already removed every old-wave Tech and will
	// spawn the configured fresh set at wave start. Mid-wave expiry still returns
	// exactly the snapshot-owned copy.
	const bool horde_wave_owns_tech_reset = MM_GhostHordeWaveOwnsTechReset(
		GT(GT_HORDE),
		g_horde_tech_reset_each_wave && g_horde_tech_reset_each_wave->integer,
		snapshot.round_epoch,
		level.round_epoch);
	if (!horde_wave_owns_tech_reset)
		for (const item_id_t tech_id : tech_ids)
			if (snapshot.client.pers.inventory[tech_id])
				Tech_ReturnToWorld(tech_id, snapshot.entity.s.origin,
					snapshot.client.tech_expire_time);
}

bool SnapshotCarriesFlag(const AutoGhostSnapshot &snapshot)
{
	return snapshot.valid &&
		(snapshot.client.pers.inventory[IT_FLAG_RED] ||
			snapshot.client.pers.inventory[IT_FLAG_BLUE]);
}

void ClearSnapshotFlagState(AutoGhostSnapshot &snapshot)
{
	snapshot.client.pers.inventory[IT_FLAG_RED] = 0;
	snapshot.client.pers.inventory[IT_FLAG_BLUE] = 0;
	snapshot.client.pers.team_state.flag_pickup_time = 0_ms;
	snapshot.client.resp.ctf_flagsince = 0_ms;
	snapshot.entity.s.effects &= ~(EF_FLAG_RED | EF_FLAG_BLUE);
}

void ClearRestoringClientMatchItems(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	gclient_t *client = ent->client;
	client->pers.inventory[IT_FLAG_RED] = 0;
	client->pers.inventory[IT_FLAG_BLUE] = 0;
	client->pers.team_state.flag_pickup_time = 0_ms;
	client->resp.ctf_flagsince = 0_ms;
	for (const item_id_t tech_id : tech_ids)
		client->pers.inventory[tech_id] = 0;
	client->tech_regen_time = 0_ms;
	client->tech_sound_time = 0_ms;
	client->tech_last_message_time = 0_ms;
	client->tech_expire_time = 0_ms;
	ent->s.effects &= ~(EF_FLAG_RED | EF_FLAG_BLUE);
}

void DropSnapshotFlag(size_t index, item_id_t flag_id)
{
	if (index >= auto_ghosts.size())
		return;

	auto &snapshot = auto_ghosts[index];
	if (!snapshot.valid || !snapshot.client.pers.inventory[flag_id])
		return;

	gentity_t *ent = level.ghosts[index].ent;
	if (GTF(GTF_CTF) && ent && ent->client) {
		ent->client->pers.inventory[IT_FLAG_RED] = flag_id == IT_FLAG_RED ? snapshot.client.pers.inventory[IT_FLAG_RED] : 0;
		ent->client->pers.inventory[IT_FLAG_BLUE] = flag_id == IT_FLAG_BLUE ? snapshot.client.pers.inventory[IT_FLAG_BLUE] : 0;
		ent->client->pers.team_state.flag_pickup_time = snapshot.client.pers.team_state.flag_pickup_time;
		ent->client->resp.ctf_flagsince = snapshot.client.resp.ctf_flagsince;
		CTF_DeadDropFlag(ent);
		snapshot.client.pers.match = ent->client->pers.match;
		MM_MatchStats_ClientEnd(ent);
	} else {
		CTF_ResetTeamFlag(flag_id == IT_FLAG_RED ? TEAM_RED : TEAM_BLUE);
	}

	snapshot.client.pers.inventory[flag_id] = 0;
}

void DropSnapshotFlags(size_t index)
{
	if (index >= auto_ghosts.size() || !SnapshotCarriesFlag(auto_ghosts[index]))
		return;

	DropSnapshotFlag(index, IT_FLAG_RED);
	DropSnapshotFlag(index, IT_FLAG_BLUE);
	ClearSnapshotFlagState(auto_ghosts[index]);
}

void ExpireSnapshot(size_t index)
{
	if (index >= auto_ghosts.size() || !auto_ghosts[index].valid)
		return;
	if (auto_ghosts[index].phase == AutoGhostPhase::Reinstating) {
		AbortPendingRestore(index, nullptr, true);
		return;
	}

	SettleSnapshotDeparture(index);
	level.ghosts[index].code = 0;
	ClearSnapshot(index, true);
}

void DiscardStaleSnapshot(size_t index)
{
	if (index >= auto_ghosts.size() || !auto_ghosts[index].valid)
		return;
	if (auto_ghosts[index].phase == AutoGhostPhase::Reinstating) {
		AbortPendingRestore(index, nullptr, false);
		return;
	}

	// A round/world reload has already recreated flags and Techs. Releasing an
	// older snapshot into that new world would reset current-round item state.
	SettleSnapshotDeparture(index);
	level.ghosts[index].code = 0;
	ClearSnapshot(index, false);
}

void ExpireSnapshotsForSocialId(const char *social_id, int except_index = -1)
{
	if (!HasUsableSocialId(social_id))
		return;

	for (size_t i = 0; i < GhostSlotCapacity(); i++) {
		if (static_cast<int>(i) == except_index)
			continue;
		if (!SnapshotOwnsSocialId(auto_ghosts[i], social_id))
			continue;
		if (SnapshotBelongsToCurrentWorld(auto_ghosts[i]))
			ExpireSnapshot(i);
		else
			DiscardStaleSnapshot(i);
	}
}

int FindSnapshotForSocialId(const char *social_id)
{
	int match = -1;

	for (size_t i = 0; i < GhostSlotCapacity(); i++) {
		if (!SnapshotMatchesSocialId(auto_ghosts[i], social_id))
			continue;

		if (match >= 0)
			return -1;

		match = static_cast<int>(i);
	}

	return match;
}

void GenerateGhostCode(size_t ghost)
{
	for (;;) {
		level.ghosts[ghost].code = irandom(muffmode::ghost::GHOST_CODE_MIN, muffmode::ghost::GHOST_CODE_MAX_EXCLUSIVE);

		bool duplicate = false;
		for (size_t i = 0; i < GhostSlotCapacity(); i++) {
			if (i != ghost && level.ghosts[i].code == level.ghosts[ghost].code) {
				duplicate = true;
				break;
			}
		}

		if (!duplicate)
			return;
	}
}

ghost_t *AllocateGhostSlot(gentity_t *ent)
{
	if (!ent || !ent->client)
		return nullptr;

	if (int index = GhostIndex(ent->client->resp.ghost); index >= 0)
		return &level.ghosts[index];

	for (size_t i = 0; i < GhostSlotCapacity(); i++) {
		if (!level.ghosts[i].ent && !auto_ghosts[i].valid)
			return &level.ghosts[i];
	}

	for (size_t i = 0; i < GhostSlotCapacity(); i++) {
		if (!level.ghosts[i].code && !auto_ghosts[i].valid)
			return &level.ghosts[i];
	}

	return nullptr;
}

EntityLifeSnapshot CaptureEntityLife(gentity_t *ent)
{
	EntityLifeSnapshot snapshot{};

	snapshot.s = ent->s;
	snapshot.svflags = ent->svflags;
	snapshot.mins = ent->mins;
	snapshot.maxs = ent->maxs;
	snapshot.velocity = ent->velocity;
	snapshot.avelocity = ent->avelocity;
	snapshot.gravityVector = ent->gravityVector;
	snapshot.solid = ent->solid;
	snapshot.clipmask = ent->clipmask;
	snapshot.movetype = ent->movetype;
	snapshot.flags = ent->flags;
	snapshot.timestamp = ent->timestamp;
	snapshot.air_finished = ent->air_finished;
	snapshot.powerarmor_time = ent->powerarmor_time;
	snapshot.teleport_time = ent->teleport_time;
	snapshot.waterlevel = ent->waterlevel;
	snapshot.watertype = ent->watertype;
	snapshot.mass = ent->mass;
	snapshot.gravity = ent->gravity;
	snapshot.health = ent->health;
	snapshot.max_health = ent->max_health;
	snapshot.gib_health = ent->gib_health;
	snapshot.viewheight = ent->viewheight;
	snapshot.dmg = ent->dmg;
	snapshot.deadflag = ent->deadflag;
	snapshot.takedamage = ent->takedamage;

	if (ent->groundentity) {
		snapshot.groundentity_number = static_cast<int32_t>(ent->groundentity - g_entities);
		snapshot.groundentity_spawn_count = ent->groundentity->spawn_count;
		snapshot.groundentity_linkcount = ent->groundentity_linkcount;
	}

	return snapshot;
}

gentity_t *ResolveSnapshotEntity(int32_t number, int32_t spawn_count)
{
	if (number < 0 || number >= static_cast<int32_t>(globals.num_entities))
		return nullptr;

	gentity_t *ent = &g_entities[number];
	if (!ent->inuse || ent->spawn_count != spawn_count)
		return nullptr;

	return ent;
}

void RestoreEntityLife(gentity_t *ent, const EntityLifeSnapshot &snapshot)
{
	if (ent->linked)
		gi.unlinkentity(ent);

	ent->s = snapshot.s;
	ent->s.number = static_cast<int32_t>(ent - g_entities);
	ent->svflags = (snapshot.svflags | SVF_PLAYER) & ~SVF_NOCLIENT;
	ent->mins = snapshot.mins;
	ent->maxs = snapshot.maxs;
	ent->velocity = snapshot.velocity;
	ent->avelocity = snapshot.avelocity;
	ent->gravityVector = snapshot.gravityVector;
	ent->solid = snapshot.solid;
	ent->clipmask = snapshot.clipmask;
	ent->movetype = snapshot.movetype;
	ent->flags = snapshot.flags;
	ent->timestamp = snapshot.timestamp;
	ent->air_finished = snapshot.air_finished;
	ent->powerarmor_time = snapshot.powerarmor_time;
	ent->teleport_time = snapshot.teleport_time;
	ent->waterlevel = snapshot.waterlevel;
	ent->watertype = snapshot.watertype;
	ent->mass = snapshot.mass;
	ent->gravity = snapshot.gravity;
	ent->health = snapshot.health;
	ent->max_health = snapshot.max_health;
	ent->gib_health = snapshot.gib_health;
	ent->viewheight = snapshot.viewheight;
	ent->dmg = snapshot.dmg;
	ent->deadflag = snapshot.deadflag;
	ent->takedamage = snapshot.takedamage;
	ent->groundentity = ResolveSnapshotEntity(snapshot.groundentity_number, snapshot.groundentity_spawn_count);
	ent->groundentity_linkcount = ent->groundentity ? snapshot.groundentity_linkcount : 0;

	ent->client = &game.clients[ent - g_entities - 1];
	ent->inuse = true;
	ent->classname = "player";
	ent->model = "players/male/tris.md2";
	ent->prethink = {};
	ent->postthink = {};
	ent->think = {};
	ent->touch = {};
	ent->use = {};
	ent->pain = {};
	ent->die = player_die;
	ent->nextthink = 0_ms;
	ent->owner = nullptr;
	ent->enemy = nullptr;
	ent->oldenemy = nullptr;
	ent->chain = nullptr;
	ent->activator = nullptr;
	ent->mynoise = nullptr;
	ent->mynoise2 = nullptr;

	if (ClientIsPlaying(ent->client) && !ent->deadflag && ent->health > 0)
		ent->svflags &= ~SVF_DEADMONSTER;
}

void SanitizeRestoredClient(gentity_t *ent, int ghost_index)
{
	gclient_t *client = ent->client;

	client->pers.connected = true;
	client->pers.spawned = true;
	client->pers.ingame = true;
	client->pers.health = ent->health;
	client->pers.max_health = ent->max_health;
	client->pers.saved_flags = ent->flags & (FL_FLASHLIGHT | FL_GODMODE | FL_NOTARGET | FL_POWER_ARMOR | FL_WANTS_POWER_ARMOR);
	client->sess.is_a_bot = false;
	client->sess.admin = false;
	client->resp.ghost = &level.ghosts[ghost_index];

	client->showscores = false;
	client->showinventory = false;
	client->showhelp = false;
	client->resp.voted = false;
	client->buttons = BUTTON_NONE;
	client->oldbuttons = BUTTON_NONE;
	client->latched_buttons = BUTTON_NONE;
	client->cmd = {};

	client->inmenu = false;
	client->menu = nullptr;
	client->menudirty = false;
	client->follow_queued_target = nullptr;
	client->follow_queued_time = 0_ms;
	// ClearTransientClientReferences already detached the saved target. Do not use
	// SetFollowTarget here: its presentation refresh would emit an immediate burst
	// of per-viewer skin configstrings before the bounded post-restore queue.
	client->follow_target = nullptr;
	client->follow_update = false;
	client->owned_sphere = nullptr;
	client->owned_sphere_generation = 0;
	client->grapple_ent = nullptr;
	client->grapple_state = GRAPPLE_STATE_FLY;
	client->tracker_pain_time = 0_ms;
	client->trail_head = nullptr;
	client->trail_tail = nullptr;
	client->oldgroundentity = nullptr;
	client->sight_entity = nullptr;
	client->sound_entity = nullptr;
	client->sound2_entity = nullptr;
	G_ClearLagCompensationHistory(ent);
	client->awaiting_respawn = false;
	client->respawn_timeout = 0_ms;
	client->initial_menu_delay = 0_ms;
	client->initial_menu_shown = true;
	client->initial_menu_closure = false;

	client->ps.pmove.origin = ent->s.origin;
	client->ps.pmove.velocity = ent->velocity;
	if (!client->ps.pmove.viewheight)
		client->ps.pmove.viewheight = static_cast<int8_t>(clamp(ent->viewheight,
			static_cast<int32_t>(std::numeric_limits<int8_t>::min()),
			static_cast<int32_t>(std::numeric_limits<int8_t>::max())));

	AngleVectors(client->v_angle, client->v_forward, nullptr, nullptr);
}

ReconnectClientState CaptureReconnectClientState(const gclient_t &client)
{
	ReconnectClientState state{};
	muffmode::CopyString(state.userinfo, client.pers.userinfo);
	muffmode::CopyString(state.social_id, client.pers.social_id);
	state.config = client.sess.pc;
	state.weapon_prefs = client.sess.weapon_prefs;
	state.weapon_pref_count = client.sess.weapon_pref_count;
	state.profile_persistence_ready =
		client.sess.profile_persistence_ready;
	state.admin = client.sess.admin;
	state.ping = client.ping;
	state.inactivity_time = client.sess.inactivity_time;
	state.inactivity_warning = client.sess.inactivity_warning;
	state.voted = client.pers.voted;
	state.cmd_angles = client.resp.cmd_angles;
	state.flood_locktill = client.flood_locktill;
	std::copy(std::begin(client.flood_when), std::end(client.flood_when),
		state.flood_when.begin());
	state.flood_whenhead = client.flood_whenhead;
	state.last_banned_message_time = client.last_banned_message_time;
	state.follow_msg_time = client.follow_msg_time;
	return state;
}

void ApplyReconnectClientState(gentity_t *ent, const ReconnectClientState &state,
	bool preserve_snapshot_profile_state = false)
{
	gclient_t *client = ent->client;
	// A successful reconnect load owns the latest preferences. If that load
	// failed, keep a trusted reservation's hydrated snapshot instead of
	// replacing it with the new connection's defaults. Rating readiness remains
	// the old match's authority until that match lifecycle closes.
	if (!preserve_snapshot_profile_state || state.profile_persistence_ready) {
		client->sess.pc = state.config;
		client->sess.weapon_prefs = state.weapon_prefs;
		client->sess.weapon_pref_count = state.weapon_pref_count;
	}
	if (!preserve_snapshot_profile_state) {
		client->sess.profile_persistence_ready =
			state.profile_persistence_ready;
	}
	client->sess.admin = MM_GhostRestoreAdminState(state.admin, ent == &g_entities[1]);
	client->sess.is_a_bot = false;
	client->ping = state.ping;
	client->sess.inactivity_time = state.inactivity_time;
	client->sess.inactivity_warning = state.inactivity_warning;
	client->pers.voted = state.voted;
	client->resp.cmd_angles = state.cmd_angles;
	client->flood_locktill = state.flood_locktill;
	std::copy(state.flood_when.begin(), state.flood_when.end(),
		std::begin(client->flood_when));
	client->flood_whenhead = state.flood_whenhead;
	client->last_banned_message_time = state.last_banned_message_time;
	client->follow_msg_time = state.follow_msg_time;

	// Re-parse the new connection's identity and presentation preferences after
	// gameplay state is restored. The snapshot must never rename or reconfigure
	// the user who owns this connection.
	ClientUserinfoChangedForRestore(ent, state.userinfo);
	muffmode::CopyString(client->pers.social_id, state.social_id);
	client->ps.pmove.delta_angles = client->ps.viewangles - client->resp.cmd_angles;
	client->old_pmove = client->ps.pmove;
	AngleVectors(client->v_angle, client->v_forward, nullptr, nullptr);
}

void ReapplyRestoredFollowLeader(gentity_t *ent)
{
	if (!ent || !ent->client || !ent->client->sess.pc.follow_leader ||
		!ClientIsPlaying(ent->client) || !ent->client->eliminated) {
		return;
	}

	// Clearing cross-connection references also clears the queued camera target.
	// Applying the unchanged value rebuilds only that local queue; skin updates
	// remain on the bounded deferred reconnect lane.
	MM_PConfigSetBool(ent, mm_pconfig_bool_setting_t::follow_leader, true);
}

bool FindRestorePlacement(gentity_t *ent, const EntityLifeSnapshot &snapshot,
	RestorePlacement &placement)
{
	placement.origin = snapshot.s.origin;
	placement.angles = snapshot.s.angles;

	const bool saved_position_unsafe = snapshot.solid != SOLID_NOT &&
		G_UnsafeSpawnPosition(snapshot.s.origin, true, ent, false);
	if (MM_GhostRestorePlacementStrategy(
			saved_position_unsafe, false, false) ==
		mm_ghost_restore_placement_t::SavedPosition)
		return true;

	// The world kept moving while the reservation was non-solid. Never turn a
	// congested old location into a mass telefrag; choose an ordinary clear spawn
	// under the restored team/elimination rules and wait if none is available.
	bool landmark = false;
	const bool fallback_spawn_available =
		SelectSpawnPoint(ent, placement.origin, placement.angles, false, landmark);
	if (!fallback_spawn_available)
		return false;

	const bool fallback_spawn_unsafe =
		G_UnsafeSpawnPosition(placement.origin, true, ent, false);
	if (MM_GhostRestorePlacementStrategy(
			saved_position_unsafe,
			fallback_spawn_available,
			fallback_spawn_unsafe) !=
		mm_ghost_restore_placement_t::FallbackSpawn)
		return false;

	placement.fallback = true;
	return true;
}

void ApplyRestorePlacement(gentity_t *ent, const RestorePlacement &placement)
{
	if (!placement.fallback)
		return;

	gclient_t *client = ent->client;
	client->spawn_origin = placement.origin;
	client->ps.pmove.origin = placement.origin;
	client->ps.pmove.velocity = {};
	client->ps.viewangles = placement.angles;
	client->v_angle = placement.angles;
	client->oldviewangles = placement.angles;

	ent->s.origin = placement.origin;
	ent->s.origin[2] += 1.0f;
	ent->s.old_origin = ent->s.origin;
	ent->s.angles = placement.angles;
	ent->velocity = {};
	ent->avelocity = {};
	ent->groundentity = nullptr;
	ent->groundentity_linkcount = 0;
}

void QueueDeferredSkinSync(gentity_t *restored)
{
	if (!IsClientSlot(restored))
		return;

	const size_t client_index = static_cast<size_t>(restored - g_entities - 1);
	if (MM_GhostQueueSkinSync(deferred_skin_sync, client_index,
			restored->spawn_count, level.round_epoch, level.world_epoch)) {
		IncrementDiagnostic(diagnostics.skin_queues_queued);
		diagnostics.peak_active_skin_queues = std::max(
			diagnostics.peak_active_skin_queues,
			MM_GhostActiveSkinSyncQueueCount(deferred_skin_sync));
	}
}

std::array<mm_ghost_skin_sync_slot_t, MM_GHOST_MAX_CLIENT_CAPACITY>
CaptureDeferredSkinSyncSlots()
{
	std::array<mm_ghost_skin_sync_slot_t, MM_GHOST_MAX_CLIENT_CAPACITY> slots{};
	const size_t capacity = GhostSlotCapacity();
	for (size_t i = 0; i < capacity; i++) {
		gentity_t *ent = &g_entities[i + 1];
		slots[i] = {
			ent->inuse && ent->client,
			ent->client && ent->client->pers.connected,
			ent->client && ent->client->pers.spawned,
			ent->spawn_count
		};
	}
	return slots;
}

void RunDeferredSkinSyncs(bool presentation_allowed)
{
	const size_t capacity = GhostSlotCapacity();
	const auto slots = CaptureDeferredSkinSyncSlots();
	const mm_ghost_skin_sync_context_t context{
		capacity,
		level.round_epoch,
		level.world_epoch,
		presentation_allowed
	};

	if (!presentation_allowed || !capacity) {
		MM_GhostStepSkinSync(deferred_skin_sync, slots, context);
		return;
	}
	if (!MM_GhostActiveSkinSyncQueueCount(deferred_skin_sync))
		return;

	mm_reliable_fanout_scope_t fanout_budget(
		MM_GHOST_POST_RESTORE_SKIN_MESSAGES_PER_FRAME,
		MM_GHOST_POST_RESTORE_SKIN_MESSAGES_PER_FRAME *
			MM_GHOST_MAX_SKIN_CONFIGSTRING_MESSAGE_BYTES,
		"post-restore skin synchronization");

	size_t sent = 0;
	for (size_t attempted = 0;
		attempted < MM_GHOST_MAX_SKIN_SYNC_ACTIONS_PER_DRAIN &&
			sent < MM_GHOST_POST_RESTORE_SKIN_MESSAGES_PER_FRAME;
		attempted++) {
		const mm_ghost_skin_sync_step_t step =
			MM_GhostStepSkinSync(deferred_skin_sync, slots, context);
		if (step.action == mm_ghost_skin_sync_action_t::None)
			break;
		IncrementDiagnostic(diagnostics.skin_actions_attempted);

		bool emitted = false;
		if (step.action == mm_ghost_skin_sync_action_t::PublishCanonical) {
			if (step.restored_index < capacity)
				emitted = MM_PublishCanonicalPlayerSkin(
					&g_entities[step.restored_index + 1]);
		} else if (step.action == mm_ghost_skin_sync_action_t::ReapplyOverride &&
			step.pair.valid && step.pair.viewer_index < capacity &&
			step.pair.target_index < capacity) {
			emitted = MM_ReapplySkinOverride(
				&g_entities[step.pair.viewer_index + 1],
				&g_entities[step.pair.target_index + 1]);
		}

		if (emitted) {
			IncrementDiagnostic(diagnostics.skin_messages_emitted);
			sent++;
		} else {
			IncrementDiagnostic(diagnostics.skin_actions_without_message);
		}
	}
}

struct SavedSessionMembership {
	team_t team{};
	bool duel_queued{};
	int wins{};
	int losses{};
	gtime_t team_join_time{};
	int32_t score{};
	int32_t old_score{};
	int32_t round_start_score{};
	int32_t round_dmg{};
};

SavedSessionMembership CaptureSavedSessionMembership(const gclient_t &source)
{
	return {
		source.sess.team,
		source.sess.duel_queued,
		source.sess.wins,
		source.sess.losses,
		source.sess.team_join_time,
		source.resp.score,
		source.resp.old_score,
		source.resp.round_start_score,
		source.resp.round_dmg
	};
}

SavedProfileAuthority CaptureSavedProfileAuthority(const gclient_t &source)
{
	return {
		source.sess.skill_rating,
		source.sess.skill_rating_change,
		source.sess.stats_saved_match_serial,
		source.sess.stats_saved_match_outcome,
		source.sess.stats_reconnect_suspended,
		source.sess.profile_persistence_ready,
		source.sess.pc,
		source.sess.weapon_prefs,
		source.sess.weapon_pref_count
	};
}

void ApplySavedProfileAuthority(
	gclient_t &destination, const SavedProfileAuthority &saved)
{
	const bool reconnect_profile_ready =
		destination.sess.profile_persistence_ready;
	destination.sess.skill_rating = saved.rating;
	destination.sess.skill_rating_change = saved.rating_change;
	destination.sess.stats_saved_match_serial = saved.stats_serial;
	destination.sess.stats_saved_match_outcome = saved.stats_outcome;
	destination.sess.stats_reconnect_suspended = saved.stats_suspended;
	destination.sess.profile_persistence_ready = saved.persistence_ready;
	if (!reconnect_profile_ready) {
		destination.sess.pc = saved.config;
		destination.sess.weapon_prefs = saved.weapon_prefs;
		destination.sess.weapon_pref_count = saved.weapon_pref_count;
	}
}

client_config_t MergePostRestoreConfig(
	const SavedProfileAuthority &reconnect,
	const SavedProfileAuthority &installed,
	const gclient_t &current)
{
	client_config_t merged = reconnect.config;
	const client_config_t &live = current.sess.pc;
	const client_config_t &baseline = installed.config;
	if (live.show_id != baseline.show_id)
		merged.show_id = live.show_id;
	if (live.show_timer != baseline.show_timer)
		merged.show_timer = live.show_timer;
	if (live.show_match_info != baseline.show_match_info)
		merged.show_match_info = live.show_match_info;
	if (live.show_fragmessages != baseline.show_fragmessages)
		merged.show_fragmessages = live.show_fragmessages;
	if (live.killbeep_num != baseline.killbeep_num)
		merged.killbeep_num = live.killbeep_num;
	if (live.follow_killer != baseline.follow_killer)
		merged.follow_killer = live.follow_killer;
	if (live.follow_leader != baseline.follow_leader)
		merged.follow_leader = live.follow_leader;
	if (live.follow_powerup != baseline.follow_powerup)
		merged.follow_powerup = live.follow_powerup;
	if (live.follow_first_person != baseline.follow_first_person)
		merged.follow_first_person = live.follow_first_person;
	if (live.announcer_enabled != baseline.announcer_enabled)
		merged.announcer_enabled = live.announcer_enabled;
	if (!std::equal(std::begin(live.enemy_skin), std::end(live.enemy_skin),
			std::begin(baseline.enemy_skin))) {
		muffmode::CopyString(merged.enemy_skin, live.enemy_skin);
	}
	if (!std::equal(std::begin(live.team_skin), std::end(live.team_skin),
			std::begin(baseline.team_skin))) {
		muffmode::CopyString(merged.team_skin, live.team_skin);
	}
	return merged;
}

size_t DeferredProfileIndex(const gentity_t *ent)
{
	if (!ent || ent <= g_entities)
		return deferred_profile_reconciliations.size();
	const ptrdiff_t index = ent - g_entities - 1;
	return index >= 0 &&
		index < static_cast<ptrdiff_t>(deferred_profile_reconciliations.size())
		? static_cast<size_t>(index)
		: deferred_profile_reconciliations.size();
}

void ClearDeferredProfileReconciliation(gentity_t *ent)
{
	const size_t index = DeferredProfileIndex(ent);
	if (index < deferred_profile_reconciliations.size())
		deferred_profile_reconciliations[index] = {};
}

void QueueDeferredProfileReconciliation(
	gentity_t *ent, const SavedProfileAuthority &reconnect_profile)
{
	const size_t index = DeferredProfileIndex(ent);
	if (index >= deferred_profile_reconciliations.size() || !ent->client ||
		!ent->client->pers.social_id[0]) {
		return;
	}

	auto &pending = deferred_profile_reconciliations[index];
	pending = {};
	pending.valid = true;
	pending.spawn_count = ent->spawn_count;
	pending.match_serial = MM_PlayerStats_CurrentMatchSerial();
	muffmode::CopyString(pending.social_id, ent->client->pers.social_id);
	pending.reconnect_profile = reconnect_profile;
	pending.installed_profile = CaptureSavedProfileAuthority(*ent->client);
}

void ReconcileDeferredProfiles()
{
	for (size_t i = 0; i < deferred_profile_reconciliations.size(); ++i) {
		auto pending = deferred_profile_reconciliations[i];
		deferred_profile_reconciliations[i] = {};
		if (!pending.valid || i >= static_cast<size_t>(game.maxclients))
			continue;

		gentity_t *ent = &g_entities[i + 1];
		if (!ent->client || !ent->client->pers.connected ||
			ent->spawn_count != pending.spawn_count ||
			pending.match_serial != MM_PlayerStats_CurrentMatchSerial() ||
			std::string_view(ent->client->pers.social_id) != pending.social_id) {
			continue;
		}

		const SavedProfileAuthority &preference_base =
			MM_GhostReconnectPreferencesUseInstalledProfile(
				pending.reconnect_profile.persistence_ready)
			? pending.installed_profile
			: pending.reconnect_profile;
		SavedProfileAuthority reconciled = pending.reconnect_profile;
		reconciled.config = MergePostRestoreConfig(
			preference_base, pending.installed_profile, *ent->client);
		reconciled.weapon_prefs = preference_base.weapon_prefs;
		reconciled.weapon_pref_count = preference_base.weapon_pref_count;
		const bool weapon_preferences_changed =
			ent->client->sess.weapon_pref_count !=
				pending.installed_profile.weapon_pref_count ||
			!std::equal(ent->client->sess.weapon_prefs.begin(),
				ent->client->sess.weapon_prefs.end(),
				pending.installed_profile.weapon_prefs.begin());
		if (weapon_preferences_changed) {
			reconciled.weapon_prefs = ent->client->sess.weapon_prefs;
			reconciled.weapon_pref_count =
				ent->client->sess.weapon_pref_count;
		}

		ent->client->sess.skill_rating = reconciled.rating;
		ent->client->sess.skill_rating_change = reconciled.rating_change;
		ent->client->sess.stats_saved_match_serial = reconciled.stats_serial;
		ent->client->sess.stats_saved_match_outcome = reconciled.stats_outcome;
		ent->client->sess.stats_reconnect_suspended =
			reconciled.stats_suspended;
		ent->client->sess.profile_persistence_ready =
			reconciled.persistence_ready;
		ent->client->sess.pc = reconciled.config;
		ent->client->sess.weapon_prefs = reconciled.weapon_prefs;
		ent->client->sess.weapon_pref_count = reconciled.weapon_pref_count;
		MM_PlayerStats_OnClientResume(ent);
	}
}

void RestoreSavedSessionMembership(
	client_session_t &destination, const SavedSessionMembership *saved,
	const mm_ghost_session_membership_policy_t &policy)
{
	if (!saved || !policy.reapply_saved_membership)
		return;

	// Membership belongs to the authenticated reservation. Preferences,
	// authority, inactivity and the remaining connection-owned fields stay live.
	// Follow state is deliberately normalized because its client-slot reference
	// is not valid across reconnects or world rebuilds.
	destination.team = saved->team;
	if (policy.clear_follow_target) {
		destination.spectator_state = policy.use_free_spectator
			? SPECTATOR_FREE : SPECTATOR_NOT;
		destination.spectator_client = 0;
	}
	destination.duel_queued = saved->duel_queued;
	destination.wins = saved->wins;
	destination.losses = saved->losses;
	destination.team_join_time = saved->team_join_time;
}

void RestoreSavedRespawnScoring(
	client_respawn_t &destination, const SavedSessionMembership *saved,
	const mm_ghost_session_membership_policy_t &policy)
{
	if (!saved || !policy.reapply_saved_membership)
		return;

	destination.score = saved->score;
	destination.old_score = saved->old_score;
	destination.round_start_score = saved->round_start_score;
	destination.round_dmg = saved->round_dmg;
}

void RestartPendingRestoreTarget(gentity_t *target, const char *reason,
	bool resume_auto_timeout, const SavedSessionMembership *saved_membership = nullptr,
	bool stage_for_world_reset = false)
{
	if (!target || !target->client || !target->client->pers.connected)
		return;

	gclient_t *client = target->client;
	const ReconnectClientState connection = CaptureReconnectClientState(*client);
	const mm_ghost_session_membership_policy_t membership_policy =
		MM_GhostSessionMembershipPolicy(saved_membership != nullptr,
			saved_membership && saved_membership->team == TEAM_SPECTATOR);
	client_session_t desired_session = client->sess;
	RestoreSavedSessionMembership(desired_session, saved_membership,
		membership_policy);
	client_session_t restart_session = desired_session;
	const bool restart_as_spectator =
		!membership_policy.reapply_saved_membership || stage_for_world_reset;
	if (restart_as_spectator) {
		// A stale/cross-match snapshot grants no membership. Normalize before the
		// ordinary spawn path; a pending world reset likewise keeps the invalidated
		// reconnect non-playing until the rebuilt world can spawn it safely.
		restart_session.team = TEAM_SPECTATOR;
		restart_session.spectator_state = SPECTATOR_FREE;
		restart_session.spectator_client = 0;
		restart_session.duel_queued = false;
	}

	CancelDeferredSkinSync(target);
	SetDeferredAbortSpawnPending(target, false);
	if (client->menu)
		P_Menu_Close(target);
	Weapon_Grapple_DoReset(client);
	ClearRestoringClientMatchItems(target);

	// A reserved slot still contains the disconnected player's full gameplay
	// payload. Start the abort from a clean client so Horde cannot mistake an old
	// weapon/lives payload for an intentional between-wave rejoin. Only the new
	// connection's session, identity, preferences, authentication and controls are
	// overlaid before and after the ordinary spawn path.
	memset(client, 0, sizeof(*client));
	client->sess = restart_session;
	client->pers.connected = true;
	client->pers.spawned = false;
	client->pers.ingame = true;
	ApplyReconnectClientState(target, connection);
	target->client->resp.ghost = nullptr;

	ClientRestartAfterGhostRestoreAbort(target);
	ApplyReconnectClientState(target, connection);
	// World-reset aborts use a spectator only while the old world is still live.
	// Reapply only authenticated, same-match membership afterward; stale-match
	// fallbacks must keep the ordinary restart's normalized spectator result.
	if (membership_policy.reapply_saved_membership) {
		RestoreSavedSessionMembership(target->client->sess, saved_membership,
			membership_policy);
		RestoreSavedRespawnScoring(target->client->resp, saved_membership,
			membership_policy);
		P_PublishEngineTeam(target);
		CalculateRanks();
	}
	target->client->pers.connected = true;
	target->client->pers.ingame = true;
	if (target->client->pers.spawned)
		QueueDeferredSkinSync(target);
	else
		SetDeferredAbortSpawnPending(target, true);

	if (resume_auto_timeout && level.timeout_auto && !level.timeout_resuming &&
		level.timeout_in_place > 0_ms && level.timeout_ent == target)
		::TimeoutEnd();
	if (reason)
		gi.LocClient_Print(target, PRINT_HIGH, "{}\n", reason);
}

void ExpireStaleSnapshotsForSocialId(const char *social_id)
{
	if (!HasUsableSocialId(social_id))
		return;

	for (size_t i = 0; i < GhostSlotCapacity(); i++)
		if (SnapshotOwnsSocialId(auto_ghosts[i], social_id) &&
			!SnapshotMatchesCurrentMatch(auto_ghosts[i])) {
			if (SnapshotBelongsToCurrentWorld(auto_ghosts[i]))
				ExpireSnapshot(i);
			else
				DiscardStaleSnapshot(i);
		}
}

void UpdateGhostStatsFromEntity(gentity_t *ent, ghost_t &ghost)
{
	ghost.team = ent->client->sess.team;
	ghost.score = ent->client->resp.score;
	ghost.ent = ent;
	ghost.number = static_cast<int>(ent->s.number);
	muffmode::CopyString(ghost.netname, ent->client->resp.netname);
}

bool EntityIsBot(gentity_t *ent)
{
	return ent && ((ent->svflags & SVF_BOT) || (ent->client && ent->client->sess.is_a_bot));
}

bool EntityCanOwnGhost(gentity_t *ent)
{
	return ent &&
		ent->client &&
		deathmatch->integer &&
		level.match_state == match_state_t::MATCH_IN_PROGRESS &&
		ClientIsPlaying(ent->client) &&
		!EntityIsBot(ent);
}

void ClearEntityGhostSlot(gentity_t *ent)
{
	if (!ent || !ent->client) {
		return;
	}

	if (int ghost_index = GhostIndex(ent->client->resp.ghost); ghost_index >= 0) {
		if (auto_ghosts[ghost_index].valid &&
			auto_ghosts[ghost_index].phase == AutoGhostPhase::Reinstating) {
			if (SnapshotBelongsToCurrentWorld(auto_ghosts[ghost_index]))
				ExpireSnapshot(static_cast<size_t>(ghost_index));
			else
				DiscardStaleSnapshot(static_cast<size_t>(ghost_index));
			return;
		}
		ClearSnapshot(static_cast<size_t>(ghost_index));
		level.ghosts[ghost_index] = ghost_t{};
	}

	ent->client->resp.ghost = nullptr;
}

int AutoGhostTimeoutSeconds()
{
	if (!g_auto_ghost_timeout)
		return 0;

	return clamp(g_auto_ghost_timeout->integer, 0, AutoGhostDurationSeconds());
}

bool IsFlagRetentionTimeoutActive()
{
	return level.timeout_in_place > 0_ms && !level.timeout_resuming;
}

gentity_t *PlaceholderEntity(size_t index)
{
	if (index >= auto_ghosts.size())
		return nullptr;

	gentity_t *ent = level.ghosts[index].ent;
	return IsClientSlot(ent) ? ent : nullptr;
}

void UpdateReservedPlaceholderVisual(size_t index)
{
	auto &snapshot = auto_ghosts[index];
	gentity_t *ent = PlaceholderEntity(index);
	if (!IsAutoGhostPlaceholder(ent))
		return;

	ent->classname = "auto_ghost";
	if (IsFlagRetentionTimeoutActive()) {
		snapshot.phase_elapsed = 0_ms;
		SetPlaceholderOpacity(ent, muffmode::ghost::AUTO_GHOST_PLACEHOLDER_ALPHA);
		return;
	}

	snapshot.phase_elapsed += FRAME_TIME_MS;
	const float fade = TimeFraction(snapshot.phase_elapsed, muffmode::ghost::AUTO_GHOST_FADE_OUT_TIME);
	SetPlaceholderOpacity(ent, muffmode::ghost::AUTO_GHOST_PLACEHOLDER_ALPHA * (1.0f - fade));
}

void UpdateReinstatingPlaceholderVisual(size_t index)
{
	auto &snapshot = auto_ghosts[index];
	gentity_t *ent = PlaceholderEntity(index);
	if (!IsAutoGhostPlaceholder(ent))
		return;

	ent->classname = "auto_ghost_reinstating";
	const float fade = TimeFraction(snapshot.phase_elapsed, muffmode::ghost::AUTO_GHOST_FADE_IN_TIME);
	SetPlaceholderOpacity(ent, fade);
}

void UpdateExpiringPlaceholderVisual(size_t index)
{
	auto &snapshot = auto_ghosts[index];
	gentity_t *ent = PlaceholderEntity(index);
	if (!IsAutoGhostPlaceholder(ent))
		return;

	ent->classname = "auto_ghost_expiring";
	const float t = TimeFraction(snapshot.phase_elapsed, muffmode::ghost::AUTO_GHOST_EXPIRE_PULSE_TIME);
	const float pulse = t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;
	SetPlaceholderOpacity(ent, clamp(pulse, 0.0f, 1.0f));
}

void CopySnapshotDropStateToEntity(const AutoGhostSnapshot &snapshot, gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	ent->client->pers.inventory = snapshot.client.pers.inventory;
	ent->client->pers.weapon = snapshot.client.pers.weapon;
	ent->client->pers.lastweapon = snapshot.client.pers.lastweapon;
	ent->client->pers.selected_item = snapshot.client.pers.selected_item;
	ent->client->pers.selected_item_time = snapshot.client.pers.selected_item_time;
	ent->client->pers.power_cubes = snapshot.client.pers.power_cubes;
	ent->client->pu_time_quad = snapshot.client.pu_time_quad;
	ent->client->pu_time_haste = snapshot.client.pu_time_haste;
	ent->client->pu_time_double = snapshot.client.pu_time_double;
	ent->client->pu_time_protection = snapshot.client.pu_time_protection;
	ent->client->pu_time_invisibility = snapshot.client.pu_time_invisibility;
	ent->client->pu_time_regeneration = snapshot.client.pu_time_regeneration;
	ent->client->pers.team_state = snapshot.client.pers.team_state;
	ent->client->resp.ctf_flagsince = snapshot.client.resp.ctf_flagsince;
}

void ReleaseTimedOutInventory(size_t index)
{
	if (index >= auto_ghosts.size())
		return;

	auto &snapshot = auto_ghosts[index];
	if (!snapshot.valid || snapshot.inventory_released)
		return;

	gentity_t *ent = PlaceholderEntity(index);
	if (IsAutoGhostPlaceholder(ent)) {
		CopySnapshotDropStateToEntity(snapshot, ent);
		TossClientItems(ent);
		snapshot.client.pers.match = ent->client->pers.match;
		MM_MatchStats_ClientEnd(ent);

		// Tossing is policy-sensitive: combat-disabled phases can skip everything,
		// and Horde may deliberately keep Techs on death. Treat only inventory that
		// actually left the mirror as released, then return any remaining match
		// items explicitly before destroying the reservation.
		if (!ent->client->pers.inventory[IT_FLAG_RED])
			snapshot.client.pers.inventory[IT_FLAG_RED] = 0;
		if (!ent->client->pers.inventory[IT_FLAG_BLUE])
			snapshot.client.pers.inventory[IT_FLAG_BLUE] = 0;
		for (const item_id_t tech_id : tech_ids)
			if (!ent->client->pers.inventory[tech_id])
				snapshot.client.pers.inventory[tech_id] = 0;
		ReleaseExpiredMatchItems(snapshot);
		ClearRestoringClientMatchItems(ent);
	} else {
		ReleaseExpiredMatchItems(snapshot);
	}

	snapshot.inventory_released = true;
}

void BeginSnapshotTimeoutExpiry(size_t index)
{
	if (index >= auto_ghosts.size())
		return;

	auto &snapshot = auto_ghosts[index];
	if (!snapshot.valid)
		return;

	ReleaseTimedOutInventory(index);
	level.ghosts[index].code = 0;

	gentity_t *ent = PlaceholderEntity(index);
	if (!IsAutoGhostPlaceholder(ent) || IsFlagRetentionTimeoutActive()) {
		ExpireSnapshot(index);
		return;
	}

	snapshot.phase = AutoGhostPhase::Expiring;
	snapshot.phase_elapsed = 0_ms;
	snapshot.reinstate_remaining = 0_ms;
	snapshot.pending_entnum = -1;
	snapshot.pending_spawn_count = 0;
	snapshot.pending_manual = false;
	UpdateExpiringPlaceholderVisual(index);
}

bool StartAutoGhostTimeout(gentity_t *ent)
{
	const int timeout_seconds = AutoGhostTimeoutSeconds();
	if (IsFlagRetentionTimeoutActive())
		return true;
	if (timeout_seconds <= 0 || level.timeout_in_place > 0_ms)
		return false;

	level.timeout_ent = ent;
	level.timeout_auto = true;
	level.timeout_resuming = false;
	level.timeout_in_place = gtime_t::from_sec(timeout_seconds);
	level.countdown_check = 0_sec;

	gi.LocBroadcast_Print(PRINT_CENTER, "{} disconnected.\nWaiting {} for rejoin.",
		ent->client->resp.netname, G_TimeString(timeout_seconds * 1000, false));
	gi.positioned_sound(world->s.origin, world, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX,
		gi.soundindex("world/klaxon2.wav"), 1, ATTN_NONE, 0);
	MM_MatchStats_LogEvent("MATCH TIMEOUT STARTED");
	return true;
}

bool CaptureActivePlayerSnapshot(gentity_t *ent, bool start_auto_timeout)
{
	IncrementDiagnostic(diagnostics.capture_attempts);
	if (!EntityCanOwnGhost(ent)) {
		IncrementDiagnostic(diagnostics.capture_rejected_client_state);
		ClearEntityGhostSlot(ent);
		return false;
	}
	if (level.intermission_time || level.intermission_queued) {
		IncrementDiagnostic(diagnostics.capture_rejected_intermission);
		return false;
	}
	if (!HasUsableSocialId(ent->client->pers.social_id)) {
		IncrementDiagnostic(diagnostics.capture_rejected_social_id);
		return false;
	}
	if (level.match_id.empty()) {
		IncrementDiagnostic(diagnostics.capture_rejected_match_id);
		return false;
	}

	const int ghost_seconds = AutoGhostDurationSeconds();
	if (ghost_seconds <= 0) {
		IncrementDiagnostic(diagnostics.capture_rejected_disabled);
		return false;
	}

	const int existing_ghost_index = GhostIndex(ent->client->resp.ghost);
	ExpireSnapshotsForSocialId(ent->client->pers.social_id, existing_ghost_index);
	const int max_ghosts = AutoGhostMaxReservations();
	if (max_ghosts <= 0) {
		IncrementDiagnostic(diagnostics.capture_rejected_disabled);
		return false;
	}
	if (ActiveSnapshotCount(existing_ghost_index) >= max_ghosts) {
		IncrementDiagnostic(diagnostics.capture_rejected_capacity);
		return false;
	}

	if (!ent->client->resp.ghost)
		MM_Ghost_Assign(ent);

	const int ghost_index = GhostIndex(ent->client->resp.ghost);
	if (ghost_index < 0) {
		IncrementDiagnostic(diagnostics.capture_rejected_no_slot);
		return false;
	}

	ExpireSnapshotsForSocialId(ent->client->pers.social_id, ghost_index);
	if (!level.ghosts[ghost_index].code)
		GenerateGhostCode(static_cast<size_t>(ghost_index));

	auto &snapshot = auto_ghosts[ghost_index];
	ResetAutoGhostSnapshot(snapshot);
	snapshot.valid = true;
	snapshot.match_id = level.match_id;
	snapshot.round_epoch = level.round_epoch;
	snapshot.world_epoch = level.world_epoch;
	snapshot.remaining = gtime_t::from_sec(ghost_seconds);
	muffmode::CopyString(snapshot.social_id, ent->client->pers.social_id);
	snapshot.client = *ent->client;
	// A snapshot owns values, not live entity or TAG_LEVEL references. Keeping
	// those pointers would let a later world reload turn them into aliases of
	// newly spawned map entities before the client is reinstated.
	ClearTransientClientReferences(snapshot.client);
	snapshot.client.pers.health = ent->health;
	snapshot.client.pers.max_health = ent->max_health;
	snapshot.client.pers.saved_flags = ent->flags & (FL_FLASHLIGHT | FL_GODMODE | FL_NOTARGET | FL_POWER_ARMOR | FL_WANTS_POWER_ARMOR);
	snapshot.entity = CaptureEntityLife(ent);

	UpdateGhostStatsFromEntity(ent, level.ghosts[ghost_index]);
	if (start_auto_timeout) {
		if (!StartAutoGhostTimeout(ent))
			DropSnapshotFlags(static_cast<size_t>(ghost_index));
	} else {
		DropSnapshotFlags(static_cast<size_t>(ghost_index));
	}

	IncrementDiagnostic(diagnostics.capture_successes);
	return true;
}

enum class RestoreSnapshotResult : uint8_t {
	Restored,
	Retry,
	Invalid
};

RestoreSnapshotResult RestoreSnapshot(gentity_t *ent, int ghost_index, bool manual)
{
	IncrementDiagnostic(diagnostics.restore_attempts);
	if (!ent || !ent->client || ghost_index < 0 || ghost_index >= static_cast<int>(GhostSlotCapacity())) {
		IncrementDiagnostic(diagnostics.restore_invalid);
		return RestoreSnapshotResult::Invalid;
	}

	auto &snapshot = auto_ghosts[ghost_index];
	// Exact entity state (including Freeze Tag's slot-owned state) is only safe
	// to reinstate into the slot that owns the reservation.
	if (!SnapshotMatchesSocialId(snapshot, ent->client->pers.social_id) ||
		level.ghosts[ghost_index].ent != ent) {
		IncrementDiagnostic(diagnostics.restore_invalid);
		return RestoreSnapshotResult::Invalid;
	}

	const ReconnectClientState reconnect = CaptureReconnectClientState(*ent->client);
	const SavedProfileAuthority reconnect_profile =
		CaptureSavedProfileAuthority(*ent->client);
	const auto current_client = std::make_unique<gclient_t>(*ent->client);
	*ent->client = snapshot.client;
	// Defend older/in-memory snapshots too: pointers and authority never cross
	// the disconnect boundary, even if the snapshot predates this sanitizer.
	ClearTransientClientReferences(*ent->client);
	ent->client->sess.admin = false;

	RestorePlacement placement{};
	if (!FindRestorePlacement(ent, snapshot.entity, placement)) {
		*ent->client = *current_client;
		IncrementDiagnostic(diagnostics.restore_retries);
		return RestoreSnapshotResult::Retry;
	}
	if (placement.fallback)
		IncrementDiagnostic(diagnostics.fallback_placements);
	else
		IncrementDiagnostic(diagnostics.saved_placements);

	RestoreEntityLife(ent, snapshot.entity);
	SanitizeRestoredClient(ent, ghost_index);
	ApplyRestorePlacement(ent, placement);
	ApplyReconnectClientState(ent, reconnect, true);

	P_AssignClientSkinnum(ent);
	P_ForceFogTransition(ent, true, true);
	gi.linkentity(ent);

	level.ghosts[ghost_index].code = 0;
	ClearSnapshot(static_cast<size_t>(ghost_index));
	UpdateGhostStatsFromEntity(ent, level.ghosts[ghost_index]);
	MM_MatchStats_ClientBegin(ent);
	MM_PlayerStats_OnClientResume(ent);
	QueueDeferredProfileReconciliation(ent, reconnect_profile);
	QueueDeferredSkinSync(ent);

	if (level.timeout_auto && !level.timeout_resuming && level.timeout_in_place > 0_ms &&
		level.timeout_ent == ent)
		::TimeoutEnd();

	if (!(ent->svflags & SVF_NOCLIENT)) {
		gi.WriteByte(svc_muzzleflash);
		gi.WriteEntity(ent);
		gi.WriteByte(MZ_LOGIN);
		gi.multicast(ent->s.origin, MULTICAST_PVS, false);
	}

	gi.LocBroadcast_Print(PRINT_HIGH, "{} rejoined and was reinstated.\n", ent->client->resp.netname);
	if (manual)
		gi.LocClient_Print(ent, PRINT_HIGH, "Ghost code accepted, your match state has been reinstated.\n");
	else
		gi.LocClient_Print(ent, PRINT_HIGH, "Your match state has been restored from auto-ghost.\n");

	CalculateRanks();
	ReapplyRestoredFollowLeader(ent);
	IncrementDiagnostic(diagnostics.restore_successes);
	return RestoreSnapshotResult::Restored;
}

bool EntityMatchesPendingRestore(const AutoGhostSnapshot &snapshot, const gentity_t *ent)
{
	return ent &&
		snapshot.phase == AutoGhostPhase::Reinstating &&
		snapshot.pending_entnum == static_cast<int32_t>(ent - g_entities) &&
		snapshot.pending_spawn_count == ent->spawn_count;
}

gentity_t *PendingRestoreTarget(const AutoGhostSnapshot &snapshot)
{
	if (snapshot.phase != AutoGhostPhase::Reinstating ||
			snapshot.pending_entnum <= 0 ||
			snapshot.pending_entnum >= static_cast<int32_t>(globals.num_entities))
		return nullptr;

	gentity_t *ent = &g_entities[snapshot.pending_entnum];
	if (!EntityMatchesPendingRestore(snapshot, ent) || !ent->inuse || !ent->client || !ent->client->pers.connected)
		return nullptr;

	return ent;
}

int PendingRestoreIndexForTarget(gentity_t *ent)
{
	if (!ent)
		return -1;

	for (size_t i = 0; i < GhostSlotCapacity(); i++)
		if (EntityMatchesPendingRestore(auto_ghosts[i], ent))
			return static_cast<int>(i);

	return -1;
}

void PreparePendingRestoreClient(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	gclient_t *client = ent->client;
	client->pers.connected = true;
	// The reconnect is authenticated but not yet a live/spawned viewer. Keeping
	// this false prevents pre-commit skin/config fan-out and gameplay iteration.
	client->pers.spawned = false;
	client->pers.ingame = true;
	client->awaiting_respawn = false;
	client->respawn_timeout = 0_ms;
	client->showscores = false;
	client->showinventory = false;
	client->showhelp = false;
	client->buttons = BUTTON_NONE;
	client->oldbuttons = BUTTON_NONE;
	client->latched_buttons = BUTTON_NONE;
	client->cmd = {};
	client->weapon_thunk = false;
	client->weapon_fire_buffered = false;
	client->ps.pmove.pm_type = PM_FREEZE;
	client->ps.pmove.origin = ent->s.origin;
	client->ps.pmove.velocity = {};
	client->ps.pmove.pm_flags |= PMF_NO_POSITIONAL_PREDICTION;
	client->ps.gunindex = 0;
	client->ps.gunskin = 0;
}

void PrepareReinstatingPlaceholder(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	ent->svflags = (ent->svflags | SVF_PLAYER) & ~SVF_NOCLIENT;
	ent->solid = SOLID_NOT;
	ent->clipmask = CONTENTS_NONE;
	ent->movetype = MOVETYPE_NONE;
	ent->takedamage = false;
	ent->velocity = {};
	ent->avelocity = {};
	ent->groundentity = nullptr;
	ent->groundentity_linkcount = 0;
	ent->s.effects = EF_NONE;
	ent->s.event = EV_NONE;
	ent->s.sound = 0;
	ent->s.loop_attenuation = 0;
	ent->s.loop_volume = 0;
	ent->inuse = true;
	ent->classname = "auto_ghost_reinstating";
	ent->sv.init = false;
	SetPlaceholderOpacity(ent, max(ent->s.alpha, muffmode::ghost::AUTO_GHOST_PLACEHOLDER_ALPHA));
	gi.linkentity(ent);
}

void CancelPendingRestore(size_t index)
{
	if (index >= auto_ghosts.size())
		return;

	auto &snapshot = auto_ghosts[index];
	if (snapshot.phase != AutoGhostPhase::Reinstating)
		return;
	IncrementDiagnostic(diagnostics.restore_cancellations);

	snapshot.phase = AutoGhostPhase::Reserved;
	snapshot.phase_elapsed = 0_ms;
	snapshot.reinstate_remaining = 0_ms;
	snapshot.pending_entnum = -1;
	snapshot.pending_spawn_count = 0;
	snapshot.pending_manual = false;
	if (!level.ghosts[index].code)
		GenerateGhostCode(index);

	gentity_t *placeholder = PlaceholderEntity(index);
	if (IsAutoGhostPlaceholder(placeholder) && !placeholder->client->pers.connected)
		placeholder->classname = "auto_ghost";
}

void AbortPendingRestore(size_t index, const char *reason, bool release_match_items,
	bool resume_auto_timeout)
{
	if (index >= auto_ghosts.size())
		return;

	auto &snapshot = auto_ghosts[index];
	if (snapshot.phase == AutoGhostPhase::Reinstating)
		IncrementDiagnostic(diagnostics.restore_aborts);
	SavedSessionMembership saved_membership{};
	SavedProfileAuthority saved_profile{};
	// A new round/world invalidates exact combat state, but not the authenticated
	// team reservation inside the same match. Preserve that membership so the
	// ordinary abort path cannot strand a reconnect as a spectator in a lock.
	const bool preserve_saved_membership = SnapshotBelongsToCurrentMatchId(snapshot);
	if (preserve_saved_membership) {
		saved_membership = CaptureSavedSessionMembership(snapshot.client);
		saved_profile = CaptureSavedProfileAuthority(snapshot.client);
	}
	const bool preserve_snapshot_profile_authority =
		preserve_saved_membership && MM_PlayerStats_IsMatchOpen();
	gentity_t *target = nullptr;
	if (snapshot.phase == AutoGhostPhase::Reinstating &&
		snapshot.pending_entnum > 0 &&
		snapshot.pending_entnum < static_cast<int32_t>(globals.num_entities)) {
		gentity_t *candidate = &g_entities[snapshot.pending_entnum];
		if (candidate->spawn_count == snapshot.pending_spawn_count &&
			candidate->inuse && candidate->client && candidate->client->pers.connected)
			target = candidate;
	}
	SavedProfileAuthority reconnect_profile{};
	if (target)
		reconnect_profile = CaptureSavedProfileAuthority(*target->client);
	if (release_match_items)
		ReleaseExpiredMatchItems(snapshot);
	if (!target)
		SettleSnapshotDeparture(index);
	level.ghosts[index].code = 0;
	ClearSnapshot(index, false);
	level.ghosts[index] = ghost_t{};

	if (!target)
		return;

	RestartPendingRestoreTarget(target, reason, resume_auto_timeout,
		preserve_saved_membership ? &saved_membership : nullptr);
	if (preserve_saved_membership && target->client &&
		target->client->pers.connected) {
		if (preserve_snapshot_profile_authority) {
			ApplySavedProfileAuthority(*target->client, saved_profile);
			MM_MatchStats_ClientBegin(target);
		}
		MM_PlayerStats_OnClientResume(target);
		if (preserve_snapshot_profile_authority)
			QueueDeferredProfileReconciliation(target, reconnect_profile);
		ReapplyRestoredFollowLeader(target);
	}
}

bool CancelPendingRestoreForTarget(gentity_t *ent, bool &target_is_placeholder)
{
	target_is_placeholder = false;
	const int index = PendingRestoreIndexForTarget(ent);
	if (index < 0)
		return false;

	target_is_placeholder = level.ghosts[index].ent == ent;
	CancelPendingRestore(static_cast<size_t>(index));
	return true;
}

bool BeginRestoreDelay(gentity_t *ent, int ghost_index, bool manual)
{
	if (!ent || !ent->client || ghost_index < 0 || ghost_index >= static_cast<int>(GhostSlotCapacity()))
		return false;

	auto &snapshot = auto_ghosts[ghost_index];
	if (!SnapshotMatchesSocialId(snapshot, ent->client->pers.social_id) ||
		level.ghosts[ghost_index].ent != ent)
		return false;
	if (snapshot.phase == AutoGhostPhase::Reinstating)
		return EntityMatchesPendingRestore(snapshot, ent);
	// A newer connection supersedes any reconnect base retained by an earlier
	// committed restore in this slot.
	ClearDeferredProfileReconciliation(ent);

	snapshot.phase = AutoGhostPhase::Reinstating;
	snapshot.phase_elapsed = 0_ms;
	snapshot.reinstate_remaining = muffmode::ghost::AUTO_GHOST_REINSTATE_DELAY;
	snapshot.pending_entnum = static_cast<int32_t>(ent - g_entities);
	snapshot.pending_spawn_count = ent->spawn_count;
	snapshot.pending_manual = manual;
	level.ghosts[ghost_index].code = 0;
	if (level.timeout_auto && !level.timeout_resuming &&
			level.timeout_in_place > 0_ms &&
			level.timeout_in_place < muffmode::ghost::AUTO_GHOST_REINSTATE_DELAY) {
		level.timeout_in_place = muffmode::ghost::AUTO_GHOST_REINSTATE_DELAY;
		level.countdown_check = 0_sec;
	}

	PreparePendingRestoreClient(ent);
	if (gentity_t *placeholder = PlaceholderEntity(static_cast<size_t>(ghost_index)))
		if (IsAutoGhostPlaceholder(placeholder))
			PrepareReinstatingPlaceholder(placeholder);

	gi.LocClient_Print(ent, PRINT_CENTER, "Reinstating in 3 seconds.");
	return true;
}

bool FinishPendingRestore(size_t index)
{
	if (index >= auto_ghosts.size())
		return false;

	auto &snapshot = auto_ghosts[index];
	if (snapshot.phase != AutoGhostPhase::Reinstating)
		return false;

	gentity_t *target = PendingRestoreTarget(snapshot);
	if (!target) {
		CancelPendingRestore(index);
		return false;
	}

	const bool manual = snapshot.pending_manual;
	switch (RestoreSnapshot(target, static_cast<int>(index), manual)) {
	case RestoreSnapshotResult::Restored:
		return true;
	case RestoreSnapshotResult::Retry:
		// A mover/player still occupies the saved location and no clear normal
		// spawn exists. Remain frozen briefly and retry without telefragging the
		// world; then fall back through the normal spawn/limbo policy.
		if (snapshot.phase_elapsed >= muffmode::ghost::AUTO_GHOST_REINSTATE_DELAY +
			muffmode::ghost::AUTO_GHOST_PLACEMENT_RETRY_TIME) {
			AbortPendingRestore(index,
				"No safe reinstatement position became available; you were returned to normal play.");
			return false;
		}
		snapshot.reinstate_remaining = FRAME_TIME_MS;
		return false;
	case RestoreSnapshotResult::Invalid:
		AbortPendingRestore(index,
			"Your saved match state is no longer safe to reinstate; you were returned to normal play.");
		return false;
	}

	return false;
}

bool RunOneDueRestoreCommit()
{
	const size_t capacity = GhostSlotCapacity();
	std::array<bool, MM_GHOST_MAX_CLIENT_CAPACITY> due{};
	for (size_t i = 0; i < capacity; i++) {
		const AutoGhostSnapshot &snapshot = auto_ghosts[i];
		due[i] = SnapshotMatchesCurrentMatch(snapshot) &&
			snapshot.phase == AutoGhostPhase::Reinstating &&
			snapshot.reinstate_remaining <= 0_ms;
	}

	const size_t index = MM_GhostSelectDueRestoreCommit(
		due, capacity, pending_restore_commit_cursor);
	if (index == MM_GHOST_NO_CLIENT_INDEX)
		return false;

	// An attempt owns this outer server frame even if placement must retry or
	// validation aborts. The fair cursor lets every simultaneously due client
	// receive one attempt before an earlier slot is retried.
	FinishPendingRestore(index);
	return true;
}

bool IsPendingRestoreTarget(gentity_t *ent)
{
	return PendingRestoreIndexForTarget(ent) >= 0;
}

bool FreezePendingRestoreFrame(gentity_t *ent)
{
	if (!IsPendingRestoreTarget(ent) || !ent || !ent->client)
		return false;

	PreparePendingRestoreClient(ent);
	ent->velocity = {};
	ent->avelocity = {};
	ent->client->ps.pmove.pm_type = PM_FREEZE;
	ent->client->ps.pmove.origin = ent->s.origin;
	ent->client->ps.pmove.velocity = {};
	ent->client->ps.gunindex = 0;
	ent->client->ps.gunskin = 0;
	return true;
}

void QuiesceClientForSnapshot(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;
	if (ent->client->menu)
		P_Menu_Close(ent);
	Weapon_Grapple_DoReset(ent->client);
	ent->flags &= ~FL_NO_KNOCKBACK;
}

void ClearSnapshotTechState(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;
	const int ghost_index = GhostIndex(ent->client->resp.ghost);
	if (ghost_index < 0 || ghost_index >= static_cast<int>(auto_ghosts.size()))
		return;

	auto &client = auto_ghosts[ghost_index].client;
	for (const item_id_t tech_id : tech_ids)
		client.pers.inventory[tech_id] = 0;
	client.tech_regen_time = 0_ms;
	client.tech_sound_time = 0_ms;
	client.tech_last_message_time = 0_ms;
	client.tech_expire_time = 0_ms;
}

} // namespace muffmode::ghost::snapshot

namespace ghost_snapshot = muffmode::ghost::snapshot;

bool MM_Ghost_IsAbortSpawnPending(gentity_t *ent)
{
	return ghost_snapshot::DeferredAbortSpawnIsPending(ent);
}

void MM_Ghost_CancelAbortSpawn(gentity_t *ent)
{
	ghost_snapshot::SetDeferredAbortSpawnPending(ent, false);
}

void MM_Ghost_CompleteAbortSpawn(gentity_t *ent)
{
	if (!ghost_snapshot::DeferredAbortSpawnIsPending(ent) || !ent->client ||
		!ent->client->pers.connected || !ent->client->pers.spawned ||
		ent->client->awaiting_respawn)
		return;

	ghost_snapshot::SetDeferredAbortSpawnPending(ent, false);
	ghost_snapshot::QueueDeferredSkinSync(ent);
}

/*
================
MM_Ghost_ClearAll
================
*/
void MM_Ghost_ClearAll(bool restart_pending_clients) {
	struct PendingRestoreRestart {
		gentity_t *target = nullptr;
		ghost_snapshot::SavedSessionMembership saved_membership{};
		ghost_snapshot::SavedProfileAuthority saved_profile{};
		ghost_snapshot::SavedProfileAuthority reconnect_profile{};
	};
	const bool preserve_snapshot_profile_authority =
		MM_PlayerStats_IsMatchOpen();
	std::vector<PendingRestoreRestart> pending_targets;
	if (restart_pending_clients)
		pending_targets.reserve(ghost_snapshot::GhostSlotCapacity());
	for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++) {
		const auto &snapshot = ghost_snapshot::auto_ghosts[i];
		if (snapshot.valid &&
			snapshot.phase == ghost_snapshot::AutoGhostPhase::Reinstating) {
			ghost_snapshot::IncrementDiagnostic(
				ghost_snapshot::diagnostics.restore_aborts);
			if (restart_pending_clients) {
				if (gentity_t *target = ghost_snapshot::PendingRestoreTarget(snapshot))
					pending_targets.push_back({
						target,
						ghost_snapshot::CaptureSavedSessionMembership(snapshot.client),
						ghost_snapshot::CaptureSavedProfileAuthority(snapshot.client),
						ghost_snapshot::CaptureSavedProfileAuthority(*target->client)
					});
			}
		}
	}

	for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++)
		ghost_snapshot::ClearSnapshot(i);
	for (size_t i = ghost_snapshot::GhostSlotCapacity();
		i < ghost_snapshot::auto_ghosts.size(); ++i) {
		ghost_snapshot::ResetAutoGhostSnapshot(ghost_snapshot::auto_ghosts[i]);
	}
	// A map load discards every old marker. A same-map reset must retain a genuine
	// connected spawn retry even when its snapshot was already cleared on an
	// earlier abort; otherwise disconnect could recapture its zeroed limbo state.
	// Pending snapshot targets below clear/recreate their own slot marker.
	for (size_t i = 0; i < ghost_snapshot::deferred_abort_spawns.size(); i++) {
		gclient_t *client = game.clients && i < ghost_snapshot::GhostSlotCapacity()
			? &game.clients[i] : nullptr;
		const bool retain = ghost_snapshot::deferred_abort_spawns[i] && client &&
			MM_GhostAbortMarkerSurvivesSystemClear(restart_pending_clients,
				client->pers.connected, client->pers.spawned,
				client->awaiting_respawn);
		ghost_snapshot::deferred_abort_spawns[i] = retain;
	}

	// An auto-timeout exists only to protect a reservation. A match start/reset
	// removes every reservation and must not carry that pause into the rebuilt
	// world or warmup. Manual timeouts remain under the match controller's policy.
	if (restart_pending_clients && level.timeout_auto) {
		level.timeout_in_place = 0_ms;
		level.timeout_ent = nullptr;
		level.timeout_auto = false;
		level.timeout_resuming = false;
		level.countdown_check = 0_sec;
	}

	// Match start/reset rebuilds the world immediately after this call. Return a
	// connected client that was inside the reinstatement delay to the ordinary
	// spawn path first, without releasing snapshot-owned items into the old world.
	for (auto &pending_target : pending_targets) {
		gentity_t *target = pending_target.target;
		if (!target || !target->client || !target->client->pers.connected)
			continue;
		ghost_snapshot::RestartPendingRestoreTarget(target, nullptr, false,
			&pending_target.saved_membership, true);
		if (!target->client || !target->client->pers.connected)
			continue;
		if (preserve_snapshot_profile_authority) {
			ghost_snapshot::ApplySavedProfileAuthority(
				*target->client, pending_target.saved_profile);
			MM_MatchStats_ClientBegin(target);
		}
		MM_PlayerStats_OnClientResume(target);
		if (preserve_snapshot_profile_authority) {
			ghost_snapshot::QueueDeferredProfileReconciliation(
				target, pending_target.reconnect_profile);
		}
	}
	if (!restart_pending_clients) {
		std::fill(ghost_snapshot::deferred_profile_reconciliations.begin(),
			ghost_snapshot::deferred_profile_reconciliations.end(),
			ghost_snapshot::DeferredProfileReconciliation{});
	} else if (!preserve_snapshot_profile_authority) {
		ghost_snapshot::ReconcileDeferredProfiles();
	}

	// A normal abort restart may assign a fresh code or queue presentation work.
	// Erase completed presentation work, while retaining any new abort marker from
	// a target that still awaits a valid spawn in the rebuilt world.
	MM_GhostResetSkinSync(ghost_snapshot::deferred_skin_sync);
	ghost_snapshot::pending_restore_commit_cursor = 0;
	std::fill(std::begin(level.ghosts), std::end(level.ghosts), ghost_t{});
	if (game.clients) {
		for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++)
			game.clients[i].resp.ghost = nullptr;
	}
}

/*
================
MM_Ghost_ClearClient
================
*/
void MM_Ghost_ClearClient(gentity_t *ent) {
	ghost_snapshot::CancelDeferredSkinSync(ent);
	ghost_snapshot::ClearEntityGhostSlot(ent);
}

/*
================
MM_Ghost_Assign

Assigns the player a ghost slot for match statistics and possible
same-account reconnect recovery.
================
*/
void MM_Ghost_Assign(gentity_t *ent) {
	if (!ent || !ent->client)
		return;
	if (!ghost_snapshot::EntityCanOwnGhost(ent)) {
		ghost_snapshot::ClearEntityGhostSlot(ent);
		return;
	}

	ghost_t *slot = ghost_snapshot::AllocateGhostSlot(ent);
	if (!slot)
		return;

	const int ghost_index = ghost_snapshot::GhostIndex(slot);
	if (ghost_index < 0)
		return;

	ghost_snapshot::ClearSnapshot(static_cast<size_t>(ghost_index));
	*slot = ghost_t{};
	ghost_snapshot::GenerateGhostCode(static_cast<size_t>(ghost_index));
	ent->client->resp.ghost = slot;
	ghost_snapshot::UpdateGhostStatsFromEntity(ent, *slot);
}

/*
================
MM_Ghost_DoAssign
================
*/
void MM_Ghost_DoAssign(gentity_t *ent) {
	if (!ent || !ent->client)
		return;
	if (!ghost_snapshot::EntityCanOwnGhost(ent)) {
		ghost_snapshot::ClearEntityGhostSlot(ent);
		return;
	}

	// assign a ghost code
	if (level.match_state == match_state_t::MATCH_IN_PROGRESS) {
		if (int ghost_index = ghost_snapshot::GhostIndex(ent->client->resp.ghost); ghost_index >= 0) {
			ghost_snapshot::ClearSnapshot(static_cast<size_t>(ghost_index));
			level.ghosts[ghost_index] = ghost_t{};
		}
		ent->client->resp.ghost = nullptr;
		MM_Ghost_Assign(ent);
	}
}

/*
================
MM_Ghost_CaptureInactive

Captures a live player's match state before an inactivity move to spectator.
================
*/
bool MM_Ghost_CaptureInactive(gentity_t *ent) {
	ghost_snapshot::QuiesceClientForSnapshot(ent);
	if (!ghost_snapshot::CaptureActivePlayerSnapshot(ent, false))
		return false;

	// SetTeam immediately drops a held Tech into the world. The reservation must
	// not retain a second copy that could be restored later.
	ghost_snapshot::ClearSnapshotTechState(ent);
	return true;
}

/*
================
MM_Ghost_CaptureDisconnect

Captures a live player's current match state for same-social-ID reconnect.
================
*/
bool MM_Ghost_CaptureDisconnect(gentity_t *ent) {
	if (!ent || !ent->client || !deathmatch->integer)
		return false;

	bool target_is_placeholder = false;
	if (ghost_snapshot::CancelPendingRestoreForTarget(ent, target_is_placeholder))
		return target_is_placeholder;

	ghost_snapshot::CancelDeferredSkinSync(ent);
	ghost_snapshot::QuiesceClientForSnapshot(ent);
	return ghost_snapshot::CaptureActivePlayerSnapshot(ent, true);
}

/*
================
MM_Ghost_MakeDisconnectPlaceholder
================
*/
void MM_Ghost_MakeDisconnectPlaceholder(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	ghost_snapshot::CancelDeferredSkinSync(ent);
	// End the connected entity lifetime before retaining this slot as a
	// reservation. Delayed callbacks from the old connection must fail their
	// generation check even though the placeholder remains inuse.
	ent->spawn_count = MM_NextEntityGeneration(ent->spawn_count);
	ent->svflags = (ent->svflags | SVF_PLAYER) & ~SVF_NOCLIENT;
	ent->solid = SOLID_NOT;
	ent->clipmask = CONTENTS_NONE;
	ent->movetype = MOVETYPE_NONE;
	ent->takedamage = false;
	ent->velocity = {};
	ent->avelocity = {};
	ent->groundentity = nullptr;
	ent->groundentity_linkcount = 0;
	ent->s.effects = EF_NONE;
	ent->s.event = EV_NONE;
	ent->s.sound = 0;
	ent->s.loop_attenuation = 0;
	ent->s.loop_volume = 0;
	ent->s.renderfx = RF_IR_VISIBLE;
	ghost_snapshot::SetPlaceholderOpacity(ent, muffmode::ghost::AUTO_GHOST_PLACEHOLDER_ALPHA);
	ent->inuse = true;
	ent->classname = "auto_ghost";
	ent->client->pers.connected = false;
	ent->client->pers.spawned = false;
	ent->client->buttons = BUTTON_NONE;
	ent->client->oldbuttons = BUTTON_NONE;
	ent->client->latched_buttons = BUTTON_NONE;
	ent->client->cmd = {};
	ent->sv.init = false;

	gi.linkentity(ent);
}

/*
================
MM_Ghost_ChooseReconnectSlot
================
*/
gentity_t *MM_Ghost_ChooseReconnectSlot(const char *social_id, gentity_t **ignore, size_t num_ignore) {
	// Free a same-account reservation from an earlier round before ordinary slot
	// selection. Exact gameplay state never crosses the round epoch boundary.
	ghost_snapshot::ExpireStaleSnapshotsForSocialId(social_id);
	const int ghost_index = ghost_snapshot::FindSnapshotForSocialId(social_id);
	if (ghost_index < 0)
		return nullptr;

	gentity_t *slot = level.ghosts[ghost_index].ent;
	if (!slot || slot < g_entities + 1 || slot >= g_entities + game.maxclients + 1)
		return nullptr;
	if (ghost_snapshot::IsSlotIgnored(slot, ignore, num_ignore))
		return nullptr;
	if (slot->client && slot->client->pers.connected)
		return nullptr;

	return slot;
}

/*
================
MM_Ghost_IsReservedSlot
================
*/
bool MM_Ghost_IsReservedSlot(gentity_t *slot) {
	if (!slot)
		return false;

	for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++)
		if (MM_GhostSnapshotReservesSlot(
				ghost_snapshot::auto_ghosts[i].valid,
				level.ghosts[i].ent == slot))
			return true;

	return false;
}

gclient_t *MM_Ghost_ReservedClientState(gentity_t *slot) {
	if (!slot)
		return nullptr;

	for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++) {
		auto &snapshot = ghost_snapshot::auto_ghosts[i];
		if (snapshot.valid && level.ghosts[i].ent == slot &&
			ghost_snapshot::SnapshotBelongsToCurrentMatchId(snapshot)) {
			return &snapshot.client;
		}
	}

	return nullptr;
}

const gclient_t *MM_Ghost_ReservedClientState(const gentity_t *slot) {
	if (!slot)
		return nullptr;

	for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++) {
		const auto &snapshot = ghost_snapshot::auto_ghosts[i];
		if (snapshot.valid && level.ghosts[i].ent == slot &&
			ghost_snapshot::SnapshotBelongsToCurrentMatchId(snapshot)) {
			return &snapshot.client;
		}
	}

	return nullptr;
}

size_t MM_Ghost_ActivePlayingReservationCount() {
	size_t count = 0;
	for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++) {
		const auto &snapshot = ghost_snapshot::auto_ghosts[i];
		if (ghost_snapshot::SnapshotMatchesCurrentMatch(snapshot) &&
			snapshot.client.sess.team != TEAM_NONE &&
			snapshot.client.sess.team != TEAM_SPECTATOR) {
			++count;
		}
	}
	return count;
}

size_t MM_Ghost_ActiveHumanPlayingReservationCount() {
	size_t count = 0;
	for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++) {
		const auto &snapshot = ghost_snapshot::auto_ghosts[i];
		if (ghost_snapshot::SnapshotMatchesCurrentMatch(snapshot) &&
			snapshot.client.sess.team != TEAM_NONE &&
			snapshot.client.sess.team != TEAM_SPECTATOR &&
			!snapshot.client.sess.is_a_bot &&
			!(snapshot.entity.svflags & SVF_BOT)) {
			++count;
		}
	}
	return count;
}

size_t MM_Ghost_ActivePlayingReservationCountForTeam(team_t team) {
	size_t count = 0;
	for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++) {
		const auto &snapshot = ghost_snapshot::auto_ghosts[i];
		if (ghost_snapshot::SnapshotMatchesCurrentMatch(snapshot) &&
			snapshot.client.sess.team == team) {
			++count;
		}
	}
	return count;
}

bool MM_Ghost_ReservedClientCountsForRound(const gentity_t *slot, team_t team) {
	if (!slot || !slot->client)
		return false;

	for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++) {
		const auto &snapshot = ghost_snapshot::auto_ghosts[i];
		if (level.ghosts[i].ent != slot)
			continue;

		const bool counts = MM_GhostReservedParticipantCountsForRound(
			ghost_snapshot::SnapshotMatchesCurrentMatch(snapshot),
			true,
			!slot->client->pers.connected ||
				snapshot.phase == ghost_snapshot::AutoGhostPhase::Reinstating,
			snapshot.client.sess.team == team,
			snapshot.client.sess.team != TEAM_NONE &&
				snapshot.client.sess.team != TEAM_SPECTATOR,
			snapshot.client.eliminated);
		if (counts)
			return true;
	}

	return false;
}

/*
================
MM_Ghost_IsPendingRestore
================
*/
bool MM_Ghost_IsPendingRestore(gentity_t *ent) {
	return ghost_snapshot::IsPendingRestoreTarget(ent);
}

/*
================
MM_Ghost_RunPendingRestoreFrame
================
*/
bool MM_Ghost_RunPendingRestoreFrame(gentity_t *ent) {
	return ghost_snapshot::FreezePendingRestoreFrame(ent);
}

/*
================
MM_Ghost_EndPendingRestoreFrame
================
*/
bool MM_Ghost_EndPendingRestoreFrame(gentity_t *ent) {
	return ghost_snapshot::FreezePendingRestoreFrame(ent);
}

/*
================
MM_Ghost_ClientThink
================
*/
bool MM_Ghost_ClientThink(gentity_t *ent, const usercmd_t *ucmd) {
	if (!ghost_snapshot::IsPendingRestoreTarget(ent) || !ent || !ent->client)
		return false;

	gclient_t *client = ent->client;
	client->oldbuttons = client->buttons;
	client->buttons = ucmd ? ucmd->buttons : BUTTON_NONE;
	client->latched_buttons = BUTTON_NONE;
	if (ucmd) {
		client->cmd = *ucmd;
		client->resp.cmd_angles[0] = ucmd->angles[0];
		client->resp.cmd_angles[1] = ucmd->angles[1];
		client->resp.cmd_angles[2] = ucmd->angles[2];
	}

	ghost_snapshot::FreezePendingRestoreFrame(ent);
	return true;
}

/*
================
MM_Ghost_HasActiveReservations
================
*/
bool MM_Ghost_HasActiveReservations() {
	for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++)
		// Even a stale snapshot keeps its slot reserved until RunFrame removes it.
		// Keep the frame loop awake long enough to perform that cleanup.
		if (MM_GhostSnapshotNeedsCleanup(ghost_snapshot::auto_ghosts[i].valid))
			return true;

	return false;
}

/*
================
MM_Ghost_TryRestore
================
*/
bool MM_Ghost_TryRestore(gentity_t *ent) {
	if (!ent || !ent->client)
		return false;
	if (ghost_snapshot::EntityIsBot(ent))
		return false;

	const int ghost_index = ghost_snapshot::FindSnapshotForSocialId(ent->client->pers.social_id);
	if (ghost_index < 0) {
		ghost_snapshot::ClearDeferredProfileReconciliation(ent);
		return false;
	}

	return ghost_snapshot::BeginRestoreDelay(ent, ghost_index, false);
}

/*
================
MM_Ghost_DropTimedOutFlags
================
*/
void MM_Ghost_DropTimedOutFlags() {
	for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++) {
		if (!ghost_snapshot::SnapshotMatchesCurrentMatch(ghost_snapshot::auto_ghosts[i]))
			continue;

		ghost_snapshot::DropSnapshotFlags(i);
	}
}

/*
================
MM_Ghost_RunFrame
================
*/
void MM_Ghost_RunFrame() {
	if (MM_GhostShouldDeferSnapshotCleanup(
			level.intermission_queued != 0_ms,
			MM_MatchStats_IsCollecting())) {
		return;
	}

	if (!MM_GhostMayRunRestoreCommit(
			level.match_state == match_state_t::MATCH_IN_PROGRESS,
			level.intermission_time != 0_ms,
			level.intermission_queued != 0_ms)) {
		for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++) {
			auto &snapshot = ghost_snapshot::auto_ghosts[i];
			if (!snapshot.valid)
				continue;
			const bool snapshot_is_current =
				ghost_snapshot::SnapshotBelongsToCurrentWorld(snapshot);
			if (snapshot.phase == ghost_snapshot::AutoGhostPhase::Reinstating)
				ghost_snapshot::AbortPendingRestore(i, nullptr, snapshot_is_current);
			else if (snapshot_is_current)
				ghost_snapshot::ExpireSnapshot(i);
			else
				ghost_snapshot::DiscardStaleSnapshot(i);
		}
		return;
	}

	for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++) {
		auto &snapshot = ghost_snapshot::auto_ghosts[i];
		if (!snapshot.valid)
			continue;

		const bool snapshot_is_current_world =
			ghost_snapshot::SnapshotBelongsToCurrentWorld(snapshot);
		if (!snapshot_is_current_world ||
			!MM_GhostRestoreEpochMatches(snapshot.round_epoch, level.round_epoch)) {
			if (snapshot.phase == ghost_snapshot::AutoGhostPhase::Reinstating)
				ghost_snapshot::AbortPendingRestore(i,
					"The round changed before reinstatement completed; you were returned to normal play.",
					snapshot_is_current_world);
			else if (snapshot_is_current_world)
				ghost_snapshot::ExpireSnapshot(i);
			else
				ghost_snapshot::DiscardStaleSnapshot(i);
			continue;
		}

		if (snapshot.phase == ghost_snapshot::AutoGhostPhase::Reinstating) {
			if (!ghost_snapshot::PendingRestoreTarget(snapshot)) {
				ghost_snapshot::AbortPendingRestore(i, nullptr);
				continue;
			}

			// Once due, freeze the simulated countdown until the outer-server-frame
			// fair scheduler admits this restore. Scheduler delay must not consume the
			// separate safe-placement retry window.
			if (snapshot.reinstate_remaining > 0_ms) {
				snapshot.phase_elapsed += FRAME_TIME_MS;
				snapshot.reinstate_remaining -= FRAME_TIME_MS;
				if (snapshot.reinstate_remaining < 0_ms)
					snapshot.reinstate_remaining = 0_ms;
			}
			ghost_snapshot::UpdateReinstatingPlaceholderVisual(i);
			continue;
		}

		if (snapshot.phase == ghost_snapshot::AutoGhostPhase::Expiring) {
			snapshot.phase_elapsed += FRAME_TIME_MS;
			ghost_snapshot::UpdateExpiringPlaceholderVisual(i);
			if (snapshot.phase_elapsed >= muffmode::ghost::AUTO_GHOST_EXPIRE_PULSE_TIME)
				ghost_snapshot::ExpireSnapshot(i);
			continue;
		}

		if (snapshot.remaining <= 0_ms) {
			ghost_snapshot::BeginSnapshotTimeoutExpiry(i);
			continue;
		}

		ghost_snapshot::UpdateReservedPlaceholderVisual(i);
		snapshot.remaining -= FRAME_TIME_MS;
		if (snapshot.remaining <= 0_ms)
			ghost_snapshot::BeginSnapshotTimeoutExpiry(i);
	}
}

/*
================
MM_Ghost_RunServerFrame

Runs once after all simulated game substeps in an outer engine frame. A frame
admits one due restore attempt or one bounded presentation drain, never both.
Presentation may finish during warmup after a same-map reset; only restore
commits require an active match.
================
*/
void MM_Ghost_RunServerFrame()
{
	const bool restore_commit_allowed = MM_GhostMayRunRestoreCommit(
		level.match_state == match_state_t::MATCH_IN_PROGRESS,
		level.intermission_time != 0_ms,
		level.intermission_queued != 0_ms);
	const bool presentation_allowed = MM_GhostMayRunDeferredPresentation(
		level.intermission_time != 0_ms,
		level.intermission_queued != 0_ms);
	if (!presentation_allowed) {
		ghost_snapshot::RunDeferredSkinSyncs(false);
		return;
	}

	if (restore_commit_allowed && ghost_snapshot::RunOneDueRestoreCommit())
		return;

	ghost_snapshot::RunDeferredSkinSyncs(true);
}

void MM_Ghost_ReportDiagnostics(bool reset_after)
{
	size_t active_snapshots = 0;
	size_t pending_restores = 0;
	for (const ghost_snapshot::AutoGhostSnapshot &snapshot :
			ghost_snapshot::auto_ghosts) {
		if (!snapshot.valid)
			continue;
		active_snapshots++;
		if (snapshot.phase == ghost_snapshot::AutoGhostPhase::Reinstating)
			pending_restores++;
	}

	const size_t capacity = ghost_snapshot::GhostSlotCapacity();
	const mm_ghost_skin_sync_context_t skin_context{
		capacity,
		level.round_epoch,
		level.world_epoch,
		MM_GhostMayRunDeferredPresentation(
			level.intermission_time != 0_ms,
			level.intermission_queued != 0_ms)
	};
	const size_t active_skin_queues = MM_GhostActiveSkinSyncQueueCount(
		ghost_snapshot::deferred_skin_sync);
	const size_t pending_skin_actions = MM_GhostPendingSkinSyncActionUpperBound(
		ghost_snapshot::deferred_skin_sync, skin_context);
	const auto &ghost = ghost_snapshot::diagnostics;
	const mm_reliable_fanout_diagnostics_t fanout =
		MM_GetReliableFanoutDiagnostics();

	gi.Com_PrintFmt(
		"MuffMode ghost capture: attempts={} successes={}; rejected client-state={} intermission={} social-id={} match-id={} disabled={} capacity={} no-slot={}.\n",
		ghost.capture_attempts, ghost.capture_successes,
		ghost.capture_rejected_client_state,
		ghost.capture_rejected_intermission,
		ghost.capture_rejected_social_id,
		ghost.capture_rejected_match_id,
		ghost.capture_rejected_disabled,
		ghost.capture_rejected_capacity,
		ghost.capture_rejected_no_slot);
	gi.Com_PrintFmt(
		"MuffMode ghost restore: snapshots={} pending-restores={}; attempts={} successes={} retries={} invalid={} aborts={} cancellations={}; placements saved={} fallback={}.\n",
		active_snapshots, pending_restores,
		ghost.restore_attempts, ghost.restore_successes,
		ghost.restore_retries, ghost.restore_invalid,
		ghost.restore_aborts, ghost.restore_cancellations,
		ghost.saved_placements, ghost.fallback_placements);
	gi.Com_PrintFmt(
		"MuffMode ghost presentation: queues active={} peak={} pending-actions-upper-bound={} queued={}; actions attempted={} emitted={} without-message={}.\n",
		active_skin_queues, ghost.peak_active_skin_queues,
		pending_skin_actions, ghost.skin_queues_queued,
		ghost.skin_actions_attempted, ghost.skin_messages_emitted,
		ghost.skin_actions_without_message);
	gi.Com_PrintFmt(
		"MuffMode reliable fan-out: scopes started={} completed={} active={} peak-active={} reserved messages={} bytes={} rejected messages={} bytes={}; peak-scope messages={} bytes={} last-scope messages={} bytes={} out-of-order={}.\n",
		fanout.scopes_started, fanout.scopes_completed,
		fanout.active_scopes, fanout.peak_active_scopes,
		fanout.messages_reserved, fanout.bytes_reserved,
		fanout.messages_rejected, fanout.bytes_rejected,
		fanout.peak_scope_messages, fanout.peak_scope_bytes,
		fanout.last_scope_messages, fanout.last_scope_bytes,
		fanout.out_of_order_destructions);
	gi.Com_PrintFmt(
		"MuffMode reliable fan-out diagnostics cover pre-write game-side reservations only; engine netchan backlog occupancy is not exposed by the game API.\n");

	if (reset_after) {
		ghost_snapshot::diagnostics = {};
		MM_ResetReliableFanoutDiagnostics();
		gi.Com_PrintFmt("MuffMode ghost diagnostics reset.\n");
	}
}

/*
================
MM_CmdGhost

"ghost <code>" is retained as a same-social-ID compatibility fallback.
================
*/
void MM_CmdGhost(gentity_t *ent) {
	int n;

	if (!ent || !ent->client)
		return;
	if (ghost_snapshot::EntityIsBot(ent))
		return;

	if (!MM_IsExactArgcValid(gi.argc(), 2)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <code>\n", gi.argv(0));
		return;
	}

	// throttle code guesses to the flood rate to deter brute-forcing
	if (CheckFlood(ent))
		return;

	if (ClientIsPlaying(ent->client)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You are already in the game.\n");
		return;
	}
	if (level.match_state != match_state_t::MATCH_IN_PROGRESS) {
		gi.LocClient_Print(ent, PRINT_HIGH, "No match is in progress.\n");
		return;
	}

	const auto code = MM_ParseIntArg(gi.argv(1));
	if (!code || *code < muffmode::ghost::GHOST_CODE_MIN || *code > muffmode::ghost::GHOST_CODE_MAX) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid ghost code.\n");
		return;
	}
	n = *code;

	for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++) {
		if (level.ghosts[i].code && level.ghosts[i].code == n) {
			if (ghost_snapshot::BeginRestoreDelay(ent, static_cast<int>(i), true))
				return;
			break;
		}
	}
	gi.LocClient_Print(ent, PRINT_HIGH, "Invalid ghost code.\n");
}
