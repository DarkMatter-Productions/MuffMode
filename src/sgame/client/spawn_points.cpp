// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Player spawn entities and spawn-point selection.
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <utility>
#include <vector>

#include "g_local.h"
#include "entities/teleporter.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_combat_heatmap.h"

static THINK(info_player_start_drop) (gentity_t *self) -> void {
	// allow them to drop
	self->solid = SOLID_TRIGGER;
	self->movetype = MOVETYPE_TOSS;
	self->mins = PLAYER_MINS;
	self->maxs = PLAYER_MAXS;
	gi.linkentity(self);
}

static inline void deathmatch_spawn_flags(gentity_t *self) {
	if (st.nobots)
		self->flags |= FL_NO_BOTS;
	if (st.nohumans)
		self->flags |= FL_NO_HUMANS;
}

static bool IsFiniteVec3(const vec3_t &point) {
	return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

static inline void FixStuckPlayerSpawnPoint(gentity_t *self) {
	vec3_t origin = self->s.origin;
	if (!IsFiniteVec3(origin) || !gi.trace(origin, PLAYER_MINS, PLAYER_MAXS, origin, self, MASK_SOLID).startsolid)
		return;

	const stuck_result_t result = G_FixStuckObject_Generic(origin, PLAYER_MINS, PLAYER_MAXS,
		[self](const vec3_t &start, const vec3_t &mins, const vec3_t &maxs, const vec3_t &end) {
			return gi.trace(start, mins, maxs, end, self, MASK_SOLID);
		});

	if (result != stuck_result_t::NO_GOOD_POSITION)
		self->s.origin = origin;
}

/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
The normal starting point for a level.

"nobots" will prevent bots from using this spot.
"nohumans" will prevent humans from using this spot.
*/
void SP_info_player_start(gentity_t *self) {
	// fix stuck spawn points
	FixStuckPlayerSpawnPoint(self);

	// [Paril-KEX] on n64, since these can spawn riding elevators,
	// allow them to "ride" the elevators so respawning works
	if (level.is_n64) {
		self->think = info_player_start_drop;
		self->nextthink = level.time + FRAME_TIME_S;
	}

	deathmatch_spawn_flags(self);
}

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32) INITIAL x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
A potential spawning position for deathmatch games.

The first time a player enters the game, they will be at an 'INITIAL' spot.
Targets will be fired when someone spawns in on them.
"nobots" will prevent bots from using this spot.
"nohumans" will prevent humans from using this spot.
*/
void SP_info_player_deathmatch(gentity_t *self) {
	if (!deathmatch->integer) {
		G_FreeEntity(self);
		return;
	}

	FixStuckPlayerSpawnPoint(self);
	SP_misc_teleporter_dest(self);

	deathmatch_spawn_flags(self);
}

/*QUAKED info_player_team_red (1 0 0) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
A potential Red Team spawning position for CTF games.

"nobots" will prevent bots from using this spot.
"nohumans" will prevent humans from using this spot.
*/
void SP_info_player_team_red(gentity_t *self) {
	FixStuckPlayerSpawnPoint(self);
	deathmatch_spawn_flags(self);
}

/*QUAKED info_player_team_blue (0 0 1) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
A potential Blue Team spawning position for CTF games.

"nobots" will prevent bots from using this spot.
"nohumans" will prevent humans from using this spot.
*/
void SP_info_player_team_blue(gentity_t *self) {
	FixStuckPlayerSpawnPoint(self);
	deathmatch_spawn_flags(self);
}

/*QUAKED info_player_coop (1 0 1) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
A potential spawning position for coop games.
*/
void SP_info_player_coop(gentity_t *self) {
	if (!coop->integer) {
		G_FreeEntity(self);
		return;
	}

	SP_info_player_start(self);
}

/*QUAKED info_player_coop_lava (1 0 1) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
A potential spawning position for coop games on rmine2 where lava level
needs to be checked.
*/
void SP_info_player_coop_lava(gentity_t *self) {
	if (!coop->integer) {
		G_FreeEntity(self);
		return;
	}

	// fix stuck spawn points
	FixStuckPlayerSpawnPoint(self);
}

/*QUAKED info_player_intermission (1 0 1) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
The deathmatch intermission point will be at one of these
Use 'angles' instead of 'angle', so you can set pitch or roll as well as yaw.  'pitch yaw roll'
*/
void SP_info_player_intermission(gentity_t *ent) {}

/*QUAKED info_ctf_teleport_destination (0.5 0.5 0.5) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Point trigger_teleports at these.
*/
void SP_info_ctf_teleport_destination(gentity_t *ent) {
	ent->s.origin[2] += 16;
}

// [Paril-KEX] whether instanced items should be used or not
bool P_UseCoopInstancedItems() {
	// squad respawn forces instanced items on, since we don't
	// want players to need to backtrack just to get their stuff.
	return g_coop_instanced_items->integer || g_coop_squad_respawn->integer;
}

/*
=======================================================================

  SelectSpawnPoint

=======================================================================
*/

