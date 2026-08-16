// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_message_budget.h"

#include <algorithm>
#include <limits>

namespace {

// Game exports and their gi.Write* staging buffer are used on the server thread.
mm_reliable_fanout_scope_t *reliable_fanout_scope = nullptr;
mm_reliable_fanout_diagnostics_t reliable_fanout_diagnostics{};

void SaturatingIncrement(uint64_t &value)
{
	if (value != std::numeric_limits<uint64_t>::max())
		value++;
}

void SaturatingAdd(uint64_t &value, size_t increment)
{
	const uint64_t maximum = std::numeric_limits<uint64_t>::max();
	const uint64_t converted = static_cast<uint64_t>(increment);
	value = converted > maximum - value ? maximum : value + converted;
}

void RecordCompletedScope(const mm_reliable_fanout_budget_t &budget)
{
	SaturatingIncrement(reliable_fanout_diagnostics.scopes_completed);
	reliable_fanout_diagnostics.last_scope_messages = budget.messages;
	reliable_fanout_diagnostics.last_scope_bytes = budget.bytes;
	reliable_fanout_diagnostics.peak_scope_messages = std::max(
		reliable_fanout_diagnostics.peak_scope_messages, budget.messages);
	reliable_fanout_diagnostics.peak_scope_bytes = std::max(
		reliable_fanout_diagnostics.peak_scope_bytes, budget.bytes);
	if (reliable_fanout_diagnostics.active_scopes)
		reliable_fanout_diagnostics.active_scopes--;
}

} // namespace

mm_reliable_fanout_scope_t::mm_reliable_fanout_scope_t(
	size_t max_messages, size_t max_bytes, const char *source)
{
	budget_ = { max_messages, max_bytes, 0, 0 };
	parent_ = reliable_fanout_scope;
	source_ = source;
	reliable_fanout_scope = this;
	owns_scope_ = true;
	SaturatingIncrement(reliable_fanout_diagnostics.scopes_started);
	reliable_fanout_diagnostics.active_scopes++;
	reliable_fanout_diagnostics.peak_active_scopes = std::max(
		reliable_fanout_diagnostics.peak_active_scopes,
		reliable_fanout_diagnostics.active_scopes);
}

mm_reliable_fanout_scope_t::~mm_reliable_fanout_scope_t() noexcept
{
	if (!owns_scope_)
		return;

	if (reliable_fanout_scope != this) {
		// Stack scopes should always unwind in LIFO order. If an integration
		// violates that rule, unlink this scope from its child so neither the
		// active chain nor a later reservation retains a dangling pointer.
		try {
			gi.Com_PrintFmt(
				"MuffMode: reliable fan-out budget '{}' was destroyed out of order.\n",
				source_ ? source_ : "unknown source");
		}
		catch (...) {
			// Destruction must still unlink the scope; diagnostics are best-effort.
		}
		SaturatingIncrement(reliable_fanout_diagnostics.out_of_order_destructions);
		for (auto *child = reliable_fanout_scope; child; child = child->parent_) {
			if (child->parent_ != this)
				continue;
			child->parent_ = parent_;
			break;
		}
		RecordCompletedScope(budget_);
		owns_scope_ = false;
		return;
	}

	reliable_fanout_scope = parent_;
	RecordCompletedScope(budget_);
	owns_scope_ = false;
}

bool MM_ReserveReliableFanoutMessage(size_t message_bytes)
{
	return MM_ReserveReliableFanoutMessages(1, message_bytes);
}

bool MM_ReserveReliableFanoutMessages(
	size_t message_count, size_t message_bytes)
{
	if (!message_count)
		return false;
	if (!reliable_fanout_scope)
		return true;

	// First prove that every active budget can accept the batch. Committing in
	// a second pass keeps parent accounting unchanged when a child rejects it.
	mm_reliable_fanout_scope_t *rejected_scope = nullptr;
	for (auto *scope = reliable_fanout_scope; scope; scope = scope->parent_) {
		auto candidate = scope->budget_;
		if (!MM_ReliableFanoutTryReserveBatch(
				candidate, message_count, message_bytes)) {
			rejected_scope = scope;
			break;
		}
	}

	if (!rejected_scope) {
		for (auto *scope = reliable_fanout_scope; scope; scope = scope->parent_)
			MM_ReliableFanoutTryReserveBatch(
				scope->budget_, message_count, message_bytes);
		SaturatingAdd(
			reliable_fanout_diagnostics.messages_reserved, message_count);
		SaturatingAdd(reliable_fanout_diagnostics.bytes_reserved, message_bytes);
		return true;
	}

	SaturatingAdd(
		reliable_fanout_diagnostics.messages_rejected, message_count);
	SaturatingAdd(reliable_fanout_diagnostics.bytes_rejected, message_bytes);

	if (!rejected_scope->warned_) {
		gi.Com_PrintFmt(
			"MuffMode: dropped {} over-budget reliable messages in '{}' ({} bytes after {}/{} messages and {}/{} bytes).\n",
			message_count,
			rejected_scope->source_ ? rejected_scope->source_ : "unknown source",
			message_bytes,
			rejected_scope->budget_.messages,
			rejected_scope->budget_.max_messages,
			rejected_scope->budget_.bytes,
			rejected_scope->budget_.max_bytes);
		rejected_scope->warned_ = true;
	}

	return false;
}

mm_reliable_fanout_diagnostics_t MM_GetReliableFanoutDiagnostics()
{
	return reliable_fanout_diagnostics;
}

void MM_ResetReliableFanoutDiagnostics()
{
	const size_t active_scopes = reliable_fanout_diagnostics.active_scopes;
	reliable_fanout_diagnostics = {};
	reliable_fanout_diagnostics.active_scopes = active_scopes;
	reliable_fanout_diagnostics.peak_active_scopes = active_scopes;
}
