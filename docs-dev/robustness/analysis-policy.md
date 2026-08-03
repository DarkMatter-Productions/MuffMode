# MuffMode Analysis Policy

Date: 2026-08-03

Phase two analysis gates are intentionally script-first. CI should call the same scripts that local developers call, and workflow YAML should only provide runner setup, credentials and artifact upload.

## Canonical Entrypoints

| Purpose | Entrypoint |
|---|---|
| Build with MSBuild | `scripts/ci/build-msbuild.ps1` |
| Restore vcpkg dependencies | `scripts/ci/setup-vcpkg.ps1 -Install` |
| Generate compile database | `scripts/ci/export-compile-commands.ps1` |
| MSVC code analysis | `scripts/ci/run-msvc-analyze.ps1` |
| clang-tidy | `scripts/ci/run-clang-tidy.ps1` |
| Cppcheck | `scripts/ci/run-cppcheck.ps1` |
| Sanitizer builds | `scripts/ci/run-sanitized-build.ps1` |

## Warning Policy

MSVC warning-clean builds are the first enforced warning gate. The active build workflow calls `build-msbuild.ps1 -TreatWarningsAsErrors`, which maps to the project-local `MMTreatWarningsAsErrors` property in `projects/msvc/MuffMode.Analysis.props`.

Clang diagnostics are introduced through `clang-tidy` on touched C++ files first. Header changes and runs without a trustworthy comparison commit scan the full compile database; documentation-only diffs exit cleanly without silently expanding the scan. The full configured rule set remains visible in the text artifact, while the reviewed clean-baseline correctness, lifetime, memory, nullability, and security checks in `scripts/ci/clang-tidy-blocking-checks.txt` fail the job. The runner validates the blocking list against enabled checks and considers every diagnostic alias, so stale patterns or secondary aliases cannot bypass the ratchet. MSVC `/analyze` follows the same model: every enabled diagnostic remains in its text and binary logs, while the clean high-confidence correctness and safety codes in `scripts/ci/msvc-analyze-blocking-warnings.txt` are enforced with normalized, deduplicated parsing. Cppcheck findings are blocking locally and in CI. Narrow reviewed suppressions are the only supported way to exempt a blocking finding. As legacy findings are triaged, add their clean rule families to the blocking lists and expand clang-tidy from touched files to all project compile units.

## Static Analysis Policy

Analyzer jobs are non-packaging jobs and must publish machine-readable or durable output under `build/analysis/`:

- MSVC `/analyze`: text and binary log artifacts.
- clang-tidy: text artifact from the selected compile units.
- Cppcheck: XML artifact.
- CodeQL: GitHub code scanning upload.

The security/reliability maintainer is the triage owner for new alerts unless a narrower subsystem owner is named in the suppression record.

## Sanitizer Policy

AddressSanitizer is the blocking sanitizer build on the Windows/MSBuild path. UndefinedBehaviorSanitizer is configured through ClangCL and `MMEnableUndefinedSanitizer`; it should become blocking after the CI runner and project toolset prove stable.

Sanitizer jobs currently verify that the instrumented DLL builds. Runtime sanitizer coverage belongs in phase three, when headless smoke tests and parser/unit harnesses can execute instrumented code.

## Suppression Policy

Suppressions are allowed only when all of these are true:

- The finding is confirmed false-positive or too risky to fix immediately.
- The narrowest possible rule/path scope is used.
- `docs-dev/robustness/suppressions.yml` records owner, rule, path, reason, issue and expiry.
- Expired suppressions are treated as analysis debt and must be re-triaged before release.
