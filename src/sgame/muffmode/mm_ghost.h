// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

struct gentity_t;
struct usercmd_t;
enum team_t;

constexpr size_t MM_GHOST_POST_RESTORE_SKIN_MESSAGES_PER_FRAME = 2;
constexpr size_t MM_GHOST_MAX_CLIENT_CAPACITY = 32;
constexpr size_t MM_GHOST_PLAYER_SKIN_CONFIGSTRING_VALUE_BYTES = 96;
constexpr size_t MM_GHOST_CONFIGSTRING_MESSAGE_HEADER_BYTES = 1 + 2;
constexpr size_t MM_GHOST_MAX_SKIN_CONFIGSTRING_MESSAGE_BYTES =
	MM_GHOST_CONFIGSTRING_MESSAGE_HEADER_BYTES +
	MM_GHOST_PLAYER_SKIN_CONFIGSTRING_VALUE_BYTES;
constexpr size_t MM_GHOST_NO_CLIENT_INDEX = std::numeric_limits<size_t>::max();
constexpr size_t MM_GHOST_MAX_SKIN_SYNC_ACTIONS_PER_QUEUE =
	1 + 2 * (MM_GHOST_MAX_CLIENT_CAPACITY - 1); // canonical + both directions per peer
constexpr size_t MM_GHOST_MAX_SKIN_SYNC_ACTIONS_PER_DRAIN =
	MM_GHOST_MAX_CLIENT_CAPACITY * MM_GHOST_MAX_SKIN_SYNC_ACTIONS_PER_QUEUE;

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

// A snapshot never grants authority to a new network connection. The current
// connection may keep rights it authenticated during the reinstatement delay;
// the listen-server host is always authoritative.
constexpr bool MM_GhostRestoreAdminState(bool current_connection_admin, bool is_host)
{
	return current_connection_admin || is_host;
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
};

template<size_t Capacity>
struct mm_ghost_skin_sync_scheduler_t {
	std::array<mm_ghost_skin_sync_queue_t, Capacity> queues{};
	uint64_t last_serial = 0;
	size_t cursor = 0;
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
	const mm_ghost_skin_sync_context_t &context) noexcept
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

	for (size_t checked = 0; checked < capacity; checked++) {
		const size_t queue_index = scheduler.cursor % capacity;
		scheduler.cursor = queue_index + 1 == capacity ? 0 : queue_index + 1;
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
				{}
			};
		}

		for (;;) {
			const size_t pair_operation = queue.next_operation - 1;
			if (pair_operation / 2 >= capacity) {
				queue.active = false;
				break;
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
				pair
			};
		}
	}

	return {};
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
bool MM_Ghost_IsReservedSlot(gentity_t *slot);
bool MM_Ghost_ReservedClientCountsForRound(const gentity_t *slot, team_t team);
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
void MM_Ghost_RunFrame();
void MM_Ghost_RunServerFrame();
void MM_Ghost_ReportDiagnostics(bool reset_after = false);
void MM_CmdGhost(gentity_t *ent);
