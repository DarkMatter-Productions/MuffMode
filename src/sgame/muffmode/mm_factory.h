// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

// [MuffMode] Factories: named gameplay presets layered over a base gametype.
//
// MuffMode already had a preset mechanism -- the gt-<UPPER>.cfg files -- but
// there is exactly one per gametype, it is an opaque list of console commands,
// and nothing tracks what it changed, so there is no way to run two flavours of
// CTF on one server or to undo a preset when leaving it.
//
// A factory is the missing layer, modelled on Quake Live's factories and fixed
// where that design leaks: overrides are typed data drawn from a closed
// allowlist rather than arbitrary cvar writes, every write is recorded in the
// override ledger, and switching factories restores exactly what the outgoing
// one changed.
//
// Layering, in the order each authority writes:
//
//     gt-<GT>.cfg  <  factory overrides  <  arena room  <  admin/vote
//
// The preset keeps ownership of server scope (hostname, map lists, slot
// limits); the factory wins the gameplay knobs it names, and only those.

#include "muffmode/mm_factory_rules.h"
#include "muffmode/mm_gametype_table.h"

#include <string>
#include <string_view>
#include <vector>

struct gentity_t;

namespace muffmode {
namespace factory {

struct mm_factory_t {
	std::string						id;
	std::string						title;
	std::string						desc;
	std::string						source;		// "built-in" or the file leaf
	gametype_t						base = GT_FFA;
	int								ruleset = 0;	// 0 leaves g_ruleset alone
	std::vector<parsed_override_t>	overrides;
	mm_factory_scope_t				scope;
	bool							builtin = false;
	bool							hidden = false;

	bool Sets(std::string_view cvar) const noexcept
	{
		return MM_FindOverride(overrides, cvar) != nullptr;
	}

	// True when this factory actually changes something. An identity factory
	// (the built-in for a gametype) changes nothing, so its title must not
	// displace the composed gametype name -- otherwise selecting `ffa` while
	// g_instagib is on would publish "Deathmatch" over "Instagib".
	bool Defines() const noexcept
	{
		return !overrides.empty() || ruleset != 0 || !scope.Empty();
	}
};

// Rebuilds the registry from the built-ins plus every leaf named in
// g_factory_file. Staged, and published only when every file parsed, so a bad
// edit leaves the previously working registry in place rather than silently
// dropping every operator-defined factory.
// Returns false when a named file could not be read or parsed. On failure the
// previously working registry is kept, so a caller reporting to a host must say
// "kept the old one" rather than "reloaded".
bool MM_Factory_LoadRegistry();

const mm_factory_t *MM_Factory_Find(std::string_view id);
const mm_factory_t *MM_Factory_Active();
bool               MM_Factory_IsVotable(std::string_view id);

// Space-separated ids, hidden ones omitted. Pass GT_NONE for every gametype.
std::string MM_Factory_List(gametype_t filter);

// Every registered definition, hidden and built-in included.
size_t MM_Factory_Count();

// Applies the selection named by g_factory over whatever the operator's own
// configs produced. Idempotent: it hands back the previous factory's overrides
// first.
//
// Called at boot from InitGame, and thereafter through `sv gt_factory`, which
// the gametype transaction queues ahead of the map command. It cannot simply
// wait for InitGame -- the engine calls the game's Init once when the game
// starts, not per level.
void MM_Factory_ApplySelection();

// Re-applies only the immediate-effect overrides of the active factory, for
// `factory reload` and `sv gt_apply`. Reports what had to be deferred.
void MM_Factory_ReapplyImmediate();

// The base gametype of a factory id, or -1 when unknown.
int MM_Factory_BaseGametype(std::string_view id);

} // namespace factory
} // namespace muffmode

// Player/admin command body. The vote validator and pass handler live with the
// other vote entry points in mm_vote.h, since the vote argument accessors are
// private to that translation unit.
void MM_CmdFactory(gentity_t *ent);

// `sv factory <id|none>`: the same selection path for a dedicated console.
void MM_ServerSelectFactory(const char *id);
