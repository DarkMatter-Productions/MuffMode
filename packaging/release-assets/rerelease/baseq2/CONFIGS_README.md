# Muff Mode Server Config Guide

This folder contains the package-owned server config bundle shipped with Muff
Mode. The files are ready to run as defaults, but a later package update may
replace them. Copy any config, map pool, or cycle to your own leaf filename and
point your server at that copy before customizing it.

For a customized lobby, copy the whole chain: copy `server-base.cfg`, the
chosen `lobby-*.cfg`, the pool, and the cycle; then change the copied lobby's
first `exec` and its two map filenames to point at those operator-owned copies.
`mapdb.json` is different: KEX requires that exact singleton name, so leave it
package-owned and merge any private UI rows back into the new version after an
update.

## Files

| File | Purpose |
| --- | --- |
| `server-base.cfg` | Baseline server identity, client capacity, voting and admin policy, and the legacy fallback rotation. Every lobby preset executes it. Copy it before customizing it. |
| `factories.cfg` | **The gameplay presets.** One entry per playable flavour — `ffa_classic`, `duel_comp`, `instactf`, `vampca`, `horde_endless`, and 50-odd more — each carrying its mode's ruleset, limits, map rotation and settings. Loaded automatically via `g_factory_file`. Copy it before editing. |
| `mapdb.json` | Full stock-derived KEX/UI map database with the nine bundled MuffMode maps appended. This is engine metadata, not a MuffMode structured pool. Leave it package-owned and never use it as `g_maps_pool_file`. |
| `muffmode-map-pool.json` | Production MuffMode catalog covering rerelease multiplayer maps and all nine bundled maps, with strict mode tags and active-human player bounds. |
| `muffmode-map-cycle.txt` | Production automatic cycle shared across supported gametypes; MuffMode filters it by mode, active-human load, repeat delay, and current map. |
| `lobby-casual.cfg` | Flexible 16-slot public/friends lobby; random rotation, House Deathmatch start, direct map votes and MyMap enabled. |
| `lobby-competitive.cfg` | Ranked 16-slot pickup/scrim lobby; ordered rotation, Duel Competition start, direct map votes and MyMap disabled. |
| `lobby-party.cfg` | Unranked 16-slot mutator lobby; random rotation, Instagib Jump start, curated factory voting and direct map choice enabled. |
| `lobby-horde.cfg` | Focused 8-slot Horde lobby; random rotation, Classic Horde start, direct map votes and MyMap disabled. |

## Lobby Quick Start

1. Install Muff Mode into the outer `Quake 2` folder.
2. Start Quake II Rerelease and open the console before creating the lobby.
3. Execute one complete host preset:

   ```text
   exec lobby-casual.cfg
   exec lobby-competitive.cfg
   exec lobby-party.cfg
   exec lobby-horde.cfg
   ```

   Run only the one you want.
4. Create the lobby through the normal KEX menu. Loading the preset first lets
   its `maxclients` and `kexmultiplayer maxplayers` request take effect together.
5. Run `doctor` after joining to check for risky cvar combinations.

Each lobby preset executes `server-base.cfg`, enables the production structured
pool and cycle, and selects its starting factory. It can also be used as a
dedicated-server startup config. Copy the chosen `lobby-*.cfg` before changing
capacity, voting, ranking, factory choices, or startup mode. If you customize
it, also copy the baseline, pool, and cycle and update the copied lobby's
references; otherwise it will still load package-owned files on every run.

For a hand-built or legacy-map-list server, execute `server-base.cfg`, run
`factory list all`, then select a factory such as `factory ctf_classic`,
`factory instagib`, or `factory horde_endless`. A factory sets the gametype,
limits, legacy map rotation, and gameplay settings together and changes map.

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

## Map Databases And Rotation

`mapdb.json` and `muffmode-map-pool.json` are intentionally different formats:

