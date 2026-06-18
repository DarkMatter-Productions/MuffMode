// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

// g_local.h -- local definitions for game module
#pragma once

#include "bg_local.h"
#include "muffmode/mm_vote_types.h"

// the "gameversion" client command will print this plus compile date
constexpr const char *GAMEVERSION = "baseq2";

constexpr const char *GAMEMOD_TITLE = "Muff Mode";
constexpr const char *GAMEMOD_VERSION = "0.36.20";

//==================================================================

#define ARRAY_LEN(x)		(sizeof(x) / sizeof(*(x)))

constexpr const int32_t GIB_HEALTH = -40;

constexpr const int32_t AMMO_INFINITE = 1000;

constexpr size_t MAX_CLIENTS_KEX = 32; // absolute limit

enum mstats_t : uint32_t {
	MSTAT_NONE,
	MSTAT_KILLS_TOTAL,
	MSTAT_KILLS_SPAWN,
	MSTAT_DEATHS_TOTAL,
	MSTAT_DEATHS_SUICIDES,
	MSTAT_DEATHS_ENVIRO,
	MSTAT_DEATHS_SPAWN,
	MSTAT_SHOTS,
	MSTAT_HITS,
	MSTAT_DMG_DEALT,
	MSTAT_DMG_RECEIVED,

	MSTAT_PING_PEAK,
	MSTAT_PING_TRACKER,
	MSTAT_PING_TICKS,

	MSTAT_HEALTH_PEAK,
	MSTAT_HEALTH_TRACKER,
	MSTAT_HEALTH_TICKS,
#if 0
	MSTAT_PKUP_MEGA_COUNT,
	MSTAT_PKUP_MEGA_TIMER,
	MSTAT_PKUP_YA_COUNT,
	MSTAT_PKUP_YA_TIMER,
	MSTAT_PKUP_RA_COUNT,
	MSTAT_PKUP_RA_TIMER,
	MSTAT_PKUP_QUAD_COUNT,
	MSTAT_PKUP_QUAD_TIMER,
	MSTAT_PKUP_DOUBLER_COUNT,
	MSTAT_PKUP_DOUBLER_TIMER,
	MSTAT_PKUP_PROTECTION_COUNT,
	MSTAT_PKUP_PROTECTION_TIMER,
	MSTAT_PKUP_INVIS_COUNT,
	MSTAT_PKUP_INVIS_TIMER,
	MSTAT_PKUP_DUELFIRE_COUNT,
	MSTAT_PKUP_DUELFIRE_TIMER,
	MSTAT_PKUP_ADRENALINE_COUNT,
	MSTAT_PKUP_ADRENALINE_TIMER,

	MSTAT_WP_BL_SHOTS,
	MSTAT_WP_BL_HITS,
	MSTAT_WP_BL_KILLS,
	MSTAT_WP_BL_DEATHS,
	MSTAT_WP_BL_DMGD,
	MSTAT_WP_BL_DMGR,
	MSTAT_WP_SG_SHOTS,
	MSTAT_WP_SG_HITS,
	MSTAT_WP_SG_KILLS,
	MSTAT_WP_SG_DEATHS,
	MSTAT_WP_SG_DMGD,
	MSTAT_WP_SG_DMGR,
	MSTAT_WP_SSG_SHOTS,
	MSTAT_WP_SSG_HITS,
	MSTAT_WP_SSG_KILLS,
	MSTAT_WP_SSG_DEATHS,
	MSTAT_WP_SSG_DMGD,
	MSTAT_WP_SSG_DMGR,
	MSTAT_WP_MG_SHOTS,
	MSTAT_WP_MG_HITS,
	MSTAT_WP_MG_KILLS,
	MSTAT_WP_MG_DEATHS,
	MSTAT_WP_MG_DMGD,
	MSTAT_WP_MG_DMGR,
	MSTAT_WP_CG_SHOTS,
	MSTAT_WP_CG_HITS,
	MSTAT_WP_CG_KILLS,
	MSTAT_WP_CG_DEATHS,
	MSTAT_WP_CG_DMGD,
	MSTAT_WP_CG_DMGR,
	MSTAT_WP_HG_SHOTS,
	MSTAT_WP_HG_HITS,
	MSTAT_WP_HG_KILLS,
	MSTAT_WP_HG_DEATHS,
	MSTAT_WP_HG_DMGD,
	MSTAT_WP_HG_DMGR,
	MSTAT_WP_GL_SHOTS,
	MSTAT_WP_GL_HITS,
	MSTAT_WP_GL_KILLS,
	MSTAT_WP_GL_DEATHS,
	MSTAT_WP_GL_DMGD,
	MSTAT_WP_GL_DMGR,
	MSTAT_WP_RL_SHOTS,
	MSTAT_WP_RL_HITS,
	MSTAT_WP_RL_KILLS,
	MSTAT_WP_RL_DEATHS,
	MSTAT_WP_RL_DMGD,
	MSTAT_WP_RL_DMGR,
	MSTAT_WP_HB_SHOTS,
	MSTAT_WP_HB_HITS,
	MSTAT_WP_HB_KILLS,
	MSTAT_WP_HB_DEATHS,
	MSTAT_WP_HB_DMGD,
	MSTAT_WP_HB_DMGR,
	MSTAT_WP_RG_SHOTS,
	MSTAT_WP_RG_HITS,
	MSTAT_WP_RG_KILLS,
	MSTAT_WP_RG_DEATHS,
	MSTAT_WP_RG_DMGD,
	MSTAT_WP_RG_DMGR,
	MSTAT_WP_PB_SHOTS,
	MSTAT_WP_PB_HITS,
	MSTAT_WP_PB_KILLS,
	MSTAT_WP_PB_DEATHS,
	MSTAT_WP_PB_DMGD,
	MSTAT_WP_PB_DMGR,
	MSTAT_WP_TM_SHOTS,
	MSTAT_WP_TM_HITS,
	MSTAT_WP_TM_KILLS,
	MSTAT_WP_TM_DEATHS,
	MSTAT_WP_TM_DMGD,
	MSTAT_WP_TM_DMGR,
	MSTAT_WP_PL_SHOTS,
	MSTAT_WP_PL_HITS,
	MSTAT_WP_PL_KILLS,
	MSTAT_WP_PL_DEATHS,
	MSTAT_WP_PL_DMGD,
	MSTAT_WP_PL_DMGR,
	MSTAT_WP_IR_SHOTS,
	MSTAT_WP_IR_HITS,
	MSTAT_WP_IR_KILLS,
	MSTAT_WP_IR_DEATHS,
	MSTAT_WP_IR_DMGD,
	MSTAT_WP_IR_DMGR,
	MSTAT_WP_BFG_SHOTS,
	MSTAT_WP_BFG_HITS,
	MSTAT_WP_BFG_KILLS,
	MSTAT_WP_BFG_DEATHS,
	MSTAT_WP_BFG_DMGD,
	MSTAT_WP_BFG_DMGR,
#endif
	MSTAT_TOTAL
};

// gameplay rulesets
enum ruleset_t : uint8_t {
	RS_NONE,
	RS_Q2RE,
	RS_MM,
	RS_Q3A,
	RS_VANILLA_PLUS,
	RS_Q1,
	RS_QC,
	RS_NUM_RULESETS
};
#define RS( x ) (game.ruleset == (x))

constexpr const char *rs_short_name[RS_NUM_RULESETS] = {
	"",
	"q2re",
	"mm",
	"q3a",
	"q2reb",
	"q",
	"qc",
};
constexpr const char *rs_long_name[RS_NUM_RULESETS] = {
	"",
	"QUAKE II Rerelease",
	"Muff Mode",
	"QUAKE III Arena style",
	"Q2RE Balanced",
	"QUAKE style",
	"Quake Champions style",
};

enum team_t {
	TEAM_NONE,
	TEAM_SPECTATOR,
	TEAM_FREE,
	TEAM_RED,
	TEAM_BLUE,
	TEAM_NUM_TEAMS
};

enum gametype_t {
	GT_NONE,
	GT_FFA,
	GT_DUEL,
	GT_TDM,
	GT_CTF,
	GT_CA,
	GT_FREEZE,
	GT_STRIKE,
	GT_RR,
	GT_LMS,
	GT_HORDE,
	GT_BALL,
	GT_INSTAGIB,
	GT_NADEFEST,
	GT_NUM_GAMETYPES
};
constexpr gametype_t GT_FIRST = GT_FFA;
constexpr gametype_t GT_LAST = GT_NADEFEST;

enum gtf_t {
	GTF_TEAMS	= 0x01,
	GTF_CTF		= 0x02,
	GTF_ARENA	= 0x04,
	GTF_ROUNDS	= 0x08,
	GTF_ELIMINATION	= 0x10,
	GTF_FRAGS	= 0x20
};

extern int _gt[GT_NUM_GAMETYPES];

#define GTF( x ) (_gt[g_gametype->integer] & (x))
#define GT( x ) (g_gametype->integer == (int)(x))
#define notGT( x ) (g_gametype->integer != (int)(x))

constexpr const char *gt_short_name[GT_NUM_GAMETYPES] = {
	"cmp",
	"ffa",
	"duel",
	"tdm",
	"ctf",
	"ca",
	"ft",
	"strike",
	"rr",
	"lms",
	"horde",
	"ball",
	"instagib",
	"nadefest"
};
constexpr const char *gt_short_name_upper[GT_NUM_GAMETYPES] = {
	"CMP",
	"FFA",
	"DUEL",
	"TDM",
	"CTF",
	"CA",
	"FT",
	"STRIKE",
	"REDROVER",
	"LMS",
	"HORDE",
	"BALL",
	"INSTAGIB",
	"NADEFEST",
};
constexpr const char *gt_long_name[GT_NUM_GAMETYPES] = {
	"Campaign",
	"Deathmatch",
	"Duel",
	"Team Deathmatch",
	"Capture the Flag",
	"Clan Arena",
	"Freeze Tag",
	"CaptureStrike",
	"Red Rover",
	"Last Man Standing",
	"Horde",
	"ProBall",
	"Instagib",
	"NadeFest"
};

typedef enum {
	MATCH_NONE,
	MATCH_WARMUP_DELAYED,	// pre-warmup (delay is active)
	MATCH_WARMUP_DEFAULT,	// 'waiting for players' / match short of players / imbalanced teams
	MATCH_WARMUP_READYUP,	// time for players to get ready
	MATCH_COUNTDOWN,		// all conditions met, counting down to match start, check conditions again at end of countdown before match start
	MATCH_IN_PROGRESS,		// match is in progress, not used in round-based gametypes
	MATCH_ENDED				// match or final round has ended
} matchst_t;

typedef enum {
	WARMUP_REQ_NONE,
	WARMUP_REQ_MORE_PLAYERS,
	WARMUP_REQ_BALANCE,
	WARMUP_REQ_READYUP
} warmupreq_t;

typedef enum {
	ROUND_NONE,
	ROUND_COUNTDOWN,	// round-based gametypes only: initial delay before round starts
	ROUND_IN_PROGRESS,	// round-based gametypes only: round is in progress
	ROUND_ENDED			// round-based gametypes only: round has ended
} roundst_t;

enum playerspawn_t {
	SPAWN_FULL_RAND,
	SPAWN_FAR_HALF,
	SPAWN_FARTHEST,
	SPAWN_NEAREST
};

enum medal_t : uint8_t {
	MEDAL_NONE,
	MEDAL_EXCELLENT,
	MEDAL_HUMILIATION,
	MEDAL_IMPRESSIVE,
	MEDAL_RAMPAGE,
	MEDAL_DEFENCE,
	MEDAL_ASSIST,
	MEDAL_CAPTURE,

	MEDAL_TOTAL
};

#define	RANK_TIED_FLAG		0x4000

typedef enum {
	SPECTATOR_NOT,
	SPECTATOR_FREE,
	SPECTATOR_FOLLOW
} spectator_state_t;

typedef enum {
	TEAM_BEGIN,		// Beginning a team game, spawn at base
	TEAM_ACTIVE		// Now actively playing
} player_team_state_state_t;

enum grapple_state_t {
	GRAPPLE_STATE_FLY,
	GRAPPLE_STATE_PULL,
	GRAPPLE_STATE_HANG
};

extern int ii_highlight;
extern int ii_duel_header;
extern int ii_teams_red_default;
extern int ii_teams_blue_default;
extern int ii_ctf_red_dropped;
extern int ii_ctf_blue_dropped;
extern int ii_ctf_red_taken;
extern int ii_ctf_blue_taken;
extern int ii_teams_red_tiny;
extern int ii_teams_blue_tiny;
extern int ii_teams_header_red;
extern int ii_teams_header_blue;
extern int mi_ctf_red_flag, mi_ctf_blue_flag; // [Paril-KEX]

//==================================================================

constexpr vec3_t PLAYER_MINS = { -16, -16, -24 };
constexpr vec3_t PLAYER_MAXS = { 16, 16, 32 };

#include <charconv>

template<typename T>
constexpr bool is_char_ptr_v = std::is_convertible_v<T, const char *>;

template<typename T>
constexpr bool is_valid_loc_embed_v = !std::is_null_pointer_v<T> && (std::is_floating_point_v<std::remove_reference_t<T>> || std::is_integral_v<std::remove_reference_t<T>> || is_char_ptr_v<T>);

struct local_game_import_t : game_import_t {
	inline local_game_import_t() = default;
	inline local_game_import_t(const game_import_t &imports) :
		game_import_t(imports) {}

public:
#ifdef USE_CPP20_FORMAT
	template<typename... Args>
	inline void Com_PrintFmt(std::format_string<Args...> format_str, Args &&... args)
#else
#define Com_PrintFmt(str, ...) \
	Com_PrintFmt_(FMT_STRING(str), __VA_ARGS__)

	template<typename S, typename... Args>
	inline void Com_PrintFmt_(const S &format_str, Args &&... args)
#endif
	{
		char print_buffer[0x10000];
		G_FmtTo_(print_buffer, format_str, std::forward<Args>(args)...);
		Com_Print(print_buffer);
	}

#ifdef USE_CPP20_FORMAT
	template<typename... Args>
	inline void Com_ErrorFmt(std::format_string<Args...> format_str, Args &&... args)
#else
#define Com_ErrorFmt(str, ...) \
	Com_ErrorFmt_(FMT_STRING(str), __VA_ARGS__)

	template<typename S, typename... Args>
	inline void Com_ErrorFmt_(const S &format_str, Args &&... args)
#endif
	{
		char print_buffer[0x10000];
		G_FmtTo_(print_buffer, format_str, std::forward<Args>(args)...);
		Com_Error(print_buffer);
	}

private:
	// localized print functions
	template<typename T>
	inline void loc_embed(T input, char *buffer, const char *&output) {
		if constexpr (std::is_floating_point_v<T> || std::is_integral_v<T>) {
			auto result = std::to_chars(buffer, buffer + MAX_INFO_STRING - 1, input);
			*result.ptr = '\0';
			output = buffer;
		} else if constexpr (is_char_ptr_v<T>) {
			if (!input)
				Com_Error("null const char ptr passed to loc");

			output = input;
		} else
			Com_Error("invalid loc argument");
	}

public:
	template<typename... Args>
	inline void LocClient_Print(gentity_t *e, print_type_t level, const char *base, Args&& ...args) {
		static_assert(sizeof...(args) < MAX_LOCALIZATION_ARGS, "too many arguments to gi.LocClient_Print");
		static_assert(((is_valid_loc_embed_v<Args>) && ...), "invalid argument passed to gi.LocClient_Print");

		std::array<char[MAX_INFO_STRING], MAX_LOCALIZATION_ARGS> buffers;
		std::array<const char *, MAX_LOCALIZATION_ARGS> buffer_ptrs{};
		size_t n = 0;
		((loc_embed(args, buffers[n], buffer_ptrs[n]), ++n), ...);

		Loc_Print(e, level, base, &buffer_ptrs.front(), sizeof...(args));
	}

	template<typename... Args>
	inline void LocBroadcast_Print(print_type_t level, const char *base, Args&& ...args) {
		static_assert(sizeof...(args) < MAX_LOCALIZATION_ARGS, "too many arguments to gi.LocBroadcast_Print");
		static_assert(((is_valid_loc_embed_v<Args>) && ...), "invalid argument passed to gi.LocBroadcast_Print");

		std::array<char[MAX_INFO_STRING], MAX_LOCALIZATION_ARGS> buffers;
		std::array<const char *, MAX_LOCALIZATION_ARGS> buffer_ptrs{};
		size_t n = 0;
		((loc_embed(args, buffers[n], buffer_ptrs[n]), ++n), ...);

		Loc_Print(nullptr, (print_type_t)(level | print_type_t::PRINT_BROADCAST), base, &buffer_ptrs.front(), sizeof...(args));
	}

	template<typename... Args>
	inline void LocCenter_Print(gentity_t *e, const char *base, Args&& ...args) {
		static_assert(sizeof...(args) < MAX_LOCALIZATION_ARGS, "too many arguments to gi.LocCenter_Print");
		static_assert(((is_valid_loc_embed_v<Args>) && ...), "invalid argument passed to gi.LocCenter_Print");

		std::array<char[MAX_INFO_STRING], MAX_LOCALIZATION_ARGS> buffers;
		std::array<const char *, MAX_LOCALIZATION_ARGS> buffer_ptrs{};
		size_t n = 0;
		((loc_embed(args, buffers[n], buffer_ptrs[n]), ++n), ...);

		Loc_Print(e, PRINT_CENTER, base, &buffer_ptrs.front(), sizeof...(args));
	}

	// collision detection
	[[nodiscard]] inline trace_t trace(const vec3_t &start, const vec3_t &mins, const vec3_t &maxs, const vec3_t &end, const gentity_t *passent, contents_t contentmask) {
		return game_import_t::trace(start, &mins, &maxs, end, passent, contentmask);
	}

	[[nodiscard]] inline trace_t traceline(const vec3_t &start, const vec3_t &end, const gentity_t *passent, contents_t contentmask) {
		return game_import_t::trace(start, nullptr, nullptr, end, passent, contentmask);
	}

	// [Paril-KEX] clip the box against the specified entity
	[[nodiscard]] inline trace_t clip(gentity_t *entity, const vec3_t &start, const vec3_t &mins, const vec3_t &maxs, const vec3_t &end, contents_t contentmask) {
		return game_import_t::clip(entity, start, &mins, &maxs, end, contentmask);
	}

	[[nodiscard]] inline trace_t clip(gentity_t *entity, const vec3_t &start, const vec3_t &end, contents_t contentmask) {
		return game_import_t::clip(entity, start, nullptr, nullptr, end, contentmask);
	}

	void unicast(gentity_t *ent, bool reliable, uint32_t dupe_key = 0) {
		game_import_t::unicast(ent, reliable, dupe_key);
	}

	void local_sound(gentity_t *target, const vec3_t &origin, gentity_t *ent, soundchan_t channel, int soundindex, float volume, float attenuation, float timeofs, uint32_t dupe_key = 0) {
		game_import_t::local_sound(target, &origin, ent, channel, soundindex, volume, attenuation, timeofs, dupe_key);
	}

	void local_sound(gentity_t *target, gentity_t *ent, soundchan_t channel, int soundindex, float volume, float attenuation, float timeofs, uint32_t dupe_key = 0) {
		game_import_t::local_sound(target, nullptr, ent, channel, soundindex, volume, attenuation, timeofs, dupe_key);
	}

	void local_sound(const vec3_t &origin, gentity_t *ent, soundchan_t channel, int soundindex, float volume, float attenuation, float timeofs, uint32_t dupe_key = 0) {
		game_import_t::local_sound(ent, &origin, ent, channel, soundindex, volume, attenuation, timeofs, dupe_key);
	}

	void local_sound(gentity_t *ent, soundchan_t channel, int soundindex, float volume, float attenuation, float timeofs, uint32_t dupe_key = 0) {
		game_import_t::local_sound(ent, nullptr, ent, channel, soundindex, volume, attenuation, timeofs, dupe_key);
	}
};

extern local_game_import_t  gi;

// entity->spawnflags
// these are set with checkboxes on each entity in the map editor.
// the following 8 are reserved and should never be used by any entity.
// (power cubes in coop use these after spawning as well)
struct spawnflags_t {
	uint32_t		value;

	explicit constexpr spawnflags_t(uint32_t v) :
		value(v) {}

	explicit operator uint32_t() const {
		return value;
	}

	// has any flags at all (!!a)
	constexpr bool any() const { return !!value; }
	// has any of the given flags (!!(a & b))
	constexpr bool has(const spawnflags_t &flags) const { return !!(value & flags.value); }
	// has all of the given flags ((a & b) == b)
	constexpr bool has_all(const spawnflags_t &flags) const { return (value & flags.value) == flags.value; }
	constexpr bool operator!() const { return !value; }

	constexpr bool operator==(const spawnflags_t &flags) const {
		return value == flags.value;
	}

	constexpr bool operator!=(const spawnflags_t &flags) const {
		return value != flags.value;
	}

