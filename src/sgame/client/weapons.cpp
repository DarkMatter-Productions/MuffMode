// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Player weapon state machines and firing rules.

#include "g_local.h"
#include "monsters/m_player.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_client_profile.h"
#include "muffmode/mm_freezetag.h"
#include "muffmode/mm_match_stats.h"
// [MuffMode] Per-ruleset weapon tuning hooks (MM_Ruleset_*)
#include "muffmode/mm_ruleset_weapons.h"

bool is_quad;
bool is_haste;
player_muzzle_t is_silenced;
byte damage_multiplier;

static bool Weapon_ExcessiveEnabled(const gentity_t *ent) {
	return GT(GT_ARENA)
		? MM_Arena_ExcessiveEnabled(ent)
		: g_frenzy->integer != 0;
}

static bool Weapon_InstantSwitchEnabled(const gentity_t *ent) {
	// MuffMode Arena owns this setting per room. Do not let the legacy global
	// cvar flatten differently configured rooms into one switch policy.
	const bool fast_switch = GT(GT_ARENA) ?
		MM_Arena_FastSwitchEnabled(ent) : g_instant_weapon_switch->integer != 0;
	return fast_switch || Weapon_ExcessiveEnabled(ent);
}

static bool Weapon_QuickSwitchEnabled(const gentity_t *ent) {
	const bool quick_switch = GT(GT_ARENA) ?
		MM_Arena_FastSwitchEnabled(ent) : g_quick_weapon_switch->integer != 0;
	return quick_switch || Weapon_ExcessiveEnabled(ent);
}

static float Weapon_FireTimeScale(const gentity_t *ent) {
	return GT(GT_ARENA)
		? MM_Arena_WeaponFireRateScale(ent)
		: (g_frenzy->integer ? 0.5f : 1.0f);
}

static bool Weapon_CombatDisabled(const gentity_t *ent) {
	return GT(GT_ARENA) ? !MM_Arena_CombatEnabled(ent) : IsCombatDisabled();
}

static trace_t Weapon_ArenaTrace(const vec3_t &start, const vec3_t *mins,
	const vec3_t *maxs, const vec3_t &end, gentity_t *passent,
	contents_t mask, const gentity_t *source) {
	auto trace = [&]() {
		return mins && maxs
			? gi.trace(start, *mins, *maxs, end, passent, mask)
			: gi.traceline(start, end, passent, mask);
	};

	trace_t tr = trace();
	if (notGT(GT_ARENA))
		return tr;

	struct skipped_entity_t {
		gentity_t *ent;
		int32_t spawn_count;
	};
	constexpr size_t MAX_ARENA_TRACE_SKIPS = 64;
	std::array<skipped_entity_t, MAX_ARENA_TRACE_SKIPS> skipped {};
	size_t skipped_count = 0;

	while (tr.ent && tr.ent != world &&
		!MM_Arena_CanInteract(source, tr.ent) &&
		skipped_count < skipped.size()) {
		skipped[skipped_count++] = { tr.ent, tr.ent->spawn_count };
		gi.unlinkentity(tr.ent);
		tr = trace();
	}

	while (skipped_count > 0) {
		const skipped_entity_t entry = skipped[--skipped_count];
		if (entry.ent->inuse && entry.ent->spawn_count == entry.spawn_count)
			gi.linkentity(entry.ent);
	}

	return tr;
}

static trace_t Weapon_ArenaTraceline(const vec3_t &start, const vec3_t &end,
	gentity_t *passent, contents_t mask, const gentity_t *source) {
	return Weapon_ArenaTrace(start, nullptr, nullptr, end, passent, mask, source);
}

static trace_t Weapon_ArenaBoxTrace(const vec3_t &start, const vec3_t &mins,
	const vec3_t &maxs, const vec3_t &end, gentity_t *passent,
	contents_t mask, const gentity_t *source) {
	return Weapon_ArenaTrace(start, &mins, &maxs, end, passent, mask, source);
}

/*
================
InfiniteAmmoOn
================
*/
bool InfiniteAmmoOn(gitem_t *item) {
	if (item && item->flags & IF_NO_INFINITE_AMMO)
		return false;

	// [MuffMode] GT_ARENA resolves infinite ammo per room (Practice or
	// excessive). A global modifier must not flatten independent arena rules.
	if (GT(GT_ARENA))
		return false;

	return g_infinite_ammo->integer || (deathmatch->integer && (g_instagib->integer || g_nadefest->integer));
}

/*
================
Stats_AddShot
================
*/
static void Stats_AddShot(gentity_t *ent, uint32_t count = 1) {
	if (!ent || !ent->client || !count)
		return;
	// Preserve the legacy live-stat event count. Hitscan pellets are already
	// counted individually in fire_lead; the export records projectile count.
	MS_Adjust(ent->client, MSTAT_SHOTS, 1);
	const item_id_t weapon = ent->client->pers.weapon
		? ent->client->pers.weapon->id
		: IT_NULL;
	MM_MatchStats_RecordShot(ent->client,
		MM_MatchStats_WeaponForItem(weapon), count);
}

/*
================
P_DamageModifier
================
*/
byte P_DamageModifier(gentity_t *ent) {
	is_quad = false;
	damage_multiplier = 1;

	if (ent->client->pu_time_quad > level.time) {
		damage_multiplier *= 4;
		is_quad = true;

		// if we're quad and DF_NO_STACK_DOUBLE is on, return now.
		if (g_dm_no_stack_double->integer)
			return damage_multiplier;
	}

	if (ent->client->pu_time_double > level.time) {
		damage_multiplier *= 2;
		is_quad = true;
	}

	return damage_multiplier;
}

/*
================
P_CurrentKickFactor

[Paril-KEX] kicks in vanilla take place over 2 10hz server
frames; this is to mimic that visual behavior on any tickrate.
================
*/

static inline float P_CurrentKickFactor(gentity_t *ent) {
	if (ent->client->kick.time < level.time)
		return 0.f;

	float total_sec = ent->client->kick.total.seconds();
	if (total_sec <= 0.0f)
		return 0.f;

	float f = (ent->client->kick.time - level.time).seconds() / total_sec;
	return f;
}

/*
================
P_CurrentKickAngles
================
*/
vec3_t P_CurrentKickAngles(gentity_t *ent) {
	return ent->client->kick.angles * P_CurrentKickFactor(ent);
}

/*
================
P_CurrentKickOrigin
================
*/
vec3_t P_CurrentKickOrigin(gentity_t *ent) {
	return ent->client->kick.origin * P_CurrentKickFactor(ent);
}

/*
================
P_AddWeaponKick
================
*/
void P_AddWeaponKick(gentity_t *ent, const vec3_t &origin, const vec3_t &angles) {
	ent->client->kick.origin = origin;
	ent->client->kick.angles = angles;
	ent->client->kick.total = 200_ms;
	ent->client->kick.time = level.time + ent->client->kick.total;
}

/*
================
P_ProjectSource
================
*/
class scoped_lag_compensation_t {
public:
	scoped_lag_compensation_t() = default;
	scoped_lag_compensation_t(const scoped_lag_compensation_t &) = delete;
	scoped_lag_compensation_t &operator=(const scoped_lag_compensation_t &) = delete;

	~scoped_lag_compensation_t() {
		reset();
	}

	void activate() {
		active = true;
	}

	void reset() {
		if (!active)
			return;

		active = false;
		G_UnLagCompensate();
	}

private:
	bool active = false;
};

static void P_ProjectSourceInternal(gentity_t *ent, const vec3_t &angles, vec3_t distance, vec3_t &result_start, vec3_t &result_dir, bool lag_compensate_aim) {
	if (g_weapon_projection->integer) {
		distance[1] = 0;
		if (g_weapon_projection->integer > 1)
			distance[2] = 0;
	} else if (ent->client->pers.hand == LEFT_HANDED)
		distance[1] *= -1;
	else if (ent->client->pers.hand == CENTER_HANDED || ent->client->pers.hand == CENTER_HANDED_VISIBLE)
		distance[1] = 0;

	vec3_t forward, right, up;
	vec3_t eye_position = (ent->s.origin + vec3_t{ 0, 0, (float)ent->viewheight });

	AngleVectors(angles, forward, right, up);

	result_start = G_ProjectSource2(eye_position, distance, forward, right, up);

	vec3_t	   end = eye_position + forward * 8192;
	contents_t mask = MASK_PROJECTILE & ~CONTENTS_DEADMONSTER;

	// [Paril-KEX]
	if (!G_ShouldPlayersCollide(true))
		mask &= ~CONTENTS_PLAYER;

	scoped_lag_compensation_t aim_lag_compensation;
	if (lag_compensate_aim && G_LagCompensate(ent, eye_position, forward))
		aim_lag_compensation.activate();

	trace_t tr = Weapon_ArenaTraceline(eye_position, end, ent, mask, ent);
	aim_lag_compensation.reset();

	// if the point was a monster & close to us, use raw forward
	// so railgun pierces properly
	if (tr.startsolid || ((tr.contents & (CONTENTS_MONSTER | CONTENTS_PLAYER)) && (tr.fraction * 8192.f) < 128.f))
		result_dir = forward;
	else {
		end = tr.endpos;
		result_dir = (end - result_start).normalized();
	}
}

void P_ProjectSource(gentity_t *ent, const vec3_t &angles, vec3_t distance, vec3_t &result_start, vec3_t &result_dir) {
	P_ProjectSourceInternal(ent, angles, distance, result_start, result_dir, false);
}

static void P_ProjectSourceQ3A(gentity_t *ent, const vec3_t &angles, vec3_t &result_start, vec3_t &result_dir) {
	AngleVectors(angles, result_dir, nullptr, nullptr);

	result_start = ent->s.origin + vec3_t{ 0, 0, (float)ent->viewheight } + (result_dir * 14.0f);

	// Q3 snaps muzzle points before firing.
	for (size_t i = 0; i < 3; i++)
		result_start[i] = (float)(int)result_start[i];
}

// Rewinds players for P_ProjectSource's aim probe in enhanced mode, then starts
// the caller's immediate-hit compensation from a clean live-world state.
static void P_ProjectSourceAndLagCompensate(gentity_t *ent, const vec3_t &angles, vec3_t distance, vec3_t &result_start, vec3_t &result_dir, scoped_lag_compensation_t &lag_compensation) {
	P_ProjectSourceInternal(ent, angles, distance, result_start, result_dir, g_lag_compensation_enhanced->integer != 0);
	if (G_LagCompensate(ent, result_start, result_dir))
		lag_compensation.activate();
}

static void P_ProjectSourceQ3AAndLagCompensate(gentity_t *ent, const vec3_t &angles, vec3_t &result_start, vec3_t &result_dir, scoped_lag_compensation_t &lag_compensation) {
	P_ProjectSourceQ3A(ent, angles, result_start, result_dir);
	if (G_LagCompensate(ent, result_start, result_dir))
		lag_compensation.activate();
}

/*
===============
PlayerNoise

Each player can have two noise objects associated with it:
a personal noise (jumping, pain, weapon firing), and a weapon
target noise (bullet wall impacts)

Monsters that don't directly see the player can move
to a noise in hopes of seeing the player from there.
===============
*/
void PlayerNoise(gentity_t *who, const vec3_t &where, player_noise_t type) {
	gentity_t *noise;

	if (type == PNOISE_WEAPON) {
		if (who->client->silencer_shots)
			who->client->invisibility_fade_time = level.time + (INVISIBILITY_TIME / 5);
		else
			who->client->invisibility_fade_time = level.time + INVISIBILITY_TIME;

		if (who->client->silencer_shots) {
			who->client->silencer_shots--;
			return;
		}
	}

	if (deathmatch->integer)
		return;

	if (who->flags & FL_NOTARGET)
		return;

	if (type == PNOISE_SELF &&
		(who->client->landmark_free_fall || who->client->landmark_noise_time >= level.time))
		return;

	if (who->flags & FL_DISGUISED) {
		if (type == PNOISE_WEAPON) {
			level.disguise_violator = who;
			level.disguise_violation_time = level.time + 500_ms;
		} else
			return;
	}

	if (!who->mynoise) {
		noise = G_Spawn();
		noise->classname = "player_noise";
		noise->mins = { -8, -8, -8 };
		noise->maxs = { 8, 8, 8 };
		noise->owner = who;
		noise->svflags = SVF_NOCLIENT;
		who->mynoise = noise;

		noise = G_Spawn();
		noise->classname = "player_noise";
		noise->mins = { -8, -8, -8 };
		noise->maxs = { 8, 8, 8 };
		noise->owner = who;
		noise->svflags = SVF_NOCLIENT;
		who->mynoise2 = noise;
	}

	if (type == PNOISE_SELF || type == PNOISE_WEAPON) {
		noise = who->mynoise;
		who->client->sound_entity = noise;
		who->client->sound_entity_time = level.time;
	} else // type == PNOISE_IMPACT
	{
		noise = who->mynoise2;
		who->client->sound2_entity = noise;
		who->client->sound2_entity_time = level.time;
	}

	noise->s.origin = where;
	noise->absmin = where - noise->maxs;
	noise->absmax = where + noise->maxs;
	noise->teleport_time = level.time;
	gi.linkentity(noise);
}

