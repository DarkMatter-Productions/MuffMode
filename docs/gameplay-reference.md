# MuffMode Gameplay Reference

[README](../README.md) | [Player Guide](player-guide.md) | [Server Host Guide](server-host-guide.md) | [Rulesets](rulesets.md) | [Configuration Reference](configuration-reference.md)

This reference helps players and hosts choose what to play. Casual players can use it to find modes that sound fun; competitive players can compare match-focused options; hosts can use it when building rotations and per-gametype configs.

## Choosing What To Play

| If you want... | Try |
| --- | --- |
| A quick public game | Deathmatch, Instagib, NadeFest, or Horde Mode. |
| A competitive head-to-head match | Duel with a [known ruleset](rulesets.md) and a focused map list. |
| Organized team play | Team Deathmatch, Capture the Flag, Clan Arena, or Capture Strike. |
| A different pace for a community night | Red Rover, Capture Strike, Horde, Vampiric Damage, Weapons Frenzy, Quad Hog, or custom [ruleset](rulesets.md)/map combinations. |

## Feature Overview

- Purpose-built HUD and scoreboard with frag messages, dynamic miniscores, scorelimit context, match state, timer, help text, MOTD support, and a compact scoreboard.
- Game menu for joining matches, changing or voting on settings, and viewing mod or server info.
- GUI voting for maps, gametypes, rulesets, server settings, and administrative actions.
- Team captain system with automatic captain assignment, captain transfer, and captain-managed team controls.
- Match progression with warmups, ready states, countdowns, post-match delays, sudden death, overtime, and round handling.
- Enhanced teamplay with auto-balancing, forced balance rules, improved team messaging, major item pickup notices, weapon drop points of interest, and friendly fire warnings.
- Extensive controls for map item spawns and entity string overrides.
- First-person and third-person spectator following with smooth behavior and aim prediction.
- MyMap queueing inspired by Tastyspleen.
- Bug fixes, minor refinements, balance tweaks, and additional server settings.

## Muff Maps

Muff Mode maintains a curated set of final `mm-*` remasters and ports, plus a separate work-in-progress source appendix. The full [Muff Mode Map Guide](maps/index.md) includes per-map history, original release dates where found, preserved original readmes/BSPs, source-map links, recommended gametypes, and item registers taken from the final BSP entity data.

| Map | File | Status | Good fits |
| --- | --- | --- | --- |
| Aerowalk | `mm-aerow` | Final release | Duel, small FFA, 2v2, Clan Arena |
| Bio Rust | `mm-biorust` | Final release | Duel, small FFA, 2v2 |
| Conventional | `mm-conven` | Final release | FFA, 2v2, TDM, Quad Hog |
| The Crucible | `mm-crucible` | Final release | Duel, FFA, 2v2 |
| Cold Zero | `mm-czero` | Final release | FFA, 2v2, TDM, Instagib |
| Degeneration | `mm-degen` | Final release | FFA, 2v2, TDM |
| The Flesh Refinery | `mm-fleshref` | Final release | Duel, small FFA, Power Screen experiment |
| Grind | `mm-grind` | Final release | Duel, 2v2, FFA |
| Iron Oxide | `mm-ironox` | Final release | Duel, small FFA, 2v2 |
| The Killing Machine | `mm-kmach` | Final release | FFA, 2v2, casual Duel |
| Lava Lamp | `mm-llamp` | Final release | FFA, TDM, party server |
| The Longest Yard | `mm-longyd` | Final release | FFA, Instagib, Clan Arena, jump-pad chaos |
| Mortal Coil | `mm-mcoil` | Final release | Duel, small FFA, 2v2 |
| Negative Impulse | `mm-negimp` | Final release | FFA, 2v2, TDM |
| The Oppressor | `mm-oppress` | Final release | FFA, TDM, 2v2 |
| Painkiller | `mm-pkill` | Final release | Duel, small FFA, Clan Arena |
| The Rage | `mm-rage` | Final release | Duel, FFA, 2v2 |
| Railgun 101 | `mm-rail101` | Final release | Instagib, rail practice, aim warmups |
| Reclamation | `mm-reclam` | Final release | Duel, small FFA |
| Thunderstruck | `mm-thunders` | Final release | Duel, Clan Arena, Instagib, rail/rocket practice |
| Unknown Domain | `mm-undom` | Final release | FFA, TDM, large public play |
| Wicked | `mm-wicked` | Final release | Duel, Clan Arena, small FFA |
| Window Pain | `mm-winpain` | Final release | Clan Arena, Instagib, FFA warmups |

