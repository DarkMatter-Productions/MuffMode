// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Miscellaneous game framework utilities.

#include "g_local.h"
#include "muffmode/mm_ordnance_identity.h"
#include "debug_log.h"
// [MuffMode] Team management lives in muffmode/mm_team
#include "muffmode/mm_arena.h"
#include "muffmode/mm_freezetag.h"
#include "muffmode/mm_profile.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_skin.h"
#include "muffmode/mm_spawn_rules.h"
#include "muffmode/mm_time_format.h"
#include "muffmode/mm_team.h"
#include "muffmode/mm_announcer_rules.h"
#include "muffmode/mm_util.h"
#include <ctime>
#include <vector>

static size_t G_EntitySearchLimit() {
	return min(static_cast<size_t>(globals.num_entities), static_cast<size_t>(game.maxentities));
}

/*
=============
G_Find

Searches all active entities for the next one that validates the given callback.

Searches beginning at the entity after from, or the beginning if nullptr
nullptr will be returned if the end of the list is reached.
=============
*/
gentity_t *G_Find(gentity_t *from, std::function<bool(gentity_t *e)> matcher) {
	if (!from)
		from = g_entities;
	else
		from++;

	gentity_t *end = &g_entities[G_EntitySearchLimit()];
	if (from >= end)
		return nullptr;

	for (; from < end; from++) {
		if (!from->inuse)
			continue;
		if (matcher(from))
			return from;
	}

	return nullptr;
}

void G_LogInvalidEntityString(gentity_t *e, const char *ptr, const char *context) {
	const int ent_num = e ? (int)(e - g_entities) : -1;
	MuffModeLog("WARN", "G_FindByString: skipping ent %d invalid string ptr %p (search '%s')",
		ent_num, (void *)ptr, context ? context : "");

	// #region agent log
	{
		FILE *f = fopen("debug-9a6197.log", "a");
		if (f) {
			const auto ts = (long long)time(nullptr) * 1000LL;
			fprintf(f,
				"{\"sessionId\":\"9a6197\",\"runId\":\"post-fix\",\"hypothesisId\":\"H1\","
				"\"location\":\"sgame/core/game_utils.cpp:G_LogInvalidEntityString\","
				"\"message\":\"invalid entity string ptr skipped\","
				"\"data\":{\"ent\":%d,\"ptr\":\"%p\",\"search\":\"%s\",\"inuse\":%d},"
				"\"timestamp\":%lld}\n",
				ent_num, (void *)ptr, context ? context : "", e ? (int)e->inuse : 0, ts);
			fclose(f);
		}
	}
	// #endregion
}

/*
=================
findradius

Returns entities that have origins within a spherical area

findradius (origin, radius)
=================
*/
gentity_t *findradius(gentity_t *from, const vec3_t &org, float rad) {
	vec3_t eorg;
	int	   j;

	if (!from)
		from = g_entities;
	else
		from++;

	gentity_t *end = &g_entities[G_EntitySearchLimit()];
	if (from >= end)
		return nullptr;

	for (; from < end; from++) {
		if (!from->inuse)
			continue;
		if (from->solid == SOLID_NOT)
			continue;
		for (j = 0; j < 3; j++)
			eorg[j] = org[j] - (from->s.origin[j] + (from->mins[j] + from->maxs[j]) * 0.5f);
		if (eorg.length() > rad)
			continue;
		return from;
	}

	return nullptr;
}


/*
=============
G_PickTarget

Searches all active entities for the next one that holds
the matching string at fieldofs in the structure.

Searches beginning at the entity after from, or the beginning if nullptr
nullptr will be returned if the end of the list is reached.

=============
*/
constexpr size_t MAXCHOICES = 8;

gentity_t *G_PickTarget(const char *targetname) {
	gentity_t	*choice[MAXCHOICES];
	gentity_t	*ent = nullptr;
	int		num_choices = 0;

	if (!targetname) {
		gi.Com_PrintFmt("{}: called with nullptr targetname.\n", __FUNCTION__);
		return nullptr;
	}

	while (1) {
		ent = G_FindByString<&gentity_t::targetname>(ent, targetname);
		if (!ent)
			break;
		choice[num_choices++] = ent;
		if (num_choices == MAXCHOICES)
			break;
	}

	if (!num_choices) {
		gi.Com_PrintFmt("{}: target {} not found\n", __FUNCTION__, targetname);
		return nullptr;
	}

	return choice[irandom(num_choices)];
}

static gentity_t *G_ResolveDelayedActivator(gentity_t *ent) {
	if (!ent || !ent->activator)
		return nullptr;

	gentity_t *activator = ent->activator;
	const bool has_client = activator->client != nullptr;
	const bool connected =
		!has_client || activator->client->pers.connected;
	const bool arena_client = GT(GT_ARENA) && has_client;
	const int source_room = arena_client ? ent->arena : -1;
	const int current_room = arena_client ? MM_Arena_Id(activator) : -1;
	if (!MM_ArenaDelayedActivatorValid(activator->inuse, connected,
		ent->count, activator->spawn_count, source_room, current_room))
		return nullptr;

	return activator;
}

// Internal marker for a DelayedUse created by a spawn-time map relay. The
// entity may fire after SERVER_FLAG_LOADING has cleared, but it still belongs
// to map initialization and must not be suppressed by a round countdown.
constexpr int32_t HACKFLAG_DELAYED_MAP_INITIALIZATION = 1 << 30;

static THINK(Think_Delay) (gentity_t *ent) -> void {
	const int32_t ent_generation = ent->spawn_count;
	// Keep firing the source room's environmental targets, but do not pass a
	// disconnected, reused, or cross-room client to player-specific callbacks.
	// Map use callbacks historically assume a non-null activator, so normalize a
	// retired lifetime to the canonical world source instead of a recycled slot.
	ent->activator = G_ResolveDelayedActivator(ent);
	if (!ent->activator)
		ent->activator = world;
	const bool source_survived = G_UseTargets(ent, ent->activator);
	if (source_survived && ent->inuse && ent->spawn_count == ent_generation)
		G_FreeEntity(ent);
}

void G_PrintActivationMessage(gentity_t *ent, gentity_t *activator, bool coop_global) {
	//
	// print the message
	//
	if ((ent->message) && (!activator || !(activator->svflags & SVF_MONSTER))) {
		if (coop_global && coop->integer)
			gi.LocBroadcast_Print(PRINT_CENTER, "{}", ent->message);
		else if (activator)
			gi.LocCenter_Print(activator, "{}", ent->message);

		// [Paril-KEX] allow non-noisy centerprints
		if (activator && ent->noise_index >= 0) {
			if (ent->noise_index)
				gi.sound(activator, CHAN_AUTO, ent->noise_index, 1, ATTN_NORM, 0);
			else
				gi.sound(activator, CHAN_AUTO, gi.soundindex("misc/talk1.wav"), 1, ATTN_NORM, 0);
		}
	}
}

/*
=============
BroadcastFriendlyMessage

Broadcast a friendly message to active teammates or, in non-team modes, all
active players.
=============
*/
void BroadcastFriendlyMessage(team_t team, const char *msg) {
	for (auto ce : active_clients()) {
		const bool playing = ClientIsPlaying(ce->client);
		if (!playing) {
			if (!Teams())
				continue;
			gentity_t *follow = ce->client->follow_target;
			if (!follow || !follow->client || follow->client->sess.team != team)
				continue;
		} else if (Teams() && ce->client->sess.team != team) {
			continue;
		}
		gi.LocClient_Print(ce, PRINT_HIGH, G_Fmt("{}{}", playing && ce->client->sess.team != TEAM_SPECTATOR ? "[TEAM]: " : "", msg).data());
	}
}

