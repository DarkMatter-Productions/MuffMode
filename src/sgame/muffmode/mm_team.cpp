// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_captain.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_duel.h"
#include "muffmode/mm_ghost.h"
#include "muffmode/mm_match.h"
#include "muffmode/mm_match_stats.h"
#include "muffmode/mm_player_name.h"
#include "muffmode/mm_player_stats.h"
#include "muffmode/mm_red_rover_rules.h"
#include "muffmode/mm_statusbar.h"
#include "muffmode/mm_team.h"
#include "muffmode/mm_util.h"
#include "muffmode/mm_vote.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string>
#include <vector>

/*
=================
PlayerSortByJoinTime
=================
*/
namespace muffmode::team {

bool IsClientIndexInRange(int client_num)
{
	return client_num >= 0 && static_cast<size_t>(client_num) < game.maxclients;
}

bool IsLiveRankedPlayer(const gentity_t *ent)
{
	return ent && ent->inuse && ent->client &&
		ent->client->pers.connected && ClientIsPlaying(ent->client);
}

bool IsHumanPlayer(const gentity_t *ent)
{
	return ent && ent->client && !ent->client->sess.is_a_bot &&
		!(ent->svflags & SVF_BOT);
}

struct LogicalTeamView {
	size_t red_count = 0;
	size_t blue_count = 0;
	int64_t red_score = 0;
	int64_t blue_score = 0;
};

LogicalTeamView BuildLogicalTeamView(const gentity_t *ignore)
{
	const size_t live_red = static_cast<size_t>(
		std::max(level.num_playing_red, 0));
	const size_t live_blue = static_cast<size_t>(
		std::max(level.num_playing_blue, 0));
	const size_t reserved_red =
		MM_Ghost_ActivePlayingReservationCountForTeam(TEAM_RED);
	const size_t reserved_blue =
		MM_Ghost_ActivePlayingReservationCountForTeam(TEAM_BLUE);
	const mm_team_count_side_t ignored_live_side =
		IsLiveRankedPlayer(ignore) && ignore->client->sess.team == TEAM_RED
			? mm_team_count_side_t::Red
			: IsLiveRankedPlayer(ignore) && ignore->client->sess.team == TEAM_BLUE
				? mm_team_count_side_t::Blue
				: mm_team_count_side_t::None;
	mm_team_count_side_t ignored_reserved_side = mm_team_count_side_t::None;
	if (ignore) {
		if (reserved_red !=
			MM_Ghost_ActivePlayingReservationCountForTeam(TEAM_RED, ignore))
			ignored_reserved_side = mm_team_count_side_t::Red;
		else if (reserved_blue !=
			MM_Ghost_ActivePlayingReservationCountForTeam(TEAM_BLUE, ignore))
			ignored_reserved_side = mm_team_count_side_t::Blue;
	}
	const mm_team_logical_counts_t counts = MM_TeamLogicalCounts(
		live_red, live_blue, reserved_red, reserved_blue,
		ignored_live_side, ignored_reserved_side);
	LogicalTeamView view{ counts.red, counts.blue };

	for (size_t i = 0; i < game.maxclients; ++i) {
		const gentity_t *slot = &g_entities[i + 1];
		if (slot == ignore || !IsLiveRankedPlayer(slot))
			continue;
		if (slot->client->sess.team == TEAM_RED)
			view.red_score += static_cast<int64_t>(slot->client->resp.score);
		else if (slot->client->sess.team == TEAM_BLUE)
			view.blue_score += static_cast<int64_t>(slot->client->resp.score);
	}
	view.red_score += MM_Ghost_ActivePlayingReservationScoreForTeam(
		TEAM_RED, ignore);
	view.blue_score += MM_Ghost_ActivePlayingReservationScoreForTeam(
		TEAM_BLUE, ignore);

	return view;
}

bool HumanPlayerLimitReached(const gentity_t *ignore = nullptr)
{
	const int limit = CvarInteger(maxplayers);
	if (limit <= 0)
		return false;

	int64_t logical_humans = std::max<int64_t>(
		static_cast<int64_t>(level.num_playing_human_clients), 0);
	if (IsLiveRankedPlayer(ignore) && IsHumanPlayer(ignore) && logical_humans > 0)
		--logical_humans;
	logical_humans += static_cast<int64_t>(
		MM_Ghost_ActiveHumanPlayingReservationCount(ignore));
	return logical_humans >= limit;
}

std::string DisplayName(gentity_t *ent)
{
	const std::string_view name = ent && ent->client
		? MM_PlayerDisplayName(ent->client)
		: std::string_view{};

	std::string display;
	for (const unsigned char ch : name) {
		if (ch < ' ' || ch == 0x7F || ch == '%')
			display += ' ';
		else
			display += static_cast<char>(ch);
	}

	while (!display.empty() && display.back() == ' ')
		display.pop_back();

	return display.empty() ? "player" : display;
}

int CompareJoinTimeDescending(int lhs, int rhs)
{
	const bool a_valid = IsClientIndexInRange(lhs);
	const bool b_valid = IsClientIndexInRange(rhs);
	if (!a_valid && !b_valid)
		return 0;
	if (!a_valid)
		return 1;
	if (!b_valid)
		return -1;

	const auto lhs_time = game.clients[lhs].sess.team_join_time.milliseconds();
	const auto rhs_time = game.clients[rhs].sess.team_join_time.milliseconds();

	if (lhs_time > rhs_time)
		return -1;
	if (lhs_time < rhs_time)
		return 1;
	return 0;
}

bool JoinedMoreRecently(int lhs, int rhs)
{
	return CompareJoinTimeDescending(lhs, rhs) < 0;
}

bool IsTeamInRange(team_t team)
{
	const int value = static_cast<int>(team);
	return value >= static_cast<int>(TEAM_NONE) && value < static_cast<int>(TEAM_NUM_TEAMS);
}

bool IsPlayingTeam(team_t team)
{
	return team == TEAM_RED || team == TEAM_BLUE;
}

bool IsTargetValidForCurrentMode(team_t team, bool force)
{
	if (team == TEAM_SPECTATOR)
		return true;
	if (team == TEAM_NONE)
		return force && GT(GT_DUEL);
	if (Teams())
		return team == TEAM_RED || team == TEAM_BLUE;
	return team == TEAM_FREE;
}

} // namespace muffmode::team

