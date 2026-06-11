// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_duel.h"

bool MM_Duel_AddPlayer()
{
	if (notGT(GT_DUEL))
		return false;

	if (level.num_playing_clients >= 2)
		return false;

	if (level.match_state > matchst_t::MATCH_WARMUP_READYUP || level.intermission_time || level.intermission_queued)
		return false;

	gclient_t *next_in_line = nullptr;

	for (auto ec : active_clients()) {
		if (ClientIsPlaying(ec->client))
			continue;

		if (!ec->client->sess.duel_queued)
			continue;

		if (!next_in_line || ec->client->sess.team_join_time < next_in_line->sess.team_join_time)
			next_in_line = ec->client;
	}

	if (!next_in_line)
		return false;

	SetTeam(&g_entities[next_in_line - game.clients + 1], TEAM_FREE, false, true, false);

	return true;
}

void MM_Duel_RemoveLoser()
{
	if (level.num_playing_clients != 2)
		return;

	gentity_t *ent = &g_entities[level.sorted_clients[1] + 1];

	if (!ent || !ent->client || !ent->client->pers.connected)
		return;

	if (g_verbose->integer)
		gi.Com_PrintFmt("Duel: Moving the loser, {}, to end of queue.\n", ent->client->resp.netname);

	SetTeam(ent, TEAM_NONE, false, true, false);
}

void MM_Duel_MatchEnd_AdjustScores()
{
	if (notGT(GT_DUEL))
		return;

	int client_num = level.sorted_clients[0];
	if (game.clients[client_num].pers.connected)
		game.clients[client_num].sess.wins++;

	client_num = level.sorted_clients[1];
	if (game.clients[client_num].pers.connected) {
		// handled in SetTeam
	}
}

void MM_Duel_QueueSpectatorBots()
{
	if (notGT(GT_DUEL) || level.intermission_time || level.intermission_queued)
		return;

	for (auto ec : active_clients()) {
		if (ClientIsPlaying(ec->client))
			continue;
		if (!(ec->client->sess.is_a_bot || ec->svflags & SVF_BOT))
			continue;
		if (ec->client->sess.duel_queued)
			continue;

		SetTeam(ec, TEAM_SPECTATOR, false, true, false);
	}
}
