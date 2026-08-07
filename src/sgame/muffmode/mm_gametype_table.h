// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

// [MuffMode] The single source of truth for gametype identity.
//
// Gametype facts used to live in six index-parallel structures that nothing
// forced to agree: gt_short_name / gt_short_name_upper / gt_long_name and
// _gt[] in g_local.h + core/runtime.cpp, k_gametype_availability in
// mm_gametype.cpp, and k_gametype_spawn_tokens in mm_spawn_filter.cpp. Adding a
// gametype meant editing all six and hoping. _gt[] was even declared
// `extern int _gt[GT_NUM_GAMETYPES]` against a definition sized only by its
// initialiser, so a short table would have been a silent out-of-bounds read.
//
// One descriptor table now carries every fact, its self-consistency is proven
// by static_assert, and the legacy arrays are generated projections of it so
// existing subscript call sites keep working verbatim.
//
// This header is deliberately free of game headers so the whole model is
// host-testable.

#include "muffmode/mm_parse.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

enum gametype_t : int {
	GT_NONE,
	GT_FFA,
	GT_DUEL,
	GT_TDM,
	GT_CTF,
	GT_CA,
	GT_FREEZE,
	GT_STRIKE,
	GT_RR,
	GT_LMS,
	GT_HORDE,
	GT_BALL,
	GT_INSTAGIB,
	GT_NADEFEST,
	GT_ARENA,
	GT_NUM_GAMETYPES
};
inline constexpr gametype_t GT_FIRST = GT_FFA;
inline constexpr gametype_t GT_LAST = GT_ARENA;

enum gtf_t {
	// Bit values 0x01-0x40 are frozen: MM_CurrentGametypeFlags() is serialised
	// as the "flags" field of the statusbar dump, so renumbering them would
	// silently change an operator-facing diagnostic. New bits append.
	GTF_TEAMS		= 0x0001,
	GTF_CTF			= 0x0002,
	GTF_ARENA		= 0x0004,
	GTF_ROUNDS		= 0x0008,
	GTF_ELIMINATION	= 0x0010,
	GTF_FRAGS		= 0x0020,
	// Independent RA2/RA3 arenas. Unlike GTF_ROUNDS and GTF_ELIMINATION, this
	// never opts into the singleton match state.
	GTF_MULTI_ARENA	= 0x0040,
	// [MuffMode] Capability bits added with the gametype table. Purely
	// additive, and each one replaces a gametype list that was previously
	// written out by hand in more than one place -- which is the only reason to
	// add one. A flag with no consumer is table decoration, not a capability.
	GTF_LIVES	= 0x0080,	// elimination by life counter, not round death
	GTF_TECHS	= 0x0100	// techs are meaningful here
};

enum class gametype_avail_t : uint8_t {
	Enabled,
	Disabled,
	Removed,
};

// Which limit gates the match. Typed rather than a cvar name so the lookup
// stays a switch on a per-client-per-frame path, and so the noun shown to
// players cannot drift from the limit actually being read.
enum class mm_gt_score_t : uint8_t {
	None,		// the mode owns its own limit
	Frags,
	Captures,
	Rounds
};

inline constexpr const char *MM_GTScoreWord(mm_gt_score_t model) noexcept
{
	switch (model) {
	case mm_gt_score_t::Captures:	return "capture";
	case mm_gt_score_t::Rounds:		return "round";
	case mm_gt_score_t::Frags:
	case mm_gt_score_t::None:
		break;
	}
	return "frag";
}

struct mm_gametype_desc_t {
	gametype_t			id;					// must equal its own table index
	const char			*short_name;		// vote/admin token, g_votable_gametypes
	const char			*upper_name;		// gt-<UPPER>.cfg, persisted stats key
	const char			*long_name;			// level.gametype_name base
	const char			*mutator_label;		// label used when a mutator prefixes this mode
	const char			*spawn_token;		// mm_spawn_filter gametype filter token
	int					flags;				// gtf_t mask
	mm_gt_score_t		score_model;
	bool				legacy_teamplay;	// the `teamplay` cvar value this implies
	bool				legacy_ctf;			// the `ctf` cvar value this implies
	gametype_avail_t	availability;
};

