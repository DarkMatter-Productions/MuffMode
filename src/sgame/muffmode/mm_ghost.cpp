// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_ghost.h"
#include "muffmode/mm_match.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_util.h"

#include <algorithm>
#include <array>
#include <string>

extern cvar_t *g_auto_ghost_max;
extern cvar_t *g_auto_ghost_time;
extern cvar_t *g_auto_ghost_timeout;

namespace muffmode::ghost {
constexpr int GHOST_CODE_MIN = 10000;
constexpr int GHOST_CODE_MAX_EXCLUSIVE = 100000;
constexpr int GHOST_CODE_MAX = GHOST_CODE_MAX_EXCLUSIVE - 1;
constexpr int AUTO_GHOST_DEFAULT_REJOIN_SECONDS = 120;
constexpr int AUTO_GHOST_MAX_REJOIN_SECONDS = 3600;
constexpr int AUTO_GHOST_DEFAULT_MAX_RESERVATIONS = 3;
constexpr float AUTO_GHOST_PLACEHOLDER_ALPHA = 0.25f;
constexpr gtime_t AUTO_GHOST_FADE_OUT_TIME = 700_ms;
constexpr gtime_t AUTO_GHOST_FADE_IN_TIME = 500_ms;
constexpr gtime_t AUTO_GHOST_REINSTATE_DELAY = 3_sec;
constexpr gtime_t AUTO_GHOST_EXPIRE_PULSE_TIME = 700_ms;
} // namespace muffmode::ghost

