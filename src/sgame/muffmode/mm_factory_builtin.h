// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

// [MuffMode] Built-in factories: one identity factory per selectable gametype.
//
// These exist so a fresh install with no factories.cfg still has a complete,
// selectable registry, and so `factory <gametype>` always names something.
// Each carries no overrides at all, which makes selecting one exactly
// equivalent to selecting the gametype -- and its title is therefore the plain
// gametype name, never a claim about settings it does not apply.
//
// There are deliberately NO mutator cross-products here (no "vampctf",
// "frenzyca", ...). A factory that carried only the title "Vampiric CTF" and an
// empty override set would rename the server for a mutator it never enables.
// Composed mutator names come from MM_ComposeGametypeName reading the live
// mutator cvars, which is the only source that cannot lie about them.
//
// A file definition with the same id replaces the built-in, so an operator can
// re-point `ctf` at their own settings without losing the name.

#include "muffmode/mm_gametype_table.h"

#include <array>
#include <cstddef>

namespace muffmode {
namespace factory {

struct mm_builtin_factory_t {
	const char	*id;
	const char	*title;
	const char	*desc;
	gametype_t	base;
};

inline constexpr std::array<mm_builtin_factory_t, 11> MM_BUILTIN_FACTORIES = { {
	{ "ffa", "Deathmatch", "Stock free-for-all.", GT_FFA },
	{ "duel", "Duel", "One on one, winner stays.", GT_DUEL },
	{ "tdm", "Team Deathmatch", "Two teams, frags score.", GT_TDM },
	{ "ctf", "Capture the Flag", "Two teams, flags score.", GT_CTF },
	{ "ca", "Clan Arena", "Round-based team elimination.", GT_CA },
	{ "ft", "Freeze Tag", "Freeze the other team, thaw your own.", GT_FREEZE },
	{ "strike", "Capture Strike", "Round-based attack and defend.", GT_STRIKE },
	{ "rr", "Red Rover", "Die and switch sides.", GT_RR },
	{ "lms", "Last Man Standing", "Fixed lives, last player wins.", GT_LMS },
	{ "horde", "Horde", "Co-operative waves of monsters.", GT_HORDE },
	{ "arena", "MuffMode Arena", "Independent RA2/RA3 rooms.", GT_ARENA },
} };

// Every built-in must name a distinct, selectable gametype, and its id must be
// the gametype's own short name -- `factory ctf` has to mean the CTF identity.
inline constexpr bool MM_BuiltinFactoriesAreIdentities() noexcept
{
	for (size_t i = 0; i < MM_BUILTIN_FACTORIES.size(); i++) {
		const mm_builtin_factory_t &entry = MM_BUILTIN_FACTORIES[i];
		if (!MM_GTEqualsI(entry.id, MM_GTDesc(entry.base).short_name))
			return false;
		if (MM_GTDesc(entry.base).availability != gametype_avail_t::Enabled)
			return false;
		for (size_t j = i + 1; j < MM_BUILTIN_FACTORIES.size(); j++)
			if (MM_BUILTIN_FACTORIES[j].base == entry.base)
				return false;
	}
	return true;
}

// ...and there must be one for every selectable gametype, so the registry can
// never be missing an entry for the mode the server is actually running.
inline constexpr bool MM_BuiltinFactoriesCoverEverySelectableGametype() noexcept
{
	for (const mm_gametype_desc_t &desc : MM_GT_TABLE) {
		if (desc.availability != gametype_avail_t::Enabled)
			continue;
		bool covered = false;
		for (const mm_builtin_factory_t &entry : MM_BUILTIN_FACTORIES)
			if (entry.base == desc.id)
				covered = true;
		if (!covered)
			return false;
	}
	return true;
}

static_assert(MM_BuiltinFactoriesAreIdentities());
static_assert(MM_BuiltinFactoriesCoverEverySelectableGametype());

} // namespace factory
} // namespace muffmode
