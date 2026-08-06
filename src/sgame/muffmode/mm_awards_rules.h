// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// [MuffMode] Catalog and selection rules behind the post-match awards reel. The
// reel is the last thing a ranked match shows: after the scoreboard, after the
// next-map pick, at the point the level would otherwise change, the scores come
// down and a short list of titles goes up instead.
//
// Everything here is arithmetic over a plain snapshot of one player's match
// counters, deliberately free of game headers so the host suite can exercise the
// whole catalog without a live entity world. The module .cpp only fills the
// snapshots in and draws the result.
//
// An award is a superlative, not a participation trophy. Every entry needs a
// strict winner -- ties award nobody -- and a floor low enough to be reachable in
// a short match but high enough that the title still means something.

// The reel has to be readable at a glance from the intermission camera, and the
// layout string it becomes is capped at a single network message. Twelve pairs of
// title and name is what comfortably fits both.
inline constexpr size_t MM_AWARDS_DISPLAY_LIMIT = 12;

// One player running away with a match should not run away with the whole reel.
// The first pass hands out at most this many titles each; anything left over is
// back-filled from the remainder, so a genuinely dominant player can still take
// more when nobody else qualified.
inline constexpr size_t MM_AWARDS_SOFT_PER_PLAYER_LIMIT = 3;

// MAX_STRING_CHARS is 1024 and the client's layout buffer is the same size, so
// the built string has to terminate inside 1023 bytes. Unlike the scoreboard this
// layout contains no if/endif, so it needs no footer reserve: a truncated award
// line is ugly, but a truncated conditional is a client-side fatal error.
inline constexpr size_t MM_AWARDS_LAYOUT_BUDGET = 1023;

// Names are ellipsised past this width. The binding constraint is not the screen
// but the layout budget: a full twelve-pair reel of the longest titles in the
// catalog has to terminate inside MM_AWARDS_LAYOUT_BUDGET, and the name is the
// only part of a line that is not fixed. The host suite measures the worst case
// against the budget, so raising this without re-checking that test will start
// silently dropping the last pair off a full reel.
inline constexpr size_t MM_AWARDS_MAX_NAME_CHARS = 18;

// Vertical geometry, in the 320x240 layout space that xv/yv address. A pair is a
// green title with a white name directly under it; the extra gap before the next
// title is the blank spacer line between pairs.
inline constexpr int MM_AWARDS_TITLE_TO_NAME = 10;
inline constexpr int MM_AWARDS_PAIR_PITCH = 30;
inline constexpr int MM_AWARDS_HEADER_GAP = 26;

// Vertical centre of the 320x240 box that xv/yv address.
inline constexpr int MM_AWARDS_LAYOUT_CENTRE_Y = 120;

// Bounds on the configured reel length. The floor is long enough to read a full
// twelve-entry reel; the ceiling keeps a fat-fingered cvar from parking everyone
// at the intermission camera for a minute after every match.
inline constexpr int MM_AWARDS_MIN_SECONDS = 3;
inline constexpr int MM_AWARDS_MAX_SECONDS = 30;

// How long the reel stays up unattended, or 0 when it is switched off. Zero and
// negatives read as "disabled" rather than clamping up to the floor, so one cvar
// both enables the reel and sets its length -- the same contract as g_map_pick.
inline int MM_AwardsDurationSeconds(int configured)
{
	if (configured <= 0)
		return 0;

	return std::clamp(configured, MM_AWARDS_MIN_SECONDS, MM_AWARDS_MAX_SECONDS);
}

// The reel takes a key press to move on early, exactly as the scoreboard before
// it does, but not immediately: the same press that dismissed the scoreboard is
// still being held a frame later and would carry straight through the reel
// without anybody seeing it. This is how long it refuses the skip for.
inline constexpr int MM_AWARDS_MIN_HOLD_SECONDS = 3;

// The hold, capped strictly below the reel's own length so that a short reel is
// still skippable at all rather than being held for its whole duration.
inline int MM_AwardsSkipHoldSeconds(int duration_seconds)
{
	if (duration_seconds <= 0)
		return 0;

	return std::min(MM_AWARDS_MIN_HOLD_SECONDS, std::max(1, duration_seconds - 1));
}

// Layout-space y of the first title, chosen so the whole block sits centred.
inline int MM_AwardsBlockTop(size_t shown)
{
	if (!shown)
		return MM_AWARDS_LAYOUT_CENTRE_Y;

	const int height = static_cast<int>(shown - 1) * MM_AWARDS_PAIR_PITCH +
		MM_AWARDS_TITLE_TO_NAME;

	return MM_AWARDS_LAYOUT_CENTRE_Y - height / 2;
}

// Name as it appears under a title: layout-token hostile bytes removed, then
// clipped to a width that stays on screen at 4:3. Truncation never splits a
// UTF-8 sequence, since a stray continuation byte would render as garbage.
inline std::string MM_AwardsDisplayName(std::string_view name)
{
	std::string safe;
	safe.reserve(std::min(name.size(), MM_AWARDS_MAX_NAME_CHARS + 3));

	for (const char raw : name) {
		const unsigned char value = static_cast<unsigned char>(raw);
		// A quote would close the token early and a backslash could escape the
		// one that follows; both hand the rest of the name to the layout parser.
		if (value < 32 || value == 127 || raw == '"' || raw == '\\')
			continue;
		safe.push_back(raw);
	}

	if (safe.size() <= MM_AWARDS_MAX_NAME_CHARS)
		return safe;

	size_t cut = MM_AWARDS_MAX_NAME_CHARS;
	while (cut > 0 && (static_cast<unsigned char>(safe[cut]) & 0xC0) == 0x80)
		cut--;

	safe.resize(cut);
	safe.append("...");
	return safe;
}

