# MuffMode Agent Guide

This is the canonical repo guidance for Codex, Cursor, VS Code Copilot, and any other coding agent. Keep this file focused on durable project rules. Use tool-specific files only as adapters:

- Cursor: `.cursor/rules/muffmode.mdc`
- VS Code / GitHub Copilot: `.github/copilot-instructions.md`
- Codex: `AGENTS.md`

If a rule should apply to every agent, update this file first and keep adapters short.

## Working Agreements

- Start by checking the worktree. This repo is often mid-refactor; treat existing edits, deletes, moves, and untracked files as user work unless the user explicitly asks you to clean them up.
- Keep changes scoped to the request. Avoid drive-by refactors, dependency churn, generated-output churn, and broad formatting passes.
- Prefer `rg` / `rg --files` for discovery, then read the smallest useful set of files before editing.
- Do not commit, amend, push, or open PRs unless explicitly asked.
- Do not add new production dependencies without asking.
- Prefer updating existing documentation. Add a new Markdown file only when requested or when creating a durable agent/editor entrypoint like this file.

## Repository Shape

MuffMode is a Quake II Rerelease multiplayer mod built primarily as a Windows x64 C++ game DLL with Visual Studio/MSBuild.

Current source layout:

- `src/sgame/` - server game module and most gameplay logic.
- `src/sgame/muffmode/` - MuffMode-specific server-side systems.
- `src/cgame/` - client game/HUD-facing code.
- `src/shared/` - shared API, gameplay, math, and compatibility types.
- `tests/host/` - fast host-side tests.
- `scripts/ci/` - canonical local and CI entrypoints.
- `packaging/` and `updater/` - release packaging and updater/launcher work.

When docs still mention older paths such as `src/muffmode/` or `src/g_local.h`, verify the live path with `rg`. In the split tree, `GAMEMOD_VERSION` currently lives in `src/sgame/g_local.h`.

## Architecture

- Preserve the thin-vanilla principle from `docs/THIN_VANILLA_PRINCIPLES.md`: keep upstream-style Quake II files as small hooks and put MuffMode behavior in dedicated modules.
- Put new server-side feature bodies in `src/sgame/muffmode/mm_<feature>.{h,cpp}` when practical, expose the API through the header, include it from `src/sgame/muffmode/muffmode.h`, and wire `projects/msvc/game.vcxproj` plus `.filters`.
- Keep `src/sgame/core/commands.cpp` command handlers as thin `Cmd_*_f` wrappers that delegate to `MM_Cmd*` functions in MuffMode modules.
- Store player preferences in `client_config_t` / `client->sess.pc`, not raw userinfo, unless working on the userinfo parser itself.
- Register server cvars in `src/sgame/core/runtime.cpp`, declare externs in `src/sgame/g_local.h`, and document user-facing cvars in `docs/configuration-reference.md`.
- Put client/HUD-only work in `src/cgame/`; put code in `src/shared/` only when both game modules genuinely need it.
- Mark small vanilla touchpoints with concise `[MuffMode]` comments when the surrounding file is not already clearly MuffMode-owned.

## Build And Test

Run commands from the repository root in a Visual Studio 2022 developer shell.

Fast release build:

```powershell
./scripts/ci/build-msbuild.ps1 -Configuration Release -Platform x64
```

Strict build gate:

```powershell
./scripts/ci/build-msbuild.ps1 -Configuration Release -Platform x64 -TreatWarningsAsErrors
```

Fast host tests:

```powershell
./scripts/ci/run-host-tests.ps1 -Configuration Release -Platform x64
```

Release/hardening gates live in `docs/hardening-guide.md`. Use the narrowest relevant gate for the change, and say clearly when a command could not be run.

## Documentation And Release Metadata

- Player-facing commands belong in `docs/player-guide.md`.
- Server cvars and config behavior belong in `docs/configuration-reference.md`.
- Build, analysis, hardening, and release behavior belong in the existing docs under `docs/` or `docs-dev/`.
- Implementation-facing changes normally need a grouped `Unreleased` row in `docs/changelog.md`; docs-only or agent-only workflow changes do not need a version bump unless the user asks or release automation requires it.
- For release-bound version work, keep root `VERSION` and `GAMEMOD_VERSION` in `src/sgame/g_local.h` exactly aligned. Do not change `GAMEVERSION`.

## Multi-Agent Workflow

- `AGENTS.md` is the source of truth. Cursor and Copilot adapters should point here instead of drifting into separate rule sets.
- Tool-specific behavior belongs in the adapter for that tool only. Shared repo conventions belong here.
- Before parallel or handoff work, inspect `git status --short` and agree on file ownership. Avoid having two agents edit the same files without syncing first.
- For one-off preferences, use the prompt/thread. For recurring repo behavior, update this file. For enforceable checks, prefer scripts, CI, hooks, or tests over prose.

## Commit Style

When the user asks for a commit:

- Use a one-line subject in repo style, for example `mm_skin: add per-viewer skin overrides; bump 0.36.43.`
- Keep commits focused and reviewable.
- Do not amend pushed commits or skip hooks unless explicitly asked.