/*
================
PlayersRangeFromSpot

Returns the distance to the nearest player from the given spot
muffmode: excludes current client
================
*/
static float PlayersRangeFromSpot(gentity_t *ent, gentity_t *spot) {
	if (!spot || !IsFiniteVec3(spot->s.origin))
		return 0.0f;

	float bestplayerdistance = 9999999.0f;

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client) || ec->health <= 0 || ec->client->eliminated ||
			ec->client->awaiting_respawn || ec->solid == SOLID_NOT || ec->movetype == MOVETYPE_FREECAM)
			continue;
		if (ent && ec == ent)
			continue;
		if (GT(GT_ARENA) && ent && !MM_Arena_SameArena(ent, ec))
			continue;

		const vec3_t v = spot->s.origin - ec->s.origin;
		const float playerdistance = v.length();

		if (playerdistance < bestplayerdistance)
			bestplayerdistance = playerdistance;
	}

	return bestplayerdistance;
}

static gentity_t *UnsafeSpawnPosition(vec3_t spot, bool check_players, const gentity_t *ignore = nullptr, bool allow_nudge = true) {
	if (!IsFiniteVec3(spot))
		return world;

	contents_t mask = MASK_PLAYERSOLID;

	if (!check_players)
		mask &= ~CONTENTS_PLAYER;

	gentity_t *ignore_ent = const_cast<gentity_t *>(ignore);
	trace_t tr = gi.trace(spot, PLAYER_MINS, PLAYER_MAXS, spot, ignore_ent, mask);

	if (!allow_nudge) {
		if (tr.startsolid || tr.fraction != 1.0f) {
			if (GT(GT_ARENA) && ignore && tr.ent && tr.ent->client &&
				!MM_Arena_CanInteract(ignore, tr.ent))
				return nullptr;
			return tr.ent ? tr.ent : world;
		}
		return nullptr;
	}

	if (tr.startsolid && (!tr.ent || !tr.ent->client)) {
		spot[2] += 1.0f;
		tr = gi.trace(spot, PLAYER_MINS, PLAYER_MAXS, spot, ignore_ent, mask);
	}

	if (tr.startsolid || tr.fraction != 1.0f) {
		if (GT(GT_ARENA) && ignore && tr.ent && tr.ent->client &&
			!MM_Arena_CanInteract(ignore, tr.ent))
			return nullptr;
		return tr.ent ? tr.ent : world;
	}

	return nullptr;
}

static bool SpotIsClearOfSolidsAndPlayers(const vec3_t &spot, bool check_players, const gentity_t *ignore = nullptr, bool allow_nudge = true) {
	return UnsafeSpawnPosition(spot, check_players, ignore, allow_nudge) == nullptr;
}

static float DeathmatchSpawnOriginLift() {
	return (g_dm_spawnpads && g_dm_spawnpads->integer) ? 9.0f : 1.0f;
}

static float DeathmatchSpawnSafetyLift() {
	// PutClientOnSpawnPoint lifts the linked entity one more unit after selection.
	return DeathmatchSpawnOriginLift() + 1.0f;
}

static bool SpotIsSafe(gentity_t *spot, const gentity_t *ent, bool check_players = true) {
	if (!spot || !IsFiniteVec3(spot->s.origin))
		return false;

	const float zlift = deathmatch->integer ? DeathmatchSpawnSafetyLift() : 0.0f;
	const vec3_t point = spot->s.origin + vec3_t{ 0.0f, 0.0f, zlift };
	return SpotIsClearOfSolidsAndPlayers(point, check_players, ent ? ent : spot, false);
}

static bool SpawnPointAllowsClient(const gentity_t *ent, const gentity_t *spot) {
	if (!spot)
		return false;

	if (!ent || !ent->client)
		return true;

	if (GT(GT_ARENA) && !MM_Arena_SpawnAllowed(ent, spot))
		return false;

	const bool is_bot = ent->client->sess.is_a_bot || (ent->svflags & SVF_BOT);
	if (is_bot)
		return !(spot->flags & FL_NO_BOTS);

	return !(spot->flags & FL_NO_HUMANS);
}

static inline bool IsProxMine(const gentity_t *ent) {
	return ent && ent->classname && !strcmp(ent->classname, "prox_mine");
}

static inline bool IsTeslaMine(const gentity_t *ent) {
	return ent && ent->classname && !strncmp(ent->classname, "tesla", 5);
}

static inline bool IsTrap(const gentity_t *ent) {
	return ent && ent->classname && !strncmp(ent->classname, "food_cube_trap", 14);
}

static bool SpawnPointHasNearbyMines(const gentity_t *requester, const vec3_t &origin, float radius) {
	if (!IsFiniteVec3(origin))
		return false;

	gentity_t *ent = nullptr;
	while ((ent = findradius(ent, origin, radius)) != nullptr) {
		if (GT(GT_ARENA) && requester && !MM_Arena_CanInteract(requester, ent))
			continue;
		if (IsProxMine(ent) || IsTeslaMine(ent) || IsTrap(ent))
			return true;
	}

	return false;
}

static inline vec3_t SpawnEye(const vec3_t &origin) {
	return origin + vec3_t{ 0.0f, 0.0f, 16.0f };
}

static bool IsEnemy(const gentity_t *requester, const gentity_t *other) {
	if (!requester || !other || !requester->client || !other->client)
		return true;
	if (GT(GT_ARENA))
		return !MM_Arena_SameTeam(requester, other);
	if (!Teams())
		return true;
	return requester->client->sess.team != other->client->sess.team;
}

