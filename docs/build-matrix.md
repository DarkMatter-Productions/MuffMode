# MuffMode Build Matrix

[README](../README.md) | [Build Guide](build-guide.md) | [Release Process](release-process.md)

This document records the supported build matrix for the current hardening branch. It is intentionally conservative and matches the active CI workflow.

## Current CI Matrix

| Target | Status | Runner | Toolchain | Config | Notes |
| --- | --- | --- | --- | --- | --- |
| Windows x64 | Enforced in CI | `windows-2025-vs2026` | MSVC `v143` via MSBuild | `Release|x64` | Runs generated-artifact and phase-zero asset-seed guards, builds through `scripts/ci/build-msbuild.ps1`, treats warnings as errors, and uploads `build/msbuild/x64/Release/game_x64.dll`. |
| MSVC code analysis | CI analysis job | `windows-2025-vs2026` | MSVC `v143` `/analyze` | `Release|x64` | Runs through `scripts/ci/run-msvc-analyze.ps1` and uploads a binary log artifact. |
| Static analysis | CI analysis job | `windows-2025-vs2026` | `clang-tidy` and Cppcheck | Compile database from `projects/msvc/game.vcxproj` | `clang-tidy` runs on touched files first; Cppcheck uploads XML output while the baseline is triaged. |
| AddressSanitizer | Enforced in CI | `windows-2025-vs2026` | MSVC `v143` ASan | `Debug|x64` | Runs through `scripts/ci/run-sanitized-build.ps1 -Sanitizer Address`; runtime execution is planned for phase three. |
| UndefinedBehaviorSanitizer | Experimental CI job | `windows-2025-vs2026` | ClangCL UBSan | `Debug|x64` | Configured through `MMEnableUndefinedSanitizer`; non-blocking until the VS ClangCL platform toolset is available and validated. |
| CodeQL C/C++ | CI code-scanning job | `windows-2025-vs2026` | CodeQL manual build | `Release|x64` | Uses manual MSBuild extraction because the project has vcpkg dependencies and Windows-specific build settings. |
| Host tests | Enforced in CI | `windows-2025-vs2026` | MSVC `v143` console test project | `Release|x64` | Runs `tests/host/MuffMode.HostTests.vcxproj` through `scripts/ci/run-host-tests.ps1`. |
| Fuzz targets | Experimental scheduled job | `windows-2025-vs2026` | LLVM `clang-cl` libFuzzer | Parser target smoke | Builds `tests/fuzz/fuzz_numeric_parsers.cpp`; runtime fuzzing depends on sanitizer runtime availability. |

The active CI trigger targets `muffdev`. Release packaging remains a manual workflow and is not treated as a verification matrix entry.

## Local Developer Matrix

| Target | Status | Toolchain | Configurations |
| --- | --- | --- | --- |
| Windows x64 | Supported | Visual Studio 2022 or Build Tools with MSVC `v143` | `Debug|x64`, `Release|x64` |

## Language And Dependencies

- The project minimum language mode is C++17 (`stdcpp17` in `projects/msvc/game.vcxproj`).
- C++20 is not enabled in the current solution.
- Dependencies are restored through the vcpkg manifest at `vcpkg.json`.
- The current vcpkg triplet is `x64-windows-static`.
- The current vcpkg baseline is `000d1bda1ffa95a73e0b40334fa4103d6f4d3d48`.

## Planned Hardening Matrix

The remaining matrix gap is a blocking UBSan build. The current repository is MSBuild/Windows-first; UBSan is configured through ClangCL but stays experimental until the CI image includes the Visual Studio ClangCL platform toolset and the project links cleanly with the sanitizer runtime.