enum class mm_match_award_t : uint8_t {
	none,
	top_dog,
	first_blood,
	shotgun_sheriff,
	rail_slut,
	rocket_surgeon,
	lead_farmer,
	pineapple_express,
	big_fragging_gun,
	quad_god,
	quad_dummy,
	aimbot,
	untouchable,
	wrecking_ball,
	spawn_fragger,
	dirty_rotten_camper,
	humiliator,
	on_a_rampage,
	flag_runner,
	last_line,
	return_to_sender,
	pack_mule,
	punching_bag,
	stormtrooper,
	bullet_sponge,
	mayfly,
	cockroach,
	lemming,
	butterfingers,
	benedict_arnold,
	kleptomaniac,
	total
};

inline constexpr size_t MM_MATCH_AWARD_COUNT =
	static_cast<size_t>(mm_match_award_t::total);

// The exported award taxonomy is a public artifact contract, exactly like the
// medal schema next door: adding an entry is additive, renumbering is not.
static_assert(MM_MATCH_AWARD_COUNT == 31,
	"The public match-award schema must remain stable");

// Qualification thresholds. Each is named rather than inlined so a server owner
// reading the docs and a maintainer reading the code see the same numbers.
inline constexpr uint32_t MM_AWARD_SIGNATURE_KILLS = 20;	// shotgun / rail
inline constexpr uint32_t MM_AWARD_SIGNATURE_SHARE_PCT = 80;
inline constexpr uint32_t MM_AWARD_ROCKET_KILLS = 15;
inline constexpr uint32_t MM_AWARD_ROCKET_SHARE_PCT = 50;
inline constexpr uint32_t MM_AWARD_BULLET_KILLS = 25;
inline constexpr uint32_t MM_AWARD_BULLET_SHARE_PCT = 60;
inline constexpr uint32_t MM_AWARD_GRENADE_KILLS = 5;
inline constexpr uint32_t MM_AWARD_BFG_KILLS = 5;
inline constexpr uint32_t MM_AWARD_QUAD_CONTROL_PCT = 90;
inline constexpr uint32_t MM_AWARD_QUAD_KILL_SHARE_PCT = 50;
inline constexpr uint32_t MM_AWARD_QUAD_GOD_MIN_KILLS = 5;
inline constexpr uint32_t MM_AWARD_ACCURACY_PCT = 80;
inline constexpr uint64_t MM_AWARD_ACCURACY_MIN_SHOTS = 150;
inline constexpr uint32_t MM_AWARD_STORMTROOPER_PCT = 15;
inline constexpr uint64_t MM_AWARD_STORMTROOPER_MIN_SHOTS = 200;
inline constexpr uint32_t MM_AWARD_UNTOUCHABLE_KILLS = 15;
inline constexpr uint32_t MM_AWARD_UNTOUCHABLE_KD_X10 = 30;	// K/D >= 3.0
inline constexpr uint32_t MM_AWARD_SPAWN_KILLS = 5;
inline constexpr uint32_t MM_AWARD_SPAWN_SHARE_PCT = 25;
inline constexpr uint32_t MM_AWARD_CAMP_SHARE_PCT = 60;
inline constexpr uint32_t MM_AWARD_CAMP_MIN_SAMPLES = 90;
inline constexpr uint32_t MM_AWARD_CAMP_MAX_IDLE_PCT = 20;
inline constexpr uint64_t MM_AWARD_CAMP_MIN_SHOTS = 30;
inline constexpr uint32_t MM_AWARD_HUMILIATION_MEDALS = 3;
inline constexpr uint32_t MM_AWARD_EXCELLENT_MEDALS = 3;
inline constexpr uint32_t MM_AWARD_PUNCHING_BAG_DEATHS = 10;
inline constexpr uint32_t MM_AWARD_PUNCHING_BAG_KD_X100 = 35;	// K/D <= 0.35
inline constexpr uint32_t MM_AWARD_PUNCHING_BAG_DMG_X100 = 35;	// dealt/taken <= 0.35
inline constexpr uint32_t MM_AWARD_MAYFLY_LIVES = 8;
inline constexpr uint32_t MM_AWARD_MAYFLY_LIFE_MSEC = 12000;
inline constexpr uint32_t MM_AWARD_COCKROACH_LIVES = 5;
inline constexpr uint32_t MM_AWARD_COCKROACH_LIFE_MSEC = 45000;
inline constexpr uint32_t MM_AWARD_LEMMING_DEATHS = 4;
inline constexpr uint32_t MM_AWARD_SUICIDES = 3;
inline constexpr uint32_t MM_AWARD_TEAM_KILLS = 3;
inline constexpr uint32_t MM_AWARD_PICKUPS = 15;
inline constexpr uint32_t MM_AWARD_CTF_CAPTURES = 2;
inline constexpr uint32_t MM_AWARD_CTF_DEFENCES = 3;
inline constexpr uint32_t MM_AWARD_CTF_RETURNS = 5;
inline constexpr uint64_t MM_AWARD_CTF_CARRIER_MSEC = 30000;
inline constexpr uint32_t MM_AWARD_DAMAGE_LEAD_PCT = 150;	// 1.5x the field average

// A match too short or too thin to be interesting produces no reel at all.
inline constexpr size_t MM_AWARD_MIN_PARTICIPANTS = 2;
inline constexpr uint64_t MM_AWARD_MIN_DURATION_MSEC = 60000;

