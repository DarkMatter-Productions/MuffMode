// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <string_view>

#include "g_local.h"
#include "bots/bot_debug.h"
#include "core/debug_log.h"
#include "shadow_lights.h"
// [MuffMode] Spawn filtering, statusbar and gametype hooks
#include "muffmode/mm_arena.h"
#include "muffmode/mm_awards.h"
#include "muffmode/mm_combat_heatmap.h"
#include "muffmode/mm_ent_respawn.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_gibs.h"
#include "muffmode/mm_map_pick.h"
#include "muffmode/mm_ghost.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_horde.h"
#include "muffmode/mm_items_rules.h"
#include "muffmode/mm_map_pool.h"
#include "muffmode/mm_player_stats.h"
#include "muffmode/mm_spawn_filter.h"
#include "muffmode/mm_spawn_rules.h"
#include "muffmode/mm_statusbar.h"
#include "muffmode/mm_strike.h"

bool G_CallSpawnClass(gentity_t *ent);

static void SpawnEnt_MapFixes(gentity_t *ent) {
	if (!Q_strcasecmp(level.mapname, "bunk1")) {
		if (!Q_strcasecmp(ent->classname, "func_button") && !Q_strcasecmp(ent->model, "*36")) {
			ent->wait = -1;
		}
		return;
	}
	if (!Q_strcasecmp(ent->classname, "item_health_mega")) {
		if (!Q_strcasecmp(level.mapname, "q2dm1")) {
			if (ent->s.origin == vec3_t{ 480, 1376, 912 }) {
				ent->s.angles = { 0, -45, 0 };
			}
			return;
		}
		if (!Q_strcasecmp(level.mapname, "q2dm8")) {
			if (ent->s.origin == vec3_t{ -832, 192, -232 }) {
				ent->s.angles = { 0, 90, 0 };
			}
			return;
		}
		if (!Q_strcasecmp(level.mapname, "fact3")) {
			if (ent->s.origin == vec3_t{ -80, 568, 144 }) {
				ent->s.angles = { 0, -90, 0 };
			}
			return;
		}
	}
}

// ----------

/*
===============
ED_CallSpawn

Finds the spawn function for the entity and calls it
===============
*/
void ED_CallSpawn(gentity_t *ent) {
	gitem_t	*item;
	int		 i;

	if (!ent->classname) {
		gi.Com_PrintFmt("{}: nullptr classname\n", __FUNCTION__);
		G_FreeEntity(ent);
		return;
	}

	// do this before calling the spawn function so it can be overridden.
	ent->gravityVector[0] = 0.0;
	ent->gravityVector[1] = 0.0;
	ent->gravityVector[2] = -1.0;

	ent->sv.init = false;

	MM_RemapSpawnClassname(ent);

	SpawnEnt_MapFixes(ent);

	// check item spawn functions
	for (i = 0, item = itemlist; i < IT_TOTAL; i++, item++) {
		if (!item->classname)
			continue;
		if (!strcmp(item->classname, ent->classname)) {
			// found it
			// before spawning, pick random item replacement
			if (g_dm_random_items->integer) {
				ent->item = item;
				item_id_t new_item = DoRandomRespawn(ent);

				if (new_item) {
					item = GetItemByIndex(new_item);
					ent->classname = item->classname;
				}
			}

			SpawnItem(ent, item);
			return;
		}
	}

	// check normal spawn functions
	if (G_CallSpawnClass(ent))
		return;

	if (!strcmp(ent->classname, "item_ball")) {
		G_FreeEntity(ent);
		return;
	}

	gi.Com_PrintFmt("{}: {} doesn't have a spawn function.\n", __FUNCTION__, *ent);
	G_FreeEntity(ent);
}

/*
=============
ED_NewString
=============
*/
char *ED_NewString(const char *string) {
	// [MuffMode] Escape expansion never grows the value, so strlen + 1 always
	// holds the result. The previous in-place loop dropped the terminator when a
	// value ended in a lone backslash, because the backslash consumed it, and
	// left every later reader of that field walking off the allocation.
	const std::string_view value = string ? std::string_view(string) : std::string_view();
	const size_t capacity = value.size() + 1;
	char *newb = (char *)gi.TagMalloc(capacity, TAG_LEVEL);

	MM_UnescapeEntityValue(value, newb, capacity);

	return newb;
}

//
// fields are used for spawning from the entity string
//

struct field_t {
	const char *name;
	void (*load_func) (gentity_t *e, const char *s) = nullptr;
};

// utility template for getting the type of a field
template<typename>
struct member_object_container_type {};
template<typename T1, typename T2>
struct member_object_container_type<T1 T2:: *> { using type = T2; };
template<typename T>
using member_object_container_type_t = typename member_object_container_type<std::remove_cv_t<T>>::type;

static bool ED_TryLoadFloat(const char *s, float &out) {
	const auto value = MM_ParseFloatArg(s);
	if (!value)
		return false;

	out = *value;
	return true;
}

template<typename T>
static T ED_LoadInteger(const char *s) {
	if (!s || !*s)
		return {};

	int64_t raw_value = 0;
	const char *begin = s;
	const char *end = s + strlen(s);
	const auto [ptr, ec] = std::from_chars(begin, end, raw_value);
	if (ec != std::errc{} || ptr != end) {
		return {};
	}

	const int64_t min_value = static_cast<int64_t>(std::numeric_limits<T>::min());
	const int64_t max_value = static_cast<int64_t>(std::numeric_limits<T>::max());
	return static_cast<T>(clamp(raw_value, min_value, max_value));
}

static float ED_LoadFloat(const char *s) {
	float value = 0.0f;
	ED_TryLoadFloat(s, value);
	return value;
}

struct type_loaders_t {
	template<typename T, std::enable_if_t<std::is_same_v<T, const char *>, int> = 0>
	static T load(const char *s) {
		return ED_NewString(s);
	}

	template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
	static T load(const char *s) {
		return ED_LoadInteger<T>(s);
	}

	template<typename T, std::enable_if_t<std::is_same_v<T, spawnflags_t>, int> = 0>
	static T load(const char *s) {
		return spawnflags_t(ED_LoadInteger<uint32_t>(s));
	}

	template<typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
	static T load(const char *s) {
		return static_cast<T>(ED_LoadFloat(s));
	}

	template<typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
	static T load(const char *s) {
		using underlying_t = std::underlying_type_t<T>;
		return static_cast<T>(ED_LoadInteger<underlying_t>(s));
	}

	template<typename T, std::enable_if_t<std::is_same_v<T, vec3_t>, int> = 0>
	static T load(const char *s) {
		// [MuffMode] Every component parses through a private buffer. `s` is the
		// value token ED_ParseEntity is still holding, which lives in COM_Parse's
		// shared buffer, so an unbuffered COM_Parse here rewrote the very string
		// it was reading. Sizing the buffer like the shared one keeps y and z at
		// the bound they already had and lifts x to match its siblings.
		char component[MAX_TOKEN_CHARS];
		vec3_t vec;
		vec.x = ED_LoadFloat(COM_Parse(&s, component, sizeof(component)));
		vec.y = ED_LoadFloat(COM_Parse(&s, component, sizeof(component)));
		vec.z = ED_LoadFloat(COM_Parse(&s, component, sizeof(component)));
		return vec;
	}
};

#define AUTO_LOADER_FUNC(M) \
	[](gentity_t *e, const char *s) { \
		e->M = type_loaders_t::load<decltype(e->M)>(s); \
	}

static int32_t ED_LoadColor(const char *value) {
	// space means rgba as values
	if (strchr(value, ' ')) {
		// [MuffMode] The buffer is per-call, not shared: `value` can point into
		// COM_Parse's own token buffer. Packing moved to MM_PackEntityColorRgba,
		// which clamps each channel before composing the word. Casting an
		// unbounded map float to int32_t is undefined, and the saturated result
		// then reached a left shift of a negative value; a merely large component
		// stayed defined but bled across the neighbouring channel boundary.
		char component[MM_ENTITY_COLOR_COMPONENT_CHARS];
		std::array<float, 4> raw_values{ 0, 0, 0, 1.0f };

		for (auto &v : raw_values) {
			const char *token = COM_Parse(&value, component, sizeof(component));

			if (*token)
				v = ED_LoadFloat(token);
		}

		return MM_PackEntityColorRgba(raw_values);
	}

	// integral
	return ED_LoadInteger<int32_t>(value);
}

#define FIELD_COLOR(n, x) \
	{ n, [](gentity_t *e, const char *s) { \
		e->x = ED_LoadColor(s); \
	} }

