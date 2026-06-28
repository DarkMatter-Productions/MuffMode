// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_lms.h"

// GT_LMS lives handling. Parallels the Horde lives flow (mm_horde.cpp) but applied
// to a free-for-all arena round rather than co-op survival: there are no teams and
// no monsters, just the last fighter standing.

int MM_LMS_LivesPerRound()
{
	return max(1, g_lms_lives->integer);
}

bool MM_LMS_ClientIsActiveFighter(gentity_t *ec)
{
	if (!ec->client || !ClientIsPlaying(ec->client))
		return false;
	if (ec->client->eliminated)
		return false;
	if (ec->health > 0)
		return true;

	return ec->client->pers.lives > 0;
}

void MM_LMS_GrantRoundLives()
{
	if (notGT(GT_LMS))
		return;

	const int lives = MM_LMS_LivesPerRound();

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client))
			continue;

		// Round_StartNew()'s ResetEntities() has already respawned everyone and
		// cleared the eliminated flag for LMS (it is not a Horde-style reset skip),
		// so we only need to re-arm the per-round life counter here.
		ec->client->pers.lives = lives;
		ec->client->eliminated = false;
	}
}

void MM_LMS_OnPlayerDeath(gentity_t *ent)
{
	if (notGT(GT_LMS))
		return;
	if (level.round_state != roundst_t::ROUND_IN_PROGRESS)
		return;
	if (!ent || !ent->client || !ClientIsPlaying(ent->client))
		return;
	if (ent->client->eliminated)
		return;

	if (ent->client->pers.lives > 0)
		ent->client->pers.lives--;

	if (ent->client->pers.lives <= 0) {
		ClientSetEliminated(ent);
		ent->client->respawn_time = level.time + 1_sec;
		CalculateRanks();
		gi.LocClient_Print(ent, PRINT_CENTER, "You have been eliminated!\nYou will spectate until the next round.");
	} else {
		gi.LocClient_Print(ent, PRINT_CENTER, "You have {} {} remaining.",
			ent->client->pers.lives, ent->client->pers.lives == 1 ? "life" : "lives");
	}
}