	constexpr spawnflags_t operator~() const {
		return spawnflags_t(~value);
	}
	constexpr spawnflags_t operator|(const spawnflags_t &v2) const {
		return spawnflags_t(value | v2.value);
	}
	constexpr spawnflags_t operator&(const spawnflags_t &v2) const {
		return spawnflags_t(value & v2.value);
	}
	constexpr spawnflags_t operator^(const spawnflags_t &v2) const {
		return spawnflags_t(value ^ v2.value);
	}
	constexpr spawnflags_t &operator|=(const spawnflags_t &v2) {
		value |= v2.value;
		return *this;
	}
	constexpr spawnflags_t &operator&=(const spawnflags_t &v2) {
		value &= v2.value;
		return *this;
	}
	constexpr spawnflags_t &operator^=(const spawnflags_t &v2) {
		value ^= v2.value;
		return *this;
	}
};

// these spawnflags affect every entity. note that items are a bit special
// because these 8 bits are instead used for power cube bits.
constexpr spawnflags_t  SPAWNFLAG_NONE = spawnflags_t(0);
constexpr spawnflags_t	SPAWNFLAG_NOT_EASY	= spawnflags_t(0x00000100),
				SPAWNFLAG_NOT_MEDIUM		= spawnflags_t(0x00000200),
				SPAWNFLAG_NOT_HARD			= spawnflags_t(0x00000400),
				SPAWNFLAG_NOT_DEATHMATCH	= spawnflags_t(0x00000800),
				SPAWNFLAG_NOT_COOP			= spawnflags_t(0x00001000),
				SPAWNFLAG_RESERVED1			= spawnflags_t(0x00002000),
				SPAWNFLAG_COOP_ONLY			= spawnflags_t(0x00004000);

constexpr spawnflags_t SPAWNFLAG_EDITOR_MASK = (SPAWNFLAG_NOT_EASY | SPAWNFLAG_NOT_MEDIUM | SPAWNFLAG_NOT_HARD | SPAWNFLAG_NOT_DEATHMATCH |
	SPAWNFLAG_NOT_COOP | SPAWNFLAG_RESERVED1 | SPAWNFLAG_COOP_ONLY);

// use this for global spawnflags
constexpr spawnflags_t operator "" _spawnflag(unsigned long long int v) {
	if (v & SPAWNFLAG_EDITOR_MASK.value)
		throw std::invalid_argument("attempting to use reserved spawnflag");

	return static_cast<spawnflags_t>(static_cast<uint32_t>(v));
}

// use this for global spawnflags
constexpr spawnflags_t operator "" _spawnflag_bit(unsigned long long int v) {
	v = 1ull << v;

	if (v & SPAWNFLAG_EDITOR_MASK.value)
		throw std::invalid_argument("attempting to use reserved spawnflag");

	return static_cast<spawnflags_t>(static_cast<uint32_t>(v));
}

// stores a level time; most newer engines use int64_t for
// time storage, but seconds are handy for compatibility
// with Quake and older mods.
struct gtime_t {
private:
	// times always start at zero, just to prevent memory issues
	int64_t _ms = 0;

	// internal; use _sec/_ms/_min or gtime_t::from_sec(n)/gtime_t::from_ms(n)/gtime_t::from_min(n)
	constexpr explicit gtime_t(const int64_t &ms) : _ms(ms) {}

public:
	constexpr gtime_t() = default;
	constexpr gtime_t(const gtime_t &) = default;
	constexpr gtime_t &operator=(const gtime_t &) = default;

	// constructors are here, explicitly named, so you always
	// know what you're getting.

	// new time from ms
	static constexpr gtime_t from_ms(const int64_t &ms) {
		return gtime_t(ms);
	}

	// new time from seconds
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	static constexpr gtime_t from_sec(const T &s) {
		return gtime_t(static_cast<int64_t>(s * 1000));
	}

	// new time from minutes
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	static constexpr gtime_t from_min(const T &s) {
		return gtime_t(static_cast<int64_t>(s * 60000));
	}

	// new time from hz
	static constexpr gtime_t from_hz(const uint64_t &hz) {
		return from_ms(static_cast<int64_t>((1.0 / hz) * 1000));
	}

	// get value in minutes (truncated for integers)
	template<typename T = float>
	constexpr T minutes() const {
		return static_cast<T>(_ms / static_cast<std::conditional_t<std::is_floating_point_v<T>, T, float>>(60000));
	}

	// get value in seconds (truncated for integers)
	template<typename T = float>
	constexpr T seconds() const {
		return static_cast<T>(_ms / static_cast<std::conditional_t<std::is_floating_point_v<T>, T, float>>(1000));
	}

	// get value in milliseconds
	constexpr const int64_t &milliseconds() const {
		return _ms;
	}

	int64_t frames() const {
		return _ms / gi.frame_time_ms;
	}

	// check if non-zero
	constexpr explicit operator bool() const {
		return !!_ms;
	}

	// invert time
	constexpr gtime_t operator-() const {
		return gtime_t(-_ms);
	}

	// operations with other times as input
	constexpr gtime_t operator+(const gtime_t &r) const {
		return gtime_t(_ms + r._ms);
	}
	constexpr gtime_t operator-(const gtime_t &r) const {
		return gtime_t(_ms - r._ms);
	}
	constexpr gtime_t &operator+=(const gtime_t &r) {
		return *this = *this + r;
	}
	constexpr gtime_t &operator-=(const gtime_t &r) {
		return *this = *this - r;
	}

	// operations with scalars as input
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	constexpr gtime_t operator*(const T &r) const {
		return gtime_t::from_ms(static_cast<int64_t>(_ms * r));
	}
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	constexpr gtime_t operator/(const T &r) const {
		return gtime_t::from_ms(static_cast<int64_t>(_ms / r));
	}
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	constexpr gtime_t &operator*=(const T &r) {
		return *this = *this * r;
	}
	template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>>
	constexpr gtime_t &operator/=(const T &r) {
		return *this = *this / r;
	}

	// comparisons with gtime_ts
	constexpr bool operator==(const gtime_t &time) const {
		return _ms == time._ms;
	}
	constexpr bool operator!=(const gtime_t &time) const {
		return _ms != time._ms;
	}
	constexpr bool operator<(const gtime_t &time) const {
		return _ms < time._ms;
	}
	constexpr bool operator>(const gtime_t &time) const {
		return _ms > time._ms;
	}
	constexpr bool operator<=(const gtime_t &time) const {
		return _ms <= time._ms;
	}
	constexpr bool operator>=(const gtime_t &time) const {
		return _ms >= time._ms;
	}
};

// user literals, allowing you to specify times
// as 128_sec and 128_ms
constexpr gtime_t operator"" _min(long double s) {
	return gtime_t::from_min(s);
}
constexpr gtime_t operator"" _min(unsigned long long int s) {
	return gtime_t::from_min(s);
}
constexpr gtime_t operator"" _sec(long double s) {
	return gtime_t::from_sec(s);
}
constexpr gtime_t operator"" _sec(unsigned long long int s) {
	return gtime_t::from_sec(s);
}
constexpr gtime_t operator"" _ms(long double s) {
	return gtime_t::from_ms(static_cast<int64_t>(s));
}
constexpr gtime_t operator"" _ms(unsigned long long int s) {
	return gtime_t::from_ms(static_cast<int64_t>(s));
}
constexpr gtime_t operator"" _hz(unsigned long long int s) {
	return gtime_t::from_ms(static_cast<int64_t>((1.0 / s) * 1000));
}

#define SERVER_TICK_RATE gi.tick_rate // in hz
extern gtime_t FRAME_TIME_S;
extern gtime_t FRAME_TIME_MS;

// Vote state machine data structure (defined after gtime_t)
struct VoteStateData {
	VoteState state;
	vcmds_t *command;
	std::string arg;
	gclient_t *caller;
	gtime_t start_time;
	gtime_t execute_time;
	int yes_votes;
	int no_votes;
	int num_eligible;
	
	VoteStateData() : 
		state(VoteState::IDLE),
		command(nullptr),
		caller(nullptr),
		start_time(0_sec),
		execute_time(0_sec),
		yes_votes(0),
		no_votes(0),
		num_eligible(0)
	{}
};

// view pitching times
inline gtime_t DAMAGE_TIME_SLACK() {
	return (100_ms - FRAME_TIME_MS);
}

inline gtime_t DAMAGE_TIME() {
	return 500_ms + DAMAGE_TIME_SLACK();
}

inline gtime_t FALL_TIME() {
	return 300_ms + DAMAGE_TIME_SLACK();
}

// every save_data_list_t has a tag
// which is used for integrity checks.
enum save_data_tag_t {
	SAVE_DATA_MMOVE,

	SAVE_FUNC_MONSTERINFO_STAND,
	SAVE_FUNC_MONSTERINFO_IDLE,
	SAVE_FUNC_MONSTERINFO_SEARCH,
	SAVE_FUNC_MONSTERINFO_WALK,
	SAVE_FUNC_MONSTERINFO_RUN,
	SAVE_FUNC_MONSTERINFO_DODGE,
	SAVE_FUNC_MONSTERINFO_ATTACK,
	SAVE_FUNC_MONSTERINFO_MELEE,
	SAVE_FUNC_MONSTERINFO_SIGHT,
	SAVE_FUNC_MONSTERINFO_CHECKATTACK,
	SAVE_FUNC_MONSTERINFO_SETSKIN,

	SAVE_FUNC_MONSTERINFO_BLOCKED,
	SAVE_FUNC_MONSTERINFO_DUCK,
	SAVE_FUNC_MONSTERINFO_UNDUCK,
	SAVE_FUNC_MONSTERINFO_SIDESTEP,
	SAVE_FUNC_MONSTERINFO_PHYSCHANGED,

	SAVE_FUNC_MOVEINFO_ENDFUNC,
	SAVE_FUNC_MOVEINFO_BLOCKED,

	SAVE_FUNC_PRETHINK,
	SAVE_FUNC_THINK,
	SAVE_FUNC_TOUCH,
	SAVE_FUNC_USE,
	SAVE_FUNC_PAIN,
	SAVE_FUNC_DIE
};

// forward-linked list, storing data for
// saving pointers. every save_data_ptr has an
// instance of this; there's one head instance of this
// in g_save.cpp.
struct save_data_list_t {
	const char *name; // name of savable object; persisted in the JSON file
	save_data_tag_t			tag;
	const void *ptr; // pointer to raw data
	const save_data_list_t *next; // next in list

	save_data_list_t(const char *name, save_data_tag_t tag, const void *ptr);

	static const save_data_list_t *fetch(const void *link_ptr, save_data_tag_t tag);
};

#include <functional>

// save data wrapper, which holds a pointer to a T
// and the tag value for integrity. this is how you
// store a savable pointer of data safely.
template<typename T, size_t Tag>
struct save_data_t {
	using value_type = typename std::conditional<std::is_pointer<T>::value &&
		std::is_function<typename std::remove_pointer<T>::type>::value,
		T,
		const T *>::type;
private:
	value_type value;
	const save_data_list_t *list;

public:
	constexpr save_data_t() :
		value(nullptr),
		list(nullptr) {}

	constexpr save_data_t(nullptr_t) :
		save_data_t() {}

	constexpr save_data_t(const save_data_list_t *list_in) :
		value(list_in->ptr),
		list(list_in) {}

	inline save_data_t(value_type ptr_in) :
		value(ptr_in),
		list(ptr_in ? save_data_list_t::fetch(reinterpret_cast<const void *>(ptr_in), static_cast<save_data_tag_t>(Tag)) : nullptr) {}

	inline save_data_t(const save_data_t<T, Tag> &ref_in) :
		save_data_t(ref_in.value) {}

	inline save_data_t &operator=(value_type ptr_in) {
		if (value != ptr_in) {
			value = ptr_in;
			list = value ? save_data_list_t::fetch(reinterpret_cast<const void *>(ptr_in), static_cast<save_data_tag_t>(Tag)) : nullptr;
		}

		return *this;
	}

	constexpr const value_type pointer() const { return value; }
	constexpr const save_data_list_t *save_list() const { return list; }
	constexpr const char *name() const { return value ? list->name : "null"; }
	constexpr const value_type operator->() const { return value; }
	constexpr explicit operator bool() const { return value; }
	constexpr bool operator==(value_type ptr_in) const { return value == ptr_in; }
	constexpr bool operator!=(value_type ptr_in) const { return value != ptr_in; }
	constexpr bool operator==(const save_data_t<T, Tag> *ptr_in) const { return value == ptr_in->value; }
	constexpr bool operator==(const save_data_t<T, Tag> &ref_in) const { return value == ref_in.value; }
	constexpr bool operator!=(const save_data_t<T, Tag> *ptr_in) const { return value != ptr_in->value; }
	constexpr bool operator!=(const save_data_t<T, Tag> &ref_in) const { return value != ref_in.value; }

	// invoke wrapper, for function-likes
	template<typename... Args>
	inline auto operator()(Args&& ...args) const {
		static_assert(std::is_invocable_v<std::remove_pointer_t<T>, Args...>, "bad invoke args");
		return std::invoke(value, std::forward<Args>(args)...);
	}
};

// memory tags to allow dynamic memory to be cleaned up
enum {
	TAG_GAME = 765, // clear when unloading the dll
	TAG_LEVEL = 766 // clear when loading a new level
};

constexpr float MELEE_DISTANCE = 50;

constexpr size_t BODY_QUEUE_SIZE = 8;

// null trace used when touches don't need a trace
constexpr trace_t null_trace{};

enum weaponstate_t {
	WEAPON_READY,
	WEAPON_ACTIVATING,
	WEAPON_DROPPING,
	WEAPON_FIRING
};

// gib flags
enum gib_type_t {
	GIB_NONE = 0, // no flags (organic)
	GIB_METALLIC = 1, // bouncier
	GIB_ACID = 2, // acidic (gekk)
	GIB_HEAD = 4, // head gib; the input entity will transform into this
	GIB_DEBRIS = 8, // explode outwards rather than in velocity, no blood
	GIB_SKINNED = 16, // use skinnum
	GIB_UPRIGHT = 32, // stay upright on ground
};
MAKE_ENUM_BITFLAGS(gib_type_t);

// monster ai flags
enum monster_ai_flags_t : uint64_t {
	AI_NONE					= 0,
	AI_STAND_GROUND			= bit_v<0>,
	AI_TEMP_STAND_GROUND	= bit_v<1>,
	AI_SOUND_TARGET			= bit_v<2>,
	AI_LOST_SIGHT			= bit_v<3>,
	AI_PURSUIT_LAST_SEEN	= bit_v<4>,
	AI_PURSUE_NEXT			= bit_v<5>,
	AI_PURSUE_TEMP			= bit_v<6>,
	AI_HOLD_FRAME			= bit_v<7>,
	AI_GOOD_GUY				= bit_v<8>,
	AI_BRUTAL				= bit_v<9>,
	AI_NOSTEP				= bit_v<10>,
	AI_DUCKED				= bit_v<11>,
	AI_COMBAT_POINT			= bit_v<12>,
	AI_MEDIC				= bit_v<13>,
	AI_RESURRECTING			= bit_v<14>,
	AI_MANUAL_STEERING		= bit_v<15>,
	AI_TARGET_ANGER			= bit_v<16>,
	AI_DODGING				= bit_v<17>,
	AI_CHARGING				= bit_v<18>,
	AI_HINT_PATH			= bit_v<19>,
	AI_IGNORE_SHOTS			= bit_v<20>,
	// PMM - FIXME - last second added for E3 .. there's probably a better way to do this, but
	// this works
	AI_DO_NOT_COUNT			= bit_v<21>,	 // set for healed monsters
	AI_SPAWNED_CARRIER		= bit_v<22>, // both do_not_count and spawned are set for spawned monsters
	AI_SPAWNED_MEDIC_C		= bit_v<23>, // both do_not_count and spawned are set for spawned monsters
	AI_SPAWNED_WIDOW		= bit_v<24>,	 // both do_not_count and spawned are set for spawned monsters
	AI_BLOCKED				= bit_v<25>, // used by blocked_checkattack: set to say I'm attacking while blocked (prevents run-attacks)
	AI_SPAWNED_ALIVE		= bit_v<26>, // [Paril-KEX] for spawning dead
	AI_SPAWNED_DEAD			= bit_v<27>,
	AI_HIGH_TICK_RATE		= bit_v<28>, // not limited by 10hz actions
	AI_NO_PATH_FINDING		= bit_v<29>, // don't try nav nodes for path finding
	AI_PATHING				= bit_v<30>, // using nav nodes currently
	AI_STINKY				= bit_v<31>, // spawn flies
	AI_STUNK				= bit_v<32>, // already spawned files

	AI_ALTERNATE_FLY		= bit_v<33>, // use alternate flying mechanics; see monsterinfo.fly_xxx
	AI_TEMP_MELEE_COMBAT	= bit_v<34>, // temporarily switch to the melee combat style
	AI_FORGET_ENEMY			= bit_v<35>,			// forget the current enemy
	AI_DOUBLE_TROUBLE		= bit_v<36>, // JORG only
	AI_REACHED_HOLD_COMBAT	= bit_v<37>,
	AI_THIRD_EYE			= bit_v<38>
};
MAKE_ENUM_BITFLAGS(monster_ai_flags_t);

constexpr monster_ai_flags_t AI_SPAWNED_MASK =
AI_SPAWNED_CARRIER | AI_SPAWNED_MEDIC_C | AI_SPAWNED_WIDOW; // mask to catch all three flavors of spawned

// monster attack state
enum monster_attack_state_t {
	AS_NONE,
	AS_STRAIGHT,
	AS_SLIDING,
	AS_MELEE,
	AS_MISSILE,
	AS_BLIND // PMM - used by boss code to do nasty things even if it can't see you
};

// handedness values
enum handedness_t {
	RIGHT_HANDED,
	LEFT_HANDED,
	CENTER_HANDED
};

enum class auto_switch_t {
	SMART,
	ALWAYS,
	ALWAYS_NO_AMMO,
	NEVER
};

constexpr uint32_t SFL_CROSS_TRIGGER_MASK = (0xffffffffu & ~SPAWNFLAG_EDITOR_MASK.value);

// noise types for PlayerNoise
enum player_noise_t {
	PNOISE_SELF,
	PNOISE_WEAPON,
	PNOISE_IMPACT
};

struct gitem_armor_t {
	int32_t base_count;
	int32_t max_count;
	float	normal_protection;
	float	energy_protection;
};

static constexpr gitem_armor_t jacketarmor_info = { 25, 50, .30f, .00f };
static constexpr gitem_armor_t combatarmor_info = { 50, 100, .60f, .30f };
static constexpr gitem_armor_t bodyarmor_info = { 100, 200, .80f, .60f };

// entity->movetype values
enum movetype_t {
	MOVETYPE_NONE,	 // never moves
	MOVETYPE_NOCLIP, // origin and angles change with no interaction
	MOVETYPE_PUSH,	 // no clip to world, push on box contact
	MOVETYPE_STOP,	 // no clip to world, stops on box contact

	MOVETYPE_WALK, // gravity
	MOVETYPE_STEP, // gravity, special edge handling
	MOVETYPE_FLY,
	MOVETYPE_TOSS,		 // gravity
	MOVETYPE_FLYMISSILE, // extra size to monsters
	MOVETYPE_BOUNCE,
	MOVETYPE_WALLBOUNCE,
	MOVETYPE_NEWTOSS, // PGM - for deathball
	MOVETYPE_FREECAM, // spectator free cam
};

// entity->flags
enum ent_flags_t : uint64_t {
	FL_NONE					= 0, // no flags
	FL_FLY					= bit_v<0>,
	FL_SWIM					= bit_v<1>, // implied immunity to drowning
	FL_IMMUNE_LASER			= bit_v<2>,
	FL_INWATER				= bit_v<3>,
	FL_GODMODE				= bit_v<4>,
	FL_NOTARGET				= bit_v<5>,
	FL_IMMUNE_SLIME			= bit_v<6>,
	FL_IMMUNE_LAVA			= bit_v<7>,
	FL_PARTIALGROUND		= bit_v<8>, // not all corners are valid
	FL_WATERJUMP			= bit_v<9>, // player jumping out of water
	FL_TEAMSLAVE			= bit_v<10>, // not the first on the team
	FL_NO_KNOCKBACK			= bit_v<11>,
	FL_POWER_ARMOR			= bit_v<12>, // power armor (if any) is active

	FL_MECHANICAL			= bit_v<13>, // entity is mechanical, use sparks not blood
	FL_SAM_RAIMI			= bit_v<14>, // entity is in sam raimi cam mode
	FL_DISGUISED			= bit_v<15>, // entity is in disguise, monsters will not recognize.
	FL_NOGIB				= bit_v<16>, // player has been vaporized by a nuke, drop no gibs
	FL_DAMAGEABLE			= bit_v<17>,
	FL_STATIONARY			= bit_v<18>,

