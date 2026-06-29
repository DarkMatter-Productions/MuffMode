// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

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
	return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

inline bool EqualsI(std::string_view lhs, std::string_view rhs) noexcept
{
	if (lhs.size() != rhs.size())
		return false;

	for (size_t i = 0; i < lhs.size(); i++) {
		if (ToAsciiLower(lhs[i]) != ToAsciiLower(rhs[i]))
			return false;
	}

	return true;
}

inline std::string SanitizeConfigCommentText(std::string_view text, size_t max_length)
{
	if (text.empty())
		return "Player";

	std::string out;
	out.reserve(max_length);

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

	return out.empty() ? "Player" : out;
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
	if (prefix.size() + (social_id.size() * 2) > max_stem_length)
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
	if (EqualsI(value, "1") || EqualsI(value, "on") || EqualsI(value, "true") ||
		EqualsI(value, "yes") || EqualsI(value, "enable") || EqualsI(value, "enabled")) {
		return true;
	}

	if (EqualsI(value, "0") || EqualsI(value, "off") || EqualsI(value, "false") ||
		EqualsI(value, "no") || EqualsI(value, "disable") || EqualsI(value, "disabled")) {
		return false;
	}

	return std::nullopt;
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
	return (max_netname_length + 1 + skin.size() + 1 + std::string_view("default").size()) < configstring_size;
}

inline bool IsStorableSkinPath(std::string_view skin, size_t max_qpath, size_t max_netname_length, size_t configstring_size) noexcept
{
	return IsSafeSkinPath(skin) &&
		skin.size() < max_qpath &&
		SkinFitsPlayerConfigString(skin, max_netname_length, configstring_size);
}

} // namespace muffmode::pconfig
