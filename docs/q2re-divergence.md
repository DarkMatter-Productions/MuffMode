# Where MuffMode's `RS_Q2RE` ruleset diverges from stock Quake II Rerelease

For player-facing ruleset selection and gameplay summaries, use [Muff Mode Rulesets](rulesets.md). This page is a technical parity audit for `q2re` only.

**Scope:** MuffMode running under the default ruleset `g_ruleset 1` (`RS_Q2RE`), compared against
stock Quake II Rerelease, for **single player**, **deathmatch**, and **CTF**. Sources compared:
`src/` (MuffMode) vs `q2re-src/` (stock q2re).

## 1. Summary — how to read this

`RS_Q2RE` is meant to "reproduce vanilla q2re." The **gameplay-rules logic is faithful**: across
the ~66 `RS()` ruleset branch sites, the `RS_Q2RE`/default path reproduces stock q2re values
essentially everywhere (weapons, armor, health, fall damage, powerup respawn, protection — see §5).

What actually makes MuffMode play differently out of the box comes from **three sources, none of
which are ruleset-gated**:

1. **Changed default cvar values** — config, not ruleset, and often overridden by the engine/menu
   at launch (§2, §3).
2. **A small set of unconditional code changes** that apply under every ruleset (§4).
3. **New default-ON deathmatch systems** layered on top of vanilla (§3, §7).

Two things that look like divergences but are **not** (verified against `q2re-src`):

- The **Defender / Hunter / Vengeance spheres** and the **Doppelganger** are stock q2re items
  (`q2re-src/g_items.cpp:3085+`), not MuffMode additions.
- The **Protection powerup is preserved**: `src/sgame/core/combat.cpp`
  `take = RS(RS_Q2RE) ? 0 : ceil(take/2)` → under ruleset 1, `take = 0`, identical to q2re's
  `invincible_time` path (`q2re-src/g_combat.cpp:672`).
- **Monster AI is vanilla**: `src/sgame/core/ai_navigation.cpp` is just stock q2re "blocked" navigation
  (`blocked_checkplat`, `face_wall`) relocated out of the monster files; the same functions exist
  in `q2re-src` and are called from every `m_*.cpp`.

## 2. Framing: latched cvars are usually set by the engine, not the DLL default

`deathmatch`, `coop`, and `skill` are `CVAR_LATCH`. In normal play the **engine/menu sets them at
launch** — "Start Campaign" sets `deathmatch 0` plus the chosen skill; starting a DM server sets
`deathmatch 1`. The DLL default only takes effect when the mode/skill is **not** explicitly
provided (a bare console `map foo`, or a server cfg that omits them).

So MuffMode changing these defaults shifts the *fallback*, not the menu-driven experience. A
menu-launched campaign still runs `deathmatch 0` at your chosen skill. Claims that "the campaign is
secretly deathmatch" or "much harder by default" are overstated for normal launches.

## 3. Changed default cvar values

Verified in `src/sgame/core/runtime.cpp` vs `q2re-src/g_main.cpp`.

### Changed defaults on cvars that exist in both

| Cvar | q2re | MuffMode | Effect |
|------|------|----------|--------|
| `deathmatch` | `0` | `1` (`runtime.cpp`) | Fallback mode is DM. Overridden by engine/menu for campaign. MuffMode is a DM-first mod. |
| `skill` | `1` | `3` (`runtime.cpp`) | Fallback skill is Hard. Overridden by menu skill selection. |
| `coop` | `0` | `0` | Unchanged. |
| `g_dm_force_respawn` | `0` | `1` (`runtime.cpp`) | Forced respawn after death (real DM behavior change). |
| `g_map_list_shuffle` | `0` | `1` (`runtime.cpp`) | Map rotation is shuffled. |

> Note: `g_dm_instant_items` (`1`) and `g_dm_spawn_farthest` (`1`) are **q2re-native defaults** and
> are unchanged in MuffMode — not divergences.

### New cvars, on by default, adding DM behavior absent from q2re

All are `CVAR_NOFLAGS` and absent from `q2re-src`:

| Cvar | Default | Effect |
|------|---------|--------|
| `g_dm_do_warmup` | `1` (`runtime.cpp`) | Warmup phase before the match starts. |
| `g_dm_force_respawn_time` | `3` (`runtime.cpp`) | 3-second forced respawn delay. |
| `g_dm_respawn_delay_min` | `1` (`runtime.cpp`) | Minimum 1s before a player may respawn. |
| `g_dm_holdable_adrenaline` | `1` (`runtime.cpp`) | Adrenaline becomes a holdable item. |
| `g_dm_powerup_drop` | `1` (`runtime.cpp`) | Players drop powerups on death. |
| `g_dm_crosshair_id` | `1` (`runtime.cpp`) | Crosshair shows teammate/target ID. |
| `g_dm_spawnpads` | `1` (`runtime.cpp`) | Spawn pad effects. |
| `g_dm_respawn_point_min_dist` | `256` (`runtime.cpp`) | Minimum distance between reused spawns. |
| `g_dm_allow_no_humans` | `1` | Bots may play with no humans present. |

