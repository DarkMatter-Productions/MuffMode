// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <algorithm>
#include <cmath>
#include <string>

struct mm_freezetag_team_counts_t {
	int participants = 0;
	int active = 0;
	int total_health = 0;
};

enum class mm_freezetag_round_result_t {
	Continue,
	RedWinsByFreeze,
	BlueWinsByFreeze,
	Draw,
	RedWinsByActiveCount,
	BlueWinsByActiveCount,
	RedWinsByHealth,
	BlueWinsByHealth
};

enum class mm_freezetag_hud_team_t {
	Red,
	Blue,
	Neutral
};

inline int MM_FreezeTagClampedParticipants(const mm_freezetag_team_counts_t &team)
{
	return std::max(team.participants, 0);
}

inline int MM_FreezeTagClampedActive(const mm_freezetag_team_counts_t &team)
{
	return std::clamp(team.active, 0, MM_FreezeTagClampedParticipants(team));
}

inline std::string MM_FreezeTagFormatHudStatus(
	const mm_freezetag_team_counts_t &red,
	const mm_freezetag_team_counts_t &blue,
	mm_freezetag_hud_team_t pov_team)
{
	const int red_active = MM_FreezeTagClampedActive(red);
	const int red_total = MM_FreezeTagClampedParticipants(red);
	const int blue_active = MM_FreezeTagClampedActive(blue);
	const int blue_total = MM_FreezeTagClampedParticipants(blue);

	const mm_freezetag_team_counts_t *allies = &red;
	const mm_freezetag_team_counts_t *enemies = &blue;
	if (pov_team == mm_freezetag_hud_team_t::Blue) {
		allies = &blue;
		enemies = &red;
	}

	if (pov_team == mm_freezetag_hud_team_t::Red || pov_team == mm_freezetag_hud_team_t::Blue) {
		return "Alive A " + std::to_string(MM_FreezeTagClampedActive(*allies)) + "/" +
			std::to_string(MM_FreezeTagClampedParticipants(*allies)) + " E " +
			std::to_string(MM_FreezeTagClampedActive(*enemies)) + "/" +
			std::to_string(MM_FreezeTagClampedParticipants(*enemies));
	}

	return "Alive R " + std::to_string(red_active) + "/" + std::to_string(red_total) +
		" B " + std::to_string(blue_active) + "/" + std::to_string(blue_total);
}

inline std::string MM_FreezeTagFormatCompactAliveStatus(
	const mm_freezetag_team_counts_t &red,
	const mm_freezetag_team_counts_t &blue,
	mm_freezetag_hud_team_t pov_team)
{
	const int red_active = MM_FreezeTagClampedActive(red);
	const int blue_active = MM_FreezeTagClampedActive(blue);

	if (pov_team == mm_freezetag_hud_team_t::Blue)
		return std::to_string(blue_active) + " vs " + std::to_string(red_active);

	return std::to_string(red_active) + " vs " + std::to_string(blue_active);
}

inline std::string MM_FreezeTagFormatRoundHudStatus(
	int display_round,
	int round_limit,
	int red_score,
	int blue_score)
{
	const int round = std::max(display_round, 0);
	(void) round_limit;
	(void) red_score;
	(void) blue_score;

	if (round <= 0)
		return {};
	return "Round " + std::to_string(round);
}

inline mm_freezetag_round_result_t MM_FreezeTagResolveRound(
	const mm_freezetag_team_counts_t &red,
	const mm_freezetag_team_counts_t &blue,
	bool time_expired)
{
	if (red.participants <= 0 || blue.participants <= 0)
		return mm_freezetag_round_result_t::Continue;

	if (red.active <= 0 && blue.active > 0)
		return mm_freezetag_round_result_t::BlueWinsByFreeze;

	if (blue.active <= 0 && red.active > 0)
		return mm_freezetag_round_result_t::RedWinsByFreeze;

	if (red.active <= 0 && blue.active <= 0)
		return mm_freezetag_round_result_t::Draw;

	if (!time_expired)
		return mm_freezetag_round_result_t::Continue;

	if (red.active > blue.active)
		return mm_freezetag_round_result_t::RedWinsByActiveCount;

	if (blue.active > red.active)
		return mm_freezetag_round_result_t::BlueWinsByActiveCount;

	if (red.total_health > blue.total_health)
		return mm_freezetag_round_result_t::RedWinsByHealth;

	if (blue.total_health > red.total_health)
		return mm_freezetag_round_result_t::BlueWinsByHealth;

	return mm_freezetag_round_result_t::Draw;
}

