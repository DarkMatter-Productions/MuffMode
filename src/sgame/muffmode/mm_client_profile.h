// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct gclient_t;
struct gentity_t;
struct client_config_t;
enum item_id_t : int32_t;

enum class mm_client_profile_load_status_t : uint8_t {
	skipped,
	created,
	loaded,
	repaired,
	recovered,
	failed
};

enum class mm_client_profile_outcome_t : uint8_t {
	win,
	loss,
	draw,
	abandon
};

struct mm_client_profile_match_result_t {
	std::string_view social_id;
	std::string_view gametype;
	mm_client_profile_outcome_t outcome = mm_client_profile_outcome_t::draw;
	float skill_rating = 0.0f;
	int32_t skill_rating_change = 0;
	int64_t duration_ms = 0;
	std::string_view player_name;
	// Exact trusted preference state captured with the queued result. It is
	// consulted only when a missing/corrupt profile must be recreated.
	std::string_view recovery_preferences_json;
	// [MuffMode] Stable keys of the post-match awards this player took, from
	// MM_AwardKey. Borrowed from the queued settlement, which owns the storage
	// because profile writes are retried across frames. Null or empty means the
	// player earned nothing, which is the overwhelmingly common case.
	const std::vector<std::string> *match_awards = nullptr;
};

enum class mm_weapon_preference_result_t : uint8_t {
	added,
	duplicate,
	invalid,
	full
};

using mm_client_profile_preference_mask_t = uint32_t;
inline constexpr mm_client_profile_preference_mask_t
	MM_CLIENT_PROFILE_PREFERENCE_SHOW_ID = 1u << 0;
inline constexpr mm_client_profile_preference_mask_t
	MM_CLIENT_PROFILE_PREFERENCE_SHOW_TIMER = 1u << 1;
inline constexpr mm_client_profile_preference_mask_t
	MM_CLIENT_PROFILE_PREFERENCE_SHOW_MATCH_INFO = 1u << 2;
inline constexpr mm_client_profile_preference_mask_t
	MM_CLIENT_PROFILE_PREFERENCE_SHOW_FRAG_MESSAGES = 1u << 3;
inline constexpr mm_client_profile_preference_mask_t
	MM_CLIENT_PROFILE_PREFERENCE_KILL_BEEP = 1u << 4;
inline constexpr mm_client_profile_preference_mask_t
	MM_CLIENT_PROFILE_PREFERENCE_FOLLOW_KILLER = 1u << 5;
inline constexpr mm_client_profile_preference_mask_t
	MM_CLIENT_PROFILE_PREFERENCE_FOLLOW_LEADER = 1u << 6;
inline constexpr mm_client_profile_preference_mask_t
	MM_CLIENT_PROFILE_PREFERENCE_FOLLOW_POWERUP = 1u << 7;
inline constexpr mm_client_profile_preference_mask_t
	MM_CLIENT_PROFILE_PREFERENCE_FOLLOW_VIEW = 1u << 8;
inline constexpr mm_client_profile_preference_mask_t
	MM_CLIENT_PROFILE_PREFERENCE_ANNOUNCER = 1u << 9;
inline constexpr mm_client_profile_preference_mask_t
	MM_CLIENT_PROFILE_PREFERENCE_ENEMY_SKIN = 1u << 10;
inline constexpr mm_client_profile_preference_mask_t
	MM_CLIENT_PROFILE_PREFERENCE_TEAM_SKIN = 1u << 11;
inline constexpr mm_client_profile_preference_mask_t
	MM_CLIENT_PROFILE_PREFERENCE_WEAPONS = 1u << 12;
inline constexpr mm_client_profile_preference_mask_t
	MM_CLIENT_PROFILE_PREFERENCE_ALL = (1u << 13) - 1;

inline constexpr mm_client_profile_preference_mask_t
MM_ClientProfilePreferenceMergeMask(
	bool document_recreated,
	mm_client_profile_preference_mask_t dirty_mask) noexcept
{
	// A queued snapshot contains every preference. Use all of it only when the
	// backing document had to be recreated; normal updates retain field-level
	// ownership so stale snapshots cannot overwrite unrelated disk changes.
	return document_recreated
		? MM_CLIENT_PROFILE_PREFERENCE_ALL
		: dirty_mask & MM_CLIENT_PROFILE_PREFERENCE_ALL;
}

// [MuffMode] Versioned per-player profile persistence. The caller supplies an
// authenticated social ID; profiles never grant administrative authority.
mm_client_profile_load_status_t MM_ClientProfileLoad(
	gclient_t *client,
	std::string_view social_id,
	std::string_view player_name,
	std::string_view gametype,
	const client_config_t *missing_profile_seed = nullptr);
bool MM_ClientProfileSavePreferences(
	const gclient_t *client,
	std::string_view social_id,
	mm_client_profile_preference_mask_t dirty_mask =
		MM_CLIENT_PROFILE_PREFERENCE_ALL);
bool MM_ClientProfilePersistMatchResult(
	const mm_client_profile_match_result_t &result);
std::string MM_ClientProfileSerializePreferences(const gclient_t *client);
// Retries at most one due preference snapshot per call. Shutdown performs a
// separately bounded, one-attempt-per-identity drain.
void MM_ClientProfile_RunFrame();
void MM_ClientProfile_Shutdown();
float MM_ClientProfileDefaultRating() noexcept;
// True when the authenticated identity can be represented by the canonical,
// collision-free profile filename. Other identities remain session-only.
bool MM_ClientProfileCanPersistIdentity(std::string_view social_id);
bool MM_ClientProfileCanonicalExists(std::string_view social_id);

// Weapon preferences are stored as a bounded, de-duplicated item ID list.
void MM_ClientProfileClearWeaponPreferences(gclient_t *client) noexcept;
mm_weapon_preference_result_t MM_ClientProfileAppendWeaponPreference(
	gclient_t *client,
	std::string_view token) noexcept;
std::string_view MM_ClientProfileWeaponPreferenceToken(item_id_t weapon) noexcept;
std::string MM_ClientProfileWeaponPreferenceList(const gclient_t *client);

// Produces explicit preferences followed by MuffMode's normal fallback order.
// Returns the number written to output. A null output returns the required size.
size_t MM_ClientProfileBuildWeaponOrder(
	const gclient_t *client,
	item_id_t *output,
	size_t output_capacity) noexcept;
size_t MM_ClientProfileWeaponPreferenceRank(
	const gclient_t *client,
	item_id_t weapon) noexcept;
bool MM_ClientProfilePrefersWeapon(
	const gclient_t *client,
	item_id_t candidate,
	item_id_t current) noexcept;

// Full setweaponpref command body; the vanilla command table only needs a thin
// wrapper/registration hook.
void MM_CmdSetWeaponPref(gentity_t *ent);
