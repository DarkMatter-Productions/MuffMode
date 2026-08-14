// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "shared/game.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

struct gentity_t;
struct gclient_t;
struct usercmd_t;
enum team_t;

struct mm_ghost_reserved_round_state_t {
	gclient_t *client = nullptr;
	int32_t health = 0;
	bool dead = false;
	bool exact_health = false;
};

constexpr size_t MM_GHOST_POST_RESTORE_SKIN_MESSAGES_PER_FRAME = 2;
constexpr size_t MM_GHOST_MAX_CLIENT_CAPACITY = MAX_LOBBY_PLAYERS;
constexpr size_t MM_GHOST_PLAYER_SKIN_CONFIGSTRING_VALUE_BYTES = 96;
constexpr size_t MM_GHOST_CONFIGSTRING_MESSAGE_HEADER_BYTES = 1 + 2;
constexpr size_t MM_GHOST_MAX_SKIN_CONFIGSTRING_MESSAGE_BYTES =
	MM_GHOST_CONFIGSTRING_MESSAGE_HEADER_BYTES +
	MM_GHOST_PLAYER_SKIN_CONFIGSTRING_VALUE_BYTES;
constexpr size_t MM_GHOST_NO_CLIENT_INDEX = std::numeric_limits<size_t>::max();
constexpr size_t MM_GHOST_MAX_SKIN_SYNC_ACTIONS_PER_QUEUE =
	1 + 2 * (MM_GHOST_MAX_CLIENT_CAPACITY - 1); // canonical + both directions per peer
constexpr size_t MM_GHOST_MAX_SKIN_SYNC_ACTIONS_TOTAL =
	MM_GHOST_MAX_CLIENT_CAPACITY * MM_GHOST_MAX_SKIN_SYNC_ACTIONS_PER_QUEUE;
// Bound no-op or failed presentation work independently of the message budget;
// a 128-client all-queue wave must not scan its full quadratic matrix in one frame.
constexpr size_t MM_GHOST_MAX_SKIN_SYNC_ACTIONS_PER_DRAIN =
	MM_GHOST_MAX_CLIENT_CAPACITY * 2;
static_assert(MM_GHOST_MAX_SKIN_SYNC_ACTIONS_PER_DRAIN <=
	MM_GHOST_MAX_SKIN_SYNC_ACTIONS_TOTAL);

// Converts an opaque address into an array index without subtracting or
// ordering potentially unrelated C++ pointers. The caller still owns the
// lifetime of the array; this helper only validates bounds and alignment.
constexpr size_t MM_GhostAddressIndex(uintptr_t address, uintptr_t base,
	size_t element_size, size_t element_count) noexcept
{
	if (!address || !base || !element_size || !element_count || address < base)
		return MM_GHOST_NO_CLIENT_INDEX;

	const uintptr_t delta = address - base;
	if (delta % element_size)
		return MM_GHOST_NO_CLIENT_INDEX;

	const uintptr_t index = delta / element_size;
	return index < element_count
		? static_cast<size_t>(index)
		: MM_GHOST_NO_CLIENT_INDEX;
}

enum class mm_ghost_restore_placement_t {
	SavedPosition,
	FallbackSpawn,
	Wait
};

// A blocked saved hull is never made clear by telefragging its occupants. Only
// an already-clear normal spawn is eligible; otherwise reinstatement waits.
constexpr mm_ghost_restore_placement_t MM_GhostRestorePlacementStrategy(
	bool saved_position_unsafe,
	bool fallback_spawn_available,
	bool fallback_spawn_unsafe) noexcept
{
	if (!saved_position_unsafe)
		return mm_ghost_restore_placement_t::SavedPosition;
	if (fallback_spawn_available && !fallback_spawn_unsafe)
		return mm_ghost_restore_placement_t::FallbackSpawn;
	return mm_ghost_restore_placement_t::Wait;
}

constexpr bool MM_GhostMayRunRestoreCommit(
	bool match_in_progress, bool intermission_active, bool intermission_queued) noexcept
{
	return match_in_progress && !intermission_active && !intermission_queued;
}

