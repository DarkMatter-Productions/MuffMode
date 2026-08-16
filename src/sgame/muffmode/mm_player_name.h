// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <string>
#include <string_view>

struct gclient_t;

inline constexpr std::string_view MM_PLAYER_NAME_FALLBACK = "badinfo";

// The rerelease localization layer resolves exact ##P<slot> references per
// viewer. A single-hash #P<digits> string is ordinary user data; only the
// double-hash form is reserved by the engine.
inline bool MM_IsPlayerNameToken(std::string_view name) noexcept
{
	if (name.size() < 4 || name[0] != '#' || name[1] != '#' ||
		name[2] != 'P')
		return false;
	for (size_t offset = 3; offset < name.size(); offset++)
		if (name[offset] < '0' || name[offset] > '9')
			return false;
	return true;
}

inline constexpr bool MM_ShouldEncodePlayerName(bool is_bot) noexcept
{
	return !is_bot;
}

inline bool MM_IsBotConnectionNamePlaceholder(
	std::string_view name) noexcept
{
	return name.empty() || name == "0" || MM_IsPlayerNameToken(name);
}

// A bot reconnect can arrive with the engine placeholder "0" (or no name).
// Prefer the prior bot's canonical name when available, but never restore a
// localization token left by the old connect-order bug.
inline std::string_view MM_SelectBotConnectionBaseName(
	std::string_view incoming_name,
	std::string_view retained_name) noexcept
{
	const bool incoming_is_placeholder =
		MM_IsBotConnectionNamePlaceholder(incoming_name);
	if (incoming_is_placeholder && !retained_name.empty() &&
		!MM_IsPlayerNameToken(retained_name))
		return retained_name;
	if (MM_IsBotConnectionNamePlaceholder(incoming_name))
		return MM_PLAYER_NAME_FALLBACK;
	return incoming_name;
}

inline bool MM_PlayerNameHasPrefix(
	std::string_view name, std::string_view prefix) noexcept
{
	return prefix.empty() ||
		(name.size() >= prefix.size() && name.substr(0, prefix.size()) == prefix);
}

struct mm_bot_connection_name_t {
	std::string base_name;
	std::string display_name;
};

// Resolve the engine's map-transition placeholder without baking the current
// bot prefix into the retained base name. Keeping the base separately lets a
// later prefix change replace or remove the old prefix instead of stacking it.
inline mm_bot_connection_name_t MM_ResolveBotConnectionName(
	std::string_view incoming_name,
	std::string_view retained_display_name,
	std::string_view retained_base_name,
	std::string_view prefix)
{
	const bool incoming_is_placeholder =
		MM_IsBotConnectionNamePlaceholder(incoming_name);
	const bool retained_base_is_usable = !retained_base_name.empty() &&
		!MM_IsPlayerNameToken(retained_base_name);
	const bool incoming_replays_retained_display =
		!retained_display_name.empty() &&
		incoming_name == retained_display_name;

	const bool use_retained_base =
		(incoming_is_placeholder || incoming_replays_retained_display) &&
		retained_base_is_usable;
	std::string_view base_name = use_retained_base
		? retained_base_name
		: MM_SelectBotConnectionBaseName(
			incoming_name, retained_display_name);

	// Only a placeholder fallback or an exact replay of the prior displayed
	// name is evidence that this value was already decorated. A fresh base name
	// may legitimately begin with the configured prefix (for example Ranger
	// under prefix R) and must not lose those bytes.
	const bool source_may_be_decorated = !use_retained_base &&
		(incoming_is_placeholder || incoming_replays_retained_display);
	if (source_may_be_decorated && !prefix.empty() &&
		base_name.size() > prefix.size() &&
		MM_PlayerNameHasPrefix(base_name, prefix))
		base_name.remove_prefix(prefix.size());
	if (base_name.empty() || MM_IsPlayerNameToken(base_name))
		base_name = MM_PLAYER_NAME_FALLBACK;

	mm_bot_connection_name_t resolved;
	resolved.base_name.assign(base_name.data(), base_name.size());
	resolved.display_name.reserve(prefix.size() + resolved.base_name.size());
	resolved.display_name.append(prefix.data(), prefix.size());
	resolved.display_name.append(resolved.base_name);
	if (MM_IsPlayerNameToken(resolved.display_name))
		resolved.display_name.assign(MM_PLAYER_NAME_FALLBACK);
	return resolved;
}

// Bot userinfo keeps a simple boundary: an exact copy of the last generated
// display value is an unrelated-update replay; every changed value is a new
// raw base name. The server owns prefix application and publishes display_name.
inline mm_bot_connection_name_t MM_ResolveBotUserinfoName(
	std::string_view incoming_name,
	std::string_view retained_display_name,
	std::string_view retained_base_name,
	std::string_view prefix)
{
	const bool replays_generated_display =
		!retained_base_name.empty() &&
		!MM_IsPlayerNameToken(retained_base_name) &&
		incoming_name == retained_display_name;
	return replays_generated_display
		? MM_ResolveBotConnectionName(incoming_name, retained_display_name,
			retained_base_name, prefix)
		: MM_ResolveBotConnectionName(incoming_name, {}, {}, prefix);
}

// Complete trust-boundary pipelines used by ClientConnect and
// ClientUserinfoChanged. These preserve engine placeholders where appropriate,
// sanitize raw bases, apply the configured prefix, and sanitize the final
// published display value in one operation.
mm_bot_connection_name_t MM_PrepareBotConnectionName(
	std::string_view incoming_name,
	std::string_view retained_display_name,
	std::string_view retained_base_name,
	std::string_view prefix);
mm_bot_connection_name_t MM_PrepareBotUserinfoName(
	std::string_view incoming_name,
	std::string_view retained_display_name,
	std::string_view retained_base_name,
	std::string_view prefix);

inline std::string_view MM_SelectPlayerDisplayName(
	std::string_view submitted_name,
	std::string_view persistent_name) noexcept
{
	if (!submitted_name.empty() && !MM_IsPlayerNameToken(submitted_name))
		return submitted_name;
	if (!persistent_name.empty() && !MM_IsPlayerNameToken(persistent_name))
		return persistent_name;
	return MM_PLAYER_NAME_FALLBACK;
}

// Canonical server-side name for exports, profile persistence, layouts, logs,
// and preformatted text. The returned view is bounded by the fixed client
// buffers and never exposes an internal player-reference token. Callers still
// apply escaping/sanitization required by their own output format.
std::string_view MM_PlayerDisplayName(const gclient_t *client) noexcept;

// NUL-terminated form for C APIs and varargs loggers.
const char *MM_PlayerDisplayNameCString(const gclient_t *client) noexcept;

// Engine localization argument. Humans normally return ##P<slot> so each
// viewer can resolve the correct platform name; bots return their display name.
// Never persist or preformat this value into server-owned text.
const char *MM_PlayerLocalizationName(const gclient_t *client) noexcept;

// Userinfo/configstring-safe display value. Enforces MAX_NETNAME, removes the
// engine/layout delimiter bytes rejected by userinfo, preserves UTF-8 codepoint
// boundaries while truncating, and rejects tokens.
std::string MM_SanitizePlayerDisplayName(std::string_view name);

// Canonical profile field value. Control bytes are reduced to spaces, the
// fixed player-name limit is enforced without splitting UTF-8, and engine
// tokens are rejected so profile repair can replace polluted names and aliases.
std::string MM_PlayerNameForStorage(std::string_view name);