	FL_ALIVE_KNOCKBACK_ONLY	= bit_v<19>, // only apply knockback if alive or on same frame as death
	FL_NO_DAMAGE_EFFECTS	= bit_v<20>,

	// [Paril-KEX] gets scaled by coop health scaling
	FL_COOP_HEALTH_SCALE	= bit_v<21>,
	FL_FLASHLIGHT			= bit_v<22>, // enable flashlight
	FL_KILL_VELOCITY		= bit_v<23>, // for berserker slam
	FL_NOVISIBLE			= bit_v<24>, // super invisibility
	FL_DODGE				= bit_v<25>, // monster should try to dodge this
	FL_TEAMMASTER			= bit_v<26>, // is a team master (only here so that entities abusing teammaster/teamchain for stuff don't break)
	FL_LOCKED				= bit_v<27>, // entity is locked for the purposes of navigation
	FL_ALWAYS_TOUCH			= bit_v<28>, // always touch, even if we normally wouldn't
	FL_NO_STANDING			= bit_v<29>, // don't allow "standing" on non-brush entities
	FL_WANTS_POWER_ARMOR	= bit_v<30>, // for players, auto-shield

	FL_RESPAWN				= bit_v<31>, // used for item respawning
	FL_TRAP					= bit_v<32>, // entity is a trap of some kind
	FL_TRAP_LASER_FIELD		= bit_v<33>, // enough of a special case to get it's own flag...
	FL_IMMORTAL				= bit_v<34>,  // never go below 1hp

	FL_NO_BOTS				= bit_v<35>,  // not to be used by bots
	FL_NO_HUMANS			= bit_v<36>,  // not to be used by humans
};
MAKE_ENUM_BITFLAGS(ent_flags_t);

// gitem_t->flags
enum item_flags_t : uint32_t {
	IF_NONE				= 0,

	IF_WEAPON			= bit_v<0>, // use makes active weapon
	IF_AMMO				= bit_v<1>,
	IF_ARMOR			= bit_v<2>,
	IF_STAY_COOP		= bit_v<3>,
	IF_KEY				= bit_v<4>,
	IF_TIMED			= bit_v<5>, // minor powerup items
	IF_NOT_GIVEABLE		= bit_v<6>, // item can not be given
	IF_HEALTH			= bit_v<7>,
	IF_TECH				= bit_v<8>,
	IF_NO_HASTE			= bit_v<9>,
	IF_NO_INFINITE_AMMO	= bit_v<10>, // [Paril-KEX] don't allow infinite ammo to affect
	IF_POWERUP_WHEEL	= bit_v<11>, // [Paril-KEX] item should be in powerup wheel
	IF_POWERUP_ONOFF	= bit_v<12>, // [Paril-KEX] for wheel; can't store more than one, show on/off state
	IF_NOT_RANDOM		= bit_v<13>, // [Paril-KEX] item never shows up in randomizations
	IF_POWER_ARMOR		= bit_v<14>,
	IF_POWERUP			= bit_v<15>,
	IF_SPHERE			= bit_v<16>,

	IF_ANY = 0xFFFFFFFF
};

MAKE_ENUM_BITFLAGS(item_flags_t);
constexpr item_flags_t IF_TYPE_MASK = (IF_WEAPON | IF_AMMO | IF_TIMED | IF_POWERUP | IF_SPHERE | IF_ARMOR | IF_POWER_ARMOR | IF_KEY);

// health gentity_t->style
enum {
	HEALTH_IGNORE_MAX = 1,
	HEALTH_TIMED = 2
};

enum superpu_t {
	SPW_NONE,
	SPW_QUAD,
	SPW_DUELFIRE,
	SPW_PROTECTION,
	SPW_DOUBLE,
	SPW_INVISIBILITY,
	SPW_MAX_SPW
};

// item IDs; must match itemlist order
enum item_id_t : int32_t {
	IT_NULL, // must always be zero

	IT_ARMOR_BODY,
	IT_ARMOR_COMBAT,
	IT_ARMOR_JACKET,
	IT_ARMOR_SHARD,

	IT_POWER_SCREEN,
	IT_POWER_SHIELD,

	IT_WEAPON_GRAPPLE,
	IT_WEAPON_BLASTER,
	IT_WEAPON_CHAINFIST,
	IT_WEAPON_SHOTGUN,
	IT_WEAPON_SSHOTGUN,
	IT_WEAPON_MACHINEGUN,
	IT_WEAPON_ETF_RIFLE,
	IT_WEAPON_CHAINGUN,
	IT_AMMO_GRENADES,
	IT_AMMO_TRAP,
	IT_AMMO_TESLA,
	IT_WEAPON_GLAUNCHER,
	IT_WEAPON_PROXLAUNCHER,
	IT_WEAPON_RLAUNCHER,
	IT_WEAPON_HYPERBLASTER,
	IT_WEAPON_IONRIPPER,
	IT_WEAPON_PLASMABEAM,
	IT_WEAPON_RAILGUN,
	IT_WEAPON_PHALANX,
	IT_WEAPON_BFG,
	IT_WEAPON_DISRUPTOR,

	IT_AMMO_SHELLS,
	IT_AMMO_BULLETS,
	IT_AMMO_CELLS,
	IT_AMMO_ROCKETS,
	IT_AMMO_SLUGS,
	IT_AMMO_MAGSLUG,
	IT_AMMO_FLECHETTES,
	IT_AMMO_PROX,
	IT_AMMO_NUKE,
	IT_AMMO_ROUNDS,

	IT_POWERUP_QUAD,
	IT_POWERUP_HASTE,
	IT_POWERUP_PROTECTION,
	IT_POWERUP_INVISIBILITY,
	IT_POWERUP_SILENCER,
	IT_POWERUP_REBREATHER,
	IT_POWERUP_ENVIROSUIT,
	IT_ANCIENT_HEAD,
	IT_LEGACY_HEAD,
	IT_ADRENALINE,
	IT_BANDOLIER,
	IT_PACK,
	IT_IR_GOGGLES,
	IT_POWERUP_DOUBLE,
	IT_POWERUP_SPHERE_VENGEANCE,
	IT_POWERUP_SPHERE_HUNTER,
	IT_POWERUP_SPHERE_DEFENDER,
	IT_DOPPELGANGER,
	IT_TAG_TOKEN,

	IT_KEY_DATA_CD,
	IT_KEY_POWER_CUBE,
	IT_KEY_EXPLOSIVE_CHARGES,
	IT_KEY_YELLOW,
	IT_KEY_POWER_CORE,
	IT_KEY_PYRAMID,
	IT_KEY_DATA_SPINNER,
	IT_KEY_PASS,
	IT_KEY_BLUE_KEY,
	IT_KEY_RED_KEY,
	IT_KEY_GREEN_KEY,
	IT_KEY_COMMANDER_HEAD,
	IT_KEY_AIRSTRIKE,
	IT_KEY_NUKE_CONTAINER,
	IT_KEY_NUKE,

	IT_HEALTH_SMALL,
	IT_HEALTH_MEDIUM,
	IT_HEALTH_LARGE,
	IT_HEALTH_MEGA,

	IT_FLAG_RED,
	IT_FLAG_BLUE,

	IT_TECH_DISRUPTOR_SHIELD,
	IT_TECH_POWER_AMP,
	IT_TECH_TIME_ACCEL,
	IT_TECH_AUTODOC,

	IT_AMMO_SHELLS_LARGE ,
	IT_AMMO_SHELLS_SMALL,
	IT_AMMO_BULLETS_LARGE,
	IT_AMMO_BULLETS_SMALL,
	IT_AMMO_CELLS_LARGE,
	IT_AMMO_CELLS_SMALL,
	IT_AMMO_ROCKETS_SMALL,
	IT_AMMO_SLUGS_LARGE,
	IT_AMMO_SLUGS_SMALL,

	IT_TELEPORTER,
	IT_POWERUP_REGEN,

	IT_FOODCUBE,

	IT_BALL,

	IT_FLASHLIGHT,
	IT_COMPASS,

	IT_TOTAL
};

constexpr int FIRST_WEAPON = IT_WEAPON_GRAPPLE;
constexpr int LAST_WEAPON = IT_WEAPON_DISRUPTOR;

constexpr item_id_t tech_ids[] = { IT_TECH_DISRUPTOR_SHIELD, IT_TECH_POWER_AMP, IT_TECH_TIME_ACCEL, IT_TECH_AUTODOC };

constexpr const char *ITEM_CTF_FLAG_RED = "item_flag_team_red";
constexpr const char *ITEM_CTF_FLAG_BLUE = "item_flag_team_blue";

struct gitem_t {
	item_id_t		id;		   // matches item list index
	const char		*classname; // spawning name
	bool			(*pickup)(gentity_t *ent, gentity_t *other);
	void			(*use)(gentity_t *ent, gitem_t *item);
	void			(*drop)(gentity_t *ent, gitem_t *item);
	void			(*weaponthink)(gentity_t *ent);
	const char		*pickup_sound;
	const char		*world_model;
	effects_t		world_model_flags;
	const char		*view_model;

	// client side info
	const char		*icon;
	const char		*use_name; // for use command, english only
	const char		*pickup_name; // for printing on pickup
	const char		*pickup_name_definite; // definite article version for languages that need it

	int				quantity = 0;	  // for ammo how much, for weapons how much is used per shot
	item_id_t		ammo = IT_NULL;  // for weapons
	item_id_t		chain = IT_NULL; // weapon chain root
	item_flags_t	flags = IF_NONE; // IT_* flags

	const char		*vwep_model = nullptr; // vwep model string (for weapons)

	const gitem_armor_t *armor_info = nullptr;
	int				tag = 0;

	const char		*precaches = nullptr; // string of all models, sounds, and images this item will use

	int32_t			sort_id = 0; // used by some items to control their sorting
	int32_t			quantity_warn = 5; // when to warn on low ammo

	// set in InitItems, don't set by hand
	// circular list of chained weapons
	gitem_t			*chain_next = nullptr;

	// set in SP_worldspawn, don't set by hand
	// model index for vwep
	int32_t			vwep_index = 0;

	// set in SetItemNames, don't set by hand
	// offset into CS_WHEEL_AMMO/CS_WHEEL_WEAPONS/CS_WHEEL_POWERUPS
	int32_t			ammo_wheel_index = -1;
	int32_t			weapon_wheel_index = -1;
	int32_t			powerup_wheel_index = -1;
};

// means of death
enum mod_id_t : uint8_t {
	MOD_UNKNOWN,
	MOD_BLASTER,
	MOD_SHOTGUN,
	MOD_SSHOTGUN,
	MOD_MACHINEGUN,
	MOD_CHAINGUN,
	MOD_GRENADE,
	MOD_G_SPLASH,
	MOD_ROCKET,
	MOD_R_SPLASH,
	MOD_HYPERBLASTER,
	MOD_RAILGUN,
	MOD_BFG_LASER,
	MOD_BFG_BLAST,
	MOD_BFG_EFFECT,
	MOD_HANDGRENADE,
	MOD_HG_SPLASH,
	MOD_WATER,
	MOD_SLIME,
	MOD_LAVA,
	MOD_CRUSH,
	MOD_TELEFRAG,
	MOD_TELEFRAG_SPAWN,
	MOD_FALLING,
	MOD_SUICIDE,
	MOD_HELD_GRENADE,
	MOD_EXPLOSIVE,
	MOD_BARREL,
	MOD_BOMB,
	MOD_EXIT,
	MOD_SPLASH,
	MOD_TARGET_LASER,
	MOD_TRIGGER_HURT,
	MOD_HIT,
	MOD_TARGET_BLASTER,
	MOD_RIPPER,
	MOD_PHALANX,
	MOD_BRAINTENTACLE,
	MOD_BLASTOFF,
	MOD_GEKK,
	MOD_TRAP,
	MOD_CHAINFIST,
	MOD_DISINTEGRATOR,
	MOD_ETF_RIFLE,
	MOD_BLASTER2,
	MOD_PLASMABEAM,
	MOD_TESLA,
	MOD_PROX,
	MOD_NUKE,
	MOD_VENGEANCE_SPHERE,
	MOD_HUNTER_SPHERE,
	MOD_DEFENDER_SPHERE,
	MOD_TRACKER,
	MOD_DOPPEL_EXPLODE,
	MOD_DOPPEL_VENGEANCE,
	MOD_DOPPEL_HUNTER,
	MOD_GRAPPLE,
	MOD_BLUEBLASTER,
	MOD_RAILGUN_SPLASH,
	MOD_EXPIRE,
	MOD_CHANGE_TEAM
};

struct mod_t {
	mod_id_t	id;
	bool		friendly_fire = false;
	bool		no_point_loss = false;

	mod_t() = default;
	constexpr mod_t(mod_id_t id, bool no_point_loss = false) :
		id(id),
		no_point_loss(no_point_loss) {}
};

// the total number of levels we'll track for the
// end of unit screen.
constexpr size_t MAX_LEVELS_PER_UNIT = 8;

struct level_entry_t {
	// bsp name
	char map_name[MAX_QPATH];
	// map name
	char pretty_name[MAX_QPATH];
	// these are set when we leave the level
	int32_t total_secrets;
	int32_t found_secrets;
	int32_t total_monsters;
	int32_t killed_monsters;
	// total time spent in the level, for end screen
	gtime_t time;
	// the order we visited levels in
	int32_t visit_order;
};

#define NUM_SPAWN_SPOTS MAX_ENTITIES
#define SPAWN_SPOT_INTERMISSION NUM_SPAWN_SPOTS-1

//
// this structure is left intact through an entire game
// it should be initialized at dll load time, and read/written to
// the server.ssv file for savegames
//
struct game_locals_t {
	char	helpmessage1[MAX_TOKEN_CHARS];
	char	helpmessage2[MAX_TOKEN_CHARS];
	int32_t help1changed, help2changed;

	gclient_t *clients; // [maxclients]

	// can't store spawnpoint in level, because
	// it would get overwritten by the savegame restore
	char spawnpoint[MAX_TOKEN_CHARS]; // needed for coop respawns

	// store latched cvars here that we want to get at often
	uint32_t maxclients;
	uint32_t maxentities;

	// cross level triggers
	uint32_t cross_level_flags, cross_unit_flags;

	bool autosaved;

	// [Paril-KEX]
	int32_t airacceleration_modified, gravity_modified;
	std::array<level_entry_t, MAX_LEVELS_PER_UNIT> level_entries;
	int32_t max_lag_origins;
	vec3_t *lag_origins; // maxclients * max_lag_origins

	std::vector<std::string> mapqueue;

	gametype_t	gametype;
	std::string motd;
	int motd_mod_count = 0;

	ruleset_t	ruleset;

	int8_t item_inhibit_pu;
	int8_t item_inhibit_pa;
	int8_t item_inhibit_ht;
	int8_t item_inhibit_ar;
	int8_t item_inhibit_am;
	int8_t item_inhibit_wp;
};

constexpr size_t MAX_HEALTH_BARS = 2;

struct ghost_t {
	char	netname[MAX_NETNAME];
	int		number;

	// stats
	int		deaths;
	int		kills;
	int		caps;
	int		basedef;
	int		carrierdef;

	int		code;		// ghost code
	team_t	team;		// team
	int		score;		// score at time of disconnect
	gentity_t *ent;
};

typedef struct {
	player_team_state_state_t	state;

	gtime_t		returned_flag_time;
	gtime_t		flag_pickup_time;
	gtime_t		fragged_carrier_time;

	int			location;

	int			captures;
	int			base_defense;
	int			carrier_defense;
	int			frag_recovery;
	int			frag_carrier;
	int			assists;

	int			hurt_carrier_time;
} player_team_state_t;

//
// this structure is cleared as each map is entered
// it is read/written to the level.sav file for savegames
//
struct level_locals_t {
	bool		in_frame;
	gtime_t		time;
	gtime_t		start_time;
	gtime_t		exit_time;
	bool		ready_to_exit;

	char		level_name[MAX_QPATH]; // the descriptive name (Outer Base, etc)
	char		mapname[MAX_QPATH];	// the server name (base1, etc)
	char		nextmap[MAX_QPATH];	// go here when score limit is hit
	char		forcemap[MAX_QPATH];	// go here

	// intermission state
	gtime_t		intermission_time;		// time the intermission was started
	gtime_t		intermission_queued;	// intermission was qualified, but
										// wait INTERMISSION_DELAY_TIME before
										// actually going there so the last
										// frag can be watched.  Disable future
										// kills during this delay

	const char	*changemap;
	const char	*achievement;
	bool		intermission_exit;
	bool		intermission_eou;
	bool		intermission_clear; // [Paril-KEX] clear inventory on switch
	bool		level_intermission_set; // [Paril-KEX] for target_camera switches; don't find intermission point
	bool		intermission_fade, intermission_fading; // [Paril-KEX] fade on exit instead of immediately leaving
	gtime_t		intermission_fade_time;
	vec3_t		intermission_origin;
	vec3_t		intermission_angle;
	bool		intermission_spot;
	bool		respawn_intermission; // only set once for respawning players

	// spawn spots	//Q3
	gentity_t	*spawn_spots[NUM_SPAWN_SPOTS];
	int			num_spawn_spots;
	int			num_spawn_spots_team;
	int			num_spawn_spots_free;

	int32_t		pic_health;
	int32_t		pic_ping;

	int32_t		total_secrets;
	int32_t		found_secrets;

	int32_t		total_goals;
	int32_t		found_goals;

	int32_t		total_monsters;
	std::array<gentity_t *, MAX_ENTITIES> monsters_registered; // only for debug
	int32_t		killed_monsters;

	gentity_t	*current_entity; // entity running from G_RunFrame
	int32_t		body_que;		 // dead bodies

	int32_t		power_cubes; // ugly necessity for coop

	gentity_t	*disguise_violator;
	gtime_t		disguise_violation_time;
	int32_t		disguise_icon; // [Paril-KEX]

	int32_t		shadow_light_count; // [Sam-KEX]
	bool		is_n64;
	gtime_t		coop_level_restart_time; // restart the level after this time
	bool		instantitems; // instantitems 1 set in worldspawn

	// N64 goal stuff
	const char	*goals; // nullptr if no goals in world
	int32_t		goal_num; // current relative goal number, increased with each target_goal

	// offset for the first vwep model, for
	// skinnum encoding
	int32_t		vwep_offset;

	// coop health scaling factor;
	// this percentage of health is added
	// to the monster's health per player.
	float		coop_health_scaling;
	// this isn't saved in the save file, but stores
	// the amount of players currently active in the
	// level, compared against monsters' individual 
	// scale #
	int32_t		coop_scale_players;

	// [Paril-KEX] current level entry
	level_entry_t *entry;

	// [Paril-KEX] current poi
	bool		valid_poi;
	vec3_t		current_poi;
	int32_t		current_poi_image;
	int32_t		current_poi_stage;
	gentity_t	*current_dynamic_poi;
	vec3_t		*poi_points[MAX_SPLIT_PLAYERS]; // temporary storage for POIs in coop

	// start items
	const char	*start_items;
	// disable grappling hook
	bool		no_grapple;
	// disable spawn pads
	bool		no_dm_spawnpads;

	// saved gravity
	float		gravity;
	// level is a hub map, and shouldn't be included in EOU stuff
	bool		hub_map;
	// active health bar entities
	std::array<gentity_t *, MAX_HEALTH_BARS> health_bar_entities;
	int32_t		intermission_server_frame;
	bool		deadly_kill_box;
	bool		story_active;
	gtime_t		next_auto_save;
	gtime_t		next_match_report;

	char		gamemod_name[64];
	char		gametype_name[64];

	//voting (state machine)
	VoteStateData vote_state;

	uint8_t		num_connected_clients;
	uint8_t		num_nonspectator_clients;	// includes connecting clients
	uint8_t		num_playing_clients;		// connected, non-spectators
	uint8_t		num_playing_human_clients;	// players, bots excluded
	int			sorted_clients[MAX_CLIENTS];// sorted by score
	uint8_t		follow1, follow2;			// clientNums for auto-follow spectators

	int			num_living_red;
	int			num_eliminated_red;
	int			num_living_blue;
	int			num_eliminated_blue;
	int			num_playing_red;
	int			num_playing_blue;

	int			team_scores[TEAM_NUM_TEAMS];
	int			team_old_scores[TEAM_NUM_TEAMS];

	matchst_t	match_state;
	warmupreq_t	warmup_requisite;
	gtime_t		warmup_notice_time;
	gtime_t		match_time;
	gtime_t		match_start_time;
	int			match_state_queued;
	gtime_t		match_state_timer;			// change match state at this time
	int			warmup_modification_count;	// for detecting if g_warmup_countdown is changed

	gtime_t		countdown_check;
	gtime_t		matchendwarn_check;
	gtime_t		match_cancel_delay_timer;	// delay before cancelling match due to too few players

	int			round_number;
	roundst_t	round_state;
	int			round_state_queued;
	gtime_t		round_state_timer;			// change match state at this time

	bool		restarted;