/*
=============
BroadcastTeamMessage

Broadcast a message to all clients actively playing for the specified team.
=============
*/
void BroadcastTeamMessage(team_t team, print_type_t level, const char *msg) {
	for (auto ce : active_clients()) {
		if (!ClientIsPlaying(ce->client))
			continue;
		if (ce->client->sess.team != team)
			continue;

		gi.LocClient_Print(ce, level, msg);

	}
}

void G_MonsterKilled(gentity_t *self);

static bool G_EntityLifetimeMatches(
	const gentity_t *entity, int32_t generation) noexcept
{
	return entity && entity->inuse && entity->spawn_count == generation;
}

static bool G_IsReciprocalTeamMember(
	const gentity_t *member, const gentity_t *master) noexcept
{
	if (!member || !member->inuse || !master)
		return false;

	if (member == master) {
		return member->teammaster == master &&
			(member->flags & FL_TEAMMASTER) &&
			!(member->flags & FL_TEAMSLAVE);
	}

	return member->teammaster == master &&
		(member->flags & FL_TEAMSLAVE) &&
		!(member->flags & FL_TEAMMASTER);
}

// Validate every edge before following it. A non-null edge after visiting the
// maximum possible number of entities is necessarily cyclic, so truncate it.
static bool G_SanitizeEntityTeamChain(
	gentity_t *master, int32_t master_generation) noexcept
{
	if (!G_EntityLifetimeMatches(master, master_generation) ||
		!G_IsReciprocalTeamMember(master, master))
		return false;

	gentity_t *member = master;
	size_t remaining = G_EntitySearchLimit();
	while (remaining > 0) {
		--remaining;
		gentity_t *const next = member->teamchain;
		if (!next)
			return true;

		if (!G_IsReciprocalTeamMember(next, master) || remaining == 0) {
			member->teamchain = nullptr;
			return true;
		}

		member = next;
	}

	return false;
}

static void G_RemoveEntityFromTeam(gentity_t *entity) noexcept
{
	if (!entity || !entity->teammaster)
		return;

	gentity_t *const master = entity->teammaster;
	const int32_t entity_generation = entity->spawn_count;
	const int32_t master_generation = master->spawn_count;
	if (!G_SanitizeEntityTeamChain(master, master_generation))
		return;

	if (entity->flags & FL_TEAMSLAVE) {
		gentity_t *predecessor = master;
		size_t remaining = G_EntitySearchLimit();
		while (predecessor && remaining > 0) {
			--remaining;
			if (!G_EntityLifetimeMatches(master, master_generation) ||
				!G_IsReciprocalTeamMember(predecessor, master))
				return;

			if (predecessor->teamchain == entity) {
				if (!G_EntityLifetimeMatches(entity, entity_generation) ||
					!G_IsReciprocalTeamMember(entity, master))
					return;

				gentity_t *successor = entity->teamchain;
				if (successor &&
					!G_IsReciprocalTeamMember(successor, master))
					successor = nullptr;
				predecessor->teamchain = successor;
				entity->teamchain = nullptr;
				entity->teammaster = nullptr;
				entity->flags &= ~FL_TEAMSLAVE;
				return;
			}

			predecessor = predecessor->teamchain;
		}
		return;
	}

	if (!(entity->flags & FL_TEAMMASTER) || entity != master)
		return;

	gentity_t *const new_master = master->teamchain;
	master->teamchain = nullptr;
	master->teammaster = nullptr;
	master->flags &= ~FL_TEAMMASTER;
	if (!new_master)
		return;

	new_master->flags |= FL_TEAMMASTER;
	new_master->flags &= ~FL_TEAMSLAVE;

	gentity_t *member = new_master;
	size_t remaining = G_EntitySearchLimit();
	while (member && remaining > 0) {
		--remaining;
		gentity_t *const next = member->teamchain;
		member->teammaster = new_master;
		if (member != new_master) {
			member->flags |= FL_TEAMSLAVE;
			member->flags &= ~FL_TEAMMASTER;
		}
		member = next;
	}
}

static void G_BoundRestoredEntityChains(size_t entity_count)
{
	// teamchain is a functional graph: each entity has at most one outgoing
	// edge. Color each live node once so corrupt save data cannot leave cycles
	// for pusher, toss, arena-ordnance, or Tesla-area walkers to follow.
	std::vector<uint8_t> state(entity_count, 0);
	std::vector<size_t> path;
	path.reserve(entity_count);
	size_t truncated = 0;
	for (size_t root = 0; root < entity_count; ++root) {
		if (!g_entities[root].inuse || state[root] != 0)
			continue;

		path.clear();
		size_t current = root;
		while (true) {
			if (state[current] == 2)
				break;
			if (state[current] == 1) {
				g_entities[path.back()].teamchain = nullptr;
				++truncated;
				break;
			}

			state[current] = 1;
			path.push_back(current);
			gentity_t *const next = g_entities[current].teamchain;
			if (!next)
				break;

			const ptrdiff_t next_index = next - g_entities;
			if (next_index < 0 ||
				static_cast<size_t>(next_index) >= entity_count ||
				!next->inuse) {
				g_entities[current].teamchain = nullptr;
				++truncated;
				break;
			}
			current = static_cast<size_t>(next_index);
		}

		for (const size_t index : path)
			state[index] = 2;
	}

	if (truncated != 0)
		gi.Com_PrintFmt(
			"WARNING: truncated {} invalid restored entity chain edge{}.\n",
			truncated, truncated == 1 ? "" : "s");
}

void G_SanitizeRestoredEntityTeams()
{
	const size_t entity_count = G_EntitySearchLimit();
	G_BoundRestoredEntityChains(entity_count);
	for (size_t index = 0; index < entity_count; ++index) {
		gentity_t *const entity = &g_entities[index];
		if (!entity->inuse || !entity->teamchain)
			continue;
		const bool claims_team_master = entity->flags & FL_TEAMMASTER;
		if (entity->teammaster == entity && claims_team_master &&
			!(entity->flags & FL_TEAMSLAVE) &&
			G_SanitizeEntityTeamChain(entity, entity->spawn_count)) {
			continue;
		}
		// teamchain is also intentionally used by ordnance helpers (prox mines,
		// teslas, doppelgangers, and others). Only malformed pusher roots are
		// eligible for mover cleanup here; leave those private graphs intact.
		const bool pusher_root =
			entity->movetype == MOVETYPE_PUSH || entity->movetype == MOVETYPE_STOP;
		if (claims_team_master ||
			(pusher_root && !(entity->flags & FL_TEAMSLAVE)))
			entity->teamchain = nullptr;
	}
}

