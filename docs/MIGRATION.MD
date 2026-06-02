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

## Current Thin-Hook Pattern

- Keep vanilla files (`g_main.cpp`, `g_svcmds.cpp`) as orchestration points.
- Place feature body logic in `src/muffmode/*.cpp`.
- Keep behavior unchanged for each extraction slice.
- Validate each slice with build + in-game smoke tests before moving on.

## Next Recommended Targets

- Phase 6E–6F gameplay slices as needed (items rules, spawn filter).
- Intentionally left in `g_local.h` for Phase 5 scope control:
  - `gametype_t`, `ruleset_t`, and name/index tables (widely referenced across gameplay)
  - embedded session state (`game.mapqueue`, `game.motd`, `level.vote_state`)
  - thin vanilla orchestration hooks (`ChangeGametype()`, `GT_Changes()`, `TransitionVoteState()`, `ClearVote()`)

## Notes

- This is a staged refactor, not a rewrite.
- Focus remains on reducing merge/debug risk while preserving gameplay behavior.

