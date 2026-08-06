// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "core/debug_log.h"
#include "muffmode/mm_announcer.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_map_pick.h"
#include "muffmode/mm_map_pool.h"
#include "muffmode/mm_maps.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_util.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

cvar_t *g_map_pick;

namespace muffmode::map_pick {
namespace {

// Layout rows. The pick borrows the vote menu's proportions so the two read as
// the same piece of screen furniture rather than two unrelated dialogs.
constexpr int kTitleRow = 2;
constexpr int kFirstChoiceRow = 4;
constexpr int kBarRow = kFirstChoiceRow + static_cast<int>(MM_MAP_PICK_CHOICES) + 1;
constexpr int kStatusRow = kBarRow + 1;
constexpr int kRequiredRows = kStatusRow + 1;

// How long the winner stays on screen before the level actually changes. WORR
// waits two seconds after its selector closes; MuffMode holds a beat longer
// because the result is read off the menu rather than a dedicated results page.
constexpr gtime_t kRevealHold = 3_sec;

// How often an open pick menu is pushed to a client. Only the countdown moves on
// its own and the bar is drawn in whole segments, so twice a second is plenty.
//
// The pick keeps its own refresh clock rather than reusing client->menutime:
// P_Menu_Update pushes menutime three seconds out whenever the cursor moves, and
// borrowing it would freeze the countdown for anyone browsing the choices.
constexpr gtime_t kMenuRefreshInterval = 500_ms;

struct choice_t {
	std::string bsp;
	std::string title;
};

struct state_t {
	// Set by Match_End: false when a MyMap entry, g_dm_same_level or a forced map
	// already settled the next map, in which case the pick never opens.
	bool next_map_open = false;
	bool running = false;
	bool decided = false;
	gtime_t start_time = 0_ms;
	gtime_t exit_time = 0_ms;
	int duration_seconds = 0;
	int winner = -1;
	std::array<choice_t, MM_MAP_PICK_CHOICES> choices {};
	mm_map_pick_counts_t counts {};
	std::array<int8_t, MAX_CLIENTS> votes {};
	std::array<gtime_t, MAX_CLIENTS> next_refresh {};
};

state_t s_state;

void Finish();

void ClearVotes()
{
	s_state.votes.fill(-1);
	s_state.counts.fill(0);
	s_state.next_refresh.fill(0_ms);
}

int ClientNumber(const gclient_t *client)
{
	if (!client || client < game.clients ||
		client >= game.clients + static_cast<ptrdiff_t>(game.maxclients))
		return -1;

	return static_cast<int>(client - game.clients);
}

int ClientNumber(const gentity_t *ent)
{
	return ent && ent->client ? ClientNumber(ent->client) : -1;
}

// Bots are moved along by the intermission's ready-to-exit poll and have no
// opinion on the next map, so they are not counted as voters either.
bool IsEligibleVoter(const gentity_t *ent)
{
	if (!ent || !ent->inuse || !ent->client || !ent->client->pers.connected)
		return false;
	if (ent->client->sess.is_a_bot || (ent->svflags & SVF_BOT))
		return false;

	return ClientCanVote(ent->client);
}

int EligibleVoterCount()
{
	int voters = 0;
	for (auto ec : active_clients())
		if (IsEligibleVoter(ec))
			voters++;

	return voters;
}

int PickDurationSeconds()
{
	return MM_MapPickDurationSeconds(muffmode::CvarInteger(g_map_pick));
}

mm_map_pick_offered_t OfferedChoices()
{
	mm_map_pick_offered_t offered {};
	for (size_t i = 0; i < MM_MAP_PICK_CHOICES; i++)
		offered[i] = !s_state.choices[i].bsp.empty();

	return offered;
}

int OfferedChoiceCount()
{
	const mm_map_pick_offered_t offered = OfferedChoices();
	return static_cast<int>(std::count(offered.begin(), offered.end(), true));
}

// Recomputes the tallies from the clients that are still here and still allowed
// to vote, retiring anything stale, and returns how many voters remain. Running
// this before every majority test is what keeps a disconnect from leaving behind
// a phantom vote that decides the pick.
int SyncVotes()
{
	const mm_map_pick_counts_t previous = s_state.counts;
	const mm_map_pick_offered_t offered = OfferedChoices();
	std::array<bool, MAX_CLIENTS> live {};
	bool dropped = false;
	int voters = 0;

	s_state.counts.fill(0);

	for (auto ec : active_clients()) {
		const int client_num = ClientNumber(ec);
		if (client_num < 0 || !IsEligibleVoter(ec))
			continue;

		live[client_num] = true;
		voters++;

		const int8_t vote = s_state.votes[client_num];
		if (vote < 0)
			continue;

		if (vote >= static_cast<int8_t>(MM_MAP_PICK_CHOICES) || !offered[vote]) {
			s_state.votes[client_num] = -1;
			dropped = true;
			continue;
		}

		s_state.counts[vote]++;
	}

	// Retire votes belonging to clients who left or lost eligibility, so a later
	// frame cannot resurrect them if the slot is reused.
	for (size_t i = 0; i < s_state.votes.size(); i++) {
		if (!live[i] && s_state.votes[i] != -1) {
			s_state.votes[i] = -1;
			dropped = true;
		}
	}

	if (dropped || s_state.counts != previous)
		P_Menu_Dirty();

	return voters;
}

void MenuUpdate(gentity_t *ent);
void SelectChoice(gentity_t *ent, menu_hnd_t *hnd);

const menu_t kMenuTemplate[] = {
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },	// title
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, SelectChoice },
	{ "", MENU_ALIGN_CENTER, SelectChoice },
	{ "", MENU_ALIGN_CENTER, SelectChoice },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },	// countdown bar
	{ "", MENU_ALIGN_CENTER, nullptr },	// seconds remaining
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr },
	{ "", MENU_ALIGN_CENTER, nullptr }
};

