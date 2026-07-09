# Horde Custom Nav Format Design (`.hnav`)

## Context

MuffMode horde mode (`GT_HORDE`) reuses stock Q2 Rerelease monster movement, which pathfinds
exclusively against the engine's compiled per-map nav mesh via `gi.GetPathToGoal(...)`
(`src/monsters/m_move.cpp:1070`). Maps without engine nav data return
`PathReturnCode::NoNavAvailable`, latch `AI_NO_PATH_FINDING` (`m_move.cpp:1072-1073`), and fall
back to straight-line chasing that snags on geometry. The companion plan
(`horde-ai-improvement-plan.md`) treats this as a hard ceiling no DLL change can lift.

This document specifies a **horde-only, MuffMode-owned pathfinding system** — a custom nav graph
format (`.hnav`) plus a loader/pathfinder module — that lifts that ceiling **for `GT_HORDE`
only**, without touching the engine nav toolchain and without a feature-body rewrite of vanilla
AI.

### Scope and non-goals

- **In scope:** horde monster pathing on maps where this graph is loaded.
- **Out of scope:** bots. Bots still pathfind through the engine nav mesh; they gain nothing from
  `.hnav`. Maps that want both bot support *and* horde nav need both files. This is a second nav
  source per map, not a replacement.
- **Out of scope (v1):** doors, elevators, and other movers; long jumps; swim links. Walk-only,
  same-floor graph first (see [Traversals](#traversals-deliberately-minimal-in-v1)).

---

## The key architectural insight

`M_NavPathToGoal` (`m_move.cpp:1034`) does only two separable things:

1. **Produce a path** — calls `gi.GetPathToGoal(request, self->monsterinfo.nav_path)` to fill the
   `PathInfo` struct `self->monsterinfo.nav_path` (`m_move.cpp:1043-1077`).
2. **Follow that path** — steers yaw toward `nav_path.firstMovePoint` / `secondMovePoint`, with
   all blocked-recovery, `G_NewChaseDir`, and step logic downstream (`m_move.cpp:1080-1133`).

The follow half consumes only the `PathInfo` struct and is agnostic to who filled it. Therefore a
custom nav system is **not a parallel movement engine** — it is a function that produces a
`PathInfo`. Everything downstream of the fill is reused unchanged. This is what keeps the change
inside thin-vanilla limits: one marked hook at the fill site, zero changes to locomotion.

### Engine structs we must honor (`src/game.h`)

`PathRequest` (`game.h:1590`) — the **input**, already populated by vanilla before the hook:

```cpp
struct PathRequest {
    gvec3_t   start;       // self->s.origin
    gvec3_t   goal;        // enemy/goalentity origin
    PathFlags pathFlags;   // Walk (+ jump/ledge/fly bits set by vanilla per monster)
    float     moveDist;    // this frame's move distance
    // nodeSearch / traversals / pathPoints — for the engine pather; we may ignore
};
```

`PathInfo` (`game.h:1618`) — the **output** our function fills:

```cpp
struct PathInfo {
    int32_t        numPathPoints;   // optional (engine sets; we can set 0/approx)
    float          pathDistSqr;     // optional
    gvec3_t        firstMovePoint;  // REQUIRED: next waypoint to steer toward
    gvec3_t        secondMovePoint; // REQUIRED only for TraversalPending paths
    PathLinkType   pathLinkType;    // Walk for v1
    PathReturnCode returnCode;      // REQUIRED: see below
};
```

`PathReturnCode` (`game.h:1555`): any code `>= StartPathErrors` is an error. To signal success we
set `InProgress` mid-path, or `ReachedPathEnd` when the goal node is the closest reachable node.
Codes `< StartPathErrors` (`ReachedGoal`, `ReachedPathEnd`, `TraversalPending`, `RawPathFound`,
`InProgress`) are all treated as "path produced."

The contract is identical to `gi.GetPathToGoal`: same inputs in `request`, same `PathInfo` out.
Because of that, the follow code at `m_move.cpp:1080-1133` needs no edits.

---

## The hook (one marked edit, `m_move.cpp:1070`)

Replace:

```cpp
if (!gi.GetPathToGoal(request, self->monsterinfo.nav_path)) {
    // fatal error, don't bother ever trying nodes
    if (self->monsterinfo.nav_path.returnCode == PathReturnCode::NoNavAvailable)
        self->monsterinfo.aiflags |= AI_NO_PATH_FINDING;
    return false;
}
```

with:

```cpp
// [MuffMode] horde: pathfind on our own loaded nav graph first; fall through to
// the engine nav mesh when no horde graph exists for this map.
bool got_path = false;
if (GT(GT_HORDE) && g_horde_nav->integer)
    got_path = MM_HordeNav_GetPathToGoal(self, request, self->monsterinfo.nav_path);

if (!got_path && !gi.GetPathToGoal(request, self->monsterinfo.nav_path)) {
    // Only latch AI_NO_PATH_FINDING when there is no nav source at all. If a horde
    // graph is loaded, never latch it — keep retrying so straight-line chase stays live.
    if (self->monsterinfo.nav_path.returnCode == PathReturnCode::NoNavAvailable &&
        !MM_HordeNav_Loaded())
        self->monsterinfo.aiflags |= AI_NO_PATH_FINDING;
    return false;
}
```

`g_horde_nav` is a new cvar (see [cvars](#cvars)); `extern cvar_t *g_horde_nav;` goes near the
existing externs in `m_move.cpp`.

### Why the `AI_NO_PATH_FINDING` change matters

Today, the first failed engine path on a nav-less map permanently latches `AI_NO_PATH_FINDING`
(`m_move.cpp:1072`), and `M_MoveToPath` then early-outs forever (`m_move.cpp:1147`). With a horde
graph loaded we must **not** latch: a graph that momentarily can't find a path (e.g. monster
spawned off-graph) should still degrade to straight-line chase and recover next tick, not go
dormant for the rest of the map. Gating the latch on `!MM_HordeNav_Loaded()` preserves vanilla
behavior everywhere a horde graph is absent.

### Free per-monster path cache

`M_NavPathToGoal` only re-requests a path when the cached waypoint is reached *or*
`nav_path_cache_time <= level.time` (`m_move.cpp:1041-1042`), then sets
`nav_path_cache_time = level.time + 2_sec` (`m_move.cpp:1077`). The hook sits **inside** that
guard, so `MM_HordeNav_GetPathToGoal` is called at most ~once per monster per 2s (or on waypoint
arrival). We inherit path caching for free — no new cache plumbing, and A* cost is bounded by that
cadence rather than by frame rate.

---

## Module: `src/muffmode/mm_horde_nav.{h,cpp}`

Standard module registration:

1. Create `src/muffmode/mm_horde_nav.{h,cpp}`.
2. Add `#include "mm_horde_nav.h"` to `src/muffmode/muffmode.h` (alphabetical — after
   `mm_horde.h`).
3. Register both files in `src/game.vcxproj` and `src/game.vcxproj.filters` under the `muffmode`
   filter (alphabetical).

### Public API (`mm_horde_nav.h`)

```cpp
struct gentity_t;
struct PathRequest;
struct PathInfo;

// Map lifecycle.
void MM_HordeNav_LoadForMap();   // call at horde map init: level.mapname -> .hnav
void MM_HordeNav_Shutdown();     // free graph (map change / GT change away from horde)
bool MM_HordeNav_Loaded();       // is a graph live for this map?

// Path provider, contract-compatible with gi.GetPathToGoal.
// Returns true and fills `out` on success; false to let the engine nav try.
bool MM_HordeNav_GetPathToGoal(gentity_t *self, const PathRequest &req, PathInfo &out);

// Optional offline-author aid (see Generation).
void MM_HordeNav_Bake();         // server cmd: grid-sample + trace-validate -> dump .hnav
```

`MM_HordeNav_LoadForMap` hooks in at the same point horde initializes its per-map state. It is a
no-op (and `MM_HordeNav_Loaded()` stays false) when not in `GT_HORDE` or when no `.hnav` exists,
so the hook above transparently falls through to engine nav.

### Internal representation

```cpp
struct hnav_node_t {
    vec3_t  origin;
    float   radius;        // acceptance radius for "reached"
    // adjacency stored CSR-style in a flat link array:
    int32_t link_first;    // index into links[]
    int32_t link_count;
};

struct hnav_link_t {
    int32_t to;
    uint8_t type;          // 0 = walk (v1); jump/drop later
};

struct hnav_graph_t {
    std::vector<hnav_node_t> nodes;
    std::vector<hnav_link_t> links;
    // Coarse uniform grid bucket: world XY -> small list of node ids, for
    // O(1)-ish nearest-node lookup instead of scanning every node per query.
    // Cell size ~128u; tune against node density.
};
```

### `MM_HordeNav_GetPathToGoal` algorithm

1. `start_node = NearestReachableNode(req.start)`; `goal_node = NearestReachableNode(req.goal)`
   via the grid bucket. If either is missing, return `false` (engine nav / straight-line takes
   over).
2. If `start_node == goal_node` *or* the goal is within line-of-sight of the monster, set
   `firstMovePoint = req.goal`, `returnCode = ReachedPathEnd`, return `true` (don't waypoint when
   you can make a beeline — matches how the goal is the real target).
3. A* from `start_node` to `goal_node` over walk links; Euclidean heuristic. Cache the result on
   the monster implicitly via the existing 2s `nav_path_cache_time` window — no extra storage.
4. Fill `firstMovePoint` = origin of the next node on the path (or `req.goal` if that node is the
   goal node). Set `secondMovePoint` only once traversal links exist; for walk-only v1 leave it
   equal to `firstMovePoint`. `returnCode = InProgress`. `pathLinkType = Walk`. Return `true`.
5. No path found → return `false`.

For horde scale (many monsters, one/few moving goals) this is fine behind the 2s cache. If a
profiling spike shows A* cost (e.g. a full wave re-pathing the same frame), the escalation is a
single shared **flow field** rebuilt toward the living-player cluster every N ms, from which each
monster reads its `firstMovePoint` in O(1) — simpler per-monster than A* and naturally batched.
Treat that as a later optimization, not v1.

---

## File format: `.hnav`

Mirror the bot nav layout. Engine bot navs live at
`packaging/release-assets/rerelease/bots/navigation/*.nav`; place horde navs at:

```
packaging/release-assets/rerelease/bots/horde/<mapname>.hnav
```

Loader resolves `bots/horde/<level.mapname>.hnav` through the game filesystem at map init.

JSON — git-diffable, tool-friendly, human-patchable:

```json
{
  "map": "mm-pkill",
  "version": 1,
  "nodes": [
    { "id": 0, "o": [128, -64, 24], "r": 48 },
    { "id": 1, "o": [256,  32, 24], "r": 48 }
  ],
  "links": [
    { "a": 0, "b": 1, "t": "walk" }
  ]
}
```

- `o` = origin `[x,y,z]`, `r` = acceptance radius.
- `links` are **bidirectional** by default for walk; the loader expands each into two CSR entries.
- `t` (link type) is `"walk"` in v1. `"jump"` / `"drop"` are reserved and ignored until traversal
  support lands.
- `version` gates format migrations; loader rejects unknown major versions and falls through to
  engine nav.

A minimal hand-rollable JSON reader is enough (no external dependency needed); validate node-id
references in `links` and bounds-check on load, logging and falling through on any malformed graph
rather than crashing.

### Traversals: deliberately minimal in v1

Walk-only, same-floor links cover roughly 80% of "get to the player" behavior on arena-style horde
maps (e.g. `mm-pkill`) for ~20% of the engine nav's complexity. Add `"jump"` links only where the
baker or a human flags a vertical gap under the monster's `jump_height` with a clear trace; only
then do `secondMovePoint` / `TraversalPending` / `pathLinkType` need real population (the vanilla
follow code already handles `TraversalPending` at `m_move.cpp:1038-1039`). Doors/elevators are
out for v1.

---

## Generation, in priority order

### 1. In-DLL bake command (recommended first)

A server command `mm_horde_nav_bake` that, in a running horde map:

- Grid-samples floor points around `info_player_deathmatch` spots and the horde spawn spots
  already gathered in `level.spawn_spots` (`mm_horde.cpp:685`).
- Drops each sample to the floor with a downward `gi.trace`; keeps valid standable points as nodes.
- Connects neighbor nodes with a walk link when a monster-hull sweep (`gi.trace` with a
  representative monster bounding box) between them is clear and the floor height delta is small.
- Writes `bots/horde/<mapname>.hnav`.

This is the cheapest path to a **correct** graph: collision comes free from the live `gi.trace`,
so there is no offline BSP/brush parser to write or keep in sync with the compiler. The existing
`utils/mm-pkill.nav` can serve as a reference to diff node coverage against during bring-up.

### 2. Offline BSP/.map tool (defer)

A standalone tool that reads the BSP and approximates collision from brushes to emit `.hnav`
without a running server. Strictly worse than the in-DLL bake for equal effort (it re-implements
collision approximately), and only worth it if you later need fully unattended generation across a
large rotation. Defer until v1 proves out.

In both cases, hand-patching the resulting JSON (add/remove links around problem geometry like
jump pads or thin ledges) is the expected finishing step.

---

## cvars

Follow the standard horde-cvar wiring (`horde-ai-improvement-plan.md` cvar pattern):

| cvar | default | purpose |
| --- | --- | --- |
| `g_horde_nav` | `1` | master enable for horde custom nav; off → engine nav only |

1. Declare global in `src/g_main.cpp` (with the other `g_horde_*` declarations).
2. Register `gi.cvar("g_horde_nav", "1", CVAR_NOFLAGS)` in `src/g_main.cpp`.
3. `extern cvar_t *g_horde_nav;` where used (`m_move.cpp` near its externs; `mm_horde_nav.cpp`).
4. Document in `docs/configuration-reference.md` (existing doc; no new user-facing markdown).

Optional later cvars: `g_horde_nav_debug` (draw the graph + active paths) and a perf toggle to
switch A* → flow field.

---

## Sequencing relative to the AI plan

This module slots in as **Tier 1.5** in the horde AI roadmap: larger than a marked hook, smaller
than the Tier 2 director. Tier 0/1 orchestration (target spread, re-acquire, spawn tactics) ships
value with zero nav work and should land first. `mm_horde_nav` is what specifically unblocks
**nav-less maps**, so it is the right next step once Tier 0/1 are validated in-game.

When this lands, amend the "Nav-mesh dependency" hard-constraint bullet in
`horde-ai-improvement-plan.md:35-39`: the ceiling it describes is lifted for `GT_HORDE` on maps
that ship an `.hnav`.

---

## Risks / things to validate

| Area | Risk | Mitigation |
| --- | --- | --- |
| Thin vanilla | New code in `m_move.cpp` | Single marked hook at the fill site; locomotion untouched |
| Perf | Full wave re-pathing same frame | 2s `nav_path_cache_time` bounds A* cadence; flow-field escalation if needed |
| Quality | Auto graph misses jump pads / thin ledges | Hand-patch `.hnav`; explicit `"jump"` links only where validated |
| Bots | No benefit to bots | Documented non-goal; engine `.nav` still required for bots |
| Maintenance | Every shipped horde map needs an `.hnav` | In-DLL baker keeps authoring cheap; falls through to engine nav when absent |
| Latch regression | `AI_NO_PATH_FINDING` behavior change | Gated on `!MM_HordeNav_Loaded()`; vanilla path identical when no graph loaded |

## Verification

- **Build (CI parity):**
  `./scripts/ci/build-msbuild.ps1 -Configuration Release -Platform x64 -TreatWarningsAsErrors -BinaryLog`
- **Host tests:** `./scripts/ci/run-host-tests.ps1 -Configuration Release -Platform x64`. The
  graph loader/validator and A* are pure helpers — add an `MM_TEST` in `tests/host/` covering
  `.hnav` parse (valid + malformed) and a known-answer A* path on a small fixture graph.
- **Static analysis:** `./scripts/ci/run-clang-tidy.ps1 -Files src/muffmode/mm_horde_nav.cpp`
  (and the changed `m_move.cpp`).
- **Sanitizer:** `./scripts/ci/run-sanitized-build.ps1 -Sanitizer Address`.
- **In-game (required):** `./build.bat`, then a horde test on a nav-less map — confirm monsters
  route around geometry toward players instead of snagging on walls, the 2s cache holds frame
  cost flat under a full wave, and `g_horde_nav 0` cleanly reverts to engine-nav/straight-line
  behavior. Per the established workflow this manual check precedes any commit.

## Files to add / modify

- **add** `src/muffmode/mm_horde_nav.{h,cpp}` — loader, graph, A*, baker.
- `src/muffmode/muffmode.h` — `#include "mm_horde_nav.h"` (alphabetical).
- `src/game.vcxproj`, `src/game.vcxproj.filters` — register the new files.
- `src/monsters/m_move.cpp` — the marked path-provider hook + `g_horde_nav` extern.
- `src/g_main.cpp` — `g_horde_nav` declaration + registration.
- horde map-init path (`mm_horde.cpp`) — call `MM_HordeNav_LoadForMap()` / `_Shutdown()`.
- `src/g_local.h` + `VERSION` — matching patch bump.
- `docs/configuration-reference.md` — document `g_horde_nav`.
- `packaging/release-assets/rerelease/bots/horde/<map>.hnav` — ship one map (e.g. `mm-pkill`) as
  the proof.
