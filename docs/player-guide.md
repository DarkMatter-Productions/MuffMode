# MuffMode Player Guide

[README](../README.md) | [Server Host Guide](server-host-guide.md) | [Gameplay Reference](gameplay-reference.md) | [Rulesets](rulesets.md) | [Configuration Reference](configuration-reference.md)

This guide is for anyone joining a MuffMode game. You do not need to learn every console command to have a good time: most everyday actions are available through the game menu, and the commands below are here when you want faster control.

## Quick Orientation

| If you are... | Start with |
| --- | --- |
| New or playing casually | Join through the menu, use `team auto`, read `motd`, and call votes from the voting menu. |
| Playing a match or pickup | Use `readyup`, know `time-out`, check the active ruleset, and use team/captain commands when needed. |
| Spectating or casting | Use `follow`, `followview`, `followleader`, `followkiller`, and `followpowerup` to keep the action easy to watch. |

## Install

1. Download the [latest Muff Mode release](https://github.com/DarkMatter-Productions/MuffMode/releases/latest).
2. Use the Windows installer when available. It detects Steam, Epic Games Store, GOG, and Xbox app / Microsoft Store installs, while still offering an Other location choice for custom library folders.
3. If you use the zip instead, extract it into the outer `Quake 2` folder and allow file replacements.
4. Launch Quake II normally, then join or host a multiplayer game.

If you are joining someone else's server, they control most match settings. Your client-side commands still let you adjust your display preferences, ready state, team, votes, and spectator behavior.

Display, audio, skin override, follow view, and spectator auto-follow preferences are saved per server by your social ID. Open **Player Config** from the game menu for everyday changes, or use the commands below when you want binds, explicit values, or a custom skin path.

## First Match

- Use forward/back movement or item-selection keys to navigate the game menu, then attack or use-item to select an entry. The main menu also shows this reminder.
- Use the game menu to join a match, change teams, view server and match information, open voting options, or adjust saved player settings.
- Use `team auto` for the quickest team join on team servers.
- Use `ready`, `notready`, or `readyup` when the server uses competitive-style warmups.
- Use `motd` to read server rules, notes, or event information.
- Use `maplist` to see maps available in the server rotation.
- If you disconnect during an active match and return quickly with the same account, Muff Mode usually restores you automatically. If the server gives you a fallback ghost code, rejoin as spectator and use `ghost <code>`.

## Team Commands

| Command | What it does |
| --- | --- |
| `team auto` or `team a` | Join the best available team automatically. |
| `team red` or `team r` | Join red team when team picking is allowed. |
| `team blue` or `team b` | Join blue team when team picking is allowed. |
| `team free` or `team f` | Join free-for-all play. |
| `team spectator` or `team s` | Move to spectators. |
| `captain` | Claim captain if vacant, or show the current captain. |
| `captain <player>` | Transfer captain status to a teammate. |
| `readyteam` | Ready your whole team when you are captain or admin. |
| `loc <message with macros>` | Call out info to teammates (everyone in FFA). Your message must contain at least one macro token. `%l` is your nearest location in brackets, e.g. `bind x "cmd loc at %l"` or `bind c "cmd loc enemy at %l, %h %a"`. Status macros: `%h` health, `%a` armor, `%w` weapon+ammo, `%m` current ammo, `%n` nearby teammates, `%N` nearby players. |

## Display Commands

The **Player Config** menu separates **Display & Audio**, **Spectator & Follow**, and **Skin Overrides** into short pages. It covers crosshair ID, match timer, match-info HUD, frag messages, the optional voice announcer, kill beep, follow view, spectator auto-follow toggles, and quick skin override presets. Changes save automatically. Custom `model/skin` override paths still use `eskin` and `tskin`; a command-entered custom skin appears as `custom` in the menu and can be cycled back to normal or a stock preset there.

| Command | What it does |
| --- | --- |
| `announcer [on|off]` | Toggle Quake Live style voice announcements. The voice pack is off by default; stock fallback cues still play where defined. |
| `eskin <model/skin>` or `eskin off` | In team games and Arena Rooms, re-skin enemies on your screen only; in duel, re-skin your opponent (e.g. `eskin male/grunt`). Other rooms and nonfighters are unaffected. No argument shows the current setting. |
| `fm [on|off]` | Toggle frag messages. |
| `help` | Toggle help text drawing. |
| `id [on|off]` | Toggle crosshair player identification. |
| `infohud [on|off]` | Toggle the top-right match-info HUD and save the preference. |
| `kb [0-4]` | Cycle kill beeps, or set one directly. `0`/`off` disables it. Named values are `clang`, `beep-boop`, `insane`, and `tang-tang`. |
| `timer [on|off]` | Toggle the match timer. |
| `tskin <model/skin>` or `tskin off` | In team games and Arena Rooms, re-skin teammates on your screen only (not available in duel). No argument shows the current setting. |

## Gameplay Commands

Arena is MuffMode's room-based gametype. On a tagged multi-room map, start in
arena 0 (the lobby), use a room-selector teleporter or `arena go <id>` to
choose a room, and use `arena list` to see what each room is running. Classic
RA2 idmaps are also supported as a one-room compatibility profile when the
host enables it or the map explicitly declares that profile. Ordinary Quake II
maps never become synthetic Arena rooms.

Each room can independently run Rocket Arena, Clan Arena, Red Rover, or
Practice. Rocket Arena uses the familiar winner-stays queue; Clan Arena is a
red/blue elimination match; Red Rover moves defeated players to the other
side; Practice keeps hits and knockback non-lethal. A room's teams, ready state,
vote, timeout, score, and settings do not affect any other room.
Practice is teamless, grants unlimited ammunition, scores damage dealt, and
immediately respawns environmental deaths.
The normal Multiplayer menu remains the hub. Use **Browse Rooms** or
**Change Room** for the room browser, then **Teams & Queue** for the focused
team and queue page. Both submenus paginate when needed and return directly to
the uncluttered Multiplayer menu.

The usual MuffMode commands remain the shortest route: `team auto/red/blue`,
`captain`, `lockteam`, `unlockteam`, `ready`, `readyteam`, `vote yes|no`,
`time-out`, and `time-in` automatically operate on your current room.
RA3-era convenience spellings are also accepted as compatibility input:
`teamlock`, `teamunlock`,
`teamcaptain`, `teamname`, `teamkick`, `teammute`, `teamunmute`,
`specinvite`, `specrevoke`, `specwho`, `timeout`, and `timein`. Q2RE treats
`timeout` as a local client setting, so use `arena timeout` or MuffMode's
`time-out` command instead. These aliases do not replace MuffMode's native
room, queue, team, and match controls.

Use `say_arena <message>` (or `arena say <message>`) for room-local chat.
Q2RE/KEX handles bare `say` before the game DLL can route it, so bare `say`
remains map-wide. Use `arena say_team <message>` for a reliably room-local
logical-team channel; KEX also owns bare `say_team`, which follows the engine's
projected red/blue team rather than MuffMode's logical room team. `say_world`
explicitly reaches the whole map. In the lobby, `say_arena` becomes world
chat. The portable logical-team channel is exempt from chat flood limiting
only in a Clan Arena room with competition mode enabled. When a room enables
the grapple option, `use Grapple` selects it and
`+hook` provides MuffMode's offhand binding.

In Freeze Tag, dying during a live round freezes you in place instead of sending you to a respawn screen. While frozen, your body stays on the field with a white shell and you view it from a third-person camera that can look around. Live teammates thaw you by standing nearby, and extra teammates near the same frozen player speed up the thaw. The primary rescuer scores for the thaw, while teammates who spend meaningful time helping a successful thaw receive assist credit. Press attack while frozen to send a throttled team help call with a teammate marker and location when location data is available. The HUD shows frozen/thawing state and a clean round display in the top-right. Frozen bodies can be nudged by damage and pulled with the grapple; thawed players normally respawn at player spawn points with white-shell gibs thrown at the thaw spot, though servers can opt into restoring them at the safe thaw location, and thawed players use normal map/item-control loadouts unless the server enables the optional arena kit. Players who join a team during a live round wait until the next round starts.

| Command | What it does |
| --- | --- |
| `arena` | Show current room status, or list rooms while in the lobby. |
| `arena list` | List the discovered rooms, names, modes, populations, and states. |
| `arena go <id>` | Enter or observe a room. |
| `arena leave` | Return to the lobby. |
| `arena status` / `arena settings` | Show current state or effective rules. |
| `arena line [on\|off]` / `arena queue` | Join or leave a Rocket Arena-type room's queue with `line`; inspect it with `queue`. `line` is the retained historical spelling. |
| `arena create [name]` | Create a logical team. |
| `arena join <team-id\|player\|red\|blue> [password]` | Join a logical or fixed team. |
| `arena teamleave` | Leave your current team but remain in the room. |
| `arena ready [0\|1]` | In competition mode, toggle ready state or set it explicitly. |
| `arena propose <key> <value>` / `arena vote <yes\|no>` | Start or answer a room-local settings vote. |
| `arena timeout` / `arena timein` | Use a competition timeout or end the timeout called by your active side. |
| `arena lock [password]` / `arena unlock` | In competition mode, let the captain control entry to the team. Passwords are a MuffMode extension. |
| `arena name <name>` | In competition mode, rename your team when you are its captain. |
| `arena captain [player]` / `arena kick [player]` | In competition mode, inspect/transfer captaincy or list/remove team members. |
| `arena teammute` / `arena teamunmute` | In competition mode, restrict noncaptains to team chat or restore their room/world chat. |
| `arena invite <player>` / `arena revoke <player>` | In competition mode, manage access to a locked team; this is a MuffMode extension. |
| `arena specinvite <player> [coach]` / `arena specrevoke <player>` | In competition mode, let any non-coach team member manage same-room private spectator access. Coaching is a MuffMode extension. |
| `arena coach <team\|player>` / `arena specwho` | In competition mode, accept a coach invitation or list team spectators/coaches. |
| `arena say <message>` / `arena say_team <message>` / `arena say_world <message>` | Use the room, team, or world chat channel through the dispatcher. |
| `say_arena <message>` / `say_world <message>` | Direct portable room-local or map-wide chat; use `arena say_team` for portable logical-team chat. |
| `ready [0\|1]`, `notready`, `readyup` | Set or toggle ready status; room readiness is competition-only. |
| `callvote <command> <arg>` or `cv <command> <arg>` | Start a vote. |
| `vote yes` or `vote no` | Vote on an active proposal. |
| `mymap <map>` | Add a valid map to the MyMap queue. |
| `maplist` | Print the current server map list. |
| `motd` | Print the message of the day. |
| `forfeit` | Forfeit a duel when `g_allow_forfeit` is enabled. |
| `ghost <code>` | Restore a saved in-progress match slot when automatic reconnect recovery is not available. |
| `time-out` | Call a timeout when the server allows timeouts. |
| `time-in` | End an active timeout early. |
| `follow <player>` | Spectate a specific player. |
| `follow next`, `follow prev`, `follow stop` | Cycle targets or return to free spectator view. |
| `follownext` / `followprev` | Move through follow targets directly; jump/crouch and inventory next/previous do the same while following. |
| `followview [first|third]` or `follow view` | Toggle or set first-person versus third-person following. While following, press `+use` to toggle this quickly. |
| `followkiller [on|off]` | Toggle auto-following killers while spectating. |
| `followleader [on|off]` | Toggle auto-following the leading player while spectating. |
| `followpowerup [on|off]` | Toggle auto-following players who pick up powerups. |

## Voting

Use `callvote <command> <argument>` from the console, or use the voting menu when the server exposes it. Common examples:

```text
callvote map q2dm1
callvote gametype duel
callvote ruleset mm
callvote timelimit 15
callvote scorelimit 50
```

Servers can restrict voting, spectator voting, mid-match voting, available gametypes, and available rulesets. If a vote option is missing, it is probably a host choice rather than a client problem. Use the [Rulesets](rulesets.md) guide when you want to know what a ruleset vote changes, and see [Vote Commands](configuration-reference.md#vote-commands) for the full command list.

## Offhand Hook

The offhand hook works only when the server enables `g_grapple_offhand 1`. A common button-style bind is:

```text
alias +hook hook
alias -hook unhook
bind mouse2 +hook
```

Use `hook` and `unhook` directly if you prefer separate commands.

## Helpful Notes

- Some gametypes are works in progress. The [Gameplay Reference](gameplay-reference.md) calls these out.
- Rulesets change starts, weapons, ammo, armor, health, powerups, and movement feel. The [Rulesets](rulesets.md) guide has the player-facing differences.
- Custom skins, voting, team picking, timeouts, MyMap, and ready-up behavior are server controlled.
- During active matches, servers can restore your match state automatically if you reconnect quickly with the same social ID. The older `ghost <code>` path remains as a fallback.
- If something feels misconfigured, ask the host to run `doctor`; it reports risky cvar combinations and suggested fixes.
- The `hand` cvar sets weapon handedness: `0` right, `1` left, `2` center (fires from screen center, weapon model hidden). Muff Mode adds `hand 3` — centered fire like `hand 2` but with the weapon model still visible. Set it from the console (`hand 3`); the in-game menu only exposes 0–2.