int PlayerSortByJoinTime(const void *a, const void *b) {
	if (!a || !b)
		return 0;

	return muffmode::team::CompareJoinTimeDescending(
		*static_cast<const int *>(a),
		*static_cast<const int *>(b));
}

// =========================================
// TEAMPLAY - MOSTLY PORTED FROM QUAKE III
// =========================================

/*
================
PickTeam
================
*/
team_t PickTeam(int ignore_client_num) {
	if (!Teams())
		return TEAM_FREE;

	const gentity_t *ignore = muffmode::team::IsClientIndexInRange(
		ignore_client_num) ? &g_entities[ignore_client_num + 1] : nullptr;
	const muffmode::team::LogicalTeamView logical =
		muffmode::team::BuildLogicalTeamView(ignore);

	if (MM_TeamLowerLogicalSide({
			logical.red_count, logical.blue_count }) ==
		mm_team_count_side_t::Red)
		return TEAM_RED;

	if (MM_TeamLowerLogicalSide({
			logical.red_count, logical.blue_count }) ==
		mm_team_count_side_t::Blue)
		return TEAM_BLUE;

	// equal team count, so join the team with the lowest score
	if (level.team_scores[TEAM_BLUE] > level.team_scores[TEAM_RED])
		return TEAM_RED;
	if (level.team_scores[TEAM_RED] > level.team_scores[TEAM_BLUE])
		return TEAM_BLUE;

	// equal team scores, so join team with lowest total individual scores
	// skip in tdm as it's redundant
	if (notGT(GT_TDM)) {
		if (logical.blue_score > logical.red_score)
			return TEAM_RED;
		if (logical.red_score > logical.blue_score)
			return TEAM_BLUE;
	}

	// otherwise just randomly select a team
	return brandom() ? TEAM_RED : TEAM_BLUE;
}

