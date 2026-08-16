// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;

// [MuffMode] Per-viewer team/enemy player skin overrides.
// Each player can choose how enemies and teammates look on their own screen
// (eskin/tskin) without affecting anyone else's view. Preferences live in the
// player's session config (client->sess.pc), matching the rest of the MuffMode
// preference system.
void MM_CmdEnemySkin(gentity_t *ent);
void MM_CmdTeamSkin(gentity_t *ent);
void MM_RefreshSkinOverridesForTarget(gentity_t *target);
void MM_RefreshSkinOverridesForViewer(gentity_t *viewer);
// Atomically publishes the target's canonical follow name and team skin.
bool MM_PublishCanonicalPlayerPresentation(gentity_t *target);
bool MM_ReapplySkinOverride(gentity_t *viewer, gentity_t *target);