constexpr bool MM_GhostMayRunDeferredPresentation(
	bool intermission_active, bool intermission_queued) noexcept
{
	// A recovery spawn can finish in warmup after a same-map match reset. Its
	// bounded canonical/override repair is connection-owned, not match-owned.
	return !intermission_active && !intermission_queued;
}

constexpr bool MM_GhostShouldDeferSnapshotCleanup(
	bool intermission_queued, bool match_stats_collecting) noexcept
{
	// QueueIntermission freezes gameplay before Match_End snapshots the result.
	// Keep reservations authoritative throughout that short handoff window.
	return intermission_queued && match_stats_collecting;
}

// A snapshot never grants authority to a new network connection. The current
// connection may keep rights it authenticated during the reinstatement delay;
// the listen-server host is always authoritative.
constexpr bool MM_GhostRestoreAdminState(bool current_connection_admin, bool is_host)
{
	return current_connection_admin || is_host;
}

constexpr bool MM_GhostIsListenServerHost(
	bool dedicated_server, bool first_client_slot) noexcept
{
	return !dedicated_server && first_client_slot;
}

constexpr bool MM_GhostConnectionMayClaimSlot(bool duplicate_identity,
	bool slot_reserved, bool owns_current_reservation) noexcept
{
	return !duplicate_identity &&
		(!slot_reserved || owns_current_reservation);
}

// A client slot outlives the network connection that used it. Only an
// authenticated reconnect transaction may carry session state into the next
// connection; every ordinary accepted connection must start from clean state.
constexpr bool MM_GhostAcceptedConnectStartsFreshSession(
	bool owns_restore_transaction) noexcept
{
	return !owns_restore_transaction;
}

constexpr bool MM_GhostSnapshotCarriesLogicalMembership(
	bool exact_snapshot_current, bool awaiting_begin,
	bool belongs_to_current_match, bool match_in_progress,
	bool intermission_active, bool intermission_queued) noexcept
{
	return exact_snapshot_current ||
		(awaiting_begin && belongs_to_current_match && match_in_progress &&
			!intermission_active && !intermission_queued);
}

constexpr bool MM_GhostFallbackAuthorityMatches(
	bool pending, uint32_t source_match_serial,
	uint32_t current_match_serial) noexcept
{
	return pending && source_match_serial == current_match_serial;
}

constexpr bool MM_GhostFallbackOwnsRoundEntry(
	bool pending, bool entitled,
	uint32_t source_match_serial, uint32_t current_match_serial,
	uint32_t source_round_epoch, uint32_t current_round_epoch) noexcept
{
	return pending && entitled &&
		source_match_serial == current_match_serial &&
		source_round_epoch == current_round_epoch;
}

// A fallback cleared after its first ClientBegin may carry round-entry authority
// across a spawn-point retry, but never across a different entity lifetime,
// connection identity, match, or round.
constexpr bool MM_GhostFallbackRoundEntryRetryMatches(
	bool pending, bool entitled,
	int32_t source_spawn_generation, int32_t current_spawn_generation,
	bool identity_matches,
	uint32_t source_match_serial, uint32_t current_match_serial,
	uint32_t source_round_epoch, uint32_t current_round_epoch) noexcept
{
	return MM_GhostFallbackOwnsRoundEntry(
		pending, entitled,
		source_match_serial, current_match_serial,
		source_round_epoch, current_round_epoch) &&
		source_spawn_generation == current_spawn_generation &&
		identity_matches;
}

// A pre-Begin handoff and a failed ordinary fallback spawn both need server
// frames even when no fully spawned client remains to keep the main loop awake.
constexpr bool MM_GhostRecoveryNeedsFrame(
	bool fallback_pending, bool abort_spawn_pending, bool connected,
	bool persistent_state_initialized, bool awaiting_respawn) noexcept
{
	return connected &&
		(fallback_pending ||
			(abort_spawn_pending &&
				(!persistent_state_initialized || awaiting_respawn)));
}

// CalculateRanks already owns an in-use, connected playing client. A logical
// reservation is additive only while that live rank entry is absent.
constexpr bool MM_GhostReservationNeedsSeparateCount(
	bool slot_inuse, bool connected, bool live_client_playing) noexcept
{
	return !(slot_inuse && connected && live_client_playing);
}

