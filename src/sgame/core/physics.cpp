// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Entity physics and movement integration.

#include "g_local.h"

/*


pushmove objects do not obey gravity, and do not interact with each other or trigger fields, but block normal movement
and push normal objects when they move.

onground is set for toss objects when they come to a complete rest.  it is set for steping or walking objects

doors, plats, etc are SOLID_BSP, and MOVETYPE_PUSH
bonus items are SOLID_TRIGGER touch, and MOVETYPE_TOSS
corpses are SOLID_NOT and MOVETYPE_TOSS
crates are SOLID_BBOX and MOVETYPE_TOSS
walking monsters are SOLID_SLIDEBOX and MOVETYPE_STEP
flying/floating monsters are SOLID_SLIDEBOX and MOVETYPE_FLY

solid_edge items only clip against bsp models.

*/

void G_Physics_NewToss(gentity_t *ent); // PGM

// [Paril-KEX] fetch the clipmask for this entity; certain modifiers
// affect the clipping behavior of objects.
contents_t G_GetClipMask(gentity_t *ent) {
	contents_t mask = ent->clipmask;

	// default masks
	if (!mask) {
		if (ent->svflags & SVF_MONSTER)
			mask = MASK_MONSTERSOLID;
		else if (ent->svflags & SVF_PROJECTILE)
			mask = MASK_PROJECTILE;
		else
			mask = MASK_SHOT & ~CONTENTS_DEADMONSTER;
	}

	// non-solid objects (items, etc) shouldn't try to clip
	// against players/monsters
	if (ent->solid == SOLID_NOT || ent->solid == SOLID_TRIGGER)
		mask &= ~(CONTENTS_MONSTER | CONTENTS_PLAYER);

	// monsters/players that are also dead shouldn't clip
	// against players/monsters
	if ((ent->svflags & (SVF_MONSTER | SVF_PLAYER)) && (ent->svflags & SVF_DEADMONSTER))
		mask &= ~(CONTENTS_MONSTER | CONTENTS_PLAYER);

	// Horde: monsters don't clip against each other (see q2horde)
	if (GT(GT_HORDE) && (ent->svflags & SVF_MONSTER))
		mask &= ~CONTENTS_MONSTER;

	return mask;
}

/*
============
G_TestEntityPosition

============
*/
static gentity_t *G_TestEntityPosition(gentity_t *ent) {
	trace_t	   trace;

	trace = gi.trace(ent->s.origin, ent->mins, ent->maxs, ent->s.origin, ent, G_GetClipMask(ent));

	if (trace.startsolid)
		return g_entities;

	return nullptr;
}

/*
================
G_CheckVelocity
================
*/
void G_CheckVelocity(gentity_t *ent) {
	//
	// bound velocity
	//
	float speed = ent->velocity.length();

	if (speed > g_maxvelocity->value)
		ent->velocity = (ent->velocity / speed) * g_maxvelocity->value;
}

/*
=============
G_RunThink

Runs thinking code for this frame if necessary
=============
*/
bool G_RunThink(gentity_t *ent) {
	gtime_t thinktime = ent->nextthink;
	if (thinktime <= 0_ms)
		return true;
	if (thinktime > level.time)
		return true;

	ent->nextthink = 0_ms;
	if (!ent->think)
		//gi.Com_Error("nullptr ent->think");
		return false;	//true;
	ent->think(ent);

	return false;
}

/*
==================
G_Impact

Two entities have touched, so run their touch functions
==================
*/
void G_Impact(gentity_t *e1, const trace_t &trace) {
	gentity_t *e2 = trace.ent;

	if (e1->touch && (e1->solid != SOLID_NOT || (e1->flags & FL_ALWAYS_TOUCH)))
		e1->touch(e1, e2, trace, false);

	if (e2->touch && (e2->solid != SOLID_NOT || (e2->flags & FL_ALWAYS_TOUCH)))
		e2->touch(e2, e1, trace, true);
}

