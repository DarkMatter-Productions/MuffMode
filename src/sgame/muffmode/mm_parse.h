// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

inline bool MM_IsAsciiWhitespace(char c) {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

inline std::string_view MM_TrimAsciiWhitespace(std::string_view text) {
	while (!text.empty() && MM_IsAsciiWhitespace(text.front()))
		text.remove_prefix(1);

	while (!text.empty() && MM_IsAsciiWhitespace(text.back()))
		text.remove_suffix(1);

	return text;
}

inline std::optional<int32_t> MM_ParseIntText(std::string_view text) {
	if (text.empty())
		return std::nullopt;

	int32_t value = 0;
	const char *begin = text.data();
	const char *end = begin + text.size();
	const auto [ptr, ec] = std::from_chars(begin, end, value);
	if (ec != std::errc{} || ptr != end)
		return std::nullopt;

	return value;
}

inline std::optional<int32_t> MM_ParseIntArg(const char *s) {
	if (!s)
		return std::nullopt;

	return MM_ParseIntText(s);
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

inline std::optional<float> MM_ParseFloatArg(std::string_view text) {
	if (text.find('\0') != std::string_view::npos)
		return std::nullopt;

	const std::string scratch(text);
	return MM_ParseFloatArg(scratch.c_str());
}

inline std::optional<int32_t> MM_ParseCfgIntArg(const char *s) {
	if (!s)
		return std::nullopt;

	std::string_view text = MM_TrimAsciiWhitespace(s);
	if (text.empty())
		return std::nullopt;

	if (text.front() == '"') {
		const std::string_view quoted = text.substr(1);
		const size_t closing_quote = quoted.find('"');
		if (closing_quote == std::string_view::npos)
			return std::nullopt;

		const std::string_view tail = MM_TrimAsciiWhitespace(quoted.substr(closing_quote + 1));
		if (!tail.empty())
			return std::nullopt;

		text = MM_TrimAsciiWhitespace(quoted.substr(0, closing_quote));
	}

	return MM_ParseIntText(text);
}
