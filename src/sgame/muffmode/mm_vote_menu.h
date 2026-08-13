// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstdint>

struct gentity_t;
struct menu_hnd_t;

namespace muffmode::vote_menu {

struct MapMenuSourceRevision {
	const void *map_list_source = nullptr;
	std::int32_t map_list_modified = 0;
	const void *map_pool_source = nullptr;
	std::int32_t map_pool_modified = 0;
	std::uint64_t structured_pool_revision = 0;
};

inline constexpr bool MapMenuSnapshotNeedsRefresh(
	bool initialized,
	const MapMenuSourceRevision &cached,
	const MapMenuSourceRevision &current) noexcept
{
	return !initialized ||
		cached.map_list_source != current.map_list_source ||
		cached.map_list_modified != current.map_list_modified ||
		cached.map_pool_source != current.map_pool_source ||
		cached.map_pool_modified != current.map_pool_modified ||
		cached.structured_pool_revision != current.structured_pool_revision;
}

// [MuffMode] The factory submenu's list is rebuilt from a string join plus a
// COM_Parse scan per id, on every redraw for every client in the menu. Snapshot
// it on the same terms the map list uses. Lifted here, like the map predicate,
// so the refresh contract is host-testable without the game.
struct FactoryMenuSourceRevision {
	const void *votable_factories_source = nullptr;
	std::int32_t votable_factories_modified = 0;
	const void *votable_gametypes_source = nullptr;
	std::int32_t votable_gametypes_modified = 0;
	std::int32_t committed_gametype = 0;
	std::uint64_t registry_count = 0;
};

inline constexpr bool FactoryMenuSnapshotNeedsRefresh(
	bool initialized,
	const FactoryMenuSourceRevision &cached,
	const FactoryMenuSourceRevision &current) noexcept
{
	return !initialized ||
		cached.votable_factories_source != current.votable_factories_source ||
		cached.votable_factories_modified != current.votable_factories_modified ||
		cached.votable_gametypes_source != current.votable_gametypes_source ||
		cached.votable_gametypes_modified != current.votable_gametypes_modified ||
		cached.committed_gametype != current.committed_gametype ||
		cached.registry_count != current.registry_count;
}

} // namespace muffmode::vote_menu

// [MuffMode] Vote menu entry points. Implemented in muffmode/mm_vote_menu.cpp.
bool Vote_Menu_Active(gentity_t *ent);
void G_Menu_CallVote(gentity_t *ent, menu_hnd_t *p);
void G_Menu_ReturnToCallVote(gentity_t *ent, menu_hnd_t *p);
void G_Menu_Vote_Open(gentity_t *ent);
