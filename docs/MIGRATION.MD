# MuffMode Thin Vanilla Migration Log

This document tracks migration progress from a monolithic fork style toward thin-vanilla structure.

## Scope

- Base: `quake2-rerelease-dll/rerelease`
- Mod target: `MuffMode/src`
- Goal: keep vanilla-named files as thin hooks and move MuffMode behavior into `src/muffmode/`.

## Progress So Far

### Phase 1 (Completed)

#### Module scaffold + debug migration

- Added new module folder and shared header entrypoint:
  - `src/muffmode/muffmode.h`
- Moved centralized debug logging implementation to:
  - `src/muffmode/mm_debug.h`
  - `src/muffmode/mm_debug.cpp`
- Kept compatibility for existing includes by converting:
  - `src/g_debug_log.h` into a shim that includes `muffmode/mm_debug.h`
- Updated project wiring:
  - `src/game.vcxproj`
  - `src/game.vcxproj.filters`

#### Validation

- Build passes after migration.
- Runtime logging behavior preserved (`MuffModeLog`, `MuffModeLog_Separator`, `g_muffmode_debug` gate).

### Phase 2 (In Progress)

#### Extracted map-list/shuffle subsystem

- Added map module API:
  - `src/muffmode/mm_maps.h`
- Added map module implementation:
  - `src/muffmode/mm_maps.cpp`

#### Migrations completed in this subsystem

1. **Map shuffle implementation extracted**
   - `G_ShuffleMapList()` in `g_main.cpp` is now a thin hook to `MM_ShuffleMapList()`.

2. **Gametype first-map transition extracted**
   - `SVCmd_GametypeChangeMapFirst_f()` in `g_svcmds.cpp` is now a thin hook to `MM_GametypeChangeMapFirst()`.

3. **Match-end map-list selection extracted**
   - `Match_End()` in `g_main.cpp` now delegates map-list intermission routing to `MM_TryBeginIntermissionFromMapList()`.

4. **Shuffle cvar-change handling extracted**
   - `GT_Changes()` in `g_main.cpp` now delegates to `MM_HandleMapShuffleCvarChange()`.

#### Extracted vote subsystem (partial)

- Added vote module API:
  - `src/muffmode/mm_vote.h`
  - `src/muffmode/mm_vote_menu.h`
- Added vote module implementation:
  - `src/muffmode/mm_vote.cpp`
  - `src/muffmode/mm_vote_menu.cpp`

#### Migrations completed in this subsystem

1. **Vote state machine extracted**
   - `TransitionVoteState()` in `g_cmds.cpp` is now a thin hook to `MM_TransitionVoteState()`.
   - `ClearVote()` in `g_cmds.cpp` is now a thin hook to `MM_ClearVote()`.
   - `Vote_Passed()` in `g_cmds.cpp` is now a thin hook to `MM_VotePassed()`.

2. **Gametype/ruleset vote execution extracted**
   - `Vote_Pass_Gametype()` in `g_cmds.cpp` now delegates to `MM_VotePassGametype()`.
   - `Vote_Pass_Ruleset()` in `g_cmds.cpp` now delegates to `MM_VotePassRuleset()`.

3. **Votability/list helpers extracted**
   - `IsGametypeVotable()` now delegates to `MM_IsGametypeVotable()`.
   - `IsRulesetVotable()` now delegates to `MM_IsRulesetVotable()`.
   - votable-list help builders now delegate to:
     - `MM_GetVotableGametypesList()`
     - `MM_GetVotableRulesetsList()`

4. **Vote command validation extracted**
   - `ValidVoteCommand()` in `g_cmds.cpp` is now a thin hook to `MM_ValidVoteCommand()`.
   - Existing argument behavior is preserved, including the quoted-name `handicap` path.

5. **Vote startup/storage path extracted**
   - `VoteCommandStore()` in `g_cmds.cpp` is now a thin hook to `MM_VoteCommandStore()`.
   - Vote menu/broadcast/reset behavior is preserved.

6. **Callvote entry orchestration extracted**
   - `Cmd_CallVote_f()` in `g_cmds.cpp` is now a thin hook to `MM_CmdCallVote()`.
   - Added a null sentinel to `vote_cmds[]` so module code can iterate command entries safely.

7. **Vote-casting command extracted**
   - `Cmd_Vote_f()` in `g_cmds.cpp` is now a thin hook to `MM_CmdVote()`.
   - Yes/no vote-casting behavior and guard checks are preserved.

