# MuffMode Level Design Guide

[README](../README.md) | [Gameplay Reference](gameplay-reference.md) | [Configuration Reference](configuration-reference.md) | [Server Host Guide](server-host-guide.md)

MuffMode gives level designers and server hosts extra control over item placement, conditional entity spawning, map behavior, and entity overrides. The goal is simple: let casual rotations, competitive map pools, and special event maps share the same BSP while still playing cleanly in each mode.

## New Items

| Item | Classname | Notes |
| --- | --- | --- |
| Personal Teleporter | `item_teleporter` | Holdable deathmatch item that teleports the activator to a spawn point. |
| Small ammo | `ammo_bullets_small`, etc. | Small variants for shells, bullets, rockets, cells, and slugs. |
| Large ammo | `ammo_bullets_large`, etc. | Large variants for shells, bullets, and cells. |
| Regeneration | `item_regen` | Thirty-second powerup that regenerates health up to twice max health. |

## Map Tweaks

Some MuffMode map entity overrides add subtle ambient sounds, mover sounds, intermission cameras, and gametype-specific item changes.

One included campaign fix changes `bunk1`: the button for the lift to `ware2` uses a wait of `-1`, preventing co-op players from toggling the lift again.

## Item Replacement Controls

Use cvars to disable or replace deathmatch map items by classname:

```text
disable_[classname]
replace_[classname]
[mapname]_disable_[classname]
[mapname]_replace_[classname]
```

Examples:

```text
disable_weapon_bfg 1
q2dm1_replace_item_quad item_regen
```

## Entity Override Files

