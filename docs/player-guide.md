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

Display, audio, skin override, follow view, and spectator auto-follow preferences are saved per server by your social ID. Open **Player Config** from the game menu for everyday changes, or use the commands below when you want binds, explicit values, or a custom skin path. Rapid changes are combined automatically, and each changed setting is merged independently so saving one preference does not revert another.

## First Match

- Use forward/back movement or item-selection keys to navigate the game menu, then attack or use-item to select an entry. The main menu also shows this reminder.
- Use the game menu to join a match, change teams, view server and match information, open voting options, or adjust saved player settings.
- Use `team auto` for the quickest team join on team servers.
- Use `ready`, `notready`, or `readyup` when the server uses competitive-style warmups.
- Use `motd` to read server rules, notes, or event information.
- Use `maplist` to see maps available in the server rotation.
- If you disconnect during an active match and return quickly with the same account, Muff Mode usually restores you automatically. The saved match snapshot finishes that match, while your current profile and any reconnect-time preference changes carry forward into the next one. If the server gives you a fallback ghost code, rejoin as spectator and use `ghost <code>`.

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
| `awards` | Reprint the last match's post-match awards reel to the console. The reel itself is gone once the level changes, so this is the only way to read it back. |
| `eskin <model/skin>` or `eskin off` | In team games and Arena Rooms, re-skin enemies on your screen only; in duel, re-skin your opponent (e.g. `eskin male/grunt`). Other rooms and nonfighters are unaffected. No argument shows the current setting. |
| `fm [on|off]` | Toggle frag messages. |
| `help` | Toggle help text drawing. |
| `id [on|off]` | Toggle crosshair player identification. |
| `infohud [on|off]` | Toggle the top-right match-info HUD and save the preference. |
| `kb [0-4]` | Cycle kill beeps, or set one directly. `0`/`off` disables it. Named values are `clang`, `beep-boop`, `insane`, and `tang-tang`. |
| `setweaponpref [weapon ...]` | Replace your saved preferred weapon order. Use compact names such as `RL`, `RG`, and `SSG` or friendly full names; run it with no weapons to clear the custom order. The order guides no-ammo fallback and automatic switching to a newly picked-up weapon, with unlisted weapons retaining the default order. |
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

Entering a room and creating or joining a team drops you into that room's own
warmup, the same shape as MuffMode's warmup elsewhere: the room tells you what it
is waiting on, `ready` marks you up, and once enough players are ready the room
counts down with the announcer before FIGHT. Every room warms up independently,
so one room counting down never disturbs another. The HUD shows `WARMUP - NEED
PLAYERS`, `WARMUP - UNBALANCED`, or `WARMUP (n/m READY)`, and the scoreboard
repeats the reason and marks who is ready. Servers that prefer the old behavior
of starting the moment two sides pair up can turn ready-up off.

Bots choose a room for themselves. They head for a room that already has players
in it, prefer one where somebody is waiting for an opponent, and take an opposing
team, founding one when a lone player has nobody to fight.
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
| `arena ready [0\|1]` | Toggle ready state or set it explicitly during room warmup. Available whenever the server enables Arena ready-up or the room is in competition mode. |
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
| `mymap <map> [modifier ...]` | Add a valid map to the MyMap queue, optionally with one-shot item rules for that entry. |
| `maplist` | Summarize the active structured pool/cycle and any legacy map-list fallback. |
| `mappool [filter]` | List the first 32 matching maps in the structured voting/MyMap catalog, optionally filtered by name, title, episode, mode, `popular`, or `custom`. |
| `mapcycle [filter]` | List the first 32 matches in the active structured automatic-rotation cycle with the same optional filters. |
| `mappick [1-3]` | Choose the next map during the post-scoreboard pick, or list the current candidates and their tallies when given no number. |
| `motd` | Print the message of the day. |
| `forfeit` | Forfeit a duel when `g_allow_forfeit` is enabled. |
| `sr` | Show your current singleton gametype's skill rating, its latest change, and the average rating of profile-ready active human players. Arena Rooms, non-Duel matches with a departure, matches containing a player whose profile could not be loaded, and matches whose complete rating result cannot be admitted to the bounded persistence queue are unranked. Your rating is also shown on the centerprint you receive when you join the match, whenever the session is ranked. |
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

When the server runs the next-map pick (`g_map_pick`), the end-of-match
scoreboard is followed by a short menu offering up to three maps. Move the
cursor with forward/back and choose with attack or jump, or type
`mappick <number>`. Vote counts appear beside each map, your own choice is
highlighted, and you can change it until the timer runs out. A map that holds
more than half of the eligible voters wins immediately. Spectators take part
whenever `g_allow_spec_vote` is enabled.

A ranked match ends with three screens rather than one: the scoreboard, then the
awards reel, then the next-map pick. Each moves on when somebody presses a key,
or on its own after a few seconds if nobody does.

The awards reel replaces the scoreboard with up to a dozen titles, each in green
with the player who earned it underneath. It stays up for `g_match_awards`
seconds unattended; any key moves it along, though not for the first three
seconds, so the press that dismissed the scoreboard cannot skip it unseen.
Anything you won is repeated in your end-of-match summary, and `awards` reprints
the last reel at any time.

MyMap modifiers are `pu` (powerups), `pa` (power armor), `ht` (health),
`ar` (armor), `am` (ammo), and `wp` (weapons). Prefix a code with `+` to
force that category on for the queued map or `-` to remove it, for example
`mymap q2dm1 +pu -wp`. Each queued map retains its own modifiers; a later
entry cannot change the item rules attached to an earlier one.

`motd` and `mymap` use the server's shared flood controls. Queue listings are
split into bounded messages, and the full 32-entry queue fits the capped
four-message response to an explicit request. A successful addition broadcasts
only that new bounded entry. An unusually large MOTD is shown as a bounded prefix:
the explicit command uses at most eight chunks (about 7 KiB total), while the
automatic join-time preview is limited to one chunk. Both add a truncation
notice without splitting a UTF-8 character.

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
- Completed matches update your saved per-gametype rating and aggregate record when the server can identify your account and can guarantee admission of every required result. Match-end summaries show the result and rating change; matches containing bots or profile-unready players, non-Duel matches with a departure, and result sets the bounded persistence queue cannot accept remain unranked and do not apply Elo. A valid two-player Duel departure instead settles both players atomically as a forfeit and closes the match.
- During active matches, servers can restore your match state automatically if you reconnect quickly with the same social ID. That reserved state remains the authority for finishing the interrupted match, then the admitted result is reconciled with your current profile before the next match; reconnect-time preferences are preserved. The older `ghost <code>` path remains as a fallback.
- If something feels misconfigured, ask the host to run `doctor`; it reports risky cvar combinations and suggested fixes.
- The `hand` cvar sets weapon handedness: `0` right, `1` left, `2` center (fires from screen center, weapon model hidden). Muff Mode adds `hand 3` — centered fire like `hand 2` but with the weapon model still visible. Set it from the console (`hand 3`); the in-game menu only exposes 0–2.
