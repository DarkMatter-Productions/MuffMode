# MuffMode Configuration Reference

[README](../README.md) | [Player Guide](player-guide.md) | [Server Host Guide](server-host-guide.md) | [Gameplay Reference](gameplay-reference.md) | [Rulesets](rulesets.md)

This is the lookup document for MuffMode commands, cvars, vote options, and per-gametype config behavior. It is mainly for server hosts, admins, and competitive organizers who already know what setting they want to change. Players should start with the [Player Guide](player-guide.md); hosts should start with the [Server Host Guide](server-host-guide.md).

## Admin Commands

Use commands in the form `command [arg]`.

| Command | Purpose |
| --- | --- |
| `admin` | Authenticate or use admin functionality, depending on server setup. |
| `startmatch` | Force match start when warmup conditions apply. |
| `endmatch` | Force an active match to end. |
| `resetmatch` | Reset the match to warmup. |
| `map_restart` | Restart the current level/session and apply latched cvar changes. |
| `setmap <map>` | Change to a map in the configured map list. |
| `nextmap` | Force level change to the next map. |
| `gametype <gametype>` | Change gametype and reset the level. |
| `ruleset <q2re|mm|q3a|q2reb|q|qc>` | Change gameplay ruleset. |
| `shuffle` | Shuffle and balance teams, then reset the match. |
| `balance` | Balance teams without a shuffle. |
| `setteam <player>` | Force a player team change. |
| `lockteam <red|blue>` | Lock a team from being joined. Captains can lock their own team. |
| `unlockteam <red|blue>` | Unlock a team. Captains can unlock their own team. |
| `readyall` | Force all players ready during ready-up warmup. |
| `unreadyall` | Clear ready status during ready-up warmup. |
| `vote <yes|no>` | Force-pass or fail a vote when used with admin authority. |
| `forcevote` | Force the current vote result. |
| `spawn <entity> [spawn_args]` | Spawn an entity without requiring cheats. |
| `loadmotd` | Reload the message of the day file. |
| `doctor` | Print diagnostics for risky or inconsistent cvar combinations. |
| `boot <player>` | Remove a player, depending on server admin configuration. |
| `handicap <player> <weapon> <on|off>` | Apply duel weapon restrictions. |
| `handicap_clear` | Clear duel weapon restrictions. |

## Client Commands

The most useful player-facing commands are documented in the [Player Guide](player-guide.md). This quick list is provided for lookup:

| Area | Commands |
| --- | --- |
| Display | `announcer`, `fm`, `help`, `id`, `kb`, `timer` |
| Match state | `ready`, `notready`, `readyup`, `readyteam`, `forfeit`, `time-out`, `time-in` |
| Team selection | `team auto`, `team red`, `team blue`, `team free`, `team spectator` |
| Voting | `callvote`, `cv`, `vote yes`, `vote no` |
| Server info | `maplist`, `mapinfo`, `motd`, `players`, `stats` |
| Spectating | `follow`, `followkiller`, `followleader`, `followpowerup` |
| Hook | `hook`, `unhook` |
| Queueing | `mymap` |
| Reconnect recovery | `ghost <code>` |
| Captains | `captain`, `captain <player>` |

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

## Gametype Values

| Value | Short name | Gametype |
| --- | --- | --- |
| `1` | `ffa` | Deathmatch |
| `2` | `duel` | Duel |
| `3` | `tdm` | Team Deathmatch |
| `4` | `ctf` | Capture the Flag |
| `5` | `ca` | Clan Arena |
| `7` | `strike` | Capture Strike |
| `8` | `rr` | Red Rover |
| `10` | `horde` | Horde Mode |
| `12` | `instagib` | Instagib |
| `13` | `nadefest` | NadeFest |

Values `6` (`ft`), `9` (`lms`), and `11` (`ball`) are reserved or removed in the current build.

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
| `maxclients` | engine default | Maximum connected clients. |
| `maxplayers` | `16` | Maximum active players; capped to `maxclients`. |
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