// One player's finished match, flattened. Everything the catalog can reason about
// lives here; nothing in this header knows what a gentity_t is.
struct mm_award_player_facts_t {
	uint32_t kills = 0;
	uint32_t deaths = 0;
	uint32_t suicides = 0;
	uint32_t team_kills = 0;
	uint32_t spawn_kills = 0;
	uint32_t environment_deaths = 0;

	uint64_t shots = 0;
	uint64_t hits = 0;
	uint64_t damage_dealt = 0;
	uint64_t damage_received = 0;

	// Kills collapsed onto the weapon families the catalog cares about. Splash
	// variants are folded into their parent weapon by the caller.
	uint32_t shotgun_kills = 0;
	uint32_t rail_kills = 0;
	uint32_t rocket_kills = 0;
	uint32_t bullet_kills = 0;
	uint32_t grenade_kills = 0;
	uint32_t bfg_kills = 0;

	uint32_t quad_pickups = 0;
	uint32_t quad_kills = 0;

	uint32_t excellent_medals = 0;
	uint32_t humiliation_medals = 0;
	uint32_t rampage_medals = 0;
	uint32_t first_frag_medals = 0;

	uint32_t ctf_captures = 0;
	uint32_t ctf_defences = 0;
	uint32_t ctf_returns = 0;
	uint64_t ctf_carrier_time_msec = 0;

	uint32_t high_value_pickups = 0;

	uint32_t completed_lives = 0;
	uint32_t life_average_msec = 0;

	// Position dwell, sampled once a second while alive and playing.
	// camp_best_cell_samples is the busiest single map cell of those samples.
	uint32_t camp_samples = 0;
	uint32_t camp_idle_samples = 0;
	uint32_t camp_best_cell_samples = 0;

	uint64_t play_time_msec = 0;
	int32_t score = 0;
	bool bot = false;
};

struct mm_award_match_facts_t {
	uint32_t quad_spawns = 0;
	uint32_t total_kills = 0;
	uint64_t duration_msec = 0;
	size_t participants = 0;
	bool team_mode = false;
	bool ctf_mode = false;
};

struct mm_award_result_t {
	mm_match_award_t award = mm_match_award_t::none;
	size_t player_index = 0;
	// The number that won it: the kill count, the accuracy percentage, the
	// capture total. Carried through to the exports so a reader can see why.
	uint32_t value = 0;
};

// Display title, shown in green on the reel and used verbatim in the HTML export.
inline const char *MM_AwardTitle(mm_match_award_t award)
{
	switch (award) {
	case mm_match_award_t::top_dog: return "TOP DOG";
	case mm_match_award_t::first_blood: return "FIRST BLOOD";
	case mm_match_award_t::shotgun_sheriff: return "SHOTGUN SHERIFF";
	case mm_match_award_t::rail_slut: return "RAIL SLUT";
	case mm_match_award_t::rocket_surgeon: return "ROCKET SURGEON";
	case mm_match_award_t::lead_farmer: return "LEAD FARMER";
	case mm_match_award_t::pineapple_express: return "PINEAPPLE EXPRESS";
	case mm_match_award_t::big_fragging_gun: return "BIG FRAGGING GUN";
	case mm_match_award_t::quad_god: return "QUAD GOD";
	case mm_match_award_t::quad_dummy: return "QUAD DUMMY";
	case mm_match_award_t::aimbot: return "AIMBOT ALLEGATIONS";
	case mm_match_award_t::untouchable: return "UNTOUCHABLE";
	case mm_match_award_t::wrecking_ball: return "WRECKING BALL";
	case mm_match_award_t::spawn_fragger: return "SPAWN FRAGGER";
	case mm_match_award_t::dirty_rotten_camper: return "DIRTY ROTTEN CAMPER";
	case mm_match_award_t::humiliator: return "THE HUMILIATOR";
	case mm_match_award_t::on_a_rampage: return "ON A RAMPAGE";
	case mm_match_award_t::flag_runner: return "FLAG RUNNER";
	case mm_match_award_t::last_line: return "LAST LINE OF DEFENCE";
	case mm_match_award_t::return_to_sender: return "RETURN TO SENDER";
	case mm_match_award_t::pack_mule: return "PACK MULE";
	case mm_match_award_t::punching_bag: return "THE PUNCHING BAG";
	case mm_match_award_t::stormtrooper: return "STORMTROOPER";
	case mm_match_award_t::bullet_sponge: return "BULLET SPONGE";
	case mm_match_award_t::mayfly: return "MAYFLY";
	case mm_match_award_t::cockroach: return "THE COCKROACH";
	case mm_match_award_t::lemming: return "LEMMING";
	case mm_match_award_t::butterfingers: return "BUTTERFINGERS";
	case mm_match_award_t::benedict_arnold: return "BENEDICT ARNOLD";
	case mm_match_award_t::kleptomaniac: return "KLEPTOMANIAC";
	case mm_match_award_t::none:
	case mm_match_award_t::total:
		break;
	}
	return "";
}