inline constexpr std::array<mm_gametype_desc_t, GT_NUM_GAMETYPES> MM_GT_TABLE = { {
	{ GT_NONE, "cmp", "CMP", "Campaign", "", "campaign",
		0,
		mm_gt_score_t::None, false, false, gametype_avail_t::Removed },
	{ GT_FFA, "ffa", "FFA", "Deathmatch", "Deathmatch", "ffa",
		GTF_FRAGS | GTF_TECHS,
		mm_gt_score_t::Frags, false, false, gametype_avail_t::Enabled },
	{ GT_DUEL, "duel", "DUEL", "Duel", "Duel", "tournament",
		GTF_FRAGS,
		mm_gt_score_t::Frags, false, false, gametype_avail_t::Enabled },
	{ GT_TDM, "tdm", "TDM", "Team Deathmatch", "TDM", "team",
		GTF_TEAMS | GTF_FRAGS | GTF_TECHS,
		mm_gt_score_t::Frags, true, false, gametype_avail_t::Enabled },
	{ GT_CTF, "ctf", "CTF", "Capture the Flag", "CTF", "ctf",
		GTF_TEAMS | GTF_CTF | GTF_TECHS,
		mm_gt_score_t::Captures, false, true, gametype_avail_t::Enabled },
	{ GT_CA, "ca", "CA", "Clan Arena", "CA", "ca",
		GTF_TEAMS | GTF_ARENA | GTF_ROUNDS | GTF_ELIMINATION,
		mm_gt_score_t::Rounds, false, false, gametype_avail_t::Enabled },
	{ GT_FREEZE, "ft", "FT", "Freeze Tag", "Freeze Tag", "freeze",
		GTF_TEAMS | GTF_ROUNDS,
		mm_gt_score_t::Rounds, true, false, gametype_avail_t::Enabled },
	{ GT_STRIKE, "strike", "STRIKE", "Capture Strike", "Strike", "strike",
		GTF_TEAMS | GTF_ARENA | GTF_ROUNDS | GTF_CTF | GTF_ELIMINATION,
		mm_gt_score_t::Captures, false, false, gametype_avail_t::Enabled },
	{ GT_RR, "rr", "REDROVER", "Red Rover", "RR", "rr",
		GTF_TEAMS | GTF_ARENA | GTF_ROUNDS | GTF_FRAGS,
		mm_gt_score_t::Rounds, false, false, gametype_avail_t::Enabled },
	// GTF_LIVES: both of these eliminate on a life counter rather than on a
	// round death, which is the pairing four separate call sites spelled out as
	// "GT(GT_HORDE) || GT(GT_LMS)".
	{ GT_LMS, "lms", "LMS", "Last Man Standing", "LMS", "lms",
		GTF_ARENA | GTF_ROUNDS | GTF_ELIMINATION | GTF_LIVES,
		mm_gt_score_t::Rounds, false, false, gametype_avail_t::Enabled },
	{ GT_HORDE, "horde", "HORDE", "Horde", "Horde", "horde",
		GTF_ROUNDS | GTF_LIVES | GTF_TECHS,
		mm_gt_score_t::Rounds, false, false, gametype_avail_t::Enabled },
	{ GT_BALL, "ball", "BALL", "ProBall", "", "ball",
		0,
		mm_gt_score_t::None, false, false, gametype_avail_t::Removed },
	// Instagib and NadeFest are modifiers, not gametypes: g_instagib and
	// g_nadefest apply on top of whichever mode is actually running, which is
	// how every gameplay site had always tested them. Their enumerators stay so
	// GT_ARENA keeps id 14 and no operator config renumbers, but they are no
	// longer selectable -- a factory expresses them instead.
	{ GT_INSTAGIB, "instagib", "INSTAGIB", "Instagib", "", "instagib",
		0,
		mm_gt_score_t::None, false, false, gametype_avail_t::Removed },
	{ GT_NADEFEST, "nadefest", "NADEFEST", "NadeFest", "", "nadefest",
		0,
		mm_gt_score_t::None, false, false, gametype_avail_t::Removed },
	// Each room owns its own series limit and its own match state.
	{ GT_ARENA, "arena", "ARENA", "MuffMode Arena", "Arena", "arena",
		GTF_ARENA | GTF_MULTI_ARENA,
		mm_gt_score_t::None, false, false, gametype_avail_t::Enabled },
} };

