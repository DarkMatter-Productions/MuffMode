// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Player spawn entities and spawn-point selection.
#include <algorithm>
#include <vector>

#include "g_local.h"
void SP_misc_teleporter_dest(gentity_t *ent);

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
		self->flags = FL_NO_BOTS;
	if (st.nohumans)
		self->flags = FL_NO_HUMANS;
}

/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
The normal starting point for a level.

"nobots" will prevent bots from using this spot.
"nohumans" will prevent humans from using this spot.
*/
void SP_info_player_start(gentity_t *self) {
	// fix stuck spawn points
	if (gi.trace(self->s.origin, PLAYER_MINS, PLAYER_MAXS, self->s.origin, self, MASK_SOLID).startsolid)
		G_FixStuckObject(self, self->s.origin);

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
	SP_misc_teleporter_dest(self);

	deathmatch_spawn_flags(self);
}

/*QUAKED info_player_team_red (1 0 0) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
A potential Red Team spawning position for CTF games.
*/
void SP_info_player_team_red(gentity_t *self) {}

/*QUAKED info_player_team_blue (0 0 1) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
A potential Blue Team spawning position for CTF games.
*/
void SP_info_player_team_blue(gentity_t *self) {}

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
	if (gi.trace(self->s.origin, PLAYER_MINS, PLAYER_MAXS, self->s.origin, self, MASK_SOLID).startsolid)
		G_FixStuckObject(self, self->s.origin);
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
	float	bestplayerdistance;
	vec3_t	v;
	float	playerdistance;

	bestplayerdistance = 9999999;

	for (auto ec : active_clients()) {
		if (ec->health <= 0 || ec->client->eliminated)
			continue;
#if 0
		if (ent != nullptr)
			if (ec == ent)
				continue;
#endif
		v = spot->s.origin - ec->s.origin;
		playerdistance = v.length();

		if (playerdistance < bestplayerdistance)
			bestplayerdistance = playerdistance;
	}

	return bestplayerdistance;
}

static bool SpawnPointClear(gentity_t *spot) {
	vec3_t p = spot->s.origin + vec3_t{ 0, 0, 9.f };
	return !gi.trace(p, PLAYER_MINS, PLAYER_MAXS, p, spot, CONTENTS_PLAYER | CONTENTS_MONSTER).startsolid;
}

static bool SpawnPointAllowsClient(const gentity_t *ent, const gentity_t *spot) {
	if (!ent || !ent->client)
		return true;

	if (ent->client->sess.is_a_bot)
		return !(spot->flags & FL_NO_BOTS);

	return !(spot->flags & FL_NO_HUMANS);
}

static bool SpawnPointOutsideAvoidRadius(const gentity_t *spot, const vec3_t &avoid_point, float avoid_radius) {
	if (avoid_point == spot->s.origin)
		return false;

	if (!avoid_point || !avoid_radius)
		return true;

	const vec3_t delta = spot->s.origin - avoid_point;
	if (delta.length() > avoid_radius)
		return true;

	if (g_dm_respawn_point_min_dist_debug->integer)
		gi.Com_PrintFmt("{}: avoiding spawn point\n", *spot);

	return false;
}

static bool SpawnPointSelectable(const gentity_t *ent, gentity_t *spot, const vec3_t &avoid_point, float avoid_radius) {
	return SpawnPointOutsideAvoidRadius(spot, avoid_point, avoid_radius) &&
		SpawnPointAllowsClient(ent, spot) &&
		SpawnPointClear(spot);
}

