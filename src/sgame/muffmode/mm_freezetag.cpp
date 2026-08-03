// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "client/userinfo.h"
#include "muffmode/mm_announcer.h"
#include "muffmode/mm_freezetag.h"
#include "muffmode/mm_freezetag_rules.h"
#include "muffmode/mm_ghost.h"
#include "muffmode/mm_loc.h"
#include "muffmode/mm_match.h"
#include "muffmode/mm_match_stats.h"
#include "muffmode/mm_spawn_loadout.h"
#include "monsters/m_player.h"

#include <array>
#include <string>

namespace {

constexpr int kNoThawer = -1;
constexpr gtime_t kFrozenNoticeInterval = 3_sec;
constexpr gtime_t kHelpCallInterval = 5_sec;
constexpr gtime_t kThawPulseInterval = 250_ms;
constexpr float kThawDropUp = 48.0f;
constexpr float kThawDropDown = 128.0f;
constexpr float kFrozenCameraDistance = 72.0f;
constexpr float kFrozenCameraTargetHeight = 24.0f;
constexpr float kFrozenCameraWallInset = 4.0f;
constexpr float kFrozenCameraClipPadding = 6.0f;
constexpr int kNoFrozenViewProxy = 0;
constexpr const char *kFrozenViewProxyClassname = "freezetag_frozen_view_proxy";
constexpr renderfx_t kFrozenShell = RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE;
constexpr renderfx_t kThawingShell = kFrozenShell;
constexpr renderfx_t kFreezeShellMask = RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE;

struct preserved_loadout_t {
	item_id_t selected_item = IT_NULL;
	gtime_t selected_item_time = 0_ms;
	std::array<int32_t, IT_TOTAL> inventory = {};
	std::array<int16_t, AMMO_MAX> max_ammo = {};
	gitem_t *weapon = nullptr;
	gitem_t *lastweapon = nullptr;
	int32_t power_cubes = 0;
};

struct freeze_state_t {
	bool frozen = false;
	bool crouched = false;
	gtime_t frozen_at = 0_ms;
	gtime_t thaw_started = 0_ms;
	gtime_t thaw_last_update = 0_ms;
	float thaw_progress_seconds = 0.0f;
	std::array<float, MAX_CLIENTS> thaw_credit_seconds = {};
	gtime_t next_notice = 0_ms;
	gtime_t next_help = 0_ms;
	gtime_t next_thaw_feedback = 0_ms;
	int thawer_entnum = kNoThawer;
	int thawer_count = 0;
	int bot_rescuer_entnum = kNoThawer;
	int view_proxy_entnum = kNoFrozenViewProxy;
	vec3_t body_angles = {};
	vec3_t orbit_angles = {};
	vec3_t last_cmd_angles = {};
	bool has_last_cmd_angles = false;
};

std::array<freeze_state_t, MAX_CLIENTS> s_freeze;

int ClientIndex(const gentity_t *ent)
{
	if (!ent || !ent->client)
		return -1;

	const int index = static_cast<int>(ent - g_entities - 1);
	if (index < 0 || index >= static_cast<int>(game.maxclients) || index >= MAX_CLIENTS)
		return -1;

	return index;
}

freeze_state_t *StateFor(gentity_t *ent)
{
	const int index = ClientIndex(ent);
	if (index < 0)
		return nullptr;

	return &s_freeze[index];
}

const freeze_state_t *StateFor(const gentity_t *ent)
{
	const int index = ClientIndex(ent);
	if (index < 0)
		return nullptr;

	return &s_freeze[index];
}

float ThawRadius()
{
	return clamp(g_freezetag_thaw_radius ? g_freezetag_thaw_radius->value : 96.0f, 32.0f, 256.0f);
}

float ThawSeconds()
{
	return clamp(g_freezetag_thaw_time ? g_freezetag_thaw_time->value : 3.0f, 0.1f, 30.0f);
}

float MultiThawScale()
{
	return g_freezetag_multi_thaw_scale ? g_freezetag_multi_thaw_scale->value : 0.5f;
}

gtime_t AutoThawTime()
{
	const float seconds = g_freezetag_auto_thaw_time ? g_freezetag_auto_thaw_time->value : 0.0f;
	if (seconds <= 0.0f)
		return 0_ms;

	return gtime_t::from_sec(seconds);
}

bool RoundRespawnsAll()
{
	return !g_freezetag_round_respawn_all || g_freezetag_round_respawn_all->integer != 0;
}

bool ResetAliveInventoryOnRoundRespawn()
{
	return !g_freezetag_round_reset_alive_inventory || g_freezetag_round_reset_alive_inventory->integer != 0;
}

bool ThawedPlayersRespawnAtThawLocation()
{
	return g_freezetag_thaw_respawn_at_location && g_freezetag_thaw_respawn_at_location->integer != 0;
}

bool IsRoundLive()
{
	return level.match_state == matchst_t::MATCH_IN_PROGRESS &&
		level.round_state == roundst_t::ROUND_IN_PROGRESS;
}

bool RoundKeepsFrozenPlayers()
{
	return level.match_state == matchst_t::MATCH_IN_PROGRESS &&
		level.round_state == roundst_t::ROUND_ENDED;
}

bool IsFreezeTeam(team_t team)
{
	return team == TEAM_RED || team == TEAM_BLUE;
}

bool RawFrozen(const gentity_t *ent)
{
	const freeze_state_t *state = StateFor(ent);
	return state && state->frozen;
}

bool IsFrozenViewProxyEntity(const gentity_t *ent)
{
	return ent && ent->inuse &&
		ent->classname == kFrozenViewProxyClassname &&
		ent->owner &&
		ent->owner->client;
}

gentity_t *EntityFromNumber(int entnum)
{
	if (entnum <= 0 || entnum >= static_cast<int>(globals.num_entities))
		return nullptr;

	gentity_t *ent = &g_entities[entnum];
	return ent->inuse ? ent : nullptr;
}

gentity_t *FrozenViewProxyFromState(gentity_t *owner, const freeze_state_t &state)
{
	gentity_t *proxy = EntityFromNumber(state.view_proxy_entnum);
	if (!IsFrozenViewProxyEntity(proxy) || proxy->owner != owner)
		return nullptr;

	return proxy;
}

void FreeFrozenViewProxy(gentity_t *owner, freeze_state_t &state)
{
	gentity_t *proxy = FrozenViewProxyFromState(owner, state);
	if (proxy)
		G_FreeEntity(proxy);

	state.view_proxy_entnum = kNoFrozenViewProxy;
}

bool IsThawer(gentity_t *frozen, gentity_t *candidate)
{
	if (!frozen || !candidate || frozen == candidate)
		return false;
	if (!candidate->client || !ClientIsPlaying(candidate->client))
		return false;
	if (MM_Ghost_IsPendingRestore(candidate))
		return false;
	if (candidate->client->eliminated || candidate->deadflag || candidate->health <= 0)
		return false;
	if (RawFrozen(candidate))
		return false;
	if (candidate->client->sess.team != frozen->client->sess.team)
		return false;
	if (!IsFreezeTeam(candidate->client->sess.team))
		return false;

	const float radius = ThawRadius();
	if ((candidate->s.origin - frozen->s.origin).lengthSquared() > radius * radius)
		return false;

	vec3_t start = candidate->s.origin;
	start[2] += candidate->viewheight;
	vec3_t end = frozen->s.origin;
	end[2] += 16.0f;

	const trace_t tr = gi.trace(start, vec3_origin, vec3_origin, end, candidate, MASK_SOLID);
	return tr.fraction == 1.0f || tr.ent == frozen;
}

struct thaw_scan_t {
	gentity_t *primary = nullptr;
	std::array<gentity_t *, MAX_CLIENTS> contributors = {};
	int count = 0;
	float best_dist = 0.0f;
};

thaw_scan_t FindThawers(gentity_t *frozen)
{
	thaw_scan_t scan;

	for (auto ec : active_clients()) {
		if (!IsThawer(frozen, ec))
			continue;

		if (scan.count < static_cast<int>(scan.contributors.size()))
			scan.contributors[scan.count] = ec;
		scan.count++;

		const float dist = (ec->s.origin - frozen->s.origin).lengthSquared();
		if (!scan.primary || dist < scan.best_dist) {
			scan.primary = ec;
			scan.best_dist = dist;
		}
	}

	return scan;
}

void RecordThawContributors(freeze_state_t &state, const thaw_scan_t &scan, float delta_seconds)
{
	if (delta_seconds <= 0.0f)
		return;

	const int stored = std::min(scan.count, static_cast<int>(scan.contributors.size()));
	for (int i = 0; i < stored; ++i) {
		gentity_t *contributor = scan.contributors[i];
		const int index = ClientIndex(contributor);
		if (index >= 0)
			state.thaw_credit_seconds[static_cast<size_t>(index)] += delta_seconds;
	}
}

gentity_t *ThawerFromState(const freeze_state_t &state)
{
	return EntityFromNumber(state.thawer_entnum);
}

bool BotRescueEnabled()
{
	return !g_freezetag_bot_rescue || g_freezetag_bot_rescue->integer != 0;
}

bool BotCanRescueTeam(gentity_t *bot, team_t team)
{
	return bot && bot->client &&
		(bot->svflags & SVF_BOT) != 0 &&
		MM_FreezeTag_ClientIsActiveFighter(bot) &&
		bot->client->sess.team == team;
}

bool BotCanRescueFrozen(gentity_t *bot, gentity_t *frozen)
{
	return frozen && frozen->client &&
		frozen != bot &&
		RawFrozen(frozen) &&
		ClientIsPlaying(frozen->client) &&
		!frozen->client->eliminated &&
		IsFreezeTeam(frozen->client->sess.team) &&
		BotCanRescueTeam(bot, frozen->client->sess.team);
}

bool BotAssignmentIsValid(gentity_t *frozen, const freeze_state_t &state)
{
	gentity_t *bot = EntityFromNumber(state.bot_rescuer_entnum);
	return BotCanRescueFrozen(bot, frozen);
}

void ClearBotRescueAssignmentsFor(const gentity_t *ent)
{
	if (!ent || ent->s.number <= 0)
		return;

	for (freeze_state_t &state : s_freeze) {
		if (state.bot_rescuer_entnum == ent->s.number)
			state.bot_rescuer_entnum = kNoThawer;
	}
}

float BotRescueTolerance()
{
	return clamp(ThawRadius() * 0.16f, 4.0f, 16.0f);
}

float BotRescueGoalDistance()
{
	const float radius = ThawRadius();
	return clamp(radius * 0.45f, 24.0f, max(24.0f, radius - BotRescueTolerance()));
}

float BotRescueHoldTolerance()
{
	return clamp(ThawRadius() * 0.1f, 4.0f, 12.0f);
}

bool BotRescueGoalIsUsable(gentity_t *frozen, const vec3_t &goal)
{
	const trace_t body = gi.trace(goal, PLAYER_MINS, PLAYER_MAXS, goal, frozen, MASK_PLAYERSOLID);
	if (body.startsolid || body.allsolid)
		return false;

	vec3_t start = goal;
	start[2] += 22.0f;
	vec3_t end = frozen->s.origin;
	end[2] += 16.0f;

	const trace_t sight = gi.trace(start, vec3_origin, vec3_origin, end, frozen, MASK_SOLID);
	return sight.fraction == 1.0f;
}

vec3_t BotRescueGoal(gentity_t *bot, gentity_t *frozen)
{
	const float distance = BotRescueGoalDistance();
	std::array<vec3_t, 9> dirs = {
		bot->s.origin - frozen->s.origin,
		vec3_t{ 1, 0, 0 },
		vec3_t{ -1, 0, 0 },
		vec3_t{ 0, 1, 0 },
		vec3_t{ 0, -1, 0 },
		vec3_t{ 0.707f, 0.707f, 0 },
		vec3_t{ 0.707f, -0.707f, 0 },
		vec3_t{ -0.707f, 0.707f, 0 },
		vec3_t{ -0.707f, -0.707f, 0 },
	};

	for (vec3_t dir : dirs) {
		dir[2] = 0;
		if (dir.lengthSquared() < 1.0f)
			continue;

		dir.normalize();
		const vec3_t goal = frozen->s.origin + dir * distance;
		if (BotRescueGoalIsUsable(frozen, goal))
			return goal;
	}

	return frozen->s.origin;
}

bool AssignedBotIsThawing(gentity_t *frozen, const freeze_state_t &state)
{
	gentity_t *bot = EntityFromNumber(state.bot_rescuer_entnum);
	return BotCanRescueFrozen(bot, frozen) && IsThawer(frozen, bot);
}

void ClearBotAttackIntent(gentity_t *bot)
{
	if (!bot || !bot->client)
		return;

	bot->client->buttons &= ~BUTTON_ATTACK;
	bot->client->latched_buttons &= ~BUTTON_ATTACK;
	bot->client->weapon_fire_buffered = false;
}

void ClearBotRescueCombatFocus(gentity_t *bot)
{
	if (!bot)
		return;

	ClearBotAttackIntent(bot);
	bot->enemy = nullptr;
	bot->oldenemy = nullptr;
}

void ClearBotInvalidFrozenFocus(gentity_t *bot)
{
	if (!bot)
		return;

	bool cleared = false;

	if (RawFrozen(bot->enemy)) {
		bot->enemy = nullptr;
		cleared = true;
	}

	if (RawFrozen(bot->oldenemy)) {
		bot->oldenemy = nullptr;
		cleared = true;
	}

	if (RawFrozen(bot->goalentity) && !BotCanRescueFrozen(bot, bot->goalentity)) {
		bot->goalentity = nullptr;
		cleared = true;
	}

	if (RawFrozen(bot->movetarget) && !BotCanRescueFrozen(bot, bot->movetarget)) {
		bot->movetarget = nullptr;
		cleared = true;
	}

	if (cleared)
		ClearBotAttackIntent(bot);
}

gentity_t *FindBotRescueTarget(gentity_t *bot)
{
	if (!bot || !bot->client || !MM_FreezeTag_ClientIsActiveFighter(bot))
		return nullptr;

	gentity_t *fallback = nullptr;
	float fallback_dist = 0.0f;
	gentity_t *best = nullptr;
	float best_dist = 0.0f;

	for (auto ec : active_clients()) {
		if (!BotCanRescueFrozen(bot, ec))
			continue;

		freeze_state_t *state = StateFor(ec);
		if (!state)
			continue;

		const bool assigned_here = state->bot_rescuer_entnum == bot->s.number;
		const bool assigned_bot_thawing = AssignedBotIsThawing(ec, *state);
		if (!assigned_here && BotAssignmentIsValid(ec, *state) && !assigned_bot_thawing)
			continue;

		const float dist = (ec->s.origin - bot->s.origin).lengthSquared();
		const bool already_thawing = IsThawer(ec, ThawerFromState(*state));

		if (assigned_here)
			return ec;

		if (assigned_bot_thawing) {
			if (!fallback || dist < fallback_dist) {
				fallback = ec;
				fallback_dist = dist;
			}
			continue;
		}

		if (already_thawing) {
			if (!fallback || dist < fallback_dist) {
				fallback = ec;
				fallback_dist = dist;
			}
			continue;
		}

		if (!best || dist < best_dist) {
			best = ec;
			best_dist = dist;
		}
	}

	return best ? best : fallback;
}

void ApplyFrozenShellToGib(gentity_t *gib)
{
	if (!gib)
		return;

	gib->s.effects |= EF_COLOR_SHELL;
	gib->s.renderfx |= kFrozenShell;
}

void ThrowFrozenThawGibs(gentity_t *ent)
{
	if (!ent)
		return;

	static constexpr int kThawGibDamage = 80;
	static constexpr const char *kMeatGib = "models/objects/gibs/sm_meat/tris.md2";
	static constexpr const char *kBoneGib = "models/objects/gibs/bone/tris.md2";

	for (int i = 0; i < 6; ++i)
		ApplyFrozenShellToGib(ThrowGib(ent, kMeatGib, kThawGibDamage, GIB_NONE, 1.0f));
	for (int i = 0; i < 2; ++i)
		ApplyFrozenShellToGib(ThrowGib(ent, kBoneGib, kThawGibDamage, GIB_NONE, 1.0f));
}

void ClearFrozenVisuals(gentity_t *ent)
{
	if (!ent)
		return;

	ent->s.renderfx &= ~kFreezeShellMask;
	ent->s.effects &= ~EF_COLOR_SHELL;
}

renderfx_t FrozenShellFor(const freeze_state_t &state)
{
	if (state.thawer_entnum != kNoThawer &&
		((level.time.milliseconds() / kThawPulseInterval.milliseconds()) & 1) != 0)
		return kThawingShell;

	return kFrozenShell;
}

void ApplyFrozenShell(gentity_t *ent, const freeze_state_t &state)
{
	ent->s.effects |= EF_COLOR_SHELL;
	ent->s.renderfx &= ~kFreezeShellMask;
	ent->s.renderfx |= FrozenShellFor(state);
}

void ClearFrozenHealthHudStats(gclient_t *client)
{
	if (!client)
		return;

	client->ps.stats[STAT_HEALTH_ICON] = 0;
	client->ps.stats[STAT_HEALTH] = 0;
	client->ps.stats[STAT_HEALTH_BARS] = 0;
}

gentity_t *EnsureFrozenViewProxy(gentity_t *ent, freeze_state_t &state)
{
	gentity_t *proxy = FrozenViewProxyFromState(ent, state);
	if (proxy)
		return proxy;

	proxy = G_Spawn();
	if (!proxy)
		return nullptr;

	proxy->classname = kFrozenViewProxyClassname;
	proxy->owner = ent;
	proxy->movetype = MOVETYPE_NONE;
	proxy->solid = SOLID_NOT;
	proxy->clipmask = CONTENTS_NONE;
	proxy->svflags |= SVF_INSTANCED;
	proxy->takedamage = false;
	proxy->s.modelindex = MODELINDEX_PLAYER;
	proxy->s.modelindex2 = 0;
	proxy->s.modelindex3 = 0;
	proxy->s.modelindex4 = 0;
	state.view_proxy_entnum = proxy->s.number;
	return proxy;
}

void SyncFrozenViewProxy(gentity_t *ent, freeze_state_t &state)
{
	if (!ent || !ent->client)
		return;

	gentity_t *proxy = EnsureFrozenViewProxy(ent, state);
	if (!proxy)
		return;

	proxy->owner = ent;
	proxy->svflags |= SVF_INSTANCED;
	proxy->svflags &= ~SVF_NOCLIENT;
	proxy->movetype = MOVETYPE_NONE;
	proxy->solid = SOLID_NOT;
	proxy->clipmask = CONTENTS_NONE;
	proxy->mins = {};
	proxy->maxs = {};
	proxy->s.origin = ent->s.origin;
	proxy->s.old_origin = ent->s.origin;
	proxy->s.angles = state.body_angles;
	proxy->s.modelindex = MODELINDEX_PLAYER;
	proxy->s.modelindex2 = 0;
	proxy->s.modelindex3 = 0;
	proxy->s.modelindex4 = 0;
	proxy->s.frame = FRAME_stand01;
	proxy->s.old_frame = FRAME_stand01;
	proxy->s.skinnum = ent->s.skinnum;
	proxy->s.effects = (ent->s.effects & ~EF_COLOR_SHELL);
	proxy->s.renderfx = (ent->s.renderfx & ~(kFreezeShellMask | RF_VIEWERMODEL | RF_WEAPONMODEL));
	proxy->s.solid = 0;
	proxy->s.sound = 0;
	proxy->s.event = EV_NONE;
	proxy->s.alpha = ent->s.alpha;
	proxy->s.scale = ent->s.scale;
	proxy->s.owner = 0;
	proxy->s.loop_volume = 0;
	proxy->s.loop_attenuation = 0;
	ApplyFrozenShell(proxy, state);

	gi.linkentity(proxy);
}

preserved_loadout_t PreserveLoadout(const gclient_t *client)
{
	preserved_loadout_t out;
	if (!client)
		return out;

	out.selected_item = client->pers.selected_item;
	out.selected_item_time = client->pers.selected_item_time;
	out.inventory = client->pers.inventory;
	out.max_ammo = client->pers.max_ammo;
	out.weapon = client->pers.weapon;
	out.lastweapon = client->pers.lastweapon;
	out.power_cubes = client->pers.power_cubes;
	return out;
}

bool InventoryContainsWeapon(const gclient_t *client, const gitem_t *weapon)
{
	if (!client || !weapon)
		return false;

	const item_id_t id = weapon->id;
	return id > IT_NULL && id < IT_TOTAL && client->pers.inventory[id] > 0;
}

gitem_t *FirstAvailableWeapon(gclient_t *client)
{
	if (!client)
		return nullptr;

	for (int id = FIRST_WEAPON; id <= LAST_WEAPON; ++id) {
		gitem_t *item = &itemlist[id];
		if ((item->flags & IF_WEAPON) && client->pers.inventory[id] > 0)
			return item;
	}

	return nullptr;
}

void RestoreLoadout(gentity_t *ent, const preserved_loadout_t &loadout)
{
	if (!ent || !ent->client)
		return;

	gclient_t *client = ent->client;
	client->pers.selected_item = loadout.selected_item;
	client->pers.selected_item_time = loadout.selected_item_time;
	client->pers.inventory = loadout.inventory;
	client->pers.max_ammo = loadout.max_ammo;
	client->pers.power_cubes = loadout.power_cubes;

	if (InventoryContainsWeapon(client, loadout.weapon))
		client->pers.weapon = loadout.weapon;
	if (InventoryContainsWeapon(client, loadout.lastweapon))
		client->pers.lastweapon = loadout.lastweapon;

	if (!InventoryContainsWeapon(client, client->pers.weapon)) {
		if (InventoryContainsWeapon(client, client->pers.lastweapon))
			client->pers.weapon = client->pers.lastweapon;
		else if (gitem_t *weapon = FirstAvailableWeapon(client))
			client->pers.weapon = weapon;
		else {
			client->pers.inventory[IT_WEAPON_BLASTER] = 1;
			client->pers.weapon = &itemlist[IT_WEAPON_BLASTER];
		}
	}

	client->newweapon = client->pers.weapon;
	Change_Weapon(ent);
}

void RespawnRoundPlayer(gentity_t *ent, bool preserve_loadout)
{
	if (!ent || !ent->client)
		return;

	const preserved_loadout_t loadout = preserve_loadout ? PreserveLoadout(ent->client) : preserved_loadout_t {};

	ent->s.modelindex = 0;
	ent->client->eliminated = false;
	ent->client->respawn_min_time = 0_ms;
	ent->client->respawn_time = level.time;
	ClientRespawn(ent);

	if (preserve_loadout && !ent->client->awaiting_respawn)
		RestoreLoadout(ent, loadout);
}

void StripFrozenInventory(gentity_t *ent, const mod_t &mod)
{
	gclient_t *client = ent->client;

	client->pu_time_quad = 0_ms;
	client->pu_time_haste = 0_ms;
	client->pu_time_double = 0_ms;
	client->pu_time_protection = 0_ms;
	client->pu_time_invisibility = 0_ms;
	client->pu_time_regeneration = 0_ms;
	client->pu_time_rebreather = 0_ms;
	client->pu_time_enviro = 0_ms;
	ent->flags &= ~FL_POWER_ARMOR;

	if (client->tracker_pain_time)
		RemoveAttackingPainDaemons(ent);

	if (gentity_t *sphere = G_ResolveOwnedSphere(client)) {
		const int32_t sphere_generation = sphere->spawn_count;
		if (sphere->die)
			sphere->die(sphere, ent, ent, 0, vec3_origin, mod);
		else if (sphere->inuse && sphere->spawn_count == sphere_generation)
			G_FreeEntity(sphere);
		G_ClearOwnedSphere(client);
	}

	client->pers.inventory.fill(0);
	client->newweapon = nullptr;
	client->pers.weapon = nullptr;
	client->pers.lastweapon = nullptr;
	client->pers.selected_item = IT_NULL;
	client->weapon_sound = 0;
	client->weapon_thunk = false;
	client->weapon_fire_buffered = false;
	client->ps.gunindex = 0;
	client->ps.gunskin = 0;
	ent->s.modelindex2 = 0;
	ent->s.modelindex3 = 0;
}

void ApplyFrozenState(gentity_t *ent, freeze_state_t &state)
{
	gclient_t *client = ent->client;

	ent->health = 1;
	ent->client->pers.health = 1;
	ent->deadflag = false;
	ent->takedamage = true;
	ent->movetype = MOVETYPE_TOSS;
	ent->solid = SOLID_BBOX;
	ent->clipmask = MASK_PLAYERSOLID;
	ent->svflags &= ~(SVF_NOCLIENT | SVF_DEADMONSTER);
	ent->svflags |= SVF_PLAYER;
	ent->flags &= ~(FL_NO_KNOCKBACK | FL_ALIVE_KNOCKBACK_ONLY | FL_NO_DAMAGE_EFFECTS);
	ent->flags |= FL_NOTARGET;
	ent->air_finished = level.time + 12_sec;

	ent->mins = PLAYER_MINS;
	ent->maxs = PLAYER_MAXS;
	if (state.crouched)
		ent->maxs[2] = 4;
	ent->s.angles = state.body_angles;

	client->ps.pmove.pm_type = PM_FREEZE;
	client->ps.pmove.origin = ent->s.origin;
	client->ps.pmove.velocity = ent->velocity;
	client->ps.pmove.pm_flags &= ~(PMF_TIME_TELEPORT | PMF_TIME_WATERJUMP | PMF_TIME_LAND);
	if (state.crouched)
		client->ps.pmove.pm_flags |= PMF_DUCKED;
	client->ps.stats[STAT_SHOW_STATUSBAR] = 1;
	ClearFrozenHealthHudStats(client);

	// Lock the model on an upright player frame; the death priority prevents
	// G_SetClientFrame from advancing it into run/jump/corpse animations.
	client->anim_priority = ANIM_DEATH;
	client->anim_end = FRAME_stand01;
	ent->s.frame = FRAME_stand01;
	ApplyFrozenShell(ent, state);

	G_ClearLagCompensationHistory(ent);
	gi.linkentity(ent);
}

void FrozenPlayerBounds(bool crouched, vec3_t &mins, vec3_t &maxs)
{
	mins = PLAYER_MINS;
	maxs = PLAYER_MAXS;
	if (crouched)
		maxs[2] = 4;
}

contents_t ThawPlacementMask()
{
	contents_t mask = MASK_PLAYERSOLID | CONTENTS_LAVA | CONTENTS_SLIME;
	if (!G_ShouldPlayersCollide(false))
		mask &= ~CONTENTS_PLAYER;
	return mask;
}

bool IsSafeThawOrigin(gentity_t *ent, const vec3_t &origin, bool crouched)
{
	vec3_t mins, maxs;
	FrozenPlayerBounds(crouched, mins, maxs);

	const contents_t mask = ThawPlacementMask();
	const trace_t tr = gi.trace(origin, mins, maxs, origin, ent, mask);
	if (tr.startsolid || tr.allsolid || (tr.contents & (CONTENTS_LAVA | CONTENTS_SLIME)))
		return false;

	const contents_t body = gi.pointcontents(origin + vec3_t{ 0, 0, 8 });
	const contents_t head = gi.pointcontents(origin + vec3_t{ 0, 0, 22 });
	return !(body & (CONTENTS_SOLID | CONTENTS_LAVA | CONTENTS_SLIME)) &&
		!(head & (CONTENTS_SOLID | CONTENTS_LAVA | CONTENTS_SLIME));
}

bool TryDropToSafeThawOrigin(gentity_t *ent, const vec3_t &base, bool crouched, vec3_t &out)
{
	vec3_t mins, maxs;
	FrozenPlayerBounds(crouched, mins, maxs);

	const vec3_t start = base + vec3_t{ 0, 0, kThawDropUp };
	const vec3_t end = base - vec3_t{ 0, 0, kThawDropDown };
	const trace_t tr = gi.trace(start, mins, maxs, end, ent, ThawPlacementMask());

	if (tr.startsolid || tr.allsolid || tr.fraction == 1.0f || (tr.contents & (CONTENTS_LAVA | CONTENTS_SLIME)))
		return false;
	if (tr.plane.normal.z < 0.7f)
		return false;

	const vec3_t candidate = tr.endpos + vec3_t{ 0, 0, 1 };
	if (!IsSafeThawOrigin(ent, candidate, crouched))
		return false;

	out = candidate;
	return true;
}

bool FindSafeThawOrigin(gentity_t *ent, gentity_t *thawer, bool crouched, vec3_t &out)
{
	if (IsSafeThawOrigin(ent, ent->s.origin, crouched)) {
		out = ent->s.origin;
		return true;
	}

	static constexpr float offsets[][3] = {
		{ 48, 0, 0 },
		{ -48, 0, 0 },
		{ 0, 48, 0 },
		{ 0, -48, 0 },
		{ 34, 34, 0 },
		{ 34, -34, 0 },
		{ -34, 34, 0 },
		{ -34, -34, 0 },
		{ 96, 0, 0 },
		{ -96, 0, 0 },
		{ 0, 96, 0 },
		{ 0, -96, 0 },
	};

	for (const auto &offset : offsets) {
		const vec3_t base = ent->s.origin + vec3_t{ offset[0], offset[1], offset[2] };
		if (TryDropToSafeThawOrigin(ent, base, crouched, out))
			return true;
	}

	if (thawer && thawer->client) {
		if (TryDropToSafeThawOrigin(ent, thawer->s.origin, crouched, out))
			return true;

		for (const auto &offset : offsets) {
			const vec3_t base = thawer->s.origin + vec3_t{ offset[0], offset[1], offset[2] };
			if (TryDropToSafeThawOrigin(ent, base, crouched, out))
				return true;
		}
	}

	return false;
}

float AngleDelta(float current, float previous)
{
	float delta = current - previous;
	while (delta > 180.0f)
		delta -= 360.0f;
	while (delta < -180.0f)
		delta += 360.0f;
	return delta;
}

float SignedPitch(float pitch)
{
	pitch = anglemod(pitch);
	return pitch > 180.0f ? pitch - 360.0f : pitch;
}

void ClampFrozenOrbitAngles(vec3_t &angles)
{
	angles[PITCH] = anglemod(clamp(SignedPitch(angles[PITCH]), -80.0f, 80.0f));
	angles[YAW] = anglemod(angles[YAW]);
	angles[ROLL] = 0.0f;
}

vec3_t UpdateFrozenOrbitAngles(freeze_state_t &state, const usercmd_t *ucmd)
{
	if (!ucmd)
		return state.orbit_angles;

	if (!state.has_last_cmd_angles) {
		state.last_cmd_angles = ucmd->angles;
		state.has_last_cmd_angles = true;
		ClampFrozenOrbitAngles(state.orbit_angles);
		return state.orbit_angles;
	}

	state.orbit_angles[PITCH] += AngleDelta(ucmd->angles[PITCH], state.last_cmd_angles[PITCH]);
	state.orbit_angles[YAW] += AngleDelta(ucmd->angles[YAW], state.last_cmd_angles[YAW]);
	ClampFrozenOrbitAngles(state.orbit_angles);
	state.last_cmd_angles = ucmd->angles;
	return state.orbit_angles;
}

void PadFrozenCameraAgainstSolid(gentity_t *ent, vec3_t &goal, const vec3_t &axis)
{
	if (axis.lengthSquared() < 0.001f)
		return;

	trace_t trace = gi.traceline(goal, goal + (axis * kFrozenCameraClipPadding), ent, MASK_SOLID);
	if (trace.fraction < 1.0f)
		goal = trace.endpos - (axis * kFrozenCameraClipPadding);

	trace = gi.traceline(goal, goal - (axis * kFrozenCameraClipPadding), ent, MASK_SOLID);
	if (trace.fraction < 1.0f)
		goal = trace.endpos + (axis * kFrozenCameraClipPadding);
}

vec3_t FrozenCameraOrigin(gentity_t *ent, const freeze_state_t &state)
{
	vec3_t angles = state.orbit_angles;

	vec3_t forward, right;
	AngleVectors(angles, forward, right, nullptr);
	forward.normalize();
	right.normalize();

	vec3_t target = ent->s.origin;
	target[2] += state.crouched ? 12.0f : kFrozenCameraTargetHeight;

	const vec3_t desired = target + (forward * -kFrozenCameraDistance);
	trace_t trace = gi.traceline(target, desired, ent, MASK_SOLID);
	vec3_t goal = trace.endpos + (forward * kFrozenCameraWallInset);

	// Same idea as the follow camera: pull inward on the main trace, then
	// keep a little breathing room from floors, ceilings, and side walls.
	PadFrozenCameraAgainstSolid(ent, goal, vec3_t{ 0, 0, 1 });
	PadFrozenCameraAgainstSolid(ent, goal, right);

	trace = gi.traceline(target, goal, ent, MASK_SOLID);
	if (trace.fraction < 1.0f)
		goal = trace.endpos + (forward * kFrozenCameraWallInset);

	return goal;
}

void ApplyFrozenCamera(gentity_t *ent, const freeze_state_t &state)
{
	gclient_t *client = ent->client;
	client->ps.pmove.pm_type = PM_SPECTATOR;
	client->ps.pmove.origin = FrozenCameraOrigin(ent, state);
	client->ps.pmove.velocity = {};
	client->ps.pmove.viewheight = 0;
	client->ps.pmove.pm_flags |= PMF_NO_POSITIONAL_PREDICTION;
	client->ps.pmove.pm_flags &= ~(PMF_NO_ANGULAR_PREDICTION | PMF_TIME_TELEPORT | PMF_TIME_WATERJUMP | PMF_TIME_LAND);
	client->ps.pmove.delta_angles = state.orbit_angles - client->resp.cmd_angles;
	client->ps.viewoffset = {};
	client->ps.viewangles = state.orbit_angles;
	client->ps.kick_angles = {};
	client->ps.gunangles = {};
	client->ps.gunoffset = {};
	client->ps.gunindex = 0;
	client->ps.gunskin = 0;
	client->ps.gunframe = 0;
	client->ps.gunrate = 0;
}

gentity_t *ClientEntityForIndex(int index)
{
	if (index < 0 || index >= static_cast<int>(MAX_CLIENTS) || index >= static_cast<int>(game.maxclients))
		return nullptr;

	gentity_t *ent = &g_entities[index + 1];
	return ent->inuse && ent->client ? ent : nullptr;
}

bool CanReceiveThawCredit(gentity_t *frozen, gentity_t *helper)
{
	return frozen && frozen->client &&
		helper && helper->client &&
		helper != frozen &&
		ClientIsPlaying(helper->client) &&
		helper->client->sess.team == frozen->client->sess.team &&
		IsFreezeTeam(helper->client->sess.team);
}

struct thaw_credit_t {
	gentity_t *primary = nullptr;
	std::array<gentity_t *, MAX_CLIENTS> assists = {};
	int assist_count = 0;
};

thaw_credit_t BuildThawCredit(gentity_t *frozen, const freeze_state_t &state, gentity_t *primary)
{
	thaw_credit_t credit;
	if (!CanReceiveThawCredit(frozen, primary))
		return credit;

	credit.primary = primary;
	const float thaw_seconds = ThawSeconds();
	const int limit = std::min(static_cast<int>(game.maxclients), static_cast<int>(MAX_CLIENTS));

	for (int i = 0; i < limit; ++i) {
		gentity_t *helper = ClientEntityForIndex(i);
		if (!helper || helper == credit.primary || !CanReceiveThawCredit(frozen, helper))
			continue;
		if (!MM_FreezeTagThawAssistQualifies(state.thaw_credit_seconds[static_cast<size_t>(i)], thaw_seconds))
			continue;

		credit.assists[static_cast<size_t>(credit.assist_count++)] = helper;
	}

	return credit;
}

void ResetThawedPlayerState(gentity_t *ent, const vec3_t &view_angles, bool crouched)
{
	gclient_t *client = ent->client;

	memset(&client->ps, 0, sizeof(client->ps));

	char fov[MAX_INFO_VALUE];
	if (gi.Info_ValueForKey(client->pers.userinfo, "fov", fov, sizeof(fov)))
		client->ps.fov = muffmode::player::ParseUserinfoFov(fov);
	else
		client->ps.fov = muffmode::player::kDefaultUserinfoFov;

	client->ps.pmove.pm_type = PM_NORMAL;
	client->ps.pmove.origin = ent->s.origin;
	client->ps.pmove.velocity = {};
	client->ps.pmove.viewheight = ent->viewheight;
	client->ps.pmove.delta_angles = view_angles - client->resp.cmd_angles;
	client->ps.viewangles = view_angles;
	client->v_angle = view_angles;
	AngleVectors(client->v_angle, client->v_forward, nullptr, nullptr);

	if (crouched)
		client->ps.pmove.pm_flags |= PMF_DUCKED;

	client->ps.stats[STAT_SHOW_STATUSBAR] = 1;
}

void AwardThawCredit(gentity_t *ent, const thaw_credit_t &credit)
{
	if (!ent || !ent->client)
		return;

	if (!credit.primary || !credit.primary->client) {
		gi.LocBroadcast_Print(PRINT_CHAT, "{} thawed automatically.\n", ent->client->resp.netname);
		return;
	}

	G_AdjustPlayerScore(credit.primary->client, 1, false, 0);
	if (credit.assist_count > 0) {
		gi.LocClient_Print(credit.primary, PRINT_CENTER, "You thawed {}!\n{} assist{}",
			ent->client->resp.netname,
			credit.assist_count,
			credit.assist_count == 1 ? "" : "s");
	} else {
		gi.LocClient_Print(credit.primary, PRINT_CENTER, "You thawed {}!", ent->client->resp.netname);
	}

	for (int i = 0; i < credit.assist_count; ++i) {
		gentity_t *helper = credit.assists[static_cast<size_t>(i)];
		if (!helper || !helper->client)
			continue;

		G_AdjustPlayerScore(helper->client, 1, false, 0);
		gi.LocClient_Print(helper, PRINT_CENTER, "Thaw assist: {}!", ent->client->resp.netname);
	}

	if (credit.assist_count == 1 && credit.assists[0] && credit.assists[0]->client) {
		gi.LocBroadcast_Print(PRINT_CHAT, "{} thawed {} with {} assisting.\n",
			credit.primary->client->resp.netname,
			ent->client->resp.netname,
			credit.assists[0]->client->resp.netname);
	} else if (credit.assist_count > 1) {
		gi.LocBroadcast_Print(PRINT_CHAT, "{} thawed {} with {} teammates assisting.\n",
			credit.primary->client->resp.netname,
			ent->client->resp.netname,
			credit.assist_count);
	} else {
		gi.LocBroadcast_Print(PRINT_CHAT, "{} thawed {}.\n",
			credit.primary->client->resp.netname,
			ent->client->resp.netname);
	}
}

void RespawnThawedPlayerAtSpawnPoint(gentity_t *ent, const freeze_state_t &state, gentity_t *thawer)
{
	if (!ent || !ent->client)
		return;

	const thaw_credit_t credit = BuildThawCredit(ent, state, thawer);

	gi.sound(ent, CHAN_BODY, gi.soundindex("world/brkglas.wav"), 1, ATTN_NORM, 0);
	ThrowFrozenThawGibs(ent);

	ent->s.modelindex = 0;
	ent->client->eliminated = false;
	ent->client->respawn_min_time = 0_ms;
	ent->client->respawn_time = level.time;
	ClientRespawn(ent);

	AwardThawCredit(ent, credit);
	gi.LocClient_Print(ent, PRINT_CENTER, "THAWED!");
}

void RestoreFrozenPlayerAt(gentity_t *ent, const freeze_state_t &state, gentity_t *thawer, const vec3_t &thaw_origin)
{
	if (!ent || !ent->client)
		return;

	const thaw_credit_t credit = BuildThawCredit(ent, state, thawer);
	team_t team = ent->client->sess.team;
	vec3_t origin = thaw_origin;
	vec3_t angles = ent->s.angles;
	vec3_t view_angles = ent->client->v_angle;
	const bool crouched = RawFrozen(ent) && StateFor(ent)->crouched;
	char social_id[MAX_INFO_VALUE];
	Q_strlcpy(social_id, ent->client->pers.social_id, sizeof(social_id));

	gi.sound(ent, CHAN_BODY, gi.soundindex("world/brkglas.wav"), 1, ATTN_NORM, 0);
	MM_FreezeTag_ClearClient(ent);
	InitClientPersistant(ent, ent->client);
	Q_strlcpy(ent->client->pers.social_id, social_id, sizeof(social_id));

	ent->client->sess.team = team;
	P_PublishEngineTeam(ent);
	FetchClientEntData(ent);

	ent->s.origin = origin;
	ent->s.angles = angles;
	ent->velocity = {};
	ent->avelocity = {};
	ent->groundentity = nullptr;
	ent->takedamage = true;
	ent->movetype = MOVETYPE_WALK;
	ent->viewheight = 22;
	ent->solid = SOLID_BBOX;
	ent->deadflag = false;
	ent->air_finished = level.time + 12_sec;
	ent->clipmask = MASK_PLAYERSOLID;
	ent->model = "players/male/tris.md2";
	ent->die = player_die;
	ent->waterlevel = WATER_NONE;
	ent->watertype = CONTENTS_NONE;
	ent->flags &= ~(FL_NO_KNOCKBACK | FL_ALIVE_KNOCKBACK_ONLY | FL_NO_DAMAGE_EFFECTS | FL_NOTARGET | FL_SAM_RAIMI);
	ent->svflags &= ~(SVF_NOCLIENT | SVF_DEADMONSTER);
	ent->svflags |= SVF_PLAYER;
	ent->mins = PLAYER_MINS;
	ent->maxs = PLAYER_MAXS;
	if (crouched)
		ent->maxs[2] = 4;
	if (!G_ShouldPlayersCollide(false))
		ent->clipmask &= ~CONTENTS_PLAYER;

	ent->client->eliminated = false;
	ent->client->respawn_min_time = 0_ms;
	ent->client->respawn_time = level.time;
	ent->client->pers.last_spawn_time = level.time;
	ResetThawedPlayerState(ent, view_angles, crouched);
	ent->client->weapon_thunk = false;
	ent->client->weapon_fire_buffered = false;

	ent->s.effects = EF_NONE;
	ent->s.modelindex = MODELINDEX_PLAYER;
	ent->s.modelindex2 = MODELINDEX_PLAYER;
	ent->s.modelindex3 = 0;
	P_AssignClientSkinnum(ent);

	if (ent->client->pers.weapon)
		ent->client->ps.gunindex = gi.modelindex(ent->client->pers.weapon->view_model);
	else
		ent->client->ps.gunindex = 0;
	ent->client->ps.gunskin = 0;

	const int thaw_weapon_id = MM_FreezeTagThawWeaponId(
		MM_UsesArenaSpawnLoadout(),
		ent->client->pers.inventory[IT_WEAPON_RLAUNCHER] != 0,
		ent->client->pers.weapon ? ent->client->pers.weapon->id : IT_NULL,
		IT_WEAPON_RLAUNCHER);
	ent->client->newweapon = thaw_weapon_id > IT_NULL && thaw_weapon_id < IT_TOTAL ? &itemlist[thaw_weapon_id] : nullptr;
	Change_Weapon(ent);

	G_ClearLagCompensationHistory(ent);
	gi.linkentity(ent);
	G_PostRespawn(ent);
	MM_MatchStats_RecordSpawn(ent->client);

	AwardThawCredit(ent, credit);
	gi.LocClient_Print(ent, PRINT_CENTER, "THAWED!");
}

bool CompleteThaw(gentity_t *ent, freeze_state_t &state, gentity_t *thawer)
{
	if (!ThawedPlayersRespawnAtThawLocation()) {
		RespawnThawedPlayerAtSpawnPoint(ent, state, thawer);
		return true;
	}

	const bool crouched = state.crouched;
	vec3_t safe_origin;
	if (FindSafeThawOrigin(ent, thawer, crouched, safe_origin)) {
		RestoreFrozenPlayerAt(ent, state, thawer, safe_origin);
		return true;
	}

	if (state.next_thaw_feedback <= level.time) {
		gi.LocClient_Print(ent, PRINT_CENTER, "FROZEN\nCannot thaw here.\nMove onto open ground.");
		if (thawer && thawer->client)
			gi.LocClient_Print(thawer, PRINT_CENTER, "Cannot thaw {}\nhere. Move the body to open ground.", ent->client->resp.netname);
		state.next_thaw_feedback = level.time + 1_sec;
	}

	return false;
}

void UpdateFrozenNotice(gentity_t *ent, freeze_state_t &state)
{
	if (state.next_notice > level.time)
		return;

	state.next_notice = level.time + kFrozenNoticeInterval;
	gi.LocClient_Print(ent, PRINT_CENTER, "FROZEN\nA teammate must stand near you to thaw you.\nAttack: call for help");
}

void ResetThawProgress(freeze_state_t &state)
{
	state.thaw_started = 0_ms;
	state.thaw_last_update = 0_ms;
	state.thaw_progress_seconds = 0.0f;
	state.thaw_credit_seconds.fill(0.0f);
	state.thawer_entnum = kNoThawer;
	state.thawer_count = 0;
	state.next_thaw_feedback = 0_ms;
}

std::string ThawerSummary(gentity_t *primary, int thawer_count)
{
	if (!primary || !primary->client)
		return "A teammate";

	if (thawer_count <= 1)
		return primary->client->resp.netname;

	return std::string(G_Fmt("{} +{}", primary->client->resp.netname, thawer_count - 1));
}

void SendThawProgressFeedback(gentity_t *ent, gentity_t *primary, int thawer_count, int pct)
{
	const std::string thawers = ThawerSummary(primary, thawer_count);
	gi.LocClient_Print(ent, PRINT_CENTER, "FROZEN\n{} thawing... {}%", thawers.c_str(), pct);

	for (auto ec : active_clients()) {
		if (!IsThawer(ent, ec))
			continue;

		gi.LocClient_Print(ec, PRINT_CENTER, ".Thawing {}\n{}%", ent->client->resp.netname, pct);
	}
}

void UpdateThaw(gentity_t *ent, freeze_state_t &state)
{
	const thaw_scan_t scan = FindThawers(ent);
	gentity_t *thawer = scan.primary;

	if (!thawer) {
		ResetThawProgress(state);
		UpdateFrozenNotice(ent, state);
		return;
	}

	if (!state.thaw_started) {
		state.thaw_started = level.time;
		state.thaw_last_update = level.time;
		state.next_thaw_feedback = 0_ms;
		gi.sound(ent, CHAN_BODY, gi.soundindex("world/steam3.wav"), 1, ATTN_NORM, 0);
	}

	state.thawer_entnum = thawer->s.number;
	state.thawer_count = scan.count;

	const float delta = max(0.0f, (level.time - state.thaw_last_update).seconds());
	state.thaw_last_update = level.time;
	RecordThawContributors(state, scan, delta);
	state.thaw_progress_seconds += delta * MM_FreezeTagThawRate(scan.count, MultiThawScale());

	const float needed = ThawSeconds();
	if (state.thaw_progress_seconds >= needed) {
		CompleteThaw(ent, state, thawer);
		return;
	}

	if (state.next_thaw_feedback <= level.time) {
		const int pct = max(1, MM_FreezeTagThawProgressPercent(state.thaw_progress_seconds, needed));
		SendThawProgressFeedback(ent, thawer, scan.count, pct);
		state.next_thaw_feedback = level.time + 1_sec;
	}
}

mm_freezetag_team_counts_t CountTeam(team_t team)
{
	mm_freezetag_team_counts_t counts;

	for (auto ec : active_clients()) {
		// A same-slot reconnect remains represented by its reservation while the
		// transactional restore is pending; count it exactly once below using the
		// saved team/elimination state.
		if (MM_Ghost_IsPendingRestore(ec))
			continue;
		if (!ec->client || ec->client->sess.team != team)
			continue;
		if (!MM_FreezeTagClientCountsForRound(
				ClientIsPlaying(ec->client),
				IsFreezeTeam(ec->client->sess.team),
				ec->client->eliminated))
			continue;

		counts.participants++;
		if (MM_FreezeTag_ClientIsActiveFighter(ec)) {
			counts.active++;
			counts.total_health += max(ec->health, 0);
		}
	}

	for (uint32_t i = 1; i <= game.maxclients; ++i) {
		gentity_t *slot = &g_entities[i];
		if (!slot->inuse || !slot->client)
			continue;
		if (!MM_Ghost_ReservedClientCountsForRound(slot, team) || !RawFrozen(slot))
			continue;

		counts.participants++;
	}

	return counts;
}

mm_freezetag_hud_team_t HudTeamFor(gentity_t *viewer)
{
	if (!viewer || !viewer->client)
		return mm_freezetag_hud_team_t::Neutral;

	gentity_t *pov = viewer;
	if (!ClientIsPlaying(viewer->client) && viewer->client->follow_target && viewer->client->follow_target->client)
		pov = viewer->client->follow_target;

	switch (pov->client->sess.team) {
	case TEAM_RED:
		return mm_freezetag_hud_team_t::Red;
	case TEAM_BLUE:
		return mm_freezetag_hud_team_t::Blue;
	default:
		return mm_freezetag_hud_team_t::Neutral;
	}
}

bool ShouldReceiveFrozenHelp(gentity_t *frozen, gentity_t *recipient)
{
	if (!frozen || !frozen->client || !recipient || !recipient->client)
		return false;
	if (!IsFreezeTeam(frozen->client->sess.team))
		return false;

	if (ClientIsPlaying(recipient->client))
		return recipient->client->sess.team == frozen->client->sess.team;

	gentity_t *follow = recipient->client->follow_target;
	return follow && follow->client && follow->client->sess.team == frozen->client->sess.team;
}

std::string BuildFrozenHelpMessage(gentity_t *ent, const freeze_state_t &state)
{
	const int thaw_pct = state.thaw_started
		? MM_FreezeTagThawProgressPercent(state.thaw_progress_seconds, ThawSeconds())
		: 0;

	std::string location;
	const bool has_location = MM_LocLabelForEntity(ent, location);
	const std::string where = has_location ? std::string(G_Fmt(" at [{}]", location.c_str())) : std::string();

	if (thaw_pct > 0)
		return std::string(G_Fmt("[TEAM]: {} is thawing{} ({}%).\n", ent->client->resp.netname, where.c_str(), thaw_pct));

	return std::string(G_Fmt("[TEAM]: {} is frozen{} and needs a thaw!\n", ent->client->resp.netname, where.c_str()));
}

void SendFrozenHelpCall(gentity_t *ent, const freeze_state_t &state)
{
	if (!ent || !ent->client)
		return;

	const std::string message = BuildFrozenHelpMessage(ent, state);
	const uint32_t key = GetUnicastKey();
	const int marker_icon = gi.imageindex("i_help");

	for (auto recipient : active_clients()) {
		if (!ShouldReceiveFrozenHelp(ent, recipient))
			continue;

		if (recipient != ent) {
			gi.WriteByte(svc_poi);
			gi.WriteShort(POI_PING + (ent->s.number - 1));
			gi.WriteShort(5000);
			gi.WritePosition(ent->s.origin);
			gi.WriteShort(marker_icon);
			gi.WriteByte(215);
			gi.WriteByte(POI_FLAG_NONE);
			gi.unicast(recipient, false);
			gi.local_sound(recipient, CHAN_AUTO, gi.soundindex("misc/help_marker.wav"), 1.0f, ATTN_NONE, 0.0f, key);
		}

		gi.LocClient_Print(recipient, PRINT_CHAT, "{}", message.c_str());
	}
}

void AwardRound(team_t team, const char *reason)
{
	G_AdjustTeamScore(team, 1);
	gi.LocBroadcast_Print(PRINT_CENTER, "{} wins the round!\n({})\n", Teams_TeamName(team), reason);
	MM_AnnounceRaw(world, team == TEAM_BLUE ? "blue_wins_round" : "red_wins_round", "ctf/flagcap.wav", team == TEAM_BLUE);
	Round_End();
}

bool ResolveRoundFromCounts(const mm_freezetag_team_counts_t &red, const mm_freezetag_team_counts_t &blue, bool time_expired)
{
	switch (MM_FreezeTagResolveRound(red, blue, time_expired)) {
	case mm_freezetag_round_result_t::Continue:
		return false;
	case mm_freezetag_round_result_t::BlueWinsByFreeze:
		AwardRound(TEAM_BLUE, "froze Red");
		return true;
	case mm_freezetag_round_result_t::RedWinsByFreeze:
		AwardRound(TEAM_RED, "froze Blue");
		return true;
	case mm_freezetag_round_result_t::Draw:
		gi.LocBroadcast_Print(PRINT_CENTER, "Round draw!");
		Round_End();
		return true;
	case mm_freezetag_round_result_t::RedWinsByActiveCount:
		AwardRound(TEAM_RED, "more players active");
		return true;
	case mm_freezetag_round_result_t::BlueWinsByActiveCount:
		AwardRound(TEAM_BLUE, "more players active");
		return true;
	case mm_freezetag_round_result_t::RedWinsByHealth:
		AwardRound(TEAM_RED, "more health remaining");
		return true;
	case mm_freezetag_round_result_t::BlueWinsByHealth:
		AwardRound(TEAM_BLUE, "more health remaining");
		return true;
	}

	return false;
}

} // namespace