namespace muffmode::ghost::snapshot {

enum class AutoGhostPhase {
	Reserved,
	Reinstating,
	Expiring
};

struct EntityLifeSnapshot {
	entity_state_t s{};
	svflags_t svflags = SVF_NONE;
	vec3_t mins{};
	vec3_t maxs{};
	vec3_t velocity{};
	vec3_t avelocity{};
	vec3_t gravityVector{ 0.0f, 0.0f, -1.0f };
	solid_t solid = SOLID_NOT;
	contents_t clipmask = CONTENTS_NONE;
	movetype_t movetype = MOVETYPE_NONE;
	ent_flags_t flags = FL_NONE;
	gtime_t timestamp;
	gtime_t air_finished;
	gtime_t powerarmor_time;
	gtime_t teleport_time;
	water_level_t waterlevel = WATER_NONE;
	contents_t watertype = CONTENTS_NONE;
	int32_t mass = 0;
	float gravity = 1.0f;
	int32_t health = 0;
	int32_t max_health = 0;
	int32_t gib_health = 0;
	int32_t viewheight = 0;
	int32_t dmg = 0;
	bool deadflag = false;
	bool takedamage = false;
	int32_t groundentity_number = -1;
	int32_t groundentity_spawn_count = 0;
	int32_t groundentity_linkcount = 0;
};

struct AutoGhostSnapshot {
	bool valid = false;
	char social_id[MAX_INFO_VALUE]{};
	std::string match_id;
	gtime_t remaining;
	gclient_t client{};
	EntityLifeSnapshot entity{};
	AutoGhostPhase phase = AutoGhostPhase::Reserved;
	gtime_t phase_elapsed = 0_ms;
	gtime_t reinstate_remaining = 0_ms;
	int32_t pending_entnum = -1;
	int32_t pending_spawn_count = 0;
	bool pending_manual = false;
	bool inventory_released = false;
};

std::array<AutoGhostSnapshot, MAX_CLIENTS> auto_ghosts;

void ClearTransientClientReferences(gclient_t &client)
{
	client.resp.ghost = nullptr;
	client.oldgroundentity = nullptr;
	client.follow_queued_target = nullptr;
	client.follow_queued_time = 0_ms;
	client.follow_target = nullptr;
	client.follow_update = false;
	client.owned_sphere = nullptr;
	client.tracker_pain_time = 0_ms;
	client.inmenu = false;
	client.menu = nullptr;
	client.menudirty = false;
	client.grapple_ent = nullptr;
	client.grapple_state = GRAPPLE_STATE_FLY;
	client.trail_head = nullptr;
	client.trail_tail = nullptr;
	client.landmark_free_fall = false;
	client.landmark_name = nullptr;
	client.landmark_rel_pos = {};
	client.sight_entity = nullptr;
	client.sound_entity = nullptr;
	client.sound2_entity = nullptr;
	client.num_lag_origins = 0;
	client.next_lag_origin = 0;
	client.is_lag_compensated = false;
	client.lag_restore = {};
}

size_t GhostSlotCapacity()
{
	const size_t configured = game.maxclients ? static_cast<size_t>(game.maxclients) : MAX_CLIENTS_KEX;
	return std::min({ configured, static_cast<size_t>(MAX_CLIENTS_KEX), static_cast<size_t>(MAX_CLIENTS) });
}

int AutoGhostDurationSeconds()
{
	if (!g_auto_ghost_time)
		return muffmode::ghost::AUTO_GHOST_DEFAULT_REJOIN_SECONDS;

	return clamp(g_auto_ghost_time->integer, 0, muffmode::ghost::AUTO_GHOST_MAX_REJOIN_SECONDS);
}

int AutoGhostMaxReservations()
{
	const int capacity = static_cast<int>(GhostSlotCapacity());
	if (!g_auto_ghost_max)
		return std::min(muffmode::ghost::AUTO_GHOST_DEFAULT_MAX_RESERVATIONS, capacity);

	return clamp(g_auto_ghost_max->integer, 0, capacity);
}

bool IsSlotIgnored(gentity_t *slot, gentity_t **ignore, size_t num_ignore)
{
	for (size_t i = 0; i < num_ignore; i++)
		if (slot == ignore[i])
			return true;

	return false;
}

int GhostIndex(const ghost_t *ghost)
{
	if (!ghost)
		return -1;

	const ptrdiff_t index = ghost - level.ghosts;
	if (index < 0 || index >= static_cast<ptrdiff_t>(MAX_CLIENTS))
		return -1;

	return static_cast<int>(index);
}

bool HasUsableSocialId(const char *social_id)
{
	return social_id && social_id[0];
}

bool SnapshotMatchesCurrentMatch(const AutoGhostSnapshot &snapshot)
{
	return snapshot.valid &&
		(snapshot.remaining > 0_ms ||
			snapshot.phase == AutoGhostPhase::Reinstating ||
			snapshot.phase == AutoGhostPhase::Expiring) &&
		!snapshot.match_id.empty() &&
		snapshot.match_id == level.match_id &&
		level.match_state == matchst_t::MATCH_IN_PROGRESS &&
		!level.intermission_time &&
		!level.intermission_queued;
}

int ActiveSnapshotCount(int except_index = -1)
{
	int count = 0;

	for (size_t i = 0; i < GhostSlotCapacity(); i++) {
		if (static_cast<int>(i) == except_index)
			continue;
		if (SnapshotMatchesCurrentMatch(auto_ghosts[i]))
			count++;
	}

	return count;
}

bool SnapshotMatchesSocialId(const AutoGhostSnapshot &snapshot, const char *social_id)
{
	return SnapshotMatchesCurrentMatch(snapshot) &&
		snapshot.phase != AutoGhostPhase::Expiring &&
		HasUsableSocialId(social_id) &&
		muffmode::CStringEquals(snapshot.social_id, social_id);
}

bool IsClientSlot(gentity_t *ent)
{
	return ent && ent >= g_entities + 1 && ent < g_entities + game.maxclients + 1;
}

bool IsAutoGhostPlaceholder(const gentity_t *ent)
{
	return ent && ent->client &&
		(muffmode::CStringEquals(ent->classname, "auto_ghost") ||
			muffmode::CStringEquals(ent->classname, "auto_ghost_reinstating") ||
			muffmode::CStringEquals(ent->classname, "auto_ghost_expiring"));
}

float TimeFraction(gtime_t elapsed, gtime_t duration)
{
	if (duration <= 0_ms)
		return 1.0f;

	return clamp(elapsed.seconds() / duration.seconds(), 0.0f, 1.0f);
}

void SetPlaceholderOpacity(gentity_t *ent, float alpha)
{
	if (!ent)
		return;

	alpha = clamp(alpha, 0.0f, 1.0f);
	ent->s.alpha = alpha;
	ent->s.renderfx |= RF_IR_VISIBLE;
	if (alpha < 0.999f)
		ent->s.renderfx |= RF_TRANSLUCENT;
	else
		ent->s.renderfx &= ~RF_TRANSLUCENT;
}

void RemovePlaceholder(size_t index)
{
	if (index >= auto_ghosts.size())
		return;

	gentity_t *ent = level.ghosts[index].ent;
	if (!IsClientSlot(ent))
		return;
	if (!ent->client || ent->client->pers.connected)
		return;
	if (!auto_ghosts[index].valid && !IsAutoGhostPlaceholder(ent))
		return;

	if (ent->linked)
		gi.unlinkentity(ent);

	ent->s.modelindex = 0;
	ent->s.modelindex2 = 0;
	ent->s.modelindex3 = 0;
	ent->s.modelindex4 = 0;
	ent->s.effects = EF_NONE;
	ent->s.event = EV_NONE;
	ent->s.sound = 0;
	ent->s.loop_attenuation = 0;
	ent->s.loop_volume = 0;
	ent->s.renderfx = RF_NONE;
	ent->s.alpha = 0.0f;
	ent->solid = SOLID_NOT;
	ent->clipmask = CONTENTS_NONE;
	ent->inuse = false;
	ent->sv.init = false;
	ent->classname = "disconnected";
	ent->client->resp.ghost = nullptr;
	ent->client->pers.connected = false;
	ent->client->pers.spawned = false;
	ent->client->pers.ingame = false;
	ent->client->awaiting_respawn = false;
	ent->client->respawn_timeout = 0_ms;
	ent->timestamp = level.time + 1_sec;
}

void ReleaseExpiredMatchItems(const AutoGhostSnapshot &snapshot);

void ClearSnapshot(size_t index, bool release_match_items = false)
{
	if (index >= auto_ghosts.size())
		return;

	if (release_match_items)
		ReleaseExpiredMatchItems(auto_ghosts[index]);

	RemovePlaceholder(index);
	auto_ghosts[index] = AutoGhostSnapshot{};
}

void ReleaseExpiredMatchItems(const AutoGhostSnapshot &snapshot)
{
	if (!snapshot.valid || snapshot.inventory_released)
		return;

	if (snapshot.client.pers.inventory[IT_FLAG_RED])
		CTF_ResetTeamFlag(TEAM_RED);
	if (snapshot.client.pers.inventory[IT_FLAG_BLUE])
		CTF_ResetTeamFlag(TEAM_BLUE);

	bool held_tech = false;
	for (size_t i = 0; i < q_countof(tech_ids); i++)
		held_tech = held_tech || snapshot.client.pers.inventory[tech_ids[i]];
	if (held_tech)
		Tech_Reset();
}

bool SnapshotCarriesFlag(const AutoGhostSnapshot &snapshot)
{
	return snapshot.valid &&
		(snapshot.client.pers.inventory[IT_FLAG_RED] ||
			snapshot.client.pers.inventory[IT_FLAG_BLUE]);
}

void ClearSnapshotFlagState(AutoGhostSnapshot &snapshot)
{
	snapshot.client.pers.inventory[IT_FLAG_RED] = 0;
	snapshot.client.pers.inventory[IT_FLAG_BLUE] = 0;
	snapshot.client.pers.team_state.flag_pickup_time = 0_ms;
	snapshot.client.resp.ctf_flagsince = 0_ms;
	snapshot.entity.s.effects &= ~(EF_FLAG_RED | EF_FLAG_BLUE);
}

void DropSnapshotFlag(size_t index, item_id_t flag_id)
{
	if (index >= auto_ghosts.size())
		return;

	auto &snapshot = auto_ghosts[index];
	if (!snapshot.valid || !snapshot.client.pers.inventory[flag_id])
		return;

	gentity_t *ent = level.ghosts[index].ent;
	if (GTF(GTF_CTF) && ent && ent->client) {
		ent->client->pers.inventory[IT_FLAG_RED] = flag_id == IT_FLAG_RED ? snapshot.client.pers.inventory[IT_FLAG_RED] : 0;
		ent->client->pers.inventory[IT_FLAG_BLUE] = flag_id == IT_FLAG_BLUE ? snapshot.client.pers.inventory[IT_FLAG_BLUE] : 0;
		ent->client->pers.team_state.flag_pickup_time = snapshot.client.pers.team_state.flag_pickup_time;
		CTF_DeadDropFlag(ent);
	} else {
		CTF_ResetTeamFlag(flag_id == IT_FLAG_RED ? TEAM_RED : TEAM_BLUE);
	}

	snapshot.client.pers.inventory[flag_id] = 0;
}

void DropSnapshotFlags(size_t index)
{
	if (index >= auto_ghosts.size() || !SnapshotCarriesFlag(auto_ghosts[index]))
		return;

	DropSnapshotFlag(index, IT_FLAG_RED);
	DropSnapshotFlag(index, IT_FLAG_BLUE);
	ClearSnapshotFlagState(auto_ghosts[index]);
}

void ExpireSnapshot(size_t index)
{
	if (index >= auto_ghosts.size() || !auto_ghosts[index].valid)
		return;

	level.ghosts[index].code = 0;
	ClearSnapshot(index, true);
}

void ExpireSnapshotsForSocialId(const char *social_id, int except_index = -1)
{
	if (!HasUsableSocialId(social_id))
		return;

	for (size_t i = 0; i < GhostSlotCapacity(); i++) {
		if (static_cast<int>(i) == except_index)
			continue;
		if (SnapshotMatchesSocialId(auto_ghosts[i], social_id))
			ExpireSnapshot(i);
	}
}

int FindSnapshotForSocialId(const char *social_id)
{
	int match = -1;

	for (size_t i = 0; i < GhostSlotCapacity(); i++) {
		if (!SnapshotMatchesSocialId(auto_ghosts[i], social_id))
			continue;

		if (match >= 0)
			return -1;

		match = static_cast<int>(i);
	}

	return match;
}

void GenerateGhostCode(size_t ghost)
{
	for (;;) {
		level.ghosts[ghost].code = irandom(muffmode::ghost::GHOST_CODE_MIN, muffmode::ghost::GHOST_CODE_MAX_EXCLUSIVE);

		bool duplicate = false;
		for (size_t i = 0; i < GhostSlotCapacity(); i++) {
			if (i != ghost && level.ghosts[i].code == level.ghosts[ghost].code) {
				duplicate = true;
				break;
			}
		}

		if (!duplicate)
			return;
	}
}

ghost_t *AllocateGhostSlot(gentity_t *ent)
{
	if (!ent || !ent->client)
		return nullptr;

	if (int index = GhostIndex(ent->client->resp.ghost); index >= 0)
		return &level.ghosts[index];

	for (size_t i = 0; i < GhostSlotCapacity(); i++) {
		if (!level.ghosts[i].ent && !auto_ghosts[i].valid)
			return &level.ghosts[i];
	}

	for (size_t i = 0; i < GhostSlotCapacity(); i++) {
		if (!level.ghosts[i].code && !auto_ghosts[i].valid)
			return &level.ghosts[i];
	}

	return nullptr;
}

EntityLifeSnapshot CaptureEntityLife(gentity_t *ent)
{
	EntityLifeSnapshot snapshot{};

	snapshot.s = ent->s;
	snapshot.svflags = ent->svflags;
	snapshot.mins = ent->mins;
	snapshot.maxs = ent->maxs;
	snapshot.velocity = ent->velocity;
	snapshot.avelocity = ent->avelocity;
	snapshot.gravityVector = ent->gravityVector;
	snapshot.solid = ent->solid;
	snapshot.clipmask = ent->clipmask;
	snapshot.movetype = ent->movetype;
	snapshot.flags = ent->flags;
	snapshot.timestamp = ent->timestamp;
	snapshot.air_finished = ent->air_finished;
	snapshot.powerarmor_time = ent->powerarmor_time;
	snapshot.teleport_time = ent->teleport_time;
	snapshot.waterlevel = ent->waterlevel;
	snapshot.watertype = ent->watertype;
	snapshot.mass = ent->mass;
	snapshot.gravity = ent->gravity;
	snapshot.health = ent->health;
	snapshot.max_health = ent->max_health;
	snapshot.gib_health = ent->gib_health;
	snapshot.viewheight = ent->viewheight;
	snapshot.dmg = ent->dmg;
	snapshot.deadflag = ent->deadflag;
	snapshot.takedamage = ent->takedamage;

	if (ent->groundentity) {
		snapshot.groundentity_number = static_cast<int32_t>(ent->groundentity - g_entities);
		snapshot.groundentity_spawn_count = ent->groundentity->spawn_count;
		snapshot.groundentity_linkcount = ent->groundentity_linkcount;
	}

	return snapshot;
}

gentity_t *ResolveSnapshotEntity(int32_t number, int32_t spawn_count)
{
	if (number < 0 || number >= static_cast<int32_t>(globals.num_entities))
		return nullptr;

	gentity_t *ent = &g_entities[number];
	if (!ent->inuse || ent->spawn_count != spawn_count)
		return nullptr;

	return ent;
}

void RestoreEntityLife(gentity_t *ent, const EntityLifeSnapshot &snapshot)
{
	if (ent->linked)
		gi.unlinkentity(ent);

	ent->s = snapshot.s;
	ent->s.number = static_cast<int32_t>(ent - g_entities);
	ent->svflags = (snapshot.svflags | SVF_PLAYER) & ~SVF_NOCLIENT;
	ent->mins = snapshot.mins;
	ent->maxs = snapshot.maxs;
	ent->velocity = snapshot.velocity;
	ent->avelocity = snapshot.avelocity;
	ent->gravityVector = snapshot.gravityVector;
	ent->solid = snapshot.solid;
	ent->clipmask = snapshot.clipmask;
	ent->movetype = snapshot.movetype;
	ent->flags = snapshot.flags;
	ent->timestamp = snapshot.timestamp;
	ent->air_finished = snapshot.air_finished;
	ent->powerarmor_time = snapshot.powerarmor_time;
	ent->teleport_time = snapshot.teleport_time;
	ent->waterlevel = snapshot.waterlevel;
	ent->watertype = snapshot.watertype;
	ent->mass = snapshot.mass;
	ent->gravity = snapshot.gravity;
	ent->health = snapshot.health;
	ent->max_health = snapshot.max_health;
	ent->gib_health = snapshot.gib_health;
	ent->viewheight = snapshot.viewheight;
	ent->dmg = snapshot.dmg;
	ent->deadflag = snapshot.deadflag;
	ent->takedamage = snapshot.takedamage;
	ent->groundentity = ResolveSnapshotEntity(snapshot.groundentity_number, snapshot.groundentity_spawn_count);
	ent->groundentity_linkcount = ent->groundentity ? snapshot.groundentity_linkcount : 0;

	ent->client = &game.clients[ent - g_entities - 1];
	ent->inuse = true;
	ent->classname = "player";
	ent->model = "players/male/tris.md2";
	ent->prethink = {};
	ent->postthink = {};
	ent->think = {};
	ent->touch = {};
	ent->use = {};
	ent->pain = {};
	ent->die = player_die;
	ent->nextthink = 0_ms;
	ent->owner = nullptr;
	ent->enemy = nullptr;
	ent->oldenemy = nullptr;
	ent->chain = nullptr;
	ent->activator = nullptr;
	ent->mynoise = nullptr;
	ent->mynoise2 = nullptr;

	if (ClientIsPlaying(ent->client) && !ent->deadflag && ent->health > 0)
		ent->svflags &= ~SVF_DEADMONSTER;

	gi.linkentity(ent);

	if (ent->solid != SOLID_NOT && ent->health > 0)
		KillBox(ent, true, MOD_TELEFRAG_SPAWN);
}

void SanitizeRestoredClient(gentity_t *ent, int ghost_index, bool keep_current_admin)
{
	gclient_t *client = ent->client;

	client->pers.connected = true;
	client->pers.spawned = true;
	client->pers.ingame = true;
	client->pers.health = ent->health;
	client->pers.max_health = ent->max_health;
	client->pers.saved_flags = ent->flags & (FL_FLASHLIGHT | FL_GODMODE | FL_NOTARGET | FL_POWER_ARMOR | FL_WANTS_POWER_ARMOR);
	client->sess.is_a_bot = false;
	client->sess.admin = client->sess.admin || keep_current_admin || ent == &g_entities[1];
	client->resp.ghost = &level.ghosts[ghost_index];

	client->showscores = false;
	client->showinventory = false;
	client->showhelp = false;
	client->resp.voted = false;
	client->buttons = BUTTON_NONE;
	client->oldbuttons = BUTTON_NONE;
	client->latched_buttons = BUTTON_NONE;
	client->cmd = {};

	client->inmenu = false;
	client->menu = nullptr;
	client->menudirty = false;
	client->follow_queued_target = nullptr;
	client->follow_queued_time = 0_ms;
	SetFollowTarget(ent, nullptr);
	client->follow_update = false;
	client->owned_sphere = nullptr;
	client->grapple_ent = nullptr;
	client->grapple_state = GRAPPLE_STATE_FLY;
	client->tracker_pain_time = 0_ms;
	client->trail_head = nullptr;
	client->trail_tail = nullptr;
	client->oldgroundentity = nullptr;
	client->sight_entity = nullptr;
	client->sound_entity = nullptr;
	client->sound2_entity = nullptr;
	G_ClearLagCompensationHistory(ent);
	client->awaiting_respawn = false;
	client->respawn_timeout = 0_ms;
	client->initial_menu_delay = 0_ms;
	client->initial_menu_shown = true;
	client->initial_menu_closure = false;

	client->ps.pmove.origin = ent->s.origin;
	client->ps.pmove.velocity = ent->velocity;
	if (!client->ps.pmove.viewheight)
		client->ps.pmove.viewheight = ent->viewheight;

	AngleVectors(client->v_angle, client->v_forward, nullptr, nullptr);
}

void UpdateGhostStatsFromEntity(gentity_t *ent, ghost_t &ghost)
{
	ghost.team = ent->client->sess.team;
	ghost.score = ent->client->resp.score;
	ghost.ent = ent;
	ghost.number = ent->s.number;
	muffmode::CopyString(ghost.netname, ent->client->resp.netname);
}

bool EntityIsBot(gentity_t *ent)
{
	return ent && ((ent->svflags & SVF_BOT) || (ent->client && ent->client->sess.is_a_bot));
}

bool EntityCanOwnGhost(gentity_t *ent)
{
	return ent &&
		ent->client &&
		deathmatch->integer &&
		level.match_state == matchst_t::MATCH_IN_PROGRESS &&
		ClientIsPlaying(ent->client) &&
		!EntityIsBot(ent);
}

void ClearEntityGhostSlot(gentity_t *ent)
{
	if (!ent || !ent->client) {
		return;
	}

	if (int ghost_index = GhostIndex(ent->client->resp.ghost); ghost_index >= 0) {
		ClearSnapshot(static_cast<size_t>(ghost_index));
		level.ghosts[ghost_index] = ghost_t{};
	}

	ent->client->resp.ghost = nullptr;
}

int AutoGhostTimeoutSeconds()
{
	if (!g_auto_ghost_timeout)
		return 0;

	return clamp(g_auto_ghost_timeout->integer, 0, AutoGhostDurationSeconds());
}

bool IsFlagRetentionTimeoutActive()
{
	return level.timeout_in_place > 0_ms && !level.timeout_resuming;
}

gentity_t *PlaceholderEntity(size_t index)
{
	if (index >= auto_ghosts.size())
		return nullptr;

	gentity_t *ent = level.ghosts[index].ent;
	return IsClientSlot(ent) ? ent : nullptr;
}

void UpdateReservedPlaceholderVisual(size_t index)
{
	auto &snapshot = auto_ghosts[index];
	gentity_t *ent = PlaceholderEntity(index);
	if (!IsAutoGhostPlaceholder(ent))
		return;

	ent->classname = "auto_ghost";
	if (IsFlagRetentionTimeoutActive()) {
		snapshot.phase_elapsed = 0_ms;
		SetPlaceholderOpacity(ent, muffmode::ghost::AUTO_GHOST_PLACEHOLDER_ALPHA);
		return;
	}

	snapshot.phase_elapsed += FRAME_TIME_MS;
	const float fade = TimeFraction(snapshot.phase_elapsed, muffmode::ghost::AUTO_GHOST_FADE_OUT_TIME);
	SetPlaceholderOpacity(ent, muffmode::ghost::AUTO_GHOST_PLACEHOLDER_ALPHA * (1.0f - fade));
}

void UpdateReinstatingPlaceholderVisual(size_t index)
{
	auto &snapshot = auto_ghosts[index];
	gentity_t *ent = PlaceholderEntity(index);
	if (!IsAutoGhostPlaceholder(ent))
		return;

	ent->classname = "auto_ghost_reinstating";
	const float fade = TimeFraction(snapshot.phase_elapsed, muffmode::ghost::AUTO_GHOST_FADE_IN_TIME);
	SetPlaceholderOpacity(ent, fade);
}

void UpdateExpiringPlaceholderVisual(size_t index)
{
	auto &snapshot = auto_ghosts[index];
	gentity_t *ent = PlaceholderEntity(index);
	if (!IsAutoGhostPlaceholder(ent))
		return;

	ent->classname = "auto_ghost_expiring";
	const float t = TimeFraction(snapshot.phase_elapsed, muffmode::ghost::AUTO_GHOST_EXPIRE_PULSE_TIME);
	const float pulse = t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;
	SetPlaceholderOpacity(ent, clamp(pulse, 0.0f, 1.0f));
}

void CopySnapshotDropStateToEntity(const AutoGhostSnapshot &snapshot, gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	ent->client->pers.inventory = snapshot.client.pers.inventory;
	ent->client->pers.weapon = snapshot.client.pers.weapon;
	ent->client->pers.lastweapon = snapshot.client.pers.lastweapon;
	ent->client->pers.selected_item = snapshot.client.pers.selected_item;
	ent->client->pers.selected_item_time = snapshot.client.pers.selected_item_time;
	ent->client->pers.power_cubes = snapshot.client.pers.power_cubes;
	ent->client->pu_time_quad = snapshot.client.pu_time_quad;
	ent->client->pu_time_haste = snapshot.client.pu_time_haste;
	ent->client->pu_time_double = snapshot.client.pu_time_double;
	ent->client->pu_time_protection = snapshot.client.pu_time_protection;
	ent->client->pu_time_invisibility = snapshot.client.pu_time_invisibility;
	ent->client->pu_time_regeneration = snapshot.client.pu_time_regeneration;
	ent->client->pers.team_state = snapshot.client.pers.team_state;
	ent->client->resp.ctf_flagsince = snapshot.client.resp.ctf_flagsince;
}

void ReleaseTimedOutInventory(size_t index)
{
	if (index >= auto_ghosts.size())
		return;

	auto &snapshot = auto_ghosts[index];
	if (!snapshot.valid || snapshot.inventory_released)
		return;

	gentity_t *ent = PlaceholderEntity(index);
	if (IsAutoGhostPlaceholder(ent)) {
		CopySnapshotDropStateToEntity(snapshot, ent);
		TossClientItems(ent);
	} else {
		ReleaseExpiredMatchItems(snapshot);
	}

	snapshot.inventory_released = true;
}

void BeginSnapshotTimeoutExpiry(size_t index)
{
	if (index >= auto_ghosts.size())
		return;

	auto &snapshot = auto_ghosts[index];
	if (!snapshot.valid)
		return;

	ReleaseTimedOutInventory(index);
	level.ghosts[index].code = 0;

	gentity_t *ent = PlaceholderEntity(index);
	if (!IsAutoGhostPlaceholder(ent) || IsFlagRetentionTimeoutActive()) {
		ExpireSnapshot(index);
		return;
	}

	snapshot.phase = AutoGhostPhase::Expiring;
	snapshot.phase_elapsed = 0_ms;
	snapshot.reinstate_remaining = 0_ms;
	snapshot.pending_entnum = -1;
	snapshot.pending_spawn_count = 0;
	snapshot.pending_manual = false;
	UpdateExpiringPlaceholderVisual(index);
}

bool StartAutoGhostTimeout(gentity_t *ent)
{
	const int timeout_seconds = AutoGhostTimeoutSeconds();
	if (IsFlagRetentionTimeoutActive())
		return true;
	if (timeout_seconds <= 0 || level.timeout_in_place > 0_ms)
		return false;

	level.timeout_ent = ent;
	level.timeout_auto = true;
	level.timeout_resuming = false;
	level.timeout_in_place = gtime_t::from_sec(timeout_seconds);
	level.countdown_check = 0_sec;

	gi.LocBroadcast_Print(PRINT_CENTER, "{} disconnected.\nWaiting {} for rejoin.",
		ent->client->resp.netname, G_TimeString(timeout_seconds * 1000, false));
	gi.positioned_sound(world->s.origin, world, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX,
		gi.soundindex("world/klaxon2.wav"), 1, ATTN_NONE, 0);
	return true;
}

bool CaptureActivePlayerSnapshot(gentity_t *ent, bool start_auto_timeout)
{
	if (!EntityCanOwnGhost(ent)) {
		ClearEntityGhostSlot(ent);
		return false;
	}
	if (level.intermission_time || level.intermission_queued)
		return false;
	if (!HasUsableSocialId(ent->client->pers.social_id))
		return false;
	if (level.match_id.empty())
		return false;

	const int ghost_seconds = AutoGhostDurationSeconds();
	if (ghost_seconds <= 0)
		return false;

	const int existing_ghost_index = GhostIndex(ent->client->resp.ghost);
	ExpireSnapshotsForSocialId(ent->client->pers.social_id, existing_ghost_index);
	const int max_ghosts = AutoGhostMaxReservations();
	if (max_ghosts <= 0 || ActiveSnapshotCount(existing_ghost_index) >= max_ghosts)
		return false;

	if (!ent->client->resp.ghost)
		MM_Ghost_Assign(ent);

	const int ghost_index = GhostIndex(ent->client->resp.ghost);
	if (ghost_index < 0)
		return false;

	ExpireSnapshotsForSocialId(ent->client->pers.social_id, ghost_index);
	if (!level.ghosts[ghost_index].code)
		GenerateGhostCode(static_cast<size_t>(ghost_index));

	auto &snapshot = auto_ghosts[ghost_index];
	snapshot = AutoGhostSnapshot{};
	snapshot.valid = true;
	snapshot.match_id = level.match_id;
	snapshot.remaining = gtime_t::from_sec(ghost_seconds);
	muffmode::CopyString(snapshot.social_id, ent->client->pers.social_id);
	snapshot.client = *ent->client;
	// A snapshot owns values, not live entity or TAG_LEVEL references. Keeping
	// those pointers would let a later world reload turn them into aliases of
	// newly spawned map entities before the client is reinstated.
	ClearTransientClientReferences(snapshot.client);
	snapshot.client.pers.health = ent->health;
	snapshot.client.pers.max_health = ent->max_health;
	snapshot.client.pers.saved_flags = ent->flags & (FL_FLASHLIGHT | FL_GODMODE | FL_NOTARGET | FL_POWER_ARMOR | FL_WANTS_POWER_ARMOR);
	snapshot.entity = CaptureEntityLife(ent);

	UpdateGhostStatsFromEntity(ent, level.ghosts[ghost_index]);
	if (start_auto_timeout) {
		if (!StartAutoGhostTimeout(ent))
			DropSnapshotFlags(static_cast<size_t>(ghost_index));
	} else {
		DropSnapshotFlags(static_cast<size_t>(ghost_index));
	}

	return true;
}

bool RestoreSnapshot(gentity_t *ent, int ghost_index, bool manual)
{
	if (!ent || !ent->client || ghost_index < 0 || ghost_index >= static_cast<int>(GhostSlotCapacity()))
		return false;

	auto &snapshot = auto_ghosts[ghost_index];
	if (!SnapshotMatchesSocialId(snapshot, ent->client->pers.social_id))
		return false;

	char userinfo[MAX_INFO_STRING];
	char social_id[MAX_INFO_VALUE];
	muffmode::CopyString(userinfo, ent->client->pers.userinfo);
	muffmode::CopyString(social_id, ent->client->pers.social_id);
	const bool keep_current_admin = ent->client->sess.admin;

	*ent->client = snapshot.client;
	// Defend older/in-memory snapshots too: RestoreEntityLife links the player
	// and can invoke KillBox before the full post-restore sanitizer runs.
	ClearTransientClientReferences(*ent->client);
	RestoreEntityLife(ent, snapshot.entity);
	SanitizeRestoredClient(ent, ghost_index, keep_current_admin);

	ClientUserinfoChanged(ent, userinfo);
	muffmode::CopyString(ent->client->pers.social_id, social_id);
	P_AssignClientSkinnum(ent);
	P_ForceFogTransition(ent, true);

	level.ghosts[ghost_index].code = 0;
	ClearSnapshot(static_cast<size_t>(ghost_index));
	UpdateGhostStatsFromEntity(ent, level.ghosts[ghost_index]);

	if (level.timeout_auto && !level.timeout_resuming && level.timeout_in_place > 0_ms)
		::TimeoutEnd();

	if (!(ent->svflags & SVF_NOCLIENT)) {
		gi.WriteByte(svc_muzzleflash);
		gi.WriteEntity(ent);
		gi.WriteByte(MZ_LOGIN);
		gi.multicast(ent->s.origin, MULTICAST_PVS, false);
	}

	gi.LocBroadcast_Print(PRINT_HIGH, "{} rejoined and was reinstated.\n", ent->client->resp.netname);
	if (manual)
		gi.LocClient_Print(ent, PRINT_HIGH, "Ghost code accepted, your match state has been reinstated.\n");
	else
		gi.LocClient_Print(ent, PRINT_HIGH, "Your match state has been restored from auto-ghost.\n");

	CalculateRanks();
	ClientEndServerFrame(ent);
	return true;
}

bool EntityMatchesPendingRestore(const AutoGhostSnapshot &snapshot, const gentity_t *ent)
{
	return ent &&
		snapshot.phase == AutoGhostPhase::Reinstating &&
		snapshot.pending_entnum == static_cast<int32_t>(ent - g_entities) &&
		snapshot.pending_spawn_count == ent->spawn_count;
}

gentity_t *PendingRestoreTarget(const AutoGhostSnapshot &snapshot)
{
	if (snapshot.phase != AutoGhostPhase::Reinstating ||
			snapshot.pending_entnum <= 0 ||
			snapshot.pending_entnum >= static_cast<int32_t>(globals.num_entities))
		return nullptr;

	gentity_t *ent = &g_entities[snapshot.pending_entnum];
	if (!EntityMatchesPendingRestore(snapshot, ent) || !ent->inuse || !ent->client || !ent->client->pers.connected)
		return nullptr;

	return ent;
}

int PendingRestoreIndexForTarget(gentity_t *ent)
{
	if (!ent)
		return -1;

	for (size_t i = 0; i < GhostSlotCapacity(); i++)
		if (EntityMatchesPendingRestore(auto_ghosts[i], ent))
			return static_cast<int>(i);

	return -1;
}

void PreparePendingRestoreClient(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	gclient_t *client = ent->client;
	client->pers.connected = true;
	client->pers.spawned = true;
	client->pers.ingame = true;
	client->awaiting_respawn = false;
	client->respawn_timeout = 0_ms;
	client->showscores = false;
	client->showinventory = false;
	client->showhelp = false;
	client->buttons = BUTTON_NONE;
	client->oldbuttons = BUTTON_NONE;
	client->latched_buttons = BUTTON_NONE;
	client->cmd = {};
	client->weapon_thunk = false;
	client->weapon_fire_buffered = false;
	client->ps.pmove.pm_type = PM_FREEZE;
	client->ps.pmove.origin = ent->s.origin;
	client->ps.pmove.velocity = {};
	client->ps.pmove.pm_flags |= PMF_NO_POSITIONAL_PREDICTION;
	client->ps.gunindex = 0;
	client->ps.gunskin = 0;
}

void PrepareReinstatingPlaceholder(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	ent->svflags = (ent->svflags | SVF_PLAYER) & ~SVF_NOCLIENT;
	ent->solid = SOLID_NOT;
	ent->clipmask = CONTENTS_NONE;
	ent->movetype = MOVETYPE_NONE;
	ent->takedamage = false;
	ent->velocity = {};
	ent->avelocity = {};
	ent->groundentity = nullptr;
	ent->groundentity_linkcount = 0;
	ent->s.effects = EF_NONE;
	ent->s.event = EV_NONE;
	ent->s.sound = 0;
	ent->s.loop_attenuation = 0;
	ent->s.loop_volume = 0;
	ent->inuse = true;
	ent->classname = "auto_ghost_reinstating";
	ent->sv.init = false;
	SetPlaceholderOpacity(ent, max(ent->s.alpha, muffmode::ghost::AUTO_GHOST_PLACEHOLDER_ALPHA));
	gi.linkentity(ent);
}

void CancelPendingRestore(size_t index)
{
	if (index >= auto_ghosts.size())
		return;

	auto &snapshot = auto_ghosts[index];
	if (snapshot.phase != AutoGhostPhase::Reinstating)
		return;

	snapshot.phase = AutoGhostPhase::Reserved;
	snapshot.phase_elapsed = 0_ms;
	snapshot.reinstate_remaining = 0_ms;
	snapshot.pending_entnum = -1;
	snapshot.pending_spawn_count = 0;
	snapshot.pending_manual = false;
	if (!level.ghosts[index].code)
		GenerateGhostCode(index);

	gentity_t *placeholder = PlaceholderEntity(index);
	if (IsAutoGhostPlaceholder(placeholder) && !placeholder->client->pers.connected)
		placeholder->classname = "auto_ghost";
}

bool CancelPendingRestoreForTarget(gentity_t *ent, bool &target_is_placeholder)
{
	target_is_placeholder = false;
	const int index = PendingRestoreIndexForTarget(ent);
	if (index < 0)
		return false;

	target_is_placeholder = level.ghosts[index].ent == ent;
	CancelPendingRestore(static_cast<size_t>(index));
	return true;
}

bool BeginRestoreDelay(gentity_t *ent, int ghost_index, bool manual)
{
	if (!ent || !ent->client || ghost_index < 0 || ghost_index >= static_cast<int>(GhostSlotCapacity()))
		return false;

	auto &snapshot = auto_ghosts[ghost_index];
	if (!SnapshotMatchesSocialId(snapshot, ent->client->pers.social_id))
		return false;
	if (snapshot.phase == AutoGhostPhase::Reinstating)
		return EntityMatchesPendingRestore(snapshot, ent);

	snapshot.phase = AutoGhostPhase::Reinstating;
	snapshot.phase_elapsed = 0_ms;
	snapshot.reinstate_remaining = muffmode::ghost::AUTO_GHOST_REINSTATE_DELAY;
	snapshot.pending_entnum = static_cast<int32_t>(ent - g_entities);
	snapshot.pending_spawn_count = ent->spawn_count;
	snapshot.pending_manual = manual;
	level.ghosts[ghost_index].code = 0;
	if (level.timeout_auto && !level.timeout_resuming &&
			level.timeout_in_place > 0_ms &&
			level.timeout_in_place < muffmode::ghost::AUTO_GHOST_REINSTATE_DELAY) {
		level.timeout_in_place = muffmode::ghost::AUTO_GHOST_REINSTATE_DELAY;
		level.countdown_check = 0_sec;
	}

	PreparePendingRestoreClient(ent);
	if (gentity_t *placeholder = PlaceholderEntity(static_cast<size_t>(ghost_index)))
		if (IsAutoGhostPlaceholder(placeholder))
			PrepareReinstatingPlaceholder(placeholder);

	gi.LocClient_Print(ent, PRINT_CENTER, "Reinstating in 3 seconds.");
	return true;
}

bool FinishPendingRestore(size_t index)
{
	if (index >= auto_ghosts.size())
		return false;

	auto &snapshot = auto_ghosts[index];
	if (snapshot.phase != AutoGhostPhase::Reinstating)
		return false;

	gentity_t *target = PendingRestoreTarget(snapshot);
	if (!target) {
		CancelPendingRestore(index);
		return false;
	}

	const bool manual = snapshot.pending_manual;
	if (!RestoreSnapshot(target, static_cast<int>(index), manual)) {
		CancelPendingRestore(index);
		return false;
	}

	return true;
}

bool IsPendingRestoreTarget(gentity_t *ent)
{
	return PendingRestoreIndexForTarget(ent) >= 0;
}

bool FreezePendingRestoreFrame(gentity_t *ent)
{
	if (!IsPendingRestoreTarget(ent) || !ent || !ent->client)
		return false;

	PreparePendingRestoreClient(ent);
	ent->velocity = {};
	ent->avelocity = {};
	ent->client->ps.pmove.pm_type = PM_FREEZE;
	ent->client->ps.pmove.origin = ent->s.origin;
	ent->client->ps.pmove.velocity = {};
	ent->client->ps.gunindex = 0;
	ent->client->ps.gunskin = 0;
	return true;
}

} // namespace muffmode::ghost::snapshot

