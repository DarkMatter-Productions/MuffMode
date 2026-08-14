// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_duel.h"
#include "muffmode/mm_ghost.h"
#include "muffmode/mm_team.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace muffmode::duel {

bool MM_DuelAppendLayout(std::string &layout, std::string_view entry)
{
	constexpr size_t terminator_reserve = 1;
	if (entry.size() > MAX_STRING_CHARS - terminator_reserve)
		return false;
	if (layout.size() > MAX_STRING_CHARS - terminator_reserve - entry.size())
		return false;

	layout += entry;
	return true;
}

bool MM_DuelCvarEnabled(cvar_t *cvar)
{
	return cvar && cvar->integer != 0;
}

constexpr size_t MM_DUEL_MAX_LAYOUT_TEXT = 256;

std::string MM_DuelLayoutText(const char *text)
{
	if (!text || !*text)
		return "";

	std::string out;
	for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p && out.size() < MM_DUEL_MAX_LAYOUT_TEXT; p++) {
		if (*p < ' ' || *p == 0x7F || *p == '"' || *p == '\\')
			out += ' ';
		else
			out += static_cast<char>(*p);
	}

	while (!out.empty() && out.back() == ' ')
		out.pop_back();

	return out;
}

std::string MM_DuelSkinIconToken(const char *skin)
{
	std::string safe_skin;

	if (skin) {
		for (const unsigned char *p = reinterpret_cast<const unsigned char *>(skin); *p && safe_skin.size() < MAX_QPATH / 2; p++) {
			if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
				(*p >= '0' && *p <= '9') || *p == '_' || *p == '-' || *p == '/') {
				if (*p == '/' && (safe_skin.empty() || safe_skin.back() == '/'))
					continue;
				safe_skin += static_cast<char>(*p);
			}
		}
	}

	if (safe_skin.empty() || safe_skin.front() == '/' || safe_skin.back() == '/')
		safe_skin = "male/grunt";

	return fmt::format("/players/{}_i", safe_skin);
}

int MM_DuelSortedClientAt(size_t slot)
{
	if (slot >= std::size(level.sorted_clients))
		return -1;

	const int client_num = level.sorted_clients[slot];
	if (client_num < 0 || client_num >= (int)game.maxclients)
		return -1;

	return client_num;
}

gentity_t *MM_DuelEntityForClient(const gclient_t *client)
{
	if (!client || !game.clients)
		return nullptr;

	const size_t client_num = MM_GhostAddressIndex(
		reinterpret_cast<uintptr_t>(client),
		reinterpret_cast<uintptr_t>(game.clients),
		sizeof(game.clients[0]), static_cast<size_t>(game.maxclients));
	if (client_num == MM_GHOST_NO_CLIENT_INDEX)
		return nullptr;

	gentity_t *ent = &g_entities[client_num + 1];
	if (!ent->inuse || ent->client != client)
		return nullptr;

	return ent;
}

size_t MM_DuelSortedClientLimit()
{
	return min<size_t>(game.maxclients, std::size(level.sorted_clients));
}

} // namespace muffmode::duel

namespace duel = muffmode::duel;