/*
============
G_FlyMove

The basic solid body movement clip that slides along multiple planes
============
*/
void G_FlyMove(gentity_t *ent, float time, contents_t mask) {
	ent->groundentity = nullptr;

	touch_list_t touch;
	PM_StepSlideMove_Generic(ent->s.origin, ent->velocity, time, ent->mins, ent->maxs, touch, false, [&](const vec3_t &start, const vec3_t &mins, const vec3_t &maxs, const vec3_t &end) {
		return gi.trace(start, mins, maxs, end, ent, mask);
		});

	for (size_t i = 0; i < touch.num; i++) {
		auto &trace = touch.traces[i];

		if (trace.plane.normal[2] > 0.7f) {
			ent->groundentity = trace.ent;
			ent->groundentity_linkcount = trace.ent->linkcount;
		}

		//
		// run the impact function
		//
		G_Impact(ent, trace);

		// impact func requested velocity kill
		if (ent->flags & FL_KILL_VELOCITY) {
			ent->flags &= ~FL_KILL_VELOCITY;
			ent->velocity = {};
		}
	}
}

/*
============
G_AddGravity

============
*/
void G_AddGravity(gentity_t *ent) {
	ent->velocity += ent->gravityVector * (ent->gravity * level.gravity * gi.frame_time_s);
}

/*
===============================================================================

PUSHMOVE

===============================================================================
*/

/*
============
G_PushEntity

Does not change the entities velocity at all
============
*/
static trace_t G_PushEntity(gentity_t *ent, const vec3_t &push) {
	vec3_t start = ent->s.origin;
	vec3_t end = start + push;

	trace_t trace = gi.trace(start, ent->mins, ent->maxs, end, ent, G_GetClipMask(ent));

	ent->s.origin = trace.endpos + (trace.plane.normal * .5f);
	gi.linkentity(ent);

	if (trace.fraction != 1.0f || trace.startsolid) {
		G_Impact(ent, trace);

		// if the pushed entity went away and the pusher is still there
		if (!trace.ent->inuse && ent->inuse) {
			// move the pusher back and try again
			ent->s.origin = start;
			gi.linkentity(ent);
			return G_PushEntity(ent, push);
		}
	}

	// FIXME - is this needed?
	ent->gravity = 1.0;

	if (ent->inuse)
		G_TouchTriggers(ent);

	return trace;
}

struct pushed_t {
	gentity_t *ent;
	vec3_t	 origin;
	vec3_t	 angles;
	bool	 rotated;
	float	 yaw;
};

pushed_t pushed[MAX_ENTITIES], *pushed_p;

gentity_t *obstacle;

static pushed_t *G_ReservePushed() {
	if (pushed_p >= &pushed[MAX_ENTITIES])
		gi.Com_Error("pushed_p >= &pushed[MAX_ENTITIES], memory corrupted");

	return pushed_p++;
}

static uint32_t G_PhysicsEntityLimit() {
	return min(globals.num_entities, game.maxentities);
}