namespace ghost_snapshot = muffmode::ghost::snapshot;

/*
================
MM_Ghost_ClearAll
================
*/
void MM_Ghost_ClearAll() {
	for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++)
		ghost_snapshot::ClearSnapshot(i);

	std::fill(std::begin(level.ghosts), std::end(level.ghosts), ghost_t{});

	if (game.clients) {
		for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++)
			game.clients[i].resp.ghost = nullptr;
	}
}

/*
================
MM_Ghost_ClearClient
================
*/
void MM_Ghost_ClearClient(gentity_t *ent) {
	ghost_snapshot::ClearEntityGhostSlot(ent);
}

/*
================
MM_Ghost_Assign

Assigns the player a ghost slot for match statistics and possible
same-account reconnect recovery.
================
*/
void MM_Ghost_Assign(gentity_t *ent) {
	if (!ent || !ent->client)
		return;
	if (!ghost_snapshot::EntityCanOwnGhost(ent)) {
		ghost_snapshot::ClearEntityGhostSlot(ent);
		return;
	}

	ghost_t *slot = ghost_snapshot::AllocateGhostSlot(ent);
	if (!slot)
		return;

	const int ghost_index = ghost_snapshot::GhostIndex(slot);
	if (ghost_index < 0)
		return;

	ghost_snapshot::ClearSnapshot(static_cast<size_t>(ghost_index));
	*slot = ghost_t{};
	ghost_snapshot::GenerateGhostCode(static_cast<size_t>(ghost_index));
	ent->client->resp.ghost = slot;
	ghost_snapshot::UpdateGhostStatsFromEntity(ent, *slot);
}

