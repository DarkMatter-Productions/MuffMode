# Temp changelog

## Unreleased

### Fix

- **Crash in `Weapon_RunThink` when player has no weapon:** When the time-acceleration loop in `Think_Weapon` ran multiple weapon ticks per server frame, a weapon think callback could switch weapons and leave `pers.weapon` null; the next loop iteration then crashed dereferencing it. Added a null guard at the top of `Weapon_RunThink`.

### Change

- **Horde item respawn is now an explicit cvar (`g_horde_item_respawn_scale`, default 4):** The previously hardcoded 4× Horde item-respawn slowdown is now a documented, tunable cvar. **Weapons are exempt** — they respawn at exactly `g_weapon_respawn_time` so the configured value matches the real in-game time (the in-game menu and config reference no longer disagree with actual behavior). `gt-HORDE.cfg` now sets `g_weapon_respawn_time 60` (was `15`, which silently became 60 via the old multiplier) and `g_horde_item_respawn_scale 4`. In-game timings are unchanged from before; only the configurability and the truthfulness of the weapon cvar changed.
- **Horde player-scale cap raised:** `g_horde_player_scale_max` default 4→8, so servers with 5–8 fighters get a proportionally larger wave point budget (6 players: 2.2x→3.0x). The 0.4 per-player factor and 1–4 player behavior are unchanged.
