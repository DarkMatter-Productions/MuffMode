// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_scoring.h"

#include <algorithm>
#include <iterator>

int MM_AwardDamageScore(gclient_t *client, int damage)
{
	if (!client || damage <= 0)
		return 0;
	client->pers.dmg_scorer += damage;
	const int score = client->pers.dmg_scorer / 100;
	if (score <= 0)
		return 0;
	client->pers.dmg_scorer -= score * 100;
	G_AdjustPlayerScore(client, score, false, 0);
	return score;
}

void MM_ResetClientScoring(gclient_t *client)
{
	if (!client)
		return;
	client->resp.score = 0;
	client->resp.round_dmg = 0;
	client->pers.dmg_scorer = 0;
	std::fill(std::begin(client->resp.mstats), std::end(client->resp.mstats), 0);
}
