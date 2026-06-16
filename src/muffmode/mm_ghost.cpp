// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_ghost.h"
#include "muffmode/mm_parse.h"

/*
================
MM_Ghost_Assign

Assigns the player a ghost slot and code so they can reclaim their
score with the "ghost" command if they lose connection mid-match.
================
*/
void MM_Ghost_Assign(gentity_t *ent) {
	int ghost, i;

	for (ghost = 0; ghost < MAX_CLIENTS_KEX; ghost++)
		if (!level.ghosts[ghost].code)
			break;
	if (ghost == MAX_CLIENTS_KEX)
		return;
	level.ghosts[ghost].team = ent->client->sess.team;
	level.ghosts[ghost].score = 0;
	for (;;) {
		level.ghosts[ghost].code = irandom(10000, 100000);
		for (i = 0; i < MAX_CLIENTS_KEX; i++)
			if (i != ghost && level.ghosts[i].code == level.ghosts[ghost].code)
				break;
		if (i == MAX_CLIENTS_KEX)
			break;
	}
	level.ghosts[ghost].ent = ent;
	Q_strlcpy(level.ghosts[ghost].netname, ent->client->resp.netname, sizeof(level.ghosts[ghost].netname));
	ent->client->resp.ghost = level.ghosts + ghost;
	gi.LocClient_Print(ent, PRINT_CHAT, "Your ghost code is **** {} ****\n", level.ghosts[ghost].code);
	gi.LocClient_Print(ent, PRINT_HIGH, "If you lose connection, you can rejoin with your score intact by typing \"ghost {}\".\n",
		level.ghosts[ghost].code);
}

/*
================
MM_Ghost_DoAssign
================
*/
void MM_Ghost_DoAssign(gentity_t *ent) {
	// assign a ghost code
	if (level.match_state == matchst_t::MATCH_IN_PROGRESS) {
		if (ent->client->resp.ghost)
			ent->client->resp.ghost->code = 0;
		ent->client->resp.ghost = nullptr;
		MM_Ghost_Assign(ent);
	}
}

/*
================
MM_CmdGhost

"ghost <code>" reinstates a disconnected player's position and score.
================
*/
void MM_CmdGhost(gentity_t *ent) {
	int i, n;

	if (!MM_IsExactArgcValid(gi.argc(), 2)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <code>\n", gi.argv(0));
		return;
	}

	// throttle code guesses to the flood rate to deter brute-forcing
	if (CheckFlood(ent))
		return;

	if (ClientIsPlaying(ent->client)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You are already in the game.\n");
		return;
	}
	if (level.match_state != matchst_t::MATCH_IN_PROGRESS) {
		gi.LocClient_Print(ent, PRINT_HIGH, "No match is in progress.\n");
		return;
	}

	const auto code = MM_ParseIntArg(gi.argv(1));
	if (!code || *code < 10000 || *code > 99999) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid ghost code.\n");
		return;
	}
	n = *code;

	for (i = 0; i < MAX_CLIENTS_KEX; i++) {
		if (level.ghosts[i].code && level.ghosts[i].code == n) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Ghost code accepted, your position has been reinstated.\n");
			if (level.ghosts[i].ent && level.ghosts[i].ent->client && level.ghosts[i].ent->client->resp.ghost == &level.ghosts[i])
				level.ghosts[i].ent->client->resp.ghost = nullptr;
			ent->client->sess.team = level.ghosts[i].team;
			ent->client->resp.ghost = level.ghosts + i;
			ent->client->resp.score = level.ghosts[i].score;
			ent->client->resp.ctf_state = 0;
			level.ghosts[i].ent = ent;
			ent->svflags = SVF_NONE;
			ent->flags &= ~FL_GODMODE;
			ClientSpawn(ent);
			gi.LocBroadcast_Print(PRINT_HIGH, "{} has been reinstated to {} team.\n",
				ent->client->resp.netname, Teams_TeamName(ent->client->sess.team));
			return;
		}
	}
	gi.LocClient_Print(ent, PRINT_HIGH, "Invalid ghost code.\n");
}
