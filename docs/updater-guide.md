# MuffMode Updater Guide

The MuffMode updater is a Windows desktop app in [updater/MuffMode.Updater](../updater/MuffMode.Updater). It checks GitHub releases, compares the latest release version to the local Quake II rerelease install, downloads the release zip, and syncs the package contents into the selected Quake 2 folder.

Release packages include `MuffModeUpdater.exe` at the package root so users can run the updater after downloading and extracting MuffMode.

## User Flow

On startup, the updater:

1. Detects the Quake 2 install path from saved settings or Steam library folders.
2. Queries `DarkMatter-Productions/MuffMode` GitHub releases.
3. Shows the latest version, local version, release changelog, and actions.

The user can then:

- **Update**: download the latest release zip and copy its contents into the Quake 2 install folder.
- **Refresh**: check GitHub again.
- **Launch**: launch Quake II, falling back to `steam://rungameid/2320` if no local executable is found.
- **Quit**: close the updater.

The auto-launch checkbox controls whether Quake II starts after a successful update.

## Local Version Marker

The updater reads the installed MuffMode version from:

```text
<Quake 2>\rerelease\baseq2\muffmode-version.json
<Quake 2>\rerelease\baseq2\muffmode.version
```

The release script writes both files into new packages, and the updater rewrites them after every successful update.

## Build

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

The updater accepts either the outer Quake 2 folder or its `rerelease` folder, then copies all files from the release package into the correct outer install folder and overwrites existing files. Before replacing `rerelease\baseq2\game_x64.dll`, it stores a timestamped backup under:

```text
<Quake 2>\rerelease\baseq2\MuffModeBackups
```

It does not delete unrelated files from the installation folder.

If the release package contains `MuffModeUpdater.exe` and that destination is the updater executable currently running, the updater skips that file instead of trying to overwrite itself.
