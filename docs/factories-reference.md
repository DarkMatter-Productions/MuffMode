# Factory Catalogue Reference

Generated from `packaging/release-assets/rerelease/baseq2/factories.cfg` as shipped
today (VERSION `0.85.8`). This is a snapshot, not a build artifact — if
`factories.cfg` changes, this doc drifts and needs a re-pass. For how the factory
*system* works (layering, the cvar allowlist, `inherit`, voting), see
[configuration-reference.md#factories](configuration-reference.md#factories); this
doc is only "what does each shipped one actually do."

Every factory's numbers below are **fully resolved** through its `inherit` chain
— e.g. `instagib_frenzy` shows `fraglimit 60`, even though the file only states
`set fraglimit 60` on `instagib_frenzy` itself and everything else comes from
`instagib` → `_base_ffa`. "not set" means no factory in that chain touches the
cvar at all — the plain engine/`server-base.cfg` value is in force. Boilerplate
that is identical across almost every factory in a mode (`g_dm_crosshair_id`,
`g_mover_speed_scale`, `g_owner_auto_join`, spawn/respawn plumbing, ...) is
omitted here for readability; run `factory info <id>` in-game for the exhaustive
cvar-by-cvar dump.

## The "built-in" vs "classic" confusion

Every gametype has a plain **built-in** identity factory (`ffa`, `duel`, `tdm`,
`ctf`, `ca`, `ft`, `strike`, `rr`, `lms`, `horde`, `arena`) that carries **zero
overrides**. Picking it is a no-op: whatever `server-base.cfg` (or the operator's
own config) already has configured for that mode is what runs. It is not a
preset — it doesn't even set a ruleset.

Every one of those also has a `<mode>_classic` (or for FFA, both `ffa_classic`
*and* `house_dm`) that **does** carry an opinionated, fully-specified baseline.
This is the "Deathmatch vs. House Deathmatch" mix-up from the chat: `ffa` (built-in,
title "Deathmatch") changes nothing; `house_dm` (title "House Deathmatch") sets
the `mm` ruleset and turns powerups/techs back on over the stock preset; `ffa_classic`
(title "Classic Deathmatch") is the stock Q2 item economy. Three different things,
two of which sound almost the same.

---

## Hidden mode bases (`_base_*`)

Not selectable from a menu or vote (leading `_`), still selectable by name. Every
visible factory in a gametype inherits one of these and states only its own
differences. Full baseline shown once here rather than repeated per row below.

| id | Ruleset | Map pool | Key baseline settings |
|---|---|---|---|
| `_base_ffa` | q2re | q2dm1-3,5-8 | fraglimit 30, timelimit 15, weapons stay, no powerups, no grapple/techs |
| `_base_duel` | q2reb | q2dm1 | fraglimit 20, timelimit 10, ready-up required, match locked, no grapple/techs |
| `_base_tdm` | q2reb | q2dm1-3,5-8 | timelimit 10, ready-up required, match locked, friendly fire on |
| `_base_ctf` | q2re | q2ctf1-5 | timelimit 10, grapple + techs on, ready-up required, match locked |
| `_base_ca` | q2re | q2dm1-3,5-8 | timelimit 15, roundtimelimit 3, 150/150 stack, grapple on |
| `_base_ft` | q2reb | q2dm1-3,5-8 | timelimit 15, roundtimelimit 3, grapple on, no techs |
| `_base_strike` | q2re | q2ctf1-5 | capturelimit 15, roundlimit 0, 100/100 stack, grapple on, ready-up required |
| `_base_rr` | q2re | q2dm1-3,5-8 | timelimit 15, roundtimelimit 3, 100/100 stack, grapple on |
| `_base_lms` | q2re | q2dm1-3,5-8 | roundlimit 3, roundtimelimit 3, timelimit 15, 150/150 stack, grapple on |
| `_base_horde` | q2reb | q2dm1-3,5-8 | roundlimit (waves) 12, roundtimelimit 0, no powerups |
| `_base_arena` | mm | (rooms own it) | timelimit 15, 100/100 stack, roundtimelimit/roundlimit/capturelimit 0 |

---

## Free-For-All (13)

| id | Ruleset | Frag/Time | What makes it different |
|---|---|---|---|
| `ffa` *(built-in)* | — (untouched) | not set / not set | No overrides at all. "Whatever the server is already configured for." |
| `ffa_classic` | q2re | 30 / 15 | Stock Q2 economy: powerups on the map, weapons vanish and respawn on 30s. |
| `ffa_comp` | q2reb | 0 / 15 | Tournament FFA on `ffa_classic`: warmup+ready-up (100%), match lock, overtime 120, `mercylimit 0`. Timed only. |
| `house_dm` | mm | 30 / 15 | `_base_ffa` on the MuffMode ruleset with powerups and techs switched back on. |
| `ffa_rockets` | q2re | 30 / 15 | Spawn with rocket launcher + infinite ammo; weapons don't stay, respawn 45s. Rocket jumps cost fall damage. |
| `ffa_rockets_air` | q2re | 30 / 15 | Same as above but fall damage off — free rocket jumping. |
| `instagib` | q2re | 40 / 15 | Railgun only, one-shot kill, no splash. |
| `instagib_jump` | q2re | 40 / 15 | Instagib with splash on (rail-jump knockback, no damage) and fall damage off. |
| `instagib_frenzy` | q2re | 60 / 15 | Instagib at double rail fire rate. |
| `nadefest` | q2re | 20 / 5 | Grenades only. Short palate-cleanser round. |
| `vampffa` | q2re | 20 / 15 | Damage dealt heals 50%, capped at 200 hp, decays to a 25 hp floor. Health pickups removed. |
| `frenzyffa` | q2re | 50 / 15 | Double fire rate, faster rockets, regenerating ammo. |
| `quadffa` | q2re | 20 / 15 | One roaming quad; friendly-fire-off FFA means only fights involving the holder can happen. |

## Duel (6)

| id | Ruleset | Frag/Time | What makes it different |
|---|---|---|---|
| `duel` *(built-in)* | — (untouched) | not set / not set | No overrides. |
| `duel_classic` | q2reb | 0 / 10 | Timed only, both players ready up before the clock starts. |
| `duel_comp` | q2reb | 0 / 10 | `duel_classic` + warmup, overtime 120, no mid-match join, `mercylimit 0`. |
| `duel_mm` | mm | 0 / 10 | `duel_classic` on the MuffMode ruleset instead of Q2RE Balanced. |
| `instaduel` | q2reb | 20 / 10 | Railgun only, no splash. (Inherits `_base_duel` directly — frag cap stays.) |
| `vampduel` | q2reb | 15 / 10 | Vampiric: 50% leech, cap 200, decay floor 1 hp (tighter than FFA's 25 — hiding isn't safe in a 1v1). |

## Team Deathmatch (8)

| id | Ruleset | Frag/Time | What makes it different |
|---|---|---|---|
| `tdm` *(built-in)* | — (untouched) | not set / not set | No overrides. |
| `tdm_classic` | q2re | 100 / 20 | Team-balanced classic TDM. |
| `tdm_comp` | q2reb | 0 / 15 | **Inherits `_base_tdm` directly, not `tdm_classic`** (keeps q2reb). Timed only, 30s weapon respawns, quick-switch, warmup, overtime 120. |
| `instatdm` | q2re | 50 / 15 | Railgun only, splash on, fall damage off, friendly fire off. |
| `vamptdm` | q2re | 60 / 15 | Vampiric (50%/cap 200/floor 25), friendly fire off (else teammates are a heal source). |
| `frenzytdm` | q2re | 150 / 15 | Frenzy + weapons stay, friendly fire off. |
| `quadtdm` | q2re | 25 / 10 | Team-scored Quad Hog: only kills involving the holder land or count. |
| `nadetdm` | q2re | 50 / 10 | Grenades only, friendly fire off. |

## Capture the Flag (8)

| id | Ruleset | Cap/Time | What makes it different |
|---|---|---|---|
| `ctf` *(built-in)* | — (untouched) | not set / not set | No overrides. |
| `ctf_classic` | q2re | not set / 20 | Techs off, teams balanced. Capturelimit left to server config. |
| `ctf_comp` | q2reb | 8 / 20 | League CTF: 30s weapon respawns, warmup, overtime 120, `mercylimit 0`. |
| `ctf_ironman` | q2reb | not set / 20 | No grapple, friendly fire **on**, weapons don't stay (30s), no powerup drop. Grapple stays in `_comp` — this is the house-rules variant that drops it. |
| `instactf` | q2re | 5 / 15 | Railgun only, splash on (rail jump), fall damage off, grapple stays. |
| `vampctf` | q2re | 5 / 20 | Vampiric (50%/cap 200/floor 25). |
| `frenzyctf` | q2re | 10 / 15 | Frenzy. |
| `nadectf` | q2re | 5 / 15 | Grenades + grapple only. |

## Clan Arena (7)

| id | Ruleset | Round/RoundTime/Time | What makes it different |
|---|---|---|---|
| `ca` *(built-in)* | — (untouched) | not set / not set / not set | No overrides. |
| `ca_classic` | mm | not set / 3 / 0 | 200/200 full stack, no match clock — rounds decide. |
| `ca_comp` | mm | 10 / 3 / 0 | Self-damage-to-armor on, no grapple, warmup+ready-up, force balance, `mercylimit 0`. |
| `instaca` | mm | 10 / 2 / 0 | Railgun only, splash on, stack dropped to 100/0 (every hit kills anyway). |
| `vampca` | mm | not set / 2 / 0 | Vampiric, stack cut to 100/50 (leeching has to earn it back, armor is the counterplay). |
| `frenzyca` | mm | 10 / 2 / 0 | Frenzy, short rounds. |
| `nadeca` | mm | not set / 2 / 0 | Grenades only, stack 100/100 (200/200 vs. nades-only turns every round into a timeout). |

## Freeze Tag (6)

| id | Ruleset | Round/RoundTime/Time | What makes it different |
|---|---|---|---|
| `ft` *(built-in)* | — (untouched) | not set / not set / not set | No overrides. |
| `ft_classic` | q2reb | not set / 4 / 0 | Rounds decide; 4-min rounds leave room for thaw comebacks. |
| `ft_comp` | q2reb | 10 / 5 / 0 | Warmup+ready-up, force balance, no grapple (a grapple rescue trivializes thaw), `mercylimit 0`. |
| `instaft` | q2reb | not set / 2 / 0 | Railgun only, splash **off** deliberately (no free body-shoving frozen teammates). |
| `vampft` | q2reb | not set / 3 / 0 | Vampiric, cap 150, floor 30 (a freeze should always belong to a player, not the decay tick). |
| `frenzyft` | q2reb | not set / 3 / 0 | Frenzy + weapons stay + instant respawn (frenzy about firing, not the pickup race). |

## Capture Strike (5)

Round-based attack/defend; `roundlimit` stays `0` (uncapped/unused) throughout the
whole family, so it's omitted from the table below.

| id | Ruleset | Capturelimit | What makes it different |
|---|---|---|---|
| `strike` *(built-in)* | — (untouched) | not set | No overrides. |
| `strike_classic` | mm | 9 | Balanced teams, 3-point-max turns → 6-8 turn match. |
| `strike_comp` | mm | 9 | Warmup, no mid-match join, fixed sides (`g_teamplay_allow_team_pick 0`), `mercylimit 0`. |
| `instastrike` | mm | 9 | Railgun only, splash off, armor stripped to 0. |
| `vampstrike` | mm | 9 | Vampiric, cap 150, floor 50 — one life a turn, but damage heals. |

## Red Rover (5)

| id | Ruleset | Round/RoundTime/Time | What makes it different |
|---|---|---|---|
| `rr` *(built-in)* | — (untouched) | not set / not set / not set | No overrides. |
| `rr_classic` | mm | 6 / 3 / 0 | 6 rounds, no clock — round count ends the match. |
| `instarr` | mm | 6 / 2 / 0 | Railgun only, splash on, armor 0. |
| `vamprr` | mm | 6 / 3 / 0 | Vampiric, cap 200, floor 20, 60% leech (RR scores individual frags, so leech skews higher than team modes). |
| `frenzyrr` | mm | 6 / 2 / 0 | Frenzy. |

## Last Man Standing (4)

| id | Ruleset | Round/RoundTime/Time | What makes it different |
|---|---|---|---|
| `lms` *(built-in)* | — (untouched) | not set / not set / not set | No overrides. |
| `lms_classic` | mm | 5 / 3 / 0 | First to 5 round wins, one life a round, no clock. |
| `instalms` | mm | 5 / 2 / 0 | Railgun only, splash on, stack 100/0. |
| `vamplms` | mm | 5 / 3 / 0 | Vampiric, cap 250, floor **0** — the one factory here where decay is lethal on purpose (anti-camping; no team to let down). |

## Horde (5)

| id | Ruleset | Waves (roundlimit) | What makes it different |
|---|---|---|---|
| `horde` *(built-in)* | — (untouched) | not set | No overrides. |
| `horde_classic` | q2reb | 12 | Map weapons stay so a whole squad can arm up. |
| `horde_endless` | q2reb | 0 (endless) | Past wave 12, late-wave escalation (budget taper, ramped alive cap, themed waves) takes over. |
| `horde_hard` | q2reb | 20 | **Inherits `_base_horde` directly, not `horde_classic`** — weapons stay OFF (scarce on purpose). Friendly fire on, weapon respawn 120s. |
| `vamphorde` | q2reb | 12 | Vampiric — no health pickups on the map; hurting monsters is the only way back up. Cap 200, floor 50. |

## MuffMode Arena (2)

Almost everything here belongs to the room (`baseq2/arena.cfg`, keyed by map +
room ID), not the factory — these only own the two session-scope clocks.

| id | Ruleset | RoundTime/Time | What makes it different |
|---|---|---|---|
| `arena` *(built-in)* | — (untouched) | not set / not set | No overrides. |
| `arena_ra2` | mm | 5 / 30 | The only shipped Arena factory: 30-min map timer, 5-min room-round cap so a stalemate can't hold a queue hostage. |

## Hidden / development-only (2)

Not in any listing or vote menu; selectable by exact id only.

| id | Base | What it's for |
|---|---|---|
| `_sandbox` | ffa | Free-flight testing: no limits, infinite ammo, no self/fall damage. |
| `_horde_soak` | horde | Unattended bot-only Horde soak test off `horde_endless`: infinite ammo, no self/fall damage. |

---

## Already-shipped curated sets

These exist today in `packaging/release-assets/rerelease/baseq2/`, as
`g_votable_factories` values in the four lobby templates — i.e. the curation
idea from the chat is already half-built, just not the *default* experience
(`server-base.cfg` ships `g_votable_factories ""`, meaning unfiltered/everything):

| Config | Votable set |
|---|---|
| `lobby-casual.cfg` | Every `_classic` (+ `duel_mm`, `house_dm`, `ctf_ironman`, `horde_endless`, `horde_hard`) — 15 ids |
| `lobby-competitive.cfg` | Every `_comp` — 7 ids |
| `lobby-horde.cfg` | `horde_classic horde_endless horde_hard vamphorde` — 4 ids |
| `lobby-party.cfg` | Every mutator variant (`insta*`/`vamp*`/`frenzy*`/`nade*`/`quad*`) across every mode — 33 ids |
| `server-base.cfg` (default) | *(empty — every non-hidden factory is votable)* |

Worth noting: `lobby-casual.cfg`'s list is effectively "one obvious pick per
mode," which is close to what a curated default-install list would look like —
it's just not wired up as the shipped default.
