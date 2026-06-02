// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstdint>

struct gentity_t;
struct gitem_t;
enum item_id_t : int32_t;
enum item_flags_t : uint32_t;

// [MuffMode] Ruleset-specific item pickup, respawn, inhibit, and autoswitch rules.
void MM_GetItemInhibitMode(item_flags_t flags, bool &add, bool &subtract);
void MM_ClearItemInhibitFlags();

bool MM_PickupArmor(gentity_t *ent, gentity_t *other);
item_id_t MM_ClientArmorIndex(gentity_t *ent);

bool MM_AllowSmartAutoSwitch(gentity_t *ent, gitem_t *item);

gtime_t MM_PowerupInstantPickupTimeout(gentity_t *ent, bool is_dropped_from_death);
int MM_PowerupRespawnSeconds(gentity_t *ent);
bool MM_DeferInitialPowerupSpawn(gentity_t *ent);

int MM_HealthPickupCap(gentity_t *other);
int MM_HealthPickupAmount(gentity_t *ent, int quantity);
gtime_t MM_HealthPickupRespawnDelay();
bool MM_HealthPickupUsesMegaThink(gentity_t *ent, gentity_t *other);

int MM_AmmoSlugPickupCount(int quantity);