// Pointer alignment/range proves only that a saved alias names some ghost
// record. Reload may restore it only when that record still names this client.
constexpr bool MM_GhostReloadAliasMayRestore(
	bool index_valid, bool owner_matches) noexcept
{
	return index_valid && owner_matches;
}

struct mm_ghost_fallback_lifecycle_policy_t {
	bool refresh_settled_authority = false;
	bool resume_player_stats = false;
	bool begin_match_stats = false;
};

constexpr mm_ghost_fallback_lifecycle_policy_t
MM_GhostFallbackLifecyclePolicy(
	uint32_t source_match_serial, uint32_t current_match_serial,
	bool player_stats_open, bool match_stats_collecting) noexcept
{
	const bool same_match = source_match_serial == current_match_serial;
	const bool resume_player_stats = same_match && player_stats_open;
	return {
		same_match && !player_stats_open,
		resume_player_stats,
		resume_player_stats && match_stats_collecting
	};
}

// A failed reconnect profile load has no preference authority. The installed
// reservation snapshot remains the trusted base until a later successful load;
// edits made after restore are layered on top separately.
constexpr bool MM_GhostReconnectPreferencesUseInstalledProfile(
	bool reconnect_profile_ready) noexcept
{
	return !reconnect_profile_ready;
}

struct mm_ghost_skin_sync_pair_t {
	size_t viewer_index = 0;
	size_t target_index = 0;
	bool valid = false;
};

// A restored player can affect both directions of a per-viewer override:
// existing viewer -> restored target, and restored viewer -> existing target.
constexpr mm_ghost_skin_sync_pair_t MM_GhostSkinSyncPair(
	size_t restored_index, size_t operation, size_t client_capacity)
{
	const size_t peer_index = operation / 2;
	if (restored_index >= client_capacity || peer_index >= client_capacity ||
		peer_index == restored_index)
		return {};

	return operation % 2 == 0
		? mm_ghost_skin_sync_pair_t{ peer_index, restored_index, true }
		: mm_ghost_skin_sync_pair_t{ restored_index, peer_index, true };
}

struct mm_ghost_skin_sync_queue_t {
	bool active = false;
	// A completed queue retains ownership of its pairs until the slot is queued
	// again or invalidated. Otherwise an older overlapping queue could resend the
	// same pair after the newer queue finishes.
	bool owns_pairs = false;
	size_t restored_index = 0;
	int32_t restored_spawn_count = 0;
	uint32_t round_epoch = 0;
	uint32_t world_epoch = 0;
	uint64_t serial = 0;
	size_t next_operation = 0;
};

struct mm_ghost_skin_sync_slot_t {
	bool valid = false;
	bool connected = false;
	bool spawned = false;
	int32_t spawn_count = 0;
};

struct mm_ghost_skin_sync_context_t {
	size_t client_capacity = 0;
	uint32_t round_epoch = 0;
	uint32_t world_epoch = 0;
	bool presentation_allowed = false;
};

enum class mm_ghost_skin_sync_action_t {
	None,
	PublishCanonical,
	ReapplyOverride
};

struct mm_ghost_skin_sync_step_t {
	mm_ghost_skin_sync_action_t action = mm_ghost_skin_sync_action_t::None;
	size_t restored_index = MM_GHOST_NO_CLIENT_INDEX;
	mm_ghost_skin_sync_pair_t pair{};
	size_t inspections = 0;
};

template<size_t Capacity>
struct mm_ghost_skin_sync_scheduler_t {
	std::array<mm_ghost_skin_sync_queue_t, Capacity> queues{};
	uint64_t last_serial = 0;
	size_t cursor = 0;
	// Capacity changes are uncommon, but clearing removed slots is still charged
	// to the same raw-inspection budget as ordinary queue scanning. Remember
	// partial pruning so a small caller budget cannot resurrect stale work if the
	// configured capacity changes again before cleanup completes.
	size_t observed_capacity = Capacity;
	size_t prune_cursor = Capacity;
};

template<size_t Capacity>
constexpr void MM_GhostResetSkinSync(
	mm_ghost_skin_sync_scheduler_t<Capacity> &scheduler) noexcept
{
	scheduler = {};
}

