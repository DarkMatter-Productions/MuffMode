// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "entities/brush_misc.h"
#include "muffmode/mm_ent_respawn.h"

#include <algorithm>
#include <cstring>
#include <new>
#include <vector>

// func_explosive comes from entities/brush_misc.h; misc_explobox is published
// straight from the spawn table and has no header of its own.
void SP_misc_explobox(gentity_t *self);

namespace {

struct respawn_class_t {
	const char *classname;
	void (*spawn)(gentity_t *ent);
};

// Adding a class here is all it takes for its map-authored instances to come back,
// provided the destruction path calls MM_EntRespawn_Schedule before the prop dies.
constexpr respawn_class_t kRespawnClasses[] = {
	{ "func_explosive", SP_func_explosive },
	{ "misc_explobox", SP_misc_explobox }
};

// A generous ceiling on tracked props: real Q2 maps sit far below it. The bound is
// here so a hostile or generated entity lump cannot grow the record set without
// limit.
constexpr size_t kMaxRecords = 512;

constexpr int32_t kNoSlot = -1;

struct record_t {
	// Spawn-time state, captured once the spawn function has applied its defaults
	// so that health, bounds and damage are the values the prop actually had.
	vec3_t origin {};
	vec3_t angles {};
	vec3_t mins {};
	vec3_t maxs {};
	float scale = 0.0f;
	int32_t health = 0;
	int32_t dmg = 0;
	int32_t mass = 0;
	spawnflags_t spawnflags { 0 };
	const char *classname = nullptr;
	const char *target = nullptr;
	const char *targetname = nullptr;
	const char *model = nullptr;
	void (*spawn)(gentity_t *ent) = nullptr;

