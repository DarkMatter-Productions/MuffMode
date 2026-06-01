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
- Added vote module implementation:
  - `src/muffmode/mm_vote.cpp`

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

## Current Thin-Hook Pattern

- Keep vanilla files (`g_main.cpp`, `g_svcmds.cpp`) as orchestration points.
- Place feature body logic in `src/muffmode/*.cpp`.
- Keep behavior unchanged for each extraction slice.
- Validate each slice with build + in-game smoke tests before moving on.

## Next Recommended Targets

- Vote flow extraction boundaries from `g_cmds.cpp` into `src/muffmode/`:
  - grouped migration of remaining `Vote_Val_*` / `Vote_Pass_*` handlers
  - potential relocation or encapsulation strategy for `vote_cmds[]`
- Menu integration boundary cleanup (`g_menu.cpp`/`p_menu.cpp`) as a separate slice.

## Notes

- This is a staged refactor, not a rewrite.
- Focus remains on reducing merge/debug risk while preserving gameplay behavior.