void G_MigrateLegacyEntityReferenceStamps()
{
	const size_t entity_count = G_EntitySearchLimit();
	for (size_t index = 0; index < entity_count; ++index) {
		gentity_t *const entity = &g_entities[index];
		if (!entity->inuse || !entity->classname)
			continue;
		if (entity->target_ent && entity->target_ent->inuse) {
			entity->target_ent_generation =
				entity->target_ent->spawn_count;
		}

		if (strcmp(entity->classname, "misc_explobox") == 0 ||
			strcmp(entity->classname, "target_explosion") == 0) {
			entity->count = entity->activator && entity->activator->inuse
				? entity->activator->spawn_count : 0;
			continue;
		}
		if (strcmp(entity->classname, "item_health_mega") == 0) {
			entity->megahealth_owner_generation =
				entity->owner && entity->owner->inuse
					? entity->owner->spawn_count : 0;
			continue;
		}
		if (strcmp(entity->classname, "doppelganger") == 0) {
			entity->count = entity->teammaster && entity->teammaster->inuse
				? entity->teammaster->spawn_count : 0;
			entity->sounds = MM_Arena_Id(entity->teammaster);
			entity->arena = entity->sounds;
			entity->sphere_enemy_generation =
				entity->enemy && entity->enemy->inuse
					? entity->enemy->spawn_count : 0;
			entity->doppel_body_generation =
				entity->teamchain && entity->teamchain->inuse &&
					entity->teamchain->teammaster == entity
					? entity->teamchain->spawn_count : 0;
			if (entity->doppel_body_generation) {
				entity->teamchain->arena = entity->arena;
				entity->teamchain->doppel_body_generation =
					entity->spawn_count;
			}
			continue;
		}
		if (strcmp(entity->classname, "sphere") == 0) {
			gentity_t *const owner = entity->spawnflags.has(SF_DOPPELGANGER)
				? entity->teammaster : entity->owner;
			entity->count = owner && owner->inuse ? owner->spawn_count : 0;
			entity->sounds = MM_Arena_Id(owner);
			entity->arena = entity->sounds;
			entity->sphere_enemy_generation =
				entity->enemy && entity->enemy->inuse
					? entity->enemy->spawn_count : 0;
		}
	}
}

/*
==============================
G_UseTargets

the global "activator" should be set to the entity that initiated the firing.

If self.delay is set, a DelayedUse entity will be created that will actually
do the SUB_UseTargets after that many seconds have passed.

Centerprints any self.message to the activator.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function

==============================
*/
bool G_UseTargets(gentity_t *ent, gentity_t *activator) {
	gentity_t *t;

	if (!ent || !ent->inuse)
		return false;
	const int32_t ent_generation = ent->spawn_count;
	int32_t activator_generation = activator ? activator->spawn_count : 0;
	const auto ent_is_current = [&]() {
		return ent->inuse && ent->spawn_count == ent_generation;
	};
	const auto refresh_activator = [&]() {
		if (!activator || !activator->inuse ||
			activator->spawn_count != activator_generation ||
			(activator->client && !activator->client->pers.connected)) {
			// Map callbacks routinely dereference activator. Preserve the event
			// with neutral attribution when its original lifetime has retired.
			activator = world;
			activator_generation = world ? world->spawn_count : 0;
		}
	};
	refresh_activator();
	// Keep the room boundary stable for the full synchronous dispatch. A target
	// callback may disconnect or move the original client before later matches.
	const bool delayed_source = ent->classname &&
		!std::strcmp(ent->classname, "DelayedUse");
	const gentity_t *const arena_source = delayed_source || MM_Arena_Id(ent) > 0
		? ent
		: (activator && activator->client ? activator : ent);
	const int activation_arena = GT(GT_ARENA)
		? MM_Arena_Id(arena_source) : 0;
	const bool paused_fighter_source = GT(GT_ARENA) &&
		arena_source && arena_source->client && activation_arena > 0 &&
		MM_Arena_IsFighter(arena_source->client) &&
		MM_Arena_IsPaused(activation_arena);
	const auto can_use_in_activation_arena = [&](const gentity_t *target) {
		if (!GT(GT_ARENA))
			return true;
		if (paused_fighter_source)
			return false;
		const int target_arena = MM_Arena_Id(target);
		return activation_arena <= 0 || target_arena <= 0 ||
			activation_arena == target_arena;
	};

	// Map-authored spawn-time relays (notably trigger_always) must initialize
	// while an entity lump is loading. A round reload deliberately occurs
	// during countdown, but that transient combat state must not suppress map
	// setup targets or change their helper-entity allocation topology.
	const bool delayed_map_initialization =
		ent->think == Think_Delay &&
		(ent->hackflags & HACKFLAG_DELAYED_MAP_INITIALIZATION);
	if (!(globals.server_flags & SERVER_FLAG_LOADING) &&
		!delayed_map_initialization &&
		notGT(GT_ARENA) && IsCombatDisabled())
		return true;

	//
	// check for a delay
	//
	if (ent->delay) {
		// create a temp object to fire at a later time
		t = G_Spawn();
		t->classname = "DelayedUse";
		t->nextthink = level.time + gtime_t::from_sec(ent->delay);
		t->think = Think_Delay;
		if (globals.server_flags & SERVER_FLAG_LOADING)
			t->hackflags |= HACKFLAG_DELAYED_MAP_INITIALIZATION;
		t->activator = activator;
		t->count = activator ? activator->spawn_count : 0;
		if (!activator)
			gi.Com_PrintFmt("{}: {} with no activator.\n", __FUNCTION__, *t);
		t->message = ent->message;
		t->target = ent->target;
		t->killtarget = ent->killtarget;
		if (GT(GT_ARENA)) {
			int source_arena = MM_Arena_Id(ent);
			if (source_arena <= 0 && activator && activator->client)
				source_arena = MM_Arena_Id(activator);
			// Snapshot the room at scheduling time.  The activator may leave,
			// disconnect, or enter another arena before this entity fires.
			t->arena = source_arena > 0 ? source_arena : 0;
		} else {
			t->arena = ent->arena;
		}
		return ent_is_current();
	}

	//
	// print the message
	//
	G_PrintActivationMessage(ent, activator, true);

	//
	// kill killtargets
	//
	if (ent->killtarget) {
		t = nullptr;
		while ((t = G_FindByString<&gentity_t::targetname>(t, ent->killtarget))) {
			refresh_activator();
			if (!can_use_in_activation_arena(t))
				continue;

			// [Paril-KEX] if we killtarget a monster, clean up properly
			if (t->svflags & SVF_MONSTER) {
				if (!t->deadflag && !(t->monsterinfo.aiflags & AI_DO_NOT_COUNT) && !(t->spawnflags & SPAWNFLAG_MONSTER_DEAD))
					G_MonsterKilled(t);
			}

			G_FreeEntity(t);

			if (!ent_is_current()) {
				gi.Com_PrintFmt("{}: Entity was removed while using killtargets.\n", __FUNCTION__);
				return false;
			}
		}
	}

	//
	// fire targets
	//
	if (ent->target) {
		t = nullptr;
		while ((t = G_FindByString<&gentity_t::targetname>(t, ent->target))) {
			refresh_activator();
			if (!can_use_in_activation_arena(t))
				continue;

			// doors fire area portals in a specific way
			if (!Q_strcasecmp(t->classname, "func_areaportal") &&
				(!Q_strcasecmp(ent->classname, "func_door") || !Q_strcasecmp(ent->classname, "func_door_rotating")
					|| !Q_strcasecmp(ent->classname, "func_door_secret") || !Q_strcasecmp(ent->classname, "func_water")))
				continue;

			if (t == ent) {
				gi.Com_PrintFmt("{}: WARNING: Entity used itself.\n", __FUNCTION__);
			} else {
				if (t->use)
					t->use(t, ent, activator);
			}
			if (!ent_is_current()) {
				gi.Com_PrintFmt("{}: Entity was removed while using targets.\n", __FUNCTION__);
				return false;
			}
			refresh_activator();
		}
	}
	return ent_is_current();
}

/*
===============
G_SetMovedir
===============
*/
void G_SetMovedir(vec3_t &angles, vec3_t &movedir) {
	static vec3_t VEC_UP		= { 0, -1, 0 };
	static vec3_t MOVEDIR_UP	= { 0, 0, 1 };
	static vec3_t VEC_DOWN		= { 0, -2, 0 };
	static vec3_t MOVEDIR_DOWN	= { 0, 0, -1 };

	if (angles == VEC_UP) {
		movedir = MOVEDIR_UP;
	} else if (angles == VEC_DOWN) {
		movedir = MOVEDIR_DOWN;
	} else {
		AngleVectors(angles, movedir, nullptr, nullptr);
	}

	angles = {};
}