/*
============
G_Push

Objects need to be moved back on a failed push,
otherwise riders would continue to slide.
============
*/
static bool G_Push(gentity_t *pusher, vec3_t &move, vec3_t &amove) {
	gentity_t *check, *block = nullptr;
	vec3_t	  mins, maxs;
	pushed_t *p;
	vec3_t	  org, org2, move2, forward, right, up;

	// find the bounding box
	mins = pusher->absmin + move;
	maxs = pusher->absmax + move;

	// we need this for pushing things later
	org = -amove;
	AngleVectors(org, forward, right, up);

	// save the pusher's original position
	p = G_ReservePushed();
	p->ent = pusher;
	p->origin = pusher->s.origin;
	p->angles = pusher->s.angles;
	p->rotated = false;

	// move the pusher to it's final position
	pusher->s.origin += move;
	pusher->s.angles += amove;
	gi.linkentity(pusher);

	// see if any solid entities are inside the final position
	check = g_entities + 1;
	const uint32_t entity_limit = G_PhysicsEntityLimit();
	for (uint32_t e = 1; e < entity_limit; e++, check++) {
		if (!check->inuse)
			continue;
		if (check->movetype == MOVETYPE_PUSH || check->movetype == MOVETYPE_STOP || check->movetype == MOVETYPE_NONE ||
			check->movetype == MOVETYPE_NOCLIP || check->movetype == MOVETYPE_FREECAM)
			continue;

		if (!check->linked)
			continue; // not linked in anywhere

		// if the entity is standing on the pusher, it will definitely be moved
		if (check->groundentity != pusher) {
			// see if the ent needs to be tested
			if (check->absmin[0] >= maxs[0] || check->absmin[1] >= maxs[1] || check->absmin[2] >= maxs[2] ||
				check->absmax[0] <= mins[0] || check->absmax[1] <= mins[1] || check->absmax[2] <= mins[2])
				continue;

			// see if the ent's bbox is inside the pusher's final position
			if (!G_TestEntityPosition(check))
				continue;
		}

		if ((pusher->movetype == MOVETYPE_PUSH) || (check->groundentity == pusher)) {
			// move this entity
			p = G_ReservePushed();
			p->ent = check;
			p->origin = check->s.origin;
			p->angles = check->s.angles;
			p->rotated = !!amove[YAW];
			if (p->rotated)
				p->yaw =
				pusher->client ? (float)pusher->client->ps.pmove.delta_angles[YAW] : pusher->s.angles[YAW];

			vec3_t old_position = check->s.origin;

			// try moving the contacted entity
			check->s.origin += move;
			if (check->client) {
				// Paril: disabled because in vanilla delta_angles are never
				// lerped. delta_angles can probably be lerped as long as event
				// isn't EV_PLAYER_TELEPORT or a new RDF flag is set
				// check->client->ps.pmove.delta_angles[YAW] += amove[YAW];
			} else
				check->s.angles[YAW] += amove[YAW];

			// figure movement due to the pusher's amove
			org = check->s.origin - pusher->s.origin;
			org2[0] = org.dot(forward);
			org2[1] = -(org.dot(right));
			org2[2] = org.dot(up);
			move2 = org2 - org;
			check->s.origin += move2;

			// may have pushed them off an edge
			if (check->groundentity != pusher)
				check->groundentity = nullptr;

			block = G_TestEntityPosition(check);

			// [Paril-KEX] this is a bit of a hack; allow dead player skulls
			// to be a blocker because otherwise elevators/doors get stuck
			if (block && check->client && !check->takedamage) {
				check->s.origin = old_position;
				block = nullptr;
			}

			if (!block) { // pushed ok
				gi.linkentity(check);
				// impact?
				continue;
			}

			// if it is ok to leave in the old position, do it.
			// this is only relevent for riding entities, not pushed
			check->s.origin = old_position;
			block = G_TestEntityPosition(check);
			if (!block) {
				pushed_p--;
				continue;
			}
		}

		// save off the obstacle so we can call the block function
		obstacle = check;

		// move back any entities we already moved
		// go backwards, so if the same entity was pushed
		// twice, it goes back to the original position
		for (p = pushed_p - 1; p >= pushed; p--) {
			p->ent->s.origin = p->origin;
			p->ent->s.angles = p->angles;
			if (p->rotated) {
				//if (p->ent->client)
				//	p->ent->client->ps.pmove.delta_angles[YAW] = p->yaw;
				//else
				p->ent->s.angles[YAW] = p->yaw;
			}
			gi.linkentity(p->ent);
		}
		return false;
	}

	// FIXME: is there a better way to handle this?
	//  see if anything we moved has touched a trigger
	for (p = pushed_p - 1; p >= pushed; p--)
		G_TouchTriggers(p->ent);

	return true;
}

