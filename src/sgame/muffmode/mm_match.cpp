// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_captain.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_duel.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_ghost.h"
#include "muffmode/mm_horde.h"
#include "muffmode/mm_lms.h"
#include "muffmode/mm_lms_rules.h"
#include "muffmode/mm_match.h"
#include "muffmode/mm_red_rover_rules.h"
#include "muffmode/mm_strike.h"
#include "muffmode/mm_team.h"
#include "muffmode/mm_util.h"
#include "monsters/m_player.h"	// corpse frames on match reset

#include <algorithm>
#include <cstdlib>
#include <iterator>

extern cvar_t *g_horde_champions;
extern cvar_t *g_horde_champion_max_per_run;
extern cvar_t *g_horde_champion_chance;

namespace muffmode::match {

gclient_t *SortedConnectedClient(size_t slot)
{
	if (slot >= std::size(level.sorted_clients))
		return nullptr;

	const int client_num = level.sorted_clients[slot];
	if (client_num < 0 || client_num >= static_cast<int>(game.maxclients))
		return nullptr;

	gclient_t *client = &game.clients[client_num];
	if (!client->pers.connected)
		return nullptr;

	return client;
}

bool IsConnectedClientEntity(gentity_t *ent)
{
	return ent && ent->inuse && ent->client && ent->client->pers.connected;
}

void KillAllMonsters()
{
	for (size_t i = 0; i < globals.max_entities; i++) {
		if (!g_entities[i].inuse)
			continue;
		if (g_entities[i].svflags & SVF_MONSTER)
			//if (g_entities[i].health <= 0 || g_entities[i].deadflag || (g_entities[i].svflags & SVF_DEADMONSTER))
			G_FreeEntity(&g_entities[i]);
	}
	level.total_monsters = 0;
	level.killed_monsters = 0;
}

void ResetItemTeams()
{
	// Mirror item-team spawn setup (SpawnItem in sgame/entities/items.cpp): hide every team
	// member, then let each team master re-run RespawnItem to reveal one. A single
	// iterator is used; the old code reused `ent` for inner chain walks, which
	// desynced it from `i` and walked out of bounds on every match reset.
	for (gentity_t *ent = g_entities + 1, *end = g_entities + globals.num_entities; ent < end; ++ent) {
		if (!ent->inuse)
			continue;

		if (!ent->item)
			continue;

		if (!ent->team)
			continue;

		ent->svflags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;
		gi.linkentity(ent);

		if (ent == ent->teammaster) {
			ent->nextthink = level.time + 10_hz;
			ent->think = RespawnItem;
		} else {
			ent->nextthink = 0_sec;
		}
	}
	/*
	for (ent = g_entities + 1, i = 1; i < globals.num_entities; i++, ent++) {
		if (!ent->inuse)
			continue;

		if (!ent->team)
			continue;

		if (!ent->item)
			continue;

		ent->flags &= ~FL_TEAMSLAVE;
		ent->chain = ent->teamchain;
		ent->teamchain = nullptr;

		ent->svflags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;

		if (ent == ent->teammaster) {
			ent->nextthink = level.time + 10_hz;
			if (!ent->think)
				ent->think = RespawnItem;
		} else
			ent->nextthink = 0_sec;
	}
	*/
}

/*
============
ResetEntities

Reset clients and items
============
*/
void ResetEntities(bool reset_players, bool reset_ghost, bool reset_score)
{
	// reset the players
	if (reset_players) {
		for (auto ec : active_clients()) {
			// [muff] Store bot team before reset to preserve it
			team_t saved_bot_team = TEAM_NONE;
			bool is_bot = (ec->svflags & SVF_BOT) || ec->client->sess.is_a_bot;
			if (is_bot && ec->client && ClientIsPlaying(ec->client)) {
				saved_bot_team = ec->client->sess.team;
			}

			ec->client->resp.ctf_state = 0;
			if (reset_score)
				ec->client->resp.score = 0;
			if (reset_ghost)
				MM_Ghost_ClearClient(ec);

			if (ClientIsPlaying(ec->client)) {
				if (reset_ghost) {
					// make up a ghost code
					MM_Ghost_Assign(ec);
				}
				//if (!ec->client->eliminated) {
				Weapon_Grapple_DoReset(ec->client);
				ec->svflags = SVF_NOCLIENT;
				ec->flags &= ~FL_GODMODE;

				ec->client->eliminated = false;
				ec->client->pers.dmg_scorer = 0;
				ec->client->pers.dmg_team = 0;
				ec->client->respawn_time = level.time;	// +random_time(1_sec, 4_sec);
				ec->client->pers.last_spawn_time = level.time;
				ec->client->ps.pmove.pm_type = PM_DEAD;
				ec->client->anim_priority = ANIM_DEATH;
				ec->s.frame = FRAME_death308 - 1;
				ec->client->anim_end = FRAME_death308;
				ec->deadflag = true;
				ec->movetype = MOVETYPE_NOCLIP;
				ec->client->ps.gunindex = 0;
				ec->client->ps.gunskin = 0;
				gi.linkentity(ec);
				//}
			}

			// [muff] Restore bot team assignment to keep them playing
			if (is_bot && ec->client && saved_bot_team != TEAM_NONE) {
				ec->client->sess.team = saved_bot_team;
				P_PublishEngineTeam(ec);
				// Ensure bot SVF flags are preserved
				if (ec->client->sess.is_a_bot)
					ec->svflags |= SVF_BOT;
			}
		}

		CalculateRanks();
	}
	
	// reset the level items
	Tech_Reset();
	CTF_ResetFlags();

	KillAllMonsters();

	ResetItemTeams();

	// reset item spawns and gibs/corpses, remove dropped items and projectiles
	for (gentity_t *ent = g_entities + 1, *end = g_entities + globals.num_entities; ent < end; ++ent) {
		if (!ent->inuse)
			continue;

		if (muffmode::CStringEqualsI(ent->classname, "bodyque") || muffmode::CStringEqualsI(ent->classname, "gib")) {
			ent->svflags = SVF_NOCLIENT;
			ent->takedamage = false;
			ent->solid = SOLID_NOT;
			gi.unlinkentity(ent);
			G_FreeEntity(ent);
		} else if ((ent->svflags & SVF_PROJECTILE) || (ent->clipmask & CONTENTS_PROJECTILECLIP)) {
			G_FreeEntity(ent);
		} else if (ent->item) {
			// already processed in CTF_ResetFlags()
			if (ent->item->id == IT_FLAG_RED || ent->item->id == IT_FLAG_BLUE)
				continue;

			if (ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER)) {
				//G_FreeEntity(ent);
				ent->nextthink = level.time;
			} else {
				// powerups don't spawn in for a while
				if (ent->item->flags & IF_POWERUP) {
					if (g_quadhog->integer && ent->item->id == IT_POWERUP_QUAD) {
						G_FreeEntity(ent);
						QuadHog_SetupSpawn(5_sec);
					} else if (RS(RS_MM) || RS(RS_Q3A)) {
						ent->svflags |= SVF_NOCLIENT;
						ent->solid = SOLID_NOT;

						ent->nextthink = level.time + gtime_t::from_sec(irandom(30, 60));
						//if (!ent->think)
							ent->think = RespawnItem;
					}
					continue;
				} else {
					if (ent->svflags & (SVF_NOCLIENT | SVF_RESPAWNING) || ent->solid == SOLID_NOT) {
						gtime_t t = 0_sec;
						if (ent->random) {
							t += gtime_t::from_ms((crandom() * ent->random) * 1000);
							if (t < FRAME_TIME_MS) {
								t = FRAME_TIME_MS;
							}
						}
						//if (ent->item->id == IT_HEALTH_MEGA)
							ent->think = RespawnItem;
						ent->nextthink = level.time + t;
					}
				}
			}
		}
	}