/*
================
MM_Ghost_DoAssign
================
*/
void MM_Ghost_DoAssign(gentity_t *ent) {
	if (!ent || !ent->client)
		return;
	if (!ghost_snapshot::EntityCanOwnGhost(ent)) {
		ghost_snapshot::ClearEntityGhostSlot(ent);
		return;
	}

	// assign a ghost code
	if (level.match_state == matchst_t::MATCH_IN_PROGRESS) {
		if (int ghost_index = ghost_snapshot::GhostIndex(ent->client->resp.ghost); ghost_index >= 0) {
			ghost_snapshot::ClearSnapshot(static_cast<size_t>(ghost_index));
			level.ghosts[ghost_index] = ghost_t{};
		}
		ent->client->resp.ghost = nullptr;
		MM_Ghost_Assign(ent);
	}
}

/*
================
MM_Ghost_CaptureInactive

Captures a live player's match state before an inactivity move to spectator.
================
*/
bool MM_Ghost_CaptureInactive(gentity_t *ent) {
	return ghost_snapshot::CaptureActivePlayerSnapshot(ent, false);
}

/*
================
MM_Ghost_CaptureDisconnect

Captures a live player's current match state for same-social-ID reconnect.
================
*/
bool MM_Ghost_CaptureDisconnect(gentity_t *ent) {
	if (!ent || !ent->client || !deathmatch->integer)
		return false;

	bool target_is_placeholder = false;
	if (ghost_snapshot::CancelPendingRestoreForTarget(ent, target_is_placeholder))
		return target_is_placeholder;

	return ghost_snapshot::CaptureActivePlayerSnapshot(ent, true);
}

