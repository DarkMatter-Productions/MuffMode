// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_debug.h"
#include "muffmode/mm_util.h"
#include <array>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <string>
#ifdef _WIN32
#include <io.h>
#include <sys/stat.h>
#else
#include <sys/stat.h>
#endif

extern cvar_t *g_muffmode_debug;

namespace muffmode::debug {

constexpr const char *kLogFile = "muffmode_debug.log";

std::ofstream log_stream;
bool log_initialized = false;

std::string FormatLocalTime(std::time_t time, const char *format)
{
	std::array<char, 64> timestamp = {};
	std::tm time_info = {};
	if (!muffmode::LocalTimeSnapshot(time, time_info))
		return "";

	if (!std::strftime(timestamp.data(), timestamp.size(), format, &time_info))
		return "";

	return timestamp.data();
}

bool ShouldTruncateLog()
{
	// Check if log file exists.
#ifdef _WIN32
	struct _stat fileInfo;
	if (_stat(kLogFile, &fileInfo) != 0)
#else
	struct stat fileInfo;
	if (stat(kLogFile, &fileInfo) != 0)
#endif
	{
		// File doesn't exist, create a new one.
		return true;
	}

	// Get file modification time.
	std::tm file_date = {};
	if (!muffmode::LocalTimeSnapshot(fileInfo.st_mtime, file_date))
		return true;

	// Get current time.
	std::tm now_date = {};
	if (!muffmode::LocalTimeSnapshot(std::time(nullptr), now_date))
		return false;

	// If file date is different from today, truncate.
	return file_date.tm_year != now_date.tm_year ||
		file_date.tm_mon != now_date.tm_mon ||
		file_date.tm_mday != now_date.tm_mday;
}

void EnsureLogInitialized()
{
	if (log_initialized)
		return;

	const bool should_truncate = ShouldTruncateLog();
	const std::ios_base::openmode mode = should_truncate ? std::ios::trunc : std::ios::app;

	log_stream.open(kLogFile, mode);
	if (log_stream.is_open()) {
		const std::string timestamp = FormatLocalTime(std::time(nullptr), "%Y-%m-%d %H:%M:%S");

		if (should_truncate) {
			log_stream << "==========================================\n";
			log_stream << "MuffMode Debug Log Started: " << timestamp << "\n";
			log_stream << "==========================================\n";
		} else {
			log_stream << "\n----------------------------------------\n";
			log_stream << "Log Continued: " << timestamp << "\n";
			log_stream << "----------------------------------------\n";
		}
		log_stream.flush();
	}
	log_initialized = true;
}

bool LogIsOpen()
{
	return log_stream.is_open();
}

bool LogEnabled()
{
	return muffmode::CvarEnabled(g_muffmode_debug);
}

void WriteLogLine(const char *category, const char *text)
{
	log_stream << "[" << FormatLocalTime(std::time(nullptr), "%H:%M:%S") << "] [" << category << "] " << text << '\n';
	log_stream.flush();
}

void WriteSeparator()
{
	log_stream << "----------------------------------------\n";
	log_stream.flush();
}

} // namespace muffmode::debug

void MuffModeLog(const char *category, const char *format, ...)
{
	if (!muffmode::debug::LogEnabled())
		return;

	muffmode::debug::EnsureLogInitialized();

	if (!muffmode::debug::LogIsOpen())
		return;

	std::array<char, 512> buffer = {};
	va_list args;
	va_start(args, format);
	std::vsnprintf(buffer.data(), buffer.size(), format, args);
	va_end(args);

	muffmode::debug::WriteLogLine(category, buffer.data());
}

void MuffModeLog_Separator()
{
	if (!muffmode::debug::LogEnabled())
		return;

	muffmode::debug::EnsureLogInitialized();

	if (!muffmode::debug::LogIsOpen())
		return;

	muffmode::debug::WriteSeparator();
}