char *G_CopyString(const char *in, int32_t tag) {
	if (!in)
		return nullptr;
	const size_t amt = strlen(in) + 1;
	char *const out = static_cast<char *>(gi.TagMalloc(amt, tag));
	Q_strlcpy(out, in, amt);
	return out;
}

void G_InitGentity(gentity_t *e) {
	// FIXME -
	//   this fixes a bug somewhere that is setting "nextthink" for an entity that has
	//   already been released. nextthink is being set to FRAME_TIME_S after level.time,
	//   since freetime = nextthink - FRAME_TIME_S
	if (e->nextthink)
		e->nextthink = 0_ms;

	e->inuse = true;
	e->sv.init = false;
	e->classname = "noclass";
	e->gravity = 1.0;
	e->s.number = e - g_entities;

	// do this before calling the spawn function so it can be overridden.
	e->gravityVector = { 0.0, 0.0, -1.0 };
}

/*
=================
G_Spawn

Either finds a free entity, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
gentity_t *G_Spawn() {
	size_t i;
	const size_t first_spawn_entity = min(static_cast<size_t>(game.maxclients) + 1, static_cast<size_t>(game.maxentities));
	const size_t entity_limit = G_EntitySearchLimit();
	const bool loading_entities = globals.server_flags & SERVER_FLAG_LOADING;

	for (i = first_spawn_entity; i < entity_limit; i++) {
		gentity_t *e = &g_entities[i];
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy. A saved
		// entity-lump reload has the same allocation pattern at an arbitrary
		// level time and must also be able to recycle inhibited definitions.
		if (!e->inuse &&
			(loading_entities || e->freetime < 2_sec ||
				level.time - e->freetime > 500_ms)) {
			G_InitGentity(e);
			return e;
		}
	}

	i = max(i, first_spawn_entity);
	if (i >= static_cast<size_t>(game.maxentities))
		gi.Com_ErrorFmt("{}: no free entities.", __FUNCTION__);

	gentity_t *e = &g_entities[i];
	globals.num_entities = static_cast<uint32_t>(i + 1);
	G_InitGentity(e);
	//gi.Com_PrintFmt("{}: total:{}\n", __FUNCTION__, i);
	return e;
}

/*
=================
G_FreeEntity

Marks the entity as free
=================
*/
THINK(G_FreeEntity) (gentity_t *ed) -> void {
	// already freed
	if (!ed || !ed->inuse)
		return;

	gi.unlinkentity(ed); // unlink from world

	if ((ed - g_entities) <= (ptrdiff_t)(game.maxclients + BODY_QUEUE_SIZE)) {
#ifdef _DEBUG
		gi.Com_Print("Tried to free special entity.\n");
#endif
		return;
	}
	const bool freeing_door = ed->classname &&
		(!strcmp(ed->classname, "func_door") ||
		 !strcmp(ed->classname, "func_door_rotating") ||
		 !strcmp(ed->classname, "func_door_secret") ||
		 !strcmp(ed->classname, "func_water"));
	gentity_t *const possible_door_successor = freeing_door &&
		ed->teammaster == ed && (ed->flags & FL_TEAMMASTER)
		? ed->teamchain : nullptr;
	const int32_t possible_door_successor_generation = possible_door_successor
		? possible_door_successor->spawn_count : 0;
	if (ed->classname && !strcmp(ed->classname, "spawngro")) {
		gentity_t *const beam = ed->target_ent;
		const bool live_beam = beam && beam->inuse && beam->classname &&
			!strcmp(beam->classname, "spawngro_beam") &&
			(!ed->target_ent_generation ||
				beam->spawn_count == ed->target_ent_generation) &&
			beam->owner == ed;
		ed->target_ent = nullptr;
		ed->target_ent_generation = 0;
		if (live_beam) {
			beam->owner = nullptr;
			G_FreeEntity(beam);
		}
	}
	//gi.Com_PrintFmt("{}: removing {}\n", __FUNCTION__, *ed);
	// [MuffMode] Freeing any team member, not only a killtarget, must leave a
	// reciprocal chain. Turrets also quiesce their persistent mover graph first.
	G_PrepareDoppelEntityFree(ed);
	G_PrepareSphereEntityFree(ed);
	G_TurretPrepareEntityFree(ed);
	G_RemoveEntityFromTeam(ed);

	// [MuffMode] Persistent map helpers can retain these aliases long after the
	// callback that assigned them. Retire reverse references before the slot can
	// be recycled so a timer, button, platform, or door trigger cannot adopt an
	// unrelated replacement entity. Client slots use the disconnect counterpart
	// in MM_ClearDepartingClientReferences because they are not freed here.
	const bool freeing_platform = ed->classname &&
		(!strcmp(ed->classname, "func_plat") ||
		 !strcmp(ed->classname, "func_plat2"));
	const size_t entity_limit = G_EntitySearchLimit();
	for (size_t index = 0; index < entity_limit; ++index) {
		gentity_t *const candidate = &g_entities[index];
		if (!candidate->inuse || candidate == ed)
			continue;
		if (candidate->activator == ed)
			candidate->activator = world;
		if (freeing_door && candidate->owner == ed &&
			candidate->solid == SOLID_TRIGGER) {
			const bool successor_promoted = possible_door_successor &&
				possible_door_successor->inuse &&
				possible_door_successor->spawn_count ==
					possible_door_successor_generation &&
				possible_door_successor->teammaster == possible_door_successor &&
				(possible_door_successor->flags & FL_TEAMMASTER);
			candidate->owner = successor_promoted
				? possible_door_successor : nullptr;
			candidate->count = successor_promoted
				? possible_door_successor_generation : 0;
		}
		if (freeing_platform && candidate->enemy == ed &&
			candidate->solid == SOLID_TRIGGER) {
			candidate->enemy = nullptr;
		}
		const bool tracked_target_ent = candidate->classname &&
			(!strcmp(candidate->classname, "func_train") ||
			 !strcmp(candidate->classname, "target_teleporter") ||
			 !strcmp(candidate->classname, "spawngro") ||
			 (ed->classname &&
				!strcmp(ed->classname, "turret_lasersight")));
		if (candidate->target_ent == ed && tracked_target_ent) {
			candidate->target_ent = nullptr;
			candidate->target_ent_generation = 0;
		}
	}

	gi.Bot_UnRegisterEntity(ed);

	const int32_t id = MM_NextEntityGeneration(ed->spawn_count);
	memset(ed, 0, sizeof(*ed)); // NOLINT(bugprone-undefined-memory-manipulation): engine-owned gentity_t slots are intentionally raw C ABI storage.
	ed->s.number = ed - g_entities;
	ed->classname = "freed";
	ed->freetime = level.time;
	ed->inuse = false;
	ed->spawn_count = id;
	ed->sv.init = false;
}

BoxEntitiesResult_t G_TouchTriggers_BoxFilter(gentity_t *hit, void *) {
	if (!hit->touch)
		return BoxEntitiesResult_t::Skip;

	return BoxEntitiesResult_t::Keep;
}

