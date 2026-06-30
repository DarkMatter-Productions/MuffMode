# Muff Mode Rulesets

[README](../README.md) | [Player Guide](player-guide.md) | [Gameplay Reference](gameplay-reference.md) | [Server Host Guide](server-host-guide.md) | [Configuration Reference](configuration-reference.md)

Rulesets change the feel of the same map. They tune starting loadouts, weapon behavior, armor, health, ammo, powerups, and a few match-flow details. Use this page when a server vote asks for `q2re`, `mm`, `q3a`, `q2reb`, `q`, or `qc`.

## Quick Picker

| If you want... | Pick | Why |
| --- | --- | --- |
| The closest Quake II Rerelease baseline | `q2re` | Stock rerelease weapon feel and item rules. |
| Muff Mode's house balance | `mm` | Keeps Quake II identity, but trims problem weapons and improves powerup flow. |
| A Quake III Arena shaped match | `q3a` | Q3-style weapons, armor, pickups, splash, and knockback using existing Muff Mode assets. |
| A conservative competitive rebalance | `q2reb` | Vanilla-friendly changes without turning the game into another arena shooter. |
| A classic Quake-inspired arsenal | `q` | Shotgun/Axe starts, Quake-like rockets, and remapped weapon slots. |
| A modern arena-inspired loadout | `qc` | Random starting weapon, tighter caps, and modernized weapon tuning. |

## Values

| Value | Short name | Name |
| --- | --- | --- |
| `1` | `q2re` | Quake II Rerelease |
| `2` | `mm` | Muff Mode |
| `3` | `q3a` | Quake III Arena Style |
| `4` | `q2reb` | Q2RE Balanced |
| `5` | `q` | Quake Style |
| `6` | `qc` | Quake Champions Style |

Players can vote with `callvote ruleset <shortname>` when the server allows ruleset voting. Hosts can set `g_ruleset <value>` or use `ruleset <shortname>`.

## Reading Notes

- Arena gametypes such as Clan Arena and Red Rover can replace the normal spawn loadout. The active ruleset still affects weapon damage, fire timing, knockback, armor behavior, and ammo rules.
- In non-arena deathmatch warmup, most rulesets can grant visible map weapons for practice. `qc` is the exception: it keeps its random-start economy even in warmup.
- "Haste" refers to the rerelease DualFire/Haste behavior. `q2re` and `q2reb` keep it fire-rate only; `mm`, `q3a`, `q`, and `qc` also let it boost movement.
- Protection blocks splash damage while active. `q2re` keeps the faithful full direct-damage protection; the other rulesets halve remaining direct damage after armor instead.
- Q3A deliberately keeps Muff Mode's double-jump behavior. Q3-style knockback is used, but not the Q3 knockback movement lockout that would sever double jumps.
- Q3A does not add custom assets or entities. It remaps Q3 weapon and ammo names onto existing Quake II/Muff Mode items.

## Quake II Rerelease (`q2re`)

Choose this when you want the rerelease baseline.

| Area | Player-facing behavior |
| --- | --- |
| Starts | Standard Quake II deathmatch start. |
| Weapons | Baseline rerelease weapon damage, projectile speed, spread, ammo use, and recoil behavior. |
| Armor and health | Standard armor tiers, normal health rules, no Muff Mode/Q3 cap layer. |
| Powerups | Haste/DualFire changes fire rate only. Protection fully absorbs remaining direct damage and blocks splash while active. Protection and Invisibility use the faithful long respawn behavior. Activating major powerups plays the stock local use sound. |
| Pickups | Standard item economy, including Q2-style weapon and ammo relationships. |

Good for: stock-feeling servers, compatibility testing, and players who want the least surprise.

## Muff Mode (`mm`)

Muff Mode is the house ruleset: still Quake II, but less spiky.

| Area | Player-facing behavior |
| --- | --- |
| Starts | Standard Muff Mode deathmatch start. In warmup, visible map weapons can be pre-granted so players can practice with the map's arsenal. |
| Rockets | `100` direct damage, `100` splash damage, `120` splash radius, `650` speed, with lighter `50` direct-hit kick. |
| Plasma Beam | `10` deathmatch damage, `15` co-op damage, `768` unit range. |
| Ion Ripper | `20` deathmatch damage and `800` projectile speed. |
| Railgun | Normal damage profile, but stronger Muff Mode rail knockback. |
| Ammo | Slugs are tighter than the rerelease baseline: the normal slug box is `5` in this ruleset. |
| Power armor | Uses the CTF-style `1` damage per cell behavior in deathmatch. |
| Protection | Blocks splash and halves remaining direct damage after armor instead of acting as full q2re invulnerability. |
| Powerups | Haste boosts both weapon rate and movement. Major powerups use the Muff Mode deathmatch flow: delayed initial spawn, `120` second default respawn, global spawn/pickup sounds, and clearer announcements. |
| Utility items | Adrenaline grants `+5` max health in deathmatch. Rebreather lasts `45` seconds. Auto Doc regeneration is slower and alternates between health and armor instead of feeding both at once. |

