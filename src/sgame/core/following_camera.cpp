// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include <cctype>

#include "g_local.h"
// [MuffMode] Join menu lives in muffmode/mm_menu
#include "muffmode/mm_arena.h"
#include "muffmode/mm_menu.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_pconfig.h"
#include "muffmode/mm_skin.h"
#include "muffmode/mm_team.h"

static bool FollowTargetIsActive(gentity_t *target) {
	return target && target->inuse && target->client && ClientIsPlaying(target->client) && !target->client->eliminated;
}

bool FollowTargetAllowed(gentity_t *viewer, gentity_t *target) {
	if (!viewer || !viewer->client || !FollowTargetIsActive(target))
		return false;

	if (GT(GT_ARENA) && !MM_Arena_CanFollow(viewer, target))
		return false;

	// RA3's normal (non-competition) Rocket Arena lets eliminated fighters
	// observe either side. Other elimination modes retain teammate-only follow.
	if (viewer->client->eliminated && notGT(GT_ARENA) &&
		viewer->client->sess.team != target->client->sess.team)
		return false;

	return true;
}

bool FollowFirstPersonEnabled(const gentity_t *ent) {
	return ent && ent->client &&
		(ent->client->sess.pc.follow_first_person || MM_Arena_ForceFirstPerson(ent));
}

static void ClearFollowEntityPresentation(gentity_t *ent) {
	if (!ent)
		return;

	ent->s.effects = EF_NONE;
	ent->s.renderfx &= RF_STAIR_STEP;
	ent->s.renderfx |= RF_IR_VISIBLE;
	ent->s.sound = 0;
	ent->s.loop_attenuation = 0;
	ent->s.loop_volume = 0;
}

static void PrintFollowTargetChange(gentity_t *ent, gentity_t *previous_target, gentity_t *target) {
	if (!ent || !ent->client || previous_target == target)
		return;

	if (!target && previous_target)
		gi.LocClient_Print(ent, PRINT_HIGH, "Stopped following.\n");
}

static void PrintNoFollowTargets(gentity_t *ent) {
	if (!ent || !ent->client || ent->client->follow_msg_time > level.time)
		return;

	gi.LocClient_Print(ent, PRINT_HIGH, "No players to follow.\n");
	ent->client->follow_msg_time = level.time + 5_sec;
}

static void PrintNoOtherFollowTargets(gentity_t *ent) {
	if (!ent || !ent->client || ent->client->follow_msg_time > level.time)
		return;

	gi.LocClient_Print(ent, PRINT_HIGH, "No other players to follow.\n");
	ent->client->follow_msg_time = level.time + 5_sec;
}

static bool FollowTargetNeedsInstancing(gentity_t *target) {
	if (!target || !target->client)
		return false;

	for (auto viewer : active_clients())
		if (viewer->client->follow_target == target && FollowFirstPersonEnabled(viewer))
			return true;

	return false;
}

static void RefreshFollowTargetInstancing(gentity_t *target) {
	if (!target || !target->client)
		return;

	if (FollowTargetNeedsInstancing(target))
		target->svflags |= SVF_INSTANCED;
	else
		target->svflags &= ~SVF_INSTANCED;
}

void SyncFollowPresentation(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	gentity_t *target = ent->client->follow_target;
	if (!target || !target->client)
		return;

	ent->client->ps.screen_blend = target->client->ps.screen_blend;
	ent->client->ps.damage_blend = target->client->ps.damage_blend;

	if (!FollowFirstPersonEnabled(ent))
		return;

	ent->s.effects = target->s.effects;
	ent->s.renderfx = target->s.renderfx;
	ent->s.sound = target->s.sound;
	ent->s.loop_attenuation = target->s.loop_attenuation;
	ent->s.loop_volume = target->s.loop_volume;
}

void ToggleFollowViewMode(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (MM_Arena_ForceFirstPerson(ent)) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"Competition mode requires first-person follow.\n");
		return;
	}

	const bool first_person = !ent->client->sess.pc.follow_first_person;
	if (!MM_PConfigSetBool(ent, mm_pconfig_bool_setting_t::follow_first_person, first_person))
		return;

	gi.LocClient_Print(ent, PRINT_HIGH, "Follow view: {}\n", MM_PConfigFollowViewName(first_person));
	gi.local_sound(ent, CHAN_AUTO, gi.soundindex("misc/menu3.wav"), 1, ATTN_NONE, 0);
}