/*
============
G_TouchTriggers

============
*/
void G_TouchTriggers(gentity_t *ent) {
	MM_PROFILE_ZONE("G_TouchTriggers");
	MM_PROFILE_INC(trigger_touch_calls);
	if (!ent || !ent->inuse)
		return;
	const int32_t ent_generation = ent->spawn_count;

	int				num;
	static gentity_t	*touch[MAX_ENTITIES];
	struct trigger_reference_t {
		gentity_t *entity;
		int32_t generation;
	};

	// Dead or eliminated things don't activate triggers. Arena lobby/observer
	// freecams are the exception: RA2/RA3 selector pads are teleport triggers,
	// and the dispatch loop below already restricts freecams to teleport
	// classnames. Fighter roles stay on the normal dead/eliminated gate.
	const bool arena_selector_observer = GT(GT_ARENA) && ent->client &&
		ent->movetype == MOVETYPE_FREECAM &&
		!MM_Arena_IsFighter(ent->client);
	if (!arena_selector_observer &&
		((ent->client && ent->client->eliminated) ||
		 ((ent->client || (ent->svflags & SVF_MONSTER)) && ent->health <= 0)))
		return;

	// [MuffMode] Freeze Tag: a shoved frozen body is an inert prop. It never
	// reached this function before bodies could move; letting it in now would
	// have bodies opening doors, riding jump pads and taking teleporters.
	if (MM_FreezeTag_IsFrozen(ent))
		return;

	num = gi.BoxEntities(ent->absmin, ent->absmax, touch, MAX_ENTITIES, AREA_TRIGGERS, G_TouchTriggers_BoxFilter, nullptr);
	MM_PROFILE_ADD(trigger_box_entities, num);
	// BoxEntities scratch can be overwritten by a nested trigger scan. Copy the
	// result identities before dispatching any re-entrant callbacks. Size the
	// snapshot to the actual result so a normal touch does not reserve more than
	// 128 KiB of the game thread's stack.
	const size_t reference_count = num > 0
		? min(static_cast<size_t>(num), static_cast<size_t>(MAX_ENTITIES)) : 0;
	std::vector<trigger_reference_t> references;
	references.reserve(reference_count);
	for (size_t i = 0; i < reference_count; ++i)
		references.push_back({ touch[i], touch[i] ? touch[i]->spawn_count : 0 });

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for (const trigger_reference_t &reference : references) {
		gentity_t *const hit = reference.entity;
		if (!hit || !hit->inuse ||
			hit->spawn_count != reference.generation)
			continue;
		if (!hit->touch)
			continue;
		if (GT(GT_ARENA) && !MM_Arena_CanUseEntity(ent, hit))
			continue;
		if (ent->movetype == MOVETYPE_FREECAM)
			if (!strstr(hit->classname, "teleport"))
				continue;

		MM_PROFILE_INC(trigger_touch_dispatches);
		hit->touch(hit, ent, null_trace, true);
		if (!ent->inuse || ent->spawn_count != ent_generation)
			return;
	}
}

// [Paril-KEX] scan for projectiles between our movement positions
// to see if we need to collide against them
bool G_TouchProjectiles(gentity_t *ent, vec3_t previous_origin) {
	MM_PROFILE_ZONE("G_TouchProjectiles");
	MM_PROFILE_INC(projectile_touch_calls);
	if (!ent || !ent->inuse)
		return false;
	const int32_t ent_generation = ent->spawn_count;

	struct skipped_projectile {
		gentity_t *projectile;
		int32_t		spawn_count;
	};
	std::vector<skipped_projectile> skipped;
	skipped.reserve(16);
	struct skipped_projectile_restore_t {
		std::vector<skipped_projectile> &entries;

		void restore_now() noexcept {
			for (const skipped_projectile &entry : entries) {
				if (entry.projectile->inuse &&
					entry.projectile->spawn_count == entry.spawn_count) {
					entry.projectile->svflags |= SVF_PROJECTILE;
				}
			}
			entries.clear();
		}

		~skipped_projectile_restore_t() noexcept {
			restore_now();
		}
	} restore_skipped { skipped };
	bool entity_current = true;

	while (true) {
		if (!ent->inuse || ent->spawn_count != ent_generation) {
			entity_current = false;
			break;
		}
		MM_PROFILE_INC(projectile_traces);
		trace_t tr = gi.trace(previous_origin, ent->mins, ent->maxs, ent->s.origin, ent, ent->clipmask | CONTENTS_PROJECTILE);

		if (tr.fraction == 1.0f)
			break;
		else if (!(tr.ent->svflags & SVF_PROJECTILE))
			break;

		// always skip this projectile since certain conditions may cause the projectile
		// to not disappear immediately
		if (skipped.size() == static_cast<size_t>(MAX_ENTITIES)) {
			MM_PROFILE_INC(projectile_skip_overflows);
			break;
		}

		// Record before mutating the entity so allocation failure cannot strand a
		// projectile outside the collision set.
		skipped.push_back({ tr.ent, tr.ent->spawn_count });
		tr.ent->svflags &= ~SVF_PROJECTILE;
		MM_PROFILE_INC(projectile_skipped);

		if (GT(GT_ARENA) && !MM_Arena_CanInteract(ent, tr.ent))
			continue;

		// if we're both players and it's coop, allow the projectile to "pass" through
		gentity_t *const projectile_owner = MM_ResolveOrdnanceOwner(tr.ent);
		if (ent->client && projectile_owner && projectile_owner->client &&
			!G_ShouldPlayersCollide(true))
			continue;

		MM_PROFILE_INC(projectile_impacts);
		if (!G_Impact(ent, tr)) {
			entity_current = false;
			break;
		}
	}

	MM_PROFILE_MAX(projectile_max_skipped_per_call, skipped.size());
	restore_skipped.restore_now();
	return entity_current && ent->inuse && ent->spawn_count == ent_generation;
}

/*
==============================================================================

Kill box

==============================================================================
*/

/*
=================
KillBox

Kills all entities that would touch the proposed new positioning
of ent.
=================
*/

BoxEntitiesResult_t KillBox_BoxFilter(gentity_t *hit, void *) {
	if (!hit->solid || !hit->takedamage || hit->solid == SOLID_TRIGGER)
		return BoxEntitiesResult_t::Skip;

	return BoxEntitiesResult_t::Keep;
}

