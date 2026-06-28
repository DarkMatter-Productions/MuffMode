// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <string>

// [MuffMode] Deathmatch/co-op statusbar layout string builder.
void MM_InitStatusbar();

// [MuffMode] Dev tooling: write the most recently built CS_STATUSBAR layout (current
// gametype) to hud_dump.json in the working dir for the offline HUD previewer
// (docs-dev/hud-editor). Returns true on success; fills out_path with the file written.
bool MM_DumpStatusbar(std::string *out_path = nullptr);