inline bool MM_FreezeTagSpawnShouldWaitForNextRound(
	bool freeze_tag_active,
	bool match_in_progress,
	bool round_in_progress,
	bool round_ended)
{
	return freeze_tag_active && match_in_progress && (round_in_progress || round_ended);
}

inline bool MM_FreezeTagClientCountsForRound(
	bool client_playing,
	bool client_on_freeze_team,
	bool client_eliminated)
{
	return client_playing && client_on_freeze_team && !client_eliminated;
}

inline bool MM_FreezeTagRoundCountdownShouldRespawn(
	bool freeze_tag_active,
	bool match_in_progress,
	bool round_countdown,
	bool client_playing)
{
	return freeze_tag_active && match_in_progress && round_countdown && client_playing;
}

inline bool MM_FreezeTagDeathShouldFreeze(
	bool freeze_tag_active,
	bool round_live,
	bool victim_playing,
	bool victim_on_freeze_team,
	bool victim_already_frozen,
	bool victim_eliminated,
	bool attacker_is_client,
	bool attacker_is_victim,
	bool attacker_same_team,
	bool same_team_death_should_freeze)
{
	if (!freeze_tag_active || !round_live)
		return false;
	if (!victim_playing || !victim_on_freeze_team || victim_already_frozen || victim_eliminated)
		return false;

	// Enemy kills, world deaths, and suicides freeze. Same-team kills only freeze
	// when the resolved damage event counted as a same-team death.
	if (attacker_is_client && !attacker_is_victim && attacker_same_team)
		return same_team_death_should_freeze;

	return true;
}

inline bool MM_FreezeTagUsesArenaLoadout(
	bool gametype_arena_flag,
	bool freeze_tag_active,
	bool freeze_tag_arena_loadout)
{
	return gametype_arena_flag || (freeze_tag_active && freeze_tag_arena_loadout);
}

inline bool MM_FreezeTagDropWeaponOnFreeze(bool freeze_tag_arena_loadout)
{
	return !freeze_tag_arena_loadout;
}

inline int MM_FreezeTagThawWeaponId(
	bool freeze_tag_arena_loadout,
	bool has_rocket_launcher,
	int fallback_weapon_id,
	int rocket_launcher_id)
{
	return freeze_tag_arena_loadout && has_rocket_launcher ? rocket_launcher_id : fallback_weapon_id;
}

inline float MM_FreezeTagThawRate(int thawer_count, float extra_thawer_scale)
{
	if (thawer_count <= 0)
		return 0.0f;

	const float scale = std::clamp(extra_thawer_scale, 0.0f, 4.0f);
	return std::clamp(1.0f + (std::min(thawer_count, 16) - 1) * scale, 1.0f, 8.0f);
}

inline int MM_FreezeTagThawProgressPercent(float elapsed_seconds, float thaw_seconds)
{
	if (elapsed_seconds <= 0.0f || thaw_seconds <= 0.0f)
		return 0;

	return std::clamp(static_cast<int>((elapsed_seconds / thaw_seconds) * 100.0f), 0, 99);
}

inline float MM_FreezeTagThawAssistThreshold(float thaw_seconds)
{
	if (thaw_seconds <= 0.0f)
		return 0.0f;

	return std::min(thaw_seconds, std::clamp(thaw_seconds * 0.25f, 0.25f, 1.0f));
}