Match-flow / neutral new cvars (note but low gameplay impact at default): `g_dm_overtime=120`,
`g_dm_timeout_length=120`, `g_dm_tie_max_time=1800`, `g_dm_item_respawn_rate=1.0` (neutral
multiplier), `g_dm_intermission_shots=0`, `g_starting_health_bonus=0`.

## 4. Unconditional code divergences (apply under `RS_Q2RE` always)

These are real behavior changes that apply under every ruleset — except the haste/DualFire item
below, which has since been ruleset-gated (kept here for history; it is now preserved under
`RS_Q2RE`/`RS_VANILLA_PLUS`).

### Haste / DualFire powerup movement speed — RULESET-GATED (was unconditional)
`IT_POWERUP_HASTE` (classname `item_quadfire`) is q2re's fire-rate "DualFire Damage" powerup. In
MuffMode it **also grants +30% movement speed** (`MaxSpeed()`, `src/shared/movement.cpp`,
`pm_maxspeed * HASTE_MOVESPEED_SCALE` when `ps->haste` is set). q2re's same powerup is fire-rate
only.

This was an always-on divergence; it is now **gated by ruleset** — the movement buff is suppressed
under the q2re-faithful rulesets (`RS_Q2RE`, `RS_VANILLA_PLUS`) via `MM_RulesetHasteBoostsMovement()`
(`src/sgame/muffmode/mm_ruleset.cpp`), applied where the synced `ps.pmove.haste` flag is set
(`src/sgame/client/lifecycle.cpp`). Under `RS_Q2RE`/`RS_VANILLA_PLUS` the DualFire powerup now matches stock
q2re (fire-rate only); MM/Q1/Q3A/QC keep the +30% buff. The CTF **Time-Accel tech** was always
weapon-speed only and is unchanged. See §5.

### pmove sub-zero-Z geometry handling
When the player origin `Z < 0`:
- Step height uses `STEPSIZE_BELOW = 20` instead of `STEPSIZE = 18` (`src/shared/game.h`; used at
  `src/shared/movement.cpp`).
- Jump gets `jump_height += 4` (`src/shared/movement.cpp`).

A targeted fix for maps with below-world-origin geometry. Affects SP and DM. Only triggers in those
map regions.

### Jump velocity rounding
`pml.velocity[2] = ceil(pml.velocity[2] + jump_height)` vs q2re's raw `+=`. Sub-unit, always on.

### Water-level sampling correctness
`PM_GetWaterLevel` now honors its `position` argument (`baseZ`) instead of reading global
`pml.origin` for upper sample points (`src/shared/movement.cpp`). Minor correctness change.

### Entity physics (`src/sgame/core/physics.cpp`)
No player-physics behavior change. The diff is renames (`edict_t`→`gentity_t`, `SV_*`→`G_*`,
`sv_maxvelocity`→`g_maxvelocity`) plus two non-default-play items: a Horde-gated monster-vs-monster
clip removal (`mask &= ~CONTENTS_MONSTER`), and `G_RunThink` returning `false` instead of
`Com_Error` on a null `think` (defensive). Treat as effectively vanilla.

### Core movement constants — identical
For reassurance: every core pmove constant matches q2re byte-for-byte — `pm_maxspeed 300`,
`pm_accelerate 10`, `pm_airaccelerate`, `pm_friction 6`, `pm_stopspeed 100`, `pm_waterspeed 400`,
`pm_waterfriction 1`, `pm_duckspeed 100`, `pm_laddermod 0.5`. Strafe/air-control/bunny-hop feel is
unchanged.

## 5. Ruleset-gated logic that PRESERVES q2re under `RS_Q2RE`

These exist as ruleset switches, but the `RS_Q2RE`/default branch equals stock q2re. Confirms the
baseline is faithful:

- **Weapons** — `src/sgame/muffmode/mm_ruleset_weapons.cpp`: machinegun, chaingun, rocket launcher,
  hyperblaster, railgun (dmg + kick), BFG, plasma beam, shotgun (dmg + pellets), slug pickup — all
  default to q2re values.
- **Items** — `src/sgame/muffmode/mm_items_rules.cpp`: armor tiers, health pickup caps/amounts, health
  respawn 30s, mega-health behavior, AutoDoc regen 500ms, breather 30s, adrenaline, powerup respawn
  (incl. 300s protection/invisibility), powerup pickup/respawn broadcast suppression, smart weapon auto-switch.
- **Combat** — `src/sgame/core/combat.cpp`: armor protection percentages, Protection powerup, and
  rocket splash knockback (`src/sgame/core/weapon_projectiles.cpp`, MM-only branch).
- **DualFire/haste movement buff** — `MM_RulesetHasteBoostsMovement()` (`src/sgame/muffmode/mm_ruleset.cpp`)
  suppresses the +30% movespeed under `RS_Q2RE`/`RS_VANILLA_PLUS` (fire-rate-only DualFire, like
  q2re); other rulesets keep it.