template<size_t Capacity>
constexpr bool MM_GhostQueueSkinSync(
	mm_ghost_skin_sync_scheduler_t<Capacity> &scheduler,
	size_t restored_index,
	int32_t restored_spawn_count,
	uint32_t round_epoch,
	uint32_t world_epoch) noexcept
{
	if (restored_index >= Capacity)
		return false;

	// Serial zero means "never queued". On the practically unreachable wrap,
	// drop every presentation queue together before beginning a new ordering era.
	if (scheduler.last_serial == std::numeric_limits<uint64_t>::max())
		MM_GhostResetSkinSync(scheduler);

	mm_ghost_skin_sync_queue_t &queue = scheduler.queues[restored_index];
	queue = {};
	queue.active = true;
	queue.owns_pairs = true;
	queue.restored_index = restored_index;
	queue.restored_spawn_count = restored_spawn_count;
	queue.round_epoch = round_epoch;
	queue.world_epoch = world_epoch;
	queue.serial = ++scheduler.last_serial;
	return true;
}

template<size_t Capacity>
constexpr void MM_GhostCancelSkinSync(
	mm_ghost_skin_sync_scheduler_t<Capacity> &scheduler,
	size_t restored_index) noexcept
{
	if (restored_index < Capacity)
		scheduler.queues[restored_index] = {};
}

template<size_t Capacity>
constexpr size_t MM_GhostSkinSyncCapacity(
	const mm_ghost_skin_sync_context_t &context) noexcept
{
	return context.client_capacity < Capacity ? context.client_capacity : Capacity;
}

template<size_t Capacity>
constexpr size_t MM_GhostActiveSkinSyncQueueCount(
	const mm_ghost_skin_sync_scheduler_t<Capacity> &scheduler) noexcept
{
	size_t count = 0;
	for (const mm_ghost_skin_sync_queue_t &queue : scheduler.queues)
		if (queue.active)
			count++;
	return count;
}

// Counts a structural upper bound on remaining production actions. Runtime
// slot readiness and newer overlapping queues can only reduce this number.
template<size_t Capacity>
constexpr size_t MM_GhostPendingSkinSyncActionUpperBound(
	const mm_ghost_skin_sync_scheduler_t<Capacity> &scheduler,
	const mm_ghost_skin_sync_context_t &context) noexcept
{
	const size_t capacity = MM_GhostSkinSyncCapacity<Capacity>(context);
	size_t pending = 0;
	for (const mm_ghost_skin_sync_queue_t &queue : scheduler.queues) {
		if (!queue.active || queue.restored_index >= capacity)
			continue;

		if (queue.next_operation == 0)
			pending++;
		const size_t pair_start = queue.next_operation == 0
			? 0
			: queue.next_operation - 1;
		for (size_t operation = pair_start; operation < 2 * capacity; operation++)
			if (MM_GhostSkinSyncPair(
				queue.restored_index, operation, capacity).valid)
				pending++;
	}
	return pending;
}

template<size_t Capacity>
constexpr bool MM_GhostSkinSyncSlotReady(
	const std::array<mm_ghost_skin_sync_slot_t, Capacity> &slots,
	size_t index,
	const mm_ghost_skin_sync_context_t &context) noexcept
{
	const size_t capacity = MM_GhostSkinSyncCapacity<Capacity>(context);
	return index < capacity && slots[index].valid && slots[index].connected &&
		slots[index].spawned;
}

template<size_t Capacity>
constexpr bool MM_GhostSkinSyncQueueOwnsCurrentPairs(
	const mm_ghost_skin_sync_queue_t &queue,
	const std::array<mm_ghost_skin_sync_slot_t, Capacity> &slots,
	const mm_ghost_skin_sync_context_t &context) noexcept
{
	if (!context.presentation_allowed || !queue.owns_pairs || !queue.serial ||
		!MM_GhostSkinSyncSlotReady(slots, queue.restored_index, context))
		return false;

	const mm_ghost_skin_sync_slot_t &slot = slots[queue.restored_index];
	return slot.spawn_count == queue.restored_spawn_count &&
		queue.round_epoch == context.round_epoch &&
		queue.world_epoch == context.world_epoch;
}

