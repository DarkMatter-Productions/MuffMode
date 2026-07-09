# HUD layout previewer (dev tool, v1)

Offline previewer for the server-side `CS_STATUSBAR` layout, so you can align HUD elements
across gametypes/states without rebuilding the DLL or manufacturing each state in-game.
Repo tooling only — **not shipped in `game_x64.dll`**.

## Workflow

1. In a running server (listen server / local), as an admin, type:
   ```
   hud_dump
   ```
   This writes `hud_dump.json` to the server's working directory, containing the **current
   gametype's** built layout string plus a stat name→index map. Switch gametype and re-run to
   capture another mode.
2. Open `index.html` in a browser, click **Load hud_dump.json**, pick the file.
3. Toggle the **mock stats** in the right panel to flip every `if` branch (non-zero =
   condition true; `num` fields use the value). Use **all stats on** to see every element at once.
4. **Click** an element to inspect its token + anchor; **drag** it to read back the corrected
   anchor value (e.g. `xr -52`) to paste into the C++ emitter in
   `src/sgame/muffmode/mm_statusbar.cpp`.

There is also a built-in **Load sample** button for a quick smoke test without a dump.

## Fidelity (v1, intentional limits)

- Renders against a canonical **320×240** virtual screen (the space the layout numbers are tuned
  in). On a 320-wide canvas `xv` and `xl` coincide; `xr` is right-anchored, `yb` bottom-anchored.
- Text width is approximated at **8px/char** (classic conchars). Servers here use **kfont**
  (proportional), so right-aligned text (`loc_rstring`, round-progress, labels) can be a few px
  off. This is enough to spot misalignment, not pixel-exact. Real kfont metrics are a deferred
  v2 (a client-side `hud_metrics` export).
- Pic sizes are approximate: miniscore pics 24×24, `/players/*` 32, compass 12, otherwise a 16×16
  box. `pic STAT` can't resolve to a real image offline, so it's drawn as a labelled box.

## Mirrors

The token walk mirrors the geometry pass of `CG_ExecuteLayoutString` in
`src/cgame/screen.cpp` (anchor math, `if`/`endif`). If that parser gains a new token,
add it to `app.js`. The dump command lives in `src/sgame/muffmode/mm_statusbar.cpp`
(`MM_DumpStatusbar`) and `src/sgame/core/commands.cpp` (`hud_dump`).
