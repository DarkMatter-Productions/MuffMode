# MuffMode Hardening Guide

[README](../README.md) | [Build Guide](build-guide.md) | [Build Matrix](build-matrix.md) | [Release Process](release-process.md)

This guide is the contributor-facing entrypoint for robustness checks. Run the same scripts locally that CI runs.

## Required Local Gates

```powershell
./scripts/ci/check-generated-artifacts.ps1
./scripts/ci/check-test-assets.ps1 -RepoOnly
./scripts/ci/check-dependency-inventory.ps1
./scripts/ci/check-regression-corpus.ps1
./scripts/ci/run-host-tests.ps1 -Configuration Release -Platform x64
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
./scripts/ci/run-clang-tidy.ps1 -Files src/muffmode/mm_pconfig.cpp
./scripts/ci/run-cppcheck.ps1
./scripts/ci/run-sanitized-build.ps1 -Sanitizer Address
./scripts/ci/run-sanitized-build.ps1 -Sanitizer Undefined -AllowUnsupported
```

AddressSanitizer is the blocking sanitizer build on the Windows/MSBuild path. UndefinedBehaviorSanitizer remains experimental until the ClangCL toolset and runtime are stable on CI.

## Tests And Fuzzing

Host tests cover parser helpers, command argument contracts, vote/config parsing, Red Rover rules, scoreboard footer safety, and fake import boundaries.

Regression and fuzz corpora live under `docs-dev/test-assets/` and `tests/fuzz/`.

```powershell
./scripts/ci/run-host-tests.ps1 -Configuration Release -Platform x64
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
