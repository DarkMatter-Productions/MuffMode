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

## Horde Spawn Anchors

Horde automatically reuses combat-oriented single-player `monster_*` placements as spawn geography. The
campaign monster itself is not created: its origin and angles become an inert anchor. `monster_flipper`
locations and underwater `monster_gekk` locations become water anchors that can produce Flippers or Gekks; boss
classes become boss anchors; flying classes become aerial anchors; other combat monsters become ground anchors.
Decorative stands, commander bodies, and fixed monster turrets are left unused. Exact source-class matches are
preferred, but a compatible anchor can host another Horde monster when needed.

Bespoke maps and `.ent` overrides can place explicit anchors:

| Entity | Intended use |
| --- | --- |
| `info_horde_spawn` | Walking/ground monster placement. |
| `info_horde_flying_spawn` | Flyer, Floater, Hover, Daedalus, and compatible flying boss placement. |
| `info_horde_water_spawn` | Fully submerged Flipper/Gekk placement. |
| `info_horde_boss_spawn` | Large, deliberately cleared boss arena placement. |

All anchors are optional. The director validates hull clearance, minimum fighter distance, wave bounds, water
volume where applicable, and PHS relevance. During level load, it also tests each scaled boss profile against
all boss anchors and player spawns, marking the player points that can safely fit that specific profile. Boss
selection is restricted to profiles with enough validated placements; enabled pair encounters require two.
Maps with no compatible boss profile run ordinary escort waves at the scheduled cadence instead of forcing a
boss. Other monster spawns fall back through compatible anchors and then player spawn locations.

| Key | Default | Effect |
| --- | --- | --- |
| `horde_monster` | unset | Restricts an explicit anchor to one exact `monster_*` classname. Omit it for any habitat-compatible Horde monster. |
| `horde_min_wave` | `0` | Earliest wave that may use the anchor. |
| `horde_max_wave` | `0` | Last wave that may use the anchor; `0` has no upper bound. |
| `horde_weight` | `1` | Relative selection weight among valid anchors. |
| `horde_cooldown` | `0` | Positive per-anchor reuse delay in seconds; `0` uses `g_horde_map_spawn_cooldown`. |
| `horde_boss` | unset | On `info_horde_boss_spawn`, selects a named boss profile ID and implies its required `horde_monster` class. The profile can also influence the boss-wave roll before spawning begins. |
| `horde_boss_health_mult` | profile value | Positive replacement for the profile's authored health multiplier. Global, fighter, pair, and endless-wave multipliers still apply. |
| `horde_boss_damage_mult` | profile value | Positive replacement for the profile's outgoing-damage multiplier. |
| `horde_boss_scale` | profile value | Replacement model/hull scale, bounded by `g_horde_boss_scale_limit`; setting that cvar to `0` still retains the absolute safety ceiling of `16`. |
| `horde_boss_spawnflags` | profile value | Raw class-specific monster spawnflags. Set `0` to clear profile flags. Do not include common monster `AMBUSH`, `TRIGGER_SPAWN`, or `SIGHT` bits (`1`, `2`, `4`); the built-in Masters profile handles the Shambler's overlapping precision flag internally. |
| `horde_boss_power_armor_type` | profile value | `0` none, `1` power screen, `2` power shield. |
| `horde_boss_power_armor` | profile value | Power-armor capacity before `g_horde_boss_armor_mult`. |
| `horde_boss_monster_slots` | profile value | Summon capacity for Carrier-style profiles. |
| `horde_boss_reinforcements` | profile value | Rerelease reinforcement list, for example `monster_stalker 1` or a semicolon-separated weighted list. An empty value disables profile reinforcements. |
| `target` / `killtarget` | unset | Fires when a monster successfully spawns here. `delay` applies through normal target handling. |
| `deathtarget` | unset | Copied to the spawned monster and fired when that monster dies. |
| `healthtarget` / `itemtarget` | unset | Copied to the spawned monster for normal monster-death behavior. |

Use `gametype horde` on explicit anchors unless the same entity string is Horde-only. Author the origin as a
monster origin, not as a player origin; the director preserves authored height rather than applying the player
spawn lift.

Example staged boss arena:

```text
{
"classname" "info_horde_boss_spawn"
"origin" "0 0 64"
"angles" "0 180 0"
"gametype" "horde"
"horde_boss" "garbage_carrier"
"horde_min_wave" "12"
"horde_weight" "4"
"horde_cooldown" "30"
"horde_boss_monster_slots" "6"
"horde_boss_reinforcements" "monster_stalker 1;monster_gekk 2"
"target" "boss_gate_close"
"deathtarget" "boss_gate_open"
}
```

For `children_of_makron` or `masters_of_the_machine`, provide two suitably separated boss anchors when possible.
Both units receive a native health bar. A copied `deathtarget` fires for each killed unit, so route pair-completion
logic through `trigger_deathcount` with `count 2` when a door or phase must wait for the whole encounter.

Example water ambush:

```text
{
"classname" "info_horde_water_spawn"
"origin" "512 -256 -96"
"gametype" "horde"
"horde_min_wave" "2"
"horde_weight" "2"
}
```

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
| `info_horde_spawn` | Authored Horde ground-monster anchor; see [Horde Spawn Anchors](#horde-spawn-anchors). |
| `info_horde_flying_spawn` | Authored Horde aerial-monster anchor. |
| `info_horde_water_spawn` | Authored fully submerged Flipper/Gekk anchor. |
| `info_horde_boss_spawn` | Authored large-boss anchor. |
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
- Keep Horde boss anchors clear for the largest intended hull and use `deathtarget` for arena release scripting.
- Place water anchors fully inside safe water and verify nearby fighters share a PHS with the pool.
- Test entity changes under every gametype listed in the map's rotation.
