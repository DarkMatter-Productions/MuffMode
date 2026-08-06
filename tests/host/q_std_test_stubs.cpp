// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"

// q_std.cpp reports discarded overlong tokens through the game import table.
// Parser host tests do not exercise that diagnostic path, but the production
// translation unit still needs the import object at link time.
local_game_import_t gi;