static bool AnyDirectEnemyLoS(const gentity_t *requester, const vec3_t &spot, float max_dist) {
	if (!requester || !requester->client || !IsFiniteVec3(spot))
		return false;

	const vec3_t to_check = SpawnEye(spot);

	for (auto ec : active_clients()) {
		if (ec == requester || !ClientIsPlaying(ec->client) || ec->health <= 0 || ec->client->eliminated ||
			ec->client->awaiting_respawn || ec->solid == SOLID_NOT || ec->movetype == MOVETYPE_FREECAM ||
			!IsEnemy(requester, ec))
			continue;
		if (GT(GT_ARENA) && !MM_Arena_SameArena(requester, ec))
			continue;

		const vec3_t from = SpawnEye(ec->s.origin);
		if ((to_check - from).length() > max_dist)
			continue;

		const trace_t tr = gi.trace(from, vec3_origin, vec3_origin, to_check, ec, MASK_SOLID & ~CONTENTS_PLAYER);
		if (tr.fraction == 1.0f)
			return true;
	}

	return false;
}

constexpr spawnflags_t SPAWNFLAG_INITIAL = 1_spawnflag;
constexpr float SPAWN_DEFAULT_MIN_PLAYER_RADIUS = 160.0f;
constexpr float SPAWN_MINE_RADIUS = 196.0f;
constexpr float SPAWN_MAX_LOS_DIST = 2048.0f;
constexpr float SPAWN_SCORE_PLAYER_RANGE = 2048.0f;
constexpr float SPAWN_SCORE_AVOID_RANGE = 1024.0f;

static float SpawnAvoidPointRadius() {
	return std::clamp(g_dm_respawn_point_min_dist ? g_dm_respawn_point_min_dist->value : 0.0f, 0.0f, 4096.0f);
}

static bool SpawnAvoidDebugEnabled() {
	return g_dm_respawn_point_min_dist_debug && g_dm_respawn_point_min_dist_debug->integer;
}

static bool HasSpawnAvoidPoint(const gentity_t *ent, const vec3_t &avoid_point) {
	if (!IsFiniteVec3(avoid_point))
		return false;

	if (ent && ent->client && ent->client->pers.spawned)
		return true;

	return static_cast<bool>(avoid_point);
}

static float SpawnMinPlayerRadius() {
	if (!g_dm_respawn_point_min_dist)
		return SPAWN_DEFAULT_MIN_PLAYER_RADIUS;

	return std::clamp(g_dm_respawn_point_min_dist->value, 0.0f, 4096.0f);
}

static playerspawn_t LegacySpawnMode() {
	return (playerspawn_t)clamp(g_dm_spawn_farthest ? g_dm_spawn_farthest->integer : 1, 0, 3);
}

static bool SpawnPointHasUsableOrigin(const gentity_t *spot) {
	return spot && IsFiniteVec3(spot->s.origin);
}

static float NormalizeDistancePenalty(float distance, float full_penalty_range) {
	if (full_penalty_range <= 0.0f)
		return 0.0f;

	return std::clamp(1.0f - (distance / full_penalty_range), 0.0f, 1.0f);
}

static std::vector<gentity_t *> GatherSpawnPoints(const char *classname) {
	std::vector<gentity_t *> spawns;
	gentity_t *spot = nullptr;
	while ((spot = G_FindByString<&gentity_t::classname>(spot, classname)) != nullptr) {
		if (spot->inuse && SpawnPointHasUsableOrigin(spot))
			spawns.push_back(spot);
	}

	return spawns;
}

static std::vector<gentity_t *> GatherFallbackSpawnPoints() {
	std::vector<gentity_t *> spawns = GatherSpawnPoints("info_player_start");

	std::vector<gentity_t *> coop = GatherSpawnPoints("info_player_coop");
	spawns.insert(spawns.end(), coop.begin(), coop.end());

	std::vector<gentity_t *> lava = GatherSpawnPoints("info_player_coop_lava");
	spawns.insert(spawns.end(), lava.begin(), lava.end());

	return spawns;
}

static std::vector<gentity_t *> FilterInitialSpawns(const std::vector<gentity_t *> &spawns) {
	std::vector<gentity_t *> flagged;
	flagged.reserve(spawns.size());

	for (gentity_t *spot : spawns) {
		if (spot && spot->spawnflags.has(SPAWNFLAG_INITIAL))
			flagged.push_back(spot);
	}

	return flagged.empty() ? spawns : flagged;
}

static std::vector<gentity_t *> FilterFallbackSpawns(const std::vector<gentity_t *> &spawns, const gentity_t *ent, const vec3_t &avoid_point, bool respect_avoid_point, bool force_spawn = false) {
	std::vector<gentity_t *> out;
	out.reserve(spawns.size());

	const float avoid_radius = SpawnAvoidPointRadius();
	for (gentity_t *spot : spawns) {
		if (!spot || !SpawnPointAllowsClient(ent, spot))
			continue;
		if (!SpotIsSafe(spot, ent, !force_spawn))
			continue;
		if (!force_spawn && respect_avoid_point && avoid_radius > 0.0f && (spot->s.origin - avoid_point).length() < avoid_radius) {
			if (SpawnAvoidDebugEnabled())
				gi.Com_PrintFmt("{}: avoiding spawn point near previous spawn\n", *spot);
			continue;
		}

		out.push_back(spot);
	}

	return out;
}

