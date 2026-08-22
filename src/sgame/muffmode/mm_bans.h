// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

// Engine-facing ban list, keyed by social_id. Persisted to baseq2/bans.cfg.
// Pure record logic lives in mm_bans_rules.h; this exposes the live operations
// used by connect-time enforcement (lifecycle.cpp) and admin commands
// (commands.cpp). Bots are never banned (they carry no stable social_id).

#include <string_view>

struct gentity_t;

// Load the ban file into memory. Called once from InitGame().
void MM_Bans_Load();

// True if this social_id is on the ban list. Empty/null ids are never banned.
bool MM_IsBanned(const char *social_id);

// Add (or refresh name/reason of) a ban and persist. Returns true if newly added.
bool MM_Bans_Add(std::string_view social_id, std::string_view name, std::string_view reason);

// Remove a ban by social_id and persist. Returns true if a record was removed.
bool MM_Bans_Remove(std::string_view social_id);

// Print the current ban list to a client (or console when ent is null).
void MM_Bans_Print(gentity_t *ent);
