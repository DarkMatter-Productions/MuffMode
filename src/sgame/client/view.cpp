// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "monsters/m_player.h"
#include "bots/bot_includes.h"
// [MuffMode] AutoDoc regen lives in muffmode/mm_items_rules
#include "muffmode/mm_items_rules.h"

static gentity_t *current_player;
static gclient_t *current_client;

static vec3_t forward, right, up;
float		  xyspeed;

float bobmove;
int	  bobcycle, bobcycle_run;	  // odd cycles are right foot going forward
float bobfracsin; // sinf(bobfrac*M_PI)

/*
===============
SkipViewModifiers
===============
*/
static inline bool SkipViewModifiers() {
	if (g_skip_view_modifiers->integer && g_cheats->integer)
		return true;

	// don't do bobbing, etc on grapple
	if (current_client->grapple_ent &&
			current_client->grapple_state > GRAPPLE_STATE_FLY) {
		return true;
	}

	// spectator mode
	if (!ClientIsPlaying(current_client))
		return true;

	return false;
}

/*
===============
G_CalcRoll

===============
*/
static float G_CalcRoll(const vec3_t &angles, const vec3_t &velocity) {
	if (SkipViewModifiers()) {
		return 0.0f;
	}

	float sign;
	float side;
	float value;

	side = velocity.dot(right);
	sign = side < 0 ? -1.f : 1.f;
	side = fabsf(side);

	value = g_rollangle->value;

	if (side < g_rollspeed->value)
		side = side * value / g_rollspeed->value;
	else
		side = value;

	return side * sign;
}

/*
===============
P_DamageFeedback

Handles color blends and view kicks
===============
*/
void P_DamageFeedback(gentity_t *player) {
	gclient_t *client;
	float			 side;
	float			 realcount, count, kick;
	vec3_t			 v;
	int				 l;
	constexpr vec3_t armor_color = { 1.0, 1.0, 1.0 };
	constexpr vec3_t power_color = { 0.0, 1.0, 0.0 };
	constexpr vec3_t bcolor = { 1.0, 0.0, 0.0 };

	client = player->client;

	// flash the backgrounds behind the status numbers
	int16_t want_flashes = 0;

	if (client->damage_blood)
		want_flashes |= 1;
	if (client->damage_armor && !(player->flags & FL_GODMODE))
		want_flashes |= 2;

	if (want_flashes) {
		client->flash_time = level.time + 100_ms;
		client->ps.stats[STAT_FLASHES] = want_flashes;
	} else if (client->flash_time < level.time)
		client->ps.stats[STAT_FLASHES] = 0;

	// total points of damage shot at the player this frame
	count = (float)(client->damage_blood + client->damage_armor + client->damage_parmor);
	if (count == 0)
		return; // didn't take any damage

	// start a pain animation if still in the player model
	if (client->anim_priority < ANIM_PAIN && player->s.modelindex == MODELINDEX_PLAYER) {
		static int i;

		client->anim_priority = ANIM_PAIN;
		if (client->ps.pmove.pm_flags & PMF_DUCKED) {
			player->s.frame = FRAME_crpain1 - 1;
			client->anim_end = FRAME_crpain4;
		} else {
			i = (i + 1) % 3;
			switch (i) {
			case 0:
				player->s.frame = FRAME_pain101 - 1;
				client->anim_end = FRAME_pain104;
				break;
			case 1:
				player->s.frame = FRAME_pain201 - 1;
				client->anim_end = FRAME_pain204;
				break;
			case 2:
				player->s.frame = FRAME_pain301 - 1;
				client->anim_end = FRAME_pain304;
				break;
			}
		}

		client->anim_time = 0_ms;
	}

	realcount = count;

	// if we took health damage, do a minimum clamp
	if (client->damage_blood) {
		if (count < 10)
			count = 10; // always make a visible effect
	} else {
		if (count > 2)
			count = 2; // don't go too deep
	}

	// play an appropriate pain sound
	if ((level.time > player->pain_debounce_time) && !(player->flags & FL_GODMODE)) {
		player->pain_debounce_time = level.time + 700_ms;

		constexpr const char *pain_sounds[] = {
			"*pain25_1.wav",
			"*pain25_2.wav",
			"*pain50_1.wav",
			"*pain50_2.wav",
			"*pain75_1.wav",
			"*pain75_2.wav",
			"*pain100_1.wav",
			"*pain100_2.wav",
		};

		if (player->health < 25)
			l = 0;
		else if (player->health < 50)
			l = 2;
		else if (player->health < 75)
			l = 4;
		else
			l = 6;

		if (brandom())
			l |= 1;

		gi.sound(player, CHAN_VOICE, gi.soundindex(pain_sounds[l]), 1, ATTN_NORM, 0);
		// Paril: pain noises alert monsters
		PlayerNoise(player, player->s.origin, PNOISE_SELF);
	}

	// the total alpha of the blend is always proportional to count
	if (client->damage_alpha < 0)
		client->damage_alpha = 0;

	// [Paril-KEX] tweak the values to rely less on this
	// and more on damage indicators
	if (client->damage_blood || (client->damage_alpha + count * 0.06f) < 0.15f) {
		client->damage_alpha += count * 0.06f;

		if (client->damage_alpha < 0.06f)
			client->damage_alpha = 0.06f;
		if (client->damage_alpha > 0.4f)
			client->damage_alpha = 0.4f; // don't go too saturated
	}

	// mix in colors
	v = {};

	if (client->damage_parmor)
		v += power_color * (client->damage_parmor / realcount);
	if (client->damage_blood)
		v += bcolor * max(15.0f, (client->damage_blood / realcount));
	if (client->damage_armor)
		v += armor_color * (client->damage_armor / realcount);
	client->damage_blend = v.normalized();

	//
	// calculate view angle kicks
	//
	kick = (float)abs(client->damage_knockback);
	if (kick && player->health > 0) // kick of 0 means no view adjust at all
	{
		kick = kick * 100 / player->health;

		if (kick < count * 0.5f)
			kick = count * 0.5f;
		if (kick > 50)
			kick = 50;

		v = client->damage_from - player->s.origin;
		v.normalize();

		side = v.dot(right);
		client->v_dmg_roll = kick * side * 0.3f;

		side = -v.dot(forward);
		client->v_dmg_pitch = kick * side * 0.3f;

		client->v_dmg_time = level.time + DAMAGE_TIME();
	}

	// [Paril-KEX] send view indicators
	if (client->num_damage_indicators) {
		gi.WriteByte(svc_damage);
		gi.WriteByte(client->num_damage_indicators);

		for (size_t i = 0; i < client->num_damage_indicators; i++) {
			auto &indicator = client->damage_indicators[i];

			// encode total damage into 5 bits
			uint8_t encoded = std::clamp((indicator.health + indicator.power + indicator.armor) / 3, 1, 0x1F);

			// encode types in the latter 3 bits
			if (indicator.health)
				encoded |= 0x20;
			if (indicator.armor)
				encoded |= 0x40;
			if (indicator.power)
				encoded |= 0x80;

			gi.WriteByte(encoded);
			gi.WriteDir((player->s.origin - indicator.from).normalized());
		}

		gi.unicast(player, false);
	}

	//
	// clear totals
	//
	client->damage_blood = 0;
	client->damage_armor = 0;
	client->damage_parmor = 0;
	client->damage_knockback = 0;
	client->num_damage_indicators = 0;
}