8. **Vote cleanup helper extracted**
   - `G_RevertVote()` in `g_cmds.cpp` is now a thin hook to `MM_RevertVote()`.
   - Active-vote-only revert behavior is preserved.

9. **Simple vote handlers extracted**
   - `timelimit`, `scorelimit`/`fraglimit`, `unlagged`, `powerups`, and `friendlyfire` validation/pass handlers now execute in `mm_vote.cpp`.
   - `g_cmds.cpp` retains thin wrappers so `vote_cmds[]` wiring stays unchanged.

10. **Team-flow vote handlers extracted**
   - `shuffle`, `balanceteams`, and `readyall` validation/pass handlers now execute in `mm_vote.cpp`.
   - `g_cmds.cpp` retains thin wrappers to preserve command table and behavior.

11. **Random/nextmap vote handlers extracted**
   - `nextmap`, `random`, and `cointoss` pass handlers now execute in `mm_vote.cpp`.
   - `random` argument validation now executes in `mm_vote.cpp`.

12. **Map/restart vote handlers extracted**
   - `map` vote pass + validation logic now executes in `mm_vote.cpp` (including map pool/list validation and help-list printing).
   - `restart` vote pass logic now executes in `mm_vote.cpp`.
   - `g_cmds.cpp` keeps a thin `IsMapValid` bridge so admin setmap uses the same map validation path.

13. **Handicap vote handlers extracted**
   - `handicap` vote validation and pass execution now execute in `mm_vote.cpp`.
   - Existing quoted-name parsing and weapon restriction behavior are preserved.
   - `g_cmds.cpp` retains standalone handicap admin helpers/commands while vote path is now a thin wrapper.

14. **Final vote validation context + handlers extracted**
   - `gametype` and `ruleset` vote validation now execute in `mm_vote.cpp`.
   - Menu vote validation context is now bridged through `mm_vote` (`MM_BeginVoteValidationContext` / `MM_EndVoteValidationContext`) so menu and console vote validation share the same parser path.
   - `g_cmds.cpp` keeps thin wrapper entrypoints and `vote_cmds[]` wiring.

15. **Vote registry boundary cleanup**
   - `vote_cmds[]`, `FindVoteCmdByName()`, and `ValidateMenuVoteCommand()` now live in `mm_vote.cpp`.
   - `g_cmds.cpp` no longer owns vote registry internals; it now remains focused on orchestration/admin paths.
   - Vote command table entries now point directly to `MM_*` handlers.

16. **Vote menu extraction**
   - Callvote menu definitions, selection handlers, vote menu display/update logic, and submenu return flow now live in `mm_vote_menu.cpp`.
   - `g_menu.cpp` now includes `muffmode/mm_vote_menu.h` and retains only the join-menu callvote entry hook.
   - `muffmode/muffmode.h`, `game.vcxproj`, and `game.vcxproj.filters` include the new vote-menu module.

17. **Vote lifecycle ticking extraction**
   - Active-vote recounting, timeout handling, pass/fail transition checks, and delayed execution are now handled by `MM_CheckVote()` in `mm_vote.cpp`.
   - `g_main.cpp` now calls `MM_CheckVote()` from `G_RunFrame_()` as a thin vanilla hook.
   - Existing vote state-machine transitions and vote execution behavior are preserved.

#### Extracted admin subsystem (partial)

- Added admin module API:
  - `src/muffmode/mm_admin.h`
- Added admin module implementation:
  - `src/muffmode/mm_admin.cpp`

#### Migrations completed in this subsystem

1. **Admin command bodies extracted**
   - MuffMode Doctor diagnostics now execute in `MM_CmdDoctor()`.
   - Admin `gametype`, `ruleset`, and `setmap` command bodies now execute in `mm_admin.cpp`.
   - Duel handicap admin commands and their private helper logic now execute in `mm_admin.cpp`.
   - `g_cmds.cpp` retains command-table wrappers and permission flag flow.

#### Extracted gametype/ruleset subsystem (partial)

- Added gametype module API:
  - `src/muffmode/mm_gametype.h`
- Added gametype module implementation:
  - `src/muffmode/mm_gametype.cpp`

#### Migrations completed in this subsystem