## Access And Player Policy

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_allow_admin` | `1` | Allows admin powers. |
| `g_allow_custom_skins` | `1` | Allows custom player models and skins. |
| `g_allow_skin_overrides` | `1` | Allows players to re-skin enemies/teammates on their own screen via the `eskin`/`tskin` commands (team games; in duel, `eskin` re-skins your opponent). |
| `g_allow_forfeit` | `1` | Allows Duel forfeits. |
| `g_allow_grapple` | `auto` | Controls normal grapple availability. `auto` follows mode defaults; `0` disables; `1` enables. |
| `g_allow_kill` | `1` | Allows the `kill` suicide command. |
| `g_allow_mymap` | `1` | Allows MyMap queueing. |
| `g_allow_spec_vote` | `0` | Allows spectators to vote. |
| `g_allow_techs` | `auto` | Controls tech pickups in FFA/TDM/CTF/Horde. `auto` follows mode defaults; votes can force `0` or `1`. |
| `g_allow_vote_midgame` | `0` | Allows votes during active matches. |
| `g_allow_voting` | `1` | Enables voting globally. |
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

## Map And Rotation Cvars

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_map_list` | empty | Space-separated map rotation. |
| `g_map_list_shuffle` | `1` | `0` disables shuffle, `1` shuffles on wrap, `2` shuffles once per gametype session. |
| `g_map_pool` | empty | Additional voting map pool. |
| `g_gametype_cfg` | `1` | Executes `gt-[GAMETYPE].cfg` on gametype changes. |
| `g_dm_exec_level_cfg` | `0` | Executes level-specific configs when enabled. |
| `g_loc` | `1` | Enables the `loc` teammate callout command. |
| `g_loc_items` | `1` | Allows `loc` to derive a fallback location from visible weapons, powerups, or mega health when no map `.loc` file exists. |
| `g_motd_filename` | `motd.txt` | Message of the day file. |
| `g_entity_override_dir` | `maps` | Directory for entity override `.ent` files. |
| `g_entity_override_load` | `1` | Loads entity override files on map load. |
| `g_entity_override_save` | `0` | Saves entity override files when none exist. |

## Item And Gameplay Cvars

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_arena_start_armor` | `200` | Starting armor in arena modes. |
| `g_arena_start_health` | `200` | Starting health in arena modes. |
| `g_arena_dmg_armor` | `0` | Allows armor damage in arena modes. |
| `g_coop_health_scaling` | `0` | Scales co-op health by player count. |
| `g_corpse_sink_time` | `15` | Seconds before corpses sink and disappear. |
| `g_damage_scale` | `1` | Global damage scale. |
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
| `g_eyecam` | `1` | Enables EyeCam spectator behavior. |
| `g_frag_messages` | `1` | Enables frag message drawing. |
| `g_frames_per_frame` | `1` | Game frames run per server frame. Useful for performance tuning. |
| `g_huntercam` | `1` | Enables huntercam spectator behavior. |
| `g_item_bobbing` | `1` | Enables item bobbing. |
| `g_matchstats` | `0` | Enables match statistics menu/reporting features. |
| `g_muffmode_debug` | `0` | Enables `muffmode_debug.log` output. |
| `g_select_empty` | `0` | Allows selecting weapons without ammo. |
| `g_showhelp` | `1` | Prints quick explanations for game modifications. |
| `g_showmotd` | `1` | Shows message of the day behavior when enabled. |
| `g_verbose` | `0` | Enables extra console diagnostics. |

## Drop Command Flags

`g_drop_cmds` is a bitflag:

| Value | Allows |
| --- | --- |
| `1` | Dropping CTF flags. |
| `2` | Dropping powerups. |
| `4` | Dropping weapons and ammo. |

The default `7` enables all three.

## Per-Gametype Config Files

When `g_gametype_cfg` is enabled, MuffMode executes a config named for the active gametype:

| Gametype | Config |
| --- | --- |
| Free for All | `gt-FFA.cfg` |
| Duel | `gt-DUEL.cfg` |
| Team Deathmatch | `gt-TDM.cfg` |
| Capture the Flag | `gt-CTF.cfg` |
| Clan Arena | `gt-CA.cfg` |
| Capture Strike | `gt-STRIKE.cfg` |
| Red Rover | `gt-REDROVER.cfg` |
| Horde Mode | `gt-HORDE.cfg` |
| Instagib | `gt-INSTAGIB.cfg` |
| NadeFest | `gt-NADEFEST.cfg` |

These files can contain any server commands, cvar settings, map lists, or other gametype-specific setup. For examples, see the [MuffMode Server Configs repository](https://github.com/ozy24/muffmode-server-configs).

Set `roundlimit 0` after loading `gt-HORDE.cfg` to run endless Horde. See [Horde Late-Wave & Endless](#horde-late-wave--endless).

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
| `g_horde_warmup_cap` | `30` | Maximum warmup monsters alive. |
| `g_horde_max_alive` | `60` | Maximum live monsters during active waves; `0` disables the cap. |
| `g_horde_wave_spawn_delay_ms` | `500` | Delay before a new wave starts spawning monsters. |
| `g_horde_player_scale` | `1` | Scales wave budget by active fighter count. |
| `g_horde_player_scale_factor` | `0.4` | Additional budget factor per extra fighter. |
| `g_horde_player_scale_max` | `8` | Maximum fighter count considered by player scaling. |
| `g_horde_lives` | `1` | Lives granted to each fighter per wave. |
| `g_horde_mark_monsters_threshold` | `3` | Starts marking remaining monsters when the living count is at or below this value. |
| `g_horde_mark_monsters_max` | `8` | Maximum monster marker slots. |
| `g_horde_map_scale` | `1` | Enables map-size-based budget scaling. |
| `g_horde_map_scale_ref` | `4000` | Reference map span for map-size scaling. |
| `g_horde_map_scale_factor` | `0.5` | Strength of map-size scaling. |
| `g_horde_start_chainsaw` | `1` | Gives Horde players Chainfist/Chainsaw-style starting melee support when applicable. |

## Horde Champions And Themes

Champions are stronger monster variants. Themes bias a wave toward a monster category when enough matching monsters are available.

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

## Horde Item Respawn

In Horde, non-weapon items (health, ammo, armor, powerups) respawn slower than in other modes. The
effective respawn time is `base × g_dm_item_respawn_rate × g_horde_item_respawn_scale`, where `base`
is the item's built-in respawn time. **Weapons are exempt** from `g_horde_item_respawn_scale` — they
respawn at exactly `g_weapon_respawn_time` (× `g_dm_item_respawn_rate`), so the configured value is the
real in-game time. `gt-HORDE.cfg` ships `g_weapon_respawn_time 60` and `g_horde_item_respawn_scale 4`.

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_horde_item_respawn_scale` | `4` | Multiplies non-weapon item respawn time in Horde. `1` disables the slowdown; values below `1` are treated as `1`. Weapons are unaffected. |
| `g_horde_tech_reset_each_wave` | `1` | When techs are enabled in Horde, `1` clears all techs (world and held) at the countdown to the next wave and spawns a fresh set at wave start. `0` makes techs persist across waves for the whole match (spawned once at map load). |
| `g_horde_tech_relocate` | `0` | `0` = Horde techs stay where they spawn or are dropped. `1` = unpicked techs relocate to a new spot every 60s (the behavior in other modes). |
| `g_horde_tech_count` | `0` | Number of distinct techs to spawn per Horde wave. `0` = adaptive `ceil(players / 2)`; `1`–`4` = fixed. Clamped to the four tech types. |
| `g_horde_tech_drop_on_death` | `1` | `1` = a killed player drops their held tech. `0` = they keep it. |
| `g_horde_tech_spawn_anywhere` | `1` | `1` = scatter techs at random validated floor spots across the play area. `0` = spawn them at deathmatch spawn points (as in other modes). |