/*
=================
BroadcastTeamChange

Let everyone know about a team change
=================
*/
void BroadcastTeamChange(gentity_t *ent, int old_team, bool inactive, bool silent) {

	if (!muffmode::CvarEnabled(deathmatch))
		return;

	if (!ent || !ent->client)
		return;

	if (notGT(GT_DUEL) && ent->client->sess.team == old_team)
		return;

	if (silent)
		return;

	std::string s;
	std::string t;
	const std::string name = muffmode::team::DisplayName(ent);

	// [MuffMode] Ported from WORR: the joining player's own notice carries their skill rating.
	// Only on a ranked session — an unranked one would otherwise advertise a default placeholder
	// rating that never moves, which is worse than saying nothing.
	const int32_t rating = MM_PlayerStats_SessionIsRanked(ent)
		? MM_PlayerStats_DisplayRating(MM_PlayerStats_Rating(ent))
		: 0;

	switch (ent->client->sess.team) {
	case TEAM_FREE:
		s = fmt::format("{} joined the battle.\n", name);
		//t = "%bind:inven:Toggles Menu%You have joined the game.";
		t = rating > 0
			? fmt::format("You have joined the game.\nYour Skill Rating: {}", rating)
			: "You have joined the game.";
		break;
	case TEAM_SPECTATOR:
		if (inactive) {
			s = fmt::format("{} is inactive,\nmoved to spectators.\n", name);
			t = "You are inactive and have been\nmoved to spectators.";
		} else {
			if (GT(GT_DUEL) && ent->client->sess.duel_queued) {
				s = fmt::format("{} is in the queue to play.\n", name);
				t = "You are in the queue to play.";
			} else {
				s = fmt::format("{} joined the spectators.\n", name);
				t = "You are now spectating.";
			}
		}
		break;
	case TEAM_RED:
	case TEAM_BLUE:
		s = fmt::format("{} joined the {} Team.\n", name, Teams_TeamName(ent->client->sess.team));
		t = rating > 0
			? fmt::format("You have joined the {} Team.\nYour Skill Rating: {}",
				Teams_TeamName(ent->client->sess.team), rating)
			: fmt::format("You have joined the {} Team.\n", Teams_TeamName(ent->client->sess.team));
		break;
	}

	if (!s.empty()) {
		for (auto ec : active_clients()) {
			if (ec == ent)
				continue;
			if (ec->svflags & SVF_BOT)
				continue;
			gi.LocClient_Print(ec, PRINT_CENTER, "{}", s.c_str());
		}
		//gi.Com_Print(s);
	}

	if (muffmode::CvarEnabled(g_dm_do_readyup) && level.match_state == match_state_t::MATCH_WARMUP_READYUP) {
		BroadcastReadyReminderMessage();
	} else if (!t.empty()) {
		gi.LocClient_Print(ent, PRINT_CENTER, "%bind:inven:Open menu%{}", t.c_str());
	}
}

/*
=================
AllowTeamSwitch
=================
*/
namespace muffmode::team {

bool AllowTeamSwitch(gentity_t *ent, team_t desired_team) {
	if (!ent || !ent->client || !IsTeamInRange(desired_team) || desired_team == TEAM_NONE)
		return false;

	// Red Rover: death is the only way to switch teams during a match, so block an
	// already-playing player from manually switching sides to dodge the defect mechanic.
	// Joining from spectator is allowed (otherwise nobody could enter a match in progress),
	// and leaving to spectator is allowed - that's quitting, not dodging a defect.
	if (MM_RedRoverBlocksManualTeamSwitch(ent->client->sess.team, desired_team, TEAM_SPECTATOR, GT(GT_RR), level.match_state == match_state_t::MATCH_IN_PROGRESS)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You cannot change teams during a Red Rover match.\n");
		return false;
	}
	if (desired_team != TEAM_SPECTATOR && HumanPlayerLimitReached(ent)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Maximum player count has been reached.\n");
		return false; // ignore the request
	}

	if (muffmode::match::IsTeamLocked(desired_team)) {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} is locked.\n", Teams_TeamName(desired_team));
		return false; // ignore the request
	}

	if (Teams()) {
		if (muffmode::CvarEnabled(g_teamplay_force_balance)) {
			// Exclude the target's current live/reserved membership, then project
			// the requested admission. A final spread of two remains allowed.
			const LogicalTeamView without_target = BuildLogicalTeamView(ent);
			const mm_team_count_side_t desired_side = desired_team == TEAM_RED
				? mm_team_count_side_t::Red
				: desired_team == TEAM_BLUE
					? mm_team_count_side_t::Blue
					: mm_team_count_side_t::None;
			const auto projection = MM_TeamProjectForceBalanceSwitch(
				without_target.red_count, without_target.blue_count,
				0, 0, mm_team_count_side_t::None,
				mm_team_count_side_t::None, desired_side);
			if (!projection.allowed) {
				gi.LocClient_Print(ent, PRINT_HIGH, "{} has too many players.\n", Teams_TeamName(desired_team));
				return false; // ignore the request
			}

			// It's ok, the team we are switching to has less or same number of players
		}
	}

