// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <algorithm>
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

inline char MM_ToAsciiLower(char c) noexcept {
	return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

inline bool MM_EqualsAsciiI(std::string_view lhs, std::string_view rhs) noexcept {
	if (lhs.size() != rhs.size())
		return false;

	for (size_t i = 0; i < lhs.size(); i++) {
		if (MM_ToAsciiLower(lhs[i]) != MM_ToAsciiLower(rhs[i]))
			return false;
	}

	return true;
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

inline int32_t MM_ParseClampedIntArgOrDefault(const char *s, int32_t default_value, int32_t min_value, int32_t max_value) {
	if (min_value > max_value)
		return default_value;

	const auto parsed = MM_ParseIntArg(s);
	if (!parsed)
		return default_value;

	return std::clamp(*parsed, min_value, max_value);
}

inline std::optional<uint32_t> MM_ParseUInt32Text(std::string_view text) {
	if (text.empty() || text.front() == '-' || text.front() == '+')
		return std::nullopt;

	uint32_t value = 0;
	const char *begin = text.data();
	const char *end = begin + text.size();
	const auto [ptr, ec] = std::from_chars(begin, end, value);
	if (ec != std::errc{} || ptr != end)
		return std::nullopt;

	return value;
}

inline std::optional<uint32_t> MM_ParseUInt32Arg(const char *s) {
	if (!s)
		return std::nullopt;

	return MM_ParseUInt32Text(s);
}

inline std::optional<bool> MM_ParseBoolText(std::string_view text) {
	if (MM_EqualsAsciiI(text, "1") || MM_EqualsAsciiI(text, "on") || MM_EqualsAsciiI(text, "true") ||
		MM_EqualsAsciiI(text, "yes") || MM_EqualsAsciiI(text, "enable") || MM_EqualsAsciiI(text, "enabled")) {
		return true;
	}

	if (MM_EqualsAsciiI(text, "0") || MM_EqualsAsciiI(text, "off") || MM_EqualsAsciiI(text, "false") ||
		MM_EqualsAsciiI(text, "no") || MM_EqualsAsciiI(text, "disable") || MM_EqualsAsciiI(text, "disabled")) {
		return false;
	}

	return std::nullopt;
}

inline std::optional<bool> MM_ParseBoolArg(const char *s) {
	if (!s)
		return std::nullopt;

	return MM_ParseBoolText(s);
}

inline std::optional<int32_t> MM_ParseNonNegativeIntArg(const char *s) {
	const auto value = MM_ParseIntArg(s);
	if (!value || *value < 0)
		return std::nullopt;

	return value;
}

struct mm_reinforcement_spec_t {
	std::string_view classname;
	int32_t          strength;
};

inline std::optional<mm_reinforcement_spec_t> MM_ParseReinforcementSpec(std::string_view entry) {
	entry = MM_TrimAsciiWhitespace(entry);
	if (entry.empty())
		return std::nullopt;

	size_t split = 0;
	while (split < entry.size() && !MM_IsAsciiWhitespace(entry[split]))
		split++;

	if (split == entry.size())
		return std::nullopt;

	const std::string_view classname = entry.substr(0, split);
	const std::string_view strength_text = MM_TrimAsciiWhitespace(entry.substr(split));
	const auto strength = MM_ParseIntText(strength_text);
	if (!strength || *strength <= 0)
		return std::nullopt;

	return mm_reinforcement_spec_t { classname, *strength };
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
