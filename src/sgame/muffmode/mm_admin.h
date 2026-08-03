// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstddef>

struct gentity_t;

namespace muffmode::admin {

enum class mm_admin_attempt_result_t {
	Accepted,
	InvalidCredentials,
	Throttled
};

// Passwords are secrets, not identifiers: preserve exact byte/case semantics.
// The equal-length comparison examines every byte to avoid prefix-dependent
// early exits in the authentication path.
inline bool MM_AdminPasswordMatches(const char *configured, const char *supplied) noexcept
{
	if (!configured || !supplied || !*configured || !*supplied)
		return false;

	size_t configured_length = 0;
	while (configured[configured_length])
		configured_length++;

	size_t supplied_length = 0;
	while (supplied[supplied_length])
		supplied_length++;

	if (configured_length != supplied_length)
		return false;

	unsigned char difference = 0;
	for (size_t i = 0; i < configured_length; i++)
		difference |= static_cast<unsigned char>(configured[i]) ^
			static_cast<unsigned char>(supplied[i]);

	return difference == 0;
}

inline mm_admin_attempt_result_t MM_EvaluateAdminAttempt(
	bool flood_blocked, const char *configured, const char *supplied) noexcept
{
	if (flood_blocked)
		return mm_admin_attempt_result_t::Throttled;

	return MM_AdminPasswordMatches(configured, supplied)
		? mm_admin_attempt_result_t::Accepted
		: mm_admin_attempt_result_t::InvalidCredentials;
}

} // namespace muffmode::admin

// [MuffMode] Admin command bodies. sgame/core/commands.cpp keeps command-table wrappers.
void MM_CmdDoctor(gentity_t *ent);
void MM_CmdGametype(gentity_t *ent);
void MM_CmdRuleset(gentity_t *ent);
void MM_CmdSetMap(gentity_t *ent);
void MM_CmdStartMatch(gentity_t *ent);
void MM_CmdEndMatch(gentity_t *ent);
void MM_CmdResetMatch(gentity_t *ent);
void MM_CmdForceVote(gentity_t *ent);
void MM_CmdMapRestart(gentity_t *ent);
void MM_CmdNextMap(gentity_t *ent);
void MM_CmdAdmin(gentity_t *ent);
