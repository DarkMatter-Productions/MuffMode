# MuffMode Release Process

[README](../README.md) | [Build Guide](build-guide.md) | [Configuration Reference](configuration-reference.md) | [Changelog](changelog.md)

This project is currently in **Beta**. The release script defaults to `-Channel beta`, names packages with the `-beta` suffix, and publishes GitHub releases with the prerelease flag. GitHub does not allow prereleases to be marked as the repository's "Latest" release, so only stable releases force the latest flag.

This project uses [VERSION](../VERSION) as the release version source of truth. The in-game mod version in [src/sgame/g_local.h](../src/sgame/g_local.h) must match it before publishing a release.

The release script is [scripts/release.ps1](../scripts/release.ps1). It builds a beta zip package shaped like the `muffmode-0.22.15-beta.zip` release asset, plus a Windows installer asset:

Run it with PowerShell 7 (`pwsh`). The script enforces that prerequisite before
doing version, build, or packaging work.

```text
muffmode-<version>-beta/
  README.html
  README.de.html
  README.pl.html
  README.fr.html
  README.hu.html
  README.bg.html
  CHANGELOG.md
  LICENSE
  THIRD_PARTY_NOTICES.md
  MuffModeUpdater.exe
  rerelease/
    baseq2/
      game_x64.dll
      muffmode-version.json
      muffmode.version
      ...
```

```text
muffmode-<version>-beta.zip
muffmode-<version>-beta-windows-installer.exe
muffmode-<version>-beta-map-sources.zip
```

GitHub also adds the automatic source-code archives. Discord announcements keep the primary download list intentionally short and ordered as the installable `muffmode-<version>[-channel].zip`, Windows installer, map sources, then Source Code. Supplemental original-map archives may still be uploaded for preservation, but they are not listed as primary Discord downloads.

