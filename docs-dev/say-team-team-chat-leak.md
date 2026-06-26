# `say_team` team chat leaks to everyone (KEX lobby)

**Status:** OPEN / unresolved as of 2026-06-26 (v0.40.2). Multiple fixes attempted,
none confirmed working in-game. Code currently carries a one-line speculative fix
(see [Current code state](#current-code-state)).

## Symptom

In team modes (reproduced reliably in **TDM with both teams populated**), a player
who sends team chat (press **T → team**, or `say_team`) has the message delivered to
**all players on the server**, not just their own team. Global `say` and team
`say_team` are indistinguishable in delivery.

- **Does NOT occur in stock/`Vanilla`** Q2 Rerelease (`e:\Projects\muffmode\Vanilla`)
  running on the same engine. So this is a MuffMode-specific divergence/regression,
  not an engine bug.
- The user recalls it being fixed by commit
  [`7f1e971`](https://github.com/DarkMatter-Productions/MuffMode/commit/7f1e97185be03d1f3cba45017cebda37c533bcfe)
  ("Fix say_team by mapping sess.team to engine team_index values") and later
  regressing — hence "incredibly fragile".

## Confirmed facts (from in-game probes)

Probe builds logged `[cmddiag]` (every `ClientCommand`) and `[teamdiag]`
(`P_AssignClientSkinnum` team fields). Cross-referenced with engine RE notes in
`e:\Projects\q2kex-reverse\rev\` (`SAY_TEAM_FLOW.md`, `FINDINGS.md`).

1. **`say` / `say_team` never reach the game DLL under `KEX_Q2_GAME`.**
   26 `[cmddiag]` lines captured in a session; **zero** for `say`/`say_team`/`steam`
   (normal commands like `drop`, `inven`, `invnext`, `invuse`, `loc` all appeared).
   The engine's Steam **lobby chat control** owns chat end-to-end. Console shows
   `RegisterChatControl` / `UpdateChatControls Steamworks <id> 0`.
   → **Intercepting `say_team` in `ClientCommand` cannot work** (handlers are also
   `#ifndef KEX_Q2_GAME` compiled out anyway).

2. **The engine filters team chat using team data the DLL publishes.** Per RE notes
   the relevant fields are, in priority order of suspicion:
   - `entity_state_t.skinnum` team_index nibble (4 bits: 0=none, 1=red, 2=blue) —
     `7f1e971`'s commit message states the lobby "filters on skinnum team_index".
   - `player_state_t.team_id` (`ps.team_id`), replicated on the wire via `PS_TEAM_ID`
     — `SAY_TEAM_FLOW.md` flowchart says "Team filter reads ps.team_id" (this read
     site is **inferred, not yet decompiled**).
   - `gentity_t::sv.team`.

3. **For a spawned/alive player, all three published values are CORRECT.**
   `[teamdiag]` for an alive BLUE player: `sess.team=4 want_idx=2 skin.team_idx=2
   sv.team=2 ps.team_id=2 modelidx=255`. RED resolves to 1. So the steady-state
   values are not the problem.

4. **The engine appears to cache team=0 for the chat control.** Despite the correct
   alive-state values, `UpdateChatControls Steamworks <id> 0` shows team `0`,
   consistent with a snapshot taken at a moment when the published team was still 0
   (spectator / pre-spawn window) and never refreshed.

## Root-cause hypothesis (still unconfirmed)

A **publish-timing / stale-snapshot** problem, not a wrong-value problem:

- The team nibble in `skinnum` is only packed once the player has an active body
  (`P_AssignClientSkinnum` early-returns when `ent->s.modelindex != 255`).
- `ClientSpawn` does `memset(&ps, 0, ...)`, which clears `ps.team_id`; MuffMode (unlike
  stock q2re) did not restore it inline and relied on the *next* frame's `G_SetStats`.
- The engine's lobby chat control snapshots a player's team **at/around the
  spectator→team join and spawn**, i.e. inside the window where MuffMode's published
  team is still 0. It caches 0 and never refreshes → team chat broadcasts to all.

This explains why `Vanilla` works (it sets `ps.team_id` the instant `ps` is cleared,
so any snapshot reads 1/2) and why `7f1e971` "fixed then regressed" (it corrected the
*value* 3/4→1/2 but never the *timing*).

**Caveat:** this hypothesis is not proven. The decisive engine read/snapshot site is
still un-decompiled (`kexQuakeLobbyMonitor ~0x1402C9D6D`, `mmChatChannel` callback in
`CL_InitLocal`). It is possible the engine keys team off a **userinfo string** rather
than entity/player state (open question in `FINDINGS.md`), in which case none of the
entity-state publishing below can fix it.

## What was tried and did NOT fix it

| # | Attempt | Result |
|---|---------|--------|
| 1 | Intercept `say_team`/`steam` in `ClientCommand` and unicast `svc_print` to same-team clients (mirroring `mm_loc`, which *does* keep loc callouts team-private). | **No effect.** Confirmed via `[cmddiag]` that `say_team` never reaches the game DLL under KEX. Reverted. |
| 2 | Publish engine team early + keep it consistent across the board: new `P_PublishEngineTeam`/`P_GetPublishedTeamIndex` helpers setting `ps.team_id` + `sv.team` + skinnum nibble with **no `modelindex==255` gate**; called from `SetTeam` (after `sess.team` set), `P_AssignClientSkinnum` (even pre-spawn), `Player_UpdateState` (authoritative `sv.team` instead of decoding stale skinnum), and `G_SetStats`/`G_SetSpectatorStats`. | **Did not fix.** Also note `SetTeam` calls `ClientSpawn`, whose `memset` wipes the just-published `ps.team_id`. Reverted as over-engineered/fragile. |
| 3 | Minimal vanilla parity: restore `ps.team_id = P_EngineTeamIndex(sess.team)` in `ClientSpawn` immediately after the `memset` (matches stock q2re `p_client.cpp:2188`). | **Did not fix.** Left in the tree (harmless, correct, matches vanilla). |

Common thread: every entity-state/player-state publishing approach leaves the alive
values correct, yet the lobby still broadcasts — pointing at either a snapshot the
DLL cannot refresh, or a field (userinfo) we have not identified.

## Current code state

The tree retains **only attempt #3** (v0.40.2): one line in `ClientSpawn`
(`src/sgame/client/lifecycle.cpp`, just after the `ps` memset):

```cpp
ent->client->ps.team_id = P_EngineTeamIndex(ent->client->sess.team);
```

This is correct and matches vanilla, but does **not** by itself resolve the leak. All
other experimental changes were reverted. No diagnostic (`[cmddiag]`/`[teamdiag]`)
code remains in the shipped path.

## Reference: how `Vanilla` (working) differs

Stock `e:\Projects\muffmode\Vanilla\p_client.cpp`:
- `P_AssignClientSkinnum` (line ~1741): packs `team_index = resp.ctf_team` (already
  1/2), sets **only** `ent->s.skinnum`.
- `ClientSpawn` (line ~2188): `memset(&ps,0)` then immediately
  `ps.team_id = resp.ctf_team`.
- Awaiting-respawn / intermission path (line ~2070): also sets
  `ps.team_id = resp.ctf_team` even with `modelindex = 0`.

i.e. vanilla's invariant is **"`ps.team_id` is always correct wherever `ps` is
touched."** MuffMode replaced that with a per-frame `G_SetStats` patch (`hud.cpp`)
plus skinnum packing, which has a spawn-frame hole.

## Suggested next steps (for whoever resumes)

The decisive datapoint is the engine-side read/snapshot, which needs RE work:

1. **Confirm the snapshot field + timing** via Ghidra at `kexQuakeLobbyMonitor`
   (`~0x1402C9D6D`, xref `0x1402CA1F6`) and the `mmChatChannel` callback registered
   in `CL_InitLocal`. Determine whether team is read from `ps.team_id`, skinnum, or a
   userinfo key, and whether it's per-message (live) or cached at channel
   registration.
2. **If snapshot at lobby-join (before team pick):** publish team on `ClientBegin`
   too — but note the player is a spectator (team 0) then, so this only helps if the
   engine re-reads on a later trigger. There is **no `game_import_t` API** to force a
   chat-control refresh.
3. **If keyed off userinfo:** set a team userinfo key in `ClientUserinfoChanged`
   (open question in `FINDINGS.md`: "Which userinfo keys does lobby HUD read?"). This
   is the most promising untried lever because it's the one input class we have NOT
   yet varied, and the lobby/HUD demonstrably reads userinfo.
4. **A/B against `Vanilla`:** run the same engine with the stock DLL and a MuffMode
   DLL instrumented to dump the exact bytes of `ps`, `skinnum`, and userinfo at the
   moment `UpdateChatControls` logs, to see what stock publishes that MuffMode does
   not.

## Related

- Engine RE workspace: `e:\Projects\q2kex-reverse\rev\SAY_TEAM_FLOW.md`,
  `FINDINGS.md`, `TAPIN_CATALOG`, `ghidra/BOOKMARKS.md`.
- Working reference source: `e:\Projects\muffmode\Vanilla\`.
- Fix that historically worked then regressed: commit `7f1e971`.
- MuffMode `mm_loc` (`src/sgame/muffmode/mm_loc.cpp`) — the *working* pattern for
  team-private delivery, but only usable for game-owned commands, not engine-owned
  `say_team`.
