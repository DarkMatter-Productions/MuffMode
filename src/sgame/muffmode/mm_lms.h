// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;

// [MuffMode] GT_LMS (Last Man Standing) per-round lives handling, mirroring the
// Horde lives model: each round a player gets g_lms_lives lives; dying spends a
// life and respawns mid-round with the arena loadout until the last life is lost,
// at which point the player is eliminated and spectates until the next round.

// max(1, g_lms_lives) - the number of lives granted at the start of each round.
int  MM_LMS_LivesPerRound();

// A playing, non-eliminated client who is alive OR still has a life left. This is
// the unit counted for round-end (last-standing) decisions.
bool MM_LMS_ClientIsActiveFighter(gentity_t *ec);

// Called from Round_StartNew(): (re)grant each playing client their per-round
// lives. The round reset (ResetEntities) already respawns players and clears the
// eliminated flag, so this only re-arms the life counter.
void MM_LMS_GrantRoundLives();

// Called from player_die() after death setup: spend a life and, when none remain,
// eliminate the player into spectate. Otherwise notify remaining lives.
void MM_LMS_OnPlayerDeath(gentity_t *ent);