1. **Gametype/ruleset orchestration extracted**
   - Ruleset cvar reconciliation now executes in `MM_CheckRuleset()`.
   - `ChangeGametype()` in `g_main.cpp` is now a thin hook to `MM_ChangeGametype()`.
   - `GT_Changes()` in `g_main.cpp` is now a thin hook to `MM_GTChanges()`.
   - Gametype tracking counters are owned by `mm_gametype.cpp` and initialized through `MM_SyncGametypeTracking()`.
   - Existing gametype cvar synchronization, team reset, config execution, map reload, and map-list reshuffle behavior are preserved.

#### Validation completed so far

- Build passes after each extraction step.
- Runtime tests passed for:
  - `sv nextmap` repeated rotation
  - `g_map_list_shuffle` modes `0`, `1`, `2`
  - wrap-around behavior
  - fallback behavior when list is missing or map is not in list
  - `gt_changemap_first` flow
  - vote lifecycle (active/passed/failed/complete)
  - gametype/ruleset vote pass/reject behavior
  - handicap vote parser path (`callvote handicap "Player Name" <weapon|all> <on|off>`)
- Build validation passed for vote-menu extraction:
  - `build.bat Debug x64 v145`
  - `0 Warning(s), 0 Error(s)`
- Build validation passed for vote lifecycle ticking extraction:
  - `build.bat Debug x64 v145`
  - `0 Warning(s), 0 Error(s)`
- Build validation passed for admin command extraction:
  - `build.bat Debug x64 v145`
  - `0 Warning(s), 0 Error(s)`
- Build validation passed for gametype/ruleset orchestration extraction:
  - `build.bat Debug x64 v145`
  - `0 Warning(s), 0 Error(s)`

### Phase 5 (Completed)

#### Header boundary cleanup — vote types slice

- Added vote-only type header:
  - `src/muffmode/mm_vote_types.h`
- Moved out of `g_local.h`:
  - `vcmds_t`, `vote_cmds[]`, `VoteState`
- Moved vote registry declarations into `mm_vote.h`:
  - `FindVoteCmdByName()`
  - `ValidateMenuVoteCommand()`
- Removed redundant vanilla declarations from `g_local.h`:
  - `IsGametypeVotable()`, `IsRulesetVotable()`
  - `VoteCommandStore()`, `Vote_Passed()`
  - vote registry helpers now owned by the vote module
- Kept in `g_local.h` (save/session and wide vanilla use):
  - `VoteStateData` embedded in `level_locals_t`
  - thin vanilla hooks `TransitionVoteState()`, `ClearVote()`
- Updated MuffMode consumers:
  - `mm_vote_menu.cpp` calls `MM_*` vote APIs directly
  - removed dead thin wrappers from `g_cmds.cpp`
- Updated project wiring:
  - `src/game.vcxproj`
  - `src/game.vcxproj.filters`
  - `src/muffmode/muffmode.h`

#### Validation

- Build validation passed for vote header boundary slice:
  - `build.bat Debug x64 v145`
  - `0 Warning(s), 0 Error(s)`

#### Map queue boundary cleanup

- Extended `mm_maps.h` / `mm_maps.cpp` with MyMap queue ownership:
  - `MM_MQ_Count()`, `MM_MQ_Add()`, `MM_MQ_Go_Next()`
  - `MM_CmdMapList()`, `MM_CmdMyMap()`
- Removed from `g_local.h`:
  - `MQ_Count()`, `MQ_Add()` declarations
- Removed from `g_main.cpp`:
  - entire map-queue implementation block (~130 lines)
  - `Match_End()` now calls `MM_MQ_*` directly
- Removed from `g_cmds.cpp`:
  - `MQ_PrintList()` and MyMap command bodies
  - dead `MyMap_FlagString()` stub
  - `maplist` / `mymap` commands remain as thin wrappers
- Kept in `g_local.h`:
  - `game.mapqueue` vector in `game_locals_t` (session state)

#### Validation

- Build validation passed for map queue boundary slice:
  - `build.bat Debug x64 v145`
  - `0 Warning(s), 0 Error(s)`

#### MOTD boundary cleanup

- Added MOTD module:
  - `src/muffmode/mm_motd.h`
  - `src/muffmode/mm_motd.cpp`
- Moved from vanilla files:
  - `G_LoadMOTD()` body from `g_main.cpp` → `MM_LoadMOTD()`
  - `motd` / `loadmotd` command bodies from `g_cmds.cpp` → `MM_CmdMotd()` / `MM_CmdLoadMotd()`
