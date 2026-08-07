# MuffMode Gameplay Reference

[README](../README.md) | [Player Guide](player-guide.md) | [Server Host Guide](server-host-guide.md) | [Rulesets](rulesets.md) | [Configuration Reference](configuration-reference.md)

This reference helps players and hosts choose what to play. Casual players can use it to find modes that sound fun; competitive players can compare match-focused options; hosts can use it when building rotations and factories.

## Choosing What To Play

| If you want... | Try |
| --- | --- |
| A quick public game | Deathmatch or Horde Mode, or one of the modifier factories such as Instagib or NadeFest. |
| A competitive head-to-head match | Duel or one-player-squad Rocket Arena with a [known ruleset](rulesets.md) and a focused map list. |
| Organized team play | Team Deathmatch, Capture the Flag, Clan Arena, Rocket Arena with larger squads, Freeze Tag, or Capture Strike. |
| A different pace for a community night | Red Rover, Capture Strike, Horde, Vampiric Damage, Weapons Frenzy, Quad Hog, or custom [ruleset](rulesets.md)/map combinations. |

## Feature Overview

- Purpose-built HUD and scoreboard with frag messages, dynamic miniscores, scorelimit context, match state, timer, help text, MOTD support, and a compact scoreboard.
- Game menu for joining matches, changing or voting on settings, and viewing mod or server info.
- GUI voting for maps, gametypes, factories, rulesets, server settings, and administrative actions.
- Factory presets: named gameplay presets layered on a base gametype, so one server can offer several flavours of the same mode.
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
| `arena` | Arena Rooms (Rocket Arena) | MuffMode room-based play on tagged multi-room maps, with classic RA2 map profiles supported. Independent Rocket Arena, Clan Arena, Red Rover, and Practice rooms reuse MuffMode's teams, votes, readiness, competition, coaching, and timeout controls. |
| `ft` | Freeze Tag | Team round mode where deaths freeze players in place instead of respawning them. Live teammates thaw frozen players by standing nearby, with additional teammates accelerating the thaw and earning assist credit when they meaningfully help a successful rescue; freezing every active opponent wins the round. Frozen players cannot move, fire, use items, or pick up items, but they remain on the field with a white shell, a standing pose, and a third-person look-around camera; frozen bodies can be nudged by knockback or pulled with the grapple. Frozen players can call for help with a throttled teammate marker and location-aware chat line, and the HUD shows frozen/thawing state plus a clean top-right round display. Thawed players normally respawn at player spawn points with white-shell gibs thrown at the thaw spot, with a server option to restore them at the safe thaw location instead, and spawned/thawed players use normal map/item-control loadouts unless the server enables the optional Freeze Tag arena kit. |
| `lms` | Last Man Standing | Round-based free-for-all elimination. Each player starts with a limited number of lives; the final surviving player wins the round, and `roundlimit` determines the match winner. |
| `strike` | Capture Strike | Threewave-inspired attack/defend mode: teams alternate offense and defense on CTF maps with a single life per turn and a full arena loadout (100 health / 100 armor). Offense earns 1 team point for the first enemy-flag touch and 2 more for a capture or defender wipe; defense earns no team points. Match ends at `capturelimit` (default 15) after both teams have attacked in the current round-pair. |
| `rr` | Red Rover | Two teams with the Clan Arena loadout; on death you defect to the opposing team and respawn instantly. An arena mode like CA: each round ends on a team wipe (everyone forced onto one team) or `roundtimelimit`, announces that round's top fragger, and reshuffles for the next one. The match ends on `roundlimit` (primary) or `timelimit` (backstop) and the player with the most frags wins. Scored by individual frags; `fraglimit` is disabled, as in CA. |
| `horde` | Horde Mode | Fight monster waves and stay on top of the scoreboard through a finite or endless run. Good for casual groups; packaged presets use limited lives and slower non-weapon item respawns. |

`ball` is reserved or removed in the current build and is not exposed as a supported play choice.

