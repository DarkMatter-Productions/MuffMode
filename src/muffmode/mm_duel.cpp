// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_duel.h"
#include "muffmode/mm_team.h"

#include <iterator>
#include <string>
#include <string_view>

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

	const ptrdiff_t client_num = client - game.clients;
	if (client_num < 0 || client_num >= (ptrdiff_t)game.maxclients)
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

	gentity_t *ent = duel::MM_DuelEntityForClient(next_in_line);
	if (!ent)
		return false;

	SetTeam(ent, TEAM_FREE, false, true, false);

	return true;
}

void MM_Duel_RemoveLoser()
{
	if (level.num_playing_clients != 2)
		return;

	const int loser_num = duel::MM_DuelSortedClientAt(1);
	if (loser_num < 0)
		return;

	gentity_t *ent = &g_entities[loser_num + 1];

	if (!ent || !ent->client || !ent->client->pers.connected)
		return;

	if (duel::MM_DuelCvarEnabled(g_verbose))
		gi.Com_PrintFmt("Duel: Moving the loser, {}, to end of queue.\n", ent->client->resp.netname);

	SetTeam(ent, TEAM_NONE, false, true, false);
}

void MM_Duel_MatchEnd_AdjustScores()
{
	if (notGT(GT_DUEL))
		return;

	int client_num = duel::MM_DuelSortedClientAt(0);
	if (client_num >= 0 && game.clients[client_num].pers.connected)
		game.clients[client_num].sess.wins++;

	client_num = duel::MM_DuelSortedClientAt(1);
	if (client_num >= 0 && game.clients[client_num].pers.connected) {
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
			if (level.match_state == matchst_t::MATCH_WARMUP_READYUP && (cl->sess.is_a_bot || cl->resp.ready))
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
					if (!duel::MM_DuelAppendLayout(string, G_Fmt("xv 0 yv {} loc_string2 0 \"Queued Contenders:\" ", j)))
						break;
					j += 8;
					if (!duel::MM_DuelAppendLayout(string, G_Fmt("xv -40 yv {} loc_string2 0 \"w  l  name\" ", j)))
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
					if (!duel::MM_DuelAppendLayout(string, G_Fmt("xv 0 yv {} loc_string2 0 \"Spectators:\" ", j)))
						break;
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
	if (level.match_state < matchst_t::MATCH_IN_PROGRESS) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Forfeit is not available during warmup.\n");
		return;
	}
	const int loser_num = duel::MM_DuelSortedClientAt(1);
	if (loser_num < 0 || ent->client != &game.clients[loser_num]) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Forfeit is only available to the losing player.\n");
		return;
	}
	if (!duel::MM_DuelCvarEnabled(g_allow_forfeit)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Forfeits are not enabled on this server.\n");
		return;
	}

	const std::string message = fmt::format("{} forfeits the match.", duel::MM_DuelLayoutText(ent->client->resp.netname));
	QueueIntermission(message.c_str(), true, false);
}