bool MM_FreezeTag_IsActive()
{
	return deathmatch->integer && GT(GT_FREEZE);
}

bool MM_FreezeTag_IsFrozen(const gentity_t *ent)
{
	return RawFrozen(ent);
}

bool MM_FreezeTag_IsFrozenViewProxy(const gentity_t *ent)
{
	return IsFrozenViewProxyEntity(ent);
}

bool MM_FreezeTag_IsFrozenViewProxyVisibleTo(const gentity_t *ent, const gentity_t *viewer)
{
	if (!IsFrozenViewProxyEntity(ent) || !viewer || !viewer->client)
		return false;

	return ent->owner == viewer && RawFrozen(viewer);
}

bool MM_FreezeTag_ShouldFreezeDeath(gentity_t *self, gentity_t *attacker, const mod_t &mod)
{
	const bool victim_has_client = self && self->client;
	const bool attacker_is_client = attacker && attacker->client;
	const bool attacker_is_victim = attacker == self;
	const bool attacker_same_team = victim_has_client && attacker_is_client && !attacker_is_victim && OnSameTeam(self, attacker);
	const bool same_team_death_should_freeze = g_friendly_fire->integer != 0 || mod.friendly_fire;

	return MM_FreezeTagDeathShouldFreeze(
		MM_FreezeTag_IsActive(),
		IsRoundLive(),
		victim_has_client && ClientIsPlaying(self->client),
		victim_has_client && IsFreezeTeam(self->client->sess.team),
		RawFrozen(self),
		victim_has_client && self->client->eliminated,
		attacker_is_client,
		attacker_is_victim,
		attacker_same_team,
		same_team_death_should_freeze);
}

