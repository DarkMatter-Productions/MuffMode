// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;
struct gclient_t;

// [MuffMode] Deathmatch spawn health, armor, and weapon loadout.
bool MM_UsesArenaSpawnLoadout();
void MM_ApplyStartingHealthArmor(gentity_t *ent, gclient_t *client);
void MM_ApplySpawnLoadout(gentity_t *ent, gclient_t *client, bool taken_loadout);