	// Reset trains that might be used by elevators
	// This fixes elevators breaking after match start when trains have nextthink set
	for (gentity_t *ent = g_entities + 1, *end = g_entities + globals.num_entities; ent < end; ++ent) {
		if (!ent->inuse)
			continue;

		// Reset trains that are not currently moving
		if (muffmode::CStringEqualsI(ent->classname, "func_train")) {
			if (ent->movetype == MOVETYPE_PUSH && !ent->velocity) {
				// Clear nextthink to allow elevators to work after match start
				ent->nextthink = 0_ms;
				// Clear target_ent so train can be reinitialized by elevator trigger
				ent->target_ent = nullptr;
			}
		}
	}
}

} // namespace muffmode::match

namespace match = muffmode::match;
#if 0
static int SortRoundScores(const void *a, const void *b) {
	gclient_t *ca, *cb;

	ca = &game.clients[*(int *)a];
	cb = &game.clients[*(int *)b];

	// sort special clients last
	if (ca->sess.spectator_client < 0)
		return 1;
	if (cb->sess.spectator_client < 0)
		return -1;

	// then connecting clients
	if (!ca->pers.connected)
		return 1;
	if (!cb->pers.connected)
		return -1;

	// then spectators
	if (!ClientIsPlaying(ca) && !ClientIsPlaying(cb)) {
		if (ca->sess.duel_queued && cb->sess.duel_queued) {
			if (ca->sess.team_join_time > cb->sess.team_join_time)
				return -1;
			if (ca->sess.team_join_time < cb->sess.team_join_time)
				return 1;
		}
		if (ca->sess.duel_queued)
			return -1;
		if (cb->sess.duel_queued)
			return 1;
		if (ca->sess.team_join_time > cb->sess.team_join_time)
			return -1;
		if (ca->sess.team_join_time < cb->sess.team_join_time)
			return 1;
		return 0;
	}
	if (!ClientIsPlaying(ca))
		return 1;
	if (!ClientIsPlaying(cb))
		return -1;

	// then sort by score
	if (ca->resp.score - ca->resp.old_score > cb->resp.score - cb->resp.old_score)
		return -1;
	if (ca->resp.score - ca->resp.old_score < cb->resp.score - cb->resp.old_score)
		return 1;

	return 0;
}

gclient_t *Round_SaveOldPlayerScore() {
	gclient_t *cl = nullptr;
	int high = 0;
	for (auto ec : active_clients()) {

		ec->client->resp.old_score = ec->client->resp.score;
	}
}
#endif
/*
=============
StartNewRound
=============
*/
namespace muffmode::match {

bool StartNewRound()
{
	if (!MM_GametypeHasFlag(GTF_ROUNDS)) {
		level.round_state = roundst_t::ROUND_NONE;
		level.round_state_timer = 0_sec;
		return false;
	}

	level.round_state = roundst_t::ROUND_COUNTDOWN;
	level.round_state_timer = level.time + gtime_t::from_sec(g_round_countdown->integer);
	level.countdown_check = 0_sec;

	if (!MM_Horde_ShouldSkipEntitiesReset())
		ResetEntities(true, false, false);

	// LMS: re-arm each player's per-round lives after the reset respawned them.
	if (GT(GT_LMS))
		MM_LMS_GrantRoundLives();

	// Red Rover: re-split the teams evenly at the start of every round, so each round opens
	// balanced after the previous one funnelled everyone onto one side.
	if (GT(GT_RR))
		TeamShuffle();

	if (GT(GT_STRIKE)) {
		// A "round" is a pair of turns: each team attacks once. strike_turn tracks which
		// turn of the pair we're starting (0 = first attacker, 1 = roles swapped). The
		// match-end gate only fires after turn 1 so both teams get equal attacking turns.
		if (level.round_number == 0) {
			// first turn of the match; strike_red_attacks already chosen in Match_Start()
			level.round_number = 1;
			level.strike_turn = 0;
		} else if (level.strike_turn == 0) {
			// second turn of the same round: swap who attacks
			level.strike_turn = 1;
			level.strike_red_attacks ^= true;
		} else {
			// both teams have attacked; begin a new round, swap back to first attacker
			level.round_number++;
			level.strike_turn = 0;
			level.strike_red_attacks ^= true;
		}
		level.strike_flag_touch = false;

		BroadcastTeamMessage(TEAM_RED, PRINT_CENTER, G_Fmt("Your team is on {}!\nRound {} - Begins in...", level.strike_red_attacks ? "OFFENSE" : "DEFENSE", level.round_number).data());
		BroadcastTeamMessage(TEAM_BLUE, PRINT_CENTER, G_Fmt("Your team is on {}!\nRound {} - Begins in...", !level.strike_red_attacks ? "OFFENSE" : "DEFENSE", level.round_number).data());
	} else {
		const int round_num = GT(GT_HORDE) ? MM_Horde_CountdownWaveNumber() : (level.round_number + 1);
		const char *round_label = GT(GT_HORDE) ? "Wave" : "Round";

		gi.LocBroadcast_Print(PRINT_CENTER, "{} {}\nBegins in...", round_label, round_num);
	}

	AnnouncerSound(world, "round_begins_in", nullptr, false);

	if (GT(GT_HORDE))
		MM_Horde_OnRoundCountdown();

	return true;
}

} // namespace muffmode::match

/*
=============
Round_End
=============
*/
void Round_End() {
	// reset if not round based
	if (!MM_GametypeHasFlag(GTF_ROUNDS)) {
		level.round_state = roundst_t::ROUND_NONE;
		level.round_state_timer = 0_sec;
		return;
	}

	// there must be a round to end
	if (level.round_state != ROUND_IN_PROGRESS)
		return;

	level.round_state = roundst_t::ROUND_ENDED;
	level.round_state_timer = level.time + 3_sec;
	MM_Horde_OnRoundEnd();
}

/*
=============
SetMatchId
=============
*/
namespace muffmode::match {

void SetMatchId()
{
	//level.match_id = gt_short_name_upper[g_gametype->integer];
	//level.match_id += "-";
	level.match_id = stime();
}

} // namespace muffmode::match