static std::vector<gentity_t *> FilterFallbackSpawnsRelaxingAvoid(const std::vector<gentity_t *> &spawns, const gentity_t *ent, const vec3_t &avoid_point, bool respect_avoid_point, bool force_spawn = false) {
	std::vector<gentity_t *> out = FilterFallbackSpawns(spawns, ent, avoid_point, respect_avoid_point, force_spawn);

	// Reusing a clear previous spawn is better than force-spawning into another player.
	if (out.empty() && respect_avoid_point)
		out = FilterFallbackSpawns(spawns, ent, avoid_point, false, force_spawn);

	return out;
}

static std::vector<gentity_t *> FilterEligibleSpawns(const std::vector<gentity_t *> &spawns, gentity_t *ent, const vec3_t &avoid_point, bool force_spawn, bool respect_avoid_point) {
	std::vector<gentity_t *> out;
	out.reserve(spawns.size());

	const float avoid_radius = SpawnAvoidPointRadius();
	const float min_player_radius = SpawnMinPlayerRadius();

	for (gentity_t *spot : spawns) {
		if (!spot || !SpawnPointAllowsClient(ent, spot))
			continue;
		if (!SpotIsSafe(spot, ent, !force_spawn))
			continue;

		if (!force_spawn) {
			if (respect_avoid_point && avoid_radius > 0.0f && (spot->s.origin - avoid_point).length() < avoid_radius) {
				if (SpawnAvoidDebugEnabled())
					gi.Com_PrintFmt("{}: avoiding spawn point near previous spawn\n", *spot);
				continue;
			}
			if (SpawnPointHasNearbyMines(ent, spot->s.origin, SPAWN_MINE_RADIUS))
				continue;
			if (min_player_radius > 0.0f && PlayersRangeFromSpot(ent, spot) < min_player_radius)
				continue;
			if (AnyDirectEnemyLoS(ent, spot->s.origin, SPAWN_MAX_LOS_DIST))
				continue;
		}

		out.push_back(spot);
	}

	return out;
}

template <typename T>
static T *PickRandomly(const std::vector<T *> &spawns) {
	if (spawns.empty())
		return nullptr;

	return spawns[static_cast<size_t>(irandom(static_cast<int32_t>(spawns.size())))];
}

static float CompositeDangerScore(gentity_t *spot, gentity_t *ent, const vec3_t &avoid_point) {
	if (!SpawnPointHasUsableOrigin(spot))
		return FLT_MAX;

	float heat = muffmode::combat_heatmap::DangerAt(spot->s.origin);
	if (!std::isfinite(heat))
		heat = 0.0f;

	float nearest = std::max(1.0f, PlayersRangeFromSpot(ent, spot));
	if (!std::isfinite(nearest))
		nearest = SPAWN_SCORE_PLAYER_RANGE;

	const float near_penalty = NormalizeDistancePenalty(nearest, SPAWN_SCORE_PLAYER_RANGE);
	const float los_penalty = AnyDirectEnemyLoS(ent, spot->s.origin, SPAWN_MAX_LOS_DIST) ? 1.0f : 0.0f;
	const bool has_avoid_point = HasSpawnAvoidPoint(ent, avoid_point);
	const float avoid_distance = has_avoid_point ? (spot->s.origin - avoid_point).length() : SPAWN_SCORE_AVOID_RANGE;
	const float avoid_penalty = has_avoid_point ? NormalizeDistancePenalty(avoid_distance, SPAWN_SCORE_AVOID_RANGE) : 0.0f;
	const float mine_penalty = SpawnPointHasNearbyMines(ent, spot->s.origin, SPAWN_MINE_RADIUS) ? 1.0f : 0.0f;

	return 0.35f * heat +
		0.25f * near_penalty +
		0.20f * los_penalty +
		0.15f * avoid_penalty +
		0.05f * mine_penalty;
}

template <typename ScoreFn>
static gentity_t *SelectFromSpawnList(const std::vector<gentity_t *> &spawns, const ScoreFn &score_fn) {
	if (spawns.empty())
		return nullptr;

	struct scored_spawn_t {
		gentity_t *spot = nullptr;
		float score = 0.0f;
	};

	std::vector<scored_spawn_t> scored;
	scored.reserve(spawns.size());

	float best = FLT_MAX;
	bool has_finite_score = false;
	for (gentity_t *spot : spawns) {
		float score = score_fn(spot);
		if (!std::isfinite(score)) {
			score = FLT_MAX;
		} else {
			has_finite_score = true;
			best = std::min(best, score);
		}
		scored.push_back({ spot, score });
	}

	if (!has_finite_score)
		return PickRandomly(spawns);

	constexpr float epsilon = 0.05f;
	const float finalist_window = std::max(epsilon, 0.01f * std::max(1.0f, fabsf(best)));
	std::vector<gentity_t *> finalists;
	finalists.reserve(spawns.size());

	for (const scored_spawn_t &candidate : scored) {
		if (candidate.score <= best + finalist_window)
			finalists.push_back(candidate.spot);
	}

	if (finalists.empty())
		return PickRandomly(spawns);

	return PickRandomly(finalists);
}

static gentity_t *SelectFallbackStartPoint(gentity_t *ent, const vec3_t &avoid_point, bool force_spawn) {
	const std::vector<gentity_t *> fallback = GatherFallbackSpawnPoints();
	const bool has_avoid_point = HasSpawnAvoidPoint(ent, avoid_point);
	const std::vector<gentity_t *> eligible = FilterFallbackSpawnsRelaxingAvoid(fallback, ent, avoid_point, has_avoid_point, force_spawn);

	if (eligible.empty())
		return nullptr;

	return SelectFromSpawnList(eligible, [ent, avoid_point](gentity_t *spot) {
		return CompositeDangerScore(spot, ent, avoid_point);
	});
}

