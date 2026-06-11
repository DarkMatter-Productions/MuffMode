# MuffMode Build Instructions

This document explains how to build `MuffMode` on Windows using Visual Studio/MSBuild.

## Prerequisites

- Windows 10/11
- Visual Studio 2022 Build Tools (or Visual Studio 2022) with C++ workload installed
- `MSBuild` and the Visual C++ build environment (`vcvarsall.bat`)

## Project Root

Run all commands from:

`MuffMode/`

## Open a Developer Command Prompt

Use one of these:

- **x64 Native Tools Command Prompt for VS 2022**
- **Developer PowerShell for VS 2022**

This ensures `msbuild` and compiler toolchains are configured.

## Build Commands

### Using `build.bat` (recommended)

From `MuffMode/`:

```bat
build.bat
```

Optional args:

```bat
build.bat [Platform] [PlatformToolset]
```

Configuration defaults to `Debug`. Set `MUFFMODE_BUILD_CONFIG=Release` for a release build.

Examples:

```bat
build.bat x64 v143
build.bat x64 v145
```

### Release build

```bat
msbuild src\MuffMode.sln /p:Configuration=Release /p:Platform=x64
```

### Debug build

```bat
msbuild src\MuffMode.sln /p:Configuration=Debug /p:Platform=x64
```

## Output

The build produces `game_x64.dll`.
After building, copy the DLL into your Quake 2 rerelease `muffmode` game folder to test in-game.

## Common Issues

- **`msbuild` is not recognized**
  Open a Visual Studio Developer Command Prompt/PowerShell, then run the command again.
- **Build fails with missing C++ tools**
  Install Visual Studio C++ build components and Windows SDK.
- **MSB8020: build tools for v145 cannot be found**
  You have VS 2022 (toolset `v143`). Run `build.bat` as-is, or pass `build.bat x64 v143`. Install VS 2026+ for `v145`.
- **DLL copy/test issues**
  Verify your Quake 2 install path and folder permissions.
