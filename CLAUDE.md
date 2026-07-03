# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

MuffMode is a **server-side multiplayer mod for Quake II Rerelease**, shipped as a C++ game DLL (`game_x64.dll`). It builds on **Windows with Visual Studio 2022 / MSBuild**. Primary dev branches: `muffdev` / `ozdev` (not `main`).

## Build, deploy, test

```powershell
# Canonical build (what CI runs): Release, warnings-as-errors
./scripts/ci/build-msbuild.ps1 -Configuration Release -Platform x64 -TreatWarningsAsErrors

# Fast local Debug build (~8 MB DLL, debug cvars); outputs game_x64.dll at repo root
./build.bat            # env MUFFMODE_BUILD_CONFIG=Release for a release local build

# Host unit tests — builds tests\host\MuffMode.HostTests.vcxproj and runs the exe
./scripts/ci/run-host-tests.ps1 -Configuration Release -Platform x64
```

- `deploy.bat` / `play.bat` copy the built DLL into a Quake II install and (for `play.bat`) launch the game. **Both contain hard-coded maintainer paths** — edit before use or run the copy/launch manually.
- Host tests run as **one suite** (the exe runs every registered test; there is no per-test filter). Add a test with the `MM_TEST(name) { MM_CHECK(...); }` macro in `tests/host/`. Tests cover pure helpers (parsers, command-arg contracts, vote/config parsing, Red Rover rules) via a fake game-import shim — not the live DLL.

### Robustness gates (run before finishing substantive work)

`docs/hardening-guide.md` lists the full set. The common ones:

```powershell
./scripts/ci/build-msbuild.ps1 -Configuration Release -Platform x64 -TreatWarningsAsErrors -BinaryLog
./scripts/ci/run-host-tests.ps1 -Configuration Release -Platform x64
./scripts/ci/run-clang-tidy.ps1 -Files src/muffmode/<file>.cpp   # static analysis on a single file
./scripts/ci/run-sanitized-build.ps1 -Sanitizer Address          # ASan is the blocking sanitizer
```

## Architecture: "thin vanilla"

The single most important convention (full text in `docs/THIN_VANILLA_PRINCIPLES.md`). The tree is split into **vanilla** files inherited from id's Q2 Rerelease and **MuffMode** code:

- `src/g_*.cpp`, `src/p_*.cpp`, `src/m_*.cpp`, `src/monsters/` — vanilla files. Keep edits to **small hooks/config-gates**, never feature bodies, and **always mark them** `// [MuffMode] why + what`. Do not change vanilla function signatures for mod needs.
- `src/muffmode/` — all MuffMode feature logic. Each feature is `mm_<feature>.{h,cpp}`; `muffmode/muffmode.h` is the aggregator that `#include`s every module header (alphabetical).
- `q2re-src/` — a pristine copy of upstream Q2 Rerelease source kept for divergence comparison (`docs/q2re-divergence.md`). **Reference only; do not edit or build it.**

### Adding a feature module

1. Create `src/muffmode/mm_<feature>.{h,cpp}` (declare the public `MM_*` API in the header).
2. Add `#include "mm_<feature>.h"` to `src/muffmode/muffmode.h` (alphabetical).
3. Register both files in `src/game.vcxproj` (alphabetical `<ClInclude>`/`<ClCompile>`) **and** `src/game.vcxproj.filters` under the `muffmode` filter.

### Where things live

- **Player commands**: thin `static Cmd_<Name>_f(gentity_t *ent)` wrapper in `src/g_cmds.cpp` that calls `MM_Cmd<Name>` in the module, plus an entry in the command table (alphabetical; flags like `CF_ALLOW_SPEC | CF_ALLOW_DEAD`).
- **Per-player preferences**: fields on `client_config_t` (`client->sess.pc` in `src/g_local.h`) — **not** raw userinfo. `sess.pc` persists across map changes within a session but resets on disconnect.
- **Cvars**: declare + `gi.cvar(...)` register in `src/g_main.cpp`, `extern` in `src/g_local.h`. Document user-facing cvars in `docs/configuration-reference.md`; player commands in `docs/player-guide.md`.
- Reuse existing helpers (e.g. `mt_rand`, `G_Fmt`/`G_FmtTo`, `Q_strlcpy`, `active_clients()`) rather than duplicating utilities.

## Versioning (required for every shipped change)

`VERSION` (repo root) and `GAMEMOD_VERSION` in `src/g_local.h` are the source of truth and **must always match exactly**. Releases and CI fail if they diverge.

- Each dev commit bumps the **patch** (e.g. `0.36.34` → `0.36.35`) in **both** files together, and the commit message mentions the bump.
- Never change `GAMEVERSION` (that is `baseq2`) — only `GAMEMOD_VERSION` / `VERSION`.
- For release/automated alignment use `scripts/release.ps1` (see `docs/release-process.md`), e.g. `./scripts/release.ps1 -VersionMode patch -UpdateVersionFiles -SkipBuild -AllowDirtyPackage`.

## Commits

- **Only commit when explicitly asked.** Branch first if on `main`.
- One-line subject in repo style: `area: what changed; bump X.Y.Z.` (e.g. `mm_skin: add per-viewer eskin/tskin; bump 0.36.31.`).
- Match the surrounding style; no drive-by refactors. Do not amend pushed commits or skip hooks unless asked.
- Do not add new markdown docs unless requested.