select_spawn_result_t SelectDeathmatchSpawnPoint(gentity_t *ent, vec3_t avoid_point, playerspawn_t mode, bool force_spawn, bool fallback_to_ctf_or_start, bool intermission, bool initial) {
	float cv_dist = g_dm_respawn_point_min_dist->value;
	struct spawn_point_t {
		gentity_t *point;
		float dist;
	};

	static std::vector<spawn_point_t> spawn_points;

	spawn_points.clear();

	// gather all spawn points 
	gentity_t *spot = nullptr;

	if (cv_dist > 512) cv_dist = 512;
	else if (cv_dist < 0) cv_dist = 0;

	if (intermission)
		while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_intermission")) != nullptr)
			spawn_points.push_back({ spot, PlayersRangeFromSpot(ent, spot) });

	if (spawn_points.size() == 0) {
		spot = nullptr;
		while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_deathmatch")) != nullptr)
			spawn_points.push_back({ spot, PlayersRangeFromSpot(ent, spot) });

		// no points
		if (spawn_points.size() == 0) {
			// try CTF spawns...
			if (fallback_to_ctf_or_start) {
				spot = nullptr;
				while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_team_red")) != nullptr)
					spawn_points.push_back({ spot, PlayersRangeFromSpot(ent, spot) });
				spot = nullptr;
				while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_team_blue")) != nullptr)
					spawn_points.push_back({ spot, PlayersRangeFromSpot(ent, spot) });

				// we only have an info_player_start then
				if (spawn_points.size() == 0) {
					spot = G_FindByString<&gentity_t::classname>(nullptr, "info_player_start");

					if (spot)
						spawn_points.push_back({ spot, PlayersRangeFromSpot(ent, spot) });

					// map is malformed
					if (spawn_points.size() == 0)
						return { nullptr, false };
				}
			} else
				return { nullptr, false };
		}
	}

	// if there's only one spawn point, that's the one.
	if (spawn_points.size() == 1) {
		if (force_spawn || SpawnPointClear(spawn_points[0].point))
			return { spawn_points[0].point, true };

		return { nullptr, true };
	}

	// order by distances ascending (top of list has closest players to point)
	std::sort(spawn_points.begin(), spawn_points.end(), [](const spawn_point_t &a, const spawn_point_t &b) { return a.dist < b.dist; });

	switch (mode) {
	default:	// high random
	case playerspawn_t::SPAWN_FAR_HALF:		// farthest half
		{
			size_t margin = spawn_points.size() / 2;

			// for random, select a random point other than the two
			// that are closest to the player if possible.
			// shuffle the non-distance-related spawn points
			std::shuffle(spawn_points.begin() + margin, spawn_points.end(), mt_rand);

			// run down the list and pick the first one that we can use
			for (auto it = spawn_points.begin() + margin; it != spawn_points.end(); ++it) {
				auto spot = it->point;

				if (SpawnPointSelectable(ent, spot, avoid_point, cv_dist))
					return { spot, true };
			}

			// none clear, so we have to pick one of the other two
			if (SpawnPointClear(spawn_points[1].point))
				return { spawn_points[1].point, true };
			else if (SpawnPointClear(spawn_points[0].point))
				return { spawn_points[0].point, true };

			break;
		}
	case playerspawn_t::SPAWN_FARTHEST:		// farthest
		{
			for (auto it = spawn_points.rbegin(); it != spawn_points.rend(); ++it) {
				if (SpawnPointSelectable(ent, it->point, avoid_point, cv_dist))
					return { it->point, true };
			}
			// none clear, so we have to pick one of the other two
			if (spawn_points.size() > 1 && SpawnPointClear(spawn_points[1].point))
				return { spawn_points[1].point, true };
			else if (spawn_points.size() > 0 && SpawnPointClear(spawn_points[0].point))
				return { spawn_points[0].point, true };

			break;
		}
	case playerspawn_t::SPAWN_NEAREST:		// nearest
		{
			for (const spawn_point_t &candidate : spawn_points) {
				if (SpawnPointSelectable(ent, candidate.point, avoid_point, cv_dist))
					return { candidate.point, true };
			}
			// none clear
			break;
		}
	}

	if (force_spawn)
		return { random_element(spawn_points).point, true };

	return { nullptr, true };
}

static vec3_t TeamCentralPoint(team_t team) {
	vec3_t	team_origin = { 0, 0, 0 };
	uint8_t team_count = 0;
	for (auto ec : active_players()) {
		if (ec->client->sess.team != team)
			continue;

		team_origin += ec->s.origin;
		team_count++;
	}
	if (team_origin)
		return team_origin / team_count;
	else
		return team_origin;
}

