// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "muffmode/mm_client_refs.h"

namespace {

struct client_lifetime_t {};

constexpr client_lifetime_t departing {};
constexpr client_lifetime_t other {};
constexpr client_lifetime_t noise_primary {};
constexpr client_lifetime_t noise_secondary {};

constexpr auto both = MM_ClientReferenceClearPolicy(
	&departing, &departing, &departing);
static_assert(both.active_follow && both.queued_follow);

constexpr auto queued_only = MM_ClientReferenceClearPolicy(
	&other, &departing, &departing);
static_assert(!queued_only.active_follow && queued_only.queued_follow);

constexpr auto unrelated = MM_ClientReferenceClearPolicy(
	&other, &other, &departing);
static_assert(!unrelated.active_follow && !unrelated.queued_follow);

constexpr auto no_departing_lifetime = MM_ClientReferenceClearPolicy(
	&departing, &departing, static_cast<const client_lifetime_t *>(nullptr));
static_assert(!no_departing_lifetime.active_follow &&
	!no_departing_lifetime.queued_follow);

constexpr auto all_monster_references =
	MM_MonsterClientReferenceClearPolicy(
		mm_monster_client_reference_set_t<client_lifetime_t> {
			&departing,
			&departing,
			&departing,
			&departing,
			&departing,
			&departing,
			&departing,
			&departing
		},
		&departing,
		&noise_primary,
		&noise_secondary);
static_assert(all_monster_references.current_enemy);
static_assert(all_monster_references.old_enemy);
static_assert(all_monster_references.goal_entity);
static_assert(all_monster_references.move_target);
static_assert(all_monster_references.last_player_enemy);
static_assert(all_monster_references.damage_attacker);
static_assert(all_monster_references.damage_inflictor);
static_assert(all_monster_references.ground_entity);
static_assert(all_monster_references.lost_current_target);

constexpr auto noise_aliases = MM_MonsterClientReferenceClearPolicy(
	mm_monster_client_reference_set_t<client_lifetime_t> {
		&noise_primary,
		&noise_secondary,
		&noise_secondary,
		&other,
		&departing,
		&noise_primary,
		&other,
		&noise_secondary
	},
	&departing,
	&noise_primary,
	&noise_secondary);
static_assert(noise_aliases.current_enemy);
static_assert(noise_aliases.old_enemy);
static_assert(noise_aliases.goal_entity);
static_assert(!noise_aliases.move_target);
static_assert(noise_aliases.last_player_enemy);
static_assert(noise_aliases.damage_attacker);
static_assert(!noise_aliases.damage_inflictor);
static_assert(noise_aliases.ground_entity);
static_assert(noise_aliases.lost_current_target);

constexpr auto unrelated_path_target =
	MM_MonsterClientReferenceClearPolicy(
		mm_monster_client_reference_set_t<client_lifetime_t> {
			&departing,
			&other,
			&other,
			&other,
			&other,
			&other,
			&other,
			&other
		},
		&departing,
		&noise_primary,
		&noise_secondary);
static_assert(unrelated_path_target.current_enemy);
static_assert(!unrelated_path_target.goal_entity);
static_assert(!unrelated_path_target.move_target);
static_assert(unrelated_path_target.lost_current_target);

constexpr auto no_monster_departing_lifetime =
	MM_MonsterClientReferenceClearPolicy(
		mm_monster_client_reference_set_t<client_lifetime_t> {
			&departing,
			&noise_primary,
			&noise_secondary,
			&departing,
			&departing,
			&departing,
			&departing,
			&departing
		},
		static_cast<const client_lifetime_t *>(nullptr),
		&noise_primary,
		&noise_secondary);
static_assert(!no_monster_departing_lifetime.current_enemy);
static_assert(!no_monster_departing_lifetime.old_enemy);
static_assert(!no_monster_departing_lifetime.goal_entity);
static_assert(!no_monster_departing_lifetime.move_target);
static_assert(!no_monster_departing_lifetime.last_player_enemy);
static_assert(!no_monster_departing_lifetime.damage_attacker);
static_assert(!no_monster_departing_lifetime.damage_inflictor);
static_assert(!no_monster_departing_lifetime.ground_entity);
static_assert(!no_monster_departing_lifetime.lost_current_target);

} // namespace