/*
================
MM_Ghost_MakeDisconnectPlaceholder
================
*/
void MM_Ghost_MakeDisconnectPlaceholder(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	ent->svflags = (ent->svflags | SVF_PLAYER) & ~SVF_NOCLIENT;
	ent->solid = SOLID_NOT;
	ent->clipmask = CONTENTS_NONE;
	ent->movetype = MOVETYPE_NONE;
	ent->takedamage = false;
	ent->velocity = {};
	ent->avelocity = {};
	ent->groundentity = nullptr;
	ent->groundentity_linkcount = 0;
	ent->s.effects = EF_NONE;
	ent->s.event = EV_NONE;
	ent->s.sound = 0;
	ent->s.loop_attenuation = 0;
	ent->s.loop_volume = 0;
	ent->s.renderfx = RF_IR_VISIBLE;
	ghost_snapshot::SetPlaceholderOpacity(ent, muffmode::ghost::AUTO_GHOST_PLACEHOLDER_ALPHA);
	ent->inuse = true;
	ent->classname = "auto_ghost";
	ent->client->pers.connected = false;
	ent->client->pers.spawned = false;
	ent->client->buttons = BUTTON_NONE;
	ent->client->oldbuttons = BUTTON_NONE;
	ent->client->latched_buttons = BUTTON_NONE;
	ent->client->cmd = {};
	ent->sv.init = false;

	gi.linkentity(ent);
}