/*
===============
G_CalcViewOffset

Auto pitching on slopes?

  fall from 128: 400 = 160000
  fall from 256: 580 = 336400
  fall from 384: 720 = 518400
  fall from 512: 800 = 640000
  fall from 640: 960 =

  damage = deltavelocity*deltavelocity  * 0.0001

===============
*/
static void G_CalcViewOffset(gentity_t *ent) {
	float  bob;
	float  ratio;
	float  delta;
	vec3_t v;

	//===================================

	// base angles
	vec3_t &angles = ent->client->ps.kick_angles;

	// if dead, fix the angle and don't add any kick
	if (ent->deadflag && ClientIsPlaying(ent->client)) {
		angles = {};

		if (ent->flags & FL_SAM_RAIMI) {
			ent->client->ps.viewangles[ROLL] = 0;
			ent->client->ps.viewangles[PITCH] = 0;
		} else {
			ent->client->ps.viewangles[ROLL] = 40;
			ent->client->ps.viewangles[PITCH] = -15;
		}
		ent->client->ps.viewangles[YAW] = ent->client->killer_yaw;
	} else if (!ent->client->pers.bob_skip && !SkipViewModifiers()) {
		// add angles based on weapon kick
		angles = P_CurrentKickAngles(ent);

		// add angles based on damage kick
		if (ent->client->v_dmg_time > level.time) {
			// [Paril-KEX] 100ms of slack is added to account for
			// visual difference in higher tickrates
			gtime_t diff = ent->client->v_dmg_time - level.time;

			// slack time remaining
			if (DAMAGE_TIME_SLACK()) {
				if (diff > DAMAGE_TIME() - DAMAGE_TIME_SLACK())
					ratio = (DAMAGE_TIME() - diff).seconds() / DAMAGE_TIME_SLACK().seconds();
				else
					ratio = diff.seconds() / (DAMAGE_TIME() - DAMAGE_TIME_SLACK()).seconds();
			} else
				ratio = diff.seconds() / (DAMAGE_TIME() - DAMAGE_TIME_SLACK()).seconds();

			angles[PITCH] += ratio * ent->client->v_dmg_pitch;
			angles[ROLL] += ratio * ent->client->v_dmg_roll;
		}

		// add pitch based on fall kick
		if (ent->client->fall_time > level.time) {
			// [Paril-KEX] 100ms of slack is added to account for
			// visual difference in higher tickrates
			gtime_t diff = ent->client->fall_time - level.time;

			// slack time remaining
			if (DAMAGE_TIME_SLACK()) {
				if (diff > FALL_TIME() - DAMAGE_TIME_SLACK())
					ratio = (FALL_TIME() - diff).seconds() / DAMAGE_TIME_SLACK().seconds();
				else
					ratio = diff.seconds() / (FALL_TIME() - DAMAGE_TIME_SLACK()).seconds();
			} else
				ratio = diff.seconds() / (FALL_TIME() - DAMAGE_TIME_SLACK()).seconds();
			angles[PITCH] += ratio * ent->client->fall_value;
		}

		// add angles based on velocity
		if (!ent->client->pers.bob_skip && !SkipViewModifiers()) {
			delta = ent->velocity.dot(forward);
			angles[PITCH] += delta * run_pitch->value;

			delta = ent->velocity.dot(right);
			angles[ROLL] += delta * run_roll->value;

			// add angles based on bob
			delta = bobfracsin * bob_pitch->value * xyspeed;
			if ((ent->client->ps.pmove.pm_flags & PMF_DUCKED) && ent->groundentity)
				delta *= 6; // crouching
			delta = min(delta, 1.2f);
			angles[PITCH] += delta;
			delta = bobfracsin * bob_roll->value * xyspeed;
			if ((ent->client->ps.pmove.pm_flags & PMF_DUCKED) && ent->groundentity)
				delta *= 6; // crouching
			delta = min(delta, 1.2f);
			if (bobcycle & 1)
				delta = -delta;
			angles[ROLL] += delta;
		}

		// add earthquake angles
		if (ent->client->quake_time > level.time) {
			float factor = min(1.0f, (ent->client->quake_time.seconds() / level.time.seconds()) * 0.25f);

			angles.x += crandom_open() * factor;
			angles.z += crandom_open() * factor;
			angles.y += crandom_open() * factor;
		}
	}

	// [Paril-KEX] clamp angles
	for (int i = 0; i < 3; i++)
		angles[i] = clamp(angles[i], -31.f, 31.f);

	//===================================

	// base origin

	v = {};

	// add fall height

	if (!ent->client->pers.bob_skip && !SkipViewModifiers()) {
		if (ent->client->fall_time > level.time) {
			// [Paril-KEX] 100ms of slack is added to account for
			// visual difference in higher tickrates
			gtime_t diff = ent->client->fall_time - level.time;

			// slack time remaining
			if (DAMAGE_TIME_SLACK()) {
				if (diff > FALL_TIME() - DAMAGE_TIME_SLACK())
					ratio = (FALL_TIME() - diff).seconds() / DAMAGE_TIME_SLACK().seconds();
				else
					ratio = diff.seconds() / (FALL_TIME() - DAMAGE_TIME_SLACK()).seconds();
			} else
				ratio = diff.seconds() / (FALL_TIME() - DAMAGE_TIME_SLACK()).seconds();
			v[2] -= ratio * ent->client->fall_value * 0.4f;
		}

		// add bob height
		bob = bobfracsin * xyspeed * bob_up->value;
		if (bob > 6)
			bob = 6;
		// gi.DebugGraph (bob *2, 255);
		v[2] += bob;
	}

	// add kick offset

	if (!ent->client->pers.bob_skip && !SkipViewModifiers())
		v += P_CurrentKickOrigin(ent);

	// absolutely bound offsets
	// so the view can never be outside the player box

	if (v[0] < -14)
		v[0] = -14;
	else if (v[0] > 14)
		v[0] = 14;
	if (v[1] < -14)
		v[1] = -14;
	else if (v[1] > 14)
		v[1] = 14;
	if (v[2] < -22)
		v[2] = -22;
	else if (v[2] > 30)
		v[2] = 30;

	ent->client->ps.viewoffset = v;
}

/*
==============
G_CalcGunOffset
==============
*/
static void G_CalcGunOffset(gentity_t *ent) {
	int	  i;

	if (ent->client->pers.weapon &&
		!((ent->client->pers.weapon->id == IT_WEAPON_PLASMABEAM || ent->client->pers.weapon->id == IT_WEAPON_GRAPPLE) && ent->client->weaponstate == WEAPON_FIRING)
		&& !SkipViewModifiers()) {
		// gun angles from bobbing
		ent->client->ps.gunangles[ROLL] = xyspeed * bobfracsin * 0.005f;
		ent->client->ps.gunangles[YAW] = xyspeed * bobfracsin * 0.01f;
		if (bobcycle & 1) {
			ent->client->ps.gunangles[ROLL] = -ent->client->ps.gunangles[ROLL];
			ent->client->ps.gunangles[YAW] = -ent->client->ps.gunangles[YAW];
		}

		ent->client->ps.gunangles[PITCH] = xyspeed * bobfracsin * 0.005f;

		vec3_t viewangles_delta = ent->client->oldviewangles - ent->client->ps.viewangles;

		for (i = 0; i < 3; i++)
			ent->client->slow_view_angles[i] += viewangles_delta[i];

		// gun angles from delta movement
		for (i = 0; i < 3; i++) {
			float &d = ent->client->slow_view_angles[i];

			if (!d)
				continue;

			if (d > 180)
				d -= 360;
			if (d < -180)
				d += 360;
			if (d > 45)
				d = 45;
			if (d < -45)
				d = -45;

			// [Sam-KEX] Apply only half-delta. Makes the weapons look less detatched from the player.
			if (i == ROLL)
				ent->client->ps.gunangles[i] += (0.1f * d) * 0.5f;
			else
				ent->client->ps.gunangles[i] += (0.2f * d) * 0.5f;

			float reduction_factor = viewangles_delta[i] ? 0.05f : 0.15f;

			if (d > 0)
				d = max(0.f, d - gi.frame_time_ms * reduction_factor);
			else if (d < 0)
				d = min(0.f, d + gi.frame_time_ms * reduction_factor);
		}

		// [Paril-KEX] cl_rollhack
		ent->client->ps.gunangles[ROLL] = -ent->client->ps.gunangles[ROLL];
	} else {
		for (i = 0; i < 3; i++)
			ent->client->ps.gunangles[i] = 0;
	}

	// gun height
	ent->client->ps.gunoffset = {};

	// gun_x / gun_y / gun_z are development tools
	for (i = 0; i < 3; i++) {
		ent->client->ps.gunoffset[i] += forward[i] * (gun_y->value);
		ent->client->ps.gunoffset[i] += right[i] * gun_x->value;
		ent->client->ps.gunoffset[i] += up[i] * (-gun_z->value);
	}
}