/*
============
Match_Start

Starts a match
============
*/
void Match_Start() {
	if (!deathmatch->integer)
		return;

	level.match_time = level.time;
	level.match_start_time = level.time;
	level.overtime = 0_sec;
	level.tied_overtime_start = 0_sec;

	const char *s = G_TimeString(timelimit->value ? timelimit->value * 1000 : 0, true);
	gi.configstring(CONFIG_MATCH_STATE, s);

	level.match_state = matchst_t::MATCH_IN_PROGRESS;
	level.match_state_timer = level.time;
	level.warmup_requisite = warmupreq_t::WARMUP_REQ_NONE;
	level.warmup_notice_time = 0_sec;
	level.warmup_gametype_hud_time = 0_sec;

	level.team_scores[TEAM_RED] = level.team_scores[TEAM_BLUE] = 0;

	level.total_player_deaths = 0;

	// Horde: roll this run's champion budget (none/one/two) from independent slot rolls.
	level.horde_champions_remaining = 0;
	if (GT(GT_HORDE) && g_horde_champions->integer) {
		int n = 0;
		for (int i = 0; i < g_horde_champion_max_per_run->integer; i++)
			if (frandom() < g_horde_champion_chance->value)
				n++;
		level.horde_champions_remaining = static_cast<int8_t>(n);
	}

	MM_Ghost_ClearAll();
	match::ResetEntities(true, true, true);
	UnReadyAll();
	ValidateCaptains();

	// g_match_lock: lock teams via level.locked so unlockteam can override per-team
	if (g_match_lock->integer && Teams()) {
		level.locked[TEAM_RED] = true;
		level.locked[TEAM_BLUE] = true;
	} else if (g_match_lock->integer) {
		level.locked[TEAM_FREE] = true;
	}

	match::SetMatchId();

	gi.LocBroadcast_Print(PRINT_TTS, "Match ID: {}\n", level.match_id.c_str());

	if (GT(GT_STRIKE)) {
		level.strike_red_attacks = brandom();
		level.strike_turn = 0;
		level.round_number = 0;
	}

	// Red Rover ends after roundlimit rounds, so the count must start at 0 each match.
	if (GT(GT_RR))
		level.round_number = 0;

	if (match::StartNewRound())
		return;

	gi.LocBroadcast_Print(PRINT_CENTER, "FIGHT!");
	//gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex("misc/tele_up.wav"), 1, ATTN_NONE, 0);
	AnnouncerSound(world, "fight", "misc/tele_up.wav", true);
}

/*
============
Match_Reset
============
*/
void Match_Reset() {
	//if (!g_dm_do_warmup->integer) {
	//	Match_Start();
	//	return;
	//}

	MM_Ghost_ClearAll();
	match::ResetEntities(true, false, true);
	UnReadyAll();
	ValidateCaptains();

	// clear any team locks (g_match_lock or captain locks) on reset
	level.locked[TEAM_SPECTATOR] = level.locked[TEAM_FREE] = level.locked[TEAM_RED] = level.locked[TEAM_BLUE] = false;

	level.match_time = level.time;
	level.match_state = matchst_t::MATCH_WARMUP_DEFAULT;
	level.warmup_requisite = warmupreq_t::WARMUP_REQ_NONE;
	level.warmup_notice_time = 0_sec;
	level.warmup_gametype_hud_time = level.time;
	level.match_state_timer = 0_sec;
	level.intermission_queued = 0_sec;
	level.intermission_exit = false;
	level.intermission_time = 0_sec;
	level.tied_overtime_start = 0_sec;

	CalculateRanks();

	gi.LocBroadcast_Print(PRINT_TTS, "The match has been reset.\n");
}

/*
=============
ReadyConditionsMet
=============
*/
namespace muffmode::match {

void GetWarmupReadyCounts(int &ready_humans, int &playing_humans)
{
	ready_humans = 0;
	playing_humans = 0;

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client))
			continue;
		if (ec->svflags & SVF_BOT || ec->client->sess.is_a_bot)
			continue;

		if (ec->client->resp.ready)
			ready_humans++;
		playing_humans++;
	}
}

void BroadcastWarmupWaitNotice()
{
	if (!deathmatch->integer)
		return;

	const char *msg = nullptr;

	switch (level.warmup_requisite) {
	case warmupreq_t::WARMUP_REQ_MORE_PLAYERS:
		msg = G_Fmt("Waiting for players ({} minimum)", minplayers->integer).data();
		break;
	case warmupreq_t::WARMUP_REQ_BALANCE:
		msg = "Teams are imbalanced.";
		break;
	default:
		return;
	}

	gi.LocBroadcast_Print(PRINT_CENTER, "{}", msg);
	level.warmup_notice_time = level.time;
}

static void SendWarmupReadyNudge(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	int ready_humans = 0, playing_humans = 0;
	GetWarmupReadyCounts(ready_humans, playing_humans);

	if (playing_humans > 0)
		gi.LocCenter_Print(ent, "%bind:inven:Open menu%You are NOT ready. ({}/{} ready)", ready_humans, playing_humans);
	else
		gi.LocCenter_Print(ent, "%bind:inven:Open menu%You are NOT ready.");

	ent->client->last_warmup_nudge_time = level.time;
}

void SendWarmupReadyReminder(gentity_t *ent)
{
	SendWarmupReadyNudge(ent);
}

static void TickWarmupWaitNudges()
{
	if (!deathmatch->integer)
		return;
	if (level.match_state != matchst_t::MATCH_WARMUP_DEFAULT)
		return;
	if (level.warmup_requisite != warmupreq_t::WARMUP_REQ_MORE_PLAYERS
		&& level.warmup_requisite != warmupreq_t::WARMUP_REQ_BALANCE)
		return;

	if (level.warmup_notice_time != 0_sec
		&& level.time < level.warmup_notice_time + WARMUP_READY_NUDGE_INTERVAL)
		return;

	BroadcastWarmupWaitNotice();
}

void TickWarmupReadyNudges()
{
	if (!deathmatch->integer || !muffmode::CvarEnabled(g_dm_do_readyup))
		return;
	if (level.match_state != matchst_t::MATCH_WARMUP_READYUP)
		return;

	for (auto ec : active_players()) {
		if (!ec || !ec->client)
			continue;
		if (!ClientIsPlaying(ec->client))
			continue;
		if (ec->client->sess.is_a_bot || (ec->svflags & SVF_BOT))
			continue;
		if (ec->client->resp.ready)
			continue;

		if (ec->client->last_warmup_nudge_time != 0_sec
			&& level.time < ec->client->last_warmup_nudge_time + WARMUP_READY_NUDGE_INTERVAL)
			continue;

		SendWarmupReadyNudge(ec);
	}
}