	// Live tracking. slot is kNoSlot while the prop is destroyed and queued.
	int32_t slot = kNoSlot;
	int32_t generation = 0;
	gtime_t respawn_time = 0_ms;
	bool retired = false;
};

std::vector<record_t> records;
bool warned_about_capacity = false;

struct rebuild_result_t {
	bool rebuilt = false;
	int32_t slot = kNoSlot;
	int32_t generation = 0;
};

mm_ent_respawn_vec_t ToRuleVec(const vec3_t &v)
{
	return { v.x, v.y, v.z };
}

const respawn_class_t *FindClass(const char *classname)
{
	if (!classname || !*classname)
		return nullptr;

	for (const respawn_class_t &entry : kRespawnClasses)
		if (!strcmp(entry.classname, classname))
			return &entry;

	return nullptr;
}

gtime_t RespawnDelay()
{
	if (!deathmatch->integer || !g_dm_explosive_respawn_time)
		return 0_ms;

	return gtime_t::from_sec(
		MM_EntRespawnDelaySeconds(g_dm_explosive_respawn_time->value));
}

record_t *FindLiveRecord(const gentity_t *ent)
{
	if (!ent || !ent->inuse)
		return nullptr;

	const int32_t slot = static_cast<int32_t>(ent - g_entities);
	for (record_t &record : records)
		if (!record.retired && record.slot == slot &&
			record.generation == ent->spawn_count)
			return &record;

	return nullptr;
}

// True while the slot still holds the exact prop this record produced.
bool TrackedEntityIsLive(const record_t &record)
{
	if (record.slot < 0 || record.slot >= static_cast<int32_t>(game.maxentities))
		return false;

	const gentity_t *const tracked = &g_entities[record.slot];
	return tracked->inuse && tracked->spawn_count == record.generation;
}

// The eight corners of the recorded bounding box in world space. loc_CanSee builds
// the same set, but it needs a live entity and refuses brush models outright, and
// the prop no longer exists in either sense by the time this runs.
void BuildBoxPoints(vec3_t (&points)[8], const record_t &record)
{
	for (size_t i = 0; i < 8; i++) {
		points[i] = {
			record.origin.x + ((i & 1) ? record.maxs.x : record.mins.x),
			record.origin.y + ((i & 2) ? record.maxs.y : record.mins.y),
			record.origin.z + ((i & 4) ? record.maxs.z : record.mins.z)
		};
	}
}

// The placement rules, in WORR's order: nobody may see the spot, nothing may be
// standing in it, and nobody may be close enough for the arrival to read as a
// teleport. Any one of them defers the rebuild by MM_ENT_RESPAWN_RETRY_SECONDS.
bool SpotIsClear(const record_t &record)
{
	const mm_ent_respawn_vec_t center = MM_EntRespawnBoxCenter(
		ToRuleVec(record.origin), ToRuleVec(record.mins), ToRuleVec(record.maxs));
	const vec3_t center_world { center.x, center.y, center.z };

	vec3_t corners[8];
	BuildBoxPoints(corners, record);

	// (a) Visibility. WORR runs the line-of-sight and facing tests against every
	// client on the server, which in an open map means one player idly aimed down
	// a corridor can hold a prop off indefinitely. Gate both on PVS first: a
	// player who cannot possibly have the spot on screen has no say in it.
	for (gentity_t *player : active_clients()) {
		if (!player->client->pers.spawned)
			continue;
		if (!gi.inPVS(center_world, player->s.origin, true))
			continue;

		vec3_t viewpoint = player->s.origin;
		viewpoint.z += static_cast<float>(player->viewheight);

		for (const vec3_t &corner : corners) {
			const trace_t tr = gi.traceline(viewpoint, corner, player,
				CONTENTS_MIST | MASK_WATER | MASK_SOLID);
			if (tr.fraction == 1.0f)
				return false;
		}

		vec3_t forward;
		AngleVectors(player->client->ps.viewangles, &forward, nullptr, nullptr);
		if (MM_EntRespawnFacesPoint(ToRuleVec(viewpoint), ToRuleVec(forward), center))
			return false;
	}

	// (b) Occupancy. Lift the probe so a prop that rests on the floor does not
	// read its own ground plane as an obstruction.
	vec3_t probe = record.origin;
	probe.z += MM_ENT_RESPAWN_PROBE_LIFT;
	const trace_t tr = gi.trace(probe, record.mins, record.maxs, probe, nullptr,
		CONTENTS_PLAYER | CONTENTS_MONSTER);
	if (tr.startsolid)
		return false;

	// (c) Clearance. Deliberately not PVS-gated: a player standing right behind a
	// thin wall still should not have a barrel appear an arm's length away.
	for (gentity_t *player : active_clients()) {
		if (!player->client->pers.spawned)
			continue;

		const mm_ent_respawn_vec_t player_mins {
			player->s.origin.x + player->mins.x,
			player->s.origin.y + player->mins.y,
			player->s.origin.z + player->mins.z
		};
		const mm_ent_respawn_vec_t player_maxs {
			player->s.origin.x + player->maxs.x,
			player->s.origin.y + player->maxs.y,
			player->s.origin.z + player->maxs.z
		};

		if (MM_EntRespawnWithinClearance(center, player_mins, player_maxs))
			return false;
	}

	return true;
}

// Taken by value on purpose: the spawn function runs arbitrary vanilla code, and a
// reference into the record vector is not worth proving stable across it.
rebuild_result_t RebuildProp(record_t record)
{
	gentity_t *const ent = G_Spawn();
	if (!ent)
		return {};

	ent->classname = record.classname;
	ent->s.origin = record.origin;
	ent->s.angles = record.angles;
	ent->s.scale = record.scale;
	ent->health = record.health;
	ent->dmg = record.dmg;
	ent->mass = record.mass;
	ent->spawnflags = record.spawnflags;
	ent->target = record.target;
	ent->targetname = record.targetname;
	ent->model = record.model;
	ent->mins = record.mins;
	ent->maxs = record.maxs;

	const int32_t slot = static_cast<int32_t>(ent - g_entities);
	const int32_t generation = ent->spawn_count;

	record.spawn(ent);

	// The spawn function is free to reject the entity. A slot that no longer holds
	// our prop is a permanent failure, not something to retry every second.
	if (!ent->inuse || ent->spawn_count != generation)
		return {};

	// ParseWorldEntities stamps this on every map entity after its spawn function
	// runs; a rebuilt prop has to look the same to the renderer as the original.
	ent->s.renderfx |= RF_IR_VISIBLE;

	return { true, slot, generation };
}

void CompactRecords()
{
	records.erase(
		std::remove_if(records.begin(), records.end(),
			[](const record_t &record) { return record.retired; }),
		records.end());
}

} // namespace