// Stable machine-readable name. Titles are cosmetic and may be reworded; these
// are the keys career tallies and match exports are written under, so they are
// not free to change.
inline const char *MM_AwardKey(mm_match_award_t award)
{
	switch (award) {
	case mm_match_award_t::top_dog: return "topDog";
	case mm_match_award_t::first_blood: return "firstBlood";
	case mm_match_award_t::shotgun_sheriff: return "shotgunSheriff";
	case mm_match_award_t::rail_slut: return "railSlut";
	case mm_match_award_t::rocket_surgeon: return "rocketSurgeon";
	case mm_match_award_t::lead_farmer: return "leadFarmer";
	case mm_match_award_t::pineapple_express: return "pineappleExpress";
	case mm_match_award_t::big_fragging_gun: return "bigFraggingGun";
	case mm_match_award_t::quad_god: return "quadGod";
	case mm_match_award_t::quad_dummy: return "quadDummy";
	case mm_match_award_t::aimbot: return "aimbotAllegations";
	case mm_match_award_t::untouchable: return "untouchable";
	case mm_match_award_t::wrecking_ball: return "wreckingBall";
	case mm_match_award_t::spawn_fragger: return "spawnFragger";
	case mm_match_award_t::dirty_rotten_camper: return "dirtyRottenCamper";
	case mm_match_award_t::humiliator: return "humiliator";
	case mm_match_award_t::on_a_rampage: return "onARampage";
	case mm_match_award_t::flag_runner: return "flagRunner";
	case mm_match_award_t::last_line: return "lastLineOfDefence";
	case mm_match_award_t::return_to_sender: return "returnToSender";
	case mm_match_award_t::pack_mule: return "packMule";
	case mm_match_award_t::punching_bag: return "punchingBag";
	case mm_match_award_t::stormtrooper: return "stormtrooper";
	case mm_match_award_t::bullet_sponge: return "bulletSponge";
	case mm_match_award_t::mayfly: return "mayfly";
	case mm_match_award_t::cockroach: return "cockroach";
	case mm_match_award_t::lemming: return "lemming";
	case mm_match_award_t::butterfingers: return "butterfingers";
	case mm_match_award_t::benedict_arnold: return "benedictArnold";
	case mm_match_award_t::kleptomaniac: return "kleptomaniac";
	case mm_match_award_t::none:
	case mm_match_award_t::total:
		break;
	}
	return "";
}

// Ordering tier. The reel reads top-down, so the match-defining titles come
// first, the specialisms next, and the wooden spoons last -- which also means
// that when more than MM_AWARDS_DISPLAY_LIMIT awards qualify, the jokes are what
// get dropped rather than the honours.
inline int MM_AwardTier(mm_match_award_t award)
{
	switch (award) {
	case mm_match_award_t::top_dog:
	case mm_match_award_t::first_blood:
		return 0;
	case mm_match_award_t::shotgun_sheriff:
	case mm_match_award_t::rail_slut:
	case mm_match_award_t::rocket_surgeon:
	case mm_match_award_t::lead_farmer:
	case mm_match_award_t::pineapple_express:
	case mm_match_award_t::big_fragging_gun:
	case mm_match_award_t::quad_god:
		return 1;
	case mm_match_award_t::flag_runner:
	case mm_match_award_t::last_line:
	case mm_match_award_t::return_to_sender:
	case mm_match_award_t::pack_mule:
		return 2;
	case mm_match_award_t::aimbot:
	case mm_match_award_t::untouchable:
	case mm_match_award_t::wrecking_ball:
	case mm_match_award_t::on_a_rampage:
	case mm_match_award_t::humiliator:
		return 3;
	case mm_match_award_t::spawn_fragger:
	case mm_match_award_t::dirty_rotten_camper:
	case mm_match_award_t::cockroach:
	case mm_match_award_t::kleptomaniac:
		return 4;
	default:
		break;
	}
	return 5;
}

// True when the match itself is worth handing out awards for. Ranked-ness is the
// caller's business; this is only about there being a real contest to judge.
inline bool MM_AwardsMatchQualifies(const mm_award_match_facts_t &match)
{
	return match.participants >= MM_AWARD_MIN_PARTICIPANTS &&
		match.duration_msec >= MM_AWARD_MIN_DURATION_MSEC;
}

// Percentage of `whole` that `part` represents, saturating and division-safe. A
// zero whole reads as 0%, which fails every "at least N%" test by construction.
inline uint32_t MM_AwardPercent(uint64_t part, uint64_t whole)
{
	if (!whole)
		return 0;
	if (part >= whole)
		return 100;

	return static_cast<uint32_t>(part * 100 / whole);
}

// One qualifying award before ranking. Kept out of the detail namespace because
// MM_AwardsCollect hands these back to callers -- the host suite reads them
// directly to assert which titles a given field of players produces.
struct mm_award_candidate_t {
	mm_match_award_t award = mm_match_award_t::none;
	size_t player_index = 0;
	uint32_t value = 0;
	int tier = 0;
};