/*
=============
G_CalcBlend
=============
*/
static void G_CalcBlend(gentity_t *ent) {
	gtime_t remaining;

	ent->client->ps.damage_blend = ent->client->ps.screen_blend = {};

	// add for powerups
	if (ent->client->pu_time_quad > level.time) {
		remaining = ent->client->pu_time_quad - level.time;
		if (remaining.milliseconds() == 3000) // beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage2.wav"), 1, ATTN_NORM, 0);
		if (G_PowerUpExpiringRelative(remaining))
			G_AddBlend(0, 0, 1, 0.08f, ent->client->ps.screen_blend);
	} else if (ent->client->pu_time_haste > level.time) {
		remaining = ent->client->pu_time_haste - level.time;
		if (remaining.milliseconds() == 3000) // beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/quadfire2.wav"), 1, ATTN_NORM, 0);
		if (G_PowerUpExpiringRelative(remaining))
			G_AddBlend(1, 0.2f, 0.5f, 0.08f, ent->client->ps.screen_blend);
	} else if (ent->client->pu_time_double > level.time) {
		remaining = ent->client->pu_time_double - level.time;
		if (remaining.milliseconds() == 3000) // beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/ddamage2.wav"), 1, ATTN_NORM, 0);
		if (G_PowerUpExpiringRelative(remaining))
			G_AddBlend(0.9f, 0.7f, 0, 0.08f, ent->client->ps.screen_blend);
	} else if (ent->client->pu_time_protection > level.time) {
		remaining = ent->client->pu_time_protection - level.time;
		if (remaining.milliseconds() == 3000) // beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/protect2.wav"), 1, ATTN_NORM, 0);
		if (G_PowerUpExpiringRelative(remaining))
			G_AddBlend(1, 1, 0, 0.08f, ent->client->ps.screen_blend);
	} else if (ent->client->pu_time_invisibility > level.time) {
		remaining = ent->client->pu_time_invisibility - level.time;
		if (remaining.milliseconds() == 3000) // beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/protect2.wav"), 1, ATTN_NORM, 0);
		if (G_PowerUpExpiringRelative(remaining))
			G_AddBlend(0.8f, 0.8f, 0.8f, 0.08f, ent->client->ps.screen_blend);
	} else if (ent->client->pu_time_regeneration > level.time) {
		remaining = ent->client->pu_time_regeneration - level.time;
		if (remaining.milliseconds() == 3000) // beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/protect2.wav"), 1, ATTN_NORM, 0);
	} else if (ent->client->pu_time_enviro > level.time) {
		remaining = ent->client->pu_time_enviro - level.time;
		if (remaining.milliseconds() == 3000) // beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/airout.wav"), 1, ATTN_NORM, 0);
		if (G_PowerUpExpiringRelative(remaining))
			G_AddBlend(0, 1, 0, 0.08f, ent->client->ps.screen_blend);
	} else if (ent->client->pu_time_rebreather > level.time) {
		remaining = ent->client->pu_time_rebreather - level.time;
		if (remaining.milliseconds() == 3000) // beginning to fade
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/airout.wav"), 1, ATTN_NORM, 0);
		if (G_PowerUpExpiringRelative(remaining))
			G_AddBlend(0.4f, 1, 0.4f, 0.04f, ent->client->ps.screen_blend);
	}
	if (ent->client->nuke_time > level.time) {
		float brightness = (ent->client->nuke_time - level.time).seconds() / 2.0f;
		G_AddBlend(1, 1, 1, brightness, ent->client->ps.screen_blend);
	}
	if (ent->client->ir_time > level.time) {
		remaining = ent->client->ir_time - level.time;
		if (G_PowerUpExpiringRelative(remaining)) {
			ent->client->ps.rdflags |= RDF_IRGOGGLES;
			G_AddBlend(1, 0, 0, 0.2f, ent->client->ps.screen_blend);
		} else
			ent->client->ps.rdflags &= ~RDF_IRGOGGLES;
	} else {
		ent->client->ps.rdflags &= ~RDF_IRGOGGLES;
	}

	// add for damage
	if (ent->client->damage_alpha > 0)
		G_AddBlend(ent->client->damage_blend[0], ent->client->damage_blend[1], ent->client->damage_blend[2], ent->client->damage_alpha, ent->client->ps.damage_blend);

	// [Paril-KEX] drowning visual indicator
	if (ent->air_finished < level.time + 9_sec) {
		constexpr vec3_t drown_color = { 0.1f, 0.1f, 0.2f };
		constexpr float max_drown_alpha = 0.75f;
		float alpha = (ent->air_finished < level.time) ? 1 : (1.f - ((ent->air_finished - level.time).seconds() / 9.0f));
		G_AddBlend(drown_color[0], drown_color[1], drown_color[2], min(alpha, max_drown_alpha), ent->client->ps.damage_blend);
	}

	// drop the damage value
	ent->client->damage_alpha -= gi.frame_time_s * 0.6f;
	if (ent->client->damage_alpha < 0)
		ent->client->damage_alpha = 0;

	// drop the bonus value
	ent->client->bonus_alpha -= gi.frame_time_s;
	if (ent->client->bonus_alpha < 0)
		ent->client->bonus_alpha = 0;
}