void MM_FreezeTag_FreezePlayer(gentity_t *self, gentity_t *attacker, const mod_t &mod)
{
	if (!self || !self->client)
		return;

	freeze_state_t *state = StateFor(self);
	if (!state)
		return;

	FreeFrozenViewProxy(self, *state);
	*state = {};
	state->frozen = true;
	state->crouched = (self->client->ps.pmove.pm_flags & PMF_DUCKED) != 0;
	state->frozen_at = level.time;
	state->next_notice = level.time + kFrozenNoticeInterval;
	state->next_help = 0_ms;
	state->thawer_entnum = kNoThawer;
	state->body_angles = self->s.angles;
	state->body_angles[PITCH] = 0;
	state->body_angles[ROLL] = 0;
	state->orbit_angles = self->client->v_angle;
	ClampFrozenOrbitAngles(state->orbit_angles);

	StripFrozenInventory(self, mod);
	self->client->eliminated = false;
	ApplyFrozenState(self, *state);
	gi.sound(self, CHAN_BODY, gi.soundindex("boss3/d_hit.wav"), 1, ATTN_NORM, 0);
	CalculateRanks();

	if (attacker && attacker->client && attacker != self)
		gi.LocClient_Print(self, PRINT_CENTER, "FROZEN\n{} froze you!\nA teammate must stand near you.", attacker->client->resp.netname);
	else
		gi.LocClient_Print(self, PRINT_CENTER, "FROZEN\nA teammate must stand near you to thaw you.");
}

