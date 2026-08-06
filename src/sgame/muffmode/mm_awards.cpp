// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

// client/menu.h has no include guard and g_local.h already pulls it in, so
// P_Menu_Close arrives with g_local.h rather than through a second include.
#include "g_local.h"
#include "muffmode/mm_awards.h"
#include "muffmode/mm_debug.h"
#include "muffmode/mm_map_pick.h"
#include "muffmode/mm_util.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

cvar_t *g_match_awards;

namespace muffmode::awards {
namespace {

// The reel is latched client-side once drawn, so it only needs re-sending often
// enough to reclaim the layout from anything that drew over it (an inventory
// bind opening the join menu, a late joiner arriving mid-reel).
constexpr gtime_t kRefreshInterval = 2_sec;

// The reel is one message, so the layout must terminate inside the client's
// buffer. Unlike the scoreboard it contains no if/endif, so a clipped line is
// merely ugly rather than a client-side fatal error -- but the budget still has
// to match the buffer it is measured against.
static_assert(MM_AWARDS_LAYOUT_BUDGET == MAX_STRING_CHARS - 1,
	"The awards layout budget must track the client layout buffer");

struct state_t {
	// The last decided reel. This deliberately outlives the level it was shown
	// in: once the map changes the reel is gone from the screen, and `awards` is
	// the only way left to read it back.
	std::vector<mm_award_entry_t> entries;
	bool ranked = false;
	// Set by Publish and cleared by Reset. Without it, entries left over from a
	// previous match would let a later unranked match -- which publishes nothing
	// at all -- replay somebody else's reel.
	bool armed = false;
	bool running = false;
	// Set once the reel has had its turn, so a second intermission path offered
	// the exit afterwards lets the level go rather than replaying it.
	bool spent = false;
	// The unattended timeout, and the point before which a key press is refused.
	gtime_t end_time = 0_ms;
	gtime_t skip_time = 0_ms;
	std::string layout;
	// The reel keeps its own per-client refresh clock rather than reusing
	// client->menutime. A client arriving mid-reel is pushed through
	// MoveClientToIntermission, which sends the scoreboard and stamps menutime
	// three seconds out; borrowing that clock would make the reel claim the
	// layout while declining to draw it, and the joiner would sit looking at the
	// scoreboard until the stamp expired -- on a short reel, for all of it.
	std::array<gtime_t, MAX_CLIENTS> next_refresh {};
};

state_t s_state;

int DurationSeconds()
{
	return MM_AwardsDurationSeconds(muffmode::CvarInteger(g_match_awards));
}

// Builds the whole reel as one layout string. `xv` is sticky across tokens, so
// the horizontal centre is set once and every line after it is a bare `yv` plus
// a centred string -- which is what makes twelve pairs fit in one message.
std::string BuildLayout(const std::vector<mm_award_entry_t> &entries)
{
	std::string layout;
	if (entries.empty())
		return layout;

	const size_t shown = std::min(entries.size(), MM_AWARDS_DISPLAY_LIMIT);
	layout.reserve(MM_AWARDS_LAYOUT_BUDGET);

	int y = MM_AwardsBlockTop(shown);

	const auto append = [&layout](const std::string &line) {
		if (layout.size() + line.size() > MM_AWARDS_LAYOUT_BUDGET)
			return false;
		layout += line;
		return true;
	};

	append(G_Fmt("xv 0 yv {} cstring2 \"MATCH AWARDS\" ",
		y - MM_AWARDS_HEADER_GAP).data());

	for (size_t i = 0; i < shown; i++) {
		const mm_award_entry_t &entry = entries[i];
		const std::string name = MM_AwardsDisplayName(entry.player_name);

		// A pair is appended whole or not at all: a title with nobody under it
		// reads as a bug rather than as a clipped list.
		const std::string title_line = G_Fmt("yv {} cstring2 \"{}\" ",
			y, MM_AwardTitle(entry.award)).data();
		const std::string name_line = G_Fmt("yv {} cstring \"{}\" ",
			y + MM_AWARDS_TITLE_TO_NAME, name.c_str()).data();

		if (layout.size() + title_line.size() + name_line.size() >
			MM_AWARDS_LAYOUT_BUDGET) {
			// The host suite proves a worst-case full reel fits, so reaching
			// here means an assumption behind that proof has moved. Say so
			// rather than quietly showing a short reel.
			gi.Com_PrintFmt(
				"MM_Awards: layout budget reached; {} of {} award(s) not shown.\n",
				shown - i, shown);
			break;
		}

		append(title_line);
		append(name_line);
		y += MM_AWARDS_PAIR_PITCH;
	}

	return layout;
}

int ClientNumber(const gentity_t *ent)
{
	if (!ent || !ent->client || ent->client < game.clients ||
		ent->client >= game.clients + static_cast<ptrdiff_t>(game.maxclients))
		return -1;

	return static_cast<int>(ent->client - game.clients);
}

// Somebody who can actually read the reel. Bots are moved along by the
// intermission's own ready-to-exit poll and have nothing to look at.
bool IsViewer(const gentity_t *ent)
{
	if (!ent || !ent->inuse || !ent->client || !ent->client->pers.connected)
		return false;

	return !ent->client->sess.is_a_bot && !(ent->svflags & SVF_BOT);
}

int ViewerCount()
{
	int viewers = 0;
	for (auto ec : active_clients())
		if (IsViewer(ec))
			viewers++;

	return viewers;
}

bool ShouldRun()
{
	if (!deathmatch || !deathmatch->integer || !level.intermission_time)
		return false;

	// End-of-unit exits belong to the campaign flow, not to a deathmatch match.
	if (level.intermission_eou)
		return false;

	// Awards are a ranked-match feature: an unranked or bot-padded match has
	// nothing worth engraving anyone's name on.
	if (!s_state.armed || !s_state.ranked)
		return false;

	if (s_state.entries.empty() || s_state.layout.empty())
		return false;

	if (DurationSeconds() <= 0)
		return false;

	// Nobody left to show it to. A ranked reel is armed from the frozen result,
	// so it would otherwise still open and hold an emptied server at the
	// intermission camera for its full duration with nothing being drawn.
	return ViewerCount() > 0;
}

bool Begin()
{
	const int seconds = DurationSeconds();
	if (seconds <= 0)
		return false;

	s_state.running = true;
	s_state.spent = true;
	s_state.end_time = level.time + gtime_t::from_sec(seconds);
	s_state.skip_time =
		level.time + gtime_t::from_sec(MM_AwardsSkipHoldSeconds(seconds));

	// Consume the press that dismissed the scoreboard. It is latched, and the
	// reel would otherwise end the instant its hold expired without anybody
	// having asked it to.
	level.intermission_exit = false;

	// Zeroed so the first draw lands on the very next frame for everybody, and so
	// a client who joins later -- whose slot still holds a stale stamp from a
	// previous match on this level -- is drawn to immediately too.
	s_state.next_refresh.fill(0_ms);

	for (auto ec : active_clients()) {
		// Whatever the client had up belongs to the match that just ended, and a
		// live pmenu would keep drawing itself over the reel.
		P_Menu_Close(ec);
		// P_Menu_Close restores the statusbar for anyone still counted as
		// playing, which MoveClientToIntermission had deliberately hidden.
		ec->client->showscores = true;
		ec->client->ps.stats[STAT_SHOW_STATUSBAR] = 0;
	}

	MuffModeLog("MATCH", "Post-match awards reel opened with %d entr%s for %d second(s)",
		static_cast<int>(s_state.entries.size()),
		s_state.entries.size() == 1 ? "y" : "ies", seconds);
	return true;
}

void End()
{
	s_state.running = false;
	s_state.next_refresh.fill(0_ms);
	// Re-arm the scoreboard's own refresh so whatever draws next -- the pick's
	// menu, or the scoreboard again if it declines -- is pushed immediately.
	for (auto ec : active_clients())
		ec->client->menutime = 0_ms;
}

} // namespace
} // namespace muffmode::awards

