# MuffMode Build Guide

[README](../README.md) | [Server Host Guide](server-host-guide.md) | [Configuration Reference](configuration-reference.md)

This guide explains how to build MuffMode on Windows with Visual Studio/MSBuild.

## Prerequisites

- Windows 10 or Windows 11.
- Visual Studio 2022 or Visual Studio 2022 Build Tools.
- The C++ desktop workload and Windows SDK.
- MSBuild and a configured Visual C++ environment.

The Visual Studio project uses the vcpkg manifest at [vcpkg.json](../vcpkg.json). Dependency policy and third-party notice obligations are recorded in [Dependency Policy](dependencies.md).

The currently supported branch/build matrix is recorded in [Build Matrix](build-matrix.md).

## Project Root

Run commands from the repository root:

```text
MuffMode/
```

The solution file is [projects/msvc/MuffMode.sln](../projects/msvc/MuffMode.sln).

## Open A Developer Shell

Use one of these Visual Studio shells:

- `x64 Native Tools Command Prompt for VS 2022`
- `Developer PowerShell for VS 2022`

This ensures `msbuild`, the compiler, and library paths are available.

## Build Commands

Release build:

```bat
msbuild projects\msvc\MuffMode.sln /p:Configuration=Release /p:Platform=x64
```

Canonical CI/local release build:

```powershell
./scripts/ci/build-msbuild.ps1 -Configuration Release -Platform x64
```

Strict warning gate:

```powershell
./scripts/ci/build-msbuild.ps1 -Configuration Release -Platform x64 -TreatWarningsAsErrors
```

Debug build:

```bat
msbuild projects\msvc\MuffMode.sln /p:Configuration=Debug /p:Platform=x64
```

## Analysis Commands

Generate a compile database for host-side analysis tools:

```powershell
./scripts/ci/export-compile-commands.ps1
```

Run the currently configured analyzer entrypoints:

```powershell
./scripts/ci/run-msvc-analyze.ps1
./scripts/ci/run-clang-tidy.ps1 -Files src/sgame/muffmode/mm_pconfig.cpp
./scripts/ci/run-cppcheck.ps1
./scripts/ci/run-sanitized-build.ps1 -Sanitizer Address
```

UndefinedBehaviorSanitizer is configured through the ClangCL MSBuild platform toolset:

```powershell
./scripts/ci/run-sanitized-build.ps1 -Sanitizer Undefined
```

That job is experimental until the Visual Studio ClangCL build tools are installed and validated on the CI runner.

## Test And Fuzz Commands

Before pushing to GitHub, run the consolidated local push verifier:

```powershell
./scripts/ci/verify-github-push.ps1
```

Use `-RemoteBranch <branch>` only when the GitHub branch you intend to update differs from the local branch name or upstream.

Run the fast host-side smoke tests:

```powershell
./scripts/ci/run-host-tests.ps1 -Configuration Release -Platform x64
```

Validate checked-in regression and fuzz corpus seeds:

```powershell
./scripts/ci/check-regression-corpus.ps1
```

Validate dependency inventory and release notices:

```powershell
./scripts/ci/check-dependency-inventory.ps1
```

Build and execute the first-wave libFuzzer smoke target:

```powershell
./scripts/ci/build-fuzz-targets.ps1
./scripts/ci/run-fuzz-smoke.ps1 -Runs 1000
```

The build verifies the x64 libFuzzer/ASan runtime before compiling, stages the
required runtime DLL beside the target, and then executes a bounded generated
copy of the checked-in corpus. Compiler or target failures are not treated as
unsupported-toolchain success.

## Output

The build produces `build\msbuild\x64\<Configuration>\game_x64.dll`. To test locally after a release build:

1. Back up your Quake II rerelease `baseq2\game_x64.dll`.
2. Copy `build\msbuild\x64\Release\game_x64.dll` into the rerelease `baseq2` folder.
3. Launch Quake II and start or join a multiplayer session.

## Common Issues

| Problem | Fix |
| --- | --- |
| `msbuild` is not recognized | Open a Visual Studio Developer Command Prompt or Developer PowerShell, then run the command again. |
| Missing C++ toolchain errors | Install the Visual Studio C++ desktop workload and Windows SDK. |
| Missing dependency libraries | Confirm vcpkg manifest restore is enabled, then rebuild from the Visual Studio developer shell. |
| DLL copy or test failures | Check your Quake II install path, file permissions, and whether the game is already running. |

## Related Docs

- Install and run a server: [Server Host Guide](server-host-guide.md)
- Configure gametypes, cvars, and votes: [Configuration Reference](configuration-reference.md)
- Hardening and release gates: [Hardening Guide](hardening-guide.md)
- Dependency policy: [Dependency Policy](dependencies.md)
- Licensing: [Licensing](licensing.md)
- Package and publish releases: [Release Process](release-process.md)