- `mapdb.json` is the full stock-derived KEX engine/UI database. Its root has
  `episodes` and `maps`, and its mode metadata uses the KEX schema. The loose
  file preserves the stock campaigns and menus while adding the nine bundled
  MuffMode maps. Never point `g_maps_pool_file` at it.
- `muffmode-map-pool.json` is MuffMode's strict multiplayer catalog. Its root
  contains only `maps`; entries can add `min`, `max`, `duel`, `arena`,
  `popular`, and custom-resource policy. Unsupported root or entry keys fail
  closed instead of being ignored.
- `muffmode-map-cycle.txt` is the shared automatic sequence. With a valid pool
  and cycle, structured selection supersedes a factory's `maps` rotation for
  normal automatic transitions and factory/gametype changes. Factory rotations
  remain the legacy fallback when structured selection is disabled, invalid,
  or cannot find a compatible map.

The production lobby presets already select the pool and cycle. To customize
them, copy both files to operator-owned leaf names and change
`g_maps_pool_file` and `g_maps_cycle_file` in your copied lobby config.

Pool `min` and `max` bounds count active human players only, not bots,
spectators, or spare connected slots. Automatic rotation and the post-match
next-map pick apply mode tags and those bounds. A direct map vote or MyMap
request validates that the map belongs to the active catalog, but deliberately
does not enforce its mode tag or player bounds. Casual and Party leave that
freedom enabled; Competitive and Horde disable both paths so their rotation
policy cannot be bypassed.

`mm-rail101` is a specialist rail/Instagib practice map with no weapon
pickups. The schema cannot express an Instagib-only factory requirement, so the
production cycle deliberately excludes it. It remains in the pool and KEX map
database for an explicit map choice when the selected factory supplies the
intended weapon.

## Updates And Backups

The installer and current updater treat the exact package filenames above as
replaceable release assets. Before replacing an existing one, they preserve
server/map-policy files under `rerelease/baseq2/MuffModeBackups`; the updater
backs up files whose bytes differ from the incoming release, while the installer
backs up existing package-named files. Operator-owned leaf filenames are not in
the install plan and are left alone. If a current-updater copy fails partway
through, it restores the previous mutually-referencing host bundle before
reporting the failure.

After an update, keep using your operator copies and merge any wanted upstream
changes from the new templates. To restore a package-named customization, copy
the matching file back from the newest `server-configs-before-muffmode-*`
backup directory. For `mapdb.json`, merge custom rows into the newly shipped
full database rather than renaming it or pointing MuffMode's pool cvar at it.
Older upgrades may leave the retired
`muffmode-map-pool.example.json` and
`muffmode-map-cycle.example.txt` files on disk because installers do not delete
unrelated files. MuffMode does not reference them; after preserving any edits,
remove or archive them so they are not mistaken for the production pool/cycle.

## MuffMode Arena

No bundled map is Arena-compatible, so the production pool has no `arena: true`
entry and all four lobby presets keep Arena out of their votable modes and
factories. Before enabling it, install maps you are licensed to host, add them
to operator-owned pool/cycle copies with the correct Arena metadata, and give
the `arena_ra2` factory (or your own copy) a matching rotation. Tagged
multi-room maps are the native path. The shipped factory leaves
`g_arena_legacy_idmap` at its fail-closed default of `0`; set it to `1` only for
a known rotation of untagged classic RA2 idmaps.

## Migrating From `gt-*.cfg`

Earlier releases shipped twelve per-gametype config files that the mod executed
automatically on a gametype change. Factories replaced them. Generated update
ZIPs may temporarily contain inert `gt-*.cfg` marker files so the v0.60.20
updater accepts the archive; current updaters skip those markers and the Windows
installer does not install them. That older updater cannot replace its own
running executable and installs every accepted config name, so preserve any
custom legacy files and use the current Windows installer once (or replace the
closed updater manually) to move onto the self-updating path.

- The compatibility markers do nothing. Replace `exec gt-FFA.cfg` and friends with
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