/*
================
G_WeaponShouldStay
================
*/
static inline bool G_WeaponShouldStay() {
	if (deathmatch->integer)
		return g_dm_weapons_stay->integer;
	else if (coop->integer)
		return !P_UseCoopInstancedItems();

	return false;
}

/*
================
Pickup_Weapon
================
*/
void G_CheckAutoSwitch(gentity_t *ent, gitem_t *item, bool is_new);
bool Pickup_Weapon(gentity_t *ent, gentity_t *other) {
	item_id_t index = ent->item->id;

	if (G_WeaponShouldStay() && other->client->pers.inventory[index]) {
		if (!(ent->spawnflags & (SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER)))
			return false; // leave the weapon for others to pickup
	}

	gitem_t	*ammo;
	bool	is_new = !other->client->pers.inventory[index];

	if (!(ent->spawnflags & SPAWNFLAG_ITEM_DROPPED) || ent->count) {
		// give them some ammo with it if appropriate
		item_id_t ammo_id = MM_Ruleset_WeaponAmmoId(ent->item);
		if (ammo_id) {
			ammo = GetItemByIndex(ammo_id);
			if (InfiniteAmmoOn(ammo) || MM_Arena_InfiniteAmmoEnabled(other))
				Add_Ammo(other, ammo, AMMO_INFINITE);
			else {
				int quantity = MM_Ruleset_WeaponPickupAmmoQuantity(ent, other, ammo);
				Add_Ammo(other, ammo, quantity);
			}
		}

		if (!(ent->spawnflags & SPAWNFLAG_ITEM_DROPPED_PLAYER)) {
			if (deathmatch->integer) {
				if (g_dm_weapons_stay->integer)
					ent->flags |= FL_RESPAWN;

				SetRespawn(ent, gtime_t::from_sec(g_weapon_respawn_time->integer), !g_dm_weapons_stay->integer);
			}
			if (coop->integer)
				ent->flags |= FL_RESPAWN;
		}
	}

	other->client->pers.inventory[index]++;

	G_CheckAutoSwitch(other, ent->item, is_new);

	return true;
}

/*
================
Weapon_RunThink
================
*/
static void Weapon_RunThink(gentity_t *ent) {
	// call active weapon think routine
	if (!ent->client->pers.weapon || !ent->client->pers.weapon->weaponthink)
		return;

	P_DamageModifier(ent);

	is_haste = (ent->client->pu_time_haste > level.time);

	if (ent->client->silencer_shots)
		is_silenced = MZ_SILENCED;
	else
		is_silenced = MZ_NONE;
	ent->client->pers.weapon->weaponthink(ent);
}

/*
===============
Change_Weapon

The old weapon has been dropped all the way, so make the new one
current
===============
*/
void Change_Weapon(gentity_t *ent) {
	// [Paril-KEX]
	if (ent->health > 0 && !Weapon_InstantSwitchEnabled(ent) && ((ent->client->latched_buttons | ent->client->buttons) & BUTTON_HOLSTER))
		return;

	if (ent->client->grenade_time) {
		// force a weapon think to drop the held grenade
		ent->client->weapon_sound = 0;
		Weapon_RunThink(ent);
		ent->client->grenade_time = 0_ms;
	}

	if (ent->client->pers.weapon) {
		ent->client->pers.lastweapon = ent->client->pers.weapon;

		if (ent->client->newweapon && ent->client->newweapon != ent->client->pers.weapon) {
			//muff: only make the sound if we can switch faster
			if (Weapon_QuickSwitchEnabled(ent) || Weapon_InstantSwitchEnabled(ent))
				gi.sound(ent, CHAN_WEAPON, gi.soundindex("weapons/change.wav"), 1, ATTN_NORM, 0);
		}
	}

	ent->client->pers.weapon = ent->client->newweapon;
	ent->client->newweapon = nullptr;

	// set visible model
	if (ent->s.modelindex == MODELINDEX_PLAYER)
		P_AssignClientSkinnum(ent);

	if (!ent->client->pers.weapon) { // dead
		ent->client->ps.gunindex = 0;
		ent->client->ps.gunskin = 0;
		return;
	}

	ent->client->weaponstate = WEAPON_ACTIVATING;
	ent->client->ps.gunframe = 0;
	ent->client->ps.gunindex = gi.modelindex(ent->client->pers.weapon->view_model);
	ent->client->ps.gunskin = 0;
	ent->client->weapon_sound = 0;

	ent->client->anim_priority = ANIM_PAIN;
	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
		ent->s.frame = FRAME_crpain1;
		ent->client->anim_end = FRAME_crpain4;
	} else {
		ent->s.frame = FRAME_pain301;
		ent->client->anim_end = FRAME_pain304;
	}
	ent->client->anim_time = 0_ms;

	// for instantweap, run think immediately
	// to set up correct start frame
	if (Weapon_InstantSwitchEnabled(ent))
		Weapon_RunThink(ent);
}

/*
=================
NoAmmoWeaponChange
=================
*/
void NoAmmoWeaponChange(gentity_t *ent, bool sound) {
	// [MuffMode] This helper is also reached from map targets and lifecycle code.
	if (!ent || !ent->client)
		return;

	if (sound) {
		if (level.time >= ent->client->empty_click_sound) {
			gi.sound(ent, CHAN_WEAPON, gi.soundindex("weapons/noammo.wav"), 1, ATTN_NORM, 0);
			ent->client->empty_click_sound = level.time + 1_sec;
		}
	}

	std::array<item_id_t, IT_TOTAL> no_ammo_order{};
	const size_t no_ammo_count = MM_ClientProfileBuildWeaponOrder(
		ent->client, no_ammo_order.data(), no_ammo_order.size());
	for (size_t i = 0; i < no_ammo_count; i++) {
		gitem_t *item = GetItemByIndex(no_ammo_order[i]);

		if (!item) {
			gi.Com_ErrorFmt("Invalid no ammo weapon switch weapon {}\n", (int32_t)no_ammo_order[i]);
			continue;
		}

		if (RS(RS_Q3A) && item->id == IT_WEAPON_SSHOTGUN)
			continue;

		if (!ent->client->pers.inventory[item->id])
			continue;

		if (RS(RS_Q1) && item->id == IT_WEAPON_PLASMABEAM && ent->waterlevel >= WATER_WAIST)
			continue;

		item_id_t ammo_id = MM_Ruleset_WeaponAmmoId(item);
		if (ammo_id && ent->client->pers.inventory[ammo_id] < MM_Ruleset_WeaponAmmoRequired(item))
			continue;

		ent->client->newweapon = item;
		return;
	}
}

/*
================
RemoveAmmo
================
*/
static void RemoveAmmo(gentity_t *ent, int32_t quantity) {
	if (InfiniteAmmoOn(ent->client->pers.weapon) || MM_Arena_InfiniteAmmoEnabled(ent))
		return;

	item_id_t ammo_id = MM_Ruleset_WeaponAmmoId(ent->client->pers.weapon);
	if (!ammo_id)
		return;

	bool pre_warning = ent->client->pers.inventory[ammo_id] <= ent->client->pers.weapon->quantity_warn;

	ent->client->pers.inventory[ammo_id] -= quantity;

	bool post_warning = ent->client->pers.inventory[ammo_id] <= ent->client->pers.weapon->quantity_warn;

	if (!pre_warning && post_warning)
		gi.local_sound(ent, CHAN_AUTO, gi.soundindex("weapons/lowammo.wav"), 1, ATTN_NORM, 0);

	if (ammo_id == IT_AMMO_CELLS)
		G_CheckPowerArmor(ent);
}

/*
================
Weapon_AnimationTime

[Paril-KEX] get time per animation frame
================
*/
static inline gtime_t Weapon_AnimationTime(gentity_t *ent) {
	const gitem_t *weapon = ent->client->pers.weapon;

	if (Weapon_QuickSwitchEnabled(ent) && (gi.tick_rate >= 20) &&
		(ent->client->weaponstate == WEAPON_ACTIVATING || ent->client->weaponstate == WEAPON_DROPPING))
		ent->client->ps.gunrate = 20;
	else
		ent->client->ps.gunrate = 10;

	if (RS(RS_Q3A) && ent->client->weaponstate == WEAPON_FIRING && weapon) {
		if (weapon->id == IT_WEAPON_PLASMABEAM)
			ent->client->ps.gunrate = 20;
		else if (weapon->id == IT_WEAPON_CHAINGUN)
			ent->client->ps.gunrate = 30;
	}

	if (weapon && ent->client->ps.gunframe != 0 && (!(weapon->flags & IF_NO_HASTE) || ent->client->weaponstate != WEAPON_FIRING)) {
		if (is_haste)
			ent->client->ps.gunrate *= 1.5;
		if (Tech_ApplyTimeAccel(ent))
			ent->client->ps.gunrate *= 2;
		ent->client->ps.gunrate /= Weapon_FireTimeScale(ent);
	}

	// network optimization...
	if (ent->client->ps.gunrate == 10) {
		ent->client->ps.gunrate = 0;
		return 100_ms;
	}

	return gtime_t::from_ms((1.f / ent->client->ps.gunrate) * 1000);
}

void Weapon_CancelFiring(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	gclient_t *client = ent->client;
	client->latched_buttons &= ~BUTTON_ATTACK;
	client->weapon_fire_buffered = false;

	if (client->weaponstate != WEAPON_FIRING)
		return;

	// Re-enter the normal activation path after combat resumes instead of
	// continuing on a pending fire/throw frame. This also safely abandons a
	// primed throwable without spawning it or consuming ammunition.
	client->weaponstate = WEAPON_ACTIVATING;
	client->ps.gunframe = 0;
	client->weapon_sound = 0;
	client->grenade_time = 0_ms;
	client->grenade_finished_time = 0_ms;
	client->grenade_blew_up = false;
	client->weapon_thunk = false;
	client->weapon_think_time = level.time;
	client->weapon_fire_finished = level.time;
}

/*
=================
Think_Weapon

Called by ClientBeginServerFrame and ClientThink
=================
*/
void Think_Weapon(gentity_t *ent) {
	if (!ClientIsPlaying(ent->client) || ent->client->eliminated || MM_FreezeTag_IsFrozen(ent))
		return;

	// if just died, put the weapon away
	if (ent->health < 1) {
		ent->client->newweapon = nullptr;
		Change_Weapon(ent);
	}

	if (!ent->client->pers.weapon) {
		if (ent->client->newweapon)
			Change_Weapon(ent);
		return;
	}

	// call active weapon think routine
	Weapon_RunThink(ent);

	// check remainder from time accel; on 100ms/50ms server frames we may have
	// 'run next frame in' times that we can't possibly catch up to,
	// so we have to run them now.
	if (33_ms < FRAME_TIME_MS) {
		gtime_t relative_time = Weapon_AnimationTime(ent);

		if (relative_time < FRAME_TIME_MS) {
			// check how many we can't run before the next server tick
			gtime_t next_frame = level.time + FRAME_TIME_S;
			int64_t remaining_ms = (next_frame - ent->client->weapon_think_time).milliseconds();

			while (remaining_ms > 0) {
				ent->client->weapon_think_time -= relative_time;
				ent->client->weapon_fire_finished -= relative_time;
				Weapon_RunThink(ent);
				remaining_ms -= relative_time.milliseconds();
			}
		}
	}
}

/*
================
Weapon_AttemptSwitch
================
*/
enum class weap_switch_t {
	already_using,
	no_weapon,
	no_ammo,
	not_enough_ammo,
	valid
};

static weap_switch_t Weapon_AttemptSwitch(gentity_t *ent, gitem_t *item, bool silent) {
	if (ent->client->pers.weapon == item)
		return weap_switch_t::already_using;
	else if (RS(RS_Q3A) && item->id == IT_WEAPON_SSHOTGUN)
		return weap_switch_t::no_weapon;
	else if (!ent->client->pers.inventory[item->id])
		return weap_switch_t::no_weapon;

	item_id_t ammo_id = MM_Ruleset_WeaponAmmoId(item);
	if (ammo_id && !g_select_empty->integer && !(item->flags & IF_AMMO)) {
		gitem_t *ammo_item = GetItemByIndex(ammo_id);

		if (!ent->client->pers.inventory[ammo_id]) {
			if (!silent)
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_no_ammo", ammo_item->pickup_name, item->pickup_name_definite);
			return weap_switch_t::no_ammo;
		} else if (ent->client->pers.inventory[ammo_id] < MM_Ruleset_WeaponAmmoRequired(item)) {
			if (!silent)
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_not_enough_ammo", ammo_item->pickup_name, item->pickup_name_definite);
			return weap_switch_t::not_enough_ammo;
		}
	}

	return weap_switch_t::valid;
}

static inline bool Weapon_IsPartOfChain(gitem_t *item, gitem_t *other) {
	return other && other->chain && item->chain && other->chain == item->chain;
}

