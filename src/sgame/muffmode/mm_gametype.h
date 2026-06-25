// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <string>

enum gametype_t;

enum class gametype_avail_t {
	Enabled,
	Disabled,
	Removed,
};

// [MuffMode] Gametype/ruleset orchestration hooks.
gametype_avail_t MM_GetGametypeAvailability(gametype_t gt);
bool             MM_IsGametypeEnabled(gametype_t gt);
gametype_t       MM_CurrentGametype();
int              MM_CurrentGametypeFlags();
bool             MM_GametypeHasFlag(int flag);
gametype_t       MM_SanitizeGametype(gametype_t gt);
void             MM_SanitizeCurrentGametype();
std::string      MM_GetEnabledGametypesList();

void MM_CheckRuleset();
void MM_ChangeGametype(gametype_t gt, bool force_cfg = false);
void MM_GTChanges();
void MM_SyncGametypeTracking();
void MM_GTSetLongName();
