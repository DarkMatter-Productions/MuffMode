// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "mm_map_pick_rules.h"

struct gentity_t;

// [MuffMode] Post-scoreboard next-map pick, ported from WORR's MapSelector.
// Once everyone has read the end-of-match scoreboard the level would normally
// change straight away; instead the intermission camera holds and the players
// choose the next map from three candidates drawn out of the rotation.
//
// WORR splits the intermission into a scoreboard phase and a "post intermission"
// phase carrying its own exit timer. MuffMode has no such phase, so the pick
// claims the exit itself: every deathmatch path that would call ExitLevel() from
// intermission offers it here first, and the pick calls ExitLevel() once a map
// has been chosen and shown.

// Registers g_map_pick. Called from the main cvar registration pass.
void MM_MapPick_RegisterCvars();

// Drops all pick state. Called wherever level_locals_t is reconstructed, since
// the pick keeps its state module-side rather than in `level`.
void MM_MapPick_Reset();

// Records whether the next map came from the rotation (open to a pick) or was
// deliberately chosen -- a MyMap queue entry, g_dm_same_level or a forced map --
// in which case the pick stands down.
void MM_MapPick_NoteNextMapIsOpen(bool open);

// True while the pick owns the intermission, from the moment it opens until it
// calls ExitLevel(). Vanilla intermission handling defers to it in that window.
bool MM_MapPick_Active();

// Offered the exit by an intermission path that was about to call ExitLevel().
// Returns true when the pick has taken it over, false to exit as normal.
bool MM_MapPick_ClaimIntermissionExit();

// Advances the open pick: syncs tallies, catches late joiners, finalizes on
// timeout, and exits the level once the winner has been shown.
void MM_MapPick_RunFrame();

// True when ent is looking at the pick menu, which is drawn in place of the
// intermission scoreboard and takes the usercmd that would otherwise skip out.
bool MM_MapPick_MenuOpen(const gentity_t *ent);

// Sends ent its pick menu in place of the intermission scoreboard, on the pick's
// own refresh cadence. Returns true when the pick owned the client's layout this
// frame, in which case the caller must not send the scoreboard over the top.
bool MM_MapPick_DrawIntermissionMenu(gentity_t *ent);

// Retires a leaving client's vote so it cannot decide the pick in their absence.
void MM_MapPick_ClearClientVote(gentity_t *ent);

// Player command body: `mappick [1-3]`.
void MM_CmdMapPick(gentity_t *ent);