void MM_FreezeTag_ClearClient(gentity_t *ent)
{
	ClearBotRescueAssignmentsFor(ent);

	freeze_state_t *state = StateFor(ent);
	if (state) {
		FreeFrozenViewProxy(ent, *state);
		*state = {};
	}

	ClearFrozenVisuals(ent);

	if (ent && ent->client) {
		ent->flags &= ~FL_NOTARGET;
		ent->client->weapon_fire_buffered = false;
	}
}

void MM_FreezeTag_DetachWorldEntities()
{
	if (!MM_FreezeTag_IsActive())
		return;

	// Preserve the frozen/dead decision until MM_FreezeTag_ResetRoundPlayers
	// runs against the freshly rebuilt spawn cache, but release proxies before
	// their entity slots can be reused by map entities.
	for (auto ec : active_clients()) {
		if (freeze_state_t *state = StateFor(ec))
			FreeFrozenViewProxy(ec, *state);
	}
}

void MM_FreezeTag_OnRoundReset()
{
	if (!MM_FreezeTag_IsActive())
		return;

	for (auto ec : active_clients())
		MM_FreezeTag_ClearClient(ec);
}

void MM_FreezeTag_ResetRoundPlayers()
{
	if (!MM_FreezeTag_IsActive())
		return;

	const bool respawn_all = RoundRespawnsAll();
	const bool reset_alive_inventory = ResetAliveInventoryOnRoundRespawn();

	for (auto ec : active_clients()) {
		if (!ec->client || !ClientIsPlaying(ec->client))
			continue;

		const bool frozen = RawFrozen(ec);
		const bool waiting_or_dead = ec->client->eliminated || ec->deadflag || ec->health <= 0;
		// A survivor may keep their loadout and position between rounds, but the
		// restored entity string can put a reset mover or solid brush around that
		// position. Respawn only those survivors whose player hull is no longer
		// valid in the rebuilt world.
		const bool unsafe_survivor_position =
			!respawn_all && !frozen && !waiting_or_dead &&
			G_UnsafeSpawnPosition(ec->s.origin, false, ec, false);
		const bool respawn = respawn_all || frozen || waiting_or_dead || unsafe_survivor_position;
		if (!respawn)
			continue;

		const bool alive_survivor = !frozen && !waiting_or_dead;
		const bool preserve_loadout = alive_survivor && !reset_alive_inventory;
		RespawnRoundPlayer(ec, preserve_loadout);
	}
}