	gtime_t		overtime;
	bool		suddendeath;
	gtime_t		tied_overtime_start;

	int			count_living[TEAM_NUM_TEAMS];

	bool		locked[TEAM_NUM_TEAMS];
	gentity_t	*captain[TEAM_NUM_TEAMS];	// team captains (nullptr = no captain)

	gtime_t		ctf_last_flag_capture;
	team_t		ctf_last_capture_team;

	ghost_t		ghosts[MAX_CLIENTS]; // ghost codes

	int			weapon_count[LAST_WEAPON - FIRST_WEAPON];

	gtime_t		no_players_time;

	int			total_player_deaths;

	bool		init;

	std::string	entstring;

	bool		strike_red_attacks;		// team currently on offense this turn
	bool		strike_flag_touch;		// attackers reached/held the flag this turn (messaging only)
	int8_t		strike_turn;			// which turn of the current round-pair: 0 = first attacker, 1 = roles swapped

	gtime_t		horde_monster_spawn_time;
	int32_t		horde_spawn_points_remaining;
	int8_t		horde_fighters_snapshotted;
	bool		horde_all_spawned;
	gtime_t		horde_mark_time;
	int16_t		horde_mark_living;
	float		horde_map_scale_mult; // cached map-size multiplier, 0 = not yet computed
	int8_t		horde_champions_remaining; // champions left to place this match (per-run roll)
	bool		horde_champion_pending;    // current wave owes its champion to the next valid spawn
	int8_t		horde_wave_theme;          // horde_theme_t of the current wave (0 = none)
	uint32_t	horde_wave_roster;         // bitmask over monsters[] indices, 0 = unrestricted

	char		author[MAX_QPATH];
	char		author2[MAX_QPATH];

	char		intermission_victor_msg[64];

	gtime_t		timeout_in_place;
	gentity_t	*timeout_ent;

	std::string match_id;

	bool		frag_warning[3];

	bool		prepare_to_fight;
};

struct shadow_light_temp_t {
	shadow_light_data_t data;
	const char *lightstyletarget = nullptr;
};

void G_LoadShadowLights();

#include <unordered_set>

// spawn_temp_t is only used to hold entity field values that
// can be set from the editor, but aren't actualy present
// in gentity_t during gameplay.
// defaults can/should be set in the struct.
struct spawn_temp_t {
	// world vars
	const char *sky;
	float  skyrotate;
	vec3_t skyaxis;
	int32_t skyautorotate = 1;
	const char *nextmap;

	int32_t		lip;
	int32_t		distance;
	int32_t		height;
	const char *noise;
	float		pausetime;
	const char *item;
	const char *gravity;

	float minyaw;
	float maxyaw;
	float minpitch;
	float maxpitch;

	shadow_light_temp_t sl; // [Sam-KEX]
	const char *music; // [Edward-KEX]
	int instantitems;
	float radius; // [Paril-KEX]
	bool hub_map; // [Paril-KEX]
	const char *achievement; // [Paril-KEX]

	// [Paril-KEX]
	const char *goals;

	// [Paril-KEX]
	const char *image;

	int fade_start_dist = 96;
	int fade_end_dist = 384;
	const char *start_items;
	int no_grapple = 0;
	int no_dm_spawnpads = 0;
	float health_multiplier = 1.0f;

	const char *reinforcements; // [Paril-KEX]
	const char *noise_start, *noise_middle, *noise_end; // [Paril-KEX]
	int32_t loop_count; // [Paril-KEX]

	std::unordered_set<const char *> keys_specified;

	inline bool was_key_specified(const char *key) const {
		return keys_specified.find(key) != keys_specified.end();
	}

	const char *cvar;
	const char *cvarvalue;

	const char *author;
	const char *author2;

	const char *ruleset;

	bool nobots;
	bool nohumans;
};

enum move_state_t {
	STATE_TOP,
	STATE_BOTTOM,
	STATE_UP,
	STATE_DOWN
};

#define DEFINE_DATA_FUNC(ns_lower, ns_upper, returnType, ...) \
	using save_##ns_lower##_t = save_data_t<returnType(*)(__VA_ARGS__), SAVE_FUNC_##ns_upper>

#define SAVE_DATA_FUNC(n, ns, returnType, ...) \
	using save_##n##_t = save_data_t<returnType(*)(__VA_ARGS__), SAVE_FUNC_##ns>; \
	extern returnType n(__VA_ARGS__); \
	static const save_data_list_t save__##n(#n, SAVE_FUNC_##ns, reinterpret_cast<const void *>(n)); \
	auto n

DEFINE_DATA_FUNC(moveinfo_endfunc, MOVEINFO_ENDFUNC, void, gentity_t *self);
#define MOVEINFO_ENDFUNC(n) \
	SAVE_DATA_FUNC(n, MOVEINFO_ENDFUNC, void, gentity_t *self)

DEFINE_DATA_FUNC(moveinfo_blocked, MOVEINFO_BLOCKED, void, gentity_t *self, gentity_t *other);
#define MOVEINFO_BLOCKED(n) \
	SAVE_DATA_FUNC(n, MOVEINFO_BLOCKED, void, gentity_t *self, gentity_t *other)

// a struct that can store type-safe allocations
// of a fixed amount of data. it self-destructs when
// re-assigned. TODO: because gentities are still kind of
// managed like C memory, the destructor may not be
// called for a freed entity if this is stored as a member.
template<typename T, int32_t tag>
struct savable_allocated_memory_t {
	T *ptr;
	size_t	count;

	constexpr savable_allocated_memory_t(T *ptr, size_t count) :
		ptr(ptr),
		count(count) {}

	inline ~savable_allocated_memory_t() {
		release();
	}

	// no copy
	constexpr savable_allocated_memory_t(const savable_allocated_memory_t &) = delete;
	constexpr savable_allocated_memory_t &operator=(const savable_allocated_memory_t &) = delete;

	// free move
	constexpr savable_allocated_memory_t(savable_allocated_memory_t &&move) {
		ptr = move.ptr;
		count = move.count;

		move.ptr = nullptr;
		move.count = 0;
	}

	constexpr savable_allocated_memory_t &operator=(savable_allocated_memory_t &&move) {
		ptr = move.ptr;
		count = move.count;

		move.ptr = nullptr;
		move.count = 0;

		return *this;
	}

	inline void release() {
		if (ptr) {
			gi.TagFree(ptr);
			count = 0;
			ptr = nullptr;
		}
	}

	constexpr explicit operator T *() { return ptr; }
	constexpr explicit operator const T *() const { return ptr; }

	constexpr std::add_lvalue_reference_t<T> operator[](const size_t index) { return ptr[index]; }
	constexpr const std::add_lvalue_reference_t<T> operator[](const size_t index) const { return ptr[index]; }

	constexpr size_t size() const { return count * sizeof(T); }
	constexpr operator bool() const { return !!ptr; }
};

template<typename T, int32_t tag>
inline savable_allocated_memory_t<T, tag> make_savable_memory(size_t count) {
	if (!count)
		return { nullptr, 0 };

	return { reinterpret_cast<T *>(gi.TagMalloc(sizeof(T) * count, tag)), count };
}

struct moveinfo_t {
	// fixed data
	vec3_t start_origin;
	vec3_t start_angles;
	vec3_t end_origin;
	vec3_t end_angles, end_angles_reversed;

	int32_t sound_start;
	int32_t sound_middle;
	int32_t sound_end;

	float accel;
	float speed;
	float decel;
	float distance;

	float wait;

	// state data
	move_state_t state;
	bool		 reversing;
	vec3_t		 dir;
	vec3_t		 dest;
	float		 current_speed;
	float		 move_speed;
	float		 next_speed;
	float		 remaining_distance;
	float		 decel_distance;
	save_moveinfo_endfunc_t endfunc;
	save_moveinfo_blocked_t blocked;

	// [Paril-KEX] new accel state
	vec3_t  curve_ref;
	savable_allocated_memory_t<float, TAG_LEVEL> curve_positions;
	size_t	curve_frame;
	uint8_t	subframe, num_subframes;
	size_t  num_frames_done;
};

struct mframe_t {
	void (*aifunc)(gentity_t *self, float dist) = nullptr;
	float dist = 0;
	void (*thinkfunc)(gentity_t *self) = nullptr;
	int32_t lerp_frame = -1;
};

// this check only works on windows, and is only
// of importance to developers anyways
#if defined(_WIN32) && defined(_MSC_VER)
#if _MSC_VER >= 1934
#define COMPILE_TIME_MOVE_CHECK
#endif
#endif

struct mmove_t {
	int32_t	  firstframe;
	int32_t	  lastframe;
	const mframe_t *frame;
	void (*endfunc)(gentity_t *self);
	float sidestep_scale;

#ifdef COMPILE_TIME_MOVE_CHECK
	template<size_t N>
	constexpr mmove_t(int32_t firstframe, int32_t lastframe, const mframe_t(&frames)[N], void (*endfunc)(gentity_t *self) = nullptr, float sidestep_scale = 0.0f) :
		firstframe(firstframe),
		lastframe(lastframe),
		frame(frames),
		endfunc(endfunc),
		sidestep_scale(sidestep_scale) {
		if ((lastframe - firstframe + 1) != N)
			throw std::exception("bad animation frames; check your numbers!");
	}
#endif
};

using save_mmove_t = save_data_t<mmove_t, SAVE_DATA_MMOVE>;
#ifdef COMPILE_TIME_MOVE_CHECK
#define MMOVE_T(n) \
	extern const mmove_t n; \
	static const save_data_list_t save__##n(#n, SAVE_DATA_MMOVE, &n); \
	constexpr mmove_t n
#else
#define MMOVE_T(n) \
	extern const mmove_t n; \
	static const save_data_list_t save__##n(#n, SAVE_DATA_MMOVE, &n); \
	const mmove_t n
#endif

DEFINE_DATA_FUNC(monsterinfo_stand, MONSTERINFO_STAND, void, gentity_t *self);
#define MONSTERINFO_STAND(n) \
	SAVE_DATA_FUNC(n, MONSTERINFO_STAND, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_idle, MONSTERINFO_IDLE, void, gentity_t *self);
#define MONSTERINFO_IDLE(n) \
	SAVE_DATA_FUNC(n, MONSTERINFO_IDLE, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_search, MONSTERINFO_SEARCH, void, gentity_t *self);
#define MONSTERINFO_SEARCH(n) \
	SAVE_DATA_FUNC(n, MONSTERINFO_SEARCH, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_walk, MONSTERINFO_WALK, void, gentity_t *self);
#define MONSTERINFO_WALK(n) \
	SAVE_DATA_FUNC(n, MONSTERINFO_WALK, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_run, MONSTERINFO_RUN, void, gentity_t *self);
#define MONSTERINFO_RUN(n) \
	SAVE_DATA_FUNC(n, MONSTERINFO_RUN, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_dodge, MONSTERINFO_DODGE, void, gentity_t *self, gentity_t *attacker, gtime_t eta, trace_t *tr, bool gravity);
#define MONSTERINFO_DODGE(n) \
	SAVE_DATA_FUNC(n, MONSTERINFO_DODGE, void, gentity_t *self, gentity_t *attacker, gtime_t eta, trace_t *tr, bool gravity)

DEFINE_DATA_FUNC(monsterinfo_attack, MONSTERINFO_ATTACK, void, gentity_t *self);
#define MONSTERINFO_ATTACK(n) \
	SAVE_DATA_FUNC(n, MONSTERINFO_ATTACK, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_melee, MONSTERINFO_MELEE, void, gentity_t *self);
#define MONSTERINFO_MELEE(n) \
	SAVE_DATA_FUNC(n, MONSTERINFO_MELEE, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_sight, MONSTERINFO_SIGHT, void, gentity_t *self, gentity_t *other);
#define MONSTERINFO_SIGHT(n) \
	SAVE_DATA_FUNC(n, MONSTERINFO_SIGHT, void, gentity_t *self, gentity_t *other)

DEFINE_DATA_FUNC(monsterinfo_checkattack, MONSTERINFO_CHECKATTACK, bool, gentity_t *self);
#define MONSTERINFO_CHECKATTACK(n) \
	SAVE_DATA_FUNC(n, MONSTERINFO_CHECKATTACK, bool, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_setskin, MONSTERINFO_SETSKIN, void, gentity_t *self);
#define MONSTERINFO_SETSKIN(n) \
	SAVE_DATA_FUNC(n, MONSTERINFO_SETSKIN, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_blocked, MONSTERINFO_BLOCKED, bool, gentity_t *self, float dist);
#define MONSTERINFO_BLOCKED(n) \
	SAVE_DATA_FUNC(n, MONSTERINFO_BLOCKED, bool, gentity_t *self, float dist)

DEFINE_DATA_FUNC(monsterinfo_physicschange, MONSTERINFO_PHYSCHANGED, void, gentity_t *self);
#define MONSTERINFO_PHYSCHANGED(n) \
	SAVE_DATA_FUNC(n, MONSTERINFO_PHYSCHANGED, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_duck, MONSTERINFO_DUCK, bool, gentity_t *self, gtime_t eta);
#define MONSTERINFO_DUCK(n) \
	SAVE_DATA_FUNC(n, MONSTERINFO_DUCK, bool, gentity_t *self, gtime_t eta)

DEFINE_DATA_FUNC(monsterinfo_unduck, MONSTERINFO_UNDUCK, void, gentity_t *self);
#define MONSTERINFO_UNDUCK(n) \
	SAVE_DATA_FUNC(n, MONSTERINFO_UNDUCK, void, gentity_t *self)

DEFINE_DATA_FUNC(monsterinfo_sidestep, MONSTERINFO_SIDESTEP, bool, gentity_t *self);
#define MONSTERINFO_SIDESTEP(n) \
	SAVE_DATA_FUNC(n, MONSTERINFO_SIDESTEP, bool, gentity_t *self)

// combat styles, for navigation
enum combat_style_t {
	COMBAT_UNKNOWN, // automatically choose based on attack functions
	COMBAT_MELEE, // should attempt to get up close for melee
	COMBAT_MIXED, // has mixed melee/ranged; runs to get up close if far enough away
	COMBAT_RANGED // don't bother pathing if we can see the player
};

struct reinforcement_t {
	const char *classname;
	int32_t strength;
	vec3_t mins, maxs;
};

struct reinforcement_list_t {
	reinforcement_t *reinforcements;
	uint32_t		num_reinforcements;
};

constexpr size_t MAX_REINFORCEMENTS = 5; // max number of spawns we can do at once.

constexpr gtime_t HOLD_FOREVER = gtime_t::from_ms(std::numeric_limits<int64_t>::max());

struct monsterinfo_t {
	// [Paril-KEX] allow some moves to be done instantaneously, but
	// others can wait the full frame.
	// NB: always use `M_SetAnimation` as it handles edge cases.
	save_mmove_t	   active_move, next_move;
	monster_ai_flags_t aiflags; // PGM - unsigned, since we're close to the max
	int32_t			   nextframe; // if next_move is set, this is ignored until a frame is ran
	float			   scale;

	save_monsterinfo_stand_t stand;
	save_monsterinfo_idle_t idle;
	save_monsterinfo_search_t search;
	save_monsterinfo_walk_t walk;
	save_monsterinfo_run_t run;
	save_monsterinfo_dodge_t dodge;
	save_monsterinfo_attack_t attack;
	save_monsterinfo_melee_t melee;
	save_monsterinfo_sight_t sight;
	save_monsterinfo_checkattack_t checkattack;
	save_monsterinfo_setskin_t setskin;
	save_monsterinfo_physicschange_t physics_change;

	gtime_t pausetime;
	gtime_t attack_finished;
	gtime_t fire_wait;

	vec3_t				   saved_goal;
	gtime_t				   search_time;
	gtime_t				   trail_time;
	vec3_t				   last_sighting;
	monster_attack_state_t attack_state;
	bool				   lefty;
	gtime_t				   idle_time;
	int32_t				   linkcount;

	item_id_t power_armor_type;
	int32_t	  power_armor_power;

	// for monster revive
	item_id_t initial_power_armor_type;
	int32_t	  max_power_armor_power;
	int32_t	  weapon_sound, engine_sound;

	save_monsterinfo_blocked_t blocked;
	gtime_t	 last_hint_time; // last time the monster checked for hintpaths.
	gentity_t *goal_hint;		 // which hint_path we're trying to get to
	int32_t	 medicTries;
	gentity_t *badMedic1, *badMedic2; // these medics have declared this monster "unhealable"
	gentity_t *healer;				// this is who is healing this monster
	save_monsterinfo_duck_t duck;
	save_monsterinfo_unduck_t unduck;
	save_monsterinfo_sidestep_t sidestep;
	float	 base_height;
	gtime_t	 next_duck_time;
	gtime_t	 duck_wait_time;
	gentity_t *last_player_enemy;
	// blindfire stuff .. the boolean says whether the monster will do it, and blind_fire_time is the timing
	// (set in the monster) of the next shot
	bool	blindfire;		// will the monster blindfire?
	bool    can_jump;       // will the monster jump?
	bool	had_visibility; // Paril: used for blindfire
	float	drop_height, jump_height;
	gtime_t blind_fire_delay;
	vec3_t	blind_fire_target;
	// used by the spawners to not spawn too much and keep track of #s of monsters spawned
	int32_t	 monster_slots; // nb: for spawned monsters, this is how many slots we took from our commander
	int32_t	 monster_used;
	gentity_t *commander;
	// powerup timers, used by widow, our friend
	gtime_t quad_time;
	gtime_t invincibility_time;
	gtime_t double_time;

	// Paril
	gtime_t	  surprise_time;
	item_id_t armor_type;
	int32_t	  armor_power;
	bool	  close_sight_tripped;
	gtime_t   melee_debounce_time; // don't melee until this time has passed
	gtime_t	  strafe_check_time; // time until we should reconsider strafing
	int32_t	  base_health; // health that we had on spawn, before any co-op adjustments
	int32_t   health_scaling; // number of players we've been scaled up to
	float	  champion_damage_scale; // horde: tapered outgoing-damage multiplier for champions (<= 1 means no buff / not a champion)
	gtime_t   next_move_time; // high tick rate
	gtime_t	  bad_move_time; // don't try straight moves until this is over
	gtime_t	  bump_time; // don't slide against walls for a bit
	gtime_t	  random_change_time; // high tickrate
	gtime_t	  path_blocked_counter; // break out of paths when > a certain time
	gtime_t	  path_wait_time; // don't try nav nodes until this is over
	PathInfo  nav_path; // if AI_PATHING, this is where we are trying to reach
	gtime_t	  nav_path_cache_time; // cache nav_path result for this much time
	combat_style_t combat_style; // pathing style

	gentity_t *damage_attacker;
	gentity_t *damage_inflictor;
	int32_t   damage_blood, damage_knockback;
	vec3_t	  damage_from;
	mod_t	  damage_mod;

	// alternate flying mechanics
	float fly_max_distance, fly_min_distance; // how far we should try to stay
	float fly_acceleration; // accel/decel speed
	float fly_speed; // max speed from flying
	vec3_t fly_ideal_position; // ideally where we want to end up to hover, relative to our target if not pinned
	gtime_t fly_position_time; // if <= level.time, we can try changing positions
	bool fly_buzzard, fly_above; // orbit around all sides of their enemy, not just the sides
	bool fly_pinned; // whether we're currently pinned to ideal position (made absolute)
	bool fly_thrusters; // slightly different flight mechanics, for melee attacks
	gtime_t fly_recovery_time; // time to try a new dir to get away from hazards
	vec3_t fly_recovery_dir;

	gtime_t checkattack_time;
	int32_t start_frame;
	gtime_t dodge_time;
	int32_t move_block_counter;
	gtime_t move_block_change_time;
	gtime_t react_to_damage_time;

	reinforcement_list_t					reinforcements;
	std::array<uint8_t, MAX_REINFORCEMENTS>	chosen_reinforcements; // readied for spawn; 255 is value for none

	gtime_t jump_time;

	// NOTE: if adding new elements, make sure to add them
	// in g_save.cpp too!
};

// non-monsterinfo save stuff
using save_prethink_t = save_data_t<void(*)(gentity_t *self), SAVE_FUNC_PRETHINK>;
#define PRETHINK(n) \
	void n(gentity_t *self); \
	static const save_data_list_t save__##n(#n, SAVE_FUNC_PRETHINK, reinterpret_cast<const void *>(n)); \
	auto n

using save_think_t = save_data_t<void(*)(gentity_t *self), SAVE_FUNC_THINK>;
#define THINK(n) \
	void n(gentity_t *self); \
	static const save_data_list_t save__##n(#n, SAVE_FUNC_THINK, reinterpret_cast<const void *>(n)); \
	auto n

using save_touch_t = save_data_t<void(*)(gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self), SAVE_FUNC_TOUCH>;
#define TOUCH(n) \
	void n(gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self); \
	static const save_data_list_t save__##n(#n, SAVE_FUNC_TOUCH, reinterpret_cast<const void *>(n)); \
	auto n