Release notes are generated from the central [changelog ledger](changelog.md), not from commit subjects. The script still prefers [GitHub Copilot CLI](https://docs.github.com/copilot/how-tos/set-up/install-copilot-cli) for the English `README.html` in non-interactive mode. If Copilot is not installed or authenticated, it falls back to a styled deterministic English HTML README. When Copilot is available, the script translates that guide into German, Polish, French, Hungarian, and Bulgarian as `README.de.html`, `README.pl.html`, `README.fr.html`, `README.hu.html`, and `README.bg.html`, validating protected code snippets, commands, cvars, config names, links, key constants, and visible prose before the installer is assembled. Without a Copilot token, local packaging ships a self-contained English-only guide; pass `-RequireCopilot` when a release must fail instead of omitting localized guides.
Pass `-RequireCopilot` when you intentionally want the release to fail unless Copilot also generates the original English README, instead of using the deterministic English fallback.
It also writes `rerelease/baseq2/muffmode-version.json` and `rerelease/baseq2/muffmode.version` so the Windows updater and launcher can compare the installed version with GitHub releases.
The package also includes the published Windows updater and launcher executable at the package root.

## Changelog Ledger

[docs/changelog.md](changelog.md) is the source of truth for release notes, release highlights, package `CHANGELOG.md`, GitHub release notes, and the Discord announcement summary.

Required table columns:

| Column | Purpose |
| --- | --- |
| `Release` | `Unreleased` until the release script stamps the shipped version, such as `0.36.21`. |
| `Category` | One of the approved public release categories: player experience, competitive play, server hosting, gameplay and balance, maps and content, fixes, documentation and packaging, or internal maintenance. |
| `Magnitude` | `major`, `minor`, or `patch`. This is release-note impact, not a promise that the semantic version must move by the same word. |
| `Summary` | A grouped user-readable change title. Similar work should share one row. |
| `Details` | A practical explanation of what changed and why it matters. |

Before finishing any implementation, inspect the existing `Unreleased` rows. If the work belongs with an existing row, rewrite that row's summary/details/category/magnitude instead of adding a duplicate. The release script validates the table, rejects duplicate summaries within the same release/category, and refuses to package a release with no matching `Unreleased` or target-version rows.

CI runs `scripts/ci/check-changelog.ps1` to validate the table. On pull requests, implementation-facing changes must include a `docs/changelog.md` update with at least one `Unreleased` row.

Highlight rules are strict: release highlights include only `major` rows. If a release has no `major` rows, the generator falls back to the most relevant smaller rows and says that no major changes are logged.

When `-UpdateVersionFiles` is used, the script updates `VERSION`, `src/sgame/g_local.h`, and every `Unreleased` changelog row to the target release version.

The Windows installer is built with [Inno Setup 6](https://jrsoftware.org/isinfo.php). The release script looks for `ISCC.exe` on `PATH`, then in the normal Inno Setup install folders. You can pass `-InnoSetupCompiler "C:\Path\To\ISCC.exe"` to override detection or `-SkipInstaller` for a zip-only local package. The wrapper validates the packaged DLL/updater and generated installer as Windows PE images, preserves the Inno Setup compiler log beside the release output, and fails publishing if the expected installer is missing when installer generation was not skipped.

## Release State

| Channel | Package suffix | GitHub release flag |
| --- | --- | --- |
| `alpha` | `-alpha` | Prerelease, not latest |
| `beta` | `-beta` | Prerelease, not latest |
| `rc` | `-rc` | Prerelease, not latest |
| `stable` | none | Latest release |

Use the default `beta` channel until the project is ready to leave beta.

## Version Modes

| Mode | Behavior |
| --- | --- |
| `auto` | Inspects changes since the previous release and chooses major, minor, or patch. Breaking-change markers select major; new features, installer/updater/package/workflow additions, commands, cvars, maps, rulesets, gametypes, menu, or voting work select minor; smaller fixes and docs select patch. If the committed source version is already at or above the detected target, it is kept. |
| `major` | Bumps from the latest release to the next major version. |
| `minor` | Bumps from the latest release to the next minor version. |
| `patch` | Bumps from the latest release to the next patch version. |
| `latch` | Accepted as a patch alias. |

You can also pass `-Version 0.23.0` to use an exact version.

## Prepare Version Files

Use this when the selected target version does not match `VERSION` and `GAMEMOD_VERSION`:

```powershell
.\scripts\release.ps1 -VersionMode patch -UpdateVersionFiles -SkipBuild -AllowDirtyPackage
```

Commit the version and changelog stamp changes before publishing a GitHub release.

## Add Release-Only Assets

Place package-only files in [packaging/release-assets](../packaging/release-assets) using the final package layout. For example:

```text
packaging/release-assets/
  rerelease/
    baseq2/
      server-base.cfg
      CONFIGS_README.md
      gt-FFA.cfg
      gt-DUEL.cfg
      gt-LMS.cfg
      bots/navigation/*.nav
      maps/*.ent
      maps/*.bsp
```

The script copies these files into the generated package, then overwrites/adds the built DLL, `README.html`, localized `README.<language>.html` guides, `CHANGELOG.md`, `LICENSE`, and `THIRD_PARTY_NOTICES.md`.

## Build A Local Package

Run from a Visual Studio developer shell:

```powershell
.\scripts\release.ps1 -VersionMode auto
```

Normal `release.ps1` builds compile with warnings as errors. Use `-SkipBuild` only when `build\msbuild\x64\Release\game_x64.dll` already exists. Published skipped-build paths also require `muffmode-build-receipt.json` beside the DLL, binding its SHA-256 digest, strict build setting, and source commit. A normal `release.ps1` build writes this receipt automatically; the release workflow writes it immediately after the strict build. A local unpublished package may reuse a DLL without a receipt, but emits a warning and cannot be promoted with `-CreateGitHubRelease` until rebuilt through a receipt-producing path.
Use `-SkipUpdaterBuild` only for a local, unpublished package when `MuffModeUpdater.exe` already exists under the updater publish output. GitHub Actions and `-CreateGitHubRelease` reject updater reuse because that executable has no commit-bound build receipt.
Use `-SkipInstaller` only when you intentionally want to create the zip without the Windows installer asset.

The package and generated release files are written to `dist/release`.

## Windows Installer

The installer is generated from [packaging/installer/muffmode-installer.iss](../packaging/installer/muffmode-installer.iss). It installs the same payload as the zip into the outer Quake II folder, not directly into `rerelease` or `baseq2`.

Installer behavior:

| Choice | Default path |
| --- | --- |
| Steam | `C:\Program Files (x86)\Steam\steamapps\common\Quake 2` |
| Epic Games Store | `C:\Program Files\Epic Games\Quake 2` |
| GOG | `C:\GOG Games\Quake II` |
| Xbox app / Microsoft Store | Detected app install path when available |
| Custom or another library | User-selected folder |

The installer checks Steam's install registry and `libraryfolders.vdf`, Epic Games Store manifest files, GOG registry/common install locations, and Xbox app / Microsoft Store candidates before showing the store choices. It displays the selected path before the folder page and still offers an **Other location** choice for custom libraries. The installer shows the project license, lets users browse after choosing a preset, warns if `rerelease\baseq2` is not found, corrects accidental `rerelease` or `baseq2` folder selection back to the outer Quake II folder, rejects existing reparse points or junctions along every payload and backup destination branch, bounds automatic config/DLL backup size and count with a matching disk-space reserve, prompts users to close applications holding files open, offers Desktop and Start menu shortcuts for the updater, launcher, install guide, changelog, and server config guide, verifies installed files, writes an install receipt, and backs up existing server configs plus `rerelease\baseq2\game_x64.dll` under `rerelease\baseq2\MuffModeBackups` before replacing them. The installed root also includes `THIRD_PARTY_NOTICES.md`.

Silent installer runs should pass `/DIR="C:\Path\To\Quake 2"` explicitly. In silent mode the installer preserves the supplied destination and refuses to continue unless it points at an outer Quake II folder containing `rerelease\baseq2`.

## Updater And Launcher

The release script publishes the Windows updater and launcher automatically and includes `MuffModeUpdater.exe` in every release package. To publish a self-contained executable manually:

```powershell
dotnet publish updater\MuffMode.Updater\MuffMode.Updater.csproj -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true
```

See the [Updater Guide](updater-guide.md) for the updater workflow and local version marker behavior.

## Publish From GitHub Actions

Use the **Release Muff Mode** workflow in GitHub Actions for normal releases. It validates free-form dispatch values through step-scoped environment variables, requires `previous_tag` to resolve through the exact local `refs/tags` namespace, updates and commits version files when requested, resolves the resulting commit to an immutable SHA, and checks out and verifies that exact SHA in the build job. Every third-party action is pinned to a reviewed commit, Node.js, .NET, Copilot CLI, Inno Setup, and vcpkg are fixed and runtime-checked where applicable, and release credentials are not exposed during tool installation. The workflow reruns the tooling/dependency/regression contracts plus host and updater tests, builds the Release x64 DLL with warnings as errors, and records the exact DLL hash in a commit-bound build receipt. Packaging then runs [scripts/release.ps1](../scripts/release.ps1) with `-SkipBuild` so the verified strict DLL is reused instead of being replaced by a second non-strict build. The workflow publishes a freshly built updater, builds the Windows installer with the verified compiler path, creates or verifies the exact version tag at the packaged SHA, publishes with `--verify-tag`, uploads the package zip, installer, map-source archive, and supplemental original-map archive, and posts the Discord announcement.

Every package records the exact source commit and whether its source tree was dirty in `rerelease/baseq2/muffmode-version.json`. Release output also includes a provenance JSON document that binds the clean source commit to every binary asset's name, size, and SHA-256 digest, plus a standard `SHA256SUMS.txt` covering those assets and the provenance document. The publish job derives the one exact asset set from the selected version and channel, requires every provenance property and asset record exactly once, recomputes every digest, and rejects missing, duplicate, extra, dirty, or commit-mismatched metadata before uploading any GitHub release asset. Local `-CreateGitHubRelease` publishing likewise targets the recorded commit explicitly. `-Prerelease` cannot be combined with `-Channel stable`, because the updater intentionally recognizes prereleases only by their `-alpha`, `-beta`, or `-rc` package suffix.

Protect `v*` release tags against update and deletion in repository tag rules. The publisher handles a benign create race by re-reading the tag and accepting it only when annotated-tag peeling reaches the packaged source commit, but tag protection is what prevents another writer from moving the ref between verification and release creation.

GitHub's workflow graph shows jobs, not individual steps. This workflow is intentionally split into visible release jobs: **Preflight**, **Resolve Version**, **Build And Package**, **Publish GitHub Release**, and **Announce On Discord**.

Use the separate **Validate Release Workflow** action when you only want to verify workflow startup and version resolution. It never builds, publishes, or announces a release, which keeps dry-run validation out of the actual release graph.

## Release Go/No-Go

Before publishing, run the hardening gates in [Hardening Guide](hardening-guide.md). The release workflow repeats its release-local dependency, tooling, corpus, host-test, updater-test, and strict-build subset against the exact package commit. A release is blocked if dependency notices are stale, generated artifacts are tracked, required tests fail, CodeQL/analysis findings are untriaged, sanitizer support regresses, or the package is missing `LICENSE` / `THIRD_PARTY_NOTICES.md`.

Required repository secrets:

| Secret | Purpose |
| --- | --- |
| `COPILOT_GITHUB_TOKEN` | Optional. Fine-grained user PAT used only by the standalone `copilot` CLI. It must belong to a user with GitHub Copilot access and include the Copilot Requests permission. Without it (and with `require_copilot` unchecked), the release ships an English-only README and skips localized translations. |
| `DISCORD_RELEASE_WEBHOOK` | Discord webhook consumed by the release announcement job. The release workflow checks that it exists before publishing. |

Optional repository variables:

| Variable | Purpose |
| --- | --- |
| `DISCORD_RELEASE_EMOJI` | Emoji prepended to the Discord announcement headline. Defaults to `<:quake2:1174744083403657316>`. |
| `DISCORD_RELEASE_MENTIONS` | Text appended to the Discord announcement headline. Defaults to `<@&1424165484491964667> <@&1390287267276525628>` for the `@quake2` and `@playtester` roles. Use Discord role mention IDs such as `<@&123456789>` if you want actual role notifications. |
| `DISCORD_FEEDBACK_CHANNEL` | Channel link used in the feedback line. Defaults to `<#1509926054175834133>` for `#muffmode`. |

`RELEASE_BOT_TOKEN` is accepted as a legacy fallback for Copilot authentication, but the built-in `GITHUB_TOKEN` handles version-file commits and `gh release create`. Write permission is granted only to those version and publish jobs; the build job is read-only. GitHub and Copilot tokens are scoped to their consuming steps and are not exposed while release tools are installed and verified. If no Copilot token is present, the package step warns and ships without localized README translations (English README only) -- unless `require_copilot` is checked, in which case it fails instead. Because releases created with `GITHUB_TOKEN` do not trigger other workflows, this release workflow posts the Discord announcement itself after the GitHub release is published. The separate **Broadcast Release To Discord** workflow remains useful for releases published manually through GitHub.

Workflow inputs:

| Input | Purpose |
| --- | --- |
| `version_mode` | `auto`, `major`, `minor`, `patch`, or `latch` as a patch alias. |
| `version` | Optional exact version, such as `0.23.0`. |
| `previous_tag` | Optional changelog start tag override. |
| `channel` | Defaults to `beta`; non-stable channels publish as prereleases. |
| `commit_version_files` | Updates `VERSION` and `src/sgame/g_local.h`, commits, and pushes before publishing. |
| `skip_installer` | Creates only the zip package when the installer is intentionally not wanted. |
| `require_copilot` | Fails the release if Copilot authentication is unavailable, instead of falling back to a deterministic English-only README with no localized translations. Leave disabled to allow that fallback. |
| `release_intro` | Optional manual intro for the GitHub release notes and Discord announcement. Leave blank to let the script generate one from the most significant changelog entries. |

The release workflow downloads the self-contained Windows x64 GitHub Copilot CLI `1.0.77` platform tarball once, verifies its bytes against the reviewed SHA-512 integrity value, extracts and runs that exact executable without npm dependency resolution, and installs the reviewed Inno Setup `6.7.1` Chocolatey package. Its workflow contract rejects unpinned or re-fetched Copilot replacements. The workflow primes `copilot --help` before any Copilot credential or shell-visible `GH_TOKEN` is made available to later build steps. When a Copilot token is configured, `COPILOT_GITHUB_TOKEN` exists only inside the package step that generates the end-user HTML README and its localized German, Polish, French, Hungarian, and Bulgarian siblings. Without a token, packaging falls back to a deterministic English-only README and skips the localized siblings entirely (unless `require_copilot` is checked).

## Publish Locally

After committing version changes and ensuring the working tree is clean:

```powershell
.\scripts\release.ps1 -VersionMode auto -CreateGitHubRelease
```

The script creates `v<version>` at the packaged source commit, or rejects an existing tag that peels to any other commit, then publishes with `--verify-tag`. It uploads the package zip, Windows installer, map-source archive, and supplemental original-map archive, and uses the generated changelog as release notes. Stable releases pass `--latest`; beta, alpha, and release-candidate builds pass `--prerelease --latest=false` because GitHub rejects releases that are both latest and prerelease.

## Changelog Scope

The changelog is generated from [docs/changelog.md](changelog.md). Rows marked `Unreleased` are used for local package builds before release stamping; rows stamped with the target release version are used after `-UpdateVersionFiles` commits the release metadata. Pass `-PreviousTag v0.22.15` to override the compare link start tag.

The compare link still uses `<previous-release-tag>...v<target-version>` so GitHub can show the exact commits, but public release wording comes from the grouped ledger rows. This keeps release details stable even when a feature arrives through several commits or when a commit contains both user-facing and internal work.

`-PreviousTag` must name an existing exact local tag; branches, `HEAD` aliases, and raw commit IDs are not accepted as changelog boundaries.

If `-ReleaseIntro` or `MUFFMODE_RELEASE_INTRO` is supplied, that text becomes the release-note intro and Discord intro. During a local interactive Windows release run, the script also tries to show a small text box seeded with the generated intro so maintainers can replace it before packaging. Use `-NoIntroPrompt` for unattended local builds.