// ---------------------------------------------------------------------------
// Compile-time consistency. Every one of these is a rule the old six parallel
// tables could violate silently.
// ---------------------------------------------------------------------------

// ASCII case-insensitive compare, shared with the rest of the mod rather than
// forked here: mm_parse.h is game-header-free, so including it keeps this
// header host-testable without a second implementation to drift.
inline constexpr bool MM_GTEqualsI(std::string_view a, std::string_view b) noexcept
{
	return MM_EqualsAsciiI(a, b);
}

namespace muffmode {
namespace gametype_detail {

// The name accessors are const char * so the projections stay drop-in
// compatible with the arrays they replace; treat null as empty everywhere.
inline constexpr std::string_view View(const char *text) noexcept
{
	return text ? std::string_view(text) : std::string_view();
}

using name_getter_t = const char *(*)(const mm_gametype_desc_t &);

inline constexpr const char *GetShort(const mm_gametype_desc_t &d) noexcept { return d.short_name; }
inline constexpr const char *GetUpper(const mm_gametype_desc_t &d) noexcept { return d.upper_name; }
inline constexpr const char *GetLong(const mm_gametype_desc_t &d) noexcept { return d.long_name; }
inline constexpr const char *GetSpawn(const mm_gametype_desc_t &d) noexcept { return d.spawn_token; }

inline constexpr bool NamesUnique(name_getter_t get) noexcept
{
	for (size_t i = 0; i < MM_GT_TABLE.size(); i++) {
		const std::string_view a = View(get(MM_GT_TABLE[i]));
		if (a.empty())
			return false;
		for (size_t j = i + 1; j < MM_GT_TABLE.size(); j++)
			if (MM_GTEqualsI(a, View(get(MM_GT_TABLE[j]))))
				return false;
	}
	return true;
}

} // namespace gametype_detail
} // namespace muffmode

inline constexpr bool MM_GTIdsMatchIndices() noexcept
{
	for (size_t i = 0; i < MM_GT_TABLE.size(); i++)
		if (static_cast<size_t>(MM_GT_TABLE[i].id) != i)
			return false;
	return true;
}

inline constexpr bool MM_GTShortNamesUnique() noexcept
{
	return muffmode::gametype_detail::NamesUnique(muffmode::gametype_detail::GetShort);
}

inline constexpr bool MM_GTUpperNamesUnique() noexcept
{
	return muffmode::gametype_detail::NamesUnique(muffmode::gametype_detail::GetUpper);
}

inline constexpr bool MM_GTLongNamesUnique() noexcept
{
	return muffmode::gametype_detail::NamesUnique(muffmode::gametype_detail::GetLong);
}

inline constexpr bool MM_GTSpawnTokensUnique() noexcept
{
	return muffmode::gametype_detail::NamesUnique(muffmode::gametype_detail::GetSpawn);
}

// The upper name is interpolated straight into `exec gt-<UPPER>.cfg` and into a
// filesystem path, so it must never carry a separator, a quote or a space.
inline constexpr bool MM_GTUpperNamesCfgSafe() noexcept
{
	for (const mm_gametype_desc_t &d : MM_GT_TABLE) {
		const std::string_view upper = muffmode::gametype_detail::View(d.upper_name);
		if (upper.empty() || upper.size() > 16)
			return false;
		for (const char c : upper) {
			const bool ok = (c >= 'A' && c <= 'Z') ||
				(c >= '0' && c <= '9') || c == '_';
			if (!ok)
				return false;
		}
	}
	return true;
}

