// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <string>
#include <vector>

struct gentity_t;

namespace muffmode::maps {

// Typed helpers for internal MuffMode systems that need the sanitized map pool/list.
std::vector<std::string> CollectConfiguredMaps();
bool ContainsConfiguredMap(const char *mapname);

} // namespace muffmode::maps

// [MuffMode] Map-list module entry points.
bool MM_IsSafeMapToken(const char *mapname);
void MM_ShuffleMapList();
void MM_GametypeChangeMapFirst();
bool MM_TryBeginIntermissionFromMapList();
void MM_HandleMapShuffleCvarChange();

// [MuffMode] MyMap queue (game.mapqueue state remains in game_locals_t).
int MM_MQ_Count();
bool MM_MQ_Add(gentity_t *ent, const char *mapname);
const char *MM_MQ_Go_Next();
void MM_CmdMapList(gentity_t *ent);
void MM_CmdMyMap(gentity_t *ent);
