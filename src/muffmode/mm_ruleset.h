// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstdint>

struct gclient_t;
struct gentity_t;

constexpr int32_t MM_RULESET_HEALTH_CAP = 200;
constexpr int32_t MM_RULESET_ARMOR_CAP = 150;

// [MuffMode] RS_VANILLA_PLUS and RS_QC health/armor limits.
bool MM_RulesetHealthArmorCap();
void MM_ClampClientPersistHealthArmor(gclient_t *client);
void MM_ClampEntityHealthArmor(gentity_t *ent);
void MM_RulesetQ3AHealthArmorDecay(gentity_t *ent);

// [MuffMode] Ruleset-specific falling damage tuning.
int MM_RulesetFallDamageMedMin();
int MM_RulesetFallDamageFarMin();
int MM_RulesetFallDamage(bool far_fall, float delta);
