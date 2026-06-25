// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

// [MuffMode] Bot nav baker. Grid-samples the live map's walkable floor with
// gi.trace, flood-fills a walk-only node graph, and writes a KEX "NAV3" v6 file
// the engine's bots can load. Format spec: docs-dev/nav-file-format.md
//
// This produces the Walk backbone only. Teleporters, jumps and movers are added
// afterwards by hand in QuakeNavEditor (each as a non-Walk link + traversal).

#include "../g_local.h"
#include "mm_nav_bake.h"

#include <vector>
#include <unordered_map>
#include <queue>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <filesystem>

namespace {

// --- tuning -----------------------------------------------------------------
constexpr float DEFAULT_GRID   = 96.0f;   // lattice spacing (stock maps ~96-130)
constexpr float MIN_GRID       = 48.0f;
constexpr float MAX_GRID       = 256.0f;
constexpr float STEP_UP        = 18.0f;   // max floor rise to a neighbour
constexpr float MAX_DROP       = 64.0f;   // max floor drop to a neighbour
constexpr float MIN_FLOOR_NZ   = 0.7f;    // floor plane normal.z (slope limit)
constexpr float Z_MERGE        = 64.0f;   // same XY-cell nodes merge within this
constexpr uint16_t NODE_RADIUS = 32;
constexpr uint16_t LINK_FLAGS  = 3;       // every stock link uses flags == 3
constexpr uint8_t  LINK_WALK   = 0;
constexpr uint16_t TRAV_NONE   = 0xFFFF;
constexpr size_t   MAX_NODES   = 8192;    // safety cap for huge/open maps

struct NavNode {
	vec3_t origin;
	std::vector<int32_t> links; // neighbour node indices (directional, deduped)
};

// Pack a 2D lattice cell into a 64-bit hash key.
inline int64_t CellKey(int ix, int iy) {
	return (static_cast<int64_t>(ix) << 32) ^ static_cast<uint32_t>(iy);
}

inline int LatticeIndex(float v, float grid) {
	return static_cast<int>(floorf(v / grid + 0.5f));
}

// --- binary writer helpers --------------------------------------------------
inline void PutU16(std::vector<uint8_t> &b, uint16_t v) {
	b.push_back(uint8_t(v & 0xFF));
	b.push_back(uint8_t((v >> 8) & 0xFF));
}
inline void PutU32(std::vector<uint8_t> &b, uint32_t v) {
	for (int i = 0; i < 4; i++) b.push_back(uint8_t((v >> (i * 8)) & 0xFF));
}
inline void PutF32(std::vector<uint8_t> &b, float f) {
	uint32_t u;
	std::memcpy(&u, &f, 4);
	PutU32(b, u);
}

} // namespace