bool ReadyConditionsMet()
{
	if (!muffmode::CvarEnabled(g_dm_do_readyup))
		return true;

	int count_ready = 0;
	int count_humans = 0;
	int count_bots = 0;
	GetWarmupReadyCounts(count_ready, count_humans);
	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client))
			continue;
		if (ec->svflags & SVF_BOT || ec->client->sess.is_a_bot) {
			count_bots++;
			continue;
		}
	}

	// wait if no players at all
	if (!count_humans && !count_bots)
		return true;

	// wait if below minimum players
	const int min_players = muffmode::CvarInteger(minplayers);
	if (min_players > 0 && (count_humans + count_bots) < min_players)
		return false;

	// start if only bots
	if (!count_humans && count_bots && muffmode::CvarEnabled(g_dm_allow_no_humans))
		return true;

	// wait if no ready humans
	if (!count_ready)
		return false;

	// start if over min ready percentile
	const float ready_percentage = clamp(muffmode::CvarValue(g_warmup_ready_percentage), 0.0f, 1.0f);
	if ((static_cast<float>(count_ready) / static_cast<float>(count_humans)) >= ready_percentage)
		return true;

	return false;
}

} // namespace muffmode::match

/*
=============
CheckLastManStanding
=============
*/
namespace muffmode::match {

static void CheckLastManStanding() {
	if (notGT(GT_CA) && notGT(GT_STRIKE) && notGT(GT_RR) && notGT(GT_HORDE) && notGT(GT_LMS))
		return;

	auto announce_survivor = [](gentity_t *survivor) {
		gi.LocClient_Print(survivor, PRINT_CENTER, "You are the last one standing!");
		survivor->client->last_standing_clear_time = level.time + 3_sec;
	};

	// Horde is co-op survival and LMS is free-for-all: both put every fighter on TEAM_FREE,
	// so the "last one standing" is the final fighter still in the round. Count fighters who
	// have not been eliminated (out of lives) rather than current health - a fighter who is
	// briefly dead but still has lives will respawn, so they are not yet the last survivor.
	if (GT(GT_HORDE) || GT(GT_LMS)) {
		gentity_t *survivor = nullptr;
		int count = 0;

		for (auto ec : active_clients()) {
			if (!ClientIsPlaying(ec->client) || ec->client->eliminated)
				continue;
			count++;
			survivor = ec;
		}

		if (level.last_standing_count[TEAM_FREE] > 1 && count == 1 && survivor)
			announce_survivor(survivor);

		level.last_standing_count[TEAM_FREE] = count;
		return;
	}

	for (team_t team : { TEAM_RED, TEAM_BLUE }) {
		gentity_t *survivor = nullptr;
		int count = 0;

		for (auto ec : active_clients()) {
			if (ec->client->sess.team != team || !ClientIsPlaying(ec->client))
				continue;
			// Red Rover keeps everyone currently on the team (death there defects rather
			// than eliminates); CA/Strike count only living, non-eliminated round players.
			if (GT(GT_RR) || (!ec->client->eliminated && ec->health > 0)) {
				count++;
				survivor = ec;
			}
		}

		if (level.last_standing_count[team] > 1 && count == 1 && survivor)
			announce_survivor(survivor);

		level.last_standing_count[team] = count;
	}
}

static void CountLivingRoundPlayers(int &count_living_red, int &count_living_blue)
{
	count_living_red = 0;
	count_living_blue = 0;

	for (auto ec : active_clients()) {
		if (!ec->client || !ClientIsPlaying(ec->client) || ec->client->eliminated || ec->health <= 0)
			continue;

		if (ec->client->sess.team == TEAM_RED)
			count_living_red++;
		else if (ec->client->sess.team == TEAM_BLUE)
			count_living_blue++;
	}
}

} // namespace muffmode::match