- Removed from `g_local.h`:
  - `G_LoadMOTD()` declaration
- Kept in `g_local.h`:
  - `game.motd`, `game.motd_mod_count`, and per-client MOTD display state

#### Vote and map-list wrapper cleanup

- Removed dead or redundant vanilla wrappers:
  - `G_RevertVote()` — callers now use `MM_RevertVote()` (`g_cmds.cpp`, `p_client.cpp`)
  - `G_ShuffleMapList()` — removed; `mm_maps.cpp` calls `MM_ShuffleMapList()` internally
  - unused `IsMapValid()` and `ValidVoteCommand()` stubs in `g_cmds.cpp`
- Removed from `g_local.h`:
  - `G_RevertVote()`, `G_ShuffleMapList()`

#### MuffMode header include tightening

- `mm_admin.h`, `mm_gametype.h`, `mm_vote_menu.h` no longer include `g_local.h`
- Public module headers use forward declarations where sufficient
- `muffmode/muffmode.h` includes `mm_motd.h`

#### Validation

- Build validation passed for final Phase 5 slice:
  - `build.bat Debug x64 v145`
  - `0 Warning(s), 0 Error(s)`

### Duel handicap removal

- Removed the unused duel weapon-restriction feature end-to-end:
  - admin commands (`handicap`, `handicap_clear`)
  - vote type (`callvote handicap`)
  - `client->handicap` state in `gclient_t`
  - gameplay enforcement in `p_weapon.cpp` and `g_items.cpp`
  - reset/init paths in `p_client.cpp` and `mm_gametype.cpp`

### Phase 6B — Ruleset health/armor caps (complete)

1. **Ruleset cap helpers extracted**
   - Added `mm_ruleset.h` / `mm_ruleset.cpp` with `MM_RULESET_HEALTH_CAP`, `MM_RULESET_ARMOR_CAP`, `MM_RulesetHealthArmorCap()`, `MM_ClampClientPersistHealthArmor()`, and `MM_ClampEntityHealthArmor()`.
   - Removed `G_RULESET_*` constants and `G_RulesetHealthArmorCap()` from `g_local.h`.
   - `p_client.cpp` spawn and periodic clamp paths call the new helpers; `g_items.cpp` pickup/respawn logic uses `MM_*` symbols.

### Phase 6C — Ruleset weapon tuning (complete)

1. **Ruleset weapon helpers extracted**
   - Added `mm_ruleset_weapons.h` / `mm_ruleset_weapons.cpp` with per-ruleset damage, spread, pickup, and projectile defaults (`MM_Ruleset_*`).
   - Moved `k_ruleset_pickup_tuning` slug quantity table out of `p_weapon.cpp`.
   - `p_weapon.cpp` weapon fire and ammo pickup/drop paths now call `MM_Ruleset_*`; weapon fire functions remain in vanilla.

### Phase 6D — Spawn loadout (complete)

1. **Spawn loadout helpers extracted**
   - Added `mm_spawn_loadout.h` / `mm_spawn_loadout.cpp` with `MM_ApplyStartingHealthArmor()` and `MM_ApplySpawnLoadout()`.
   - Covers arena/Q3A/Q1/QC/default weapon branches, instagib/nadefest/BALL, and pre-match warmup weapon grants.
   - `InitClientPersistant()` keeps coop loadout steal, `Player_GiveStartItems`, compass/grapple grants, and ruleset health/armor clamp in vanilla.

### Phase 6E — Item rules (complete)

1. **Item rules helpers extracted**
   - Added `mm_items_rules.h` / `mm_items_rules.cpp` for armor pickup (Q3A/Q1), health pickup caps, powerup respawn timing, smart autoswitch, slug ammo multiplier, MyMap item inhibit flags, and initial powerup spawn delay.
   - `g_items.cpp` keeps thin wrappers for `Pickup_Armor`, `ArmorIndex`, `G_CheckAutoSwitch`, `CheckItemEnabled`, and vanilla `CheckItemEnabled` cvar logic.

### Phase 6F — Spawn filter and gametype names (complete)