void SetFollowTarget(gentity_t *ent, gentity_t *target) {
	if (!ent || !ent->client)
		return;

	gentity_t *previous_target = ent->client->follow_target;
	ent->client->follow_target = target;
	ent->client->follow_update = true;

	if (target)
		ent->client->sess.spectator_state = SPECTATOR_FOLLOW;

	if (target && target->client) {
		const ptrdiff_t clientnum = target - g_entities - 1;
		if (clientnum >= 0 && clientnum < game.maxclients)
			ent->client->sess.spectator_client = static_cast<int8_t>(clientnum);
	}

	RefreshFollowTargetInstancing(previous_target);
	RefreshFollowTargetInstancing(target);
	MM_RefreshSkinOverridesForViewer(ent);
	PrintFollowTargetChange(ent, previous_target, target);
}

void FreeFollower(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (!ent->client->follow_target)
		return;

	SetFollowTarget(ent, nullptr);
	if (!ClientIsPlaying(ent->client) || ent->client->eliminated)
		ent->client->sess.spectator_state = SPECTATOR_FREE;
	ent->client->ps.pmove.pm_flags &= ~(PMF_NO_POSITIONAL_PREDICTION | PMF_NO_ANGULAR_PREDICTION);

	ent->client->ps.kick_angles = {};
	ent->client->ps.gunangles = {};
	ent->client->ps.gunoffset = {};
	ent->client->ps.gunindex = 0;
	ent->client->ps.gunskin = 0;
	ent->client->ps.gunframe = 0;
	ent->client->ps.gunrate = 0;
	ent->client->ps.screen_blend = {};
	ent->client->ps.damage_blend = {};
	ent->client->ps.rdflags = RDF_NONE;
	ClearFollowEntityPresentation(ent);
}

void FreeClientFollowers(gentity_t *ent) {
	if (!ent)
		return;

	for (auto ec : active_clients()) {
		if (!ec->client->follow_target)
			continue;
		if (ec->client->follow_target == ent)
			FreeFollower(ec);
	}
}