/*
================
Use_Weapon

Make the weapon ready if there is ammo
================
*/
void Use_Weapon(gentity_t *ent, gitem_t *item) {
	gitem_t			*wanted, *root;
	weap_switch_t	result = weap_switch_t::no_weapon;

	// if we're switching to a weapon in this chain already,
	// start from the weapon after this one in the chain
	if (!ent->client->no_weapon_chains && Weapon_IsPartOfChain(item, ent->client->newweapon)) {
		root = ent->client->newweapon;
		wanted = root->chain_next;
	}
	// if we're already holding a weapon in this chain,
	// start from the weapon after that one
	else if (!ent->client->no_weapon_chains && Weapon_IsPartOfChain(item, ent->client->pers.weapon)) {
		root = ent->client->pers.weapon;
		wanted = root->chain_next;
	}
	// start from beginning of chain (if any)
	else
		wanted = root = item;

	while (true) {
		// try the weapon currently in the chain
		if ((result = Weapon_AttemptSwitch(ent, wanted, false)) == weap_switch_t::valid)
			break;

		// no chains
		if (!wanted->chain_next || ent->client->no_weapon_chains)
			break;

		wanted = wanted->chain_next;

		// we wrapped back to the root item
		if (wanted == root)
			break;
	}

	if (result == weap_switch_t::valid)
		ent->client->newweapon = wanted; // change to this weapon when down
	else if (Weapon_AttemptSwitch(ent, wanted, true) == weap_switch_t::no_weapon && wanted != ent->client->pers.weapon && wanted != ent->client->newweapon)
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_out_of_item", wanted->pickup_name);
}

/*
================
Drop_Weapon
================
*/
void Drop_Weapon(gentity_t *ent, gitem_t *item) {
	// [Paril-KEX]
	if (deathmatch->integer && g_dm_weapons_stay->integer)
		return;

	item_id_t index = item->id;

	if (ent->client->pers.inventory[index] < 1)
		return;

	gentity_t *drop = Drop_Item(ent, item);
	drop->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
	drop->svflags &= ~SVF_INSTANCED;

	item_id_t ammo_id = MM_Ruleset_WeaponAmmoId(drop->item);
	gitem_t *ammo = ammo_id ? GetItemByIndex(ammo_id) : nullptr;
	if (!ammo)
		return;
	if (ent->client->pers.inventory[ammo->id] <= 0)
		return;
	
	drop->count = MM_Ruleset_WeaponDropAmmoQuantity(item, ammo);

	drop->count = min(drop->count, ent->client->pers.inventory[ammo->id]);

	if (drop->count <= 0)
		return;

	if (ent->client->pers.inventory[ammo->id] - drop->count < 0) {
		G_FreeEntity(drop);
		return;
	}

	ent->client->pers.inventory[ammo->id] -= drop->count;
	ent->client->pers.inventory[index]--;

	// see if we were already using it
	if ((item == ent->client->pers.weapon) || (item == ent->client->newweapon)) {
		if (ent->client->pers.inventory[index] < 1 || ent->client->pers.inventory[ammo->id] < 1)
			NoAmmoWeaponChange(ent, true);
	}
}

/*
================
Weapon_PowerupSound
================
*/
void Weapon_PowerupSound(gentity_t *ent) {
	if (!Tech_ApplyPowerAmpSound(ent)) {
		if (ent->client->pu_time_quad > level.time && ent->client->pu_time_double > level.time)
			gi.sound(ent, CHAN_ITEM, gi.soundindex("ctf/tech2x.wav"), 1, ATTN_NORM, 0);
		else if (ent->client->pu_time_quad > level.time)
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage3.wav"), 1, ATTN_NORM, 0);
		else if (ent->client->pu_time_double > level.time)
			gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/ddamage3.wav"), 1, ATTN_NORM, 0);
		else if (ent->client->pu_time_haste > level.time
				&& ent->client->tech_sound_time < level.time) {
			ent->client->tech_sound_time = level.time + 1_sec;
			gi.sound(ent, CHAN_ITEM, gi.soundindex("ctf/tech3.wav"), 1, ATTN_NORM, 0);
		}
	}

	Tech_ApplyTimeAccelSound(ent);
}

/*
================
Weapon_CanAnimate
================
*/
static inline bool Weapon_CanAnimate(gentity_t *ent) {
	// VWep animations screw up corpses
	return !ent->deadflag && ent->s.modelindex == MODELINDEX_PLAYER;
}

/*
================
Weapon_SetFinished

[Paril-KEX] called when finished to set time until
we're allowed to switch to fire again
================
*/
static inline void Weapon_SetFinished(gentity_t *ent) {
	ent->client->weapon_fire_finished = level.time + Weapon_AnimationTime(ent);
}

/*
================
Weapon_HandleDropping
================
*/
static inline bool Weapon_HandleDropping(gentity_t *ent, int FRAME_DEACTIVATE_LAST) {
	if (ent->client->weaponstate == WEAPON_DROPPING) {
		if (ent->client->weapon_think_time <= level.time) {
			if (ent->client->ps.gunframe == FRAME_DEACTIVATE_LAST) {
				Change_Weapon(ent);
				return true;
			} else if ((FRAME_DEACTIVATE_LAST - ent->client->ps.gunframe) == 4) {
				ent->client->anim_priority = ANIM_ATTACK | ANIM_REVERSED;
				if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
					ent->s.frame = FRAME_crpain4 + 1;
					ent->client->anim_end = FRAME_crpain1;
				} else {
					ent->s.frame = FRAME_pain304 + 1;
					ent->client->anim_end = FRAME_pain301;
				}
				ent->client->anim_time = 0_ms;
			}

			ent->client->ps.gunframe++;
			ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);
		}
		return true;
	}

	return false;
}

/*
================
Weapon_HandleActivating
================
*/
static inline bool Weapon_HandleActivating(gentity_t *ent, int FRAME_ACTIVATE_LAST, int FRAME_IDLE_FIRST) {
	if (ent->client->weaponstate == WEAPON_ACTIVATING) {
		if (ent->client->weapon_think_time <= level.time || Weapon_InstantSwitchEnabled(ent)) {
			ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);

			if (ent->client->ps.gunframe == FRAME_ACTIVATE_LAST || Weapon_InstantSwitchEnabled(ent)) {
				ent->client->weaponstate = WEAPON_READY;
				ent->client->ps.gunframe = FRAME_IDLE_FIRST;
				ent->client->weapon_fire_buffered = false;
				if (!Weapon_InstantSwitchEnabled(ent) || Weapon_ExcessiveEnabled(ent))
					Weapon_SetFinished(ent);
				else
					ent->client->weapon_fire_finished = 0_ms;
				return true;
			}

			ent->client->ps.gunframe++;
			return true;
		}
	}

	return false;
}

/*
================
Weapon_HandleNewWeapon
================
*/
static inline bool Weapon_HandleNewWeapon(gentity_t *ent, int FRAME_DEACTIVATE_FIRST, int FRAME_DEACTIVATE_LAST) {
	bool is_holstering = false;

	if (!Weapon_InstantSwitchEnabled(ent) || Weapon_ExcessiveEnabled(ent))
		is_holstering = ((ent->client->latched_buttons | ent->client->buttons) & BUTTON_HOLSTER);

	if ((ent->client->newweapon || is_holstering) && (ent->client->weaponstate != WEAPON_FIRING)) {
		if (Weapon_InstantSwitchEnabled(ent) || ent->client->weapon_think_time <= level.time) {
			if (!ent->client->newweapon)
				ent->client->newweapon = ent->client->pers.weapon;

			ent->client->weaponstate = WEAPON_DROPPING;

			if (Weapon_InstantSwitchEnabled(ent)) {
				Change_Weapon(ent);
				return true;
			}

			ent->client->ps.gunframe = FRAME_DEACTIVATE_FIRST;

			if ((FRAME_DEACTIVATE_LAST - FRAME_DEACTIVATE_FIRST) < 4) {
				ent->client->anim_priority = ANIM_ATTACK | ANIM_REVERSED;
				if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
					ent->s.frame = FRAME_crpain4 + 1;
					ent->client->anim_end = FRAME_crpain1;
				} else {
					ent->s.frame = FRAME_pain304 + 1;
					ent->client->anim_end = FRAME_pain301;
				}
				ent->client->anim_time = 0_ms;
			}

			ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);
		}
		return true;
	}

	return false;
}

/*
================
Weapon_HandleReady
================
*/
enum class weapon_ready_state_t {
	none,
	changing,
	firing
};

static inline weapon_ready_state_t Weapon_HandleReady(gentity_t *ent, int FRAME_FIRE_FIRST, int FRAME_IDLE_FIRST, int FRAME_IDLE_LAST, const int *pause_frames) {
	if (ent->client->weaponstate == WEAPON_READY) {
		bool request_firing;

		if (Weapon_CombatDisabled(ent)) {
			request_firing = false;
			ent->client->latched_buttons &= ~BUTTON_ATTACK;
			ent->client->weapon_fire_buffered = false;
		} else
			request_firing = ent->client->weapon_fire_buffered || ((ent->client->latched_buttons | ent->client->buttons) & BUTTON_ATTACK);

		if (request_firing && ent->client->weapon_fire_finished <= level.time) {
			ent->client->latched_buttons &= ~BUTTON_ATTACK;
			ent->client->weapon_think_time = level.time;

			item_id_t ammo_id = MM_Ruleset_WeaponAmmoId(ent->client->pers.weapon);
			int required_ammo = MM_Ruleset_WeaponAmmoRequired(ent->client->pers.weapon);
			if (MM_Ruleset_SuperShotgunFallsBackToSingleShell() &&
				ent->client->pers.weapon->id == IT_WEAPON_SSHOTGUN &&
				ammo_id && ent->client->pers.inventory[ammo_id] > 0)
				required_ammo = 1;

			if (!ammo_id ||
				(ent->client->pers.inventory[ammo_id] >= required_ammo)) {
				ent->client->weaponstate = WEAPON_FIRING;
				ent->client->last_firing_time = level.time + COOP_DAMAGE_FIRING_TIME;
				return weapon_ready_state_t::firing;
			} else {
				NoAmmoWeaponChange(ent, true);
				return weapon_ready_state_t::changing;
			}
		} else if (ent->client->weapon_think_time <= level.time) {
			ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);

			if (ent->client->ps.gunframe == FRAME_IDLE_LAST) {
				ent->client->ps.gunframe = FRAME_IDLE_FIRST;
				return weapon_ready_state_t::changing;
			}

			if (pause_frames)
				for (int n = 0; pause_frames[n]; n++)
					if (ent->client->ps.gunframe == pause_frames[n])
						if (irandom(16))
							return weapon_ready_state_t::changing;

			ent->client->ps.gunframe++;
			return weapon_ready_state_t::changing;
		}
	}

	return weapon_ready_state_t::none;
}

/*
================
Weapon_HandleFiring
================
*/
[[nodiscard]] static inline bool Weapon_HandleFiring(gentity_t *ent,
	int32_t FRAME_IDLE_FIRST, std::function<void()> fire_handler) {
	const int32_t ent_generation = ent->spawn_count;
	Weapon_SetFinished(ent);

	if (ent->client->weapon_fire_buffered) {
		ent->client->buttons |= BUTTON_ATTACK;
		ent->client->weapon_fire_buffered = false;
	}

	fire_handler();
	if (!ent->inuse || ent->spawn_count != ent_generation)
		return false;

	if (ent->client->ps.gunframe == FRAME_IDLE_FIRST) {
		ent->client->weaponstate = WEAPON_READY;
		ent->client->weapon_fire_buffered = false;
	}

	ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);
	return true;
}

/*
================
Weapon_Generic
================
*/
void Weapon_Generic(gentity_t *ent, int FRAME_ACTIVATE_LAST, int FRAME_FIRE_LAST, int FRAME_IDLE_LAST, int FRAME_DEACTIVATE_LAST, const int *pause_frames, const int *fire_frames, void (*fire)(gentity_t *ent)) {
	int FRAME_FIRE_FIRST = (FRAME_ACTIVATE_LAST + 1);
	int FRAME_IDLE_FIRST = (FRAME_FIRE_LAST + 1);
	int FRAME_DEACTIVATE_FIRST = (FRAME_IDLE_LAST + 1);

	if (!Weapon_CanAnimate(ent))
		return;

	if (Weapon_HandleDropping(ent, FRAME_DEACTIVATE_LAST))
		return;
	else if (Weapon_HandleActivating(ent, FRAME_ACTIVATE_LAST, FRAME_IDLE_FIRST))
		return;
	else if (Weapon_HandleNewWeapon(ent, FRAME_DEACTIVATE_FIRST, FRAME_DEACTIVATE_LAST))
		return;
	else if (const auto state = Weapon_HandleReady(ent, FRAME_FIRE_FIRST, FRAME_IDLE_FIRST, FRAME_IDLE_LAST, pause_frames);
		state != weapon_ready_state_t::none) {
		if (state == weapon_ready_state_t::firing) {
			const int32_t ent_generation = ent->spawn_count;
			ent->client->ps.gunframe = FRAME_FIRE_FIRST;
			ent->client->weapon_fire_buffered = false;

			if (ent->client->weapon_thunk)
				ent->client->weapon_think_time += FRAME_TIME_S;

			ent->client->weapon_think_time += Weapon_AnimationTime(ent);
			Weapon_SetFinished(ent);

			for (int n = 0; fire_frames[n]; n++) {
				if (ent->client->ps.gunframe == fire_frames[n]) {
					Weapon_PowerupSound(ent);
					fire(ent);
					if (!ent->inuse || ent->spawn_count != ent_generation)
						return;
					break;
				}
			}

			// start the animation
			ent->client->anim_priority = ANIM_ATTACK;
			if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
				ent->s.frame = FRAME_crattak1 - 1;
				ent->client->anim_end = FRAME_crattak9;
			} else {
				ent->s.frame = FRAME_attack1 - 1;
				ent->client->anim_end = FRAME_attack8;
			}
			ent->client->anim_time = 0_ms;
		}

		return;
	}

	if (ent->client->weaponstate == WEAPON_FIRING && ent->client->weapon_think_time <= level.time) {
		ent->client->last_firing_time = level.time + COOP_DAMAGE_FIRING_TIME;
		ent->client->ps.gunframe++;
		if (!Weapon_HandleFiring(ent, FRAME_IDLE_FIRST, [&]() {
			for (int n = 0; fire_frames[n]; n++) {
				if (ent->client->ps.gunframe == fire_frames[n]) {
					Weapon_PowerupSound(ent);
					fire(ent);
					break;
				}
			}
			}))
			return;
	}
}