using save_use_t = save_data_t<void(*)(gentity_t *self, gentity_t *other, gentity_t *activator), SAVE_FUNC_USE>;
#define USE(n) \
	void n(gentity_t *self, gentity_t *other, gentity_t *activator); \
	static const save_data_list_t save__##n(#n, SAVE_FUNC_USE, reinterpret_cast<const void *>(n)); \
	auto n

using save_pain_t = save_data_t<void(*)(gentity_t *self, gentity_t *other, float kick, int damage, const mod_t &mod), SAVE_FUNC_PAIN>;
#define PAIN(n) \
	void n(gentity_t *self, gentity_t *other, float kick, int damage, const mod_t &mod); \
	static const save_data_list_t save__##n(#n, SAVE_FUNC_PAIN, reinterpret_cast<const void *>(n)); \
	auto n

using save_die_t = save_data_t<void(*)(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod), SAVE_FUNC_DIE>;
#define DIE(n) \
	void n(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod); \
	static const save_data_list_t save__##n(#n, SAVE_FUNC_DIE, reinterpret_cast<const void *>(n)); \
	auto n

// this determines how long to wait after a duck to duck again.
// if we finish a duck-up, this gets cut in half.
constexpr gtime_t DUCK_INTERVAL = 5000_ms;

extern game_locals_t  game;

extern level_locals_t level;
extern game_export_t  globals;
extern spawn_temp_t	  st;

extern gentity_t *g_entities;

#include <random>
extern std::mt19937 mt_rand;

// uniform float [0, 1)
[[nodiscard]] inline float frandom() {
	return std::uniform_real_distribution<float>()(mt_rand);
}

// uniform float [min_inclusive, max_exclusive)
[[nodiscard]] inline float frandom(float min_inclusive, float max_exclusive) {
	return std::uniform_real_distribution<float>(min_inclusive, max_exclusive)(mt_rand);
}

// uniform float [0, max_exclusive)
[[nodiscard]] inline float frandom(float max_exclusive) {
	return std::uniform_real_distribution<float>(0, max_exclusive)(mt_rand);
}

// uniform time [min_inclusive, max_exclusive)
[[nodiscard]] inline gtime_t random_time(gtime_t min_inclusive, gtime_t max_exclusive) {
	return gtime_t::from_ms(std::uniform_int_distribution<int64_t>(min_inclusive.milliseconds(), max_exclusive.milliseconds())(mt_rand));
}

// uniform time [0, max_exclusive)
[[nodiscard]] inline gtime_t random_time(gtime_t max_exclusive) {
	return gtime_t::from_ms(std::uniform_int_distribution<int64_t>(0, max_exclusive.milliseconds())(mt_rand));
}

// uniform float [-1, 1)
// note: closed on min but not max
// to match vanilla behavior
[[nodiscard]] inline float crandom() {
	return std::uniform_real_distribution<float>(-1.f, 1.f)(mt_rand);
}

// uniform float (-1, 1)
[[nodiscard]] inline float crandom_open() {
	return std::uniform_real_distribution<float>(std::nextafterf(-1.f, 0.f), 1.f)(mt_rand);
}

// raw unsigned int32 value from random
[[nodiscard]] inline uint32_t irandom() {
	return mt_rand();
}

// uniform int [min, max)
// always returns min if min == (max - 1)
// undefined behavior if min > (max - 1)
[[nodiscard]] inline int32_t irandom(int32_t min_inclusive, int32_t max_exclusive) {
	if (min_inclusive == max_exclusive - 1)
		return min_inclusive;

	return std::uniform_int_distribution<int32_t>(min_inclusive, max_exclusive - 1)(mt_rand);
}

// uniform int [0, max)
// always returns 0 if max <= 0
// note for Q2 code:
// - to fix rand()%x, do irandom(x)
// - to fix rand()&x, do irandom(x + 1)
[[nodiscard]] inline int32_t irandom(int32_t max_exclusive) {
	if (max_exclusive <= 0)
		return 0;

	return irandom(0, max_exclusive);
}

// uniform random index from given container
template<typename T>
[[nodiscard]] inline int32_t random_index(const T &container) {
	return irandom(std::size(container));
}

// uniform random element from given container
template<typename T>
[[nodiscard]] inline auto random_element(T &container) -> decltype(*std::begin(container)) {
	return *(std::begin(container) + random_index(container));
}

// flip a coin
[[nodiscard]] inline bool brandom() {
	return irandom(2) == 0;
}

extern cvar_t *hostname;

extern cvar_t *deathmatch;
extern cvar_t *ctf;
extern cvar_t *teamplay;
extern cvar_t *g_gametype;

extern cvar_t *coop;

extern cvar_t *skill;
extern cvar_t *fraglimit;
extern cvar_t *capturelimit;
extern cvar_t *timelimit;
extern cvar_t *roundlimit;
extern cvar_t *roundtimelimit;
extern cvar_t *mercylimit;
extern cvar_t *noplayerstime;

extern cvar_t *g_ruleset;

extern cvar_t *password;
extern cvar_t *spectator_password;
extern cvar_t *admin_password;
extern cvar_t *needpass;

extern cvar_t *maxplayers;
extern cvar_t *minplayers;

extern cvar_t *ai_allow_dm_spawn;
extern cvar_t *ai_damage_scale;
extern cvar_t *ai_model_scale;
extern cvar_t *ai_movement_disabled;

extern cvar_t *bot_debug_follow_actor;
extern cvar_t *bot_debug_move_to_point;

extern cvar_t *flood_msgs;
extern cvar_t *flood_persecond;
extern cvar_t *flood_waitdelay;

extern cvar_t *bob_pitch;
extern cvar_t *bob_roll;
extern cvar_t *bob_up;
extern cvar_t *gun_x, *gun_y, *gun_z;
extern cvar_t *run_pitch;
extern cvar_t *run_roll;

extern cvar_t *g_airaccelerate;
extern cvar_t *g_allow_admin;
extern cvar_t *g_allow_custom_skins;
extern cvar_t *g_team_force_models;
extern cvar_t *g_team_red_model;
extern cvar_t *g_team_blue_model;
extern cvar_t *g_allow_forfeit;
extern cvar_t *g_allow_grapple;
extern cvar_t *g_allow_kill;
extern cvar_t *g_allow_mymap;
extern cvar_t *g_allow_spec_vote;
extern cvar_t *g_allow_techs;
extern cvar_t *g_allow_vote_midgame;
extern cvar_t *g_allow_voting;
extern cvar_t *g_arena_dmg_armor;
extern cvar_t *g_arena_start_armor;
extern cvar_t *g_arena_start_health;
extern cvar_t *g_cheats;
extern cvar_t *g_coop_enable_lives;
extern cvar_t *g_coop_health_scaling;
extern cvar_t *g_coop_instanced_items;
extern cvar_t *g_coop_num_lives;
extern cvar_t *g_horde_lives;
extern cvar_t *g_horde_start_chainsaw;
extern cvar_t *g_coop_player_collision;
extern cvar_t *g_coop_squad_respawn;
extern cvar_t *g_corpse_sink_time;
extern cvar_t *g_damage_scale;
extern cvar_t *g_debug_monster_kills;
extern cvar_t *g_debug_monster_paths;
extern cvar_t *g_dedicated;
extern cvar_t *g_disable_player_collision;
extern cvar_t *g_dm_allow_exit;
extern cvar_t *g_dm_allow_no_humans;
extern cvar_t *g_dm_auto_join;
extern cvar_t *g_dm_crosshair_id;
extern cvar_t *g_dm_do_readyup;
extern cvar_t *g_dm_do_warmup;
extern cvar_t *g_dm_exec_level_cfg;
extern cvar_t *g_dm_force_join;
extern cvar_t *g_dm_force_respawn;
extern cvar_t *g_dm_force_respawn_time;
extern cvar_t *g_dm_holdable_adrenaline;
extern cvar_t *g_dm_instant_items;
extern cvar_t *g_dm_intermission_shots;
extern cvar_t *g_dm_item_respawn_rate;
extern cvar_t *g_dm_no_fall_damage;
extern cvar_t *g_dm_no_quad_drop;
extern cvar_t *g_dm_no_self_damage;
extern cvar_t *g_dm_no_stack_double;
extern cvar_t *g_dm_overtime;
extern cvar_t *g_dm_tie_max_time;
extern cvar_t *g_dm_powerup_drop;
extern cvar_t *g_dm_powerups_minplayers;
extern cvar_t *g_dm_random_items;
extern cvar_t *g_dm_respawn_delay_min;
extern cvar_t *g_dm_respawn_point_min_dist;
extern cvar_t *g_dm_respawn_point_min_dist_debug;
extern cvar_t *g_dm_same_level;
extern cvar_t *g_dm_spawn_farthest;
extern cvar_t *g_dm_spawnpads;
extern cvar_t *g_dm_strong_mines;
extern cvar_t *g_dm_timeout_length;
extern cvar_t *g_dm_weapons_stay;
extern cvar_t *g_drop_cmds;
extern cvar_t *g_entity_override_dir;
extern cvar_t *g_entity_override_load;
extern cvar_t *g_entity_override_save;
extern cvar_t *g_eyecam;
extern cvar_t *g_fast_doors;
extern cvar_t *g_frag_messages;
extern cvar_t *g_frenzy;
extern cvar_t *g_friendly_fire;
extern cvar_t *g_grapple_damage;
extern cvar_t *g_grapple_fly_speed;
extern cvar_t *g_grapple_offhand;
extern cvar_t *g_grapple_pull_speed;
extern cvar_t *g_gravity;
extern cvar_t *g_huntercam;
extern cvar_t *g_inactivity;
extern cvar_t *g_infinite_ammo;
extern cvar_t *g_instagib;
extern cvar_t *g_instagib_splash;
extern cvar_t *g_instant_weapon_switch;
extern cvar_t *g_item_bobbing;
extern cvar_t *g_knockback_scale;
extern cvar_t *g_ladder_steps;
extern cvar_t *g_lag_compensation;
extern cvar_t *g_map_list;
extern cvar_t *g_map_list_shuffle;
extern cvar_t *g_map_pool;
extern cvar_t *g_votable_gametypes;
extern cvar_t *g_votable_rulesets;
extern cvar_t *g_match_lock;
extern cvar_t *g_matchstats;
extern cvar_t *g_maxvelocity;
extern cvar_t *g_motd_filename;
extern cvar_t *g_mover_debug;
extern cvar_t *g_mover_speed_scale;
extern cvar_t *g_nadefest;
extern cvar_t *g_no_armor;
extern cvar_t *g_no_health;
extern cvar_t *g_no_items;
extern cvar_t *g_no_mines;
extern cvar_t *g_no_nukes;
extern cvar_t *g_no_powerups;
extern cvar_t *g_no_spheres;
extern cvar_t *g_mapspawn_no_bfg;
extern cvar_t *g_mapspawn_no_plasmabeam;
extern cvar_t *g_owner_auto_join;
extern cvar_t *g_owner_push_scores;
extern cvar_t *g_gametype_cfg;
extern cvar_t *g_quadhog;
extern cvar_t *g_quick_weapon_switch;
extern cvar_t *g_rollangle;
extern cvar_t *g_rollspeed;
extern cvar_t *g_round_countdown;
extern cvar_t *g_rr_rounds;
extern cvar_t *g_select_empty;
extern cvar_t *g_showhelp;
extern cvar_t *g_showmotd;
extern cvar_t *g_skip_view_modifiers;
extern cvar_t *g_start_items;
extern cvar_t *g_starting_health;
extern cvar_t *g_starting_health_bonus;
extern cvar_t *g_starting_armor;
extern cvar_t *g_stopspeed;
extern cvar_t *g_strict_saves;
extern cvar_t *g_teamplay_allow_team_pick;
extern cvar_t *g_teamplay_armor_protect;
extern cvar_t *g_teamplay_auto_balance;
extern cvar_t *g_teamplay_force_balance;
extern cvar_t *g_teamplay_item_drop_notice;
extern cvar_t *g_teleporter_freeze;
extern cvar_t *g_vampiric_damage;
extern cvar_t *g_vampiric_exp_min;
extern cvar_t *g_vampiric_health_max;
extern cvar_t *g_vampiric_percentile;
extern cvar_t *g_verbose;
extern cvar_t *g_vote_flags;
extern cvar_t *g_vote_limit;
extern cvar_t *g_warmup_countdown;
extern cvar_t *g_warmup_ready_percentage;
extern cvar_t *g_weapon_projection;
extern cvar_t *g_weapon_respawn_time;
// Weapon balance cvars - declared in both builds, but only initialized in DEBUG
extern cvar_t *g_weapon_balance_dev;
extern cvar_t *g_chaingun_max_shots;
extern cvar_t *g_chaingun_damage;
extern cvar_t *g_chaingun_hspread;
extern cvar_t *g_chaingun_vspread;
extern cvar_t *g_chaingun_spread_offset;
extern cvar_t *g_machinegun_damage;
extern cvar_t *g_machinegun_hspread;
extern cvar_t *g_machinegun_vspread;
extern cvar_t *g_hyperblaster_speed;
extern cvar_t *g_railgun_damage;
extern cvar_t *g_rocketlauncher_damage;
extern cvar_t *g_rocketlauncher_speed;

extern cvar_t *bot_name_prefix;

#define world (&g_entities[0])

uint32_t GetUnicastKey();

// item spawnflags
constexpr spawnflags_t SPAWNFLAG_ITEM_TRIGGER_SPAWN		= 0x00000001_spawnflag;
constexpr spawnflags_t SPAWNFLAG_ITEM_NO_TOUCH			= 0x00000002_spawnflag;
constexpr spawnflags_t SPAWNFLAG_ITEM_TOSS_SPAWN		= 0x00000004_spawnflag;
constexpr spawnflags_t SPAWNFLAG_ITEM_SUSPENDED			= 0x00000008_spawnflag;
constexpr spawnflags_t SPAWNFLAG_ITEM_MAX				= 0x00000010_spawnflag;
// 8 bits reserved for editor flags & power cube bits
// (see SPAWNFLAG_NOT_EASY above)
constexpr spawnflags_t SPAWNFLAG_ITEM_DROPPED			= 0x00010000_spawnflag;
constexpr spawnflags_t SPAWNFLAG_ITEM_DROPPED_PLAYER	= 0x00020000_spawnflag;
constexpr spawnflags_t SPAWNFLAG_ITEM_TARGETS_USED		= 0x00040000_spawnflag;

extern gitem_t itemlist[IT_TOTAL];

//
// g_cmds.cpp
//
bool CheckFlood(gentity_t *ent);
void Cmd_Help_f(gentity_t *ent);
void Cmd_Score_f(gentity_t *ent);

void G_AssignPlayerSkin(gentity_t *ent, const char *s);

void TransitionVoteState(VoteState new_state);
void ClearVote();

//
// g_items.cpp
//
void		Powerup_ApplyRegeneration(gentity_t *ent);
void		QuadHog_DoSpawn(gentity_t *ent);
void		QuadHog_DoReset(gentity_t *ent);
void		QuadHog_SetupSpawn(gtime_t delay);
void		QuadHog_Spawn(gitem_t *item, gentity_t *spot, bool reset);
bool		AllowTechs();
void		Tech_DeadDrop(gentity_t *ent);
void		Tech_Reset();
void		Tech_SetupSpawn();
gitem_t		*Tech_Held(gentity_t *ent);
int			Tech_ApplyDisruptorShield(gentity_t *ent, int dmg);
int			Tech_ApplyPowerAmp(gentity_t *ent, int dmg);
bool		Tech_ApplyPowerAmpSound(gentity_t *ent);
bool		Tech_ApplyTimeAccel(gentity_t *ent);
void		Tech_ApplyTimeAccelSound(gentity_t *ent);
void		PrecacheItem(gitem_t *it);
bool		CheckItemEnabled(gitem_t *item);
gitem_t		*CheckItemReplacements(gitem_t *item);
void		InitItems();
void		SetItemNames();
gitem_t		*FindItem(const char *pickup_name);
gitem_t		*FindItemByClassname(const char *classname);
gentity_t		*Drop_Item(gentity_t *ent, gitem_t *item);
void		SetRespawn(gentity_t *ent, gtime_t delay, bool hide_self = true);
void		Change_Weapon(gentity_t *ent);
bool		SpawnItem(gentity_t *ent, gitem_t *item);
void		Think_Weapon(gentity_t *ent);
item_id_t	ArmorIndex(gentity_t *ent);
item_id_t	PowerArmorType(gentity_t *ent);
gitem_t		*GetItemByIndex(item_id_t index);
gitem_t		*GetItemByAmmo(ammo_t ammo);
gitem_t		*GetItemByPowerup(powerup_t powerup);
bool		Add_Ammo(gentity_t *ent, gitem_t *item, int count);
void		G_CheckPowerArmor(gentity_t *ent);
void		Touch_Item(gentity_t *ent, gentity_t *other, const trace_t &tr, bool other_touching_self);
void		P_ToggleFlashlight(gentity_t *ent, bool state);
bool		Entity_IsVisibleToPlayer(gentity_t *ent, gentity_t *player);
void		Compass_Update(gentity_t *ent, bool first);
item_id_t	DoRandomRespawn(gentity_t *ent);
void		RespawnItem(gentity_t *ent);
void		fire_doppelganger(gentity_t *ent, const vec3_t &start, const vec3_t &aimdir);

//
// g_utils.cpp
//
bool KillBox(gentity_t *ent, bool from_spawning, mod_id_t mod = MOD_TELEFRAG, bool bsp_clipping = true);
gentity_t *G_Find(gentity_t *from, std::function<bool(gentity_t *e)> matcher);

// Reject null-page and other non-canonical pointers before strlen/strcmp.
constexpr bool G_IsValidStringPtr(const char *p) {
	if (!p)
		return false;
	const auto addr = reinterpret_cast<uintptr_t>(p);
	return addr >= 0x1000 && addr <= 0x7FFFFFFFFFFF;
}

void G_LogInvalidEntityString(gentity_t *e, const char *ptr, const char *context);

// utility template for getting the type of a field
template<typename>
struct member_object_type {};
template<typename T1, typename T2>
struct member_object_type<T1 T2:: *> { using type = T1; };
template<typename T>
using member_object_type_t = typename member_object_type<std::remove_cv_t<T>>::type;

template<auto M>
gentity_t *G_FindByString(gentity_t *from, const std::string_view &value) {
	static_assert(std::is_same_v<member_object_type_t<decltype(M)>, const char *>, "can only use string member functions");

	return G_Find(from, [&](gentity_t *e) {
		const char *field = e->*M;
		if (!G_IsValidStringPtr(field)) {
			if (field)
				G_LogInvalidEntityString(e, field, value.data());
			return false;
		}
		return strlen(field) == value.length() && !Q_strncasecmp(field, value.data(), value.length());
		});
}

gentity_t *findradius(gentity_t *from, const vec3_t &org, float rad);
gentity_t *G_PickTarget(const char *targetname);
void	 G_UseTargets(gentity_t *ent, gentity_t *activator);
void	 G_PrintActivationMessage(gentity_t *ent, gentity_t *activator, bool coop_global);
void	 G_SetMovedir(vec3_t &angles, vec3_t &movedir);

void	 G_InitGentity(gentity_t *e);
gentity_t *G_Spawn();
void	 G_FreeEntity(gentity_t *e);

void G_TouchTriggers(gentity_t *ent);
void G_TouchProjectiles(gentity_t *ent, vec3_t previous_origin);

char *G_CopyString(const char *in, int32_t tag);

void G_PlayerNotifyGoal(gentity_t *player);

const char *Teams_TeamName(team_t team);
const char *Teams_OtherTeamName(team_t team);
team_t Teams_OtherTeam(team_t team);
bool Teams();
void G_AdjustPlayerScore(gclient_t *cl, int32_t offset, bool adjust_team, int32_t team_offset);
void G_SetPlayerScore(gclient_t *cl, int32_t value);
void G_AdjustTeamScore(team_t team, int32_t offset);
void G_SetTeamScore(team_t team, int32_t value);
const char *G_PlaceString(int rank);
bool ItemSpawnsEnabled();
bool loc_CanSee(gentity_t *targ, gentity_t *inflictor);
const char *G_TimeString(const int msec, bool state);
const char *G_TimeStringMs(const int msec, bool state);
void BroadcastFriendlyMessage(team_t team, const char *msg);
void BroadcastTeamMessage(team_t team, print_type_t level, const char *msg);
team_t StringToTeamNum(const char *in);
bool IsCombatDisabled();
bool IsPickupsDisabled();
gametype_t GT_IndexFromString(const char *in);
bool IsScoringDisabled();
void TeleportPlayerToRandomSpawnPoint(gentity_t *ent, bool fx);
bool InCoopStyle();
gentity_t *ClientEntFromString(const char *in);
ruleset_t RS_IndexFromString(const char *in);
void TeleporterVelocity(gentity_t *ent, gvec3_t angles);
int MS_Value(gclient_t *cl, mstats_t index);
void MS_Adjust(gclient_t *cl, mstats_t index, int count);
void MS_AdjustDuo(gclient_t *cl, mstats_t index1, mstats_t index2, int count);
void MS_Set(gclient_t *cl, mstats_t index, int value);
const char *stime();
void AnnouncerSound(gentity_t *ent, const char *announcer_sound, const char *backup_sound, bool use_backup);
void QLSound(gentity_t *ent, const char *ql_sound, const char *backup_sound, bool use_backup);
void G_StuffCmd(gentity_t *e, const char *fmt, ...);

