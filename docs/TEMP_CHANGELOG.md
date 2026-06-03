# Temp changelog

## Unreleased

### Change

- **Gametype availability:** Central registry in `mm_gametype` controls which modes can be selected. Removed stub gametypes (Freeze Tag, ProBall, LMS) and their gameplay code; disabled CaptureStrike and Red Rover from vote/admin/menu selection (implementation retained for a future release). Invalid `g_gametype` values fall back to FFA. Enum indices unchanged for configs and map entity keys.

### Fix

- **Miniscore match limit HUD:** Show frag/round/capture limit with statusbar `num` and a numeric `STAT_SCORELIMIT` so the value centers under miniscore faces on vanilla and Muff Mode clients (replaces left-aligned `stat_string`).
- **Horde map load crash (Debug RTC #3):** Initialize statusbar layout `y` to `2` in `MM_InitStatusbar()` when building horde wave/monster HUD. Horde uses `InCoopStyle()` but `g_coop_enable_lives` defaults off, so `y` was never set before `y += 10` on map change.
- **Horde warmup spawn crash:** Horde monster spawning calls `SelectDeathmatchSpawnPoint(nullptr, …)`; `SPAWN_FARTHEST` and `SPAWN_NEAREST` dereferenced `ent->client` without a null check (access violation at offset 0x78). Guard bot/human spawn flags when `ent` is null; require `result.spot` (not only `any_valid`) before placing horde monsters; skip spawn when `Horde_PickMonster()` returns null.
- **Arena elimination corpses (CA / CaptureStrike):** Eliminated players keep the brief death pose, then auto-enter chase cam on a teammate. Client corpses during that beat and bodyque leftovers are non-damageable and non-solid so splash/rockets no longer interact with dead bodies. Fixed `T_Damage` uninitialized `asave`/`psave` on arena self-damage (Debug RTC #3 crash after death). Clear first-person view weapon on death so elimination modes match FFA death cam (no warped gun model).
- **Arena loadout:** Plasma Beam removed from starting loadout in all arena modes (Clan Arena, CaptureStrike, Red Rover), not just CA.
- **Round timer HUD (CA / round modes):** Dual bottom timer showed match elapsed twice (both counting up) because two `G_TimeString` calls in one format shared its static buffer; copy the match time string before formatting round remaining.
