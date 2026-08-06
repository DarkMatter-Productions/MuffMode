// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <string>

struct gentity_t;

// [MuffMode] Deathmatch/co-op statusbar layout string builder.
void MM_InitStatusbar();

// [MuffMode] Top-right match info (gametype / ruleset) is a transient notice rather than a
// permanent warmup fixture: it shows for 5 seconds after a player joins a team and after the
// gametype or ruleset changes, then goes away.
void MM_MatchInfoHud_Show(gentity_t *ent);
void MM_MatchInfoHud_ShowAll();
bool MM_MatchInfoHud_Visible(const gentity_t *ent);

// [MuffMode] Dev tooling: write the most recently built CS_STATUSBAR layout (current
// gametype) to hud_dump.json in the working dir for the offline HUD previewer
// (docs-dev/hud-editor). Returns true on success; fills out_path with the file written.
bool MM_DumpStatusbar(std::string *out_path = nullptr);
