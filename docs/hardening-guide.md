# MuffMode Hardening Guide

Run the PowerShell gates in PowerShell 7 (`pwsh`); the push verifier enforces
this up front so its JSON and staged-asset checks cannot fail late under
Windows PowerShell 5.1.

[README](../README.md) | [Build Guide](build-guide.md) | [Build Matrix](build-matrix.md) | [Release Process](release-process.md)

This guide is the contributor-facing entrypoint for robustness checks. Run the same scripts locally that CI runs.

## GitHub Push Verification

Before pushing to GitHub, run the consolidated verifier from the repository root:

```powershell
./scripts/ci/verify-github-push.ps1
```

The verifier requires a clean worktree, checks that the target GitHub branch can be pushed fast-forward, confirms that the branch is covered by the configured push workflows, then runs the required local gates below. If the remote branch you intend to update differs from the local branch name or upstream, pass it explicitly:

```powershell
./scripts/ci/verify-github-push.ps1 -RemoteBranch main
```

After pushing, the same entrypoint can wait for the matching GitHub Actions runs:

```powershell
./scripts/ci/verify-github-push.ps1 -SkipLocalGates -WaitForGitHub
```

Use `-IncludeAnalysis` and `-IncludeSanitizers` for slower local parity with the analysis workflow before high-risk pushes.

## Required Local Gates

```powershell
./scripts/ci/check-generated-artifacts.ps1
./scripts/ci/check-map-pool-examples.ps1
./scripts/ci/check-changelog.ps1
./scripts/ci/check-test-assets.ps1 -RepoOnly
./scripts/ci/check-dependency-inventory.ps1
./scripts/ci/check-regression-corpus.ps1
./scripts/ci/run-host-tests.ps1 -Configuration Release -Platform x64
./scripts/ci/run-updater-tests.ps1 -Configuration Release
./scripts/ci/build-msbuild.ps1 -Configuration Release -Platform x64 -TreatWarningsAsErrors -BinaryLog
```

When the full external asset pack is available:

```powershell
./scripts/ci/check-test-assets.ps1 -AssetRoot E:\Repositories\MuffMode-test-assets
```

## Analysis And Sanitizers

Static analysis policy lives in [docs-dev/robustness/analysis-policy.md](../docs-dev/robustness/analysis-policy.md). The supported matrix lives in [docs/build-matrix.md](build-matrix.md).

Useful local commands:

```powershell
./scripts/ci/export-compile-commands.ps1
./scripts/ci/run-msvc-analyze.ps1
./scripts/ci/run-clang-tidy.ps1 -Files src/sgame/muffmode/mm_pconfig.cpp
./scripts/ci/run-cppcheck.ps1
./scripts/ci/run-sanitized-build.ps1 -Sanitizer Address
./scripts/ci/run-sanitized-build.ps1 -Sanitizer Undefined -AllowUnsupported
```

AddressSanitizer is the blocking sanitizer build on the Windows/MSBuild path. UndefinedBehaviorSanitizer remains experimental until the ClangCL toolset and runtime are stable on CI.

## Tests And Fuzzing

Host tests cover parser helpers, command argument contracts, vote/config parsing, Red Rover rules, scoreboard footer safety, and fake import boundaries. The dependency-free updater harness exercises exact-hash obsolete-file removal, preservation cases, path containment, and deferred self-update cleanup wiring.

Regression and fuzz corpora live under `docs-dev/test-assets/` and `tests/fuzz/`.

```powershell
./scripts/ci/run-host-tests.ps1 -Configuration Release -Platform x64
./scripts/ci/run-updater-tests.ps1 -Configuration Release
./scripts/ci/check-regression-corpus.ps1
./scripts/ci/build-fuzz-targets.ps1 -AllowUnsupported
```

Runtime fuzzing is still experimental because local libFuzzer execution depends on the LLVM sanitizer runtime DLL being available.

## Crash Triage

Follow [docs-dev/robustness/crash-policy.md](../docs-dev/robustness/crash-policy.md).

Minimum crash triage record:

- Exact branch and commit.
- Build configuration, platform, compiler, sanitizer/profiling flags.
- Map, gametype, ruleset, config, command sequence, save file, or corpus input.
- Crash stack, minidump, sanitizer output, or repro video/log.
- Whether the issue is parser/input rejection, state corruption, ABI drift, save/load migration, or gameplay logic.

Add a regression seed or host test before closing a crash fix whenever the failure can be reduced.

### Kex `bad allocation` / `muffmode_alloc.log`

The engine dialog `Standard exception caught in kexPlatformApp::Main: bad allocation` is caught inside Kex, so it often produces no `CRASHLOG.TXT` or minidump. MuffMode's `mm_alloc_guard` logs oversized or failed game-DLL `operator new` calls to `muffmode_alloc.log` (and mirrors lines to DebugView via `OutputDebugStringA`).

Look for the log next to the loaded `game_x64.dll`, then `%TEMP%\muffmode_alloc.log`, then the process working directory. A healthy deploy writes a `LOADED` breadcrumb as soon as the DLL loads. If you see the Kex dialog with a `LOADED` line but no `FAILED`/`HUGE` line, the failing allocation was almost certainly outside the game DLL (engine heap / another module).

## Performance Profiling

Follow [docs-dev/robustness/profiling-guide.md](../docs-dev/robustness/profiling-guide.md).

Counter-enabled build:

```powershell
./scripts/ci/build-msbuild.ps1 -Configuration Release -Platform x64 -AdditionalMsBuildArgs "/p:MMEnableProfileCounters=true"
```

Use WPR/WPA for Windows gameplay/server traces. Use RenderDoc only for `cg_*`, HUD, and client-visual work.

## Release Go/No-Go

Do not publish a release unless:

- Version files are aligned: `VERSION` and `GAMEMOD_VERSION`.
- Required local gates pass.
- Active GitHub Actions build and release validation workflows are green.
- Dependency inventory and third-party notices are current.
- Generated artifacts are not tracked in the source diff.
- New crash fixes include regression coverage or a documented reason coverage is not feasible.
- Known sanitizer/analyzer findings are triaged in [docs-dev/robustness/suppressions.yml](../docs-dev/robustness/suppressions.yml) with owner and expiry.
- Release package includes `LICENSE` and `THIRD_PARTY_NOTICES.md`.
