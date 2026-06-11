# Temp changelog

## Unreleased

### Fix

- **Crash in `Weapon_RunThink` when player has no weapon:** When the time-acceleration loop in `Think_Weapon` ran multiple weapon ticks per server frame, a weapon think callback could switch weapons and leave `pers.weapon` null; the next loop iteration then crashed dereferencing it. Added a null guard at the top of `Weapon_RunThink`.

### Change

- **Horde player-scale cap raised:** `g_horde_player_scale_max` default 4→8, so servers with 5–8 fighters get a proportionally larger wave point budget (6 players: 2.2x→3.0x). The 0.4 per-player factor and 1–4 player behavior are unchanged.