	return true;
}

} // namespace muffmode::team

/*
=================
AllowClientTeamSwitch
=================
*/
bool AllowClientTeamSwitch(gentity_t *ent) {
	if (!muffmode::CvarEnabled(deathmatch))
		return false;

	if (!ent || !ent->client)
		return false;

	if (muffmode::CvarEnabled(g_dm_force_join) || !muffmode::CvarEnabled(g_teamplay_allow_team_pick)) {
		if (!(ent->svflags & SVF_BOT)) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Team picks are disabled.");
			return false;
		}
	}
	
	if (ent->client->resp.team_delay_time > level.time) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You may not switch teams more than once per 5 seconds.\n");
		return false;
	}

	return true;
}

/*
================
TeamBalance

Balance the teams without shuffling.
Switch last joined player(s) from stacked team.
================
*/
int TeamBalance(bool force) {
	if (GT(GT_ARENA))
		return 0;

	if (!Teams())
		return 0;

	if (GT(GT_RR))
		return 0;

	const muffmode::team::LogicalTeamView logical =
		muffmode::team::BuildLogicalTeamView(nullptr);
	int64_t logical_red = static_cast<int64_t>(logical.red_count);
	int64_t logical_blue = static_cast<int64_t>(logical.blue_count);
	int64_t delta = std::abs(logical_red - logical_blue);

	if (delta < 2)
		return 0;

	const team_t stack_team = logical_red > logical_blue ? TEAM_RED : TEAM_BLUE;

	std::array<int, MAX_LOBBY_PLAYERS> client_indices = {};
	size_t count = 0;

	// assemble list of client nums of everyone on stacked team
	for (auto ec : active_clients()) {
		if (count >= client_indices.size())
			break;
		if (ec->client->sess.team != stack_team)
			continue;
		// Store the client number (not the entity number); the switch loop below indexes
		// game.clients[] with this directly.
		client_indices[count] = static_cast<int>(ec - g_entities - 1);
		count++;
	}

	// sort client num list by join time
	std::sort(client_indices.begin(), client_indices.begin() + count, muffmode::team::JoinedMoreRecently);

	//run through sort list, switching from stack_team until teams are even
	int switched = 0;
	const team_t new_team = stack_team == TEAM_RED ? TEAM_BLUE : TEAM_RED;
	for (size_t i = 0; i < count && delta > 1; i++) {
		const int client_index = client_indices[i];
		gclient_t &client = game.clients[client_index];

		if (!client.pers.connected)
			continue;

		if (client.sess.team != stack_team)
			continue;

		// Route the switch through SetTeam (force) so CTF flag/skin/follower/captain
		// state is cleaned up; the old raw `sess.team = ...; ClientRespawn()` left that
		// state dangling and could crash on a CTF rebalance.
		gentity_t *sw_ent = &g_entities[client_index + 1];
		if (!SetTeam(sw_ent, new_team, false, force, false))
			continue;
		gi.LocClient_Print(sw_ent, PRINT_CENTER, "You have changed teams to rebalance the game.\n");

		if (stack_team == TEAM_RED) {
			logical_red--;
			logical_blue++;
		} else {
			logical_blue--;
			logical_red++;
		}
		delta = std::abs(logical_red - logical_blue);
		switched++;
	}

	if (switched) {
		gi.LocBroadcast_Print(PRINT_HIGH, "Teams have been balanced.\n");
		return switched;
	}

	return 0;
}

