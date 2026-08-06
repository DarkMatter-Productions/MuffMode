// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstdint>
#include <string_view>

struct gentity_t;
struct gitem_t;
struct gtime_t;
enum item_id_t : int32_t;
enum item_flags_t : uint32_t;

inline bool MM_IsItemOverrideCvarFor(std::string_view cvar_name,
	std::string_view map_name, std::string_view item_classname)
{
	if (item_classname.empty())
		return false;

	const auto matches_operation = [item_classname](std::string_view candidate,
		std::string_view operation) {
		return candidate.size() >= operation.size() &&
			candidate.compare(0, operation.size(), operation) == 0 &&
			candidate.substr(operation.size()) == item_classname;
	};

	if (matches_operation(cvar_name, "disable_") ||
		matches_operation(cvar_name, "replace_"))
		return true;

	if (map_name.empty() || cvar_name.size() <= map_name.size() ||
		cvar_name.compare(0, map_name.size(), map_name) != 0 ||
		cvar_name[map_name.size()] != '_')
		return false;

	const std::string_view map_scoped_name = cvar_name.substr(map_name.size() + 1);
	return matches_operation(map_scoped_name, "disable_") ||
		matches_operation(map_scoped_name, "replace_");
}

bool MM_IsKnownItemOverrideCvarName(std::string_view cvar_name,
	std::string_view map_name);

// [MuffMode] Ruleset-specific item pickup, respawn, inhibit, and autoswitch rules.
inline bool MM_ItemTouchClientMayPickup(bool client_playing, int health, bool frozen)
{
	return client_playing && health >= 1 && !frozen;
}

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

int MM_AmmoPickupCount(gentity_t *ent, int quantity);
int MM_AmmoSlugPickupCount(int quantity);

int MM_PickRespawnItemTeamIndex(int current_index, int count);
void MM_OnPowerupItemRespawned(gentity_t *ent);
bool MM_ShouldAnnouncePowerupUse();

// [MuffMode] AutoDoc tech regen (vanilla adapter names; moved from sgame/entities/items.cpp).
void Tech_ApplyAutoDoc(gentity_t *ent);
bool Tech_HasRegeneration(gentity_t *ent);
