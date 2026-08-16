// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstddef>
#include <cstdint>

// A local, fail-closed budget for a bounded burst of reliable game messages.
// This does not claim visibility into the engine's private netchan backlog; it
// prevents a game-side fan-out from growing beyond its reviewed contribution.
struct mm_reliable_fanout_budget_t {
	size_t max_messages = 0;
	size_t max_bytes = 0;
	size_t messages = 0;
	size_t bytes = 0;
};

// Lifetime, game-side diagnostics for reviewed reliable fan-out scopes. The
// engine does not expose its private netchan occupancy through the game API, so
// these counters describe only MuffMode's pre-write reservations.
struct mm_reliable_fanout_diagnostics_t {
	uint64_t scopes_started = 0;
	uint64_t scopes_completed = 0;
	uint64_t messages_reserved = 0;
	uint64_t bytes_reserved = 0;
	uint64_t messages_rejected = 0;
	uint64_t bytes_rejected = 0;
	uint64_t out_of_order_destructions = 0;
	size_t active_scopes = 0;
	size_t peak_active_scopes = 0;
	size_t peak_scope_messages = 0;
	size_t peak_scope_bytes = 0;
	size_t last_scope_messages = 0;
	size_t last_scope_bytes = 0;
};

constexpr bool MM_ReliableFanoutTryReserveBatch(
	mm_reliable_fanout_budget_t &budget,
	size_t message_count,
	size_t message_bytes) noexcept
{
	if (!message_count || budget.messages > budget.max_messages ||
		message_count > budget.max_messages - budget.messages)
		return false;
	if (budget.bytes > budget.max_bytes ||
		message_bytes > budget.max_bytes - budget.bytes)
		return false;

	budget.messages += message_count;
	budget.bytes += message_bytes;
	return true;
}

constexpr bool MM_ReliableFanoutTryReserve(
	mm_reliable_fanout_budget_t &budget, size_t message_bytes) noexcept
{
	return MM_ReliableFanoutTryReserveBatch(budget, 1, message_bytes);
}

// While a scope is active, participating message writers must reserve their
// complete wire payload before any engine message write or publication. Nested
// scopes are hierarchical: a reservation must fit every active budget, so a
// narrow local cap cannot reset or bypass its enclosing server-frame cap.
class mm_reliable_fanout_scope_t final {
public:
	mm_reliable_fanout_scope_t(
		size_t max_messages, size_t max_bytes, const char *source);
	~mm_reliable_fanout_scope_t() noexcept;

	mm_reliable_fanout_scope_t(const mm_reliable_fanout_scope_t &) = delete;
	mm_reliable_fanout_scope_t &operator=(const mm_reliable_fanout_scope_t &) = delete;

private:
	friend bool MM_ReserveReliableFanoutMessage(size_t message_bytes);
	friend bool MM_ReserveReliableFanoutMessages(
		size_t message_count, size_t message_bytes);

	mm_reliable_fanout_budget_t budget_{};
	mm_reliable_fanout_scope_t *parent_ = nullptr;
	const char *source_ = nullptr;
	bool warned_ = false;
	bool owns_scope_ = false;
};

// Returns true when no fan-out scope is active. When one is active, the caller
// may write only after this reservation succeeds.
bool MM_ReserveReliableFanoutMessage(size_t message_bytes);
// Atomically reserves a group of messages that must either all be published or
// all be deferred. message_bytes is their combined wire size.
bool MM_ReserveReliableFanoutMessages(
	size_t message_count, size_t message_bytes);

mm_reliable_fanout_diagnostics_t MM_GetReliableFanoutDiagnostics();
void MM_ResetReliableFanoutDiagnostics();