Instagib and NadeFest are **modifiers**, not gametypes. They are applied on top
of whichever mode is running -- Instagib CTF, NadeFest Clan Arena and so on --
and the shipped factory catalogue offers the useful combinations by name. See
[Game Modifications](#game-modifications) and [Factories](#factories).

### Factories

A gametype says what the rules of engagement are; a **factory** says which
flavour of that gametype is being played. It names a base gametype and a set of
gameplay overrides on top of it, so a single server can offer `ctf_classic`
(eight captures, twenty minutes, techs on) and `instactf` (railgun only, five
captures) as separate, selectable presets rather than as two different servers.

Run `factory` to see which one is active and what else is on offer, and
`factory info <id>` to see exactly which settings it changes. Where the server
allows it, `callvote factory <id>` switches between them.

Factories are how a server is configured for play -- they replaced the
per-gametype config files the mod used to execute, so nothing runs underneath
one, and switching away from a factory restores everything it changed. The full
model, the settings a factory is allowed to touch, and the file format are in
[Factories](configuration-reference.md#factories).

### Arena Rooms Flow

Arena is MuffMode's room-based gametype. Rocket Arena 2 and 3 are behavioral
references and compatibility sources, not replication targets: MuffMode's
Q2RE networking, teams, commands, menus, HUD, voting, match controls, and
safety rules take precedence wherever the designs differ. The preferred tagged
multi-room profile declares `worldspawn` `arena` from `1` through `31`, uses
arena 0 as a usable lobby, and gives each positive room at least two tagged
deathmatch starts so both sides can spawn safely. Tagged entities stay local
to their room; a tagged
intermission can provide a name and observer view but is not an activation
requirement. Classic RA2 idmaps use a single shared-room profile instead:
explicit `worldspawn` `arena 0` selects it, or an untagged idmap is accepted
only when the host opts in with `g_arena_legacy_idmap 1` before a map load.
Within that one virtual room, all entity `arena` tags are treated as shared.
Ordinary maps never receive synthetic Arena behavior.

Every discovered room owns its state, players, teams, queue, settings, vote, ready state, clock, pause, scoreboard, and round series. Combat and presentation are separated as well: players cannot damage, telefrag, trigger, follow, identify, or spectate through another room, and projectiles stay in their originating room.

The four room types are MuffMode play modes:

- **Rocket Arena** uses the classic first-in queue. The champion fights the next eligible challenger; the winner stays, the loser returns to the queue, and a drawn round replays without rotating either side.
- **Clan Arena** uses persistent red and blue teams, single-life rounds, and a team wipe win condition.
- **Red Rover** transfers a defeated player to the opposing side and continues until one side owns the field.
- **Practice** is teamless, non-lethal free practice: player hits retain
  knockback and score damage progress without eliminating the target,
  ammunition is unlimited, and environmental deaths respawn immediately.

Room settings cover team size, best-of length, health, armor, weapons and ammo, health/armor protection modes, falling damage, accelerated switching, rocket speed, a room-scoped selectable grapple with an offhand binding, and excessive rapid-fire/faster-rocket/infinite-ammo play, plus balance, locks, and player limits. Competition mode activates MuffMode's room-local ready, captain, team-name, lock, kick, mute, private-spectator, and timeout controls. Captains own team-level changes, while any non-coach team member may grant private spectator access. RA3-era spellings remain accepted as compatibility aliases where Q2RE does not reserve them. Votes and pauses never affect a different room.

The HUD and scoreboard are generated for the viewer's selected room. Room chat stays inside that room, team chat stays inside the logical team, and `say_world` is the explicit map-wide channel.

The normal MuffMode Multiplayer menu remains the shell: **Browse Rooms** or
**Change Room**, **Teams & Queue**, and returning to the lobby are compact contextual
entries alongside the usual Follow, Player Config, Vote, Stats, Server,
Match, and Admin tools.

Freeze Tag's original Quake II concept and lineage are credited to [Darrell "Doolittle" Bircsak](https://darrellbircsak.com/2016/03/25/freeze-tag-reminisced/). Muff Mode's implementation was informed by his [Quake II Rerelease Freeze Tag port](https://github.com/dbircsak/freeze-tag-quake2-rerelease) and adapts the mode to Muff Mode's match, HUD, bot, and server-control systems.

## Game Modifications

| Modification | Description |
| --- | --- |
| Instagib | Railgun only, one shot one kill. |
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
