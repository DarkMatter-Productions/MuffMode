// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_debug.h"
#include "muffmode/mm_factory.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_gt_session.h"
#include "muffmode/mm_maps.h"
#include "muffmode/mm_statusbar.h"
#include "muffmode/mm_team.h"
#include "muffmode/mm_util.h"
#include "muffmode/mm_vote.h"

#include <cstring>

extern bool g_map_list_shuffled;

namespace muffmode {
namespace gametype {

// ---------------------------------------------------------------------------
// Published resolution.
// ---------------------------------------------------------------------------

// Constant-initialised, so GT()/GTF() answer effective Deathmatch before
// PreInitGame registers g_gametype -- exactly what the old null-guarded macro
// answered, without a lazy-init branch on the hot path.
mm_gt_live_t g_gt_live = {
	(int)GT_FFA, (int)GT_FFA, MM_GT_TABLE[(size_t)GT_FFA].flags, false, -1
};

void MM_GT_PublishLive()
{
	const int requested = g_gametype ? g_gametype->integer : (int)GT_FFA;
	const bool arena_live =
		(g_gametype && g_gametype->integer == (int)GT_ARENA) &&
		MM_Arena_MapActiveRaw();

	const mm_gametype_resolution_t resolved =
		MM_ResolveGametypeState(requested, arena_live);

	g_gt_live = {
		resolved.configured, resolved.effective, resolved.flags, arena_live,
		g_gametype ? g_gametype->modified_count : -1
	};
}

void MM_GT_ValidateLive()
{
	if (g_gametype &&
		g_gt_live.gametype_modified_count != g_gametype->modified_count)
		MM_GT_PublishLive();
}

#ifdef _DEBUG
void MM_GT_AssertLivePublished()
{
	const int requested = g_gametype ? g_gametype->integer : (int)GT_FFA;
	const bool arena_live =
		(g_gametype && g_gametype->integer == (int)GT_ARENA) &&
		MM_Arena_MapActiveRaw();
	const mm_gametype_resolution_t expected =
		MM_ResolveGametypeState(requested, arena_live);

	if (expected.configured == g_gt_live.configured &&
		expected.effective == g_gt_live.effective &&
		expected.flags == g_gt_live.flags &&
		arena_live == g_gt_live.arena_active)
		return;

	gi.Com_ErrorFmt(
		"gametype cache is stale: published {}/{} flags {} arena {}, "
		"recomputed {}/{} flags {} arena {}. A writer of g_gametype or the "
		"Arena map contract skipped MM_GT_PublishLive().\n",
		g_gt_live.configured, g_gt_live.effective, g_gt_live.flags,
		g_gt_live.arena_active ? 1 : 0,
		expected.configured, expected.effective, expected.flags,
		arena_live ? 1 : 0);
}
#endif

const char *MM_GT_RejectText(gt_reject_t reject) noexcept
{
	switch (reject) {
	case gt_reject_t::None:					return "accepted";
	case gt_reject_t::NoChange:				return "already active";
	case gt_reject_t::UnknownGametype:		return "unknown gametype";
	case gt_reject_t::GametypeRemoved:		return "gametype has been removed";
	case gt_reject_t::GametypeDisabled:		return "gametype is disabled";
	case gt_reject_t::NotVotable:			return "gametype is not votable";
	case gt_reject_t::UnknownFactory:		return "unknown factory";
	case gt_reject_t::FactoryBaseMismatch:	return "factory belongs to a different gametype";
	case gt_reject_t::UnsafeReloadTarget:	return "current map name is unsafe to reload";
	case gt_reject_t::ReloadTargetTooLong:	return "current map name is too long to reload";
	case gt_reject_t::LedgerExhausted:		return "too many tracked cvar overrides";
	case gt_reject_t::TransactionBusy:		return "another gametype change is committing";
	case gt_reject_t::NotDeathmatch:		return "not a deathmatch session";
	case gt_reject_t::SelectionLocked:		return "the gametype is locked by the server (g_gametype_locked)";
	}
	return "rejected";
}

namespace {

// ---------------------------------------------------------------------------
// Override ledger.
// ---------------------------------------------------------------------------

// Values are std::string rather than fixed buffers: a prior value is whatever
// the operator's own configuration left in the cvar and has no length bound.
// g_start_items in particular is CVAR_NOFLAGS, so it is not subject to the
// engine's info-string limit and routinely holds a long item list -- truncating
// it would have made "restore" write a string ending mid-classname.
struct ledger_entry_t {
	std::string		name;
	std::string		prior_value;
	std::string		prior_latched;
	std::string		applied_value;
	bool			prior_had_latch = false;
	cvar_owner_t	owner = cvar_owner_t::None;
};

std::vector<ledger_entry_t> s_ledger;

ledger_entry_t *FindLedgerEntry(const char *name)
{
	for (ledger_entry_t &entry : s_ledger)
		if (Q_strcasecmp(entry.name.c_str(), name) == 0)
			return &entry;
	return nullptr;
}

// ---------------------------------------------------------------------------
// Session state.
//
// PreInitGame and InitGame run once, when the game starts -- NOT per level.
// InitGame does `game = {}` and re-allocates game.clients under TAG_GAME, which
// would destroy the cross-map client->sess that the whole session-persistence
// contract depends on if it ran again. SpawnEntities is the per-level hook.
//
// So this state is genuinely long-lived: it survives every map change, and the
// ledger's recorded baselines stay meaningful across a rotation. Anything that
// must be re-applied at a map boundary has to be queued into the command buffer
// ahead of the map command rather than waiting for an InitGame that will not
// come; see Commit() step 6.
// ---------------------------------------------------------------------------

enum class gt_phase_t : uint8_t {
	Uninitialized,
	Idle,
	Pending,
	Committing
};

struct baseline_t {
	std::string	value;
	int			modified_count = -1;
};

gt_phase_t s_phase = gt_phase_t::Uninitialized;
mm_gt_selection_t s_committed;
mm_gt_request_t s_pending;
bool s_has_pending = false;

baseline_t s_gametype_base;
baseline_t s_teamplay_base;
baseline_t s_ctf_base;
baseline_t s_factory_base;

bool s_name_dirty = true;
uint32_t s_name_fingerprint = 0;

void CaptureBaseline(const cvar_t *cvar, baseline_t &out)
{
	if (!cvar) {
		out.value.clear();
		out.modified_count = -1;
		return;
	}
	out.value = cvar->string ? cvar->string : "";
	out.modified_count = cvar->modified_count;
}

// Compares by VALUE, not by modified count. A config re-asserting
// `set g_gametype "4"` bumps the count without moving the value; treating that
// as a change is what produced the phantom reload loop the old code only
// avoided by accident, through an undocumented engine short-circuit.
bool BaselineValueChanged(const cvar_t *cvar, const baseline_t &base)
{
	if (!cvar)
		return false;
	if (cvar->modified_count == base.modified_count)
		return false;
	return Q_strcasecmp(cvar->string ? cvar->string : "", base.value.c_str()) != 0;
}

void RelatchBaselines()
{
	CaptureBaseline(g_gametype, s_gametype_base);
	CaptureBaseline(teamplay, s_teamplay_base);
	CaptureBaseline(ctf, s_ctf_base);
	CaptureBaseline(g_factory, s_factory_base);
}

const char *IntText(int value)
{
	return G_Fmt("{}", value).data();
}

} // namespace

// ---------------------------------------------------------------------------
// The one cvar write primitive.
// ---------------------------------------------------------------------------

cvar_write_t MM_GT_WriteOwnedCvar(const char *name, const char *value,
	cvar_owner_t owner)
{
	if (!name || !*name || !value)
		return cvar_write_t::Missing;

	// Lookup only: a factory must never be able to create a cvar or re-flag an
	// existing one.
	cvar_t *cvar = gi.cvar(name, nullptr, CVAR_NOFLAGS);
	if (!cvar)
		return cvar_write_t::Missing;

	const char *current = cvar->string ? cvar->string : "";
	if (std::strcmp(current, value) == 0) {
		// Nothing to do, and skipping the write also avoids gratuitously
		// discarding a latched value that is waiting for the next map.
		return cvar_write_t::Unchanged;
	}

	ledger_entry_t *entry = FindLedgerEntry(name);
	const bool first_touch = entry == nullptr;
	if (first_touch) {
		if (s_ledger.size() >= MM_LEDGER_MAX) {
			gi.Com_PrintFmt(
				"WARNING: cvar override ledger is full ({} entries); not "
				"tracking {}.\n", MM_LEDGER_MAX, name);
			return cvar_write_t::Rejected;
		}
		ledger_entry_t fresh;
		fresh.name = name;
		fresh.prior_value = current;
		fresh.prior_had_latch =
			cvar->latched_string != nullptr && cvar->latched_string[0] != '\0';
		if (fresh.prior_had_latch)
			fresh.prior_latched = cvar->latched_string;
		s_ledger.push_back(std::move(fresh));
		entry = &s_ledger.back();
	}

	gi.cvar_forceset(name, value);

	// The engine drops serverinfo values containing a backslash, semicolon or
	// quote, and values of 64 bytes or more, *before* it reaches the write --
	// and returns the cvar regardless. Re-reading is the only way to notice.
	cvar = gi.cvar(name, nullptr, CVAR_NOFLAGS);
	const char *written = cvar && cvar->string ? cvar->string : "";
	const bool landed = std::strcmp(written, value) == 0;
	// A code-side write normally discards any pending latch and applies at once,
	// so `landed` is the usual answer even for a CVAR_LATCH cvar -- note that
	// the *cvar* moving is not the same as the gameplay effect moving, which is
	// what mm_factory_table.h's apply class describes. Accept a value that only
	// reached latched_string as success too, so a host that latches code-side
	// writes is not reported as having refused them.
	const bool latched_ok = cvar && cvar->latched_string &&
		std::strcmp(cvar->latched_string, value) == 0;

	if (!landed && !latched_ok) {
		if (first_touch)
			s_ledger.pop_back();
		gi.Com_PrintFmt(
			"WARNING: the engine refused to set {} to \"{}\" (it is still "
			"\"{}\"); the value is not usable in a server info string.\n",
			name, value, written);
		return cvar_write_t::Rejected;
	}

	entry->owner = owner;
	entry->applied_value = value;
	return cvar_write_t::Applied;
}

// Gives back everything the outgoing factory took, including settings the
// incoming factory is about to set again.
//
// Skipping the shared names would avoid one redundant write, but it would also
// leave those cvars holding the outgoing factory's value when the incoming
// factory records its own baseline -- so leaving the second factory would
// "restore" to a value the server never actually had. Restoring everything
// keeps the recorded baseline meaning "the value the operator's configuration
// produced", which is the whole point of the ledger.
void MM_GT_RestoreOwned()
{
	// Walk backwards so a name written more than once ends on its earliest
	// recorded value.
	for (size_t index = s_ledger.size(); index-- > 0;) {
		const ledger_entry_t entry = s_ledger[index];
		if (entry.owner != cvar_owner_t::Factory)
			continue;

		if (gi.cvar(entry.name.c_str(), nullptr, CVAR_NOFLAGS))
			gi.cvar_forceset(entry.name.c_str(), entry.prior_value.c_str());

		// cvar_forceset is a FROM_CODE write and frees any pending latch, so a
		// value that was waiting for the next map has to be re-issued through
		// the command buffer to latch again. Factory switches only happen at a
		// map boundary, where that buffered write lands before the next level.
		if (entry.prior_had_latch && !entry.prior_latched.empty())
			gi.AddCommandString(G_Fmt("set {} \"{}\"\n",
				entry.name.c_str(), entry.prior_latched.c_str()).data());

		s_ledger.erase(s_ledger.begin() + static_cast<ptrdiff_t>(index));
	}
}

mm_cvar_provenance_t MM_GT_CvarProvenance(const char *name)
{
	mm_cvar_provenance_t out;
	if (!name || !*name)
		return out;

	const ledger_entry_t *entry = FindLedgerEntry(name);
	if (!entry)
		return out;

	const cvar_t *cvar = gi.cvar(name, nullptr, CVAR_NOFLAGS);
	out.tracked = true;
	out.owner = entry->owner;
	out.prior_value = entry->prior_value;
	out.applied_value = entry->applied_value;
	out.drifted = cvar && cvar->string &&
		entry->applied_value != cvar->string;
	return out;
}

// ---------------------------------------------------------------------------
// Published gametype name.
// ---------------------------------------------------------------------------

namespace {

mm_mutator_t ActiveMutator()
{
	// Precedence unchanged from the ladder this replaces.
	if (g_instagib && g_instagib->integer)
		return mm_mutator_t::Instagib;
	if (g_vampiric_damage && g_vampiric_damage->integer)
		return mm_mutator_t::Vampiric;
	if (g_frenzy && g_frenzy->integer)
		return mm_mutator_t::Frenzy;
	if (g_nadefest && g_nadefest->integer)
		return mm_mutator_t::NadeFest;
	if (g_quadhog && g_quadhog->integer)
		return mm_mutator_t::QuadHog;
	return mm_mutator_t::None;
}

uint32_t NameFingerprint()
{
	uint32_t hash = 2166136261u;
	const auto mix = [&hash](uint32_t value) {
		hash = (hash ^ value) * 16777619u;
	};

	mix(static_cast<uint32_t>(g_gt_live.effective));
	mix(g_gt_live.arena_active ? 1u : 0u);
	mix(GT_RAW(GT_ARENA) ? 1u : 0u);
	mix(deathmatch && deathmatch->integer ? 1u : 0u);
	mix(coop && coop->integer ? 1u : 0u);
	mix(static_cast<uint32_t>(ActiveMutator()));

	const muffmode::factory::mm_factory_t *active =
		muffmode::factory::MM_Factory_Active();
	if (active && active->Defines())
		for (const char c : active->title)
			mix(static_cast<uint32_t>(static_cast<unsigned char>(c)));

	return hash;
}

} // namespace

void MM_GT_MarkNameDirty()
{
	s_name_dirty = true;
}

void MM_GT_RefreshName()
{
	const uint32_t fingerprint = NameFingerprint();
	if (!s_name_dirty && fingerprint == s_name_fingerprint)
		return;

	s_name_dirty = false;
	s_name_fingerprint = fingerprint;

	const muffmode::factory::mm_factory_t *active =
		muffmode::factory::MM_Factory_Active();

	mm_gametype_name_input_t input;
	input.effective_gametype = g_gt_live.effective;
	input.deathmatch = deathmatch && deathmatch->integer != 0;
	input.coop = coop && coop->integer != 0;
	input.arena_selected = GT_RAW(GT_ARENA);
	input.arena_active = g_gt_live.arena_active;
	input.mutator = ActiveMutator();
	// A factory that changes something names itself; that is the point of
	// giving one a title. One that changes nothing must not, or it would
	// advertise a mode the server is not actually running.
	input.factory_title = active && active->Defines() && !active->title.empty()
		? active->title.c_str() : nullptr;

	const mm_gametype_name_t composed = MM_ComposeGametypeName(input);
	if (std::strcmp(level.gametype_name, composed.text) == 0)
		return;

	muffmode::CopyString(level.gametype_name, composed.view());
}

// ---------------------------------------------------------------------------
// Transactions.
// ---------------------------------------------------------------------------

namespace {

bool ClientsAllocated()
{
	return game.clients != nullptr && game.maxclients > 0;
}

bool TimeoutActive()
{
	return level.timeout_in_place > 0_ms;
}

void ForceSetGametype(int gametype)
{
	gi.cvar_forceset("g_gametype", IntText(gametype));
	MM_GT_PublishLive();
}

// Put both halves of the committed selection back after a rejected external
// write. g_gametype alone is not enough: g_factory is a plain serverinfo cvar
// an operator or rcon can write at any moment, and the relatch that follows
// every reconcile would otherwise adopt the rejected id as the new baseline --
// leaving serverinfo advertising a factory the server is not running, with
// nothing left to notice it.
void RestoreCommittedSelection()
{
	if (g_gametype && g_gametype->integer != s_committed.configured)
		ForceSetGametype(s_committed.configured);

	if (!g_factory)
		return;

	const char *committed = s_committed.has_factory ? s_committed.factory_id : "";
	const char *current = g_factory->string ? g_factory->string : "";
	if (!MM_GTEqualsI(current, committed))
		gi.cvar_forceset("g_factory", committed);
}

mm_gt_plan_t BuildPlan(const mm_gt_request_t &request)
{
	const bool arena_active = g_gt_live.arena_active;

	// Which factory the request implies, and what base gametype it carries.
	const char *factory_id = nullptr;
	if (request.clear_factory)
		factory_id = nullptr;
	else if (request.has_factory)
		factory_id = request.factory_id;
	else if (s_committed.has_factory)
		factory_id = s_committed.factory_id;

	const bool factory_named = factory_id != nullptr && factory_id[0] != '\0';
	const int factory_basegt = factory_named
		? muffmode::factory::MM_Factory_BaseGametype(factory_id) : -1;

	const mm_gt_selection_t next = MM_GT_Resolve(
		request, s_committed, factory_basegt, arena_active);

	const bool gametype_moves = s_committed.configured != next.configured;
	const bool factory_moves = s_committed.has_factory != next.has_factory ||
		(next.has_factory &&
			!session_detail::IdsEqual(s_committed.factory_id, next.factory_id));
	const bool will_reload = request.allow_reload &&
		(gametype_moves || factory_moves);

	// MM_GT_Resolve already ran the requested value through the resolver; this
	// is the same derivation, kept only to recover whether the raw request was
	// in range and what its availability was.
	const int raw_request = request.gametype >= 0
		? request.gametype
		: (request.has_factory && factory_basegt >= 0
			? factory_basegt : s_committed.configured);

	mm_gt_validate_input_t check;
	check.source = request.source;
	check.requested_known = true;
	check.requested_in_range =
		MM_ResolveGametypeState(raw_request, arena_active).requested_in_range;
	check.availability = MM_IsGametypeIndex(raw_request)
		? MM_GT_TABLE[(size_t)raw_request].availability
		: gametype_avail_t::Removed;

	// A vote is checked against the list for what it actually changes: a
	// factory vote against g_votable_factories, a gametype vote against
	// g_votable_gametypes. Checking here as well as at callvote time closes the
	// window where the list moves while the vote is running.
	check.votable = (factory_moves && next.has_factory)
		? muffmode::factory::MM_Factory_IsVotable(next.factory_id)
		: MM_IsGametypeVotable((gametype_t)next.configured);

	check.factory_required = request.has_factory && request.factory_id[0] != '\0';
	check.factory_known = !check.factory_required ||
		muffmode::factory::MM_Factory_Find(request.factory_id) != nullptr;
	check.factory_basegt = factory_basegt;
	check.deathmatch = deathmatch && deathmatch->integer;
	check.ledger_has_room = s_ledger.size() < MM_LEDGER_MAX;
	check.selection_locked = g_gametype_locked && g_gametype_locked->integer;

	// Only meaningful when a reload is planned. Checking it here means an
	// unsafe map name rejects the whole transaction instead of aborting it
	// halfway through, after the cvars were written and the clients frozen.
	check.reload_target_safe = !will_reload || MM_IsSafeMapToken(level.mapname);
	check.reload_target_fits = !will_reload ||
		std::strlen(level.mapname) < sizeof(level.nextmap);

	check.changes_anything = gametype_moves || factory_moves ||
		request.force_reapply || request.source == gt_source_t::Boot;

	const gt_reject_t validation = MM_GT_Validate(next, check);

	mm_gt_plan_input_t plan_input;
	plan_input.clients_allocated = ClientsAllocated();
	plan_input.teams_model_flipped =
		((MM_GT_TABLE[(size_t)s_committed.effective].flags & GTF_TEAMS) != 0) !=
		((MM_GT_TABLE[(size_t)next.effective].flags & GTF_TEAMS) != 0);

	return MM_GT_Plan(request, s_committed, next, validation, plan_input);
}

// This is a *session* reset, not an in-level team change, and it is
// deliberately a raw write rather than a SetTeam() call. client->sess survives
// the map change that follows, so without this every player would arrive in the
// new gametype still holding the team they had in the old one. SetTeam is the
// wrong tool twice over: it refuses TEAM_NONE outside Duel
// (mm_team.cpp: `desired_team == TEAM_NONE && !(force && GT(GT_DUEL))`), and
// the respawn/spectator entity work it does would be discarded by the reload
// anyway.
void ResetClientSessions()
{
	for (auto ec : active_clients()) {
		if (!ec->client)
			continue;

		if (ec->client->sess.is_a_bot || (ec->svflags & SVF_BOT)) {
			// Reset bots fully so they go through ClientConnect's spectator
			// initialisation and are re-assigned by the warmup tick rather than
			// inheriting a team from the previous gametype.
			ec->client->sess.team = TEAM_NONE;
			P_PublishEngineTeam(ec);
			ec->client->sess.initialised = false;
			continue;
		}

		ec->client->sess.team = TEAM_NONE;
		P_PublishEngineTeam(ec);
		ec->client->sess.duel_queued = false;
		ec->client->sess.initialised = false;
		ec->client->initial_menu_shown = false;
		ec->client->initial_menu_delay = level.time + 10_hz;
	}
}

void Broadcast(const mm_gt_plan_t &plan)
{
	if (!plan.gametype_changed && !plan.factory_changed)
		return;

	const muffmode::factory::mm_factory_t *active = plan.next.has_factory
		? muffmode::factory::MM_Factory_Find(plan.next.factory_id) : nullptr;

	if (active)
		gi.LocBroadcast_Print(PRINT_HIGH, "Gametype: {} ({}).\n",
			active->title.c_str(), gt_short_name[plan.next.configured]);
	else
		gi.LocBroadcast_Print(PRINT_HIGH, "Gametype: {}.\n",
			gt_long_name[plan.next.configured]);
}

void Commit(const mm_gt_plan_t &plan)
{
	s_phase = gt_phase_t::Committing;

	const mm_gt_selection_t previous = s_committed;

	// A re-apply of the selection already in force writes the same overrides
	// again, so it needs the same restore-then-apply treatment a switch gets.
	// Gating those two steps on factory_changed alone is what made
	// `factory <active id>` and `gametype <current>` reload the map and apply
	// nothing, despite both being documented as "apply this preset again".
	const bool writes_factory = plan.factory_changed || plan.reapply_factory;

	// 1. Give back everything the outgoing factory took. Doing this first means
	//    the incoming factory writes over a clean baseline.
	if (writes_factory)
		MM_GT_RestoreOwned();

	// 2. Identity and the legacy aliases, always.
	ForceSetGametype(plan.next.configured);
	MM_GT_WriteOwnedCvar("ctf", plan.aliases.ctf ? "1" : "0",
		cvar_owner_t::SessionAlias);
	MM_GT_WriteOwnedCvar("teamplay", plan.aliases.teamplay ? "1" : "0",
		cvar_owner_t::SessionAlias);
	if (deathmatch && !deathmatch->integer) {
		gi.Com_Print("Forcing deathmatch.\n");
		MM_GT_WriteOwnedCvar("deathmatch", "1", cvar_owner_t::SessionAlias);
	}

	// 3. Arena's self-armour preset must not leak into the other arena modes.
	if (plan.arena_dmg_armor >= 0)
		MM_GT_WriteOwnedCvar("g_arena_dmg_armor", IntText(plan.arena_dmg_armor),
			cvar_owner_t::SessionAlias);

	// 4. Publish the selection.
	if (plan.factory_changed)
		gi.cvar_forceset("g_factory",
			plan.next.has_factory ? plan.next.factory_id : "");

	// 5. Apply the factory's overrides, queued so they land AFTER the identity
	//    writes above and BEFORE the map command below.
	//
	//    This cannot wait for InitGame: the engine calls the game's Init once
	//    when the game starts, not per level, so nothing would ever apply them.
	//    Queueing here is also what makes the latched overrides work -- they are
	//    written before the map load, so they are live when the level comes up.
	if (writes_factory)
		gi.AddCommandString("sv gt_factory\n");

	// 6. Commit, publish, and latch the reconciler baselines from the values
	//    the cvars actually ended up holding.
	s_committed = plan.next;
	MM_GT_PublishLive();
	RelatchBaselines();

	// 7. Put every client back through the join flow. client->sess persists
	//    across the map change below, so this is what stops a player carrying
	//    their old team into the new gametype.
	if (plan.reset_client_sessions)
		ResetClientSessions();

	// 8. Exactly one map command, or none. The reload decision belongs to the
	//    plan; previously each caller queued its own and the frame poller could
	//    queue a third.
	if (plan.requires_reload)
		gi.AddCommandString("sv gt_changemap_first\n");

	if (plan.gametype_changed) {
		g_map_list_shuffled = false;
		MuffModeLog("GAMETYPE", "committed %s (%d) -> %s (%d)%s%s",
			gt_short_name[previous.configured], previous.configured,
			gt_short_name[plan.next.configured], plan.next.configured,
			plan.next.has_factory ? " factory " : "",
			plan.next.has_factory ? plan.next.factory_id : "");
	}

	MM_GT_RefreshName();
	MM_MatchInfoHud_ShowAll();
	Broadcast(plan);

	s_phase = gt_phase_t::Idle;
}

} // namespace

const mm_gt_selection_t &MM_GT_Committed() noexcept
{
	return s_committed;
}

gt_reject_t MM_GT_Submit(const mm_gt_request_t &request)
{
	if (s_phase == gt_phase_t::Committing)
		return gt_reject_t::TransactionBusy;

	const mm_gt_plan_t plan = BuildPlan(request);
	if (!plan.valid)
		return plan.reject;

	// Boot runs inside PreInitGame/InitGame, outside any frame, and must take
	// effect before the level finishes coming up.
	if (request.source == gt_source_t::Boot) {
		Commit(plan);
		return gt_reject_t::None;
	}

	// Everything else lands at a frame boundary. Committing inside
	// ClientCommand is how a passing vote used to wipe every client's team
	// mid-frame and then spend the rest of that frame fighting the warmup tick
	// putting them straight back.
	if (s_has_pending &&
		MM_GT_SourcePriority(request.source) <
			MM_GT_SourcePriority(s_pending.source))
		return gt_reject_t::TransactionBusy;

	s_pending = request;
	s_has_pending = true;
	s_phase = gt_phase_t::Pending;
	return gt_reject_t::None;
}

void MM_GT_RunFrame()
{
	if (s_phase == gt_phase_t::Uninitialized || !s_has_pending)
		return;

	if (!MM_GT_CommitAllowedNow(TimeoutActive(), s_pending.source))
		return;

	const mm_gt_request_t request = s_pending;
	s_has_pending = false;
	s_phase = gt_phase_t::Idle;

	// Re-validate from the drained request. State can have moved since submit:
	// this generalises the votability re-check the vote path used to hand-roll,
	// and extends it to availability, factory existence and reload safety.
	const mm_gt_plan_t plan = BuildPlan(request);
	if (!plan.valid) {
		if (plan.reject != gt_reject_t::NoChange)
			gi.LocBroadcast_Print(PRINT_HIGH,
				"Gametype change cancelled: {}.\n",
				MM_GT_RejectText(plan.reject));

		// A rejected external write left its value in the cvar, and the
		// reconciler has already re-latched against it -- so without this the
		// cvars would disagree with the selection actually in force for the
		// rest of the level, with nothing left to notice.
		RestoreCommittedSelection();
		RelatchBaselines();
		return;
	}

	Commit(plan);
}

void MM_GT_Reconcile()
{
	if (s_phase == gt_phase_t::Uninitialized || !g_gametype)
		return;

	const bool gametype_moved = BaselineValueChanged(g_gametype, s_gametype_base);
	const bool teamplay_moved = BaselineValueChanged(teamplay, s_teamplay_base);
	const bool ctf_moved = BaselineValueChanged(ctf, s_ctf_base);
	// g_factory is the other half of the committed selection and is a plain
	// serverinfo cvar an operator or rcon can write at any moment. Watching it
	// here is what stops serverinfo advertising one factory while the session,
	// the published name and the ledger all still hold another.
	const bool factory_moved = BaselineValueChanged(g_factory, s_factory_base);

	if (!gametype_moved && !teamplay_moved && !ctf_moved && !factory_moved) {
		RelatchBaselines();
		return;
	}

	mm_gt_request_t request;
	request.source = gt_source_t::ExternalCvar;
	request.gametype = gametype_moved
		? g_gametype->integer
		: (int)MM_GTGametypeFromAliases(
			(gametype_t)s_committed.configured,
			ctf && ctf->integer != 0,
			teamplay && teamplay->integer != 0);

	if (factory_moved) {
		const char *selected = g_factory->string ? g_factory->string : "";
		if (selected[0]) {
			request.has_factory = true;
			session_detail::CopyId(request.factory_id, selected);
			// An externally named factory brings its own base gametype unless
			// the same write also moved g_gametype.
			if (!gametype_moved) {
				const int base =
					muffmode::factory::MM_Factory_BaseGametype(selected);
				if (base >= 0)
					request.gametype = base;
			}
		} else {
			request.clear_factory = true;
		}
	}

	const gt_reject_t reject = MM_GT_Submit(request);

	// An alias write that resolves back to the gametype already in force is a
	// NoChange, so no transaction puts the alias back -- and the relatch below
	// would then adopt the drifted value, leaving `ctf 1` advertised on a
	// Deathmatch server with nothing left to notice. Re-project it here.
	if (reject == gt_reject_t::NoChange && (teamplay_moved || ctf_moved)) {
		const mm_gt_alias_projection_t aliases =
			MM_GTAliasesFor((gametype_t)s_committed.configured);
		MM_GT_WriteOwnedCvar("ctf", aliases.ctf ? "1" : "0",
			cvar_owner_t::SessionAlias);
		MM_GT_WriteOwnedCvar("teamplay", aliases.teamplay ? "1" : "0",
			cvar_owner_t::SessionAlias);
	}

	// A factory write that resolves to the selection already in force is a
	// NoChange for the transaction, but the cvar may still hold a differently
	// spelled or stale id that the relatch below would adopt.
	if (reject == gt_reject_t::NoChange && factory_moved)
		RestoreCommittedSelection();

	if (reject != gt_reject_t::None && reject != gt_reject_t::NoChange) {
		gi.Com_PrintFmt("g_gametype {} rejected ({}); restoring {}.\n",
			g_gametype->integer, MM_GT_RejectText(reject),
			gt_short_name[s_committed.configured]);

		// Put the identity back by hand. A synthesised restore *request* cannot
		// do it: its selection is the committed one by definition, so it
		// validates as NoChange and writes nothing -- and the relatch below
		// would then adopt the rejected value as the new baseline, leaving the
		// cvars advertising a selection the server is not running with nothing
		// left to notice.
		RestoreCommittedSelection();

		const mm_gt_alias_projection_t aliases =
			MM_GTAliasesFor((gametype_t)s_committed.configured);
		MM_GT_WriteOwnedCvar("ctf", aliases.ctf ? "1" : "0",
			cvar_owner_t::SessionAlias);
		MM_GT_WriteOwnedCvar("teamplay", aliases.teamplay ? "1" : "0",
			cvar_owner_t::SessionAlias);
	}

	RelatchBaselines();
}

// The boot transaction. It does not route through Commit because there is
// nothing to write: InitGametype has already reconciled deathmatch/teamplay/ctf
// and MM_SanitizeCurrentGametype has already corrected g_gametype. Committing
// here would also be unsafe -- g_entities and game.clients are not allocated
// until InitGame, and most cvars are not registered yet. gt_source_t::Boot
// still exists so a plan built during boot is provably inert.
void MM_GT_SessionInit()
{
	MM_GT_PublishLive();

	s_committed = mm_gt_selection_t{};
	s_committed.configured = g_gt_live.configured;
	s_committed.effective = g_gt_live.effective;
	s_committed.has_factory = false;
	session_detail::CopyId(s_committed.factory_id, "");

	s_ledger.clear();
	s_has_pending = false;
	s_phase = gt_phase_t::Idle;
	RelatchBaselines();
	MM_GT_MarkNameDirty();
}

void MM_GT_SessionRebaseline()
{
	MM_GT_PublishLive();

	s_committed.configured = g_gt_live.configured;
	s_committed.effective = g_gt_live.effective;

	const char *selected = g_factory && g_factory->string ? g_factory->string : "";
	s_committed.has_factory = selected[0] != '\0' &&
		muffmode::factory::MM_Factory_Find(selected) != nullptr;
	session_detail::CopyId(s_committed.factory_id,
		s_committed.has_factory ? selected : "");

	// A fresh level starts with no tracked overrides: whatever the operator's
	// own configs produced is the new baseline, and it is that baseline a factory
	// switch restores to.
	s_ledger.clear();
	s_has_pending = false;
	s_phase = gt_phase_t::Idle;
	RelatchBaselines();
	MM_GT_MarkNameDirty();
}

} // namespace gametype
} // namespace muffmode