namespace mm_awards_detail {

// Index of the strict maximum of `value` over the field, or npos when the field
// is empty, nobody cleared `floor`, or the best score is shared. Requiring a
// strict win is what keeps "more than anyone else in the match" honest: a two-way
// tie awards nobody rather than picking by slot number.
template<typename Fn>
inline size_t StrictLeader(const mm_award_player_facts_t *players, size_t count,
	uint64_t floor_value, Fn value)
{
	size_t leader = static_cast<size_t>(-1);
	uint64_t best = 0;
	bool tied = false;

	for (size_t i = 0; i < count; i++) {
		const uint64_t score = value(players[i]);
		if (score < floor_value || !score)
			continue;

		if (leader == static_cast<size_t>(-1) || score > best) {
			leader = i;
			best = score;
			tied = false;
		} else if (score == best) {
			tied = true;
		}
	}

	return tied ? static_cast<size_t>(-1) : leader;
}

inline uint32_t KdRatioX100(const mm_award_player_facts_t &player)
{
	if (!player.deaths)
		return player.kills ? 10000u : 0u;

	return static_cast<uint32_t>(
		std::min<uint64_t>(player.kills * 100ull / player.deaths, 10000ull));
}

inline uint32_t DamageRatioX100(const mm_award_player_facts_t &player)
{
	if (!player.damage_received)
		return player.damage_dealt ? 10000u : 0u;

	return static_cast<uint32_t>(std::min<uint64_t>(
		player.damage_dealt * 100ull / player.damage_received, 10000ull));
}

inline void Add(mm_award_candidate_t *out, size_t &count, size_t capacity,
	mm_match_award_t award, size_t player_index, uint32_t value)
{
	if (player_index == static_cast<size_t>(-1) || count >= capacity)
		return;

	out[count++] = { award, player_index, value, MM_AwardTier(award) };
}

// A "signature weapon" title: most kills with the family, more than anyone else,
// a minimum body count, and the family accounting for most of the player's work.
template<typename Fn>
inline void AddSignature(mm_award_candidate_t *out, size_t &count, size_t capacity,
	const mm_award_player_facts_t *players, size_t player_count,
	mm_match_award_t award, uint32_t min_kills, uint32_t min_share_pct, Fn kills)
{
	const size_t leader = StrictLeader(players, player_count, min_kills, kills);
	if (leader == static_cast<size_t>(-1))
		return;

	const uint32_t family = static_cast<uint32_t>(kills(players[leader]));
	if (MM_AwardPercent(family, players[leader].kills) < min_share_pct)
		return;

	Add(out, count, capacity, award, leader, family);
}

} // namespace mm_awards_detail