1. **Spawn filter and display names extracted**
   - Added `mm_spawn_filter.h` / `mm_spawn_filter.cpp` with `MM_RemapSpawnClassname()` (Q1/Q3A weapon aliases, team spawn keys) and `MM_ShouldInhibitSpawnEntity()` (gametype/ruleset spawn keys, `notq2`/`notq3a`/`notarena`, quad hog).
   - Moved `GT_SetLongName()` variant-mode matrix to `MM_GTSetLongName()` in `mm_gametype.cpp`; vanilla `GT_SetLongName()` is a one-line hook.
   - `G_InhibitEntity()` and `ED_CallSpawn()` call the new helpers; vanilla cvar/skill spawn gating remains in `g_spawn.cpp`.

### Tier 1 thin-vanilla sweep (complete)

1. **Items rules gap (6E follow-up)**
   - Extended `mm_items_rules` with teamed-item respawn index, powerup respawn broadcast, and powerup use announcer gate.

2. **Statusbar layout extracted**
   - Added `mm_statusbar.h` / `mm_statusbar.cpp` with `MM_InitStatusbar()`; `SP_worldspawn` calls it from `g_spawn.cpp`.

3. **Duel queue extracted**
   - Added `mm_duel.h` / `mm_duel.cpp` with `MM_Duel_AddPlayer()`, `MM_Duel_RemoveLoser()`, `MM_Duel_MatchEnd_AdjustScores()`, and `MM_Duel_QueueSpectatorBots()`.

### Tier 2 thin-vanilla sweep (complete)

1. **Horde mode extracted**
   - Added `mm_horde.h` / `mm_horde.cpp` with weighted monster/item tables, `MM_Horde_Init()`, `MM_Horde_RunSpawning()`, `MM_Horde_BeginWave()`, and `MM_Horde_AdjustPlayerScore()`.
   - `g_main.cpp` retains round/match orchestration hooks; `g_monster.cpp` calls `MM_Horde_AdjustPlayerScore()`.

2. **Q3A health/armor decay**
   - Added `MM_RulesetQ3AHealthArmorDecay()` to `mm_ruleset`; `ClientTimerActions` in `p_client.cpp` calls it before clamp.

## MuffMode module map

| Module | Responsibility |
|--------|----------------|
| `mm_admin` | Admin commands (gametype, ruleset, setmap, doctor) |
| `mm_debug` | Debug logging |
| `mm_duel` | GT_DUEL queue and match-end score adjustment |
| `mm_gametype` | Gametype/ruleset orchestration and display names |
| `mm_horde` | GT_HORDE wave spawning, tables, and kill scoring |
| `mm_items_rules` | Item pickup, respawn, inhibit, autoswitch rules |
| `mm_maps` | Map list, queue, shuffle, MyMap |
| `mm_motd` | Message-of-the-day load and commands |
| `mm_ruleset` | Health/armor caps, Q3A over-max decay, and clamp helpers |
| `mm_ruleset_weapons` | Per-ruleset weapon tuning |
| `mm_spawn_filter` | Entity classname remap and spawn-key filtering |
| `mm_spawn_loadout` | Deathmatch spawn health/armor and weapon loadout |
| `mm_statusbar` | Statusbar layout string builder |
| `mm_vote` | Vote registry, validation, lifecycle |
| `mm_vote_menu` | Callvote menu UI |
| `mm_vote_types` | Vote type definitions and tables |

## Current Thin-Hook Pattern

- Keep vanilla files (`g_main.cpp`, `g_svcmds.cpp`) as orchestration points.
- Place feature body logic in `src/muffmode/*.cpp`.
- Keep behavior unchanged for each extraction slice.
- Validate each slice with build + in-game smoke tests before moving on.

## Refactor Complete

**Status:** Thin-vanilla refactor declared complete (Phases 1–6, Tier 1–2, Phase 5 header boundaries).

Cohesive MuffMode feature bodies live in `src/muffmode/`. Remaining vanilla code is intentionally thin: match orchestration hubs, 1–3 line `GT()`/`RS()`/`GTF()` gates, shared types/state in `g_local.h`, and cvar registration in `InitGame`. Removing every gametype/ruleset conditional from vanilla files is explicitly **out of scope** (see [`THIN_VANILLA_PRINCIPLES.md`](THIN_VANILLA_PRINCIPLES.md)).

### Vanilla adapter table

