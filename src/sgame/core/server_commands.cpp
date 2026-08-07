// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "debug_log.h"
#include "muffmode/mm_factory.h"
#include "muffmode/mm_ghost.h"
#include "muffmode/mm_map_pool.h"
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

// [MuffMode] Re-apply the active factory's immediate-effect overrides without a
// map change. Dedicated servers have no client to run `factory reload` from.
static void SVCmd_FactoryApply_f() {
	if (gi.argc() != 2) {
		gi.LocClient_Print(nullptr, PRINT_HIGH, "Usage: sv gt_apply\n");
		return;
	}
	if (!muffmode::factory::MM_Factory_LoadRegistry()) {
		gi.Com_Print("factories: reload failed; the previous registry is still "
			"in force.\n");
		return;
	}
	muffmode::factory::MM_Factory_ReapplyImmediate();
}

// [MuffMode] Apply the selected factory in full. The gametype session queues
// this ahead of the map command, which is what gets the overrides applied at
// all: the engine calls the game's Init once when the game starts, not per
// level, so an apply that waited for InitGame would never run for any selection
// made after the server came up.
static void SVCmd_FactoryApplyFull_f() {
	muffmode::factory::MM_Factory_ApplySelection();
}

// [MuffMode] Select a factory from a dedicated server console, where there is
// no client to run the `factory` command from.
static void SVCmd_Factory_f() {
	if (gi.argc() != 3) {
		gi.LocClient_Print(nullptr, PRINT_HIGH,
			"Usage: sv factory <id|none>\nAvailable: {}\n",
			muffmode::factory::MM_Factory_List(GT_NONE).c_str());
		return;
	}
	MM_ServerSelectFactory(gi.argv(2));
}

static void SVCmd_LoadMapPool_f() {
	if (gi.argc() != 2) {
		gi.LocClient_Print(nullptr, PRINT_HIGH, "Usage: sv load_mappool\n");
		return;
	}
	MM_ReloadMapPool(nullptr);
}

static void SVCmd_LoadMapCycle_f() {
	if (gi.argc() != 2) {
		gi.LocClient_Print(nullptr, PRINT_HIGH, "Usage: sv load_mapcycle\n");
		return;
	}
	MM_ReloadMapCycle(nullptr);
}

static void SVCmd_GhostDiagnostics_f() {
	if (gi.argc() == 2) {
		MM_Ghost_ReportDiagnostics(false);
		return;
	}
	if (gi.argc() == 3 && Q_strcasecmp(gi.argv(2), "reset") == 0) {
		MM_Ghost_ReportDiagnostics(true);
		return;
	}

	gi.LocClient_Print(nullptr, PRINT_HIGH, "Usage: sv ghost_diag [reset]\n");
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
	else if (Q_strcasecmp(cmd, "gt_apply") == 0)
		SVCmd_FactoryApply_f();
	else if (Q_strcasecmp(cmd, "gt_factory") == 0)
		SVCmd_FactoryApplyFull_f();
	else if (Q_strcasecmp(cmd, "factory") == 0)
		SVCmd_Factory_f();
	else if (Q_strcasecmp(cmd, "load_mapcycle") == 0)
		SVCmd_LoadMapCycle_f();
	else if (Q_strcasecmp(cmd, "load_mappool") == 0)
		SVCmd_LoadMapPool_f();
	else if (Q_strcasecmp(cmd, "ghost_diag") == 0)
		SVCmd_GhostDiagnostics_f();
	else if (Q_strcasecmp(cmd, "nav_bake") == 0)
		SVCmd_NavBake_f();
	else
		gi.LocClient_Print(nullptr, PRINT_HIGH, "Unknown server command \"{}\"\n", cmd);
}