/*
================
TeamShuffle

Distributes active players by descending skill rating.
================
*/
bool TeamShuffle() {
	if (GT(GT_ARENA))
		return false;

	if (!Teams())
		return false;
	std::vector<int32_t> client_indices;
	client_indices.reserve(static_cast<size_t>(level.num_playing_clients));
	for (size_t client_index = 0; client_index < game.maxclients; ++client_index) {
		// client_indices holds client numbers (0..MAX_LOBBY_PLAYERS-1); client N is entity N+1.
		gentity_t *ent = &g_entities[client_index + 1];
		if (ent->inuse && ent->client && ent->client->pers.connected &&
			ClientIsPlaying(ent->client))
			client_indices.push_back(static_cast<int32_t>(client_index));
	}
	if (client_indices.size() < 2)
		return false;

	std::sort(client_indices.begin(), client_indices.end(),
		[](int32_t lhs, int32_t rhs) {
			const float lhs_rating = MM_PlayerStats_Rating(&g_entities[lhs + 1]);
			const float rhs_rating = MM_PlayerStats_Rating(&g_entities[rhs + 1]);
			if (lhs_rating != rhs_rating)
				return lhs_rating > rhs_rating;
			return lhs < rhs;
		});

	team_t target_team = TEAM_RED;
	for (const int32_t client_index : client_indices) {
		gentity_t *ent = &g_entities[client_index + 1];
		if (!ent->inuse || !ent->client || !ent->client->pers.connected ||
			!ClientIsPlaying(ent->client))
			continue;
		if (ent->client->sess.team != target_team)
			SetTeam(ent, target_team, false, true, true);
		target_team = target_team == TEAM_RED ? TEAM_BLUE : TEAM_RED;
	}

	return true;
}