void UpdateFollowCamera(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	vec3_t	o, ownerv, goal;
	gentity_t	*targ = ent->client->follow_target;
	vec3_t	forward, right;
	trace_t	trace;
	vec3_t	oldgoal;
	vec3_t	angles;
	
	// is our follow target gone?
	if (!FollowTargetIsActive(targ)) {
		//SetTeam(ent, TEAM_SPECTATOR, false, false, false);
		if (targ)
			FreeClientFollowers(targ);
		else
			SetFollowTarget(ent, nullptr);
		if (!ClientIsPlaying(ent->client) || ent->client->eliminated)
			GetFollowTarget(ent);
		return;
	}

	if (!FollowTargetAllowed(ent, targ)) {
		FreeFollower(ent);
		if (!ClientIsPlaying(ent->client) || ent->client->eliminated)
			GetFollowTarget(ent);
		return;
	}

	ownerv = targ->s.origin;
	oldgoal = ent->s.origin;
	const bool first_person = FollowFirstPersonEnabled(ent);

	// First-person follow handling
	if (first_person) {
		// Mark the followed player as instanced so we can disable their model for this viewer.
		targ->svflags |= SVF_INSTANCED;

		// copy everything from ps but pmove, pov, stats, and team
		ent->client->ps.viewangles = targ->client->ps.viewangles;
		ent->client->ps.viewoffset = targ->client->ps.viewoffset;
		ent->client->ps.kick_angles = targ->client->ps.kick_angles;
		ent->client->ps.gunangles = targ->client->ps.gunangles;
		ent->client->ps.gunoffset = targ->client->ps.gunoffset;
		ent->client->ps.gunindex = targ->client->ps.gunindex;
		ent->client->ps.gunskin = targ->client->ps.gunskin;
		ent->client->ps.gunframe = targ->client->ps.gunframe;
		ent->client->ps.gunrate = targ->client->ps.gunrate;
		ent->client->ps.screen_blend = targ->client->ps.screen_blend;
		ent->client->ps.damage_blend = targ->client->ps.damage_blend;
		ent->client->ps.fov = targ->client->ps.fov;
		ent->client->ps.rdflags = targ->client->ps.rdflags;
		ent->client->ps.pmove.pm_flags &= ~(PMF_NO_POSITIONAL_PREDICTION | PMF_NO_ANGULAR_PREDICTION);

		// do pmove stuff so view looks right, but not pm_flags
		ent->client->ps.pmove.origin = targ->client->ps.pmove.origin;
		ent->client->ps.pmove.velocity = targ->client->ps.pmove.velocity;
		ent->client->ps.pmove.pm_time = targ->client->ps.pmove.pm_time;
		ent->client->ps.pmove.gravity = targ->client->ps.pmove.gravity;
		// Zero delta_angles - view is fully authoritative, avoids jitter from spectator cmd_angles mismatch
		ent->client->ps.pmove.delta_angles = {};
		ent->client->ps.pmove.viewheight = targ->client->ps.pmove.viewheight;
		
		ent->client->pers.hand = targ->client->pers.hand;
		
		// unadjusted view and origin handling
		angles = targ->client->v_angle;
		AngleVectors(angles, forward, right, nullptr);
		forward.normalize();
		// Align ent origin with pmove (view position) for consistency
		goal = targ->client->ps.pmove.origin;

		ent->s.modelindex = 0;
		ent->s.modelindex2 = 0;
		ent->s.modelindex3 = 0;
	}
	// vanilla follow camera code
	else {
		RefreshFollowTargetInstancing(targ);
		ClearFollowEntityPresentation(ent);

		ownerv[2] += targ->viewheight;

		angles = targ->client->v_angle;
		if (angles[PITCH] > 56)
			angles[PITCH] = 56;
		AngleVectors(angles, forward, right, nullptr);
		forward.normalize();
		o = ownerv + (forward * -30);

		if (o[2] < targ->s.origin[2] + 20)
			o[2] = targ->s.origin[2] + 20;

		// jump animation lifts
		if (!targ->groundentity)
			o[2] += 16;

		trace = gi.traceline(ownerv, o, targ, MASK_SOLID);

		goal = trace.endpos;

		goal += (forward * 2);

		// pad for floors and ceilings
		o = goal;
		o[2] += 6;
		trace = gi.traceline(goal, o, targ, MASK_SOLID);
		if (trace.fraction < 1) {
			goal = trace.endpos;
			goal[2] -= 6;
		}

		o = goal;
		o[2] -= 6;
		trace = gi.traceline(goal, o, targ, MASK_SOLID);
		if (trace.fraction < 1) {
			goal = trace.endpos;
			goal[2] += 6;
		}

		ent->client->ps.gunindex = 0;
		ent->client->ps.gunskin = 0;
		ent->s.modelindex = 0;
		ent->s.modelindex2 = 0;
		ent->s.modelindex3 = 0;
	}

	if (ent->client->eliminated) {
		ent->svflags |= SVF_NOCLIENT;
		ent->s.modelindex = 0;
		ent->s.modelindex2 = 0;
		ent->s.modelindex3 = 0;
	}

	if (targ->deadflag)
		ent->client->ps.pmove.pm_type = PM_DEAD;
	else
		ent->client->ps.pmove.pm_type = PM_FREEZE;

	ent->s.origin = goal;
	if (!first_person)
		ent->client->ps.pmove.delta_angles = targ->client->v_angle - ent->client->resp.cmd_angles;

	if (targ->deadflag) {
		ent->client->ps.viewangles[ROLL] = 40;
		ent->client->ps.viewangles[PITCH] = -15;
		ent->client->ps.viewangles[YAW] = targ->client->killer_yaw;
	} else {
		ent->client->ps.viewangles = targ->client->v_angle;
		ent->client->v_angle = targ->client->v_angle;
		AngleVectors(ent->client->v_angle, ent->client->v_forward, nullptr, nullptr);
	}
	
	gentity_t *e = targ ? targ : ent;
	ent->client->ps.stats[STAT_SHOW_STATUSBAR] = !ClientIsPlaying(e->client) || e->client->eliminated ? 0 : 1;

	ent->viewheight = 0;
	if (!first_person)
		ent->client->ps.pmove.pm_flags |= PMF_NO_POSITIONAL_PREDICTION | PMF_NO_ANGULAR_PREDICTION;
	SyncFollowPresentation(ent);
	gi.linkentity(ent);
}

/*
==================
SanitizeString

Remove case and control characters
==================
*/
static void SanitizeString(const char *in, char *out, size_t out_size) {
	if (!out || out_size == 0)
		return;

	char *write = out;
	size_t remaining = out_size - 1;

	if (!in) {
		*out = '\0';
		return;
	}

	while (*in && remaining > 0) {
		const unsigned char ch = static_cast<unsigned char>(*in++);
		if (ch < ' ')
			continue;

		*write++ = static_cast<char>(std::tolower(ch));
		remaining--;
	}

	*write = '\0';
}

static bool ParseClientSlot(const char *s, uint32_t &idnum) {
	const auto value = MM_ParseUInt32Arg(s);
	if (!value || game.maxclients <= 0 || *value >= static_cast<uint32_t>(game.maxclients))
		return false;

	idnum = *value;
	return true;
}