/*
================
G_Physics_Pusher

Bmodel objects don't interact with each other, but
push all box objects
================
*/
static void G_Physics_Pusher(gentity_t *ent) {
	vec3_t	 move, amove;
	gentity_t *part;

	// if not a team captain, so movement will be handled elsewhere
	if (ent->flags & FL_TEAMSLAVE)
		return;

	// make sure all team slaves can move before commiting
	// any moves or calling any think functions
	// if the move is blocked, all moved objects will be backed out
retry:
	pushed_p = pushed;
	for (part = ent; part; part = part->teamchain) {
		if (part->velocity[0] || part->velocity[1] || part->velocity[2] || part->avelocity[0] || part->avelocity[1] ||
			part->avelocity[2]) { // object is moving
			move = part->velocity * gi.frame_time_s;
			amove = part->avelocity * gi.frame_time_s;

			if (!G_Push(part, move, amove))
				break; // move was blocked
		}
	}
	if (pushed_p > &pushed[MAX_ENTITIES])
		gi.Com_Error("pushed_p > &pushed[MAX_ENTITIES], memory corrupted");

	if (part) {
		// if the pusher has a "blocked" function, call it
		// otherwise, just stay in place until the obstacle is gone
		if (part->moveinfo.blocked) {
			if (obstacle->inuse && obstacle->movetype != MOVETYPE_FREECAM && obstacle->movetype != MOVETYPE_NOCLIP)
				part->moveinfo.blocked(part, obstacle);
		}

		if (!obstacle->inuse)
			goto retry;
	} else {
		// the move succeeded, so call all think functions
		for (part = ent; part; part = part->teamchain) {
			// prevent entities that are on trains that have gone away from thinking!
			if (part->inuse)
				G_RunThink(part);
		}
	}
}

//==================================================================

/*
=============
G_Physics_None

Non moving objects can only think
=============
*/
static void G_Physics_None(gentity_t *ent) {
	// regular thinking
	G_RunThink(ent);
}

/*
=============
G_Physics_Noclip

A moving object that doesn't obey physics
=============
*/
static void G_Physics_Noclip(gentity_t *ent) {
	// regular thinking
	if (!G_RunThink(ent) || !ent->inuse)
		return;

	ent->s.angles += (ent->avelocity * gi.frame_time_s);
	ent->s.origin += (ent->velocity * gi.frame_time_s);

	gi.linkentity(ent);
}

/*
==============================================================================

TOSS / BOUNCE

==============================================================================
*/

