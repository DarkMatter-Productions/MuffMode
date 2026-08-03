# MuffMode Fuzz Targets

These are first-wave libFuzzer targets for phase three. They intentionally start with pure parser helpers so the corpus can run without booting the Quake II engine.

Initial targets:

- `fuzz_numeric_parsers.cpp`: integer, non-negative integer, finite float and gametype cfg integer parsers, plus the entity-string field-value helpers (escape expansion and RGBA packing) that BSP and `.ent` data reaches directly.

Planned targets:

- save/load JSON readers after a host-side fake `game_import_t` boundary is broad enough.
- map rotation/config parsing after those helpers are extracted from game state.
- info-key/userinfo helpers once their ownership/lifetime assumptions are documented.
