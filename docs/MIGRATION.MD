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