static_assert(std::size(kMenuTemplate) == MENU_MAX_ROWS);
static_assert(kFirstChoiceRow + MM_MAP_PICK_CHOICES <= std::size(kMenuTemplate));
static_assert(kRequiredRows <= static_cast<int>(std::size(kMenuTemplate)));

bool HasPickMenu(const gentity_t *ent)
{
	return ent && ent->client && ent->client->menu &&
		ent->client->menu->UpdateFunc == &MenuUpdate;
}

void OpenMenu(gentity_t *ent)
{
	if (!ent || !ent->client || HasPickMenu(ent))
		return;

	// A menu on a bot is never closed by anything the bot does and keeps
	// unicasting layouts at a client that cannot read them.
	if (ent->client->sess.is_a_bot || (ent->svflags & SVF_BOT))
		return;

	// Whatever the client had up belongs to the match that just ended, and
	// P_Menu_Open declines to replace a live menu on its own.
	P_Menu_Close(ent);
	if (!P_Menu_Open(ent, kMenuTemplate, kFirstChoiceRow,
			muffmode::CountAsInt(kMenuTemplate), nullptr, MenuUpdate)) {
		return;
	}

	const int client_num = ClientNumber(ent);
	if (client_num >= 0)
		s_state.next_refresh[client_num] = level.time + kMenuRefreshInterval;
}

void CloseMenus()
{
	for (auto ec : active_clients())
		if (HasPickMenu(ec))
			P_Menu_Close(ec);
}

int SecondsRemaining()
{
	const float remaining = static_cast<float>(s_state.duration_seconds) -
		(level.time - s_state.start_time).seconds();

	return static_cast<int>(std::ceil(std::max(remaining, 0.0f)));
}

