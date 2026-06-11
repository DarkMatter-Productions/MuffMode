# MuffMode Server Configs

This repository contains server configuration files for MuffMode, providing per-gametype configurations and a baseline setup for running a MuffMode server.

## Files Overview

- **`server-base.cfg`** - Core server configuration loaded first
- **`gt-*.cfg`** - Gametype-specific configs (executed when gametype changes)

## How They Work

1. **Base Config**: `server-base.cfg` contains core server settings, safety limits, and default values
2. **Gametype Switching**: When the gametype changes (via vote or admin command), MuffMode automatically executes the corresponding `gt-*.cfg` file
3. **Layered Approach**: Gametype configs override base settings as needed while inheriting common defaults

## Usage

### Quick Start
1. Copy `server-base.cfg` to your Quake 2 Rerelease `baseq2` directory
   (usually: `C:\Program Files (x86)\Steam\steamapps\common\Quake 2\rerelease\baseq2`)
2. Copy any `gt-*.cfg` files you plan to use to the same directory
3. Launch Quake 2 Rerelease
4. Navigate to Multiplayer > Continue > Online > Create Match > Continue - this will bring you to the match configuration screen
5. Bring up the console (`~` key) and type: `exec server-base.cfg`
6. Close the console and select Play
7. When the map loads, execute your desired gametype config: `exec gt-FFA.cfg`
8. From this point, MuffMode will automatically execute the correct gametype config when the gametype changes

### Gametype Configs
Each `gt-*.cfg` file contains:
- Gametype-specific settings (fraglimits, timelimits, etc.)
- Server hostname for that gametype
- Match flow configuration
- Gametype-specific overrides

The files are named according to their gametype:
- `gt-FFA.cfg` - Free For All
- `gt-DUEL.cfg` - Duel
- `gt-TDM.cfg` - Team Deathmatch
- `gt-CTF.cfg` - Capture the Flag
- `gt-CA.cfg` - Clan Arena
- `gt-INSTAGIB.cfg` - Instagib
- `gt-NADEFEST.cfg` - Nade Fest

## Configuration Details

### Base Settings (`server-base.cfg`)
- Inactivity timeout and QoL settings
- Entity override configuration
- Core game mode baseline
- Client management and voting options

### Gametype Execution
MuffMode automatically executes gametype configs when:
- A vote passes to change gametype
- An admin changes the gametype
- The server starts with a specific gametype

### Customization
You can modify these configs to:
- Adjust server limits and rules
- Change server names per gametype
- Enable/disable specific features
- Add custom map rotations

## For Server Admins

These configs serve as a solid foundation for any MuffMode server. You can:
- Use them as-is for a standard setup
- Modify values to match your community's preferences
- Add additional gametype configs as needed
- Extend with custom map lists or additional cvars

## Technical Notes

- Configs use standard Quake 2 syntax (`set cvar "value"`)
- Comments are prefixed with `//`
- Gametype configs are executed after base config
- Settings in gametype configs override base settings
- Some cvars are marked as `CVAR_SERVERINFO` and will be visible to clients

## Understanding MuffMode

For detailed information about how MuffMode works, its features, and advanced configuration options, see the main MuffMode source code and documentation at [https://github.com/themuffinator/muffmode](https://github.com/themuffinator/muffmode)

