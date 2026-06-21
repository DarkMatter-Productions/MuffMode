# MuffMode Player Guide

[README](../README.md) | [Server Host Guide](server-host-guide.md) | [Gameplay Reference](gameplay-reference.md) | [Configuration Reference](configuration-reference.md)

This guide is for anyone joining a MuffMode game. You do not need to learn every console command to have a good time: most everyday actions are available through the game menu, and the commands below are here when you want faster control.

## Quick Orientation

| If you are... | Start with |
| --- | --- |
| New or playing casually | Join through the menu, use `team auto`, read `motd`, and call votes from the voting menu. |
| Playing a match or pickup | Use `readyup`, know `time-out`, check the active ruleset, and use team/captain commands when needed. |
| Spectating or casting | Use `follow`, `followleader`, `followkiller`, and `followpowerup` to keep the action easy to watch. |

## Install

1. Download the [latest Muff Mode release](https://github.com/DarkMatter-Productions/MuffMode/releases/latest).
2. Use the Windows installer when available. It defaults to the Steam Quake II Remastered path and also offers Epic Online Store / Epic Games Store, GOG, and custom library choices.
3. If you use the zip instead, extract it into the outer `Quake 2` folder and allow file replacements.
4. Launch Quake II normally, then join or host a multiplayer game.

If you are joining someone else's server, they control most match settings. Your client-side commands still let you adjust your display preferences, ready state, team, votes, and spectator behavior.

## First Match

- Use the game menu to join a match, change teams, view server info, or open voting options.
- Use `team auto` for the quickest team join on team servers.
- Use `ready`, `notready`, or `readyup` when the server uses competitive-style warmups.
- Use `motd` to read server rules, notes, or event information.
- Use `maplist` to see maps available in the server rotation.

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

## Display Commands

| Command | What it does |
| --- | --- |
| `announcer` | Toggle Quake Live style match announcer event support. |
| `eskin <model/skin>` or `eskin off` | In team games, re-skin all enemies to the chosen model on your screen only (e.g. `eskin male/grunt`). No argument shows the current setting. |
| `fm` | Toggle frag messages. |
| `help` | Toggle help text drawing. |
| `id` | Toggle crosshair player identification. |
| `kb` | Toggle kill beeps. |
| `timer` | Toggle the match timer. |
| `tskin <model/skin>` or `tskin off` | In team games, re-skin all teammates to the chosen model on your screen only. No argument shows the current setting. |

## Gameplay Commands

| Command | What it does |
| --- | --- |
| `ready`, `notready`, `readyup` | Set or toggle ready status. |
| `callvote <command> <arg>` or `cv <command> <arg>` | Start a vote. |
| `vote yes` or `vote no` | Vote on an active proposal. |
| `mymap <map>` | Add a valid map to the MyMap queue. |
| `maplist` | Print the current server map list. |
| `motd` | Print the message of the day. |
| `forfeit` | Forfeit a duel when `g_allow_forfeit` is enabled. |
| `time-out` | Call a timeout when the server allows timeouts. |
| `time-in` | End an active timeout early. |
| `follow <player>` | Spectate a specific player. |
| `followkiller` | Toggle auto-following killers while spectating. |
| `followleader` | Toggle auto-following the leading player while spectating. |
| `followpowerup` | Toggle auto-following players who pick up powerups. |

## Voting

Use `callvote <command> <argument>` from the console, or use the voting menu when the server exposes it. Common examples:

```text
callvote map q2dm1
callvote gametype duel
callvote ruleset mm
callvote timelimit 15
callvote scorelimit 50
```

Servers can restrict voting, spectator voting, mid-match voting, available gametypes, and available rulesets. If a vote option is missing, it is probably a host choice rather than a client problem. See [Vote Commands](configuration-reference.md#vote-commands) for the full list.

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
- Custom skins, voting, team picking, timeouts, MyMap, and ready-up behavior are server controlled.
- If something feels misconfigured, ask the host to run `doctor`; it reports risky cvar combinations and suggested fixes.