// Builds the unranked candidate set: every award that has a qualifying strict
// winner, in catalog order. Returns how many were written.
inline size_t MM_AwardsCollect(const mm_award_player_facts_t *players, size_t count,
	const mm_award_match_facts_t &match,
	mm_award_candidate_t *out, size_t capacity)
{
	using namespace mm_awards_detail;

	size_t written = 0;
	if (!players || !count || !out || !capacity)
		return 0;
	if (!MM_AwardsMatchQualifies(match))
		return 0;

	// -- Honours -------------------------------------------------------------
	// Top Dog is only interesting when there was a field to beat.
	if (count >= 3) {
		const size_t leader = StrictLeader(players, count, 1,
			[](const mm_award_player_facts_t &p) {
				return p.score > 0 ? static_cast<uint64_t>(p.score) : 0ull;
			});
		Add(out, written, capacity, mm_match_award_t::top_dog, leader,
			leader == static_cast<size_t>(-1)
				? 0u : static_cast<uint32_t>(players[leader].score));
	}

	{
		const size_t leader = StrictLeader(players, count, 1,
			[](const mm_award_player_facts_t &p) {
				return static_cast<uint64_t>(p.first_frag_medals);
			});
		Add(out, written, capacity, mm_match_award_t::first_blood, leader, 1);
	}

	// -- Signature weapons ---------------------------------------------------
	AddSignature(out, written, capacity, players, count,
		mm_match_award_t::shotgun_sheriff, MM_AWARD_SIGNATURE_KILLS,
		MM_AWARD_SIGNATURE_SHARE_PCT,
		[](const mm_award_player_facts_t &p) {
			return static_cast<uint64_t>(p.shotgun_kills);
		});
	AddSignature(out, written, capacity, players, count,
		mm_match_award_t::rail_slut, MM_AWARD_SIGNATURE_KILLS,
		MM_AWARD_SIGNATURE_SHARE_PCT,
		[](const mm_award_player_facts_t &p) {
			return static_cast<uint64_t>(p.rail_kills);
		});
	AddSignature(out, written, capacity, players, count,
		mm_match_award_t::rocket_surgeon, MM_AWARD_ROCKET_KILLS,
		MM_AWARD_ROCKET_SHARE_PCT,
		[](const mm_award_player_facts_t &p) {
			return static_cast<uint64_t>(p.rocket_kills);
		});
	AddSignature(out, written, capacity, players, count,
		mm_match_award_t::lead_farmer, MM_AWARD_BULLET_KILLS,
		MM_AWARD_BULLET_SHARE_PCT,
		[](const mm_award_player_facts_t &p) {
			return static_cast<uint64_t>(p.bullet_kills);
		});

	// Hand grenades and the BFG are rare enough that raw volume is the story;
	// no share test, or nobody would ever take them.
	{
		const size_t leader = StrictLeader(players, count, MM_AWARD_GRENADE_KILLS,
			[](const mm_award_player_facts_t &p) {
				return static_cast<uint64_t>(p.grenade_kills);
			});
		Add(out, written, capacity, mm_match_award_t::pineapple_express, leader,
			leader == static_cast<size_t>(-1) ? 0 : players[leader].grenade_kills);
	}
	{
		const size_t leader = StrictLeader(players, count, MM_AWARD_BFG_KILLS,
			[](const mm_award_player_facts_t &p) {
				return static_cast<uint64_t>(p.bfg_kills);
			});
		Add(out, written, capacity, mm_match_award_t::big_fragging_gun, leader,
			leader == static_cast<size_t>(-1) ? 0 : players[leader].bfg_kills);
	}

	// -- The Quad ------------------------------------------------------------
	// Control is taking the Quad off the floor more than anyone else and letting
	// almost none of its spawns go to somebody else. What the holder then DID
	// with it decides which of the two titles they walk away with.
	if (match.quad_spawns > 0) {
		const size_t leader = StrictLeader(players, count, 1,
			[](const mm_award_player_facts_t &p) {
				return static_cast<uint64_t>(p.quad_pickups);
			});
		// More pickups than there were spawns means Quads reached this player by
		// some route the spawn counter does not see -- a teammate's hand-dropped
		// Quad, most likely. MM_AwardPercent would saturate that to full control
		// and hand out a title on an impossible figure, so the whole category
		// stands down rather than guessing.
		if (leader != static_cast<size_t>(-1) &&
			players[leader].quad_pickups <= match.quad_spawns) {
			const mm_award_player_facts_t &boss = players[leader];
			const uint32_t control =
				MM_AwardPercent(boss.quad_pickups, match.quad_spawns);
			if (control >= MM_AWARD_QUAD_CONTROL_PCT) {
				const uint32_t kill_share =
					MM_AwardPercent(boss.quad_kills, boss.kills);
				const bool earned = boss.kills >= MM_AWARD_QUAD_GOD_MIN_KILLS &&
					kill_share >= MM_AWARD_QUAD_KILL_SHARE_PCT;
				Add(out, written, capacity,
					earned ? mm_match_award_t::quad_god
						: mm_match_award_t::quad_dummy,
					leader, control);
			}
		}
	}

	// -- Craft ---------------------------------------------------------------
	{
		size_t leader = static_cast<size_t>(-1);
		uint32_t best = 0;
		bool tied = false;
		for (size_t i = 0; i < count; i++) {
			if (players[i].shots < MM_AWARD_ACCURACY_MIN_SHOTS)
				continue;
			const uint32_t accuracy =
				MM_AwardPercent(players[i].hits, players[i].shots);
			if (accuracy < MM_AWARD_ACCURACY_PCT)
				continue;
			if (leader == static_cast<size_t>(-1) || accuracy > best) {
				leader = i;
				best = accuracy;
				tied = false;
			} else if (accuracy == best) {
				tied = true;
			}
		}
		if (!tied)
			Add(out, written, capacity, mm_match_award_t::aimbot, leader, best);
	}

	{
		const size_t leader = StrictLeader(players, count,
			MM_AWARD_UNTOUCHABLE_KD_X10,
			[](const mm_award_player_facts_t &p) {
				if (p.kills < MM_AWARD_UNTOUCHABLE_KILLS)
					return 0ull;
				return static_cast<uint64_t>(KdRatioX100(p) / 10);
			});
		Add(out, written, capacity, mm_match_award_t::untouchable, leader,
			leader == static_cast<size_t>(-1) ? 0 : KdRatioX100(players[leader]));
	}

	// Damage superlatives are only worth showing when the winner actually stood
	// out; in a close match "most damage" is just "played the longest".
	{
		uint64_t total_dealt = 0;
		uint64_t total_taken = 0;
		for (size_t i = 0; i < count; i++) {
			total_dealt += players[i].damage_dealt;
			total_taken += players[i].damage_received;
		}
		const uint64_t average_dealt = total_dealt / count;
		const uint64_t average_taken = total_taken / count;

		// Compared as a ratio rather than through MM_AwardPercent: that helper
		// saturates at 100, and the leader's damage is by definition at least the
		// field average, so routing this through it would make both of these
		// awards unwinnable.
		const auto leads_field = [](uint64_t value, uint64_t average) {
			return average > 0 &&
				value >= average * MM_AWARD_DAMAGE_LEAD_PCT / 100;
		};

		const size_t dealer = StrictLeader(players, count, 1,
			[](const mm_award_player_facts_t &p) { return p.damage_dealt; });
		if (dealer != static_cast<size_t>(-1) &&
			leads_field(players[dealer].damage_dealt, average_dealt)) {
			Add(out, written, capacity, mm_match_award_t::wrecking_ball, dealer,
				static_cast<uint32_t>(std::min<uint64_t>(
					players[dealer].damage_dealt, UINT32_MAX)));
		}

		const size_t sponge = StrictLeader(players, count, 1,
			[](const mm_award_player_facts_t &p) { return p.damage_received; });
		if (sponge != static_cast<size_t>(-1) &&
			leads_field(players[sponge].damage_received, average_taken)) {
			Add(out, written, capacity, mm_match_award_t::bullet_sponge, sponge,
				static_cast<uint32_t>(std::min<uint64_t>(
					players[sponge].damage_received, UINT32_MAX)));
		}
	}

	{
		const size_t leader = StrictLeader(players, count,
			MM_AWARD_EXCELLENT_MEDALS,
			[](const mm_award_player_facts_t &p) {
				return static_cast<uint64_t>(p.excellent_medals);
			});
		Add(out, written, capacity, mm_match_award_t::on_a_rampage, leader,
			leader == static_cast<size_t>(-1) ? 0 : players[leader].excellent_medals);
	}
	{
		const size_t leader = StrictLeader(players, count,
			MM_AWARD_HUMILIATION_MEDALS,
			[](const mm_award_player_facts_t &p) {
				return static_cast<uint64_t>(p.humiliation_medals);
			});
		Add(out, written, capacity, mm_match_award_t::humiliator, leader,
			leader == static_cast<size_t>(-1)
				? 0 : players[leader].humiliation_medals);
	}

	// -- Habits --------------------------------------------------------------
	{
		const size_t leader = StrictLeader(players, count, MM_AWARD_SPAWN_KILLS,
			[](const mm_award_player_facts_t &p) {
				return static_cast<uint64_t>(p.spawn_kills);
			});
		if (leader != static_cast<size_t>(-1) &&
			MM_AwardPercent(players[leader].spawn_kills, players[leader].kills) >=
				MM_AWARD_SPAWN_SHARE_PCT) {
			Add(out, written, capacity, mm_match_award_t::spawn_fragger, leader,
				players[leader].spawn_kills);
		}
	}

	// Camping is a share of dwell time in one map cell -- but only for somebody
	// who was demonstrably playing. A player parked at the keyboard would score
	// 100% and has earned nothing, so idle samples and a shot count disqualify.
	{
		size_t leader = static_cast<size_t>(-1);
		uint32_t best = 0;
		bool tied = false;
		for (size_t i = 0; i < count; i++) {
			const mm_award_player_facts_t &p = players[i];
			if (p.camp_samples < MM_AWARD_CAMP_MIN_SAMPLES ||
				p.shots < MM_AWARD_CAMP_MIN_SHOTS) {
				continue;
			}
			if (MM_AwardPercent(p.camp_idle_samples, p.camp_samples) >
				MM_AWARD_CAMP_MAX_IDLE_PCT) {
				continue;
			}
			const uint32_t share =
				MM_AwardPercent(p.camp_best_cell_samples, p.camp_samples);
			if (share < MM_AWARD_CAMP_SHARE_PCT)
				continue;
			if (leader == static_cast<size_t>(-1) || share > best) {
				leader = i;
				best = share;
				tied = false;
			} else if (share == best) {
				tied = true;
			}
		}
		if (!tied) {
			Add(out, written, capacity, mm_match_award_t::dirty_rotten_camper,
				leader, best);
		}
	}

	{
		const size_t leader = StrictLeader(players, count, MM_AWARD_PICKUPS,
			[](const mm_award_player_facts_t &p) {
				return static_cast<uint64_t>(p.high_value_pickups);
			});
		Add(out, written, capacity, mm_match_award_t::kleptomaniac, leader,
			leader == static_cast<size_t>(-1)
				? 0 : players[leader].high_value_pickups);
	}

	// -- Capture the Flag ----------------------------------------------------
	if (match.ctf_mode) {
		const size_t runner = StrictLeader(players, count, MM_AWARD_CTF_CAPTURES,
			[](const mm_award_player_facts_t &p) {
				return static_cast<uint64_t>(p.ctf_captures);
			});
		Add(out, written, capacity, mm_match_award_t::flag_runner, runner,
			runner == static_cast<size_t>(-1) ? 0 : players[runner].ctf_captures);

		const size_t guard = StrictLeader(players, count, MM_AWARD_CTF_DEFENCES,
			[](const mm_award_player_facts_t &p) {
				return static_cast<uint64_t>(p.ctf_defences);
			});
		Add(out, written, capacity, mm_match_award_t::last_line, guard,
			guard == static_cast<size_t>(-1) ? 0 : players[guard].ctf_defences);

		const size_t returner = StrictLeader(players, count, MM_AWARD_CTF_RETURNS,
			[](const mm_award_player_facts_t &p) {
				return static_cast<uint64_t>(p.ctf_returns);
			});
		Add(out, written, capacity, mm_match_award_t::return_to_sender, returner,
			returner == static_cast<size_t>(-1) ? 0 : players[returner].ctf_returns);

		const size_t mule = StrictLeader(players, count, MM_AWARD_CTF_CARRIER_MSEC,
			[](const mm_award_player_facts_t &p) {
				return p.ctf_carrier_time_msec;
			});
		Add(out, written, capacity, mm_match_award_t::pack_mule, mule,
			mule == static_cast<size_t>(-1) ? 0 : static_cast<uint32_t>(
				players[mule].ctf_carrier_time_msec / 1000));
	}

	// -- Wooden spoons -------------------------------------------------------
	// Non-lethality has to be bad on both axes at once. Someone who trades badly
	// but hits hard is having a rough match; someone who neither kills nor hurts
	// anybody is the bag.
	{
		size_t leader = static_cast<size_t>(-1);
		uint32_t worst = 0;
		bool tied = false;
		for (size_t i = 0; i < count; i++) {
			const mm_award_player_facts_t &p = players[i];
			if (p.deaths < MM_AWARD_PUNCHING_BAG_DEATHS)
				continue;
			// DamageRatioX100 reads 0/0 as 0, which would sail through the
			// "dealt almost nothing" test rather than failing it. Somebody who
			// took no damage at all has no trade to be bad at.
			if (!p.damage_received)
				continue;
			const uint32_t kd = KdRatioX100(p);
			const uint32_t dmg = DamageRatioX100(p);
			if (kd > MM_AWARD_PUNCHING_BAG_KD_X100 ||
				dmg > MM_AWARD_PUNCHING_BAG_DMG_X100) {
				continue;
			}
			// Worst of the qualifiers wins: most damage soaked up.
			const uint32_t badness = static_cast<uint32_t>(
				std::min<uint64_t>(p.damage_received, UINT32_MAX));
			if (leader == static_cast<size_t>(-1) || badness > worst) {
				leader = i;
				worst = badness;
				tied = false;
			} else if (badness == worst) {
				tied = true;
			}
		}
		if (!tied) {
			Add(out, written, capacity, mm_match_award_t::punching_bag, leader,
				leader == static_cast<size_t>(-1) ? 0 : KdRatioX100(players[leader]));
		}
	}

	{
		size_t leader = static_cast<size_t>(-1);
		uint64_t most_shots = 0;
		bool tied = false;
		for (size_t i = 0; i < count; i++) {
			if (players[i].shots < MM_AWARD_STORMTROOPER_MIN_SHOTS)
				continue;
			if (MM_AwardPercent(players[i].hits, players[i].shots) >
				MM_AWARD_STORMTROOPER_PCT) {
				continue;
			}
			if (leader == static_cast<size_t>(-1) || players[i].shots > most_shots) {
				leader = i;
				most_shots = players[i].shots;
				tied = false;
			} else if (players[i].shots == most_shots) {
				tied = true;
			}
		}
		if (!tied) {
			Add(out, written, capacity, mm_match_award_t::stormtrooper, leader,
				leader == static_cast<size_t>(-1) ? 0 : MM_AwardPercent(
					players[leader].hits, players[leader].shots));
		}
	}

	{
		size_t leader = static_cast<size_t>(-1);
		uint32_t shortest = 0;
		bool tied = false;
		for (size_t i = 0; i < count; i++) {
			const mm_award_player_facts_t &p = players[i];
			if (p.completed_lives < MM_AWARD_MAYFLY_LIVES || !p.life_average_msec ||
				p.life_average_msec > MM_AWARD_MAYFLY_LIFE_MSEC) {
				continue;
			}
			if (leader == static_cast<size_t>(-1) || p.life_average_msec < shortest) {
				leader = i;
				shortest = p.life_average_msec;
				tied = false;
			} else if (p.life_average_msec == shortest) {
				tied = true;
			}
		}
		if (!tied)
			Add(out, written, capacity, mm_match_award_t::mayfly, leader, shortest / 1000);
	}

	{
		const size_t leader = StrictLeader(players, count,
			MM_AWARD_COCKROACH_LIFE_MSEC,
			[](const mm_award_player_facts_t &p) {
				if (p.completed_lives < MM_AWARD_COCKROACH_LIVES)
					return 0ull;
				return static_cast<uint64_t>(p.life_average_msec);
			});
		Add(out, written, capacity, mm_match_award_t::cockroach, leader,
			leader == static_cast<size_t>(-1)
				? 0 : players[leader].life_average_msec / 1000);
	}

	{
		const size_t leader = StrictLeader(players, count, MM_AWARD_LEMMING_DEATHS,
			[](const mm_award_player_facts_t &p) {
				return static_cast<uint64_t>(p.environment_deaths);
			});
		Add(out, written, capacity, mm_match_award_t::lemming, leader,
			leader == static_cast<size_t>(-1)
				? 0 : players[leader].environment_deaths);
	}
	{
		const size_t leader = StrictLeader(players, count, MM_AWARD_SUICIDES,
			[](const mm_award_player_facts_t &p) {
				return static_cast<uint64_t>(p.suicides);
			});
		Add(out, written, capacity, mm_match_award_t::butterfingers, leader,
			leader == static_cast<size_t>(-1) ? 0 : players[leader].suicides);
	}
	if (match.team_mode) {
		const size_t leader = StrictLeader(players, count, MM_AWARD_TEAM_KILLS,
			[](const mm_award_player_facts_t &p) {
				return static_cast<uint64_t>(p.team_kills);
			});
		Add(out, written, capacity, mm_match_award_t::benedict_arnold, leader,
			leader == static_cast<size_t>(-1) ? 0 : players[leader].team_kills);
	}

	return written;
}