/*
=================
SetTeam
=================
*/
bool SetTeam(gentity_t *ent, team_t desired_team, bool inactive, bool force, bool silent) {
	if (!ent || !ent->client)
		return false;
	// Team automation, admin force-team, and ordinary commands must not mutate a
	// claimed/reinstating target outside the transactional ghost commit/abort path.
	if (MM_Ghost_IsPendingRestore(ent))
		return false;

	bool arena_result = false;
	if (MM_Arena_HandleTeamRequest(ent, desired_team, inactive, force, silent, arena_result))
		return arena_result;

	if (!muffmode::team::IsTeamInRange(desired_team) ||
		!muffmode::team::IsTargetValidForCurrentMode(desired_team, force))
		return false;

	const team_t old_team = ent->client->sess.team;
	bool queue = false;

	if (desired_team != TEAM_NONE && desired_team != TEAM_SPECTATOR &&
		ent->client->sess.inactive && MM_Ghost_TryRestore(ent))
		return true;

	// A queued spectator may repeat the ordinary play command while a slot is
	// open. Promotion still belongs to the ordered queue scanner; otherwise a
	// command retry could jump older contenders. The forced TEAM_FREE request
	// used by that scanner is deliberately exempt.
	if (GT(GT_DUEL) && old_team == TEAM_SPECTATOR &&
		ent->client->sess.duel_queued &&
		((desired_team == TEAM_FREE && !force) || desired_team == TEAM_NONE)) {
		P_Menu_Close(ent);
		return true;
	}

	if (!force && desired_team != TEAM_SPECTATOR && desired_team == old_team) {
		P_Menu_Close(ent);
		return false;
	}

	// Duel has one roster-admission policy for every entry point. Resolve queue
	// intent before player-cap and lock checks so maxplayers=2 and an active-match
	// lock still permit spectators to join the waiting line. Forced TEAM_FREE is
	// intentionally rejected when it would bypass the two-slot/phase invariant;
	// TEAM_NONE is the explicit internal request to queue.
	if (GT(GT_DUEL)) {
		if (desired_team == TEAM_NONE) {
			desired_team = TEAM_SPECTATOR;
			queue = true;
		} else if (desired_team == TEAM_FREE && MM_Duel_JoinWouldQueue()) {
			if (force)
				return false;
			desired_team = TEAM_SPECTATOR;
			queue = true;
		}
	}

	if (!force) {
		if (!ClientIsPlaying(ent->client) && desired_team != TEAM_SPECTATOR) {
			bool revoke = false;
			// Check if the desired team is locked (covers both captain lock and g_match_lock)
			if (muffmode::match::IsTeamLocked(desired_team)) {
				gi.LocClient_Print(ent, PRINT_HIGH, "{} is locked.\n", Teams_TeamName(desired_team));
				revoke = true;
			} else if (muffmode::team::HumanPlayerLimitReached(ent)) {
				gi.LocClient_Print(ent, PRINT_HIGH, "Maximum player load reached.\n");
				revoke = true;
			}
			if (revoke) {
				P_Menu_Close(ent);
				return false;
			}
		}

		if (!muffmode::team::AllowTeamSwitch(ent, desired_team))
			return false;

		// Don't rate limit switching TO spectator - allow players to spectate immediately
		// Only rate limit switches between playing teams
		if (!inactive && desired_team != TEAM_SPECTATOR && ent->client->resp.team_delay_time > level.time) {
			gi.LocClient_Print(ent, PRINT_HIGH, "You may not switch teams more than once per 5 seconds.\n");
			P_Menu_Close(ent);
			return false;
		}
	}

	// allow the change...

	P_Menu_Close(ent);
	const bool was_match_player = muffmode::team::IsPlayingTeam(old_team);
	const bool will_be_match_player = muffmode::team::IsPlayingTeam(desired_team);
	bool inactive_reserved = false;
	if (old_team != desired_team) {
		const bool capture_inactive = inactive &&
			desired_team == TEAM_SPECTATOR && was_match_player &&
			!will_be_match_player;
		if (capture_inactive) {
			// Pause and archive before copying the reconnect state. A successful
			// inactivity reservation remains an unsettled logical participant;
			// a failed capture falls through to ordinary abandonment settlement.
			MM_PlayerStats_OnClientPause(ent);
			MM_MatchStats_ClientEnd(ent);
			inactive_reserved = MM_Ghost_CaptureInactive(ent);
		}
		if (!inactive_reserved) {
			MM_PlayerStats_OnTeamTransition(ent, old_team, desired_team);
			if (was_match_player && !will_be_match_player && !capture_inactive)
				MM_MatchStats_ClientEnd(ent);
		}
	}

	// vacate captain if leaving a team
	if (notGT(GT_ARENA) &&
		muffmode::team::IsPlayingTeam(old_team) && old_team != desired_team) {
		if (level.captain[old_team] == ent)
			VacateCaptain(old_team, ent);
	}

	// start as spectator
	if (ent->movetype == MOVETYPE_NOCLIP)
		Weapon_Grapple_DoReset(ent->client);

	CTF_DeadDropFlag(ent);
	Tech_DeadDrop(ent);
	if (old_team != desired_team && was_match_player && !will_be_match_player)
		MM_MatchStats_ClientEnd(ent);

	FreeFollower(ent);

	ent->svflags &= ~SVF_NOCLIENT;
	// Red Rover and multi-arena role projection change engine teams during play.
	// Their own match controllers reset scores at real series boundaries.
	if (notGT(GT_RR) && notGT(GT_ARENA))
		ent->client->resp.score = 0;
	ent->client->sess.team = desired_team;
	P_PublishEngineTeam(ent);
	if (old_team != desired_team && !was_match_player && will_be_match_player)
		MM_MatchStats_ClientBegin(ent);
	if (desired_team == TEAM_SPECTATOR)
		ent->client->eliminated = false;
	ent->client->resp.ctf_state = 0;
	ent->client->sess.inactive = inactive;
	ent->client->sess.inactivity_time = level.time + 1_min;
	// If queued for duel, record when they joined the queue for proper ordering.
	// Otherwise, non-queued spectators get 0_sec, playing players get current time.
	if (desired_team == TEAM_SPECTATOR) {
		ent->client->sess.team_join_time = queue ? level.time : 0_sec;
	} else {
		ent->client->sess.team_join_time = level.time;
	}
	ent->client->resp.team_delay_time = force || !ent->client->sess.initialised ? level.time : level.time + 5_sec;
	ent->client->sess.spectator_state = desired_team == TEAM_SPECTATOR ? SPECTATOR_FREE : SPECTATOR_NOT;
	ent->client->sess.spectator_client = 0;
	ent->client->sess.duel_queued = queue;
	ent->client->sess.duel_queue_order = queue ? MM_Duel_AllocateQueueOrder() : 0;

	if (desired_team != TEAM_SPECTATOR) {
		if (Teams() || GT(GT_ARENA))
			G_AssignPlayerSkin(ent, ent->client->pers.skin);

		if (notGT(GT_ARENA)) {
			MM_RevertVote(ent->client);

			// assign a ghost code
			MM_Ghost_DoAssign(ent);
		}

		// free any followers
		FreeClientFollowers(ent);

		// auto-assign captain if team has none
		if (notGT(GT_ARENA) &&
			muffmode::team::IsPlayingTeam(desired_team) && !level.captain[desired_team])
			SetCaptain(desired_team, ent);
	} else if (!inactive && notGT(GT_ARENA)) {
		MM_Ghost_ClearClient(ent);
	}

	ent->client->sess.initialised = true;

	// Joining the match re-arms the top-right gametype/ruleset notice. Test the real playing
	// predicate, not IsPlayingTeam(): that one is red/blue only, so FFA/Duel/LMS/Horde joins land
	// on TEAM_FREE and would never arm it. sess.team is already assigned above.
	if (ClientIsPlaying(ent->client))
		MM_MatchInfoHud_Show(ent);

	ClientSpawn(ent);
	G_PostRespawn(ent);

	BroadcastTeamChange(ent, old_team, inactive, silent);

	ent->client->ps.stats[STAT_SHOW_STATUSBAR] = desired_team == TEAM_SPECTATOR || ent->client->eliminated ? 0 : 1;

	// if anybody has a menu open, update it immediately
	P_Menu_Dirty();

	return true;
}