//
// g_spawn.cpp
//
void  ED_CallSpawn(gentity_t *ent);
char *ED_NewString(char *string);
void GT_SetLongName(void);
void GT_PrecacheAssets();

//
// g_target.cpp
//
void target_laser_think(gentity_t *self);
void target_laser_off(gentity_t *self);

constexpr spawnflags_t SPAWNFLAG_LASER_ON = 0x0001_spawnflag;
constexpr spawnflags_t SPAWNFLAG_LASER_RED = 0x0002_spawnflag;
constexpr spawnflags_t SPAWNFLAG_LASER_GREEN = 0x0004_spawnflag;
constexpr spawnflags_t SPAWNFLAG_LASER_BLUE = 0x0008_spawnflag;
constexpr spawnflags_t SPAWNFLAG_LASER_YELLOW = 0x0010_spawnflag;
constexpr spawnflags_t SPAWNFLAG_LASER_ORANGE = 0x0020_spawnflag;
constexpr spawnflags_t SPAWNFLAG_LASER_FAT = 0x0040_spawnflag;
constexpr spawnflags_t SPAWNFLAG_LASER_ZAP = 0x80000000_spawnflag;
constexpr spawnflags_t SPAWNFLAG_LASER_LIGHTNING = 0x10000_spawnflag;

constexpr spawnflags_t SPAWNFLAG_HEALTHBAR_PVS_ONLY = 1_spawnflag;

// damage flags
enum damageflags_t {
	DAMAGE_NONE				= 0,			// no damage flags
	DAMAGE_RADIUS			= 0x00000001,	// damage was indirect
	DAMAGE_NO_ARMOR			= 0x00000002,	// armour does not protect from this damage
	DAMAGE_ENERGY			= 0x00000004,	// damage is from an energy based weapon
	DAMAGE_NO_KNOCKBACK		= 0x00000008,	// do not affect velocity, just view angles
	DAMAGE_BULLET			= 0x00000010,	// damage is from a bullet (used for ricochets)
	DAMAGE_NO_PROTECTION	= 0x00000020,	// armor, shields, protection, godmode etc. have no effect
	DAMAGE_DESTROY_ARMOR	= 0x00000040,	// damage is done to armor and health.
	DAMAGE_NO_REG_ARMOR		= 0x00000080,	// damage skips regular armor
	DAMAGE_NO_POWER_ARMOR	= 0x00000100,	// damage skips power armor
	DAMAGE_NO_INDICATOR		= 0x00000200,	// for clients: no damage indicators
	DAMAGE_STAT_ONCE		= 0x00000400	// only add as a hit to player stats once
};

MAKE_ENUM_BITFLAGS(damageflags_t);

//
// g_combat.cpp
//
bool OnSameTeam(gentity_t *ent1, gentity_t *ent2);
bool CanDamage(gentity_t *targ, gentity_t *inflictor);
bool CheckTeamDamage(gentity_t *targ, gentity_t *attacker);
void T_Damage(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker, const vec3_t &dir, const vec3_t &point,
	const vec3_t &normal, int damage, int knockback, damageflags_t dflags, mod_t mod);
void T_RadiusDamage(gentity_t *inflictor, gentity_t *attacker, float damage, gentity_t *ignore, float radius, damageflags_t dflags, mod_t mod);
void Killed(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, mod_t mod);

void T_RadiusNukeDamage(gentity_t *inflictor, gentity_t *attacker, float damage, gentity_t *ignore, float radius, mod_t mod);
void T_RadiusClassDamage(gentity_t *inflictor, gentity_t *attacker, float damage, char *ignoreClass, float radius, mod_t mod);
void cleanupHealTarget(gentity_t *ent);

constexpr int32_t DEFAULT_BULLET_HSPREAD = 300;
constexpr int32_t DEFAULT_BULLET_VSPREAD = 500;
constexpr int32_t DEFAULT_SHOTGUN_HSPREAD = 1000;
constexpr int32_t DEFAULT_SHOTGUN_VSPREAD = 500;
constexpr int32_t DEFAULT_SSHOTGUN_COUNT = 20;

//
// g_func.cpp
//
void train_use(gentity_t *self, gentity_t *other, gentity_t *activator);
void func_train_find(gentity_t *self);
gentity_t *plat_spawn_inside_trigger(gentity_t *ent);
void	 Move_Calc(gentity_t *ent, const vec3_t &dest, void(*endfunc)(gentity_t *self));
void G_SetMoveinfoSounds(gentity_t *self, const char *default_start, const char *default_mid, const char *default_end);

constexpr spawnflags_t SPAWNFLAG_TRAIN_START_ON = 1_spawnflag;

constexpr spawnflags_t SPAWNFLAG_WATER_SMART = 2_spawnflag;

constexpr spawnflags_t SPAWNFLAG_TRAIN_MOVE_TEAMCHAIN = 8_spawnflag;

constexpr spawnflags_t SPAWNFLAG_DOOR_REVERSE = 2_spawnflag;

//
// g_monster.cpp
//
void monster_muzzleflash(gentity_t *self, const vec3_t &start, monster_muzzleflash_id_t id);
void monster_fire_bullet(gentity_t *self, const vec3_t &start, const vec3_t &dir, int damage, int kick, int hspread,
	int vspread, monster_muzzleflash_id_t flashtype);
void monster_fire_shotgun(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int kick, int hspread,
	int vspread, int count, monster_muzzleflash_id_t flashtype);
void monster_fire_blaster(gentity_t *self, const vec3_t &start, const vec3_t &dir, int damage, int speed,
	monster_muzzleflash_id_t flashtype, effects_t effect);
void monster_fire_flechette(gentity_t *self, const vec3_t &start, const vec3_t &dir, int damage, int speed,
	monster_muzzleflash_id_t flashtype);
void monster_fire_grenade(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int speed,
	monster_muzzleflash_id_t flashtype, float right_adjust, float up_adjust);
void monster_fire_rocket(gentity_t *self, const vec3_t &start, const vec3_t &dir, int damage, int speed,
	monster_muzzleflash_id_t flashtype);
void monster_fire_railgun(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int kick,
	monster_muzzleflash_id_t flashtype);
void monster_fire_bfg(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int speed, int kick,
	float damage_radius, monster_muzzleflash_id_t flashtype);
bool M_CheckClearShot(gentity_t *self, const vec3_t &offset);
bool M_CheckClearShot(gentity_t *self, const vec3_t &offset, vec3_t &start);
vec3_t M_ProjectFlashSource(gentity_t *self, const vec3_t &offset, const vec3_t &forward, const vec3_t &right);
bool M_droptofloor_generic(vec3_t &origin, const vec3_t &mins, const vec3_t &maxs, bool ceiling, gentity_t *ignore, contents_t mask, bool allow_partial);
bool M_droptofloor(gentity_t *ent);
void monster_think(gentity_t *self);
void monster_dead_think(gentity_t *self);
void monster_dead(gentity_t *self);
void walkmonster_start(gentity_t *self);
void swimmonster_start(gentity_t *self);
void flymonster_start(gentity_t *self);
void monster_death_use(gentity_t *self);
void M_CatagorizePosition(gentity_t *self, const vec3_t &in_point, water_level_t &waterlevel, contents_t &watertype);
void M_WorldEffects(gentity_t *ent);
bool M_CheckAttack(gentity_t *self);
void M_CheckGround(gentity_t *ent, contents_t mask);
void monster_use(gentity_t *self, gentity_t *other, gentity_t *activator);
void M_ProcessPain(gentity_t *e);
bool M_ShouldReactToPain(gentity_t *self, const mod_t &mod);
void M_SetAnimation(gentity_t *self, const save_mmove_t &move, bool instant = true);
bool M_AllowSpawn(gentity_t *self);

// Paril: used in N64. causes them to be mad at the player
// regardless of circumstance.
constexpr size_t HACKFLAG_ATTACK_PLAYER = 1;
// used in N64, appears to change their behavior for the end scene.
constexpr size_t HACKFLAG_END_CUTSCENE = 4;

bool monster_start(gentity_t *self);
void monster_start_go(gentity_t *self);

void monster_fire_ionripper(gentity_t *self, const vec3_t &start, const vec3_t &dir, int damage, int speed,
	monster_muzzleflash_id_t flashtype, effects_t effect);
void monster_fire_heat(gentity_t *self, const vec3_t &start, const vec3_t &dir, int damage, int speed,
	monster_muzzleflash_id_t flashtype, float lerp_factor);
void monster_fire_dabeam(gentity_t *self, int damage, bool secondary, void(*update_func)(gentity_t *self));
void dabeam_update(gentity_t *self, bool damage);
void monster_fire_blueblaster(gentity_t *self, const vec3_t &start, const vec3_t &dir, int damage, int speed,
	monster_muzzleflash_id_t flashtype, effects_t effect);
void G_Monster_CheckCoopHealthScaling();

void monster_fire_blaster2(gentity_t *self, const vec3_t &start, const vec3_t &dir, int damage, int speed,
	monster_muzzleflash_id_t flashtype, effects_t effect);
void monster_fire_disruptor(gentity_t *self, const vec3_t &start, const vec3_t &dir, int damage, int speed, gentity_t *enemy,
	monster_muzzleflash_id_t flashtype);
void monster_fire_heatbeam(gentity_t *self, const vec3_t &start, const vec3_t &dir, const vec3_t &offset, int damage,
	int kick, monster_muzzleflash_id_t flashtype);
void stationarymonster_start(gentity_t *self);
void monster_done_dodge(gentity_t *self);

stuck_result_t G_FixStuckObject(gentity_t *self, vec3_t check);

// this is for the count of monsters
int32_t M_SlotsLeft(gentity_t *self);

// shared with monsters
constexpr spawnflags_t SPAWNFLAG_MONSTER_AMBUSH = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAG_MONSTER_TRIGGER_SPAWN = 2_spawnflag;
constexpr spawnflags_t SPAWNFLAG_MONSTER_DEAD = 16_spawnflag_bit;
constexpr spawnflags_t SPAWNFLAG_MONSTER_SUPER_STEP = 17_spawnflag_bit;
constexpr spawnflags_t SPAWNFLAG_MONSTER_NO_DROP = 18_spawnflag_bit;
constexpr spawnflags_t SPAWNFLAG_MONSTER_SCENIC = 19_spawnflag_bit;

// fixbot spawnflags
constexpr spawnflags_t SPAWNFLAG_FIXBOT_FIXIT = 4_spawnflag;
constexpr spawnflags_t SPAWNFLAG_FIXBOT_TAKEOFF = 8_spawnflag;
constexpr spawnflags_t SPAWNFLAG_FIXBOT_LANDING = 16_spawnflag;
constexpr spawnflags_t SPAWNFLAG_FIXBOT_WORKING = 32_spawnflag;

//
// g_misc.cpp
//
void ThrowClientHead(gentity_t *self, int damage);
void gib_die(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod);
gentity_t *ThrowGib(gentity_t *self, const char *gibname, int damage, gib_type_t type, float scale);
void BecomeExplosion1(gentity_t *self);
void misc_viper_use(gentity_t *self, gentity_t *other, gentity_t *activator);
void misc_strogg_ship_use(gentity_t *self, gentity_t *other, gentity_t *activator);
void VelocityForDamage(int damage, vec3_t &v);
void ClipGibVelocity(gentity_t *ent);
void TeleportPlayer(gentity_t *player, vec3_t origin, vec3_t angles);

constexpr spawnflags_t SPAWNFLAG_PATH_CORNER_TELEPORT = 1_spawnflag;

constexpr spawnflags_t SPAWNFLAG_POINT_COMBAT_HOLD = 1_spawnflag;

// max chars for a clock string;
// " 0:00:00" is the longest string possible
// plus null terminator.
constexpr size_t CLOCK_MESSAGE_SIZE = 9;

//
// g_ai.cpp
//
gentity_t *AI_GetSightClient(gentity_t *self);

void ai_stand(gentity_t *self, float dist);
void ai_move(gentity_t *self, float dist);
void ai_walk(gentity_t *self, float dist);
void ai_turn(gentity_t *self, float dist);
void ai_run(gentity_t *self, float dist);
void ai_charge(gentity_t *self, float dist);

constexpr float RANGE_MELEE = 20; // bboxes basically touching
constexpr float RANGE_NEAR = 440;
constexpr float RANGE_MID = 940;

// [Paril-KEX] adjusted to return an actual distance, measured
// in a way that is consistent regardless of what is fighting what
float range_to(gentity_t *self, gentity_t *other);

void FoundTarget(gentity_t *self);
void HuntTarget(gentity_t *self, bool animate_state = true);
bool infront(gentity_t *self, gentity_t *other);
bool visible(gentity_t *self, gentity_t *other, bool through_glass = true);
bool FacingIdeal(gentity_t *self);
// [Paril-KEX] generic function
bool M_CheckAttack_Base(gentity_t *self, float stand_ground_chance, float melee_chance, float near_chance, float mid_chance, float far_chance, float strafe_scalar);

//
// g_weapon.cpp
//
bool fire_hit(gentity_t *self, vec3_t aim, int damage, int kick);
void fire_bullet(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int kick, int hspread,
	int vspread, mod_t mod);
void fire_shotgun(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int kick, int hspread,
	int vspread, int count, mod_t mod);
void blaster_touch(gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self);
void fire_blaster(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int speed, effects_t effect,
	mod_t mod);
void fire_blueblaster(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int speed,
	effects_t effect);
void fire_greenblaster(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int speed, effects_t effect,
	bool hyper);
void Grenade_Explode(gentity_t *ent);
void fire_grenade(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int speed, gtime_t timer,
	float damage_radius, float right_adjust, float up_adjust, bool monster);
void fire_handgrenade(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int speed, gtime_t timer,
	float damage_radius, bool held);
void rocket_touch(gentity_t *ent, gentity_t *other, const trace_t &tr, bool other_touching_self);
gentity_t *fire_rocket(gentity_t *self, const vec3_t &start, const vec3_t &dir, int damage, int speed, float damage_radius,
	int radius_damage, int splash_knockback = 0);
void fire_rail(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int kick);
void fire_bfg(gentity_t *self, const vec3_t &start, const vec3_t &dir, int damage, int speed, float damage_radius);
void fire_ionripper(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int speed, effects_t effect);
void fire_heat(gentity_t *self, const vec3_t &start, const vec3_t &dir, int damage, int speed, float damage_radius,
	int radius_damage, float turn_fraction);
void fire_phalanx(gentity_t *self, const vec3_t &start, const vec3_t &dir, int damage, int speed, float damage_radius,
	int radius_damage);
void fire_trap(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, int speed);
void fire_plasmabeam(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, const vec3_t &offset, int damage, int kick,
	bool monster);
void fire_disruptor(gentity_t *self, const vec3_t &start, const vec3_t &dir, int damage, int speed, gentity_t *enemy);
void fire_flechette(gentity_t *self, const vec3_t &start, const vec3_t &dir, int damage, int speed, int kick);
void fire_prox(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int speed);
void fire_nuke(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, int speed);
bool fire_player_melee(gentity_t *self, const vec3_t &start, const vec3_t &aim, int reach, int damage, int kick, mod_t mod);
void fire_tesla(gentity_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int speed);

void fire_disintegrator(gentity_t *self, const vec3_t &start, const vec3_t &dir, int speed);
vec3_t P_CurrentKickAngles(gentity_t *ent);
vec3_t P_CurrentKickOrigin(gentity_t *ent);
void P_AddWeaponKick(gentity_t *ent, const vec3_t &origin, const vec3_t &angles);

void Nuke_Explode(gentity_t *ent);

// we won't ever pierce more than this many entities for a single trace.
constexpr size_t MAX_PIERCE = 16;

// base class for pierce args; this stores
// the stuff we are piercing.
struct pierce_args_t {
	// stuff we pierced
	std::array<gentity_t *, MAX_PIERCE> pierced;
	std::array<solid_t, MAX_PIERCE> pierce_solidities;
	size_t num_pierced = 0;
	// the last trace that was done, when piercing stopped
	trace_t tr;

	// mark entity as pierced
	inline bool mark(gentity_t *ent);

	// restore entities' previous solidities
	inline void restore();

	// we hit an entity; return false to stop the piercing.
	// you can adjust the mask for the re-trace (for water, etc).
	virtual bool hit(contents_t &mask, vec3_t &end) = 0;

	virtual ~pierce_args_t() {
		restore();
	}
};

void pierce_trace(const vec3_t &start, const vec3_t &end, gentity_t *ignore, pierce_args_t &pierce, contents_t mask);

//
// g_ptrail.cpp
//
void PlayerTrail_Add(gentity_t *player);
void PlayerTrail_Destroy(gentity_t *player);
gentity_t *PlayerTrail_Pick(gentity_t *self, bool next);

//
// g_client.cpp
//
constexpr spawnflags_t SPAWNFLAG_CHANGELEVEL_CLEAR_INVENTORY = 8_spawnflag;
constexpr spawnflags_t SPAWNFLAG_CHANGELEVEL_NO_END_OF_UNIT = 16_spawnflag;
constexpr spawnflags_t SPAWNFLAG_CHANGELEVEL_FADE_OUT = 32_spawnflag;
constexpr spawnflags_t SPAWNFLAG_CHANGELEVEL_IMMEDIATE_LEAVE = 64_spawnflag;

void ClientSetEliminated(gentity_t *self);
void ClientRespawn(gentity_t *ent);
void BeginIntermission(gentity_t *targ);
void ClientSpawn(gentity_t *ent);
void InitClientPersistant(gentity_t *ent, gclient_t *client);
void InitBodyQue();
void CopyToBodyQue(gentity_t *ent);
void ClientBeginServerFrame(gentity_t *ent);
void ClientUserinfoChanged(gentity_t *ent, const char *userinfo);
void P_AssignClientSkinnum(gentity_t *ent);
void P_ForceFogTransition(gentity_t *ent, bool instant);
void P_SendLevelPOI(gentity_t *ent);
unsigned int P_GetLobbyUserNum(const gentity_t *player);
void G_UpdateLevelEntry();
void G_EndOfUnitMessage();
bool SelectSpawnPoint(gentity_t *ent, vec3_t &origin, vec3_t &angles, bool force_spawn, bool &landmark);

struct select_spawn_result_t {
	gentity_t *spot;
	bool		any_valid = false; // set if a spawn point was found, even if it was taken
};

select_spawn_result_t SelectDeathmatchSpawnPoint(gentity_t *ent, vec3_t avoid_point, playerspawn_t mode, bool force_spawn, bool fallback_to_ctf_or_start, bool intermission, bool initial);
void G_PostRespawn(gentity_t *self);

//
// g_ctf.cpp
//
bool CTF_PickupFlag(gentity_t *ent, gentity_t *other);
void CTF_DropFlag(gentity_t *ent, gitem_t *item);
void CTF_ClientEffects(gentity_t *player);
void CTF_DeadDropFlag(gentity_t *self);
void CTF_FlagSetup(gentity_t *ent);
void CTF_ResetTeamFlag(team_t team);
void CTF_ResetFlags();
void CTF_ScoreBonuses(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker);
void CTF_CheckHurtCarrier(gentity_t *targ, gentity_t *attacker);


//
// g_player.cpp
//
void player_die(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod);

//
// g_svcmds.cpp
//
void ServerCommand();

//
// p_view.cpp
//
void ClientEndServerFrame(gentity_t *ent);
void G_LagCompensate(gentity_t *from_player, const vec3_t &start, const vec3_t &dir);
void G_UnLagCompensate();

//
// p_hud.cpp
//
void MoveClientToIntermission(gentity_t *ent);
void G_SetStats(gentity_t *ent);
void G_SetCoopStats(gentity_t *ent);
void G_SetSpectatorStats(gentity_t *ent);
void G_CheckChaseStats(gentity_t *ent);
void ValidateSelectedItem(gentity_t *ent);
void DeathmatchScoreboardMessage(gentity_t *ent, gentity_t *killer);
void TeamsScoreboardMessage(gentity_t *ent, gentity_t *killer);
void G_ReportMatchDetails(bool is_end);

//
// p_weapon.cpp
//
void PlayerNoise(gentity_t *who, const vec3_t &where, player_noise_t type);
void P_ProjectSource(gentity_t *ent, const vec3_t &angles, vec3_t distance, vec3_t &result_start, vec3_t &result_dir);
void NoAmmoWeaponChange(gentity_t *ent, bool sound);
void Weapon_Generic(gentity_t *ent, int FRAME_ACTIVATE_LAST, int FRAME_FIRE_LAST, int FRAME_IDLE_LAST,
	int FRAME_DEACTIVATE_LAST, const int *pause_frames, const int *fire_frames,
	void (*fire)(gentity_t *ent));
