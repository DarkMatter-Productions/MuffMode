# MuffMode Server Host Guide

[README](../README.md) | [Player Guide](player-guide.md) | [Gameplay Reference](gameplay-reference.md) | [Rulesets](rulesets.md) | [Configuration Reference](configuration-reference.md) | [Build Guide](build-guide.md)

This guide is for lobby owners, dedicated server hosts, event organizers, and admins who want a practical setup path before diving into the full cvar reference. It is written around real server use: casual public play, private friend lobbies, pickups, scrims, and competitive events.

## Install

1. Download the [latest Muff Mode release](https://github.com/DarkMatter-Productions/MuffMode/releases/latest).
2. Use the Windows installer when available. It defaults to the Steam Quake II Remastered path and also offers Epic Online Store / Epic Games Store, GOG, and custom library choices.
3. If you use the zip instead, extract it into the outer `Quake 2` install folder and allow file replacements.
4. Start the game or dedicated server normally.
5. Execute the bundled server config with `exec muff-sv.cfg` when it is available in the release package.

For ready-to-use per-gametype config examples, see the [MuffMode Server Configs repository](https://github.com/ozy24/muffmode-server-configs).

## Choose A Server Style

| Server style | Prioritize |
| --- | --- |
| Casual public server | Easy joining, clear map rotation, player voting, relaxed ready-up rules, and a short MOTD explaining the server style. |
| Competitive pickup or scrim | Ready-up, locked match flow, controlled voting, known gametypes, known rulesets, timeouts, and admin/captain tools. |
| Private lobby | Fast setup, flexible voting, a small map list, and enough admin control to recover quickly from bad settings. |
| Testing or development server | Debug logging, `doctor`, map/entity overrides, and focused gametype configs. |

## Minimal Server Config

This is a small public-server starting point. Adjust the map list, player limits, hostname, voting, and ruleset for your community.

```text
deathmatch 1
hostname "MuffMode Server"
maxclients 16
maxplayers 12

g_gametype 1
g_ruleset 1
g_map_list "q2dm1 q2dm3 q2dm5"
g_map_list_shuffle 1

g_allow_voting 1
g_allow_vote_midgame 0
g_allow_spec_vote 0

g_dm_do_warmup 1
g_dm_do_readyup 0
g_warmup_countdown 10

map q2dm1
```

Run `doctor` after changing server settings. It checks common misconfigurations and reports suggested fixes.

## Competitive Match Baseline

For pickups, scrims, and events, start stricter than a casual public server and then loosen only what your group actually wants.

```text
g_dm_do_warmup 1
g_dm_do_readyup 1
g_warmup_ready_percentage 1.0
g_allow_voting 1
g_allow_vote_midgame 0
g_allow_spec_vote 0
g_vote_limit 2
g_votable_gametypes "duel tdm ca ctf"
g_votable_rulesets "q2re mm q2reb"
```

Use `readyall`, `startmatch`, `resetmatch`, `shuffle`, `balance`, `lockteam`, and `unlockteam` to keep match administration quick without giving every player full control.

## Gametypes

Set `g_gametype` by index, or use the admin command `gametype <shortname>`.

| Value | Short name | Gametype |
| --- | --- | --- |
| `1` | `ffa` | Deathmatch |
| `2` | `duel` | Duel |
| `3` | `tdm` | Team Deathmatch |
| `4` | `ctf` | Capture the Flag |
| `5` | `ca` | Clan Arena |
| `6` | `ft` | Freeze Tag |
| `7` | `strike` | CaptureStrike |
| `8` | `rr` | Red Rover |
| `9` | `lms` | Last Man Standing |
| `10` | `horde` | Horde Mode |
| `11` | `ball` | ProBall |
| `12` | `instagib` | Instagib |
| `13` | `nadefest` | NadeFest |

See [Gametypes](gameplay-reference.md#gametypes) for descriptions and work-in-progress notes.

## Rulesets

Set `g_ruleset` by index, or use `ruleset <shortname>`.

| Value | Short name | Ruleset |
| --- | --- | --- |
| `1` | `q2re` | Quake II Rerelease |
| `2` | `mm` | Muff Mode |
| `3` | `q3a` | Quake III Arena style |
| `4` | `q2reb` | Q2RE Balanced |
| `5` | `q` | Quake style |
| `6` | `qc` | Quake Champions style |

See [Rulesets](rulesets.md) for the player-facing gameplay differences.

## Match Flow

Good match flow matters for both audiences: casual players should not get stuck waiting, and competitive players need predictable starts, pauses, and finishes.

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_dm_do_warmup` | `1` | Enables warmup before match start. |
| `g_dm_do_readyup` | `0` | Requires ready status during warmup. |
| `g_warmup_ready_percentage` | `0.51f` | Percentage of ready players needed to start. |
| `g_warmup_countdown` | `10` | Countdown length once match conditions are met. |
| `g_round_countdown` | `10` | Round countdown for round-based gametypes. |
| `g_dm_overtime` | `120` | Overtime session length in seconds, currently for Duels. |
| `g_dm_tie_max_time` | `1800` | Maximum total tied-overtime duration before forced resolution. |
| `g_auto_ghost_timeout` | `0` | Auto-pauses an active match for disconnected players, in seconds up to `120`; `0` disables. |
| `g_dm_timeout_length` | `120` | Timeout length in seconds. Set `0` to disable timeouts. |
| `g_dm_timeout_resume_countdown` | `30` | Countdown announced before a paused match resumes, in seconds up to `120`; `0` resumes immediately. |
| `mercylimit` | `0` | Score gap that ends a match. `0` disables. |

## Team Controls

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_teamplay_allow_team_pick` | `0` | Allows players to pick a specific team. |
| `g_teamplay_auto_balance` | `1` | Rebalances teams during a match. |
| `g_teamplay_force_balance` | `0` | Blocks joining an over-stacked team. |
| `g_teamplay_item_drop_notice` | `1` | Sends team notices for dropped major items. |
| `g_friendly_fire` | `0` | Enables friendly fire. |
| `g_match_lock` | `0` | Prevents players from joining while a match is active. |

Team captains can lock or unlock their own team and can ready their team. Admins can transfer players, shuffle teams, balance teams, and force ready state.

## Voting

Voting is menu-driven and console-driven. For casual servers, keep enough voting open that players can self-serve maps and modes. For competitive servers, restrict voting to the decisions players should actually make during a match session.

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_allow_voting` | `1` | Global voting switch. |
| `g_allow_vote_midgame` | `0` | Allows votes during active matches. |
| `g_allow_spec_vote` | `0` | Allows spectators to vote. |
| `g_vote_limit` | `3` | Maximum votes per client per match. `0` disables the limit. |
| `g_vote_flags` | `0` | Bitmask disabling specific vote commands. |
| `g_votable_gametypes` | empty | Space-separated gametype short names allowed in votes. Empty allows all implemented options. |
| `g_votable_rulesets` | empty | Space-separated ruleset short names allowed in votes. Empty allows all options. |

See [Vote Commands](configuration-reference.md#vote-commands) for command names and [Vote Flags](configuration-reference.md#vote-flags) for the bitmask.

## Per-Gametype Configs

When `g_gametype_cfg` is `1`, MuffMode automatically executes `gt-[GAMETYPE].cfg` when the gametype changes. This lets hosts keep different map lists, limits, rulesets, and item rules for each mode.

Examples:

| Gametype | Config filename |
| --- | --- |
| Free for All | `gt-FFA.cfg` |
| Duel | `gt-DUEL.cfg` |
| Team Deathmatch | `gt-TDM.cfg` |
| Capture the Flag | `gt-CTF.cfg` |
| Clan Arena | `gt-CA.cfg` |
| Freeze Tag | `gt-FT.cfg` |
| CaptureStrike | `gt-STRIKE.cfg` |
| Red Rover | `gt-REDROVER.cfg` |
| Last Man Standing | `gt-LMS.cfg` |
| Horde | `gt-HORDE.cfg` |
| ProBall | `gt-BALL.cfg` |
| Instagib | `gt-INSTAGIB.cfg` |
| NadeFest | `gt-NADEFEST.cfg` |

Place these files in the active game directory. The system executes them only when the gametype actually changes.

## Admin Commands

| Command | Purpose |
| --- | --- |
| `startmatch` | Force warmup to progress into match start. |
| `endmatch` | End an active match. |
| `resetmatch` | Reset the current match to warmup. |
| `map_restart` | Restart the current level and session, applying latched cvar changes. |
| `gametype <name>` | Change gametype and reset the level. |
| `ruleset <name>` | Change gameplay ruleset. |
| `setmap <map>` | Change to a map in the map list. |
| `nextmap` | Move to the next map. |
| `shuffle` | Shuffle and balance teams, then reset the match. |
| `balance` | Balance teams without a full shuffle. |
| `setteam <player>` | Force a player team change. |
| `lockteam <red|blue>` | Lock a team from joins. |
| `unlockteam <red|blue>` | Unlock a team. |
| `readyall` | Mark all players ready during ready-up warmup. |
| `unreadyall` | Clear ready status during ready-up warmup. |
| `doctor` | Print diagnostics for server cvar combinations. |

The [Configuration Reference](configuration-reference.md#admin-commands) includes additional admin and vote details.

## Debug Logging

Enable debug logging with:

```text
g_muffmode_debug 1
```

Logs are written to `muffmode_debug.log` in the game directory. The default is `0`. Turn it on while diagnosing a server issue, then turn it back off for normal public or match play. When enabled, the log captures match state transitions, player joins and team changes, vote progress, gametype and ruleset changes, entity spawning, map loading, errors, and other diagnostics.

## Host Checklist

- Set `hostname`, `maxclients`, `maxplayers`, and passwords before going public.
- Add a short MOTD so casual players know what kind of server they joined.
- Set `g_map_list` and optionally `g_map_pool`.
- Decide whether players can vote during matches with `g_allow_vote_midgame`.
- Restrict votable modes with `g_votable_gametypes` and `g_votable_rulesets` if your server has a focused identity.
- Run `doctor` after config changes.
- Turn on `g_muffmode_debug 1` only while investigating issues, then turn it off again for normal operation.