/*
=============
G_Physics_Toss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
static void G_Physics_Toss(gentity_t *ent) {
	trace_t	 trace;
	vec3_t	 move;
	float	 backoff;
	gentity_t *slave;
	bool	 wasinwater;
	bool	 isinwater;
	vec3_t	 old_origin;

	// regular thinking
	G_RunThink(ent);

	if (!ent->inuse)
		return;

	// if not a team captain, so movement will be handled elsewhere
	if (ent->flags & FL_TEAMSLAVE)
		return;

	if (ent->velocity[2] > 0)
		ent->groundentity = nullptr;

	// check for the groundentity going away
	if (ent->groundentity)
		if (!ent->groundentity->inuse)
			ent->groundentity = nullptr;

	// if onground, return without moving
	if (ent->groundentity && ent->gravity > 0.0f) // PGM - gravity hack
	{
		if (ent->svflags & SVF_MONSTER) {
			M_CatagorizePosition(ent, ent->s.origin, ent->waterlevel, ent->watertype);
			M_WorldEffects(ent);
		}

		return;
	}

	old_origin = ent->s.origin;

	G_CheckVelocity(ent);

	// add gravity
	if (ent->movetype != MOVETYPE_FLY && ent->movetype != MOVETYPE_FLYMISSILE && ent->movetype != MOVETYPE_WALLBOUNCE)
		G_AddGravity(ent);

	// move angles
	ent->s.angles += (ent->avelocity * gi.frame_time_s);

	// move origin
	int num_tries = 5;
	float time_left = gi.frame_time_s;

	while (time_left) {
		if (num_tries == 0)
			break;

		num_tries--;
		move = ent->velocity * time_left;
		trace = G_PushEntity(ent, move);

		if (!ent->inuse)
			return;

		if (trace.fraction == 1.f)
			break;
		// [Paril-KEX] don't build up velocity if we're stuck.
		// just assume that the object we hit is our ground.
		else if (trace.allsolid) {
			ent->groundentity = trace.ent;
			ent->groundentity_linkcount = trace.ent->linkcount;
			ent->velocity = {};
			ent->avelocity = {};
			break;
		}

		time_left -= time_left * trace.fraction;

		if (ent->movetype == MOVETYPE_TOSS)
			ent->velocity = SlideClipVelocity(ent->velocity, trace.plane.normal, 0.5f);
		else {
			if (ent->movetype == MOVETYPE_WALLBOUNCE)
				backoff = 2.0f;
			else
				backoff = 1.6f;

			ent->velocity = ClipVelocity(ent->velocity, trace.plane.normal, backoff);
		}

		if (ent->movetype == MOVETYPE_WALLBOUNCE)
			ent->s.angles = vectoangles(ent->velocity);

		// stop if on ground
		else {
			if (trace.plane.normal[2] > 0.7f) {
				if ((ent->movetype == MOVETYPE_TOSS && ent->velocity.length() < 60.f) ||
					(ent->movetype != MOVETYPE_TOSS && ent->velocity.scaled(trace.plane.normal).length() < 60.f)) {
					if (!(ent->flags & FL_NO_STANDING) || trace.ent->solid == SOLID_BSP) {
						ent->groundentity = trace.ent;
						ent->groundentity_linkcount = trace.ent->linkcount;
					}
					ent->velocity = {};
					ent->avelocity = {};
					break;
				}

				// friction for tossing stuff (gibs, etc)
				if (ent->movetype == MOVETYPE_TOSS) {
					ent->velocity *= 0.75f;
					ent->avelocity *= 0.75f;
				}
			}
		}

		// only toss "slides" multiple times
		if (ent->movetype != MOVETYPE_TOSS)
			break;
	}

	// check for water transition
	wasinwater = (ent->watertype & MASK_WATER);
	ent->watertype = gi.pointcontents(ent->s.origin);
	isinwater = ent->watertype & MASK_WATER;

	if (isinwater)
		ent->waterlevel = WATER_FEET;
	else
		ent->waterlevel = WATER_NONE;

	if (ent->svflags & SVF_MONSTER) {
		M_CatagorizePosition(ent, ent->s.origin, ent->waterlevel, ent->watertype);
		M_WorldEffects(ent);
	} else {
		if (!wasinwater && isinwater)
			gi.positioned_sound(old_origin, g_entities, CHAN_AUTO, gi.soundindex("misc/h2ohit1.wav"), 1, 1, 0);
		else if (wasinwater && !isinwater)
			gi.positioned_sound(ent->s.origin, g_entities, CHAN_AUTO, gi.soundindex("misc/h2ohit1.wav"), 1, 1, 0);
	}

	// prevent softlocks from keys falling into slime/lava
	if (isinwater && ent->watertype & (CONTENTS_SLIME | CONTENTS_LAVA) && ent->item &&
		(ent->item->flags & IF_KEY) && ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED))
		ent->velocity = { crandom_open() * 300, crandom_open() * 300, 300.f + (crandom_open() * 300.f) };

	// move teamslaves
	for (slave = ent->teamchain; slave; slave = slave->teamchain) {
		slave->s.origin = ent->s.origin;
		gi.linkentity(slave);
	}
}


/*
=============
G_Physics_NewToss

Toss, bounce, and fly movement. When on ground and no velocity, do nothing. With velocity,
slide.
=============
*/
void G_Physics_NewToss(gentity_t *ent) {
	trace_t trace;
	vec3_t	move;
	//	float		backoff;
	gentity_t *slave;
	bool	 wasinwater;
	bool	 isinwater;
	float	 speed, newspeed;
	vec3_t	 old_origin;
	//	float		firstmove;
	//	int			mask;

	// regular thinking
	G_RunThink(ent);

	// if not a team captain, so movement will be handled elsewhere
	if (ent->flags & FL_TEAMSLAVE)
		return;

	wasinwater = ent->waterlevel;

	// find out what we're sitting on.
	move = ent->s.origin;
	move[2] -= 0.25f;
	trace = gi.trace(ent->s.origin, ent->mins, ent->maxs, move, ent, ent->clipmask);
	if (ent->groundentity && ent->groundentity->inuse)
		ent->groundentity = trace.ent;
	else
		ent->groundentity = nullptr;

	// if we're sitting on something flat and have no velocity of our own, return.
	if (ent->groundentity && (trace.plane.normal[2] == 1.0f) &&
		!ent->velocity[0] && !ent->velocity[1] && !ent->velocity[2]) {
		return;
	}

	// store the old origin
	old_origin = ent->s.origin;

	G_CheckVelocity(ent);

	// add gravity
	G_AddGravity(ent);

	if (ent->avelocity[0] || ent->avelocity[1] || ent->avelocity[2])
		G_AddRotationalFriction(ent);

	// add friction
	speed = ent->velocity.length();
	if (ent->waterlevel) // friction for water movement
	{
		newspeed = speed - (g_waterfriction * 6 * (float)ent->waterlevel);
		if (newspeed < 0)
			newspeed = 0;
		newspeed /= speed;
		ent->velocity *= newspeed;
	} else if (!ent->groundentity) // friction for air movement
	{
		newspeed = speed - ((g_friction));
		if (newspeed < 0)
			newspeed = 0;
		newspeed /= speed;
		ent->velocity *= newspeed;
	} else // use ground friction
	{
		newspeed = speed - (g_friction * 6);
		if (newspeed < 0)
			newspeed = 0;
		newspeed /= speed;
		ent->velocity *= newspeed;
	}

	G_FlyMove(ent, gi.frame_time_s, ent->clipmask);
	gi.linkentity(ent);

	G_TouchTriggers(ent);

	// check for water transition
	wasinwater = (ent->watertype & MASK_WATER);
	ent->watertype = gi.pointcontents(ent->s.origin);
	isinwater = ent->watertype & MASK_WATER;

	if (isinwater)
		ent->waterlevel = WATER_FEET;
	else
		ent->waterlevel = WATER_NONE;

	if (!wasinwater && isinwater)
		gi.positioned_sound(old_origin, g_entities, CHAN_AUTO, gi.soundindex("misc/h2ohit1.wav"), 1, 1, 0);
	else if (wasinwater && !isinwater)
		gi.positioned_sound(ent->s.origin, g_entities, CHAN_AUTO, gi.soundindex("misc/h2ohit1.wav"), 1, 1, 0);

	// move teamslaves
	for (slave = ent->teamchain; slave; slave = slave->teamchain) {
		slave->s.origin = ent->s.origin;
		gi.linkentity(slave);
	}
}

