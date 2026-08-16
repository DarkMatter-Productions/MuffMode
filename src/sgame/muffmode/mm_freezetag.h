// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <string>

struct gentity_t;
struct mod_t;
struct usercmd_t;

// [MuffMode] GT_FREEZE rules: death freezes fighters in place, teammates thaw
// by holding nearby, and freezing a full team wins the round.
bool MM_FreezeTag_IsActive();
bool MM_FreezeTag_IsFrozen(const gentity_t *ent);
bool MM_FreezeTag_IsFrozenViewProxy(const gentity_t *ent);
bool MM_FreezeTag_IsFrozenViewProxyVisibleTo(const gentity_t *ent, const gentity_t *viewer);
bool MM_FreezeTag_ShouldFreezeDeath(gentity_t *self, gentity_t *attacker, const mod_t &mod);
void MM_FreezeTag_FreezePlayer(gentity_t *self, gentity_t *attacker, const mod_t &mod);
void MM_FreezeTag_ClearClient(gentity_t *ent);
void MM_FreezeTag_GhostCapture(const gentity_t *ent);
void MM_FreezeTag_GhostRestore(gentity_t *ent);
void MM_FreezeTag_GhostClear(const gentity_t *ent);
void MM_FreezeTag_GhostClearAll();
bool MM_FreezeTag_GhostIsFrozen(const gentity_t *ent);
void MM_FreezeTag_DetachWorldEntities();
void MM_FreezeTag_OnRoundReset();
void MM_FreezeTag_ResetRoundPlayers();
bool MM_FreezeTag_ShouldHoldSpawnForRound();
bool MM_FreezeTag_ShouldRespawnForRoundCountdown(gentity_t *ent);

bool MM_FreezeTag_RunClientFrame(gentity_t *ent);
bool MM_FreezeTag_ClientThink(gentity_t *ent, const usercmd_t *ucmd);
bool MM_FreezeTag_BlockFrozenFootsteps(gentity_t *ent);
void MM_FreezeTag_ApplyClientEffects(gentity_t *ent);
void MM_FreezeTag_ApplyFrozenPresentation(gentity_t *ent);
void MM_FreezeTag_BotBeginFrame(gentity_t *bot);
bool MM_FreezeTag_ClientIsActiveFighter(gentity_t *ent);
bool MM_FreezeTag_UpdateRound();
bool MM_FreezeTag_BlockFrozenCommand(gentity_t *ent, bool notify = true);
int  MM_FreezeTag_ThawProgressPercent(const gentity_t *ent);
bool MM_FreezeTag_BuildRoundHudStatus(int display_round, std::string &out);
bool MM_FreezeTag_BuildHudStatus(gentity_t *viewer, std::string &out);
bool MM_FreezeTag_BuildCompactAliveHudStatus(gentity_t *viewer, std::string &out);

void MM_FreezeTag_OnFrozenDamage(gentity_t *target, gentity_t *attacker, const mod_t &mod);

// Frozen bodies are shovable. G_Physics_Toss ignores a grounded entity, so
// anything that pushes a body has to route through the impulse helper, and the
// toss integrator asks the module for the body's slide friction.
void MM_FreezeTag_ApplyBodyImpulse(gentity_t *ent);
float MM_FreezeTag_TossFriction(const gentity_t *ent);
