# MuffMode Release Process

[README](../README.md) | [Build Guide](build-guide.md) | [Configuration Reference](configuration-reference.md)

This project is currently in **Beta**. The release script defaults to `-Channel beta`, names packages with the `-beta` suffix, and publishes GitHub releases with the prerelease flag while still marking them as latest.

This project uses [VERSION](../VERSION) as the release version source of truth. The in-game mod version in [src/g_local.h](../src/g_local.h) must match it before publishing a release.

The release script is [scripts/release.ps1](../scripts/release.ps1). It builds a beta package shaped like the `muffmode-0.22.15-beta.zip` release asset:

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

The script always generates both `CHANGELOG.md` and `README.html` through GitHub Copilot in non-interactive mode. If `gh copilot` is not installed or authenticated, the script fails instead of falling back to plain commit output.
It also writes `rerelease/baseq2/muffmode-version.json` and `rerelease/baseq2/muffmode.version` so the Windows updater can compare the installed version with GitHub releases.
The package also includes the published Windows updater executable at the package root.

## Release State

| Channel | Package suffix | GitHub release flag |
| --- | --- | --- |
| `alpha` | `-alpha` | Prerelease and latest |
| `beta` | `-beta` | Prerelease and latest |
| `rc` | `-rc` | Prerelease and latest |
| `stable` | none | Latest release |

Use the default `beta` channel until the project is ready to leave beta.

## Version Modes

| Mode | Behavior |
| --- | --- |
| `auto` | Uses the current source version when it is newer than the latest GitHub release; otherwise bumps patch. |
| `major` | Bumps from the latest release to the next major version. |
| `minor` | Bumps from the latest release to the next minor version. |
| `patch` | Bumps from the latest release to the next patch version. |

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

The package and generated release files are written to `dist/release`.

## Updater

The release script publishes the Windows updater automatically and includes `MuffModeUpdater.exe` in every release package. To publish a self-contained updater executable manually:

```powershell
dotnet publish updater\MuffMode.Updater\MuffMode.Updater.csproj -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true
```

See the [Updater Guide](updater-guide.md) for the updater workflow and local version marker behavior.

## Publish A GitHub Release

After committing version changes and ensuring the working tree is clean:

```powershell
.\scripts\release.ps1 -VersionMode auto -CreateGitHubRelease
```

The script creates `v<version>`, uploads the package zip, uses the generated changelog as release notes, and passes `--latest` to `gh release create`. For the default beta channel it also passes `--prerelease`, so the release is both latest and clearly flagged as beta-stage.

## Changelog Scope

The changelog is generated from `git log <previous-release-tag>..HEAD`, so it includes only commits since the previous release. Pass `-PreviousTag v0.22.15` to override the detected start tag.

Copilot receives a structured change context for that exact range: commit subjects, merge subjects, changed files, diff stats, and compare URL. The prompt is piped into Copilot to avoid command-line length limits, uses silent/no-question mode, and grants only Git shell access for inspection. It is prompted to summarize by practical impact for casual players, competitive players, and server hosts, using only relevant categories such as player experience, competitive play, server hosting, gameplay and balance, fixes, documentation, packaging, and internal maintenance.

The script validates that Copilot returned clean Markdown with a release title, category headings, bullet-point notes, the target version, and the previous release tag before it packages or publishes the notes.