namespace {

uint64_t next_duel_queue_order = 0;

struct final_duel_result_t {
	bool frozen = false;
	bool valid = false;
	bool draw = false;
	mm_duel_score_entry_t winner{};
	mm_duel_score_entry_t loser{};
};

final_duel_result_t final_duel_result;

const gclient_t *AuthoritativeDuelParticipantState(gentity_t *ent)
{
	if (!ent || !ent->client)
		return nullptr;

	if (const gclient_t *reserved = MM_Ghost_ReservedClientState(ent)) {
		return reserved->sess.team != TEAM_NONE &&
			reserved->sess.team != TEAM_SPECTATOR
			? reserved : nullptr;
	}

	return ent->inuse && ent->client->pers.connected &&
		ClientIsPlaying(ent->client)
		? ent->client : nullptr;
}

int DuelClientNum(const gentity_t *ent) noexcept
{
	if (!ent || !g_entities)
		return -1;
	const size_t client_num = MM_GhostAddressIndex(
		reinterpret_cast<uintptr_t>(ent),
		reinterpret_cast<uintptr_t>(&g_entities[1]), sizeof(g_entities[0]),
		static_cast<size_t>(game.maxclients));
	return client_num == MM_GHOST_NO_CLIENT_INDEX
		? -1 : static_cast<int>(client_num);
}

bool DuelEntryMatchesIdentity(const mm_duel_score_entry_t &entry,
	int client_num, int32_t spawn_count, std::string_view social_id) noexcept
{
	if (!social_id.empty() || !entry.social_id.empty())
		return !social_id.empty() && !entry.social_id.empty() &&
			entry.social_id == social_id;
	return client_num >= 0 && entry.client_num == client_num &&
		entry.spawn_count == spawn_count;
}

bool DuelEntryRanksBefore(
	const mm_duel_score_entry_t &left,
	const mm_duel_score_entry_t &right) noexcept
{
	return left.score > right.score ||
		(left.score == right.score && left.client_num < right.client_num);
}

gentity_t *ConnectedEntityForDuelEntry(const mm_duel_score_entry_t &entry)
{
	if (!entry.social_id.empty()) {
		for (gentity_t *ent : active_clients())
			if (ent && ent->client && ent->client->pers.connected &&
				std::string_view(ent->client->pers.social_id) == entry.social_id)
				return ent;
		return nullptr;
	}

	if (entry.client_num < 0 ||
		static_cast<size_t>(entry.client_num) >= game.maxclients)
		return nullptr;
	gentity_t *ent = &g_entities[entry.client_num + 1];
	if (!ent->inuse || !ent->client || !ent->client->pers.connected ||
		ent->spawn_count != entry.spawn_count)
		return nullptr;
	return ent;
}

gentity_t *SlotForDuelEntry(const mm_duel_score_entry_t &entry)
{
	if (entry.client_num < 0 ||
		static_cast<size_t>(entry.client_num) >= game.maxclients)
		return nullptr;
	return &g_entities[entry.client_num + 1];
}

bool SlotMayOwnDuelEntry(
	const gentity_t *slot, const mm_duel_score_entry_t &entry) noexcept
{
	// Social identity survives a legitimate slot migration. Without it, the
	// entity generation is the only thing separating this result from a later
	// client that recycled the same numeric slot.
	return slot && (!entry.social_id.empty() ||
		slot->spawn_count == entry.spawn_count);
}

void ApplyDuelSessionResult(const mm_duel_score_entry_t &entry, bool won)
{
	if (gentity_t *slot = SlotForDuelEntry(entry))
		if (SlotMayOwnDuelEntry(slot, entry) &&
			MM_Ghost_ApplyReservedDuelResult(
				slot, entry.social_id.c_str(), won))
			return;

	if (gentity_t *ent = ConnectedEntityForDuelEntry(entry)) {
		int &counter = won ? ent->client->sess.wins : ent->client->sess.losses;
		if (counter < std::numeric_limits<int>::max())
			counter++;
	}
}

bool CommitFinalDuelResult(const mm_duel_score_entry_t &winner,
	const mm_duel_score_entry_t &loser, bool draw)
{
	if (final_duel_result.frozen)
		return false;
	final_duel_result.frozen = true;
	final_duel_result.valid = true;
	final_duel_result.draw = draw;
	final_duel_result.winner = winner;
	final_duel_result.loser = loser;
	if (!draw) {
		// The reservation snapshot is authoritative while disconnected or pending;
		// update it (and its connected mirror) exactly once for both participants.
		ApplyDuelSessionResult(winner, true);
		ApplyDuelSessionResult(loser, false);
	}
	return true;
}

} // namespace