## Gametypes

| Short name | Gametype | Description |
| --- | --- | --- |
| `ffa` | Deathmatch | Free-for-all play for quick public games and warmups. |
| `duel` | Duel | Competitive one-on-one play; the winner faces the next queued opponent. |
| `tdm` | Team Deathmatch | Competitive or casual team frag competition. |
| `ctf` | Capture the Flag | Team flag capture play with stronger coordination and map control. |
| `ca` | Clan Arena | Round-based team elimination with no item spawns, no self-damage, and a full arsenal. Good for fast team matches. |
| `strike` | Capture Strike | Threewave-inspired attack/defend mode: teams alternate offense and defense on CTF maps with a single life per turn and a full arena loadout (100 health / 100 armor). Offense earns 1 team point for the first enemy-flag touch and 2 more for a capture or defender wipe; defense earns no team points. Match ends at `capturelimit` (default 15) after both teams have attacked in the current round-pair. |
| `rr` | Red Rover | Two teams with the Clan Arena loadout; on death you defect to the opposing team and respawn instantly. An arena mode like CA: each round ends on a team wipe (everyone forced onto one team) or `roundtimelimit`, announces that round's top fragger, and reshuffles for the next one. The match ends on `roundlimit` (primary) or `timelimit` (backstop) and the player with the most frags wins. Scored by individual frags; `fraglimit` is disabled, as in CA. |
| `horde` | Horde Mode | Fight monster waves and stay on top of the scoreboard through a finite or endless run. Good for casual groups; packaged presets use limited lives and slower non-weapon item respawns. |
| `instagib` | Instagib | Rail-focused instant-kill combat. |
| `nadefest` | NadeFest | Grenade-only combat. |

`ft`, `lms`, and `ball` are reserved or removed in the current build and are not exposed as supported play choices.

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

Rulesets change the feel of the same map: starts, weapon specs, ammo, armor, health, powerups, and some movement details. The full player-facing breakdown lives in [Muff Mode Rulesets](rulesets.md).

Use this quick picker when choosing a server vote:

| If you want... | Pick |
| --- | --- |
| Closest Quake II Rerelease baseline | `q2re` |
| Muff Mode's house balance | `mm` |
| Quake III Arena style weapons, pickups, splash, and knockback | `q3a` |
| Conservative competitive Quake II rebalance | `q2reb` |
| Classic Quake-inspired starts and arsenal | `q` |
| Modern arena-inspired random starts and tighter caps | `qc` |

| Value | Short name | Ruleset |
| --- | --- | --- |
| `1` | `q2re` | Quake II Rerelease |
| `2` | `mm` | Muff Mode |
| `3` | `q3a` | Quake III Arena style |
| `4` | `q2reb` | Q2RE Balanced |
| `5` | `q` | Quake style |
| `6` | `qc` | Quake Champions style |

## Tweaks And Fixes

- Instagib and NadeFest give players regeneration to recover from environmental and fall damage.
- Quad and Protection color shells do not change by team color, reducing visual confusion.
- `func_rotating` explodes non-player entities such as dropped items, preventing blocked rotators.
- Fragging Spree award broadcasts every 10 frags without dying or killing a teammate.
- Techs can be picked up after being dropped.
- Gametype changes can happen instantly, such as switching from FFA to TDM.
- DualFire Damage is presented as Haste. In `q2re` and `q2reb` it boosts fire rate only; in `mm`, `q3a`, `q`, and `qc` it also boosts movement.

## Related Docs

- Player commands and hook binds: [Player Guide](player-guide.md)
- Player-facing ruleset differences: [Rulesets](rulesets.md)
- Host setup and match cvars: [Server Host Guide](server-host-guide.md)
- Full command and cvar lookup: [Configuration Reference](configuration-reference.md)
- Mapper features and entity controls: [Level Design Guide](level-design-guide.md)