| Vanilla hook (call site) | Canonical API | Module |
|-------------------------|---------------|--------|
| `ChangeGametype()` | `MM_ChangeGametype()` | `mm_gametype` |
| `GT_Changes()` | `MM_GTChanges()` | `mm_gametype` |
| `CheckRuleset()` | `MM_CheckRuleset()` | `mm_gametype` |
| `GT_SetLongName()` | `MM_GTSetLongName()` | `mm_gametype` |
| `TransitionVoteState()` | `MM_TransitionVoteState()` | `mm_vote` |
| `ClearVote()` | `MM_ClearVote()` | `mm_vote` |
| `G_RunFrame_` vote tick | `MM_CheckVote()` | `mm_vote` |
| `Cmd_CallVote_f` / `Cmd_Vote_f` | `MM_CmdCallVote()` / `MM_CmdVote()` | `mm_vote` |
| `MM_RevertVote()` (direct) | `MM_RevertVote()` | `mm_vote` |
| `G_Menu_Vote_Open()` / `Vote_Menu_Active()` | same (implemented in module) | `mm_vote_menu` |
| `Cmd_Doctor_f` / `Cmd_Gametype_f` / `Cmd_Ruleset_f` / `Cmd_SetMap_f` | `MM_CmdDoctor()` etc. | `mm_admin` |
| `Cmd_MapList_f` / `Cmd_MyMap_f` | `MM_CmdMapList()` / `MM_CmdMyMap()` | `mm_maps` |
| `Cmd_Motd_f` / `Cmd_LoadMotd_f` | `MM_CmdMotd()` / `MM_CmdLoadMotd()` | `mm_motd` |
| `G_ShuffleMapList` / map-list intermission | `MM_ShuffleMapList()` / `MM_TryBeginIntermissionFromMapList()` | `mm_maps` |
| `SVCmd_GametypeChangeMapFirst_f` | `MM_GametypeChangeMapFirst()` | `mm_maps` |
| `MM_RemapSpawnClassname()` / spawn inhibit | `mm_spawn_filter` | `mm_spawn_filter` |
| `SP_worldspawn` statusbar | `MM_InitStatusbar()` | `mm_statusbar` |
| Duel warmup/match hooks | `MM_Duel_*()` | `mm_duel` |
| Horde spawn/wave hooks | `MM_Horde_*()` | `mm_horde` |
| Item pickup/respawn/inhibit | `MM_*` item helpers | `mm_items_rules` |
| Spawn health/armor/loadout | `MM_ApplyStartingHealthArmor()` / `MM_ApplySpawnLoadout()` | `mm_spawn_loadout` |
| Weapon ruleset tuning | `MM_Ruleset_*()` | `mm_ruleset_weapons` |
| Health/armor caps + Q3A decay | `MM_Clamp*()` / `MM_RulesetQ3AHealthArmorDecay()` | `mm_ruleset` |

### Include policy

- **`g_local.h`** — shared game types, macros (`GT`, `RS`, `GTF`), embedded `game`/`level`/`client` state, vanilla-named adapter declarations.
- **Feature translation units** — include the specific `mm_*.h` for APIs they call (e.g. `g_spawn.cpp` → `mm_statusbar.h`, `mm_spawn_filter.h`).
- **`muffmode/muffmode.h`** — optional umbrella include listing all module headers; not required project-wide.
- **`[MuffMode]` markers** — used at vanilla hook boundaries and in module public headers; not required on every one-line `GT()` gate.

### Permanent vanilla surface (intentional)

- **`g_main.cpp`** — match warmup/round/exit state machines, `CalculateRanks`, `Match_End`, `InitGame` cvar registration (~25 MuffMode cvars).
- **`g_cmds.cpp`** — command dispatch table; remaining admin/captain/ready/duel SetTeam bodies; thin wrappers for extracted commands.
- **`g_local.h`** — `gametype_t`, `ruleset_t`, name/index tables, `VoteStateData`, map queue/MOTD/horde/duel embedded fields, adapter function names.
- **`p_hud.cpp`** — gametype scoreboard layout (Phase 6H deferred).
- **`g_combat.cpp`** — scattered gametype/ruleset damage gates (Phase 6G deferred).
- **1–5 line `GT()`/`RS()`/`GTF()` gates** across gameplay files where extraction would obscure control flow.

### Post-completion backlog (optional future slices)

**Priority A — extend existing modules**

| Target | ~Lines | Module |
|--------|--------|--------|
| Duel scoreboard + vote gate (`p_hud.cpp`) | ~210 | `mm_duel` |
| AutoDoc RS_MM regen + health pickup glue (`g_items.cpp`) | ~110 | `mm_items_rules` |
| Fall damage Q3A thresholds (`p_client.cpp`) | ~30 | `mm_ruleset` |
| Horde round-win/exit snippets (`g_main.cpp`) | ~20 | `mm_horde` |
| Remaining admin commands (`g_cmds.cpp`) | ~210 | `mm_admin` |
| Duel SetTeam/forfeit/screenshot (`g_cmds.cpp`, `g_main.cpp`) | ~220 | `mm_duel` |

