# HUD architecture

MuffMode ships one `game_x64.dll` with both sgame and cgame, but **remote clients render the HUD using their locally installed DLL**. The server pushes layout via `CS_STATUSBAR`; each client parses it in its own cgame (`CG_ExecuteLayoutString`).

## Two layers

| Layer | Owner | Contract | Stock Q2RE client | MuffMode-installed client |
|-------|-------|----------|-------------------|---------------------------|
| **Vanilla base** | sgame | `CS_STATUSBAR` + `ps.stats[]` + configstrings | Must parse and draw correctly | Same |
| **MM client enhancement** | cgame | Optional redraw / alignment after base parse | Not present | Adds polish on top |

**Policy:** New HUD features land in the vanilla base first unless they require a token or draw behaviour stock Q2RE lacks. Enhancements belong in cgame and must not be required for information to appear.

The layout string + stats must be **self-sufficient** for stock clients. MM cgame may redraw on top; it does not replace layout tokens.

## Draw order (deathmatch)

```mermaid
flowchart TB
  subgraph server [sgame]
    Init["MM_InitStatusbar()"]
    Writers["hud.cpp stat writers"]
    CS["CS_STATUSBAR"]
    Init --> CS
    Writers --> Stats["ps.stats + configstrings"]
  end
  subgraph client [cgame]
    Parse["CG_ExecuteLayoutString"]
    Notify["CG_DrawNotify"]
    Enhance["CG_DrawMuffModeHudEnhancements"]
    Parse --> Notify --> Enhance
  end
  CS --> Parse
  Stats --> Parse
```

## Vanilla-safe layout tokens

Only tokens implemented in stock `Vanilla/cg_screen.cpp` may appear in emitted layouts. See `MM_STATUSBAR_VANILLA_TOKENS` in `mm_hud_stat_contracts.h`.

### Forbidden tokens

| Token | Why |
|-------|-----|
| `ifbit` | Skipped by stock parser; paired `endif` from `endifstat()` triggers `Com_Error("endif without matching if")` |

Use `ifstat` + configstrings instead (same pattern as `STAT_WARMUP_NOTICE` / `STAT_ROUND_NUMBER`).

## Key stats (vanilla base)

| Stat | Writer | Layout consumer | Notes |
|------|--------|-----------------|-------|
| `STAT_MATCH_STATE` | `hud.cpp` / match | `xv 0 yb -78 stat_string` | Match phase label (`WARMUP`, timer, etc.) |
| `STAT_WARMUP_NOTICE` | unused | `xv 0 yb -90` | Reserved; warmup/ready guidance is centerprint (menu-bind band) |
| `STAT_GAMETYPE_HUD` | `hud.cpp` | top-right `loc_stat_rstring` | Gametype label; 5 s notice armed by team join and gametype/ruleset change (`MM_MatchInfoHud_Show`) |
| `STAT_RULESET_HUD` | `hud.cpp` | top-right | Ruleset label; same 5 s notice window as `STAT_GAMETYPE_HUD` |
| `STAT_ROUND_NUMBER` | `hud.cpp` | unused in the DM layout | Cleared every frame; the match limit rides on `STAT_SCORELIMIT` |
| `STAT_CENTER_LINE` | `hud.cpp` | `xv 0 yt 26` | Duel pic; LMS primary-lane POV text |
| `STAT_COUNTDOWN` | match | `yb -256 num` | Layout position (same on all clients) |
| `STAT_HORDE_REMAINING` | `hud.cpp` | Horde right stack `num` | Horde only |
| `STAT_SCORELIMIT` | `view.cpp` | match limit under the miniscore rows (`xr -28 yb -57 stat_string`) | Holds the `CONFIG_STORY_SCORELIMIT` index, so the limit draws as small white text rather than big HUD digits |
| `STAT_ARENA_ROLE` | `hud.cpp` / `MM_Arena_SetHudStats` | Strike/MuffMode Arena top-right or CA/Freeze centre yt 48 | Per-client room number/name, role, elimination, team, or queue status via the primary POV configstring lane |
| `STAT_MINISCORE_*` | `hud.cpp` | bottom corners | Team / FFA miniscore, shown only while the match is live (hidden through warmup, countdown, and intermission); MuffMode Arena publishes the selected room's red/blue series scores while that room's series is active |

Full contract comments: `src/sgame/muffmode/mm_hud_stat_contracts.h`.

## MM client enhancement (`CG_DrawMuffModeHudEnhancements`)

Hook after notify; **currently empty**. Timer, warmup, centre line, and countdown are drawn only via `CG_ExecuteLayoutString` so stock and MM clients match pixel-for-pixel.

Future MM-only polish (team border, etc.) belongs here and must not replace layout tokens.

## MuffMode Arena presentation

MuffMode Arena keeps its essential presentation in the vanilla-safe server layer. The server publishes each viewer's selected room, role, team/elimination state, and queue position through the existing arena-role stat/configstring contract, uses the normal red/blue miniscore fields for that room's series, and emits a room-local `svc_layout` scoreboard. A remote stock Q2RE client can therefore understand which room it is watching, who is fighting, the score, and who is next without `CG_DrawMuffModeHudEnhancements` or an Arena-specific cgame signal. Arena identity and room-local clock/countdown tokens sit outside the fighter-vitals `STAT_SHOW_STATUSBAR` gate, so lobby users, queued teams, coaches, and free observers retain that context without receiving combat HUD data.

Arena role and score data must be generated for the actual viewer after any followed-player HUD stats are copied. The scoreboard, crosshair ID, and follow target must use the viewer's arena rather than global player lists; otherwise a spectator could expose another room's participants or state.

## Validation

- **Runtime:** `MM_InitStatusbar()` errors if layout contains banned tokens (e.g. `ifbit`).
- **CI:** Host tests assert layout whitelist logic and banned-token detection via `MM_StatusbarLayoutUsesOnlyVanillaTokens`.

## Verification (stock client)

1. Restore vanilla Q2RE `game_x64.dll`.
2. Connect to a MuffMode server.
3. Confirm no `Com_Error` / client drop on HUD draw.
4. Spot-check FFA, CA, MuffMode Arena, Strike, Horde, LMS, RR — vitals, miniscore, timer (bottom-left), gametype stack, room labels, and Arena queue/series scoreboard.