bool MM_FreezeTag_ShouldHoldSpawnForRound()
{
	return MM_FreezeTagSpawnShouldWaitForNextRound(
		MM_FreezeTag_IsActive(),
		level.match_state == matchst_t::MATCH_IN_PROGRESS,
		level.round_state == roundst_t::ROUND_IN_PROGRESS,
		level.round_state == roundst_t::ROUND_ENDED);
}

bool MM_FreezeTag_ShouldRespawnForRoundCountdown(gentity_t *ent)
{
	return MM_FreezeTagRoundCountdownShouldRespawn(
		MM_FreezeTag_IsActive(),
		level.match_state == matchst_t::MATCH_IN_PROGRESS,
		level.round_state == roundst_t::ROUND_COUNTDOWN,
		ent && ent->client && ClientIsPlaying(ent->client));
}

bool MM_FreezeTag_RunClientFrame(gentity_t *ent)
{
	if (!RawFrozen(ent))
		return false;

	if (!MM_FreezeTag_IsActive() || !ent->client || !ClientIsPlaying(ent->client)) {
		MM_FreezeTag_ClearClient(ent);
		return false;
	}

	freeze_state_t *state = StateFor(ent);
	if (!state)
		return false;

	if (!IsRoundLive()) {
		if (RoundKeepsFrozenPlayers()) {
			ent->velocity = {};
			ent->avelocity = {};
			ApplyFrozenState(ent, *state);
			return true;
		}

		MM_FreezeTag_ClearClient(ent);
		return false;
	}

	if (ent->health < 1)
		ent->health = 1;
	ent->client->pers.health = ent->health;
	ent->client->ps.pmove.pm_type = PM_FREEZE;
	ent->client->weapon_thunk = false;
	ent->client->weapon_fire_buffered = false;

	const gtime_t auto_thaw = AutoThawTime();
	if (auto_thaw > 0_ms && level.time >= state->frozen_at + auto_thaw) {
		if (CompleteThaw(ent, *state, nullptr))
			return false;

		ApplyFrozenState(ent, *state);
		G_RunEntity(ent);
		return true;
	}

	UpdateThaw(ent, *state);
	if (!RawFrozen(ent))
		return false;

	ApplyFrozenState(ent, *state);
	G_RunEntity(ent);
	return true;
}

