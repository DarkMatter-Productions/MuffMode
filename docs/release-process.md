# MuffMode Release Process

[README](../README.md) | [Build Guide](build-guide.md) | [Configuration Reference](configuration-reference.md)

This project is currently in **Beta**. The release script defaults to `-Channel beta`, names packages with the `-beta` suffix, and publishes GitHub releases with the prerelease flag. GitHub does not allow prereleases to be marked as the repository's "Latest" release, so only stable releases force the latest flag.

This project uses [VERSION](../VERSION) as the release version source of truth. The in-game mod version in [src/g_local.h](../src/g_local.h) must match it before publishing a release.

The release script is [scripts/release.ps1](../scripts/release.ps1). It builds a beta zip package shaped like the `muffmode-0.22.15-beta.zip` release asset, plus a Windows installer asset:

```text
muffmode-<version>-beta/
  README.html
  CHANGELOG.md
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
```

The script prefers [GitHub Copilot CLI](https://docs.github.com/copilot/how-tos/set-up/install-copilot-cli) for both `CHANGELOG.md` and `README.html` in non-interactive mode. If Copilot is not installed or authenticated, it falls back to deterministic release notes and a styled end-user HTML README so packaging can still complete. Pass `-RequireCopilot` when you intentionally want the release to fail unless Copilot generation succeeds.
It also writes `rerelease/baseq2/muffmode-version.json` and `rerelease/baseq2/muffmode.version` so the Windows updater can compare the installed version with GitHub releases.
The package also includes the published Windows updater executable at the package root.

The Windows installer is built with [Inno Setup 6](https://jrsoftware.org/isinfo.php). The release script looks for `ISCC.exe` on `PATH`, then in the normal Inno Setup install folders. You can pass `-InnoSetupCompiler "C:\Path\To\ISCC.exe"` to override detection or `-SkipInstaller` for a zip-only local package.

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

Commit the version changes before publishing a GitHub release.

## Add Release-Only Assets

Place package-only files in [packaging/release-assets](../packaging/release-assets) using the final package layout. For example:

```text
packaging/release-assets/
  rerelease/
    baseq2/
      muff-sv.cfg
      SERVER_CONFIGS.md
      bots/navigation/*.nav
      maps/*.ent
      maps/*.bsp
```

The script copies these files into the generated package, then overwrites/adds the built DLL, `README.html`, and `CHANGELOG.md`.

## Build A Local Package

Run from a Visual Studio developer shell:

```powershell
.\scripts\release.ps1 -VersionMode auto
```

Use `-SkipBuild` only when `game_x64.dll` already exists at the repository root.
Use `-SkipUpdaterBuild` only when `MuffModeUpdater.exe` already exists under the updater publish output.
Use `-SkipInstaller` only when you intentionally want to create the zip without the Windows installer asset.

The package and generated release files are written to `dist/release`.

## Windows Installer

The installer is generated from [packaging/installer/muffmode-installer.iss](../packaging/installer/muffmode-installer.iss). It installs the same payload as the zip into the outer Quake II folder, not directly into `rerelease` or `baseq2`.

Installer behavior:

| Choice | Default path |
| --- | --- |
| Steam | `C:\Program Files (x86)\Steam\steamapps\common\Quake 2` |
| Epic Online Store / Epic Games Store | `C:\Program Files\Epic Games\Quake 2` |
| GOG | `C:\GOG Games\Quake II` |
| Custom or another library | User-selected folder |

Steam is selected by default. The installer lets users browse after choosing a preset, warns if `rerelease\baseq2` is not found, corrects accidental `rerelease` folder selection back to the outer Quake II folder, and backs up an existing `rerelease\baseq2\game_x64.dll` under `rerelease\baseq2\MuffModeBackups` before replacing it.

## Updater

The release script publishes the Windows updater automatically and includes `MuffModeUpdater.exe` in every release package. To publish a self-contained updater executable manually:

```powershell
dotnet publish updater\MuffMode.Updater\MuffMode.Updater.csproj -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true
```

See the [Updater Guide](updater-guide.md) for the updater workflow and local version marker behavior.

## Publish From GitHub Actions

Use the **Release Muff Mode** workflow in GitHub Actions for normal releases. It runs [scripts/release.ps1](../scripts/release.ps1), updates and commits version files when requested, builds the DLL, publishes the updater, builds the Windows installer, creates the GitHub release, uploads the zip and installer assets, and posts the Discord announcement.

GitHub's workflow graph shows jobs, not individual steps. This workflow is intentionally split into visible release jobs: **Preflight**, **Resolve Version**, **Build And Package**, **Publish GitHub Release**, and **Announce On Discord**.

Use the separate **Validate Release Workflow** action when you only want to verify workflow startup and version resolution. It never builds, publishes, or announces a release, which keeps dry-run validation out of the actual release graph.

Required repository secrets:

| Secret | Purpose |
| --- | --- |
| `COPILOT_GITHUB_TOKEN` | Fine-grained user PAT used only by the standalone `copilot` CLI. It must belong to a user with GitHub Copilot access and include the Copilot Requests permission. |
| `DISCORD_RELEASE_WEBHOOK` | Discord webhook consumed by the release announcement job. The release workflow checks that it exists before publishing. |

`RELEASE_BOT_TOKEN` is accepted as a legacy fallback for Copilot authentication, but the built-in `GITHUB_TOKEN` now handles version-file commits and `gh release create`. If no Copilot token is present, the workflow warns and uses the deterministic documentation generator unless `require_copilot` is enabled. Because releases created with `GITHUB_TOKEN` do not trigger other workflows, this release workflow posts the Discord announcement itself after the GitHub release is published. The separate **Broadcast Release To Discord** workflow remains useful for releases published manually through GitHub.

Workflow inputs:

| Input | Purpose |
| --- | --- |
| `version_mode` | `auto`, `major`, `minor`, `patch`, or `latch` as a patch alias. |
| `version` | Optional exact version, such as `0.23.0`. |
| `previous_tag` | Optional changelog start tag override. |
| `channel` | Defaults to `beta`; non-stable channels publish as prereleases. |
| `commit_version_files` | Updates `VERSION` and `src/g_local.h`, commits, and pushes before publishing. |
| `skip_installer` | Creates only the zip package when the installer is intentionally not wanted. |
| `require_copilot` | Requires GitHub Copilot CLI generation and fails if Copilot authentication is unavailable. Leave disabled to use the deterministic fallback when needed. |

When a Copilot token is configured, the release workflow installs the current standalone GitHub Copilot CLI with `npm install -g @github/copilot`, exports `COPILOT_GITHUB_TOKEN`, and primes `copilot --help` before generating the changelog and end-user HTML README.

## Publish Locally

After committing version changes and ensuring the working tree is clean:

```powershell
.\scripts\release.ps1 -VersionMode auto -CreateGitHubRelease
```

The script creates `v<version>`, uploads the package zip and Windows installer, and uses the generated changelog as release notes. Stable releases pass `--latest`; beta, alpha, and release-candidate builds pass `--prerelease --latest=false` because GitHub rejects releases that are both latest and prerelease.

## Changelog Scope

The changelog is generated from `git log <previous-release-tag>..HEAD`, so it includes only commits since the previous release. Pass `-PreviousTag v0.22.15` to override the detected start tag.

When Copilot is available, it receives a structured change context for that exact range: commit subjects, merge subjects, changed files, diff stats, and compare URL. The script writes the full prompt to `dist/release/copilot-prompts`, then gives Copilot a short programmatic instruction to read that file, which avoids Windows command-line length limits. The changelog prompt uses silent/no-question mode and grants only Git shell access for inspection. It is prompted to summarize by practical impact for casual players, competitive players, and server hosts, using only relevant categories such as player experience, competitive play, server hosting, gameplay and balance, fixes, documentation, packaging, and internal maintenance.

The script validates Copilot output before packaging. If Copilot is unavailable and `-RequireCopilot` is not used, the deterministic fallback still scopes the changelog to the same git range and creates a Quake II styled end-user README focused on installation, usage, hosting, package contents, and the changelog.
