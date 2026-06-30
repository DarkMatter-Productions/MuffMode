// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

inline int64_t MM_AbsMilliseconds(int msec) noexcept {
	const int64_t value = msec;
	return value < 0 ? -value : value;
}

inline std::string MM_FormatMatchTime(int msec) {
	char buffer[48];
	const int64_t ms = MM_AbsMilliseconds(msec);
	const int64_t total_seconds = ms / 1000;
	const int64_t seconds = total_seconds % 60;
	const int64_t total_minutes = total_seconds / 60;
	const int64_t minutes = total_minutes % 60;
	const int64_t hours = total_minutes / 60;
	const char *sign = msec < 0 ? "-" : "";

	if (hours > 0)
		std::snprintf(buffer, sizeof(buffer), "%s%lld:%02lld:%02lld", sign, static_cast<long long>(hours), static_cast<long long>(minutes), static_cast<long long>(seconds));
	else
		std::snprintf(buffer, sizeof(buffer), "%s%02lld:%02lld", sign, static_cast<long long>(minutes), static_cast<long long>(seconds));

	return buffer;
}

inline std::string MM_FormatMatchTimeMs(int msec) {
	char buffer[48];
	const int64_t ms = MM_AbsMilliseconds(msec);
	const int64_t milliseconds = ms % 1000;
	const int64_t total_seconds = ms / 1000;
	const int64_t seconds = total_seconds % 60;
	const int64_t total_minutes = total_seconds / 60;
	const int64_t minutes = total_minutes % 60;
	const int64_t hours = total_minutes / 60;
	const char *sign = msec < 0 ? "-" : "";

	if (hours > 0)
		std::snprintf(buffer, sizeof(buffer), "%s%lld:%02lld:%02lld.%03lld", sign, static_cast<long long>(hours), static_cast<long long>(minutes), static_cast<long long>(seconds), static_cast<long long>(milliseconds));
	else
		std::snprintf(buffer, sizeof(buffer), "%s%02lld:%02lld.%03lld", sign, static_cast<long long>(minutes), static_cast<long long>(seconds), static_cast<long long>(milliseconds));

	return buffer;
}