/*
=============
P_WorldEffects
=============
*/
static void P_WorldEffects() {
	bool		  breather, envirosuit, protection;
	water_level_t waterlevel, old_waterlevel;

	if (level.timeout_in_place)
		return;

	if (current_player->movetype == MOVETYPE_FREECAM) {
		current_player->air_finished = level.time + 12_sec; // don't need air
		return;
	}

	waterlevel = current_player->waterlevel;
	old_waterlevel = current_client->old_waterlevel;
	current_client->old_waterlevel = waterlevel;

	breather = current_client->pu_time_rebreather > level.time;
	envirosuit = current_client->pu_time_enviro > level.time;
	protection = current_client->pu_time_protection > level.time;

	//
	// if just entered a water volume, play a sound
	//
	if (!old_waterlevel && waterlevel) {
		PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
		if (current_player->watertype & CONTENTS_LAVA)
			gi.sound(current_player, CHAN_BODY, gi.soundindex("player/lava_in.wav"), 1, ATTN_NORM, 0);
		else if (current_player->watertype & CONTENTS_SLIME)
			gi.sound(current_player, CHAN_BODY, gi.soundindex("player/watr_in.wav"), 1, ATTN_NORM, 0);
		else if (current_player->watertype & CONTENTS_WATER)
			gi.sound(current_player, CHAN_BODY, gi.soundindex("player/watr_in.wav"), 1, ATTN_NORM, 0);
		current_player->flags |= FL_INWATER;

		// clear damage_debounce, so the pain sound will play immediately
		current_player->damage_debounce_time = level.time - 1_sec;
	}

	//
	// if just completely exited a water volume, play a sound
	//
	if (old_waterlevel && !waterlevel) {
		PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
		gi.sound(current_player, CHAN_BODY, gi.soundindex("player/watr_out.wav"), 1, ATTN_NORM, 0);
		current_player->flags &= ~FL_INWATER;
	}

	//
	// check for head just going under water
	//
	if (old_waterlevel != WATER_UNDER && waterlevel == WATER_UNDER) {
		gi.sound(current_player, CHAN_BODY, gi.soundindex("player/watr_un.wav"), 1, ATTN_NORM, 0);
	}

	//
	// check for head just coming out of water
	//
	if (current_player->health > 0 && old_waterlevel == WATER_UNDER && waterlevel != WATER_UNDER) {
		if (current_player->air_finished < level.time) { // gasp for air
			gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/gasp1.wav"), 1, ATTN_NORM, 0);
			PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
		} else if (current_player->air_finished < level.time + 11_sec) { // just break surface
			gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/gasp2.wav"), 1, ATTN_NORM, 0);
		}
	}

	//
	// check for drowning
	//
	if (waterlevel == WATER_UNDER) {
		// breather or envirosuit give air
		if (breather || envirosuit) {
			current_player->air_finished = level.time + 10_sec;

			if (((current_client->pu_time_rebreather - level.time).milliseconds() % 2500) == 0) {
				if (!current_client->breather_sound)
					gi.sound(current_player, CHAN_AUTO, gi.soundindex("player/u_breath1.wav"), 1, ATTN_NORM, 0);
				else
					gi.sound(current_player, CHAN_AUTO, gi.soundindex("player/u_breath2.wav"), 1, ATTN_NORM, 0);
				current_client->breather_sound ^= 1;
				PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
				// FIXME: release a bubble?
			}
		}

		// if out of air, start drowning
		if (current_player->air_finished < level.time) { // drown!
			if (current_player->client->next_drown_time < level.time && current_player->health > 0) {
				current_player->client->next_drown_time = level.time + 1_sec;

				// take more damage the longer underwater
				current_player->dmg += 2;
				if (current_player->dmg > 15)
					current_player->dmg = 15;

				// play a gurp sound instead of a normal pain sound
				if (current_player->health <= current_player->dmg)
					gi.sound(current_player, CHAN_VOICE, gi.soundindex("*drown1.wav"), 1, ATTN_NORM, 0); // [Paril-KEX]
				else if (brandom())
					gi.sound(current_player, CHAN_VOICE, gi.soundindex("*gurp1.wav"), 1, ATTN_NORM, 0);
				else
					gi.sound(current_player, CHAN_VOICE, gi.soundindex("*gurp2.wav"), 1, ATTN_NORM, 0);

				current_player->pain_debounce_time = level.time;

				T_Damage(current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, current_player->dmg, 0, DAMAGE_NO_ARMOR, MOD_WATER);
			}
		}
		// Paril: almost-drowning sounds
		else if (current_player->air_finished <= level.time + 3_sec) {
			if (current_player->client->next_drown_time < level.time) {
				gi.sound(current_player, CHAN_VOICE, gi.soundindex(fmt::format("player/wade{}.wav", 1 + ((int32_t)level.time.seconds() % 3)).c_str()), 1, ATTN_NORM, 0);
				current_player->client->next_drown_time = level.time + 1_sec;
			}
		}
	} else {
		current_player->air_finished = level.time + 12_sec;
		current_player->dmg = 2;
	}

	//
	// check for sizzle damage
	//
	if (waterlevel && (current_player->watertype & (CONTENTS_LAVA | CONTENTS_SLIME)) && current_player->slime_debounce_time <= level.time) {
		if (current_player->watertype & CONTENTS_LAVA) {
			if (current_player->health > 0 && current_player->pain_debounce_time <= level.time) {
				if (brandom())
					gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/burn1.wav"), 1, ATTN_NORM, 0);
				else
					gi.sound(current_player, CHAN_VOICE, gi.soundindex("player/burn2.wav"), 1, ATTN_NORM, 0);

				if (envirosuit || protection)
					gi.sound(current_player, CHAN_AUX, gi.soundindex("items/protect3.wav"), 1, ATTN_NORM, 0);

				current_player->pain_debounce_time = level.time + 1_sec;
			}

			int dmg = ((envirosuit || protection) ? 1 : 3) * waterlevel; // take 1/3 damage with envirosuit/protection

			T_Damage(current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, dmg, 0, DAMAGE_NONE, MOD_LAVA);
			current_player->slime_debounce_time = level.time + 10_hz;
		}

		if (current_player->watertype & CONTENTS_SLIME) {
			if (!envirosuit && !protection) { // no damage from slime with envirosuit/protection
				T_Damage(current_player, world, world, vec3_origin, current_player->s.origin, vec3_origin, 1 * waterlevel, 0, DAMAGE_NONE, MOD_SLIME);
				current_player->slime_debounce_time = level.time + 10_hz;
			} else {
				if (current_player->health > 0 && current_player->pain_debounce_time <= level.time) {
					gi.sound(current_player, CHAN_AUX, gi.soundindex("items/protect3.wav"), 1, ATTN_NORM, 0);
					current_player->pain_debounce_time = level.time + 1_sec;
				}
			}
		}
	}
}

/*
===============
G_SetClientEffects
===============
*/
static void G_SetClientEffects(gentity_t *ent) {
	int pa_type;

	ent->s.effects = EF_NONE;
	ent->s.renderfx &= RF_STAIR_STEP;
	ent->s.renderfx |= RF_IR_VISIBLE;
	ent->s.alpha = 1.0;

	if (ent->health <= 0 || ent->client->eliminated || level.intermission_time)
		return;

	if (ent->flags & FL_FLASHLIGHT)
		ent->s.effects |= EF_FLASHLIGHT;

	if (ent->flags & FL_DISGUISED)
		ent->s.renderfx |= RF_USE_DISGUISE;

	if (ent->powerarmor_time > level.time) {
		pa_type = PowerArmorType(ent);
		if (pa_type == IT_POWER_SCREEN) {
			ent->s.effects |= EF_POWERSCREEN;
		} else if (pa_type == IT_POWER_SHIELD) {
			ent->s.effects |= EF_COLOR_SHELL;
			ent->s.renderfx |= RF_SHELL_GREEN;
		}
	}

	if (ent->client->pu_regen_time_blip > level.time) {
		ent->s.effects |= EF_COLOR_SHELL;
		ent->s.renderfx |= RF_SHELL_RED;
	}

	CTF_ClientEffects(ent);

	if (ent->client->pu_time_quad > level.time)
		if (G_PowerUpExpiring(ent->client->pu_time_quad))
			ent->s.effects |= EF_QUAD;
	if (ent->client->pu_time_protection > level.time)
		if (G_PowerUpExpiring(ent->client->pu_time_protection))
			ent->s.effects |= EF_PENT;
	if (ent->client->pu_time_haste > level.time)
		if (G_PowerUpExpiring(ent->client->pu_time_haste))
			ent->s.effects |= EF_DUALFIRE;
	if (ent->client->pu_time_double > level.time)
		if (G_PowerUpExpiring(ent->client->pu_time_double))
			ent->s.effects |= EF_DOUBLE;
	if ((ent->client->owned_sphere) && (ent->client->owned_sphere->spawnflags == SF_SPHERE_DEFENDER))
		ent->s.effects |= EF_HALF_DAMAGE;
	if (ent->client->tracker_pain_time > level.time)
		ent->s.effects |= EF_TRACKERTRAIL;
	if (ent->client->pu_time_invisibility > level.time) {
		if (ent->client->invisibility_fade_time <= level.time)
			ent->s.alpha = 0.05f;
		else {
			float x = (ent->client->invisibility_fade_time - level.time).seconds() / INVISIBILITY_TIME.seconds();
			ent->s.alpha = std::clamp(x, 0.05f, 0.2f);
		}
	}
}

/*
=================
HasSpectators

Check if any spectators are viewing this player.
=================
*/
static inline bool HasSpectators(gentity_t *ent) {
	for (auto ec : active_clients()) {
		if (ec != ent && ec->client && ec->client->follow_target == ent)
			return true;
	}
	return false;
}