void Weapon_Repeating(gentity_t *ent, int FRAME_ACTIVATE_LAST, int FRAME_FIRE_LAST, int FRAME_IDLE_LAST,
	int FRAME_DEACTIVATE_LAST, const int *pause_frames, void (*fire)(gentity_t *ent));
void Throw_Generic(gentity_t *ent, int FRAME_FIRE_LAST, int FRAME_IDLE_LAST, int FRAME_PRIME_SOUND,
	const char *prime_sound, int FRAME_THROW_HOLD, int FRAME_THROW_FIRE, const int *pause_frames,
	int EXPLODE, const char *primed_sound, void (*fire)(gentity_t *ent, bool held), bool extra_idle_frame);
byte P_DamageModifier(gentity_t *ent);
bool InfiniteAmmoOn(gitem_t *item);
void Weapon_PowerupSound(gentity_t *ent);

// GRAPPLE
void Weapon_Grapple(gentity_t *ent);
void Weapon_Grapple_DoReset(gclient_t *cl);
void Weapon_Grapple_Pull(gentity_t *self);

// HOOK
void Weapon_Hook(gentity_t *ent);

constexpr gtime_t GRENADE_TIMER = 3_sec;
constexpr float GRENADE_MINSPEED = 400.f;
constexpr float GRENADE_MAXSPEED = 800.f;

extern player_muzzle_t is_silenced;
extern byte damage_multiplier;

//
// m_move.cpp
//
bool M_CheckBottom_Fast_Generic(const vec3_t &absmins, const vec3_t &absmaxs, bool ceiling);
bool M_CheckBottom_Slow_Generic(const vec3_t &origin, const vec3_t &absmins, const vec3_t &absmaxs, gentity_t *ignore, contents_t mask, bool ceiling, bool allow_any_step_height);
bool M_CheckBottom(gentity_t *ent);
bool G_CloseEnough(gentity_t *ent, gentity_t *goal, float dist);
bool M_walkmove(gentity_t *ent, float yaw, float dist);
void M_MoveToGoal(gentity_t *ent, float dist);
void M_ChangeYaw(gentity_t *ent);
bool ai_check_move(gentity_t *self, float dist);

//
// g_phys.cpp
//
constexpr float g_friction = 6;
constexpr float g_waterfriction = 1;

void		G_RunEntity(gentity_t *ent);
bool		G_RunThink(gentity_t *ent);
void		G_AddRotationalFriction(gentity_t *ent);
void		G_AddGravity(gentity_t *ent);
void		G_CheckVelocity(gentity_t *ent);
void		G_FlyMove(gentity_t *ent, float time, contents_t mask);
contents_t	G_GetClipMask(gentity_t *ent);
void		G_Impact(gentity_t *e1, const trace_t &trace);

//
// g_main.cpp
//
void SaveClientData();
void FetchClientEntData(gentity_t *ent);
void Match_End();
void SetIntermissionPoint(void);
void FindIntermissionPoint(void);
void CalculateRanks();
void CheckDMExitRules();
int GT_ScoreLimit();
const char *GT_ScoreLimitString();
void ExitLevel();
void Teams_CalcRankings(std::array<uint32_t, MAX_CLIENTS> &player_ranks); // [Paril-KEX]
void QueueIntermission(const char *msg, bool boo, bool reset);
gentity_t *CreateTargetChangeLevel(const char *map);
bool InAMatch();
void ChangeGametype(gametype_t gt);
void GT_Changes();
void SpawnEntities(const char *mapname, const char *entities, const char *spawnpoint);

//
// g_chase.cpp
//
void FreeFollower(gentity_t *ent);
void FreeClientFollowers(gentity_t *ent);
void UpdateChaseCam(gentity_t *ent);
void FollowNext(gentity_t *ent);
void FollowPrev(gentity_t *ent);
void GetFollowTarget(gentity_t *ent);

//====================
// ROGUE PROTOTYPES

//
// g_rogue_newai.cpp
//
bool	 blocked_checkplat(gentity_t *self, float dist);

enum class blocked_jump_result_t {
	NO_JUMP,
	JUMP_TURN,
	JUMP_JUMP_UP,
	JUMP_JUMP_DOWN
};

blocked_jump_result_t blocked_checkjump(gentity_t *self, float dist);
bool	 monsterlost_checkhint(gentity_t *self);
bool	 inback(gentity_t *self, gentity_t *other);
float	 realrange(gentity_t *self, gentity_t *other);
gentity_t *SpawnBadArea(const vec3_t &mins, const vec3_t &maxs, gtime_t lifespan, gentity_t *owner);
gentity_t *CheckForBadArea(gentity_t *ent);
bool	 MarkTeslaArea(gentity_t *self, gentity_t *tesla);
void	 InitHintPaths();
void PredictAim(gentity_t *self, gentity_t *target, const vec3_t &start, float bolt_speed, bool eye_height, float offset, vec3_t *aimdir,
	vec3_t *aimpoint);
bool M_CalculatePitchToFire(gentity_t *self, const vec3_t &target, const vec3_t &start, vec3_t &aim, float speed, float time_remaining, bool mortar, bool destroy_on_touch = false);
bool below(gentity_t *self, gentity_t *other);
void drawbbox(gentity_t *self);
void M_MonsterDodge(gentity_t *self, gentity_t *attacker, gtime_t eta, trace_t *tr, bool gravity);
void monster_duck_down(gentity_t *self);
void monster_duck_hold(gentity_t *self);
void monster_duck_up(gentity_t *self);
bool has_valid_enemy(gentity_t *self);
void TargetTesla(gentity_t *self, gentity_t *tesla);
void hintpath_stop(gentity_t *self);
gentity_t *PickCoopTarget(gentity_t *self);
int		 CountPlayers();
bool	 monster_jump_finished(gentity_t *self);
void BossExplode(gentity_t *self);

// g_rogue_func
void plat2_spawn_danger_area(gentity_t *ent);
void plat2_kill_danger_area(gentity_t *ent);

// g_rogue_spawn
gentity_t *CreateMonster(const vec3_t &origin, const vec3_t &angles, const char *classname);
gentity_t *CreateFlyMonster(const vec3_t &origin, const vec3_t &angles, const vec3_t &mins, const vec3_t &maxs,
	const char *classname);
gentity_t *CreateGroundMonster(const vec3_t &origin, const vec3_t &angles, const vec3_t &mins, const vec3_t &maxs,
	const char *classname, float height);
bool	 FindSpawnPoint(const vec3_t &startpoint, const vec3_t &mins, const vec3_t &maxs, vec3_t &spawnpoint,
	float maxMoveUp, bool drop = true);
bool	 CheckSpawnPoint(const vec3_t &origin, const vec3_t &mins, const vec3_t &maxs);
bool	 CheckGroundSpawnPoint(const vec3_t &origin, const vec3_t &entMins, const vec3_t &entMaxs, float height,
	float gravity);
void	 SpawnGrow_Spawn(const vec3_t &startpos, float start_size, float end_size);
void	 Widowlegs_Spawn(const vec3_t &startpos, const vec3_t &angles);

//
// g_rogue_sphere.cpp
//
void Defender_Launch(gentity_t *self);
void Vengeance_Launch(gentity_t *self);
void Hunter_Launch(gentity_t *self);

//
// p_client.cpp
//
void RemoveAttackingPainDaemons(gentity_t *self);
bool G_ShouldPlayersCollide(bool weaponry);
bool P_UseCoopInstancedItems();

constexpr spawnflags_t SPAWNFLAG_LANDMARK_KEEP_Z = 1_spawnflag;

// [Paril-KEX] convenience functions that returns true
// if the powerup should be 'active' (false to disable,
// will flash at 500ms intervals after 3 sec)
[[nodiscard]] constexpr bool G_PowerUpExpiringRelative(gtime_t left) {
	return left.milliseconds() > 3000 || (left.milliseconds() % 1000) < 500;
}

[[nodiscard]] constexpr bool G_PowerUpExpiring(gtime_t time) {
	return G_PowerUpExpiringRelative(time - level.time);
}

bool ClientIsPlaying(gclient_t *cl);
bool ClientCanVote(gclient_t *cl);

#include "p_menu.h"
//============================================================================

// client_t->anim_priority
enum anim_priority_t {
	ANIM_BASIC, // stand / run
	ANIM_WAVE,
	ANIM_JUMP,
	ANIM_PAIN,
	ANIM_ATTACK,
	ANIM_DEATH,

	// flags
	ANIM_REVERSED = bit_v<8>
};

MAKE_ENUM_BITFLAGS(anim_priority_t);

// height fog data values
struct height_fog_t {
	// r g b dist
	std::array<float, 4> start;
	std::array<float, 4> end;
	float falloff;
	float density;

	inline bool operator==(const height_fog_t &o) const {
		return start == o.start && end == o.end && falloff == o.falloff && density == o.density;
	}
};

constexpr gtime_t SELECTED_ITEM_TIME = 3_sec;

enum bmodel_animstyle_t : int32_t {
	BMODEL_ANIM_FORWARDS,
	BMODEL_ANIM_BACKWARDS,
	BMODEL_ANIM_RANDOM
};

struct bmodel_anim_t {
	// range, inclusive
	int32_t				start, end;
	bmodel_animstyle_t	style;
	int32_t				speed; // in milliseconds
	bool				nowrap;

	int32_t				alt_start, alt_end;
	bmodel_animstyle_t	alt_style;
	int32_t				alt_speed; // in milliseconds
	bool				alt_nowrap;

	// game-only
	bool				enabled;
	bool				alternate, currently_alternate;
	gtime_t				next_tick;
};

// never turn back shield on automatically; this is
// the legacy behavior.
constexpr int32_t AUTO_SHIELD_MANUAL = -1;
// when it is >= 0, the shield will turn back on
// when we have that many cells in our inventory
// if possible.
constexpr int32_t AUTO_SHIELD_AUTO = 0;

struct client_match_stats_t {
	uint32_t	total_dmg_dealt;
	uint32_t	total_dmg_received;

	uint32_t	total_shots;
	uint32_t	total_hits;

	uint32_t	total_kills;
	uint32_t	total_deaths;
};

// client data that stays across multiple level loads
struct client_persistant_t {
	char			userinfo[MAX_INFO_STRING];
	char			social_id[MAX_INFO_VALUE];
	char			netname[MAX_NETNAME];
	handedness_t	hand;
	auto_switch_t	autoswitch;
	int32_t			autoshield; // see AUTO_SHIELD_*

	bool			connected, spawned; // a loadgame will leave valid entities that
	// just don't have a connection yet

// values saved and restored from gentities when changing levels
	int32_t			health;
	int32_t			max_health;
	ent_flags_t		saved_flags;

	item_id_t		selected_item;
	gtime_t			selected_item_time;
	std::array<int32_t, IT_TOTAL>	  inventory;

	// ammo capacities
	std::array<int16_t, AMMO_MAX> max_ammo;

	gitem_t			*weapon;
	gitem_t			*lastweapon;

	int32_t			power_cubes; // used for tracking the cubes in coop games
	int32_t			score;		 // for calculating total unit score in coop games

	int32_t			game_help1changed, game_help2changed;
	int32_t			helpchanged; // flash F1 icon if non 0, play sound
	// and increment only if 1, 2, or 3
	gtime_t			help_time;

	bool			bob_skip; // [Paril-KEX] client wants no movement bob

	// [Paril-KEX] fog that we want to achieve; density rgb skyfogfactor
	std::array<float, 5> wanted_fog;
	height_fog_t	wanted_heightfog;
	// relative time value, copied from last touched trigger
	gtime_t			fog_transition_time;
	gtime_t			megahealth_time; // relative megahealth time value
	int32_t			lives; // player lives left (1 = no respawns remaining)
	uint8_t			n64_crouch_warn_times;
	gtime_t			n64_crouch_warning;

	//q3:
	player_team_state_t team_state;	// status in teamplay games
	bool			ingame;

	int32_t			dmg_scorer;		// for clan arena scoring from damage dealt
	int32_t			dmg_team;		// for team damage checks and warnings

	int				skin_icon_index;
	char			skin[MAX_INFO_VALUE];

	int32_t			vote_count;			// to prevent people from constantly calling votes
	int				voted;

	int32_t			health_bonus;
	gtime_t			health_bonus_timer;

	bool			timeout_used;

	bool			holdable_item_msg_adren;
	bool			holdable_item_msg_tele;
	bool			holdable_item_msg_doppel;

	gtime_t			medal_time;
	medal_t			medal_type;
	uint32_t		medal_count[MEDAL_TOTAL];

	bool			rail_hit;
	gtime_t			kill_time;

	gtime_t			last_spawn_time;
};

// player config vars:
struct client_config_t {
	bool			show_id;
	bool			show_timer;
	bool			show_fragmessages;
	int				killbeep_num;

	bool			follow_killer;
	bool			follow_leader;
	bool			follow_powerup;

	bool			use_expanded;
};

// client data that stays across deathmatch level changes, handled differently to client_persistent_t
struct client_session_t {
	client_config_t	pc;

	bool			initialised;

	team_t			team;
	spectator_state_t	spectator_state;
	int8_t			spectator_client;	// for chasecam and follow mode

	// client flags
	bool			admin;
	bool			is_888;
	bool			is_a_bot;

	// inactivity timer
	bool			inactive;
	gtime_t			inactivity_time;
	bool			inactivity_warning;

	// duel stats
	bool			duel_queued;
	int				wins, losses;

	// real time of team joining
	gtime_t			team_join_time;
};

// client data that stays across deathmatch respawns
struct client_respawn_t {
	client_persistant_t coop_respawn; // what to set client->pers to on a respawn
	gtime_t				entertime;	  // level.time the client entered the game
	int32_t				score;		  // frags, etc
	int32_t				old_score;		// track changes in score
	vec3_t				cmd_angles;	  // angles sent over in the last command

	bool				spectator; // client is a spectator

	int32_t				ctf_state;
	gtime_t				ctf_lasthurtcarrier;
	gtime_t				ctf_lastreturnedflag;
	gtime_t				ctf_flagsince;
	gtime_t				ctf_lastfraggedcarrier;
	gtime_t				lastidtime;
	bool				voted;
	bool				ready;
	ghost_t				*ghost; // for ghost codes

	int32_t				kill_count;	// for rampage award, reset on respawn

	bool				showed_motd;
	int					motd_mod_count;
	bool				showed_help;

	int					rank, old_rank;

	char				netname[MAX_NETNAME];
	gtime_t				team_delay_time;

	int					mstats[MSTAT_TOTAL];
};

// [Paril-KEX] seconds until we are fully invisible after
// making a racket
constexpr gtime_t INVISIBILITY_TIME = 2_sec;

// max number of individual damage indicators we'll track
constexpr size_t MAX_DAMAGE_INDICATORS = 4;

struct damage_indicator_t {
	vec3_t from;
	int32_t health, armor, power;
};

// time between ladder sounds
constexpr gtime_t LADDER_SOUND_TIME = 300_ms;

// time after damage that we can't respawn on a player for
constexpr gtime_t COOP_DAMAGE_RESPAWN_TIME = 2000_ms;

// time after firing that we can't respawn on a player for
constexpr gtime_t COOP_DAMAGE_FIRING_TIME = 2500_ms;

// this structure is cleared on each ClientSpawn(),
// except for 'client->pers'
struct gclient_t {
	// shared with server; do not touch members until the "private" section
	player_state_t ps; // communicated by server to clients
	int32_t		   ping;

	// private to game
	client_persistant_t pers;
	client_respawn_t	resp;
	client_session_t	sess;
	pmove_state_t		old_pmove; // for detecting out-of-pmove changes

	bool showscores;	// set layout stat
	bool showeou;       // end of unit screen
	bool showinventory; // set layout stat
	bool showhelp;

	button_t buttons;
	button_t oldbuttons;
	button_t latched_buttons;
	usercmd_t cmd; // last CMD send

	// weapon cannot fire until this time is up
	gtime_t weapon_fire_finished;
	// time between processing individual animation frames
	gtime_t weapon_think_time;
	// if we latched fire between server frames but before
	// the weapon fire finish has elapsed, we'll "press" it
	// automatically when we have a chance
	bool weapon_fire_buffered;
	bool weapon_thunk;

	gitem_t *newweapon;

	// sum up damage over an entire frame, so
	// shotgun blasts give a single big kick
	int32_t damage_armor;	  // damage absorbed by armor
	int32_t damage_parmor;	  // damage absorbed by power armor
	int32_t damage_blood;	  // damage taken out of health
	int32_t damage_knockback; // impact damage
	vec3_t	damage_from;	  // origin for vector calculation

	damage_indicator_t		  damage_indicators[MAX_DAMAGE_INDICATORS];
	uint8_t                   num_damage_indicators;

	float killer_yaw; // when dead, look at killer

	weaponstate_t	weaponstate;
	struct {
		vec3_t		angles, origin;
		gtime_t		time, total;
	} kick;
	gtime_t			quake_time;
	vec3_t			kick_origin;
	float			v_dmg_roll, v_dmg_pitch;
	gtime_t			v_dmg_time; // damage kicks
	gtime_t			fall_time;
	float			fall_value; // for view drop on fall
	float			damage_alpha;
	float			bonus_alpha;
	vec3_t			damage_blend;
	vec3_t			v_angle, v_forward; // aiming direction
	float			bobtime;			  // so off-ground doesn't change it
	vec3_t			oldviewangles;
	vec3_t			oldvelocity;
	gentity_t			*oldgroundentity; // [Paril-KEX]
	gtime_t			flash_time; // [Paril-KEX] for high tickrate

	gtime_t			next_drown_time;
	water_level_t	old_waterlevel;
	int32_t			breather_sound;

	int32_t			machinegun_shots; // for weapon raising
	int32_t			chaingun_shots;   // for alternating damage (4/5)

	// animation vars
	int32_t			anim_end;
	anim_priority_t	anim_priority;
	bool			anim_duck;
	bool			anim_run;
	gtime_t			anim_time;

	// powerup timers
	gtime_t pu_time_quad;
	gtime_t pu_time_haste;
	gtime_t pu_time_double;
	gtime_t pu_time_protection;
	gtime_t pu_time_invisibility;
	gtime_t pu_time_regeneration;
	gtime_t pu_time_rebreather;
	gtime_t pu_time_enviro;

	gtime_t	pu_regen_time_regen;
	gtime_t	pu_regen_time_blip;

	bool	grenade_blew_up;
	gtime_t grenade_time, grenade_finished_time;
	int32_t silencer_shots;
	int32_t weapon_sound;

	gtime_t pickup_msg_time;

	gtime_t flood_locktill; // locked from talking
	gtime_t flood_when[10]; // when messages were said
	int32_t flood_whenhead; // head pointer for when said

	gtime_t respawn_min_time; // can't respawn before time > this
	gtime_t respawn_time; // can respawn when time > this

	gentity_t *follow_queued_target;
	gtime_t	follow_queued_time;
	gentity_t *follow_target; // player we are following
	bool	 follow_update; // need to update follow info?

	gtime_t ir_time;
	gtime_t nuke_time;
	gtime_t tracker_pain_time;

	gentity_t *owned_sphere; // this points to the player's sphere

	gtime_t empty_click_sound;

	bool		inmenu;	  // in menu
	menu_hnd_t *menu;	  // current menu
	gtime_t		menutime; // time to update menu
	bool		menudirty;

	gentity_t	*grapple_ent;			// entity of grapple
	int32_t		grapple_state;			// true if pulling
	gtime_t		grapple_release_time;	// time of grapple release

	gtime_t		tech_regen_time;			// regen tech
	gtime_t		tech_sound_time;
	gtime_t		tech_last_message_time;

	gtime_t		frenzy_ammoregentime;

	gtime_t		vampire_expiretime;

	// used for player trails.
	gentity_t *trail_head, *trail_tail;
	// whether to use weapon chains
	bool no_weapon_chains;

	// seamless level transitions
	bool landmark_free_fall;
	const char *landmark_name;
	vec3_t landmark_rel_pos; // position relative to landmark, un-rotated from landmark angle
	gtime_t landmark_noise_time;

	gtime_t invisibility_fade_time; // [Paril-KEX] at this time, the player will be mostly fully cloaked
	gtime_t chase_msg_time; // to prevent CTF message spamming
	int32_t menu_sign; // menu sign
	vec3_t last_ladder_pos; // for ladder step sounds
	gtime_t last_ladder_sound;
	coop_respawn_t coop_respawn_state;
	gtime_t last_damage_time;