mm_duel_score_view_t MM_Duel_CurrentScoreView()
{
	mm_duel_score_view_t view;
	if (notGT(GT_DUEL))
		return view;

	for (size_t i = 0; i < game.maxclients; ++i) {
		gentity_t *ent = &g_entities[i + 1];
		const gclient_t *state = AuthoritativeDuelParticipantState(ent);
		if (!state)
			continue;

		mm_duel_score_entry_t entry;
		entry.client_num = static_cast<int>(i);
		entry.spawn_count = ent->spawn_count;
		entry.score = state->resp.score;
		entry.player_name = state->resp.netname;
		entry.social_id = state->pers.social_id;
		entry.bot = state->sess.is_a_bot || (ent->svflags & SVF_BOT);

		if (view.participant_count == 0) {
			view.players[0] = std::move(entry);
		} else if (view.participant_count == 1) {
			if (DuelEntryRanksBefore(entry, view.players[0])) {
				view.players[1] = std::move(view.players[0]);
				view.players[0] = std::move(entry);
			} else {
				view.players[1] = std::move(entry);
			}
		} else if (DuelEntryRanksBefore(entry, view.players[0])) {
			view.players[1] = std::move(view.players[0]);
			view.players[0] = std::move(entry);
		} else if (DuelEntryRanksBefore(entry, view.players[1])) {
			view.players[1] = std::move(entry);
		}
		++view.participant_count;
	}

	view.tied = view.participant_count >= 2 &&
		view.players[0].score == view.players[1].score;
	return view;
}

void MM_Duel_ResetFinalResult()
{
	final_duel_result = {};
}

namespace {

bool CommitForfeit(gentity_t *forfeiter, bool departure)
{
	if (notGT(GT_DUEL) || !forfeiter || !forfeiter->client ||
		final_duel_result.frozen)
		return false;
	const size_t client_num = MM_GhostAddressIndex(
		reinterpret_cast<uintptr_t>(forfeiter),
		reinterpret_cast<uintptr_t>(&g_entities[1]), sizeof(g_entities[0]),
		static_cast<size_t>(game.maxclients));
	if (client_num == MM_GHOST_NO_CLIENT_INDEX)
		return false;

	const mm_duel_score_view_t view = MM_Duel_CurrentScoreView();
	if (view.participant_count != 2)
		return false;
	size_t forfeiter_index = view.players[0].client_num == static_cast<int>(client_num) ? 0 :
		view.players[1].client_num == static_cast<int>(client_num) ? 1 : 2;
	if (forfeiter_index >= view.participant_count ||
		forfeiter_index >= view.players.size())
		return false;
	const bool allowed = departure
		? MM_DuelDepartureForfeitAllowed(
			view.participant_count, forfeiter_index)
		: MM_DuelForfeitAllowed(
			view.participant_count, view.tied, forfeiter_index);
	if (!allowed)
		return false;
	const size_t winner_index = forfeiter_index == 0 ? 1 : 0;
	return CommitFinalDuelResult(
		view.players[winner_index], view.players[forfeiter_index], false);
}

} // namespace

bool MM_Duel_CommitForfeit(gentity_t *forfeiter)
{
	return CommitForfeit(forfeiter, false);
}

bool MM_Duel_CommitDepartureForfeit(gentity_t *forfeiter)
{
	return CommitForfeit(forfeiter, true);
}

mm_duel_final_outcome_t MM_Duel_FinalOutcome(
	int client_num, std::string_view social_id) noexcept
{
	// This compatibility overload has no entity generation. An authenticated
	// social identity is still sufficient, but a bare reusable slot is not.
	if (social_id.empty())
		return mm_duel_final_outcome_t::unavailable;
	return MM_Duel_FinalOutcome(client_num, 0, social_id);
}