Good for: the default Muff Mode experience, public servers that want Quake II with a cleaner balance curve, and competitive play that still wants Q2 movement and item identity.

## Quake III Arena Style (`q3a`)

Q3A reshapes Muff Mode toward Quake III Arena while staying inside Muff Mode's no-custom-assets restriction. It uses existing map layouts and existing Quake II/Muff Mode assets.

### Q3A Weapon Translations

| Q3 role | Muff Mode item used | Player-facing behavior |
| --- | --- | --- |
| Gauntlet | Chainfist | `50` damage, Q3-like short trace, no Quake II melee kick. |
| Shotgun | Shotgun | Regular Quake II shotgun asset with Q3 specs: `10` damage, `11` pellets, `700` spread. Super Shotgun is removed from Q3A. |
| Machinegun | Machinegun | `7` damage, or `5` in TDM, `200` circular spread. |
| Chaingun | Chaingun | Team Arena style role: `7` damage, `600` circular spread, fast one-bullet ticks. |
| Grenade Launcher | Grenade Launcher | `100` damage, `150` radius, `700` speed, Q3-like upward bias. |
| Rocket Launcher | Rocket Launcher | `100` direct, `100` splash, `120` radius, `900` speed. |
| Lightning Gun | Plasma Beam | `8` damage ticks, `768` range, 1 cell per tick. |
| Plasma Gun | HyperBlaster | `20` direct damage, `2000` speed, `15` splash damage, `20` splash radius. |
| Railgun | Railgun | `100` damage. |
| BFG | BFG | `100` direct/splash, `120` radius, `2000` speed. It is a Q3-style projectile blast without the Q2 BFG laser/zap sweep. Because cells are shared across weapons, each BFG shot costs `10` cells. |
| Nailgun | Ion Ripper | 15 nails, `20` damage each, `500` spread, randomized Q3-style nail speed, no bouncing. |

### Q3A Items And Movement Feel

| Area | Player-facing behavior |
| --- | --- |
| Starts | Spawn with Chainfist and Machinegun. Starting bullets are `100`, or `50` in TDM. Non-arena spawns get a `+25` health bonus. |
| Ammo caps | Standard and mapped ammo caps are broadly raised to `200`. |
| Ammo pickups | Q3-style amounts are preserved where they can be mapped cleanly: cells `30`, slugs `10`, lightning ammo as `60` cells, BFG ammo as `15` cells, nails as `20` cells, and chaingun belt as `100` bullets. |
| Weapon pickups | Q3-style weapon pickup amounts are used: Shotgun `10`, Machinegun `40`, Chaingun `80`, Grenade/Rocket/Rail/Nailgun `10`, Lightning Gun `100`, Plasma Gun `50`, BFG `20`. Normal pickups top you up to that amount, then give `+1` when you already have at least that much. Dropped and TDM-style pickups give the full amount. |
| Firing projection | Projectiles and hitscan shots use a snapped Q3-style muzzle point from eye height plus 14 units forward, without Q2 side offsets or weapon recoil. |
| Armor | Armor acts as one pool. Shards give `5`; armor protects for roughly `66%`; armor can climb to double max health and then decays back down. |
| Health | Small/medium/large health are `5`/`25`/`50`. Mega-style overheal can reach double max health, then decays toward max health. Mega Health respawns after `60` seconds. |
| Splash and knockback | Uses Q3-style splash falloff and Q3-style knockback scaling. Self-damage is halved after knockback is calculated, preserving rocket-jump force. |
| Falling damage | Q3-like thresholds with fixed `5` or `10` damage bands. |
| Powerups | Haste boosts movement and weapon rate. Major powerups use a delayed initial spawn, `120` second default respawn, global sounds, and announcements. |
| Death drops | Starting Machinegun is not dropped on death. Super Shotgun is not spawnable or switchable in Q3A. |

Good for: players who want Quake III Arena rhythm on Quake II maps, while keeping Muff Mode's double jumps and existing asset rules.

## Q2RE Balanced (`q2reb`)

Q2RE Balanced is the conservative competitive option. It keeps the Quake II Rerelease shape but softens a few edges.

