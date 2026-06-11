# MuffMode Build Guide

[README](../README.md) | [Server Host Guide](server-host-guide.md) | [Configuration Reference](configuration-reference.md)

This guide explains how to build MuffMode on Windows with Visual Studio/MSBuild.

## Prerequisites

- Windows 10 or Windows 11.
- Visual Studio 2022 or Visual Studio 2022 Build Tools.
- The C++ desktop workload and Windows SDK.
- MSBuild and a configured Visual C++ environment.

The Visual Studio project uses the vcpkg manifest at [src/vcpkg.json](../src/vcpkg.json).

## Project Root

Run commands from the repository root:

```text
MuffMode/
```

The solution file is [src/MuffMode.sln](../src/MuffMode.sln).

## Open A Developer Shell

Use one of these Visual Studio shells:

- `x64 Native Tools Command Prompt for VS 2022`
- `Developer PowerShell for VS 2022`

This ensures `msbuild`, the compiler, and library paths are available.

## Build Commands

Release build:

```bat
msbuild src\MuffMode.sln /p:Configuration=Release /p:Platform=x64
```

Debug build:

```bat
msbuild src\MuffMode.sln /p:Configuration=Debug /p:Platform=x64
```

## Output

The build produces `game_x64.dll` in the repository root. To test locally:

1. Back up your Quake II rerelease `baseq2\game_x64.dll`.
2. Copy the built `game_x64.dll` into the rerelease `baseq2` folder.
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
- Package and publish releases: [Release Process](release-process.md)