mm_duel_final_outcome_t MM_Duel_FinalOutcome(
	int client_num, int32_t spawn_count, std::string_view social_id) noexcept
{
	if (!final_duel_result.frozen || !final_duel_result.valid)
		return mm_duel_final_outcome_t::unavailable;
	const bool winner = DuelEntryMatchesIdentity(
		final_duel_result.winner, client_num, spawn_count, social_id);
	const bool loser = DuelEntryMatchesIdentity(
		final_duel_result.loser, client_num, spawn_count, social_id);
	if (!winner && !loser)
		return mm_duel_final_outcome_t::unavailable;
	if (final_duel_result.draw)
		return mm_duel_final_outcome_t::draw;
	return winner
		? mm_duel_final_outcome_t::win
		: mm_duel_final_outcome_t::loss;
}

mm_duel_final_outcome_t MM_Duel_FinalOutcomeForClient(
	const gentity_t *ent) noexcept
{
	const size_t client_num = MM_GhostAddressIndex(
		reinterpret_cast<uintptr_t>(ent),
		reinterpret_cast<uintptr_t>(&g_entities[1]), sizeof(g_entities[0]),
		static_cast<size_t>(game.maxclients));
	if (client_num == MM_GHOST_NO_CLIENT_INDEX)
		return mm_duel_final_outcome_t::unavailable;
	return MM_Duel_FinalOutcome(
		static_cast<int>(client_num),
		ent->spawn_count,
		ent->client ? std::string_view(ent->client->pers.social_id) :
			std::string_view{});
}

size_t MM_Duel_OccupiedSlots()
{
	if (notGT(GT_DUEL))
		return 0;

	size_t occupied = 0;
	for (size_t i = 0; i < game.maxclients; ++i)
		if (AuthoritativeDuelParticipantState(&g_entities[i + 1]))
			++occupied;
	return occupied;
}

bool MM_Duel_JoinWouldQueue() noexcept
{
	if (notGT(GT_DUEL))
		return false;
	return MM_DuelJoinWouldQueue(
		MM_Duel_OccupiedSlots(),
		level.match_state <= match_state_t::MATCH_WARMUP_READYUP,
		level.intermission_time != 0_ms || level.intermission_queued != 0_ms);
}

uint64_t MM_Duel_AllocateQueueOrder()
{
	uint64_t highest = next_duel_queue_order;
	for (gentity_t *ent : active_clients()) {
		if (!ent || !ent->client || !ent->client->sess.duel_queued)
			continue;
		highest = std::max(highest, ent->client->sess.duel_queue_order);
	}

	if (highest != std::numeric_limits<uint64_t>::max()) {
		next_duel_queue_order = highest + 1;
		return next_duel_queue_order;
	}

	// A uint64 wrap is practically unreachable, but rebasing the bounded live
	// queue keeps allocation total and preserves its exact current ordering.
	std::vector<gentity_t *> queued;
	queued.reserve(game.maxclients);
	for (gentity_t *ent : active_clients())
		if (ent && ent->client && ent->client->sess.duel_queued)
			queued.push_back(ent);
	std::sort(queued.begin(), queued.end(), [](const gentity_t *lhs,
		const gentity_t *rhs) {
		return MM_DuelQueueOrderBefore(
			lhs->client->sess.duel_queue_order, DuelClientNum(lhs),
			rhs->client->sess.duel_queue_order, DuelClientNum(rhs));
	});

	next_duel_queue_order = 0;
	for (gentity_t *ent : queued)
		ent->client->sess.duel_queue_order = ++next_duel_queue_order;
	return ++next_duel_queue_order;
}