bool MM_FreezeTag_ClientThink(gentity_t *ent, const usercmd_t *ucmd)
{
	if (!RawFrozen(ent))
		return false;
	if (!ent || !ent->client || !ucmd)
		return true;

	freeze_state_t *state = StateFor(ent);
	if (!state)
		return true;

	gclient_t *client = ent->client;
	client->v_angle = UpdateFrozenOrbitAngles(*state, ucmd);
	client->resp.cmd_angles = ucmd->angles;
	client->ps.pmove.delta_angles = client->v_angle - client->resp.cmd_angles;
	client->ps.viewangles = client->v_angle;
	AngleVectors(client->v_angle, client->v_forward, nullptr, nullptr);

	client->ps.pmove.pm_type = PM_SPECTATOR;
	client->ps.pmove.velocity = ent->velocity;
	client->ps.pmove.pm_flags |= PMF_NO_POSITIONAL_PREDICTION;
	client->ps.pmove.pm_flags &= ~PMF_NO_ANGULAR_PREDICTION;
	client->weapon_thunk = false;
	client->weapon_fire_buffered = false;

	if (client->latched_buttons & BUTTON_ATTACK) {
		MM_FreezeTag_BlockFrozenCommand(ent, false);
		client->latched_buttons &= ~BUTTON_ATTACK;
	}

	return true;
}