bool KillBox(gentity_t *ent, bool from_spawning, mod_id_t mod, bool bsp_clipping) {
	MM_PROFILE_ZONE("KillBox");
	MM_PROFILE_INC(killbox_calls);
	if (!ent || !ent->inuse)
		return false;
	const int32_t ent_generation = ent->spawn_count;

	// don't telefrag as spectator or noclip player...
	if (ent->movetype == MOVETYPE_NOCLIP || ent->movetype == MOVETYPE_FREECAM)
		return true;

	// [MuffMode] Freeze Tag: a frozen body must never be the telefragger. It is
	// immune to KillBox damage itself, so a body coming to rest on a spawn point
	// would otherwise freeze whoever lands there and score for its owner.
	if (MM_FreezeTag_IsFrozen(ent))
		return true;

	contents_t mask = CONTENTS_MONSTER | CONTENTS_PLAYER;

	// [Paril-KEX] don't gib other players in coop if we're not colliding
	if (from_spawning && ent->client && InCoopStyle() && !G_ShouldPlayersCollide(false))
		mask &= ~CONTENTS_PLAYER;

	std::vector<gentity_t *> touch(MAX_ENTITIES);

	const int num = gi.BoxEntities(ent->absmin, ent->absmax, touch.data(),
		static_cast<int>(touch.size()), AREA_SOLID, KillBox_BoxFilter, nullptr);
	MM_PROFILE_ADD(killbox_box_entities, num);
	struct killbox_candidate_t {
		gentity_t *entity;
		int32_t generation;
		bool inuse;
	};
	const size_t candidate_count = num > 0
		? min(static_cast<size_t>(num), touch.size()) : 0;
	std::vector<killbox_candidate_t> candidates;
	candidates.reserve(candidate_count);
	for (size_t index = 0; index < candidate_count; ++index) {
		gentity_t *const hit = touch[index];
		candidates.push_back({ hit, hit ? hit->spawn_count : 0,
			hit && hit->inuse });
	}

	if (num > 0)
		MuffModeLog("TELEFRAG", "KillBox: %d entities in box (spawner=%s, from_spawning=%d, mod=%d)",
			num,
			(ent->client && ent->client->pers.netname[0]) ? ent->client->pers.netname : ent->classname ? ent->classname : "?",
			(int)from_spawning, (int)mod);

	for (const killbox_candidate_t &candidate : candidates) {
		if (!ent->inuse || ent->spawn_count != ent_generation)
			return false;
		gentity_t *const hit = candidate.entity;
		if (!candidate.inuse || !hit || !hit->inuse ||
			hit->spawn_count != candidate.generation)
			continue;

		if (hit == ent)
			continue;
		else if (!hit->takedamage || !hit->solid || hit->solid == SOLID_TRIGGER || hit->solid == SOLID_BSP)
			continue;
		else if (hit->client && !(mask & CONTENTS_PLAYER))
			continue;
		else if (GT(GT_ARENA) && !MM_Arena_CanInteract(ent, hit))
			continue;

		if ((ent->solid == SOLID_BSP || (ent->svflags & SVF_HULL)) && bsp_clipping) {
			trace_t clip = gi.clip(ent, hit->s.origin, hit->mins, hit->maxs, hit->s.origin, G_GetClipMask(hit));

			if (clip.fraction == 1.0f)
				continue;
		}

		// [Paril-KEX] don't allow telefragging of friends in coop.
		// the player that is about to be telefragged will have collision
		// disabled until another time.
		if (ent->client && hit->client &&
			(InCoopStyle() ||
				(from_spawning && GT(GT_ARENA) &&
				 mod == MOD_TELEFRAG_SPAWN))) {
			hit->clipmask &= ~CONTENTS_PLAYER;
			ent->clipmask &= ~CONTENTS_PLAYER;
			continue;
		}

		if (hit->client)
			MuffModeLog("TELEFRAG", "KillBox: '%s' telefragging '%s' (mod=%d, from_spawning=%d)",
				(ent->client && ent->client->pers.netname[0]) ? ent->client->pers.netname : "?",
				hit->client->pers.netname,
				(int)mod, (int)from_spawning);

		MM_PROFILE_INC(killbox_damage_events);
		T_Damage(hit, ent, ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, mod);
		if (!ent->inuse || ent->spawn_count != ent_generation)
			return false;
	}

	return ent->inuse && ent->spawn_count == ent_generation; // all clear
}

/*--------------------------------------------------------------------------*/

const char *Teams_TeamName(team_t team) {
	switch (team) {
	case TEAM_RED:
		return "RED";
	case TEAM_BLUE:
		return "BLUE";
	case TEAM_SPECTATOR:
		return "SPECTATOR";
	case TEAM_FREE:
		return "FREE";
	}
	return "NONE";
}

const char *Teams_OtherTeamName(team_t team) {
	switch (team) {
	case TEAM_RED:
		return "BLUE";
	case TEAM_BLUE:
		return "RED";
	}
	return "UNKNOWN";
}

team_t Teams_OtherTeam(team_t team) {
	switch (team) {
	case TEAM_RED:
		return TEAM_BLUE;
	case TEAM_BLUE:
		return TEAM_RED;
	}
	return TEAM_SPECTATOR; // invalid value
}

constexpr const char *TEAM_RED_SKIN = "ctf_r";
constexpr const char *TEAM_BLUE_SKIN = "ctf_b";

/*
=================
G_AssignPlayerSkin
=================
*/
std::string_view G_FormatPlayerSkinConfigString(gentity_t *ent, const char *skin)
{
	std::string_view model_skin(skin);

	if (size_t i = model_skin.find_first_of('/'); i != std::string_view::npos)
		model_skin = model_skin.substr(0, i + 1);
	else
		model_skin = "male/";

	switch (ent->client->sess.team) {
	case TEAM_RED:
		if (g_team_force_models->integer && *g_team_red_model->string)
			return G_Fmt("{}\\{}\\default",
				ent->client->resp.netname, g_team_red_model->string);
		return G_Fmt("{}\\{}{}\\default",
			ent->client->resp.netname, model_skin, TEAM_RED_SKIN);
	case TEAM_BLUE:
		if (g_team_force_models->integer && *g_team_blue_model->string)
			return G_Fmt("{}\\{}\\default",
				ent->client->resp.netname, g_team_blue_model->string);
		return G_Fmt("{}\\{}{}\\default",
			ent->client->resp.netname, model_skin, TEAM_BLUE_SKIN);
	default:
		return G_Fmt("{}\\{}\\default",
			ent->client->resp.netname, skin);
	}
}

void G_AssignPlayerSkin(gentity_t *ent, const char *s, bool refresh_overrides) {
	const int playernum = ent - g_entities - 1;
	const std::string_view value = G_FormatPlayerSkinConfigString(ent, s);

	gi.configstring(CS_PLAYERSKINS + playernum, value.data());

	// [MuffMode] The canonical broadcast above clobbers any per-viewer skin
	// override of this player, and a team change here can flip enemy/teammate
	// relationships, so re-send overrides both for this player as a target and
	// as a viewer.
	if (refresh_overrides) {
		MM_RefreshSkinOverridesForTarget(ent);
		MM_RefreshSkinOverridesForViewer(ent);
	}

	//	gi.LocClient_Print(ent, PRINT_HIGH, "$g_assigned_team", ent->client->resp.netname);
}

/*
===================
G_AdjustPlayerScore
===================
*/
void G_AdjustPlayerScore(gclient_t *cl, int32_t offset, bool adjust_team, int32_t team_offset) {
	if (!cl) return;

	// [MuffMode] Multi-arena rounds own their match/combat state independently;
	// the singleton level match state deliberately never enters MATCH_IN_PROGRESS.
	if (GT(GT_ARENA)) {
		const ptrdiff_t client_num = game.clients ? cl - game.clients : -1;
		if (client_num < 0 ||
			client_num >= static_cast<ptrdiff_t>(game.maxclients))
			return;
		const gentity_t *ent = &g_entities[client_num + 1];
		const int arena_id = MM_Arena_Id(ent);
		if (!MM_Arena_IsFighter(cl) || arena_id <= 0 ||
			!MM_Arena_IsRunning(arena_id))
			return;
	} else if (IsScoringDisabled()) {
		return;
	}

	if (level.intermission_queued)
		return;

	if (offset || team_offset) {
		// [MuffMode] Horde exposes configurable score bonuses and can run endlessly;
		// keep a bad cvar or very long session from overflowing signed score state.
		const int64_t score = static_cast<int64_t>(cl->resp.score) + offset;
		cl->resp.score = static_cast<int32_t>(std::clamp(score,
			static_cast<int64_t>(std::numeric_limits<int32_t>::min()),
			static_cast<int64_t>(std::numeric_limits<int32_t>::max())));
		// CalculateRanks has a room-filtered branch for multi-arena play.
		CalculateRanks();
	}

	if (adjust_team && team_offset)
		G_AdjustTeamScore(cl->sess.team, team_offset);
}

/*
===================
G_SetPlayerScore
===================
*/
void G_SetPlayerScore(gclient_t *cl, int32_t value) {
	if (!cl) return;

	if (IsScoringDisabled())
		return;

	if (level.intermission_queued)
		return;

	cl->resp.score = value;
	CalculateRanks();
}


/*
===================
G_AdjustTeamScore
===================
*/
void G_AdjustTeamScore(team_t team, int32_t offset) {
	if (IsScoringDisabled())
		return;

	if (level.intermission_queued)
		return;

	if (!Teams() || GT(GT_RR))
		return;

	if (team == TEAM_RED)
		level.team_scores[TEAM_RED] += offset;
	else if (team == TEAM_BLUE)
		level.team_scores[TEAM_BLUE] += offset;
	else return;
	CalculateRanks();
}

