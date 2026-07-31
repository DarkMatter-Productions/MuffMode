// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "muffmode/mm_arena_rules.h"

#include <cstdint>

struct gclient_t;
struct gentity_t;
struct menu_hnd_t;
enum team_t;

// MuffMode Arena is deliberately level-local. The module discovers the RA2
// "arena" entity key after SpawnEntities, then owns one independent state
// machine for every discovered room. Classic RA reserves slot zero for the
// lobby, leaving 31 playable rooms.
inline constexpr int MM_MAX_ARENAS = MM_ARENA_MAP_MAX_ROOMS;

enum class mm_arena_chat_scope_t : uint8_t {
	Arena,
	Team,
	World
};

struct mm_arena_loadout_t {
	int health = 100;
	int armor = 200;
	uint32_t weapon_mask = 0;
	int shells = 0;
	int bullets = 0;
	int grenades = 0;
	int rockets = 0;
	int cells = 0;
	int slugs = 0;
	bool infinite_ammo = false;
};

// Level/client lifecycle.
// Preflight examines the final entity lump before any entity is spawned. It
// resets map-local activation and enables Arena only for a complete RA2 map
// contract; Init then builds the live room state from the spawned entities.
void MM_Arena_PreflightMap(const char *mapname, const char *entity_lump);
bool MM_Arena_Active();
void MM_Arena_Init();
void MM_Arena_OnMatchStart();
void MM_Arena_OnMatchReset();
void MM_Arena_OnClientBegin(gentity_t *ent);
void MM_Arena_OnClientDisconnect(gentity_t *ent);
void MM_Arena_OnDeath(gentity_t *victim, gentity_t *attacker);

// Runs every discovered room. RunFrame reports whether any room has a runnable
// pairing, while CheckExitRules fully consumes GT_ARENA exit handling.
bool MM_Arena_RunFrame();
bool MM_Arena_CheckExitRules();

// RA2 map discovery and spatial isolation.
int MM_Arena_Count();
int MM_Arena_Id(const gentity_t *ent);
const char *MM_Arena_Name(int arena_id);
bool MM_Arena_ValidId(int arena_id);
bool MM_Arena_UsesTaggedMap();
// Available after map preflight, including while map entities are spawning.
bool MM_Arena_MapRoomDeclared(int arena_id);
bool MM_Arena_IsRunning(int arena_id);
bool MM_Arena_IsPaused(int arena_id);
bool MM_Arena_OrdnanceActive(int arena_id);
bool MM_Arena_SameArena(const gentity_t *first, const gentity_t *second);
bool MM_Arena_SpawnAllowed(const gentity_t *player, const gentity_t *spot);
gentity_t *MM_Arena_SelectSpawnPoint(gentity_t *player, bool spectator);
bool MM_Arena_CanUseEntity(const gentity_t *player, const gentity_t *target);
bool MM_Arena_CanFollow(const gentity_t *viewer, const gentity_t *target);
bool MM_Arena_CanInteract(const gentity_t *first, const gentity_t *second);

// Combat isolation and per-arena rules.  FilterDamage is called before armor
// calculation; false means suppress damage entirely.  AdjustProtection is
// called after armor calculation and may independently suppress health/armor
// loss while retaining knockback, matching RA3 protection modes.
bool MM_Arena_FilterDamage(gentity_t *target, gentity_t *attacker,
	int &damage, int &knockback);
void MM_Arena_AdjustProtection(gentity_t *target, gentity_t *attacker,
	int &health_damage, int &armor_damage);
bool MM_Arena_SameTeam(const gclient_t *first, const gclient_t *second);
bool MM_Arena_SameTeam(const gentity_t *first, const gentity_t *second);
bool MM_Arena_IsFighter(const gclient_t *client);
bool MM_Arena_IsEliminated(const gclient_t *client);
bool MM_Arena_CombatEnabled(const gentity_t *ent);

// Intercepts external SetTeam requests; internal Arena role assignments bypass
// it.  Existing callers may keep this contract while arena/team selection is
// performed by MM_Arena_Cmd or the join menu.
bool MM_Arena_HandleTeamRequest(gentity_t *ent, team_t desired_team, bool inactive,
	bool force, bool silent, bool &result);