void MM_FreezeTag_ApplyClientEffects(gentity_t *ent)
{
	if (!RawFrozen(ent))
		return;

	freeze_state_t *state = StateFor(ent);
	if (!state || !ent || !ent->client)
		return;

	ApplyFrozenShell(ent, *state);
}

void MM_FreezeTag_ApplyFrozenPresentation(gentity_t *ent)
{
	if (!RawFrozen(ent))
		return;

	freeze_state_t *state = StateFor(ent);
	if (!state || !ent || !ent->client)
		return;

	ent->client->anim_priority = ANIM_DEATH;
	ent->client->anim_end = FRAME_stand01;
	ent->s.angles = state->body_angles;
	ent->s.frame = FRAME_stand01;
	ApplyFrozenShell(ent, *state);
	ClearFrozenHealthHudStats(ent->client);
	SyncFrozenViewProxy(ent, *state);
	ApplyFrozenCamera(ent, *state);
}

void HoldBotAtThaw(gentity_t *bot, gentity_t *target)
{
	if (!bot || !bot->client || !target)
		return;

	bot->velocity = {};
	bot->client->ps.pmove.velocity = {};
	ClearBotRescueCombatFocus(bot);

	const vec3_t look_target = target->s.origin + vec3_t{ 0, 0, 16.0f };
	const vec3_t look_delta = look_target - (bot->s.origin + vec3_t{ 0, 0, static_cast<float>(bot->viewheight) });
	if (look_delta.lengthSquared() > 1.0f) {
		vec3_t look_angles = vectoangles(look_delta.normalized());
		look_angles[ROLL] = 0;
		bot->client->v_angle = look_angles;
		bot->client->ps.viewangles = look_angles;
		AngleVectors(bot->client->v_angle, bot->client->v_forward, nullptr, nullptr);
	}

	gi.Bot_MoveToPoint(bot, bot->s.origin, BotRescueHoldTolerance());
}