inline constexpr bool MM_GTFlagsCoherent() noexcept
{
	for (const mm_gametype_desc_t &d : MM_GT_TABLE) {
		const bool removed = d.availability == gametype_avail_t::Removed;
		if (removed) {
			if (d.flags != 0 || d.score_model != mm_gt_score_t::None)
				return false;
			continue;
		}
		if ((d.flags & GTF_CTF) && !(d.flags & GTF_TEAMS))
			return false;
		if ((d.flags & GTF_ELIMINATION) && !(d.flags & GTF_ROUNDS))
			return false;
		if ((d.flags & GTF_MULTI_ARENA) && !(d.flags & GTF_ARENA))
			return false;
		if (muffmode::gametype_detail::View(d.mutator_label).empty())
			return false;
	}
	return true;
}

inline constexpr bool MM_GTScoreModelsCoherent() noexcept
{
	for (const mm_gametype_desc_t &d : MM_GT_TABLE) {
		if (d.score_model == mm_gt_score_t::Captures && !(d.flags & GTF_CTF))
			return false;
		if (d.score_model == mm_gt_score_t::Rounds && !(d.flags & GTF_ROUNDS))
			return false;
		// Only a multi-arena world owns its own limit; every other selectable
		// mode has to be gated by something.
		if (d.score_model == mm_gt_score_t::None &&
			d.availability == gametype_avail_t::Enabled &&
			!(d.flags & GTF_MULTI_ARENA))
			return false;
	}
	return true;
}

inline constexpr bool MM_GTLegacyAliasesCoherent() noexcept
{
	for (const mm_gametype_desc_t &d : MM_GT_TABLE) {
		// `teamplay` and `ctf` are mutually exclusive engine aliases.
		if (d.legacy_teamplay && d.legacy_ctf)
			return false;
		if (d.legacy_ctf && !(d.flags & GTF_CTF))
			return false;
	}
	return true;
}

// Licenses GT(GT_ARENA) <=> GTF(GTF_MULTI_ARENA).
inline constexpr bool MM_GTMultiArenaIsUnique() noexcept
{
	size_t count = 0;
	for (const mm_gametype_desc_t &d : MM_GT_TABLE)
		if (d.flags & GTF_MULTI_ARENA)
			count++;
	return count == 1 && (MM_GT_TABLE[GT_ARENA].flags & GTF_MULTI_ARENA) != 0;
}

static_assert(MM_GT_TABLE.size() == GT_NUM_GAMETYPES);
static_assert(MM_GTIdsMatchIndices());
static_assert(MM_GTShortNamesUnique());
static_assert(MM_GTUpperNamesUnique());
static_assert(MM_GTLongNamesUnique());
static_assert(MM_GTSpawnTokensUnique());
static_assert(MM_GTUpperNamesCfgSafe());
static_assert(MM_GTFlagsCoherent());
static_assert(MM_GTScoreModelsCoherent());
static_assert(MM_GTLegacyAliasesCoherent());
static_assert(MM_GTMultiArenaIsUnique());
// Red Rover's upper name is a persisted stats and profile key; pin the one
// name that does not follow mechanically from the short name.
static_assert(MM_GTEqualsI(MM_GT_TABLE[GT_RR].upper_name, "REDROVER"));

// ---------------------------------------------------------------------------
// Accessors.
// ---------------------------------------------------------------------------

inline constexpr bool MM_IsGametypeIndex(int index) noexcept
{
	return index >= 0 && index < GT_NUM_GAMETYPES;
}

// Total: an out-of-range index reads as Deathmatch rather than reading off the
// end of the table.
inline constexpr const mm_gametype_desc_t &MM_GTDesc(gametype_t gt) noexcept
{
	return MM_GT_TABLE[MM_IsGametypeIndex(static_cast<int>(gt))
		? static_cast<size_t>(gt) : static_cast<size_t>(GT_FFA)];
}

inline constexpr int MM_GTFlags(gametype_t gt) noexcept
{
	return MM_GTDesc(gt).flags;
}

struct mm_gt_lookup_t {
	bool		found = false;
	gametype_t	gt = GT_NONE;
};