namespace awards = muffmode::awards;

void MM_Awards_RegisterCvars()
{
	g_match_awards = gi.cvar("g_match_awards", "10", CVAR_NOFLAGS);
}

void MM_Awards_Reset()
{
	// Only the level-scoped display state is dropped. The decided reel itself is
	// kept so `awards` still answers on the next map, which is exactly when a
	// player who missed it wants to look it up.
	awards::s_state.armed = false;
	awards::s_state.running = false;
	awards::s_state.spent = false;
	awards::s_state.end_time = 0_ms;
	awards::s_state.skip_time = 0_ms;
	awards::s_state.next_refresh.fill(0_ms);
	awards::s_state.layout.clear();
	awards::s_state.layout.shrink_to_fit();
}

void MM_Awards_Publish(std::vector<mm_award_entry_t> entries, bool ranked)
{
	awards::s_state.entries = std::move(entries);
	awards::s_state.ranked = ranked;
	awards::s_state.running = false;
	awards::s_state.spent = false;
	awards::s_state.end_time = 0_ms;
	awards::s_state.layout = awards::BuildLayout(awards::s_state.entries);
	awards::s_state.armed = !awards::s_state.entries.empty();
}

const std::vector<mm_award_entry_t> &MM_Awards_Entries()
{
	return awards::s_state.entries;
}