/*
================
MM_NavBake

Server command worker. gridArg <= 0 selects the default lattice spacing.
================
*/
void MM_NavBake(float gridArg) {
	const float grid = (gridArg > 0.0f) ? clamp(gridArg, MIN_GRID, MAX_GRID) : DEFAULT_GRID;

	std::vector<NavNode> nodes;
	std::unordered_map<int64_t, std::vector<int32_t>> cells; // cell -> node indices

	// Find a standable origin at lattice cell (ix,iy) near reference origin-Z.
	// Sweeps the player hull straight down; endpos is the standing origin.
	auto FindFloor = [&](int ix, int iy, float refZ, vec3_t &out) -> bool {
		const float x = ix * grid, y = iy * grid;
		const vec3_t start = { x, y, refZ + STEP_UP + 1.0f };
		const vec3_t end   = { x, y, refZ - MAX_DROP - 1.0f };
		trace_t tr = gi.trace(start, PLAYER_MINS, PLAYER_MAXS, end, nullptr, MASK_NAV_SOLID);
		if (tr.startsolid || tr.allsolid) return false;
		if (tr.fraction == 1.0f) return false;          // no floor in range
		if (tr.plane.normal.z < MIN_FLOOR_NZ) return false; // too steep
		const contents_t c = gi.pointcontents(tr.endpos);
		if (c & (CONTENTS_LAVA | CONTENTS_SLIME)) return false; // hazard liquid
		out = tr.endpos;
		return true;
	};

	// Can the player hull travel from a to b unobstructed?
	auto Reachable = [&](const vec3_t &a, const vec3_t &b) -> bool {
		trace_t tr = gi.trace(a, PLAYER_MINS, PLAYER_MAXS, b, nullptr, MASK_NAV_SOLID);
		if (tr.startsolid) return false;
		return (tr.endpos - b).length() <= 1.0f;
	};

	auto FindExisting = [&](int ix, int iy, float z) -> int32_t {
		auto it = cells.find(CellKey(ix, iy));
		if (it != cells.end())
			for (int32_t idx : it->second)
				if (fabsf(nodes[idx].origin.z - z) <= Z_MERGE)
					return idx;
		return -1;
	};

	std::queue<int32_t> bfs;

	auto AddNode = [&](int ix, int iy, const vec3_t &o) -> int32_t {
		int32_t idx = static_cast<int32_t>(nodes.size());
		nodes.push_back(NavNode{ o, {} });
		cells[CellKey(ix, iy)].push_back(idx);
		bfs.push(idx);
		return idx;
	};

	auto AddLink = [&](int32_t a, int32_t b) {
		if (a == b) return;
		for (int32_t t : nodes[a].links)
			if (t == b) return;
		nodes[a].links.push_back(b);
	};

	// --- seed the flood ----------------------------------------------------
	// Spawn spots first, then item pickups and players, so disconnected regions
	// (across gaps the walk-flood can't cross) still get covered.
	auto Seed = [&](const vec3_t &origin) {
		const int ix = LatticeIndex(origin.x, grid);
		const int iy = LatticeIndex(origin.y, grid);
		vec3_t o;
		if (FindFloor(ix, iy, origin.z, o) && FindExisting(ix, iy, o.z) < 0)
			AddNode(ix, iy, o);
	};

	for (int i = 0; i < level.num_spawn_spots; i++)
		if (level.spawn_spots[i])
			Seed(level.spawn_spots[i]->s.origin);

	for (uint32_t i = 0; i < globals.num_entities; i++) {
		gentity_t *e = &g_entities[i];
		if (!e->inuse) continue;
		if (e->item || e->client)
			Seed(e->s.origin);
	}

	if (nodes.size() < 2) {
		gi.Com_PrintFmt("nav_bake: not enough seed points on \"{}\" (found {} node(s)). "
			"Run on a loaded map with spawn spots or items.\n", level.mapname, nodes.size());
		return;
	}

	// --- flood fill (8-neighbour lattice) ----------------------------------
	static const int DIRS[8][2] = {
		{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
		{ 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }
	};

	while (!bfs.empty() && nodes.size() < MAX_NODES) {
		int32_t ci = bfs.front();
		bfs.pop();
		const vec3_t co = nodes[ci].origin;
		const int cix = LatticeIndex(co.x, grid);
		const int ciy = LatticeIndex(co.y, grid);

		for (auto &d : DIRS) {
			const int nx = cix + d[0], ny = ciy + d[1];
			vec3_t no;
			if (!FindFloor(nx, ny, co.z, no)) continue;
			if (!Reachable(co, no)) continue;

			int32_t ni = FindExisting(nx, ny, no.z);
			if (ni < 0)
				ni = AddNode(nx, ny, no);

			AddLink(ci, ni);
			AddLink(ni, ci);
		}
	}

	// --- serialize NAV3 v6 -------------------------------------------------
	uint32_t linkCount = 0;
	for (auto &n : nodes) linkCount += static_cast<uint32_t>(n.links.size());

	std::vector<uint8_t> buf;
	buf.reserve(24 + nodes.size() * 20 + linkCount * 6 + 4);

	// header
	buf.push_back('N');
	buf.push_back('A');
	buf.push_back('V');
	buf.push_back('3');
	PutU32(buf, 6);                                  // version
	PutU32(buf, static_cast<uint32_t>(nodes.size())); // node_count
	PutU32(buf, linkCount);                          // link_count
	PutU32(buf, 0);                                  // traversal_count
	PutF32(buf, 0.8f);                               // constant

	// node table (CSR): flags, num_links, first_link, radius
	uint16_t firstLink = 0;
	for (auto &n : nodes) {
		PutU16(buf, 0);
		PutU16(buf, static_cast<uint16_t>(n.links.size()));
		PutU16(buf, firstLink);
		PutU16(buf, NODE_RADIUS);
		firstLink = static_cast<uint16_t>(firstLink + n.links.size());
	}

	// origins
	// Nodes are tracked internally at the player *standing* origin (floor + 24,
	// since PLAYER_MINS.z == -24) because that is what the reachability hull
	// sweeps need. The .nav format, however, stores the floor contact point
	// (stock files sit ~0.5u above the floor brush). Writing the standing origin
	// leaves every node hovering 24u in the air, which stops the engine binding
	// bots to a start node. Convert back to the floor here.
	constexpr float FLOOR_OFFSET = PLAYER_MINS.z + 0.5f; // == -23.5
	for (auto &n : nodes) {
		PutF32(buf, n.origin.x);
		PutF32(buf, n.origin.y);
		PutF32(buf, n.origin.z + FLOOR_OFFSET);
	}

	// links (walk, no traversal)
	for (auto &n : nodes)
		for (int32_t target : n.links) {
			PutU16(buf, static_cast<uint16_t>(target));
			buf.push_back(LINK_WALK);
			buf.push_back(static_cast<uint8_t>(LINK_FLAGS));
			PutU16(buf, TRAV_NONE);
		}

	// traversal section is empty; trailing edict_count = 0
	PutU32(buf, 0);

	// --- write file --------------------------------------------------------
	const char *dir = "baseq2/bots/navigation";
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);

	const char *path = G_Fmt("{}/{}.nav", dir, level.mapname).data();
	FILE *f = fopen(path, "wb");
	if (!f) {
		gi.Com_PrintFmt("nav_bake: failed to open \"{}\" for writing.\n", path);
		return;
	}
	fwrite(buf.data(), 1, buf.size(), f);
	fclose(f);

	gi.Com_PrintFmt("nav_bake: wrote \"{}\" - {} nodes, {} links, grid {} ({} bytes).\n",
		path, nodes.size(), linkCount, grid, buf.size());
	if (nodes.size() >= MAX_NODES)
		gi.Com_PrintFmt("nav_bake: WARNING node cap ({}) hit; map may be incompletely covered.\n", MAX_NODES);
	gi.Com_PrintFmt("nav_bake: open it in QuakeNavEditor to add teleporters/jumps/lifts.\n");
}
