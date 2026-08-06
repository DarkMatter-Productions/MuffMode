// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;

struct mm_client_reference_clear_policy_t {
	bool active_follow = false;
	bool queued_follow = false;
};

template<typename T>
struct mm_monster_client_reference_set_t {
	const T *current_enemy = nullptr;
	const T *old_enemy = nullptr;
	const T *goal_entity = nullptr;
	const T *move_target = nullptr;
	const T *last_player_enemy = nullptr;
	const T *damage_attacker = nullptr;
	const T *damage_inflictor = nullptr;
	const T *ground_entity = nullptr;
};

struct mm_monster_client_reference_clear_policy_t {
	bool current_enemy = false;
	bool old_enemy = false;
	bool goal_entity = false;
	bool move_target = false;
	bool last_player_enemy = false;
	bool damage_attacker = false;
	bool damage_inflictor = false;
	bool ground_entity = false;
	bool lost_current_target = false;
};

// Client entity slots are reusable. Decide which viewer-side references belong
// to the lifetime that is ending before the slot can represent another player.
template<typename T>
constexpr mm_client_reference_clear_policy_t MM_ClientReferenceClearPolicy(
	const T *active_follow, const T *queued_follow, const T *departing) noexcept
{
	return {
		departing && active_follow == departing,
		departing && queued_follow == departing
	};
}

// A player's two noise entities are part of the same client lifetime for AI
// purposes: they retain owner == departing and can otherwise lead monsters to
// a later occupant of the reusable client slot.
template<typename T>
constexpr bool MM_ClientLifetimeReferenceMatches(
	const T *reference,
	const T *departing,
	const T *noise_primary = nullptr,
	const T *noise_secondary = nullptr) noexcept
{
	return departing && reference &&
		(reference == departing ||
		 (noise_primary && reference == noise_primary) ||
		 (noise_secondary && reference == noise_secondary));
}

template<typename T>
constexpr mm_monster_client_reference_clear_policy_t
MM_MonsterClientReferenceClearPolicy(
	const mm_monster_client_reference_set_t<T> &references,
	const T *departing,
	const T *noise_primary = nullptr,
	const T *noise_secondary = nullptr) noexcept
{
	const auto matches = [=](const T *reference) constexpr noexcept {
		return MM_ClientLifetimeReferenceMatches(
			reference, departing, noise_primary, noise_secondary);
	};
	const bool current_enemy = matches(references.current_enemy);
	return {
		current_enemy,
		matches(references.old_enemy),
		matches(references.goal_entity),
		matches(references.move_target),
		matches(references.last_player_enemy),
		matches(references.damage_attacker),
		matches(references.damage_inflictor),
		matches(references.ground_entity),
		current_enemy
	};
}

// Clears raw client-entity references while the departing entity still carries
// the identity and presentation state needed by the normal follow cleanup.
void MM_ClearDepartingClientReferences(gentity_t *departing);
