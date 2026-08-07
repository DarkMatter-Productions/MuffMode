# MuffMode Configuration Reference

[README](../README.md) | [Player Guide](player-guide.md) | [Server Host Guide](server-host-guide.md) | [Gameplay Reference](gameplay-reference.md) | [Rulesets](rulesets.md)

This is the lookup document for MuffMode commands, cvars, vote options, and factory behavior. It is mainly for server hosts, admins, and competitive organizers who already know what setting they want to change. Players should start with the [Player Guide](player-guide.md); hosts should start with the [Server Host Guide](server-host-guide.md).

## Admin Commands

Use commands in the form `command [arg]`.

| Command | Purpose |
| --- | --- |
| `admin` | Authenticate or use admin functionality, depending on server setup. |
| `startmatch` | Force match start when warmup conditions apply. In MuffMode Arena it targets the admin's selected room and bypasses that room's ready wait. |
| `endmatch` | Force an active match to end. In MuffMode Arena it aborts only the admin's selected room. |
| `resetmatch` | Reset the match to warmup. In MuffMode Arena it resets only the admin's selected room. |
| `map_restart` | Restart the current level/session and apply latched cvar changes. |
| `setmap <map>` | Change to a map in the configured map list. |
| `nextmap` | Force level change to the next map. |
| `gametype <gametype>` | Change gametype and reset the level. |
| `ruleset <q2re|mm|q3a|q2reb|q|qc>` | Change gameplay ruleset. |
| `shuffle` | Shuffle and balance teams, then reset the match. |
| `balance` | Balance teams without a shuffle. |
| `setteam <player> [auto\|red\|blue\|spectator]` | Inspect or force a player team change. |
| `lockteam <red|blue>` | Lock a team from being joined. Captains can lock their own team. |
| `unlockteam <red|blue>` | Unlock a team. Captains can unlock their own team. |
| `readyall` | Force all players ready during ready-up warmup; in MuffMode Arena, target the admin's selected room. |
| `unreadyall` | Clear ready status during ready-up warmup; in MuffMode Arena, target the admin's selected room. |
| `vote <yes|no>` | Force-pass or fail a vote when used with admin authority. |
| `forcevote` | Force the current vote result. |
| `spawn <entity> [spawn_args]` | Spawn an entity without requiring cheats. |
| `load_mappool` / dedicated console `sv load_mappool` | Reload the structured pool and validate its configured cycle. |
| `load_mapcycle` / dedicated console `sv load_mapcycle` | Reload only the configured structured cycle. |
| Dedicated console `sv ghost_diag [reset]` | Report ghost capture eligibility/rejections, live reinstatement outcomes, deferred skin synchronization, and game-side reliable-message budget counters. The optional `reset` reports first, then clears the lifetime counters. Engine netchan backlog occupancy is not available through the game API. |
| `loadmotd` | Reload the message of the day file. |
| `doctor` | Print diagnostics for risky or inconsistent cvar combinations. |
| `boot <player>` | Remove a player, depending on server admin configuration. |
| `handicap <player> <weapon> <on|off>` | Apply duel weapon restrictions. |
| `handicap_clear` | Clear duel weapon restrictions. |

## Client Commands

The most useful player-facing commands are documented in the [Player Guide](player-guide.md). This quick list is provided for lookup:

| Area | Commands |
| --- | --- |
| Display | `announcer`, `eskin`, `fm`, `help`, `id`, `infohud`, `kb`, `timer`, `tskin` |
| Match state | `ready`, `notready`, `readyup`, `readyteam`, `forfeit`, `arena timeout`, `arena timein`, `time-out`, `time-in` |
| Team selection | `team auto`, `team red`, `team blue`, `team free`, `team spectator` |
| Voting | `callvote`, `cv`, `vote yes`, `vote no` |
| Server info | `maplist`, `mapinfo`, `motd`, `players`, `stats` |
| Spectating | `follow`, `follownext`, `followprev`, `followview`, `followkiller`, `followleader`, `followpowerup` |
| Hook | `hook`, `unhook` |
| Arena rooms | `arena list`, `arena go`, `arena leave`, `arena status`, `arena settings` |
| Room teams and queue | `arena line`, `arena queue`, `arena create`, `arena join`, `arena teamleave`, `arena ready` |
| Room chat | `say_arena`, `say_world`, `arena say`, `arena say_team`, `arena say_world` |
| Reconnect recovery | `ghost <code>` |
| Captains | `captain`, `teamcaptain`, `teamname`, `teamlock`, `teamunlock`, `teamkick`, `teammute`, `teamunmute` |

### Arena Room Commands

The `arena` dispatcher is available only while the Arena gametype is active on
a validated Arena-compatible map. Tagged multi-room maps use positive room IDs
and reserve arena 0 for the lobby; a classic one-room idmap uses the legacy
profile described below. The normal Multiplayer menu exposes **Browse Rooms**
or **Change Room**, **Teams & Queue**, and **Return to Lobby** without
replacing its standard Follow, Player Config, Vote, Stats, Server, Match, and
Admin entries. MuffMode's existing `team`, `captain`, `lockteam`, `unlockteam`,
`ready`, `notready`, `readyup`, `readyteam`, `vote`, `time-out`, and `time-in`
commands are room-aware and remain the native shortcuts. Convenience forms
under `arena` use the same room-local state and shared command policies.

| Command | Purpose |
| --- | --- |
| `arena` | Show the selected room's status/settings, or list rooms from the lobby. |
| `arena list` | List every discovered room, its name/type, population, and state. |
| `arena go <id>` | Enter or observe a playable room. Selector teleporters and the join menu provide the same navigation. |
| `arena leave` | Leave the current room and return to the lobby. |
| `arena status` / `arena settings` | Show the current room state or its effective settings. |
| `arena line [on\|off]` / `arena queue` | Join or leave a Rocket Arena-type room's queue, or inspect that queue. `line` is retained as the historical command spelling. |
| `arena create [name]` | Create a logical team in the current room. |
| `arena join <team-id\|player\|red\|blue> [password]` | Join a named/logical team or a fixed red/blue side in the current room. |
| `arena teamleave` | Leave the current logical team without leaving the room. |
| `arena name <name>` / `arena captain [player]` | In competition mode, rename the team or inspect/transfer its captain role. |
| `arena lock [password]` / `arena unlock` | In competition mode, control entry to the logical team. Administrators may explicitly override this restriction outside competition mode. Passwords are a MuffMode extension. |
| `arena kick [player]` | In competition mode, list team members or let the captain remove one. |
| `arena teammute` / `arena teamunmute` | In competition mode, the captain can restrict noncaptains to team chat or restore their room/world chat. `arena mute` / `arena unmute` are aliases. |
| `arena invite <player>` / `arena revoke <player>` | In competition mode, grant or revoke access to a locked logical team. This is a MuffMode extension. |
| `arena specinvite <player> [coach]` / `arena specrevoke <player>` | In competition mode, let any non-coach team member grant or revoke same-room private spectating. Coaching is a MuffMode extension. |
| `arena coach <team\|player>` / `arena specwho` | In competition mode, choose a coached team or list the caller's team spectators/coaches and outstanding invitations. |
| `arena ready [0\|1]` | In competition mode, toggle ready state or set it explicitly. |
| `arena propose <key> <value>` / `arena vote <yes\|no>` | Start or answer a room-local settings ballot. |
| `arena timeout` / `arena timein` | Pause using the active side's competition allowance, or resume a timeout called by that same side. |
| `arena say <message>` / `arena say_team <message>` / `arena say_world <message>` | Send room, logical-team, or map-wide chat. |
| `arena admin <arena> <setting\|reset\|start\|abort> [value]` | Administer one room without changing the others. |

The RA3-era commands `teamlock`, `teamunlock`, `teamcaptain`, `teamname`,
`teamkick`, `teammute`, `teamunmute`, `specinvite`, `specrevoke`, `specwho`,
`timeout`, and `timein` are registered conveniences for the corresponding
room-local operations when the client and engine forward those tokens to the
game DLL. MuffMode's older `lockteam`, `unlockteam`, `captain`, `time-out`, and
`time-in` spellings remain available and are the portable forms. Q2RE owns some
client-console tokens locally, notably `timeout`; use `arena timeout` or
`time-out` in that case. These aliases preserve familiar input, but do not
replace MuffMode's own team, room, and queue controls.

`say_arena <message>` is the portable direct arena-chat command, including on
KEX clients where the engine owns ordinary `say`. It becomes world chat in the
lobby. `arena say_team <message>` is the portable logical-team channel. KEX
also owns bare `say_team`, so that form follows the projected engine red/blue
team and cannot apply MuffMode's room-local logical-team filtering.
`say_world` is always map-wide.

MuffMode stores each server-side version-2 player profile in `baseq2/pcfg/profiles/sid-<encoded-social-id>.json`, using a safe hex encoding of the engine-provided social ID and a directory that cannot collide with WORR's legacy filename scheme. Missing or unusually long social IDs keep session-only state instead of using fallback filenames. The versioned JSON schema records current, original, and up to 16 previous player-name aliases; first-seen, last-seen, and last-updated timestamps; display, audio, follow, and skin preferences; a custom weapon preference order; per-gametype skill ratings and latest rating changes; and aggregate match totals for wins, losses, draws, abandons, play time, and best rating. Profile data never grants administrator or ban authority.

The JSON profile in `baseq2/pcfg/profiles` is the canonical file. When it is missing, MuffMode performs a one-time import from either the previous root-level `baseq2/pcfg/sid-<encoded-social-id>.json` location or a matching WORR profile that uses WORR's older sanitized-social-ID `.json` filename, then publishes the migrated version-2 profile in the canonical directory. A legacy document's full `socialID` must exactly match the authenticated identity, so sanitizer collisions are never trusted. When the canonical path is absent and no usable legacy JSON profile can migrate, MuffMode can seed the new profile from the older `baseq2/pcfg/sid-<encoded-social-id>.cfg` preference file; subsequent saves update JSON. Corrupt canonical JSON and canonical files whose stored identity does not match the authenticated social ID are quarantined before recovery without importing a stale `.cfg` over them. Profiles with a newer unsupported schema are left untouched rather than guessed at. Retained clients reload transactionally across map and gametype changes, so an unreadable profile cannot erase the live session state. A client whose current profile could not be loaded remains playable with session-only settings, but persistence stays disabled and any match containing that player is unranked until a later successful load.

Writes use unique temporary files, durable atomic replacement, and a bounded per-profile interprocess lock. Accepted preference changes are coalesced and debounced in bounded memory, then persisted by a fair frame pump. Each normal save merges only the changed preference fields into the latest profile document and preserves unknown extension keys, so delayed work from one connection cannot overwrite an unrelated setting written elsewhere; if the document must be recreated, the complete trusted pending preference snapshot is restored instead of defaulting untouched settings. Failed attempts retry with backoff, merge into later profile work, and use an exact-generation check so an older delayed writer cannot clear newer choices. Match-result failures retain the exact computed result in per-identity FIFO order and retry with bounded backoff and fair per-frame work instead of recalculating Elo. Settlement admission is also bounded: if the server cannot guarantee queue space for every required profile result, the match becomes sticky-unranked and no Elo update is applied. Non-Duel departures likewise make the whole match sticky-unranked so an early quitter can never receive Elo before a later bot, failed profile load, or persistence-capacity failure invalidates the remaining result; an exact two-player Duel forfeit still settles both sides atomically and ends immediately.

During reconnect recovery, the reserved gameplay snapshot remains authoritative for settling and exporting the match that was already in progress. Once that match closes, MuffMode reconciles the admitted result onto the reconnecting client's current profile state. A successfully loaded reconnect profile supplies the next match's preference base; if that load failed, the trusted reserved preferences and weapon order remain in place. Changes made during the reinstatement delay are layered over either base.

The in-game **Player Config** menu exposes the same saved preferences through separate Display & Audio, Spectator & Follow, and Skin Overrides pages. Inventory-menu controls remain available during intermission so Player Config and Player Stats can be reviewed after the match. The optional voice announcer defaults to off and can be enabled with `announcer on` or from Display & Audio; stock fallback cues remain available where defined. Free-form skin paths are still entered with `eskin <model/skin>` and `tskin <model/skin>`.

## Match Statistics Exports

Completed singleton matches in the normal WORR-supported gametypes can produce a versioned structured record at `baseq2/matches/<sanitized-match-id>.json`, an optional companion HTML report at the same stem, and the atomic `baseq2/matches/catalog.json` artifact index. Every intermission path freezes one exact result before client state can change. The match record includes server and listen-host attribution, match and team totals, every participant including players who departed before the end, the settled win/loss/draw/abandon/no-contest result, rating results, weapon and damage statistics, deaths and spawns, item timing, medals, CTF actions, and bounded event and death logs with explicit truncation markers. Serialization and writes normally run in the background, report each success or failure, and fall back to a synchronous write if the bounded queue is full. JSON and catalog publication are the required pair; an HTML failure is reported but does not discard a valid JSON match. Catalog access is interprocess-locked, strictly size- and structure-bounded, quarantines malformed data, keeps `latest` chronological even when jobs complete out of order, and retains at most 4,096 artifact entries within a 16 MiB catalog; the oldest entries are pruned first without deleting their standalone match files.