bool MM_Duel_AddPlayer()
{
	if (notGT(GT_DUEL))
		return false;

	if (MM_Duel_OccupiedSlots() >= 2)
		return false;

	if (level.match_state > match_state_t::MATCH_WARMUP_READYUP || level.intermission_time || level.intermission_queued)
		return false;

	std::vector<gentity_t *> queued;
	queued.reserve(game.maxclients);
	for (gentity_t *ec : active_clients()) {
		if (!ec || !ec->client || ClientIsPlaying(ec->client))
			continue;
		if (!ec->client->sess.duel_queued)
			continue;
		queued.push_back(ec);
	}

	std::sort(queued.begin(), queued.end(), [](const gentity_t *lhs,
		const gentity_t *rhs) {
		return MM_DuelQueueOrderBefore(
			lhs->client->sess.duel_queue_order, DuelClientNum(lhs),
			rhs->client->sess.duel_queue_order, DuelClientNum(rhs));
	});

	// A pending restore or another transiently blocked head must not make the
	// warmup controller believe promotion succeeded, nor strand valid followers.
	for (gentity_t *ent : queued)
		if (SetTeam(ent, TEAM_FREE, false, true, false))
			return true;
	return false;
}

void MM_Duel_RemoveLoser()
{
	if (notGT(GT_DUEL) || !final_duel_result.valid ||
		final_duel_result.draw)
		return;

	const mm_duel_score_entry_t loser = final_duel_result.loser;
	if (gentity_t *slot = SlotForDuelEntry(loser)) {
		if (SlotMayOwnDuelEntry(slot, loser) &&
			MM_Ghost_QueueReservedDuelLoser(
			slot, loser.social_id.c_str())) {
			MM_Duel_ResetFinalResult();
			return;
		}
	}
	gentity_t *ent = ConnectedEntityForDuelEntry(loser);
	if (!ent || !ClientIsPlaying(ent->client)) {
		MM_Duel_ResetFinalResult();
		return;
	}

	if (duel::MM_DuelCvarEnabled(g_verbose))
		gi.Com_PrintFmt("Duel: Moving the loser, {}, to end of queue.\n", ent->client->resp.netname);

	if (!SetTeam(ent, TEAM_NONE, false, true, false)) {
		gi.Com_PrintFmt("Duel: unable to move loser {} to the queue.\n",
			ent->client->resp.netname);
		return;
	}
	MM_Duel_ResetFinalResult();
}

void MM_Duel_MatchEnd_AdjustScores()
{
	if (notGT(GT_DUEL) || final_duel_result.frozen)
		return;

	const mm_duel_score_view_t view = MM_Duel_CurrentScoreView();
	if (view.participant_count != 2) {
		final_duel_result.frozen = true;
		return;
	}
	CommitFinalDuelResult(view.players[0], view.players[1], view.tied);
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

		// TEAM_NONE (not TEAM_SPECTATOR) is the signal SetTeam's force path turns into
		// "spectator + duel_queued"; TEAM_SPECTATOR would re-spectate without queuing,
		// so MM_Duel_AddPlayer (which only pulls queued spectators) never picks the bot up.
		SetTeam(ec, TEAM_NONE, false, true, false);
	}
}

