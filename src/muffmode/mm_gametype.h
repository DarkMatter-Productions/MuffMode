// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

enum gametype_t;

// [MuffMode] Gametype/ruleset orchestration hooks.
void MM_CheckRuleset();
void MM_ChangeGametype(gametype_t gt);
void MM_GTChanges();
void MM_SyncGametypeTracking();
void MM_GTSetLongName();
