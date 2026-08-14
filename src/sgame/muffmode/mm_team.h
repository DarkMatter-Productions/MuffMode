// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

struct gentity_t;
enum team_t;

// Count-only team identities keep the shared policy host-testable without
// coupling it to the server-only team_t definition.
enum class mm_team_count_side_t : uint8_t {
	None,
	Red,
	Blue
};

struct mm_team_logical_counts_t {
	size_t red = 0;
	size_t blue = 0;
};

constexpr size_t MM_TeamSaturatingCountAdd(
	size_t lhs, size_t rhs) noexcept
{
	return lhs > std::numeric_limits<size_t>::max() - rhs
		? std::numeric_limits<size_t>::max()
		: lhs + rhs;
}

// Rank counts omit reconnect reservations. Remove an optional target's live
// and reserved memberships independently before composing the logical view;
// this makes PickTeam(ignore_client_num) and a projected switch exact even if
// the target is represented by either handoff state.
constexpr mm_team_logical_counts_t MM_TeamLogicalCounts(
	size_t live_red, size_t live_blue,
	size_t reserved_red, size_t reserved_blue,
	mm_team_count_side_t ignored_live_side = mm_team_count_side_t::None,
	mm_team_count_side_t ignored_reserved_side = mm_team_count_side_t::None) noexcept
{
	if (ignored_live_side == mm_team_count_side_t::Red && live_red)
		--live_red;
	else if (ignored_live_side == mm_team_count_side_t::Blue && live_blue)
		--live_blue;

	if (ignored_reserved_side == mm_team_count_side_t::Red && reserved_red)
		--reserved_red;
	else if (ignored_reserved_side == mm_team_count_side_t::Blue && reserved_blue)
		--reserved_blue;

	return {
		MM_TeamSaturatingCountAdd(live_red, reserved_red),
		MM_TeamSaturatingCountAdd(live_blue, reserved_blue)
	};
}

// A tie deliberately yields None so PickTeam can continue through its existing
// score and random tie-breakers.
constexpr mm_team_count_side_t MM_TeamLowerLogicalSide(
	const mm_team_logical_counts_t &counts) noexcept
{
	if (counts.red < counts.blue)
		return mm_team_count_side_t::Red;
	if (counts.blue < counts.red)
		return mm_team_count_side_t::Blue;
	return mm_team_count_side_t::None;
}

struct mm_team_switch_projection_t {
	mm_team_logical_counts_t counts{};
	bool allowed = false;
};

// Project a non-forced switch after removing every representation of the
// target. Only an excessive lead by the requested side is rejected: joining
// the underfull side remains legal even when it cannot repair a large deficit
// in one move. The default preserves the historical allowed final lead of two.
constexpr mm_team_switch_projection_t MM_TeamProjectForceBalanceSwitch(
	size_t live_red, size_t live_blue,
	size_t reserved_red, size_t reserved_blue,
	mm_team_count_side_t ignored_live_side,
	mm_team_count_side_t ignored_reserved_side,
	mm_team_count_side_t desired_side,
	size_t maximum_final_lead = 2) noexcept
{
	auto counts = MM_TeamLogicalCounts(
		live_red, live_blue, reserved_red, reserved_blue,
		ignored_live_side, ignored_reserved_side);

	if (desired_side == mm_team_count_side_t::None)
		return { counts, true };

	const bool joining_red = desired_side == mm_team_count_side_t::Red;
	if (!joining_red && desired_side != mm_team_count_side_t::Blue)
		return { counts, false };

	const size_t desired_count = joining_red ? counts.red : counts.blue;
	const size_t other_count = joining_red ? counts.blue : counts.red;
	// Compare the conceptual post-join lead without overflowing either count.
	const bool allowed = desired_count < other_count ||
		(maximum_final_lead > 0 &&
			desired_count - other_count < maximum_final_lead);

	if (joining_red)
		counts.red = MM_TeamSaturatingCountAdd(counts.red, 1);
	else
		counts.blue = MM_TeamSaturatingCountAdd(counts.blue, 1);

	return { counts, allowed };
}

// [MuffMode] Team management core. Implemented in muffmode/mm_team.cpp.
// Vanilla adapter names retained for high-fan-in call sites.
team_t PickTeam(int ignore_client_num);
void BroadcastTeamChange(gentity_t *ent, int old_team, bool inactive, bool silent);
bool AllowClientTeamSwitch(gentity_t *ent);
int TeamBalance(bool force);
bool TeamShuffle();
bool SetTeam(gentity_t *ent, team_t desired_team, bool inactive, bool force, bool silent);
int PlayerSortByJoinTime(const void *a, const void *b);
void MM_CmdTeam(gentity_t *ent);
void MM_CmdSetTeam(gentity_t *ent);
void MM_CmdShuffle(gentity_t *ent);
void MM_CmdBalanceTeams(gentity_t *ent);
