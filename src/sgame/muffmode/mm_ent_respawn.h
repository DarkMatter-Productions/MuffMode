// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "mm_ent_respawn_rules.h"

#include <cstddef>

struct gentity_t;

// [MuffMode] Deathmatch prop respawning, ported from WORR. Vanilla destroys a
// misc_explobox or func_explosive once and the map is poorer for the rest of the
// match; WORR records each prop's spawn-time state and rebuilds it later from that
// record. MuffMode keeps the records module-side rather than on gentity_t: the
// entity slot is recycled the moment the prop dies, and a raw pointer field on the
// vanilla struct would also have to be taught to the savegame writer.
//
// Only map-authored props are tracked. A prop produced at runtime by
// target_spawner belongs to whatever script made it, and respawning those forever
// would grow the record set without bound.

// Records are TAG_LEVEL-string-referencing, so every caller that is about to free
// TAG_LEVEL or wipe the entity array must clear them first.
void MM_EntRespawn_ClearAll();

// Captures ent's spawn-time state if its class respawns. Called once per
// map-authored entity, straight after its spawn function has run so that defaulted
// health, bounds and damage are already in place.
void MM_EntRespawn_CaptureMapEntity(gentity_t *ent);

// Queues ent's recorded prop for a later rebuild. Returns false when the entity is
// untracked or the feature is disabled, in which case the prop stays destroyed.
bool MM_EntRespawn_Schedule(gentity_t *ent);

// Makes every queued rebuild eligible immediately. Round resets that cannot
// rebuild the world from the cached entity lump use this so a new round does not
// start with the previous round's props still missing.
void MM_EntRespawn_ExpediteAll();

// Retries queued rebuilds whose delay has elapsed. Placement rules can push any
// individual attempt back by MM_ENT_RESPAWN_RETRY_SECONDS.
void MM_EntRespawn_RunFrame();
