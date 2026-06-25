// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "shadow_lights.h"

#include "muffmode/mm_parse.h"

#include <array>

/*QUAKED light (0 1 0) (-8 -8 -8) (8 8 8) START_OFF ALLOW_IN_DM x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Non-displayed light.
Default light value is 300.
Default style is 0.
If targeted, will toggle between on and off.
Default _cone value is 10 (used to set size of light for spotlights)
*/

namespace {

constexpr spawnflags_t SPAWNFLAG_LIGHT_START_OFF = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAG_LIGHT_ALLOW_IN_DM = 2_spawnflag;

struct ShadowLightInfo {
	int entity_number = 0;
	shadow_light_data_t shadowlight = {};
};

std::array<ShadowLightInfo, MAX_SHADOW_LIGHTS> shadow_lights;

bool LightStyleIndexIsValid(int32_t style)
{
	return style >= 0 && style < static_cast<int32_t>(MAX_LIGHTSTYLES);
}

size_t ShadowLightCount()
{
	if (level.shadow_light_count <= 0)
		return 0;

	return min(static_cast<size_t>(level.shadow_light_count), shadow_lights.size());
}

bool ParseWholeIntToken(const char *token, int &out)
{
	const auto parsed = MM_ParseIntArg(token);
	if (!parsed)
		return false;

	out = *parsed;
	return true;
}

bool ParseWholeFloatToken(const char *token, float &out)
{
	const auto parsed = MM_ParseFloatArg(token);
	if (!parsed)
		return false;

	out = *parsed;
	return true;
}

bool ShadowLightEntityIndexIsValid(int entity_number)
{
	return entity_number > 0 && static_cast<uint32_t>(entity_number) < globals.num_entities;
}

const char *LightStyleStringOrDefault(const char *value, const char *default_value)
{
	if (value == nullptr || value[0] == '\0')
		return default_value;

	if (value[0] < '0' || value[0] > '9')
		return value;

	int style_index = 0;
	if (!ParseWholeIntToken(value, style_index) || !LightStyleIndexIsValid(style_index))
		return default_value;

	return gi.get_configstring(CS_LIGHTS + style_index);
}

void RegisterShadowLight(gentity_t *self)
{
	if (st.sl.data.radius <= 0)
		return;

	if (level.shadow_light_count < 0 || level.shadow_light_count >= static_cast<int32_t>(shadow_lights.size())) {
		gi.Com_PrintFmt("{}: too many shadow lights; ignoring {}\n", __FUNCTION__, *self);
		return;
	}

	self->s.renderfx = RF_CASTSHADOW;
	self->itemtarget = st.sl.lightstyletarget;

	auto &slot = shadow_lights[static_cast<size_t>(level.shadow_light_count)];
	slot.entity_number = self->s.number;
	slot.shadowlight = st.sl.data;
	level.shadow_light_count++;

	self->mins = {};
	self->maxs = {};

	gi.linkentity(self);
}

USE(light_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void
{
	if (!LightStyleIndexIsValid(self->style))
		return;

	if (self->spawnflags.has(SPAWNFLAG_LIGHT_START_OFF)) {
		gi.configstring(CS_LIGHTS + self->style, self->style_on);
		self->spawnflags &= ~SPAWNFLAG_LIGHT_START_OFF;
	} else {
		gi.configstring(CS_LIGHTS + self->style, self->style_off);
		self->spawnflags |= SPAWNFLAG_LIGHT_START_OFF;
	}
}

USE(dynamic_light_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void
{
	self->svflags ^= SVF_NOCLIENT;
}

bool ParseShadowLightConfig(const char *config, ShadowLightInfo &loaded)
{
	const char *cursor = config;
	int light_type = 0;

	if (!ParseWholeIntToken(COM_ParseEx(&cursor, ";"), loaded.entity_number) ||
		!ShadowLightEntityIndexIsValid(loaded.entity_number) ||
		!ParseWholeIntToken(COM_ParseEx(&cursor, ";"), light_type) ||
		!ParseWholeFloatToken(COM_ParseEx(&cursor, ";"), loaded.shadowlight.radius) ||
		!ParseWholeIntToken(COM_ParseEx(&cursor, ";"), loaded.shadowlight.resolution) ||
		!ParseWholeFloatToken(COM_ParseEx(&cursor, ";"), loaded.shadowlight.intensity) ||
		!ParseWholeFloatToken(COM_ParseEx(&cursor, ";"), loaded.shadowlight.fade_start) ||
		!ParseWholeFloatToken(COM_ParseEx(&cursor, ";"), loaded.shadowlight.fade_end) ||
		!ParseWholeIntToken(COM_ParseEx(&cursor, ";"), loaded.shadowlight.lightstyle) ||
		!ParseWholeFloatToken(COM_ParseEx(&cursor, ";"), loaded.shadowlight.coneangle) ||
		!ParseWholeFloatToken(COM_ParseEx(&cursor, ";"), loaded.shadowlight.conedirection[0]) ||
		!ParseWholeFloatToken(COM_ParseEx(&cursor, ";"), loaded.shadowlight.conedirection[1]) ||
		!ParseWholeFloatToken(COM_ParseEx(&cursor, ";"), loaded.shadowlight.conedirection[2])) {
		return false;
	}

	loaded.shadowlight.lighttype = light_type == static_cast<int>(shadow_light_type_t::cone) ?
		shadow_light_type_t::cone :
		shadow_light_type_t::point;
	return true;
}

} // namespace

const shadow_light_data_t *GetShadowLightData(int32_t entity_number)
{
	for (size_t i = 0; i < ShadowLightCount(); i++) {
		if (shadow_lights[i].entity_number == entity_number)
			return &shadow_lights[i].shadowlight;
	}

	return nullptr;
}

void setup_shadow_lights()
{
	for (size_t i = 0; i < ShadowLightCount(); ++i) {
		auto &info = shadow_lights[i];
		if (!ShadowLightEntityIndexIsValid(info.entity_number)) {
			gi.configstring(CS_SHADOWLIGHTS + i, "");
			continue;
		}

		gentity_t *self = &g_entities[info.entity_number];

		info.shadowlight.lighttype = shadow_light_type_t::point;
		info.shadowlight.conedirection = {};

		if (self->target) {
			gentity_t *target = G_FindByString<&gentity_t::targetname>(nullptr, self->target);
			if (target) {
				info.shadowlight.conedirection = (target->s.origin - self->s.origin).normalized();
				info.shadowlight.lighttype = shadow_light_type_t::cone;
			}
		}

		if (self->itemtarget) {
			gentity_t *target = G_FindByString<&gentity_t::targetname>(nullptr, self->itemtarget);
			if (target)
				info.shadowlight.lightstyle = target->style;
		}

		gi.configstring(CS_SHADOWLIGHTS + i, G_Fmt("{};{};{:1};{};{:1};{:1};{:1};{};{:1};{:1};{:1};{:1}",
			self->s.number,
			static_cast<int>(info.shadowlight.lighttype),
			info.shadowlight.radius,
			info.shadowlight.resolution,
			info.shadowlight.intensity,
			info.shadowlight.fade_start,
			info.shadowlight.fade_end,
			info.shadowlight.lightstyle,
			info.shadowlight.coneangle,
			info.shadowlight.conedirection[0],
			info.shadowlight.conedirection[1],
			info.shadowlight.conedirection[2]).data());
	}
}

// Restore shadow-light metadata from configstrings after loading a saved level.
void G_LoadShadowLights()
{
	for (size_t i = 0; i < ShadowLightCount(); i++) {
		ShadowLightInfo loaded{};
		if (!ParseShadowLightConfig(gi.get_configstring(CS_SHADOWLIGHTS + i), loaded)) {
			shadow_lights[i] = {};
			continue;
		}

		shadow_lights[i] = loaded;
	}
}

void SP_dynamic_light(gentity_t *self)
{
	RegisterShadowLight(self);

	if (self->targetname)
		self->use = dynamic_light_use;

	if (self->spawnflags.has(SPAWNFLAG_LIGHT_START_OFF))
		self->svflags ^= SVF_NOCLIENT;
}

void SP_light(gentity_t *self)
{
	// No targeted lights in deathmatch, because they cause global messages.
	if ((!self->targetname || (deathmatch->integer && !self->spawnflags.has(SPAWNFLAG_LIGHT_ALLOW_IN_DM))) &&
		st.sl.data.radius == 0) {
		G_FreeEntity(self);
		return;
	}

	if (self->style >= 32 && LightStyleIndexIsValid(self->style)) {
		self->use = light_use;

		self->style_on = LightStyleStringOrDefault(self->style_on, "m");
		self->style_off = LightStyleStringOrDefault(self->style_off, "a");

		if (self->spawnflags.has(SPAWNFLAG_LIGHT_START_OFF))
			gi.configstring(CS_LIGHTS + self->style, self->style_off);
		else
			gi.configstring(CS_LIGHTS + self->style, self->style_on);
	} else if (self->style >= static_cast<int32_t>(MAX_LIGHTSTYLES)) {
		gi.Com_PrintFmt("{}: invalid lightstyle {}; light will not toggle\n", *self, self->style);
	}

	RegisterShadowLight(self);
}