| Area | Player-facing behavior |
| --- | --- |
| Starts | Standard Quake II-style start. In warmup, visible map weapons can be pre-granted for practice. |
| Health and armor caps | Persistent max health is capped at `200`; armor is capped at `150`. |
| Rockets | `100` direct damage, `100` splash damage, `120` radius. Rocket speed is `750` in deathmatch and `650` outside deathmatch. Self rocket-jump knockback stays close to q2re height even though self/enemy damage is lower. |
| Blaster | Faster projectile speed: `1700` in deathmatch and `1500` outside deathmatch. |
| Machinegun | `7` deathmatch damage. |
| Chaingun | Alternates between `4` and `5` damage in deathmatch. |
| HyperBlaster | HyperBlaster speed is `1100` in deathmatch and `1000` outside deathmatch. |
| Plasma Beam | `10` deathmatch damage, `15` co-op damage, `768` range. |
| Railgun | `90` damage. |
| Powerups | Haste/DualFire is fire-rate only, like q2re. Major powerup pickup and use sounds are broadcast so fights around them are easier to read. |

Good for: competitive Quake II servers that want a lighter touch than Muff Mode or Q3A.

## Quake Style (`q`)

Quake Style pushes the arsenal and pickups toward classic Quake.

| Area | Player-facing behavior |
| --- | --- |
| Starts | Spawn with Chainfist as the Axe stand-in, Shotgun, and `10` shells. |
| Arena loadouts | Arena modes skip Machinegun, Chaingun, and Railgun in the full loadout. |
| Shotguns | Shotgun uses Q1-style specs: `4` damage, `6` pellets, and tighter classic spread. Super Shotgun uses `14` pellets at `4` damage each; if you are already holding it with one shell left, it falls back to the regular shotgun shot. |
| Rockets | Randomized `100-120` direct damage, `120` splash damage, `160` radius, `1000` speed, and a centered Q1-style launch point. |
| Grenades and rockets | Grenade Launcher and Rocket Launcher share the rocket pool in Quake Style. Grenade ammo pickups become small rocket pickups. |
| Plasma Beam | Acts as the Thunderbolt stand-in: `30` damage ticks, `600` range, and `1` cell per tick. Firing while waist-deep or deeper in water discharges all cells into a radial blast worth `35` damage per cell before splash falloff. |
| Ammo caps | Shells, rockets, grenades, and cells cap at `100`; flechettes, used as nails, cap at `200`. |
| Armor | Jacket/Combat/Body armor stand in for Q1 green/yellow/red armor: `100` at `30%`, `150` at `60%`, and `200` at `80%`. Pickups replace your current armor only when the new armor value is stronger. |
| Map item remaps | Some Quake II slots are repurposed to stay closer to the Quake-shaped arsenal: Machinegun to ETF Rifle, Chaingun to Plasma Beam, Railgun to HyperBlaster, slugs to cells, bullets to flechettes, and grenades to small rockets. |
| Movement sound | Landing footstep sounds are disabled. |
| Death drops | Starting Shotgun is not dropped on death. |
| Powerups | Haste boosts movement and weapon rate. |

Good for: classic-leaning community nights and maps where rocket/shotgun control should dominate.

## Quake Champions Style (`qc`)

Quake Champions Style is the modern-arena flavored ruleset. It is more loadout-driven and less map-weapon-grant driven.

| Area | Player-facing behavior |
| --- | --- |
| Starts | Spawn with one random weapon: Shotgun with `50` shells, Machinegun with `200` bullets, or HyperBlaster with `200` cells. |
| Warmup economy | Unlike most other non-arena rulesets, QC does not pre-grant every visible map weapon during pre-match warmup. |
| Health and armor caps | Persistent max health is capped at `200`; armor is capped at `150`. |
| Ammo caps | Bullets, cells, and flechettes cap at `200`; shells cap at `100`; most other normal ammo starts from a tighter `50` cap, with a few special ammo caps lower. |
| Rockets | `100` direct damage, `100` splash damage, `120` radius, `750` speed. |
| Machinegun | `6` damage. |
| Chaingun | Alternates between `4` and `5` damage in deathmatch; `6` outside deathmatch. |
| HyperBlaster | `12` damage with the default `1000` projectile speed. |
| Plasma Beam | `10` deathmatch damage, `15` co-op damage, `768` range. |
| Railgun | `90` damage. |
| Haste | Boosts both movement and weapon rate. |

Good for: faster, more modern arena nights where the opening weapon roll changes the early fight.

## Related Docs

- [Gameplay Reference](gameplay-reference.md) for gametypes, maps, and game modifications.
- [Player Guide](player-guide.md) for voting and everyday commands.
- [Server Host Guide](server-host-guide.md) for `g_ruleset`, `g_votable_rulesets`, and per-gametype configs.
- [Configuration Reference](configuration-reference.md) for command and cvar lookup.