inline constexpr mm_gt_lookup_t MM_GTFindByShortName(std::string_view token) noexcept
{
	if (token.empty())
		return {};
	for (const mm_gametype_desc_t &d : MM_GT_TABLE)
		if (MM_GTEqualsI(token, muffmode::gametype_detail::View(d.short_name)))
			return { true, d.id };
	return {};
}

// Accepts either spelling, matching the historical GT_IndexFromString contract.
inline constexpr mm_gt_lookup_t MM_GTFindByAnyName(std::string_view token) noexcept
{
	const mm_gt_lookup_t by_short = MM_GTFindByShortName(token);
	if (by_short.found)
		return by_short;
	if (token.empty())
		return {};
	for (const mm_gametype_desc_t &d : MM_GT_TABLE)
		if (MM_GTEqualsI(token, muffmode::gametype_detail::View(d.long_name)))
			return { true, d.id };
	return {};
}

// ---------------------------------------------------------------------------
// Legacy projections. Subscript syntax is identical to the raw arrays these
// replace, so no call site changes. New code should call MM_GTDesc().
// ---------------------------------------------------------------------------

namespace muffmode {
namespace gametype_detail {

template <typename Getter>
inline constexpr std::array<const char *, GT_NUM_GAMETYPES> ProjectNames(Getter get) noexcept
{
	std::array<const char *, GT_NUM_GAMETYPES> out = {};
	for (size_t i = 0; i < out.size(); i++)
		out[i] = get(MM_GT_TABLE[i]);
	return out;
}

inline constexpr std::array<int, GT_NUM_GAMETYPES> ProjectFlags() noexcept
{
	std::array<int, GT_NUM_GAMETYPES> out = {};
	for (size_t i = 0; i < out.size(); i++)
		out[i] = MM_GT_TABLE[i].flags;
	return out;
}

} // namespace gametype_detail
} // namespace muffmode

inline constexpr auto gt_short_name =
	muffmode::gametype_detail::ProjectNames(muffmode::gametype_detail::GetShort);
inline constexpr auto gt_short_name_upper =
	muffmode::gametype_detail::ProjectNames(muffmode::gametype_detail::GetUpper);
inline constexpr auto gt_long_name =
	muffmode::gametype_detail::ProjectNames(muffmode::gametype_detail::GetLong);
inline constexpr auto _gt = muffmode::gametype_detail::ProjectFlags();

// ---------------------------------------------------------------------------
// Legacy alias projection, both directions.
//
// `teamplay` and `ctf` predate g_gametype and remain live serverinfo cvars, so
// the mapping has to work both ways. Deriving both directions from one table
// entry is what makes them consistent.
// ---------------------------------------------------------------------------

struct mm_gt_alias_projection_t {
	bool ctf = false;
	bool teamplay = false;
};

inline constexpr mm_gt_alias_projection_t MM_GTAliasesFor(gametype_t gt) noexcept
{
	const mm_gametype_desc_t &d = MM_GTDesc(gt);
	return { d.legacy_ctf, d.legacy_teamplay };
}

// The inverse. Returns `current` when the observed alias state already agrees
// with it, so clearing `ctf` on a Clan Arena or Horde server no longer promotes
// the whole server to Team Deathmatch.
inline constexpr gametype_t MM_GTGametypeFromAliases(
	gametype_t current, bool ctf_on, bool teamplay_on) noexcept
{
	const mm_gt_alias_projection_t projected = MM_GTAliasesFor(current);
	if (projected.ctf == ctf_on && projected.teamplay == teamplay_on)
		return current;
	if (ctf_on)
		return GT_CTF;
	if (teamplay_on)
		return GT_TDM;
	return GT_FFA;
}

// ---------------------------------------------------------------------------
// Published gametype name.
//
// This replaces a 150-line if/else ladder that spelled out all 45 gametype x
// mutator combinations by hand, dropped the base gametype entirely on the
// NadeFest path, and disagreed with gt_long_name on the spelling of Instagib.
// ---------------------------------------------------------------------------

enum class mm_mutator_t : uint8_t {
	None,
	Instagib,
	Vampiric,
	Frenzy,
	NadeFest,
	QuadHog
};

