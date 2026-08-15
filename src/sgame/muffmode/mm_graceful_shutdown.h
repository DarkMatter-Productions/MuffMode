// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

namespace muffmode::graceful_shutdown {

constexpr bool DedicatedCommandAvailable(int dedicated) noexcept
{
	return dedicated != 0;
}

constexpr bool HumanArrivalNeedsNotice(
	int previous_human_count, int current_human_count) noexcept
{
	return current_human_count > previous_human_count;
}

} // namespace muffmode::graceful_shutdown

void MM_GracefulShutdown_Reset();
void MM_GracefulShutdown_Queue();
void MM_GracefulShutdown_Cancel();
// Returns true once a clean engine close has been queued. Callers should not
// enqueue competing map changes in that frame.
bool MM_GracefulShutdown_RunFrame();

// Returns true when the pending map transition has been consumed by a queued
// server close. The caller must not issue its ordinary gamemap command then.
bool MM_GracefulShutdown_HandleLevelExit();