/*
================
MM_Ghost_ChooseReconnectSlot
================
*/
gentity_t *MM_Ghost_ChooseReconnectSlot(const char *social_id, gentity_t **ignore, size_t num_ignore) {
	const int ghost_index = ghost_snapshot::FindSnapshotForSocialId(social_id);
	if (ghost_index < 0)
		return nullptr;

	gentity_t *slot = level.ghosts[ghost_index].ent;
	if (!slot || slot < g_entities + 1 || slot >= g_entities + game.maxclients + 1)
		return nullptr;
	if (ghost_snapshot::IsSlotIgnored(slot, ignore, num_ignore))
		return nullptr;
	if (slot->client && slot->client->pers.connected)
		return nullptr;

	return slot;
}

/*
================
MM_Ghost_IsReservedSlot
================
*/
bool MM_Ghost_IsReservedSlot(gentity_t *slot) {
	if (!slot)
		return false;

	for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++)
		if (level.ghosts[i].ent == slot && ghost_snapshot::SnapshotMatchesCurrentMatch(ghost_snapshot::auto_ghosts[i]))
			return true;

	return false;
}

/*
================
MM_Ghost_IsPendingRestore
================
*/
bool MM_Ghost_IsPendingRestore(gentity_t *ent) {
	return ghost_snapshot::IsPendingRestoreTarget(ent);
}