/*
================
SelectTeamSpawnPoint

Select from the requested team's points first, then FFA points. Broader
team/fallback recovery is handled by SelectDeathmatchSpawnPoint.
================
*/
static gentity_t *SelectTeamSpawnPoint(gentity_t *ent, team_t team, const vec3_t &avoid_point, bool force_spawn) {
	const char *classname = nullptr;
	switch (team) {
		case TEAM_RED:
			classname = "info_player_team_red";
			break;
		case TEAM_BLUE:
			classname = "info_player_team_blue";
			break;
		default:
			classname = "info_player_deathmatch";
			break;
	}

	const bool has_avoid_point = HasSpawnAvoidPoint(ent, avoid_point);
	auto pick_from_list = [ent, &avoid_point, force_spawn, has_avoid_point](const std::vector<gentity_t *> &spawns) {
		std::vector<gentity_t *> eligible = FilterEligibleSpawns(spawns, ent, avoid_point, force_spawn, has_avoid_point);
		if (eligible.empty())
			eligible = FilterFallbackSpawnsRelaxingAvoid(spawns, ent, avoid_point, has_avoid_point, force_spawn);
		if (eligible.empty())
			return static_cast<gentity_t *>(nullptr);

		return SelectFromSpawnList(eligible, [ent, avoid_point](gentity_t *spot) {
			return CompositeDangerScore(spot, ent, avoid_point);
		});
	};

	const std::vector<gentity_t *> team_spawns = GatherSpawnPoints(classname);
	if (gentity_t *spot = pick_from_list(team_spawns))
		return spot;

	if (strcmp(classname, "info_player_deathmatch")) {
		const std::vector<gentity_t *> ffa = GatherSpawnPoints("info_player_deathmatch");
		if (gentity_t *spot = pick_from_list(ffa))
			return spot;
	}

	return nullptr;
}

static gentity_t *SelectAnyTeamSpawnPoint(gentity_t *ent, const vec3_t &avoid_point, bool force_spawn) {
	std::vector<gentity_t *> team_spawns = GatherSpawnPoints("info_player_team_red");
	std::vector<gentity_t *> blue = GatherSpawnPoints("info_player_team_blue");
	team_spawns.insert(team_spawns.end(), blue.begin(), blue.end());

	if (team_spawns.empty())
		return nullptr;

	const bool has_avoid_point = HasSpawnAvoidPoint(ent, avoid_point);
	std::vector<gentity_t *> eligible = FilterEligibleSpawns(team_spawns, ent, avoid_point, force_spawn, has_avoid_point);
	if (eligible.empty())
		eligible = FilterFallbackSpawnsRelaxingAvoid(team_spawns, ent, avoid_point, has_avoid_point, force_spawn);

	if (eligible.empty())
		return nullptr;

	return SelectFromSpawnList(eligible, [ent, avoid_point](gentity_t *spot) {
		return CompositeDangerScore(spot, ent, avoid_point);
	});
}

select_spawn_result_t SelectDeathmatchSpawnPoint(gentity_t *ent, vec3_t avoid_point, [[maybe_unused]] playerspawn_t mode, bool force_spawn, bool fallback_to_ctf_or_start, bool intermission, bool initial) {
	if (intermission) {
		if (gentity_t *spot = level.spawn_spots[SPAWN_SPOT_INTERMISSION]; spot && spot->inuse && SpawnPointHasUsableOrigin(spot))
			return { spot, true };
	}

	std::vector<gentity_t *> all_spawns = GatherSpawnPoints("info_player_deathmatch");
	std::vector<gentity_t *> red_spawns;
	std::vector<gentity_t *> blue_spawns;
	if (fallback_to_ctf_or_start) {
		red_spawns = GatherSpawnPoints("info_player_team_red");
		blue_spawns = GatherSpawnPoints("info_player_team_blue");
	}

	const bool has_multiplayer_spawns = !all_spawns.empty() || !red_spawns.empty() || !blue_spawns.empty();
	std::vector<gentity_t *> base_spawns = all_spawns;
	const bool initial_player_spawn = initial && ent && ent->client;
	if (initial_player_spawn)
		base_spawns = FilterInitialSpawns(all_spawns);

	const bool using_initial_subset = initial_player_spawn && base_spawns.size() < all_spawns.size();
	bool any_defined = has_multiplayer_spawns;
	const bool has_avoid_point = HasSpawnAvoidPoint(ent, avoid_point);

	if (!has_avoid_point)
		std::shuffle(base_spawns.begin(), base_spawns.end(), mt_rand);

	std::vector<gentity_t *> eligible = FilterEligibleSpawns(base_spawns, ent, avoid_point, force_spawn, has_avoid_point);

	if (eligible.empty() && using_initial_subset) {
		std::vector<gentity_t *> all_eligible = FilterEligibleSpawns(all_spawns, ent, avoid_point, force_spawn, has_avoid_point);
		if (!all_eligible.empty()) {
			base_spawns = all_spawns;
			eligible = std::move(all_eligible);
		} else
			base_spawns = all_spawns;
	}

	if (eligible.empty() && fallback_to_ctf_or_start) {
		std::vector<gentity_t *> relaxed = FilterFallbackSpawnsRelaxingAvoid(base_spawns, ent, avoid_point, has_avoid_point, force_spawn);
		if (!relaxed.empty()) {
			return { SelectFromSpawnList(relaxed, [ent, avoid_point](gentity_t *spot) {
				return CompositeDangerScore(spot, ent, avoid_point);
			}), true };
		}
	}

	if (eligible.empty() && fallback_to_ctf_or_start) {
		if (Teams()) {
			const team_t team = (ent && ent->client) ? ent->client->sess.team : TEAM_FREE;
			if (gentity_t *spot = SelectTeamSpawnPoint(ent, team, avoid_point, force_spawn))
				return { spot, true };
		}
		if ((Teams() || all_spawns.empty()) && (!red_spawns.empty() || !blue_spawns.empty())) {
			if (gentity_t *spot = SelectAnyTeamSpawnPoint(ent, avoid_point, force_spawn))
				return { spot, true };
		}
	}

	// Solo/coop starts are malformed-map recovery only, never normal MP placement.
	if (fallback_to_ctf_or_start && !has_multiplayer_spawns) {
		const std::vector<gentity_t *> fallback = GatherFallbackSpawnPoints();
		any_defined = any_defined || !fallback.empty();
		if (gentity_t *spot = SelectFallbackStartPoint(ent, avoid_point, force_spawn))
			return { spot, true };
	}

	if (eligible.empty()) {
		std::vector<gentity_t *> loose = FilterFallbackSpawnsRelaxingAvoid(base_spawns, ent, avoid_point, has_avoid_point, force_spawn);
		if (!loose.empty()) {
			return { SelectFromSpawnList(loose, [ent, avoid_point](gentity_t *spot) {
				return CompositeDangerScore(spot, ent, avoid_point);
			}), true };
		}
		return { nullptr, any_defined };
	}

	return { SelectFromSpawnList(eligible, [ent, avoid_point](gentity_t *spot) {
		return CompositeDangerScore(spot, ent, avoid_point);
	}), true };
}