	// [Paril-KEX] these are now per-player, to work better in coop
	gentity_t *sight_entity;
	gtime_t	 sight_entity_time;
	gentity_t *sound_entity;
	gtime_t	 sound_entity_time;
	gentity_t *sound2_entity;
	gtime_t  sound2_entity_time;
	// saved positions for lag compensation
	uint8_t	 num_lag_origins; // 0 to MAX_LAG_ORIGINS, how many we can go back
	uint8_t  next_lag_origin; // the next one to write to
	bool     is_lag_compensated;
	vec3_t	 lag_restore_origin;
	// for high tickrate weapon angles
	vec3_t	 slow_view_angles;
	gtime_t	 slow_view_angle_time;

	// not saved
	bool help_draw_points;
	size_t help_draw_index, help_draw_count;
	gtime_t help_draw_time;
	uint32_t step_frame;
	int32_t help_poi_image;
	vec3_t help_poi_location;

	// only set temporarily
	bool awaiting_respawn;
	gtime_t respawn_timeout; // after this time, force a respawn

	vec3_t		spawn_origin;		// avoid this spawn location next time

	// [Paril-KEX] current active fog values; density rgb skyfogfactor
	std::array<float, 5> fog;
	height_fog_t heightfog;

	gtime_t	 last_attacker_time;
	// saved - for coop; last time we were in a firing state
	gtime_t	 last_firing_time;

	bool		eliminated;
	int16_t		horde_elim_msg_wave;

	bool		ready_to_exit;

	int			last_match_timer_update;

	client_match_stats_t mstats;

	gtime_t		initial_menu_delay;
	bool		initial_menu_shown;
	bool		initial_menu_closure;

	gtime_t		pu_last_message_time;

	gtime_t		last_banned_message_time;

	gtime_t		time_residual;

};

// ==========================================
// PLAT 2
// ==========================================
enum plat2flags_t {
	PLAT2_NONE = 0,
	PLAT2_CALLED = 1,
	PLAT2_MOVING = 2,
	PLAT2_WAITING = 4
};

MAKE_ENUM_BITFLAGS(plat2flags_t);

#include <bitset>

struct gentity_t {
	gentity_t() = delete;
	gentity_t(const gentity_t &) = delete;
	gentity_t(gentity_t &&) = delete;

	// shared with server; do not touch members until the "private" section
	entity_state_t s;
	gclient_t *client; // nullptr if not a player
	// the server expects the first part
	// of gclient_t to be a player_state_t
	// but the rest of it is opaque

	g_entity_t sv;	       // read only info about this entity for the server

	bool     inuse;

	// world linkage data
	bool     linked;
	int32_t	 linkcount;
	int32_t  areanum, areanum2;

	svflags_t  svflags;
	vec3_t	   mins, maxs;
	vec3_t	   absmin, absmax, size;
	solid_t	   solid;
	contents_t clipmask;
	gentity_t *owner;

	//================================

	// private to game
	int32_t spawn_count; // [Paril-KEX] used to differentiate different entities that may be in the same slot
	movetype_t	movetype;
	ent_flags_t flags;

	const char *model;
	gtime_t		freetime; // sv.time when the object was freed

	//
	// only used locally in game, not by server
	//
	const char *message;
	const char *classname;
	spawnflags_t	spawnflags;

	gtime_t timestamp;

	float		angle; // set in qe3, -1 = up, -2 = down
	const char *target;
	const char *targetname;
	const char *killtarget;
	const char *team;
	const char *pathtarget;
	const char *deathtarget;
	const char *healthtarget;
	const char *itemtarget; // [Paril-KEX]
	const char *combattarget;
	gentity_t *target_ent;

	float  speed, accel, decel;
	vec3_t movedir;
	vec3_t pos1, pos2, pos3;

	vec3_t	velocity;
	vec3_t	avelocity;
	int32_t mass;
	gtime_t air_finished;
	float	gravity; // per entity gravity multiplier (1.0 is normal)
	// use for lowgrav artifact, flares

	gentity_t *goalentity;
	gentity_t *movetarget;
	float	 yaw_speed;
	float	 ideal_yaw;

	gtime_t nextthink;
	save_prethink_t prethink;
	save_prethink_t postthink;
	save_think_t think;
	save_touch_t touch;
	save_use_t use;
	save_pain_t pain;
	save_die_t die;

	gtime_t touch_debounce_time; // are all these legit?  do we need more/less of them?
	gtime_t pain_debounce_time;
	gtime_t damage_debounce_time;
	gtime_t fly_sound_debounce_time; // move to clientinfo
	gtime_t last_move_time;

	int32_t		health;
	int32_t		max_health;
	int32_t		gib_health;
	gtime_t		show_hostile;

	gtime_t powerarmor_time;

	const char *map; // target_changelevel

	int32_t viewheight; // height above origin where eyesight is determined
	bool	deadflag;
	bool	takedamage;
	int32_t dmg;
	int32_t splash_damage;
	int32_t splash_knockback; // [MuffMode] 0 = use splash_damage for knockback (default)
	float	splash_radius;
	int32_t sounds; // make this a spawntemp var?
	int32_t count;

	gentity_t *chain;
	gentity_t *enemy;
	gentity_t *oldenemy;
	gentity_t *activator;
	gentity_t *groundentity;
	int32_t	 groundentity_linkcount;
	gentity_t *teamchain;
	gentity_t *teammaster;

	gentity_t *mynoise; // can go in client only
	gentity_t *mynoise2;

	int32_t noise_index;
	int32_t noise_index2;
	float	volume;
	float	attenuation;

	// timing variables
	float wait;
	float delay; // before firing targets
	float random;

	gtime_t teleport_time;

	contents_t	  watertype;
	water_level_t waterlevel;

	vec3_t move_origin;
	vec3_t move_angles;

	int32_t style; // also used as areaportal number

	gitem_t *item; // for bonus items

	// common data blocks
	moveinfo_t	  moveinfo;
	monsterinfo_t monsterinfo;

	plat2flags_t plat2flags;
	vec3_t		 offset;
	vec3_t		 gravityVector;
	gentity_t		*bad_area;
	gentity_t		*hint_chain;
	gentity_t		*monster_hint_chain;
	gentity_t		*target_hint_chain;
	int32_t		 hint_chain_id;

	char clock_message[CLOCK_MESSAGE_SIZE];

	// Paril: we died on this frame, apply knockback even if we're dead
	gtime_t dead_time;
	// used for dabeam monsters
	gentity_t *beam, *beam2;
	// proboscus for Parasite
	gentity_t *proboscus;
	// for vooping things
	gentity_t *disintegrator;
	gtime_t disintegrator_time;
	int32_t hackflags; // n64

	// fog stuff
	struct {
		vec3_t color;
		float density;
		float sky_factor;

		vec3_t color_off;
		float density_off;
		float sky_factor_off;
	} fog;

	struct {
		float falloff;
		float density;
		vec3_t start_color;
		float start_dist;
		vec3_t end_color;
		float end_dist;

		float falloff_off;
		float density_off;
		vec3_t start_color_off;
		float start_dist_off;
		vec3_t end_color_off;
		float end_dist_off;
	} heightfog;

	// instanced coop items
	std::bitset<MAX_CLIENTS>	item_picked_up_by;
	gtime_t						slime_debounce_time;

	// [Paril-KEX]
	bmodel_anim_t bmodel_anim;

	mod_t	lastMOD;
	const char *style_on, *style_off;
	uint32_t crosslevel_flags;
	// NOTE: if adding new elements, make sure to add them
	// in g_save.cpp too!

//muff
	const char *gametype;
	const char *not_gametype;
	const char *notteam;
	const char *notfree;
	const char *notq2;
	const char *notq3a;
	const char *notarena;
	const char *ruleset;
	const char *not_ruleset;
	const char *powerups_on;
	const char *powerups_off;
	const char *bfg_on;
	const char *bfg_off;
	const char *plasmabeam_on;
	const char *plasmabeam_off;

	gvec3_t		origin2;

	bool		skip;
//-muff

	// team for spawn spot
	team_t		fteam;
};

constexpr spawnflags_t SF_SPHERE_DEFENDER	= 0x0001_spawnflag;
constexpr spawnflags_t SF_SPHERE_HUNTER		= 0x0002_spawnflag;
constexpr spawnflags_t SF_SPHERE_VENGEANCE	= 0x0004_spawnflag;
constexpr spawnflags_t SF_DOPPELGANGER		= 0x10000_spawnflag;

constexpr spawnflags_t SF_SPHERE_TYPE = SF_SPHERE_DEFENDER | SF_SPHERE_HUNTER | SF_SPHERE_VENGEANCE;
constexpr spawnflags_t SF_SPHERE_FLAGS = SF_DOPPELGANGER;

struct dm_game_rt {
	void (*GameInit)();
	void (*PostInitSetup)();
	void (*ClientBegin)(gentity_t *ent);
	bool (*SelectSpawnPoint)(gentity_t *ent, vec3_t &origin, vec3_t &angles, bool force_spawn);
	void (*PlayerDeath)(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker);
	void (*Score)(gentity_t *attacker, gentity_t *victim, int scoreChange, const mod_t &mod);
	void (*PlayerEffects)(gentity_t *ent);
	void (*DogTag)(gentity_t *ent, gentity_t *killer, const char **pic);
	void (*PlayerDisconnect)(gentity_t *ent);
	int (*ChangeDamage)(gentity_t *targ, gentity_t *attacker, int damage, mod_t mod);
	int (*ChangeKnockback)(gentity_t *targ, gentity_t *attacker, int knockback, mod_t mod);
	int (*CheckDMExitRules)();
};

// [Paril-KEX]
inline void monster_footstep(gentity_t *self) {
	if (self->groundentity)
		self->s.event = EV_OTHER_FOOTSTEP;
}

// [Kex] helpers
// TFilter must be a type that is invokable with the
// signature bool(gentity_t *); it must return true if
// the entity given is valid for the given filter
template<typename TFilter>
struct entity_iterator_t {
	using iterator_category = std::random_access_iterator_tag;
	using value_type = gentity_t *;
	using reference = gentity_t *;
	using pointer = gentity_t *;
	using difference_type = ptrdiff_t;

private:
	uint32_t index;
	uint32_t end_index; // where the end index is located for this iterator
	// index < globals.num_entities are valid
	TFilter filter;

	// this doubles as the "end" iterator
	inline bool is_out_of_range(uint32_t i) const {
		return i >= end_index;
	}

	inline bool is_out_of_range() const {
		return is_out_of_range(index);
	}

	inline void throw_if_out_of_range() const {
		if (is_out_of_range())
			throw std::out_of_range("index");
	}

	inline difference_type clamped_index() const {
		if (is_out_of_range())
			return end_index;

		return index;
	}

public:
	// note: index is not affected by filter. it is up to
	// the caller to ensure this index is filtered.
	constexpr entity_iterator_t(uint32_t i, uint32_t end_index = -1) : index(i), end_index((end_index >= globals.num_entities) ? globals.num_entities : end_index) {}

	inline reference operator*() { throw_if_out_of_range(); return &g_entities[index]; }
	inline pointer operator->() { throw_if_out_of_range(); return &g_entities[index]; }

	inline entity_iterator_t &operator++() {
		throw_if_out_of_range();
		return *this = *this + 1;
	}

	inline entity_iterator_t &operator--() {
		throw_if_out_of_range();
		return *this = *this - 1;
	}

	inline difference_type operator-(const entity_iterator_t &it) const {
		return clamped_index() - it.clamped_index();
	}

	inline entity_iterator_t operator+(const difference_type &offset) const {
		entity_iterator_t it(index + offset, end_index);

		// move in the specified direction, only stopping if we
		// run out of range or find a filtered entity
		while (!is_out_of_range(it.index) && !filter(*it))
			it.index += offset > 0 ? 1 : -1;

		return it;
	}

	// + -1 and - 1 are the same (and - -1 & + 1)
	inline entity_iterator_t operator-(const difference_type &offset) const { return *this + (-offset); }

	// comparison. hopefully this won't break anything, but == and != use the
	// clamped index (so -1 and num_entities will be equal technically since they
	// are the same "invalid" entity) but <= and >= will affect them properly.
	inline bool operator==(const entity_iterator_t &it) const { return clamped_index() == it.clamped_index(); }
	inline bool operator!=(const entity_iterator_t &it) const { return clamped_index() != it.clamped_index(); }
	inline bool operator<(const entity_iterator_t &it) const { return index < it.index; }
	inline bool operator>(const entity_iterator_t &it) const { return index > it.index; }
	inline bool operator<=(const entity_iterator_t &it) const { return index <= it.index; }
	inline bool operator>=(const entity_iterator_t &it) const { return index >= it.index; }

	inline gentity_t *operator[](const difference_type &offset) const { return *(*this + offset); }
};

// iterate over range of entities, with the specified filter.
// can be "open-ended" (automatically expand with num_entities)
// by leaving the max unset.
template<typename TFilter>
struct entity_iterable_t {
private:
	uint32_t begin_index, end_index;
	TFilter filter;

	// find the first entity that matches the filter, from the specified index,
	// in the specified direction
	inline uint32_t find_matched_index(uint32_t index, int32_t direction) {
		while (index < globals.num_entities && !filter(&g_entities[index]))
			index += direction;

		return index;
	}

public:
	// iterate all allocated entities that match the filter,
	// including ones allocated after this iterator is constructed
	inline entity_iterable_t<TFilter>() : begin_index(find_matched_index(0, 1)), end_index(game.maxentities) {}
	// iterate all allocated entities that match the filter from the specified begin offset
	// including ones allocated after this iterator is constructed
	inline entity_iterable_t<TFilter>(uint32_t start) : begin_index(find_matched_index(start, 1)), end_index(game.maxentities) {}
	// iterate all allocated entities that match the filter from the specified begin offset
	// to the specified INCLUSIVE end offset (or the first entity that matches before it),
	// including end itself but not ones that may appear after this iterator is done
	inline entity_iterable_t<TFilter>(uint32_t start, uint32_t end) :
		begin_index(find_matched_index(start, 1)),
		end_index(find_matched_index(end, -1) + 1) {}

	inline entity_iterator_t<TFilter> begin() const { return entity_iterator_t<TFilter>(begin_index, end_index); }
	inline entity_iterator_t<TFilter> end() const { return end_index; }
};

// inuse clients that are connected; may not be spawned yet, however
struct active_clients_filter_t {
	inline bool operator()(gentity_t *ent) const {
		return (ent->inuse && ent->client && ent->client->pers.connected);
	}
};

inline entity_iterable_t<active_clients_filter_t> active_clients() {
	return entity_iterable_t<active_clients_filter_t> { 1u, game.maxclients };
}

// inuse players that are connected; may not be spawned yet, however
struct active_players_filter_t {
	inline bool operator()(gentity_t *ent) const {
		return (ent->inuse && ent->client && ent->client->pers.connected && ClientIsPlaying(ent->client));
	}
};

inline entity_iterable_t<active_players_filter_t> active_players() {
	return entity_iterable_t<active_players_filter_t> { 1u, game.maxclients };
}

struct gib_def_t {
	size_t count;
	const char *gibname;
	float scale;
	gib_type_t type;

	constexpr gib_def_t(size_t count, const char *gibname) :
		count(count),
		gibname(gibname),
		scale(1.0f),
		type(GIB_NONE) {}

	constexpr gib_def_t(size_t count, const char *gibname, gib_type_t type) :
		count(count),
		gibname(gibname),
		scale(1.0f),
		type(type) {}

	constexpr gib_def_t(size_t count, const char *gibname, float scale) :
		count(count),
		gibname(gibname),
		scale(scale),
		type(GIB_NONE) {}

	constexpr gib_def_t(size_t count, const char *gibname, float scale, gib_type_t type) :
		count(count),
		gibname(gibname),
		scale(scale),
		type(type) {}

	constexpr gib_def_t(const char *gibname, float scale, gib_type_t type) :
		count(1),
		gibname(gibname),
		scale(scale),
		type(type) {}

	constexpr gib_def_t(const char *gibname, float scale) :
		count(1),
		gibname(gibname),
		scale(scale),
		type(GIB_NONE) {}

	constexpr gib_def_t(const char *gibname, gib_type_t type) :
		count(1),
		gibname(gibname),
		scale(1.0f),
		type(type) {}

	constexpr gib_def_t(const char *gibname) :
		count(1),
		gibname(gibname),
		scale(1.0f),
		type(GIB_NONE) {}
};

// convenience function to throw different gib types
// NOTE: always throw the head gib *last* since self's size is used
// to position the gibs!
inline void ThrowGibs(gentity_t *self, int32_t damage, std::initializer_list<gib_def_t> gibs) {
	for (auto &gib : gibs)
		for (size_t i = 0; i < gib.count; i++)
			ThrowGib(self, gib.gibname, damage, gib.type, gib.scale * (self->s.scale ? self->s.scale : 1));
}

inline bool M_CheckGib(gentity_t *self, const mod_t &mod) {
	if (self->deadflag && mod.id == MOD_CRUSH)
		return true;

	return self->health <= self->gib_health;
}

// Fmt support for entities
template<>
struct fmt::formatter<gentity_t> {
	template<typename ParseContext>
	constexpr auto parse(ParseContext &ctx) {
		return ctx.begin();
	}

	template<typename FormatContext>
	auto format(const gentity_t &p, FormatContext &ctx) -> decltype(ctx.out()) {
		const char *classname = G_IsValidStringPtr(p.classname) ? p.classname : "<invalid-classname>";
		if (p.linked)
			return fmt::format_to(ctx.out(), FMT_STRING("{} @ {}"), classname, (p.absmax + p.absmin) * 0.5f);
		return fmt::format_to(ctx.out(), FMT_STRING("{} @ {}"), classname, p.s.origin);
	}
};

template<>
struct fmt::formatter<gentity_t *> : fmt::formatter<gentity_t> {
	using fmt::formatter<gentity_t>::parse;

	template<typename FormatContext>
	auto format(gentity_t *const &p, FormatContext &ctx) -> decltype(ctx.out()) {
		if (!p)
			return fmt::format_to(ctx.out(), FMT_STRING("null gentity_t"));
		return fmt::formatter<gentity_t>::format(*p, ctx);
	}
};

template<>
struct fmt::formatter<const gentity_t *> : fmt::formatter<gentity_t> {
	using fmt::formatter<gentity_t>::parse;

	template<typename FormatContext>
	auto format(const gentity_t *const &p, FormatContext &ctx) -> decltype(ctx.out()) {
		if (!p)
			return fmt::format_to(ctx.out(), FMT_STRING("null gentity_t"));
		return fmt::formatter<gentity_t>::format(*p, ctx);
	}
};

// POI tags used by this mod
enum pois_t : uint16_t {
	POI_OBJECTIVE = MAX_ENTITIES, // current objective
	POI_RED_FLAG, // red flag/carrier
	POI_BLUE_FLAG, // blue flag/carrier
	POI_PING,
	POI_PING_END = POI_PING + MAX_CLIENTS - 1,

	POI_HORDE_MONSTER_0,
	POI_HORDE_MONSTER_END = POI_HORDE_MONSTER_0 + 7,
};

// implementation of pierce stuff
inline bool pierce_args_t::mark(gentity_t *ent) {
	// ran out of pierces
	if (num_pierced == MAX_PIERCE)
		return false;

	pierced[num_pierced] = ent;
	pierce_solidities[num_pierced] = ent->solid;
	num_pierced++;

	ent->solid = SOLID_NOT;
	gi.linkentity(ent);

	return true;
}

// implementation of pierce stuff
inline void pierce_args_t::restore() {
	for (size_t i = 0; i < num_pierced; i++) {
		auto &ent = pierced[i];
		ent->solid = pierce_solidities[i];
		gi.linkentity(ent);
	}

	num_pierced = 0;
}

// [Paril-KEX] these are to fix a legacy bug with cached indices
// in save games. these can *only* be static/globals!
template<auto T>
struct cached_assetindex {
	static cached_assetindex<T> *head;

	const char *name;
	int32_t					index = 0;
	cached_assetindex *next = nullptr;

	inline cached_assetindex() {
		next = head;
		cached_assetindex<T>::head = this;
	}
	constexpr operator int32_t() const { return index; }

	// assigned from spawn functions
	inline void assign(const char *name) { this->name = name; index = (gi.*T)(name); }
	// cleared before SpawnEntities
	constexpr void clear() { index = 0; }
	// re-find the index for the given cached entry, if we were cached
	// by the regular map load
	inline void reset() { if (index) index = (gi.*T)(this->name); }

	static void reset_all() {
		auto asset = head;

		while (asset) {
			asset->reset();
			asset = asset->next;
		}
	}

	static void clear_all() {
		auto asset = head;

		while (asset) {
			asset->clear();
			asset = asset->next;
		}
	}
};

using cached_soundindex = cached_assetindex<&local_game_import_t::soundindex>;
using cached_modelindex = cached_assetindex<&local_game_import_t::modelindex>;
using cached_imageindex = cached_assetindex<&local_game_import_t::imageindex>;

template<> cached_soundindex *cached_soundindex::head;
template<> cached_modelindex *cached_modelindex::head;
template<> cached_imageindex *cached_imageindex::head;

extern cached_modelindex sm_meat_index;
extern cached_soundindex snd_fry;