/*
===============
G_SetClientEvent
===============
*/
static void G_SetClientEvent(gentity_t *ent) {
	if (level.timeout_in_place)
		return;

	if (ent->s.event)
		return;

	if (ent->client->ps.pmove.pm_flags & PMF_ON_LADDER) {
		if (g_ladder_steps->integer > 1 || (g_ladder_steps->integer == 1 && !deathmatch->integer)) {
			if (current_client->last_ladder_sound < level.time &&
				(current_client->last_ladder_pos - ent->s.origin).length() > 48.f) {
				ent->s.event = EV_LADDER_STEP;
				// Send positioned sound for spectators only
				if (HasSpectators(ent)) {
					gi.positioned_sound(ent->s.origin, ent, CHAN_FOOTSTEP, gi.soundindex("player/stepl.wav"), 1, ATTN_NORM, 0);
				}
				current_client->last_ladder_pos = ent->s.origin;
				current_client->last_ladder_sound = level.time + LADDER_SOUND_TIME;
			}
		}
	} else if (ent->groundentity && xyspeed > 225) {
		if ((int)(current_client->bobtime + bobmove) != bobcycle_run) {
			ent->s.event = EV_FOOTSTEP;
			// Send positioned sound for spectators only
			if (HasSpectators(ent)) {
				gi.positioned_sound(ent->s.origin, ent, CHAN_FOOTSTEP, gi.soundindex("player/step1.wav"), 1, ATTN_NORM, 0);
			}
		}
	}
}

/*
===============
G_SetClientSound
===============
*/
static void G_SetClientSound(gentity_t *ent) {
	if (level.timeout_in_place)
		return;

	// help beep (no more than three times)
	if (ent->client->pers.helpchanged && ent->client->pers.helpchanged <= 3 && ent->client->pers.help_time < level.time) {
		if (ent->client->pers.helpchanged == 1) // [KEX] haleyjd: once only
			gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/pc_up.wav"), 1, ATTN_STATIC, 0);
		ent->client->pers.helpchanged++;
		ent->client->pers.help_time = level.time + 5_sec;
	}

	// reset defaults
	ent->s.sound = 0;
	ent->s.loop_attenuation = 0;
	ent->s.loop_volume = 0;

	if (ent->waterlevel && (ent->watertype & (CONTENTS_LAVA | CONTENTS_SLIME))) {
		ent->s.sound = snd_fry;
		return;
	}

	if (ent->deadflag || !ClientIsPlaying(ent->client) || ent->client->eliminated)
		return;

	if (ent->client->weapon_sound)
		ent->s.sound = ent->client->weapon_sound;
	else if (ent->client->pers.weapon) {
		switch (ent->client->pers.weapon->id) {
		case IT_WEAPON_RAILGUN:
			ent->s.sound = gi.soundindex("weapons/rg_hum.wav");
			break;
		case IT_WEAPON_BFG:
		case IT_WEAPON_PLASMABEAM:
			ent->s.sound = gi.soundindex("weapons/bfg_hum.wav");
			break;
		case IT_WEAPON_PHALANX:
			ent->s.sound = gi.soundindex("weapons/phaloop.wav");
			break;
		}
	}

	// [Paril-KEX] if no other sound is playing, play appropriate grapple sounds
	if (!ent->s.sound && ent->client->grapple_ent) {
		if (ent->client->grapple_state == GRAPPLE_STATE_PULL)
			ent->s.sound = gi.soundindex("weapons/grapple/grpull.wav");
		else if (ent->client->grapple_state == GRAPPLE_STATE_FLY)
			ent->s.sound = gi.soundindex("weapons/grapple/grfly.wav");
		else if (ent->client->grapple_state == GRAPPLE_STATE_HANG)
			ent->s.sound = gi.soundindex("weapons/grapple/grhang.wav");
	}

	// weapon sounds play at a higher attn
	ent->s.loop_attenuation = ATTN_NORM;
}

/*
===============
G_SetClientFrame
===============
*/
void G_SetClientFrame(gentity_t *ent) {
	gclient_t *client;
	bool	   duck, run;

	if (ent->s.modelindex != MODELINDEX_PLAYER)
		return; // not in the player model

	client = ent->client;

	duck = client->ps.pmove.pm_flags & PMF_DUCKED ? true : false;
	run = xyspeed ? true : false;

	// check for stand/duck and stop/go transitions
	if (duck != client->anim_duck && client->anim_priority < ANIM_DEATH)
		goto newanim;
	if (run != client->anim_run && client->anim_priority == ANIM_BASIC)
		goto newanim;
	if (!ent->groundentity && client->anim_priority <= ANIM_WAVE)
		goto newanim;

	if (client->anim_time > level.time)
		return;
	else if ((client->anim_priority & ANIM_REVERSED) && (ent->s.frame > client->anim_end)) {
		if (client->anim_time <= level.time) {
			ent->s.frame--;
			client->anim_time = level.time + 10_hz;
		}
		return;
	} else if (!(client->anim_priority & ANIM_REVERSED) && (ent->s.frame < client->anim_end)) {
		// continue an animation
		if (client->anim_time <= level.time) {
			ent->s.frame++;
			client->anim_time = level.time + 10_hz;
		}
		return;
	}

	if (client->anim_priority == ANIM_DEATH)
		return; // stay there
	if (client->anim_priority == ANIM_JUMP) {
		if (!ent->groundentity)
			return; // stay there
		ent->client->anim_priority = ANIM_WAVE;

		if (duck) {
			ent->s.frame = FRAME_jump6;
			ent->client->anim_end = FRAME_jump4;
			ent->client->anim_priority |= ANIM_REVERSED;
		} else {
			ent->s.frame = FRAME_jump3;
			ent->client->anim_end = FRAME_jump6;
		}
		ent->client->anim_time = level.time + 10_hz;
		return;
	}

newanim:
	// return to either a running or standing frame
	client->anim_priority = ANIM_BASIC;
	client->anim_duck = duck;
	client->anim_run = run;
	client->anim_time = level.time + 10_hz;

	if (!ent->groundentity) {
		// if on grapple, don't go into jump frame, go into standing
		// frame
		if (client->grapple_ent) {
			if (duck) {
				ent->s.frame = FRAME_crstnd01;
				client->anim_end = FRAME_crstnd19;
			} else {
				ent->s.frame = FRAME_stand01;
				client->anim_end = FRAME_stand40;
			}
		} else {
			client->anim_priority = ANIM_JUMP;

			if (duck) {
				if (ent->s.frame != FRAME_crwalk2)
					ent->s.frame = FRAME_crwalk1;
				client->anim_end = FRAME_crwalk2;
			} else {
				if (ent->s.frame != FRAME_jump2)
					ent->s.frame = FRAME_jump1;
				client->anim_end = FRAME_jump2;
			}
		}
	} else if (run) { // running
		if (duck) {
			ent->s.frame = FRAME_crwalk1;
			client->anim_end = FRAME_crwalk6;
		} else {
			ent->s.frame = FRAME_run1;
			client->anim_end = FRAME_run6;
		}
	} else { // standing
		if (duck) {
			ent->s.frame = FRAME_crstnd01;
			client->anim_end = FRAME_crstnd19;
		} else {
			ent->s.frame = FRAME_stand01;
			client->anim_end = FRAME_stand40;
		}
	}
}

// [Paril-KEX]
static void P_RunMegaHealth(gentity_t *ent) {
	if (!ent->client->pers.megahealth_time)
		return;
	else if (ent->health <= ent->max_health) {
		ent->client->pers.megahealth_time = 0_ms;
		return;
	}

	ent->client->pers.megahealth_time -= FRAME_TIME_S;

	if (ent->client->pers.megahealth_time <= 0_ms) {
		ent->health--;

		if (ent->health > ent->max_health)
			ent->client->pers.megahealth_time = 1000_ms;
		else
			ent->client->pers.megahealth_time = 0_ms;
	}
}

static lag_compensation_sample_t *G_LagCompensationSamples(gentity_t *ent) {
	if (!game.lag_samples || game.max_lag_origins <= 0 || !ent || ent->s.number <= 0 || ent->s.number > game.maxclients)
		return nullptr;

	return game.lag_samples + ((ent->s.number - 1) * game.max_lag_origins);
}

static int32_t G_LagCompensationSampleCount(gentity_t *ent) {
	if (!ent || !ent->client || game.max_lag_origins <= 0)
		return 0;

	return min<int32_t>(ent->client->num_lag_origins, game.max_lag_origins);
}