**Priority B — new modules (defer unless actively developing those modes)**

| Target | ~Lines | Module |
|--------|--------|--------|
| Match warmup/round/exit state machines | ~600+ | `mm_match` |
| Captain/lock/ready-team commands | ~470 | `mm_captain` / `mm_match` |
| Team scoreboard layout | ~250 | `mm_hud` |
| Combat gametype/ruleset blocks | ~55 | `mm_combat_rules` |
| Strike CTF round logic | ~20 | `mm_gametype` |

**Priority C — optional structural tidy**

- `mm_types.h` to break `mm_vote.h` ↔ `g_local.h` circular include
- `MM_RegisterCvars()` to consolidate MuffMode cvar registration from `InitGame`

### Final smoke-test matrix

Run once after closure housekeeping; note results in commit message or below.

| Area | Cases |
|------|-------|
| Gametypes | FFA, TDM, CTF, CA, duel, freeze, horde, instagib, strike, RR |
| Rulesets | Q2RE, MM, Q3A, Q1, QC, VANILLA_PLUS (where applicable) |
| Vote | Menu open, start map/gametype/ruleset vote, pass, fail, timeout |
| Admin | setmap, gametype, ruleset, doctor |
| Maps | map list, shuffle, MyMap queue, `sv nextmap` rotation |
| Duel | queue join, loser rotation, spectator bot queue |
| Horde | See [Horde smoke checklist](#horde-smoke-checklist) below |
| Ruleset | Q3A over-max health/armor decay |
| UI | Statusbar on map load |

_Smoke results: pending in-game validation._

### Horde smoke checklist

Manual sign-off for `GT_HORDE` after spawn/orchestration changes:

| Step | What to verify |
|------|----------------|
| Map load | Horde map loads without crash; statusbar wave/monster HUD visible |
| Warmup | At most ~30 living monsters during warmup; no point budget drain |
| Wave 1+ | Wave begins, monsters spawn until point budget spent, wave clears when budget exhausted and all dead |
| Multi-wave | Corpses do not pile up between waves; counters/HUD stay sane |
| Overrun | With cheats or tuning, 100+ living monsters triggers defeat intermission |
| Held grenade | Detonate while engaging monsters (no AV / soft-lock) |
| Modifiers | Insta-Horde (or other horde vote presets) still spawn and score |
| Admin/vote | `callvote` map/gametype to Horde on a horde-capable map |
| Lives | Default `g_horde_lives 1` (one death per wave → spectator); set higher to allow respawns; lives restored each wave |
| All out | Last fighter loses last life → defeat intermission (not next wave) |
| Coop cvar | `g_coop_enable_lives 1` in Horde must not double-decrement lives |
| Warmup death | Warmup kill does not decrement horde lives |
| Elim spectator | Last life in a wave → center message to rejoin next wave; no duplicate spam on respawn to freecam |
| Last monsters | At ≤3 living (default), `loc_ping` POIs (team ping style) for fighters and eliminated spectators; clear after wave |
| Wave rejoin | Die out mid-wave → spectator until next wave; respawn with fresh lives and kept weapons when "Wave N Begins in..." countdown starts |

### Horde cvar rename (spawn points)

| Old | New |
|-----|-----|
| `g_horde_monsters_base` | `g_horde_points_base` |
| `g_horde_monsters_per_wave` | `g_horde_points_per_wave` |
| `g_horde_monsters_min` | `g_horde_points_min` (default **0** = off; was 20) |
| `g_horde_monsters_max` | `g_horde_points_max` (default **0** = off; was 80) |

Configs that used the old min/max clamps can set `g_horde_points_min 20` and `g_horde_points_max 80`.

## Notes

- This is a staged refactor, not a rewrite.
- Focus remains on reducing merge/debug risk while preserving gameplay behavior.
- **Gametype availability** (`mm_gametype.cpp`): enabled modes are selectable; disabled modes (Strike, RR) keep code but reject selection; removed modes (Freeze, Ball, LMS) have gameplay stripped. `g_gametype` enum indices are unchanged.