static gentity_t *SelectLavaCoopSpawnPoint(gentity_t *ent, bool check_players) {
	int		 index;
	gentity_t *spot = nullptr;
	float	 lavatop;
	gentity_t *lava;
	gentity_t *pointWithLeastLava;
	float	 lowest;
	gentity_t *spawnPoints[64];
	vec3_t	 center;
	int		 numPoints;
	gentity_t *highestlava;

	lavatop = -99999;
	highestlava = nullptr;

	// first, find the highest lava
	// remember that some will stop moving when they've filled their
	// areas...
	lava = nullptr;
	while (1) {
		lava = G_FindByString<&gentity_t::classname>(lava, "func_water");
		if (!lava)
			break;

		center = lava->absmax + lava->absmin;
		center *= 0.5f;

		if (lava->spawnflags.has(SPAWNFLAG_WATER_SMART) && (gi.pointcontents(center) & MASK_WATER)) {
			if (lava->absmax[2] > lavatop) {
				lavatop = lava->absmax[2];
				highestlava = lava;
			}
		}
	}

	// if we didn't find ANY lava, then return nullptr
	if (!highestlava)
		return nullptr;

	// find the top of the lava and include a small margin of error (plus bbox size)
	lavatop = highestlava->absmax[2] + 64;

	// find all the lava spawn points and store them in spawnPoints[]
	spot = nullptr;
	numPoints = 0;
	while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_coop_lava"))) {
		if (numPoints == 64)
			break;
		if (!SpawnPointHasUsableOrigin(spot))
			continue;

		spawnPoints[numPoints++] = spot;
	}

	if (!numPoints)
		return nullptr;

	// walk up the sorted list and return the lowest, open, non-lava spawn point
	spot = nullptr;
	lowest = 999999;
	pointWithLeastLava = nullptr;
	for (index = 0; index < numPoints; index++) {
		if (spawnPoints[index]->s.origin[2] < lavatop)
			continue;

		if ((!check_players || PlayersRangeFromSpot(ent, spawnPoints[index]) > 32) &&
			!UnsafeSpawnPosition(spawnPoints[index]->s.origin, check_players, ent)) {
			if (spawnPoints[index]->s.origin[2] < lowest) {
				// save the last point
				pointWithLeastLava = spawnPoints[index];
				lowest = spawnPoints[index]->s.origin[2];
			}
		}
	}

	return pointWithLeastLava;
}

// [Paril-KEX]
static gentity_t *SelectSingleSpawnPoint(gentity_t *ent) {
	gentity_t *spot = nullptr;

	while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_start")) != nullptr) {
		if (!SpawnPointHasUsableOrigin(spot))
			continue;

		if (!game.spawnpoint[0] && !spot->targetname)
			break;

		if (!game.spawnpoint[0] || !spot->targetname)
			continue;

		if (Q_strcasecmp(game.spawnpoint, spot->targetname) == 0)
			break;
	}

	if (!spot) {
		// there wasn't a matching targeted spawnpoint, use one that has no targetname
		while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_start")) != nullptr)
			if (SpawnPointHasUsableOrigin(spot) && !spot->targetname)
				return spot;
	}

	// none at all, so just pick any
	while (!spot || !SpawnPointHasUsableOrigin(spot)) {
		spot = G_FindByString<&gentity_t::classname>(spot, "info_player_start");
		if (!spot)
			return nullptr;
	}

	return spot;
}