/*
===============================================================================

STEPPING MOVEMENT

===============================================================================
*/

/*
=============
G_Physics_Step

Monsters freefall when they don't have a ground entity, otherwise
all movement is done with discrete steps.

This is also used for objects that have become still on the ground, but
will fall if the floor is pulled out from under them.
=============
*/

void G_AddRotationalFriction(gentity_t *ent) {
	int	  n;
	float adjustment;

	ent->s.angles += (ent->avelocity * gi.frame_time_s);
	adjustment = gi.frame_time_s * g_stopspeed->value * g_friction;

	for (n = 0; n < 3; n++) {
		if (ent->avelocity[n] > 0) {
			ent->avelocity[n] -= adjustment;
			if (ent->avelocity[n] < 0)
				ent->avelocity[n] = 0;
		} else {
			ent->avelocity[n] += adjustment;
			if (ent->avelocity[n] > 0)
				ent->avelocity[n] = 0;
		}
	}
}

static void G_Physics_Step(gentity_t *ent) {
	bool	   wasonground;
	bool	   hitsound = false;
	float *vel;
	float	   speed, newspeed, control;
	float	   friction;
	gentity_t *groundentity;
	contents_t mask = G_GetClipMask(ent);

	// airborne monsters should always check for ground
	if (!ent->groundentity)
		M_CheckGround(ent, mask);

	groundentity = ent->groundentity;

	G_CheckVelocity(ent);

	if (groundentity)
		wasonground = true;
	else
		wasonground = false;

	if (ent->avelocity[0] || ent->avelocity[1] || ent->avelocity[2])
		G_AddRotationalFriction(ent);

	// FIXME: figure out how or why this is happening
	if (isnan(ent->velocity[0]) || isnan(ent->velocity[1]) || isnan(ent->velocity[2]))
		ent->velocity = {};

	// add gravity except:
	//   flying monsters
	//   swimming monsters who are in the water
	if (!wasonground)
		if (!(ent->flags & FL_FLY))
			if (!((ent->flags & FL_SWIM) && (ent->waterlevel > WATER_WAIST))) {
				if (ent->velocity[2] < level.gravity * -0.1f)
					hitsound = true;
				if (ent->waterlevel != WATER_UNDER)
					G_AddGravity(ent);
			}

	// friction for flying monsters that have been given vertical velocity
	if ((ent->flags & FL_FLY) && (ent->velocity[2] != 0) && !(ent->monsterinfo.aiflags & AI_ALTERNATE_FLY)) {
		speed = fabsf(ent->velocity[2]);
		control = speed < g_stopspeed->value ? g_stopspeed->value : speed;
		friction = g_friction / 3;
		newspeed = speed - (gi.frame_time_s * control * friction);
		if (newspeed < 0)
			newspeed = 0;
		newspeed /= speed;
		ent->velocity[2] *= newspeed;
	}

	// friction for flying monsters that have been given vertical velocity
	if ((ent->flags & FL_SWIM) && (ent->velocity[2] != 0) && !(ent->monsterinfo.aiflags & AI_ALTERNATE_FLY)) {
		speed = fabsf(ent->velocity[2]);
		control = speed < g_stopspeed->value ? g_stopspeed->value : speed;
		newspeed = speed - (gi.frame_time_s * control * g_waterfriction * (float)ent->waterlevel);
		if (newspeed < 0)
			newspeed = 0;
		newspeed /= speed;
		ent->velocity[2] *= newspeed;
	}

	if (ent->velocity[2] || ent->velocity[1] || ent->velocity[0]) {
		// apply friction
		if ((wasonground || (ent->flags & (FL_SWIM | FL_FLY))) && !(ent->monsterinfo.aiflags & AI_ALTERNATE_FLY)) {
			vel = &ent->velocity.x;
			speed = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]);
			if (speed) {
				friction = g_friction;

				// Paril: lower friction for dead monsters
				if (ent->deadflag)
					friction *= 0.5f;

				control = speed < g_stopspeed->value ? g_stopspeed->value : speed;
				newspeed = speed - gi.frame_time_s * control * friction;

				if (newspeed < 0)
					newspeed = 0;
				newspeed /= speed;

				vel[0] *= newspeed;
				vel[1] *= newspeed;
			}
		}

		vec3_t old_origin = ent->s.origin;

		G_FlyMove(ent, gi.frame_time_s, mask);

		G_TouchProjectiles(ent, old_origin);

		M_CheckGround(ent, mask);

		gi.linkentity(ent);

		// ========
		// PGM - reset this every time they move.
		//       G_touchtriggers will set it back if appropriate
		ent->gravity = 1.0;
		// ========

		// [Paril-KEX] this is something N64 does to avoid doors opening
		// at the start of a level, which triggers some monsters to spawn.
		if (notGT(GT_HORDE)) {
			if (!level.is_n64 || level.time > FRAME_TIME_S)
				G_TouchTriggers(ent);
		}

		if (!ent->inuse)
			return;

		if (ent->groundentity)
			if (!wasonground)
				if (hitsound && !(RS(RS_Q1)))
					ent->s.event = EV_FOOTSTEP;
	}

	if (GT(GT_HORDE))
		G_TouchTriggers(ent);

	if (!ent->inuse) // PGM g_touchtrigger free problem
		return;

	if (ent->svflags & SVF_MONSTER) {
		M_CatagorizePosition(ent, ent->s.origin, ent->waterlevel, ent->watertype);
		M_WorldEffects(ent);

		// [Paril-KEX] last minute hack to fix Stalker upside down gravity
		if (wasonground != !!ent->groundentity) {
			if (ent->monsterinfo.physics_change)
				ent->monsterinfo.physics_change(ent);
		}
	}

	// regular thinking
	G_RunThink(ent);
}

