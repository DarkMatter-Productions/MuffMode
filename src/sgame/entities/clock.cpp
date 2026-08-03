// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "clock.h"
#include "muffmode/mm_util.h"

#include <ctime>
#include <string_view>

namespace {

constexpr spawnflags_t SPAWNFLAG_TIMER_UP = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAG_TIMER_DOWN = 2_spawnflag;
constexpr spawnflags_t SPAWNFLAG_TIMER_START_OFF = 4_spawnflag;
constexpr spawnflags_t SPAWNFLAG_TIMER_MULTI_USE = 8_spawnflag;

int DigitFrameForChar(char value)
{
	if (value >= '0' && value <= '9')
		return value - '0';
	if (value == '-')
		return 10;
	if (value == ':')
		return 11;
	return 12;
}

void func_clock_reset(gentity_t *self)
{
	self->activator = nullptr;

	if (self->spawnflags.has(SPAWNFLAG_TIMER_UP)) {
		self->health = 0;
		self->wait = static_cast<float>(self->count);
	} else if (self->spawnflags.has(SPAWNFLAG_TIMER_DOWN)) {
		self->health = self->count;
		self->wait = 0;
	}
}

void func_clock_format_countdown(gentity_t *self)
{
	switch (self->style) {
	case 0:
		G_FmtTo(self->clock_message, "{:2}", self->health);
		break;
	case 1:
		G_FmtTo(self->clock_message, "{:2}:{:02}", self->health / 60, self->health % 60);
		break;
	case 2:
		G_FmtTo(self->clock_message, "{:2}:{:02}:{:02}", self->health / 3600,
			(self->health - (self->health / 3600) * 3600) / 60, self->health % 60);
		break;
	default:
		self->clock_message[0] = '\0';
		break;
	}
}

void func_clock_format_time_of_day(gentity_t *self)
{
	const std::time_t now = std::time(nullptr);
	std::tm local_time = {};
	if (!muffmode::LocalTimeSnapshot(now, local_time)) {
		self->clock_message[0] = '\0';
		return;
	}

	G_FmtTo(self->clock_message, "{:2}:{:02}:{:02}", local_time.tm_hour, local_time.tm_min, local_time.tm_sec);
}

bool func_clock_elapsed(const gentity_t *self)
{
	return (self->spawnflags.has(SPAWNFLAG_TIMER_UP) && self->health > self->wait) ||
		(self->spawnflags.has(SPAWNFLAG_TIMER_DOWN) && self->health < self->wait);
}

[[nodiscard]] bool func_clock_fire_pathtarget(gentity_t *self)
{
	if (!self->pathtarget)
		return true;

	const char *saved_target = self->target;
	self->target = self->pathtarget;
	if (!G_UseTargets(self, self->activator))
		return false;
	self->target = saved_target;
	return true;
}

} // namespace

/*QUAKED target_character (0 0 1) ? x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
used with target_string (must be on same "team")
"count" is position in the string (starts at 1)
*/
void SP_target_character(gentity_t *self)
{
	self->movetype = MOVETYPE_PUSH;
	gi.setmodel(self, self->model);
	self->solid = SOLID_BSP;
	self->s.frame = 12;
	gi.linkentity(self);
}

/*QUAKED target_string (0 0 1) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
 */
static USE(target_string_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void
{
	const std::string_view message{ self->message ? self->message : "" };

	for (gentity_t *entity = self->teammaster; entity; entity = entity->teamchain) {
		if (!entity->count)
			continue;

		const auto char_index = static_cast<size_t>(entity->count - 1);
		entity->s.frame = char_index < message.size() ? DigitFrameForChar(message[char_index]) : 12;
	}
}

void SP_target_string(gentity_t *self)
{
	if (!self->message)
		self->message = "";
	self->use = target_string_use;
}

/*QUAKED func_clock (0 0 1) (-8 -8 -8) (8 8 8) TIMER_UP TIMER_DOWN START_OFF MULTI_USE x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
target a target_string with this

The default is to be a time of day clock

TIMER_UP and TIMER_DOWN run for "count" seconds and then fire "pathtarget"
If START_OFF, this entity must be used before it starts

"style"		0 "xx"
			1 "xx:xx"
			2 "xx:xx:xx"
*/
static THINK(func_clock_think) (gentity_t *self) -> void
{
	// Re-resolve the display every tick. A saved raw pointer can otherwise name
	// a recycled map-entity slot long after the original target was removed.
	gentity_t *const display =
		G_FindByString<&gentity_t::targetname>(nullptr, self->target);
	if (!display || !display->use)
		return;
	self->enemy = display;

	if (self->spawnflags.has(SPAWNFLAG_TIMER_UP)) {
		func_clock_format_countdown(self);
		self->health++;
	} else if (self->spawnflags.has(SPAWNFLAG_TIMER_DOWN)) {
		func_clock_format_countdown(self);
		self->health--;
	} else {
		func_clock_format_time_of_day(self);
	}

	const int32_t self_generation = self->spawn_count;
	display->message = self->clock_message;
	display->use(display, self, self);
	if (!self->inuse || self->spawn_count != self_generation)
		return;

	if (func_clock_elapsed(self)) {
		if (!func_clock_fire_pathtarget(self))
			return;

		if (!self->spawnflags.has(SPAWNFLAG_TIMER_MULTI_USE))
			return;

		func_clock_reset(self);

		if (self->spawnflags.has(SPAWNFLAG_TIMER_START_OFF))
			return;
	}

	self->nextthink = level.time + 1_sec;
}

USE(func_clock_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void
{
	if (!self->spawnflags.has(SPAWNFLAG_TIMER_MULTI_USE))
		self->use = nullptr;
	if (self->activator)
		return;
	self->activator = activator;
	self->think(self);
}

void SP_func_clock(gentity_t *self)
{
	if (!self->target) {
		gi.Com_PrintFmt("{} with no target\n", *self);
		G_FreeEntity(self);
		return;
	}

	if (self->spawnflags.has(SPAWNFLAG_TIMER_DOWN) && !self->count) {
		gi.Com_PrintFmt("{} with no count\n", *self);
		G_FreeEntity(self);
		return;
	}

	if (self->spawnflags.has(SPAWNFLAG_TIMER_UP) && !self->count)
		self->count = 60 * 60;

	func_clock_reset(self);

	self->think = func_clock_think;

	if (self->spawnflags.has(SPAWNFLAG_TIMER_START_OFF))
		self->use = func_clock_use;
	else
		self->nextthink = level.time + 1_sec;
}