void MenuUpdate(gentity_t *ent)
{
	if (!ent || !ent->client || !ent->client->menu)
		return;

	if (!s_state.running) {
		P_Menu_Close(ent);
		return;
	}

	menu_t *entries = ent->client->menu->entries;
	if (!entries || ent->client->menu->num < kRequiredRows)
		return;

	const int client_num = ClientNumber(ent);
	const int8_t own_vote =
		client_num >= 0 ? s_state.votes[client_num] : int8_t { -1 };
	const bool can_vote = !s_state.decided && IsEligibleVoter(ent);

	// A decided pick with no winner kept the rotation's map, and an otherwise
	// empty "NEXT MAP" heading would read as a bug rather than an outcome.
	P_Menu_SetText(&entries[kTitleRow],
		!s_state.decided ? "*PICK THE NEXT MAP"
			: s_state.winner >= 0 ? "*NEXT MAP"
			: "*KEEPING SCHEDULED MAP");

	for (size_t i = 0; i < MM_MAP_PICK_CHOICES; i++) {
		menu_t &row = entries[kFirstChoiceRow + i];
		const choice_t &choice = s_state.choices[i];
		const bool is_winner = s_state.winner == static_cast<int>(i);

		// Once the pick is over only the winner is worth showing; the losing
		// offers are just noise on the way out.
		if (choice.bsp.empty() || (s_state.decided && !is_winner)) {
			P_Menu_SetText(&row, "");
			row.SelectFunc = nullptr;
			continue;
		}

		// A leading '*' is the menu's own highlight marker, so the map a client
		// voted for -- and the winner -- stand out without a second glyph.
		std::string text;
		if (s_state.decided || own_vote == static_cast<int8_t>(i))
			text += '*';
		text += choice.title;
		if (!s_state.decided && s_state.counts[i] > 0)
			text += G_Fmt(" [{}]", s_state.counts[i]).data();

		P_Menu_SetText(&row, text);
		row.SelectFunc = can_vote ? SelectChoice : nullptr;
	}

	if (s_state.decided) {
		P_Menu_SetText(&entries[kBarRow], "");
		P_Menu_SetText(&entries[kStatusRow], "");
		return;
	}

	const int remaining = SecondsRemaining();
	const int filled = MM_MapPickBarSegments(
		static_cast<float>(remaining),
		static_cast<float>(s_state.duration_seconds));

	P_Menu_SetText(&entries[kBarRow],
		std::string(static_cast<size_t>(filled), '='));
	P_Menu_SetText(&entries[kStatusRow],
		can_vote
			? G_Fmt("{}", remaining).data()
			: G_Fmt("{} - watching", remaining).data());
}

void CastVote(gentity_t *ent, int index)
{
	if (!s_state.running || s_state.decided || !ent || !ent->client)
		return;

	if (index < 0 || index >= static_cast<int>(MM_MAP_PICK_CHOICES) ||
		s_state.choices[index].bsp.empty()) {
		return;
	}

	if (!IsEligibleVoter(ent)) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"You are not allowed to pick the next map.\n");
		return;
	}

	const int client_num = ClientNumber(ent);
	if (client_num < 0 || s_state.votes[client_num] == index)
		return;

	s_state.votes[client_num] = static_cast<int8_t>(index);

	const int voters = SyncVotes();

	gi.LocBroadcast_Print(PRINT_HIGH, "{} picked {}.\n",
		ent->client->resp.netname, s_state.choices[index].title.c_str());
	P_Menu_Dirty();

	// A map the remaining voters can no longer overturn ends the pick early
	// rather than holding everyone at the camera for the rest of the window.
	if (MM_MapPickHasMajority(s_state.counts[index], voters))
		Finish();
}

void SelectChoice(gentity_t *ent, menu_hnd_t *hnd)
{
	if (!ent || !hnd)
		return;

	CastVote(ent, hnd->cur - kFirstChoiceRow);
}

// Commits the winner to level.changemap. CreateTargetChangeLevel stores the map
// in level.nextmap and points changemap at it, so writing that same field keeps
// the string owned by `level` and alive until ExitLevel reads it back.
bool ApplyWinner(const choice_t &choice)
{
	if (choice.bsp.empty() || choice.bsp.size() >= sizeof(level.nextmap) ||
		!MM_IsSafeMapToken(choice.bsp.c_str())) {
		return false;
	}

	Q_strlcpy(level.nextmap, choice.bsp.c_str(), sizeof(level.nextmap));
	level.changemap = level.nextmap;
	return true;
}