struct mm_mutator_desc_t {
	const char *prefix;		// used as "<prefix><mode label>", e.g. "Insta-CTF"
	// Name used on plain Deathmatch, where "<prefix>Deathmatch" would read
	// worse than the modifier's own name. Empty means always use the prefix.
	const char *solo_name;
};

inline constexpr std::array<mm_mutator_desc_t, 6> MM_MUTATOR_TABLE = { {
	{ "", "" },
	{ "Insta-", "Instagib" },
	{ "Vampiric ", "" },
	{ "Frenzy ", "" },
	{ "NadeFest ", "NadeFest" },
	{ "Quad Hog ", "" },
} };

inline constexpr size_t MM_GAMETYPE_NAME_MAX = 64;

struct mm_gametype_name_t {
	char	text[MM_GAMETYPE_NAME_MAX] = {};
	size_t	length = 0;

	constexpr std::string_view view() const noexcept
	{
		return std::string_view(text, length);
	}
};

struct mm_gametype_name_input_t {
	int			 effective_gametype = static_cast<int>(GT_FFA);
	bool		 deathmatch = true;
	bool		 coop = false;
	bool		 arena_selected = false;	// the cvar asks for Arena
	bool		 arena_active = false;		// ...and the map contract passed
	mm_mutator_t mutator = mm_mutator_t::None;
	const char	*factory_title = nullptr;	// wins outright when non-empty
};

namespace muffmode {
namespace gametype_detail {

inline constexpr void AppendName(mm_gametype_name_t &out, const char *text) noexcept
{
	if (!text)
		return;
	for (size_t i = 0; text[i] != '\0'; i++) {
		if (out.length + 1 >= MM_GAMETYPE_NAME_MAX)
			return;
		out.text[out.length++] = text[i];
	}
}

} // namespace gametype_detail
} // namespace muffmode

// Always NUL-terminated, always truncated inside the buffer. Composition order
// is fixed and host-tested.
inline constexpr mm_gametype_name_t MM_ComposeGametypeName(
	const mm_gametype_name_input_t &in) noexcept
{
	using muffmode::gametype_detail::AppendName;

	mm_gametype_name_t out;

	if (!in.deathmatch) {
		AppendName(out, in.coop ? "Co-op" : "Single Player");
		return out;
	}

	// Arena's fail-closed state outranks everything, including a factory title.
	// "(inactive)" is the operator's only signal that the selection survived but
	// the map failed the RA2 contract and normal Deathmatch rules are actually
	// in force; a title that hid it would be worse than no title at all.
	// Individual arenas own their mutators, so a global mutator cvar must not
	// rename a whole multi-arena session either.
	if (in.arena_selected) {
		if (in.arena_active && in.factory_title && in.factory_title[0] != '\0')
			AppendName(out, in.factory_title);
		else
			AppendName(out, in.arena_active
				? "MuffMode Arena" : "MuffMode Arena (inactive)");
		return out;
	}

	if (in.factory_title && in.factory_title[0] != '\0') {
		AppendName(out, in.factory_title);
		return out;
	}

	const gametype_t gt = MM_IsGametypeIndex(in.effective_gametype)
		? static_cast<gametype_t>(in.effective_gametype) : GT_FFA;
	const mm_gametype_desc_t &desc = MM_GTDesc(gt);
	const mm_mutator_desc_t &mutator =
		MM_MUTATOR_TABLE[static_cast<size_t>(in.mutator)];

	if (in.mutator == mm_mutator_t::None) {
		AppendName(out, desc.long_name);
		return out;
	}

	// On plain Deathmatch a modifier that has a name of its own uses it --
	// "Instagib", not "Insta-Deathmatch". Everywhere else the name keeps the
	// mode being modified: "Insta-CTF", "NadeFest Freeze Tag".
	if (gt == GT_FFA && mutator.solo_name[0] != '\0') {
		AppendName(out, mutator.solo_name);
		return out;
	}

	AppendName(out, mutator.prefix);
	AppendName(out, desc.mutator_label);
	return out;
}
