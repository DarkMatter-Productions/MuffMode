// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstdint>

struct gentity_t;

// [MuffMode] Gib and debris presentation, developed alongside the WORR gib pass.
// Vanilla throws a pile of identical meat chunks that tumble on random angular
// velocity, land silently, and sink through the floor. Everything here is driven
// from stock rerelease assets that ship in baseq2 but that no vanilla code path
// uses: the impact sounds player/gibimp1-3.wav and the leg gib model in
// particular. No custom content is required.
//
// Include this after g_local.h. gib_type_t is deliberately kept out of the
// signatures so the header can also be pulled in through muffmode/muffmode.h,
// which is included in places that have not yet defined it.

// Registers the extra stock assets the enhanced paths reference. Called from the
// shared precache pass, so a server running with the feature off still pays only
// a handful of precache slots and never a late runtime configstring update.
void MM_Gibs_Precache();

// Drops every tracked gib reference and resets the effect budget. Any caller that
// wipes or reloads the entity array must run this first, otherwise the live-gib
// budget can free a slot that has since been recycled into something else.
void MM_Gibs_ClearAll();

// True when the enhanced presentation is active. Callers in vanilla files use
// this to pick between the MuffMode lifetime and id's original one, so that
// g_gib_enhanced 0 is a genuine byte-for-byte fallback rather than a half state.
bool MM_Gibs_Enhanced();

// Bounds a freshly thrown gib's launch velocity. Vanilla clamps each axis
// independently, which squares off the spray direction and throws away whatever
// knockback the killing blow already applied to the corpse. This scales the
// horizontal component instead, so the direction of the blow survives.
void MM_Gibs_ClipVelocity(gentity_t *gib);

// Final pass over a freshly thrown gib: lighting, scale jitter, spin floor, and
// the MuffMode think/touch pair. `organic` separates meat and bone from metal
// debris for impact effects; `upright` preserves GIB_UPRIGHT's settle behaviour.
// The caller must have set gib->timestamp to the time the gib should start
// sinking before calling this.
void MM_Gibs_Finalize(gentity_t *gib, bool organic, bool upright);

// Registers a gib against the live-gib budget, freeing the oldest tracked gib
// once the cap is reached. Only pass entities that came from G_Spawn: a head gib
// is the corpse itself and must never be recycled out from under its owner.
void MM_Gibs_Track(gentity_t *gib);

// Throws the player death gib set. Vanilla tosses 8 to 46 identical sm_meat
// chunks; this scales the composition by how far past the gib threshold the
// victim went and mixes in the limb, bone and torso models that the base game
// already precaches for monsters but never gives to players.
void MM_Gibs_ThrowPlayerGibs(gentity_t *self, int32_t damage);

// Number of gib models a player death can throw at the deepest overkill, used by
// the host tests to keep the composition table and the entity budget in step.
int32_t MM_Gibs_PlayerGibCeiling();

// Severity tier (1-4) for a victim's post-mortem health, exposed for host tests.
int32_t MM_Gibs_Severity(int32_t health);