/*
================
SelectTeamSpawnPoint

Go to a team point, but NOT the two points closest
to other players
================
*/
static gentity_t *SelectTeamSpawnPoint(gentity_t *ent, bool force_spawn) {
	if (ent->client->resp.ctf_state) {
		select_spawn_result_t result = SelectDeathmatchSpawnPoint(ent, ent->client->spawn_origin, (playerspawn_t)clamp(g_dm_spawn_farthest->integer, 0, 3), force_spawn, false, false, false);	// !ClientIsPlaying(ent->client));

		if (result.any_valid)
			return result.spot;
	}
	/*
	vec3_t	team_origin = TeamCentralPoint(ent->client->sess.team);
	gi.LocClient_Print(ent, PRINT_HIGH, "team central point= {} {} {}\n", team_origin[0], team_origin[1], team_origin[2]);
	if (ent->client->resp.ctf_state) {
		select_spawn_result_t result = SelectDeathmatchSpawnPoint(ent, team_origin, SPAWN_NEAREST, force_spawn, false, !ClientIsPlaying(ent->client) || ent->client->eliminated, false);

		if (result.any_valid)
			return result.spot;
	}
	*/
	const char *cname;

	switch (ent->client->sess.team) {
		case TEAM_RED:
			cname = "info_player_team_red";
			break;
		case TEAM_BLUE:
			cname = "info_player_team_blue";
			break;
		default:
		{
			select_spawn_result_t result = SelectDeathmatchSpawnPoint(ent, ent->client->spawn_origin, (playerspawn_t)clamp(g_dm_spawn_farthest->integer, 0, 3), force_spawn, true, false, false);

			if (result.any_valid)
				return result.spot;

			gi.Com_Error("Can't find suitable spectator spawn point.");
			return nullptr;
		}
	}

	static std::vector<gentity_t *> spawn_points;
	gentity_t *spot = nullptr;

	spawn_points.clear();

	while ((spot = G_FindByString<&gentity_t::classname>(spot, cname)) != nullptr)
		spawn_points.push_back(spot);

	if (!spawn_points.size()) {
		select_spawn_result_t result = SelectDeathmatchSpawnPoint(ent, ent->client->spawn_origin, (playerspawn_t)clamp(g_dm_spawn_farthest->integer, 0, 3), force_spawn, true, false, false);

		if (!result.any_valid)
			gi.Com_Error("Can't find suitable team spawn point.");

		return result.spot;
	}

	std::shuffle(spawn_points.begin(), spawn_points.end(), mt_rand);

	for (auto &point : spawn_points)
		if (SpawnPointClear(point))
			return point;

	if (force_spawn)
		return random_element(spawn_points);

	return nullptr;
}

static gentity_t *SelectLavaCoopSpawnPoint(gentity_t *ent) {
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

		spawnPoints[numPoints++] = spot;
	}

	// walk up the sorted list and return the lowest, open, non-lava spawn point
	spot = nullptr;
	lowest = 999999;
	pointWithLeastLava = nullptr;
	for (index = 0; index < numPoints; index++) {
		if (spawnPoints[index]->s.origin[2] < lavatop)
			continue;

		if (PlayersRangeFromSpot(ent, spawnPoints[index]) > 32) {
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
			if (!spot->targetname)
				return spot;
	}

	// none at all, so just pick any
	if (!spot)
		return G_FindByString<&gentity_t::classname>(spot, "info_player_start");

	return spot;
}

// [Paril-KEX]
gentity_t *G_UnsafeSpawnPosition(vec3_t spot, bool check_players) {
	contents_t mask = MASK_PLAYERSOLID;

	if (!check_players)
		mask &= ~CONTENTS_PLAYER;

	trace_t tr = gi.trace(spot, PLAYER_MINS, PLAYER_MAXS, spot, nullptr, mask);

	// sometimes the spot is too close to the ground, give it a bit of slack
	if (tr.startsolid && !tr.ent->client) {
		spot[2] += 1;
		tr = gi.trace(spot, PLAYER_MINS, PLAYER_MAXS, spot, nullptr, mask);
	}

	// no idea why this happens in some maps..
	if (tr.startsolid && !tr.ent->client) {
		// try a nudge
		if (G_FixStuckObject_Generic(spot, PLAYER_MINS, PLAYER_MAXS, [mask](const vec3_t &start, const vec3_t &mins, const vec3_t &maxs, const vec3_t &end) {
			return gi.trace(start, mins, maxs, end, nullptr, mask);
			}) == stuck_result_t::NO_GOOD_POSITION)
			return tr.ent; // what do we do here...?

			trace_t tr = gi.trace(spot, PLAYER_MINS, PLAYER_MAXS, spot, nullptr, mask);

			if (tr.startsolid && !tr.ent->client)
				return tr.ent; // what do we do here...?
	}

	if (tr.fraction == 1.f)
		return nullptr;
	else if (check_players && tr.ent && tr.ent->client)
		return tr.ent;

	return nullptr;
}

