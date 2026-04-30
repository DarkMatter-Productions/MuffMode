# Welcome to Muff Mode BETA!

## What is Muff Mode?
Muff Mode is a server-side mod for [QUAKE II Remastered](https://github.com/id-Software/quake2-rerelease-dll) providing overall enhances functionality and refinements.

### It is for Server Hosts
With a focus on multiplayer, it provides refined match handling and an extensive set of new capabilities for server owners to configure the game in a host of new ways.

### It is for Level Designers
New creative possibilities are unlocked for level designers, with an array of new map entities and keys and a range of added gametypes to design for.

### It is for the Players(tm)
Enhanced HUD info and a number of changable settings.

## Installation
1. Locate your installation. For Steam, this is normally "C:\Program Files (x86)\Steam\steamapps\common\Quake 2\rerelease".
2. Back up your baseq2/game_x64.dll.
3. Download the latest [Muff Mode release](https://github.com/themuffinator/muffmode/releases/latest).
4. Extract the entire zip to your "Quake 2" folder (not rerelease), allow file replacements (unless you already have Muff Mode, this should only replace the .dll).
5. Load the game up as normal. A range of configs can be executed to apply settings once a game has been set up.
6. Once a lobby has been set up, you can execute the included server config via ``exec muff-sv.cfg``.

## What's in the Bag?
Muff Mode includes the game logic, a server config, bot files and some map entity overrides all straight out of the bag!

## Feature Overview
- Refined HUD and scoreboard, more purpose-built for what information is needed and some extra features:
	* Frag messages (also showing position in match)
	* Dynamic miniscores with scorelimit
	* Timer and match state
	* Help texts
	* Message of the Day
	* Mini scoreboard
- A game menu for joining a match, changing for or voting on settings and viewing mod and server info.
- Comprehensive GUI menu voting system with intuitive navigation for all vote options including maps, gametypes, settings, and administrative actions.
- Team captain system for team modes: auto-assigned captains, captain transfer, and captain-managed team controls.
- A whole host of controls for admins, voting and more.
- Refined match handling with conditional progression, including: warmups, readying, countdowns, post-match delays, sudden death, overtime and more.
- Enhanced teamplay with team auto-balancing, forced balancing rules, improved team handling, communicating joined team to players, major item pickup and weapon drop POI's, and friendly fire warnings.
- Extensive controls over specific map item spawns and entity string overrides.
- EyeCam spectating, smooth and with aim prediction (mostly!)
- MyMap map queing system inspired by tastyspleen.net.
- A number of bug fixes, minor refinements, balance tweaks and many new server settings added.
- Muff Maps: official maps under development included utilizing Muff Mode's enhanced capabilities
- Many more features, see release changelogs for more info!

### Muff Maps
- Almost Lost [ALpha v1] (mm-almostlost-a1)
  Left out from Quake III: Arena's release, this level was eventually finished off and officially released as a seperate map as pro-q3tourney7. Revised in Quake Live, this map is streamlined for fast-paced FFAs and Duels.
- Arena of Death [Alpha v3] (mm-arena-a3)
  A small and simple gem from Quake III: Arena.
- Hidden Fortress [Alpha v4] (mm-fortress-a4)
  A small-to-medium-sized level connected via two teleporters. Originally created by Raster Productions for Quake III for Dreamcast, this offers the revised layout found in Quake Live.
- Longest Yard [Beta v2] (mm-longestyard-b2)
  The quintessential space map from Quake III: Arena.
- Proving Grounds [Alpha v4] (mm-proving-a4)
  A small Duel map from Quake III: Arena.
- Vertical Vengeance [Alpha v2] (mm-vengeance-a2)
  A small Duel map from Quake III: Arena.

### New Gametypes
- Horde: Battle waves of monsters, stay on top of the scoreboard while defeating up to 16 waves to be victorious! Note: currently does not handle limited lives.
- Duel: Go head-to-head with an opponent. The victor goes on to face their next opponent in the queue.
- Clan Arena: Rocket Arena's famous round-based team elimination mode - no item spawns, no self-damage and a full arsenal of weapons.
- CaptureStrike: A Threewave classic, combines Clan Arena, CTF and Counter Strike. Teams take turns attacking or defending and battle until one team is dead, or the attacking team captures the flag.
- Red Rover: Clan Arena style where teams are changed on death. When a team has been eliminated, the round ends.
- Freeze Tag: Team elimination mode where frozen players can be thawed by teammates. The round ends when one team is completely frozen. (WIP)
- ProBall: A sports-style gametype where players compete to carry a ball into the enemy goal. (WIP)

### New Game Modifications
- Vampiric Damage: Gain health by inflicting damage on your foes! No health pickups and a draining health value means the pressure is on!
- Nade Fest: Grenade-only mode.
- Weapons Frenzy: Intensified combat! Faster rates of fire, faster rockets, regenerating ammo, faster weapon switching.
- Quad Hog: Find the Quad Damage to become the Quad Hog!

### Deathmatch Refinements
- Intermission pre-delay: a one second intermission pre-delay means you can see your winning frag or capture before final scores (no damage is taken or additional scoring during this delay).
- Minimum respawn delay: a short respawn delay helps avoid accidental respawning and creates a smoother transition.
- Kill beeps and frag messages highlight your frags and rank.
 
### Offhand Hook
- Added 'hook' and 'unhook' commands to use off-hand hook. Use `g_grapple_offhand 1` to enable this.
- Players can use ``alias +hook hook; alias -hook unhook; bind mouse2 +hook`` to use it as a button command

### Gameplay Tweaks and Fixes
 - Instagib and Nade Fest now give players regeneration to recover from environmental damage, falling damage etc.
 - Quad and Protection player color shells don't change depending on team, avoids confusion
 - func_rotating: Rotating map entities now explode non-player ents such as dropped items, practically this means no more blocked rotator in dm5.
 - Player Feedback:
	* Added Fragging Spree award - broadcasted message "x is on a fragging spree with x frags" per every 10 frags achieved without dying or killing a team mate
 - Techs: fixed not being able to pick up your dropped tech
 - Blaster and Grapple now both droppable and can spawn in world
 - Current weapon is now droppable
 - Smart weapon auto-switch: now switches to SSG from SG, CG from MG, never auto-switches to chainfist.
 - Instant gametype changing (eg: from FFA to TDM)
 - DuelFire Damage has been changed to Haste: 50% faster movement, 50% faster weapon rate of fire.
 - Many more!

## Debug Logging
Muff Mode includes a centralized debug logging system that outputs detailed information to `muffmode_debug.log` in the game directory. This is controlled by the `g_muffmode_debug` cvar:

- **g_muffmode_debug**: enables debug logging to muffmode_debug.log file (default 1)

When enabled, this log captures detailed information about:
- Match state changes and transitions
- Player connections, disconnections, and team changes
- Voting processes and results
- Gametype changes and rule applications
- Entity spawning and map loading
- Error conditions and debugging information

This feature is particularly useful for server administrators and mod developers to troubleshoot issues or analyze server behavior.

## Rulesets
Alter the gameplay balance by changing the ruleset.

### Quake II Rerelease (g_ruleset 1)
Everything remains as is.

### Muff Mode (g_ruleset 2)
This ruleset aims to tackle a few significant imbalances in the original game:
 - **Plasma Beam** DM damage reduced from 15 to 10, maximum range limited to 768 units (same as LG in Q3)
 - **Railgun**: restored to 150 damage in campaigns, rail knockback is now equal to damage*2 (no difference in DM).
 - **Slugs/Railgun**: reduced slugs quantity from 10 to 5, should force more dynamic and challenging gameplay instead of an instagib approach to some matches.
 - **Rockets**: removed randomised direct rocket damage value (rand 100-120), now a consistent 120.
 - **Invulnerability** powerup has been replaced by Protection - player receives no splash damage, full protection from slime damage, third protection from lava, half direct damage after armor protection.
 - **Adrenaline**: item now also increases max health by 5 during deathmatch.
 - **Rebreather**: increased holding time from 30 to 45 seconds.
 - **Auto Doc**: regen time increased from 500 ms to 1 sec, only regens either health or armor at a time
 - **Power Armor**: CTF's 1 damage per cell now applies across deathmatch (originally 2 damage per cell in all DM bar CTF) -- this means same protection but consumes roughly twice the cells.
 - Powerup spawn rules: 120 sec respawn default, 30-45 (randomised) initial spawn delay, global spawn and pickup sounds, spawn and pickup messages.

### Quake III Arena style (g_ruleset 3)
Inspired by Quake III Arena, this ruleset aims to replicate some of the differences:
 - Start with Machinegun and Rip Saw
 - Super Shotgun replaced by Shotgun
 - Weapon stats altered, including projectile velocity, spread and damage
 - Ammo stats altered, ammo max is 200 for each type.
 - Weapon pickup rule: +1 ammo if weapon is already held.
 - Armor system: no tiers, +5 shard value, armor always provides 66% protection
 - Health and armor counts down to max health
 - Spawning health bonus of 25.
 - Removed Mega timer rule, Mega Health respawns after 60 seconds
 - **Invulnerability** powerup has been replaced by Protection - player receives no splash damage, full protection from slime damage, third protection from lava, half direct damage after armor protection.
 - Powerup spawn rules: 120 sec respawn default, 30-45 (randomised) initial spawn delay, global spawn and pickup sounds.

### Q2RE Balanced (g_ruleset 4)
A vanilla-friendly ruleset with select balance improvements:
 - **Plasma Beam** DM damage reduced from 15 to 10, maximum range limited to 768 units (same as LG in Q3)
 - **Chaingun** damage reduced to 5 (from 6 in DM / 8 in coop)
 - **Hyperblaster** projectile speed increased to 1100 (from 1000)
 - **Machinegun** damage reduced to 7 (from 8 in DM)
 - **Rocket Launcher** speed increased to 720 in deathmatch (from 650)
 - Powerup pickup and activation sounds broadcast to all players in deathmatch

### QUAKE Style (g_ruleset 5)
Inspired by Quake 1, this ruleset brings classic arena shooter elements to Quake II:
 - Start with Shotgun and Axe only (no Machinegun, Chaingun, Railgun)
 - Max ammo limits set to 200 for all types
 - **Rocket Launcher** damage randomized 100-120, speed 1000
 - **Hyperblaster** damage 15 (DM) / 20 (coop)
 - **Machinegun** damage 8 (default)
 - **Chaingun** damage 8 (default)
 - No footstep sounds when landing from jumps
 - Armor system: stronger armor protection mechanics
 - Classic weapon balance and pickup rules

### Quake Champions Style (g_ruleset 6)
Modern arena shooter mechanics inspired by Quake Champions:
 - Random starting weapon (Shotgun, Machinegun, or Hyperblaster) with 50 ammo
 - Max ammo limits: 200 bullets, 200 for other types
 - **Rocket Launcher** damage 100, speed 750
 - **Hyperblaster** damage reduced to 12, speed 1100
 - **Machinegun** damage 6
 - **Chaingun** damage 6
 - **Railgun** damage 80
 - **Plasma Beam** damage 15 (default)
 - Faster overall gameplay pace and modern weapon balance
 - Enhanced movement mechanics and timing
 
## Commands and Variables

### Admin Commands
Use **[command] [arg]** for the below listed admin commands:
 - **startmatch**: force the match to start, requires warmup.
 - **endmatch**: force the match to end, requires a match in progress.
 - **resetmatch**: force the match to reset to warmup, requires a match in progress.
 - **lockteam [red/blue]**: locks a team from being joined. Admins can target a team by arg; captains can lock their own team.
 - **unlockteam [red/blue]**: unlocks a locked team. Admins can target a team by arg; captains can unlock their own team.
 - **setteam [clientnum/name]**: forces a team change for a client
 - **shuffle**: shuffles and balances the teams, resets the match. Requires a team gametype.
 - **balance**: balances the teams without a shuffle, this switches last joined players from stacked team. Requires a team gametype.
 - **vote [yes/no]**: passes or fails a voting in progress
 - **spawn [entity_name] [spawn_args]**: spawns an entity, works the same as normal spawn server command but allows admins to do this without cheats enabled
 - **map**: changes the level to the specified map, map needs to be a part of the map list.
 - **nextmap**: forces level change to the next map.
 - **map_restart**: restarts current level and session, applies latches cvar changes
 - **gametype [gametype_name]**: changes gametype to selected option, then resets the level
 - **ruleset <q2re|mm|q3a|q2reb|qc>**: changes gameplay style
 - **doctor**: read-only diagnostics command that runs all checks and reports cvar misconfigurations and risky combinations with suggested fixes
 - **readyall**: force all players to ready status (during readying warmup status)
 - **unreadyall**: force all players to NOT ready status (during readying warmup status)

### Client Commands - Player Configuration
Use **[command] [arg]** for the below listed client commands:
 - **announcer**: toggles support of QL match announcer events (uses vo_evil set, needs converting to 22KHz PCM WAV)
 - **fm**: toggle frag messages
 - **help**: toggle help text drawing
 - **id**: toggle crosshair ID drawing
 - **kb**: toggle kill beeps
 - **timer**: toggle match timer drawing
 
### Client Commands - Gameplay
 - **hook/unhook**: hook/unhook off-hand grapple
 - **captain**: in team modes, claim captain if vacant, or show current team captain.
 - **captain [clientname/clientnum]**: transfer captain status to a teammate (captain only).
 - **followkiller** : auto-follow killers when spectating (disabled by default)
 - **followleader** : when spectating, auto-follows leading player
 - **followpowerup** : auto-follows player picking up powerups when spectating (disabled by default)
 - **forfeit**: forfeits a match (currently only in duels, requires g_allow_forfeit 1).
 - **ready/notready**: sets ready status.
 - **readyteam**: readies your whole team (captain or admin only, team modes).
 - **readyup**: toggles ready status.
 - **callvote/cv**: calls a vote (use vote commands).
 - **vote [yes/no]**: vote or veto a callvote.
 - **maplist**: show server map list.
 - **motd**: print the message of the day.
 - **mymap**: add a map to the queue, must be a valid map from map list.
 - **team [arg]**: selects a team, args:
	- **blue/b**: select blue team
	- **red/r**: select red team
	- **auto/a**: auto-select team
	- **free/f**: join free team (non-team games)
	- **spectator/s**: spectate
 - **time-in** : cuts a time out short
 - **time-out** : call a time out, only 1 allowed per player and lasts for value set by g_dm_timeout_length (in seconds). **g_dm_timeout_length 0** disables time outs
 - **follow [clientname/clientnum]**: follow a specific player.

### Vote Commands
Use **callvote [command] [arg]** for the below listed vote commands:
 - **map**: changes the level to the specified map, map needs to be a part of the map list.
 - **nextmap**: forces level change to the next map.
 - **restart**: force the match to reset to warmup, requires a match in progress.
 - **gametype**: changes gametype to the specified type (ffa|duel|tdm|ctf|ca|ft|strike|rr|lms|horde|ball|instagib|nadefest)
 - **timelimit**: changes timelimit to the minutes specified.
 - **scorelimit**: changes scorelimit to the value specified.
 - **shuffle**: shuffles and balances the teams, resets the match. Requires a team gametype.
 - **balance**: balances the teams without a shuffle, this switches last joined players from stacked team. Requires a team gametype.
 - **unlagged**: enables or disables lag compensation.
 - **cointoss**: randomly returns either HEADS or TAILS.
 - **random**: randomly returns a number from 2 to argument value, 100 max.
 - **ruleset <q2re|mm|q3a|q2reb|qc>**: changes gameplay style

Voting control cvars:
- **g_allow_vote_midgame**: allows/prohibits starting votes during an active match state (default 0)
- **g_allow_voting**: global voting enable/disable switch; when 0, voting is fully disabled (default 0)

### Cvar Changes
 - g_dm_spawn_farthest: added an option, valid values are as follows:
	- 0: high random (selects random spawn point except the 2 nearest)
	- 1: half farthest (selects random spawn point from the furthest 50% of spawn points
	- 2: spawn farthest to current position
 - g_teamplay_force_join: renamed to g_dm_force_join
 - sv_*: all mod-based sv_* cvars renamed g_*
 - g_teleporter_nofreeze: renamed to g_teleporter_freeze, values do opposite effect (value of 1 freezes player), default is 0 (no freeze)
 - deathmatch: default changed to 1

### New Cvars
 - **bot_name_prefix**: allows changing bot name prefixes (blank to remove) (default "B|")
 - **g_allow_admin**: allows administrative powers (default 1)
 - **g_allow_custom_skins**: when set to 0, reverts any custom player models or skins to stock replacements (default: 0)
 - **g_allow_forfeit**: Allows a player to forfeit the match, currently only for Duels (default 1)
 - **g_allow_kill**: enables use of 'kill' suicide command (default 1)
 - **g_allow_mymap**: allow mymap (map queuing function) (default 1)
 - **g_allow_spec_vote**: Allows/prohibits voting from spectators. (default 1)
 - **g_allow_vote_midgame**: Allows/prohibits voting during a match. (default 0)
 - **g_allow_voting**: General control over voting, 0 prohibits any voting. (default 0)
 - **g_arena_start_armor**: sets starting armor value in arena modes, range from 1-999, value affects armor tier (default 200)
 - **g_arena_start_health**: sets starting health value in arena modes, range from 1-999 (default 200)
 - **g_arena_dmg_armor**: when set to 1, allows armor damage in arena modes (default 0)
 - **g_coop_health_scaling**: scales health in co-op mode based on player count, range 0-1 (default 0)
 - **g_corpse_sink_time**: sets time in seconds for corpses to sink and disappear (default: 60)
 - **g_dm_allow_no_humans**: when set to 1, allows matches to start or continue with only bots (default 1)
 - **g_dm_do_readyup**: Enforce players to ready up to progress from match warmup stage (requires g_dm_do_warmup 1). (default 0)
 - **g_dm_do_warmup**: Allow match warmup stage. (default 1)
 - **g_dm_force_join**: replaces g_teamplay_force_join, the menu forces the cvar change so this gets around that, it now applies to regular DM too so the change makes sense.
 - **g_dm_holdable_adrenaline** : when set to 1, allows holdable Adrenaline during deathmatch (default 1)
 - **g_dm_no_self_damage**: when set to 1, disables any self damage after calculating knockback (default: 0)
 - **g_dm_overtime**: Set stoppage time for each overtime session in seconds. Currently only applies to Duels. (default 120)
 - **g_dm_tie_max_time**: sets the maximum total tied-overtime duration in seconds before forcing match resolution, 0 disables the limit. (default 1800)
 - **g_dm_powerup_drop**: when set to 1, drops carried powerups upon death (default: 1)
 - **g_dm_powerups_minplayers**: Sets minimum current player load to allow powerup pickups, 0 to disable (default 0)
 - **g_dm_respawn_delay_min**: the counterpart to g_dm_force_respawn_time, this sets a minimum respawn delay after dying (default: 1)
 - **g_dm_respawn_point_min_dist**: sets minimum distance to respawn away from previous spawn point (default: 256, max = 512, 0 = disabled)
 - **g_dm_respawn_point_min_dist_debug**: when set to 1, prints avoiding spawn points when g_dm_respawn_point_min_dist is used (default: 0)
 - **g_dm_spawnpads**: Controls spawning of deathmatch spawn pads, removes pads when set to 0, 1 only removes in no item game modes, 2 forces pads in all dm matches. (default: 1)
 - **g_drop_cmds**: bitflag operator, allows dropping of item types (default 7):
	&1: allow dropping CTF flags
	&2: allow dropping powerups
	&4: allow dropping weapons and ammo
 - **g_fast_doors**: When set to 1, doubles the default speed of standard and rotating doors (default 1)
 - **g_frames_per_frame**: controls how many game frames to run per server frame, useful for performance tuning (default 1)
 - **g_frag_messages**: draw frag messages (default 1)
 - **g_gametype_cfg**: when set to 1, executes gametype-specific config files when gametype changes (default 1)

### GT- Config File System
When `g_gametype_cfg` is enabled (default), the game automatically executes configuration files specific to each gametype when the gametype changes via voting or admin commands. This allows server administrators to set up gametype-specific settings, maps, and rules.

The config files follow the naming convention: `gt-[GAMETYPE].cfg`

Available config files:
- `gt-FFA.cfg` - Free for All settings
- `gt-DUEL.cfg` - Duel settings  
- `gt-TDM.cfg` - Team Deathmatch settings
- `gt-CTF.cfg` - Capture the Flag settings
- `gt-CA.cfg` - Clan Arena settings
- `gt-FT.cfg` - Freeze Tag settings
- `gt-STRIKE.cfg` - CaptureStrike settings
- `gt-REDROVER.cfg` - Red Rover settings
- `gt-LMS.cfg` - Last Man Standing settings
- `gt-HORDE.cfg` - Horde mode settings
- `gt-BALL.cfg` - ProBall settings
- `gt-INSTAGIB.cfg` - Instagib settings
- `gt-NADEFEST.cfg` - Nade Fest settings

These config files should be placed in the main game directory and can contain any server commands, cvar settings, map lists, or other configurations specific to that gametype. The system ensures configs are only executed when the gametype actually changes, not on every map load.

For a complete set of ready-to-use server configs with per-gametype settings, see the [MuffMode Server Configs repository](https://github.com/ozy24/muffmode-server-configs).
 - **g_gametype**: cvar sets gametype by index number, this is the current list:
	0: Campaign (not used at present, use deathmatch 0 as usual)
	1. Free for All
	2. Duel
	3. Team Deathmatch
	4. Capture the Flag
	5. Clan Arena
	6. Freeze Tag (WIP)
	7. CaptureStrike
	8. Red Rover
	9. Last Man Standing
	10. Horde
	11. ProBall (WIP)
	12. Instagib
	13. Nade Fest
 - **g_horde_starting_wave**: sets the starting wave number for Horde mode (default 1)
 - **g_inactivity**: Values above 0 enables an inactivity timer for players, specifying number of seconds since last input to point of flagging the player as inactive. A warning is sent to the player 10 seconds before triggering and once triggered, the player is moved to spectators. Inactive clients are noted as such using the 'players' command. (default: 120)
 - **g_instagib_splash**: enables a non-damaging explosion from railgun shots in instagib, allows for rail jumping or knocking foes about (default 0)
 - **g_knockback_scale**: scales all knockback resulting from damage received (default 1.0)
 - **g_ladder_steps**: Allow ladder step sounds, 1 = only in campaigns, 2 = always on (default 1)
 - **g_match_lock**: when set to 1, prohibits joining the match while in progress (default 0)
 - **g_map_list**: space-separated list of maps for server rotation (default: "")
 - **g_map_list_shuffle**: controls map list shuffling behavior (default 1):
	0: no shuffling
	1: shuffle every time the list wraps around
	2: shuffle once per gametype session
 - **g_map_pool**: additional map pool for voting (default: "")
 - **g_motd_filename**: points to filename of message of the day file, reverts to default when blank (default motd.txt)
 - **g_mover_speed_scale**: sets speed scaling factor for all movers in maps (doors, rotators, lifts etc.) (default: 1.0f)
 - **g_no_powerups**: disable powerup pickups (Quad, Protection, Double, Haste, Invisibility, etc.)
 - **g_mapspawn_no_bfg**: when set to 1, prevents BFG from spawning in maps (default 0)
 - **g_mapspawn_no_plasmabeam**: when set to 1, prevents Plasma Beam from spawning in maps (default 0)
 - **g_owner_auto_join**: when set to 0, avoids auto-joining a match as lobby owner (default 1)
 - **g_owner_push_scores**: when set to 1, automatically shows scores to lobby owner on join (default 0)
 - **g_muffmode_debug**: enables debug logging to muffmode_debug.log file (default 1)
 - **g_round_countdown**: sets round countdown time (in seconds) in round-based gametypes (default 10)
 - **g_ruleset**: gameplay rules (default 2):
	1. Quake II Rerelease
	2. Muff Mode (rebalanced Q2Re)
	3. Quake III Arena style
	4. Q2RE Balanced
	5. QUAKE style
	6. Quake Champions style
 - **g_showhelp**: when set to 1, prints a quick explanation about game modifications to players. (default: 1)
 - **g_starting_armor**: sets starting armor for players on spawn (0-999) (default 0)
 - **g_starting_health**: sets starting health for players on spawn (1-999) (default 100)
 - **g_teamplay_allow_team_pick**: When set to 0, denies the ability to pick a specific team during teamplay. This changes the join menu accordingly. (default 0)
 - **g_teamplay_auto_balance**: Set to 1, enforces team rebalancing during a match. The last joined player(s) of the stacked team switches teams but retain their scores. (default 1)
 - **g_teamplay_force_balance**: When set to 1, prohibits joining a team with too many players. (default: 0)
 - **g_teamplay_item_drop_notice**: When set to 1, sends team notice of item drops. (default 1)
 - **g_teleporter_freeze**: When set to 0, does not freeze player velocity when teleporting. (default: 0)
 - **g_vampiric_exp_min**: with vampiric damage enabled, sets expiration minimum health value (default 0)
 - **g_vampiric_health_max**: sets maximum health cap from vampiric damage (default 999)
 - **g_vampiric_percentile**: set health percentile bonus for vampiric damage (default 0.67f)
 - **g_vote_flags**: Bitmask to disable specific vote options. (default 0)
 - **g_vote_limit**: Sets maximum number of votes per match per client, 0 for no limit. (default 3)
 - **g_votable_gametypes**: Space-separated list of gametype short names that can be voted on. If empty, all implemented gametypes are available for voting. Example: "ffa duel tdm ctf" (default: "")
 - **g_votable_rulesets**: Space-separated list of ruleset short names that can be voted on. If empty, all implemented rulesets are available for voting. Example: "q2re mm q3a q2reb qc" (default: "")
 - **g_warmup_ready_percentage**: in match mode, sets percentile of ready players out of total players required to start the match. Set to 0 to disable readying up. (default: 0.51f)
 - **g_weapon_projection**: changes weapon projection offset. 0 = normal, 1 = always force central handedness, 2 = force central view projection. looks strange with view weapons. (default: 0)
 - **g_weapon_respawn_time**: sets respawn time for weapons in seconds (default 30)
 - **hostname**: set string for server name, this gets printed at top of game menu for all to see. Limit this to 26 chars max.
 - **maxplayers**: Set max number of players in the game (ie: non-spectators), it is capped to maxclients. In team games, team max size will be maxplayers/2 and rounded down.
 - **mercylimit**: Sets score gap limit to end match, 0 to disable (default 0)
 - **noplayerstime**: Sets time in minutes in which there have been no players to force a change of map, 0 to disable (default 0)
 - **roundlimit**: sets number of round wins to win the match in round-based gametypes (default 8)
 - **roundtimelimit**: sets round time limit (in minutes) in round-based gametypes (default 2)

### DEBUG-ONLY Weapon Balance Cvars
The following cvars are only available in DEBUG builds for testing and development:
 - **g_weapon_balance_dev**: enables weapon balance development mode (default 0)
 - **g_chaingun_max_shots**: sets maximum shots for chaingun (default 0)
 - **g_chaingun_damage**: sets chaingun damage value (default 0)
 - **g_chaingun_hspread**: sets chaingun horizontal spread (default 0)
 - **g_chaingun_vspread**: sets chaingun vertical spread (default 0)
 - **g_chaingun_spread_offset**: sets chaingun spread offset (default 0)
 - **g_machinegun_damage**: sets machinegun damage value (default 0)
 - **g_machinegun_hspread**: sets machinegun horizontal spread (default 0)
 - **g_machinegun_vspread**: sets machinegun vertical spread (default 0)
 - **g_hyperblaster_speed**: sets hyperblaster projectile speed (default 0)
 - **g_railgun_damage**: sets railgun damage value (default 0)
 - **g_rocketlauncher_damage**: sets rocket launcher damage value (default 0)
 - **g_rocketlauncher_speed**: sets rocket launcher projectile speed (default 0)

## Level Controls
 
### New Items
- Personal Teleporter (item_teleporter): holdable item for deathmatch, teleports the players to a spawn point upon activation.
- Small ammo items for shells, bullets, rockets, cells and slugs (ie: ammo_bullets_small)
- Large ammo items for shells, bullets and cells (ie: ammo_bullets_large)
- Regeneration (item_regen): 30 second powerup regenerates your health up to 2x max health

### Map Tweaks
Some entity overrides are included which add some subtle ambient sounds, mover sounds, intermission cams and gametype-specific item tweaks.
 
### Map Entity Controls
 * Map Item Replacement Control:
	 - **<disable/replace>_[classname]** and **[mapname]_<disable/replace>_[classname]** user cvars to remove or replace specific DM map items (by classname) or only in specific maps if desired
 * Save and load .ent files to override entire map entity string, located in baseq2/$g_entity_override_dir$/[mapname].ent:
	 - **g_entity_override_dir**: overrides entity override file subdir within baseq2 (default: maps)
	 - **g_entity_override_save**: when set to 1, will save entity override file upon map load (should one not already be loaded) (default: 0)
	 - **g_entity_override_load**: when set to 1, will load entity override file upon map load (default: 1)
 * New entity keys**: "gametype" and "not_gametype": set conditional list of gametypes to respectively spawn or not spawn the entity in. The list can be comma or space separated. The following values correspond to a particular gametype:
	campaign: Campaigns
	ffa: Deathmatch
	tournament: Duel
	team: Team Deathmatch
	ctf: Capture the Flag
	ca: Clan Arena
	ft: Freeze Tag
	rr: Red Rover
	lms: Last Man Standing
	horde: Horde Mode
		Example: "gametype" "ffa tournament" - this will spawn the entity only in deathmatches and duels.
 * New entity keys**: "**notteam**" and "**notfree**": removes an entity from team gametypes or non-team gametypes respectively.
	Example: "**notteam**" "1" - the entity will not spawn in team gametypes such as TDM, CTF, FreezeTag and Clan Arena.
 * misc_teleporter: **"mins"/"maxs" "x y z"** entity keys to override teleport trigger size, removes teleporter pad if either keys are set
 * new item spawnflag & 8: item spawns in suspended state (does not drop to floor)
 * Hacky Map Fixes:
	* bunk1: button for lift to ware2 now has a wait of -1 (never returns), stops co-op players from pushing the button again and toggling the lift!
 * "nobots" and "nohumans": keys for info_player_deathmatch to avoid using for bots or humans respectively
	
### Entity Keys
* SPAWNFLAGS:
	- spawnflags & 8: suspends items (don't fall to ground)
* Worldspawn:
	- **author** and **author2**: sets level author information, this can be seen in the server info submenu.
	
### Entity Changes
 - misc_nuke: now applies nuke effect (screen flash, earthquake)
 - trigger_push: target a target_position/info_notnull to set a direction and apogee like in Q3, no target reverts to original behaviour
 - trigger_key: does not remove inventory item in deathmatch, deathmatch or spawnflags 1 now allows multiple uses.
 - trigger_coop_relay: annoying "all players must be present" feature in co-op has been removed as it proves a game breaker in games with afk players, always treated like a trigger_relay now
	
### New Entities
- target_remove_powerups:
	Takes away all the activator's powerups, techs, held items, keys and CTF flags.
	
- target_remove_weapons:
	Takes away all the activator's weapons and ammo (except blaster).
	BLASTER : also remove blaster
	
- target_give:
	Gives the activator the targetted item.
	
- target_delay:
	Sets a delay before firing its targets.
	"wait" seconds to pause before firing targets.
	"random" delay variance, total delay = delay +/- random seconds
	
- target_print:
	Sends a center-printed message to clients.
	"message"	text to print
	spawnflags: REDTEAM BLUETEAM PRIVATE
	If "PRIVATE", only the activator gets the message. If no checks, all clients get the message.
	
- target_setskill:
	Set skill level.
	"message" : skill level to set to (0-3)
	Skill levels are:
	0 = Easy
	1 = Medium
	2 = Hard
	3 = Nightmare/Hard+
	
- target_score:
	"count" number of points to adjust by, default 1.
	The activator is given this many points.
	spawnflags: TEAM
	TEAM : also adjust team score
	
- target_teleporter:
	The activator will be teleported to the targetted destination.
	If no target set, it will find a player spawn point instead.
	
- target_relay:
	Correctly named trigger_relay.
	
- target_kill:
	Kills the activator.
	
- target_cvar:
	When targetted sets a cvar to a value.
	"cvar" : name of cvar to set
	"cvarValue" : value to set cvar to
	
- target_shooter_grenade:
	Fires a grenade in the set direction when triggered.
	dmg		default is 120
	speed	default is 600
	
- target_shooter_rocket:
	Fires a rocket in the set direction when triggered.
	dmg		default is 120
	speed	default is 600
	
- target_shooter_bfg:
	Fires a BFG projectile in the set direction when triggered.
	dmg			default is 200 in DM, 500 in campaigns
	speed		default is 400
	
- target_shooter_prox:
	Fires a prox mine in the set direction when triggered.
	dmg			default is 90
	speed		default is 600
	
- target_shooter_ionripper:
	Fires an ionripper projectile in the set direction when triggered.
	dmg			default is 20 in DM and 50 in campaigns
	speed		default is 800
	
- target_shooter_phalanx:
	Fires a phalanx projectile in the set direction when triggered.
	dmg			default is 80
	speed		default is 725
	
- target_shooter_flechette:
	Fires a flechette in the set direction when triggered.
	dmg			default is 10
	speed		default is 1150
	
- target_position:
	Alias for info_notnull.
	
- trigger_deathcount:
	Fires targets only if minimum death count has been achieved in the level.
	Deaths considered are monsters during campaigns and players during deathmatch.
	"count"	minimum number of deaths required (default 10)
	spawnflags: REPEAT
	REPEAT : repeats per every 'count' deaths
	
- trigger_no_monsters:
	Fires targets only if all monsters have been killed or none are present.
	Auto-removed in deathmatch (except horde mode).
	ONCE : will be removed after firing once
	
- trigger_monsters:
	Fires targets only if monsters are present in the level.
	Auto-removed in deathmatch (except horde mode).
	ONCE : will be removed after firing once
	
## Roadmap:
- tastyspleen.net's mymap system: add support for dm flags
- Freeze Tag gametype (WIP)
- Server-side player configs, stats, Elo, ranked matches, Elo team balancing (WIP)
- Gladiator bots
- Menu overhaul, adding voting, full admin controls, mymap, player config

## Credits:
- Quake II Sanctuary community for testing. A shout out particularly to Sata, TurboPtys_drk and Jobe for their help.
- The Stingy Hat Games YouTube channel for their excellent modding tutorial, without it I would never be able to compile the damned source!
- Nightdive team for the impressive remaster, also some on the team who patiently answered all my annoying modding questions (particularly Paril, sponge, Edward850)
- Paril for some of the Horde Mode code (really just the spawn code), [link to Paril's mod](https://github.com/Paril/q2horde) 
- id Software, both for Quake II and Quake III Arena (some code ported from the latter)
- ceeeKay for the eyecam code from Q2Eaks
- The Q2Re player community for bug spotting and general feedback