// Returns at most one production action and advances the same queue/cursor state
// used by the runtime. Simultaneous queues assign both directions of a pair to
// the newer endpoint; a later reconnect receives a newer serial and refreshes
// both directions again.
template<size_t Capacity>
constexpr mm_ghost_skin_sync_step_t MM_GhostStepSkinSync(
	mm_ghost_skin_sync_scheduler_t<Capacity> &scheduler,
	const std::array<mm_ghost_skin_sync_slot_t, Capacity> &slots,
	const mm_ghost_skin_sync_context_t &context,
	size_t inspection_budget = std::numeric_limits<size_t>::max()) noexcept
{
	if (!context.presentation_allowed) {
		MM_GhostResetSkinSync(scheduler);
		return {};
	}

	const size_t capacity = MM_GhostSkinSyncCapacity<Capacity>(context);
	if (!capacity) {
		MM_GhostResetSkinSync(scheduler);
		return {};
	}
	if (!inspection_budget)
		return {};

	size_t inspections = 0;
	if (scheduler.observed_capacity != capacity) {
		// If a second capacity transition arrives during an earlier bounded prune,
		// restart at the lowest affected index. In particular, growing must not
		// expose an entry that a preceding shrink had not reached yet.
		scheduler.prune_cursor = std::min(scheduler.prune_cursor, capacity);
		scheduler.observed_capacity = capacity;
	}
	while (scheduler.prune_cursor < Capacity &&
		inspections < inspection_budget) {
		scheduler.queues[scheduler.prune_cursor++] = {};
		inspections++;
	}
	if (scheduler.prune_cursor < Capacity || inspections >= inspection_budget) {
		mm_ghost_skin_sync_step_t result{};
		result.inspections = inspections;
		return result;
	}
	scheduler.cursor %= capacity;

	for (size_t checked = 0;
		checked < capacity && inspections < inspection_budget;
		checked++) {
		const size_t queue_index = scheduler.cursor % capacity;
		scheduler.cursor = queue_index + 1 == capacity ? 0 : queue_index + 1;
		inspections++;
		mm_ghost_skin_sync_queue_t &queue = scheduler.queues[queue_index];
		if (!queue.active)
			continue;

		if (!MM_GhostSkinSyncQueueOwnsCurrentPairs(queue, slots, context)) {
			queue = {};
			continue;
		}

		if (queue.next_operation == 0) {
			queue.next_operation = 1;
			return {
				mm_ghost_skin_sync_action_t::PublishCanonical,
				queue.restored_index,
				{},
				inspections
			};
		}

		// Advance at most one structural pair operation for this queue. The
		// caller budgets inspections, so disconnected peers and overlapping
		// queues cannot turn one server frame into an unbounded quadratic scan.
		const size_t pair_operation = queue.next_operation - 1;
		if (pair_operation / 2 >= capacity) {
			queue.active = false;
			continue;
		}
		queue.next_operation++;

		const mm_ghost_skin_sync_pair_t pair = MM_GhostSkinSyncPair(
			queue.restored_index, pair_operation, capacity);
		if (!pair.valid)
			continue;

		const size_t peer_index = pair.viewer_index == queue.restored_index
			? pair.target_index
			: pair.viewer_index;
		if (!MM_GhostSkinSyncSlotReady(slots, peer_index, context))
			continue;

		const mm_ghost_skin_sync_queue_t &peer_queue =
			scheduler.queues[peer_index];
		const bool peer_is_newer = peer_queue.serial > queue.serial ||
			(peer_queue.serial == queue.serial && peer_queue.serial != 0 &&
				peer_index > queue.restored_index);
		if (peer_is_newer &&
			MM_GhostSkinSyncQueueOwnsCurrentPairs(peer_queue, slots, context))
			continue;

		return {
			mm_ghost_skin_sync_action_t::ReapplyOverride,
			queue.restored_index,
			pair,
			inspections
		};
	}

	mm_ghost_skin_sync_step_t result{};
	result.inspections = inspections;
	return result;
}