void Finish()
{
	if (!s_state.running || s_state.decided)
		return;

	SyncVotes();

	const int winner = MM_MapPickWinner(s_state.counts, OfferedChoices(),
		static_cast<uint32_t>(irandom(1 << 24)));

	s_state.decided = true;
	s_state.winner = -1;
	s_state.exit_time = level.time + kRevealHold;

	if (winner >= 0 && ApplyWinner(s_state.choices[winner])) {
		s_state.winner = winner;
		const choice_t &choice = s_state.choices[winner];

		MuffModeLog("MAP", "Next-map pick chose '%s' with %d vote(s)",
			choice.bsp.c_str(), s_state.counts[winner]);
		gi.LocBroadcast_Print(PRINT_CENTER, "NEXT MAP\n{}\n", choice.title.c_str());
		gi.LocBroadcast_Print(PRINT_HIGH, "Next map: {} ({}).\n",
			choice.title.c_str(), choice.bsp.c_str());
		MM_Announce(mm_announce_event_t::VotePassed, world);
	} else {
		// Nothing usable came out of the pick, so the rotation's own choice --
		// already sitting in level.changemap -- stands.
		MuffModeLog("MAP",
			"Next-map pick produced no usable winner; keeping the rotation's map");
		gi.LocBroadcast_Print(PRINT_HIGH,
			"Next-map pick failed; keeping the scheduled map.\n");
		MM_Announce(mm_announce_event_t::VoteFailed, world);
	}

	P_Menu_Dirty();
}

std::vector<choice_t> CollectChoices()
{
	std::vector<choice_t> choices;

	for (const muffmode::map_pool::map_choice_t &entry :
		MM_CollectStructuredMapChoices(MM_MAP_PICK_CHOICES)) {
		choices.push_back({ entry.bsp, entry.title });
	}

	if (!choices.empty())
		return choices;

	// No structured cycle: fall back to the legacy g_map_pool/g_map_list names,
	// which carry no titles of their own.
	std::vector<std::string> configured = muffmode::maps::CollectConfiguredMaps();
	configured.erase(
		std::remove_if(configured.begin(), configured.end(),
			[](const std::string &map) {
				return !MM_IsSafeMapToken(map.c_str()) ||
					map.size() >= sizeof(level.nextmap) ||
					muffmode::maps::MapTokensEqual(map, level.mapname);
			}),
		configured.end());

	std::shuffle(configured.begin(), configured.end(), mt_rand);
	if (configured.size() > MM_MAP_PICK_CHOICES)
		configured.resize(MM_MAP_PICK_CHOICES);

	for (std::string &map : configured) {
		std::string title = map;
		choices.push_back({ std::move(map), std::move(title) });
	}

	return choices;
}

bool Begin()
{
	const std::vector<choice_t> choices = CollectChoices();

	// One offer is not a pick. Let the rotation's own selection stand and exit
	// the level as normal rather than pausing everyone for a formality.
	if (choices.size() < 2)
		return false;

	ClearVotes();
	s_state.choices.fill(choice_t {});
	for (size_t i = 0; i < choices.size() && i < MM_MAP_PICK_CHOICES; i++)
		s_state.choices[i] = choices[i];

	s_state.running = true;
	s_state.decided = false;
	s_state.winner = -1;
	s_state.duration_seconds = PickDurationSeconds();
	s_state.start_time = level.time;
	s_state.exit_time = 0_ms;

	for (auto ec : active_clients())
		OpenMenu(ec);

	gi.LocBroadcast_Print(PRINT_HIGH,
		"Pick the next map! You have {} seconds.\n", s_state.duration_seconds);
	MM_Announce(mm_announce_event_t::VoteNow, world);

	MuffModeLog("MAP", "Next-map pick opened with %d choice(s) for %d second(s)",
		OfferedChoiceCount(), s_state.duration_seconds);
	return true;
}

bool ShouldRun()
{
	if (!deathmatch->integer || !level.intermission_time)
		return false;

	// GT_ARENA runs independent rooms on one map and reaches the shared
	// intermission only through a session timeout or an empty server, neither of
	// which is a moment to ask an audience to choose anything.
	if (GT(GT_ARENA))
		return false;

	// End-of-unit exits belong to the campaign flow, not to a map rotation.
	if (level.intermission_eou)
		return false;

	// A queued, held or forced next map was already someone's deliberate choice.
	if (!s_state.next_map_open)
		return false;

	if (!PickDurationSeconds())
		return false;

	// Nobody left to ask.
	return EligibleVoterCount() > 0;
}

} // namespace
} // namespace muffmode::map_pick

namespace map_pick = muffmode::map_pick;

void MM_MapPick_RegisterCvars()
{
	g_map_pick = gi.cvar("g_map_pick", "15", CVAR_NOFLAGS);
}

void MM_MapPick_Reset()
{
	map_pick::s_state = {};
	map_pick::ClearVotes();
}

void MM_MapPick_NoteNextMapIsOpen(bool open)
{
	map_pick::s_state.next_map_open = open;
}

