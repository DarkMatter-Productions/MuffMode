# MuffMode Gameplay Reference

[README](../README.md) | [Player Guide](player-guide.md) | [Server Host Guide](server-host-guide.md) | [Configuration Reference](configuration-reference.md)

This reference helps players and hosts choose what to play. Casual players can use it to find modes that sound fun; competitive players can compare match-focused options; hosts can use it when building rotations and per-gametype configs.

## Choosing What To Play

| If you want... | Try |
| --- | --- |
| A quick public game | Deathmatch, Instagib, NadeFest, or Horde Mode. |
| A competitive head-to-head match | Duel with a known ruleset and a focused map list. |
| Organized team play | Team Deathmatch, Capture the Flag, Clan Arena, CaptureStrike, or Red Rover. |
| A different pace for a community night | Vampiric Damage, Weapons Frenzy, Quad Hog, or custom ruleset/map combinations. |

## Feature Overview

- Purpose-built HUD and scoreboard with frag messages, dynamic miniscores, scorelimit context, match state, timer, help text, MOTD support, and a compact scoreboard.
- Game menu for joining matches, changing or voting on settings, and viewing mod or server info.
- GUI voting for maps, gametypes, rulesets, server settings, and administrative actions.
- Team captain system with automatic captain assignment, captain transfer, and captain-managed team controls.
- Match progression with warmups, ready states, countdowns, post-match delays, sudden death, overtime, and round handling.
- Enhanced teamplay with auto-balancing, forced balance rules, improved team messaging, major item pickup notices, weapon drop points of interest, and friendly fire warnings.
- Extensive controls for map item spawns and entity string overrides.
- EyeCam spectating with smooth behavior and aim prediction.
- MyMap queueing inspired by Tastyspleen.
- Bug fixes, minor refinements, balance tweaks, and additional server settings.

## Muff Maps

| Map | File | Status | Notes |
| --- | --- | --- | --- |
| Almost Lost | `mm-almostlost-a1` | Alpha v1 | A Quake III Arena layout later released as `pro-q3tourney7`, revised for fast FFA and Duel play. |
| Arena of Death | `mm-arena-a3` | Alpha v3 | A small and simple Quake III Arena arena. |
| Hidden Fortress | `mm-fortress-a4` | Alpha v4 | Small-to-medium map connected by two teleporters, based on the revised Quake Live layout. |
| Longest Yard | `mm-longestyard-b2` | Beta v2 | The classic Quake III Arena space map. |
| Proving Grounds | `mm-proving-a4` | Alpha v4 | A small Duel map from Quake III Arena. |
| Vertical Vengeance | `mm-vengeance-a2` | Alpha v2 | A small Duel map from Quake III Arena. |

## Gametypes

| Short name | Gametype | Description |
| --- | --- | --- |
| `ffa` | Deathmatch | Free-for-all play for quick public games and warmups. |
| `duel` | Duel | Competitive one-on-one play; the winner faces the next queued opponent. |
| `tdm` | Team Deathmatch | Competitive or casual team frag competition. |
| `ctf` | Capture the Flag | Team flag capture play with stronger coordination and map control. |
| `ca` | Clan Arena | Round-based team elimination with no item spawns, no self-damage, and a full arsenal. Good for fast team matches. |
| `ft` | Freeze Tag | Team elimination where teammates thaw frozen players. Work in progress. |
| `strike` | CaptureStrike | Threewave-inspired attack/defend mode combining Clan Arena, CTF, and Counter-Strike style rounds. |
| `rr` | Red Rover | Clan Arena style mode where players change teams on death. The round ends when one team is eliminated. |
| `lms` | Last Man Standing | Survival-focused elimination mode. |
| `horde` | Horde Mode | Fight monster waves and stay on top of the scoreboard through up to 16 waves. Good for casual groups. Limited lives are not currently handled. |
| `ball` | ProBall | Sports-style mode where players carry a ball into the enemy goal. Work in progress. |
| `instagib` | Instagib | Rail-focused instant-kill combat. |
| `nadefest` | NadeFest | Grenade-only combat. |

## Game Modifications

| Modification | Description |
| --- | --- |
| Vampiric Damage | Gain health by damaging enemies. No health pickups and draining health keep pressure high. |
| NadeFest | Grenade-only rules. |
| Weapons Frenzy | Faster fire rates, faster rockets, regenerating ammo, and faster weapon switching. |
| Quad Hog | Find the Quad Damage and become the Quad Hog. |

## Deathmatch Refinements

- Intermission pre-delay gives a brief moment to see the winning frag or capture before final scores.
- Minimum respawn delay helps avoid accidental respawns and smooths the death-to-spawn transition.
- Kill beeps and frag messages highlight your frags and current rank.
- Smart weapon auto-switch prefers Super Shotgun from Shotgun and Chaingun from Machinegun, and avoids auto-switching to Chainfist.
- Current weapon, Blaster, and Grapple can be droppable or world-spawned under host control.