// clang-format off
// fields that get copied directly to gentity_t
#define FIELD_AUTO(x) \
	{ #x, AUTO_LOADER_FUNC(x) }

#define FIELD_AUTO_NAMED(n, x) \
	{ n, AUTO_LOADER_FUNC(x) }

static const std::initializer_list<field_t> entity_fields = {
	FIELD_AUTO(classname),
	FIELD_AUTO(model),
	FIELD_AUTO(spawnflags),
	FIELD_AUTO(speed),
	FIELD_AUTO(accel),
	FIELD_AUTO(decel),
	FIELD_AUTO(target),
	FIELD_AUTO(targetname),
	FIELD_AUTO(pathtarget),
	FIELD_AUTO(deathtarget),
	FIELD_AUTO(healthtarget),
	FIELD_AUTO(itemtarget),
	FIELD_AUTO(killtarget),
	FIELD_AUTO(combattarget),
	FIELD_AUTO(message),
	FIELD_AUTO(team),
	FIELD_AUTO(wait),
	FIELD_AUTO(delay),
	FIELD_AUTO(random),
	FIELD_AUTO(move_origin),
	FIELD_AUTO(move_angles),
	FIELD_AUTO(style),
	FIELD_AUTO(style_on),
	FIELD_AUTO(style_off),
	FIELD_AUTO(crosslevel_flags),
	FIELD_AUTO(count),
	FIELD_AUTO(arena),
	FIELD_AUTO(health),
	FIELD_AUTO(sounds),
	{ "light" },
	FIELD_AUTO(dmg),
	FIELD_AUTO(mass),
	FIELD_AUTO(volume),
	FIELD_AUTO(attenuation),
	FIELD_AUTO(map),
	FIELD_AUTO_NAMED("origin", s.origin),
	FIELD_AUTO_NAMED("angles", s.angles),
	{ "angle", [](gentity_t *e, const char *value) {
		e->s.angles = {};
		e->s.angles[YAW] = ED_LoadFloat(value);
	} },
	FIELD_COLOR("rgba", s.skinnum), // [Sam-KEX]
	FIELD_AUTO(hackflags), // [Paril-KEX] n64
	FIELD_AUTO_NAMED("alpha", s.alpha), // [Paril-KEX]
	FIELD_AUTO_NAMED("scale", s.scale), // [Paril-KEX]
	// [MuffMode] RA2 uses mangle on info_player_intermission for its
	// spectator view. Keep the legacy alias scoped to validated RA2 maps.
	{ "mangle", [](gentity_t *e, const char *value) {
		if (MM_Arena_Active())
			e->s.angles = type_loaders_t::load<vec3_t>(value);
	} },
	FIELD_AUTO_NAMED("dead_frame", monsterinfo.start_frame), // [Paril-KEX]
	FIELD_AUTO_NAMED("frame", s.frame),
	FIELD_AUTO_NAMED("effects", s.effects),
	FIELD_AUTO_NAMED("renderfx", s.renderfx),

	// [Paril-KEX] fog keys
	FIELD_AUTO_NAMED("fog_color", fog.color),
	FIELD_AUTO_NAMED("fog_color_off", fog.color_off),
	FIELD_AUTO_NAMED("fog_density", fog.density),
	FIELD_AUTO_NAMED("fog_density_off", fog.density_off),
	FIELD_AUTO_NAMED("fog_sky_factor", fog.sky_factor),
	FIELD_AUTO_NAMED("fog_sky_factor_off", fog.sky_factor_off),

	FIELD_AUTO_NAMED("heightfog_falloff", heightfog.falloff),
	FIELD_AUTO_NAMED("heightfog_density", heightfog.density),
	FIELD_AUTO_NAMED("heightfog_start_color", heightfog.start_color),
	FIELD_AUTO_NAMED("heightfog_start_dist", heightfog.start_dist),
	FIELD_AUTO_NAMED("heightfog_end_color", heightfog.end_color),
	FIELD_AUTO_NAMED("heightfog_end_dist", heightfog.end_dist),

	FIELD_AUTO_NAMED("heightfog_falloff_off", heightfog.falloff_off),
	FIELD_AUTO_NAMED("heightfog_density_off", heightfog.density_off),
	FIELD_AUTO_NAMED("heightfog_start_color_off", heightfog.start_color_off),
	FIELD_AUTO_NAMED("heightfog_start_dist_off", heightfog.start_dist_off),
	FIELD_AUTO_NAMED("heightfog_end_color_off", heightfog.end_color_off),
	FIELD_AUTO_NAMED("heightfog_end_dist_off", heightfog.end_dist_off),

	// [Paril-KEX] func_eye stuff
	FIELD_AUTO_NAMED("eye_position", move_origin),
	FIELD_AUTO_NAMED("vision_cone", yaw_speed),

	// [Paril-KEX] for trigger_coop_relay
	FIELD_AUTO_NAMED("message2", map),
	FIELD_AUTO(mins),
	FIELD_AUTO(maxs),

	// [Paril-KEX] customizable bmodel animations
	FIELD_AUTO_NAMED("bmodel_anim_start", bmodel_anim.start),
	FIELD_AUTO_NAMED("bmodel_anim_end", bmodel_anim.end),
	FIELD_AUTO_NAMED("bmodel_anim_style", bmodel_anim.style),
	FIELD_AUTO_NAMED("bmodel_anim_speed", bmodel_anim.speed),
	FIELD_AUTO_NAMED("bmodel_anim_nowrap", bmodel_anim.nowrap),

	FIELD_AUTO_NAMED("bmodel_anim_alt_start", bmodel_anim.alt_start),
	FIELD_AUTO_NAMED("bmodel_anim_alt_end", bmodel_anim.alt_end),
	FIELD_AUTO_NAMED("bmodel_anim_alt_style", bmodel_anim.alt_style),
	FIELD_AUTO_NAMED("bmodel_anim_alt_speed", bmodel_anim.alt_speed),
	FIELD_AUTO_NAMED("bmodel_anim_alt_nowrap", bmodel_anim.alt_nowrap),

	// [Paril-KEX] customizable power armor stuff
	FIELD_AUTO_NAMED("power_armor_power", monsterinfo.power_armor_power),
	{ "power_armor_type", [](gentity_t *s, const char *v) {
			int32_t type = ED_LoadInteger<int32_t>(v);

			if (type == 0)
				s->monsterinfo.power_armor_type = IT_NULL;
			else if (type == 1)
				s->monsterinfo.power_armor_type = IT_POWER_SCREEN;
			else
				s->monsterinfo.power_armor_type = IT_POWER_SHIELD;
		}
	},

//muff
	FIELD_AUTO(gametype),
	FIELD_AUTO(not_gametype),
	FIELD_AUTO(notteam),
	FIELD_AUTO(notfree),
	FIELD_AUTO(notq2),
	FIELD_AUTO(notq3a),
	FIELD_AUTO(notarena),
	FIELD_AUTO(ruleset),
	FIELD_AUTO(not_ruleset),
	FIELD_AUTO(powerups_on),
	FIELD_AUTO(powerups_off),
	FIELD_AUTO(bfg_on),
	FIELD_AUTO(bfg_off),
	FIELD_AUTO(plasmabeam_on),
	FIELD_AUTO(plasmabeam_off),

	// [MuffMode] Bespoke Horde spawn-anchor aliases. These reuse generic
	// gentity fields so authored .ent overrides need no private map format.
	FIELD_AUTO_NAMED("horde_monster", map),
	FIELD_AUTO_NAMED("horde_min_wave", count),
	FIELD_AUTO_NAMED("horde_max_wave", health),
	FIELD_AUTO_NAMED("horde_weight", random),
	FIELD_AUTO_NAMED("horde_cooldown", wait),
	FIELD_AUTO_NAMED("horde_boss", message),
	FIELD_AUTO_NAMED("horde_boss_health_mult", speed),
	FIELD_AUTO_NAMED("horde_boss_damage_mult", accel),
	FIELD_AUTO_NAMED("horde_boss_scale", s.scale),
	{ "horde_boss_spawnflags", [](gentity_t *s, const char *v) {
		s->sounds = ED_LoadInteger<int32_t>(v);
		s->noise_index2 = 1;
	} },
	{ "horde_boss_power_armor", [](gentity_t *s, const char *v) {
		s->monsterinfo.power_armor_power = ED_LoadInteger<int32_t>(v);
		s->volume = 1.f;
	} },
	{ "horde_boss_power_armor_type", [](gentity_t *s, const char *v) {
		const int32_t type = ED_LoadInteger<int32_t>(v);

		if (type == 0)
			s->monsterinfo.power_armor_type = IT_NULL;
		else if (type == 1)
			s->monsterinfo.power_armor_type = IT_POWER_SCREEN;
		else
			s->monsterinfo.power_armor_type = IT_POWER_SHIELD;
		s->decel = 1.f;
	} },
	{ "horde_boss_monster_slots", [](gentity_t *s, const char *v) {
		s->monsterinfo.monster_slots = ED_LoadInteger<int32_t>(v);
		s->attenuation = 1.f;
	} },
	{ "horde_boss_reinforcements", [](gentity_t *s, const char *v) {
		s->model = ED_NewString(v);
		s->noise_index = 1;
	} },
//-muff

	FIELD_AUTO_NAMED("monster_slots", monsterinfo.monster_slots)
};

#undef AUTO_LOADER_FUNC

#define AUTO_LOADER_FUNC(M) \
	[](spawn_temp_t *e, const char *s) { \
		e->M = type_loaders_t::load<decltype(e->M)>(s); \
	}

struct temp_field_t {
	const char *name;
	void (*load_func) (spawn_temp_t *e, const char *s) = nullptr;
};

// temp spawn vars -- only valid when the spawn function is called
// (copied to `st`)
static const std::initializer_list<temp_field_t> temp_fields = {
	FIELD_AUTO(lip),
	FIELD_AUTO(distance),
	FIELD_AUTO(height),
	FIELD_AUTO(phase),
	FIELD_AUTO(noise),
	FIELD_AUTO(pausetime),
	FIELD_AUTO(item),

	FIELD_AUTO(gravity),
	FIELD_AUTO(sky),
	FIELD_AUTO(skyrotate),
	FIELD_AUTO(skyaxis),
	FIELD_AUTO(skyautorotate),
	FIELD_AUTO(minyaw),
	FIELD_AUTO(maxyaw),
	FIELD_AUTO(minpitch),
	FIELD_AUTO(maxpitch),
	FIELD_AUTO(nextmap),
	FIELD_AUTO(music),  // [Edward-KEX]
	FIELD_AUTO(instantitems),
	FIELD_AUTO(radius), // [Paril-KEX]
	FIELD_AUTO(hub_map),
	FIELD_AUTO(achievement),

	FIELD_AUTO_NAMED("shadowlightradius", sl.data.radius),
	FIELD_AUTO_NAMED("shadowlightresolution", sl.data.resolution),
	FIELD_AUTO_NAMED("shadowlightintensity", sl.data.intensity),
	FIELD_AUTO_NAMED("shadowlightstartfadedistance", sl.data.fade_start),
	FIELD_AUTO_NAMED("shadowlightendfadedistance", sl.data.fade_end),
	FIELD_AUTO_NAMED("shadowlightstyle", sl.data.lightstyle),
	FIELD_AUTO_NAMED("shadowlightconeangle", sl.data.coneangle),
	FIELD_AUTO_NAMED("shadowlightstyletarget", sl.lightstyletarget),

	FIELD_AUTO(goals),

	FIELD_AUTO(image),

	FIELD_AUTO(fade_start_dist),
	FIELD_AUTO(fade_end_dist),
	FIELD_AUTO(start_items),
	FIELD_AUTO(no_grapple),
	FIELD_AUTO(no_dm_spawnpads),
	FIELD_AUTO(health_multiplier),

	FIELD_AUTO(reinforcements),
	FIELD_AUTO(noise_start),
	FIELD_AUTO(noise_middle),
	FIELD_AUTO(noise_end),

	FIELD_AUTO(loop_count),

	FIELD_AUTO(cvar),
	FIELD_AUTO(cvarvalue),
	FIELD_AUTO(author),
	FIELD_AUTO(author2),

	FIELD_AUTO(ruleset),

	FIELD_AUTO(nobots),
	FIELD_AUTO(nohumans),

};
// clang-format on

/*
===============
ED_ParseField

Takes a key/value pair and sets the binary values
in an entity
===============
*/
void ED_ParseField(const char *key, const char *value, gentity_t *ent) {

	// check st first
	for (auto &f : temp_fields) {
		if (Q_strcasecmp(f.name, key))
			continue;

		st.keys_specified.emplace(f.name);

		// found it
		if (f.load_func)
			f.load_func(&st, value);

		return;
	}

	// now entity
	for (auto &f : entity_fields) {
		if (Q_strcasecmp(f.name, key))
			continue;

		st.keys_specified.emplace(f.name);

		// [Paril-KEX]
		if (!strcmp(f.name, "bmodel_anim_start") || !strcmp(f.name, "bmodel_anim_end"))
			ent->bmodel_anim.enabled = true;

		// found it
		if (f.load_func)
			f.load_func(ent, value);

		return;
	}

	//gi.Com_PrintFmt("{} is not a valid field\n", key);
}

/*
====================
ED_ParseEntity

Parses an entity out of the given string, returning the new position
ed should be a properly initialized empty entity.
====================
*/
static const char *ED_ParseEntity(const char *data, gentity_t *ent) {
	bool  init;
	char  keyname[256];
	const char *com_token;
	char  last_keyname[256] = "";

	init = false;
	st = {};

	// go through all the dictionary pairs
	while (1) {
		// parse key
		com_token = COM_Parse(&data);
		if (com_token[0] == '}')
			break;
		if (!data) {
			MuffModeLog("DEBUG", "ED_ParseEntity: EOF parsing key, last_key='%s', ent classname='%s'",
			           last_keyname, ent->classname ? ent->classname : "(null)");
			gi.Com_Error("ED_ParseEntity: EOF without closing brace");
		}

		Q_strlcpy(keyname, com_token, sizeof(keyname));
		Q_strlcpy(last_keyname, keyname, sizeof(last_keyname));

		// parse value
		com_token = COM_Parse(&data);
		if (!data) {
			MuffModeLog("DEBUG", "ED_ParseEntity: EOF parsing value for key='%s', ent classname='%s'",
			           keyname, ent->classname ? ent->classname : "(null)");
			gi.Com_Error("ED_ParseEntity: EOF without closing brace");
		}

		if (com_token[0] == '}')
			gi.Com_Error("ED_ParseEntity: closing brace without data");

		init = true;

		// keynames with a leading underscore are used for utility comments,
		// and are immediately discarded by quake
		if (keyname[0] == '_') {
			// [Sam-KEX] Hack for setting RGBA for shadow-casting lights
			if (!strcmp(keyname, "_color"))
				ent->s.skinnum = ED_LoadColor(com_token);

			continue;
		}

		ED_ParseField(keyname, com_token, ent);
	}

	if (!init)
		memset(ent, 0, sizeof(*ent)); // NOLINT(bugprone-undefined-memory-manipulation): engine-owned gentity_t slots are intentionally raw C ABI storage.

	return data;
}

/*
================
G_FindTeams

Chain together all entities with a matching team field.

All but the first will have the FL_TEAMSLAVE flag set.
All but the last will have the teamchain field set to the next one
================
*/

// adjusts teams so that trains that move their children
// are in the front of the team
static uint32_t G_SpawnEntityLimit() {
	return min(globals.num_entities, game.maxentities);
}

static bool G_CanJoinEntityTeam(const gentity_t *first, const gentity_t *second) {
	if (notGT(GT_ARENA))
		return true;

	// Positive arena ids are map-owned namespaces. Keep identically named
	// mover/item teams from separate RA2 rooms independent during spawn setup.
	if (first->arena > 0 || second->arena > 0)
		return first->arena > 0 && first->arena == second->arena;

	return true;
}

static void G_FixTeams() {
	gentity_t *e, *e2, *chain;
	uint32_t i, j;
	uint32_t c;
	const uint32_t entity_limit = G_SpawnEntityLimit();

	c = 0;
	for (i = 1, e = g_entities + i; i < entity_limit; i++, e++) {
		if (!e->inuse)
			continue;
		if (!e->team)
			continue;
		if (!strcmp(e->classname, "func_train") && e->spawnflags.has(SPAWNFLAG_TRAIN_MOVE_TEAMCHAIN)) {
			if (e->flags & FL_TEAMSLAVE) {
				chain = e;
				e->teammaster = e;
				e->teamchain = nullptr;
				e->flags &= ~FL_TEAMSLAVE;
				e->flags |= FL_TEAMMASTER;
				c++;
				for (j = 1, e2 = g_entities + j; j < entity_limit; j++, e2++) {
					if (e2 == e)
						continue;
					if (!e2->inuse)
						continue;
					if (!e2->team)
						continue;
					if (!G_CanJoinEntityTeam(e, e2))
						continue;
					if (!strcmp(e->team, e2->team)) {
						chain->teamchain = e2;
						e2->teammaster = e;
						e2->teamchain = nullptr;
						chain = e2;
						e2->flags |= FL_TEAMSLAVE;
						e2->flags &= ~FL_TEAMMASTER;
						e2->movetype = MOVETYPE_PUSH;
						e2->speed = e->speed;
					}
				}
			}
		}
	}

	if (c)
		gi.Com_PrintFmt("{}: {} entity team{} repaired.\n", __FUNCTION__, c, c != 1 ? "s" : "");
}

static void G_FindTeams() {
	gentity_t *e1, *e2, *chain;
	uint32_t i, j;
	uint32_t c1, c2;
	const uint32_t entity_limit = G_SpawnEntityLimit();

	c1 = 0;
	c2 = 0;
	for (i = 1, e1 = g_entities + i; i < entity_limit; i++, e1++) {
		if (!e1->inuse)
			continue;
		if (!e1->team)
			continue;
		if (e1->flags & FL_TEAMSLAVE)
			continue;
		chain = e1;
		e1->teammaster = e1;
		e1->flags |= FL_TEAMMASTER;
		c1++;
		c2++;
		for (j = i + 1, e2 = e1 + 1; j < entity_limit; j++, e2++) {
			if (!e2->inuse)
				continue;
			if (!e2->team)
				continue;
			if (e2->flags & FL_TEAMSLAVE)
				continue;
			if (!G_CanJoinEntityTeam(e1, e2))
				continue;
			if (!strcmp(e1->team, e2->team)) {
				c2++;
				chain->teamchain = e2;
				e2->teammaster = e1;
				chain = e2;
				e2->flags |= FL_TEAMSLAVE;
			}
		}
	}

	G_FixTeams();

	if (c1 && g_verbose->integer)
		gi.Com_PrintFmt("{}: {} entity team{} found with a total of {} entit{}.\n", __FUNCTION__, c1, c1 != 1 ? "s" : "", c2, c2 != 1 ? "ies" : "y");
}

// inhibit entities from game based on cvars & spawnflags
static inline bool G_InhibitEntity(gentity_t *ent) {
	if (MM_ShouldInhibitSpawnEntity(ent))
		return true;

	if (ent->notteam && Teams())
		return true;
	if (ent->notfree && !Teams())
		return true;

	if (ent->powerups_on && g_no_powerups->integer)
		return true;
	if (ent->powerups_off && !g_no_powerups->integer)
		return true;

	if (ent->bfg_on && g_mapspawn_no_bfg->integer)
		return true;
	if (ent->bfg_off && !g_mapspawn_no_bfg->integer)
		return true;

	if (ent->plasmabeam_on && g_mapspawn_no_plasmabeam->integer)
		return true;
	if (ent->plasmabeam_off && !g_mapspawn_no_plasmabeam->integer)
		return true;

	// dm-only
	if (deathmatch->integer)
		return ent->spawnflags.has(SPAWNFLAG_NOT_DEATHMATCH);

	// coop flags
	if (coop->integer && ent->spawnflags.has(SPAWNFLAG_NOT_COOP))
		return true;
	else if (!coop->integer && ent->spawnflags.has(SPAWNFLAG_COOP_ONLY))
		return true;

	// skill
	return ((skill->integer == 0) && ent->spawnflags.has(SPAWNFLAG_NOT_EASY)) ||
		((skill->integer == 1) && ent->spawnflags.has(SPAWNFLAG_NOT_MEDIUM)) ||
		((skill->integer >= 2) && ent->spawnflags.has(SPAWNFLAG_NOT_HARD));
}

// [Paril-KEX]
void PrecacheInventoryItems() {
	if (deathmatch->integer)
		return;

	for (auto ce : active_clients()) {
		for (item_id_t id = IT_NULL; id != IT_TOTAL; id = static_cast<item_id_t>(id + 1))
			if (ce->client->pers.inventory[id])
				PrecacheItem(GetItemByIndex(id));
	}
}

static bool ValidateStartItems(char *invalid_item, size_t invalid_item_size) {
	if (invalid_item && invalid_item_size)
		invalid_item[0] = '\0';
	if (!g_start_items || !g_start_items->string || !*g_start_items->string)
		return true;

	char token_copy[MAX_TOKEN_CHARS];
	const char *ptr = g_start_items->string;

	while (const char *token = COM_ParseEx(&ptr, ";")) {
		if (!*token)
			break;

		Q_strlcpy(token_copy, token, sizeof(token_copy));
		const char *ptr_copy = token_copy;
		const char *item_name = COM_Parse(&ptr_copy);
		gitem_t *item = FindItemByClassname(item_name);
		if (item && item->pickup)
			continue;

		if (invalid_item && invalid_item_size)
			Q_strlcpy(invalid_item, item_name, invalid_item_size);
		return false;
	}

	return true;
}

static void PrecacheStartItems() {
	if (!g_start_items || !g_start_items->string || !*g_start_items->string)
		return;

	char token_copy[MAX_TOKEN_CHARS];
	const char *token;
	const char *ptr = g_start_items->string;

	while ((token = COM_ParseEx(&ptr, ";")) && *token) {
		Q_strlcpy(token_copy, token, sizeof(token_copy));
		const char *ptr_copy = token_copy;

		const char *item_name = COM_Parse(&ptr_copy);
		gitem_t *item = FindItemByClassname(item_name);

		if (!item || !item->pickup)
			gi.Com_ErrorFmt("Invalid g_start_item entry: {}\n", item_name);

		if (*ptr_copy)
			COM_Parse(&ptr_copy);

		PrecacheItem(item);
	}
}

static void PrecachePlayerSounds() {

	gi.soundindex("player/lava1.wav");
	gi.soundindex("player/lava2.wav");

	gi.soundindex("player/gasp1.wav"); // gasping for air
	gi.soundindex("player/gasp2.wav"); // head breaking surface, not gasping

	gi.soundindex("player/watr_in.wav");  // feet hitting water
	gi.soundindex("player/watr_out.wav"); // feet leaving water

	gi.soundindex("player/watr_un.wav"); // head going underwater

	gi.soundindex("player/u_breath1.wav");
	gi.soundindex("player/u_breath2.wav");

	gi.soundindex("player/wade1.wav");
	gi.soundindex("player/wade2.wav");
	gi.soundindex("player/wade3.wav");
	gi.soundindex("misc/talk1.wav");

	gi.soundindex("world/land.wav");   // landing thud
	gi.soundindex("misc/h2ohit1.wav"); // landing splash

	// gibs
	gi.soundindex("misc/udeath.wav");

	gi.soundindex("items/respawn1.wav");
	gi.soundindex("misc/mon_power2.wav");

	// sexed sounds
	gi.soundindex("*death1.wav");
	gi.soundindex("*death2.wav");
	gi.soundindex("*death3.wav");
	gi.soundindex("*death4.wav");
	gi.soundindex("*fall1.wav");
	gi.soundindex("*fall2.wav");
	gi.soundindex("*gurp1.wav"); // drowning damage
	gi.soundindex("*gurp2.wav");
	gi.soundindex("*jump1.wav"); // player jump
	gi.soundindex("*pain25_1.wav");
	gi.soundindex("*pain25_2.wav");
	gi.soundindex("*pain50_1.wav");
	gi.soundindex("*pain50_2.wav");
	gi.soundindex("*pain75_1.wav");
	gi.soundindex("*pain75_2.wav");
	gi.soundindex("*pain100_1.wav");
	gi.soundindex("*pain100_2.wav");
	gi.soundindex("*drown1.wav"); // [Paril-KEX]
}

void GT_PrecacheAssets() {
	if (Teams()) {
		if (notGT(GT_RR)) {
			ii_teams_header_red = gi.imageindex("tag4");
			ii_teams_header_blue = gi.imageindex("tag5");
		}
		ii_teams_red_default = gi.imageindex("i_ctf1");
		ii_teams_blue_default = gi.imageindex("i_ctf2");
		ii_teams_red_tiny = gi.imageindex("sbfctf1");
		ii_teams_blue_tiny = gi.imageindex("sbfctf2");
	}

	if (GT(GT_DUEL))
		ii_duel_header = gi.imageindex("/tags/default");

	if (GTF(GTF_CTF)) {
		ii_ctf_red_dropped = gi.imageindex("i_ctf1d");
		ii_ctf_blue_dropped = gi.imageindex("i_ctf2d");
		ii_ctf_red_taken = gi.imageindex("i_ctf1t");
		ii_ctf_blue_taken = gi.imageindex("i_ctf2t");
		mi_ctf_red_flag = gi.modelindex("players/male/flag1.md2");
		mi_ctf_blue_flag = gi.modelindex("players/male/flag2.md2");
	}
}

// [Paril-KEX]
static void PrecacheAssets() {
	if (!deathmatch->integer) {
		gi.soundindex("infantry/inflies1.wav");

		// help icon for statusbar
		gi.imageindex("i_help");
		gi.imageindex("help");
		gi.soundindex("misc/pc_up.wav");
	}

	level.pic_ping = gi.imageindex("loc_ping");

	level.pic_health = gi.imageindex("i_health");
	gi.imageindex("field_3");

	gi.soundindex("items/pkup.wav");   // bonus item pickup

	//gi.soundindex("items/damage.wav");
	//gi.soundindex("items/protect.wav");
	//gi.soundindex("items/protect4.wav");
	gi.soundindex("weapons/noammo.wav");
	gi.soundindex("weapons/lowammo.wav");
	gi.soundindex("weapons/change.wav");

	// gibs
	sm_meat_index.assign("models/objects/gibs/sm_meat/tris.md2");
	gi.modelindex("models/objects/gibs/arm/tris.md2");
	gi.modelindex("models/objects/gibs/bone/tris.md2");
	gi.modelindex("models/objects/gibs/bone2/tris.md2");
	gi.modelindex("models/objects/gibs/chest/tris.md2");
	gi.modelindex("models/objects/gibs/skull/tris.md2");
	gi.modelindex("models/objects/gibs/head2/tris.md2");
	gi.modelindex("models/objects/gibs/sm_metal/tris.md2");
	// [MuffMode] Registers the stock leg model and the unused player/gibimp*
	// impact sounds up front, so the enhanced gib paths never trigger a late
	// runtime configstring update mid-match.
	MM_Gibs_Precache();

	ii_highlight = gi.imageindex("i_ctfj");

	GT_PrecacheAssets();
}

#define	MAX_READ	0x10000		// read in blocks of 64k
static void FS_Read(void *buffer, int len, FILE *f) {
	int		block, remaining;
	int		read;
	byte *buf;
	int		tries;

	buf = (byte *)buffer;

	remaining = len;
	tries = 0;
	while (remaining) {
		block = remaining;
		if (block > MAX_READ)
			block = MAX_READ;
		read = fread(buf, 1, block, f);
		if (read == 0) {
			if (!tries) {
				tries = 1;
			} else
				gi.Com_Error("FS_Read: 0 bytes read");
		}

		if (read == -1)
			gi.Com_Error("FS_Read: -1 bytes read");

		remaining -= read;
		buf += read;
	}
}


/*
==============
VerifyEntityString
==============
*/
static bool VerifyEntityString(std::string_view entities) {
	if (entities.empty())
		return false;

	const size_t reserved_slots =
		static_cast<size_t>(game.maxclients) + BODY_QUEUE_SIZE;
	const size_t definition_capacity =
		static_cast<size_t>(game.maxentities) > reserved_slots
			? static_cast<size_t>(game.maxentities) - reserved_slots
			: 0;
	const mm_entity_lump_validation_t validation =
		MM_ValidateEntityLump(entities, definition_capacity);
	if (!validation.valid) {
		gi.Com_PrintFmt("{}: invalid entity string: {}.\n",
			__FUNCTION__, validation.error ? validation.error : "unknown error");
		return false;
	}
	return true;
}

static void PrecacheForRandomRespawn() {
	gitem_t *it;
	int		 i;
	int		 itflags;

	it = itemlist;
	for (i = 0; i < IT_TOTAL; i++, it++) {
		itflags = it->flags;

		if (!itflags || (itflags & (IF_NOT_GIVEABLE | IF_TECH | IF_NOT_RANDOM)) || !it->pickup || !it->world_model)
			continue;

		PrecacheItem(it);
	}
}

static void G_LocateSpawnSpots(void) {
	gentity_t *ent;
	int			n = 0;
	const char *s = "info_player_";
	const size_t sl = strlen(s);
	gentity_t *end = &g_entities[G_SpawnEntityLimit()];

	level.spawn_spots[SPAWN_SPOT_INTERMISSION] = nullptr;
	level.num_spawn_spots = 0;
	level.num_spawn_spots_free = 0;
	level.num_spawn_spots_team = 0;
	bool spawn_spot_overflow_warned = false;

	auto spawn_origin_usable = [](const gentity_t *spot) {
		return spot && std::isfinite(spot->s.origin.x) && std::isfinite(spot->s.origin.y) && std::isfinite(spot->s.origin.z);
	};

	auto cache_spawn_spot = [&](gentity_t *spot, team_t team) {
		if (!spawn_origin_usable(spot))
			return;

		if (n >= SPAWN_SPOT_INTERMISSION) {
			if (!spawn_spot_overflow_warned) {
				gi.Com_Print("G_LocateSpawnSpots: too many player spawn spots; ignoring extras for legacy spawn cache\n");
				spawn_spot_overflow_warned = true;
			}
			return;
		}

		level.spawn_spots[n++] = spot;
		spot->fteam = team;
		spot->count = 1; // means its not initial spawn point

		if (team == TEAM_FREE)
			level.num_spawn_spots_free++;
		else
			level.num_spawn_spots_team++;
	};

	// locate all spawn spots
	for (ent = g_entities; ent < end; ent++) {

		if (!ent->inuse || !ent->classname)
			continue;

		if (Q_strncasecmp(ent->classname, s, sl))
			continue;

		// intermission/ffa spots
		if (!Q_strncasecmp(ent->classname, s, sl)) {
			if (!Q_strcasecmp(ent->classname + sl, "intermission")) {
				if (!spawn_origin_usable(ent))
					continue;

				if (level.spawn_spots[SPAWN_SPOT_INTERMISSION] == NULL) {
					level.spawn_spots[SPAWN_SPOT_INTERMISSION] = ent; // put in the last slot
					ent->fteam = TEAM_FREE;

					// if it has a target, look towards it
					if (ent->target) {
						gentity_t *target = G_PickTarget(ent->target);

						if (spawn_origin_usable(target) && (target->s.origin - ent->s.origin)) {
							level.intermission_angle = vectoangles(target->s.origin - ent->s.origin);
						} else {
							level.intermission_angle = ent->s.angles;
						}
					} else
						level.intermission_angle = ent->s.angles;
				}
				continue;
			}
			if (!Q_strcasecmp(ent->classname + sl, "deathmatch")) {
				cache_spawn_spot(ent, TEAM_FREE);
				continue;
			}
			if (!Q_strcasecmp(ent->classname + sl, "team_red")) {
				cache_spawn_spot(ent, TEAM_RED);
				continue;
			}
			if (!Q_strcasecmp(ent->classname + sl, "team_blue")) {
				cache_spawn_spot(ent, TEAM_BLUE);
				continue;
			}
			continue;
		}
	}

	level.num_spawn_spots = n;
}

struct world_spawn_stats_t {
	int entity_count = 0;
	int inhibited = 0;
	int horde_anchors_converted = 0;
};

using item_inhibit_modes_t = std::array<int8_t, 6>;

static item_inhibit_modes_t cached_entity_item_inhibit_modes {};
static bool cached_entity_item_inhibit_modes_valid = false;

struct world_spawn_profile_t {
	int effective_gametype = GT_NONE;
	ruleset_t ruleset = RS_NONE;
	int configured_ruleset = RS_NONE;
	int deathmatch_mode = 0;
	int coop_mode = 0;
	int skill_mode = 0;
	bool teams = false;
	bool no_powerups = false;
	bool no_bfg = false;
	bool no_plasmabeam = false;
	bool random_items = false;
	int dm_spawnpads = 0;
	bool quadhog = false;
	bool allow_dm_monsters = false;
	bool precache_blaster = false;
	bool precache_horde_chainfist = false;
	bool precache_grapple = false;
	bool tech_setup = false;
	int tech_copies = 0;
	std::array<int16_t, IT_TOTAL> item_replacements {};
	std::array<uint8_t, IT_TOTAL> item_enabled {};
	std::string start_items;
};

static world_spawn_profile_t cached_world_spawn_profile {};
static bool cached_world_spawn_profile_valid = false;

static item_inhibit_modes_t CaptureItemInhibitModes() {
	return {
		game.item_inhibit_pu,
		game.item_inhibit_pa,
		game.item_inhibit_ht,
		game.item_inhibit_ar,
		game.item_inhibit_am,
		game.item_inhibit_wp
	};
}

static void ApplyItemInhibitModes(const item_inhibit_modes_t &modes) {
	game.item_inhibit_pu = modes[0];
	game.item_inhibit_pa = modes[1];
	game.item_inhibit_ht = modes[2];
	game.item_inhibit_ar = modes[3];
	game.item_inhibit_am = modes[4];
	game.item_inhibit_wp = modes[5];
}

static bool ShouldPrecacheBlaster() {
	return GT(GT_ARENA) ||
		(!(g_instagib->integer || GT(GT_INSTAGIB)) &&
		 !(g_nadefest->integer || GT(GT_NADEFEST)));
}

static bool ShouldPrecacheHordeChainfist() {
	return GT(GT_HORDE) && g_horde_start_chainsaw->integer;
}

static bool ShouldPrecacheGrapple() {
	return GT(GT_ARENA) ||
		(!strcmp(g_allow_grapple->string, "auto")
			? (GTF(GTF_CTF) && !level.no_grapple)
			: !!g_allow_grapple->integer);
}

static world_spawn_profile_t CaptureWorldSpawnProfile() {
	world_spawn_profile_t profile;
	profile.effective_gametype = MM_EFFECTIVE_GT;
	profile.ruleset = game.ruleset;
	profile.configured_ruleset = g_ruleset->integer;
	profile.deathmatch_mode = deathmatch->integer;
	profile.coop_mode = coop->integer;
	profile.skill_mode = skill->integer;
	profile.teams = Teams();
	profile.no_powerups = !!g_no_powerups->integer;
	profile.no_bfg = !!g_mapspawn_no_bfg->integer;
	profile.no_plasmabeam = !!g_mapspawn_no_plasmabeam->integer;
	profile.random_items = !!g_dm_random_items->integer;
	profile.dm_spawnpads = g_dm_spawnpads->integer;
	profile.quadhog = !!g_quadhog->integer;
	profile.allow_dm_monsters = !!ai_allow_dm_spawn->integer;
	profile.precache_blaster = ShouldPrecacheBlaster();
	profile.precache_horde_chainfist = ShouldPrecacheHordeChainfist();
	profile.precache_grapple = ShouldPrecacheGrapple();

	const bool allow_techs = AllowTechs();
	profile.tech_setup = allow_techs &&
		!(GT(GT_HORDE) && g_horde_tech_reset_each_wave->integer);
	// g_allow_techs is a boolean/auto setting. Tech_SpawnAll uses one copy of
	// each tech for either enabled spelling.
	profile.tech_copies = profile.tech_setup ? 1 : 0;

	for (int i = IT_NULL; i < IT_TOTAL; i++) {
		gitem_t *item = GetItemByIndex(static_cast<item_id_t>(i));
		if (!item || !item->classname) {
			profile.item_replacements[i] = static_cast<int16_t>(IT_NULL);
			profile.item_enabled[i] = 0;
			continue;
		}

		gitem_t *replacement = CheckItemReplacements(item);
		profile.item_replacements[i] = replacement
			? static_cast<int16_t>(replacement->id)
			: static_cast<int16_t>(IT_NULL);
		profile.item_enabled[i] =
			replacement && CheckItemEnabled(replacement) ? 1 : 0;
	}

	profile.start_items =
		g_start_items && g_start_items->string ? g_start_items->string : "";
	return profile;
}

static bool WorldSpawnProfilesMatch(
	const world_spawn_profile_t &lhs, const world_spawn_profile_t &rhs) {
	return lhs.effective_gametype == rhs.effective_gametype &&
		lhs.ruleset == rhs.ruleset &&
		lhs.configured_ruleset == rhs.configured_ruleset &&
		lhs.deathmatch_mode == rhs.deathmatch_mode &&
		lhs.coop_mode == rhs.coop_mode &&
		lhs.skill_mode == rhs.skill_mode &&
		lhs.teams == rhs.teams &&
		lhs.no_powerups == rhs.no_powerups &&
		lhs.no_bfg == rhs.no_bfg &&
		lhs.no_plasmabeam == rhs.no_plasmabeam &&
		lhs.random_items == rhs.random_items &&
		lhs.dm_spawnpads == rhs.dm_spawnpads &&
		lhs.quadhog == rhs.quadhog &&
		lhs.allow_dm_monsters == rhs.allow_dm_monsters &&
		lhs.precache_blaster == rhs.precache_blaster &&
		lhs.precache_horde_chainfist == rhs.precache_horde_chainfist &&
		lhs.precache_grapple == rhs.precache_grapple &&
		lhs.tech_setup == rhs.tech_setup &&
		lhs.tech_copies == rhs.tech_copies &&
		lhs.item_replacements == rhs.item_replacements &&
		lhs.item_enabled == rhs.item_enabled &&
		lhs.start_items == rhs.start_items;
}

static world_spawn_stats_t ParseWorldEntities() {
	world_spawn_stats_t stats;
	gentity_t *ent = nullptr;
	const char *entities = level.entstring.c_str();

	// parse entities
	while (1) {
		// parse the opening brace
		const char *com_token = COM_Parse(&entities);
		if (!entities)
			break;
		if (com_token[0] != '{')
			gi.Com_ErrorFmt("{}: Found \"{}\" when expecting {{ in entity string.", __FUNCTION__, com_token);

		stats.entity_count++;
		if (!ent) {
			ent = g_entities;
			G_InitGentity(ent);
		} else {
			ent = G_Spawn();
		}
		// [MuffMode] Keep the allocator failure boundary explicit before the
		// entity parser and compatibility hooks dereference the result.
		if (!ent) {
			gi.Com_ErrorFmt("{}: Failed to allocate an entity.", __FUNCTION__);
			return stats;
		}
		entities = ED_ParseEntity(entities, ent);

		// nasty hacks time!
		if (!strcmp(level.mapname, "bunk1")) {
			if (!strcmp(ent->classname, "func_button") && !Q_strcasecmp(ent->model, "*36")) {
				ent->wait = -1;
			}
		}

		// remove things (except the world) from different skill levels or deathmatch
		if (ent != g_entities) {
			if (MM_Horde_ConvertMapMonsterSpawn(ent))
				stats.horde_anchors_converted++;

			if (G_InhibitEntity(ent)) {
				G_FreeEntity(ent);
				stats.inhibited++;
				continue;
			}

			ent->spawnflags &= ~SPAWNFLAG_EDITOR_MASK;
		}

		// do this before calling the spawn function so it can be overridden.
		ent->gravityVector = { 0.0, 0.0, -1.0 };

		ED_CallSpawn(ent);

		// [MuffMode] Record map-authored props that respawn in deathmatch. Done
		// here rather than inside ED_CallSpawn so that runtime spawns -- a
		// target_spawner firing barrels, a debug "sv spawn" -- stay untracked.
		MM_EntRespawn_CaptureMapEntity(ent);

		ent->s.renderfx |= RF_IR_VISIBLE;
	}

	if (stats.inhibited && g_verbose->integer)
		gi.Com_PrintFmt("{} entities inhibited.\n", stats.inhibited);

	if (!g_entities[0].inuse ||
		!g_entities[0].classname ||
		strcmp(g_entities[0].classname, "worldspawn") ||
		g_entities[0].solid != SOLID_BSP ||
		g_entities[0].s.modelindex != MODELINDEX_WORLD) {
		gi.Com_ErrorFmt("{}: worldspawn failed to initialize.", __FUNCTION__);
	}

	return stats;
}

static world_spawn_stats_t SpawnCachedWorldEntities(bool initialize_level_services) {
	InitBodyQue();
	const world_spawn_stats_t stats = ParseWorldEntities();

	// Arena state and the combat heatmap span a live match and are only
	// initialized on a true map load. Arena never enters the singleton match
	// reload path.
	if (initialize_level_services)
		MM_Arena_Init();

	PrecacheStartItems();
	PrecacheInventoryItems();
	G_FindTeams();

	QuadHog_SetupSpawn(5_sec);
	Tech_SetupSpawn();

	if (deathmatch->integer) {
		if (g_dm_random_items->integer)
			PrecacheForRandomRespawn();
	} else {
		InitHintPaths();
	}

	G_LocateSpawnSpots();
	MM_Horde_FinalizeLevelSpawns();
	if (initialize_level_services)
		muffmode::combat_heatmap::ResetForNewLevel();

	SetIntermissionPoint();
	setup_shadow_lights();
	level.init = true;
	return stats;
}

/*
==============
SpawnEntities

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.
==============
*/
void SpawnEntities(const char *mapname, const char *entities, const char *spawnpoint) {
	// [MuffMode] The engine routes every server state through this entry point,
	// including demo playback, cinematics and pic screens. Those states load no
	// collision model and hand us an empty entity lump, so they reset level state
	// and spawn nothing rather than failing the real-map contract below.
	const bool map_has_world = MM_MapStateHasWorld(mapname);

	// A MyMap selection is a transaction with the next full engine map load.
	// Consume it before any fallible work, but keep the modes local until the
	// target map has passed the pre-reset validation path. A worldless state is
	// not that transition, so it leaves the selection armed for the next map.
	muffmode::maps::mymap_modifier_modes_t mymap_modifier_modes {};
	const bool has_mymap_modifier_modes = map_has_world &&
		MM_MQ_TakePendingModifiersForMap(mapname, mymap_modifier_modes);
	// One-shot modes must never be inherited from an earlier load. In
	// particular, Com_Error may leave SpawnCachedWorldEntities non-locally, so
	// clear any previously published modes before this load performs fallible
	// validation. A matching pending selection remains private in the local
	// snapshot above until entity spawning begins.
	MM_ClearItemInhibitFlags();

	// [MuffMode] Record the map being left before level.mapname is replaced.
	MM_RecordStructuredMapPlayed();
	MuffModeLog("MAP", "Loading map: '%s' (spawnpoint: '%s')", mapname, spawnpoint ? spawnpoint : "(none)");
	
	bool ent_file_exists = false;
	bool ent_valid = false;
	std::string override_entities;
	const std::string override_name = std::string(G_Fmt("baseq2/{}/{}.ent",
		g_entity_override_dir->string[0] ? g_entity_override_dir->string : "maps",
		mapname));
	// Worldless states have no authored entities to override or export.
	FILE *f = map_has_world ? fopen(override_name.c_str(), "rb") : nullptr;
	if (f != nullptr) {
		ent_file_exists = true;
		long length = -1;
		if (fseek(f, 0, SEEK_END) == 0)
			length = ftell(f);
		if (length > 0 && length <= 0x40000 && fseek(f, 0, SEEK_SET) == 0) {
			override_entities.resize(static_cast<size_t>(length));
			const size_t read_length =
				fread(override_entities.data(), 1, override_entities.size(), f);
			ent_valid = read_length == override_entities.size() &&
				VerifyEntityString(override_entities);
		}
		fclose(f);

		if (ent_valid && g_entity_override_load->integer) {
			entities = override_entities.c_str();
			if (g_verbose->integer)
				gi.Com_PrintFmt("{}: Entities override file verified and loaded: \"{}\"\n",
					__FUNCTION__, override_name);
		} else if (!ent_valid) {
			gi.Com_PrintFmt("{}: Entities override file load error for \"{}\", discarding.\n",
				__FUNCTION__, override_name);
		}
	}

	// save ent override
	if (g_entity_override_save->integer && map_has_world) {
		if (!ent_file_exists) {
			f = fopen(override_name.c_str(), "wb");
			if (f) {
				if (entities)
					fwrite(entities, 1, strlen(entities), f);
				if (g_verbose->integer)
					gi.Com_PrintFmt("{}: Entities override file written to: \"{}\"\n",
						__FUNCTION__, override_name);
				fclose(f);
			}
		}
	}

	// Own and validate the final effective lump before any level allocation or
	// entity is destroyed. The same exact bytes become the deterministic source
	// for every match/round world reload.
	std::string saved_entstring(entities ? entities : "");
	const size_t reserved_slots =
		static_cast<size_t>(game.maxclients) + BODY_QUEUE_SIZE;
	const size_t definition_capacity =
		static_cast<size_t>(game.maxentities) > reserved_slots
			? static_cast<size_t>(game.maxentities) - reserved_slots
			: 0;
	// Only a real map owes us a complete world; a worldless state legitimately
	// carries no entities and no start-item contract to honour.
	if (map_has_world) {
		const mm_entity_lump_validation_t validation =
			MM_ValidateEntityLump(saved_entstring, definition_capacity);
		if (!validation.valid) {
			gi.Com_ErrorFmt("{}: invalid entity string for map \"{}\": {}.",
				__FUNCTION__, mapname,
				validation.error ? validation.error : "unknown error");
		}
		char invalid_start_item[MAX_TOKEN_CHARS];
		if (!ValidateStartItems(invalid_start_item, sizeof(invalid_start_item))) {
			gi.Com_ErrorFmt("Invalid g_start_item entry: {}\n", invalid_start_item);
		}
	}
	item_inhibit_modes_t effective_item_inhibit_modes {};
	if (has_mymap_modifier_modes)
		effective_item_inhibit_modes = mymap_modifier_modes;

	const size_t ent_lump_bytes = saved_entstring.size();
	MuffModeLog("MAP", "SpawnEntities: phase=pre-reset map='%s' ent_lump_bytes=%zu",
		mapname, ent_lump_bytes);

	// clear cached indices
	cached_soundindex::clear_all();
	cached_modelindex::clear_all();
	cached_imageindex::clear_all();

	int skill_level = clamp(skill->integer, 0, 4);
	if (skill->integer != skill_level)
		gi.cvar_forceset("skill", G_Fmt("{}", skill_level).data());

	// [MuffMode] Validate the final (possibly overridden) entity lump before
	// freeing the previous level or spawning anything. Arena remains an
	// ordinary effective FFA unless the complete RA2 map contract passes.
	MM_Arena_PreflightMap(mapname, saved_entstring.c_str());

	// A direct gamemap/map_restart can bypass normal Match_End. Close the old
	// singleton rating lifecycle while its clients and reservations still exist;
	// the no-contest hook is exact-once after an ordinary completed match.
	MM_PlayerStats_OnMatchAbort();
	SaveClientData();

	// Menus own TAG_LEVEL allocations; release them while their pointers are
	// still valid rather than merely nulling dangling pointers after FreeTags.
	const size_t client_slots = min({
		static_cast<size_t>(game.maxclients),
		static_cast<size_t>(game.maxentities > 0 ? game.maxentities - 1 : 0),
		static_cast<size_t>(MAX_CLIENTS)
	});
	for (size_t i = 0; i < client_slots; i++) {
		gentity_t *client_ent = &g_entities[i + 1];
		client_ent->client = &game.clients[i];
		if (client_ent->client->menu)
			P_Menu_Close(client_ent);
	}

	Bot_ResetDebug();
	// [MuffMode] Prop respawn records borrow TAG_LEVEL strings; retire them while
	// those pointers are still valid.
	MM_EntRespawn_ClearAll();
	// [MuffMode] The live-gib budget holds raw slot references; drop them before
	// the entity array is reused so it cannot free a recycled slot.
	MM_Gibs_ClearAll();
	// [MuffMode] The next-map pick and the post-match awards reel keep their
	// state module-side, so neither comes back cleared with level_locals_t.
	MM_MapPick_Reset();
	MM_Awards_Reset();
	gi.FreeTags(TAG_LEVEL);

	// Proper C++ reset: destroy and reconstruct the whole object instead of
	// memset + partial placement-new, which leaked heap blocks on every gamemap.
	// Avoid assigning from a value-initialized temporary: level_locals_t is large
	// enough for that temporary to consume a significant fraction of the game
	// thread's stack.
	level.~level_locals_t();
	new (&level) level_locals_t {};
	level.entstring = std::move(saved_entstring);
	entities = level.entstring.c_str();
	MM_Ghost_ClearAll();

	MuffModeLog("MAP", "SpawnEntities: phase=reset-complete ent_lump_bytes=%zu",
		level.entstring.size());

	memset(g_entities, 0, game.maxentities * sizeof(g_entities[0])); // NOLINT(bugprone-undefined-memory-manipulation): engine-owned gentity_t slots are intentionally raw C ABI storage.

	// The entity array is empty again, so its published high-water mark must be
	// reset as well. Leaving the previous map's peak here makes the engine consume
	// hundreds of zeroed entity states during the first reconnect snapshot; after
	// a busy Horde map those stale slots can retain invalid renderer resources.
	globals.num_entities = static_cast<uint32_t>(
		min(static_cast<size_t>(game.maxclients) + 1, static_cast<size_t>(game.maxentities)));
	
	// all other flags are not important atm
	globals.server_flags &= SERVER_FLAG_LOADING;

	Q_strlcpy(level.mapname, mapname, sizeof(level.mapname));
	// Paril: fixes a bug where autosaves will start you at
	// the wrong spawnpoint if they happen to be non-empty
	// (mine2 -> mine3)
	if (!game.autosaved)
		Q_strlcpy(game.spawnpoint, spawnpoint, sizeof(game.spawnpoint));

	level.is_n64 = strncmp(level.mapname, "q64/", 4) == 0;
	
	const int gametype_index = static_cast<int>(MM_CurrentGametype());
	MuffModeLog("MAP", "Map name set: '%s' (is_n64=%d, gametype=%s)", 
	           level.mapname, level.is_n64 ? 1 : 0, gt_short_name[gametype_index]);

	level.coop_scale_players = 0;
	level.coop_health_scaling = clamp(g_coop_health_scaling->value, 0.f, 1.f);

	// set client fields on player entities
	for (size_t i = 0; i < game.maxclients; i++) {
		g_entities[i + 1].client = game.clients + i;

		// "disconnect" all players since the level is switching
		game.clients[i].pers.connected = false;
		game.clients[i].pers.spawned = false;
		// clear eliminated so horde-eliminated players don't deadlock the new map's warmup
		game.clients[i].eliminated = false;
	}

	// Log entity string state before parsing
	const size_t ent_str_len = level.entstring.size();
	MuffModeLog("MAP", "SpawnEntities: phase=parse-begin ptr=%p len=%zu first_32='%.32s'",
	           (void*)entities, ent_str_len, entities ? entities : "(null)");
	if (ent_str_len > 0) {
		// Log last 64 chars to see if string is truncated
		size_t tail_start = ent_str_len > 64 ? ent_str_len - 64 : 0;
		MuffModeLog("DEBUG", "SpawnEntities: entity string tail='%s'", entities + tail_start);
	}

	// [MuffMode] A worldless state now holds a fully reset level with no entities
	// and level.init still false, which is exactly what the engine expects while a
	// demo or cinematic streams. There is no world left to rebuild from, so the
	// match/round reload path must fall back until a real map loads again.
	if (!map_has_world) {
		cached_entity_item_inhibit_modes_valid = false;
		cached_world_spawn_profile_valid = false;
		MuffModeLog("MAP",
			"SpawnEntities: phase=complete map='%s' worldless state; no entities spawned",
			level.mapname);
		MuffModeLog_Separator();
		return;
	}

	// Publish the selected modes only once the matching map is committed to its
	// entity spawn. Validation or preflight failures above leave no global state
	// that a later map can inherit.
	ApplyItemInhibitModes(effective_item_inhibit_modes);
	const world_spawn_stats_t stats = SpawnCachedWorldEntities(true);
	cached_entity_item_inhibit_modes = effective_item_inhibit_modes;
	cached_entity_item_inhibit_modes_valid = true;
	// Random-item selection consumes the one-shot MyMap flags. Reapply the
	// effective map-load modes while recording the successful world's complete
	// spawn profile, then retain the normal post-load flag behavior.
	ApplyItemInhibitModes(effective_item_inhibit_modes);
	cached_world_spawn_profile = CaptureWorldSpawnProfile();
	cached_world_spawn_profile_valid = true;
	if (deathmatch->integer)
		MM_ClearItemInhibitFlags();

	MuffModeLog("MAP",
		"SpawnEntities: phase=parse-complete entities=%d inhibited=%d horde_anchors=%d num_entities=%u",
		stats.entity_count, stats.inhibited, stats.horde_anchors_converted,
		static_cast<unsigned>(globals.num_entities));

	MuffModeLog("MAP",
		"SpawnEntities: phase=complete map='%s' entities=%d horde_anchors=%d init=true",
		level.mapname, stats.entity_count, stats.horde_anchors_converted);
	MuffModeLog_Separator();
}

namespace {

struct world_reload_state_t {
	bool in_frame = false;
	gtime_t time;
	gtime_t start_time;
	gtime_t exit_time;
	bool ready_to_exit = false;

	std::array<char, MAX_QPATH> mapname {};
	std::array<char, MAX_QPATH> nextmap {};
	std::array<char, MAX_QPATH> forcemap {};
	std::string changemap;
	std::string entstring;

	gtime_t intermission_time;
	gtime_t intermission_queued;
	bool intermission_exit = false;
	bool intermission_eou = false;
	bool intermission_clear = false;
	bool intermission_fade = false;
	bool intermission_fading = false;
	gtime_t intermission_fade_time;
	bool respawn_intermission = false;
	int32_t intermission_server_frame = 0;

	level_entry_t *entry = nullptr;
	gentity_t *current_entity = nullptr;
	gentity_t *disguise_violator = nullptr;
	gtime_t disguise_violation_time;
	gtime_t next_auto_save;
	gtime_t next_match_report;

	VoteStateData vote_state;
	uint8_t num_connected_clients = 0;
	uint8_t num_nonspectator_clients = 0;
	uint8_t num_playing_clients = 0;
	uint8_t num_playing_human_clients = 0;
	std::array<int, MAX_CLIENTS> sorted_clients {};
	uint8_t follow1 = 0;
	uint8_t follow2 = 0;
	int num_living_red = 0;
	int num_eliminated_red = 0;
	int num_living_blue = 0;
	int num_eliminated_blue = 0;
	int num_living_free = 0;
	int num_playing_red = 0;
	int num_playing_blue = 0;

	std::array<int, TEAM_NUM_TEAMS> team_scores {};
	std::array<int, TEAM_NUM_TEAMS> team_old_scores {};
	match_state_t match_state = match_state_t::MATCH_NONE;
	warmup_req_t warmup_requisite = warmup_req_t::WARMUP_REQ_NONE;
	gtime_t warmup_notice_time;
	gtime_t warmup_gametype_hud_time;
	gtime_t match_time;
	gtime_t match_start_time;
	int match_state_queued = 0;
	gtime_t match_state_timer;
	int warmup_modification_count = 0;
	gtime_t countdown_check;
	gtime_t matchendwarn_check;
	gtime_t match_cancel_delay_timer;

	int round_number = 0;
	uint32_t round_epoch = 0;
	uint32_t world_epoch = 0;
	round_state_t round_state = round_state_t::ROUND_NONE;
	int round_state_queued = 0;
	gtime_t round_state_timer;
	bool restarted = false;
	gtime_t overtime;
	bool suddendeath = false;
	gtime_t tied_overtime_start;
	std::array<int, TEAM_NUM_TEAMS> count_living {};
	std::array<int, TEAM_NUM_TEAMS> last_standing_count {};
	std::array<bool, TEAM_NUM_TEAMS> locked {};
	std::array<gentity_t *, TEAM_NUM_TEAMS> captain {};

	gtime_t ctf_last_flag_capture;
	team_t ctf_last_capture_team = TEAM_NONE;
	std::array<ghost_t, MAX_CLIENTS> ghosts {};
	gtime_t no_players_time;
	int total_player_deaths = 0;

	bool strike_red_attacks = false;
	bool strike_flag_touch = false;
	int8_t strike_turn = 0;

	gtime_t timeout_in_place;
	gentity_t *timeout_ent = nullptr;
	bool timeout_auto = false;
	bool timeout_resuming = false;
	std::string match_id;
	mm_match_overall_stats_t match;
	std::array<bool, 3> frag_warning {};
	bool prepare_to_fight = false;
	std::array<char, 64> intermission_victor_msg {};

	std::array<bool, MAX_CLIENTS> client_was_linked {};
	std::array<int, MAX_CLIENTS> client_ghost_index {};
};

size_t ReloadClientSlotCount()
{
	if (game.maxentities <= 1)
		return 0;
	return min({
		static_cast<size_t>(game.maxclients),
		static_cast<size_t>(game.maxentities) - 1,
		static_cast<size_t>(MAX_CLIENTS)
	});
}

gentity_t *ReloadEntity(gentity_t *ent)
{
	if (!ent || !g_entities)
		return nullptr;

	const uintptr_t address = reinterpret_cast<uintptr_t>(ent);
	const uintptr_t base = reinterpret_cast<uintptr_t>(g_entities);
	if (address < base)
		return nullptr;
	const uintptr_t delta = address - base;
	if (delta % sizeof(gentity_t))
		return nullptr;

	const size_t index = static_cast<size_t>(delta / sizeof(gentity_t));
	return index < static_cast<size_t>(game.maxentities) ? &g_entities[index] : nullptr;
}

gentity_t *ReloadClientEntity(gentity_t *ent)
{
	ent = ReloadEntity(ent);
	if (!ent)
		return nullptr;

	const size_t index = static_cast<size_t>(ent - g_entities);
	return index >= 1 && index <= ReloadClientSlotCount() ? ent : nullptr;
}

gentity_t *ReloadDynamicEntity(gentity_t *ent)
{
	ent = ReloadEntity(ent);
	if (!ent)
		return nullptr;

	const size_t index = static_cast<size_t>(ent - g_entities);
	const size_t first_dynamic =
		static_cast<size_t>(game.maxclients) + BODY_QUEUE_SIZE + 1;
	return index >= first_dynamic ? ent : nullptr;
}

gclient_t *ReloadClient(gclient_t *client)
{
	if (!client || !game.clients)
		return nullptr;

	const uintptr_t address = reinterpret_cast<uintptr_t>(client);
	const uintptr_t base = reinterpret_cast<uintptr_t>(game.clients);
	if (address < base)
		return nullptr;
	const uintptr_t delta = address - base;
	if (delta % sizeof(gclient_t))
		return nullptr;

	const size_t index = static_cast<size_t>(delta / sizeof(gclient_t));
	return index < ReloadClientSlotCount() ? &game.clients[index] : nullptr;
}

int ReloadGhostIndex(const ghost_t *ghost)
{
	if (!ghost)
		return -1;

	const uintptr_t address = reinterpret_cast<uintptr_t>(ghost);
	const uintptr_t base = reinterpret_cast<uintptr_t>(&level.ghosts[0]);
	if (address < base)
		return -1;
	const uintptr_t delta = address - base;
	if (delta % sizeof(ghost_t))
		return -1;

	const size_t index = static_cast<size_t>(delta / sizeof(ghost_t));
	return index < std::size(level.ghosts) ? static_cast<int>(index) : -1;
}

std::unique_ptr<world_reload_state_t> CaptureWorldReloadState()
{
	auto state = std::make_unique<world_reload_state_t>();
	state->client_ghost_index.fill(-1);

	state->in_frame = level.in_frame;
	state->time = level.time;
	state->start_time = level.start_time;
	state->exit_time = level.exit_time;
	state->ready_to_exit = level.ready_to_exit;
	Q_strlcpy(state->mapname.data(), level.mapname, state->mapname.size());
	Q_strlcpy(state->nextmap.data(), level.nextmap, state->nextmap.size());
	Q_strlcpy(state->forcemap.data(), level.forcemap, state->forcemap.size());
	if (level.changemap)
		state->changemap = level.changemap;
	state->entstring = level.entstring;

	state->intermission_time = level.intermission_time;
	state->intermission_queued = level.intermission_queued;
	state->intermission_exit = level.intermission_exit;
	state->intermission_eou = level.intermission_eou;
	state->intermission_clear = level.intermission_clear;
	state->intermission_fade = level.intermission_fade;
	state->intermission_fading = level.intermission_fading;
	state->intermission_fade_time = level.intermission_fade_time;
	state->respawn_intermission = level.respawn_intermission;
	state->intermission_server_frame = level.intermission_server_frame;

	state->entry = level.entry;
	state->current_entity = ReloadClientEntity(level.current_entity);
	state->disguise_violator = ReloadClientEntity(level.disguise_violator);
	state->disguise_violation_time = level.disguise_violation_time;
	state->next_auto_save = level.next_auto_save;
	state->next_match_report = level.next_match_report;

	state->vote_state = level.vote_state;
	state->vote_state.caller = ReloadClient(state->vote_state.caller);
	state->num_connected_clients = level.num_connected_clients;
	state->num_nonspectator_clients = level.num_nonspectator_clients;
	state->num_playing_clients = level.num_playing_clients;
	state->num_playing_human_clients = level.num_playing_human_clients;
	std::copy(std::begin(level.sorted_clients), std::end(level.sorted_clients),
		state->sorted_clients.begin());
	state->follow1 = level.follow1;
	state->follow2 = level.follow2;
	state->num_living_red = level.num_living_red;
	state->num_eliminated_red = level.num_eliminated_red;
	state->num_living_blue = level.num_living_blue;
	state->num_eliminated_blue = level.num_eliminated_blue;
	state->num_living_free = level.num_living_free;
	state->num_playing_red = level.num_playing_red;
	state->num_playing_blue = level.num_playing_blue;

	std::copy(std::begin(level.team_scores), std::end(level.team_scores),
		state->team_scores.begin());
	std::copy(std::begin(level.team_old_scores), std::end(level.team_old_scores),
		state->team_old_scores.begin());
	state->match_state = level.match_state;
	state->warmup_requisite = level.warmup_requisite;
	state->warmup_notice_time = level.warmup_notice_time;
	state->warmup_gametype_hud_time = level.warmup_gametype_hud_time;
	state->match_time = level.match_time;
	state->match_start_time = level.match_start_time;
	state->match_state_queued = level.match_state_queued;
	state->match_state_timer = level.match_state_timer;
	state->warmup_modification_count = level.warmup_modification_count;
	state->countdown_check = level.countdown_check;
	state->matchendwarn_check = level.matchendwarn_check;
	state->match_cancel_delay_timer = level.match_cancel_delay_timer;

	state->round_number = level.round_number;
	state->round_epoch = level.round_epoch;
	state->world_epoch = level.world_epoch;
	state->round_state = level.round_state;
	state->round_state_queued = level.round_state_queued;
	state->round_state_timer = level.round_state_timer;
	state->restarted = level.restarted;
	state->overtime = level.overtime;
	state->suddendeath = level.suddendeath;
	state->tied_overtime_start = level.tied_overtime_start;
	std::copy(std::begin(level.count_living), std::end(level.count_living),
		state->count_living.begin());
	std::copy(std::begin(level.last_standing_count), std::end(level.last_standing_count),
		state->last_standing_count.begin());
	std::copy(std::begin(level.locked), std::end(level.locked), state->locked.begin());
	for (size_t i = 0; i < state->captain.size(); i++)
		state->captain[i] = ReloadClientEntity(level.captain[i]);

	state->ctf_last_flag_capture = level.ctf_last_flag_capture;
	state->ctf_last_capture_team = level.ctf_last_capture_team;
	std::copy(std::begin(level.ghosts), std::end(level.ghosts), state->ghosts.begin());
	for (ghost_t &ghost : state->ghosts)
		ghost.ent = ReloadClientEntity(ghost.ent);
	state->no_players_time = level.no_players_time;
	state->total_player_deaths = level.total_player_deaths;

	state->strike_red_attacks = level.strike_red_attacks;
	state->strike_flag_touch = level.strike_flag_touch;
	state->strike_turn = level.strike_turn;
	state->timeout_in_place = level.timeout_in_place;
	state->timeout_ent = ReloadClientEntity(level.timeout_ent);
	state->timeout_auto = level.timeout_auto;
	state->timeout_resuming = level.timeout_resuming;
	state->match_id = level.match_id;
	state->match = level.match;
	std::copy(std::begin(level.frag_warning), std::end(level.frag_warning),
		state->frag_warning.begin());
	state->prepare_to_fight = level.prepare_to_fight;
	std::copy(std::begin(level.intermission_victor_msg),
		std::end(level.intermission_victor_msg),
		state->intermission_victor_msg.begin());

	for (size_t i = 0; i < ReloadClientSlotCount(); i++)
		state->client_ghost_index[i] = ReloadGhostIndex(game.clients[i].resp.ghost);

	return state;
}

void RestoreWorldReloadState(world_reload_state_t &state)
{
	level.in_frame = state.in_frame;
	level.time = state.time;
	level.start_time = state.start_time;
	level.exit_time = state.exit_time;
	level.ready_to_exit = state.ready_to_exit;
	Q_strlcpy(level.mapname, state.mapname.data(), sizeof(level.mapname));
	Q_strlcpy(level.nextmap, state.nextmap.data(), sizeof(level.nextmap));
	Q_strlcpy(level.forcemap, state.forcemap.data(), sizeof(level.forcemap));
	if (!state.changemap.empty())
		level.changemap = G_CopyString(state.changemap.c_str(), TAG_LEVEL);
	level.entstring = std::move(state.entstring);

	level.intermission_time = state.intermission_time;
	level.intermission_queued = state.intermission_queued;
	level.intermission_exit = state.intermission_exit;
	level.intermission_eou = state.intermission_eou;
	level.intermission_clear = state.intermission_clear;
	level.intermission_fade = state.intermission_fade;
	level.intermission_fading = state.intermission_fading;
	level.intermission_fade_time = state.intermission_fade_time;
	level.respawn_intermission = state.respawn_intermission;
	level.intermission_server_frame = state.intermission_server_frame;

	level.entry = state.entry;
	level.current_entity = state.current_entity;
	level.disguise_violator = state.disguise_violator;
	level.disguise_violation_time = state.disguise_violation_time;
	level.next_auto_save = state.next_auto_save;
	level.next_match_report = state.next_match_report;

	level.vote_state = std::move(state.vote_state);
	level.num_connected_clients = state.num_connected_clients;
	level.num_nonspectator_clients = state.num_nonspectator_clients;
	level.num_playing_clients = state.num_playing_clients;
	level.num_playing_human_clients = state.num_playing_human_clients;
	std::copy(state.sorted_clients.begin(), state.sorted_clients.end(),
		std::begin(level.sorted_clients));
	level.follow1 = state.follow1;
	level.follow2 = state.follow2;
	level.num_living_red = state.num_living_red;
	level.num_eliminated_red = state.num_eliminated_red;
	level.num_living_blue = state.num_living_blue;
	level.num_eliminated_blue = state.num_eliminated_blue;
	level.num_living_free = state.num_living_free;
	level.num_playing_red = state.num_playing_red;
	level.num_playing_blue = state.num_playing_blue;

	std::copy(state.team_scores.begin(), state.team_scores.end(),
		std::begin(level.team_scores));
	std::copy(state.team_old_scores.begin(), state.team_old_scores.end(),
		std::begin(level.team_old_scores));
	level.match_state = state.match_state;
	level.warmup_requisite = state.warmup_requisite;
	level.warmup_notice_time = state.warmup_notice_time;
	level.warmup_gametype_hud_time = state.warmup_gametype_hud_time;
	level.match_time = state.match_time;
	level.match_start_time = state.match_start_time;
	level.match_state_queued = state.match_state_queued;
	level.match_state_timer = state.match_state_timer;
	level.warmup_modification_count = state.warmup_modification_count;
	level.countdown_check = state.countdown_check;
	level.matchendwarn_check = state.matchendwarn_check;
	level.match_cancel_delay_timer = state.match_cancel_delay_timer;

	level.round_number = state.round_number;
	level.round_epoch = state.round_epoch;
	level.world_epoch = state.world_epoch;
	level.round_state = state.round_state;
	level.round_state_queued = state.round_state_queued;
	level.round_state_timer = state.round_state_timer;
	level.restarted = state.restarted;
	level.overtime = state.overtime;
	level.suddendeath = state.suddendeath;
	level.tied_overtime_start = state.tied_overtime_start;
	std::copy(state.count_living.begin(), state.count_living.end(),
		std::begin(level.count_living));
	std::copy(state.last_standing_count.begin(), state.last_standing_count.end(),
		std::begin(level.last_standing_count));
	std::copy(state.locked.begin(), state.locked.end(), std::begin(level.locked));
	std::copy(state.captain.begin(), state.captain.end(), std::begin(level.captain));

	level.ctf_last_flag_capture = state.ctf_last_flag_capture;
	level.ctf_last_capture_team = state.ctf_last_capture_team;
	std::copy(state.ghosts.begin(), state.ghosts.end(), std::begin(level.ghosts));
	level.no_players_time = state.no_players_time;

	level.strike_red_attacks = state.strike_red_attacks;
	level.strike_flag_touch = state.strike_flag_touch;
	level.strike_turn = state.strike_turn;
	level.timeout_in_place = state.timeout_in_place;
	level.timeout_ent = state.timeout_ent;
	level.timeout_auto = state.timeout_auto;
	level.timeout_resuming = state.timeout_resuming;
	level.match_id = std::move(state.match_id);
	level.match = std::move(state.match);
	std::copy(state.frag_warning.begin(), state.frag_warning.end(),
		std::begin(level.frag_warning));
	level.prepare_to_fight = state.prepare_to_fight;
	std::copy(state.intermission_victor_msg.begin(),
		state.intermission_victor_msg.end(),
		std::begin(level.intermission_victor_msg));
}

void PrepareClientsForWorldReload(world_reload_state_t &state)
{
	const size_t client_slots = ReloadClientSlotCount();
	for (size_t i = 0; i < client_slots; i++) {
		gentity_t *ent = &g_entities[i + 1];
		gclient_t *client = &game.clients[i];
		ent->client = client;
		state.client_was_linked[i] = ent->linked;
		client->follow_queued_target = ReloadClientEntity(client->follow_queued_target);
		client->follow_target = ReloadClientEntity(client->follow_target);

		if (client->menu)
			P_Menu_Close(ent);

		// These helpers dereference their owned entities, so first prove that
		// each pointer still names its expected dynamic entity. Then release it
		// before any slot can be reused by a freshly parsed map entity.
		gentity_t *grapple = ReloadDynamicEntity(client->grapple_ent);
		if (grapple && grapple->inuse && grapple->owner == ent &&
			grapple->count == ent->spawn_count) {
			client->grapple_ent = grapple;
			Weapon_Grapple_DoReset(client);
		} else {
			client->grapple_ent = nullptr;
		}
		if (client->owned_sphere) {
			gentity_t *sphere = ReloadDynamicEntity(client->owned_sphere);
			if (sphere && sphere->inuse &&
				sphere->spawn_count == client->owned_sphere_generation &&
				sphere->owner == ent && sphere->count == ent->spawn_count &&
				sphere->classname && !strcmp(sphere->classname, "sphere"))
				G_FreeEntity(sphere);
			G_ClearOwnedSphere(client);
		}

		client->grapple_ent = nullptr;
		client->grapple_state = GRAPPLE_STATE_FLY;
		client->grapple_release_time = level.time + 1_sec;
		client->trail_head = nullptr;
		client->trail_tail = nullptr;
		client->landmark_name = nullptr;
		client->oldgroundentity = nullptr;
		client->sight_entity = nullptr;
		client->sound_entity = nullptr;
		client->sound2_entity = nullptr;
		client->tracker_pain_time = 0_ms;
		G_ClearLagCompensationHistory(ent);

		ent->owner = ReloadClientEntity(ent->owner);
		ent->flags &= ~FL_NO_KNOCKBACK;
		ent->target_ent = ReloadClientEntity(ent->target_ent);
		ent->goalentity = ReloadClientEntity(ent->goalentity);
		ent->movetarget = ReloadClientEntity(ent->movetarget);
		ent->chain = ReloadClientEntity(ent->chain);
		ent->enemy = ReloadClientEntity(ent->enemy);
		ent->oldenemy = ReloadClientEntity(ent->oldenemy);
		ent->activator = ReloadClientEntity(ent->activator);
		ent->groundentity = nullptr;
		ent->groundentity_linkcount = 0;
		ent->teamchain = ReloadClientEntity(ent->teamchain);
		ent->teammaster = ReloadClientEntity(ent->teammaster);
		ent->mynoise = nullptr;
		ent->mynoise2 = nullptr;
		ent->bad_area = nullptr;
		ent->hint_chain = nullptr;
		ent->monster_hint_chain = nullptr;
		ent->target_hint_chain = nullptr;
		ent->beam = nullptr;
		ent->beam2 = nullptr;
		ent->proboscus = nullptr;
		ent->disintegrator = nullptr;
		ent->disintegrator_time = 0_ms;
		ent->monsterinfo.damage_attacker = nullptr;
		ent->monsterinfo.damage_inflictor = nullptr;
		ent->monsterinfo.damage_blood = 0;
		ent->monsterinfo.damage_knockback = 0;
		ent->monsterinfo.damage_from = {};
		ent->monsterinfo.damage_mod = MOD_UNKNOWN;

		ent->model = ent->inuse ? "players/male/tris.md2" : nullptr;
		ent->message = nullptr;
		ent->target = nullptr;
		ent->targetname = nullptr;
		ent->killtarget = nullptr;
		ent->team = nullptr;
		ent->pathtarget = nullptr;
		ent->deathtarget = nullptr;
		ent->healthtarget = nullptr;
		ent->itemtarget = nullptr;
		ent->combattarget = nullptr;
		ent->map = nullptr;
		ent->item = nullptr;
		ent->style_on = nullptr;
		ent->style_off = nullptr;
		ent->gametype = nullptr;
		ent->not_gametype = nullptr;
		ent->notteam = nullptr;
		ent->notfree = nullptr;
		ent->notq2 = nullptr;
		ent->notq3a = nullptr;
		ent->notarena = nullptr;
		ent->ruleset = nullptr;
		ent->not_ruleset = nullptr;
		ent->powerups_on = nullptr;
		ent->powerups_off = nullptr;
		ent->bfg_on = nullptr;
		ent->bfg_off = nullptr;
		ent->plasmabeam_on = nullptr;
		ent->plasmabeam_off = nullptr;
		ent->sv.enemy = ReloadClientEntity(ent->sv.enemy);
		ent->sv.ground_entity = nullptr;
		ent->sv.classname = ent->classname;
		ent->sv.targetname = nullptr;

		if (ent->linked)
			gi.unlinkentity(ent);
	}
}

void ClearWorldEntitySlots()
{
	const size_t max_entities = static_cast<size_t>(game.maxentities);
	const size_t first_nonclient = min(
		static_cast<size_t>(game.maxclients) + 1, max_entities);

	for (size_t i = 0; i < max_entities; i++) {
		if (i > 0 && i < first_nonclient)
			continue;

		gentity_t *ent = &g_entities[i];
		const bool occupied = ent->inuse;
		if (ent->linked)
			gi.unlinkentity(ent);
		if (occupied &&
			i > static_cast<size_t>(game.maxclients) + BODY_QUEUE_SIZE)
			gi.Bot_UnRegisterEntity(ent);

		const int32_t generation = occupied
			? MM_NextEntityGeneration(ent->spawn_count)
			: ent->spawn_count;
		memset(ent, 0, sizeof(*ent)); // NOLINT(bugprone-undefined-memory-manipulation): engine-owned gentity_t slots are intentionally raw C ABI storage.
		ent->s.number = static_cast<int32_t>(i);
		ent->spawn_count = generation;
		ent->classname = "freed";
	}
}

} // namespace

/*
=============
G_ResetWorldEntitiesFromSavedString

Rebuilds map-owned entities from the exact effective lump captured at map load
while retaining client slots and match/session state.
=============
*/
world_entity_reload_result_t G_ResetWorldEntitiesFromSavedString()
{
	static bool reload_in_progress = false;
	if (reload_in_progress)
		return world_entity_reload_result_t::already_in_progress;
	if (!deathmatch->integer || !level.init)
		return world_entity_reload_result_t::fallback_allowed;
	// Arena has its own room lifecycle and a preflight contract that is consumed
	// during true map initialization. It deliberately remains on its established
	// reset path rather than rebuilding selectors mid-session.
	if (GT(GT_ARENA))
		return world_entity_reload_result_t::fallback_allowed;
	if (game.maxclients > MAX_CLIENTS ||
		static_cast<size_t>(game.maxclients) + BODY_QUEUE_SIZE + 1 >
			static_cast<size_t>(game.maxentities)) {
		gi.Com_Print("Entity reload skipped: invalid client/entity slot layout; using legacy reset.\n");
		return world_entity_reload_result_t::fallback_allowed;
	}

	const size_t reserved_slots =
		static_cast<size_t>(game.maxclients) + BODY_QUEUE_SIZE;
	const size_t definition_capacity =
		static_cast<size_t>(game.maxentities) > reserved_slots
			? static_cast<size_t>(game.maxentities) - reserved_slots
			: 0;
	const mm_entity_lump_validation_t validation =
		MM_ValidateEntityLump(level.entstring, definition_capacity);
	if (!validation.valid) {
		gi.Com_PrintFmt(
			"Entity reload skipped: cached entity string is invalid ({}); using legacy reset.\n",
			validation.error ? validation.error : "unknown error");
		return world_entity_reload_result_t::fallback_allowed;
	}
	if (!cached_entity_item_inhibit_modes_valid) {
		gi.Com_Print(
			"Entity reload skipped: map item-filter state is unavailable; using legacy reset.\n");
		return world_entity_reload_result_t::fallback_allowed;
	}
	if (!cached_world_spawn_profile_valid) {
		gi.Com_Print(
			"Entity reload skipped: map spawn profile is unavailable; using legacy reset.\n");
		return world_entity_reload_result_t::fallback_allowed;
	}
	if (cached_world_spawn_profile.random_items || g_dm_random_items->integer) {
		// Random substitution deliberately consumes fresh RNG and can change
		// whether an authored definition survives its final disable/replacement
		// checks. Keep this mode on the non-destructive established reset path
		// instead of claiming a reload has deterministic capacity.
		gi.Com_Print(
			"Entity reload skipped: random-item mode requires the legacy reset.\n");
		return world_entity_reload_result_t::fallback_allowed;
	}
	char invalid_start_item[MAX_TOKEN_CHARS];
	if (!ValidateStartItems(invalid_start_item, sizeof(invalid_start_item))) {
		gi.Com_PrintFmt(
			"Entity reload skipped: invalid g_start_item entry \"{}\"; using legacy reset.\n",
			invalid_start_item);
		return world_entity_reload_result_t::fallback_allowed;
	}

	// Parsing invokes helper-producing spawn functions. Only repeat the cached
	// lump when all mutable inputs that can alter that topology still match the
	// profile which successfully built the current map. This check happens
	// before snapshotting clients or tearing down a single live entity.
	const item_inhibit_modes_t pending_item_inhibit_modes =
		CaptureItemInhibitModes();
	world_spawn_profile_t current_spawn_profile;
	try {
		struct profile_probe_scope_t {
			item_inhibit_modes_t item_modes;
			ruleset_t ruleset;
			~profile_probe_scope_t() {
				ApplyItemInhibitModes(item_modes);
				game.ruleset = ruleset;
			}
		} restore {
			pending_item_inhibit_modes,
			game.ruleset
		};

		ApplyItemInhibitModes(cached_entity_item_inhibit_modes);
		// SP_worldspawn will resolve this same cached result when the authored
		// lump and configured g_ruleset are unchanged.
		game.ruleset = cached_world_spawn_profile.ruleset;
		current_spawn_profile = CaptureWorldSpawnProfile();
	} catch (const std::bad_alloc &) {
		gi.Com_Print(
			"Entity reload skipped: unable to inspect map spawn profile; using legacy reset.\n");
		return world_entity_reload_result_t::fallback_allowed;
	}
	if (!WorldSpawnProfilesMatch(
			cached_world_spawn_profile, current_spawn_profile)) {
		gi.Com_Print(
			"Entity reload skipped: map spawn rules changed since load; using legacy reset.\n");
		return world_entity_reload_result_t::fallback_allowed;
	}

	std::unique_ptr<world_reload_state_t> state;
	try {
		state = CaptureWorldReloadState();
	} catch (const std::bad_alloc &) {
		gi.Com_Print("Entity reload skipped: unable to snapshot live match state; using legacy reset.\n");
		return world_entity_reload_result_t::fallback_allowed;
	}
	reload_in_progress = true;
	struct reload_scope_t {
		bool &active;
		~reload_scope_t() { active = false; }
	} reload_scope { reload_in_progress };

	const auto saved_server_flags = globals.server_flags;
	globals.server_flags |= SERVER_FLAG_LOADING;

	Bot_ResetDebug();
	PrepareClientsForWorldReload(*state);
	ClearWorldEntitySlots();
	// [MuffMode] The lump is about to recreate every prop, so any queued rebuild is
	// now a duplicate. Drop the records before their TAG_LEVEL strings go away.
	MM_EntRespawn_ClearAll();
	// [MuffMode] The live-gib budget holds raw slot references; drop them before
	// the entity array is reused so it cannot free a recycled slot.
	MM_Gibs_ClearAll();
	// [MuffMode] The next-map pick and the post-match awards reel keep their
	// state module-side, so neither comes back cleared with level_locals_t.
	MM_MapPick_Reset();
	MM_Awards_Reset();
	gi.FreeTags(TAG_LEVEL);

	level.~level_locals_t();
	new (&level) level_locals_t {};
	RestoreWorldReloadState(*state);

	level.is_n64 = strncmp(level.mapname, "q64/", 4) == 0;
	level.coop_scale_players = 0;
	level.coop_health_scaling = clamp(g_coop_health_scaling->value, 0.f, 1.f);
	globals.num_entities = static_cast<uint32_t>(
		min(static_cast<size_t>(game.maxclients) + 1,
			static_cast<size_t>(game.maxentities)));

	// MyMap item filters are one-shot map-load inputs. Reapply the exact modes
	// that built this world, then restore any modifiers queued for a future map.
	ApplyItemInhibitModes(cached_entity_item_inhibit_modes);
	const world_spawn_stats_t stats = SpawnCachedWorldEntities(false);
	ApplyItemInhibitModes(pending_item_inhibit_modes);
	// trigger_deathcount is a spawn-time map trigger. Rebuild it against the
	// same zero-death baseline as a true map load, then put the live match
	// counter back once every map entity and delayed target has been recreated.
	level.total_player_deaths = state->total_player_deaths;

	// Worldspawn owns the map default, but an in-progress vote or server command
	// may have changed the live rotation target. Keep that runtime decision.
	Q_strlcpy(level.nextmap, state->nextmap.data(), sizeof(level.nextmap));
	Q_strlcpy(level.forcemap, state->forcemap.data(), sizeof(level.forcemap));

	for (size_t i = 0; i < ReloadClientSlotCount(); i++) {
		gentity_t *ent = &g_entities[i + 1];
		ent->client = &game.clients[i];
		ent->s.number = static_cast<int32_t>(i + 1);

		const int ghost_index = state->client_ghost_index[i];
		game.clients[i].resp.ghost =
			ghost_index >= 0 && ghost_index < static_cast<int>(std::size(level.ghosts))
				? &level.ghosts[ghost_index]
				: nullptr;

		if (state->client_was_linked[i] && ent->inuse)
			gi.linkentity(ent);
	}

	globals.server_flags = saved_server_flags;
	MuffModeLog("MATCH",
		"Entity reload complete: definitions=%d inhibited=%d horde_anchors=%d live_entities=%u",
		stats.entity_count, stats.inhibited, stats.horde_anchors_converted,
		static_cast<unsigned>(globals.num_entities));
	return world_entity_reload_result_t::reloaded;
}

//===================================================================

void GT_SetLongName(void) {
	MM_GTSetLongName();
}

/*QUAKED worldspawn (0 0 0) ?

Only used for the world.
"sky"				environment map name
"skyaxis"			vector axis for rotating sky
"skyrotate"			speed of rotation in degrees/second
"sounds"			music cd track number
"music"				specific music file to play, overrides "sounds"
"gravity"			800 is default gravity
"hub_map"			in campaigns, sets as hub map
"message"			sets long level name
"author"			sets level author name
"author2"			sets another level author name
"start_items"		give players these items on spawn
"no_grapple"		disables grappling hook
"no_dm_spawnpads"	disables spawn pads in deathmatch
"ruleset"			overrides gameplay ruleset (q2re/mm/q3a/q2reb/qc)
*/
void SP_worldspawn(gentity_t *ent) {
	Q_strlcpy(level.gamemod_name, G_Fmt("{} v{}", GAMEMOD_TITLE, GAMEMOD_VERSION).data(), sizeof(level.gamemod_name));

	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_BSP;
	ent->inuse = true; // since the world doesn't use G_Spawn()
	ent->s.modelindex = MODELINDEX_WORLD;
	ent->gravity = 1.0f;

	if (st.hub_map) {
		level.hub_map = true;

		// clear helps
		game.help1changed = game.help2changed = 0;
		*game.helpmessage1 = *game.helpmessage2 = '\0';

		for (auto ec : active_clients()) {
			ec->client->pers.game_help1changed = ec->client->pers.game_help2changed = 0;
			ec->client->resp.coop_respawn.game_help1changed = ec->client->resp.coop_respawn.game_help2changed = 0;
		}
	}

	if (st.achievement && st.achievement[0])
		level.achievement = st.achievement;

	//---------------

	// set configstrings for items
	SetItemNames();

	if (st.nextmap)
		Q_strlcpy(level.nextmap, st.nextmap, sizeof(level.nextmap));
	else
		level.nextmap[0] = '\0'; // Clear stale value when map has no worldspawn nextmap (e.g. vote)

	// make some data visible to the server

	if (ent->message && ent->message[0]) {
		gi.configstring(CS_NAME, ent->message);
		Q_strlcpy(level.level_name, ent->message, sizeof(level.level_name));
	} else
		Q_strlcpy(level.level_name, level.mapname, sizeof(level.level_name));

	if (st.author && st.author[0])
		Q_strlcpy(level.author, st.author, sizeof(level.author));
	if (st.author2 && st.author2[0])
		Q_strlcpy(level.author2, st.author2, sizeof(level.author2));

	if (st.ruleset && st.ruleset[0]) {
		game.ruleset = RS_IndexFromString(st.ruleset);
		gi.Com_PrintFmt("st={} game={}\n", st.ruleset, rs_long_name[(int)game.ruleset]);
		if (!game.ruleset)
			game.ruleset = static_cast<ruleset_t>(
				ClampPlayableRulesetIndex(g_ruleset->integer));
	} else
		if ((int)game.ruleset != g_ruleset->integer)
			game.ruleset = static_cast<ruleset_t>(
				ClampPlayableRulesetIndex(g_ruleset->integer));

	if (st.sky && st.sky[0])
		gi.configstring(CS_SKY, st.sky);
	else
		gi.configstring(CS_SKY, "unit1_");

	gi.configstring(CS_SKYROTATE, G_Fmt("{} {}", st.skyrotate, st.skyautorotate).data());

	gi.configstring(CS_SKYAXIS, G_Fmt("{}", st.skyaxis).data());

	if (st.music && st.music[0]) {
		gi.configstring(CS_CDTRACK, st.music);
	} else {
		gi.configstring(CS_CDTRACK, G_Fmt("{}", ent->sounds).data());
	}

	if (level.is_n64)
		gi.configstring(CS_CD_LOOP_COUNT, "0");
	else if (st.was_key_specified("loop_count"))
		gi.configstring(CS_CD_LOOP_COUNT, G_Fmt("{}", st.loop_count).data());
	else
		gi.configstring(CS_CD_LOOP_COUNT, "");

	if (st.instantitems > 0 || level.is_n64)
		level.instantitems = true;

	// [Paril-KEX]
	if (!deathmatch->integer)
		gi.configstring(CS_GAME_STYLE, G_Fmt("{}", (int32_t)game_style_t::GAME_STYLE_PVE).data());
	else if (Teams() && notGT(GT_RR))
		gi.configstring(CS_GAME_STYLE, G_Fmt("{}", (int32_t)game_style_t::GAME_STYLE_TDM).data());
	else
		gi.configstring(CS_GAME_STYLE, G_Fmt("{}", (int32_t)game_style_t::GAME_STYLE_FFA).data());

	// [Paril-KEX]
	if (st.goals) {
		level.goals = st.goals;
		game.help1changed++;
	}

	if (st.start_items)
		level.start_items = st.start_items;

	if (st.no_grapple)
		level.no_grapple = true;

	if (st.no_dm_spawnpads)
		level.no_dm_spawnpads = true;

	gi.configstring(CS_MAXCLIENTS, G_Fmt("{}", game.maxclients).data());

	if (level.is_n64 && !deathmatch->integer) {
		gi.configstring(CONFIG_N64_PHYSICS, "1");
		pm_config.n64_physics = true;
	}

	// statusbar prog
	MM_InitStatusbar();
	gi.configstring(CONFIG_SPECTATOR_MODE_FREE, "SPECTATOR");
	gi.configstring(CONFIG_SPECTATOR_MODE_FOLLOW_FIRST, "SPECTATOR");
	gi.configstring(CONFIG_SPECTATOR_MODE_FOLLOW_THIRD, "SPECTATOR");

	// [Paril-KEX] air accel handled by game DLL now, and allow
	// it to be changed in sp/coop
	gi.configstring(CS_AIRACCEL, G_Fmt("{}", g_airaccelerate->integer).data());
	pm_config.airaccel = g_airaccelerate->integer;

	game.airacceleration_modified = g_airaccelerate->modified_count;

	//---------------

	if (!st.gravity) {
		level.gravity = 800.f;
		gi.cvar_set("g_gravity", "800");
	} else {
		if (!ED_TryLoadFloat(st.gravity, level.gravity)) {
			level.gravity = 800.f;
			st.gravity = "800";
		}
		gi.cvar_set("g_gravity", st.gravity);
	}

	snd_fry.assign("player/fry.wav"); // standing in lava / slime

	PrecacheItem(GetItemByIndex(IT_COMPASS));

	if (ShouldPrecacheBlaster())
		PrecacheItem(GetItemByIndex(IT_WEAPON_BLASTER));

	// Horde can start players on the chainfist; precache it so the vwep model/sounds exist.
	if (ShouldPrecacheHordeChainfist())
		PrecacheItem(GetItemByIndex(IT_WEAPON_CHAINFIST));

	// [MuffMode] Arena rooms can enable the grapple independently of the global cvar.
	if (ShouldPrecacheGrapple()) {
		PrecacheItem(GetItemByIndex(IT_WEAPON_GRAPPLE));
	}

	if (g_dm_random_items->integer) {
		for (item_id_t i = static_cast<item_id_t>(IT_NULL + 1); i < IT_TOTAL; i = static_cast<item_id_t>(i + 1))
			PrecacheItem(GetItemByIndex(i));
	}

	PrecachePlayerSounds();

	// sexed models
	for (auto &item : itemlist)
		item.vwep_index = 0;

	for (auto &item : itemlist) {
		if (!item.vwep_model)
			continue;

		for (auto &check : itemlist) {
			if (check.vwep_model && !Q_strcasecmp(item.vwep_model, check.vwep_model) && check.vwep_index) {
				item.vwep_index = check.vwep_index;
				break;
			}
		}

		if (item.vwep_index)
			continue;

		item.vwep_index = gi.modelindex(item.vwep_model);

		if (!level.vwep_offset)
			level.vwep_offset = item.vwep_index;
	}

	PrecacheAssets();

	MM_Horde_Init();
	MM_Strike_Init();

	GT_SetLongName();

	//
	// Setup light animation tables. 'a' is total darkness, 'z' is doublebright.
	//

	// 0 normal
	gi.configstring(CS_LIGHTS + 0, "m");

	// 1 FLICKER (first variety)
	gi.configstring(CS_LIGHTS + 1, "mmnmmommommnonmmonqnmmo");

	// 2 SLOW STRONG PULSE
	gi.configstring(CS_LIGHTS + 2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");

	// 3 CANDLE (first variety)
	gi.configstring(CS_LIGHTS + 3, "mmmmmaaaaammmmmaaaaaabcdefgabcdefg");

	// 4 FAST STROBE
	gi.configstring(CS_LIGHTS + 4, "mamamamamama");

	// 5 GENTLE PULSE 1
	gi.configstring(CS_LIGHTS + 5, "jklmnopqrstuvwxyzyxwvutsrqponmlkj");

	// 6 FLICKER (second variety)
	gi.configstring(CS_LIGHTS + 6, "nmonqnmomnmomomno");

	// 7 CANDLE (second variety)`map
	gi.configstring(CS_LIGHTS + 7, "mmmaaaabcdefgmmmmaaaammmaamm");

	// 8 CANDLE (third variety)
	gi.configstring(CS_LIGHTS + 8, "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa");

	// 9 SLOW STROBE (fourth variety)
	gi.configstring(CS_LIGHTS + 9, "aaaaaaaazzzzzzzz");

	// 10 FLUORESCENT FLICKER
	gi.configstring(CS_LIGHTS + 10, "mmamammmmammamamaaamammma");

	// 11 SLOW PULSE NOT FADE TO BLACK
	gi.configstring(CS_LIGHTS + 11, "abcdefghijklmnopqrrqponmlkjihgfedcba");

	// [Paril-KEX] 12 N64's 2 (fast strobe)
	gi.configstring(CS_LIGHTS + 12, "zzazazzzzazzazazaaazazzza");

	// [Paril-KEX] 13 N64's 3 (half of strong pulse)
	gi.configstring(CS_LIGHTS + 13, "abcdefghijklmnopqrstuvwxyz");

	// [Paril-KEX] 14 N64's 4 (fast strobe)
	gi.configstring(CS_LIGHTS + 14, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");

	// styles 32-62 are assigned by the light program for switchable lights

	// 63 testing
	gi.configstring(CS_LIGHTS + 63, "a");

	// coop respawn strings
	if (InCoopStyle()) {
		gi.configstring(CONFIG_COOP_RESPAWN_STRING + 0, "$g_coop_respawn_in_combat");
		gi.configstring(CONFIG_COOP_RESPAWN_STRING + 1, "$g_coop_respawn_bad_area");
		gi.configstring(CONFIG_COOP_RESPAWN_STRING + 2, "$g_coop_respawn_blocked");
		gi.configstring(CONFIG_COOP_RESPAWN_STRING + 3, "$g_coop_respawn_waiting");
		gi.configstring(CONFIG_COOP_RESPAWN_STRING + 4, "$g_coop_respawn_no_lives");
	}
}