static bool G_IsEnhancedLagTarget(gentity_t *ent) {
	return ent && ent->inuse && ent->client && ent->client->pers.connected &&
		ClientIsPlaying(ent->client) && !ent->client->eliminated &&
		ent->health > 0 && !ent->deadflag &&
		!(ent->svflags & SVF_NOCLIENT) && ent->solid != SOLID_NOT;
}

static bool G_IsLagCompensationShooter(gentity_t *ent) {
	return G_IsEnhancedLagTarget(ent) && !(ent->svflags & SVF_BOT);
}

static bool G_LagCompensationFrameDelta(gentity_t *from_player, uint32_t current_frame, int32_t &frames_back) {
	if (!from_player || !from_player->client)
		return false;
	else if (game.max_lag_origins <= 0)
		return false;

	uint32_t command_frame = from_player->client->cmd.server_frame;
	if (command_frame >= current_frame)
		return false;

	uint32_t delta = current_frame - command_frame;
	if (delta >= static_cast<uint32_t>(game.max_lag_origins))
		return false;

	frames_back = static_cast<int32_t>(delta);
	return true;
}

static bool G_LagSampleByOffset(gentity_t *ent, int32_t frames_back, lag_compensation_sample_t &sample) {
	if (frames_back < 0)
		return false;

	lag_compensation_sample_t *samples = G_LagCompensationSamples(ent);
	if (!samples)
		return false;

	int32_t sample_count = G_LagCompensationSampleCount(ent);
	if (sample_count <= frames_back)
		return false;

	int32_t lag_id = static_cast<int32_t>(ent->client->next_lag_origin) - 1 - frames_back;
	while (lag_id < 0)
		lag_id += game.max_lag_origins;

	if (lag_id >= game.max_lag_origins)
		lag_id %= game.max_lag_origins;

	sample = samples[lag_id];
	return true;
}

static lag_compensation_sample_t G_InterpolateLagSamples(
	const lag_compensation_sample_t &older,
	const lag_compensation_sample_t &newer,
	uint32_t target_frame,
	float frac) {
	if (frac <= 0.f)
		return older;
	else if (frac >= 1.f)
		return newer;

	lag_compensation_sample_t result = frac < 0.5f ? older : newer;
	result.server_frame = target_frame;
	result.time = older.time + ((newer.time - older.time) * frac);
	result.origin = lerp(older.origin, newer.origin, frac);
	return result;
}

static bool G_LagSampleAtFrame(
	gentity_t *ent,
	uint32_t target_frame,
	int32_t frames_back,
	lag_compensation_sample_t &sample) {
	int32_t sample_count = G_LagCompensationSampleCount(ent);
	if (sample_count <= 0)
		return false;

	lag_compensation_sample_t latest;
	if (!G_LagSampleByOffset(ent, 0, latest))
		return false;

	if (target_frame >= latest.server_frame) {
		sample = latest;
		return true;
	}

	lag_compensation_sample_t oldest;
	if (!G_LagSampleByOffset(ent, sample_count - 1, oldest))
		return false;

	if (target_frame < oldest.server_frame)
		return false;

	for (int32_t offset = 0; offset + 1 < sample_count; offset++) {
		lag_compensation_sample_t newer, older;
		if (!G_LagSampleByOffset(ent, offset, newer) ||
			!G_LagSampleByOffset(ent, offset + 1, older))
			return false;

		if (target_frame <= newer.server_frame && target_frame >= older.server_frame) {
			uint32_t span = newer.server_frame - older.server_frame;
			float frac = span ? static_cast<float>(target_frame - older.server_frame) / static_cast<float>(span) : 1.f;
			sample = G_InterpolateLagSamples(older, newer, target_frame, frac);
			return true;
		}
	}

	return G_LagSampleByOffset(ent, frames_back, sample);
}

static void G_ApplyLagCompensation(gentity_t *player, const lag_compensation_sample_t &sample, bool restore_bounds) {
	if (!player->client->is_lag_compensated) {
		player->client->is_lag_compensated = true;
		player->client->lag_restore.origin = player->s.origin;
		player->client->lag_restore.mins = player->mins;
		player->client->lag_restore.maxs = player->maxs;
		player->client->lag_restore.viewheight = player->viewheight;
		player->client->lag_restore.pmove_viewheight = player->client->ps.pmove.viewheight;
	}

	player->s.origin = sample.origin;

	if (restore_bounds) {
		player->mins = sample.mins;
		player->maxs = sample.maxs;
		player->viewheight = sample.viewheight;
		player->client->ps.pmove.viewheight = sample.pmove_viewheight;
	}

	gi.linkentity(player);
}

static bool G_LagCompensationEnabled(gentity_t *from_player, uint32_t current_frame, int32_t &frames_back) {
	// if you need this to fight monsters, you need help
	if (!deathmatch->integer)
		return false;
	else if (!g_lag_compensation->integer)
		return false;
	else if (!G_IsLagCompensationShooter(from_player))
		return false;
	else if (!G_LagCompensationFrameDelta(from_player, current_frame, frames_back))
		return false;

	return true;
}

static bool G_LagCompensateLegacy(gentity_t *from_player, const vec3_t &start, const vec3_t &dir, int32_t frames_back) {
	int32_t frame_delta = frames_back + 1;
	bool rewound = false;

	for (auto player : active_clients()) {
		// we aren't gonna hit ourselves
		if (player == from_player)
			continue;

		// not enough data, spare them
		if (G_LagCompensationSampleCount(player) < frame_delta)
			continue;

		// if they're way outside of cone of vision, they won't be captured in this
		if ((player->s.origin - start).normalized().dot(dir) < 0.75f)
			continue;

		lag_compensation_sample_t sample;
		if (!G_LagSampleByOffset(player, frame_delta - 1, sample)) {
			gi.Com_PrintFmt("{}: lag compensation error.\n", __FUNCTION__);
			G_UnLagCompensate();
			return false;
		}

		// no way they'd be hit if they aren't in the PVS
		if (!gi.inPVS(sample.origin, start, false))
			continue;

		G_ApplyLagCompensation(player, sample, false);
		rewound = true;
	}

	return rewound;
}

// [Paril-KEX] push all players' origins back to match their lag compensation
bool G_LagCompensate(gentity_t *from_player, const vec3_t &start, const vec3_t &dir) {
	uint32_t current_frame = gi.ServerFrame();
	int32_t frames_back = 0;

	if (!G_LagCompensationEnabled(from_player, current_frame, frames_back))
		return false;

	if (!g_lag_compensation_enhanced->integer)
		return G_LagCompensateLegacy(from_player, start, dir, frames_back);

	uint32_t target_frame = current_frame - static_cast<uint32_t>(frames_back);
	bool rewound = false;

	for (auto player : active_clients()) {
		// we aren't gonna hit ourselves
		if (player == from_player)
			continue;

		if (!G_IsEnhancedLagTarget(player))
			continue;

		lag_compensation_sample_t sample;
		if (!G_LagSampleAtFrame(player, target_frame, frames_back, sample))
			continue;

		// no way they'd be hit if they aren't in the PVS
		if (!gi.inPVS(sample.origin, start, false))
			continue;

		G_ApplyLagCompensation(player, sample, true);
		rewound = true;
	}

	return rewound;
}

static void G_RestoreLagCompensatedPlayer(gentity_t *player) {
	lag_compensation_restore_t restore = player->client->lag_restore;
	bool restore_bounds = G_IsEnhancedLagTarget(player);

	player->client->is_lag_compensated = false;
	player->s.origin = restore.origin;

	// Preserve death/spectator shape changes made while the player was rewound.
	if (restore_bounds) {
		player->mins = restore.mins;
		player->maxs = restore.maxs;
		player->viewheight = restore.viewheight;
		player->client->ps.pmove.viewheight = restore.pmove_viewheight;
	}

	player->client->lag_restore = {};
	gi.linkentity(player);
}

// [Paril-KEX] pop everybody's lag compensation values
void G_UnLagCompensate() {
	for (auto player : active_clients()) {
		if (player->client->is_lag_compensated)
			G_RestoreLagCompensatedPlayer(player);
	}
}