/*
================
Weapon_Repeating
================
*/
void Weapon_Repeating(gentity_t *ent, int FRAME_ACTIVATE_LAST, int FRAME_FIRE_LAST, int FRAME_IDLE_LAST, int FRAME_DEACTIVATE_LAST, const int *pause_frames, void (*fire)(gentity_t *ent)) {
	int FRAME_FIRE_FIRST = (FRAME_ACTIVATE_LAST + 1);
	int FRAME_IDLE_FIRST = (FRAME_FIRE_LAST + 1);
	int FRAME_DEACTIVATE_FIRST = (FRAME_IDLE_LAST + 1);

	if (!Weapon_CanAnimate(ent))
		return;

	if (Weapon_HandleDropping(ent, FRAME_DEACTIVATE_LAST))
		return;
	else if (Weapon_HandleActivating(ent, FRAME_ACTIVATE_LAST, FRAME_IDLE_FIRST))
		return;
	else if (Weapon_HandleNewWeapon(ent, FRAME_DEACTIVATE_FIRST, FRAME_DEACTIVATE_LAST))
		return;
	else if (Weapon_HandleReady(ent, FRAME_FIRE_FIRST, FRAME_IDLE_FIRST, FRAME_IDLE_LAST, pause_frames) == weapon_ready_state_t::changing)
		return;

	if (ent->client->weaponstate == WEAPON_FIRING && ent->client->weapon_think_time <= level.time) {
		ent->client->last_firing_time = level.time + COOP_DAMAGE_FIRING_TIME;
		if (!Weapon_HandleFiring(
				ent, FRAME_IDLE_FIRST, [&]() { fire(ent); }))
			return;

		if (ent->client->weapon_thunk)
			ent->client->weapon_think_time += FRAME_TIME_S;
	}
}

/*
======================================================================

HAND GRENADES

======================================================================
*/

static void Weapon_HandGrenade_Fire(gentity_t *ent, bool held) {
	int	  damage = 125;
	int	  speed;
	float radius = (float)(damage + 40);

	if (is_quad)
		damage *= damage_multiplier;

	vec3_t start, dir;
	// Paril: kill sideways angle on grenades
	// limit upwards angle so you don't throw behind you
	P_ProjectSource(ent, { max(-62.5f, ent->client->v_angle[PITCH]), ent->client->v_angle[YAW], ent->client->v_angle[ROLL] }, { 2, 0, -14 }, start, dir);

	gtime_t timer = ent->client->grenade_time - level.time;
	speed = (int)(ent->health <= 0 ? GRENADE_MINSPEED : min(GRENADE_MINSPEED + (GRENADE_TIMER - timer).seconds() * ((GRENADE_MAXSPEED - GRENADE_MINSPEED) / GRENADE_TIMER.seconds()), GRENADE_MAXSPEED));

	ent->client->grenade_time = 0_ms;

	fire_handgrenade(ent, start, dir, damage, speed, timer, radius, held);

	Stats_AddShot(ent);
	RemoveAmmo(ent, 1);
}

void Throw_Generic(gentity_t *ent, int FRAME_FIRE_LAST, int FRAME_IDLE_LAST, int FRAME_PRIME_SOUND,
	const char *prime_sound,
	int FRAME_THROW_HOLD, int FRAME_THROW_FIRE, const int *pause_frames, int EXPLODE,
	const char *primed_sound,
	void (*fire)(gentity_t *ent, bool held), bool extra_idle_frame) {
	// when we die, just toss what we had in our hands.
	if (ent->health <= 0) {
		fire(ent, true);
		return;
	}

	int n;
	int FRAME_IDLE_FIRST = (FRAME_FIRE_LAST + 1);

	if (ent->client->newweapon && (ent->client->weaponstate == WEAPON_READY)) {
		if (ent->client->weapon_think_time <= level.time) {
			Change_Weapon(ent);
			ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);
		}
		return;
	}

	if (ent->client->weaponstate == WEAPON_ACTIVATING) {
		if (ent->client->weapon_think_time <= level.time) {
			ent->client->weaponstate = WEAPON_READY;
			if (!extra_idle_frame)
				ent->client->ps.gunframe = FRAME_IDLE_FIRST;
			else
				ent->client->ps.gunframe = FRAME_IDLE_LAST + 1;
			ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);
			Weapon_SetFinished(ent);
		}
		return;
	}

	if (ent->client->weaponstate == WEAPON_READY) {
		bool request_firing;

		if (Weapon_CombatDisabled(ent)) {
			request_firing = false;
			ent->client->latched_buttons &= ~BUTTON_ATTACK;
			ent->client->weapon_fire_buffered = false;
		} else
			request_firing = ent->client->weapon_fire_buffered || ((ent->client->latched_buttons | ent->client->buttons) & BUTTON_ATTACK);

		if (request_firing && ent->client->weapon_fire_finished <= level.time) {
			ent->client->latched_buttons &= ~BUTTON_ATTACK;

			if (ent->client->pers.inventory[ent->client->pers.weapon->ammo]) {
				ent->client->ps.gunframe = 1;
				ent->client->weaponstate = WEAPON_FIRING;
				ent->client->grenade_time = 0_ms;
				ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);
			} else
				NoAmmoWeaponChange(ent, true);
			return;
		} else if (ent->client->weapon_think_time <= level.time) {
			ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);

			if (ent->client->ps.gunframe >= FRAME_IDLE_LAST) {
				ent->client->ps.gunframe = FRAME_IDLE_FIRST;
				return;
			}

			if (pause_frames) {
				for (n = 0; pause_frames[n]; n++) {
					if (ent->client->ps.gunframe == pause_frames[n]) {
						if (irandom(16))
							return;
					}
				}
			}

			ent->client->ps.gunframe++;
		}
		return;
	}

	if (ent->client->weaponstate == WEAPON_FIRING) {
		ent->client->last_firing_time = level.time + COOP_DAMAGE_FIRING_TIME;

		if (ent->client->weapon_think_time <= level.time) {
			if (prime_sound && ent->client->ps.gunframe == FRAME_PRIME_SOUND)
				gi.sound(ent, CHAN_WEAPON, gi.soundindex(prime_sound), 1, ATTN_NORM, 0);

			// [Paril-KEX] dualfire/time accel
			gtime_t grenade_wait_time = 1_sec;

			if (Tech_ApplyTimeAccel(ent))
				grenade_wait_time *= 0.5f;
			if (is_haste)
				grenade_wait_time *= 0.5f;
			grenade_wait_time *= Weapon_FireTimeScale(ent);

			if (ent->client->ps.gunframe == FRAME_THROW_HOLD) {
				if (!ent->client->grenade_time && !ent->client->grenade_finished_time)
					ent->client->grenade_time = level.time + GRENADE_TIMER + 200_ms;

				if (primed_sound && !ent->client->grenade_blew_up)
					ent->client->weapon_sound = gi.soundindex(primed_sound);

				// they waited too long, detonate it in their hand
				if (EXPLODE && !ent->client->grenade_blew_up && level.time >= ent->client->grenade_time) {
					Weapon_PowerupSound(ent);
					ent->client->weapon_sound = 0;
					fire(ent, true);

					ent->client->grenade_blew_up = true;

					ent->client->grenade_finished_time = level.time + grenade_wait_time;
				}

				if (ent->client->buttons & BUTTON_ATTACK) {
					ent->client->weapon_think_time = level.time + 1_ms;
					return;
				}

				if (ent->client->grenade_blew_up) {
					if (level.time >= ent->client->grenade_finished_time) {
						ent->client->ps.gunframe = FRAME_FIRE_LAST;
						ent->client->grenade_blew_up = false;
						ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);
					} else {
						return;
					}
				} else {
					ent->client->ps.gunframe++;

					Weapon_PowerupSound(ent);
					ent->client->weapon_sound = 0;
					fire(ent, false);

					if (!EXPLODE || !ent->client->grenade_blew_up)
						ent->client->grenade_finished_time = level.time + grenade_wait_time;

					if (!ent->deadflag && ent->s.modelindex == MODELINDEX_PLAYER && ent->health > 0) // VWep animations screw up corpses
					{
						if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
							ent->client->anim_priority = ANIM_ATTACK;
							ent->s.frame = FRAME_crattak1 - 1;
							ent->client->anim_end = FRAME_crattak3;
						} else {
							ent->client->anim_priority = ANIM_ATTACK | ANIM_REVERSED;
							ent->s.frame = FRAME_wave08;
							ent->client->anim_end = FRAME_wave01;
						}
						ent->client->anim_time = 0_ms;
					}
				}
			}

			ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);

			if ((ent->client->ps.gunframe == FRAME_FIRE_LAST) && (level.time < ent->client->grenade_finished_time))
				return;

			ent->client->ps.gunframe++;

			if (ent->client->ps.gunframe == FRAME_IDLE_FIRST) {
				ent->client->grenade_finished_time = 0_ms;
				ent->client->weaponstate = WEAPON_READY;
				ent->client->weapon_fire_buffered = false;
				Weapon_SetFinished(ent);

				if (extra_idle_frame)
					ent->client->ps.gunframe = FRAME_IDLE_LAST + 1;

				// Paril: if we ran out of the throwable, switch
				// so we don't appear to be holding one that we
				// can't throw
				if (!ent->client->pers.inventory[ent->client->pers.weapon->ammo]) {
					NoAmmoWeaponChange(ent, false);
					Change_Weapon(ent);
				}
			}
		}
	}
}

void Weapon_HandGrenade(gentity_t *ent) {
	constexpr int pause_frames[] = { 29, 34, 39, 48, 0 };

	Throw_Generic(ent, 15, 48, 5, "weapons/hgrena1b.wav", 11, 12, pause_frames, true, "weapons/hgrenc1b.wav", Weapon_HandGrenade_Fire, true);

	// [Paril-KEX] skip the duped frame
	if (ent->client->ps.gunframe == 1)
		ent->client->ps.gunframe = 2;
}

/*
======================================================================

GRENADE LAUNCHER

======================================================================
*/

