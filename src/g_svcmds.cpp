// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "g_debug_log.h"
#include "muffmode/mm_maps.h"

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

void ServerCommand() {
	const char *cmd = gi.argv(1);

	if (Q_strcasecmp(cmd, "test") == 0)
		Svcmd_Test_f();
	else if (Q_strcasecmp(cmd, "nextmap") == 0)
		SVCmd_NextMap_f();
	else if (Q_strcasecmp(cmd, "gt_changemap_first") == 0)
		SVCmd_GametypeChangeMapFirst_f();
	else
		gi.LocClient_Print(nullptr, PRINT_HIGH, "Unknown server command \"{}\"\n", cmd);
}
