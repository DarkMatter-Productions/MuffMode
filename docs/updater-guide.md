# Muff Mode Updater & Launcher Guide

The Muff Mode updater and launcher is a Windows desktop app in [updater/MuffMode.Updater](../updater/MuffMode.Updater). It checks GitHub releases, compares the latest release version to the local Quake II rerelease install, downloads the release package zip, syncs the package contents into the selected Quake 2 folder, and launches Quake II.

Release packages include `MuffModeUpdater.exe` at the package root so users can run the updater and launcher after downloading and extracting Muff Mode. GitHub releases may also include a Windows installer asset; the updater deliberately downloads the versioned `muffmode-<version>-<channel>.zip` package rather than the installer executable.

## User Flow

On startup, the updater and launcher:

1. Detects Quake 2 install paths from saved settings, Steam library folders, Epic manifests, GOG registry entries, and common store install folders.
2. Queries `DarkMatter-Productions/MuffMode` GitHub releases.
3. Shows detected Steam, Epic Games Store, GOG, and Xbox app / Microsoft Store locations in a source selector, while keeping an other-location path/browse option available.
4. Shows the latest version, local version, release changelog, and actions.

The user can then:

- **Update**: download the latest release package zip and copy its contents into the Quake 2 install folder.
- **Refresh**: check GitHub again.
- **Launch**: launch Quake II, falling back to `steam://rungameid/2320` if no local executable is found.
- **Shortcuts**: create Desktop and Start menu shortcuts to the updater and launcher.
- **Quit / Cancel**: close the app, or cancel the current network/extraction step while it is safe to do so.

The auto-launch checkbox controls whether Quake II starts after a successful update.
The app uses the Discord avatar image from [assets/img/discord-avatar.png](../assets/img/discord-avatar.png) as its Windows application icon.

## Local Version Marker

The updater reads the installed MuffMode version from:

```text
<Quake 2>\rerelease\baseq2\muffmode-version.json
<Quake 2>\rerelease\baseq2\muffmode.version
```

The release script writes both files into new packages, and the updater rewrites them after every successful update.

## Build

`global.json` pins the .NET SDK to the reviewed 8.0.423 with roll-forward disabled, so every `dotnet` command in this repository resolves to the same SDK that builds the shipped updater. Install that exact SDK before building; `dotnet` reports the requested version and the `global.json` that asked for it when it is missing.

Build a framework-dependent debug copy:

```powershell
dotnet build updater\MuffMode.Updater\MuffMode.Updater.csproj
```

Build a release copy:

```powershell
dotnet build updater\MuffMode.Updater\MuffMode.Updater.csproj -c Release
```

Publish a self-contained single-file Windows x64 updater:

```powershell
dotnet publish updater\MuffMode.Updater\MuffMode.Updater.csproj -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true
```

The updater requests administrator elevation because the default Steam install path is usually under `Program Files`.

## Update Behavior

The updater accepts the outer Quake 2 folder, its `rerelease` folder, or its `rerelease\baseq2` folder, then normalizes the selection back to the correct outer install folder. It validates the downloaded package before touching the install, rejects unsafe archive paths, rejects unexpected executable/script payloads, requires the package version marker to match the selected GitHub release, and then copies package files into the install folder.

Cancel is available during the network download and temporary extraction/validation work. Once the updater starts applying files to the game folder, it finishes that commit step rather than leaving a half-cancelled install. Before replacing `rerelease\baseq2\game_x64.dll`, it stores a timestamped backup under:

```text
<Quake 2>\rerelease\baseq2\MuffModeBackups
```

It does not delete unrelated files from the installation folder, and individual file replacements use temporary files so a failed copy is less likely to leave a truncated destination.

If the release package contains `MuffModeUpdater.exe` and that destination is the updater executable currently running, the updater skips that file instead of trying to overwrite itself.
