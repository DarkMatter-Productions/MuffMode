// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_player_name.h"
#include "muffmode/mm_reliable_text.h"

#include <algorithm>
#include <optional>

namespace {

template<size_t Size>
std::optional<std::string_view> FixedNameView(
	const char (&name)[Size]) noexcept
{
	const char *const end = std::find(name, name + Size, '\0');
	if (end == name + Size)
		return std::nullopt;
	return std::string_view(name, static_cast<size_t>(end - name));
}

} // namespace

std::string_view MM_PlayerDisplayName(const gclient_t *client) noexcept
{
	if (!client)
		return {};
	const auto submitted = FixedNameView(client->resp.netname);
	const auto persistent = FixedNameView(client->pers.netname);
	return MM_SelectPlayerDisplayName(
		submitted.value_or(std::string_view{}),
		persistent.value_or(std::string_view{}));
}

const char *MM_PlayerDisplayNameCString(const gclient_t *client) noexcept
{
	if (!client)
		return "";
	const auto submitted = FixedNameView(client->resp.netname);
	const auto persistent = FixedNameView(client->pers.netname);
	const std::string_view selected = MM_SelectPlayerDisplayName(
		submitted.value_or(std::string_view{}),
		persistent.value_or(std::string_view{}));
	if (submitted && selected.data() == submitted->data())
		return client->resp.netname;
	if (persistent && selected.data() == persistent->data())
		return client->pers.netname;
	return MM_PLAYER_NAME_FALLBACK.data();
}

const char *MM_PlayerLocalizationName(const gclient_t *client) noexcept
{
	if (!client)
		return "";
	if (client->sess.is_a_bot)
		return MM_PlayerDisplayNameCString(client);
	const auto persistent = FixedNameView(client->pers.netname);
	if (persistent && MM_IsPlayerNameToken(*persistent))
		return client->pers.netname;
	return MM_PlayerDisplayNameCString(client);
}

std::string MM_SanitizePlayerDisplayName(std::string_view name)
{
	constexpr size_t max_name_bytes = MAX_NETNAME - 1;
	std::string sanitized;
	sanitized.reserve(std::min(name.size(), max_name_bytes));
	for (size_t offset = 0; offset < name.size();) {
		const size_t codepoint_bytes =
			muffmode::reliable_text::Utf8CodePointLength(name, offset);
		const char raw = name[offset];
		const auto c = static_cast<unsigned char>(raw);
		if (codepoint_bytes == 1 &&
			(c < 0x20 || c == 0x7F || c == '"' || c == '{' || c == '}' ||
				c == '\\')) {
			offset++;
			continue;
		}
		if (codepoint_bytes > max_name_bytes - sanitized.size())
			break;
		sanitized.append(name.data() + offset, codepoint_bytes);
		offset += codepoint_bytes;
	}
	if (sanitized.empty() || MM_IsPlayerNameToken(sanitized))
		return std::string(MM_PLAYER_NAME_FALLBACK);
	return sanitized;
}

mm_bot_connection_name_t MM_PrepareBotConnectionName(
	std::string_view incoming_name,
	std::string_view retained_display_name,
	std::string_view retained_base_name,
	std::string_view prefix)
{
	const std::string normalized_incoming =
		MM_IsBotConnectionNamePlaceholder(incoming_name)
		? std::string(incoming_name)
		: MM_SanitizePlayerDisplayName(incoming_name);
	mm_bot_connection_name_t resolved = MM_ResolveBotConnectionName(
		normalized_incoming, retained_display_name, retained_base_name, prefix);
	resolved.display_name =
		MM_SanitizePlayerDisplayName(resolved.display_name);
	return resolved;
}

mm_bot_connection_name_t MM_PrepareBotUserinfoName(
	std::string_view incoming_name,
	std::string_view retained_display_name,
	std::string_view retained_base_name,
	std::string_view prefix)
{
	const std::string normalized_incoming =
		MM_SanitizePlayerDisplayName(incoming_name);
	mm_bot_connection_name_t resolved = MM_ResolveBotUserinfoName(
		normalized_incoming, retained_display_name, retained_base_name, prefix);
	resolved.display_name =
		MM_SanitizePlayerDisplayName(resolved.display_name);
	return resolved;
}

std::string MM_PlayerNameForStorage(std::string_view name)
{
	constexpr size_t max_name_bytes = MAX_NETNAME - 1;
	std::string result;
	result.reserve(std::min(name.size(), max_name_bytes));
	for (size_t offset = 0; offset < name.size();) {
		if (result.size() >= max_name_bytes)
			break;
		const size_t codepoint_bytes =
			muffmode::reliable_text::Utf8CodePointLength(name, offset);
		const auto ch = static_cast<unsigned char>(name[offset]);
		if (codepoint_bytes == 1 && (ch < 0x20 || ch == 0x7f)) {
			if (!result.empty() && result.back() != ' ')
				result.push_back(' ');
			offset++;
			continue;
		}
		if (codepoint_bytes > max_name_bytes - result.size())
			break;
		result.append(name.data() + offset, codepoint_bytes);
		offset += codepoint_bytes;
	}
	while (!result.empty() && result.back() == ' ')
		result.pop_back();
	if (MM_IsPlayerNameToken(result))
		return "Player";
	return result.empty() ? "Player" : result;
}