void G_ClearLagCompensationHistory(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (ent->client->is_lag_compensated)
		G_RestoreLagCompensatedPlayer(ent);

	ent->client->num_lag_origins = 0;
	ent->client->next_lag_origin = 0;
	ent->client->lag_restore = {};
}

static bool G_LagCompensationPositionDiscontinuity(gentity_t *ent) {
	if (!g_lag_compensation_enhanced->integer || G_LagCompensationSampleCount(ent) <= 0)
		return false;

	lag_compensation_sample_t latest;
	if (!G_LagSampleByOffset(ent, 0, latest))
		return false;

	float max_delta = max(512.f, g_maxvelocity->value * gi.frame_time_s * 2.f);
	return (ent->s.origin - latest.origin).lengthSquared() > (max_delta * max_delta);
}

// [Paril-KEX] save the current lag compensation value
static void G_SaveLagCompensation(gentity_t *ent) {
	if (g_lag_compensation_enhanced->integer && !G_IsEnhancedLagTarget(ent)) {
		G_ClearLagCompensationHistory(ent);
		return;
	}

	lag_compensation_sample_t *samples = G_LagCompensationSamples(ent);
	if (!samples)
		return;

	if (G_LagCompensationPositionDiscontinuity(ent))
		G_ClearLagCompensationHistory(ent);

	int32_t next_lag_origin = static_cast<int32_t>(ent->client->next_lag_origin) % game.max_lag_origins;
	ent->client->next_lag_origin = static_cast<uint16_t>(next_lag_origin);

	lag_compensation_sample_t &sample = samples[next_lag_origin];
	sample.server_frame = gi.ServerFrame();
	sample.time = level.time;
	sample.origin = ent->s.origin;
	sample.mins = ent->mins;
	sample.maxs = ent->maxs;
	sample.viewheight = ent->viewheight;
	sample.pmove_viewheight = ent->client->ps.pmove.viewheight;

	ent->client->next_lag_origin = static_cast<uint16_t>((next_lag_origin + 1) % game.max_lag_origins);

	if (ent->client->num_lag_origins < game.max_lag_origins)
		ent->client->num_lag_origins++;
}

void Frenzy_ApplyAmmoRegen(gentity_t *ent) {
	gclient_t *client;

	if (!g_frenzy->integer)
		return;

	if (InfiniteAmmoOn(nullptr))
		return;

	client = ent->client;
	if (!client)
		return;

	if (!client->frenzy_ammoregentime) {
		client->frenzy_ammoregentime = level.time;
		return;
	}

	if (client->frenzy_ammoregentime < level.time) {
		client->frenzy_ammoregentime = level.time;

		if (client->pers.inventory[IT_WEAPON_SHOTGUN] || client->pers.inventory[IT_WEAPON_SSHOTGUN]) {
			client->pers.inventory[IT_AMMO_SHELLS] += 4;

			if (client->pers.inventory[IT_AMMO_SHELLS] > client->pers.max_ammo[AMMO_SHELLS])
				client->pers.inventory[IT_AMMO_SHELLS] = client->pers.max_ammo[AMMO_SHELLS];
		}

		if (client->pers.inventory[IT_WEAPON_MACHINEGUN] || client->pers.inventory[IT_WEAPON_CHAINGUN]) {
			client->pers.inventory[IT_AMMO_BULLETS] += 10;

			if (client->pers.inventory[IT_AMMO_BULLETS] > client->pers.max_ammo[AMMO_BULLETS])
				client->pers.inventory[IT_AMMO_BULLETS] = client->pers.max_ammo[AMMO_BULLETS];
		}

		client->pers.inventory[IT_AMMO_GRENADES] += 2;

		if (client->pers.inventory[IT_AMMO_GRENADES] > client->pers.max_ammo[AMMO_GRENADES])
			client->pers.inventory[IT_AMMO_GRENADES] = client->pers.max_ammo[AMMO_GRENADES];

		if (client->pers.inventory[IT_WEAPON_RLAUNCHER]) {
			client->pers.inventory[IT_AMMO_ROCKETS] += 2;

			if (client->pers.inventory[IT_AMMO_ROCKETS] > client->pers.max_ammo[AMMO_ROCKETS])
				client->pers.inventory[IT_AMMO_ROCKETS] = client->pers.max_ammo[AMMO_ROCKETS];
		}

		if (client->pers.inventory[IT_WEAPON_HYPERBLASTER] || client->pers.inventory[IT_WEAPON_BFG] || client->pers.inventory[IT_WEAPON_IONRIPPER] || client->pers.inventory[IT_WEAPON_PLASMABEAM]) {
			client->pers.inventory[IT_AMMO_CELLS] += 8;

			if (client->pers.inventory[IT_AMMO_CELLS] > client->pers.max_ammo[AMMO_CELLS])
				client->pers.inventory[IT_AMMO_CELLS] = client->pers.max_ammo[AMMO_CELLS];
		}

		if (client->pers.inventory[IT_WEAPON_RAILGUN]) {
			client->pers.inventory[IT_AMMO_SLUGS] += 1;

			if (client->pers.inventory[IT_AMMO_SLUGS] > client->pers.max_ammo[AMMO_SLUGS])
				client->pers.inventory[IT_AMMO_SLUGS] = client->pers.max_ammo[AMMO_SLUGS];
		}

		if (client->pers.inventory[IT_WEAPON_PHALANX]) {
			client->pers.inventory[IT_AMMO_MAGSLUG] += 2;

			if (client->pers.inventory[IT_AMMO_MAGSLUG] > client->pers.max_ammo[AMMO_MAGSLUG])
				client->pers.inventory[IT_AMMO_MAGSLUG] = client->pers.max_ammo[AMMO_MAGSLUG];
		}

		if (client->pers.inventory[IT_WEAPON_ETF_RIFLE]) {
			client->pers.inventory[IT_AMMO_FLECHETTES] += 10;

			if (client->pers.inventory[IT_AMMO_FLECHETTES] > client->pers.max_ammo[AMMO_FLECHETTES])
				client->pers.inventory[IT_AMMO_FLECHETTES] = client->pers.max_ammo[AMMO_FLECHETTES];
		}

		if (client->pers.inventory[IT_WEAPON_PROXLAUNCHER]) {
			client->pers.inventory[IT_AMMO_PROX] += 1;

			if (client->pers.inventory[IT_AMMO_PROX] > client->pers.max_ammo[AMMO_PROX])
				client->pers.inventory[IT_AMMO_PROX] = client->pers.max_ammo[AMMO_PROX];
		}

		if (client->pers.inventory[IT_WEAPON_DISRUPTOR]) {
			client->pers.inventory[IT_AMMO_ROUNDS] += 1;

			if (client->pers.inventory[IT_AMMO_ROUNDS] > client->pers.max_ammo[AMMO_DISRUPTOR])
				client->pers.inventory[IT_AMMO_ROUNDS] = client->pers.max_ammo[AMMO_DISRUPTOR];
		}

		client->frenzy_ammoregentime += 2000_ms;
	}
}