- **Movement/world** — fall-damage thresholds (`src/sgame/muffmode/mm_ruleset.cpp`), landing footstep event
  (`src/sgame/core/physics.cpp`, Q1-only branch), plasma beam range
  (`src/sgame/core/weapon_projectiles.cpp`, MM/VP/QC-only).
- **Spawn** — `src/sgame/muffmode/mm_spawn_filter.cpp` / `mm_spawn_loadout.cpp`: no weapon remapping
  under `RS_Q2RE`; standard starting health/armor; entity spawn filters honored.

## 6. Single player

Net divergence under the default ruleset is **small**:

- The unconditional pmove tweaks (§4) — sub-zero-Z step/jump, jump `ceil`, water sampling.
- The `skill`/`deathmatch` *fallback* defaults (§2) — overridden by the menu in normal play.
- Spheres / Doppelganger are stock q2re (not divergences).
- Monster AI is vanilla — `g_ai_new.cpp` is relocated stock "blocked" navigation (§1).
- The DualFire/haste movespeed buff no longer applies under `RS_Q2RE` (ruleset-gated, §4).

A menu-launched campaign at a chosen skill plays like q2re aside from the §4 pmove tweaks.

## 7. Deathmatch

Default DM differs from q2re mainly through:

- **§3 default cvars** — forced respawn (3s) + 1s minimum respawn delay, warmup phase, map-list
  shuffle, holdable adrenaline, powerup drop-on-death, crosshair ID, spawn pads, spawn min-distance.

(The DualFire/haste movespeed buff no longer applies under `RS_Q2RE` — ruleset-gated, §4.)

Core combat / weapon / item **values** remain q2re-faithful (§5). The difference is rhythm and
conveniences, not damage/balance numbers.

## 8. Capture the Flag

CTF is enabled by the `ctf` cvar (independent of ruleset). The **core CTF mechanics are
q2re-faithful**; the differences are architectural and opt-in features.

### Preserved (identical to q2re)
- **Flag scoring constants** — `src/sgame/entities/ctf.cpp` matches `q2re-src/ctf/g_ctf.h`: capture
  15, team 10, recovery 1, flag-bonus 0, frag-carrier 2, 40s flag return, 30s dropped-flag
  auto-return, 400u protect radii, 8/10/10s assist timeouts. Same non-cumulative `CTF_ScoreBonuses`
  hierarchy.
- **Techs** — renamed but identical magnitudes under `RS_Q2RE`:
  Disruptor Shield = Resistance `dmg/2` (`src/sgame/entities/items.cpp`),
  Power Amp = Strength `dmg*2` (`src/sgame/entities/items.cpp`),
  Time Accel = Haste, **weapon-speed only** (`src/sgame/entities/items.cpp`) — note this CTF tech does **not**
  grant movement speed; that buff belongs to the separate DualFire powerup (§4),
  AutoDoc = Regeneration `+5/500ms → 150` (`src/sgame/muffmode/mm_items_rules.cpp`, the `1_sec`
  slowdown is `RS_MM`-only).
- **Grapple** — same defaults: fly 650 / pull 650 / damage 10 (`src/sgame/core/runtime.cpp` vs
  `q2re-src/g_main.cpp:256-258`), same FLY→PULL→HANG states.

### Divergences
- **Offhand grapple** — new cvar `g_grapple_offhand` (`src/sgame/core/runtime.cpp`), **default `0`** (off);
  q2re has no offhand grapple. No change to default play.
- **Architecture / opt-in features** (not ruleset-gated, don't change a plain pub match):
  q2re's monolithic `ctf/g_ctf.cpp` (3862 lines) is dissolved into MuffMode core files
  (`src/sgame/entities/ctf.cpp` is flag core only); MuffMode adds the `GT_STRIKE` (Capture Strike)
  gametype and a rewritten round/match lifecycle (`src/sgame/muffmode/mm_match*`) replacing q2re's
  `competition`/election/ghost match system; teams use a unified `TEAM_RED`/`TEAM_BLUE` model vs
  q2re's `CTF_TEAM1/2` (cosmetic — same red/blue spawns, skins, auto-balance).

A plain CTF match under `RS_Q2RE` plays like q2re; divergences surface only with the added
gametypes/match features.

## 9. Verdict

`RS_Q2RE` is a faithful reproduction of q2re's **gameplay rules and balance**. What makes MuffMode
feel different out of the box is (a) DM-first default cvars and new default-on DM conveniences, and
(b) the sub-zero-Z pmove handling (region-specific, sub-unit).

The haste/DualFire movement-speed buff — previously the one always-on gameplay leak — is now
ruleset-gated off under `RS_Q2RE`/`RS_VANILLA_PLUS` (§4), so the DualFire powerup matches stock q2re
(fire-rate only) on the faithful rulesets.

The remaining candidate for bit-for-bit parity is defaulting the new DM systems off (warmup, forced
respawn, etc.) — but those are intentional DM-first defaults for this mod, so that is a product
decision rather than a fidelity bug.