// [Paril-KEX]
gentity_t *G_UnsafeSpawnPosition(vec3_t spot, bool check_players,
	const gentity_t *ignore, bool allow_nudge) {
	return UnsafeSpawnPosition(spot, check_players, ignore, allow_nudge);
}

static gentity_t *SelectCoopSpawnPoint(gentity_t *ent, bool force_spawn, bool check_players) {
	gentity_t *spot = nullptr;
	const char *target;
	const bool randomize_open_coop = !check_players && !g_coop_player_collision->integer;
	std::vector<gentity_t *> open_coop_spots;

	//  rogue hack, but not too gross...
	if (!Q_strcasecmp(level.mapname, "rmine2"))
		return SelectLavaCoopSpawnPoint(ent, check_players);

	// try the main spawn point first
	spot = SelectSingleSpawnPoint(ent);

	if (spot && !G_UnsafeSpawnPosition(spot->s.origin, check_players, ent))
		return spot;

	spot = nullptr;

	// assume there are four coop spots at each spawnpoint
	int32_t num_valid_spots = 0;

	while (1) {
		spot = G_FindByString<&gentity_t::classname>(spot, "info_player_coop");
		if (!spot)
			break; // we didn't have enough...
		if (!SpawnPointHasUsableOrigin(spot))
			continue;

		target = spot->targetname;
		if (!target)
			target = "";
		if (Q_strcasecmp(game.spawnpoint, target) == 0) { // this is a coop spawn point for one of the clients here
			num_valid_spots++;

			if (!G_UnsafeSpawnPosition(spot->s.origin, check_players, ent)) {
				if (randomize_open_coop)
					open_coop_spots.push_back(spot);
				else
					return spot; // this is it
			}
		}
	}

	// if we didn't find any spots, map is probably set up wrong.
	// use empty targetname ones.
	if (!num_valid_spots) {
		while (1) {
			spot = G_FindByString<&gentity_t::classname>(spot, "info_player_coop");
			if (!spot)
				break; // we didn't have enough...
			if (!SpawnPointHasUsableOrigin(spot))
				continue;

			target = spot->targetname;
			if (!target) {
				// this is a coop spawn point for one of the clients here
				num_valid_spots++;

				if (!G_UnsafeSpawnPosition(spot->s.origin, check_players, ent)) {
					if (randomize_open_coop)
						open_coop_spots.push_back(spot);
					else
						return spot; // this is it
				}
			}
		}
	}

	if (!open_coop_spots.empty())
		return PickRandomly(open_coop_spots);

	// no safe spots..?
	if (force_spawn || (!check_players && !g_coop_player_collision->integer)) {
		spot = SelectSingleSpawnPoint(ent);
		if (spot && !G_UnsafeSpawnPosition(spot->s.origin, false, ent))
			return spot;
	}

	return nullptr;
}

static bool TryLandmarkSpawn(gentity_t *ent, vec3_t &origin, vec3_t &angles) {
	// if transitioning from another level with a landmark seamless transition
	// just set the location here
	if (!ent || !ent->client || !ent->client->landmark_name || !strlen(ent->client->landmark_name)) {
		return false;
	}

	gentity_t *landmark = G_PickTarget(ent->client->landmark_name);
	if (!landmark) {
		return false;
	}

	vec3_t old_origin = origin;
	vec3_t spot_origin = origin;
	origin = ent->client->landmark_rel_pos;

	// rotate our relative landmark into our new landmark's frame of reference
	origin = RotatePointAroundVector({ 1, 0, 0 }, origin, landmark->s.angles[PITCH]);
	origin = RotatePointAroundVector({ 0, 1, 0 }, origin, landmark->s.angles[ROLL]);
	origin = RotatePointAroundVector({ 0, 0, 1 }, origin, landmark->s.angles[YAW]);

	origin += landmark->s.origin;

	angles = ent->client->oldviewangles + landmark->s.angles;

	if (landmark->spawnflags.has(SPAWNFLAG_LANDMARK_KEEP_Z))
		origin[2] = spot_origin[2];

	// sometimes, landmark spawns can cause slight inconsistencies in collision;
	// we'll do a bit of tracing to make sure the bbox is clear
	if (G_FixStuckObject_Generic(origin, PLAYER_MINS, PLAYER_MAXS, [ent](const vec3_t &start, const vec3_t &mins, const vec3_t &maxs, const vec3_t &end) {
		return gi.trace(start, mins, maxs, end, ent, MASK_PLAYERSOLID & ~CONTENTS_PLAYER);
		}) == stuck_result_t::NO_GOOD_POSITION) {
		origin = old_origin;
		return false;
	}

	ent->s.origin = origin;

	// rotate the velocity that we grabbed from the map
	if (ent->velocity) {
		ent->velocity = RotatePointAroundVector({ 1, 0, 0 }, ent->velocity, landmark->s.angles[PITCH]);
		ent->velocity = RotatePointAroundVector({ 0, 1, 0 }, ent->velocity, landmark->s.angles[ROLL]);
		ent->velocity = RotatePointAroundVector({ 0, 0, 1 }, ent->velocity, landmark->s.angles[YAW]);
	}

	return true;
}

static bool SelectSpectatorSpawnPoint(vec3_t &origin, vec3_t &angles);