/*
===================
G_SetTeamScore
===================
*/
void G_SetTeamScore(team_t team, int32_t value) {
	if (IsScoringDisabled())
		return;

	if (level.intermission_queued)
		return;

	if (!Teams() || GT(GT_RR))
		return;

	if (team == TEAM_RED)
		level.team_scores[TEAM_RED] = value;
	else if (team == TEAM_BLUE)
		level.team_scores[TEAM_BLUE] = value;
	else return;
	CalculateRanks();
}

/*
===================
G_PlaceString

Adapted from Quake III
===================
*/
const char *G_PlaceString(int rank) {
	static char	str[64];
	const char *s, *t;

	if (rank & RANK_TIED_FLAG) {
		rank &= ~RANK_TIED_FLAG;
		t = "Tied for ";
	} else {
		t = "";
	}

	if (rank == 1) {
		s = "1st";
	} else if (rank == 2) {
		s = "2nd";
	} else if (rank == 3) {
		s = "3rd";
	} else if (rank == 11) {
		s = "11th";
	} else if (rank == 12) {
		s = "12th";
	} else if (rank == 13) {
		s = "13th";
	} else if (rank % 10 == 1) {
		s = G_Fmt("{}st", rank).data();
	} else if (rank % 10 == 2) {
		s = G_Fmt("{}nd", rank).data();
	} else if (rank % 10 == 3) {
		s = G_Fmt("{}rd", rank).data();
	} else {
		s = G_Fmt("{}th", rank).data();
	}
	Q_strlcpy(str, G_Fmt("{}{}", t, s).data(), sizeof(str));
	return str;
}

bool ItemSpawnsEnabled() {
	if (g_no_items->integer)
		return false;
	if (g_instagib->integer || g_nadefest->integer)
		return false;
	if (GTF(GTF_ARENA))
		return false;
	return true;
}


static void loc_buildboxpoints(vec3_t(&p)[8], const vec3_t &org, const vec3_t &mins, const vec3_t &maxs) {
	p[0] = org + mins;
	p[1] = p[0];
	p[1][0] -= mins[0];
	p[2] = p[0];
	p[2][1] -= mins[1];
	p[3] = p[0];
	p[3][0] -= mins[0];
	p[3][1] -= mins[1];
	p[4] = org + maxs;
	p[5] = p[4];
	p[5][0] -= maxs[0];
	p[6] = p[0];
	p[6][1] -= maxs[1];
	p[7] = p[0];
	p[7][0] -= maxs[0];
	p[7][1] -= maxs[1];
}

bool loc_CanSee(gentity_t *targ, gentity_t *inflictor) {
	trace_t trace;
	vec3_t	targpoints[8];
	int		i;
	vec3_t	viewpoint;

	// bmodels need special checking because their origin is 0,0,0
	if (targ->movetype == MOVETYPE_PUSH)
		return false; // bmodels not supported

	loc_buildboxpoints(targpoints, targ->s.origin, targ->mins, targ->maxs);

	viewpoint = inflictor->s.origin;
	viewpoint[2] += inflictor->viewheight;

	for (i = 0; i < 8; i++) {
		trace = gi.traceline(viewpoint, targpoints[i], inflictor, CONTENTS_MIST|MASK_WATER|MASK_SOLID);
		if (trace.fraction == 1.0f)
			return true;
	}

	return false;
}

// [MuffMode] Teams() is now inline in g_local.h -- it is a single masked load
// of the published gametype resolution.

/*
=============
P_EngineTeamIndex

Map internal sess.team to full engine team identifiers used by
player_state.team_id and sv.team: 1 = red, 2 = blue, 0 = none. The legacy
four-bit skinnum projection is bounded separately at publication time.
=============
*/
uint8_t P_EngineTeamIndex(team_t team) {
	switch (team) {
	case TEAM_RED:
		return 1;
	case TEAM_BLUE:
		return 2;
	default:
		return 0;
	}
}

/*
=============
G_TimeString

Format a match timer string with minute precision.
=============
*/
const char *G_TimeString(const int msec, bool state) {
	static char buffer[32];
	if (state) {
		if (level.match_state < match_state_t::MATCH_COUNTDOWN)
			return "WARMUP";

		if (level.intermission_queued || level.intermission_time)
			return "MATCH END";
	}

	Q_strlcpy(buffer, MM_FormatMatchTime(msec).c_str(), sizeof(buffer));
	return buffer;
}
/*
=============
G_TimeStringMs

Format a match timer string with millisecond precision.
=============
*/
const char *G_TimeStringMs(const int msec, bool state) {
	static char buffer[32];
	if (state) {
		if (level.match_state < match_state_t::MATCH_COUNTDOWN)
			return "WARMUP";

		if (level.intermission_queued || level.intermission_time)
			return "MATCH END";
	}

	Q_strlcpy(buffer, MM_FormatMatchTimeMs(msec).c_str(), sizeof(buffer));
	return buffer;
}

team_t StringToTeamNum(const char *in) {
	if (!Q_strcasecmp(in, "spectator") || !Q_strcasecmp(in, "s")) {
		return TEAM_SPECTATOR;
	} else if (!Q_strcasecmp(in, "auto") || !Q_strcasecmp(in, "a")) {
		return PickTeam(-1);
	} else if (Teams()) {
		if (!Q_strcasecmp(in, "blue") || !Q_strcasecmp(in, "b"))
			return TEAM_BLUE;
		else if (!Q_strcasecmp(in, "red") || !Q_strcasecmp(in, "r"))
			return TEAM_RED;
	} else {
		if (!Q_strcasecmp(in, "free") || !Q_strcasecmp(in, "f"))
			return TEAM_FREE;
	}
	return TEAM_NONE;
}

bool InAMatch() {
	if (!deathmatch->integer)
		return false;
	if (level.intermission_queued)
		return false;
	if (level.match_state == match_state_t::MATCH_IN_PROGRESS)
		return true;

	return false;
}

bool IsCombatDisabled() {
	if (!deathmatch->integer)
		return false;
	if (level.intermission_queued)
		return true;
	if (level.intermission_time)
		return true;
	if (level.match_state == match_state_t::MATCH_COUNTDOWN)
		return true;
	if (GTF(GTF_ROUNDS) && level.match_state == match_state_t::MATCH_IN_PROGRESS) {
		// added round ended to allow gibbing etc. at end of rounds
		// scoring to be explicitly disabled during this time
		if (level.round_state == round_state_t::ROUND_COUNTDOWN && (notGT(GT_HORDE)))
			return true;
	}
	return false;
}

bool IsPickupsDisabled() {
	if (!deathmatch->integer)
		return false;
	if (level.intermission_queued)
		return true;
	if (level.intermission_time)
		return true;
	if (level.match_state == match_state_t::MATCH_COUNTDOWN)
		return true;
	return false;
}

bool IsScoringDisabled() {
	if (level.match_state != match_state_t::MATCH_IN_PROGRESS)
		return true;
	if (IsCombatDisabled())
		return true;
	if (GTF(GTF_ROUNDS) && level.round_state != round_state_t::ROUND_IN_PROGRESS)
		return true;
	return false;
}

// [MuffMode] Resolves against the descriptor table; still accepts either the
// short or the long name and still reports GT_NONE for anything unrecognised.
gametype_t GT_IndexFromString(const char *in) {
	if (!in)
		return gametype_t::GT_NONE;
	const mm_gt_lookup_t found = MM_GTFindByAnyName(in);
	return found.found ? found.gt : gametype_t::GT_NONE;
}