template<size_t Capacity>
constexpr size_t MM_GhostSelectDueRestoreCommit(
	const std::array<bool, Capacity> &due,
	size_t client_capacity,
	size_t &cursor) noexcept
{
	const size_t capacity = client_capacity < Capacity ? client_capacity : Capacity;
	if (!capacity) {
		cursor = 0;
		return MM_GHOST_NO_CLIENT_INDEX;
	}

	for (size_t checked = 0; checked < capacity; checked++) {
		const size_t index = cursor % capacity;
		cursor = index + 1 == capacity ? 0 : index + 1;
		if (due[index])
			return index;
	}

	return MM_GHOST_NO_CLIENT_INDEX;
}

constexpr bool MM_GhostRestoreEpochMatches(uint32_t snapshot_epoch, uint32_t current_epoch)
{
	return snapshot_epoch == current_epoch;
}

constexpr bool MM_GhostSnapshotBelongsToWorld(
	bool match_id_matches, uint32_t snapshot_world_epoch, uint32_t current_world_epoch)
{
	return match_id_matches && snapshot_world_epoch == current_world_epoch;
}

constexpr bool MM_GhostSessionBelongsToMatch(
	bool snapshot_valid, bool match_id_matches)
{
	// Combat state is world/round-owned, but authenticated team membership is
	// match-owned and must survive an ordinary same-match world rebuild.
	return snapshot_valid && match_id_matches;
}

struct mm_ghost_session_membership_policy_t {
	bool reapply_saved_membership = false;
	bool clear_follow_target = false;
	bool use_free_spectator = false;
};

constexpr mm_ghost_session_membership_policy_t MM_GhostSessionMembershipPolicy(
	bool saved_membership_authorized, bool saved_team_is_spectator) noexcept
{
	if (!saved_membership_authorized)
		return {};

	// Follow state is a client-slot reference, not durable membership. A valid
	// match reservation keeps its team/duel identity but starts with a normalized
	// camera state after any abort or world rebuild.
	return { true, true, saved_team_is_spectator };
}

// A failed spawn is retried after the immediate abort-restart scope unwinds.
// Keep its presentation obligation explicit until the first successful spawn.
constexpr bool MM_GhostSpawnUsesDeferredPresentation(
	bool ghost_abort_restart_in_progress, bool abort_spawn_retry_pending) noexcept
{
	return ghost_abort_restart_in_progress || abort_spawn_retry_pending;
}

// The ordinary spawn path may automatically join a team or select a follow
// target. Ghost-abort spawns must keep their normalized membership/camera state
// and let the bounded outer-frame scheduler rebuild presentation.
constexpr bool MM_GhostSpawnMayAutoFollow(bool uses_deferred_presentation) noexcept
{
	return !uses_deferred_presentation;
}

constexpr bool MM_GhostSpawnMayAutoJoin(bool uses_deferred_presentation) noexcept
{
	return !uses_deferred_presentation;
}

constexpr bool MM_GhostSpawnNeedsPersistentInitialization(
	bool uses_deferred_presentation, bool persistent_state_initialized) noexcept
{
	return uses_deferred_presentation && !persistent_state_initialized;
}

constexpr bool MM_GhostDisconnectMayCaptureSnapshot(bool abort_spawn_pending) noexcept
{
	return !abort_spawn_pending;
}

constexpr bool MM_GhostAbortMarkerSurvivesSystemClear(
	bool restart_pending_clients, bool connected,
	bool persistent_state_initialized, bool awaiting_respawn) noexcept
{
	return restart_pending_clients && connected &&
		(!persistent_state_initialized || awaiting_respawn);
}

constexpr bool MM_GhostSnapshotNeedsCleanup(bool snapshot_valid)
{
	return snapshot_valid;
}

constexpr bool MM_GhostHordeWaveOwnsTechReset(bool is_horde,
	bool reset_tech_each_wave, uint32_t snapshot_round_epoch, uint32_t current_round_epoch)
{
	return is_horde && reset_tech_each_wave && snapshot_round_epoch != current_round_epoch;
}

constexpr bool MM_GhostSnapshotReservesSlot(bool snapshot_valid, bool owns_slot)
{
	// Freshness is checked when choosing the owning reconnect. Until cleanup has
	// actually removed a valid snapshot, another account must not inherit its slot.
	return snapshot_valid && owns_slot;
}

