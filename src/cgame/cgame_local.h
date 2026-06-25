// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

// Client-local definitions for the cgame module.
#pragma once

#include "shared/gameplay.h"

extern cgame_import_t cgi;
extern cgame_export_t cglobals;

#define SERVER_TICK_RATE cgi.tick_rate // in hz
#define FRAME_TIME_S cgi.frame_time_s
#define FRAME_TIME_MS cgi.frame_time_ms
