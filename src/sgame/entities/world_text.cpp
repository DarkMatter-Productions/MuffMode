// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "world_text.h"

#include <algorithm>
#include <string>

namespace {

constexpr spawnflags_t SPAWNFLAG_WORLD_TEXT_START_OFF = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAG_WORLD_TEXT_TRIGGER_ONCE = 2_spawnflag;
constexpr spawnflags_t SPAWNFLAG_WORLD_TEXT_REMOVE_ON_TRIGGER = 4_spawnflag;
constexpr spawnflags_t SPAWNFLAG_WORLD_TEXT_LEADER_BOARD = 8_spawnflag;

rgba_t WorldTextColor(gentity_t *self)
{
	switch (self->sounds) {
	case 0:
		return rgba_white;
	case 1:
		return rgba_red;
	case 2:
		return rgba_blue;
	case 3:
		return rgba_green;
	case 4:
		return rgba_yellow;
	case 5:
		return rgba_black;
	case 6:
		return rgba_cyan;
	case 7:
		return rgba_orange;
	default:
		gi.Com_PrintFmt("{}: invalid color\n", *self);
		return rgba_white;
	}
}

gentity_t *CurrentLeaderEntity()
{
	if (!g_entities)
		return nullptr;

	const int leader_index = level.sorted_clients[0];
	if (leader_index < 0 || static_cast<size_t>(leader_index) >= static_cast<size_t>(game.maxclients))
		return nullptr;

	const size_t entity_count = std::min(
		static_cast<size_t>(globals.num_entities),
		static_cast<size_t>(game.maxentities));
	const size_t leader_entity_index = static_cast<size_t>(leader_index) + 1;

	if (leader_entity_index >= entity_count)
		return nullptr;

	return &g_entities[leader_entity_index];
}

const char *ResolveWorldTextMessage(gentity_t *self, std::string &scratch)
{
	if (!deathmatch->integer || !self->spawnflags.has(SPAWNFLAG_WORLD_TEXT_LEADER_BOARD))
		return self->message;

	if (level.match_state == matchst_t::MATCH_WARMUP_READYUP)
		return "Welcome to Muff Mode\nKindly ready the fuck up...";

	if (level.match_state <= matchst_t::MATCH_WARMUP_DEFAULT)
		return "Welcome to Muff Mode";

	gentity_t *leader = CurrentLeaderEntity();
	if (!leader || !leader->client || level.total_player_deaths <= 0 || leader->client->resp.score <= 0)
		return self->message;

	const std::string_view message = G_Fmt("{} is in the lead\nwith a score of {}",
		leader->client->resp.netname, leader->client->resp.score);
	scratch.assign(message.data(), message.size());
	return scratch.c_str();
}

void DrawWorldText(gentity_t *self, const char *text, const rgba_t &color)
{
	if (self->s.angles[YAW] == -3.0f) {
		gi.Draw_OrientedWorldText(self->s.origin, text, color, self->size[2], FRAME_TIME_MS.seconds(), true);
		return;
	}

	vec3_t text_angle = { 0.0f, 0.0f, 0.0f };
	text_angle[YAW] = anglemod(self->s.angles[YAW]) + 180;
	if (text_angle[YAW] > 360.0f)
		text_angle[YAW] -= 360.0f;

	gi.Draw_StaticWorldText(self->s.origin, text_angle, text, color, self->size[2], FRAME_TIME_MS.seconds(), true);
}

static USE(info_world_text_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void
{
	if (self->activator == nullptr) {
		self->activator = activator;
		self->think(self);
	} else {
		self->nextthink = 0_ms;
		self->activator = nullptr;
	}

	if (self->spawnflags.has(SPAWNFLAG_WORLD_TEXT_TRIGGER_ONCE))
		self->use = nullptr;

	if (self->target != nullptr) {
		gentity_t *target = G_PickTarget(self->target);
		if (target != nullptr && target->inuse) {
			if (target->use)
				target->use(target, self, self);
		}
	}

	if (self->spawnflags.has(SPAWNFLAG_WORLD_TEXT_REMOVE_ON_TRIGGER))
		G_FreeEntity(self);
}

static THINK(info_world_text_think) (gentity_t *self) -> void
{
	const rgba_t color = WorldTextColor(self);
	std::string scratch;
	const char *text = ResolveWorldTextMessage(self, scratch);
	const char *draw_text = (text && text[0]) ? text : self->message;

	if (!draw_text)
		draw_text = "";

	DrawWorldText(self, draw_text, color);
	self->nextthink = level.time + FRAME_TIME_MS;
}

} // namespace

/*QUAKED info_world_text (1.0 1.0 0.0) (-16 -16 0) (16 16 32) START_OFF TRIGGER_ONCE REMOVE_ON_TRIGGER LEADER x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Designer placed in world text for debugging.
*/
void SP_info_world_text(gentity_t *self)
{
	if (self->message == nullptr && !self->spawnflags.has(SPAWNFLAG_WORLD_TEXT_LEADER_BOARD)) {
		gi.Com_PrintFmt("{}: no message\n", *self);
		G_FreeEntity(self);
		return;
	}

	self->think = info_world_text_think;
	self->use = info_world_text_use;
	self->size[2] = st.radius ? st.radius : 0.2f;

	if (!self->spawnflags.has(SPAWNFLAG_WORLD_TEXT_START_OFF)) {
		self->nextthink = level.time + FRAME_TIME_MS;
		self->activator = self;
	}
}