constexpr bool MM_GhostReservedParticipantCountsForRound(
	bool snapshot_matches_current,
	bool owns_slot,
	bool disconnected_or_pending,
	bool saved_team_matches,
	bool saved_client_playing,
	bool saved_eliminated)
{
	return snapshot_matches_current && owns_slot && disconnected_or_pending &&
		saved_team_matches && saved_client_playing && !saved_eliminated;
}

// [MuffMode] Match ghost system: rejoin a match in progress with state intact.
void MM_Ghost_ClearAll(bool restart_pending_clients = false);
void MM_Ghost_ClearClient(gentity_t *ent);
void MM_Ghost_Assign(gentity_t *ent);
void MM_Ghost_DoAssign(gentity_t *ent);
bool MM_Ghost_CaptureInactive(gentity_t *ent);
bool MM_Ghost_CaptureDisconnect(gentity_t *ent);
void MM_Ghost_MakeDisconnectPlaceholder(gentity_t *ent);
gentity_t *MM_Ghost_ChooseReconnectSlot(const char *social_id, gentity_t **ignore, size_t num_ignore);
void MM_Ghost_BeginClientConnect(gentity_t *slot);
void MM_Ghost_MarkRejectedClientConnect(gentity_t *slot);
bool MM_Ghost_ConsumeRejectedClientConnect(gentity_t *slot);
bool MM_Ghost_PrepareClientConnect(gentity_t *slot, const char *social_id);
bool MM_Ghost_PrepareClientBeginFallback(gentity_t *ent);
void MM_Ghost_CompleteClientBeginFallback(gentity_t *ent);
bool MM_Ghost_IsClientBeginFallbackPending(gentity_t *ent);
bool MM_Ghost_ClientBeginFallbackOwnsRoundEntry(const gentity_t *ent);
void MM_Ghost_OnMatchStart();
bool MM_Ghost_IsReservedSlot(gentity_t *slot);
gclient_t *MM_Ghost_ReservedClientState(gentity_t *slot);
const gclient_t *MM_Ghost_ReservedClientState(const gentity_t *slot);
bool MM_Ghost_ApplyReservedDuelResult(gentity_t *slot, const char *social_id, bool won);
bool MM_Ghost_QueueReservedDuelLoser(gentity_t *slot, const char *social_id);
size_t MM_Ghost_ActivePlayingReservationCount(
	const gentity_t *ignore = nullptr);
size_t MM_Ghost_ActiveHumanPlayingReservationCount(
	const gentity_t *ignore = nullptr);
size_t MM_Ghost_ActivePlayingReservationCountForTeam(
	team_t team, const gentity_t *ignore = nullptr);
int64_t MM_Ghost_ActivePlayingReservationScoreForTeam(
	team_t team, const gentity_t *ignore = nullptr);
bool MM_Ghost_ReservedClientCountsForRound(const gentity_t *slot, team_t team);
bool MM_Ghost_ReservedRoundState(gentity_t *slot, team_t team,
	mm_ghost_reserved_round_state_t &state);
bool MM_Ghost_AdjustReservedPlayerScore(gentity_t *slot, int32_t offset);
bool MM_Ghost_IsPendingRestore(gentity_t *ent);
bool MM_Ghost_IsAbortSpawnPending(gentity_t *ent);
void MM_Ghost_CancelAbortSpawn(gentity_t *ent);
void MM_Ghost_CompleteAbortSpawn(gentity_t *ent);
bool MM_Ghost_RunPendingRestoreFrame(gentity_t *ent);
bool MM_Ghost_EndPendingRestoreFrame(gentity_t *ent);
bool MM_Ghost_ClientThink(gentity_t *ent, const usercmd_t *ucmd);
bool MM_Ghost_HasActiveReservations();
bool MM_Ghost_TryRestore(gentity_t *ent);
void MM_Ghost_DropTimedOutFlags();
void MM_Ghost_ResolveRoundTransition();
void MM_Ghost_RunFrame();
void MM_Ghost_RunServerFrame();
void MM_Ghost_ReportDiagnostics(bool reset_after = false);
void MM_CmdGhost(gentity_t *ent);
