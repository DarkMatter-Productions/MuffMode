# MuffDev Robustness Baseline - 2026-06-15

This baseline freezes the branch used for phase-zero robustness work. The original robustness plan was written against `main`; this implementation targets `muffdev`, copied from `origin/ozdev`.

## Git Tuple

| Field | Value |
| --- | --- |
| Target branch | `muffdev` |
| Source branch | `origin/ozdev` |
| Baseline branch | `codex/audit-baseline-2026-06-15` |
| Baseline commit | `0853a089d38c0f8528e8dc009ac2bd46315608c8` |
| Short SHA | `0853a089d38c` |
| VERSION | `0.36.01` |
| Plan file | `docs-dev/plans/2026-06-15-robustness.md` |

## Current Integrated Tip

| Field | Value |
| --- | --- |
| Integrated source | `origin/ozdev` |
| Integrated commit | `9c13a5f6c46ce1a201d36eff700d25292b6c3df4` |
| Integrated short SHA | `9c13a5f` |
| Integrated VERSION | `0.36.04` |
| Integrated commits reviewed | `0e4ec722549ce54f5d3ebf32fe2ad5972f1955e4`, `9c13a5f6c46ce1a201d36eff700d25292b6c3df4` |

The audit baseline branch remains frozen at `0853a089d38c0f8528e8dc009ac2bd46315608c8`. `muffdev` has since been fast-forwarded to the integrated tip above before continuing local phase-zero work.

## Build Tuple

| Field | Value |
| --- | --- |
| CI workflow | `.github/workflows/build.yml` |
| CI target branch | `muffdev` |
| Runner image | `windows-2025-vs2026` |
| Solution | `src/MuffMode.sln` |
| Project | `src/game.vcxproj` |
| CI configuration | `Release|x64` |
| Local debug configuration | `Debug|x64` |
| Compiler family | MSVC |
| Platform toolset | `v143` |
| Language standard | C++17 (`stdcpp17`) |
| Windows SDK selector | `10.0` |
| vcpkg triplet | `x64-windows-static` |
| vcpkg manifest | `src/vcpkg.json` |
| vcpkg baseline | `000d1bda1ffa95a73e0b40334fa4103d6f4d3d48` |
| Generated-artifact guard | `scripts/ci/check-generated-artifacts.ps1` |
| Asset-seed guard | `scripts/ci/check-test-assets.ps1 -RepoOnly` |

The last tracked `ozdev` build transcript reported MSBuild `17.14.40+3e7442088` and MSVC toolset path `14.44.35207`. That transcript has been removed as generated output; future exact compiler versions should be recorded by CI run metadata or a dedicated build-manifest artifact.

## Required Game Assets

Muff Mode requires a legitimate Quake II Rerelease installation for full runtime smoke/regression testing. Licensed game data must not be committed to this repository.

The repo-side asset-pack scaffold is in `docs-dev/test-assets/`. It defines a deterministic external asset root through `MUFFMODE_TEST_ASSET_ROOT` plus redistributable smoke/config/command/rotation/save-load seeds.

Use `scripts/ci/check-test-assets.ps1 -RepoOnly` to validate the checked-in seeds. Use `scripts/ci/check-test-assets.ps1` with `MUFFMODE_TEST_ASSET_ROOT` set to validate a complete external pack containing a legitimate Quake II Rerelease installation.

## Branch Delta Notes

Compared with `origin/main`, `muffdev` already contains substantial gameplay and packaging changes:

- Red Rover, CaptureStrike and expanded Horde work are present.
- `VERSION` is `0.36.01`, not the `main` value used when the original audit was drafted.
- New release configs include `gt-REDROVER.cfg`, `gt-STRIKE.cfg` and `gt-HORDE-endless.cfg`.
- `src/g_svcmds.cpp` no longer contains the old IP filter implementation that the main-derived plan calls out, so phase-one IP filter tasks must be revalidated against `muffdev` before implementation.
- `build_out.txt` existed on the `ozdev` lineage as a generated build transcript and was removed during phase zero.
- The later `9c13a5f` Red Rover fixes address match-end CTD, score/frag-limit exit, spectator joins during active matches, team-switch blocking, and `frag_warning` lower-bound handling. Phase three now tracks regression coverage for these cases.

Phase-one implementation should use the checklist as guidance, but it must confirm each finding against `muffdev` before changing code.