// [Paril-KEX]
static inline void G_RunBmodelAnimation(gentity_t *ent) {
	auto &anim = ent->bmodel_anim;

	if (anim.currently_alternate != anim.alternate) {
		anim.currently_alternate = anim.alternate;
		anim.next_tick = 0_ms;
	}

	if (level.time < anim.next_tick)
		return;

	const auto &speed = anim.alternate ? anim.alt_speed : anim.speed;

	anim.next_tick = level.time + gtime_t::from_ms(speed);

	const auto &style = anim.alternate ? anim.alt_style : anim.style;

	const auto &start = anim.alternate ? anim.alt_start : anim.start;
	const auto &end = anim.alternate ? anim.alt_end : anim.end;

	switch (style) {
	case BMODEL_ANIM_FORWARDS:
		if (end >= start)
			ent->s.frame++;
		else
			ent->s.frame--;
		break;
	case BMODEL_ANIM_BACKWARDS:
		if (end >= start)
			ent->s.frame--;
		else
			ent->s.frame++;
		break;
	case BMODEL_ANIM_RANDOM:
		ent->s.frame = irandom(start, end + 1);
		break;
	}

	const auto &nowrap = anim.alternate ? anim.alt_nowrap : anim.nowrap;

	if (nowrap) {
		if (end >= start)
			ent->s.frame = clamp(ent->s.frame, start, end);
		else
			ent->s.frame = clamp(ent->s.frame, end, start);
	} else {
		if (ent->s.frame < start)
			ent->s.frame = end;
		else if (ent->s.frame > end)
			ent->s.frame = start;
	}
}