## Rulesets

Rulesets alter gameplay balance. Casual servers can leave the default alone or expose ruleset voting. Competitive servers should choose a known ruleset before a match and keep it consistent across maps.

| Value | Short name | Ruleset |
| --- | --- | --- |
| `1` | `q2re` | Quake II Rerelease |
| `2` | `mm` | Muff Mode |
| `3` | `q3a` | Quake III Arena style |
| `4` | `q2reb` | Q2RE Balanced |
| `5` | `q` | Quake style |
| `6` | `qc` | Quake Champions style |

### Quake II Rerelease

The rerelease ruleset keeps the baseline Quake II rerelease balance.

### Muff Mode

Muff Mode focuses on rebalancing several major pain points:

- Plasma Beam deathmatch damage reduced from `15` to `10`, with maximum range limited to `768` units.
- Railgun restored to `150` damage in campaigns; rail knockback is `damage * 2`.
- Slug pickup quantity reduced from `10` to `5`.
- Direct rocket damage changed from randomized `100-120` to a consistent `120`.
- Invulnerability is replaced by Protection: no splash damage, full slime protection, one-third lava damage, and half direct damage after armor protection.
- Adrenaline also increases max health by `5` during deathmatch.
- Rebreather hold time increased from `30` to `45` seconds.
- Auto Doc regeneration is slower and regenerates either health or armor at a time.
- Power Armor uses CTF's `1` damage per cell behavior across deathmatch.
- Powerups use default `120` second respawn, randomized `30-45` second initial delay, global spawn and pickup sounds, and spawn/pickup messages.

### Quake III Arena Style

Inspired by Quake III Arena:

- Start with Machinegun and Rip Saw.
- Super Shotgun replaced by Shotgun.
- Weapon velocity, spread, and damage adjusted toward Quake III Arena behavior.
- Ammo max set to `200` for each type.
- Weapon pickup gives `+1` ammo when already held.
- Armor has no tiers; shards are worth `+5`; armor always provides `66%` protection.
- Health and armor count down to max health.
- Spawning health bonus of `25`.
- Mega Health timer rule removed; Mega Health respawns after `60` seconds.
- Invulnerability is replaced by Protection.
- Powerups use default `120` second respawn, randomized `30-45` second initial delay, and global spawn/pickup sounds.

### Q2RE Balanced

A vanilla-friendly ruleset with selective balance improvements:

- Plasma Beam deathmatch damage reduced from `15` to `10`, with maximum range limited to `768` units.
- Chaingun damage reduced to `5`.
- Hyperblaster projectile speed increased to `1100`.
- Machinegun damage reduced to `7` in deathmatch.
- Rocket Launcher speed increased to `720` in deathmatch.
- Powerup pickup and activation sounds broadcast to all players in deathmatch.

### Quake Style

Inspired by Quake:

- Start with Shotgun and Axe only.
- No Machinegun, Chaingun, or Railgun.
- Max ammo limits set to `200`.
- Rocket Launcher uses randomized `100-120` damage and speed `1000`.
- Hyperblaster damage is `15` in deathmatch and `20` in co-op.
- Machinegun and Chaingun damage use default `8`.
- Landing footsteps are disabled.
- Armor protection uses stronger classic-style behavior.
- Classic weapon balance and pickup rules.

### Quake Champions Style

Inspired by modern arena shooters:

- Random starting weapon: Shotgun, Machinegun, or Hyperblaster, with `50` ammo.
- Max ammo limits: `200` bullets and `200` for other types.
- Rocket Launcher damage `100`, speed `750`.
- Hyperblaster damage `12`, speed `1100`.
- Machinegun damage `6`.
- Chaingun damage `6`.
- Railgun damage `80`.
- Plasma Beam damage `15`.
- Faster overall pace with modernized weapon balance.
- Enhanced movement mechanics and timing.

## Tweaks And Fixes

- Instagib and NadeFest give players regeneration to recover from environmental and fall damage.
- Quad and Protection color shells do not change by team color, reducing visual confusion.
- `func_rotating` explodes non-player entities such as dropped items, preventing blocked rotators.
- Fragging Spree award broadcasts every 10 frags without dying or killing a teammate.
- Techs can be picked up after being dropped.
- Gametype changes can happen instantly, such as switching from FFA to TDM.
- DuelFire Damage has been changed to Haste: `50%` faster movement and `50%` faster weapon rate of fire.

## Related Docs

- Player commands and hook binds: [Player Guide](player-guide.md)
- Host setup and match cvars: [Server Host Guide](server-host-guide.md)
- Full command and cvar lookup: [Configuration Reference](configuration-reference.md)
- Mapper features and entity controls: [Level Design Guide](level-design-guide.md)
