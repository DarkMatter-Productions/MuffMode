// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;

// [MuffMode] Map entity classname remaps and spawn-key filtering.
void MM_RemapSpawnClassname(gentity_t *ent);
bool MM_ShouldInhibitSpawnEntity(gentity_t *ent);