inline bool MM_FreezeTagThawAssistQualifies(float contribution_seconds, float thaw_seconds)
{
	if (contribution_seconds <= 0.0f || thaw_seconds <= 0.0f)
		return false;

	return contribution_seconds + 0.001f >= MM_FreezeTagThawAssistThreshold(thaw_seconds);
}

// vectoangles() returns PITCH as -pitch with pitch pre-wrapped into [0, 360),
// so any downward look comes back in (-360, -270]. The model-pitch divide in
// ClientEndServerFrame only special-cases PITCH > 180, so an un-normalized
// -352 becomes a -117 degree model pitch: the player model lies flat. Wrap
// into the [0, 89] u [271, 360) band that PM_ClampAngles produces for a real
// player so no consumer ever sees an angle a human could not aim.
inline float MM_FreezeTagNormalizeLookPitch(float pitch)
{
	float normalized = std::fmod(pitch, 360.0f);
	if (normalized < 0.0f)
		normalized += 360.0f;

	if (normalized > 89.0f && normalized < 180.0f)
		return 89.0f;
	if (normalized >= 180.0f && normalized < 271.0f)
		return 271.0f;

	return normalized;
}

// Several weapons carry no weapon kick and rely on splash for momentum, so
// scaling their knockback can never shift an ice block. Frozen bodies fall back
// on the Quake 3 convention -- knockback derived from damage, capped at 200 --
// which is the behaviour the mode is imitating in the first place.
inline int MM_FreezeTagFrozenKnockback(int knockback, int damage, float scale, bool allow_damage_floor)
{
	int base = std::max(knockback, 0);
	if (allow_damage_floor)
		base = std::max(base, std::clamp(damage, 0, 200));
	if (base <= 0)
		return 0;

	return static_cast<int>(std::ceil(base * std::max(scale, 0.0f)));
}

// G_Physics_Toss parks a MOVETYPE_TOSS entity when its speed drops below this
// on a floor contact. Frozen bodies deliberately keep that rule rather than a
// looser one of their own: a lower bar lets gravity's along-slope pull hold a
// body above the threshold forever, so it creeps down every ramp it lands on.
// Travel distance is bought with friction instead, which has no such failure.
constexpr float kMMFreezeTagTossStopSpeed = 60.0f;

inline float MM_FreezeTagFrozenSlideFriction(float configured)
{
	return std::clamp(configured, 0.1f, 0.99f);
}

// Only break ground contact when the shove can outlive the stop rule. A hit
// that cannot is left to accumulate on the resting body instead, because
// G_PushEntity re-seats a contacting entity half a unit off the plane -- so
// unsticking on every chip-damage tick would bob a body in place under
// machinegun fire without ever moving it anywhere.
inline bool MM_FreezeTagShoveShouldUnstick(float horizontal_speed, float vertical_speed)
{
	return horizontal_speed >= kMMFreezeTagTossStopSpeed || vertical_speed > 0.0f;
}

// The shot direction already carries a vertical component, so a shot taken from
// above yields a negative kvel[2]. Apply the hop as a floor rather than an
// addition, or it cancels out for the most common case of all: shooting down at
// a body lying on the floor.
inline float MM_FreezeTagShoveLiftVelocity(float current_vertical, float lift)
{
	return std::max(current_vertical, std::max(lift, 0.0f));
}

// Scale factor that holds a velocity at or under the cap. A grounded body's
// velocity is never integrated, so without this every hit stacks forever and
// dumps at once the moment the body leaves the ground -- a super shotgun lands
// twenty pellets before physics runs a single frame.
inline float MM_FreezeTagShoveSpeedScale(float speed, float max_speed)
{
	const float cap = std::max(max_speed, 1.0f);
	if (speed <= cap || speed <= 0.0f)
		return 1.0f;

	return cap / speed;
}

inline bool MM_FreezeTagHazardShouldRelease(float seconds_in_hazard, float release_seconds)
{
	if (release_seconds <= 0.0f)
		return false;

	return seconds_in_hazard >= release_seconds;
}