void MM_FreezeTag_BotBeginFrame(gentity_t *bot)
{
	if (!BotRescueEnabled() || !MM_FreezeTag_IsActive() || !IsRoundLive())
		return;
	if (!bot || !bot->client || (bot->svflags & SVF_BOT) == 0)
		return;
	if (!MM_FreezeTag_ClientIsActiveFighter(bot))
		return;

	ClearBotInvalidFrozenFocus(bot);

	gentity_t *target = FindBotRescueTarget(bot);
	if (!target)
		return;

	freeze_state_t *state = StateFor(target);
	if (!state || !BotCanRescueFrozen(bot, target))
		return;

	const bool assigned_here = state->bot_rescuer_entnum == bot->s.number;
	if (assigned_here || !BotAssignmentIsValid(target, *state))
		state->bot_rescuer_entnum = bot->s.number;

	ClearBotRescueCombatFocus(bot);

	if (IsThawer(target, bot)) {
		HoldBotAtThaw(bot, target);
		return;
	}

	const GoalReturnCode result = gi.Bot_MoveToPoint(bot, BotRescueGoal(bot, target), BotRescueTolerance());
	if (result == GoalReturnCode::Error && state->bot_rescuer_entnum == bot->s.number)
		state->bot_rescuer_entnum = kNoThawer;
}

bool MM_FreezeTag_ClientIsActiveFighter(gentity_t *ent)
{
	return MM_FreezeTag_IsActive() &&
		ent && ent->client &&
		ClientIsPlaying(ent->client) &&
		IsFreezeTeam(ent->client->sess.team) &&
		!MM_Ghost_IsPendingRestore(ent) &&
		!ent->client->eliminated &&
		!RawFrozen(ent) &&
		!ent->deadflag &&
		ent->health > 0;
}

bool MM_FreezeTag_UpdateRound()
{
	if (!MM_FreezeTag_IsActive() || !IsRoundLive())
		return false;

	const mm_freezetag_team_counts_t red = CountTeam(TEAM_RED);
	const mm_freezetag_team_counts_t blue = CountTeam(TEAM_BLUE);
	const bool time_expired = roundtimelimit->value > 0 && level.time >= level.round_state_timer;

	return ResolveRoundFromCounts(red, blue, time_expired);
}

bool MM_FreezeTag_BuildRoundHudStatus(int display_round, std::string &out)
{
	out.clear();

	if (!MM_FreezeTag_IsActive() || !IsRoundLive())
		return false;

	out = MM_FreezeTagFormatRoundHudStatus(
		display_round,
		GT_ScoreLimit(),
		level.team_scores[TEAM_RED],
		level.team_scores[TEAM_BLUE]);
	return !out.empty();
}

bool MM_FreezeTag_BuildHudStatus(gentity_t *viewer, std::string &out)
{
	out.clear();

	if (!MM_FreezeTag_IsActive() || !IsRoundLive())
		return false;

	out = MM_FreezeTagFormatHudStatus(CountTeam(TEAM_RED), CountTeam(TEAM_BLUE), HudTeamFor(viewer));
	return !out.empty();
}

bool MM_FreezeTag_BuildCompactAliveHudStatus(gentity_t *viewer, std::string &out)
{
	out.clear();

	if (!MM_FreezeTag_IsActive() || !IsRoundLive())
		return false;

	out = MM_FreezeTagFormatCompactAliveStatus(CountTeam(TEAM_RED), CountTeam(TEAM_BLUE), HudTeamFor(viewer));
	return !out.empty();
}

bool MM_FreezeTag_BlockFrozenCommand(gentity_t *ent, bool notify)
{
	if (!RawFrozen(ent))
		return false;

	freeze_state_t *state = StateFor(ent);
	if (!state)
		return true;

	if (notify)
		gi.LocClient_Print(ent, PRINT_CENTER, "You are frozen!\nA teammate must stand near you to thaw you.");

	if (state->next_help <= level.time) {
		state->next_help = level.time + kHelpCallInterval;
		if (ent->client && IsFreezeTeam(ent->client->sess.team))
			SendFrozenHelpCall(ent, *state);
	}

	return true;
}

int MM_FreezeTag_ThawProgressPercent(const gentity_t *ent)
{
	const freeze_state_t *state = StateFor(ent);
	if (!state || !state->frozen || !state->thaw_started)
		return 0;
	if (!ThawerFromState(*state))
		return 0;

	return MM_FreezeTagThawProgressPercent(state->thaw_progress_seconds, ThawSeconds());
}

void MM_FreezeTag_OnFrozenDamage(gentity_t *target, gentity_t *)
{
	if (!RawFrozen(target) || !target->client)
		return;

	target->health = 1;
	target->client->pers.health = 1;
	target->client->damage_blood = 0;
	target->client->damage_armor = 0;
}