/*
=================
MM_CmdTeam
=================
*/
void MM_CmdTeam(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (MM_Arena_TeamCommand(ent))
		return;

	if (!MM_IsArgcInRangeValid(gi.argc(), 1, 2)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} [team]\n", gi.argv(0));
		return;
	}

	if (gi.argc() == 1) {
		switch (ent->client->sess.team) {
		case TEAM_SPECTATOR:
			gi.LocClient_Print(ent, PRINT_HIGH, "You are spectating.\n");
			break;
		case TEAM_FREE:
			gi.LocClient_Print(ent, PRINT_HIGH, "You are in the match.\n");
			break;
		case TEAM_RED:
		case TEAM_BLUE:
			gi.LocClient_Print(ent, PRINT_HIGH, "Your team: {}\n", Teams_TeamName(ent->client->sess.team));
			break;
		default:
			break;
		}
		return;
	}

	const char *s = gi.argv(1);
	const team_t team = StringToTeamNum(s);
	if (team == TEAM_NONE) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid team.\n");
		return;
	}

	SetTeam(ent, team, false, false, false);
}

/*
=================
MM_CmdSetTeam
=================
*/
void MM_CmdSetTeam(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (MM_Arena_SetTeamCommand(ent))
		return;

	if (!MM_IsArgcInRangeValid(gi.argc(), 2, 3)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} [client name/num] [team]\n", gi.argv(0));
		return;
	}

	gentity_t *targ = ClientEntFromString(gi.argv(1));

	if (!targ || !targ->inuse || !targ->client) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid client name or number.\n");
		return;
	}

	if (gi.argc() == 2) {
		gi.LocClient_Print(ent, PRINT_HIGH, "{} is on {} team.\n", targ->client->resp.netname, Teams_TeamName(targ->client->sess.team));
		return;
	}

	const team_t team = StringToTeamNum(gi.argv(2));
	if (team == TEAM_NONE) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid team.\n");
		return;
	}

	if (targ->client->sess.team == team) {
		gi.LocClient_Print(ent, PRINT_HIGH, "{} is already on {} team.\n", targ->client->resp.netname, Teams_TeamName(team));
		return;
	}

	if ((Teams() && team == TEAM_FREE) || (!Teams() && team != TEAM_SPECTATOR && team != TEAM_FREE)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid team.\n");
		return;
	}

	if (!SetTeam(targ, team, false, true, false)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Could not move {} to {} team.\n", targ->client->resp.netname, Teams_TeamName(team));
		return;
	}

	gi.LocBroadcast_Print(PRINT_HIGH, "[ADMIN]: Moved {} to {} team.\n", targ->client->resp.netname, Teams_TeamName(team));
}

/*
=================
MM_CmdShuffle
=================
*/
void MM_CmdShuffle(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (!MM_IsExactArgcValid(gi.argc(), 1)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {}\n", gi.argv(0));
		return;
	}

	if (!Teams()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Team shuffle is only available in team modes.\n");
		return;
	}

	if (level.num_playing_clients < 2) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Not enough players to shuffle teams.\n");
		return;
	}

	if (!TeamShuffle()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Teams could not be shuffled.\n");
		return;
	}

	gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Forced team shuffle.\n");
	Match_Reset();
}

/*
=================
MM_CmdBalanceTeams
=================
*/
void MM_CmdBalanceTeams(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (!MM_IsExactArgcValid(gi.argc(), 1)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {}\n", gi.argv(0));
		return;
	}

	if (!Teams()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Team balancing is only available in team modes.\n");
		return;
	}

	const int moved = TeamBalance(true);
	if (moved > 0)
		gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Forced team balancing.\n");
	else
		gi.LocClient_Print(ent, PRINT_HIGH, "Teams are already balanced.\n");
}