static void Weapon_GrenadeLauncher_Fire(gentity_t *ent) {
	int		damage;
	float	splash_radius;
	int		speed;

	MM_Ruleset_GrenadeLauncherDefaults(damage, splash_radius, speed);

	if (is_quad)
		damage *= damage_multiplier;

	vec3_t start, dir;
	// Paril: kill sideways angle on grenades
	// muffmode: but why is this the exception? reverted
	// limit upwards angle so you don't fire it behind you
	if (RS(RS_Q3A))
		P_ProjectSourceQ3A(ent, ent->client->v_angle, start, dir);
	else
		P_ProjectSource(ent, { max(-62.5f, ent->client->v_angle[PITCH]), ent->client->v_angle[YAW], ent->client->v_angle[ROLL] }, { 8, 0, -8 }, start, dir);
	if (RS(RS_Q3A)) {
		dir[2] += 0.2f;
		dir.normalize();
	}

	if (!RS(RS_Q3A))
		P_AddWeaponKick(ent, ent->client->v_forward * -2, { -1.f, 0.f, 0.f });

	float right_adjust = RS(RS_Q3A) ? 0.0f : (crandom_open() * 10.0f);
	float up_adjust = RS(RS_Q3A) ? 0.0f : (200 + crandom_open() * 10.0f);
	fire_grenade(ent, start, dir, damage, speed, 2.5_sec, splash_radius, right_adjust, up_adjust, false);

	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte(MZ_GRENADE | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	Stats_AddShot(ent);
	RemoveAmmo(ent, 1);
}

void Weapon_GrenadeLauncher(gentity_t *ent) {
	constexpr int pause_frames[] = { 34, 51, 59, 0 };
	constexpr int fire_frames[] = { 6, 0 };

	Weapon_Generic(ent, 5, RS(RS_Q3A) ? 13 : 16, 59, 64, pause_frames, fire_frames, Weapon_GrenadeLauncher_Fire);
}

/*
======================================================================

ROCKET LAUNCHER

======================================================================
*/

static void Weapon_RocketLauncher_Fire(gentity_t *ent) {
	int	  damage, splash_damage;
	float splash_radius;
	int	  speed;
	int	  splash_knockback;

	// Use dev cvar if enabled, otherwise use ruleset-based defaults
#ifdef _DEBUG
	if (g_weapon_balance_dev && g_weapon_balance_dev->integer && g_rocketlauncher_damage && g_rocketlauncher_damage->integer > 0) {
		damage = g_rocketlauncher_damage->integer;
		MM_Ruleset_RocketLauncherDefaultsForCustomDamage(damage, splash_damage, splash_radius, speed, splash_knockback);
	} else {
#else
	{
#endif
		MM_Ruleset_RocketLauncherDefaults(damage, splash_damage, splash_radius, speed, splash_knockback);
	}
	// Use dev cvar for rocket launcher speed if enabled, otherwise use ruleset-based defaults
#ifdef _DEBUG
	if (g_weapon_balance_dev && g_weapon_balance_dev->integer && g_rocketlauncher_speed && g_rocketlauncher_speed->integer > 0) {
		speed = g_rocketlauncher_speed->integer;
	}
#endif
	speed = MM_Arena_RocketSpeed(ent, speed);
	if (Weapon_ExcessiveEnabled(ent))
		speed *= 1.5;

	if (is_quad) {
		damage *= damage_multiplier;
		splash_damage *= damage_multiplier;
	}

	vec3_t start, dir;
	if (RS(RS_Q3A))
		P_ProjectSourceQ3A(ent, ent->client->v_angle, start, dir);
	else if (RS(RS_Q1))
		P_ProjectSource(ent, ent->client->v_angle, { 8, 0, -6 }, start, dir);
	else
		P_ProjectSource(ent, ent->client->v_angle, { 8, 8, -8 }, start, dir);
	fire_rocket(ent, start, dir, damage, speed, splash_radius, splash_damage, splash_knockback);

	if (!RS(RS_Q3A))
		P_AddWeaponKick(ent, ent->client->v_forward * -2, { -1.f, 0.f, 0.f });

	// send muzzle flash
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte(MZ_ROCKET | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	Stats_AddShot(ent);
	RemoveAmmo(ent, 1);
}

void Weapon_RocketLauncher(gentity_t *ent) {
	constexpr int pause_frames[] = { 25, 33, 42, 50, 0 };
	constexpr int fire_frames[] = { 5, 0 };

	Weapon_Generic(ent, 4, 12, 50, 54, pause_frames, fire_frames, Weapon_RocketLauncher_Fire);
}


/*
======================================================================

GRAPPLE / OFF-HAND HOOK

======================================================================
*/

// Grapple and off-hand hook behavior lives in sgame/client/grapple.cpp.

/*
======================================================================

BLASTER / HYPERBLASTER

======================================================================
*/

static void Weapon_Blaster_Fire(gentity_t *ent, const vec3_t &g_offset, int damage, bool hyper, effects_t effect) {
	if (is_quad)
		damage *= damage_multiplier;

	vec3_t start, dir;
	if (RS(RS_Q3A) && hyper)
		P_ProjectSourceQ3A(ent, ent->client->v_angle, start, dir);
	else
		P_ProjectSource(ent, ent->client->v_angle, vec3_t{ 24, 8, -8 } + g_offset, start, dir);

	if (hyper && !RS(RS_Q3A))
		P_AddWeaponKick(ent, ent->client->v_forward * -2, { crandom() * 0.7f, crandom() * 0.7f, crandom() * 0.7f });
	else if (!hyper)
		P_AddWeaponKick(ent, ent->client->v_forward * -2, { -1.f, 0.f, 0.f });

	// let the regular blaster projectiles travel a bit faster because it is a completely useless gun
	int speed;
	// Use dev cvar for hyperblaster speed if enabled, otherwise use ruleset-based defaults
#ifdef _DEBUG
	if (hyper && g_weapon_balance_dev && g_weapon_balance_dev->integer && g_hyperblaster_speed && g_hyperblaster_speed->integer > 0) {
		speed = g_hyperblaster_speed->integer;
	} else {
#else
	{
#endif
		speed = MM_Ruleset_HyperBlasterSpeed(hyper);
	}

	fire_blaster(ent, start, dir, damage, speed, effect, hyper ? MOD_HYPERBLASTER : MOD_BLASTER);

	// send muzzle flash
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	if (hyper)
		gi.WriteByte(MZ_HYPERBLASTER | is_silenced);
	else
		gi.WriteByte(MZ_BLASTER | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	Stats_AddShot(ent);
}

static void Weapon_Blaster_DoFire(gentity_t *ent) {
	// give the blaster 15 across the board instead of just in dm
	int damage = 15;
	Weapon_Blaster_Fire(ent, vec3_origin, damage, false, EF_BLASTER);
}

void Weapon_Blaster(gentity_t *ent) {
	constexpr int pause_frames[] = { 19, 32, 0 };
	constexpr int fire_frames[] = { 5, 0 };

	Weapon_Generic(ent, 4, 8, 52, 55, pause_frames, fire_frames, Weapon_Blaster_DoFire);
}

static void Weapon_HyperBlaster_Fire(gentity_t *ent) {
	float	rotation;
	int		damage;

	// start on frame 6
	if (ent->client->ps.gunframe > 20)
		ent->client->ps.gunframe = 6;
	else
		ent->client->ps.gunframe++;

	// if we reached end of loop, have ammo & holding attack, reset loop
	// otherwise play wind down
	if (ent->client->ps.gunframe == 12) {
		if (ent->client->pers.inventory[ent->client->pers.weapon->ammo] && (ent->client->buttons & BUTTON_ATTACK))
			ent->client->ps.gunframe = 6;
		else
			gi.sound(ent, CHAN_AUTO, gi.soundindex("weapons/hyprbd1a.wav"), 1, ATTN_NORM, 0);
	}

	// play weapon sound for firing loop
	if (ent->client->ps.gunframe >= 6 && ent->client->ps.gunframe <= 11)
		ent->client->weapon_sound = gi.soundindex("weapons/hyprbl1a.wav");
	else
		ent->client->weapon_sound = 0;
	
	// fire frames
	bool request_firing = ent->client->weapon_fire_buffered || (ent->client->buttons & BUTTON_ATTACK);

	if (request_firing) {
		if (ent->client->ps.gunframe >= 6 && ent->client->ps.gunframe <= 11) {
			vec3_t offset = { 0 };

			ent->client->weapon_fire_buffered = false;

			if (!ent->client->pers.inventory[ent->client->pers.weapon->ammo]) {
				NoAmmoWeaponChange(ent, true);
				return;
			}

			if (!RS(RS_Q3A)) {
				rotation = (ent->client->ps.gunframe - 5) * 2 * PIf / 6;
				offset[0] = -4 * sinf(rotation);
				offset[2] = 0;
				offset[1] = 4 * cosf(rotation);
			}

			// Use dev cvar if enabled, otherwise use ruleset-based defaults
			// Note: hyperblaster damage cvar would go here if we add it, but currently only speed is configurable
			damage = MM_Ruleset_HyperBlasterDamage();

			Weapon_Blaster_Fire(ent, offset, damage, true, (ent->client->ps.gunframe % 4) ? EF_NONE : EF_HYPERBLASTER);
			Weapon_PowerupSound(ent);

			RemoveAmmo(ent, 1);

			ent->client->anim_priority = ANIM_ATTACK;
			if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
				ent->s.frame = FRAME_crattak1 - (int)(frandom() + 0.25f);
				ent->client->anim_end = FRAME_crattak9;
			} else {
				ent->s.frame = FRAME_attack1 - (int)(frandom() + 0.25f);
				ent->client->anim_end = FRAME_attack8;
			}
			ent->client->anim_time = 0_ms;
		}
	}
}

void Weapon_HyperBlaster(gentity_t *ent) {
	constexpr int pause_frames[] = { 0 };

	Weapon_Repeating(ent, 5, 20, 49, 53, pause_frames, Weapon_HyperBlaster_Fire);
}

/*
======================================================================

MACHINEGUN / CHAINGUN

======================================================================
*/

static void Weapon_Machinegun_Fire(gentity_t *ent) {
	int damage = 8;
	int kick = 2;
	int vs, hs;

	// Use dev cvars if enabled, otherwise use ruleset-based defaults
#ifdef _DEBUG
	if (g_weapon_balance_dev && g_weapon_balance_dev->integer) {
		if (g_machinegun_damage && g_machinegun_damage->integer > 0) {
			damage = g_machinegun_damage->integer;
		} else {
			damage = MM_Ruleset_MachinegunDamage();
		}
		int default_hs, default_vs;
		MM_Ruleset_MachinegunSpread(default_hs, default_vs);
		hs = (g_machinegun_hspread && g_machinegun_hspread->integer > 0) ? g_machinegun_hspread->integer : default_hs;
		vs = (g_machinegun_vspread && g_machinegun_vspread->integer > 0) ? g_machinegun_vspread->integer : default_vs;
	} else {
#else
	{
#endif
		damage = MM_Ruleset_MachinegunDamage();
		MM_Ruleset_MachinegunSpread(hs, vs);
	}

	if (!(ent->client->buttons & BUTTON_ATTACK)) {
		ent->client->ps.gunframe = 6;
		return;
	}

	if (ent->client->ps.gunframe == 4)
		ent->client->ps.gunframe = 5;
	else
		ent->client->ps.gunframe = 4;

	if (ent->client->pers.inventory[ent->client->pers.weapon->ammo] < 1) {
		ent->client->ps.gunframe = 6;
		NoAmmoWeaponChange(ent, true);
		return;
	}

	if (is_quad) {
		damage *= damage_multiplier;
		kick *= damage_multiplier;
	}

	vec3_t kick_origin{}, kick_angles{};
	for (size_t i = 0; i < 3; i++) {
		kick_origin[i] = crandom() * 0.35f;
		kick_angles[i] = crandom() * 0.7f;
	}
	if (!RS(RS_Q3A))
		P_AddWeaponKick(ent, kick_origin, kick_angles);

	// get start / end positions
	vec3_t start, dir;
	scoped_lag_compensation_t lag_compensation;
	// Paril: kill sideways angle on hitscan
	if (RS(RS_Q3A))
		P_ProjectSourceQ3AAndLagCompensate(ent, ent->client->v_angle, start, dir, lag_compensation);
	else
		P_ProjectSourceAndLagCompensate(ent, ent->client->v_angle, { 0, 0, -8 }, start, dir, lag_compensation);

	fire_bullet(ent, start, dir, damage, kick, hs, vs, MOD_MACHINEGUN);
	lag_compensation.reset();
	Weapon_PowerupSound(ent);

	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte(MZ_MACHINEGUN | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	Stats_AddShot(ent);
	RemoveAmmo(ent, 1);

	ent->client->anim_priority = ANIM_ATTACK;
	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
		ent->s.frame = FRAME_crattak1 - (int)(frandom() + 0.25f);
		ent->client->anim_end = FRAME_crattak9;
	} else {
		ent->s.frame = FRAME_attack1 - (int)(frandom() + 0.25f);
		ent->client->anim_end = FRAME_attack8;
	}
	ent->client->anim_time = 0_ms;
}

void Weapon_Machinegun(gentity_t *ent) {
	constexpr int pause_frames[] = { 23, 45, 0 };

	Weapon_Repeating(ent, 3, 5, 45, 49, pause_frames, Weapon_Machinegun_Fire);
}

static void Weapon_Chaingun_Fire(gentity_t *ent) {
	int	  i;
	int	  shots;
	float r, u;
	int	  damage;
	
	// Use dev cvars if enabled, otherwise use ruleset-based defaults
#ifdef _DEBUG
	if (g_weapon_balance_dev && g_weapon_balance_dev->integer && g_chaingun_damage && g_chaingun_damage->integer > 0) {
		damage = g_chaingun_damage->integer;
	} else {
#else
	{
#endif
		damage = MM_Ruleset_ChaingunDamage(ent);
	}
	int	  kick = 2;

	if (RS(RS_Q3A)) {
		if (!(ent->client->buttons & BUTTON_ATTACK)) {
			ent->client->ps.gunframe = 32;
			ent->client->weapon_sound = 0;
			return;
		}

		if (ent->client->ps.gunframe > 21 || ent->client->ps.gunframe < 5)
			ent->client->ps.gunframe = 5;
		else
			ent->client->ps.gunframe = ent->client->ps.gunframe == 6 ? 5 : 6;

		shots = min(1, ent->client->pers.inventory[ent->client->pers.weapon->ammo]);
		if (!shots) {
			NoAmmoWeaponChange(ent, true);
			return;
		}

		if (is_quad) {
			damage *= damage_multiplier;
			kick *= damage_multiplier;
		}

		vec3_t start, dir;
		scoped_lag_compensation_t lag_compensation;
		P_ProjectSourceQ3AAndLagCompensate(ent, ent->client->v_angle, start, dir, lag_compensation);

		int hspread, vspread;
		float spread_offset;
		MM_Ruleset_ChaingunSpreadDefaults(hspread, vspread, spread_offset);
		(void)spread_offset;
		for (i = 0; i < shots; i++)
			fire_bullet(ent, start, dir, damage, kick, hspread, vspread, MOD_CHAINGUN);

		lag_compensation.reset();
		Weapon_PowerupSound(ent);

		gi.WriteByte(svc_muzzleflash);
		gi.WriteEntity(ent);
		gi.WriteByte((MZ_CHAINGUN1 + shots - 1) | is_silenced);
		gi.multicast(ent->s.origin, MULTICAST_PVS, false);

		PlayerNoise(ent, start, PNOISE_WEAPON);

		Stats_AddShot(ent, static_cast<uint32_t>(shots));
		RemoveAmmo(ent, shots);

		ent->client->weapon_sound = gi.soundindex("weapons/chngnl1a.wav");
		ent->client->anim_priority = ANIM_ATTACK;
		if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
			ent->s.frame = FRAME_crattak1 - (ent->client->ps.gunframe & 1);
			ent->client->anim_end = FRAME_crattak9;
		} else {
			ent->s.frame = FRAME_attack1 - (ent->client->ps.gunframe & 1);
			ent->client->anim_end = FRAME_attack8;
		}
		ent->client->anim_time = 0_ms;
		return;
	}

	if (ent->client->ps.gunframe > 31) {
		ent->client->ps.gunframe = 5;
		gi.sound(ent, CHAN_AUTO, gi.soundindex("weapons/chngnu1a.wav"), 1, ATTN_IDLE, 0);
	} else if ((ent->client->ps.gunframe == 14) && !(ent->client->buttons & BUTTON_ATTACK)) {
		ent->client->ps.gunframe = 32;
		ent->client->weapon_sound = 0;
		return;
	} else if ((ent->client->ps.gunframe == 21) && (ent->client->buttons & BUTTON_ATTACK) && ent->client->pers.inventory[ent->client->pers.weapon->ammo]) {
		ent->client->ps.gunframe = 15;
	} else {
		ent->client->ps.gunframe++;
	}

	if (ent->client->ps.gunframe == 22) {
		ent->client->weapon_sound = 0;
		gi.sound(ent, CHAN_AUTO, gi.soundindex("weapons/chngnd1a.wav"), 1, ATTN_IDLE, 0);
	}

	if (ent->client->ps.gunframe < 5 || ent->client->ps.gunframe > 21)
		return;

	ent->client->weapon_sound = gi.soundindex("weapons/chngnl1a.wav");

	ent->client->anim_priority = ANIM_ATTACK;
	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
		ent->s.frame = FRAME_crattak1 - (ent->client->ps.gunframe & 1);
		ent->client->anim_end = FRAME_crattak9;
	} else {
		ent->s.frame = FRAME_attack1 - (ent->client->ps.gunframe & 1);
		ent->client->anim_end = FRAME_attack8;
	}
	ent->client->anim_time = 0_ms;

	// Determine shots based on frame, then clamp to max_shots if dev cvar is enabled
	if (ent->client->ps.gunframe <= 9)
		shots = 1;
	else if (ent->client->ps.gunframe <= 14) {
		if (ent->client->buttons & BUTTON_ATTACK)
			shots = 2;
		else
			shots = 1;
	} else
		shots = 3;
	
	// Clamp to max_shots if dev cvar is enabled
#ifdef _DEBUG
	if (g_weapon_balance_dev && g_weapon_balance_dev->integer && g_chaingun_max_shots && g_chaingun_max_shots->integer > 0) {
		int max_shots = g_chaingun_max_shots->integer;
		if (shots > max_shots)
			shots = max_shots;
	}
#endif
	
	if (ent->client->pers.inventory[ent->client->pers.weapon->ammo] < shots)
		shots = ent->client->pers.inventory[ent->client->pers.weapon->ammo];

	if (!shots) {
		NoAmmoWeaponChange(ent, true);
		return;
	}

	if (is_quad) {
		damage *= damage_multiplier;
		kick *= damage_multiplier;
	}

	vec3_t kick_origin{}, kick_angles{};
	for (i = 0; i < 3; i++) {
		kick_origin[i] = crandom() * 0.35f;
		kick_angles[i] = crandom() * (0.5f + (shots * 0.15f));
	}
	P_AddWeaponKick(ent, kick_origin, kick_angles);

	vec3_t start, dir;
	scoped_lag_compensation_t lag_compensation;
	P_ProjectSourceAndLagCompensate(ent, ent->client->v_angle, { 0, 0, -8 }, start, dir, lag_compensation);
	
	// Determine spread values and offset
	int hspread, vspread;
	float spread_offset;
#ifdef _DEBUG
	if (g_weapon_balance_dev && g_weapon_balance_dev->integer) {
		hspread = (g_chaingun_hspread && g_chaingun_hspread->integer > 0) ? g_chaingun_hspread->integer : DEFAULT_BULLET_HSPREAD;
		vspread = (g_chaingun_vspread && g_chaingun_vspread->integer > 0) ? g_chaingun_vspread->integer : DEFAULT_BULLET_VSPREAD;
		spread_offset = (g_chaingun_spread_offset && g_chaingun_spread_offset->value > 0.0f) ? g_chaingun_spread_offset->value : 4.0f;
	} else {
		MM_Ruleset_ChaingunSpreadDefaults(hspread, vspread, spread_offset);
	}
#else
	MM_Ruleset_ChaingunSpreadDefaults(hspread, vspread, spread_offset);
#endif
	
	for (i = 0; i < shots; i++) {
		// get start / end positions
		// Paril: kill sideways angle on hitscan
		r = crandom() * spread_offset;
		u = crandom() * spread_offset;
		P_ProjectSource(ent, ent->client->v_angle, { 0, r, u + -8 }, start, dir);

		fire_bullet(ent, start, dir, damage, kick, hspread, vspread, MOD_CHAINGUN);
	}
	lag_compensation.reset();

	Weapon_PowerupSound(ent);

	// send muzzle flash
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte((MZ_CHAINGUN1 + shots - 1) | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	Stats_AddShot(ent, static_cast<uint32_t>(shots));
	RemoveAmmo(ent, shots);
}

void Weapon_Chaingun(gentity_t *ent) {
	constexpr int pause_frames[] = { 38, 43, 51, 61, 0 };

	Weapon_Repeating(ent, 4, 31, 61, 64, pause_frames, Weapon_Chaingun_Fire);
}

/*
======================================================================

SHOTGUN / SUPERSHOTGUN

======================================================================
*/

static void Weapon_Shotgun_Fire(gentity_t *ent) {
	int damage = MM_Ruleset_ShotgunDamage();
	int kick = 8;

	vec3_t start, dir;
	scoped_lag_compensation_t lag_compensation;
	// Paril: kill sideways angle on hitscan
	if (RS(RS_Q3A))
		P_ProjectSourceQ3AAndLagCompensate(ent, ent->client->v_angle, start, dir, lag_compensation);
	else
		P_ProjectSourceAndLagCompensate(ent, ent->client->v_angle, { 0, 0, -8 }, start, dir, lag_compensation);

	if (!RS(RS_Q3A))
		P_AddWeaponKick(ent, ent->client->v_forward * -2, { -2.f, 0.f, 0.f });

	if (is_quad) {
		damage *= damage_multiplier;
		kick *= damage_multiplier;
	}

	int pellets = MM_Ruleset_ShotgunPelletCount();
	int spread = MM_Ruleset_ShotgunSpread();
	fire_shotgun(ent, start, dir, damage, kick, spread, spread, pellets, MOD_SHOTGUN);
	lag_compensation.reset();

	// send muzzle flash
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte(MZ_SHOTGUN | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	Stats_AddShot(ent, static_cast<uint32_t>(pellets));
	RemoveAmmo(ent, 1);
}

void Weapon_Shotgun(gentity_t *ent) {
	constexpr int pause_frames[] = { 22, 28, 34, 0 };
	constexpr int fire_frames[] = { 8, 0 };

	Weapon_Generic(ent, 7, RS(RS_Q3A) ? 17 : 18, 36, 39, pause_frames, fire_frames, Weapon_Shotgun_Fire);
}

static void Weapon_SuperShotgun_Fire(gentity_t *ent) {
	item_id_t ammo_id = MM_Ruleset_WeaponAmmoId(ent->client->pers.weapon);

	if (MM_Ruleset_SuperShotgunFallsBackToSingleShell() && ammo_id && ent->client->pers.inventory[ammo_id] == 1) {
		Weapon_Shotgun_Fire(ent);
		return;
	}

	int damage = MM_Ruleset_SuperShotgunDamage();
	int kick = 12;

	if (is_quad) {
		damage *= damage_multiplier;
		kick *= damage_multiplier;
	}

	vec3_t start, dir;
	scoped_lag_compensation_t lag_compensation;
	// Paril: kill sideways angle on hitscan
	P_ProjectSourceAndLagCompensate(ent, ent->client->v_angle, { 0, 0, -8 }, start, dir, lag_compensation);

	int pellets = MM_Ruleset_SuperShotgunPelletCount();
	int hspread, vspread;
	MM_Ruleset_SuperShotgunSpread(hspread, vspread);

	if (RS(RS_Q1)) {
		fire_shotgun(ent, start, dir, damage, kick, hspread, vspread, pellets, MOD_SSHOTGUN);
	} else {
		vec3_t v;
		v[PITCH] = ent->client->v_angle[PITCH];
		v[YAW] = ent->client->v_angle[YAW] - 5;
		v[ROLL] = ent->client->v_angle[ROLL];
		// Paril: kill sideways angle on hitscan
		P_ProjectSource(ent, v, { 0, 0, -8 }, start, dir);
		fire_shotgun(ent, start, dir, damage, kick, hspread, vspread, pellets / 2, MOD_SSHOTGUN);
		v[YAW] = ent->client->v_angle[YAW] + 5;
		P_ProjectSource(ent, v, { 0, 0, -8 }, start, dir);
		fire_shotgun(ent, start, dir, damage, kick, hspread, vspread, pellets / 2, MOD_SSHOTGUN);
	}
	lag_compensation.reset();

	P_AddWeaponKick(ent, ent->client->v_forward * -2, { -2.f, 0.f, 0.f });

	// send muzzle flash
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte(MZ_SSHOTGUN | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	Stats_AddShot(ent, static_cast<uint32_t>(pellets));
	RemoveAmmo(ent, 2);
}

void Weapon_SuperShotgun(gentity_t *ent) {
	constexpr int pause_frames[] = { 29, 42, 57, 0 };
	constexpr int fire_frames[] = { 7, 0 };

	Weapon_Generic(ent, 6, 17, 57, 61, pause_frames, fire_frames, Weapon_SuperShotgun_Fire);
}

/*
======================================================================

RAILGUN

======================================================================
*/

static void Weapon_Railgun_Fire(gentity_t *ent) {
	// Use dev cvar if enabled, otherwise use default values
	int damage;
#ifdef _DEBUG
	if (g_weapon_balance_dev && g_weapon_balance_dev->integer && g_railgun_damage && g_railgun_damage->integer > 0) {
		damage = g_railgun_damage->integer;
	} else {
#else
	{
#endif
		damage = MM_Ruleset_RailgunDamage();
	}
	int kick = MM_Ruleset_RailgunKick(damage);

	if (is_quad) {
		damage *= damage_multiplier;
		kick *= damage_multiplier;
	}

	vec3_t start, dir;
	scoped_lag_compensation_t lag_compensation;
	if (RS(RS_Q3A))
		P_ProjectSourceQ3AAndLagCompensate(ent, ent->client->v_angle, start, dir, lag_compensation);
	else
		P_ProjectSourceAndLagCompensate(ent, ent->client->v_angle, { 0, 7, -8 }, start, dir, lag_compensation);
	fire_rail(ent, start, dir, damage, kick);
	lag_compensation.reset();

	if (!RS(RS_Q3A))
		P_AddWeaponKick(ent, ent->client->v_forward * -3, { -3.f, 0.f, 0.f });

	// send muzzle flash
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte(MZ_RAILGUN | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	Stats_AddShot(ent);
	RemoveAmmo(ent, 1);
}

void Weapon_Railgun(gentity_t *ent) {
	constexpr int pause_frames[] = { 56, 0 };
	constexpr int fire_frames[] = { 4, 0 };

	Weapon_Generic(ent, 3, 18, 56, 61, pause_frames, fire_frames, Weapon_Railgun_Fire);
}

/*
======================================================================

BFG10K

======================================================================
*/
static void Weapon_BFG_Fire(gentity_t *ent) {
	bool	q3 = MM_Ruleset_BFGUsesQ3Style();
	int		damage, speed;
	float	splash_radius;

	MM_Ruleset_BFGDefaults(damage, splash_radius, speed);

	if (!q3 && ent->client->ps.gunframe == 9) {
		// send muzzle flash
		gi.WriteByte(svc_muzzleflash);
		gi.WriteEntity(ent);
		gi.WriteByte(MZ_BFG | is_silenced);
		gi.multicast(ent->s.origin, MULTICAST_PVS, false);

		PlayerNoise(ent, ent->s.origin, PNOISE_WEAPON);
		return;
	}

	// cells can go down during windup (from power armor hits), so
	// check again and abort firing if we don't have enough now
	if (ent->client->pers.inventory[ent->client->pers.weapon->ammo] < MM_Ruleset_WeaponAmmoRequired(ent->client->pers.weapon))
		return;

	if (is_quad)
		damage *= damage_multiplier;

	vec3_t start, dir;
	if (q3)
		P_ProjectSourceQ3A(ent, ent->client->v_angle, start, dir);
	else
		P_ProjectSource(ent, ent->client->v_angle, { 8, 8, -8 }, start, dir);
	fire_bfg(ent, start, dir, damage, speed, splash_radius);

	if (!q3) {
		P_AddWeaponKick(ent, ent->client->v_forward * -2, { -20.f, 0, crandom() * 8 });
		ent->client->kick.total = DAMAGE_TIME();
		ent->client->kick.time = level.time + ent->client->kick.total;
	}

	// send muzzle flash
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte(MZ_BFG2 | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	Stats_AddShot(ent);
	RemoveAmmo(ent, MM_Ruleset_BFGAmmoPerShot());
}

void Weapon_BFG(gentity_t *ent) {
	const int pause_frames[] = { 39, 45, 50, 55, 0 };
	const int fire_frames[] = { 9, 17, 0 };
	const int fire_frames_q3a[] = { 9, 0 };

	Weapon_Generic(ent, 8, MM_Ruleset_BFGUsesQ3Style() ? 10 : 32, 54, 58, pause_frames, MM_Ruleset_BFGUsesQ3Style() ? fire_frames_q3a : fire_frames, Weapon_BFG_Fire);
}

/*
======================================================================

PROX MINES

======================================================================
*/
static void Weapon_ProxLauncher_Fire(gentity_t *ent) {
	vec3_t start, dir;
	// Paril: kill sideways angle on grenades
	// limit upwards angle so you don't fire behind you
	P_ProjectSource(ent, { max(-62.5f, ent->client->v_angle[PITCH]), ent->client->v_angle[YAW], ent->client->v_angle[ROLL] }, { 8, 8, -8 }, start, dir);

	P_AddWeaponKick(ent, ent->client->v_forward * -2, { -1.f, 0.f, 0.f });

	fire_prox(ent, start, dir, damage_multiplier, 600);

	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte(MZ_PROX | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	Stats_AddShot(ent);
	RemoveAmmo(ent, 1);
}

void Weapon_ProxLauncher(gentity_t *ent) {
	constexpr int pause_frames[] = { 34, 51, 59, 0 };
	constexpr int fire_frames[] = { 6, 0 };

	Weapon_Generic(ent, 5, 16, 59, 64, pause_frames, fire_frames, Weapon_ProxLauncher_Fire);
}


/*
======================================================================

TESLA MINES

======================================================================
*/
static void Weapon_Tesla_Fire(gentity_t *ent, bool held) {
	vec3_t start, dir;
	// Paril: kill sideways angle on grenades
	// limit upwards angle so you don't throw behind you
	P_ProjectSource(ent, { max(-62.5f, ent->client->v_angle[PITCH]), ent->client->v_angle[YAW], ent->client->v_angle[ROLL] }, { 0, 0, -22 }, start, dir);

	gtime_t timer = ent->client->grenade_time - level.time;
	int	  speed = (int)(ent->health <= 0 ? GRENADE_MINSPEED : min(GRENADE_MINSPEED + (GRENADE_TIMER - timer).seconds() * ((GRENADE_MAXSPEED - GRENADE_MINSPEED) / GRENADE_TIMER.seconds()), GRENADE_MAXSPEED));

	ent->client->grenade_time = 0_ms;

	fire_tesla(ent, start, dir, damage_multiplier, speed);

	Stats_AddShot(ent);
	RemoveAmmo(ent, 1);
}

void Weapon_Tesla(gentity_t *ent) {
	constexpr int pause_frames[] = { 21, 0 };

	Throw_Generic(ent, 8, 32, -1, nullptr, 1, 2, pause_frames, false, nullptr, Weapon_Tesla_Fire, false);
}

/*
======================================================================

CHAINFIST

======================================================================
*/
constexpr int32_t CHAINFIST_REACH = 32;	// 24;

static void Weapon_ChainFist_Fire(gentity_t *ent) {
	if (!(ent->client->buttons & BUTTON_ATTACK)) {
		if (ent->client->ps.gunframe == 13 ||
			ent->client->ps.gunframe == 23 ||
			ent->client->ps.gunframe >= 32) {
			ent->client->ps.gunframe = 33;
			return;
		}
	}

	int damage = MM_Ruleset_ChainfistDamage();
	int kick = MM_Ruleset_ChainfistKick();
	bool attack_frame = !RS(RS_Q3A) || ((ent->client->ps.gunframe - 7) % 4 == 0);

	if (is_quad)
		damage *= damage_multiplier;

	// set start point
	vec3_t start, dir;

	if (RS(RS_Q3A))
		P_ProjectSourceQ3A(ent, ent->client->v_angle, start, dir);
	else
		P_ProjectSource(ent, ent->client->v_angle, { 0, 0, -4 }, start, dir);

	if (attack_frame) {
		bool hit = false;
		bool enhanced_lag_compensation = g_lag_compensation_enhanced->integer != 0;
		scoped_lag_compensation_t lag_compensation;

		if (enhanced_lag_compensation) {
			if (RS(RS_Q3A))
				P_ProjectSourceQ3AAndLagCompensate(ent, ent->client->v_angle, start, dir, lag_compensation);
			else
				P_ProjectSourceAndLagCompensate(ent, ent->client->v_angle, { 0, 0, -4 }, start, dir, lag_compensation);
		}

		if (RS(RS_Q3A)) {
			trace_t tr = Weapon_ArenaTraceline(start,
				start + (dir * CHAINFIST_REACH), ent, MASK_SHOT, ent);

			if (!(tr.surface && (tr.surface->flags & SURF_SKY)) &&
				tr.ent && tr.ent->takedamage &&
				(notGT(GT_ARENA) || MM_Arena_CanInteract(ent, tr.ent))) {
				T_Damage(tr.ent, ent, ent, dir, tr.endpos, tr.plane.normal, damage, kick, DAMAGE_NONE, MOD_CHAINFIST);
				hit = true;
			}
		} else {
			hit = fire_player_melee(ent, start, dir, CHAINFIST_REACH, damage, kick, MOD_CHAINFIST);
		}

		lag_compensation.reset();

		if (hit && ent->client->empty_click_sound < level.time) {
			ent->client->empty_click_sound = level.time + 500_ms;
			gi.sound(ent, CHAN_WEAPON, gi.soundindex("weapons/sawslice.wav"), 1.f, ATTN_NORM, 0.f);
		}
	}

	PlayerNoise(ent, start, PNOISE_WEAPON);

	ent->client->ps.gunframe++;

	if (ent->client->buttons & BUTTON_ATTACK) {
		if (ent->client->ps.gunframe == 12)
			ent->client->ps.gunframe = 14;
		else if (ent->client->ps.gunframe == 22)
			ent->client->ps.gunframe = 24;
		else if (ent->client->ps.gunframe >= 32)
			ent->client->ps.gunframe = 7;
	}

	// start the animation
	if (ent->client->anim_priority != ANIM_ATTACK || frandom() < 0.25f) {
		ent->client->anim_priority = ANIM_ATTACK;
		if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
			ent->s.frame = FRAME_crattak1 - 1;
			ent->client->anim_end = FRAME_crattak9;
		} else {
			ent->s.frame = FRAME_attack1 - 1;
			ent->client->anim_end = FRAME_attack8;
		}
		ent->client->anim_time = 0_ms;
	}
}

// this spits out some smoke from the motor. it's a two-stroke, you know.
static void Weapon_ChainFist_smoke(gentity_t *ent) {
	vec3_t tempVec, dir;
	P_ProjectSource(ent, ent->client->v_angle, { 8, 8, -4 }, tempVec, dir);

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_CHAINFIST_SMOKE);
	gi.WritePosition(tempVec);
	gi.unicast(ent, 0);
}

void Weapon_ChainFist(gentity_t *ent) {
	constexpr int pause_frames[] = { 0 };

	Weapon_Repeating(ent, 4, 32, 57, 60, pause_frames, Weapon_ChainFist_Fire);

	// smoke on idle sequence
	if (ent->client->ps.gunframe == 42 && irandom(8)) {
		if ((ent->client->pers.hand != CENTER_HANDED) && frandom() < 0.4f)
			Weapon_ChainFist_smoke(ent);
	} else if (ent->client->ps.gunframe == 51 && irandom(8)) {
		if ((ent->client->pers.hand != CENTER_HANDED) && frandom() < 0.4f)
			Weapon_ChainFist_smoke(ent);
	}

	// set the appropriate weapon sound.
	if (ent->client->weaponstate == WEAPON_FIRING)
		ent->client->weapon_sound = gi.soundindex("weapons/sawhit.wav");
	else if (ent->client->weaponstate == WEAPON_DROPPING)
		ent->client->weapon_sound = 0;
	else if (ent->client->pers.weapon->id == IT_WEAPON_CHAINFIST)
		ent->client->weapon_sound = gi.soundindex("weapons/sawidle.wav");
}

/*
======================================================================

DISRUPTOR

======================================================================
*/
static void Weapon_Disruptor_Fire(gentity_t *ent) {
	vec3_t	 end;
	gentity_t *enemy;
	trace_t	 tr;
	int		 damage;
	vec3_t	 mins, maxs;

	// PMM - felt a little high at 25
	damage = deathmatch->integer ? 45 : 135;

	if (is_quad)
		damage *= damage_multiplier; // pgm

	mins = { -16, -16, -16 };
	maxs = { 16, 16, 16 };

	vec3_t start, dir;
	bool enhanced_lag_compensation = g_lag_compensation_enhanced->integer != 0;
	scoped_lag_compensation_t lag_compensation;
	P_ProjectSourceAndLagCompensate(ent, ent->client->v_angle, { 24, 8, -8 }, start, dir, lag_compensation);

	end = start + (dir * 8192);
	enemy = nullptr;
	// PMM - doing two traces .. one point and one box.
	contents_t mask = MASK_PROJECTILE;

	// [Paril-KEX]
	if (!G_ShouldPlayersCollide(true))
		mask &= ~CONTENTS_PLAYER;

	tr = Weapon_ArenaTraceline(start, end, ent, mask, ent);
	if (tr.ent != world) {
		if ((notGT(GT_ARENA) || MM_Arena_CanInteract(ent, tr.ent)) &&
			((tr.ent->svflags & SVF_MONSTER) || tr.ent->client || (tr.ent->flags & FL_DAMAGEABLE))) {
			if (tr.ent->health > 0)
				enemy = tr.ent;
		}
	} else {
		if (!enhanced_lag_compensation)
			lag_compensation.reset();

		tr = Weapon_ArenaBoxTrace(start, mins, maxs, end, ent, mask, ent);
		if (tr.ent != world) {
			if ((notGT(GT_ARENA) || MM_Arena_CanInteract(ent, tr.ent)) &&
				((tr.ent->svflags & SVF_MONSTER) || tr.ent->client || (tr.ent->flags & FL_DAMAGEABLE))) {
				if (tr.ent->health > 0)
					enemy = tr.ent;
			}
		}
	}
	if (enhanced_lag_compensation || tr.ent != world)
		lag_compensation.reset();

	P_AddWeaponKick(ent, ent->client->v_forward * -2, { -1.f, 0.f, 0.f });

	fire_disruptor(ent, start, dir, damage, 1000, enemy);

	// send muzzle flash
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte(MZ_TRACKER | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	Stats_AddShot(ent);
	RemoveAmmo(ent, 1);
}

void Weapon_Disruptor(gentity_t *ent) {
	constexpr int pause_frames[] = { 14, 19, 23, 0 };
	constexpr int fire_frames[] = { 5, 0 };

	Weapon_Generic(ent, 4, 9, 29, 34, pause_frames, fire_frames, Weapon_Disruptor_Fire);
}

/*
======================================================================

ETF RIFLE

======================================================================
*/
static void Weapon_ETF_Rifle_Fire(gentity_t *ent) {
	int	   damage = 10;
	int	   kick = 3;
	int	   i;
	vec3_t offset;

	if (!(ent->client->buttons & BUTTON_ATTACK)) {
		ent->client->ps.gunframe = 8;
		return;
	}

	if (ent->client->ps.gunframe == 6)
		ent->client->ps.gunframe = 7;
	else
		ent->client->ps.gunframe = 6;

	if (ent->client->pers.inventory[ent->client->pers.weapon->ammo] < ent->client->pers.weapon->quantity) {
		ent->client->ps.gunframe = 8;
		NoAmmoWeaponChange(ent, true);
		return;
	}

	if (is_quad) {
		damage *= damage_multiplier;
		kick *= damage_multiplier;
	}

	vec3_t kick_origin{}, kick_angles{};
	for (i = 0; i < 3; i++) {
		kick_origin[i] = crandom() * 0.85f;
		kick_angles[i] = crandom() * 0.85f;
	}
	P_AddWeaponKick(ent, kick_origin, kick_angles);

	// get start / end positions
	if (ent->client->ps.gunframe == 6)
		offset = { 15, 8, -8 };
	else
		offset = { 15, 6, -8 };

	vec3_t start, dir;
	P_ProjectSource(ent, ent->client->v_angle + kick_angles, offset, start, dir);
	fire_flechette(ent, start, dir, damage, 1150, kick);
	Weapon_PowerupSound(ent);

	// send muzzle flash
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte((ent->client->ps.gunframe == 6 ? MZ_ETF_RIFLE : MZ_ETF_RIFLE_2) | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	Stats_AddShot(ent);
	RemoveAmmo(ent, 1);

	ent->client->anim_priority = ANIM_ATTACK;
	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
		ent->s.frame = FRAME_crattak1 - (int)(frandom() + 0.25f);
		ent->client->anim_end = FRAME_crattak9;
	} else {
		ent->s.frame = FRAME_attack1 - (int)(frandom() + 0.25f);
		ent->client->anim_end = FRAME_attack8;
	}
	ent->client->anim_time = 0_ms;
}

void Weapon_ETF_Rifle(gentity_t *ent) {
	constexpr int pause_frames[] = { 18, 28, 0 };

	Weapon_Repeating(ent, 4, 7, 37, 41, pause_frames, Weapon_ETF_Rifle_Fire);
}

/*
======================================================================

PLASMA BEAM

======================================================================
*/

constexpr int32_t Q1_PLASMABEAM_DISCHARGE_DAMAGE_PER_CELL = 35;

static bool Weapon_Q1PlasmaBeamDischarge(gentity_t *ent, int cells) {
	if (!RS(RS_Q1) || ent->waterlevel < WATER_WAIST)
		return false;

	if (cells <= 0)
		return false;

	int damage = Q1_PLASMABEAM_DISCHARGE_DAMAGE_PER_CELL * cells;
	if (is_quad)
		damage *= damage_multiplier;

	ent->client->ps.gunframe = 13;
	ent->client->weapon_sound = 0;
	ent->client->ps.gunskin = 0;

	Stats_AddShot(ent);
	RemoveAmmo(ent, cells);
	NoAmmoWeaponChange(ent, false);
	const int32_t discharge_generation = ent->spawn_count;
	T_RadiusDamage(ent, ent, (float)damage, world, (float)(damage + 40), DAMAGE_ENERGY, MOD_PLASMABEAM);
	if (!ent->inuse || ent->spawn_count != discharge_generation)
		return true;
	PlayerNoise(ent, ent->s.origin, PNOISE_WEAPON);
	return true;
}

static void Weapon_PlasmaBeam_Fire(gentity_t *ent) {
	bool firing = (ent->client->buttons & BUTTON_ATTACK) &&
		!Weapon_CombatDisabled(ent);
	item_id_t ammo_id = MM_Ruleset_WeaponAmmoId(ent->client->pers.weapon);
	int ammo_per_shot = MM_Ruleset_PlasmaBeamAmmoPerShot();
	int ammo_count = ammo_id ? ent->client->pers.inventory[ammo_id] : 0;
	bool has_ammo = ammo_count >= ammo_per_shot;

	if (!firing || !has_ammo) {
		ent->client->ps.gunframe = 13;
		ent->client->weapon_sound = 0;
		ent->client->ps.gunskin = 0;

		if (firing && !has_ammo)
			NoAmmoWeaponChange(ent, true);
		return;
	}

	if (Weapon_Q1PlasmaBeamDischarge(ent, ammo_count))
		return;

	// start on frame 8
	if (ent->client->ps.gunframe > 12)
		ent->client->ps.gunframe = 8;
	else
		ent->client->ps.gunframe++;

	if (ent->client->ps.gunframe == 12)
		ent->client->ps.gunframe = 8;

	// play weapon sound for firing
	ent->client->weapon_sound = gi.soundindex("weapons/bfg__l1a.wav");
	ent->client->ps.gunskin = 1;

	int damage;
	int kick;

	// for comparison, the hyperblaster is 15/20
	// jim requested more damage, so try 15/15 --- PGM 07/23/98
	// muffmode: jim you are a silly boy, 15 is way OP for DM
	MM_Ruleset_PlasmaBeamDefaults(damage, kick);

	if (is_quad) {
		damage *= damage_multiplier;
		kick *= damage_multiplier;
	}

	ent->client->kick.time = 0_ms;

	// This offset is the "view" offset for the beam start (used by trace)
	vec3_t start, dir;
	bool enhanced_lag_compensation = g_lag_compensation_enhanced->integer != 0;
	scoped_lag_compensation_t lag_compensation;
	if (RS(RS_Q3A)) {
		if (enhanced_lag_compensation)
			P_ProjectSourceQ3AAndLagCompensate(ent, ent->client->v_angle, start, dir, lag_compensation);
		else
			P_ProjectSourceQ3A(ent, ent->client->v_angle, start, dir);
	} else if (RS(RS_Q1)) {
		if (enhanced_lag_compensation)
			P_ProjectSourceAndLagCompensate(ent, ent->client->v_angle, { 0, 0, -8 }, start, dir, lag_compensation);
		else
			P_ProjectSource(ent, ent->client->v_angle, { 0, 0, -8 }, start, dir);
	} else {
		if (enhanced_lag_compensation)
			P_ProjectSourceAndLagCompensate(ent, ent->client->v_angle, { 7, 2, -3 }, start, dir, lag_compensation);
		else
			P_ProjectSource(ent, ent->client->v_angle, { 7, 2, -3 }, start, dir);
	}

	// This offset is the entity offset
	fire_plasmabeam(ent, start, dir, RS(RS_Q3A) ? vec3_origin : vec3_t{ 2, 7, -3 }, damage, kick, false);
	lag_compensation.reset();
	Weapon_PowerupSound(ent);

	// send muzzle flash
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte(MZ_HEATBEAM | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	Stats_AddShot(ent);
	RemoveAmmo(ent, ammo_per_shot);

	ent->client->anim_priority = ANIM_ATTACK;
	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
		ent->s.frame = FRAME_crattak1 - (int)(frandom() + 0.25f);
		ent->client->anim_end = FRAME_crattak9;
	} else {
		ent->s.frame = FRAME_attack1 - (int)(frandom() + 0.25f);
		ent->client->anim_end = FRAME_attack8;
	}
	ent->client->anim_time = 0_ms;
}

void Weapon_PlasmaBeam(gentity_t *ent) {
	constexpr int pause_frames[] = { 35, 0 };

	Weapon_Repeating(ent, 8, 12, 42, 47, pause_frames, Weapon_PlasmaBeam_Fire);
}


/*
======================================================================

ION RIPPER

======================================================================
*/
static void Weapon_IonRipper_Fire(gentity_t *ent) {
	vec3_t tempang;
	int	   damage = MM_Ruleset_IonRipperDamage();

	if (is_quad)
		damage *= damage_multiplier;

	if (RS(RS_Q3A)) {
		vec3_t start, forward, right, up;
		P_ProjectSourceQ3A(ent, ent->client->v_angle, start, forward);
		AngleVectors(ent->client->v_angle, forward, right, up);

		int projectile_count = MM_Ruleset_IonRipperProjectileCount();
		int spread = MM_Ruleset_IonRipperSpread();
		int min_speed = MM_Ruleset_IonRipperMinSpeed();
		int speed_range = MM_Ruleset_IonRipperSpeedRange();

		for (int i = 0; i < projectile_count; i++) {
			float angle = frandom(2 * PIf);
			float r = cosf(angle) * crandom() * spread * 16.0f;
			float u = sinf(angle) * crandom() * spread * 16.0f;
			vec3_t end = start + (forward * 8192 * 16) + (right * r) + (up * u);
			vec3_t dir = (end - start).normalized();
			int speed = min_speed + (speed_range > 0 ? (int)frandom((float)speed_range) : 0);

			fire_ionripper(ent, start, dir, damage, speed, EF_IONRIPPER);
		}

		gi.WriteByte(svc_muzzleflash);
		gi.WriteEntity(ent);
		gi.WriteByte(MZ_IONRIPPER | is_silenced);
		gi.multicast(ent->s.origin, MULTICAST_PVS, false);

		PlayerNoise(ent, start, PNOISE_WEAPON);

		Stats_AddShot(ent, static_cast<uint32_t>(projectile_count));
		RemoveAmmo(ent, 1);
		return;
	}

	tempang = ent->client->v_angle;
	tempang[YAW] += crandom();

	vec3_t start, dir;
	P_ProjectSource(ent, tempang, { 16, 7, -8 }, start, dir);

	P_AddWeaponKick(ent, ent->client->v_forward * -3, { -3.f, 0.f, 0.f });

	fire_ionripper(ent, start, dir, damage, MM_Ruleset_IonRipperSpeed(), EF_IONRIPPER);

	// send muzzle flash
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte(MZ_IONRIPPER | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);

	Stats_AddShot(ent);
	RemoveAmmo(ent, 1);
}

void Weapon_IonRipper(gentity_t *ent) {
	constexpr int pause_frames[] = { 36, 0 };
	constexpr int fire_frames[] = { 6, 0 };

	Weapon_Generic(ent, 5, RS(RS_Q3A) ? 15 : 7, 36, 39, pause_frames, fire_frames, Weapon_IonRipper_Fire);
}

/*
======================================================================

PHALANX

======================================================================
*/
static void Weapon_Phalanx_Fire(gentity_t *ent) {
	vec3_t v;
	int	   damage;
	float  splash_radius;
	int	   splash_damage;

	damage = irandom(70, 80);
	splash_damage = 120;
	splash_radius = 120;

	if (is_quad) {
		damage *= damage_multiplier;
		splash_damage *= damage_multiplier;
	}

	vec3_t dir;

	if (ent->client->ps.gunframe == 8) {
		v[PITCH] = ent->client->v_angle[PITCH];
		v[YAW] = ent->client->v_angle[YAW] - 1.5f;
		v[ROLL] = ent->client->v_angle[ROLL];

		vec3_t start;
		P_ProjectSource(ent, v, { 0, 8, -8 }, start, dir);

		splash_damage = 30;
		splash_radius = 120;

		fire_phalanx(ent, start, dir, damage, 725, splash_radius, splash_damage);

		// send muzzle flash
		gi.WriteByte(svc_muzzleflash);
		gi.WriteEntity(ent);
		gi.WriteByte(MZ_PHALANX2 | is_silenced);
		gi.multicast(ent->s.origin, MULTICAST_PVS, false);

		Stats_AddShot(ent, 2);
		RemoveAmmo(ent, 1);
	} else {
		v[PITCH] = ent->client->v_angle[PITCH];
		v[YAW] = ent->client->v_angle[YAW] + 1.5f;
		v[ROLL] = ent->client->v_angle[ROLL];

		vec3_t start;
		P_ProjectSource(ent, v, { 0, 8, -8 }, start, dir);

		fire_phalanx(ent, start, dir, damage, 725, splash_radius, splash_damage);

		// send muzzle flash
		gi.WriteByte(svc_muzzleflash);
		gi.WriteEntity(ent);
		gi.WriteByte(MZ_PHALANX | is_silenced);
		gi.multicast(ent->s.origin, MULTICAST_PVS, false);

		PlayerNoise(ent, start, PNOISE_WEAPON);
	}

	P_AddWeaponKick(ent, ent->client->v_forward * -2, { -2.f, 0.f, 0.f });
}

void Weapon_Phalanx(gentity_t *ent) {
	constexpr int pause_frames[] = { 29, 42, 55, 0 };
	constexpr int fire_frames[] = { 7, 8, 0 };

	Weapon_Generic(ent, 5, 20, 58, 63, pause_frames, fire_frames, Weapon_Phalanx_Fire);
}

/*
======================================================================

TRAP

======================================================================
*/

constexpr gtime_t TRAP_TIMER = 5_sec;
constexpr float TRAP_MINSPEED = 300.f;
constexpr float TRAP_MAXSPEED = 700.f;

static void Weapon_Trap_Fire(gentity_t *ent, bool held) {
	int	  speed;

	vec3_t start, dir;
	// Paril: kill sideways angle on grenades
	// limit upwards angle so you don't throw behind you
	P_ProjectSource(ent, { max(-62.5f, ent->client->v_angle[PITCH]), ent->client->v_angle[YAW], ent->client->v_angle[ROLL] }, { 8, 0, -8 }, start, dir);

	gtime_t timer = ent->client->grenade_time - level.time;
	speed = (int)(ent->health <= 0 ? TRAP_MINSPEED : min(TRAP_MINSPEED + (TRAP_TIMER - timer).seconds() * ((TRAP_MAXSPEED - TRAP_MINSPEED) / TRAP_TIMER.seconds()), TRAP_MAXSPEED));

	ent->client->grenade_time = 0_ms;

	fire_trap(ent, start, dir, speed);

	Stats_AddShot(ent);
	RemoveAmmo(ent, 1);
}

void Weapon_Trap(gentity_t *ent) {
	constexpr int pause_frames[] = { 29, 34, 39, 48, 0 };

	Throw_Generic(ent, 15, 48, 5, "weapons/trapcock.wav", 11, 12, pause_frames, false, "weapons/traploop.wav", Weapon_Trap_Fire, false);
}