void MM_EntRespawn_ClearAll()
{
	// Records point at TAG_LEVEL strings, so this has to run ahead of any
	// gi.FreeTags(TAG_LEVEL) rather than merely alongside the entity wipe.
	records.clear();
	warned_about_capacity = false;
}

void MM_EntRespawn_CaptureMapEntity(gentity_t *ent)
{
	// Capture regardless of the current delay so that raising the cvar mid-match
	// takes effect on the next prop that dies instead of only after a map change.
	if (!deathmatch->integer)
		return;
	if (!ent || !ent->inuse || !ent->classname)
		return;

	const respawn_class_t *const entry = FindClass(ent->classname);
	if (!entry)
		return;

	if (records.size() >= kMaxRecords) {
		if (!warned_about_capacity) {
			warned_about_capacity = true;
			gi.Com_PrintFmt("{}: prop respawn tracking is full at {} entries; "
				"further props will not respawn.\n", __FUNCTION__, kMaxRecords);
		}
		return;
	}

	record_t record;
	record.origin = ent->s.origin;
	record.angles = ent->s.angles;
	record.mins = ent->mins;
	record.maxs = ent->maxs;
	record.scale = ent->s.scale;
	record.health = ent->health;
	record.dmg = ent->dmg;
	record.mass = ent->mass;
	record.spawnflags = ent->spawnflags;
	record.classname = ent->classname;
	record.target = ent->target;
	record.targetname = ent->targetname;
	record.model = ent->model;
	record.spawn = entry->spawn;
	record.slot = static_cast<int32_t>(ent - g_entities);
	record.generation = ent->spawn_count;

	try {
		records.push_back(record);
	} catch (const std::bad_alloc &) {
		gi.Com_PrintFmt("{}: out of memory tracking {}; it will not respawn.\n",
			__FUNCTION__, *ent);
	}
}

bool MM_EntRespawn_Schedule(gentity_t *ent)
{
	const gtime_t delay = RespawnDelay();
	if (delay <= 0_ms)
		return false;

	record_t *const record = FindLiveRecord(ent);
	if (!record)
		return false;

	record->slot = kNoSlot;
	record->generation = 0;
	record->respawn_time = level.time + delay;
	return true;
}

void MM_EntRespawn_ExpediteAll()
{
	for (record_t &record : records)
		if (!record.retired && record.slot == kNoSlot)
			record.respawn_time = level.time;
}

void MM_EntRespawn_RunFrame()
{
	if (records.empty())
		return;

	const gtime_t delay = RespawnDelay();
	bool needs_compaction = false;

	// Indexed rather than ranged: RebuildProp reaches into vanilla spawn code, and
	// an iterator held across that is a liability for no gain.
	for (size_t i = 0; i < records.size(); i++) {
		record_t &record = records[i];
		if (record.retired)
			continue;

		if (record.slot != kNoSlot) {
			// A prop freed without exploding -- killtarget, a world teardown --
			// is gone for good. Drop the record instead of tracking a dead slot
			// whose number a completely unrelated entity will soon reuse.
			if (!TrackedEntityIsLive(record)) {
				record.retired = true;
				needs_compaction = true;
			}
			continue;
		}

		if (delay <= 0_ms) {
			// Switched off mid-match: forget the queue rather than letting every
			// pending prop arrive at once if the cvar is turned back on.
			record.retired = true;
			needs_compaction = true;
			continue;
		}

		if (level.time < record.respawn_time)
			continue;

		if (!SpotIsClear(record)) {
			record.respawn_time = level.time +
				gtime_t::from_sec(MM_ENT_RESPAWN_RETRY_SECONDS);
			continue;
		}

		const rebuild_result_t result = RebuildProp(record);
		if (!result.rebuilt) {
			records[i].retired = true;
			needs_compaction = true;
			continue;
		}

		records[i].slot = result.slot;
		records[i].generation = result.generation;
		records[i].respawn_time = 0_ms;
	}

	if (needs_compaction)
		CompactRecords();
}