/*
===========
SelectSpawnPoint

Chooses a player start, deathmatch start, coop start, etc
============
*/
bool SelectSpawnPoint(gentity_t *ent, vec3_t &origin, vec3_t &angles, bool force_spawn, bool &landmark) {
	landmark = false;
	gentity_t *spot = nullptr;

	// DM spots are simple
	if (deathmatch->integer) {
		const bool has_client = ent && ent->client;
		const bool player_is_eliminated = has_client && ent->client->eliminated;
		const bool wants_player_spawn = has_client && ClientIsPlaying(ent->client) && !player_is_eliminated;
		const vec3_t avoid_point = has_client ? ent->client->spawn_origin : vec3_origin;
		const bool initial_spawn = has_client && !ent->client->pers.spawned;

		// [MuffMode] Validated RA2 maps carry independent fighter and observer
		// starts for every playable room.
		if (GT(GT_ARENA) && has_client) {
			const bool spectator = level.intermission_time ||
				!MM_Arena_IsFighter(ent->client) ||
				MM_Arena_IsEliminated(ent->client);
			spot = MM_Arena_SelectSpawnPoint(ent, spectator);
			if (spot && spot->inuse && SpawnPointHasUsableOrigin(spot)) {
				const bool fighter_start = spot->classname &&
					(!strcmp(spot->classname, "info_player_deathmatch") ||
					 !strcmp(spot->classname, "info_player_team_red") ||
					 !strcmp(spot->classname, "info_player_team_blue"));
				const bool ra2_observer_start = spot->classname &&
					!strcmp(spot->classname, "misc_teleporter_dest");
				origin = spot->s.origin +
					vec3_t{ 0.0f, 0.0f,
						(fighter_start || ra2_observer_start) ?
							DeathmatchSpawnOriginLift() : 0.0f };
				angles = spot->s.angles;
				angles[ROLL] = 0.0f;
				return true;
			}
			// A selected arena must never fall through to the singleton
			// spectator/intermission origin, which may belong to the lobby or
			// another room. Keep the client in retryable limbo if a malformed
			// map provides no arena-local observer point.
			if (spectator && MM_Arena_Id(ent) > 0)
				return false;
		}

		if (level.intermission_time || (has_client && (!ClientIsPlaying(ent->client) || player_is_eliminated))) {
			if (SelectSpectatorSpawnPoint(origin, angles)) {
				angles[ROLL] = 0.0f;
				return true;
			}
		}

		if (Teams() && wants_player_spawn)
			spot = SelectTeamSpawnPoint(ent, ent->client->sess.team, avoid_point, force_spawn);

		if (!spot) {
			const bool intermission = static_cast<bool>(level.intermission_time);
			const playerspawn_t legacy_mode = LegacySpawnMode();
			select_spawn_result_t result = SelectDeathmatchSpawnPoint(ent, avoid_point, legacy_mode, force_spawn, true, intermission, initial_spawn);

			if (!result.spot && !force_spawn && !wants_player_spawn)
				result = SelectDeathmatchSpawnPoint(ent, avoid_point, legacy_mode, true, true, intermission, initial_spawn);

			if (!result.spot) {
				if (!result.any_valid)
					gi.Com_PrintFmt("{}: no spawn points defined for this map\n", __FUNCTION__);
				else if (g_verbose->integer)
					gi.Com_PrintFmt("{}: no clear spawn point available; will retry\n", __FUNCTION__);
				return false;
			}

			spot = result.spot;
		}

		if (!spot || !spot->inuse)
			return false;

		origin = spot->s.origin + vec3_t{ 0.0f, 0.0f, DeathmatchSpawnOriginLift() };
		angles = spot->s.angles;
		angles[ROLL] = 0;

		return true;
	}

	if (coop->integer) {
		spot = SelectCoopSpawnPoint(ent, force_spawn, true);

		if (!spot)
			spot = SelectCoopSpawnPoint(ent, force_spawn, false);

		// no open spot yet
		if (!spot) {
			// in worst case scenario in coop during intermission, just spawn us at intermission
			// spot. this only happens for a single frame, and won't break
			// anything if they come back.
			if (level.intermission_time) {
				origin = level.intermission_origin;
				angles = level.intermission_angle;
				return true;
			}

			return false;
		}
	} else {
		spot = SelectSingleSpawnPoint(ent);

		// in SP, just put us at the origin if spawn fails
		if (!spot) {
			gi.Com_PrintFmt("Couldn't find spawn point {}\n", game.spawnpoint);

			origin = { 0, 0, 0 };
			angles = { 0, 0, 0 };

			return true;
		}
	}

	// spot should always be non-null here

	origin = spot->s.origin;
	angles = spot->s.angles;

	// check landmark
	if (TryLandmarkSpawn(ent, origin, angles))
		landmark = true;
	else
		angles[ROLL] = 0.0f;

	return true;
}

/*
===========
SelectSpectatorSpawnPoint

============
*/
static bool SelectSpectatorSpawnPoint(vec3_t &origin, vec3_t &angles) {
	gentity_t *spot = level.spawn_spots[SPAWN_SPOT_INTERMISSION];
	if ((!spot || !spot->inuse || !SpawnPointHasUsableOrigin(spot)) && !level.level_intermission_set)
		SetIntermissionPoint();

	spot = level.spawn_spots[SPAWN_SPOT_INTERMISSION];
	if (spot && spot->inuse && SpawnPointHasUsableOrigin(spot))
		FindIntermissionPoint();
	else if (!level.level_intermission_set)
		return false;

	origin = level.intermission_origin;
	angles = level.intermission_angle;

	return true;
}