/*
=============
TickRoundState
=============
*/
namespace muffmode::match {

void TickRoundState() {
	if (!MM_GametypeHasFlag(GTF_ROUNDS))
		return;

	if (level.match_state != matchst_t::MATCH_IN_PROGRESS)
		return;

	// initiate round
	if (level.round_state == roundst_t::ROUND_NONE || level.round_state == roundst_t::ROUND_ENDED) {
		if (level.round_state_timer > level.time)
			return;

		StartNewRound();
		return;
	}

	// start round
	if (level.round_state == roundst_t::ROUND_COUNTDOWN) {
		if (level.time >= level.round_state_timer) {
			for (auto ec : active_clients())
				ec->client->latched_buttons = BUTTON_NONE;

			level.round_state = roundst_t::ROUND_IN_PROGRESS;
			level.round_state_timer = level.time + gtime_t::from_min(roundtimelimit->value);

			for (int &c : level.last_standing_count)
				c = 0;

			if (GT(GT_RR)) {
				for (auto ec : active_clients()) {
					ec->client->resp.round_start_score = ec->client->resp.score;
					ec->client->resp.round_dmg = 0;
				}
			}

			// Strike manages round_number/turn in StartNewRound(); others advance it here.
			if (GT(GT_HORDE))
				MM_Horde_AdvanceRoundNumber();
			else if (!GT(GT_STRIKE))
				level.round_number++;

			if (GT(GT_STRIKE)) {
				gi.LocBroadcast_Print(PRINT_CHAT, "Round {}: {} is attacking!\n", level.round_number, Teams_TeamName(level.strike_red_attacks ? TEAM_RED : TEAM_BLUE));
				const char *msg[2] = { "DEFEND", "CAPTURE" };
				BroadcastTeamMessage(TEAM_RED, PRINT_CENTER, G_Fmt("Round {} has begun!\n{} THE FLAG!", level.round_number, msg[level.strike_red_attacks]).data());
				BroadcastTeamMessage(TEAM_BLUE, PRINT_CENTER, G_Fmt("Round {} has begun!\n{} THE FLAG!", level.round_number, msg[!level.strike_red_attacks]).data());
				AnnouncerSound(world, "fight", nullptr, false);
			} else if (GT(GT_HORDE)) {
				MM_Horde_OnRoundStarted();
			} else {
				gi.LocBroadcast_Print(PRINT_CHAT, "Round {} has begun!\n", level.round_number);
				gi.LocBroadcast_Print(PRINT_CENTER, "FIGHT!");
				AnnouncerSound(world, "fight", nullptr, false);
			}
		}
		return;
	}

	// end round
	if (level.round_state == roundst_t::ROUND_IN_PROGRESS) {
		CheckLastManStanding();

		auto is_living_round_player = [](gentity_t *ent) {
			return ent->client && ClientIsPlaying(ent->client) &&
				!ent->client->eliminated && ent->health > 0;
		};

		switch (MM_CurrentGametype()) {
		case GT_CA:
		{
			int count_living_red = 0, count_living_blue = 0;
			CountLivingRoundPlayers(count_living_red, count_living_blue);

			if (!count_living_red && count_living_blue) {
				G_AdjustTeamScore(TEAM_BLUE, 1);
				gi.LocBroadcast_Print(PRINT_CENTER, "{} wins the round!\n(eliminated {})\n", Teams_TeamName(TEAM_BLUE), Teams_TeamName(TEAM_RED));
				AnnouncerSound(world, "blue_wins_round", "ctf/flagcap.wav", true);
				Round_End();
				return;
			}
			if (!count_living_blue && count_living_red) {
				G_AdjustTeamScore(TEAM_RED, 1);
				gi.LocBroadcast_Print(PRINT_CENTER, "{} wins the round!\n(eliminated {})\n", Teams_TeamName(TEAM_RED), Teams_TeamName(TEAM_BLUE));
				AnnouncerSound(world, "red_wins_round", "ctf/flagcap.wav", false);
				Round_End();
				return;
			}
			break;
		}
		case GT_STRIKE:
		{
			int count_living_red = 0, count_living_blue = 0;
			CountLivingRoundPlayers(count_living_red, count_living_blue);

			const team_t attacker = MM_Strike_AttackingTeam();
			const team_t defender = MM_Strike_DefendingTeam();
			const int count_living_attacker = (attacker == TEAM_RED) ? count_living_red : count_living_blue;
			const int count_living_defender = (defender == TEAM_RED) ? count_living_red : count_living_blue;

			if (!count_living_attacker && count_living_defender) {
				MM_Strike_EndDefenseTurn(false);
				return;
			}
			if (!count_living_defender && count_living_attacker) {
				MM_Strike_AwardTurnWin(false);
				return;
			}
			if (!count_living_red && !count_living_blue) {
				if (level.num_playing_clients >= 2)
					MM_Strike_EndMutualElimination();
				return;
			}
			break;
		}
		case GT_HORDE:
			if (MM_Horde_UpdateRoundInProgress())
				Round_End();
			return;

		case GT_LMS:
		{
			// Free-for-all elimination: count participants and the active fighters among them
			// (alive, or dead but still holding a life). The round resolves to the last fighter
			// standing, a draw on mutual elimination, or a most-health tie-break at the time limit.
			int participants = 0, active = 0;
			gentity_t *survivor = nullptr;

			for (auto ec : active_clients()) {
				if (!ClientIsPlaying(ec->client))
					continue;
				participants++;
				if (MM_LMS_ClientIsActiveFighter(ec)) {
					active++;
					survivor = ec;
				}
			}

			if (MM_LMSRoundHasWinner(active, participants)) {
				G_AdjustPlayerScore(survivor->client, 1, false, 0);
				gi.LocBroadcast_Print(PRINT_CENTER, "{} wins the round!\n", survivor->client->resp.netname);
				AnnouncerSound(world, "round_won", "ctf/flagcap.wav", true);
				Round_End();
				return;
			}

			if (MM_LMSRoundIsDraw(active, participants)) {
				gi.LocBroadcast_Print(PRINT_CENTER, "Round draw!");
				Round_End();
				return;
			}

			// Round time limit: the highest-health survivor takes the round; a tie is a draw.
			// Only fighters currently alive can win the tie-break - with multi-life rounds an
			// active fighter may be momentarily dead (health <= 0) awaiting respawn, and corpse
			// health must not decide the round. If nobody is alive at expiry, it is a draw.
			const bool time_expired = roundtimelimit->value > 0 && level.time >= level.round_state_timer;
			if (time_expired && participants >= 2) {
				gentity_t *leader = nullptr;
				int best_health = 0;
				bool tied = false;

				for (auto ec : active_clients()) {
					if (!MM_LMS_ClientIsActiveFighter(ec) || ec->health <= 0)
						continue;
					if (ec->health > best_health) {
						best_health = ec->health;
						leader = ec;
						tied = false;
					} else if (ec->health == best_health) {
						tied = true;
					}
				}

				if (leader && !tied) {
					G_AdjustPlayerScore(leader->client, 1, false, 0);
					gi.LocBroadcast_Print(PRINT_CENTER, "{} wins the round!\n(most health remaining)\n", leader->client->resp.netname);
					AnnouncerSound(world, "round_won", "ctf/flagcap.wav", true);
				} else {
					gi.LocBroadcast_Print(PRINT_CENTER, "Round draw!");
				}
				Round_End();
				return;
			}
			return;
		}

		case GT_RR:
		{
			int count_red = 0, count_blue = 0;

			for (auto ec : active_clients()) {
				if (!ClientIsPlaying(ec->client))
					continue;
				if (ec->client->sess.team == TEAM_RED)
					count_red++;
				else if (ec->client->sess.team == TEAM_BLUE)
					count_blue++;
			}

			const bool team_cleared = MM_RedRoverRoundShouldEnd(count_red, count_blue);
			const bool time_expired = roundtimelimit->value > 0 && level.time >= level.round_state_timer;
			if (team_cleared || time_expired) {
				gclient_t *top = nullptr;
				int best_round_frags = 0, best_round_dmg = 0;

				for (auto ec : active_clients()) {
					if (!ClientIsPlaying(ec->client))
						continue;

					const int round_frags = ec->client->resp.score - ec->client->resp.round_start_score;
					const int round_dmg = ec->client->resp.round_dmg;
					if (!top || round_frags > best_round_frags ||
						(round_frags == best_round_frags && round_dmg > best_round_dmg)) {
						top = ec->client;
						best_round_frags = round_frags;
						best_round_dmg = round_dmg;
					}
				}

				if (top)
					gi.LocBroadcast_Print(PRINT_CENTER, "Round winner:\n{}\nwith {} {} ({} dmg)", top->resp.netname,
						best_round_frags, best_round_frags == 1 ? "frag" : "frags", best_round_dmg);
				else
					gi.LocBroadcast_Print(PRINT_CENTER, "Round over.");
				AnnouncerSound(world, "round_won", "ctf/flagcap.wav", true);

				Round_End();
			}
			return;
		}

		}

		// hit the round time limit, check any other winning conditions
		if (roundtimelimit->value > 0 && level.time >= level.round_state_timer) {
			// highest number of players remaining or highest total health wins
			if (GT(GT_CA)) {
				int living_red = 0, living_blue = 0;

				for (auto ec : active_clients()) {
					if (!is_living_round_player(ec))
						continue;

					switch (ec->client->sess.team) {
					case TEAM_RED:
						living_red++;
						break;
					case TEAM_BLUE:
						living_blue++;
						break;
					}
				}

				if (living_red > living_blue) {
					G_AdjustTeamScore(TEAM_RED, 1);
					gi.LocBroadcast_Print(PRINT_CENTER, "{} wins the round!\n(players remaining: {} vs {})\n", Teams_TeamName(TEAM_RED), living_red, living_blue);
					//gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex("ctf/flagcap.wav"), 1, ATTN_NONE, 0);
					AnnouncerSound(world, "red_wins_round", "ctf/flagcap.wav", false);
				} else if (living_blue > living_red) {
					G_AdjustTeamScore(TEAM_BLUE, 1);
					gi.LocBroadcast_Print(PRINT_CENTER, "{} wins the round!\n(players remaining: {} vs {})\n", Teams_TeamName(TEAM_BLUE), living_blue, living_red);
					//gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex("ctf/flagcap.wav"), 1, ATTN_NONE, 0);
					AnnouncerSound(world, "blue_wins_round", "ctf/flagcap.wav", true);
				} else {
					int total_health_red = 0, total_health_blue = 0;

					for (auto ec : active_players()) {
						if (!is_living_round_player(ec))
							continue;
						switch (ec->client->sess.team) {
						case TEAM_RED:
							total_health_red += ec->health;
							break;
						case TEAM_BLUE:
							total_health_blue += ec->health;
							break;
						}
					}

					if (total_health_red > total_health_blue) {
						G_AdjustTeamScore(TEAM_RED, 1);
						gi.LocBroadcast_Print(PRINT_CENTER, "{} wins the round!\n(total health: {} vs {})\n", Teams_TeamName(TEAM_RED), total_health_red, total_health_blue);
						//gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex("ctf/flagcap.wav"), 1, ATTN_NONE, 0);
						AnnouncerSound(world, "red_wins_round", "ctf/flagcap.wav", false);
					} else if (total_health_blue > total_health_red) {
						G_AdjustTeamScore(TEAM_BLUE, 1);
						gi.LocBroadcast_Print(PRINT_CENTER, "{} wins the round!\n(total health: {} vs {})\n", Teams_TeamName(TEAM_BLUE), total_health_blue, total_health_red);
						//gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex("ctf/flagcap.wav"), 1, ATTN_NONE, 0);
						AnnouncerSound(world, "blue_wins_round", "ctf/flagcap.wav", true);
					} else {
						gi.LocBroadcast_Print(PRINT_CENTER, "Round draw!");
					}
				}
			} else {
				if (GT(GT_STRIKE)) {
					MM_Strike_EndDefenseTurn(true);
					return;
				}
			}
			//gi.LocBroadcast_Print(PRINT_CENTER, "{} wins the round!\n", Teams_TeamName(TEAM_BLUE));
			Round_End();
			return;
		}
	}
}

} // namespace muffmode::match