static gentity_t *SelectCoopSpawnPoint(gentity_t *ent, bool force_spawn, bool check_players) {
	gentity_t *spot = nullptr;
	const char *target;

	//  rogue hack, but not too gross...
	if (!Q_strcasecmp(level.mapname, "rmine2"))
		return SelectLavaCoopSpawnPoint(ent);

	// try the main spawn point first
	spot = SelectSingleSpawnPoint(ent);

	if (spot && !G_UnsafeSpawnPosition(spot->s.origin, check_players))
		return spot;

	spot = nullptr;

	// assume there are four coop spots at each spawnpoint
	int32_t num_valid_spots = 0;

	while (1) {
		spot = G_FindByString<&gentity_t::classname>(spot, "info_player_coop");
		if (!spot)
			break; // we didn't have enough...

		target = spot->targetname;
		if (!target)
			target = "";
		if (Q_strcasecmp(game.spawnpoint, target) == 0) { // this is a coop spawn point for one of the clients here
			num_valid_spots++;

			if (!G_UnsafeSpawnPosition(spot->s.origin, check_players))
				return spot; // this is it
		}
	}

	bool use_targetname = true;

	// if we didn't find any spots, map is probably set up wrong.
	// use empty targetname ones.
	if (!num_valid_spots) {
		use_targetname = false;

		while (1) {
			spot = G_FindByString<&gentity_t::classname>(spot, "info_player_coop");
			if (!spot)
				break; // we didn't have enough...

			target = spot->targetname;
			if (!target) {
				// this is a coop spawn point for one of the clients here
				num_valid_spots++;

				if (!G_UnsafeSpawnPosition(spot->s.origin, check_players))
					return spot; // this is it
			}
		}
	}

	// if player collision is disabled, just pick a random spot
	if (!g_coop_player_collision->integer) {
		spot = nullptr;

		num_valid_spots = irandom(num_valid_spots);

		while (1) {
			spot = G_FindByString<&gentity_t::classname>(spot, "info_player_coop");

			if (!spot)
				break; // we didn't have enough...

			target = spot->targetname;
			if (use_targetname && !target)
				target = "";
			if (use_targetname ? (Q_strcasecmp(game.spawnpoint, target) == 0) : !target) { // this is a coop spawn point for one of the clients here
				num_valid_spots++;

				if (!num_valid_spots)
					return spot;

				--num_valid_spots;
			}
		}

		// if this fails, just fall through to some other spawn.
	}

	// no safe spots..?
	if (force_spawn || !g_coop_player_collision->integer)
		return SelectSingleSpawnPoint(spot);

	return nullptr;
}

static bool TryLandmarkSpawn(gentity_t *ent, vec3_t &origin, vec3_t &angles) {
	// if transitioning from another level with a landmark seamless transition
	// just set the location here
	if (!ent->client->landmark_name || !strlen(ent->client->landmark_name)) {
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

/*
===========
SelectSpawnPoint

Chooses a player start, deathmatch start, coop start, etc
============
*/
bool SelectSpawnPoint(gentity_t *ent, vec3_t &origin, vec3_t &angles, bool force_spawn, bool &landmark) {
	gentity_t *spot = nullptr;

	// DM spots are simple
	if (deathmatch->integer) {
		if (Teams() && ClientIsPlaying(ent->client))
			spot = SelectTeamSpawnPoint(ent, force_spawn);
		else {
			if (false) // Race mode removed
				spot = SelectSingleSpawnPoint(ent);

			if (!spot) {
				select_spawn_result_t result = SelectDeathmatchSpawnPoint(ent, ent->client->spawn_origin, (playerspawn_t)clamp(g_dm_spawn_farthest->integer, 0, 3), force_spawn, true, !ClientIsPlaying(ent->client) || ent->client->eliminated, false);

				if (!result.any_valid)
					gi.Com_Error("No valid spawn points found.");

				spot = result.spot;
			}
		}

		if (spot) {
			origin = spot->s.origin + vec3_t{ 0, 0, (float)(g_dm_spawnpads->integer ? 9 : 1) };
			angles = spot->s.angles;

			//muff mode: we just want yaw really, definitely no roll!
			//if (ClientIsPlaying(ent->client))
				//angles[PITCH] = 0;
			angles[ROLL] = 0;

			return true;
		}

		return false;
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

	return true;
}

/*
===========
SelectSpectatorSpawnPoint

============
*/
static gentity_t *SelectSpectatorSpawnPoint(vec3_t origin, vec3_t angles) {
	//FindIntermissionPoint();
	SetIntermissionPoint();
	origin = level.intermission_origin;
	angles = level.intermission_angle;

	return level.spawn_spots[SPAWN_SPOT_INTERMISSION]; // was NULL
}
