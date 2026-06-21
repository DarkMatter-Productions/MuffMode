# Quake II Re-release Bot Navigation File Format (`NAV3` v6)

Reverse-engineered from the stock `.nav` files shipped under
`packaging/release-assets/rerelease/bots/navigation/`. Every field below was
validated byte-for-byte against all 20 stock files (node/link/traversal counts,
section sizes, and EOF all reconcile exactly).

This is the format the **KEX engine** loads for bot navigation via
`gi.GetPathToGoal(...)`. The game DLL never reads or writes it directly — the
engine compiles/loads it per map. MuffMode's `sv nav_bake` command
(`src/muffmode/mm_nav_bake.cpp`) writes this format.

> Endianness: little-endian throughout. Coordinates are standard Quake world
> units (X, Y, Z). All offsets are byte offsets from start of file.

---

## File layout

```
+-----------------------------+
| Header            (24 bytes)|
+-----------------------------+
| Node table   node_count * 8 |   adjacency + radius (no positions here)
+-----------------------------+
| Origins      node_count * 12|   one vec3 (3*f32) per node, same order
+-----------------------------+
| Links        link_count * 6 |   flat array; nodes index into it (CSR)
+-----------------------------+
| Traversals   trav_count * 48|   funnel data for special links
+-----------------------------+
| edict_count       (4 bytes) |   u32; 0 in every stock file
+-----------------------------+
| Edicts       edict_count * ?|   (not present in any stock file; see below)
+-----------------------------+
```

The four section counts (`node_count`, `link_count`, `trav_count`) live in the
header; `edict_count` is a `u32` written *after* the traversal section.

---

## Header (24 bytes)

| Offset | Type     | Field          | Notes                                        |
|-------:|----------|----------------|----------------------------------------------|
| 0x00   | char[4]  | magic          | `"NAV3"` (`4E 41 56 33`)                      |
| 0x04   | u32      | version        | `6`                                          |
| 0x08   | u32      | node_count     |                                              |
| 0x0C   | u32      | link_count     | total across all nodes                       |
| 0x10   | u32      | traversal_count|                                              |
| 0x14   | f32      | (constant)     | `0.8` in every stock file — global tuning value, exact meaning unconfirmed; write `0.8f` |

---

## Node table — `node_count × 8 bytes`

A pure adjacency/metadata table. Node **positions are not here** — they are in
the parallel Origins block at the same index.

| Offset | Type | Field      | Notes                                                        |
|-------:|------|------------|--------------------------------------------------------------|
| +0     | u16  | flags      | node flags bitfield; `0` for nearly all stock nodes          |
| +2     | u16  | num_links  | how many links this node owns                                |
| +4     | u16  | first_link | index of this node's first link in the Links array           |
| +6     | u16  | radius     | acceptance radius; **32** for ~95% of stock nodes (a few use 4/8/12/16) |

`first_link` is the running prefix-sum of `num_links` (CSR layout): node 0
starts at link 0, node *i*'s links are `links[first_link .. first_link+num_links)`.
Verified: e.g. `mm-pkill` node table begins `(0,1,0,32) (0,3,1,32) (0,3,4,32)
(0,2,7,32) …` → first_link = 0,1,4,7,… = cumulative link counts.

---

## Origins — `node_count × 12 bytes`

One `vec3` (3 × f32: x, y, z) per node, in node order. This is the node's world
position — the player *standing origin* (entity origin), not the floor point.
(E.g. on `mm-pkill` the flat arena floor is ~168 and origins read z≈192.5 =
floor − `PLAYER_MINS.z` (24).)

---

## Links — `link_count × 6 bytes`

Flat, directional array. Each node owns a contiguous run (see CSR above). Links
are **one-way**; a two-way connection is two link records (one per direction).

| Offset | Type | Field           | Notes                                                       |
|-------:|------|-----------------|-------------------------------------------------------------|
| +0     | u16  | target_node     | index of the destination node                               |
| +2     | u8   | type            | link/traversal type (see table)                             |
| +3     | u8   | flags           | link flags bitfield; **`3`** on every stock link            |
| +4     | u16  | traversal_index | index into Traversals, or `0xFFFF` for none                 |

