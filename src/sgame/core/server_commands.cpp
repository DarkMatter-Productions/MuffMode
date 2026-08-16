// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "debug_log.h"
#include "muffmode/mm_factory.h"
#include "muffmode/mm_graceful_shutdown.h"
#include "muffmode/mm_ghost.h"
#include "muffmode/mm_map_pool.h"
#include "muffmode/mm_maps.h"
#include "muffmode/mm_nav_bake.h"
#include "muffmode/mm_parse.h"

static void Svcmd_Test_f() {
	gi.LocClient_Print(nullptr, PRINT_HIGH, "Svcmd_Test_f()\n");
}

namespace {

bool graceful_shutdown_queued = false;
bool graceful_shutdown_ready_sent = false;
gtime_t graceful_shutdown_next_notice = 0_ms;
int graceful_shutdown_human_clients = 0;

int GracefulShutdownHumanClientCount()
{
	int count = 0;
	for (auto ent : active_clients()) {
		if (!(ent->svflags & SVF_BOT) && !ent->client->sess.is_a_bot)
			count++;
	}
	return count;
}

void GracefulShutdownNotice()
{
	gi.LocBroadcast_Print(
		PRINT_HIGH,
		"Server maintenance is queued. This is the final map; the server will close when it ends.\n");
}

void GracefulShutdownCommit()
{
	if (graceful_shutdown_ready_sent)
		return;
	graceful_shutdown_ready_sent = true;
	gi.Com_Print("Graceful shutdown reached a safe boundary; closing now.\n");
	// `quit` follows the engine's ordinary clean lobby/session teardown on both
	// retail and Q2REX dedicated hosts.
	gi.AddCommandString("quit\n");
}

} // namespace

void MM_GracefulShutdown_Reset()
{
	graceful_shutdown_queued = false;
	graceful_shutdown_ready_sent = false;
	graceful_shutdown_next_notice = 0_ms;
	graceful_shutdown_human_clients = 0;
}

void MM_GracefulShutdown_Queue()
{
	if (graceful_shutdown_ready_sent) {
		gi.Com_Print("Graceful shutdown is already committed.\n");
		return;
	}

	if (!graceful_shutdown_queued) {
		graceful_shutdown_queued = true;
		GracefulShutdownNotice();
	} else {
		gi.Com_Print("Graceful shutdown is already queued.\n");
	}
	graceful_shutdown_human_clients = GracefulShutdownHumanClientCount();
	graceful_shutdown_next_notice = level.time + 60_sec;

	if (!graceful_shutdown_human_clients)
		GracefulShutdownCommit();
}

void MM_GracefulShutdown_Cancel()
{
	if (graceful_shutdown_ready_sent) {
		gi.Com_Print("Graceful shutdown is already committed and cannot be cancelled.\n");
		return;
	}
	if (!graceful_shutdown_queued) {
		gi.Com_Print("Graceful shutdown is not queued.\n");
		return;
	}

	graceful_shutdown_queued = false;
	graceful_shutdown_next_notice = 0_ms;
	graceful_shutdown_human_clients = 0;
	gi.LocBroadcast_Print(PRINT_HIGH, "Server maintenance has been cancelled.\n");
}

bool MM_GracefulShutdown_RunFrame()
{
	if (!graceful_shutdown_queued || graceful_shutdown_ready_sent)
		return graceful_shutdown_ready_sent;

	const int human_clients = GracefulShutdownHumanClientCount();
	if (!human_clients) {
		graceful_shutdown_human_clients = 0;
		GracefulShutdownCommit();
		return true;
	}
	if (muffmode::graceful_shutdown::HumanArrivalNeedsNotice(
			graceful_shutdown_human_clients, human_clients)) {
		GracefulShutdownNotice();
		graceful_shutdown_next_notice = level.time + 60_sec;
	}
	graceful_shutdown_human_clients = human_clients;

	if (!graceful_shutdown_next_notice ||
		level.time >= graceful_shutdown_next_notice) {
		GracefulShutdownNotice();
		graceful_shutdown_next_notice = level.time + 60_sec;
	}
	return false;
}

bool MM_GracefulShutdown_HandleLevelExit()
{
	if (!graceful_shutdown_queued)
		return false;

	GracefulShutdownCommit();
	return true;
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

static void SVCmd_GracefulShutdown_f() {
	if (!muffmode::graceful_shutdown::DedicatedCommandAvailable(
			g_dedicated ? g_dedicated->integer : 0)) {
		gi.Com_Print(
			"Graceful shutdown is available only on a dedicated server.\n");
		return;
	}

	if (gi.argc() == 2) {
		MM_GracefulShutdown_Queue();
		return;
	}
	if (gi.argc() == 3 && Q_strcasecmp(gi.argv(2), "cancel") == 0) {
		MM_GracefulShutdown_Cancel();
		return;
	}

	gi.LocClient_Print(nullptr, PRINT_HIGH,
		"Usage: sv graceful_shutdown [cancel]\n");
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
	else if (Q_strcasecmp(cmd, "graceful_shutdown") == 0)
		SVCmd_GracefulShutdown_f();
	else if (Q_strcasecmp(cmd, "nav_bake") == 0)
		SVCmd_NavBake_f();
	else
		gi.LocClient_Print(nullptr, PRINT_HIGH, "Unknown server command \"{}\"\n", cmd);
}