/*
=============
TickCountdown
=============
*/
namespace muffmode::match {

void TickCountdown() {
	if ((level.match_state != matchst_t::MATCH_COUNTDOWN && level.round_state != roundst_t::ROUND_COUNTDOWN) || level.intermission_time) {
		if (level.countdown_check)
			level.countdown_check = 0_sec;
		return;
	}

	gtime_t base = (level.round_state == roundst_t::ROUND_COUNTDOWN) ? level.round_state_timer : level.match_state_timer;
	int t = (base + 1_sec - level.time).seconds<int>();
	if (t <= 0) {
		if (level.countdown_check)
			level.countdown_check = 0_sec;
		return;
	}

	if (!level.countdown_check || level.countdown_check.seconds<int>() > t) {
		if (!(t % 10) || t < 10) {
			AnnouncerSound(world, nullptr, G_Fmt("world/{}{}.wav", t, t >= 20 ? "sec" : "").data(), false);
			//gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex(G_Fmt("world/{}{}.wav", t, t >= 20 ? "sec" : "").data()), 1, ATTN_NONE, 0);
			if (t <= 3) {
				const char *s[3] = { "one", "two", "three" };
				AnnouncerSound(world, G_Fmt("{}", s[t-1]).data(), nullptr, false);
			}
		}
		level.countdown_check = gtime_t::from_sec(t);
	}
}

} // namespace muffmode::match

/*
=============
TickMatchEndWarning
=============
*/
namespace muffmode::match {

void TickMatchEndWarning() {
	if (MM_GametypeHasFlag(GTF_ROUNDS))
		return;

	if (level.match_state != matchst_t::MATCH_IN_PROGRESS || !timelimit->value) {
		if (level.matchendwarn_check)
			level.matchendwarn_check = 0_sec;
		return;
	}

	int t = (level.match_time + gtime_t::from_min(timelimit->value) - level.time).seconds<int>();	// +1;
	if (t <= 0) {
		if (level.matchendwarn_check)
			level.matchendwarn_check = 0_sec;
		return;
	}

	if (!level.matchendwarn_check || level.matchendwarn_check.seconds<int>() > t) {
		if (t && (t == 30 || t == 20 || t <= 10)) {
			AnnouncerSound(world, nullptr, G_Fmt("world/{}{}.wav", t, t >= 20 ? "sec" : "").data(), false);
			//gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex(G_Fmt("world/{}{}.wav", t, t >= 20 ? "sec" : "").data()), 1, ATTN_NONE, 0);
			if (t >= 10)
				gi.LocBroadcast_Print(PRINT_HIGH, "{} second warning!\n", t);
		} else if (t == 300 || t == 60) {
			AnnouncerSound(world, G_Fmt("{}_minute", t == 300 ? 5 : 1).data(), nullptr, false);
		}
		level.matchendwarn_check = gtime_t::from_sec(t);
	}
}

} // namespace muffmode::match

