# Muff Mode Server Config Guide

This folder contains the server config bundle shipped with Muff Mode. Use it as
a working starting point, then trim map lists, player limits, rulesets, voting
and hostnames for your own community.

## Files

| File | Purpose |
| --- | --- |
| `server-base.cfg` | **Your config.** Server identity, client capacity, voting and admin policy, and the fallback map rotation. Load it once at server start. Nothing MuffMode does will overwrite these. |
| `factories.cfg` | **The gameplay presets.** One entry per playable flavour — `ffa_classic`, `duel_comp`, `instactf`, `vampca`, `horde_endless`, and 50-odd more — each carrying its mode's ruleset, limits, map rotation and settings. Loaded automatically via `g_factory_file`. Copy it before editing. |
| `muffmode-map-pool.example.json` | Extensive opt-in structured catalog covering rerelease multiplayer maps and the maps included with MuffMode. Copy it to a new filename before customizing it. |
| `muffmode-map-cycle.example.txt` | Matching opt-in automatic-rotation example. Copy it to a new filename before customizing it. |

## Quick Start

1. Install Muff Mode into the outer `Quake 2` folder.
2. Start Quake II Rerelease or your dedicated server.
3. `exec server-base.cfg`
4. `factory list all` to see what is available, then pick one — `factory ctf_classic`,
   `factory instagib`, `factory horde_endless`. That sets the gametype, the limits
   and the map rotation together, and changes map to start it.
5. `doctor` to check for risky cvar combinations.

To become an admin: connect on the first client slot, or set an admin password in
your config and run `admin <password>`.

From a dedicated console with no client, use `sv factory <id>` instead.

## How The Presets Are Named

| Id | What it is |
| --- | --- |
| `<mode>_classic` | The standard version of the mode. What a public server should run. |
| `<mode>_comp` | The same mode set up for organised play: everyone readies up, nobody joins or is thrown into a running match, ties go to overtime, teams are force balanced. `ffa_comp`, `duel_comp`, `tdm_comp`, `ctf_comp`, `ca_comp`, `ft_comp`, `strike_comp`. |
| `insta*` `vamp*` `frenzy*` `nade*` `quad*` | The mutator presets, one per mode where the mutator makes sense — `instactf`, `vampca`, `frenzyft`, `nadetdm`, `quadffa`. |
| `_base_<mode>` | Hidden layer holding a mode's baseline. Everything visible for that mode inherits it; edit it to change every preset for that mode at once. |

A `_comp` preset changes match **conduct**, not weapon numbers — those belong to
the ruleset, which a preset selects by name and never overrides. Run
`factory info <id>` to see exactly what one carries.

## Working With Factories

| Command | Access | What it does |
| --- | --- | --- |
| `factory` | all | Show the active preset and the others that fit the current gametype. |
| `factory list [gametype\|all]` | all | List ids. `all` shows every one. |
| `factory info <id>` | all | Show exactly what a preset changes, line by line. |
| `factory cvars [prefix]` | all | List the settings a factory may set, e.g. `factory cvars g_arena`. |
| `factory diag <cvar>` | all | Show where a setting's current value came from and what it restores to. |
| `factory <id>` | admin | Select one. Changes map. |
| `factory none` | admin | Clear back to your `server-base.cfg` values. |
| `factory reload` | admin | Re-read `factories.cfg` from disk. |
| `sv factory <id\|none>` | console | The same, from a dedicated server console. |
| `callvote factory <id>` | all | Vote for one, subject to `g_votable_factories`. |

Restrict what players may vote for with `g_votable_factories`. Turn factory votes
off entirely by adding `131072` to `g_vote_flags`. Pin the mode so neither an
admin command nor a passed vote can move it with `g_gametype_locked 1` — the
server console still works, so you cannot lock yourself out.

## How To Customize

- **Copy `factories.cfg` before editing it.** A package update overwrites the
  shipped file. Copy it to your own leaf name in this folder and point
  `g_factory_file` at your copy. You can also load both — later files override
  earlier ones by id — so you can redefine only the presets you care about:
  `set g_factory_file "factories.cfg my-factories.cfg"`.
- Change a mode's limits, map rotation, player count or settings in its factory,
  not in a global config. `factory info <id>` shows what one currently carries.
- Keep `maxclients` and `kexmultiplayer maxplayers` aligned in `server-base.cfg`:
  the first allocates engine and game client slots, the second requests the
  separate KEX lobby capacity.
- A mode's `players` limit may intentionally be lower than the connected-client
  capacity.
- Keep map rotations short at first, then expand once you know which maps fit
  your players.
- Turn on `g_muffmode_debug 1` only while diagnosing an issue, then set it back.

A malformed definition is rejected on its own, named by file, line, id and
reason; the rest of the file still loads. If the whole document is unusable, none
of it is published and the previously working registry is kept — a bad edit
cannot leave the server with no factories. `factory reload` tells you which
happened.

## Structured Map Pools

For the optional structured map system, copy the two `muffmode-map-*.example`
files to new leaf filenames in this folder, then set `g_maps_pool_file` and
`g_maps_cycle_file` to those copies. Keeping the operator-owned copies separate
prevents a later package update from replacing local map choices. Without these
cvars, safe BSP-stem entries in `g_map_pool` and `g_map_list` — including the
ones a factory's `maps` and `mappool` directives set — remain the active source.

## MuffMode Arena

Before using or making `arena` votable, install Arena-compatible maps you are
licensed to host and give the `arena_ra2` factory (or your own copy of it) a
`maps` rotation of them. Tagged multi-room maps are the native path. The shipped
factory leaves `g_arena_legacy_idmap` at its fail-closed default of `0`; set it
to `1` only for a known rotation of untagged classic RA2 idmaps. Ordinary maps do
not activate Arena by default.

## Migrating From `gt-*.cfg`

Earlier releases shipped eleven per-gametype config files that the mod executed
automatically on a gametype change. They are gone; factories replaced them.

- `exec gt-FFA.cfg` and friends no longer work. Replace them with
  `exec server-base.cfg` followed by `factory <id>`.
- A custom `gt-*.cfg` of your own is no longer executed automatically on a
  gametype change. You can still `exec` it by hand, but the settings in it will
  not switch with the mode.
- **Per-mode map rotations, player limits and settings now live in the factory.**
  The shipped `factories.cfg` carries a hidden `_base_<mode>` for every gametype
  reproducing exactly what that mode's old preset did; the visible presets
  inherit it. If you wrote your own, put its rotation in a `maps` directive and
  its limits in `players`.
- **Per-mode `hostname` is gone, permanently.** A factory cannot set it — a vote
  must not be able to rename your server. Set it once in `server-base.cfg`.
- `g_gametype_cfg` no longer exists and nothing reads it.
- Anything a preset did that a factory cannot — arbitrary console commands,
  `alias`, `vstr`, engine cvars such as `bot_skill` — belongs in your own config
  exec'd at startup.

For the full command and cvar reference, see:
https://github.com/DarkMatter-Productions/MuffMode/blob/main/docs/configuration-reference.md