/*
================
MM_Ghost_RunPendingRestoreFrame
================
*/
bool MM_Ghost_RunPendingRestoreFrame(gentity_t *ent) {
	return ghost_snapshot::FreezePendingRestoreFrame(ent);
}

/*
================
MM_Ghost_EndPendingRestoreFrame
================
*/
bool MM_Ghost_EndPendingRestoreFrame(gentity_t *ent) {
	return ghost_snapshot::FreezePendingRestoreFrame(ent);
}

/*
================
MM_Ghost_ClientThink
================
*/
bool MM_Ghost_ClientThink(gentity_t *ent, const usercmd_t *ucmd) {
	if (!ghost_snapshot::IsPendingRestoreTarget(ent) || !ent || !ent->client)
		return false;

	gclient_t *client = ent->client;
	client->oldbuttons = client->buttons;
	client->buttons = ucmd ? ucmd->buttons : BUTTON_NONE;
	client->latched_buttons = BUTTON_NONE;
	if (ucmd) {
		client->cmd = *ucmd;
		client->resp.cmd_angles[0] = ucmd->angles[0];
		client->resp.cmd_angles[1] = ucmd->angles[1];
		client->resp.cmd_angles[2] = ucmd->angles[2];
	}

	ghost_snapshot::FreezePendingRestoreFrame(ent);
	return true;
}