/*
=============
TickWarmupState

Once a frame, check for changes in match state during warmup
=============
*/
namespace muffmode::match {

void TickWarmupState() {
	uint8_t min_players;

	if (!level.num_playing_clients) {
		// Horde: every playing client leaving mid-match counts as a defeat;
		// run the normal end-of-match flow instead of silently abandoning the
		// wave (which would leave stale wave state and idle monsters behind).
		if (MM_Horde_CheckDesertionDefeat())
			return;

		// let a queued/running intermission complete - with no playing clients
		// it auto-exits to the next map shortly. Resetting match state here
		// would strand it.
		if (level.intermission_queued || level.intermission_time)
			return;

		if (level.match_state != matchst_t::MATCH_NONE) {
			level.match_state = matchst_t::MATCH_NONE;
			level.match_state_timer = 0_sec;
			level.warmup_requisite = warmupreq_t::WARMUP_REQ_NONE;
			level.warmup_notice_time = 0_sec;
			return;
		}
		// pull in any spectating bots
		for (auto ec : active_clients())
			if (!ClientIsPlaying(ec->client) && (ec->client->sess.is_a_bot || ec->svflags & SVF_BOT))
				SetTeam(ec, PickTeam(-1), false, false, false);
		return;
	}

	// duel: pull in a queued spectator if needed
	if (MM_Duel_AddPlayer())
		return;

	MM_Duel_QueueSpectatorBots();

	min_players = GT(GT_DUEL) ? 2 : minplayers->integer;
	if (level.match_state < matchst_t::MATCH_COUNTDOWN && !g_dm_do_warmup->integer && level.num_playing_clients >= min_players
		&& (g_dm_allow_no_humans->integer || level.num_playing_human_clients > 0)) {
		Match_Start();
		return;
	}

	// check because we run 3 game frames before calling Connect and/or ClientBegin
	// for clients on a map_restart
	if (level.match_state == matchst_t::MATCH_NONE) {
		level.match_state = matchst_t::MATCH_WARMUP_DELAYED;
		level.match_state_timer = level.time + 5_sec;
		level.warmup_requisite = warmupreq_t::WARMUP_REQ_NONE;
		level.warmup_notice_time = level.time;
		level.warmup_gametype_hud_time = level.time;
		return;
	}

	if (level.match_state == matchst_t::MATCH_WARMUP_DELAYED && level.match_state_timer > level.time)
		return;

	if (level.match_state == matchst_t::MATCH_WARMUP_DEFAULT || level.match_state == matchst_t::MATCH_WARMUP_READYUP)
		MM_Horde_RunSpawning();

	bool not_enough = false;
	bool teams_imba = false;

	// Red Rover: never let a connected client sit uninitialised (TEAM_NONE) during a
	// live match - that strands them off every team (grey tag, missing from the
	// scoreboard) and skews the per-team counts the round logic relies on. Deliberate spectators
	// (TEAM_SPECTATOR) are left alone; leaving the match is allowed.
	if (GT(GT_RR) && level.match_state == matchst_t::MATCH_IN_PROGRESS) {
		for (auto ec : active_clients())
			if (ec->client && ec->client->pers.connected && ec->client->sess.team == TEAM_NONE)
				SetTeam(ec, PickTeam(-1), false, false, false);
	}

	// Red Rover: a match ends with everyone funnelled onto one team, and that team
	// assignment carries into warmup / the next map - leaving the other side empty so the
	// next match can never reach the player/balance requirements and start. Reshuffle during
	// warmup to restore balance. NOT during a live match: there an emptied team is the
	// round-end trigger (TickRoundState), and the next round reshuffles in StartNewRound.
	if (GT(GT_RR) && level.match_state < matchst_t::MATCH_IN_PROGRESS &&
		level.num_playing_clients > 1 && (!level.num_playing_red || !level.num_playing_blue)) {
		TeamShuffle();
		CalculateRanks();
	}

	if (Teams()) {
		if (g_teamplay_force_balance->integer && std::abs(level.num_playing_red - level.num_playing_blue) > 1) {
			teams_imba = true;
		} else if (level.num_playing_red < 1 || level.num_playing_blue < 1 || level.num_playing_clients < min_players) {
			not_enough = true;
		}
	} else if (GT(GT_DUEL)) {
		if (level.num_playing_clients != 2)
			not_enough = true;
	} else if (level.num_playing_clients < min_players) {
		not_enough = true;
	}

	if (notGT(GT_DUEL)) {
		// pull in any spectating bots
		for (auto ec : active_clients())
			if (!ClientIsPlaying(ec->client) && (ec->client->sess.is_a_bot || ec->svflags & SVF_BOT))
				SetTeam(ec, PickTeam(-1), false, false, false);
	}

	if (!g_dm_allow_no_humans->integer && !level.num_playing_human_clients)
		not_enough = true;

	if (teams_imba) {
		// Cancel immediately for team imbalance
		level.match_cancel_delay_timer = 0_ms;
		if (level.match_state <= matchst_t::MATCH_COUNTDOWN) {
			if (level.match_state == matchst_t::MATCH_WARMUP_READYUP)
				UnReadyAll();
			else if (level.match_state == matchst_t::MATCH_COUNTDOWN) {
				gi.LocBroadcast_Print(PRINT_CENTER, "Countdown cancelled: teams are imbalanced\n");
				// clear locks set by g_match_lock at countdown start
				level.locked[TEAM_RED] = level.locked[TEAM_BLUE] = level.locked[TEAM_FREE] = false;
			}

			if (level.match_state != matchst_t::MATCH_WARMUP_DEFAULT) {
				const matchst_t prev_state = level.match_state;
				level.match_state = matchst_t::MATCH_WARMUP_DEFAULT;
				level.match_state_timer = 0_sec;
				level.warmup_requisite = warmupreq_t::WARMUP_REQ_BALANCE;
				level.warmup_notice_time = level.time;
				if (prev_state == matchst_t::MATCH_COUNTDOWN || prev_state == matchst_t::MATCH_WARMUP_READYUP)
					level.warmup_gametype_hud_time = level.time;
				BroadcastWarmupWaitNotice();
			}
		}
		return; // still waiting for players
	}

	if (not_enough) {
		// Add 300ms delay before cancelling to allow bot_minclients to add bots
		if (level.match_cancel_delay_timer == 0_ms) {
			level.match_cancel_delay_timer = level.time + 300_ms;
		}

		// Only cancel after delay has passed
		if (level.time >= level.match_cancel_delay_timer) {
			if (level.match_state <= matchst_t::MATCH_COUNTDOWN) {
				if (level.match_state == matchst_t::MATCH_WARMUP_READYUP)
					UnReadyAll();
				else if (level.match_state == matchst_t::MATCH_COUNTDOWN) {
					gi.LocBroadcast_Print(PRINT_CENTER, "Countdown cancelled: not enough players\n");
					// clear locks set by g_match_lock at countdown start
					level.locked[TEAM_RED] = level.locked[TEAM_BLUE] = level.locked[TEAM_FREE] = false;
				}

				if (level.match_state != matchst_t::MATCH_WARMUP_DEFAULT) {
					const matchst_t prev_state = level.match_state;
					level.match_state = matchst_t::MATCH_WARMUP_DEFAULT;
					level.match_state_timer = 0_sec;
					level.warmup_requisite = warmupreq_t::WARMUP_REQ_MORE_PLAYERS;
					level.warmup_notice_time = level.time;
					if (prev_state == matchst_t::MATCH_COUNTDOWN || prev_state == matchst_t::MATCH_WARMUP_READYUP)
						level.warmup_gametype_hud_time = level.time;
					BroadcastWarmupWaitNotice();
				}
			}
			level.match_cancel_delay_timer = 0_ms; // reset
			return; // still waiting for players
		} else {
			// Still waiting for delay, don't cancel yet
			return;
		}
	}

	// We have enough players - clear the cancellation timer
	level.match_cancel_delay_timer = 0_ms;

	if (level.match_state == matchst_t::MATCH_WARMUP_DEFAULT) {
		if (!g_dm_do_readyup->integer)
			goto countdown;
		level.match_state = matchst_t::MATCH_WARMUP_READYUP;
		level.match_state_timer = 0_sec;
		level.warmup_requisite = warmupreq_t::WARMUP_REQ_READYUP;
		level.warmup_notice_time = level.time;

		BroadcastReadyReminderMessage();
		return;
	}

	if (level.match_state > matchst_t::MATCH_COUNTDOWN)
		return;

	// if the warmup is changed at the console, restart it
	if (g_warmup_countdown->modified_count != level.warmup_modification_count) {
		level.warmup_modification_count = g_warmup_countdown->modified_count;
		level.match_state_timer = 0_sec;
		level.match_state = matchst_t::MATCH_WARMUP_DEFAULT;
		level.warmup_requisite = warmupreq_t::WARMUP_REQ_NONE;
		level.warmup_notice_time = 0_sec;
		level.warmup_gametype_hud_time = level.time;
		level.prepare_to_fight = false;
		return;
	}

	// if sufficient number of players are ready, start countdown
	if (level.match_state == matchst_t::MATCH_WARMUP_READYUP) {
		if (ReadyConditionsMet()) {
countdown:
			level.match_state = matchst_t::MATCH_COUNTDOWN;
			level.warmup_requisite = warmupreq_t::WARMUP_REQ_NONE;
			level.warmup_notice_time = 0_sec;
			KillAllMonsters();

			// lock teams on countdown if g_match_lock is enabled
			if (g_match_lock->integer) {
				if (Teams()) {
					level.locked[TEAM_RED] = true;
					level.locked[TEAM_BLUE] = true;
				} else {
					level.locked[TEAM_FREE] = true;
				}
			}

			if (g_warmup_countdown->integer > 0) {
				level.match_state_timer = level.time + gtime_t::from_sec(g_warmup_countdown->integer);

				// announce it
				gclient_t *first = SortedConnectedClient(0);
				gclient_t *second = SortedConnectedClient(1);
				if ((GT(GT_DUEL) || (level.num_playing_clients == 2 && g_match_lock->integer)) && first && second)
					gi.LocBroadcast_Print(PRINT_CENTER, "{} vs {}\nBegins in...", first->resp.netname, second->resp.netname);
				else
					gi.LocBroadcast_Print(PRINT_CENTER, "{}\nBegins in...", level.gametype_name);

				//gi.LocBroadcast_Print(PRINT_HIGH, "{}Match {} starting...\n", g_match_lock->integer ? "TEAMS LOCKED! " : "", level.match_id.data());
				if (!level.prepare_to_fight) {
					AnnouncerSound(world, (Teams() && level.num_playing_clients >= 4) ? "prepare_your_team" : "prepare_to_fight", nullptr, false);
					level.prepare_to_fight = true;
				}
			} else {
				level.match_state_timer = 0_ms;
				goto start;
			}
		}
		return;
	}

	// if the warmup time has counted down, start the match
	if (level.match_state == matchst_t::MATCH_COUNTDOWN && level.time.seconds() >= level.match_state_timer.seconds()) {
start:
		Match_Start();
		return;
	}
}

} // namespace muffmode::match