void TeleportPlayerToRandomSpawnPoint(gentity_t *ent, bool fx) {
	bool	valid_spawn = false;
	vec3_t	spawn_origin, spawn_angles;
	bool	is_landmark = false;

	valid_spawn = SelectSpawnPoint(ent, spawn_origin, spawn_angles, true, is_landmark);

	if (!valid_spawn)
		return;

	TeleportPlayer(ent, spawn_origin, spawn_angles);

	ent->s.event = fx ? EV_PLAYER_TELEPORT : EV_OTHER_TELEPORT;
	//other->s.event = fx ? EV_PLAYER_TELEPORT : EV_OTHER_TELEPORT;
}

// [MuffMode] InCoopStyle() is now inline in g_local.h.

/*
=============
ClientEntFromString

Resolve a client entity from a name or validated numeric identifier string.
=============
*/
gentity_t *ClientEntFromString(const char *in) {
	if (!in || !*in)
		return nullptr;

	for (auto ec : active_clients())
		if (!strcmp(in, ec->client->resp.netname))
			return ec;

	if (*in == '-' || *in == '+')
		return nullptr;

	const auto num = MM_ParseUInt32Arg(in);
	if (!num || game.maxclients <= 0 || *num >= static_cast<uint32_t>(game.maxclients))
		return nullptr;

	return &g_entities[static_cast<size_t>(*num) + 1];
}

/*
=================
RS_IndexFromString
=================
*/
ruleset_t RS_IndexFromString(const char *in) {
	for (size_t i = 1; i < (int)RS_NUM_RULESETS; i++) {
		if (!strcmp(in, rs_short_name[i]))
			return (ruleset_t)i;
		if (!strcmp(in, rs_long_name[i]))
			return (ruleset_t)i;
	}
	return ruleset_t::RS_NONE;
}

void TeleporterVelocity(gentity_t *ent, gvec3_t angles) {
	if (g_teleporter_freeze->integer) {
		// clear the velocity and hold them in place briefly
		ent->velocity = {};
		ent->client->ps.pmove.pm_time = 160; // hold time
		ent->client->ps.pmove.pm_flags |= PMF_TIME_TELEPORT;
	} else {
		// preserve velocity and 'spit' them out of destination
		float len = ent->velocity.length();

		ent->velocity[2] = 0;
		AngleVectors(angles, ent->velocity, NULL, NULL);
		ent->velocity *= len;
	}
}

static bool MS_Validation(gclient_t *cl, mstats_t index, bool write) {
	if (!cl)
		return false;

	if (index <= MSTAT_NONE || index >= MSTAT_TOTAL) {
		gi.Com_PrintFmt("invalid match stat index: {}\n", static_cast<int>(index));
		return false;
	}

	bool active_match = level.match_state == match_state_t::MATCH_IN_PROGRESS;
	if (GT(GT_ARENA)) {
		const ptrdiff_t client_num = cl - game.clients;
		if (client_num < 0 ||
			client_num >= static_cast<ptrdiff_t>(game.maxclients))
			return false;
		const gentity_t *ent = &g_entities[client_num + 1];
		const int arena_id = MM_Arena_Id(ent);
		active_match = MM_Arena_IsFighter(cl) && arena_id > 0 &&
			(write ? MM_Arena_IsRunning(arena_id)
				: MM_Arena_SeriesActive(ent));
	}

	if (!g_matchstats->integer || !active_match)
		return false;

	if (cl->sess.is_a_bot)
		return false;

	return true;
}

int MS_Value(gclient_t *cl, mstats_t index) {
	if (!MS_Validation(cl, index, false))
		return 0;

	return cl->resp.mstats[index];
}

void MS_Adjust(gclient_t *cl, mstats_t index, int count) {
	if (!MS_Validation(cl, index, true))
		return;

	cl->resp.mstats[index] += count;
}

void MS_AdjustDuo(gclient_t *cl, mstats_t index1, mstats_t index2, int count) {
	if (!MS_Validation(cl, index1, true))
		return;

	cl->resp.mstats[index1] += count;
	cl->resp.mstats[index2] += count;
}

void MS_Set(gclient_t *cl, mstats_t index, int value) {
	if (!MS_Validation(cl, index, true))
		return;

	cl->resp.mstats[index] = value;
}

/*
=============
stime

Return a stable timestamp string for file naming.
=============
*/
const char *stime() {
	std::tm local_time = {};
	std::time_t now = {};
	static char buffer[32];

	std::time(&now);
	if (!muffmode::LocalTimeSnapshot(now, local_time)) {
		buffer[0] = '\0';
		return buffer;
	}

	G_FmtTo(buffer, "{}{:02}{:02}{:02}{:02}{:02}",
		1900 + local_time.tm_year, local_time.tm_mon + 1, local_time.tm_mday,
		local_time.tm_hour, local_time.tm_min, local_time.tm_sec);

	return buffer;
}

void AnnouncerSound(gentity_t *ent, const char *announcer_sound, const char *backup_sound, bool use_backup, bool backup_alongside_vo) {
	const bool has_stem = announcer_sound && announcer_sound[0];
	const bool has_backup = backup_sound && backup_sound[0];

	for (auto ec : active_clients()) {
		if (!ec->inuse || !ec->client || !ec->client->pers.connected)
			continue;
		if (ent == world || ent == ec || (!ClientIsPlaying(ec->client) && ec->client->follow_target == ent)) {
			if (ec->client->sess.is_a_bot)
				continue;

			const announce_action_t action = MM_AnnounceDecision(
				ec->client->sess.pc.announcer_enabled, has_stem, use_backup, has_backup, backup_alongside_vo);

			if (action.play_sting)
				gi.local_sound(ec, CHAN_RELIABLE | CHAN_AUTO, gi.soundindex(backup_sound), 1, ATTN_NONE, 0);
			if (action.play_backup)
				gi.local_sound(ec, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex(backup_sound), 1, ATTN_NONE, 0);
			if (action.play_vo)
				gi.local_sound(ec, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex(G_Fmt("vo_evil/{}.wav", announcer_sound).data()), 1, ATTN_NONE, 0);
		}
	}
}


void QLSound(gentity_t *ent, const char *ql_sound, const char *backup_sound, bool use_backup) {
	for (auto ec : active_clients()) {
		if (!ec->inuse || !ec->client || !ec->client->pers.connected)
			continue;
		if (ent == world || ent == ec || (!ClientIsPlaying(ec->client) && ec->client->follow_target == ent)) {
			if (ec->client->sess.is_a_bot)
				continue;
			if (!ec->client->sess.pc.announcer_enabled || (ql_sound == nullptr && use_backup)) {
				if (backup_sound)
					gi.local_sound(ec, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex(backup_sound), 1, ATTN_NONE, 0);
				continue;
			}
			//gi.local_sound(ec, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex(ql_sound), 1, ATTN_NONE, 0);
			
			if (ec->client->sess.pc.announcer_enabled && ql_sound)
				gi.local_sound(ec, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex(G_Fmt("{}.wav", ql_sound).data()), 1, ATTN_NONE, 0);
		}
	}
}

void G_StuffCmd(gentity_t *e, const char *fmt, ...) {
	va_list		argptr;
	char		text[512];

	if (e && !e->client->pers.connected)
		gi.Com_ErrorFmt("{}: Bad client %d for '%s'", __FUNCTION__, (int)(e - g_entities - 1), fmt);

	va_start(argptr, fmt);
	vsnprintf(text, sizeof(text), fmt, argptr);
	va_end(argptr);
	text[sizeof(text) - 1] = 0;

	gi.WriteByte(svc_stufftext);
	gi.WriteString(text);

	if (e)
		gi.unicast(e, true);
	else
		gi.multicast(vec3_origin, MULTICAST_ALL, true);
}
