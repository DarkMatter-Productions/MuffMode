// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "mm_parse.h"

#include <algorithm>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace muffmode::pconfig {

inline bool IsAsciiAlpha(char c) noexcept
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

inline bool IsAsciiDigit(char c) noexcept
{
	return c >= '0' && c <= '9';
}

inline bool IsAsciiAlphaNumeric(char c) noexcept
{
	return IsAsciiAlpha(c) || IsAsciiDigit(c);
}

inline char ToAsciiLower(char c) noexcept
{
	return MM_ToAsciiLower(c);
}

inline bool EqualsI(std::string_view lhs, std::string_view rhs) noexcept
{
	return MM_EqualsAsciiI(lhs, rhs);
}

inline std::string SanitizeConfigCommentText(std::string_view text, size_t max_length)
{
	if (max_length == 0)
		return std::string();

	const auto fallback = [max_length]() {
		constexpr std::string_view player = "Player";
		return std::string(player.substr(0, max_length));
	};

	if (text.empty())
		return fallback();

	std::string out;
	out.reserve(std::min(text.size(), max_length));

	for (const unsigned char ch : text) {
		if (out.size() >= max_length)
			break;
		if (ch < ' ' || ch == 0x7F) {
			if (!out.empty() && out.back() != ' ')
				out += ' ';
			continue;
		}

		out += static_cast<char>(ch);
	}

	while (!out.empty() && out.back() == ' ')
		out.pop_back();

	return out.empty() ? fallback() : out;
}

inline char HexDigit(unsigned int value) noexcept
{
	return static_cast<char>(value < 10 ? ('0' + value) : ('a' + (value - 10)));
}

inline std::optional<std::string> EncodeSocialIdConfigStem(std::string_view social_id, size_t max_social_id_length, size_t max_stem_length)
{
	constexpr std::string_view prefix = "sid-";
	if (social_id.empty() || social_id.size() > max_social_id_length)
		return std::nullopt;

	if (max_stem_length < prefix.size())
		return std::nullopt;
	const size_t available_hex_digits = max_stem_length - prefix.size();
	if (social_id.size() > available_hex_digits / 2)
		return std::nullopt;

	std::string out;
	out.reserve(prefix.size() + (social_id.size() * 2));
	out.append(prefix);

	for (const unsigned char ch : social_id) {
		out += HexDigit(ch >> 4);
		out += HexDigit(ch & 0x0F);
	}

	return out;
}

inline bool IsDisableToken(std::string_view value) noexcept
{
	return EqualsI(value, "off") ||
		EqualsI(value, "clear") ||
		EqualsI(value, "reset") ||
		EqualsI(value, "default");
}

inline std::optional<bool> ParseBoolToken(std::string_view value) noexcept
{
	return MM_ParseBoolText(value);
}

inline std::optional<int> ParseKillBeepToken(std::string_view value) noexcept
{
	if (EqualsI(value, "off"))
		return 0;
	if (EqualsI(value, "clang"))
		return 1;
	if (EqualsI(value, "beep-boop") || EqualsI(value, "beepboop"))
		return 2;
	if (EqualsI(value, "insane"))
		return 3;
	if (EqualsI(value, "tang-tang") || EqualsI(value, "tangtang"))
		return 4;

	int parsed = 0;
	const char *begin = value.data();
	const char *end = begin + value.size();
	const auto [ptr, ec] = std::from_chars(begin, end, parsed);
	if (ec != std::errc{} || ptr != end || parsed < 0 || parsed > 4)
		return std::nullopt;

	return parsed;
}

inline std::optional<bool> ParseFollowViewToken(std::string_view value) noexcept
{
	if (EqualsI(value, "first") || EqualsI(value, "firstperson") ||
		EqualsI(value, "first-person") || EqualsI(value, "fp") ||
		EqualsI(value, "eye") || EqualsI(value, "eyecam")) {
		return true;
	}

	if (EqualsI(value, "third") || EqualsI(value, "thirdperson") ||
		EqualsI(value, "third-person") || EqualsI(value, "tp") ||
		EqualsI(value, "behind")) {
		return false;
	}

	return ParseBoolToken(value);
}

inline std::string_view StripConfigComment(std::string_view line)
{
	bool in_quotes = false;

	for (size_t i = 0; i < line.size(); i++) {
		const char ch = line[i];
		if (ch == '"') {
			in_quotes = !in_quotes;
			continue;
		}

		if (in_quotes)
			continue;

		const bool starts_comment =
			ch == '#' ||
			ch == ';' ||
			(ch == '/' && i + 1 < line.size() && line[i + 1] == '/');
		if (!starts_comment)
			continue;

		line = line.substr(0, i);
		break;
	}

	return MM_TrimAsciiWhitespace(line);
}

inline bool ReadSingleConfigArg(std::string_view args, std::string_view &value)
{
	args = MM_TrimAsciiWhitespace(args);
	if (args.empty())
		return false;

	if (args.front() == '"') {
		const std::string_view quoted = args.substr(1);
		const size_t closing_quote = quoted.find('"');
		if (closing_quote == std::string_view::npos)
			return false;

		const std::string_view tail = MM_TrimAsciiWhitespace(quoted.substr(closing_quote + 1));
		if (!tail.empty())
			return false;

		value = quoted.substr(0, closing_quote);
		return true;
	}

	const size_t split = args.find_first_of(" \t\r\n\v\f");
	if (split == std::string_view::npos) {
		value = args;
		return true;
	}

	value = args.substr(0, split);
	return MM_TrimAsciiWhitespace(args.substr(split)).empty();
}

inline bool IsSafeSkinPath(std::string_view skin) noexcept
{
	if (skin.empty())
		return false;

	bool saw_slash = false;
	char previous = 0;

	for (size_t i = 0; i < skin.size(); i++) {
		const char ch = skin[i];

		if (IsAsciiAlphaNumeric(ch) || ch == '_' || ch == '-') {
			previous = ch;
			continue;
		}

		if (ch == '/') {
			if (i == 0 || previous == '/' || i + 1 == skin.size())
				return false;
			saw_slash = true;
			previous = ch;
			continue;
		}

		return false;
	}

	return saw_slash && skin.find("..") == std::string_view::npos;
}

inline bool SkinFitsPlayerConfigString(std::string_view skin, size_t max_netname_length, size_t configstring_size) noexcept
{
	constexpr size_t fixed_chars = 1 + 1 + std::string_view("default").size();
	if (max_netname_length >= configstring_size)
		return false;

	const size_t remaining = configstring_size - max_netname_length;
	if (remaining <= fixed_chars)
		return false;

	return skin.size() < (remaining - fixed_chars);
}

inline bool IsStorableSkinPath(std::string_view skin, size_t max_qpath, size_t max_netname_length, size_t configstring_size) noexcept
{
	return IsSafeSkinPath(skin) &&
		skin.size() < max_qpath &&
		SkinFitsPlayerConfigString(skin, max_netname_length, configstring_size);
}

} // namespace muffmode::pconfig