/*
================
MM_Ghost_HasActiveReservations
================
*/
bool MM_Ghost_HasActiveReservations() {
	for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++)
		if (ghost_snapshot::SnapshotMatchesCurrentMatch(ghost_snapshot::auto_ghosts[i]))
			return true;

	return false;
}

/*
================
MM_Ghost_TryRestore
================
*/
bool MM_Ghost_TryRestore(gentity_t *ent) {
	if (!ent || !ent->client)
		return false;
	if (ghost_snapshot::EntityIsBot(ent))
		return false;

	const int ghost_index = ghost_snapshot::FindSnapshotForSocialId(ent->client->pers.social_id);
	if (ghost_index < 0)
		return false;

	return ghost_snapshot::BeginRestoreDelay(ent, ghost_index, false);
}

/*
================
MM_Ghost_DropTimedOutFlags
================
*/
void MM_Ghost_DropTimedOutFlags() {
	for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++) {
		if (!ghost_snapshot::SnapshotMatchesCurrentMatch(ghost_snapshot::auto_ghosts[i]))
			continue;

		ghost_snapshot::DropSnapshotFlags(i);
	}
}

/*
================
MM_Ghost_RunFrame
================
*/
void MM_Ghost_RunFrame() {
	if (level.match_state != matchst_t::MATCH_IN_PROGRESS || level.intermission_time || level.intermission_queued) {
		for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++)
			if (ghost_snapshot::auto_ghosts[i].valid)
				ghost_snapshot::ExpireSnapshot(i);
		return;
	}

	for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++) {
		auto &snapshot = ghost_snapshot::auto_ghosts[i];
		if (!snapshot.valid)
			continue;

		if (snapshot.match_id != level.match_id) {
			ghost_snapshot::ExpireSnapshot(i);
			continue;
		}

		if (snapshot.phase == ghost_snapshot::AutoGhostPhase::Reinstating) {
			if (!ghost_snapshot::PendingRestoreTarget(snapshot)) {
				ghost_snapshot::CancelPendingRestore(i);
				continue;
			}

			snapshot.phase_elapsed += FRAME_TIME_MS;
			snapshot.reinstate_remaining -= FRAME_TIME_MS;
			ghost_snapshot::UpdateReinstatingPlaceholderVisual(i);
			if (snapshot.reinstate_remaining <= 0_ms)
				ghost_snapshot::FinishPendingRestore(i);
			continue;
		}

		if (snapshot.phase == ghost_snapshot::AutoGhostPhase::Expiring) {
			snapshot.phase_elapsed += FRAME_TIME_MS;
			ghost_snapshot::UpdateExpiringPlaceholderVisual(i);
			if (snapshot.phase_elapsed >= muffmode::ghost::AUTO_GHOST_EXPIRE_PULSE_TIME)
				ghost_snapshot::ExpireSnapshot(i);
			continue;
		}

		if (snapshot.remaining <= 0_ms) {
			ghost_snapshot::BeginSnapshotTimeoutExpiry(i);
			continue;
		}

		ghost_snapshot::UpdateReservedPlaceholderVisual(i);
		snapshot.remaining -= FRAME_TIME_MS;
		if (snapshot.remaining <= 0_ms)
			ghost_snapshot::BeginSnapshotTimeoutExpiry(i);
	}
}

/*
================
MM_CmdGhost

"ghost <code>" is retained as a same-social-ID compatibility fallback.
================
*/
void MM_CmdGhost(gentity_t *ent) {
	int n;

	if (!ent || !ent->client)
		return;
	if (ghost_snapshot::EntityIsBot(ent))
		return;

	if (!MM_IsExactArgcValid(gi.argc(), 2)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <code>\n", gi.argv(0));
		return;
	}

	// throttle code guesses to the flood rate to deter brute-forcing
	if (CheckFlood(ent))
		return;

	if (ClientIsPlaying(ent->client)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You are already in the game.\n");
		return;
	}
	if (level.match_state != matchst_t::MATCH_IN_PROGRESS) {
		gi.LocClient_Print(ent, PRINT_HIGH, "No match is in progress.\n");
		return;
	}

	const auto code = MM_ParseIntArg(gi.argv(1));
	if (!code || *code < muffmode::ghost::GHOST_CODE_MIN || *code > muffmode::ghost::GHOST_CODE_MAX) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid ghost code.\n");
		return;
	}
	n = *code;

	for (size_t i = 0; i < ghost_snapshot::GhostSlotCapacity(); i++) {
		if (level.ghosts[i].code && level.ghosts[i].code == n) {
			if (ghost_snapshot::BeginRestoreDelay(ent, static_cast<int>(i), true))
				return;
			break;
		}
	}
	gi.LocClient_Print(ent, PRINT_HIGH, "Invalid ghost code.\n");
}