/*
=================
ClientEndServerFrame

Called for each player at the end of the server frame
and right after spawning
=================
*/
void ClientEndServerFrame(gentity_t *ent) {
	// no player exists yet (load game)
	if (!ent->client->pers.spawned)
		return;

	// expire the brief "last one standing" centerprint. Only clear it while the round is
	// still live: if the round has since ended, a round-result centerprint is showing and
	// an instant empty centerprint would wipe it.
	if (ent->client->last_standing_clear_time && level.time >= ent->client->last_standing_clear_time) {
		if (level.round_state == roundst_t::ROUND_IN_PROGRESS)
			gi.LocClient_Print(ent, PRINT_CENTER, "");
		ent->client->last_standing_clear_time = 0_ms;
	}

	float bobtime, bobtime_run;
	gentity_t *e = ent;	// g_eyecam->integer &&ent->client->follow_target ? ent->client->follow_target : ent;

	current_player = e;
	current_client = e->client;

	if (deathmatch->integer) {
		int limit = level.match_state >= MATCH_IN_PROGRESS ? GT_ScoreLimit() : 0;
		ent->client->ps.stats[STAT_SCORELIMIT] = limit;
		if (limit != (int)strtoul(gi.get_configstring(CONFIG_STORY_SCORELIMIT), nullptr, 10)) {
			gi.configstring(CONFIG_STORY_SCORELIMIT, limit ? G_Fmt("{}", limit).data() : "");
		}
	}

	// check fog changes
	P_ForceFogTransition(ent, false);

	// check goals
	G_PlayerNotifyGoal(ent);

	// mega health
	P_RunMegaHealth(ent);

	// vampiric damage expiration
	// don't expire if only 1 player in the match
	if (g_vampiric_damage->integer && ClientIsPlaying(ent->client) && !IsCombatDisabled() && (ent->health > g_vampiric_exp_min->integer)) {
		if (level.num_playing_clients > 1 && level.time > ent->client->vampire_expiretime) {
			int quantity = floor((ent->health - 1) / ent->max_health) + 1;
			ent->health -= quantity;
			ent->client->vampire_expiretime = level.time + 1_sec;
			if (ent->health <= 0) {
				G_AdjustPlayerScore(ent->client, -1, GT(GT_TDM), -1);

				player_die(ent, ent, ent, 1, vec3_origin, { MOD_EXPIRE, true });
				if (!ent->client->eliminated)
					return;
			}
		}
	}

	//
	// If the origin or velocity have changed since ClientThink(),
	// update the pmove values.  This will happen when the client
	// is pushed by a bmodel or kicked by an explosion.
	//
	// If it wasn't updated here, the view position would lag a frame
	// behind the body position when pushed -- "sinking into plats"
	//
	current_client->ps.pmove.origin = ent->s.origin;
	current_client->ps.pmove.velocity = ent->velocity;

	//
	// If the end of unit layout is displayed, don't give
	// the player any normal movement attributes
	//
	if (level.intermission_time || ent->client->awaiting_respawn) {
		if (ent->client->awaiting_respawn || (level.intermission_eou || level.is_n64 || (deathmatch->integer && level.intermission_time))) {
			current_client->ps.screen_blend[3] = current_client->ps.damage_blend[3] = 0;
			current_client->ps.fov = 90;
			current_client->ps.gunindex = 0;
		}
		G_SetStats(ent);
		G_SetCoopStats(ent);

		// if the scoreboard is up, update it if a client leaves
		if (deathmatch->integer && ent->client->showscores && ent->client->menutime) {
			DeathmatchScoreboardMessage(e, e->enemy);
			gi.unicast(ent, false);
			ent->client->menutime = 0_ms;
		}

		return;
	}

	// auto doc tech
	Tech_ApplyAutoDoc(ent);

	// [MuffMode] Horde: expire timed techs (g_horde_tech_duration)
	Tech_ApplyExpiry(ent);

	// apply regeneration powerup
	Powerup_ApplyRegeneration(ent);

	// muff mode: weapons frenzy ammo regen
	Frenzy_ApplyAmmoRegen(ent);

	AngleVectors(ent->client->v_angle, forward, right, up);

	// burn from lava, etc
	P_WorldEffects();

	//
	// set model angles from view angles so other things in
	// the world can tell which direction you are looking
	//
	if (ent->client->v_angle[PITCH] > 180)
		ent->s.angles[PITCH] = (-360 + ent->client->v_angle[PITCH]) / 3;
	else
		ent->s.angles[PITCH] = ent->client->v_angle[PITCH] / 3;

	ent->s.angles[YAW] = ent->client->v_angle[YAW];
	ent->s.angles[ROLL] = 0;
	// [Paril-KEX] cl_rollhack
	ent->s.angles[ROLL] = -G_CalcRoll(ent->s.angles, ent->velocity) * 4;

	//
	// calculate speed and cycle to be used for
	// all cyclic walking effects
	//
	xyspeed = sqrt(ent->velocity[0] * ent->velocity[0] + ent->velocity[1] * ent->velocity[1]);

	if (xyspeed < 5) {
		bobmove = 0;
		current_client->bobtime = 0; // start at beginning of cycle again
	} else if (ent->groundentity) { // so bobbing only cycles when on ground
		if (xyspeed > 210)
			bobmove = gi.frame_time_ms / 400.f;
		else if (xyspeed > 100)
			bobmove = gi.frame_time_ms / 800.f;
		else
			bobmove = gi.frame_time_ms / 1600.f;
	}

	bobtime = (current_client->bobtime += bobmove);
	bobtime_run = bobtime;

	if ((current_client->ps.pmove.pm_flags & PMF_DUCKED) && ent->groundentity)
		bobtime *= 4;

	bobcycle = (int)bobtime;
	bobcycle_run = (int)bobtime_run;
	bobfracsin = fabsf(sinf(bobtime * PIf));

	// apply all the damage taken this frame
	P_DamageFeedback(e);

	// determine the view offsets
	G_CalcViewOffset(e);

	// determine the gun offsets
	G_CalcGunOffset(e);

	// determine the full screen color blend
	// must be after viewoffset, so eye contents can be
	// accurately determined
	G_CalcBlend(e);

	// chase cam stuff
	if (!ClientIsPlaying(ent->client) || ent->client->eliminated) {
		G_SetSpectatorStats(ent);

		if (ent->client->follow_target && ent->client->follow_target->client) {
			ent->client->ps.screen_blend = ent->client->follow_target->client->ps.screen_blend;
			ent->client->ps.damage_blend = ent->client->follow_target->client->ps.damage_blend;

			ent->s.effects = ent->client->follow_target->s.effects;
			ent->s.renderfx = ent->client->follow_target->s.renderfx;
		}
	} else
		G_SetStats(ent);

	G_CheckChaseStats(ent);

	G_SetCoopStats(ent);

	G_SetClientEvent(ent);

	G_SetClientEffects(e);

	G_SetClientSound(e);

	G_SetClientFrame(ent);
	
	ent->client->oldvelocity = ent->velocity;
	ent->client->oldviewangles = ent->client->ps.viewangles;
	ent->client->oldgroundentity = ent->groundentity;

	if (ent->client->menudirty && ent->client->menutime <= level.time) {
		if (ent->client->menu) {
			P_Menu_Do_Update(ent);
			gi.unicast(ent, true);
		}
		ent->client->menutime = level.time;
		ent->client->menudirty = false;
	}

	// if the scoreboard is up, update it
	if (ent->client->showscores && ent->client->menutime <= level.time) {
		if (ent->client->menu) {
			P_Menu_Do_Update(ent);
			ent->client->menudirty = false;
		} else {
			DeathmatchScoreboardMessage(e, e->enemy);
		}
		gi.unicast(ent, false);
		ent->client->menutime = level.time + 3_sec;
	}

	if ((ent->svflags & SVF_BOT) != 0) {
		Bot_EndFrame(ent);
	}

	P_AssignClientSkinnum(ent);

	if (deathmatch->integer)
		G_SaveLagCompensation(ent);

	Compass_Update(ent, false);

	// [Paril-KEX] in coop, if player collision is enabled and
	// we are currently in no-player-collision mode, check if
	// it's safe.
	if (InCoopStyle() && G_ShouldPlayersCollide(false) && !(ent->clipmask & CONTENTS_PLAYER) && ent->takedamage) {
		bool clipped_player = false;

		for (auto player : active_clients()) {
			if (player == ent)
				continue;

			trace_t clip = gi.clip(player, ent->s.origin, ent->mins, ent->maxs, ent->s.origin, CONTENTS_MONSTER | CONTENTS_PLAYER);

			if (clip.startsolid || clip.allsolid) {
				clipped_player = true;
				break;
			}
		}

		// safe!
		if (!clipped_player)
			ent->clipmask |= CONTENTS_PLAYER;
	}
}
