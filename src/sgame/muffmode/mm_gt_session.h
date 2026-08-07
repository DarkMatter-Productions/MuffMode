// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

// [MuffMode] The gametype session: one owner for gametype and factory changes.
//
// Before this, the gametype had no owner at all. MM_ChangeGametype mutated
// cvars inline from ClientCommand; MM_GTChanges mutated a different set from
// inside G_RunFrame by polling modified counts; the per-gametype config files
// mutated a third set asynchronously through the command buffer; and two of
// those three could each queue their own map reload in the same frame. The
// failure modes were what you would expect from three uncoordinated writers:
// double map
// loads, a mid-frame team wipe that the same frame's warmup tick then fought,
// clearing `ctf` promoting a Clan Arena server to Team Deathmatch, and a
// half-applied change left behind when a late validation failed.
//
// Every mutation now goes through one transaction:
//
//     Resolve  -- what selection is being asked for
//     Validate -- is it legal, in full, before anything is written
//     Plan     -- exactly which deltas that implies
//     Commit   -- apply them in one fixed order, at a frame boundary
//     Notify   -- publish, broadcast, refresh
//
// Resolve, Validate and Plan are pure and host-tested. Commit is the only code
// in the tree permitted to force-set g_gametype, ctf, teamplay, deathmatch or
// the mutator cvars, and the only code that issues a map command for a gametype
// or factory change.

#include "muffmode/mm_gametype_state.h"
#include "muffmode/mm_gametype_table.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct gentity_t;