MuffMode can load and save complete map entity strings as `.ent` files.

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_entity_override_dir` | `maps` | Subdirectory inside `baseq2` for override files. |
| `g_entity_override_load` | `1` | Loads `baseq2/<dir>/<mapname>.ent` on map load. |
| `g_entity_override_save` | `0` | Saves an override file on map load when one does not already exist. |

See [Map And Rotation Cvars](configuration-reference.md#map-and-rotation-cvars) for related host settings.

## Conditional Entity Keys

### Gametype Keys

Use `gametype` and `not_gametype` to include or exclude an entity for specific gametypes. Values can be comma-separated or space-separated.

```text
"gametype" "ffa duel"
"not_gametype" "ctf ca"
```

| Value | Gametype |
| --- | --- |
| `campaign` | Campaigns |
| `ffa` | Deathmatch |
| `tournament` | Duel |
| `team` | Team Deathmatch |
| `ctf` | Capture the Flag |
| `ca` | Clan Arena |
| `freeze` | Freeze Tag |
| `strike` | Capture Strike |
| `rr` | Red Rover |
| `lms` | Reserved Last Man Standing token |
| `horde` | Horde Mode |
| `ball` | Reserved ProBall token |
| `instagib` | Instagib |
| `nadefest` | NadeFest |

### Ruleset Keys

Use `ruleset` and `not_ruleset` to include or exclude an entity for specific rulesets. Values can be comma-separated or space-separated.

```text
"ruleset" "mm q2reb"
"not_ruleset" "q3a"
```

| Value | Ruleset |
| --- | --- |
| `q2re` | Quake II Rerelease |
| `mm` | Muff Mode |
| `q3a` | Quake III Arena style |
| `q2reb` | Q2RE Balanced |
| `q` | Quake style |
| `qc` | Quake Champions style |

Legacy `notq2` and `notq3a` keys are still understood, but new overrides should prefer `ruleset` and `not_ruleset`.

### Team Keys

| Key | Effect |
| --- | --- |
| `notteam` | Removes an entity from team gametypes. |
| `notfree` | Removes an entity from non-team gametypes. |
| `nobots` | Prevents a player spawn point (`info_player_start`, `info_player_deathmatch`, `info_player_team_red`, or `info_player_team_blue`) from being used by bots. |
| `nohumans` | Prevents a player spawn point (`info_player_start`, `info_player_deathmatch`, `info_player_team_red`, or `info_player_team_blue`) from being used by humans. |

Example:

```text
"notteam" "1"
```

## Entity Key Additions

| Entity | Key or spawnflag | Effect |
| --- | --- | --- |
| Items | `spawnflags & 8` | Suspends item in place; it does not fall to the floor. |
| `worldspawn` | `author`, `author2` | Sets level author information shown in server info. |
| `misc_teleporter` | `mins`, `maxs` | Overrides teleport trigger size and removes the visible teleporter pad if either key is set. |
| `trigger_push` | `target` | Can target `target_position` or `info_notnull` to set direction and apogee like Quake III Arena. |

## Entity Behavior Changes

| Entity | Change |
| --- | --- |
| `misc_nuke` | Applies nuke screen flash and earthquake effects. |
| `trigger_push` | Without a target, keeps original behavior; with a target, uses target-based launch behavior. |
| `trigger_key` | Does not remove inventory item in deathmatch. Deathmatch or `spawnflags 1` allows multiple uses. |
| `trigger_coop_relay` | Always behaves like `trigger_relay`, avoiding co-op progression blockers when players are absent. |
| `func_rotating` | Explodes non-player entities such as dropped items so rotators do not remain blocked. |

## New Entities

| Entity | Behavior |
| --- | --- |
| `target_remove_powerups` | Removes all activator powerups, techs, held items, keys, and CTF flags. |
| `target_remove_weapons` | Removes activator weapons and ammo except Blaster. Add `BLASTER` to also remove Blaster. |
| `target_give` | Gives the activator the targeted item. |
| `target_delay` | Delays before firing targets. Supports `wait` and `random`; total delay is `wait +/- random`. |
| `target_print` | Center-prints a message. Uses `message`; spawnflags include `REDTEAM`, `BLUETEAM`, and `PRIVATE`. |
| `target_setskill` | Sets skill level from `0` Easy through `3` Nightmare/Hard+. |
| `target_score` | Adjusts player score by `count`, default `1`; `TEAM` spawnflag also adjusts team score. |
| `target_teleporter` | Teleports activator to a target destination, or to a player spawn point when no target is set. |
| `target_relay` | Correctly named relay entity equivalent to `trigger_relay`. |
| `target_kill` | Kills the activator. |
| `target_cvar` | Sets a cvar using `cvar` and `cvarValue`. |
| `target_position` | Alias for `info_notnull`. |
| `func_bobbing` | Quake III Arena sine mover. Accepts `height`, `speed`, `phase`, and `dmg`; spawnflags `X_AXIS` and `Y_AXIS` change the bob axis from Z. Crushes through blockers like Q3. |
| `func_pendulum` | Quake III Arena pendulum mover. Uses brush length plus gravity for swing timing and accepts `speed`, `phase`, and `dmg`. Crushes through blockers like Q3. |
| `trigger_deathcount` | Fires targets after a minimum death count. Uses `count`, default `10`; `REPEAT` repeats every count. |
| `trigger_no_monsters` | Fires when all monsters are dead or none are present. Removed in deathmatch except Horde. `ONCE` removes after firing. |
| `trigger_monsters` | Fires when monsters are present. Removed in deathmatch except Horde. `ONCE` removes after firing. |

## Shooter Entities

| Entity | Projectile | Keys |
| --- | --- | --- |
| `target_shooter_grenade` | Grenade | `dmg` default `120`, `speed` default `600` |
| `target_shooter_rocket` | Rocket | `dmg` default `120`, `speed` default `600` |
| `target_shooter_bfg` | BFG | `dmg` default `200` in deathmatch or `500` in campaigns, `speed` default `400` |
| `target_shooter_prox` | Prox mine | `dmg` default `90`, `speed` default `600` |
| `target_shooter_ionripper` | Ionripper | `dmg` default `20` in deathmatch or `50` in campaigns, `speed` default `800` |
| `target_shooter_phalanx` | Phalanx | `dmg` default `80`, `speed` default `725` |
| `target_shooter_flechette` | Flechette | `dmg` default `10`, `speed` default `1150` |

## Design Checklist

- Add `gametype` or `not_gametype` keys to prevent mode-specific items from polluting other modes.
- Use `notteam` and `notfree` for clean team/free-for-all variants.
- Use `nobots` and `nohumans` to improve spawn quality for mixed bot and human servers.
- Prefer `.ent` overrides for server-side map variants that should not require repacking a BSP.
- Test entity changes under every gametype listed in the map's rotation.