//============================================================================

/*
================
G_RunEntity

================
*/
void G_RunEntity(gentity_t *ent) {
	trace_t trace;
	vec3_t	previous_origin;
	bool	has_previous_origin = false;

	if (level.timeout_in_place)
		return;

	if (ent->movetype == MOVETYPE_STEP) {
		previous_origin = ent->s.origin;
		has_previous_origin = true;
	}

	if (ent->prethink)
		ent->prethink(ent);

	// bmodel animation stuff runs first, so custom entities
	// can override them
	if (ent->bmodel_anim.enabled)
		G_RunBmodelAnimation(ent);

	switch ((int)ent->movetype) {
	case MOVETYPE_PUSH:
	case MOVETYPE_STOP:
		G_Physics_Pusher(ent);
		break;
	case MOVETYPE_NONE:
		G_Physics_None(ent);
		break;
	case MOVETYPE_NOCLIP:
	case MOVETYPE_FREECAM:
		G_Physics_Noclip(ent);
		break;
	case MOVETYPE_STEP:
		G_Physics_Step(ent);
		break;
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
	case MOVETYPE_FLY:
	case MOVETYPE_FLYMISSILE:
	case MOVETYPE_WALLBOUNCE:
		G_Physics_Toss(ent);
		break;
	case MOVETYPE_NEWTOSS:
		G_Physics_NewToss(ent);
		break;
	default:
		gi.Com_ErrorFmt("G_Physics: bad movetype {}", (int32_t)ent->movetype);
	}

	if (has_previous_origin && ent->movetype == MOVETYPE_STEP) {
		// if we moved, check and fix origin if needed
		if (ent->s.origin != previous_origin) {
			trace = gi.trace(ent->s.origin, ent->mins, ent->maxs, previous_origin, ent, G_GetClipMask(ent));
			if (trace.allsolid || trace.startsolid)
				ent->s.origin = previous_origin;
		}
	}

	// try to fix buggy lifts this way
	if (has_previous_origin && ent->movetype == MOVETYPE_STOP) {
		if (ent->s.origin == previous_origin) {
			switch (ent->moveinfo.state) {
			case STATE_UP:
				ent->s.origin[2] = (int)ceil(ent->s.origin[2]);
				gi.Com_Print("attempting mover fix\n");
				break;
			case STATE_DOWN:
				ent->s.origin[2] = (int)floor(ent->s.origin[2]);
				gi.Com_Print("attempting mover fix\n");
				break;
			}
			if (ent->s.origin != previous_origin) {
				trace = gi.trace(ent->s.origin, ent->mins, ent->maxs, ent->s.origin, ent, G_GetClipMask(ent));
				if (trace.allsolid || trace.startsolid)
					ent->s.origin = previous_origin;
			}
		}
	}

	if (ent->postthink)
		ent->postthink(ent);
}