/*
==================
MM_Duel_ScoreboardMessage

Duel scoreboard layout: the two contenders, queued challengers and spectators.
==================
*/
void MM_Duel_ScoreboardMessage(gentity_t *ent, gentity_t *killer) {
	uint8_t	i2 = 0;
	uint32_t	j, k;

	static std::string entry, string;
	int			x, y;

	string.clear();
	entry.clear();

	duel::MM_DuelAppendLayout(string, fmt::format("xv 0 yv -40 cstring2 \"{} on {}\" ",
		duel::MM_DuelLayoutText(level.gametype_name), duel::MM_DuelLayoutText(level.level_name)));
	duel::MM_DuelAppendLayout(string, fmt::format("xv 0 yv -30 cstring2 \"Score Limit: {}\" ", GT_ScoreLimit()));

	if (level.intermission_time) {
		if (level.match_start_time) {
			int	t = (level.intermission_time - level.match_start_time - 1_sec).milliseconds();
			duel::MM_DuelAppendLayout(string, fmt::format("xv 0 yv -50 cstring2 \"Total Match Time: {}\" ", G_TimeStringMs(t, false)));
		}
		if (level.intermission_victor_msg[0])
			duel::MM_DuelAppendLayout(string, fmt::format("xv 0 yv -10 cstring2 \"{}\" ",
				duel::MM_DuelLayoutText(level.intermission_victor_msg)));

		duel::MM_DuelAppendLayout(string, fmt::format("ifgef {} yb -48 xv 0 loc_cstring2 0 \"$m_eou_press_button\" endif ", (level.intermission_server_frame + (5_sec).frames())));
	} else if (level.match_state == MATCH_IN_PROGRESS) {
		if (ent && ent->client && ClientIsPlaying(ent->client) && ent->client->resp.score && level.num_playing_clients > 1) {
			duel::MM_DuelAppendLayout(string, fmt::format("xv 0 yv -10 cstring2 \"{} place with a score of {}\" ",
				G_PlaceString(ent->client->resp.rank + 1), ent->client->resp.score));
		}
		//fmt::format_to(std::back_inserter(string), FMT_STRING("xv 0 yb -48 cstring2 \"{}\" "), "Use inventory bind to toggle menu.");
	}

	gclient_t *cl = nullptr;
	gentity_t *cl_ent;
	const char *s;
	int32_t		img_index;

	if (level.num_playing_clients) {
		i2 = 0;
		for (size_t i = 0; i < level.num_playing_clients; i++) {
			const int client_num = duel::MM_DuelSortedClientAt(i);
			if (client_num < 0)
				continue;

			cl = &game.clients[client_num];
			if (!cl)
				continue;
			if (!cl->pers.connected)
				continue;

			if (!ClientIsPlaying(cl))
				continue;

			cl_ent = g_entities + 1 + client_num;
			if (!cl_ent)
				continue;
			if (!cl_ent->inuse)
				continue;

			//gi.Com_PrintFmt("i={} i2={} num_playing_clients={} sorted_clients={}\n", i, i2, level.num_playing_clients, level.sorted_clients[i]);

			x = i2 ? 130 : -72;
			y = 0;

			fmt::format_to(std::back_inserter(entry), FMT_STRING("xv {} yv {} picn {} "), x, y, "/tags/default");

			const std::string skin_icon = duel::MM_DuelSkinIconToken(cl->pers.skin);
			s = skin_icon.c_str();
			img_index = cl->pers.skin_icon_index;

			if (img_index)
				fmt::format_to(std::back_inserter(entry), FMT_STRING("xv {} yv {} picn {} "), x, y, s);

			// player ready marker
			if (level.match_state == match_state_t::MATCH_WARMUP_READYUP && (cl->sess.is_a_bot || cl->resp.ready))
				fmt::format_to(std::back_inserter(entry), FMT_STRING("xv {} yv {} picn {} "), x + 16, y + 16, "wheel/p_compass_selected");

			if (!duel::MM_DuelAppendLayout(string, entry))
				break;

			entry.clear();

			fmt::format_to(std::back_inserter(entry),
				FMT_STRING("client {} {} {} {} {} {} "),
				x, y, client_num, cl->resp.score, cl->ping, 0);	// (level.time - cl->sess.team_join_time).minutes<int>());

			if (!duel::MM_DuelAppendLayout(string, entry))
				break;

			entry.clear();

			i2++;
			if (i2 == 2)
				break;
		}
	}

	if (level.num_connected_clients > level.num_playing_clients) {
		j = 58;

		i2 = 0;
		k = 0;
		if (string.size() < MAX_STRING_CHARS - 50) {
			for (size_t i = 0; i < duel::MM_DuelSortedClientLimit(); i++) {
				const int client_num = duel::MM_DuelSortedClientAt(i);
				if (client_num < 0)
					continue;

				cl = &game.clients[client_num];
				cl_ent = g_entities + 1 + client_num;

				if (!cl_ent)
					continue;

				if (!cl_ent->inuse)
					continue;

				if (!cl)
					continue;

				if (!cl->pers.connected)
					continue;

				if (ClientIsPlaying(cl))
					continue;

				if (!cl->sess.duel_queued)
					continue;

				if (!k) {
					k = 1;
					if (!duel::MM_DuelAppendLayout(string, G_Fmt("xv 0 yv {} loc_string2 0 \"Queued Contenders\" ", j)))
						break;
					j += 8;
					j += 8;
					// Column headers at ctf stride (27px): x, x+27, x+54
					if (!duel::MM_DuelAppendLayout(string, G_Fmt(
						"xv -40 yv {} loc_string2 0 \"w\" "
						"xv -13 yv {} loc_string2 0 \"l\" "
						"xv 14 yv {} loc_string2 0 \"name\" ", j, j, j)))
						break;
					j += 8;
				}

				if (!duel::MM_DuelAppendLayout(string, G_Fmt("ctf {} {} {} {} {} \"\" ",
					-40,						// x
					j,							// y
					client_num,					// playernum
					cl->sess.wins,
					cl->sess.losses
				)))
					break;

				j += 8;
				i2++;
				if (i2 == 8)
					break;
			}
		}

		j += 8;

		i2 = 0;
		k = 0;
		if (string.size() < MAX_STRING_CHARS - 50) {
			for (size_t i = 0; i < duel::MM_DuelSortedClientLimit(); i++) {
				const int client_num = duel::MM_DuelSortedClientAt(i);
				if (client_num < 0)
					continue;

				cl = &game.clients[client_num];
				cl_ent = g_entities + 1 + client_num;

				if (!cl_ent)
					continue;

				if (!cl_ent->inuse)
					continue;

				if (!cl)
					continue;

				if (!cl->pers.connected)
					continue;

				if (ClientIsPlaying(cl))
					continue;

				if (cl->sess.duel_queued)
					continue;

				if (!k) {
					k = 1;
					if (!duel::MM_DuelAppendLayout(string, G_Fmt("xv 0 yv {} loc_string2 0 \"Spectators\" ", j)))
						break;
					j += 8;
					j += 8;
				}

				if (!duel::MM_DuelAppendLayout(string, G_Fmt("ctf {} {} {} 0 0 \"\" ",
					-40,						// x
					j,							// y
					client_num					// playernum
				)))
					break;

				j += 8;
				i2++;
				if (i2 == 8)
					break;
			}
		}
	}

	if (level.intermission_time)
		duel::MM_DuelAppendLayout(string, fmt::format("ifgef {} yb -48 xv 0 loc_cstring2 0 \"$m_eou_press_button\" endif ", (level.intermission_server_frame + (5_sec).frames())));
	else
		duel::MM_DuelAppendLayout(string, "xv 0 yb -48 cstring2 \"Show inventory to toggle menu.\" ");

	gi.WriteByte(svc_layout);
	gi.WriteString(string.c_str());
}

/*
==================
MM_Duel_CmdForfeit

Losing player concedes the duel.
==================
*/
void MM_Duel_CmdForfeit(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (notGT(GT_DUEL)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Forfeit is only available in a duel.\n");
		return;
	}
	if (level.match_state != match_state_t::MATCH_IN_PROGRESS ||
		level.intermission_queued || level.intermission_time) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Forfeit is not available during warmup.\n");
		return;
	}
	if (!duel::MM_DuelCvarEnabled(g_allow_forfeit)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Forfeits are not enabled on this server.\n");
		return;
	}
	if (!MM_Duel_CommitForfeit(ent)) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"Forfeit is only available to a current losing or tied duelist.\n");
		return;
	}

	const std::string message = fmt::format("{} forfeits the match.", duel::MM_DuelLayoutText(ent->client->resp.netname));
	QueueIntermission(message.c_str(), true, false);
}
