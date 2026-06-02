# Temp changelog

## Unreleased

### Fix

- **Horde map load crash (Debug RTC #3):** Initialize statusbar layout `y` to `2` in `MM_InitStatusbar()` when building horde wave/monster HUD. Horde uses `InCoopStyle()` but `g_coop_enable_lives` defaults off, so `y` was never set before `y += 10` on map change.
- **Horde warmup spawn crash:** Horde monster spawning calls `SelectDeathmatchSpawnPoint(nullptr, …)`; `SPAWN_FARTHEST` and `SPAWN_NEAREST` dereferenced `ent->client` without a null check (access violation at offset 0x78). Guard bot/human spawn flags when `ent` is null; require `result.spot` (not only `any_valid`) before placing horde monsters; skip spawn when `Horde_PickMonster()` returns null.
