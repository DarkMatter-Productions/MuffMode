// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

// [MuffMode] Centralized debug logging API.
// All MuffMode debug logs go to muffmode_debug.log and are gated by g_muffmode_debug.
void MuffModeLog(const char *category, const char *format, ...);
void MuffModeLog_Separator();