// ----------------

/*
================
MM_Match_RunFrame

Per-frame match state machine tick, called from CheckDMEndFrame in
sgame/core/runtime.cpp. Order is load-bearing: warmup -> round -> countdown ->
match-end warning.
================
*/
void MM_Match_RunFrame() {
	match::TickWarmupState();
	match::TickWarmupWaitNudges();
	match::TickWarmupReadyNudges();
	match::TickRoundState();
	match::TickCountdown();
	match::TickMatchEndWarning();
}

/*
==================
FinishTimeout
==================
*/
namespace muffmode::match {

void FinishTimeout() {
	level.timeout_in_place = 0_ms;
	level.timeout_ent = nullptr;
	level.timeout_auto = false;
	level.timeout_resuming = false;
	level.countdown_check = 0_sec;
	gi.Broadcast_Print(PRINT_CENTER, "Timeout has ended.\n");
	gi.positioned_sound(world->s.origin, world, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex("misc/tele_up.wav"), 1, ATTN_NONE, 0);
}

} // namespace muffmode::match

void MM_TimeoutBeginResumeCountdown() {
	if (level.timeout_resuming)
		return;

	const int resume_seconds = MM_ClampResumeCountdownSeconds(g_dm_timeout_resume_countdown ? g_dm_timeout_resume_countdown->integer : 30);

	MM_Ghost_DropTimedOutFlags();

	level.timeout_auto = false;
	level.timeout_ent = nullptr;
	level.timeout_resuming = true;
	level.countdown_check = 0_sec;

	if (resume_seconds <= 0) {
		match::FinishTimeout();
		return;
	}

	level.timeout_in_place = gtime_t::from_sec(resume_seconds);
	gi.LocBroadcast_Print(PRINT_CENTER, "Match resumes in {}.", G_TimeString(resume_seconds * 1000, false));
}

void TimeoutEnd() {
	if (!level.timeout_resuming) {
		MM_TimeoutBeginResumeCountdown();
		return;
	}

	match::FinishTimeout();
}

/*
==================
MM_CmdTimeIn

Ends a timeout session.
==================
*/
void MM_CmdTimeIn(gentity_t *ent) {
	if (!match::IsConnectedClientEntity(ent))
		return;

	if (!MM_IsExactArgcValid(gi.argc(), 1)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {}\n", gi.argv(0));
		return;
	}

	if (!level.timeout_in_place) {
		gi.Client_Print(ent, PRINT_HIGH, "A timeout is not currently in effect.\n");
		return;
	}
	if (level.timeout_resuming) {
		gi.Client_Print(ent, PRINT_HIGH, "The match is already resuming.\n");
		return;
	}

	if (level.timeout_ent && !match::IsConnectedClientEntity(level.timeout_ent))
		level.timeout_ent = nullptr;

	if (!ent->client->sess.admin && level.timeout_ent != ent) {
		gi.Client_Print(ent, PRINT_HIGH, "The timeout can only be ended by the timeout caller or an admin.\n");
		return;
	}

	gi.LocBroadcast_Print(PRINT_HIGH, "{} is resuming the match.\n", ent->client->pers.netname);
	MM_TimeoutBeginResumeCountdown();
}

/*
==================
MM_CmdTimeOut

Calls a timeout session.
==================
*/
void MM_CmdTimeOut(gentity_t *ent) {
	if (!match::IsConnectedClientEntity(ent))
		return;

	if (!MM_IsExactArgcValid(gi.argc(), 1)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {}\n", gi.argv(0));
		return;
	}

	const int timeout_seconds = MM_ClampTimeoutSeconds(g_dm_timeout_length->integer);
	if (timeout_seconds <= 0) {
		gi.Client_Print(ent, PRINT_HIGH, "Server has disabled timeouts.\n");
		return;
	}
	if (level.match_state != MATCH_IN_PROGRESS) {
		gi.Client_Print(ent, PRINT_HIGH, "Timeouts can only be issued during a match.\n");
		return;
	}
	if (ent->client->pers.timeout_used && !ent->client->sess.admin) {
		gi.Client_Print(ent, PRINT_HIGH, "You have already used your timeout.\n");
		return;
	}
	if (level.timeout_in_place > 0_ms) {
		gi.Client_Print(ent, PRINT_HIGH, "A timeout is already in progress.\n");
		return;
	}

	level.timeout_ent = ent;
	level.timeout_auto = false;
	level.timeout_resuming = false;
	level.timeout_in_place = gtime_t::from_sec(timeout_seconds);
	level.countdown_check = 0_sec;
	gi.LocBroadcast_Print(PRINT_CENTER, "{} called a timeout!\n{} has been granted.", ent->client->resp.netname, G_TimeString(timeout_seconds * 1000, false));
	gi.positioned_sound(world->s.origin, world, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex("world/klaxon2.wav"), 1, ATTN_NONE, 0);
	ent->client->pers.timeout_used = true;
}