bool MM_MapPick_Active()
{
	return map_pick::s_state.running;
}

bool MM_MapPick_ClaimIntermissionExit()
{
	if (map_pick::s_state.running)
		return true;

	if (!map_pick::ShouldRun())
		return false;

	return map_pick::Begin();
}

void MM_MapPick_RunFrame()
{
	if (!map_pick::s_state.running)
		return;

	// The level moved on without us -- a vote forced the next map, or the match
	// was reset. Drop the pick rather than exiting a level we no longer own.
	if (!level.intermission_time) {
		map_pick::CloseMenus();
		MM_MapPick_Reset();
		return;
	}

	if (map_pick::s_state.decided && level.time >= map_pick::s_state.exit_time) {
		map_pick::CloseMenus();
		map_pick::s_state.running = false;
		ExitLevel();
		return;
	}

	// Catches clients who connected mid-pick, and reopens the menu for anyone
	// whose client dropped it. The winner reveal is covered too: a menu lost
	// during those last few seconds used to stay lost, taking the result with it.
	for (auto ec : active_clients())
		map_pick::OpenMenu(ec);

	// Nothing left to tally once the winner is decided; the reveal just holds.
	if (map_pick::s_state.decided)
		return;

	const int voters = map_pick::SyncVotes();
	const gtime_t deadline = map_pick::s_state.start_time +
		gtime_t::from_sec(map_pick::s_state.duration_seconds);

	if (voters <= 0 || level.time >= deadline)
		map_pick::Finish();
}

bool MM_MapPick_MenuOpen(const gentity_t *ent)
{
	return map_pick::s_state.running && map_pick::HasPickMenu(ent);
}

bool MM_MapPick_DrawIntermissionMenu(gentity_t *ent)
{
	if (!MM_MapPick_MenuOpen(ent))
		return false;

	const int client_num = map_pick::ClientNumber(ent);
	if (client_num < 0)
		return true;

	// ClientBeginServerFrame bails out of the whole frame during intermission, so
	// nothing else is going to push this menu: the pick redraws it here on its own
	// clock, and immediately whenever a tally or selection marked it dirty.
	if (!ent->client->menudirty &&
		level.time < map_pick::s_state.next_refresh[client_num]) {
		return true;
	}

	menu_hnd_t *menu = ent->client->menu;
	P_Menu_Do_Update(ent);
	if (ent->client->menu != menu)
		return true;

	gi.unicast(ent, false);
	ent->client->menudirty = false;
	map_pick::s_state.next_refresh[client_num] =
		level.time + map_pick::kMenuRefreshInterval;
	return true;
}

void MM_MapPick_ClearClientVote(gentity_t *ent)
{
	const int client_num = map_pick::ClientNumber(ent);
	if (client_num < 0 || map_pick::s_state.votes[client_num] == -1)
		return;

	map_pick::s_state.votes[client_num] = -1;
	if (map_pick::s_state.running && !map_pick::s_state.decided)
		map_pick::SyncVotes();
}

void MM_CmdMapPick(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	if (!map_pick::s_state.running) {
		gi.LocClient_Print(ent, PRINT_HIGH, "No map pick is in progress.\n");
		return;
	}

	if (gi.argc() < 2) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"Pick the next map with 'mappick <number>':\n");
		for (size_t i = 0; i < MM_MAP_PICK_CHOICES; i++) {
			const map_pick::choice_t &choice = map_pick::s_state.choices[i];
			if (choice.bsp.empty())
				continue;

			gi.LocClient_Print(ent, PRINT_HIGH, "  {}: {} ({}) - {} vote(s)\n",
				static_cast<int>(i) + 1, choice.title.c_str(),
				choice.bsp.c_str(), map_pick::s_state.counts[i]);
		}
		return;
	}

	if (CheckFlood(ent))
		return;

	const auto parsed = MM_ParseNonNegativeIntArg(gi.argv(1));
	if (!parsed || *parsed < 1 ||
		*parsed > static_cast<int>(MM_MAP_PICK_CHOICES) ||
		map_pick::s_state.choices[*parsed - 1].bsp.empty()) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"Pick a map number from 1 to {}.\n", map_pick::OfferedChoiceCount());
		return;
	}

	map_pick::CastVote(ent, *parsed - 1);
}
