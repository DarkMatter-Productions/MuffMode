# MuffMode Profiling Guide

Date: 2026-06-15
Branch: `muffdev`

Phase four profiling is source-instrumented but opt-in. The normal release build keeps the hooks compiled out except for zero-cost Tracy scope names.

## Build Modes

| Mode | MSBuild switch | Purpose |
|---|---|---|
| Default | none | Production-equivalent build with profiling hooks compiled out. |
| Counter build | `/p:MMEnableProfileCounters=true` | Enables atomic counters for frame loops, trigger/projectile touches, kill-box scans and LOS checks. |
| Tracy build | `/p:MMUseTracy=true` plus a configured Tracy include path/library | Enables `ZoneScopedN` and frame markers for instrumented gameplay paths. |

Use the shared build entrypoint so the profiling configuration stays reproducible:

```powershell
./scripts/ci/build-msbuild.ps1 -Configuration Release -Platform x64 -AdditionalMsBuildArgs "/p:MMEnableProfileCounters=true"
```

## Instrumented Paths

The first-wave zones and counters cover:

- `G_RunFrame` and `G_RunFrame_`, including main-loop versus non-main-loop calls.
- Entity-loop visits, split by client and non-client entities.
- `G_TouchTriggers`, including `BoxEntities` result counts and dispatched touches.
- `G_TouchProjectiles`, including traces, skipped projectiles, impacts, overflow protection and max skips per call.
- `KillBox`, including `BoxEntities` result counts and damage events.
- AI LOS `visible()`, including visibility calls and actual `traceline` calls.

## Capture Policy

For Windows server/gameplay profiling, use WPR/WPA alongside a counter or Tracy build. Capture at least one busy-server scenario with active players, monsters and projectiles before claiming a hot-path change is performance-positive.

For Linux, use `perf` only once a Linux build target exists in the supported matrix. Until then, Linux profiling remains a portability follow-up rather than a phase-four acceptance artifact.

Use RenderDoc only for `cg_*`, HUD, and client-visual work. It is not the right tool for server gameplay frame-time analysis.

## Reading Results

Counter builds expose the `MM_ProfileCounters()` static counters to the debugger. Tracy builds should show named zones for the instrumented functions and a `G_RunFrame` frame mark. When evaluating allocation churn, compare `G_TouchProjectiles` before and after phase four: projectile-skip bookkeeping no longer allocates dynamically because it uses a bounded `MAX_ENTITIES` array.
