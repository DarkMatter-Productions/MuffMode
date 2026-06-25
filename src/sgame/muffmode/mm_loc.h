// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;

// [MuffMode] Location callouts ("loc"): broadcast the nearest named place from
// the current map's .loc file to the caller's team (everyone in FFA).
void MM_CmdLoc(gentity_t *ent);
void MM_ClearLocCache();