// User-facing interaction.
void MM_Arena_Cmd(gentity_t *ent);
void MM_Arena_OpenJoinMenu(gentity_t *ent, menu_hnd_t *menu);
void MM_Arena_OpenRoomMenu(gentity_t *ent, menu_hnd_t *menu);
bool MM_Arena_MoveTo(gentity_t *ent, int arena_id, bool observe,
	bool force = false);
bool MM_Arena_RosterLocked(const gentity_t *ent);
bool MM_Arena_UsesTeams(const gentity_t *ent);
bool MM_Arena_CanToggleReady(const gentity_t *ent);
bool MM_Arena_TeamCommand(gentity_t *ent);
bool MM_Arena_SetTeamCommand(gentity_t *ent);
bool MM_Arena_CaptainCommand(gentity_t *ent);
bool MM_Arena_LockTeamCommand(gentity_t *ent, bool locked);
bool MM_Arena_TeamNameCommand(gentity_t *ent);
bool MM_Arena_TeamKickCommand(gentity_t *ent);
bool MM_Arena_TeamMuteCommand(gentity_t *ent, bool muted);
bool MM_Arena_SpectatorInviteCommand(gentity_t *ent, bool invited);
bool MM_Arena_SpecWhoCommand(gentity_t *ent);
bool MM_Arena_ReadyTeamCommand(gentity_t *ent);
bool MM_Arena_ReadyAllCommand(gentity_t *ent, bool ready);
bool MM_Arena_AdminStart(gentity_t *ent);
bool MM_Arena_AdminReset(gentity_t *ent);
bool MM_Arena_AdminEnd(gentity_t *ent);
bool MM_Arena_ReadyCommand(gentity_t *ent);
bool MM_Arena_SetReady(gentity_t *ent, bool ready);
bool MM_Arena_ToggleReady(gentity_t *ent);
bool MM_Arena_CallTimeout(gentity_t *ent);
bool MM_Arena_CallTimein(gentity_t *ent);
bool MM_Arena_CastVote(gentity_t *ent, const char *choice);
bool MM_Arena_ChatRecipient(const gentity_t *sender, const gentity_t *recipient,
	mm_arena_chat_scope_t scope);
bool MM_Arena_CanSendChat(gentity_t *sender, mm_arena_chat_scope_t scope);
bool MM_Arena_BypassChatFlood(const gentity_t *sender,
	mm_arena_chat_scope_t scope);
void MM_Arena_ScoreboardMessage(gentity_t *viewer, gentity_t *killer);
void MM_Arena_SetHudStats(gentity_t *ent);
void MM_Arena_SetTimerHudStats(gentity_t *ent);

// Voting/loadout/gameplay hooks.
bool MM_Arena_ClientCanVote(const gclient_t *client);
bool MM_Arena_SeriesActive(const gentity_t *ent);
bool MM_Arena_GlobalVoteBlocked(const gentity_t *ent);
bool MM_Arena_SameSquad(const gclient_t *first, const gclient_t *second);
bool MM_Arena_GetSpawnLoadout(const gclient_t *client,
	mm_arena_loadout_t &loadout);
bool MM_Arena_FallingDamageEnabled(const gentity_t *ent);
bool MM_Arena_ExcessiveEnabled(const gentity_t *ent);
bool MM_Arena_InfiniteAmmoEnabled(const gentity_t *ent);
bool MM_Arena_DamageScoringEnabled(const gentity_t *attacker);
bool MM_Arena_GrappleEnabled(const gentity_t *ent);
bool MM_Arena_FastSwitchEnabled(const gentity_t *ent);
int MM_Arena_RocketSpeed(const gentity_t *ent, int fallback);
float MM_Arena_WeaponFireRateScale(const gentity_t *ent);
bool MM_Arena_ForceFirstPerson(const gentity_t *viewer);
bool MM_Arena_FreecamAllowed(const gentity_t *viewer);