### Link types (`type` byte)

| Value | Name          | Needs traversal? |
|------:|---------------|------------------|
| 0     | Walk          | no (`0xFFFF`)    |
| 1     | LongJump      | yes              |
| 2     | WalkOffLedge  | usually no       |
| 3     | Teleport      | yes              |
| 4     | BarrierJump   | yes              |
| 5     | ManualJump    | yes              |
| 6     | Pusher        | yes              |
| 7     | Elevator      | yes              |
| 8     | Train         | yes              |
| 11,13,14 | (mover/elevator variants seen in bloodrun/rage/hiddenfortress; exact names unconfirmed) | yes |

Empirical rule from the stock set: **every non-Walk/non-WalkOffLedge link
consumes exactly one traversal record.** Clean proof — `campgrounds`: 6 LongJump
+ 12 Teleport = 18 specials = 18 traversals. Teleport↔traversal counts match
1:1 on `mm-almostlost` (32/32), `mm-aerow` (14/14), `mm-ironox` (17/17),
`mm-pkill` (1/1), etc. Plain Walk links carry `traversal_index = 0xFFFF`.

---

## Traversals — `traversal_count × 48 bytes`

Funnel geometry for a special move (jump arc, teleport, lift ride). 48 bytes =
**4 × vec3** (`p1..p4`, each 3 × f32):

| Point | Typical role                                  | Stock observation                                  |
|-------|-----------------------------------------------|----------------------------------------------------|
| p1    | reserved / optional point                     | the sentinel `0x7149F2CA` (≈4.1e29) = "unused"     |
| p2    | traversal **start / launch** (higher point)   | e.g. campgrounds trav0 `(-367.9, 446.2, 514.5)`    |
| p3    | traversal **end / land** (lower point)        | e.g. campgrounds trav0 `(-424.4, 422.5, 278.5)`    |
| p4    | reserved                                       | `(0,0,0)`                                          |

Unused point slots are filled with the float sentinel `0x7149F2CA` (p1) or zero
(p4). Walk-only nav files have `traversal_count = 0` and omit this section
entirely.

---

## Edicts — trailing `u32 edict_count` + records

After the traversal section there is a `u32 edict_count`. **It is `0` in every
shipped stock file**, so the per-edict record layout is not exercised by any
reference data and is left unspecified here. Edicts associate a link/traversal
with a world entity's bounding box (`mins`/`maxs`) — e.g. moving geometry — and
are surfaced as yellow boxes in JPiolho's QuakeNavEditor. A walk-only writer
should emit `edict_count = 0` and stop.

---

## Minimal walk-only file (what `sv nav_bake` writes)

```
"NAV3", version=6, node_count=N, link_count=L, traversal_count=0, 0.8f
node[N]    : { flags=0, num_links, first_link, radius=32 }
origin[N]  : { x, y, z }
link[L]    : { target, type=0(Walk), flags=3, traversal=0xFFFF }
u32 edict_count = 0
```

That is a complete, engine-loadable nav graph. Teleporters, jumps, lifts, and
other movers are then added by hand in QuakeNavEditor (each as the appropriate
non-Walk link type plus a traversal record), since they cannot be inferred from
floor sampling alone.

---

## Reference stats (stock files)

| Metric                         | Observed                                  |
|--------------------------------|-------------------------------------------|
| Node count (arena DM)          | 65–175                                    |
| Avg nearest-node spacing       | ~96–130 u (global avg 105)                |
| Node radius                    | 32 (default); 4/8/12/16 in tight spots    |
| Links per node                 | ~2.2–2.6                                   |
| Dominant link type             | Walk (~95%)                               |
| Specials per map               | 1–32 (teleports/jumps/movers)             |

Use `mm-pkill.nav` (77 nodes, 1 teleport) or `campgrounds.nav` (classic q3dm6
layout, good jump examples) as bring-up references.
