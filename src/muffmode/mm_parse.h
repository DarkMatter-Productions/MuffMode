// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <optional>

inline bool MM_IsAsciiWhitespace(char c) {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

inline std::optional<int32_t> MM_ParseIntArg(const char *s) {
	if (!s || !*s)
		return std::nullopt;

	int32_t value = 0;
	const char *begin = s;
	const char *end = s + std::strlen(s);
	const auto [ptr, ec] = std::from_chars(begin, end, value);
	if (ec != std::errc{} || ptr != end)
		return std::nullopt;

	return value;
}

inline std::optional<int32_t> MM_ParseNonNegativeIntArg(const char *s) {
	const auto value = MM_ParseIntArg(s);
	if (!value || *value < 0)
		return std::nullopt;

	return value;
}

inline std::optional<float> MM_ParseFloatArg(const char *s) {
	if (!s || !*s)
		return std::nullopt;

	if (MM_IsAsciiWhitespace(*s))
		return std::nullopt;

	errno = 0;
	char *end = nullptr;
	const float value = std::strtof(s, &end);
	if (errno != 0 || !end || end == s || *end != '\0' || !std::isfinite(value))
		return std::nullopt;

	return value;
}

inline std::optional<int32_t> MM_ParseCfgIntArg(const char *s) {
	if (!s)
		return std::nullopt;

	while (MM_IsAsciiWhitespace(*s))
		s++;

	const char *begin = s;
	const char *end = nullptr;
	if (*s == '"') {
		begin = s + 1;
		end = std::strchr(begin, '"');
		if (!end)
			return std::nullopt;

		for (const char *tail = end + 1; *tail; tail++) {
			if (!MM_IsAsciiWhitespace(*tail))
				return std::nullopt;
		}
	} else {
		end = s + std::strlen(s);
		while (end > begin && MM_IsAsciiWhitespace(end[-1]))
			end--;
	}

	if (begin == end)
		return std::nullopt;

	int32_t value = 0;
	const auto [ptr, ec] = std::from_chars(begin, end, value);
	if (ec != std::errc{} || ptr != end)
		return std::nullopt;

	return value;
}