Arena Rooms run multiple independent room series at once, so they remain unranked and are not exported; their live Player Stats menu continues to use room-local counters. For the same reason, MuffMode's `MATCH_LOGGING_STATUS_API_V1` smoke status validates the singleton match schema and its real atomic catalog write path, but does not advertise WORR tournament-series fields.

These artifacts are controlled by the `g_statex_*` cvars below. They are independent of `g_matchstats`, which controls only the live in-game match-statistics menu.

## Vote Commands

Use `callvote <command> [arg]` or `cv <command> [arg]`.

| Command | Argument | Purpose |
| --- | --- | --- |
| `map` | `<mapname>` | Change to a specific map. |
| `nextmap` | none | Move to the next map in rotation. |
| `restart` | none | Restart the current match. |
| `gametype` | `<gametype>` | Change gametype. |
| `timelimit` | `<0..1440>` | Change match time limit in minutes; `0` disables. |
| `scorelimit` | `<0..>` | Change score limit; `0` disables. |
| `fraglimit` | `<0..>` | Alias for score limit. |
| `shuffle` | none | Shuffle teams. |
| `balance` | none | Balance teams without shuffling. |
| `unlagged` | `<0|1>` | Toggle lag compensation. |
| `cointoss` | none | Return heads or tails. |
| `random` | `<2-100>` | Return a random number from `2` to the provided value. |
| `ruleset` | `<q2re|mm|q3a|q2reb|q|qc>` | Change ruleset. |
| `powerups` | `<0|1>` | Disable or enable powerups. |
| `friendlyfire` | `<0|1>` | Disable or enable friendly fire in team modes. |
| `techs` | `<0|1>` | Disable or enable techs (FFA/TDM/CTF/Horde only). |
| `handicap` | `<player> <weapon> <on|off>` | Restrict duel weapons for a player. Weapons: `railgun`, `chaingun`, `rlauncher`, or `all`. |
| `readyall` | none | Ready all players during ready-up warmup. |
| `factory` | `<factory>` | Change the active factory preset. See [Factories](#factories). |

## Vote Flags

`g_vote_flags` is a bitmask. Add values together to disable multiple vote commands.

| Value | Disables |
| --- | --- |
| `1` | `map` |
| `2` | `nextmap` |
| `4` | `restart` |
| `8` | `gametype` |
| `16` | `timelimit` |
| `32` | `scorelimit` and `fraglimit` |
| `64` | `shuffle` |
| `128` | `unlagged` |
| `256` | `cointoss` |
| `512` | `random` |
| `1024` | `balance` |
| `2048` | `ruleset` |
| `4096` | `powerups` |
| `8192` | `friendlyfire` |
| `16384` | `handicap` |
| `32768` | `readyall` |
| `65536` | `techs` |
| `131072` | `factory` |

## Gametype Values

| Value | Short name | Gametype |
| --- | --- | --- |
| `1` | `ffa` | Deathmatch |
| `2` | `duel` | Duel |
| `3` | `tdm` | Team Deathmatch |
| `4` | `ctf` | Capture the Flag |
| `5` | `ca` | Clan Arena |
| `6` | `ft` | Freeze Tag |
| `7` | `strike` | Capture Strike |
| `8` | `rr` | Red Rover |
| `9` | `lms` | Last Man Standing |
| `10` | `horde` | Horde Mode |
| `14` | `arena` | Arena Rooms (Rocket Arena) |

Values `11` (`ball`), `12` (`instagib`) and `13` (`nadefest`) are not selectable
in the current build; setting one falls back to Deathmatch with a console
notice. Instagib and NadeFest are modifiers rather than gametypes -- they are
`g_instagib` and `g_nadefest`, applied on top of whichever mode is running, and
the shipped `factories.cfg` provides them (and their combinations with the team
and round modes) as named presets. The ids are retained so `arena` keeps id
`14` and existing configs do not renumber.

## Factories

A factory is a named gameplay preset: one base gametype plus a set of cvar
overrides and structured directives. **Factories are how a MuffMode server is
configured for play.** They replaced the per-gametype `gt-*.cfg` presets the mod
used to execute automatically, so nothing runs underneath a factory — a setting
no factory states holds whatever `server-base.cfg` left in it.

Factories exist so one server can offer several flavours of the same gametype —
classic CTF and Insta CTF, a competition Clan Arena and a casual one —
selectable by name, by an admin, from the server console, or by vote.

Definitions live in `baseq2/factories.cfg`. The shipped catalogue provides 58
selectable presets — a classic and a competitive variant of each gametype, plus
the Instagib, NadeFest, Vampiric, Frenzy and Quad Hog modifier presets for the
modes where each makes sense (`instagib`, `instactf`, `instaca`, `vampffa`,
`vampca`, `frenzyctf`, `nadefest`, and so on) — layered on eleven hidden
`_base_<mode>` factories, one per gametype, that carry each mode's baseline, plus
two hidden test entries. 71 definitions in all.
Run `factory list all` to see every id and `factory info <id>` to see exactly
what one changes. Set `g_factory_file` to load a different file, or several,
separated by spaces.

Instagib and NadeFest used to be gametypes. They are modifiers now — applied on
top of whichever mode is running — so they exist as factories rather than as
`g_gametype` values. That is what makes Insta CTF and NadeFest Clan Arena
expressible at all.

### Writing a factory

```
factory ctf_classic {
    title    "Capture the Flag"
    desc     "Classic CTF with grapple and techs."
    base     ctf
    ruleset  q2re
    inherit  _base_ctf

    maps     q2ctf1 q2ctf2 q2ctf3 q2ctf4 q2ctf5
    mappool  q2ctf1 q2ctf2 q2ctf3
    rotation shuffle-per-gametype
    players  4 16

    set capturelimit 8
    set g_allow_grapple 1
}
```

| Key | Required | Meaning |
| --- | --- | --- |
| `factory <id>` | yes | 1–31 characters of `a-z 0-9 _ -`. An id beginning with `_` is hidden from listings and from the vote menu, but can still be selected by name. The subcommand words (`all`, `clear`, `cvars`, `diag`, `info`, `list`, `none`, `reload`) are rejected, since a factory with one of those ids could never be selected. |
| `title` | yes | Display name, published in serverinfo. Under 48 characters; no `\`, `;` or `"`. |
| `desc` (or `description`) | no | One line shown by `factory info`. |
| `base` (or `basegt`) | yes | Base gametype short name: `ffa duel tdm ctf ca ft strike rr lms horde arena`. |
| `ruleset` | no | `q2re`, `mm`, `q3a`, `q2reb`, `q` or `qc`. |
| `inherit` | no | An earlier factory id with the **same** base gametype; its overrides and directives are the starting point. |
| `maps` | no | The map rotation. Repeat the key to add more; a rotation is usually longer than one line allows. |
| `mappool` | no | The maps offered to map votes and MyMap. |
| `rotation` | no | `sequential`, `shuffle-on-wrap` or `shuffle-per-gametype`. |
| `players` | no | `<min> <max>` active players for this mode. |
| `set <cvar> <value>` | no | Override an allowlisted cvar. Use `""` to clear one. |

`maps`, `mappool`, `rotation` and `players` are directives rather than plain
`set` lines because they carry structure a bare string cannot: a map token is
interpolated straight into a `gamemap` command, so it goes through the same
validation the structured map system applies everywhere else, and the player
limits are range-checked against each other. A rotation is also routinely longer
than the value length a `set` line permits.

When a child factory names its own `maps`, it *replaces* the inherited rotation
rather than extending it — otherwise an Insta CTF variant would inherit the
deathmatch rotation it was trying to override. Directives the child does not
name are inherited.

### Layering

Each authority writes at a different moment, and that ordering *is* the
precedence — there is no runtime arbiter to disagree with:

| Order | Authority | When it writes |
| --- | --- | --- |
| 1 | Compiled defaults | cvar registration at level start |
| 2 | `server-base.cfg` and your own autoexec | server start |
| 3 | **Factory overrides and directives** | immediately before the map load that follows a gametype or factory change |
| 4 | Map worldspawn and Arena room config | map spawn / room setup |
| 5 | Admin cvar write or a passed vote | immediately, and it stays until something writes the cvar again — a factory only re-asserts its own settings on a gametype or factory change |

`server-base.cfg` keeps ownership of *server scope* — `hostname`, `maxclients`,
admin and vote policy — and supplies the baseline a factory layers on top of. A
factory can never reach those, deliberately: a vote must not be able to rename
your server or resize its slab.

### Restoring on a switch

Every cvar a factory writes is recorded with the value it had beforehand.
Selecting a different factory restores every setting the outgoing factory
changed and the incoming one does not re-assert, then applies the incoming
overrides. Selecting no factory (`factory none`) restores all of them.

Restoration goes back to the value the *server's own configuration* produced —
what `server-base.cfg` left behind — not to a compiled default, because that is
the value the operator actually asked for. This is why `server-base.cfg` states
the mutators explicitly even though they are off by default: stating a value is
what establishes the value a mutator factory restores to.

### Which cvars a factory may set

Factories may only `set` cvars on a fixed allowlist of 221 names. This is what
makes the restore exact: the system can only give back settings it knows it
owns. It also bounds the blast radius of an operator-authored file — server
identity, the client slot allocation, authentication, and every
`sv_`/`cl_`/`fs_`/`net_`/`in_`/`bot_` cvar are unreachable, and a factory can
never create a cvar.

| Category | Count | Covers |
| --- | --- | --- |
| Match limits and flow | 12 | `fraglimit`, `timelimit`, `capturelimit`, `roundlimit`, `roundtimelimit`, `mercylimit`, `g_dm_overtime`, `g_match_lock`, `g_round_countdown`, `g_dm_do_warmup`, `g_warmup_countdown`, `g_warmup_ready_percentage` |
| Mutators | 9 | `g_instagib`, `g_instagib_splash`, `g_nadefest`, `g_quadhog`, `g_frenzy`, and the four `g_vampiric_*` |
| Loadout, items, damage, movement, teams | 35 | `g_start_items`, `g_starting_health_bonus`, `g_infinite_ammo`, `g_dm_weapons_stay`, `g_weapon_respawn_time`, `g_no_powerups`, `g_knockback_scale`, `g_damage_scale`, `g_dm_no_fall_damage`, `g_friendly_fire`, `g_teamplay_force_balance`, `g_lms_lives`, and similar |
| CTF and techs | 4 | `g_allow_grapple`, `g_allow_techs`, `g_grapple_damage`, `g_grapple_offhand` |
| Arena rooms | 30 | every `g_arena_*` |
| Freeze Tag | 14 | every `g_freezetag_*` |
| Horde | 117 | every `g_horde_*` |

Weapon damage, spread, pellet counts and projectile speeds are the **ruleset's**
numbers and are deliberately absent. A factory selects a ruleset by name with
`ruleset <q2re|mm|q3a|q2reb|q|qc>` and never overrides its numbers. See
[docs/rulesets.md](rulesets.md). The map rotation and player limits are absent
for the opposite reason: they are directives, with validation a `set` cannot do.

`factory info <id>` prints exactly which of these a factory sets; a `set` naming
anything else is rejected by file, line and reason when the registry loads.

### When an override takes effect

Selecting a factory always triggers a map change, and everything it carries is
written before that map load — so every override lands at a clean boundary
regardless of its kind. `factory reload` and `sv gt_apply` re-apply only the
settings that can take effect mid-map, and report how many were deferred:

- **Immediate** — read fresh at the point of use; applies at once.
- **Map load** — captured during map spawn; applies at the next map load.
- **Latched** (`g_instagib`, `g_nadefest`, `g_quadhog`, `g_frenzy`,
  `g_infinite_ammo`, `g_quick_weapon_switch`, and a few others) — these are
  `CVAR_LATCH` because the world they describe is built at map spawn. Writing
  one mid-map moves the cvar but not the item layout or loadout it governs, so a
  factory treats them as map-load settings and says so rather than reporting a
  change nobody can see.

`factory diag <cvar>` reports an allowlisted cvar's current value, who set it,
what it will be restored to, and which of the three classes it falls into.

### Failure handling

A factory file is parsed whole before any of it is used, and the new registry
replaces the old one only if the file parsed — a bad edit leaves the working
registry in place, so it cannot leave a server with no factories. A malformed
definition is rejected on its own, named by file, line and reason, and the rest
of the file still loads. An unknown `g_factory` selects nothing, clears itself,
and lists what was available; it never guesses. A factory whose `base` does not
match the configured gametype is discarded with a console notice rather than
silently switching the mode out from under the operator. A factory file contains
no console commands, so there is nothing to expand.

### Host permissions

Everything a player can do to the mode is gated by cvars the operator sets in
`server-base.cfg`, which no factory and no vote can reach:

| Cvar | Effect |
| --- | --- |
| `g_votable_factories` | Space-separated ids players may vote for. Empty allows every non-hidden factory. |
| `g_vote_flags` | Add `131072` to disable factory votes entirely. |
| `g_votable_gametypes` | Which gametypes may be voted directly. |
| `g_allow_voting`, `g_allow_vote_midgame` | Whether votes run at all. |
| `g_allow_admin` | Whether players may authenticate as admin and use `factory <id>`. |
| `g_gametype_locked` | `1` pins the gametype and factory: admin commands and passed votes are both refused with `selection locked`. The server console and rcon still get through, so an operator cannot lock themselves out. |

### Cvars

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_factory` | `""` | Active factory id. Empty means no factory: the plain gametype runs on your `server-base.cfg` values. Carried across a map change. |
| `g_factory_title` | `""` | Read-only. The active factory's display title, published in serverinfo. |
| `g_factory_file` | `factories.cfg` | Space-separated leaf names under `baseq2/` to load the registry from. Latched: a change takes effect at the next server start, or immediately on `factory reload`. Later files override earlier ones by id. |
| `g_votable_factories` | `""` | Space-separated ids players may vote for. Empty allows every non-hidden factory. |
| `g_gametype_locked` | `0` | `1` refuses every gametype and factory change from an admin command or a passed vote. The server console is exempt. |

### Commands

| Command | Access | Purpose |
| --- | --- | --- |
| `factory` | all | Show the active factory and the ones available for the current gametype. |
| `factory list [gametype\|all]` | all | List factory ids, optionally for one gametype. |
| `factory info <id>` | all | Show a factory's title, base gametype, source file and every setting it changes. |
| `factory cvars [prefix]` | all | List the cvars a factory may set, optionally filtered by prefix (`factory cvars g_arena`). |
| `factory diag <cvar>` | all | Show where an allowlisted setting's current value came from and what it restores to. |
| `factory <id>` | admin | Select a factory. Triggers a map change. |
| `factory none` | admin | Clear the factory and restore everything it changed. |
| `factory reload` | admin | Rebuild the registry from disk and re-apply what can take effect now. |
| `sv factory <id\|none>` | console | Select or clear a factory from a dedicated server console. Exempt from `g_gametype_locked`. |
| `callvote factory <id>` | all | Vote to change factory (vote flag `131072`). |
| `sv gt_apply` | console | Server-console equivalent of `factory reload`. |

Ids beginning with `_` are hidden from listings and from the vote menu, but can
still be selected by name.

## Ruleset Values

For player-facing differences between these options, see the [Rulesets](rulesets.md) guide.

| Value | Short name | Ruleset |
| --- | --- | --- |
| `1` | `q2re` | Quake II Rerelease |
| `2` | `mm` | Muff Mode |
| `3` | `q3a` | Quake III Arena style |
| `4` | `q2reb` | Q2RE Balanced |
| `5` | `q` | Quake style |
| `6` | `qc` | Quake Champions style |

## Cvar Changes

Deathmatch respawns use a WORR-style danger score instead of raw farthest-only modes. Spawn selection avoids blocked points, recent combat heat, direct enemy line of sight, nearby players, the player's previous spawn point, and nearby mines or traps. `g_dm_spawn_farthest` is retained for legacy config compatibility, while `g_dm_respawn_point_min_dist` controls hard spacing from the previous spawn and nearby players.

- `g_teamplay_force_join` was renamed to `g_dm_force_join`.
- Mod-based `sv_*` cvars were renamed to `g_*`.
- `g_teleporter_nofreeze` was renamed to `g_teleporter_freeze`, with reversed meaning.
- `deathmatch` defaults to `1`.

## Core Cvars

| Cvar | Default | Purpose |
| --- | --- | --- |
| `hostname` | `Welcome to Muff Mode!` | Server name shown in menus. Keep it short for display. |
| `maxclients` | engine default | Connected client slots allocated by the engine and game. MuffMode clamps this to `1..128` during `PreInit`, before the engine sizes its client slab. |
| `maxplayers` | `16` | Maximum active players; capped to the allocated `game.maxclients`. Spectators may occupy the remaining connected slots. |
| `minplayers` | `2` | Minimum active players. |
| `deathmatch` | `1` | Enables deathmatch mode. |
| `g_gametype` | `1` | Current gametype index. |
| `g_ruleset` | `1` | Current ruleset index. |
| `timelimit` | `0` | Match time limit in minutes. |
| `fraglimit` | `0` | Frag limit where applicable. |
| `capturelimit` | `8` | Capture or objective limit where applicable; Capture Strike applies its own default. |
| `roundlimit` | `8` | Round wins needed in round-based gametypes. In Horde, this caps the number of waves; set to `0` for endless Horde (see [Horde Late-Wave & Endless](#horde-late-wave--endless)). |
| `roundtimelimit` | `2` | Round time limit in minutes. |
| `mercylimit` | `0` | Score gap to end match; `0` disables. |
| `noplayerstime` | `10` | Minutes with no players before forcing a map change; `0` disables. |

The KEX lobby capacity is separate from `maxclients`. Packaged servers request
the same value with `kexmultiplayer maxplayers`, because the stock game-module
API does not expose the active lobby provider's limit. Keep the two configured
values aligned, but expect a provider or engine build to enforce a lower
service-specific ceiling. The shared Quake II protocol limit remains 256;
MuffMode's supported connected-client ceiling is 128.

## Access And Player Policy

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_allow_admin` | `1` | Allows admin powers. |
| `admin_password` | empty | Exact, case-sensitive password accepted by `admin <password>`. Empty disables password authentication. Attempts share the client flood history and settings; `flood_msgs <= 0` disables this throttle. |
| `g_allow_custom_skins` | `1` | Allows custom player models and skins. |
| `g_allow_skin_overrides` | `1` | Allows players to re-skin enemies/teammates on their own screen via the `eskin`/`tskin` commands (team games; in duel, `eskin` re-skins your opponent). |
| `g_allow_forfeit` | `1` | Allows Duel forfeits. |
| `g_allow_grapple` | `auto` | Controls normal grapple availability. `auto` follows mode defaults; `0` disables; `1` enables. |
| `g_allow_kill` | `1` | Allows the `kill` suicide command. |
| `g_allow_mymap` | `1` | Allows MyMap queueing. |
| `g_allow_spec_vote` | `0` | Allows spectators to vote. |
| `g_allow_techs` | `auto` | Controls tech pickups in FFA/TDM/CTF/Horde. `auto` enables techs by default in CTF and Horde (off in FFA/TDM); votes can force `0` or `1` in any of those modes. |
| `g_allow_vote_midgame` | `0` | Allows votes during active matches. |
| `g_gametype_locked` | `0` | Pins the gametype and factory. `1` refuses every gametype or factory change from an admin command or a passed vote; the server console and rcon are exempt so an operator cannot lock themselves out. See [Factories](#factories). |
| `g_allow_voting` | `1` | Enables voting globally. |
| `flood_msgs` | `4` | Flood-controlled client actions (including chat, authentication attempts, gestures/pings, `motd`, and `mymap`) allowed within the shared window; values above the ten-entry history capacity are clamped to `10`, and values at or below `0` disable this protection. |
| `flood_persecond` | `4` | Length of the shared client flood-detection window in seconds. |
| `flood_waitdelay` | `10` | Seconds a client must wait after triggering the shared client flood protection. |
| `g_inactivity` | `120` | Seconds before inactive players are moved to spectators. |
| `g_match_lock` | `0` | Prevents joining while a match is active. |
| `g_owner_auto_join` | `1` | Auto-joins lobby owner on server start. |
| `g_owner_push_scores` | `0` | Shows scores to lobby owner on join. |

## Match Cvars

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_dm_allow_no_humans` | `1` | Allows matches with only bots. |
| `g_dm_death_scoreboard` | `1` | Automatically opens the scoreboard when a player dies in deathmatch. |
| `g_dm_auto_join` | `0` | Automatically joins players into the active play pool when allowed by the current mode. |
| `g_dm_do_warmup` | `1` | Enables match warmup. |
| `g_dm_do_readyup` | `0` | Requires ready-up during warmup. |
| `g_dm_force_join` | `0` | Forces players to join instead of staying spectator, depending on mode. |
| `g_dm_force_respawn` | `1` | Forces respawn after death when the mode allows it. |
| `g_dm_force_respawn_time` | `3` | Seconds before forced respawn. |
| `g_dm_intermission_shots` | `0` | Allows players to continue firing during the brief intermission pre-delay. |
| `g_dm_overtime` | `120` | Overtime session length in seconds. |
| `g_dm_tie_max_time` | `1800` | Maximum total tied-overtime duration. |
| `g_dm_respawn_delay_min` | `1` | Minimum delay after death before respawn. |
| `g_dm_respawn_point_min_dist` | `256` | Minimum respawn distance from the previous spawn point and nearby players. |
| `g_dm_respawn_point_min_dist_debug` | `0` | Prints spawn avoidance debug information. |
| `g_dm_spawn_farthest` | `1` | Legacy spawn-mode compatibility cvar; respawns use combat-aware scoring. |
| `g_dm_spawnpads` | `1` | Controls deathmatch spawn pads. |
| `g_auto_ghost_time` | `120` | Seconds an auto-ghost reservation remains available, up to `3600`; `0` disables auto-ghost capture. |
| `g_auto_ghost_max` | `3` | Maximum active auto-ghost reservations, capped by client capacity; `0` disables auto-ghost capture. |
| `g_auto_ghost_timeout` | `0` | Auto-pauses an active match for disconnected players, in seconds capped by `g_auto_ghost_time`; `0` disables. |
| `g_dm_timeout_length` | `120` | Timeout length in seconds; `0` disables timeouts. |
| `g_dm_timeout_resume_countdown` | `30` | Countdown announced before a paused match resumes, in seconds up to `120`; `0` resumes immediately. |
| `g_round_countdown` | `10` | Round countdown time. |
| `g_warmup_countdown` | `10` | Warmup countdown time. |
| `g_warmup_ready_percentage` | `0.51f` | Ready percentage required to start. |

## Team Cvars

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_friendly_fire` | `0` | Enables friendly fire. |
| `g_team_force_models` | `0` | Forces team player models/skins when enabled. |
| `g_team_red_model` | `male/ctf_r` | Model/skin used when red team models are forced. |
| `g_team_blue_model` | `female/ctf_b` | Model/skin used when blue team models are forced. |
| `g_teamplay_allow_team_pick` | `0` | Allows players to choose teams directly. |
| `g_teamplay_armor_protect` | `0` | Enables teamplay armor-protection behavior. |
| `g_teamplay_auto_balance` | `1` | Rebalances teams during matches. |
| `g_teamplay_force_balance` | `0` | Prevents joining over-stacked teams. |
| `g_teamplay_item_drop_notice` | `1` | Announces item drops to teammates. |

## Arena Room Cvars

Arena supports up to 31 independent playable rooms plus arena 0, the lobby, on
a tagged map. These cvars form the global default layer; `arena.cfg` can
override them by map and room.

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_arena_config` | `arena.cfg` | Latched configuration file resolved beneath `basedir/baseq2`; missing files are harmless and reported before built-in/cvar defaults are used. |
| `g_arena_legacy_idmap` | `0` | Latched compatibility switch for an otherwise untagged classic RA2 idmap. Explicit `worldspawn` `arena 0` uses the legacy profile without this switch; set `1` before the next map load only when importing untagged idmaps. In this one-room profile, any entity `arena` tags are treated as shared. |
| `g_arena_default_type` | `rocket` | Default room type: `rocket`, `clan`, `rover`, or `practice`. |
| `g_arena_players_per_team` | `1` | Default team size, clamped from `1` through half of `maxclients`. |
| `g_arena_rounds` | `1` | Default room best-of length, normalized to an odd value from `1` through `99`. |
| `g_arena_start_health` | `200` (`100` in the shipped Arena factories) | Shared arena-loadout starting-health default. The shipped Arena factories select the classic 100-health value without changing Freeze Tag's default. |
| `g_arena_start_armor` | `200` (`100` in the shipped Arena factories) | Shared arena-loadout armor; the shipped Arena factories select the traditional 100-armor value without changing Freeze Tag's default. |
| `g_arena_health_protect` | `1` | Health protection: `0` damages all, `1` protects self and teammates, `2` protects teammates but permits self damage. |
| `g_arena_armor_protect` | `2` | Armor protection using the same `0`/`1`/`2` modes. The default permits self-armor damage while protecting teammates. |
| `g_arena_falling_damage` | `1` | Default falling-damage behavior. |
| `g_arena_weapon_mask` | `255` | Default spawn-weapon mask; `255` enables the standard non-BFG set. |
| `g_arena_ammo_shells` | `100` | Starting shells. |
| `g_arena_ammo_bullets` | `200` | Starting bullets. |
| `g_arena_ammo_grenades` | `20` | Starting grenades. |
| `g_arena_ammo_rockets` | `50` | Starting rockets. |
| `g_arena_ammo_cells` | `150` | Starting cells. |
| `g_arena_ammo_slugs` | `50` | Starting slugs. |
| `g_arena_fast_switch` | `1` | Enables accelerated weapon switching. |
| `g_arena_grapple` | `0` | Grants the room-scoped selectable Grapple and enables the offhand `+hook`; it is independent of the global grapple cvars. |
| `g_arena_excessive` | `0` | Enables rapid fire, faster rockets, and infinite ammo with matching HUD reporting. |
| `g_arena_rocket_speed` | `900` | Default room rocket speed. |
| `g_arena_competition` | `0` | Requires the per-room ready/competition flow. |
| `g_arena_warmup_readyup` | `1` | Requires the per-room MuffMode warmup ready-up even when competition mode is off. With `0` a room starts as soon as two eligible sides pair up, and `ready` is rejected. |
| `g_arena_unbalanced` | `0` | Allows unequal team sizes. |
| `g_arena_lock` | `0` | Starts playable rooms locked to new entrants. |
| `g_arena_lock_count` | `6` | Minimum eligible population for the unanimous special lock-arena proposal. It does not lock entry by itself. |
| `g_arena_max_players` | `0` | Per-room player cap; `0` uses the available server capacity. |
| `g_arena_vote_time` | `30` | Seconds allowed for a room-local proposal. |
| `g_arena_timeouts` | `3` | Competition timeouts available to each side. |

Each room runs its own warmup rather than sharing the level-wide match state, so
several rooms can be waiting for players, balancing, or counting down at the same
time. A room reports what it is waiting on through the HUD (`WARMUP - NEED
PLAYERS`, `WARMUP - UNBALANCED`, `WARMUP (n/m READY)`), a centerprint every 30
seconds, and the scoreboard status line. Room warmup reuses the shared warmup
cvars: `minplayers` for the player floor, `g_warmup_ready_percentage` for the
ready threshold, `g_dm_allow_no_humans` to permit a bots-only room, and
`g_warmup_countdown` for the first countdown of a series while `g_round_countdown`
covers each later round. Both countdowns clamp to 1-30 seconds.

Arena bots choose their own room: they follow players into an occupied room,
prefer one where somebody is waiting for an opponent, and take an opposing team,
creating one when a lone player has nobody to fight. To populate rooms
automatically, set `bot_minClients` in your own server config -- it is an
engine-owned `bot_` cvar, so a factory cannot set it.

Arena timeouts use the existing `g_dm_timeout_length` duration and
`g_dm_timeout_resume_countdown` time-in countdown. `g_arena_timeouts` remains
separate because its per-side allowance is unique to room competition. The
shipped `arena_ra2` factory selects `60` seconds and a five-second time-in.

`g_arena_weapon_mask` is the sum of the enabled weapon bits:

| Bit | Weapon |
| --- | --- |
| `1` | Chainfist (RA3 Gauntlet role) |
| `2` | Machinegun |
| `4` | Shotgun |
| `8` | Grenade Launcher |
| `16` | Rocket Launcher |
| `32` | Plasma Beam (RA3 Lightning Gun role) |
| `64` | Railgun |
| `128` | HyperBlaster (RA3 Plasma Gun role) |
| `256` | BFG10K |

The mask is MuffMode's native explicit loadout setting. An empty mask receives
a Chainfist as a safety fallback; otherwise each bit is authoritative.
Unsupported bits are discarded. The compact `weapons` input remains available
for compatibility; its native and imported-RA2 number rows are documented
below. Each ammo value is clamped from `0` through `999`.

### Arena map profiles

**Tagged multi-room is the preferred MuffMode map profile.** Set
`worldspawn`'s explicit `arena` key to the number of playable rooms (`1`
through `31`). Arena 0 is the lobby and needs a finite usable start or
destination. Each declared positive room needs at least two finite tagged
`info_player_deathmatch` starts so both active sides can spawn without sharing
a point. Room-local spawns, observer positions,
teleporters, and triggers carry the same positive `arena` key. A tagged
`info_player_intermission` can supply a room name and observer view, but it is
not required merely to activate a room. A positive tagged `misc_teleporter`
acts as a room selector. Negative legacy values are shared/reserved markers,
not lobby aliases.

**Classic RA2 idmaps use a one-room compatibility profile.** An explicit
`worldspawn` `arena` value of `0` selects that profile and uses the map's
shared deathmatch-start pool as one room; in this profile arena 0 is a
profile marker, not a lobby. A map with no `worldspawn` `arena` key is accepted
as the same legacy profile only when `g_arena_legacy_idmap 1` is latched before
the next map load. The cvar defaults to `0`, so ordinary untagged maps do not
quietly become Arena maps. In either legacy route, entity `arena` tags are
treated as shared inside the one virtual room; they do not create isolated
subrooms.

Classic RA2 observer views prefer a matching `misc_teleporter_dest` and fall
back to a matching deathmatch start when no destination exists; tagged maps
can instead use an intermission view and its `mangle`/angles.

As in the original RA2/RA3 map contract, playable rooms must occupy
BSP-separated visibility/hearing regions. Room tags isolate server gameplay,
but the Quake II protocol still distributes generic snapshots and temporary
effects by PVS/PHS, so arbitrary arenas overlaid at the same coordinates are
not a supported mapping pattern.

Validation runs against the final entity lump before any entity is spawned and
is checked again against the live entity set. If preflight validation fails,
Arena remains inactive and the requested mode is treated as effective FFA; no
room, loadout, isolation, spawn-filter, command, HUD, or room-menu hooks are
enabled. A disagreement after entities have spawned hard-rejects the map
instead of running a partially modified level. The shipped preset leaves
`g_map_list` empty: install your own Arena-compatible maps and configure an
Arena rotation. It leaves the untagged-idmap compatibility switch at `0`; set
`g_arena_legacy_idmap 1` only for a known rotation of untagged classic RA2
idmaps.

### `arena.cfg` layers

Settings resolve in this order: built-in defaults, global cvars and top-level
file settings, matching map block, then matching numeric room block. For
example:

```text
// baseq2/arena.cfg
health 100
armor 100
type rocket

map mm_arena_hub {
    rounds 3

    room 1 {
        type practice
        grapple 1
    }

    room 2 {
        type clan
        playersperteam 3
        competitionmode 1
    }
}
```

`#` and `//` comments and quoted values are supported. Colons and semicolons
from original RA2 files (`health: 100;`) are optional. Native MuffMode configs
make scopes explicit with `map <name>` and `room <id>`; `arena <id>` remains an
accepted compatibility alias. Bare RA2-style `ra2map1 { 1 { ... } }` blocks
and `maploop` are accepted as imported legacy syntax and are detected
automatically. A top-level-only `arena.cfg` defaults to the legacy RA2 weapon
row so unmodified old configs work; add `format: native;` (or `format: ra3;`)
at its top when a global-only MuffMode config needs the RA3 shorthand row.
`format: ra2;` makes that legacy intent explicit. Native `map`/`room` scopes
are already unambiguous.
Map rotation remains owned by MuffMode's existing `g_map_list`, `g_map_pool`,
and `g_map_list_shuffle` settings; `arena.cfg` only resolves room rules.

Recognized aliases include `type`/`gametype`, `weapons`/`weaponmask`, and
`playersperteam`/`ppt`. The full setting set is `type`, `pickup`, `weapons`,
`armor`, `health`, `playersperteam`, `rounds`, `shells`, `bullets`, `slugs`,
`grenades`, `rockets`, `cells`, `plasma`, `bfgammo`, `fastswitch`,
`fallingdamage`, `grapple`, `rocketspeed`, `excessive`, `damagescoring`,
`lockarena`, `competitionmode`,
`unbalanced`, `lockcount`, `maxplayers`, `maxteams`/`max_teams`, `minping`,
`maxping`, `votetries`, `armorprotect`, and `healthprotect`. Zero disables a
player, team, or ping bound. `votetries` defaults to two unsuccessful proposal
attempts per player per room match; a successful room vote restores every
player's allowance, while `0` disables player proposals.
Stock RA3 `gametype: pickup` rooms resolve through the latest layered
`defpickup` value (`clanarena` by default, with `redrover` supported), and
`practicearena` is accepted as the original Practice spelling. The RA2-style
boolean `pickup 1`/`pickup 0` switch remains available; enabled fixed-team
pickup rooms can fill the server capacity and use two teams.

Prefer `weaponmask` for new MuffMode configs. In explicit native config
syntax (or a global file marked `format: native;`), the `weapons:` shorthand
retains the RA3 compatibility row: `1`
Chainfist (Gauntlet), `2` Machinegun, `3` Shotgun, `4` Grenade Launcher, `5`
Rocket Launcher, `6` Plasma Beam (Lightning Gun), `7` Railgun, `8`
HyperBlaster (Plasma Gun), `9` BFG10K, and `0` grapple. Automatically imported
legacy RA2 syntax instead uses the Quake II row: `2`/`3` select the compact
Shotgun role (SG/SSG), `4`/`5` the Machinegun role (MG/Chaingun), `6` Grenade
Launcher, `7` Rocket Launcher, `8` HyperBlaster, `9` Railgun, and `0` BFG10K.
In that imported RA2 form, `1` is ignored and `0` is BFG10K, not grapple.
`allow_voting_*` switches can independently permit or deny ballots for type,
health/armor, team size, rounds, protection, weapons, falling damage,
excessive, locking, competition mode, ping limits, and maximum logical teams.
Original unseparated `allowvoting*` spellings are accepted as compatibility
aliases. The shipped policy disables excessive and grapple ballots by default;
enable them explicitly when appropriate. Starting-ammo values are server
configuration only and cannot be changed by a player ballot.

Quake II uses one cell pool for the HyperBlaster/RA3 plasma role and BFG10K.
When those weapons are enabled, the spawn reserve is therefore the largest of
`cells`, `plasma`, and `bfgammo`.

The optional `roundtimelimit` caps a round. Each room evaluates that timer and
its outcome independently; the shipped Arena factories leave it at `0`, so room rounds are
uncapped by default.

## Map And Rotation Cvars

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_maps_pool_file` | empty | Opt-in structured map-pool JSON leaf filename under `baseq2`; empty keeps legacy map sources active. |
| `g_maps_cycle_file` | empty | Optional structured cycle leaf filename under `baseq2`; requires a valid structured pool. |
| `g_maps_random` | `1` | `1` selects randomly from eligible cycle maps, with `popular` maps weighted twice; `0` follows cycle order. |
| `g_maps_repeat_delay` | `1800` | Preferred seconds before a structured-cycle map repeats; clamped to `0`–`86400` and relaxed if necessary to keep rotation moving. |
| `g_map_list` | empty | Space-separated map rotation. |
| `g_map_list_shuffle` | `1` | `0` disables shuffle, `1` shuffles on wrap, `2` shuffles once per gametype session. |
| `g_map_pick` | `15` | Seconds the post-scoreboard next-map pick stays open; `0` disables it. Clamped to `5`–`60`. See [Next-Map Pick](#next-map-pick). |
| `g_match_awards` | `10` | Seconds the post-match awards reel stays up unattended before handing on to the next-map pick; `0` disables it. Clamped to `3`–`30`, and skippable with any key after the first three seconds. Ranked matches only. See [Post-Match Awards](#post-match-awards). |
| `g_map_pool` | empty | Additional voting map pool. |
| `g_dm_exec_level_cfg` | `0` | Executes level-specific configs when enabled. |
| `g_loc` | `1` | Enables location-backed teammate callouts, including the `loc` command and Freeze Tag frozen help markers. |
| `g_loc_items` | `1` | Allows location callouts to derive a fallback location from visible weapons, powerups, or mega health when no map `.loc` file exists. |
| `g_motd_filename` | `motd.txt` | Message of the day file (maximum 256 KiB). Client output uses UTF-8-safe 900-byte chunks: `motd` is capped at eight messages and the automatic join preview at one, with a truncation notice for longer text. |
| `g_entity_override_dir` | `maps` | Directory for entity override `.ent` files. |
| `g_entity_override_load` | `1` | Loads entity override files on map load. |
| `g_entity_override_save` | `0` | Saves entity override files when none exist. |

### Next-Map Pick

When the end-of-match scoreboard — and the awards reel after it, where one runs —
is done, the level normally changes straight to whatever the rotation picked.
With `g_map_pick` set, the intermission camera instead holds and everyone chooses
the next map from up to three candidates:

```text
set g_map_pick "15"
```

Candidates come from the same eligibility rules the automatic rotation uses, so
the pick can only ever offer a map the rotation would have been willing to load
next. The current map is always excluded. With a structured pool the candidates
carry their `title`; on the legacy `g_map_pool`/`g_map_list` sources they show
the BSP name.

Players choose with the movement keys and attack/jump, or with `mappick <1-3>`
from the console. Live tallies are shown beside each map and a vote can be
changed until the window closes. Once a map holds more than half of the eligible
voters the pick ends early. Ties are broken at random, and a pick that nobody
voted in still resolves to one of the offered maps.

Eligibility follows `g_allow_spec_vote`, exactly as regular votes do. Bots never
vote and are not counted toward the majority.

The pick stands down and the level changes as usual when:

- `g_map_pick` is `0`, or the gametype is Rocket Arena;
- the next map was already chosen deliberately — a `mymap` queue entry,
  `g_dm_same_level`, or a forced map;
- fewer than two eligible candidates exist;
- no human players remain to ask.

### Post-Match Awards

The end of a ranked match is three screens in a row, each handing on to the next
the same way: a minimum hold so nobody presses straight through it, then any key,
with a timeout so an unattended server still moves.

```text
scoreboard  ->  awards reel  ->  next-map pick  ->  level changes
```

The awards reel replaces the scoreboard with a title in green and the player who
earned it in white underneath.

```text
set g_match_awards "10"
```

`g_match_awards` is how long the reel stays up unattended; `0` disables it. Any
key advances it early, but not for the first three seconds — otherwise the press
that dismissed the scoreboard would carry straight through the reel before
anybody had seen it. Once the reel is done the next-map pick opens under its own
rules, and the level changes after that.

At most 12 awards are shown, ordered so that the honours come first and the
wooden spoons are what get dropped when more than 12 qualify. A single player
takes at most 3 titles before the remainder are offered to everyone else, and any
slots still unused after that are back-filled from what the cap skipped.

Every award needs a strict winner, so a tie awards nobody, and every award has a
floor to clear — "most rail kills" with four kills is not a marksman. Awards
range from the earned (`SHOTGUN SHERIFF`, `RAIL SLUT`, `QUAD GOD`, `AIMBOT
ALLEGATIONS`, `UNTOUCHABLE`) through the observed (`SPAWN FRAGGER`, `DIRTY ROTTEN
CAMPER`, `KLEPTOMANIAC`) to the deserved (`QUAD DUMMY` for hoarding the Quad and
doing nothing with it, `STORMTROOPER`, `THE PUNCHING BAG`, `BUTTERFINGERS`).
Capture the Flag and team modes add their own.

Awards are a ranked-match feature and are only offered when:

- `g_ranked` is on and the gametype is not Rocket Arena;
- no bot took part, and at least two humans did;
- the match ran for at least a minute.

Whatever a player earned is repeated in their end-of-match summary, is available
from the console with `awards`, and is written into the match export (`matchAwards`
per match and per player) and into their career profile under `stats.awards`.

Two of the awards need counters the mod did not previously keep, both collected
only while a match is being recorded: kills landed while the attacker's Quad was
still running, and a once-a-second sample of where each living player is standing,
bucketed into 512-unit cells. The camping award is a share of those samples in one
cell, and it explicitly disqualifies anyone the inactivity timer had flagged, so
an idle body cannot out-camp a player who was actually playing.

### Structured Map Pools (Optional)

Structured map pools provide a large searchable catalog plus a separate
automatic-rotation cycle. They are opt-in: copy
`muffmode-map-pool.example.json` and `muffmode-map-cycle.example.txt` to new
leaf filenames in `rerelease/baseq2`, edit those copies, then configure them:

```text
set g_maps_pool_file "muffmode-map-pool.json"
set g_maps_cycle_file "muffmode-map-cycle.txt"
set g_maps_random "1"
set g_maps_repeat_delay "1800"
```

Only safe leaf filenames are accepted for these two cvars; paths and traversal
segments are rejected. Pool identifiers are portable lowercase ASCII BSP stems
with forward slashes, so safe subdirectories such as `q64/dm1` work on both
case-sensitive and Windows hosts. Do not add `maps/`, `.bsp`, cinematic/demo
suffixes, Windows device names, or engine map-command markers such as `+` and
`$`.

The JSON root must contain a `maps` array. Each usable entry requires a safe
`bsp` string and at least one true mode flag:

```json
{
  "maps": [
    {
      "bsp": "q2dm1",
      "title": "The Edge",
      "episode": "baseq2",
      "dm": true,
      "duel": true,
      "min": 2,
      "max": 8,
      "popular": true
    }
  ]
}
```

Supported fields are:

| Field | Required | Meaning |
| --- | --- | --- |
| `bsp` | yes | Canonical lowercase ASCII map identifier with forward slashes and without `maps/` or `.bsp`. Player lookups remain case/slash-insensitive; duplicate identifiers keep the first valid entry. |
| `dm`, `tdm`, `ctf`, `duel`, `arena` | one or more | Boolean mode-suitability flags. CTF and Arena always require their matching flags. Duel/TDM tags are preferred when present, with generic `dm` maps used as fallback at every eligibility tier. |
| `title` | no | Display title; defaults to `bsp`. |
| `episode` | no | Display/filter grouping such as `baseq2`, `rogue`, or `muffmode`. |
| `min`, `max` | no | Inclusive human-player bounds; `0` or omission means no bound. |
| `popular` | no | Gives the map weight `2` instead of `1` when `g_maps_random` is enabled. |
| `custom`, `custom_textures`, `custom_sounds` | no | Catalog metadata. Either asset flag also marks the entry as custom. |

The cycle is an ordered whitespace-separated list of `bsp` identifiers from
the pool. It accepts an optional UTF-8 BOM, `//` line comments, and `/* ... */`
block comments, removes case/slash-insensitive duplicates, and ignores safe
names not found in the pool. Stray or unterminated block-comment delimiters
fail the cycle instead of being guessed. With `g_maps_random 0`, the order
determines the next eligible map. With `g_maps_random 1`, the cycle is the
eligible set and `popular` supplies the only extra weighting.

Selection uses the requested raw gametype, so a transition into Arena can
choose an `arena`-tagged map before that mode becomes effective. It first
honors human-player bounds and the repeat delay; Duel/TDM tries its specialized
tag before generic `dm` fallback at each tier. If needed, selection relaxes
the repeat delay and then player bounds, but it always excludes the current
map. If no other compatible cycle entry remains, normal legacy `g_map_list`
transition handling continues.

An explicit gametype change is the one exception to next-map ordering: ordered
selection begins at the first compatible cycle entry, matching the legacy
"first map after the new gametype config" behavior. That path may reload the
current map when it is the first compatible entry; random selection still
chooses from the eligible set.

The structured and legacy systems interoperate as follows:

| Structured state | Voting and MyMap source | Automatic rotation |
| --- | --- | --- |
| No valid structured pool | Legacy `g_map_pool` plus `g_map_list` | Legacy `g_map_list` |
| Valid pool, but no valid cycle | Structured pool | Legacy `g_map_list` |
| Valid pool and cycle | Structured pool | Structured cycle, then legacy `g_map_list` if no eligible map can be selected |

Legacy fallback entries use the same safe BSP-stem identity rules as cycle
entries, while retaining case-insensitive and slash-insensitive matching.
Unsafe or non-map tokens are ignored instead of being sent to the engine.

Malformed roots, unsupported root properties, and oversized inputs fail
closed. Unsupported entry properties invalidate that entry so misspelled
settings cannot silently change policy. Invalid individual pool entries and
unusable cycle tokens are skipped with bounded diagnostics; a pool with no
usable multiplayer entries or a cycle with no recognized entries fails.
Titles and episodes must be well-formed UTF-8 without terminal, line-breaking,
or bidirectional display controls. Once a structured snapshot is active, a
failed live reload keeps that complete last-known-good snapshot. If a full
pool-and-cycle transaction fails, a later cycle change or `load_mapcycle`
retries the full transaction rather than combining the old pool with a new
cycle. On the first load, a valid pool is still published if its configured
cycle is invalid, so structured voting/MyMap works while legacy `g_map_list`
handles rotation; if the pool itself is invalid, both uses remain legacy.
Clearing `g_maps_cycle_file` disables only the structured cycle. Clearing
`g_maps_pool_file` disables the structured system immediately and returns both
voting and rotation to legacy sources, including during a match timeout.

Players can inspect the active catalog with `mappool [filter]` and the active
cycle with `mapcycle [filter]`. Filters match map identifiers, titles, and
episodes; the exact filters `dm`, `tdm`, `ctf`, `duel`, `arena`, `popular`,
and `custom` select metadata. Each listing prints at most the first 32 matches
and asks for a narrower filter when more remain. `maplist` summarizes both
structured status and any active legacy fallback. An authenticated admin can
run `load_mappool` after changing the pool (this transaction also validates
the configured cycle), or `load_mapcycle` after changing only the cycle. A
dedicated console can use `sv load_mappool` and `sv load_mapcycle`.

## Item And Gameplay Cvars

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_arena_start_armor` | `200` | Global MuffMode Arena starting-armor default; also used by Freeze Tag when `g_freezetag_arena_loadout` is enabled. |
| `g_arena_start_health` | `200` | Shared arena-loadout starting-health default; also used by Freeze Tag when `g_freezetag_arena_loadout` is enabled. The shipped Arena factories override it to `100`. |
| `g_arena_dmg_armor` | `0` | Legacy arena-loadout self-armor switch used outside multi-room MuffMode Arena; GT_ARENA uses each room's independent `armorprotect` setting. |
| `g_coop_health_scaling` | `0` | Scales co-op health by player count. |
| `g_corpse_sink_time` | `15` | Seconds before corpses sink and disappear. |
| `g_damage_scale` | `1` | Global damage scale. |
| `g_gib_enhanced` | `1` | Enhanced gib and debris presentation, built entirely from stock rerelease assets. Gibs take map lighting instead of rendering fullbright, vary in scale, fly nose-first along their velocity, keep the direction of the killing blow instead of having each axis clamped separately, drift and drag in water rather than falling through it, settle flat on floors, and fade out as they sink. Player deaths also mix in the limb, bone and torso gib models the base game precaches for monsters but never gives to players. `0` restores id's original behaviour exactly. |
| `g_gib_impact_effects` | `2` | Feedback when a gib hits something. `0` disables it, `1` emits the blood or spark burst only, `2` also plays the stock `player/gibimp*` impact sounds. Effects are rate-limited per gib and server-wide, and gibs that reach a sky brush are removed instead of bouncing off it. |
| `g_gib_max` | `192` | Ceiling on live gibs across the server. Once reached, the oldest gib is retired so the newest death always looks right. `0` removes the cap; the tracking ring bounds it to `1024` either way. A single deepest-overkill death costs at most 58 entities. |
| `g_dm_explosive_respawn_time` | `60` | Seconds before a destroyed `misc_explobox` or `func_explosive` returns in deathmatch; `0` disables prop respawning entirely. Values are clamped to `1`-`3600`. A prop only returns once no player can see its spot, nothing is standing in it, and no player is within 128 units, so the wait can exceed this time on a busy map. |
| `g_dm_holdable_adrenaline` | `1` | Allows holdable Adrenaline in deathmatch. |
| `g_dm_instant_items` | `1` | Makes holdable items activate instantly in deathmatch. |
| `g_dm_item_respawn_rate` | `1.0` | Global multiplier on every item's respawn time (weapons included). |
| `g_dm_no_fall_damage` | `0` | Disables deathmatch fall damage. |
| `g_dm_no_quad_drop` | `0` | Prevents Quad Damage from dropping on death. |
| `g_dm_no_self_damage` | `0` | Disables self damage after knockback calculation. |
| `g_dm_no_stack_double` | `0` | Prevents double-stacking behavior for configured deathmatch items. |
| `g_dm_powerup_drop` | `1` | Drops carried powerups on death. |
| `g_dm_powerups_minplayers` | `0` | Minimum players required for powerup pickup; `0` disables. |
| `g_dm_random_items` | `0` | Enables random item replacement behavior. |
| `g_dm_strong_mines` | `0` | Enables stronger deathmatch mine behavior. |
| `g_dm_weapons_stay` | `0` | Controls weapons-stay behavior in deathmatch. |
| `g_drop_cmds` | `7` | Bitflag for dropping flags, powerups, weapons, and ammo. |
| `g_fast_doors` | `1` | Doubles standard and rotating door speed. |
| `g_freezetag_arena_loadout` | `0` | Freeze Tag only: set `1` to give players the arena-style spawn/thaw kit using `g_arena_start_health`, `g_arena_start_armor`, full weapons, and stocked ammo. The default `0` preserves map/item-control loadouts. |
| `g_freezetag_auto_thaw_time` | `0` | Freeze Tag only: seconds before a frozen player is thawed automatically; `0` disables auto-thaw. |
| `g_freezetag_bot_rescue` | `1` | Freeze Tag only: lets bots path toward frozen teammates and hold near them to thaw. |
| `g_freezetag_frozen_hazard_release_time` | `10` | Freeze Tag only: seconds a frozen body may sit in lava or slime before it is destroyed and its owner respawns, with no thaw credit or score awarded to anyone. A safety valve for bodies shoved somewhere no teammate can reach, so the default is deliberately far slower than `g_freezetag_thaw_time` — dying in lava must not be a cheaper escape than a rescue. The timer holds while a teammate is actively thawing, and restarts if that attempt is abandoned. Bodies that leave the map are released immediately. Set `0` to disable the watchdog, at the risk of stranding a player for the rest of the round. |
| `g_freezetag_frozen_knockback_scale` | `1.0` | Freeze Tag only: multiplier for knockback applied to frozen players. Frozen bodies are shovable, and weapons that carry no weapon kick of their own fall back on knockback derived from damage (capped at 200) before this multiplier applies. Set `0` to make bodies immovable. |
| `g_freezetag_frozen_shove_lift` | `24` | Freeze Tag only: upward speed, in units per second, added to a shove so the body clears the floor and starts sliding. Lower values keep bodies grounded; higher values make them pop into the air. |
| `g_freezetag_frozen_shove_max_speed` | `700` | Freeze Tag only: speed cap, in units per second, applied to a frozen body after each hit. Prevents several pellets or a sustained grapple pull from stacking into a single map-crossing launch. |
| `g_freezetag_frozen_slide_friction` | `0.9` | Freeze Tag only: fraction of velocity a sliding frozen body retains on each floor contact, against `0.75` for gibs and dropped items; clamped from `0.1` to `0.99`. This alone sets how far a shove carries — at the default a rocket moves a body roughly two body-widths on flat ground, and much further downhill or off a ledge. Lower values stop bodies sooner; values near `0.99` make them slide like ice and creep down ramps. |
| `g_freezetag_multi_thaw_scale` | `0.5` | Freeze Tag only: extra thaw speed contributed by each additional live teammate near the frozen player; values are clamped from `0` to `4`, and total thaw rate is capped. |
| `g_freezetag_round_respawn_all` | `1` | Freeze Tag only: respawns every round participant for the next round when `1`; set `0` to respawn only frozen, dead, or waiting players while live survivors stay in place. |
| `g_freezetag_round_reset_alive_inventory` | `1` | Freeze Tag only: when full round respawns are enabled, resets live survivor inventory/loadout on the next-round respawn. Set `0` to preserve survivor inventory through that respawn. |
| `g_freezetag_thaw_radius` | `96` | Freeze Tag only: teammate proximity radius, in units, required to thaw a frozen player. |
| `g_freezetag_thaw_respawn_at_location` | `0` | Freeze Tag only: when `0`, thawed players respawn normally at player spawn points. Set `1` to restore them at the safe thaw location instead. |
| `g_freezetag_thaw_time` | `3` | Freeze Tag only: seconds a live teammate must remain near a frozen player to thaw them. |
| `g_frenzy` | `0` | Enables Weapons Frenzy: faster fire rates, faster rockets, regenerating ammo, and faster weapon switching. |
| `g_grapple_offhand` | `0` | Enables offhand hook commands. |
| `g_grapple_damage` | `10` | Grapple impact damage. |
| `g_grapple_fly_speed` | `650` | Grapple projectile speed. |
| `g_grapple_pull_speed` | `650` | Grapple pull speed. |
| `g_infinite_ammo` | `0` | Enables infinite ammo when latched before map load. |
| `g_instagib` | `0` | Enables Instagib as a game modification or through the Instagib gametype. |
| `g_instagib_splash` | `0` | Adds non-damaging instagib rail explosions for movement and knockback. |
| `g_knockback_scale` | `1.0` | Scales knockback from damage. |
| `g_ladder_steps` | `1` | Ladder step sounds: `1` campaigns only, `2` always. |
| `g_lag_compensation` | `1` | Enables lag compensation. |
| `g_lag_compensation_enhanced` | `1` | Enables richer lag compensation with historical hitboxes, lag-aware aim projection, frame-based snapshot selection, interpolation, and stale/discontinuous-history cleanup. |
| `g_mover_speed_scale` | `1.0f` | Scales mover speed for doors, rotators, lifts, and similar entities. |
| `g_no_bfg` | `0` | Prevents BFG spawning in maps. |
| `g_no_armor` | `0` | Prevents armor spawning in maps. |
| `g_no_health` | `0` | Prevents health spawning in maps. |
| `g_no_items` | `0` | Prevents normal item spawning in maps. |
| `g_no_mines` | `0` | Prevents mine spawning in maps. |
| `g_no_nukes` | `0` | Prevents nuke spawning in maps. |
| `g_no_plasmabeam` | `0` | Prevents Plasma Beam spawning in maps. |
| `g_no_powerups` | `0` | Disables powerup pickups. |
| `g_no_spheres` | `0` | Prevents sphere powerups spawning in maps. |
| `g_nadefest` | `0` | Enables grenade-only NadeFest behavior. |
| `g_quadhog` | `0` | Enables Quad Hog behavior. |
| `g_starting_armor` | `0` | Starting armor on spawn. |
| `g_starting_health` | `100` | Starting health on spawn. |
| `g_starting_health_bonus` | `0` | Bonus health granted on spawn, except where a ruleset overrides it. |
| `g_start_items` | empty | Space-separated extra item classnames or item names granted on spawn. |
| `g_vampiric_exp_min` | `0` | Minimum health value for vampiric expiration. |
| `g_vampiric_damage` | `0` | Enables Vampiric Damage healing from damage dealt. |
| `g_vampiric_health_max` | `9999` | Maximum health cap from vampiric damage. |
| `g_vampiric_percentile` | `0.67f` | Health percentile bonus for vampiric damage. |
| `g_weapon_projection` | `0` | Weapon projection offset mode. |
| `g_weapon_respawn_time` | `30` | Weapon respawn time in seconds. This is the literal time even in Horde — weapons are not affected by `g_horde_item_respawn_scale`. Effective time is `g_weapon_respawn_time` × `g_dm_item_respawn_rate`. |

## Interface And Debug Cvars

| Cvar | Default | Purpose |
| --- | --- | --- |
| `bot_name_prefix` | `B|` | Prefix for bot names. Blank removes the prefix. |
| `g_dm_crosshair_id` | `1` | Enables crosshair player identification by default. |
| `g_frag_messages` | `1` | Enables frag message drawing. |
| `g_frames_per_frame` | `1` | Game frames run per server frame, clamped to `0..64`; `0` intentionally pauses game simulation. Useful for controlled testing and performance tuning. |
| `g_huntercam` | `1` | Enables huntercam spectator behavior. |
| `g_item_bobbing` | `1` | Enables item bobbing. |
| `g_matchstats` | `0` | Enables the live in-game match-statistics menu. It does not control completed-match artifact exports. |
| `g_muffmode_debug` | `0` | Enables `muffmode_debug.log` output. |
| `g_ranked` | `1` | Master switch for ranking. Set to `0` to run a purely casual server: skill ratings never move, `sr` reports the server as unranked, the join centerprint omits the rating line, and no match statistics are collected or exported (`g_statex_*` has nothing to write). Player preference profiles still load and save. Advertised in serverinfo. |
| `g_select_empty` | `0` | Allows selecting weapons without ammo. |
| `g_showhelp` | `1` | Prints quick explanations for game modifications. |
| `g_showmotd` | `1` | Shows message of the day behavior when enabled. |
| `g_statex_enabled` | `1` | Writes completed singleton-match JSON artifacts and maintains `baseq2/matches/catalog.json`; concurrent Arena room series are excluded. Requires `g_ranked 1`. |
| `g_statex_export_html` | `1` | Writes a companion HTML report for each exported match. JSON and the catalog remain enabled when this is `0`. |
| `g_statex_humans_present` | `1` | Exports only matches with at least one human participant; set to `0` to include bot-only matches. |
| `g_verbose` | `0` | Enables extra console diagnostics. |

## Drop Command Flags

`g_drop_cmds` is a bitflag:

| Value | Allows |
| --- | --- |
| `1` | Dropping CTF flags. |
| `2` | Dropping powerups. |
| `4` | Dropping weapons and ammo. |

The default `7` enables all three.

## Per-Level Config Files

When `g_dm_exec_level_cfg` is enabled, MuffMode executes a config named for the
loaded map (`exec <mapname>`) at level start. It is off by default.

Per-gametype config files (`gt-FFA.cfg` and friends) no longer exist. Everything
they carried — the mode's ruleset, limits, map rotation, player limits and
gameplay settings — now lives in a factory. See [Factories](#factories) and the
migration notes in `baseq2/CONFIGS_README.md`.

To run endless Horde, select a factory with `roundlimit 0` — the shipped
`horde_endless` does exactly that. See
[Horde Late-Wave & Endless](#horde-late-wave--endless).

## Horde Wave And Scaling Cvars

These cvars tune Horde pacing, wave budget, player scaling, and map-size scaling.

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_horde_starting_wave` | `1` | First wave number after map load; latched before the level starts. |
| `g_horde_points_base` | `15` | Base monster point budget. |
| `g_horde_points_per_wave` | `5` | Additional point budget per wave before late-wave tapering. |
| `g_horde_points_min` | `0` | Optional minimum point budget; `0` disables. |
| `g_horde_points_max` | `0` | Optional maximum point budget; `0` disables. |
| `g_horde_spawn_interval_min` | `0.3` | Minimum time between monster spawns, in seconds. |
| `g_horde_spawn_interval_max` | `0.5` | Maximum time between monster spawns, in seconds. |
| `g_horde_spawn_burst_count` | `6` | Number of successful spawns in a pressure burst before adding a short rest. `0` disables burst rests and restores a steady stream. |
| `g_horde_spawn_burst_rest` | `2.0` | Seconds added after each completed spawn burst. |
| `g_horde_warmup_cap` | `30` | Maximum warmup monsters alive. |
| `g_horde_max_alive` | `60` | Maximum live monsters during active waves; `0` disables the cap. |
| `g_horde_wave_spawn_delay_ms` | `500` | Delay before a new wave starts spawning monsters. |
| `g_horde_player_scale` | `1` | Scales wave budget by active fighter count. |
| `g_horde_player_scale_factor` | `0.4` | Additional budget factor per extra fighter. |
| `g_horde_player_scale_max` | `8` | Maximum fighter count considered by player scaling. |
| `g_horde_lives` | `1` | Lives granted to each fighter per wave. Values above `1` allow real mid-wave respawns until the counter reaches zero. |
| `g_horde_featured_spawns` | `3` | Successful early-wave spawns reserved for monster types newly unlocked on that wave. `0` leaves every unlock to weighted chance. |
| `g_horde_wave_type_ramp` | `3` | Adds one to the effective non-themed minimum roster breadth every N waves. `0` disables the ramp and uses `g_horde_wave_min_types` unchanged. |
| `g_horde_mark_monsters_threshold` | `3` | Starts marking remaining monsters when the living count is at or below this value. |
| `g_horde_mark_monsters_max` | `8` | Maximum monster marker slots. |
| `g_horde_map_scale` | `1` | Enables map-size-based budget scaling. |
| `g_horde_map_scale_ref` | `4000` | Reference map span for map-size scaling. |
| `g_horde_map_scale_factor` | `0.5` | Strength of map-size scaling. |
| `g_horde_start_chainsaw` | `1` | Gives Horde players Chainfist/Chainsaw-style starting melee support when applicable. |

## Horde Champions And Themes

Champions are stronger monster variants. Themes bias a wave toward a monster category when enough matching
monsters are available. A killed champion rolls a strong reward at `g_horde_champion_drop_chance` (100% by
default) -- a random tech when techs are enabled (no other Horde monster drops techs), otherwise a pick from
the champion strong-item pool. Summon or resurrection kills cannot be farmed for repeat score, rally progress,
or drops.

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_horde_champions` | `1` | Enables champion monster rolls. |
| `g_horde_champion_max_per_run` | `2` | Target champion budget across the tuned run before late-wave steady-rate logic. |
| `g_horde_champion_chance` | `0.6` | Chance factor used when allocating champions. |
| `g_horde_champion_min_wave` | `3` | Earliest wave that can spawn champions. |
| `g_horde_champion_health_mult` | `3.0` | Champion health multiplier. |
| `g_horde_champion_health_floor` | `400` | Minimum champion health floor before per-wave scaling. |
| `g_horde_champion_health_per_wave` | `25` | Additional champion health floor per wave. |
| `g_horde_champion_damage_mult` | `2.0` | Champion outgoing-damage multiplier. |
| `g_horde_champion_speed_mult` | `1.25` | Champion movement-speed multiplier. |
| `g_horde_champion_strong_ratio` | `4.0` | Ratio used to taper champion strength on already-strong monsters. |
| `g_horde_champion_force` | `0` | Debug/test switch that forces a champion every wave. Leave off for public servers. |
| `g_horde_themed_waves` | `1` | Enables occasional themed waves. |
| `g_horde_theme_chance` | `0.20` | Chance for a themed wave. |
| `g_horde_theme_min_wave` | `4` | Earliest wave that can use themes. |
| `g_horde_wave_variety` | `1` | Enables roster variety limits for non-themed waves. |
| `g_horde_wave_min_types` | `3` | Minimum monster type count when wave variety can be satisfied. |

## Horde Wildcard Waves And Edge Drops

Wildcard Waves are an opt-in set of unusual wave modifiers suggested by playtester HonkHonk. They are disabled
as a group by default: individual weights only affect selection after `g_horde_preset_chance` is set above `0`.
A Wildcard replaces the normal theme, roster, featured-spawn, and champion choices for that wave, preventing
modifier stacking. Boss units never receive Wildcard scaling or combat modifiers; setting
`g_horde_preset_allow_boss_waves 1` only permits the boss wave's ordinary escorts to use one.

Model and collision scaling always stay synchronized and are clamped to `0.5`-`1.5`. Scaled monsters use the
same pre-spawn and post-spawn world validation as every other Horde threat. Clone Army excludes medics so a
single-type wave cannot recursively grow through revivals. Tiny Shamblers is ineligible before the Shambler's
wave-10 unlock. Weights are clamped to `0`-`12`; a weight of `0` disables that preset.

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_horde_monster_edge_drops` | `1` | Lets a living ground monster walk off an edge only while actively chasing a living fighter and only when a non-hazardous, sufficiently flat BSP landing exists within 256 units and remains in the target's PHS. Bosses, flyers, swimmers, stand-ground monsters, voids, lava, slime, and deep water are excluded. |
| `g_horde_preset_chance` | `0` | Chance that an eligible wave becomes a Wildcard Wave; clamped to `0`-`1`. `0` disables every Wildcard regardless of its weight. |
| `g_horde_preset_allow_boss_waves` | `0` | Allows a Wildcard on scheduled boss waves. It applies only to ordinary escorts, never to boss units. |
| `g_horde_preset_weight_clone_army` | `7` | One eligible non-medic director monster type fills the ordinary wave. |
| `g_horde_preset_weight_funhouse_horde` | `5` | Each director monster is either `0.6x` or `1.45x` model/hull scale, with bounded health, damage, and movement compensation. |
| `g_horde_preset_weight_get_over_here` | `4` | Ordinary monster hits deal `85%` damage and add a capped pull toward the attacker. |
| `g_horde_preset_weight_giant_horde` | `4` | Director monsters use `1.35x` model/hull scale, `1.6x` health, `1.1x` damage, and `0.75x` movement. |
| `g_horde_preset_weight_glass_cannon` | `8` | Director monsters use `0.4x` health and `1.8x` damage. |
| `g_horde_preset_weight_low_gravity` | `3` | Uses `55%` gravity for fighters and director monsters for this wave without changing the server's global gravity cvar. |
| `g_horde_preset_weight_tiny_shamblers` | `4` | From wave 10 onward, fills the ordinary wave with `0.55x` Shamblers using reduced health/damage and faster movement. |
| `g_horde_preset_weight_tiny_terror` | `10` | Director monsters use `0.6x` model/hull scale, `0.35x` health, `0.7x` damage, and `1.5x` movement. |
| `g_horde_preset_weight_pinball_night` | `4` | Uses `65%` fighter/director-monster gravity, `500%` knockback against fighters, and `85%` ordinary-monster damage. |
| `g_horde_preset_weight_sawstorm` | `5` | Quadruples player Chainfist damage against non-boss monsters for the wave. |

The proposed route-time hull shrinking, visual-only scaling, Hyper Wave, Jackpot Wave, Kill Medic instant
revival, Mini Me spawn multiplication, and Boss Duel are intentionally not implemented. Route-time shrinking
can restore a hull while overlapping geometry; visual-only scaling creates misleading collision; the Hyper
proposal cannot safely accelerate every monster attack animation as one uniform rule; loot/revival/copy
mechanics can multiply live entities or drops; and making hostile boss AI duel reliably would require new
faction, target, summon, completion, and reposition semantics rather than a safe director modifier.

## Horde Bosses, Water Ambushes, And Reinforcements

Scheduled boss waves replace the champion roll for that wave, reduce the normal escort budget, spawn the boss
first, and pause briefly before escorts arrive. Bosses use their real hull for spawn validation and stay marked
for the team and drive the native named boss health bars. The 25-profile catalog combines the core Quake II
Supertank, Guardian, Hornet, Carrier, Black Widow, Makron, and Black Widow II with every named Call of the Machine
boss encounter and its explicit Arachnid mini-boss. Children of Makron and Masters of the Machine deploy two
bosses and use both native health bars; the defeat announcement and boss-kill rally occur only after the complete
encounter is gone.

Selection follows unlock tiers, avoids the configured number of recent profiles when the current tier has enough
alternatives, and gives a compatible authored `horde_boss` anchor priority over the global roll. Boss health gains
20% per additional active fighter, then applies profile, pair, global, and post-unlock wave multipliers. Damage
uses the equivalent profile/global/endless growth path. Power armor can be scaled separately. The default model
scale limit keeps Modir and other unusually large campaign variants usable on multiplayer maps; setting it to
`0` disables that configurable limit while retaining an absolute safety ceiling of `16`.

Carrier and Medic Commander reinforcements remain pressure-only, but Widow-spawned monsters and successful
medic revivals become counted threats that must be killed before the wave can end. All summoned or revived
monsters remain ineligible for repeat score, rally progress, or drops, and transition cleanup removes any
pressure-only survivors. At level load, Horde tests every boss profile's scaled hull against authored boss anchors and
player spawns, records only the player points suitable for that profile, and excludes profiles without enough
placements (including two distinct placements for enabled pair encounters). A map with no compatible profile
does not schedule boss waves. Runtime blockage can still cancel a deployment after bounded retries, but never
forces a different boss that was not validated for the map.

Aquatic attempts can use authored `monster_flipper` placements, underwater `monster_gekk` placements, or
explicit water anchors, choosing either a Flipper or swimming Gekk. If no authored water location is usable, the
director falls back to a fully submerged ambush near waist-deep fighters. Maps without suitable water simply
continue with the normal roster.

Eliminated fighters can be rallied back by team kills. A boss kill immediately earns the pending rally; otherwise
the configured kill threshold is required. Rallying grants one life, uses normal Horde spawn/loadout handling,
and applies short spawn protection. The per-wave cap prevents an endless death loop.

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_horde_boss_waves` | `1` | Enables scheduled boss waves. |
| `g_horde_boss_min_wave` | `6` | First wave eligible for the boss schedule. |
| `g_horde_boss_interval` | `6` | Waves between scheduled bosses after the first; `0` disables the schedule. |
| `g_horde_boss_budget_mult` | `0.8` | Multiplies the normal wave budget on boss waves before the boss cost is deducted. Values below `0.1` are treated as `0.1`. |
| `g_horde_boss_health_mult` | `1.0` | Multiplies boss health after automatic active-fighter scaling. Values below `0.1` are treated as `0.1`. |
| `g_horde_boss_damage_mult` | `1.15` | Boss outgoing-damage multiplier. Values below `0.1` are treated as `0.1`. |
| `g_horde_boss_tier_window` | `3` | Carries forward bosses whose unlock wave is within this many waves of the newest unlocked boss. Profiles unlocked since the previous scheduled boss wave are always admitted once, so cadence cannot skip a tier; `0` otherwise keeps only the newest tier. |
| `g_horde_boss_powerup_chance` | `0.35` | Chance that a killed boss's guaranteed strong reward is a timed powerup; clamped to `0`-`1`. |
| `g_horde_boss_machinegames` | `1` | Includes the Call of the Machine named profiles in progression rolls. Explicit `g_horde_boss_force` still permits one while this is `0`. |
| `g_horde_boss_pairs` | `1` | Enables the two-unit `children_of_makron` and `masters_of_the_machine` profiles. |
| `g_horde_boss_repeat_window` | `2` | Avoids this many most-recent boss profiles when the active tier has enough alternatives; exclusions relax oldest-first if necessary. |
| `g_horde_boss_force` | empty | Forces one profile ID on every scheduled boss wave, bypassing unlock and MachineGames filters. Pair profiles still require `g_horde_boss_pairs 1`, and the forced profile is skipped if its level-load placement catalog has no suitable authored/player points. |
| `g_horde_boss_scale_limit` | `2.5` | Maximum applied boss model/hull scale. `0` disables this configurable cap; an absolute safety ceiling of `16` still protects collision and trace math. |
| `g_horde_boss_health_per_wave` | `0.05` | Adds this fraction of health for each wave after the selected profile's unlock wave. Negative values behave as `0`. |
| `g_horde_boss_damage_per_wave` | `0.01` | Adds this fraction of outgoing damage for each wave after the selected profile's unlock wave. Negative values behave as `0`. |
| `g_horde_boss_pair_health_mult` | `1.0` | Per-unit health multiplier for paired encounters. Values below `0.05` are treated as `0.05`. |
| `g_horde_boss_armor_mult` | `1.0` | Multiplies any power-screen or power-shield capacity produced by the boss class/profile/anchor. `0` removes its capacity. |
| `g_horde_water_spawns` | `1` | Enables authored or dynamic aquatic Flipper/Gekk attempts. |
| `g_horde_water_spawn_chance` | `0.30` | Chance per normal spawn opportunity to attempt an aquatic spawn; clamped to `0`-`1`. |
| `g_horde_water_max_alive` | `4` | Maximum simultaneous aquatic Horde monsters. `0` removes this separate cap. |
| `g_horde_reinforcement_kills` | `12` | Monster kills required to rally one eliminated fighter; values below `1` are treated as `1`. |
| `g_horde_reinforcements_per_wave` | `1` | Maximum mid-wave rallies. `0` disables rallies, including the boss-kill rally. |
| `g_horde_reinforcement_protection` | `2.0` | Seconds of Protection granted to a rallied fighter. |

### Call Of The Machine Boss Catalog

The shipped `baseq2/pak0.pak` contains 20 `target_healthbar` entities representing 18 named encounters. The table
below records their English rerelease names and entity-lump tuning. Difficulty-split `0.75`/`1.25` encounters use
`1.25` as the portable profile, while Horde on the original map keeps the active difficulty variant's exact
anchor value. Carrier base health remains the rerelease class's skill-dependent `2000`-`4000` before multipliers.

| Profile ID | Shipped encounter | Map | Class | Authored tuning |
| --- | --- | --- | --- | --- |
| `gate_warden` | Gate Warden | `mgu1m3` | `monster_boss2` | `2x` health, `1.25` scale; the active map variant retains its Hornet attack flag. |
| `makron` | Makron | `mgu1m5` | `monster_makron` | `1.25` scale. |
| `children_of_makron` | Children of Makron | `mgu1m5` | `monster_makron` | Two units, each `0.8` scale. |
| `bloodstarved_mutant` | Bloodstarved Mutant | `mgu2m2` | `monster_mutant` | `6x` health, `1.5` scale. |
| `strogg_supertank` | Strogg Supertank | `mgu3m4` | `monster_supertank` | `0.75x`/`1.25x` health by difficulty. |
| `strogg_carrier` | Strogg Carrier | `mgu3m4` | `monster_carrier` | `0.75x`/`1.25x` health by difficulty. |
| `strogg_megatank` | Strogg Megatank | `mgu3m4` | `monster_boss5` | `0.75x`/`1.25x` health; shielded Supertank class with its heat-seeking rockets and default 400 shield. |
| `ancient_carrier` | Ancient Carrier | `mgu3secret` | `monster_carrier` | `0.75x`/`1.25x` health by difficulty. |
| `commander` | Commander | `mgu4m1` | `monster_tank_commander` | `2x` health, `1.3` scale, 250-point shield, heat-seeking rockets. |
| `garbage_carrier` | Garbage Carrier | `mgu4m3` | `monster_carrier` | `1x`/`1.25x` health, four summon slots, Stalker reinforcements. |
| `arachnid` | Arachnid mini-boss | `mgu5m2` | `monster_arachnid` | `1.5x` health; this encounter has no campaign health-bar name. |
| `system_administrator` | The System Administrator | `mgu5m3` | `monster_makron` | `0.75x` health. |
| `janitor` | The Janitor | `mgu5m3` | `monster_supertank` | `0.2` scale with full Supertank health and attacks. |
| `overburden` | Overburden | `mgu6m1` | `monster_supertank` | Power-screen type authored without extra capacity. |
| `underminer` | The Underminer | `mgu6m2` | `monster_supertank` | `2x` health. |
| `modir` | Modir | `mgu6m3` | `monster_shambler` | `40x` health, `5.5` authored scale; geometry is capped by `g_horde_boss_scale_limit` by default. |
| `servitor_of_creation` | Servitor of Creation | `mguboss` | `monster_boss2` | `1.25x` health, `1.125` scale, alternate Hornet attack set. |
| `servitors_of_creation` | Servitors of Creation | `mguboss` | `monster_supertank` | `1.25x` health. |
| `masters_of_the_machine` | Masters of the Machine | `mguboss` | `monster_shambler` | Two units, each `3x` health and `1.125` scale, with the authored precision-lightning flag. |

The remaining forceable profile IDs are `supertank`, `guardian`, `hornet`, `carrier`, `black_widow`, and
`black_widow_ii`. `tank_commander` is the automatic compact fallback profile and may also be forced explicitly.

## Horde Authored Spawn Sources

With authored spawn sources enabled, Horde converts usable campaign `monster_*` placements into inert typed
anchors instead of spawning the campaign monsters. `monster_flipper` and underwater `monster_gekk` become water
anchors; campaign boss classes become boss anchors; flying classes become aerial anchors; other combat monsters
become ground anchors.
Canonical `mgu*` bosses are recognized by map, targetname, and class, so their boss profile and active-difficulty
health/scale/armor/summon tuning survive conversion. Normal entity inhibition runs before the inert anchor is
finalized, preventing easy/medium/hard duplicate placements from appearing together.
Exact source-class matches receive a strong preference, then the director uses other compatible monster anchors,
and finally ordinary deathmatch/team/player starts. Every location must still fit the requested hull, share a PHS
with a living fighter, and satisfy the minimum distance. Decorative stands, commander bodies, and fixed monster
turrets are ignored.

Custom maps and `.ent` overrides can use the explicit `info_horde_*` entities documented in
[Level Design Guide](level-design-guide.md#horde-spawn-anchors).

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_horde_map_monster_spawns` | `1` | Enables converted campaign monster placements and explicit `info_horde_*` anchors. |
| `g_horde_map_spawn_chance` | `0.75` | Chance for a non-water, non-boss spawn to prefer an authored compatible anchor before player spawns; clamped to `0`-`1`. Bosses always try authored anchors, while water uses `g_horde_water_spawn_chance`. |
| `g_horde_map_spawn_cooldown` | `3.0` | Default seconds before the same authored anchor can be reused. An anchor's positive `horde_cooldown` overrides it. |
| `g_horde_map_spawn_min_dist` | `192` | Minimum distance from every living fighter for an authored anchor. |

## Horde Rewards And Momentum

Monster loot is decided when the monster dies. Ordinary kills can yield nothing; when they do yield an item,
the monster's weapon or combat role heavily biases the result (for example Infantry favors bullets, Gunner
favors bullets/grenades, Chick favors rockets, and Gladiator favors slugs). The profile bias still leaves room
for the general wave-appropriate loot curve. Champions have a higher but non-guaranteed strong-drop chance.
Bosses always drop a strong reward and may upgrade it to Quad, Double, Protection, Haste, Regeneration, or
Invisibility.

Consecutive credited kills build personal momentum tiers. Each tier adds kill score, improves drop chance, and
can upgrade small ammo/health/armor into a more valuable version. Death resets the streak. A fighter who
contributes at least one kill and finishes the wave without dying receives the configured survival score bonus.

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_horde_drop_chance` | `0.35` | Base chance for a regular credited monster kill to drop an item. |
| `g_horde_drop_profile_bias` | `0.85` | Chance that a successful regular drop uses the monster-specific profile instead of the general wave loot pool. |
| `g_horde_champion_drop_chance` | `1.0` | Base chance for a killed champion to drop a strong reward (or tech, when techs are enabled). |
| `g_horde_streak_step` | `5` | Consecutive credited kills required per momentum tier; values below `1` behave as `1`. |
| `g_horde_streak_max_tier` | `3` | Maximum momentum tier. `0` disables momentum bonuses. |
| `g_horde_streak_score_bonus` | `1` | Additional score per credited kill for each active momentum tier. |
| `g_horde_streak_drop_bonus` | `0.08` | Added regular/champion drop chance per momentum tier. |
| `g_horde_streak_upgrade_chance` | `0.20` | Per-tier chance to upgrade a successful regular/champion drop to its next value class. |
| `g_horde_momentum_messages` | `0` | Print momentum-tier notifications to the player. Off by default; the score and drop bonuses still apply when disabled. |
| `g_horde_wave_survival_bonus` | `2` | Score awarded after a contributed, deathless wave. `0` disables it. |
| `g_horde_wave_flawless_message` | `1` | Centerprint the flawless-wave notice to qualifying players. `0` hides it; the score bonus still applies either way. |

## Horde Item Respawn

In Horde, non-weapon items (health, ammo, armor, powerups) respawn slower than in other modes. The
effective respawn time is `base × g_dm_item_respawn_rate × g_horde_item_respawn_scale`, where `base`
is the item's built-in respawn time. **Weapons are exempt** from `g_horde_item_respawn_scale` — they
respawn at exactly `g_weapon_respawn_time` (× `g_dm_item_respawn_rate`), so the configured value is the
real in-game time. The shipped Horde factories set `g_weapon_respawn_time 60` and `g_horde_item_respawn_scale 4`.
Active held powerup and timed-tech deadlines pause from wave end until the next wave starts; Regeneration and
AutoDoc ticks pause too, so inter-round preparation neither consumes nor exploits their duration.

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_horde_item_respawn_scale` | `4` | Multiplies non-weapon item respawn time in Horde. `1` disables the slowdown; values below `1` are treated as `1`. Weapons are unaffected. |
| `g_horde_tech_reset_each_wave` | `1` | When techs are enabled in Horde, `1` clears all techs (world and held) at the countdown to the next wave and spawns a fresh set at wave start. `0` makes techs persist across waves for the whole match (spawned once at map load). |
| `g_horde_tech_relocate` | `0` | `0` = Horde techs stay where they spawn or are dropped. `1` = unpicked techs relocate to a new spot every 60s (the behavior in other modes). |
| `g_horde_tech_count` | `0` | Number of techs to spawn per Horde wave. `0` = adaptive `ceil(players / 2)`; `1`–`4` = fixed. Clamped to the four tech types. |
| `g_horde_tech_unique` | `0` | `0` = each wave's techs are picked independently, so duplicates can appear (e.g. three AutoDocs). `1` = pick a distinct, no-repeat random subset. |
| `g_horde_tech_drop_on_death` | `1` | `1` = a killed player drops their held tech. `0` = they keep it. |
| `g_horde_tech_spawn_anywhere` | `1` | `1` = scatter techs at random validated floor spots across the play area. `0` = spawn them at deathmatch spawn points (as in other modes). |
| `g_horde_tech_duration` | `30` | Seconds a tech lasts after pickup before it expires and vanishes (like Quad). `0` = techs are held until dropped/lost. The remaining time shows in the powerup timer slot. |

## Horde Late-Wave & Endless

Horde waves 1-12 are tuned content by default. Past wave 12, reached either by setting `roundlimit 0` for endless
or by a high finite `roundlimit` such as `20` or `25`, late-wave systems engage so themes stay
truthful and budgets stay playable: a theme banner only shows when the theme can field on-category
bodies, every spawn in a themed wave stays on-category, and the per-wave point budget tapers instead
of growing linearly forever. Waves up to the peak are unchanged.

With **`g_horde_late_escalation 1`** (default), post-peak waves also use a stronger budget growth factor
and ramp the concurrent alive cap (`g_horde_max_alive` base + bonus per wave, clamped). Set to `0` for
legacy post-peak behaviour (flat cap 60, `g_horde_late_wave_factor` 0.35 budget taper only). The alive
cap exists to prevent client network-buffer overflow from large homing swarms (`SZ_GetSpace`); raise
`g_horde_late_max_alive_cap` only after stress-testing on your hardware.

Champions also keep coming: up to the peak they spend the per-run budget (`g_horde_champion_max_per_run`
× `g_horde_champion_chance`) as usual, and past the peak they switch to a steady per-wave rate derived
from those same two cvars — so an endless run never runs out of champions. Raise either cvar to make
champions more frequent (early and late alike).

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_horde_content_peak_wave` | `12` | Wave where the tuned curve ends; late-wave logic fires above it. |
| `g_horde_late_escalation` | `1` | `1` = post-peak budget + alive-cap ramp (defaults below). `0` = legacy flat cap and 0.35 budget factor. |
| `g_horde_late_wave_factor` | `0.35` | Post-peak budget growth when `g_horde_late_escalation` is `0`. |
| `g_horde_late_budget_factor` | `0.6` | Post-peak budget growth when `g_horde_late_escalation` is `1`. |
| `g_horde_late_max_alive_per_wave` | `2` | Added to `g_horde_max_alive` per wave past peak when escalation is on. |
| `g_horde_late_max_alive_cap` | `70` | Ceiling for the ramped alive cap (~+17% over 60); tune after homing-swarm stress test. |
| `g_horde_weight_floor` | `0.12` | Minimum monster spawn weight past the peak; keeps cheap chaff spendable. |
| `g_horde_theme_min_monsters` | `2` | Minimum on-theme monsters required at a wave for that theme to roll. |

Endless example (escalation is on by default; only `roundlimit` is required):

```
set roundlimit 0
```

To disable post-peak escalation: `set g_horde_late_escalation 0`.

## Horde Enhanced AI

Master switch for experimental horde AI (Tier 0 orchestration in `mm_horde` plus Tier 1 vanilla hooks).
Defaults to `1` (enabled); set to `0` to restore legacy horde monster targeting and pacing.

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_horde_enhanced_ai` | `1` | Target spread, tactical hull-aware placement, adaptive pacing, per-spawn roles, periodic retargeting, extended aggro, relentless pursuit, attack stagger, and medic corpse-resurrect priority. |
| `g_horde_target_spread_weight` | `512` | How strongly monsters avoid piling onto one fighter. Under the strategy model (`g_horde_target_model 1`) this scales the target-load weight, where `512` is the reference (`1024` doubles it, `0` ignores target load entirely). Under `g_horde_target_model 0` it keeps its legacy meaning: a raw score penalty per monster already assigned, with hunters using half this value to prefer isolated fighters and heavies 37.5% to prefer healthier ones. |
| `g_horde_target_model` | `1` | Strategy-driven target selection. Monsters weigh proximity, target load, how dangerous a fighter is right now, how finishable they are, and how isolated they are, then multiply by a reachability gate covering area connectivity, climbable height, habitat, PHS, and remembered failed routes. Large monsters weigh reachability far more heavily and stop chasing stragglers into geometry they cannot follow through. `0` restores the previous role-based scorer exactly. Requires `g_horde_enhanced_ai 1`. |
| `g_horde_target_aggression` | `1.0` | Multiplier (`0`-`2`) on how much monsters prefer the most dangerous fighter. `0` ignores threat entirely; `2` produces a pronounced "hunt the carry" feel. |
| `g_horde_target_opportunism` | `1.0` | Multiplier (`0`-`2`) on how much monsters prefer a finishable fighter (hurt, unarmoured, out of ammo, helpless). `0` never prefers a weakened target. |
| `g_horde_reach_probe_budget` | `512` | Maximum PHS reachability probes per server frame, claimed all-or-nothing per monster so a shortfall can never reorder that monster's candidates. `0` disables PHS probing; reachability still uses area connectivity, climbable height, habitat, and remembered failed routes. |
| `g_horde_retarget_interval` | `8.0` | Seconds between per-monster target-load rebalance checks. Close engagements and special AI goals remain sticky. `0` disables periodic retargeting. |
| `g_horde_pursuit` | `1` | Relentless pursuit. A threat that loses sight of its fighter keeps chasing the fighter's live position instead of a player trail that has gone cold, never times out of its search, drops hold orders, and clears its movement penalties when it stops covering ground. Scripted goals, escorts, medics, noise chases, and immobile monsters are untouched. `0` restores vanilla trail pursuit. |
| `g_horde_pursuit_repath_time` | `2.0` | Maximum seconds a pursuing threat will sit out a navigation failure before retrying, replacing vanilla's 5-10 second lockout. Higher values trade responsiveness for fewer path queries; `0` retries on the next frame. Ignored when `g_horde_pursuit` is `0`. |
| `g_horde_stall_timeout` | `90` | Seconds with no damage to a counted Horde monster after all spawns are committed before recovery runs. The first timeout retargets every threat; another timeout relocates one stranded threat to a validated in-PHS combat spawn. Invalid or escaped world-space positions are recovered immediately regardless of this value. `0` disables only the timed recovery. |

## Debug-Only Weapon Balance Cvars

These cvars are available only in debug builds:

| Cvar | Purpose |
| --- | --- |
| `g_weapon_balance_dev` | Enables weapon balance development mode. |
| `g_chaingun_max_shots` | Sets maximum shots for Chaingun. |
| `g_chaingun_damage` | Sets Chaingun damage. |
| `g_chaingun_hspread` | Sets Chaingun horizontal spread. |
| `g_chaingun_vspread` | Sets Chaingun vertical spread. |
| `g_chaingun_spread_offset` | Sets Chaingun spread offset. |
| `g_machinegun_damage` | Sets Machinegun damage. |
| `g_machinegun_hspread` | Sets Machinegun horizontal spread. |
| `g_machinegun_vspread` | Sets Machinegun vertical spread. |
| `g_hyperblaster_speed` | Sets Hyperblaster projectile speed. |
| `g_railgun_damage` | Sets Railgun damage. |
| `g_rocketlauncher_damage` | Sets Rocket Launcher damage. |
| `g_rocketlauncher_speed` | Sets Rocket Launcher projectile speed. |