namespace muffmode {
namespace gametype {

inline constexpr size_t MM_GT_FACTORY_ID_SIZE = 32;

enum class gt_source_t : uint8_t {
	// Boot is the server coming up. It never reloads and never touches clients,
	// which MM_GT_Plan enforces; MM_GT_SessionInit is the path that uses it.
	Boot,
	Admin,			// the in-game `gametype` and `factory` commands
	// The dedicated server console and rcon. Separate from Admin so a host who
	// pins the selection with g_gametype_locked cannot lock themselves out of
	// their own server.
	ServerConsole,
	Vote,			// a passed vote
	ExternalCvar	// synthesised by the reconciler from an outside cvar write
};

enum class gt_reject_t : uint8_t {
	None,
	NoChange,
	UnknownGametype,
	GametypeRemoved,
	GametypeDisabled,
	NotVotable,
	UnknownFactory,
	FactoryBaseMismatch,
	UnsafeReloadTarget,
	ReloadTargetTooLong,
	LedgerExhausted,
	TransactionBusy,
	NotDeathmatch,
	SelectionLocked
};

const char *MM_GT_RejectText(gt_reject_t reject) noexcept;

struct mm_gt_request_t {
	gt_source_t		source = gt_source_t::Boot;
	int				gametype = -1;			// -1 keeps the committed gametype
	bool			has_factory = false;
	char			factory_id[MM_GT_FACTORY_ID_SIZE] = {};
	bool			clear_factory = false;
	bool			force_reapply = false;	// admin "apply this selection again"
	bool			allow_reload = true;
};

struct mm_gt_selection_t {
	int		configured = (int)GT_FFA;
	int		effective = (int)GT_FFA;
	bool	has_factory = false;
	char	factory_id[MM_GT_FACTORY_ID_SIZE] = {};
};

struct mm_gt_plan_t {
	bool						valid = false;
	gt_reject_t					reject = gt_reject_t::None;
	mm_gt_selection_t			next;
	bool						gametype_changed = false;
	bool						factory_changed = false;
	bool						requires_reload = false;
	bool						reapply_factory = false;
	mm_gt_alias_projection_t	aliases;
	int							arena_dmg_armor = -1;	// -1 leave, 0 clear
	bool						reset_client_sessions = false;
};

namespace session_detail {

inline constexpr void CopyId(char (&dest)[MM_GT_FACTORY_ID_SIZE],
	const char *source) noexcept
{
	size_t i = 0;
	if (source) {
		for (; source[i] != '\0' && i + 1 < MM_GT_FACTORY_ID_SIZE; i++)
			dest[i] = source[i];
	}
	for (size_t pad = i; pad < MM_GT_FACTORY_ID_SIZE; pad++)
		dest[pad] = '\0';
}

inline constexpr bool IdsEqual(const char *a, const char *b) noexcept
{
	for (size_t i = 0; i < MM_GT_FACTORY_ID_SIZE; i++) {
		const char ca = a ? a[i] : '\0';
		const char cb = b ? b[i] : '\0';
		if (ca != cb)
			return false;
		if (ca == '\0')
			return true;
	}
	return true;
}

} // namespace session_detail

// What selection is being asked for. `factory_basegt` is the base gametype of
// the factory the request names (or retains), or negative when there is none.
inline constexpr mm_gt_selection_t MM_GT_Resolve(
	const mm_gt_request_t &request, const mm_gt_selection_t &current,
	int factory_basegt, bool arena_active) noexcept
{
	mm_gt_selection_t next;

	if (request.clear_factory) {
		next.has_factory = false;
	} else if (request.has_factory) {
		next.has_factory = true;
		session_detail::CopyId(next.factory_id, request.factory_id);
	} else {
		next.has_factory = current.has_factory;
		session_detail::CopyId(next.factory_id, current.factory_id);
	}

	// An explicit gametype wins. Otherwise a newly named factory supplies its
	// base, and failing that the committed gametype stands.
	int requested = request.gametype;
	if (requested < 0) {
		requested = (request.has_factory && factory_basegt >= 0)
			? factory_basegt : current.configured;
	}

	const mm_gametype_resolution_t resolved =
		MM_ResolveGametypeState(requested, arena_active);
	next.configured = resolved.configured;
	next.effective = resolved.effective;

	// A factory belongs to one base gametype. Changing the gametype out from
	// under a retained factory drops it rather than leaving a preset applied to
	// a mode it was never written for. A request that names both and disagrees
	// is rejected in Validate instead.
	if (next.has_factory && !request.has_factory && factory_basegt >= 0 &&
		factory_basegt != next.configured) {
		next.has_factory = false;
		session_detail::CopyId(next.factory_id, "");
	}

	return next;
}

struct mm_gt_validate_input_t {
	gt_source_t			source = gt_source_t::Boot;
	bool				requested_in_range = true;
	bool				requested_known = true;		// the token parsed at all
	gametype_avail_t	availability = gametype_avail_t::Enabled;
	bool				votable = true;
	bool				factory_required = false;	// the request named a factory
	bool				factory_known = false;
	int					factory_basegt = -1;
	bool				reload_target_safe = true;
	bool				reload_target_fits = true;
	bool				ledger_has_room = true;
	bool				deathmatch = true;
	bool				changes_anything = true;
	bool				selection_locked = false;	// g_gametype_locked
};

// Everything that can make a transaction illegal, checked before a single cvar
// is written. The old code force-set g_gametype and froze every client *before*
// discovering that the map name it was about to reload was unsafe, and then
// returned -- leaving the server with no reload and every player invisible.
inline constexpr gt_reject_t MM_GT_Validate(
	const mm_gt_selection_t &next, const mm_gt_validate_input_t &in) noexcept
{
	if (!in.requested_known)
		return gt_reject_t::UnknownGametype;
	if (!in.requested_in_range)
		return gt_reject_t::UnknownGametype;
	if (in.availability == gametype_avail_t::Removed)
		return gt_reject_t::GametypeRemoved;
	if (in.availability == gametype_avail_t::Disabled)
		return gt_reject_t::GametypeDisabled;
	if (in.source == gt_source_t::Vote && !in.votable)
		return gt_reject_t::NotVotable;
	if (in.factory_required && !in.factory_known)
		return gt_reject_t::UnknownFactory;
	if (in.factory_required && in.factory_known && in.factory_basegt >= 0 &&
		in.factory_basegt != next.configured)
		return gt_reject_t::FactoryBaseMismatch;
	if (!in.deathmatch && in.source != gt_source_t::Boot)
		return gt_reject_t::NotDeathmatch;
	// Ordered after availability so a locked server still reports a bad token
	// as unknown rather than blaming the lock. Boot has to get through -- it is
	// how the configured selection is established -- and so does the console,
	// which submits as ServerConsole precisely so a host is never locked out of
	// their own server.
	if (in.selection_locked && (in.source == gt_source_t::Admin ||
		in.source == gt_source_t::Vote))
		return gt_reject_t::SelectionLocked;
	if (!in.ledger_has_room)
		return gt_reject_t::LedgerExhausted;
	// Only relevant when the transaction will reload; the caller passes true
	// when no reload is planned.
	if (!in.reload_target_safe)
		return gt_reject_t::UnsafeReloadTarget;
	if (!in.reload_target_fits)
		return gt_reject_t::ReloadTargetTooLong;
	if (!in.changes_anything)
		return gt_reject_t::NoChange;

	return gt_reject_t::None;
}

struct mm_gt_plan_input_t {
	bool clients_allocated = false;		// game.clients has been allocated
	bool teams_model_flipped = false;	// Teams() differs across the change
};

inline constexpr mm_gt_plan_t MM_GT_Plan(
	const mm_gt_request_t &request, const mm_gt_selection_t &current,
	const mm_gt_selection_t &next, gt_reject_t validation,
	const mm_gt_plan_input_t &in) noexcept
{
	mm_gt_plan_t plan;
	plan.reject = validation;
	plan.next = next;

	if (validation != gt_reject_t::None)
		return plan;

	plan.valid = true;
	plan.gametype_changed = current.configured != next.configured;
	plan.factory_changed = current.has_factory != next.has_factory ||
		(next.has_factory &&
			!session_detail::IdsEqual(current.factory_id, next.factory_id));

	// Re-apply the selection's own settings when anything moved, or when an
	// admin asked for it explicitly. Only meaningful with a factory selected --
	// without one there is nothing to apply.
	plan.reapply_factory = request.source != gt_source_t::Boot &&
		next.has_factory &&
		(plan.gametype_changed || plan.factory_changed || request.force_reapply);

	// A re-apply restores the ledger and writes the overrides again, which has
	// to happen at a map boundary for the map-load and latched settings to take
	// effect, so it implies the reload.
	plan.requires_reload = request.allow_reload &&
		(plan.gametype_changed || plan.factory_changed || plan.reapply_factory);

	// Emitted unconditionally, not only on a change. Writing the aliases while
	// latching their modified counts only inside the changed branch is how an
	// admin re-applying the current gametype used to bump ctf->modified_count
	// and have the next poller tick read ctf-off as "become Team Deathmatch".
	plan.aliases = MM_GTAliasesFor((gametype_t)next.configured);

	// Instagib and NadeFest used to be gametypes as well as modifiers, and this
	// is where the two representations were forced back into agreement. They
	// are modifiers only now, owned by whatever factory or config sets them, so
	// there is nothing left to reconcile.

	// MuffMode Arena's RA3-compatible preset enables self-armor damage. Other
	// arena modes default to protected armor, so do not leak the Arena preset
	// into CA/Strike/Red Rover when leaving it.
	if (current.configured == (int)GT_ARENA &&
		next.configured != (int)GT_ARENA && plan.gametype_changed)
		plan.arena_dmg_armor = 0;

	// client->sess survives the map change, so this has to run even when a
	// reload follows -- otherwise every player arrives in the new gametype
	// still holding the team they had in the old one.
	plan.reset_client_sessions = in.clients_allocated &&
		request.source != gt_source_t::Boot &&
		(plan.gametype_changed || plan.factory_changed ||
			in.teams_model_flipped);

	return plan;
}

// A timeout freezes the world; a vote or an external cvar write waits it out
// rather than reloading the map underneath a paused match. Admin and boot
// transactions are deliberate operator actions and proceed.
inline constexpr bool MM_GT_CommitAllowedNow(
	bool timeout_active, gt_source_t source) noexcept
{
	if (!timeout_active)
		return true;
	return source == gt_source_t::Boot || source == gt_source_t::Admin ||
		source == gt_source_t::ServerConsole;
}

// A later request displaces a pending one only when it is at least as
// authoritative, so a stray external cvar write cannot eat a passing vote.
inline constexpr int MM_GT_SourcePriority(gt_source_t source) noexcept
{
	switch (source) {
	case gt_source_t::Boot:			return 4;
	case gt_source_t::ServerConsole:	return 3;
	case gt_source_t::Admin:		return 2;
	case gt_source_t::Vote:			return 1;
	case gt_source_t::ExternalCvar:	return 0;
	}
	return 0;
}

// ---------------------------------------------------------------------------
// Owned-cvar ledger.
//
// The point of the ledger is that "switch factories" restores exactly what the
// outgoing factory changed. QuakeLive-SRP's equivalent resets a hand-written
// list of thirty names that has no relationship to what a factory may actually
// set, so every override outside that list leaks permanently across a switch.
// Here the restore set is derived from what was actually written.
// ---------------------------------------------------------------------------

enum class cvar_owner_t : uint8_t {
	None,
	SessionAlias,	// gametype identity, aliases, mutators
	Factory			// a factory override or a gametype module's own preset
};

enum class cvar_write_t : uint8_t {
	Unchanged,	// already held this value; nothing written, nothing recorded
	Applied,
	Rejected,	// the engine refused the value (info-string rules)
	Missing		// no such cvar; never created
};

inline constexpr size_t MM_LEDGER_MAX = 96;

// The one write primitive. Records the pre-write value on first touch of a
// name, then verifies the write actually landed -- the engine silently drops
// serverinfo values containing a backslash, semicolon or quote, and
// cvar_forceset returns the cvar either way.
cvar_write_t MM_GT_WriteOwnedCvar(const char *name, const char *value,
	cvar_owner_t owner);

// Restore every Factory-owned entry to the value it had before that factory
// applied, and drop it from the ledger.
void MM_GT_RestoreOwned();

// Diagnostics for `factory diag <cvar>`.
struct mm_cvar_provenance_t {
	bool			tracked = false;
	cvar_owner_t	owner = cvar_owner_t::None;
	std::string		prior_value;
	std::string		applied_value;
	bool			drifted = false;	// something else wrote it since
};

mm_cvar_provenance_t MM_GT_CvarProvenance(const char *name);

// ---------------------------------------------------------------------------
// Live session.
// ---------------------------------------------------------------------------

void MM_GT_SessionInit();		// PreInitGame: publish only
void MM_GT_SessionRebaseline();	// InitGame: re-derive committed state, latch
gt_reject_t MM_GT_Submit(const mm_gt_request_t &request);
void MM_GT_Reconcile();			// once per frame, above the timeout gate
void MM_GT_RunFrame();			// once per frame, the only commit point
void MM_GT_MarkNameDirty();		// something that feeds the published name moved
void MM_GT_RefreshName();		// recompose level.gametype_name if it changed

const mm_gt_selection_t &MM_GT_Committed() noexcept;

} // namespace gametype
} // namespace muffmode
