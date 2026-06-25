// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "debug_log.h"
#include "muffmode/mm_maps.h"
#include "muffmode/mm_nav_bake.h"
#include "muffmode/mm_parse.h"

static void Svcmd_Test_f() {
	gi.LocClient_Print(nullptr, PRINT_HIGH, "Svcmd_Test_f()\n");
}

// [Paril-KEX]
static void SVCmd_NextMap_f() {
	gi.LocBroadcast_Print(PRINT_HIGH, "$g_map_ended_by_server");
	Match_End();
}

/*
=================
ServerCommand

ServerCommand will be called when an "sv" command is issued.
The game can issue gi.argc() / gi.argv() commands to get the rest
of the parameters
=================
*/
// Server command to change to first map in g_map_list after gametype config executes
static void SVCmd_GametypeChangeMapFirst_f() {
	// [MuffMode] Thin vanilla hook; implementation lives in muffmode/mm_maps.cpp.
	MM_GametypeChangeMapFirst();
}

// [MuffMode] Generate a walk-only bot nav file for the current map.
// Usage: sv nav_bake [grid]  (grid = lattice spacing in units; default 96)
static void SVCmd_NavBake_f() {
	float grid = 0.0f;
	if (gi.argc() > 2) {
		const auto parsed_grid = MM_ParseFloatArg(gi.argv(2));
		if (!parsed_grid || *parsed_grid < 0.0f) {
			gi.LocClient_Print(nullptr, PRINT_HIGH, "Usage: sv nav_bake [non-negative grid]\n");
			return;
		}
		grid = *parsed_grid;
	}

	MM_NavBake(grid);
}

void ServerCommand() {
	const char *cmd = gi.argv(1);

	if (Q_strcasecmp(cmd, "test") == 0)
		Svcmd_Test_f();
	else if (Q_strcasecmp(cmd, "nextmap") == 0)
		SVCmd_NextMap_f();
	else if (Q_strcasecmp(cmd, "gt_changemap_first") == 0)
		SVCmd_GametypeChangeMapFirst_f();
	else if (Q_strcasecmp(cmd, "nav_bake") == 0)
		SVCmd_NavBake_f();
	else
		gi.LocClient_Print(nullptr, PRINT_HIGH, "Unknown server command \"{}\"\n", cmd);
}
