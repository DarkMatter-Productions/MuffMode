# MuffMode Server Host Guide

[README](../README.md) | [Player Guide](player-guide.md) | [Gameplay Reference](gameplay-reference.md) | [Rulesets](rulesets.md) | [Configuration Reference](configuration-reference.md) | [Build Guide](build-guide.md)

This guide is for lobby owners, dedicated server hosts, event organizers, and admins who want a practical setup path before diving into the full cvar reference. It is written around real server use: casual public play, private friend lobbies, pickups, scrims, and competitive events.

## Install

1. Download the [latest Muff Mode release](https://github.com/DarkMatter-Productions/MuffMode/releases/latest).
2. Use the Windows installer when available. It detects Steam, Epic Games Store, GOG, and Xbox app / Microsoft Store installs, while still offering an Other location choice for custom library folders.
3. If you use the zip instead, extract it into the outer `Quake 2` install folder and allow file replacements.
4. Start the game or dedicated server normally.
5. Execute the bundled baseline config with `exec server-base.cfg`, then execute a gametype preset such as `exec gt-FFA.cfg`.

The release package installs `server-base.cfg`, `CONFIGS_README.md`, and per-gametype `gt-*.cfg` presets into `rerelease/baseq2`. For additional community-maintained examples, see the [MuffMode Server Configs repository](https://github.com/ozy24/muffmode-server-configs).

## Config Startup Flow

Use the packaged configs as layers:

| Step | Command | Purpose |
| --- | --- | --- |
| 1 | `exec server-base.cfg` | Loads shared safety, voting, entity override, and player-limit defaults. |
| 2 | `exec gt-FFA.cfg` or another `gt-*.cfg` | Applies the mode-specific hostname, limits, map list, ruleset, and gameplay toggles. |
| 3 | `doctor` | Checks for risky or inconsistent cvar combinations after your changes. |

When `g_gametype_cfg` is enabled, later gametype changes automatically execute the matching `gt-[GAMETYPE].cfg`.

## Choose A Server Style

| Server style | Prioritize |
| --- | --- |
| Casual public server | Easy joining, clear map rotation, player voting, relaxed ready-up rules, and a short MOTD explaining the server style. |
| Competitive pickup or scrim | Ready-up, locked match flow, controlled voting, known gametypes, known rulesets, timeouts, and admin/captain tools. |
| Private lobby | Fast setup, flexible voting, a small map list, and enough admin control to recover quickly from bad settings. |
| Testing or development server | Debug logging, `doctor`, map/entity overrides, and focused gametype configs. |

## Minimal Server Config

This is a small public-server starting point if you are writing your own config instead of starting from `server-base.cfg`. Adjust the map list, player limits, hostname, voting, and ruleset for your community.

```text
deathmatch 1
hostname "MuffMode Server"
maxclients 16
maxplayers 12

g_gametype 1
g_ruleset 1
g_gametype_cfg 1
g_map_list "q2dm1 q2dm3 q2dm5"
g_map_list_shuffle 1

g_allow_voting 1
g_allow_vote_midgame 0
g_allow_spec_vote 0
g_votable_gametypes "ffa duel tdm ctf ca ft strike rr lms horde instagib nadefest"
g_votable_rulesets "q2re mm q2reb"

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
g_votable_gametypes "duel tdm ca ctf ft"
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
| `7` | `strike` | Capture Strike |
| `8` | `rr` | Red Rover |
| `9` | `lms` | Last Man Standing |
| `10` | `horde` | Horde Mode |
| `12` | `instagib` | Instagib |
| `13` | `nadefest` | NadeFest |
| `14` | `arena` | Arena Rooms (Rocket Arena) |

Value `11` (`ball`) is reserved/removed in the current build. See [Gametypes](gameplay-reference.md#gametypes) for descriptions and work-in-progress notes.

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
| `g_auto_ghost_time` | `120` | Seconds an auto-ghost reservation remains available, up to `3600`; `0` disables auto-ghost capture. |
| `g_auto_ghost_max` | `3` | Maximum active auto-ghost reservations, capped by client capacity; `0` disables auto-ghost capture. |
| `g_auto_ghost_timeout` | `0` | Auto-pauses an active match for disconnected players, in seconds capped by `g_auto_ghost_time`; `0` disables. |
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

## Arena Rooms (Rocket Arena)

Arena has two supported map profiles. **Tagged multi-room maps are the native
MuffMode path:** `worldspawn` declares from 1 through 31 playable rooms with
an explicit `arena` value, arena 0 is the lobby, and each positive room needs
at least two tagged deathmatch starts so both active sides can spawn safely.
Tagged spawns, teleporters, destinations,
and triggers remain local to their room. A tagged `info_player_intermission`
can supply a room name and fallback observer view, but is not required for a
room to activate. Classic RA2 `misc_teleporter_dest` observer positions take
precedence. Negative legacy tags remain shared/reserved markers.

**Classic RA2 idmaps use the one-room compatibility profile.** An explicit
`worldspawn` `arena 0` selects it without a cvar; a map with no `arena` key is
accepted only when the latched `g_arena_legacy_idmap` is set to `1` before the
next map load. It uses one shared deathmatch-start pool and treats every
per-entity `arena` tag as shared within its one virtual room. This opt-in keeps
otherwise untagged idmaps intentional. If either profile does not validate,
Arena stays inactive and none of its spawn, loadout, combat,
isolation, HUD, command, or menu hooks run; the selected mode behaves as FFA.
A disagreement discovered only after entities have spawned rejects the map
instead of leaving a partially activated Arena session behind.

Every room has an independent state machine, teams, queue, settings, ballot,
ready state, round clock, pause, timeout allowance, HUD, and scoreboard. The
four types are `rocket`, `clan`, `rover`, and `practice`. Rocket uses a
winner-stays challenge queue; Clan Arena uses fixed red/blue elimination; Red
Rover transfers defeated players; Practice makes player attacks non-lethal
while retaining knockback and damage progress, keeps unlimited ammunition, and
immediately respawns environmental deaths.

MuffMode intentionally supports RA2 map compatibility and familiar RA3 play
terms, but their original game DLLs, protocols, menus, administration models,
and exact quirks are not replication targets. MuffMode's Q2RE-native teams,
menus, voting, HUD, administration, and match policies remain authoritative.

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_arena_config` | `arena.cfg` | Global/map/room configuration file under `baseq2`. |
| `g_arena_legacy_idmap` | `0` | Latched opt-in for an otherwise untagged classic RA2 idmap. Explicit `worldspawn` `arena 0` needs no opt-in; the one-room profile treats all entity `arena` tags as shared. |
| `g_arena_default_type` | `rocket` | Type inherited when a more specific layer does not set one. |
| `g_arena_players_per_team` | `1` | Default team size. |
| `g_arena_rounds` | `1` | Default odd best-of length. |
| `g_arena_start_health` | `200` (`100` in `gt-ARENA.cfg`) | Shared arena-loadout health; the shipped Arena preset selects the classic value. |
| `g_arena_start_armor` | `200` (`100` in `gt-ARENA.cfg`) | Shared arena-loadout armor; the preset selects the classic 100-armor value. |
| `g_arena_health_protect` | `1` | Protect health from self and team damage. |
| `g_arena_armor_protect` | `2` | Permit self-armor damage, protect against team armor damage. |
| `g_arena_falling_damage` | `1` | Enables falling damage during MuffMode Arena rounds. |
| `g_arena_weapon_mask` | `255` | Spawn-weapon bitmask; `255` grants every standard weapon except the BFG. |
| `g_arena_grapple` | `0` | Grants the room's selectable Grapple and enables the offhand hook command. |
| `g_arena_competition` | `0` | Enables per-room competition readiness and timeouts. |
| `g_arena_unbalanced` | `0` | Allows unequal sides. |
| `g_arena_lock` / `g_arena_max_players` | `0` / `0` | Default entry lock and player cap (`0` means no explicit cap). |
| `g_arena_timeouts` | `3` | Competition timeouts per side. Duration and time-in countdown reuse `g_dm_timeout_length` and `g_dm_timeout_resume_countdown`; `gt-ARENA.cfg` selects `60` / `5`. |

Load `server-base.cfg` and then `gt-ARENA.cfg` for the shipped MuffMode
baseline on an Arena-compatible rotation. Arena map assets are not
redistributed: install maps you are licensed to host. Tagged multi-room maps
are preferred; the preset leaves `g_arena_legacy_idmap` at `0` so ordinary
untagged maps fail closed. Set it to `1` only for a known rotation of untagged
classic RA2 idmaps. The preset uses the MuffMode ruleset, preserves the familiar
100-health / 100-armor protection model, uses the non-BFG weapon set, and
leaves individual rooms free to override those values. Add `arena` to
`g_votable_gametypes` only after that rotation is configured.

For durable per-room rules, put top-level defaults in `baseq2/arena.cfg`, then
nest map and numeric room blocks:

```text
type rocket

map mm_arena_hub {
    room 3 {
        type clan
        playersperteam 4
        rounds 5
        competitionmode 1
    }
}
```

Built-in defaults resolve first, followed by global cvars/top-level file
values, the matching map block, and finally the matching room block. Both
`//` and `#` comments are accepted. `arena <id>` remains an alias for
`room <id>`; bare RA2 blocks remain accepted for imports. See the
[configuration reference](configuration-reference.md#arenacfg-layers) for
every setting and alias.

Players can use room selectors or `arena go <id>`. Existing MuffMode commands
such as `team`, `captain`, `lockteam`, `readyteam`, `vote`, `time-out`, and
`time-in` automatically target the current room. Original RA3 aliases including
`teamlock`, `teamcaptain`, `teamkick`, `specinvite`, `timeout`, and `timein`
are retained as compatibility conveniences. If Q2RE owns a token locally, notably
`timeout`, use `arena timeout` or MuffMode's `time-out` spelling. Player
readiness and captain, naming, kick, mute, spectator-invite, and timeout
commands are competition-mode controls. Team locking follows the same policy
but permits an explicit administrator override outside competition mode. The
`arena` dispatcher owns room selection plus room queue and settings operations;
`arena line` is the retained historical queue-toggle spelling and `arena queue`
inspects the queue.
Administrators can target their selected room with `startmatch`, `endmatch`,
and `resetmatch`, or an explicit room with
`arena admin <arena> <setting|reset|start|abort> [value]`.
`say_arena`, `arena say_team`, and `say_world` provide explicit room,
logical-team, and map-wide channels. Q2RE/KEX consumes bare `say` and
`say_team` before the game DLL can scope them, so bare `say` remains map-wide
and bare `say_team` follows the projected engine red/blue side. Bind
`say_arena` for room chat and use `arena say_team` for reliable
room-local logical-team chat.

## Freeze Tag Controls

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_freezetag_arena_loadout` | `0` | Preserves map/item-control loadouts by default; set `1` to give spawned and thawed players the arena-style Freeze Tag kit. |
| `g_freezetag_thaw_time` | `3` | Seconds a live teammate must stay near a frozen player to thaw them. |
| `g_freezetag_multi_thaw_scale` | `0.5` | Extra thaw speed per additional live teammate near the same frozen player; total thaw rate is capped. |
| `g_freezetag_thaw_radius` | `96` | Thaw proximity radius in units. |
| `g_freezetag_auto_thaw_time` | `0` | Optional forced thaw timer in seconds; `0` disables. |
| `g_freezetag_bot_rescue` | `1` | Lets bots route to frozen teammates and hold position for thawing. |
| `g_freezetag_frozen_knockback_scale` | `1.0` | Knockback multiplier for frozen players. |
| `g_freezetag_thaw_respawn_at_location` | `0` | Respawns thawed players at normal player spawn points by default; set `1` to restore them at the safe thaw location. |
| `g_freezetag_round_respawn_all` | `1` | Respawns every round participant for the next round; set `0` to respawn only frozen, dead, or waiting players. |
| `g_freezetag_round_reset_alive_inventory` | `1` | Resets live survivor inventory/loadout when full round respawns are enabled; set `0` to preserve survivor inventory. |

Frozen-player help calls use the regular `g_loc` / `g_loc_items` location system for teammate markers and location text when map `.loc` data or item landmarks are available.

Thaw assist credit is automatic: the finishing rescuer scores for the thaw, and teammates who spend meaningful time helping the same successful thaw score assist credit. The assist threshold scales with `g_freezetag_thaw_time`.

The in-match HUD shows the match limit (`fraglimit`, `capturelimit`, or `roundlimit` as applicable) as a single number below the miniscore, so hosts should tune that cvar as the visible match target for the mode. When `g_freezetag_arena_loadout` is enabled, freezes do not drop the player's starter weapon; timed powerups still follow the normal death-drop cvars.

Players who join a Freeze Tag team during a live round are held as round spectators until the next round begins, preserving the active/frozen balance of the current round.

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
| Arena Rooms (Rocket Arena) | `gt-ARENA.cfg` |
| Freeze Tag | `gt-FT.cfg` |
| Last Man Standing | `gt-LMS.cfg` |
| Capture Strike | `gt-STRIKE.cfg` |
| Red Rover | `gt-REDROVER.cfg` |
| Horde | `gt-HORDE.cfg` |
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
| `setteam <player> [auto\|red\|blue\|spectator]` | Inspect or force a player team change. |
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
