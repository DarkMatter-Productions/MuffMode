// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_debug.h"
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <fstream>
#ifdef _WIN32
#include <io.h>
#include <sys/stat.h>
#else
#include <sys/stat.h>
#endif

extern cvar_t *g_muffmode_debug;

namespace {
std::ofstream g_muffmode_log;
bool g_log_initialized = false;

bool ShouldTruncateLog()
{
	// Check if log file exists.
#ifdef _WIN32
	struct _stat fileInfo;
	if (_stat("muffmode_debug.log", &fileInfo) != 0)
#else
	struct stat fileInfo;
	if (stat("muffmode_debug.log", &fileInfo) != 0)
#endif
	{
		// File doesn't exist, create a new one.
		return true;
	}

	// Get file modification time.
	time_t fileTime = fileInfo.st_mtime;
	struct tm *fileTm = localtime(&fileTime);
	if (!fileTm)
		return true;
	struct tm fileDate = *fileTm;

	// Get current time.
	time_t now = time(nullptr);
	struct tm *nowTm = localtime(&now);
	if (!nowTm)
		return false;
	struct tm nowDate = *nowTm;

	// If file date is different from today, truncate.
	if (fileDate.tm_year != nowDate.tm_year || fileDate.tm_mon != nowDate.tm_mon || fileDate.tm_mday != nowDate.tm_mday)
		return true;

	return false;
}

void EnsureLogInitialized()
{
	if (!g_log_initialized)
	{
		bool shouldTruncate = ShouldTruncateLog();
		std::ios_base::openmode mode = shouldTruncate ? std::ios::trunc : std::ios::app;

		g_muffmode_log.open("muffmode_debug.log", mode);
		if (g_muffmode_log.is_open())
		{
			time_t now = time(nullptr);
			char timestamp[64];
			strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

			if (shouldTruncate)
			{
				g_muffmode_log << "==========================================\n";
				g_muffmode_log << "MuffMode Debug Log Started: " << timestamp << "\n";
				g_muffmode_log << "==========================================\n";
			}
			else
			{
				g_muffmode_log << "\n----------------------------------------\n";
				g_muffmode_log << "Log Continued: " << timestamp << "\n";
				g_muffmode_log << "----------------------------------------\n";
			}
			g_muffmode_log.flush();
		}
		g_log_initialized = true;
	}
}
} // namespace

void MuffModeLog(const char *category, const char *format, ...)
{
	if (!g_muffmode_debug || !g_muffmode_debug->integer)
		return;

	EnsureLogInitialized();

	if (!g_muffmode_log.is_open())
		return;

	time_t now = time(nullptr);
	char timestamp[32];
	strftime(timestamp, sizeof(timestamp), "%H:%M:%S", localtime(&now));

	char buffer[512];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	g_muffmode_log << "[" << timestamp << "] [" << category << "] " << buffer << std::endl;
	g_muffmode_log.flush();
}

void MuffModeLog_Separator()
{
	if (!g_muffmode_debug || !g_muffmode_debug->integer)
		return;

	EnsureLogInitialized();

	if (!g_muffmode_log.is_open())
		return;

	g_muffmode_log << "----------------------------------------\n";
	g_muffmode_log.flush();
}