## Horde Late-Wave & Endless

Horde waves 1-12 are tuned content by default. Past wave 12, reached either by setting `roundlimit 0` for endless
or by a high finite `roundlimit` such as `20` or `25`, late-wave systems engage so themes stay
truthful and budgets stay playable: a theme banner only shows when the theme can field on-category
bodies, every spawn in a themed wave stays on-category, and the per-wave point budget tapers instead
of growing linearly forever. Waves up to the peak are unchanged.

Champions also keep coming: up to the peak they spend the per-run budget (`g_horde_champion_max_per_run`
× `g_horde_champion_chance`) as usual, and past the peak they switch to a steady per-wave rate derived
from those same two cvars — so an endless run never runs out of champions. Raise either cvar to make
champions more frequent (early and late alike).

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_horde_content_peak_wave` | `12` | Wave where the tuned curve ends; late-wave logic fires above it. |
| `g_horde_late_wave_factor` | `0.35` | Post-peak point-budget growth multiplier (lower = flatter late curve). |
| `g_horde_weight_floor` | `0.12` | Minimum monster spawn weight past the peak; keeps cheap chaff spendable. |
| `g_horde_theme_min_monsters` | `2` | Minimum on-theme monsters required at a wave for that theme to roll. |

## Horde Enhanced AI

Master switch for experimental horde AI (Tier 0 orchestration in `mm_horde` plus Tier 1 vanilla hooks).
Defaults to `1` (enabled); set to `0` to restore legacy horde monster targeting and pacing.

| Cvar | Default | Purpose |
| --- | --- | --- |
| `g_horde_enhanced_ai` | `1` | Target spread, spawn tactics, adaptive pacing, per-spawn roles, retarget-on-kill, extended aggro, and attack stagger. |

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