/*
==================
ClientNumberFromString

Returns a player number for either a number or name string
Returns -1 if invalid
==================
*/
static int ClientNumberFromString(gentity_t *to, char *s) {
	gclient_t	*cl;
	uint32_t	idnum;
	char		s2[MAX_STRING_CHARS];
	char		n2[MAX_STRING_CHARS];

	if (!s || !*s)
		return -1;
	
	// numeric values are just slot numbers
	if (s[0] >= '0' && s[0] <= '9') {
		if (!ParseClientSlot(s, idnum)) {
			gi.LocClient_Print(to, PRINT_HIGH, "Bad client slot: {}\n", s);
			return -1;
		}

		cl = &game.clients[idnum];
		if (!cl->pers.connected) {
			gi.LocClient_Print(to, PRINT_HIGH, "Client {} is not active.\n", idnum);
			return -1;
		}
		return idnum;
	}

	// check for a name match
	SanitizeString(s, s2, sizeof(s2));
	for (idnum = 0, cl = game.clients; idnum < game.maxclients; idnum++, cl++) {
		if (!cl->pers.connected)
			continue;
		SanitizeString(cl->resp.netname, n2, sizeof(n2));
		if (!strcmp(n2, s2)) {
			return idnum;
		}
	}

	gi.LocClient_Print(to, PRINT_HIGH, "User {} is not on the server.\n", s);
	return -1;
}

void FollowNext(gentity_t *ent) {
	ptrdiff_t i;
	gentity_t *e;
	gentity_t *current_target;

	if (!ent || !ent->client || !ent->client->follow_target)
		return;

	current_target = ent->client->follow_target;
	if (!FollowTargetAllowed(ent, current_target)) {
		GetFollowTarget(ent);
		return;
	}

	i = current_target - g_entities;
	for (;;) {
		i++;
		if (i > game.maxclients)
			i = 1;
		e = g_entities + i;

		if (e == current_target)
			break;

		if (FollowTargetAllowed(ent, e)) {
			SetFollowTarget(ent, e);
			UpdateFollowCamera(ent);
			return;
		}
	}

	PrintNoOtherFollowTargets(ent);
}

void FollowPrev(gentity_t *ent) {
	int		 i;
	gentity_t *e;
	gentity_t *current_target;

	if (!ent || !ent->client || !ent->client->follow_target)
		return;

	current_target = ent->client->follow_target;
	if (!FollowTargetAllowed(ent, current_target)) {
		GetFollowTarget(ent);
		return;
	}

	i = current_target - g_entities;
	for (;;) {
		i--;
		if (i < 1)
			i = game.maxclients;
		e = g_entities + i;

		if (e == current_target)
			break;

		if (FollowTargetAllowed(ent, e)) {
			SetFollowTarget(ent, e);
			UpdateFollowCamera(ent);
			return;
		}
	}

	PrintNoOtherFollowTargets(ent);
}

void FollowCycle(gentity_t *ent, int dir) {
	int			clientnum;
	int			original;
	const int	max_clients = static_cast<int>(game.maxclients);
	gclient_t	*cl = ent->client;
	gentity_t		*follow_ent = nullptr;

	if (max_clients <= 0 || dir == 0)
		return;

	// if they are playing a duel game, count as a loss
	if (GT(GT_DUEL) && ent->client->sess.team == TEAM_FREE)
		ent->client->sess.losses++;

	// first set them to spectator
	if (cl->sess.spectator_state == SPECTATOR_NOT && !cl->eliminated)
		SetTeam(ent, TEAM_SPECTATOR, false, false, false);

	clientnum = clamp(static_cast<int>(cl->sess.spectator_client), 0, max_clients - 1);
	original = clientnum;
	do {
		if (dir > 0)
			clientnum = (clientnum + 1 >= max_clients) ? 0 : clientnum + 1;
		else
			clientnum = (clientnum <= 0) ? max_clients - 1 : clientnum - 1;

		follow_ent = &g_entities[clientnum + 1];

		if (!FollowTargetAllowed(ent, follow_ent))
			continue;

		// this is good, we can use it
		//q3
		cl->sess.spectator_client = clientnum;

		//q2
		SetFollowTarget(ent, follow_ent);
		UpdateFollowCamera(ent);

		return;
	} while (clientnum != original);

	// leave it where it was
	if (!cl->follow_target)
		PrintNoFollowTargets(ent);
}

void GetFollowTarget(gentity_t *ent) {
	for (auto ec : active_clients()) {
		if (FollowTargetAllowed(ent, ec)) {
			SetFollowTarget(ent, ec);
			UpdateFollowCamera(ent);
			return;
		}
	}

	PrintNoFollowTargets(ent);
}