bool MM_Awards_MatchWasRanked()
{
	return awards::s_state.ranked;
}

bool MM_Awards_BelongsToSettlingMatch()
{
	return awards::s_state.armed && awards::s_state.ranked;
}

bool MM_Awards_Active()
{
	return awards::s_state.running;
}

bool MM_Awards_ClaimIntermissionExit()
{
	if (awards::s_state.running)
		return true;

	if (awards::s_state.spent || !awards::ShouldRun())
		return false;

	return awards::Begin();
}

bool MM_Awards_AcceptsSkip()
{
	return awards::s_state.running && level.time >= awards::s_state.skip_time;
}

void MM_Awards_RunFrame()
{
	if (!awards::s_state.running)
		return;

	// The level moved on without us -- a vote forced the next map, or the match
	// was reset. Drop the reel rather than exiting a level we no longer own.
	if (!level.intermission_time) {
		awards::End();
		return;
	}

	// Somebody pressed past it, everybody left, or nobody did either and the
	// timeout ran out.
	const bool skipped = level.intermission_exit && MM_Awards_AcceptsSkip();
	const bool abandoned = awards::ViewerCount() <= 0;
	if (!skipped && !abandoned && level.time < awards::s_state.end_time)
		return;

	awards::End();
	// The skip is spent here rather than left latched: the next-map pick arms
	// its own, and a flag still set when it opens would close it immediately.
	level.intermission_exit = false;

	// The pick is the last screen before the level changes. It declines when it
	// has nothing to offer, in which case the reel takes the exit itself.
	if (!MM_MapPick_ClaimIntermissionExit())
		ExitLevel();
}

bool MM_Awards_DrawIntermissionLayout(gentity_t *ent)
{
	if (!awards::s_state.running || !ent || !ent->client)
		return false;
	if (awards::s_state.layout.empty())
		return false;

	const int client_num = awards::ClientNumber(ent);
	if (client_num < 0)
		return false;

	if (awards::s_state.next_refresh[client_num] > level.time)
		return true;

	gi.WriteByte(svc_layout);
	gi.WriteString(awards::s_state.layout.c_str());
	gi.unicast(ent, true);

	// The scoreboard's clock is left zeroed while the reel owns the layout, so
	// that whatever draws next is pushed the moment the reel lets go.
	ent->client->menudirty = false;
	ent->client->menutime = 0_ms;
	awards::s_state.next_refresh[client_num] =
		level.time + awards::kRefreshInterval;
	return true;
}

void MM_CmdAwards(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	if (awards::s_state.entries.empty()) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"No match awards have been handed out yet.\n");
		return;
	}

	if (CheckFlood(ent))
		return;

	gi.LocClient_Print(ent, PRINT_HIGH, ":: Match Awards ::\n");
	for (const mm_award_entry_t &entry : awards::s_state.entries) {
		gi.LocClient_Print(ent, PRINT_HIGH, "  {} - {}\n",
			MM_AwardTitle(entry.award), entry.player_name.c_str());
	}
}
