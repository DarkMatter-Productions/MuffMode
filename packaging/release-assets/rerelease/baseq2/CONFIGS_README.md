# Muff Mode Server Config Guide

This folder contains the server config bundle shipped with Muff Mode. Use it as a working starting point, then trim map lists, player limits, rulesets, voting, and hostnames for your own community.

## Files

| File | Purpose |
| --- | --- |
| `server-base.cfg` | Shared baseline for safety, voting, entity overrides, player limits, and common quality-of-life settings. |
| `gt-FFA.cfg` | Free-for-all preset. |
| `gt-DUEL.cfg` | Duel preset. |
| `gt-TDM.cfg` | Team Deathmatch preset. |
| `gt-CTF.cfg` | Capture the Flag preset. |
| `gt-CA.cfg` | Clan Arena preset. |
| `gt-ARENA.cfg` | MuffMode Arena Rooms preset with per-room Rocket Arena, Clan Arena, Red Rover, and Practice support. Tagged multi-room maps are preferred; classic RA2 profiles remain supported. Arena map assets are not included. |
| `gt-FT.cfg` | Freeze Tag preset. |
| `gt-LMS.cfg` | Last Man Standing preset for round-based free-for-all elimination. |
| `gt-STRIKE.cfg` | Capture Strike preset. |
| `gt-REDROVER.cfg` | Red Rover preset. |
| `gt-HORDE.cfg` | Horde preset. Set `roundlimit 0` after loading for endless Horde (`g_horde_late_escalation` defaults to `1`). |
| `gt-INSTAGIB.cfg` | Instagib preset. |
| `gt-NADEFEST.cfg` | NadeFest preset. |

## Quick Start

1. Install Muff Mode into the outer `Quake 2` folder.
2. Start Quake II Rerelease or your dedicated server.
3. Open the console and run `exec server-base.cfg`.
4. Run one gametype preset, for example `exec gt-FFA.cfg`, `exec gt-DUEL.cfg`, or `exec gt-ARENA.cfg`.
5. Run `doctor` after editing configs to check for risky cvar combinations.

Before using or making `arena` votable, install Arena-compatible maps you are
licensed to host and configure an Arena map list in `gt-ARENA.cfg`. Tagged
multi-room maps are the native path. The supplied preset leaves
`g_arena_legacy_idmap` at its fail-closed default of `0`; set it to `1` only
for a known rotation of untagged classic RA2 idmaps. Ordinary maps do not
activate Arena by default.

When `g_gametype_cfg` is `1`, Muff Mode automatically executes the matching `gt-[GAMETYPE].cfg` after later gametype changes by vote or admin command.

## How To Customize

- Change `hostname`, `maxclients`, `maxplayers`, passwords, and voting policy in `server-base.cfg`.
- Change mode-specific limits, map lists, rulesets, item toggles, and team settings in the matching `gt-*.cfg`.
- Keep map lists short at first, then expand once you know which maps fit your players.
- Use `g_votable_gametypes` and `g_votable_rulesets` to keep public votes focused.
- Turn on `g_muffmode_debug 1` only while diagnosing an issue, then set it back to `0`.

For the full command and cvar reference, see:
https://github.com/DarkMatter-Productions/MuffMode/blob/main/docs/configuration-reference.md
