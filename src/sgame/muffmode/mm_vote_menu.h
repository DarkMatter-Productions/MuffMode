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
		cached.map_pool_modified != current.map_pool_modified;
}

} // namespace muffmode::vote_menu

// [MuffMode] Vote menu entry points. Implemented in muffmode/mm_vote_menu.cpp.
bool Vote_Menu_Active(gentity_t *ent);
void G_Menu_CallVote(gentity_t *ent, menu_hnd_t *p);
void G_Menu_ReturnToCallVote(gentity_t *ent, menu_hnd_t *p);
void G_Menu_Vote_Open(gentity_t *ent);
