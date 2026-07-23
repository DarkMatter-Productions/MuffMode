// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstdint>

struct gclient_t;
struct gentity_t;

// [MuffMode] GT_HORDE wave spawning, scoring, and round/match orchestration.
void MM_Horde_Init();
void MM_Horde_FinalizeLevelSpawns();
void MM_Horde_RunSpawning();
void MM_Horde_BeginWave();
void MM_Horde_AdjustPlayerScore(gclient_t *cl, int32_t offset);

int  MM_Horde_CountFighters();
int  MM_Horde_WavePointBudget();
int  MM_Horde_LivingThreatCount();

bool MM_Horde_ShouldSkipEntitiesReset();
int  MM_Horde_CountdownWaveNumber();
void MM_Horde_AdvanceRoundNumber();
void MM_Horde_OnRoundCountdown();
void MM_Horde_OnRoundStarted();
void MM_Horde_OnRoundEnd();
void MM_Horde_CleanWaveTransition();
void MM_Horde_OnPlayerDeath(gentity_t *ent);
void MM_Horde_OnMonsterDamaged(gentity_t *ent, int damage);
void MM_Horde_OnMonsterKilled(gentity_t *ent);
// Promotes selected reward-neutral auxiliary monsters into live wave threats.
// Returns false only when an active Horde wave rejects the monster.
bool MM_Horde_CountAuxiliaryMonster(gentity_t *ent);
// Fail-closed placement check used by both the director and auxiliary summon paths.
bool MM_Horde_ValidateMonsterPlacement(gentity_t *ent);
void MM_Horde_NotifyEliminatedSpectator(gentity_t *ent);
void MM_Horde_PauseClientPowerups(gentity_t *ent);
bool MM_Horde_PowerupsPaused();

// Bounded Wildcard Wave and movement policy hooks used by otherwise-vanilla code.
bool  MM_Horde_MonsterEdgeDropsEnabled(const gentity_t *ent);
int   MM_Horde_ModifyDamage(const gentity_t *target, const gentity_t *attacker,
	int damage, int means_of_death);
int   MM_Horde_ModifyKnockback(const gentity_t *target, const gentity_t *attacker,
	int knockback);
void  MM_Horde_ApplyDamagePull(gentity_t *target, const gentity_t *attacker, int damage);
float MM_Horde_PlayerGravityScale();

// Converts campaign monster placements into inert typed Horde spawn anchors.
bool MM_Horde_ConvertMapMonsterSpawn(gentity_t *ent);
void SP_info_horde_spawn(gentity_t *ent);
void SP_info_horde_flying_spawn(gentity_t *ent);
void SP_info_horde_water_spawn(gentity_t *ent);
void SP_info_horde_boss_spawn(gentity_t *ent);

// CheckDMExitRules / round tick; return true when defeat intermission was queued.
bool MM_Horde_CheckAllFightersLost();

// CheckDMWarmupState hook for when every playing client has left mid-match;
// counts it as a defeat. Returns true when defeat intermission was queued.
bool MM_Horde_CheckDesertionDefeat();

// Returns true when the wave is cleared (caller should invoke Round_End).
bool MM_Horde_UpdateRoundInProgress();

// CheckDMExitRules hook; return true when intermission was queued.
bool MM_Horde_CheckMatchEnd();

bool MM_Horde_SkipFragScoreLimit();
bool MM_Horde_SkipMercyLimit();

// Least-burdened living fighter for horde monster targeting (Tier 0/1).
gentity_t *MM_Horde_PickTarget(gentity_t *from);
bool MM_Horde_MaybeRetarget(gentity_t *monster);