// Ranks the candidate set and writes the final reel into `out`, honours first.
// The soft per-player limit is applied on a first pass so a dominant player does
// not monopolise a short reel; a second pass back-fills any unused slots from
// what that limit skipped, so no slot is wasted when nobody else qualified.
inline size_t MM_AwardsSelect(const mm_award_player_facts_t *players, size_t count,
	const mm_award_match_facts_t &match, mm_award_result_t *out, size_t capacity)
{
	using namespace mm_awards_detail;

	if (!out || !capacity)
		return 0;

	std::array<mm_award_candidate_t, MM_MATCH_AWARD_COUNT> candidates {};
	const size_t found = MM_AwardsCollect(players, count, match,
		candidates.data(), candidates.size());
	if (!found)
		return 0;

	std::stable_sort(candidates.begin(), candidates.begin() + found,
		[](const mm_award_candidate_t &lhs, const mm_award_candidate_t &rhs) {
			if (lhs.tier != rhs.tier)
				return lhs.tier < rhs.tier;
			return static_cast<uint8_t>(lhs.award) < static_cast<uint8_t>(rhs.award);
		});

	const size_t limit = std::min(capacity, MM_AWARDS_DISPLAY_LIMIT);
	std::array<bool, MM_MATCH_AWARD_COUNT> taken {};
	size_t written = 0;

	// The participant count is unbounded (departed players accumulate all match),
	// so the per-player tally is counted off the reel itself rather than kept in
	// an array indexed by player. At most MM_AWARDS_DISPLAY_LIMIT entries are ever
	// scanned, so this stays trivial however many people passed through.
	const auto already_held = [&](size_t owner) {
		size_t held = 0;
		for (size_t i = 0; i < written; i++)
			if (out[i].player_index == owner)
				held++;
		return held;
	};

	for (size_t pass = 0; pass < 2 && written < limit; pass++) {
		for (size_t i = 0; i < found && written < limit; i++) {
			if (taken[i])
				continue;
			const mm_award_candidate_t &candidate = candidates[i];
			if (pass == 0 &&
				already_held(candidate.player_index) >= MM_AWARDS_SOFT_PER_PLAYER_LIMIT) {
				continue;
			}
			taken[i] = true;
			out[written++] = { candidate.award, candidate.player_index, candidate.value };
		}
	}

	return written;
}
