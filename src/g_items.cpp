// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include "g_local.h"
#include "bots/bot_includes.h"
#include "monsters/m_player.h"	//doppelganger
#include "muffmode/mm_items_rules.h"
#include "muffmode/mm_ruleset.h"

bool Pickup_Weapon(gentity_t *ent, gentity_t *other);
void Use_Weapon(gentity_t *ent, gitem_t *inv);
void Drop_Weapon(gentity_t *ent, gitem_t *inv);

void Weapon_Blaster(gentity_t *ent);
void Weapon_Shotgun(gentity_t *ent);
void Weapon_SuperShotgun(gentity_t *ent);
void Weapon_Machinegun(gentity_t *ent);
void Weapon_Chaingun(gentity_t *ent);
void Weapon_HyperBlaster(gentity_t *ent);
void Weapon_RocketLauncher(gentity_t *ent);
void Weapon_HandGrenade(gentity_t *ent);
void Weapon_GrenadeLauncher(gentity_t *ent);
void Weapon_Railgun(gentity_t *ent);
void Weapon_BFG(gentity_t *ent);
void Weapon_IonRipper(gentity_t *ent);
void Weapon_Phalanx(gentity_t *ent);
void Weapon_Trap(gentity_t *ent);
void Weapon_ChainFist(gentity_t *ent);
void Weapon_Disruptor(gentity_t *ent);
void Weapon_ETF_Rifle(gentity_t *ent);
void Weapon_PlasmaBeam(gentity_t *ent);
void Weapon_Tesla(gentity_t *ent);
void Weapon_ProxLauncher(gentity_t *ent);

void	   Use_Quad(gentity_t *ent, gitem_t *item);
static gtime_t quad_drop_timeout_hack;
void	   Use_Haste(gentity_t *ent, gitem_t *item);
static gtime_t haste_drop_timeout_hack;
void	   Use_Double(gentity_t *ent, gitem_t *item);
static gtime_t double_drop_timeout_hack;
void	   Use_Invisibility(gentity_t *ent, gitem_t *item);
static gtime_t invisibility_drop_timeout_hack;
void	   Use_Protection(gentity_t *ent, gitem_t *item);
static gtime_t protection_drop_timeout_hack;
void	   Use_Regeneration(gentity_t *ent, gitem_t *item);
static gtime_t regeneration_drop_timeout_hack;

static void UsedMessage(gentity_t *ent, gitem_t *item) {
	if (!ent || !item)
		return;

	if (item->id == IT_ADRENALINE && !g_dm_holdable_adrenaline->integer)
		return;

	gi.LocClient_Print(ent, PRINT_CENTER, "Used {}", item->pickup_name);
}

// ***************************
//  DOPPELGANGER
// ***************************

gentity_t *Sphere_Spawn(gentity_t *owner, spawnflags_t spawnflags);

static DIE(doppelganger_die) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod) -> void {
	gentity_t *sphere;
	float	 dist;
	vec3_t	 dir;

	if ((self->enemy) && (self->enemy != self->teammaster)) {
		dir = self->enemy->s.origin - self->s.origin;
		dist = dir.length();

		if (dist > 80.f) {
			if (dist > 768) {
				sphere = Sphere_Spawn(self, SF_SPHERE_HUNTER | SF_DOPPELGANGER);
				sphere->pain(sphere, attacker, 0, 0, mod);
			} else {
				sphere = Sphere_Spawn(self, SF_SPHERE_VENGEANCE | SF_DOPPELGANGER);
				sphere->pain(sphere, attacker, 0, 0, mod);
			}
		}
	}

	self->takedamage = DAMAGE_NONE;

	// [Paril-KEX]
	T_RadiusDamage(self, self->teammaster, 160.f, self, 140.f, DAMAGE_NONE, MOD_DOPPEL_EXPLODE);

	if (self->teamchain)
		BecomeExplosion1(self->teamchain);
	BecomeExplosion1(self);
}

static PAIN(doppelganger_pain) (gentity_t *self, gentity_t *other, float kick, int damage, const mod_t &mod) -> void {
	self->enemy = other;
}

static THINK(doppelganger_timeout) (gentity_t *self) -> void {
	doppelganger_die(self, self, self, 9999, self->s.origin, MOD_UNKNOWN);
}

static THINK(body_think) (gentity_t *self) -> void {
	float r;

	if (fabsf(self->ideal_yaw - anglemod(self->s.angles[YAW])) < 2) {
		if (self->timestamp < level.time) {
			r = frandom();
			if (r < 0.10f) {
				self->ideal_yaw = frandom(350.0f);
				self->timestamp = level.time + 1_sec;
			}
		}
	} else
		M_ChangeYaw(self);

	if (self->teleport_time <= level.time) {
		self->s.frame++;
		if (self->s.frame > FRAME_stand40)
			self->s.frame = FRAME_stand01;

		self->teleport_time = level.time + 10_hz;
	}

	self->nextthink = level.time + FRAME_TIME_MS;
}

void fire_doppelganger(gentity_t *ent, const vec3_t &start, const vec3_t &aimdir) {
	gentity_t *base;
	gentity_t *body;
	vec3_t	 dir;
	vec3_t	 forward, right, up;
	int		 number;

	dir = vectoangles(aimdir);
	AngleVectors(dir, forward, right, up);

	base = G_Spawn();
	base->s.origin = start;
	base->s.angles = dir;
	base->movetype = MOVETYPE_TOSS;
	base->solid = SOLID_BBOX;
	base->s.renderfx |= RF_IR_VISIBLE;
	base->s.angles[PITCH] = 0;
	base->mins = { -16, -16, -24 };
	base->maxs = { 16, 16, 32 };
	base->s.modelindex = gi.modelindex("models/objects/dopplebase/tris.md2");
	base->s.alpha = 0.1f;
	base->teammaster = ent;
	base->flags |= (FL_DAMAGEABLE | FL_TRAP);
	base->takedamage = true;
	base->health = 30;
	base->pain = doppelganger_pain;
	base->die = doppelganger_die;

	base->nextthink = level.time + 30_sec;
	base->think = doppelganger_timeout;

	base->classname = "doppelganger";

	gi.linkentity(base);

	body = G_Spawn();
	number = body->s.number;
	body->s = ent->s;
	body->s.sound = 0;
	body->s.event = EV_NONE;
	body->s.number = number;
	body->yaw_speed = 30;
	body->ideal_yaw = 0;
	body->s.origin = start;
	body->s.origin[2] += 8;
	body->teleport_time = level.time + 10_hz;
	body->think = body_think;
	body->nextthink = level.time + FRAME_TIME_MS;
	gi.linkentity(body);

	base->teamchain = body;
	body->teammaster = base;

	// [Paril-KEX]
	body->owner = ent;
	gi.sound(body, CHAN_AUTO, gi.soundindex("medic_commander/monsterspawn1.wav"), 1.f, ATTN_NORM, 0.f);
}

//======================================================================

constexpr gtime_t DEFENDER_LIFESPAN		= 10_sec;	//30_sec;
constexpr gtime_t HUNTER_LIFESPAN		= 10_sec;	//30_sec;
constexpr gtime_t VENGEANCE_LIFESPAN	= 10_sec;	//30_sec;
constexpr gtime_t MINIMUM_FLY_TIME		= 10_sec;	//15_sec;

void LookAtKiller(gentity_t *self, gentity_t *inflictor, gentity_t *attacker);

void vengeance_touch(gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self);
void hunter_touch(gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self);

// *************************
// General Sphere Code
// *************************

// =================
// =================
static THINK(sphere_think_explode) (gentity_t *self) -> void {
	if (self->owner && self->owner->client && !(self->spawnflags & SF_DOPPELGANGER)) {
		self->owner->client->owned_sphere = nullptr;
	}
	BecomeExplosion1(self);
}

// =================
// sphere_explode
// =================
static DIE(sphere_explode) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod) -> void {
	sphere_think_explode(self);
}

// =================
// sphere_if_idle_die - if the sphere is not currently attacking, blow up.
// =================
static DIE(sphere_if_idle_die) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod) -> void {
	if (!self->enemy)
		sphere_think_explode(self);
}

// *************************
// Sphere Movement
// *************************

static void sphere_fly(gentity_t *self) {
	vec3_t dest, dir;

	if (level.time >= gtime_t::from_sec(self->wait)) {
		sphere_think_explode(self);
		return;
	}

	dest = self->owner->s.origin;
	dest[2] = self->owner->absmax[2] + 4;

	if (level.time.seconds() == level.time.seconds<int>()) {
		if (!visible(self, self->owner)) {
			self->s.origin = dest;
			gi.linkentity(self);
			return;
		}
	}

	dir = dest - self->s.origin;
	self->velocity = dir * 5;
}

static void sphere_chase(gentity_t *self, int stupidChase) {
	vec3_t dest;
	vec3_t dir;
	float  dist;

	if (level.time >= gtime_t::from_sec(self->wait) || (self->enemy && self->enemy->health < 1)) {
		sphere_think_explode(self);
		return;
	}

	dest = self->enemy->s.origin;
	if (self->enemy->client)
		dest[2] += self->enemy->viewheight;

	if (visible(self, self->enemy) || stupidChase) {
		// if moving, hunter sphere uses active sound
		if (!stupidChase)
			self->s.sound = gi.soundindex("spheres/h_active.wav");

		dir = dest - self->s.origin;
		dir.normalize();
		self->s.angles = vectoangles(dir);
		self->velocity = dir * 300;	// 500;
		self->monsterinfo.saved_goal = dest;
	} else if (!self->monsterinfo.saved_goal) {
		dir = self->enemy->s.origin - self->s.origin;
		dist = dir.normalize();
		self->s.angles = vectoangles(dir);

		// if lurking, hunter sphere uses lurking sound
		self->s.sound = gi.soundindex("spheres/h_lurk.wav");
		self->velocity = {};
	} else {
		dir = self->monsterinfo.saved_goal - self->s.origin;
		dist = dir.normalize();

		if (dist > 1) {
			self->s.angles = vectoangles(dir);

			if (dist > 500)
				self->velocity = dir * 500;
			else if (dist < 20)
				self->velocity = dir * (dist / gi.frame_time_s);
			else
				self->velocity = dir * dist;

			// if moving, hunter sphere uses active sound
			if (!stupidChase)
				self->s.sound = gi.soundindex("spheres/h_active.wav");
		} else {
			dir = self->enemy->s.origin - self->s.origin;
			dist = dir.normalize();
			self->s.angles = vectoangles(dir);

			// if not moving, hunter sphere uses lurk sound
			if (!stupidChase)
				self->s.sound = gi.soundindex("spheres/h_lurk.wav");

			self->velocity = {};
		}
	}
}

// *************************
// Attack related stuff
// *************************

static void sphere_fire(gentity_t *self, gentity_t *enemy) {
	vec3_t dest;
	vec3_t dir;

	if (!enemy || level.time >= gtime_t::from_sec(self->wait)) {
		sphere_think_explode(self);
		return;
	}

	dest = enemy->s.origin;
	self->s.effects |= EF_ROCKET;

	dir = dest - self->s.origin;
	dir.normalize();
	self->s.angles = vectoangles(dir);
	self->velocity = dir * 1000;

	self->touch = vengeance_touch;
	self->think = sphere_think_explode;
	self->nextthink = gtime_t::from_sec(self->wait);
}

static void sphere_touch(gentity_t *self, gentity_t *other, const trace_t &tr, mod_t mod) {
	if (self->spawnflags.has(SF_DOPPELGANGER)) {
		if (other == self->teammaster)
			return;

		self->takedamage = false;
		self->owner = self->teammaster;
		self->teammaster = nullptr;
	} else {
		if (other == self->owner)
			return;
		// PMM - don't blow up on bodies
		if (!strcmp(other->classname, "bodyque"))
			return;
	}

	if (tr.surface && (tr.surface->flags & SURF_SKY)) {
		G_FreeEntity(self);
		return;
	}

	if (self->owner) {
		if (other->takedamage) {
			T_Damage(other, self, self->owner, self->velocity, self->s.origin, tr.plane.normal,
				10000, 1, DAMAGE_DESTROY_ARMOR, mod);
		} else {
			T_RadiusDamage(self, self->owner, 512, self->owner, 256, DAMAGE_NONE, mod);
		}
	}

	sphere_think_explode(self);
}

TOUCH(vengeance_touch) (gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	if (self->spawnflags.has(SF_DOPPELGANGER))
		sphere_touch(self, other, tr, MOD_DOPPEL_VENGEANCE);
	else
		sphere_touch(self, other, tr, MOD_VENGEANCE_SPHERE);
}

TOUCH(hunter_touch) (gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	gentity_t *owner;
	// don't blow up if you hit the world.... sheesh.
	if (other == world)
		return;

	if (self->owner) {
		// if owner is flying with us, make sure they stop too.
		owner = self->owner;
		if (owner->flags & FL_SAM_RAIMI) {
			owner->velocity = {};
			owner->movetype = MOVETYPE_NONE;
			gi.linkentity(owner);
		}
	}

	if (self->spawnflags.has(SF_DOPPELGANGER))
		sphere_touch(self, other, tr, MOD_DOPPEL_HUNTER);
	else
		sphere_touch(self, other, tr, MOD_HUNTER_SPHERE);
}

static void defender_shoot(gentity_t *self, gentity_t *enemy) {
	vec3_t dir;
	vec3_t start;

	if (!(enemy->inuse) || enemy->health <= 0)
		return;

	if (enemy->client && enemy->client->eliminated)
		return;

	if (enemy == self->owner)
		return;

	dir = enemy->s.origin - self->s.origin;
	dir.normalize();

	if (self->monsterinfo.attack_finished > level.time)
		return;

	if (!visible(self, self->enemy))
		return;

	start = self->s.origin;
	start[2] += 2;
	fire_greenblaster(self->owner, start, dir, 10, 1000, EF_BLASTER, 0);

	self->monsterinfo.attack_finished = level.time + 400_ms;
}

// *************************
// Activation Related Stuff
// *************************

static void body_gib(gentity_t *self) {
	gi.sound(self, CHAN_BODY, gi.soundindex("misc/udeath.wav"), 1, ATTN_NORM, 0);
	ThrowGibs(self, 50, {
		{ 4, "models/objects/gibs/sm_meat/tris.md2" },
		{ "models/objects/gibs/skull/tris.md2" }
		});
}

static PAIN(hunter_pain) (gentity_t *self, gentity_t *other, float kick, int damage, const mod_t &mod) -> void {
	gentity_t *owner;
	float	 dist;
	vec3_t	 dir;

	if (self->enemy)
		return;

	owner = self->owner;

	if (!(self->spawnflags & SF_DOPPELGANGER)) {
		if (owner && (owner->health > 0))
			return;

		if (other == owner)
			return;
	} else {
		// if fired by a doppelganger, set it to 10 second timeout
		self->wait = (level.time + MINIMUM_FLY_TIME).seconds();
	}

	if ((gtime_t::from_sec(self->wait) - level.time) < MINIMUM_FLY_TIME)
		self->wait = (level.time + MINIMUM_FLY_TIME).seconds();
	self->s.effects |= EF_BLASTER | EF_TRACKER;
	self->touch = hunter_touch;
	self->enemy = other;

	// if we're not owned by a player, no sam raimi
	// if we're spawned by a doppelganger, no sam raimi
	if (self->spawnflags.has(SF_DOPPELGANGER) || !(owner && owner->client))
		return;

	// sam raimi cam is disabled if FORCE_RESPAWN is set.
	// sam raimi cam is also disabled if g_huntercam->value is 0.
	if (!g_dm_force_respawn->integer && g_huntercam->integer) {
		dir = other->s.origin - self->s.origin;
		dist = dir.length();

		if (owner && (dist >= 192)) {
			// detach owner from body and send him flying
			owner->movetype = MOVETYPE_FLYMISSILE;

			// gib like we just died, even though we didn't, really.
			body_gib(owner);

			// move the sphere to the owner's current viewpoint.
			// we know it's a valid spot (or will be momentarily)
			self->s.origin = owner->s.origin;
			self->s.origin[2] += owner->viewheight;

			// move the player's origin to the sphere's new origin
			owner->s.origin = self->s.origin;
			owner->s.angles = self->s.angles;
			owner->client->v_angle = self->s.angles;
			owner->mins = { -5, -5, -5 };
			owner->maxs = { 5, 5, 5 };
			owner->client->ps.fov = 140;
			owner->s.modelindex = 0;
			owner->s.modelindex2 = 0;
			owner->viewheight = 8;
			owner->solid = SOLID_NOT;
			owner->flags |= FL_SAM_RAIMI;
			gi.linkentity(owner);

			self->solid = SOLID_BBOX;
			gi.linkentity(self);
		}
	}
}

static PAIN(defender_pain) (gentity_t *self, gentity_t *other, float kick, int damage, const mod_t &mod) -> void {
	if (other == self->owner)
		return;

	self->enemy = other;
}

static PAIN(vengeance_pain) (gentity_t *self, gentity_t *other, float kick, int damage, const mod_t &mod) -> void {
	if (self->enemy)
		return;

	if (!(self->spawnflags & SF_DOPPELGANGER)) {
		if (self->owner && self->owner->health >= 25)
			return;

		if (other == self->owner)
			return;
	} else {
		self->wait = (level.time + MINIMUM_FLY_TIME).seconds();
	}

	if ((gtime_t::from_sec(self->wait) - level.time) < MINIMUM_FLY_TIME)
		self->wait = (level.time + MINIMUM_FLY_TIME).seconds();
	self->s.effects |= EF_ROCKET;
	self->touch = vengeance_touch;
	self->enemy = other;
}

// *************************
// Think Functions
// *************************

static THINK(defender_think) (gentity_t *self) -> void {
	if (!self->owner) {
		G_FreeEntity(self);
		return;
	}

	// if we've exited the level, just remove ourselves.
	if (level.intermission_time) {
		sphere_think_explode(self);
		return;
	}

	if (self->owner->health <= 0 || self->owner->client->eliminated) {
		sphere_think_explode(self);
		return;
	}

	self->s.frame++;
	if (self->s.frame > 19)
		self->s.frame = 0;

	if (self->enemy) {
		if (self->enemy->health > 0)
			defender_shoot(self, self->enemy);
		else
			self->enemy = nullptr;
	}

	sphere_fly(self);

	if (self->inuse)
		self->nextthink = level.time + 10_hz;
}

static THINK(hunter_think) (gentity_t *self) -> void {
	// if we've exited the level, just remove ourselves.
	if (level.intermission_time) {
		sphere_think_explode(self);
		return;
	}

	gentity_t *owner = self->owner;

	if (!owner && !(self->spawnflags & SF_DOPPELGANGER)) {
		G_FreeEntity(self);
		return;
	}

	if (owner)
		self->ideal_yaw = owner->s.angles[YAW];
	else if (self->enemy) // fired by doppelganger
	{
		vec3_t dir = self->enemy->s.origin - self->s.origin;
		self->ideal_yaw = vectoyaw(dir);
	}

	M_ChangeYaw(self);

	if (self->enemy) {
		sphere_chase(self, 0);

		// deal with sam raimi cam
		if (owner && (owner->flags & FL_SAM_RAIMI)) {
			if (self->inuse) {
				owner->movetype = MOVETYPE_FLYMISSILE;
				LookAtKiller(owner, self, self->enemy);
				// owner is flying with us, move him too
				owner->movetype = MOVETYPE_FLYMISSILE;
				owner->viewheight = (int)(self->s.origin[2] - owner->s.origin[2]);
				owner->s.origin = self->s.origin;
				owner->velocity = self->velocity;
				owner->mins = {};
				owner->maxs = {};
				gi.linkentity(owner);
			} else // sphere timed out
			{
				owner->velocity = {};
				owner->movetype = MOVETYPE_NONE;
				gi.linkentity(owner);
			}
		}
	} else
		sphere_fly(self);

	if (self->inuse)
		self->nextthink = level.time + 10_hz;
}

static THINK(vengeance_think) (gentity_t *self) -> void {
	// if we've exited the level, just remove ourselves.
	if (level.intermission_time) {
		sphere_think_explode(self);
		return;
	}

	if (!(self->owner) && !(self->spawnflags & SF_DOPPELGANGER)) {
		G_FreeEntity(self);
		return;
	}

	if (self->enemy)
		sphere_chase(self, 1);
	else
		sphere_fly(self);

	if (self->inuse)
		self->nextthink = level.time + 10_hz;
}

// =================
gentity_t *Sphere_Spawn(gentity_t *owner, spawnflags_t spawnflags) {
	gentity_t *sphere;

	sphere = G_Spawn();
	sphere->s.origin = owner->s.origin;
	sphere->s.origin[2] = owner->absmax[2];
	sphere->s.angles[YAW] = owner->s.angles[YAW];
	sphere->solid = SOLID_BBOX;
	sphere->clipmask = MASK_PROJECTILE;
	sphere->s.renderfx = RF_FULLBRIGHT | RF_IR_VISIBLE;
	sphere->movetype = MOVETYPE_FLYMISSILE;

	if (spawnflags.has(SF_DOPPELGANGER))
		sphere->teammaster = owner->teammaster;
	else
		sphere->owner = owner;

	sphere->classname = "sphere";
	sphere->yaw_speed = 40;
	sphere->monsterinfo.attack_finished = 0_ms;
	sphere->spawnflags = spawnflags; // need this for the HUD to recognize sphere
	sphere->takedamage = true;	// false;
	sphere->health = 20;

	switch ((spawnflags & SF_SPHERE_TYPE).value) {
	case SF_SPHERE_DEFENDER.value:
		sphere->s.modelindex = gi.modelindex("models/items/defender/tris.md2");
		sphere->s.modelindex2 = gi.modelindex("models/items/shell/tris.md2");
		sphere->s.sound = gi.soundindex("spheres/d_idle.wav");
		sphere->pain = defender_pain;
		sphere->wait = (level.time + DEFENDER_LIFESPAN).seconds();
		sphere->die = sphere_explode;
		sphere->think = defender_think;
		break;
	case SF_SPHERE_HUNTER.value:
		sphere->s.modelindex = gi.modelindex("models/items/hunter/tris.md2");
		sphere->s.sound = gi.soundindex("spheres/h_idle.wav");
		sphere->wait = (level.time + HUNTER_LIFESPAN).seconds();
		sphere->pain = hunter_pain;
		sphere->die = sphere_if_idle_die;
		sphere->think = hunter_think;
		break;
	case SF_SPHERE_VENGEANCE.value:
		sphere->s.modelindex = gi.modelindex("models/items/vengnce/tris.md2");
		sphere->s.sound = gi.soundindex("spheres/v_idle.wav");
		sphere->wait = (level.time + VENGEANCE_LIFESPAN).seconds();
		sphere->pain = vengeance_pain;
		sphere->die = sphere_if_idle_die;
		sphere->think = vengeance_think;
		sphere->avelocity = { 30, 30, 0 };
		break;
	default:
		gi.Com_Print("Tried to create an invalid sphere\n");
		G_FreeEntity(sphere);
		return nullptr;
	}

	sphere->nextthink = level.time + 10_hz;

	gi.linkentity(sphere);

	return sphere;
}

// =================
// Own_Sphere - attach the sphere to the client so we can
//		directly access it later
// =================
static void Own_Sphere(gentity_t *self, gentity_t *sphere) {
	if (!sphere)
		return;

	// ownership only for players
	if (self->client) {
		// if they don't have one
		if (!(self->client->owned_sphere)) {
			self->client->owned_sphere = sphere;
		}
		// they already have one, take care of the old one
		else {
			if (self->client->owned_sphere->inuse) {
				G_FreeEntity(self->client->owned_sphere);
				self->client->owned_sphere = sphere;
			} else {
				self->client->owned_sphere = sphere;
			}
		}
	}
}

void Defender_Launch(gentity_t *self) {
	gentity_t *sphere;

	sphere = Sphere_Spawn(self, SF_SPHERE_DEFENDER);
	Own_Sphere(self, sphere);
}

void Hunter_Launch(gentity_t *self) {
	gentity_t *sphere;

	sphere = Sphere_Spawn(self, SF_SPHERE_HUNTER);
	Own_Sphere(self, sphere);
}

void Vengeance_Launch(gentity_t *self) {
	gentity_t *sphere;

	sphere = Sphere_Spawn(self, SF_SPHERE_VENGEANCE);
	Own_Sphere(self, sphere);
}

//======================================================================

static gentity_t *QuadHog_FindSpawn() {
	return SelectDeathmatchSpawnPoint(nullptr, vec3_origin, SPAWN_FAR_HALF, true, true, false, true).spot;
}

static void QuadHod_ClearAll() {
	gentity_t *ent;

	for (ent = g_entities; ent < &g_entities[globals.num_entities]; ent++) {

		if (!ent->inuse)
			continue;

		if (ent->client) {
			ent->client->pu_time_quad = 0_ms;
			ent->client->pers.inventory[IT_POWERUP_QUAD] = 0;
			continue;
		}

		if (!ent->classname)
			continue;

		if (!ent->item)
			continue;

		if (ent->item->id != IT_POWERUP_QUAD)
			continue;

		G_FreeEntity(ent);
	}
}

void QuadHog_Spawn(gitem_t *item, gentity_t *spot, bool reset) {
	gentity_t *ent;
	vec3_t	 forward, right;
	vec3_t	 angles = vec3_origin;

	QuadHod_ClearAll();

	ent = G_Spawn();

	ent->classname = item->classname;
	ent->item = item;
	ent->spawnflags = SPAWNFLAG_ITEM_DROPPED;
	ent->s.effects = item->world_model_flags | EF_COLOR_SHELL;
	ent->s.renderfx = RF_GLOW | RF_NO_LOD | RF_SHELL_BLUE;
	ent->mins = { -15, -15, -15 };
	ent->maxs = { 15, 15, 15 };
	gi.setmodel(ent, item->world_model);
	ent->solid = SOLID_TRIGGER;
	ent->movetype = MOVETYPE_TOSS;
	ent->touch = Touch_Item;
	ent->owner = ent;
	ent->nextthink = level.time + 30_sec;
	ent->think = QuadHog_DoSpawn;

	angles[PITCH] = 0;
	angles[YAW] = (float)irandom(360);
	angles[ROLL] = 0;

	AngleVectors(angles, forward, right, nullptr);
	ent->s.origin = spot->s.origin;
	ent->s.origin[2] += 16;
	ent->velocity = forward * 100;
	ent->velocity[2] = 300;

	gi.LocBroadcast_Print(PRINT_CENTER, "The Quad {}!\n", reset ? "respawned" : "has spawned");
	gi.sound(ent, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex("misc/alarm.wav"), 1, ATTN_NONE, 0);

	gi.linkentity(ent);
}

THINK(QuadHog_DoSpawn) (gentity_t *ent) -> void {
	gentity_t *spot;
	gitem_t *it = GetItemByIndex(IT_POWERUP_QUAD);

	if (!it)
		return;

	if ((spot = QuadHog_FindSpawn()) != nullptr)
		QuadHog_Spawn(it, spot, false);

	if (ent)
		G_FreeEntity(ent);
}

THINK(QuadHog_DoReset) (gentity_t *ent) -> void {
	gentity_t *spot;
	gitem_t *it = GetItemByIndex(IT_POWERUP_QUAD);

	if (!it)
		return;

	if ((spot = QuadHog_FindSpawn()) != nullptr)
		QuadHog_Spawn(it, spot, true);

	if (ent)
		G_FreeEntity(ent);
}

void QuadHog_SetupSpawn(gtime_t delay) {
	gentity_t *ent;

	if (!g_quadhog->integer)
		return;

	ent = G_Spawn();
	ent->nextthink = level.time + delay;
	ent->think = QuadHog_DoSpawn;
}

//======================================================================

/*------------------------------------------------------------------------*/
/* TECH																	  */
/*------------------------------------------------------------------------*/

constexpr gtime_t TECH_TIMEOUT = 60_sec; // seconds before techs spawn again

static bool Tech_PlayerHasATech(gentity_t *ent) {
	if (Tech_Held(ent) != nullptr) {
		if (level.time - ent->client->tech_last_message_time > 5_sec) {
			gi.LocCenter_Print(ent, "$g_already_have_tech");
			ent->client->tech_last_message_time = level.time;
		}
		return true; // has this one
	}
	return false;
}

gitem_t *Tech_Held(gentity_t *ent) {
	for (size_t i = 0; i < q_countof(tech_ids); i++) {
		if (ent->client->pers.inventory[tech_ids[i]])
			return GetItemByIndex(tech_ids[i]);
	}
	return nullptr;
}

static bool Tech_Pickup(gentity_t *ent, gentity_t *other) {
	// client only gets one tech
	if (Tech_PlayerHasATech(other))
		return false;

	other->client->pers.inventory[ent->item->id]++;
	other->client->tech_regen_time = level.time;
	return true;
}

static void Tech_Spawn(gitem_t *item, gentity_t *spot);

static gentity_t *FindTechSpawn() {
	return SelectDeathmatchSpawnPoint(nullptr, vec3_origin, SPAWN_FAR_HALF, true, true, false, true).spot;
}

static THINK(Tech_Think) (gentity_t *tech) -> void {
	gentity_t *spot;

	if ((spot = FindTechSpawn()) != nullptr) {
		Tech_Spawn(tech->item, spot);
		G_FreeEntity(tech);
	} else {
		tech->nextthink = level.time + TECH_TIMEOUT;
		tech->think = Tech_Think;
	}
}

static THINK(Tech_Make_Touchable) (gentity_t *tech) -> void {
	tech->touch = Touch_Item;
	tech->nextthink = level.time + TECH_TIMEOUT;
	tech->think = Tech_Think;
}

static void Tech_Drop(gentity_t *ent, gitem_t *item) {
	gentity_t *tech;

	tech = Drop_Item(ent, item);
	tech->nextthink = level.time + 1_sec;
	tech->think = Tech_Make_Touchable;
	ent->client->pers.inventory[item->id] = 0;
}

void Tech_DeadDrop(gentity_t *ent) {
	gentity_t *dropped;
	int		 i;

	i = 0;
	for (; i < q_countof(tech_ids); i++) {
		if (ent->client->pers.inventory[tech_ids[i]]) {
			dropped = Drop_Item(ent, GetItemByIndex(tech_ids[i]));
			// hack the velocity to make it bounce random
			dropped->velocity[0] = crandom_open() * 300;
			dropped->velocity[1] = crandom_open() * 300;
			dropped->nextthink = level.time + TECH_TIMEOUT;
			dropped->think = Tech_Think;
			dropped->owner = nullptr;
			ent->client->pers.inventory[tech_ids[i]] = 0;
		}
	}
}

static void Tech_Spawn(gitem_t *item, gentity_t *spot) {
	gentity_t	*ent = G_Spawn();
	vec3_t	forward, right;
	vec3_t	angles = { 0, (float)irandom(360), 0 };

	ent->classname = item->classname;
	ent->item = item;
	ent->spawnflags = SPAWNFLAG_ITEM_DROPPED;
	ent->s.effects = item->world_model_flags;
	ent->s.renderfx = RF_GLOW | RF_NO_LOD;
	ent->mins = { -15, -15, -15 };
	ent->maxs = { 15, 15, 15 };
	gi.setmodel(ent, ent->item->world_model);
	ent->solid = SOLID_TRIGGER;
	ent->movetype = MOVETYPE_TOSS;
	ent->touch = Touch_Item;
	ent->owner = ent;

	AngleVectors(angles, forward, right, nullptr);
	ent->s.origin = spot->s.origin;
	ent->s.origin[2] += 16;
	ent->velocity = forward * 100;
	ent->velocity[2] = 300;

	ent->nextthink = level.time + TECH_TIMEOUT;
	ent->think = Tech_Think;

	gi.linkentity(ent);
}

static bool AllowTechs() {
	if (!strcmp(g_allow_techs->string, "auto"))
		return !!(GT(GT_CTF) && !(g_instagib->integer || GT(GT_INSTAGIB)) && !(g_nadefest->integer || GT(GT_NADEFEST)) && notGT(GT_BALL));
	else
		return !!(g_allow_techs->integer && ItemSpawnsEnabled());
}

static THINK(Tech_SpawnAll) (gentity_t *ent) -> void {
	gentity_t *spot;

	if (!AllowTechs())
		return;

	int num = 0;
	if (!strcmp(g_allow_techs->string, "auto"))
		num = 1;
	else
		num = g_allow_techs->integer;

	if (!num)
		return;

	gitem_t *it = nullptr;
	for (size_t i = 0; i < q_countof(tech_ids); i++) {
		it = GetItemByIndex(tech_ids[i]);
		if (!it)
			continue;
		for (size_t j = 0; j < num; j++)
			if ((spot = FindTechSpawn()) != nullptr)
				Tech_Spawn(it, spot);
	}
	if (ent)
		G_FreeEntity(ent);
}

void Tech_SetupSpawn() {
	if (!AllowTechs())
		return;

	gentity_t *ent = G_Spawn();
	ent->nextthink = level.time + 2_sec;
	ent->think = Tech_SpawnAll;
}

void Tech_Reset() {
	gentity_t *ent;
	uint32_t i;

	for (ent = g_entities + 1, i = 1; i < globals.num_entities; i++, ent++) {
		if (ent->inuse)
			if (ent->item && (ent->item->flags & IF_TECH))
				G_FreeEntity(ent);
	}
	Tech_SetupSpawn();
	//Tech_SpawnAll(nullptr);
}

int Tech_ApplyDisruptorShield(gentity_t *ent, int dmg) {
	float volume = 1.0;

	if (ent->client && ent->client->silencer_shots)
		volume = 0.2f;

	if (dmg && ent->client && ent->client->pers.inventory[IT_TECH_DISRUPTOR_SHIELD]) {
		// make noise
		gi.sound(ent, CHAN_AUX, gi.soundindex("ctf/tech1.wav"), volume, ATTN_NORM, 0);
		return dmg / 2;
	}
	return dmg;
}

int Tech_ApplyPowerAmp(gentity_t *ent, int dmg) {
	if (dmg && ent->client && ent->client->pers.inventory[IT_TECH_POWER_AMP]) {
		return dmg * 2;
	}
	return dmg;
}

bool Tech_ApplyPowerAmpSound(gentity_t *ent) {
	float volume = 1.0;

	if (ent->client && ent->client->silencer_shots)
		volume = 0.2f;

	if (ent->client &&
		ent->client->pers.inventory[IT_TECH_POWER_AMP]) {
		if (ent->client->tech_sound_time < level.time) {
			ent->client->tech_sound_time = level.time + 1_sec;
			if (ent->client->pu_time_quad > level.time)
				gi.sound(ent, CHAN_AUX, gi.soundindex("ctf/tech2x.wav"), volume, ATTN_NORM, 0);
			else
				gi.sound(ent, CHAN_AUX, gi.soundindex("ctf/tech2.wav"), volume, ATTN_NORM, 0);
		}
		return true;
	}
	return false;
}

bool Tech_ApplyTimeAccel(gentity_t *ent) {
	if (ent->client &&
		ent->client->pers.inventory[IT_TECH_TIME_ACCEL])
		return true;
	return false;
}

void Tech_ApplyTimeAccelSound(gentity_t *ent) {
	float volume = 1.0;

	if (ent->client && ent->client->silencer_shots)
		volume = 0.2f;

	if (ent->client &&
		ent->client->pers.inventory[IT_TECH_TIME_ACCEL] &&
		ent->client->tech_sound_time < level.time) {
		ent->client->tech_sound_time = level.time + 1_sec;
		gi.sound(ent, CHAN_AUX, gi.soundindex("ctf/tech3.wav"), volume, ATTN_NORM, 0);
	}
}

void Tech_ApplyAutoDoc(gentity_t *ent) {
	bool		noise = false;
	gclient_t	*cl;
	int			index;
	float		volume = 1.0;
	bool		mod = (g_instagib->integer || GT(GT_INSTAGIB)) || (g_nadefest->integer || GT(GT_NADEFEST));
	bool		no_health = mod || GTF(GTF_ARENA) || g_no_health->integer;
	int			max = g_vampiric_damage->integer ? ceil(g_vampiric_health_max->integer/2) : mod ? 100 : 150;

	cl = ent->client;
	if (!cl)
		return;

	if (ent->health <= 0 || ent->client->eliminated)
		return;

	if (cl->silencer_shots)
		volume = 0.2f;

	if (mod && !cl->tech_regen_time) {
		cl->tech_regen_time = level.time;
		return;
	}

	if (!(cl->pers.inventory[IT_TECH_AUTODOC] || mod))
		return;

	if (cl->tech_regen_time < level.time) {
		bool mm = !!(RS(RS_MM));
		gtime_t delay = mm ? 1_sec : 500_ms;

		cl->tech_regen_time = level.time;
		if (!g_vampiric_damage->integer) {
			if (ent->health < max) {
				ent->health += 5;
				if (ent->health > max)
					ent->health = max;
				cl->tech_regen_time += delay;
				noise = true;
			}
		}
		//muff: don't regen armor at the same time as health
		if (!no_health && (!mm || (!noise && mm))) {
			index = ArmorIndex(ent);
			if (index && cl->pers.inventory[index] < max) {
				cl->pers.inventory[index] += g_vampiric_damage->integer ? 10 : 5;
				if (cl->pers.inventory[index] > max)
					cl->pers.inventory[index] = max;
				cl->tech_regen_time += delay;
				noise = true;
			}
		}
	}
	if (noise && cl->tech_sound_time < level.time) {
		cl->tech_sound_time = level.time + 1_sec;
		gi.sound(ent, CHAN_AUX, gi.soundindex("ctf/tech4.wav"), volume, ATTN_NORM, 0);
	}
}

bool Tech_HasRegeneration(gentity_t *ent) {
	if (!ent->client) return false;
	if (ent->client->pers.inventory[IT_TECH_AUTODOC]) return true;
	if (g_instagib->integer || GT(GT_INSTAGIB)) return true;
	if (g_nadefest->integer || GT(GT_NADEFEST)) return true;
	return false;
}

// ===============================================

/*
===============
GetItemByIndex
===============
*/
gitem_t *GetItemByIndex(item_id_t index) {
	if (index <= IT_NULL || index >= IT_TOTAL)
		return nullptr;

	return &itemlist[index];
}

static gitem_t *ammolist[AMMO_MAX];

gitem_t *GetItemByAmmo(ammo_t ammo) {
	return ammolist[ammo];
}

static gitem_t *poweruplist[POWERUP_MAX];

gitem_t *GetItemByPowerup(powerup_t powerup) {
	return poweruplist[powerup];
}

/*
===============
FindItemByClassname

===============
*/
gitem_t *FindItemByClassname(const char *classname) {
	int		 i;
	gitem_t *it;

	it = itemlist;
	for (i = 0; i < IT_TOTAL; i++, it++) {
		if (!it->classname)
			continue;
		if (!Q_strcasecmp(it->classname, classname))
			return it;
	}

	return nullptr;
}

/*
===============
FindItem

===============
*/
gitem_t *FindItem(const char *pickup_name) {
	int		 i;
	gitem_t *it;

	it = itemlist;
	for (i = 0; i < IT_TOTAL; i++, it++) {
		if (!it->use_name)
			continue;
		if (!Q_strcasecmp(it->use_name, pickup_name))
			return it;
	}

	return nullptr;
}

//======================================================================

static inline item_flags_t GetSubstituteItemFlags(item_id_t id) {
	const gitem_t *item = GetItemByIndex(id);

	// we want to stay within the item class
	item_flags_t flags = item->flags & IF_TYPE_MASK;

	if ((flags & (IF_WEAPON | IF_AMMO)) == (IF_WEAPON | IF_AMMO))
		flags = IF_AMMO;

	return flags;
}

static inline item_id_t FindSubstituteItem(gentity_t *ent) {
	// never replace flags
	if (ent->item->id == IT_FLAG_RED || ent->item->id == IT_FLAG_BLUE || ent->item->id == IT_TAG_TOKEN)
		return IT_NULL;

	// never replace meaty goodness
	if (ent->item->id == IT_FOODCUBE)
		return IT_NULL;

	// stimpack/shard randomizes
	if (ent->item->id == IT_HEALTH_SMALL ||
		ent->item->id == IT_ARMOR_SHARD)
		return brandom() ? IT_HEALTH_SMALL : IT_ARMOR_SHARD;

	// health is special case
	if (ent->item->id == IT_HEALTH_MEDIUM ||
		ent->item->id == IT_HEALTH_LARGE) {
		float rnd = frandom();

		if (rnd < 0.6f)
			return IT_HEALTH_MEDIUM;
		else
			return IT_HEALTH_LARGE;
	}

	// mega health is special case
	if (ent->item->id == IT_HEALTH_MEGA ||
		ent->item->id == IT_ADRENALINE) {
		float rnd = frandom();

		if (rnd < 0.6f)
			return IT_HEALTH_MEGA;
		else
			return IT_ADRENALINE;
	}

	// armor is also special case
	else if (ent->item->id == IT_ARMOR_JACKET ||
		ent->item->id == IT_ARMOR_COMBAT ||
		ent->item->id == IT_ARMOR_BODY ||
		ent->item->id == IT_POWER_SCREEN ||
		ent->item->id == IT_POWER_SHIELD) {
		float rnd = frandom();

		if (rnd < 0.4f)
			return IT_ARMOR_JACKET;
		else if (rnd < 0.6f)
			return IT_ARMOR_COMBAT;
		else if (rnd < 0.8f)
			return IT_ARMOR_BODY;
		else if (rnd < 0.9f)
			return IT_POWER_SCREEN;
		else
			return IT_POWER_SHIELD;
	}

	item_flags_t myflags = GetSubstituteItemFlags(ent->item->id);

	std::array<item_id_t, MAX_ITEMS> possible_items;
	size_t possible_item_count = 0;

	// gather matching items
	for (item_id_t i = static_cast<item_id_t>(IT_NULL + 1); i < IT_TOTAL; i = static_cast<item_id_t>(static_cast<int32_t>(i) + 1)) {
		const gitem_t *it = GetItemByIndex(i);
		item_flags_t itflags = it->flags;
		bool add = false, subtract = false;

		MM_GetItemInhibitMode(itflags, add, subtract);

		if (subtract)
			continue;

		if (!add) {
			if (!itflags || (itflags & (IF_NOT_GIVEABLE | IF_TECH | IF_NOT_RANDOM)) || !it->pickup || !it->world_model)
				continue;

			if (g_no_powerups->integer && itflags & (IF_POWERUP | IF_SPHERE))
				continue;

			if (g_no_spheres->integer && itflags & IF_SPHERE)
				continue;

			if (g_no_nukes->integer && i == IT_AMMO_NUKE)
				continue;

			if (g_no_mines->integer &&
				(i == IT_AMMO_PROX || i == IT_AMMO_TESLA || i == IT_AMMO_TRAP || i == IT_WEAPON_PROXLAUNCHER))
				continue;
		}

		itflags = GetSubstituteItemFlags(i);

		if ((itflags & IF_TYPE_MASK) == (myflags & IF_TYPE_MASK))
			possible_items[possible_item_count++] = i;
	}

	MM_ClearItemInhibitFlags();

	if (!possible_item_count)
		return IT_NULL;

	return possible_items[irandom(possible_item_count)];
}

item_id_t DoRandomRespawn(gentity_t *ent) {
	if (!ent->item)
		return IT_NULL; // why

	item_id_t id = FindSubstituteItem(ent);

	if (id == IT_NULL)
		return IT_NULL;

	return id;
}

// originally 'DoRespawn'
THINK(RespawnItem) (gentity_t *ent) -> void {
	if (ent->team) {
		gentity_t	*master, *current;
		int			count, choice;
		
		if (!ent->teammaster)
			gi.Com_ErrorFmt("{}: {}: bad teammaster", __FUNCTION__, *ent);

		master = ent->teammaster;
		current = ent;

		// in ctf, when we are weapons stay, only the master of a team of weapons
		// is spawned
		if (GT(GT_CTF) && g_dm_weapons_stay->integer && master->item && (master->item->flags & IF_WEAPON))
			ent = master;
		else {
			int current_index = 0;

			ent->svflags |= SVF_NOCLIENT;
			ent->solid = SOLID_NOT;
			gi.linkentity(ent);

			for (count = 0, ent = master; ent; ent = ent->chain, count++) {
				// reset spawn timers on all teamed entities
				ent->nextthink = 0_sec;
				if (ent == current)
					current_index = count;
			}
			
			if (RS(RS_MM)) {
				choice = (current_index + 1) % count;
				//gi.Com_PrintFmt("ci={} co={} ch={}\n", current_index, count, choice);
				for (count = 0, ent = master; count < choice; ent = ent->chain, count++)
					;
			} else {
				choice = irandom(count);
				for (count = 0, ent = master; count < choice; ent = ent->chain, count++)
					;
			}
		}
	}

	ent->svflags &= ~(SVF_NOCLIENT | SVF_RESPAWNING);
	ent->solid = SOLID_TRIGGER;
	gi.linkentity(ent);

	// send an effect
	// don't do this at match start
	if (level.time > level.match_time + 100_ms)
		ent->s.event = EV_ITEM_RESPAWN;

	if (g_dm_random_items->integer) {
		item_id_t new_item = DoRandomRespawn(ent);

		// if we've changed entities, then do some sleight of hand.
		// otherwise, the old entity will respawn
		if (new_item) {
			ent->item = GetItemByIndex(new_item);

			ent->classname = ent->item->classname;
			ent->s.effects = ent->item->world_model_flags;
			gi.setmodel(ent, ent->item->world_model);
		}
	}

	if ((RS(RS_MM) || RS(RS_Q3A)) && deathmatch->integer) {
		if (ent->item->flags & IF_POWERUP) {
			//if (RS(RS_MM))
			//	gi.LocBroadcast_Print(PRINT_HIGH, "{} has spawned!\n", ent->item->pickup_name);

			//gi.sound(ent, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex("misc/alarm.wav"), 1, ATTN_NONE, 0);	//poweruprespawn
			QLSound(ent, "items/poweruprespawn", "misc/alarm.wav", true);
		}
	}
}

void SetRespawn(gentity_t *ent, gtime_t delay, bool hide_self) {
	if (!deathmatch->integer)
		return;

	if (ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED))
		return;

	if ((ent->item->flags & IF_AMMO) && ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED_PLAYER))
		return;

	// already respawning
	if (ent->think && ent->nextthink >= level.time)
		return;

	ent->flags |= FL_RESPAWN;

	if (hide_self) {
		ent->svflags |= (SVF_NOCLIENT | SVF_RESPAWNING);
		ent->solid = SOLID_NOT;
		gi.linkentity(ent);
	}

	gtime_t t = 0_sec;
	if (ent->random) {
		t += gtime_t::from_ms((crandom() * ent->random) * 1000);
		if (t < FRAME_TIME_MS)
			t = FRAME_TIME_MS;
	}

	delay *= g_dm_item_respawn_rate->value;

	ent->nextthink = level.time + delay + t;

	// 4x longer delay in horde
	if (GT(GT_HORDE))
		ent->nextthink += delay*3;

	ent->think = RespawnItem;
}

//======================================================================

static void Use_Teleporter(gentity_t *ent, gitem_t *item) {
	gentity_t *fx = G_Spawn();
	fx->classname = "telefx";
	fx->s.event = EV_PLAYER_TELEPORT;
	fx->s.origin = ent->s.origin;
	fx->s.origin[2] += 1.0f;
	fx->s.angles = ent->s.angles;
	fx->nextthink = level.time + 100_ms;
	fx->solid = SOLID_NOT;
	fx->think = G_FreeEntity;
	gi.linkentity(fx);
	TeleportPlayerToRandomSpawnPoint(ent, true);

	ent->client->pers.inventory[item->id]--;
	UsedMessage(ent, item);
}

static bool Pickup_Teleporter(gentity_t *ent, gentity_t *other) {
	if (!deathmatch->integer)
		return false;
	if (other->client->pers.inventory[ent->item->id])
		return false;

	other->client->pers.inventory[ent->item->id]++;

	SetRespawn(ent, 120_sec);
	return true;
}

//======================================================================

static bool IsInstantItemsEnabled() {
	if (deathmatch->integer && g_dm_instant_items->integer)
		return true;
	if (!deathmatch->integer && level.instantitems)
		return true;

	return false;
}

static bool Pickup_AllowPowerupPickup(gentity_t *ent, gentity_t *other) {
	int quantity = other->client->pers.inventory[ent->item->id];
	if ((skill->integer == 0 && quantity >= 4) ||
		(skill->integer == 1 && quantity >= 3) ||
		(skill->integer == 2 && quantity >= 2) ||
		(skill->integer == 3 && quantity >= 1) ||
		(skill->integer > 3))
		return false;

	if (coop->integer && !P_UseCoopInstancedItems() && (ent->item->flags & IF_STAY_COOP) && (quantity > 0))
		return false;

	if (deathmatch->integer) {
		if (g_quadhog->integer && ent->item->id == IT_POWERUP_QUAD)
			return true;

		if (g_dm_powerups_minplayers->integer > 0 && level.num_playing_clients < g_dm_powerups_minplayers->integer) {
			if (level.time - other->client->pu_last_message_time > 5_sec) {
				gi.LocClient_Print(other, PRINT_CENTER, "There must be {}+ players in the match\nto pick this up :(", g_dm_powerups_minplayers->integer);
				other->client->pu_last_message_time = level.time;
			}
			return false;
		}
	}

	return true;
}

static bool Pickup_Powerup(gentity_t *ent, gentity_t *other) {
	if (!Pickup_AllowPowerupPickup(ent, other))
		return false;

	other->client->pers.inventory[ent->item->id]++;
	
	if (g_quadhog->integer && ent->item->id == IT_POWERUP_QUAD) {
		if (ent->item->use)
			ent->item->use(other, ent->item);
		G_FreeEntity(ent);
		return true;
	}
	
	bool is_dropped_from_death = ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED_PLAYER) && !ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED);

	if (IsInstantItemsEnabled() || is_dropped_from_death) {
		bool use = false;
		gtime_t t = MM_PowerupInstantPickupTimeout(ent, is_dropped_from_death);
		switch (ent->item->id) {
		case IT_POWERUP_QUAD:
			quad_drop_timeout_hack = t;
			use = true;
			break;
		case IT_POWERUP_HASTE:
			haste_drop_timeout_hack = t;
			use = true;
			break;
		case IT_POWERUP_PROTECTION:
			protection_drop_timeout_hack = t;
			use = true;
			break;
		case IT_POWERUP_DOUBLE:
			double_drop_timeout_hack = t;
			use = true;
			break;
		case IT_POWERUP_INVISIBILITY:
			invisibility_drop_timeout_hack = t;
			use = true;
			break;
		case IT_POWERUP_REGEN:
			regeneration_drop_timeout_hack = t;
			use = true;
			break;
		}

		if (use && ent->item->use)
			ent->item->use(other, ent->item);
	}

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client) && ec->client->sess.pc.follow_powerup) {
			ec->client->follow_target = other;
			ec->client->follow_update = true;
			UpdateChaseCam(ec);
		}
	}
	/*
	if (g_quadhog->integer && ent->item->id == IT_POWERUP_QUAD) {
		G_FreeEntity(ent);
		return true;
	}
	*/
	int count = MM_PowerupRespawnSeconds(ent);

	if (!is_dropped_from_death)
		SetRespawn(ent, gtime_t::from_sec(count));

	return true;
}

static bool Pickup_AllowTimedItemPickup(gentity_t *ent, gentity_t *other) {
	int quantity = other->client->pers.inventory[ent->item->id];
	if ((skill->integer == 0 && quantity >= 3) ||
		(skill->integer == 1 && quantity >= 2) ||
		(skill->integer >= 2 && quantity >= 1))
		return false;

	if (coop->integer && !P_UseCoopInstancedItems() && (ent->item->flags & IF_STAY_COOP) && (quantity > 0))
		return false;

	return true;
}

static bool Pickup_TimedItem(gentity_t *ent, gentity_t *other) {
	if (!Pickup_AllowTimedItemPickup(ent, other))
		return false;

	other->client->pers.inventory[ent->item->id]++;

	bool is_dropped_from_death = ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED_PLAYER) && !ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED);

	if ((IsInstantItemsEnabled() && !(ent->item->id == IT_ADRENALINE && g_dm_holdable_adrenaline->integer)) || is_dropped_from_death)
		ent->item->use(other, ent->item);
	else {
		bool msg = false;
		if (ent->item->id == IT_ADRENALINE && !other->client->pers.holdable_item_msg_adren) {
			other->client->pers.holdable_item_msg_adren = msg = true;
		} else if (ent->item->id == IT_TELEPORTER && !other->client->pers.holdable_item_msg_tele) {
			other->client->pers.holdable_item_msg_tele = msg = true;
		} else if (ent->item->id == IT_DOPPELGANGER && !other->client->pers.holdable_item_msg_doppel) {
			other->client->pers.holdable_item_msg_doppel = msg = true;
		}
		if (msg)
			gi.LocClient_Print(other, PRINT_CENTER, "$map_this_item_must_be_activated_to_use_it");
	}

	if (!is_dropped_from_death)
		SetRespawn(ent, gtime_t::from_sec(ent->item->quantity));

	return true;
}

//======================================================================

static void Use_Defender(gentity_t *ent, gitem_t *item) {
	if (ent->client && ent->client->owned_sphere) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_only_one_sphere_time");
		return;
	}

	ent->client->pers.inventory[item->id]--;

	Defender_Launch(ent);
}

static void Use_Hunter(gentity_t *ent, gitem_t *item) {
	if (ent->client && ent->client->owned_sphere) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_only_one_sphere_time");
		return;
	}

	ent->client->pers.inventory[item->id]--;

	Hunter_Launch(ent);
}

static void Use_Vengeance(gentity_t *ent, gitem_t *item) {
	if (ent->client && ent->client->owned_sphere) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_only_one_sphere_time");
		return;
	}

	ent->client->pers.inventory[item->id]--;

	Vengeance_Launch(ent);
}

static bool Pickup_Sphere(gentity_t *ent, gentity_t *other) {
	int quantity;

	if (other->client && other->client->owned_sphere) {
		//		gi.LocClient_Print(other, PRINT_HIGH, "$g_only_one_sphere_customer");
		return false;
	}

	quantity = other->client->pers.inventory[ent->item->id];
	if ((skill->integer == 1 && quantity >= 2) || (skill->integer >= 2 && quantity >= 1))
		return false;

	if ((coop->integer) && !P_UseCoopInstancedItems() && (ent->item->flags & IF_STAY_COOP) && (quantity > 0))
		return false;

	other->client->pers.inventory[ent->item->id]++;

	SetRespawn(ent, gtime_t::from_sec(ent->item->quantity));

	if (deathmatch->integer && IsInstantItemsEnabled()) {
		if (ent->item->use)
			ent->item->use(other, ent->item);
		else
			gi.Com_Print("Powerup has no use function!\n");
	}

	return true;
}

//======================================================================

static void Use_IR(gentity_t *ent, gitem_t *item) {
	ent->client->pers.inventory[item->id]--;

	ent->client->ir_time = max(level.time, ent->client->ir_time) + 60_sec;

	gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/ir_start.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

static void Use_Nuke(gentity_t *ent, gitem_t *item) {
	vec3_t forward, right, start;

	ent->client->pers.inventory[item->id]--;

	AngleVectors(ent->client->v_angle, forward, right, nullptr);

	start = ent->s.origin;
	fire_nuke(ent, start, forward, 100);
}

static bool Pickup_Nuke(gentity_t *ent, gentity_t *other) {
	int quantity = other->client->pers.inventory[ent->item->id];

	if (quantity >= 1)
		return false;

	if (coop->integer && !P_UseCoopInstancedItems() && (ent->item->flags & IF_STAY_COOP) && (quantity > 0))
		return false;

	other->client->pers.inventory[ent->item->id]++;

	SetRespawn(ent, gtime_t::from_sec(ent->item->quantity));

	return true;
}

//======================================================================

static void Use_Doppelganger(gentity_t *ent, gitem_t *item) {
	vec3_t forward, right;
	vec3_t createPt, spawnPt;
	vec3_t ang;

	ang = { 0, ent->client->v_angle[YAW], 0 };
	AngleVectors(ang, forward, right, nullptr);

	createPt = ent->s.origin + (forward * 48);

	if (!FindSpawnPoint(createPt, ent->mins, ent->maxs, spawnPt, 32))
		return;

	if (!CheckGroundSpawnPoint(spawnPt, ent->mins, ent->maxs, 64, -1))
		return;

	ent->client->pers.inventory[item->id]--;
	UsedMessage(ent, item);

	SpawnGrow_Spawn(spawnPt, 24.f, 48.f);
	fire_doppelganger(ent, spawnPt, forward);
}

static bool Pickup_Doppelganger(gentity_t *ent, gentity_t *other) {
	int quantity;

	if (!deathmatch->integer)
		return false;

	quantity = other->client->pers.inventory[ent->item->id];
	if (quantity >= 1) // FIXME - apply max to doppelgangers
		return false;

	other->client->pers.inventory[ent->item->id]++;

	SetRespawn(ent, gtime_t::from_sec(ent->item->quantity));

	return true;
}

//======================================================================

static bool Pickup_General(gentity_t *ent, gentity_t *other) {
	if (other->client->pers.inventory[ent->item->id])
		return false;

	other->client->pers.inventory[ent->item->id]++;

	SetRespawn(ent, gtime_t::from_sec(ent->item->quantity));

	return true;
}

static bool Pickup_Ball(gentity_t *ent, gentity_t *other) {
	other->client->pers.inventory[ent->item->id] = 1;

	return true;
}

static void Drop_General(gentity_t *ent, gitem_t *item) {
	if (g_quadhog->integer && item->id == IT_POWERUP_QUAD)
		return;

	gentity_t *dropped = Drop_Item(ent, item);
	dropped->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
	dropped->svflags &= ~SVF_INSTANCED;
	ent->client->pers.inventory[item->id]--;

	if (item->flags & IF_POWERUP) {
		switch (item->id) {
		case IT_POWERUP_QUAD:
			ent->client->pu_time_quad = 0_ms;
			break;
		case IT_POWERUP_HASTE:
			ent->client->pu_time_haste = 0_ms;
			break;
		case IT_POWERUP_PROTECTION:
			ent->client->pu_time_protection = 0_ms;
			break;
		case IT_POWERUP_INVISIBILITY:
			ent->client->pu_time_invisibility = 0_ms;
			break;
		case IT_POWERUP_SILENCER:
			ent->client->silencer_shots = 0;
			break;
		case IT_POWERUP_REBREATHER:
			ent->client->pu_time_rebreather = 0_ms;
			break;
		case IT_POWERUP_ENVIROSUIT:
			ent->client->pu_time_enviro = 0_ms;
			break;
		case IT_POWERUP_DOUBLE:
			ent->client->pu_time_double = 0_ms;
			break;
		}
	}

}

//======================================================================

static void Use_Adrenaline(gentity_t *ent, gitem_t *item) {
	ent->max_health += deathmatch->integer ? ((RS(RS_MM)) ? 5 : 0) : 1;

	if (MM_RulesetHealthArmorCap() && ent->max_health > MM_RULESET_HEALTH_CAP)
		ent->max_health = MM_RULESET_HEALTH_CAP;

	if (ent->health < ent->max_health)
		ent->health = ent->max_health;

	if (MM_RulesetHealthArmorCap() && ent->health > MM_RULESET_HEALTH_CAP)
		ent->health = MM_RULESET_HEALTH_CAP;

	gi.sound(ent, CHAN_ITEM, gi.soundindex("items/m_health.wav"), 1, ATTN_NORM, 0);

	ent->client->pu_regen_time_blip = level.time + 100_ms;

	ent->client->pers.inventory[item->id]--;
	UsedMessage(ent, item);
}

static bool Pickup_LegacyHead(gentity_t *ent, gentity_t *other) {
	other->max_health += 5;
	other->health += 5;
	if (MM_RulesetHealthArmorCap()) {
		if (other->max_health > MM_RULESET_HEALTH_CAP)
			other->max_health = MM_RULESET_HEALTH_CAP;
		if (other->health > MM_RULESET_HEALTH_CAP)
			other->health = MM_RULESET_HEALTH_CAP;
	}

	SetRespawn(ent, gtime_t::from_sec(ent->item->quantity));

	return true;
}

void G_CheckPowerArmor(gentity_t *ent) {
	bool has_enough_cells;

	if (!ent->client->pers.inventory[IT_AMMO_CELLS])
		has_enough_cells = false;
	else if (ent->client->pers.autoshield >= AUTO_SHIELD_AUTO)
		has_enough_cells = (ent->flags & FL_WANTS_POWER_ARMOR) && ent->client->pers.inventory[IT_AMMO_CELLS] > ent->client->pers.autoshield;
	else
		has_enough_cells = true;

	if (ent->flags & FL_POWER_ARMOR) {
		// ran out of cells for power armor / lost power armor
		if (!has_enough_cells || (!ent->client->pers.inventory[IT_POWER_SCREEN] &&
				!ent->client->pers.inventory[IT_POWER_SHIELD])) {
			ent->flags &= ~FL_POWER_ARMOR;
			gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/power2.wav"), 1, ATTN_NORM, 0);
		}
	} else {
		// special case for power armor, for auto-shields
		if (ent->client->pers.autoshield != AUTO_SHIELD_MANUAL &&
			has_enough_cells && (ent->client->pers.inventory[IT_POWER_SCREEN] ||
				ent->client->pers.inventory[IT_POWER_SHIELD])) {
			ent->flags |= FL_POWER_ARMOR;
			gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/power1.wav"), 1, ATTN_NORM, 0);
		}
	}
}

static item_id_t AmmoConvertId(item_id_t original_id) {
	item_id_t new_id = original_id;
	if (new_id == IT_AMMO_SHELLS_LARGE || new_id == IT_AMMO_SHELLS_SMALL)
		new_id = IT_AMMO_SHELLS;
	else if (new_id == IT_AMMO_BULLETS_LARGE || new_id == IT_AMMO_BULLETS_SMALL)
		new_id = IT_AMMO_BULLETS;
	else if (new_id == IT_AMMO_CELLS_LARGE || new_id == IT_AMMO_CELLS_SMALL)
		new_id = IT_AMMO_CELLS;
	else if (new_id == IT_AMMO_ROCKETS_SMALL)
		new_id = IT_AMMO_ROCKETS;
	else if (new_id == IT_AMMO_SLUGS_LARGE || new_id == IT_AMMO_SLUGS_SMALL)
		new_id = IT_AMMO_SLUGS;

	return new_id;
}

static inline bool G_AddAmmoAndCap(gentity_t *other, item_id_t id, int32_t max, int32_t quantity) {
	item_id_t new_id = AmmoConvertId(id);
	
	if (other->client->pers.inventory[new_id] == AMMO_INFINITE)
		return false;

	if (other->client->pers.inventory[new_id] >= max)
		return false;

	if (quantity == AMMO_INFINITE) {
		other->client->pers.inventory[new_id] = AMMO_INFINITE;
	} else {
		other->client->pers.inventory[new_id] += quantity;
		if (other->client->pers.inventory[new_id] > max)
			other->client->pers.inventory[new_id] = max;
	}

	G_CheckPowerArmor(other);
	return true;
}

static inline bool G_AddAmmoAndCapQuantity(gentity_t *other, ammo_t ammo) {
	gitem_t *item = GetItemByAmmo(ammo);
	return G_AddAmmoAndCap(other, item->id, other->client->pers.max_ammo[ammo], item->quantity);
}

static inline void G_AdjustAmmoCap(gentity_t *other, ammo_t ammo, int16_t new_max) {
	other->client->pers.max_ammo[ammo] = max(other->client->pers.max_ammo[ammo], new_max);
}

static bool Pickup_Bandolier(gentity_t *ent, gentity_t *other) {
	G_AdjustAmmoCap(other, AMMO_BULLETS, 250);
	G_AdjustAmmoCap(other, AMMO_SHELLS, 150);
	G_AdjustAmmoCap(other, AMMO_CELLS, 250);
	G_AdjustAmmoCap(other, AMMO_SLUGS, 75);
	G_AdjustAmmoCap(other, AMMO_MAGSLUG, 75);
	G_AdjustAmmoCap(other, AMMO_FLECHETTES, 250);
	G_AdjustAmmoCap(other, AMMO_DISRUPTOR, 21);

	G_AddAmmoAndCapQuantity(other, AMMO_BULLETS);
	G_AddAmmoAndCapQuantity(other, AMMO_SHELLS);
	G_AddAmmoAndCapQuantity(other, AMMO_FLECHETTES);

	SetRespawn(ent, gtime_t::from_sec(ent->item->quantity));

	return true;
}

void G_CheckAutoSwitch(gentity_t *ent, gitem_t *item, bool is_new);
static bool Pickup_Pack(gentity_t *ent, gentity_t *other) {
	G_AdjustAmmoCap(other, AMMO_BULLETS, 300);
	G_AdjustAmmoCap(other, AMMO_SHELLS, 200);
	G_AdjustAmmoCap(other, AMMO_ROCKETS, 100);
	G_AdjustAmmoCap(other, AMMO_GRENADES, 100);
	G_AdjustAmmoCap(other, AMMO_CELLS, 300);
	G_AdjustAmmoCap(other, AMMO_SLUGS, 100);
	G_AdjustAmmoCap(other, AMMO_MAGSLUG, 100);
	G_AdjustAmmoCap(other, AMMO_FLECHETTES, 300);
	G_AdjustAmmoCap(other, AMMO_DISRUPTOR, 30);

	G_AddAmmoAndCapQuantity(other, AMMO_BULLETS);
	G_AddAmmoAndCapQuantity(other, AMMO_SHELLS);
	G_AddAmmoAndCapQuantity(other, AMMO_CELLS);
	G_AddAmmoAndCapQuantity(other, AMMO_GRENADES);
	G_AddAmmoAndCapQuantity(other, AMMO_ROCKETS);
	G_AddAmmoAndCapQuantity(other, AMMO_SLUGS);
	G_AddAmmoAndCapQuantity(other, AMMO_MAGSLUG);
	G_AddAmmoAndCapQuantity(other, AMMO_FLECHETTES);
	G_AddAmmoAndCapQuantity(other, AMMO_DISRUPTOR);
	
	gitem_t *it = GetItemByIndex(IT_AMMO_GRENADES);
	if (it)
		G_CheckAutoSwitch(other, it, !other->client->pers.inventory[IT_AMMO_GRENADES]);

	SetRespawn(ent, gtime_t::from_sec(ent->item->quantity));

	return true;
}

//======================================================================

static void Use_Powerup_BroadcastMsg(gentity_t *ent, gitem_t *item, const char *sound_name, const char *announcer_name) {
	if (deathmatch->integer) {
		if (g_quadhog->integer && item->id == IT_POWERUP_QUAD) {
			gi.LocBroadcast_Print(PRINT_CENTER, "{} is the Quad Hog!\n", ent->client->resp.netname);
		//} else {
		//	gi.LocBroadcast_Print(PRINT_HIGH, "{} got the {}!\n", ent->client->resp.netname, item->pickup_name);
		}
		if (RS(RS_MM) || RS(RS_Q3A) || RS(RS_VANILLA_PLUS)) {
			gi.sound(ent, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex(sound_name), 1, ATTN_NONE, 0);
			AnnouncerSound(world, announcer_name, nullptr, false);
		}
	}
}

void Use_Quad(gentity_t *ent, gitem_t *item) {
	gtime_t timeout;

	ent->client->pers.inventory[item->id]--;

	if (quad_drop_timeout_hack) {
		timeout = quad_drop_timeout_hack;
		quad_drop_timeout_hack = 0_ms;
	} else {
		timeout = 30_sec;
	}

	ent->client->pu_time_quad = max(level.time, ent->client->pu_time_quad) + timeout;

	Use_Powerup_BroadcastMsg(ent, item, "items/damage.wav", "quad_damage");
}
// =====================================================================

void Use_Haste(gentity_t *ent, gitem_t *item) {
	gtime_t timeout;

	ent->client->pers.inventory[item->id]--;

	if (haste_drop_timeout_hack) {
		timeout = haste_drop_timeout_hack;
		haste_drop_timeout_hack = 0_ms;
	} else {
		timeout = 30_sec;
	}

	ent->client->pu_time_haste = max(level.time, ent->client->pu_time_haste) + timeout;

	Use_Powerup_BroadcastMsg(ent, item, "items/quadfire1.wav", "haste");
}

//======================================================================

static void Use_Double(gentity_t *ent, gitem_t *item) {
	gtime_t timeout;

	ent->client->pers.inventory[item->id]--;

	if (double_drop_timeout_hack) {
		timeout = double_drop_timeout_hack;
		double_drop_timeout_hack = 0_ms;
	} else {
		timeout = 30_sec;
	}

	ent->client->pu_time_double = max(level.time, ent->client->pu_time_double) + timeout;

	Use_Powerup_BroadcastMsg(ent, item, "misc/ddamage1.wav", nullptr);
}

//======================================================================

static void Use_Breather(gentity_t *ent, gitem_t *item) {
	ent->client->pers.inventory[item->id]--;
	ent->client->pu_time_rebreather = max(level.time, ent->client->pu_time_rebreather) + (RS(RS_MM) ? 45_sec : 30_sec);
}

//======================================================================

static void Use_Envirosuit(gentity_t *ent, gitem_t *item) {
	ent->client->pers.inventory[item->id]--;
	ent->client->pu_time_enviro = max(level.time, ent->client->pu_time_enviro) + 30_sec;
}

//======================================================================

static void Use_Protection(gentity_t *ent, gitem_t *item) {
	gtime_t timeout;

	ent->client->pers.inventory[item->id]--;

	if (protection_drop_timeout_hack) {
		timeout = protection_drop_timeout_hack;
		protection_drop_timeout_hack = 0_ms;
	} else {
		timeout = 30_sec;
	}

	ent->client->pu_time_protection = max(level.time, ent->client->pu_time_protection) + timeout;

	Use_Powerup_BroadcastMsg(ent, item, "items/protect.wav", "battlesuit");
}

//======================================================================

void Powerup_ApplyRegeneration(gentity_t *ent) {
	bool		noise = false;
	gclient_t	*cl;
	float		volume = 1.0;
	bool		mod = (g_instagib->integer || GT(GT_INSTAGIB)) || (g_nadefest->integer || GT(GT_NADEFEST));
	bool		no_health = mod || GTF(GTF_ARENA) || g_no_health->integer;

	cl = ent->client;
	if (!cl)
		return;

	if (ent->health <= 0 || ent->client->eliminated)
		return;

	if (cl->pu_time_regeneration <= level.time)
		return;

	if (g_vampiric_damage->integer)
		return;

	if (cl->silencer_shots)
		volume = 0.2f;

	if (!cl->tech_regen_time) {
		cl->tech_regen_time = level.time;
		return;
	}

	if (cl->pu_regen_time_regen < level.time) {
		gtime_t delay = 1_sec;
		int		max = mod ? cl->pers.max_health : cl->pers.max_health * 2;
		if (MM_RulesetHealthArmorCap())
			max = min(max, (int)MM_RULESET_HEALTH_CAP);

		cl->pu_regen_time_regen = level.time;
		if (ent->health < max) {
			ent->health += 5;
			if (ent->health > max)
				ent->health = max;
			cl->pu_regen_time_regen += delay;
			gi.sound(ent, CHAN_AUX, gi.soundindex("ctf/tech4.wav"), volume, ATTN_NORM, 0);
			cl->pu_regen_time_blip = level.time + 100_ms;
		}
	}
}

static void Use_Regeneration(gentity_t *ent, gitem_t *item) {
	gtime_t timeout;

	ent->client->pers.inventory[item->id]--;

	if (regeneration_drop_timeout_hack) {
		timeout = regeneration_drop_timeout_hack;
		regeneration_drop_timeout_hack = 0_ms;
	} else {
		timeout = 30_sec;
	}

	ent->client->pu_time_regeneration = max(level.time, ent->client->pu_time_regeneration) + timeout;

	Use_Powerup_BroadcastMsg(ent, item, "items/protect.wav", "regeneration");
}

static void Use_Invisibility(gentity_t *ent, gitem_t *item) {
	gtime_t timeout;

	ent->client->pers.inventory[item->id]--;

	if (invisibility_drop_timeout_hack) {
		timeout = invisibility_drop_timeout_hack;
		invisibility_drop_timeout_hack = 0_ms;
	} else {
		timeout = 30_sec;
	}

	ent->client->pu_time_invisibility = max(level.time, ent->client->pu_time_invisibility) + timeout;

	Use_Powerup_BroadcastMsg(ent, item, "items/protect.wav", "invisibility");
}

//======================================================================

static void Use_Silencer(gentity_t *ent, gitem_t *item) {
	ent->client->pers.inventory[item->id]--;
	ent->client->silencer_shots += 30;
}

//======================================================================

static bool Pickup_Key(gentity_t *ent, gentity_t *other) {
	if (coop->integer) {
		if (ent->item->id == IT_KEY_POWER_CUBE || ent->item->id == IT_KEY_EXPLOSIVE_CHARGES) {
			if (other->client->pers.power_cubes & ((ent->spawnflags & SPAWNFLAG_EDITOR_MASK).value >> 8))
				return false;
			other->client->pers.inventory[ent->item->id]++;
			other->client->pers.power_cubes |= ((ent->spawnflags & SPAWNFLAG_EDITOR_MASK).value >> 8);
		} else {
			if (other->client->pers.inventory[ent->item->id])
				return false;
			other->client->pers.inventory[ent->item->id] = 1;
		}
		return true;
	}
	other->client->pers.inventory[ent->item->id]++;

	SetRespawn(ent, 30_sec);
	return true;
}

//======================================================================

bool Add_Ammo(gentity_t *ent, gitem_t *item, int count) {
	if (!ent->client || item->tag < AMMO_BULLETS || item->tag >= AMMO_MAX)
		return false;

	return G_AddAmmoAndCap(ent, item->id, ent->client->pers.max_ammo[item->tag], count);
}

// we just got weapon `item`, check if we should switch to it
void G_CheckAutoSwitch(gentity_t *ent, gitem_t *item, bool is_new) {
	// already using or switching to
	if (ent->client->pers.weapon == item ||
		ent->client->newweapon == item)
		return;
	// need ammo
	else if (item->ammo) {
		int32_t required_ammo = (item->flags & IF_AMMO) ? 1 : item->quantity;

		if (ent->client->pers.inventory[item->ammo] < required_ammo)
			return;
	}

	// check autoswitch setting
	if (ent->client->pers.autoswitch == auto_switch_t::NEVER)
		return;
	else if ((item->flags & IF_AMMO) && ent->client->pers.autoswitch == auto_switch_t::ALWAYS_NO_AMMO)
		return;
	else if (ent->client->pers.autoswitch == auto_switch_t::SMART) {
		// smartness algorithm: in DM, we will always switch if we have the blaster out
		// otherwise leave our active weapon alone
		if (deathmatch->integer) {
			if (ent->client->pers.weapon && !MM_AllowSmartAutoSwitch(ent, item))
				return;
		}
		// in SP, only switch if it's a new weapon, or we have the blaster out
		else if (!deathmatch->integer && !(ent->client->pers.weapon && ent->client->pers.weapon->id == IT_WEAPON_BLASTER) && !is_new)
			return;
	}

	// switch!
	ent->client->newweapon = item;
}

static bool Pickup_Ammo(gentity_t *ent, gentity_t *other) {
	bool weapon = !!(ent->item->flags & IF_WEAPON);
	int	 count, oldcount;

	if (weapon && InfiniteAmmoOn(ent->item))
		count = AMMO_INFINITE;
	else if (ent->count)
		count = ent->count;
	else {
		count = ent->item->quantity;

		if (ent->item->id == IT_AMMO_SLUGS)
			count = MM_AmmoSlugPickupCount(count);
	}

	oldcount = other->client->pers.inventory[AmmoConvertId(ent->item->id)];

	if (!Add_Ammo(other, ent->item, count))
		return false;

	if (weapon)
		G_CheckAutoSwitch(other, ent->item, !oldcount);

	SetRespawn(ent, 30_sec);
	return true;
}

static void Drop_Ammo(gentity_t *ent, gitem_t *item) {
	// [Paril-KEX]
	if (InfiniteAmmoOn(item))
		return;

	item_id_t index = item->id;

	if (ent->client->pers.inventory[index] <= 0)
		return;

	gentity_t *drop = Drop_Item(ent, item);
	drop->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
	drop->svflags &= ~SVF_INSTANCED;

	drop->count = item->quantity;

	if (item->id == IT_AMMO_SLUGS) {
		if (!(RS(RS_MM)))
			drop->count += 5;
	}

	drop->count = clamp(drop->count, drop->count, ent->client->pers.inventory[index]);

	if (ent->client->pers.inventory[index] - drop->count < 0) {
		G_FreeEntity(drop);
		return;
	}

	ent->client->pers.inventory[index] -= drop->count;
	G_CheckPowerArmor(ent);

	if (item == ent->client->pers.weapon || item == ent->client->newweapon)
		if (ent->client->pers.inventory[index] < 1)
			NoAmmoWeaponChange(ent, true);
}

//======================================================================

static THINK(MegaHealth_think) (gentity_t *self) -> void {
	int32_t health = self->max_health;
	if (health < self->owner->max_health)
		health = self->owner->max_health;

	if (self->owner->health > health && !Tech_HasRegeneration(self->owner)) {

		self->nextthink = level.time + 1_sec;
		self->owner->health -= 1;
		return;
	}

	SetRespawn(self, 20_sec);

	if (self->spawnflags.has(SPAWNFLAG_ITEM_DROPPED))
		G_FreeEntity(self);
}

static bool Pickup_Health(gentity_t *ent, gentity_t *other) {
	int health_flags = (ent->style ? ent->style : ent->item->tag);

	if (!(health_flags & HEALTH_IGNORE_MAX))
		if (other->health >= other->max_health)
			return false;

	int count = MM_HealthPickupAmount(ent, ent->count ? ent->count : ent->item->quantity);
	int max = MM_HealthPickupCap(other);

	if (deathmatch->integer && other->health >= max && count > 25)
		return false;

	other->health += count;

	if (GTF(GTF_CTF) && other->health > max && count > 25)
		other->health = max;

	if (!(health_flags & HEALTH_IGNORE_MAX))
		if (other->health > other->max_health)
			other->health = other->max_health;

	if (RS(RS_Q3A) && (health_flags & HEALTH_IGNORE_MAX)) {
		if (other->health > other->max_health * 2)
			other->health = other->max_health * 2;
	}

	if (MM_RulesetHealthArmorCap() && other->health > MM_RULESET_HEALTH_CAP)
		other->health = MM_RULESET_HEALTH_CAP;

	if (MM_HealthPickupUsesMegaThink(ent, other)) {
		if (!deathmatch->integer) {
			// mega health doesn't need to be special in SP
			// since it never respawns.
			other->client->pers.megahealth_time = 5_sec;
		} else {
			ent->think = MegaHealth_think;
			ent->nextthink = level.time + 5_sec;
			ent->owner = other;
			ent->flags |= FL_RESPAWN;
			ent->svflags |= SVF_NOCLIENT;
			ent->solid = SOLID_NOT;

			//muff: set health value trigger at which to initiate respawn delay
			// capped to max health as minimum
			ent->max_health = ent->owner->health - count;

		}
	} else {
		SetRespawn(ent, MM_HealthPickupRespawnDelay());
	}

	return true;
}

//======================================================================

item_id_t ArmorIndex(gentity_t *ent) {
	if (ent->svflags & SVF_MONSTER)
		return ent->monsterinfo.armor_type;

	if (ent->client)
		return MM_ClientArmorIndex(ent);

	return IT_NULL;
}

static bool Pickup_Armor(gentity_t *ent, gentity_t *other) {
	return MM_PickupArmor(ent, other);
}

//======================================================================

item_id_t PowerArmorType(gentity_t *ent) {
	if (!ent->client)
		return IT_NULL;

	if (!(ent->flags & FL_POWER_ARMOR))
		return IT_NULL;

	if (ent->client->pers.inventory[IT_POWER_SHIELD] > 0)
		return IT_POWER_SHIELD;

	if (ent->client->pers.inventory[IT_POWER_SCREEN] > 0)
		return IT_POWER_SCREEN;

	return IT_NULL;
}

static void Use_PowerArmor(gentity_t *ent, gitem_t *item) {
	if (ent->flags & FL_POWER_ARMOR) {
		ent->flags &= ~(FL_POWER_ARMOR | FL_WANTS_POWER_ARMOR);
		gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/power2.wav"), 1, ATTN_NORM, 0);
	} else {
		if (!ent->client->pers.inventory[IT_AMMO_CELLS]) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_no_cells_power_armor");
			return;
		}

		ent->flags |= FL_POWER_ARMOR;

		if (ent->client->pers.autoshield != AUTO_SHIELD_MANUAL &&
			ent->client->pers.inventory[IT_AMMO_CELLS] > ent->client->pers.autoshield)
			ent->flags |= FL_WANTS_POWER_ARMOR;

		gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/power1.wav"), 1, ATTN_NORM, 0);
	}
}

static bool Pickup_PowerArmor(gentity_t *ent, gentity_t *other) {
	other->client->pers.inventory[ent->item->id]++;

	SetRespawn(ent, gtime_t::from_sec(ent->item->quantity));

	if ((deathmatch->integer && !other->client->pers.inventory[ent->item->id]) || !deathmatch->integer)
		G_CheckPowerArmor(other);

	return true;
}

static void Drop_PowerArmor(gentity_t *ent, gitem_t *item) {
	if ((ent->flags & FL_POWER_ARMOR) && (ent->client->pers.inventory[item->id] == 1))
		Use_PowerArmor(ent, item);
	Drop_General(ent, item);
}

//======================================================================

bool Entity_IsVisibleToPlayer(gentity_t *ent, gentity_t *player) {
	// Q2Eaks make eyecam chase target invisible, but keep other client visible
	if (g_eyecam->integer && player->client->follow_target && ent == player->client->follow_target)
		return false;
	else if (ent->client)
		return true;

	return !ent->item_picked_up_by[player->s.number - 1];
}

/*
===============
Touch_Item
===============
*/
TOUCH(Touch_Item) (gentity_t *ent, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	bool taken;

	if (!other->client)
		return;
	if (other->health < 1)
		return; // dead people can't pickup
	if (!ent->item)
		return;
	if (!ent->item->pickup)
		return; // not a grabbable item?

	gitem_t *it = ent->item;

	// already got this instanced item
	if (coop->integer && P_UseCoopInstancedItems()) {
		if (ent->item_picked_up_by[other->s.number - 1])
			return;
	}

	// can't pickup during match countdown
	if (IsPickupsDisabled())
		return;

	taken = it->pickup(ent, other);

	ValidateSelectedItem(other);

	if (taken) {
		// flash the screen
		other->client->bonus_alpha = 0.25;

		// show icon and name on status bar
		other->client->ps.stats[STAT_PICKUP_ICON] = gi.imageindex(it->icon);
		other->client->ps.stats[STAT_PICKUP_STRING] = CS_ITEMS + it->id;
		other->client->pickup_msg_time = level.time + 3_sec;

		// change selected item if we still have it
		if (it->use && other->client->pers.inventory[it->id]) {
			other->client->ps.stats[STAT_SELECTED_ITEM] = other->client->pers.selected_item = it->id;
			other->client->ps.stats[STAT_SELECTED_ITEM_NAME] = 0; // don't set name on pickup item since it's already there
		}

		if (ent->noise_index)
			gi.sound(other, CHAN_ITEM, ent->noise_index, 1, ATTN_NORM, 0);
		else if (it->pickup_sound) {
			// Broadcast powerup pickup sounds to all players in deathmatch (including TDM/CTF) with MM/Q3A/Q2RE Balanced ruleset
			if (deathmatch->integer && (it->flags & IF_POWERUP) && (RS(RS_MM) || RS(RS_Q3A) || RS(RS_VANILLA_PLUS))) {
				gi.sound(ent, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex(it->pickup_sound), 1, ATTN_NONE, 0);
			} else {
				gi.sound(other, CHAN_ITEM, gi.soundindex(it->pickup_sound), 1, ATTN_NORM, 0);
			}
		}
		int32_t player_number = other->s.number - 1;

		if (coop->integer && P_UseCoopInstancedItems() && !ent->item_picked_up_by[player_number]) {
			ent->item_picked_up_by[player_number] = true;

			// [Paril-KEX] this is to fix a coop quirk where items
			// that send a message on pick up will only print on the
			// player that picked them up, and never anybody else; 
			// when instanced items are enabled we don't need to limit
			// ourselves to this, but it does mean that relays that trigger
			// messages won't work, so we'll have to fix those
			if (ent->message)
				G_PrintActivationMessage(ent, other, false);
		}
		if (deathmatch->integer) {
			switch (it->id) {
			case IT_ARMOR_BODY:
			case IT_POWER_SCREEN:
			case IT_POWER_SHIELD:
			case IT_ADRENALINE: 
			case IT_HEALTH_MEGA:
			case IT_POWERUP_QUAD:
			case IT_POWERUP_DOUBLE:
			case IT_POWERUP_PROTECTION:
			case IT_POWERUP_HASTE:
			case IT_POWERUP_INVISIBILITY:
			case IT_POWERUP_REGEN:
			case IT_FLAG_RED:
			case IT_FLAG_BLUE:
				uint32_t key = GetUnicastKey();

				for (auto ec : active_clients()) {
					if (other == ec)
						continue;
					if (ent == ec)
						continue;
					if (ClientIsPlaying(ec->client) && !OnSameTeam(ent, ec))
						continue;
					if (!ClientIsPlaying(ec->client) && ec->client->follow_target && !OnSameTeam(ent, ec->client->follow_target))
						continue;

					gi.WriteByte(svc_poi);
					gi.WriteShort(POI_PING + (ent->s.number - 1));
					gi.WriteShort(5000);
					gi.WritePosition(other->s.origin);
					gi.WriteShort(gi.imageindex(it->icon));
					gi.WriteByte(215);
					gi.WriteByte(POI_FLAG_NONE);
					gi.unicast(ec, false);
					gi.local_sound(ec, CHAN_AUTO, gi.soundindex("misc/help_marker.wav"), 1.0f, ATTN_NONE, 0.0f, key);

					gi.LocClient_Print(ec, PRINT_TTS, G_Fmt("{}{} got the {}.\n", ec->client->sess.team != TEAM_SPECTATOR ? "[TEAM]: " : "", other->client->resp.netname, it->use_name).data());
				}

				//BroadcastFriendlyMessage(other->client->sess.team, G_Fmt("{} got the {}.\n", other->client->resp.netname, it->use_name).data());
				break;
			}
		}
	}

	if (!(ent->spawnflags & SPAWNFLAG_ITEM_TARGETS_USED)) {
		// [Paril-KEX] see above msg; this also disables the message in DM
		// since there's no need to print pickup messages in DM (this wasn't
		// even a documented feature, relays were traditionally used for this)
		const char *message_backup = nullptr;

		if (deathmatch->integer || (coop->integer && P_UseCoopInstancedItems()))
			std::swap(message_backup, ent->message);

		G_UseTargets(ent, other);

		if (deathmatch->integer || (coop->integer && P_UseCoopInstancedItems()))
			std::swap(message_backup, ent->message);

		ent->spawnflags |= SPAWNFLAG_ITEM_TARGETS_USED;
	}

	if (taken) {
		bool should_remove = false;

		if (coop->integer) {
			// in coop with instanced items, *only* dropped 
			// player items will ever get deleted permanently.
			if (P_UseCoopInstancedItems())
				should_remove = ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED_PLAYER);
			// in coop without instanced items, IF_STAY_COOP items remain
			// if not dropped
			else
				should_remove = ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER) || !(it->flags & IF_STAY_COOP);
		} else
			should_remove = !deathmatch->integer || ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER);

		if (should_remove) {
			if (ent->flags & FL_RESPAWN)
				ent->flags &= ~FL_RESPAWN;
			else
				G_FreeEntity(ent);
		}
	}
}

//======================================================================

static TOUCH(drop_temp_touch) (gentity_t *ent, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	if (other == ent->owner)
		return;

	Touch_Item(ent, other, tr, other_touching_self);
}

static THINK(drop_make_touchable) (gentity_t *ent) -> void {
	ent->touch = Touch_Item;
	if (deathmatch->integer) {
		ent->nextthink = level.time + 29_sec;
		ent->think = G_FreeEntity;
	}
}

gentity_t *Drop_Item(gentity_t *ent, gitem_t *item) {
	gentity_t *dropped;
	vec3_t	 forward, right;
	vec3_t	 offset;

	dropped = G_Spawn();

	dropped->item = item;
	dropped->spawnflags = SPAWNFLAG_ITEM_DROPPED;
	dropped->classname = item->classname;
	dropped->s.effects = item->world_model_flags;
	gi.setmodel(dropped, dropped->item->world_model);
	dropped->s.renderfx = RF_GLOW | RF_NO_LOD | RF_IR_VISIBLE;
	dropped->mins = { -15, -15, -15 };
	dropped->maxs = { 15, 15, 15 };
	dropped->solid = SOLID_TRIGGER;
	dropped->movetype = MOVETYPE_TOSS;
	dropped->touch = drop_temp_touch;
	dropped->owner = ent;

	if (ent->client) {
		trace_t trace;

		AngleVectors(ent->client->v_angle, forward, right, nullptr);
		offset = { 24, 0, -16 };
		dropped->s.origin = G_ProjectSource(ent->s.origin, offset, forward, right);
		trace = gi.trace(ent->s.origin, dropped->mins, dropped->maxs, dropped->s.origin, ent, CONTENTS_SOLID);
		dropped->s.origin = trace.endpos;
	} else {
		AngleVectors(ent->s.angles, forward, right, nullptr);
		dropped->s.origin = (ent->absmin + ent->absmax) / 2;
	}

	G_FixStuckObject(dropped, dropped->s.origin);

	dropped->velocity = forward * 100;
	dropped->velocity[2] = 300;

	dropped->think = drop_make_touchable;
	dropped->nextthink = level.time + 1_sec;

	if (coop->integer && P_UseCoopInstancedItems())
		dropped->svflags |= SVF_INSTANCED;

	gi.linkentity(dropped);
	return dropped;
}

static USE(Use_Item) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	ent->svflags &= ~SVF_NOCLIENT;
	ent->use = nullptr;

	if (ent->spawnflags.has(SPAWNFLAG_ITEM_NO_TOUCH)) {
		ent->solid = SOLID_BBOX;
		ent->touch = nullptr;
	} else {
		ent->solid = SOLID_TRIGGER;
		ent->touch = Touch_Item;
	}

	gi.linkentity(ent);
}

//======================================================================

/*
================
FinishSpawningItem

previously 'droptofloor'
================
*/
static THINK(FinishSpawningItem) (gentity_t *ent) -> void {
	// [Paril-KEX] scale foodcube based on how much we ingested
	if (strcmp(ent->classname, "item_foodcube") == 0) {
		ent->mins = vec3_t{ -8, -8, -8 } * ent->s.scale;
		ent->maxs = vec3_t{ 8, 8, 8 } * ent->s.scale;
	} else {
		ent->mins = { -15, -15, -15 };
		ent->maxs = { 15, 15, 15 };
	}

	gi.setmodel(ent, ent->model ? ent->model : ent->item->world_model);

	ent->solid = SOLID_TRIGGER;
	ent->touch = Touch_Item;

	if (ent->spawnflags.has(SPAWNFLAG_ITEM_SUSPENDED)) {
		ent->movetype = MOVETYPE_NONE;
	} else {
		ent->movetype = MOVETYPE_TOSS;

		vec3_t	dest = ent->s.origin + vec3_t{ 0, 0, -4096 };
		trace_t tr = gi.trace(ent->s.origin, ent->mins, ent->maxs, dest, ent, MASK_SOLID);
		
		if (tr.startsolid) {
			if (G_FixStuckObject(ent, ent->s.origin) == stuck_result_t::NO_GOOD_POSITION) {
				if (strcmp(ent->classname, "item_foodcube") == 0)
					ent->velocity[2] = 0;
				else {
					gi.Com_PrintFmt("{}: {}: startsolid\n", __FUNCTION__, *ent);
					G_FreeEntity(ent);
					return;
				}
			}
		} else
			ent->s.origin = tr.endpos;
	}

	if (ent->team) {
		ent->flags &= ~FL_TEAMSLAVE;
		ent->chain = ent->teamchain;
		ent->teamchain = nullptr;

		ent->svflags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;
	
		if (ent == ent->teammaster) {
			ent->nextthink = level.time + 10_hz;
			//if (!ent->think)
				ent->think = RespawnItem;
		} else
			ent->nextthink = 0_sec;
	}

	if (ent->spawnflags.has(SPAWNFLAG_ITEM_NO_TOUCH)) {
		ent->solid = SOLID_BBOX;
		ent->touch = nullptr;
		if (!ent->spawnflags.has(SPAWNFLAG_ITEM_SUSPENDED))
			ent->s.effects &= ~(EF_ROTATE | EF_BOB);
		else ent->s.effects = (EF_ROTATE | EF_BOB);
		ent->s.renderfx &= ~RF_GLOW;
	}

	if (ent->spawnflags.has(SPAWNFLAG_ITEM_TRIGGER_SPAWN)) {
		ent->svflags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;
		ent->use = Use_Item;
	}

	if (MM_DeferInitialPowerupSpawn(ent))
		return;

	ent->watertype = gi.pointcontents(ent->s.origin);
	gi.linkentity(ent);
}

/*
===============
PrecacheItem

Precaches all data needed for a given item.
This will be called for each item spawned in a level,
and for each item in each client's inventory.
===============
*/
void PrecacheItem(gitem_t *it) {
	const char *s, *start;
	char		data[MAX_QPATH];
	ptrdiff_t	len;
	gitem_t *ammo;

	if (!it)
		return;

	if (it->pickup_sound)
		gi.soundindex(it->pickup_sound);
	if (it->world_model)
		gi.modelindex(it->world_model);
	if (it->view_model)
		gi.modelindex(it->view_model);
	if (it->icon)
		gi.imageindex(it->icon);
	
	// parse everything for its ammo
	if (it->ammo) {
		ammo = GetItemByIndex(it->ammo);
		if (ammo != it)
			PrecacheItem(ammo);
	}

	// parse the space seperated precache string for other items
	s = it->precaches;
	if (!s || !s[0])
		return;

	while (*s) {
		start = s;
		while (*s && *s != ' ')
			s++;

		len = s - start;
		if (len >= MAX_QPATH || len < 5)
			gi.Com_ErrorFmt("PrecacheItem: {} has bad precache string", it->classname);
		memcpy(data, start, len);
		data[len] = 0;
		if (*s)
			s++;

		// determine type based on extension
		if (!strcmp(data + len - 3, "md2"))
			gi.modelindex(data);
		else if (!strcmp(data + len - 3, "sp2"))
			gi.modelindex(data);
		else if (!strcmp(data + len - 3, "wav"))
			gi.soundindex(data);
		if (!strcmp(data + len - 3, "pcx"))
			gi.imageindex(data);
	}
}

/*
============
CheckItemEnabled
============
*/
bool CheckItemEnabled(gitem_t *item) {
	cvar_t *cv;

	if (!deathmatch->integer) {
		if (item->pickup == Pickup_Doppelganger || item->pickup == Pickup_Nuke)
			return false;
		if ((item->use == Use_Vengeance) || (item->use == Use_Hunter))
			return false;
		if ((item->use == Use_Teleporter))
			return false;
		return true;
	}

	cv = gi.cvar(G_Fmt("{}_disable_{}", level.mapname, item->classname).data(), "0", CVAR_NOFLAGS);
	if (cv->integer) return false;

	cv = gi.cvar(G_Fmt("disable_{}", item->classname).data(), "0", CVAR_NOFLAGS);
	if (cv->integer) return false;

	// Don't spawn the flags unless enabled
	if (!(GTF(GTF_CTF)) && (item->id == IT_FLAG_RED || item->id == IT_FLAG_BLUE)) {
		return false;
	}

	if (!ItemSpawnsEnabled()) {
		if (item->flags & (IF_ARMOR | IF_POWER_ARMOR | IF_TIMED | IF_POWERUP | IF_SPHERE | IF_HEALTH | IF_AMMO | IF_WEAPON))
			return false;
	}

	bool add = false, subtract = false;

	MM_GetItemInhibitMode(item->flags, add, subtract);

	if (subtract)
		return false;

	if (GT(GT_BALL) && item->id != IT_BALL)
		return false;

	if (!add) {
		if (g_no_armor->integer && item->flags & (IF_ARMOR | IF_POWER_ARMOR))
			return false;

		if (g_no_powerups->integer && item->flags & IF_POWERUP || ((InCoopStyle() || !deathmatch->integer) && skill->integer > 3))
			return false;

		if (g_no_items->integer) {
			if (item->flags & (IF_TIMED | IF_POWERUP | IF_SPHERE))
				return false;
			if (item->pickup == Pickup_Doppelganger)
				return false;
		}
		if (g_no_health->integer || g_vampiric_damage->integer) {
			if (item->flags & IF_HEALTH)
				return false;
		}
		if (g_no_mines->integer) {
			if (item->id == IT_WEAPON_PROXLAUNCHER || item->id == IT_AMMO_PROX || item->id == IT_AMMO_TESLA || item->id == IT_AMMO_TRAP)
				return false;
		}
		if (g_no_nukes->integer && item->id == IT_AMMO_NUKE)
			return false;
		if (g_no_spheres->integer && item->flags & IF_SPHERE)
			return false;
	}

	if (InfiniteAmmoOn(item)) {
		if (item->flags & IF_AMMO && item->id != IT_AMMO_GRENADES && item->id != IT_AMMO_TRAP && item->id != IT_AMMO_TESLA)
			return false;
		if (item->id == IT_PACK || item->id == IT_BANDOLIER)
			return false;
	}

	return true;
}

/*
============
CheckItemReplacements
============
*/
gitem_t *CheckItemReplacements(gitem_t *item) {
	cvar_t *cv;

	cv = gi.cvar(G_Fmt("{}_replace_{}", level.mapname, item->classname).data(), "", CVAR_NOFLAGS);
	if (*cv->string) {
		gitem_t *out = FindItemByClassname(cv->string);
		return out ? out : item;
	}

	cv = gi.cvar(G_Fmt("replace_{}", item->classname).data(), "", CVAR_NOFLAGS);
	if (*cv->string) {
		gitem_t *out = FindItemByClassname(cv->string);
		return out ? out : item;
	}

	if (InfiniteAmmoOn(item)) {
		// [Paril-KEX] some item swappage 
		// BFG too strong in Infinite Ammo mode
		if (item->id == IT_WEAPON_BFG)
			return GetItemByIndex(IT_WEAPON_DISRUPTOR);

		if (item->id == IT_POWER_SHIELD || item->id == IT_POWER_SCREEN)
			return GetItemByIndex(IT_ARMOR_BODY);
	}

	return item;
}

/*
============
Item_TriggeredSpawn

Create the item marked for spawn creation
============
*/
static USE(Item_TriggeredSpawn) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	self->svflags &= ~SVF_NOCLIENT;
	self->use = nullptr;

	if (self->spawnflags.has(SPAWNFLAG_ITEM_TOSS_SPAWN)) {
		self->movetype = MOVETYPE_TOSS;
		vec3_t forward, right;

		AngleVectors(self->s.angles, forward, right, nullptr);
		self->s.origin = self->s.origin;
		self->s.origin[2] += 16;
		self->velocity = forward * 100;
		self->velocity[2] = 300;
	}

	if (self->item->id != IT_KEY_POWER_CUBE && self->item->id != IT_KEY_EXPLOSIVE_CHARGES) // leave them be on key_power_cube
		self->spawnflags &= SPAWNFLAG_ITEM_NO_TOUCH;

	FinishSpawningItem(self);
}

/*
============
SetTriggeredSpawn

Sets up an item to spawn in later.
============
*/
static void SetTriggeredSpawn(gentity_t *ent) {
	// don't do anything on key_power_cubes.
	if (ent->item->id == IT_KEY_POWER_CUBE || ent->item->id == IT_KEY_EXPLOSIVE_CHARGES)
		return;

	ent->think = nullptr;
	ent->nextthink = 0_ms;
	ent->use = Item_TriggeredSpawn;
	ent->svflags |= SVF_NOCLIENT;
	ent->solid = SOLID_NOT;
}

/*
============
SpawnItem

Sets the clipping size and plants the object on the floor.

Items can't be immediately dropped to floor, because they might
be on an entity that hasn't spawned yet.
============
*/
bool SpawnItem(gentity_t *ent, gitem_t *item) {
	// check for item replacements or disablements
	item = CheckItemReplacements(item);
	if (!CheckItemEnabled(item)) {
		G_FreeEntity(ent);
		return false;
	}

	// [Sam-KEX]
	// Paril: allow all keys to be trigger_spawn'd (N64 uses this
	// a few different times)
	if (item->flags & IF_KEY) {
		if (ent->spawnflags.has(SPAWNFLAG_ITEM_TRIGGER_SPAWN)) {
			ent->svflags |= SVF_NOCLIENT;
			ent->solid = SOLID_NOT;
			ent->use = Use_Item;
		}
		if (ent->spawnflags.has(SPAWNFLAG_ITEM_NO_TOUCH)) {
			ent->solid = SOLID_BBOX;
			ent->touch = nullptr;
			ent->s.effects &= ~(EF_ROTATE | EF_BOB);
			ent->s.renderfx &= ~RF_GLOW;
		}
	} else if (ent->spawnflags.value >= SPAWNFLAG_ITEM_MAX.value) {
		ent->spawnflags = SPAWNFLAG_NONE;
		gi.Com_PrintFmt("{} has invalid spawnflags set\n", *ent);
	}

	// set final classname now
	ent->classname = item->classname;

	PrecacheItem(item);

	if (coop->integer && (item->id == IT_KEY_POWER_CUBE || item->id == IT_KEY_EXPLOSIVE_CHARGES)) {
		ent->spawnflags.value |= (1 << (8 + level.power_cubes));
		level.power_cubes++;
	}

	// mark all items as instanced
	if (coop->integer)
		if (P_UseCoopInstancedItems())
			ent->svflags |= SVF_INSTANCED;

	ent->item = item;

	ent->nextthink = level.time + 20_hz; // items start after other solids
	ent->think = FinishSpawningItem;

	ent->s.effects = item->world_model_flags;
	ent->s.renderfx = RF_GLOW | RF_NO_LOD;
	if (ent->model)
		gi.modelindex(ent->model);

	if (ent->spawnflags.has(SPAWNFLAG_ITEM_SUSPENDED))
		ent->s.effects |= (EF_ROTATE | EF_BOB);
	
	if (ent->spawnflags.has(SPAWNFLAG_ITEM_TRIGGER_SPAWN))
		SetTriggeredSpawn(ent);

	// flags are server animated and have special handling
	if (item->id == IT_FLAG_RED || item->id == IT_FLAG_BLUE)
		ent->think = CTF_FlagSetup;

	if (item->flags & IF_WEAPON && item->id >= FIRST_WEAPON && item->id <= LAST_WEAPON)
		level.weapon_count[item->id - FIRST_WEAPON]++;

	if (item->flags & IF_POWERUP && g_dm_powerups_minplayers->integer > 0) {
		if (level.num_playing_clients < g_dm_powerups_minplayers->integer) {
			ent->s.renderfx |= (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE);
			ent->s.effects |= EF_COLOR_SHELL;
		}
	}
	
	if (!g_item_bobbing->integer && !ent->spawnflags.has(SPAWNFLAG_ITEM_SUSPENDED))
		ent->s.effects &= ~EF_BOB;

	if (item->id == IT_FOODCUBE) {
		// Paril: set pickup noise for foodcube based on amount
		if (ent->count < 10)
			ent->noise_index = gi.soundindex("items/s_health.wav");
		else if (ent->count < 25)
			ent->noise_index = gi.soundindex("items/n_health.wav");
		else if (ent->count < 50)
			ent->noise_index = gi.soundindex("items/l_health.wav");
		else
			ent->noise_index = gi.soundindex("items/m_health.wav");
	}

	return true;
}

void P_ToggleFlashlight(gentity_t *ent, bool state) {
	if (!!(ent->flags & FL_FLASHLIGHT) == state)
		return;

	ent->flags ^= FL_FLASHLIGHT;

	gi.sound(ent, CHAN_AUTO, gi.soundindex(ent->flags & FL_FLASHLIGHT ? "items/flashlight_on.wav" : "items/flashlight_off.wav"), 1.f, ATTN_STATIC, 0);
}

static void Use_Flashlight(gentity_t *ent, gitem_t *inv) {
	P_ToggleFlashlight(ent, !(ent->flags & FL_FLASHLIGHT));
}

constexpr size_t MAX_TEMP_POI_POINTS = 128;

void Compass_Update(gentity_t *ent, bool first) {
	vec3_t *&points = level.poi_points[ent->s.number - 1];

	// deleted for some reason
	if (!points)
		return;

	if (!ent->client->help_draw_points)
		return;
	if (ent->client->help_draw_time >= level.time)
		return;

	// don't draw too many points
	float distance = (points[ent->client->help_draw_index] - ent->s.origin).length();
	if (distance > 4096 ||
		!gi.inPHS(ent->s.origin, points[ent->client->help_draw_index], false)) {
		ent->client->help_draw_points = false;
		return;
	}

	gi.WriteByte(svc_help_path);
	gi.WriteByte(first ? 1 : 0);
	gi.WritePosition(points[ent->client->help_draw_index]);

	if (ent->client->help_draw_index == ent->client->help_draw_count - 1)
		gi.WriteDir((ent->client->help_poi_location - points[ent->client->help_draw_index]).normalized());
	else
		gi.WriteDir((points[ent->client->help_draw_index + 1] - points[ent->client->help_draw_index]).normalized());
	gi.unicast(ent, false);

	P_SendLevelPOI(ent);

	gi.local_sound(ent, points[ent->client->help_draw_index], world, CHAN_AUTO, gi.soundindex("misc/help_marker.wav"), 1.0f, ATTN_NORM, 0.0f, GetUnicastKey());

	// done
	if (ent->client->help_draw_index == ent->client->help_draw_count - 1) {
		ent->client->help_draw_points = false;
		return;
	}

	ent->client->help_draw_index++;
	ent->client->help_draw_time = level.time + 200_ms;
}

static void Use_Compass(gentity_t *ent, gitem_t *inv) {
	if (deathmatch->integer) {
		Cmd_ReadyUp_f(ent);
		return;
	}
	if (!level.valid_poi) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$no_valid_poi");
		return;
	}

	if (level.current_dynamic_poi)
		level.current_dynamic_poi->use(level.current_dynamic_poi, ent, ent);

	ent->client->help_poi_location = level.current_poi;
	ent->client->help_poi_image = level.current_poi_image;

	vec3_t *&points = level.poi_points[ent->s.number - 1];

	if (!points)
		points = (vec3_t *)gi.TagMalloc(sizeof(vec3_t) * (MAX_TEMP_POI_POINTS + 1), TAG_LEVEL);

	PathRequest request;
	request.start = ent->s.origin;
	request.goal = level.current_poi;
	request.moveDist = 64.f;
	request.pathFlags = PathFlags::All;
	request.nodeSearch.ignoreNodeFlags = true;
	request.nodeSearch.minHeight = 128.0f;
	request.nodeSearch.maxHeight = 128.0f;
	request.nodeSearch.radius = 1024.0f;
	request.pathPoints.array = points + 1;
	request.pathPoints.count = MAX_TEMP_POI_POINTS;

	PathInfo info;

	if (gi.GetPathToGoal(request, info)) {
		// TODO: optimize points?
		ent->client->help_draw_points = true;
		ent->client->help_draw_count = min((size_t)info.numPathPoints, MAX_TEMP_POI_POINTS);
		ent->client->help_draw_index = 1;

		// remove points too close to the player so they don't have to backtrack
		for (int i = 1; i < 1 + ent->client->help_draw_count; i++) {
			float distance = (points[i] - ent->s.origin).length();
			if (distance > 192) {
				break;
			}

			ent->client->help_draw_index = i;
		}

		// create an extra point in front of us if we're facing away from the first real point
		float d = ((*(points + ent->client->help_draw_index)) - ent->s.origin).normalized().dot(ent->client->v_forward);

		if (d < 0.3f) {
			vec3_t p = ent->s.origin + (ent->client->v_forward * 64.f);

			trace_t tr = gi.traceline(ent->s.origin + vec3_t{ 0.f, 0.f, (float)ent->viewheight }, p, nullptr, MASK_SOLID);

			ent->client->help_draw_index--;
			ent->client->help_draw_count++;

			if (tr.fraction < 1.0f)
				tr.endpos += tr.plane.normal * 8.f;

			*(points + ent->client->help_draw_index) = tr.endpos;
		}

		ent->client->help_draw_time = 0_ms;
		Compass_Update(ent, true);
	} else {
		P_SendLevelPOI(ent);
		gi.local_sound(ent, CHAN_AUTO, gi.soundindex("misc/help_marker.wav"), 1.f, ATTN_NORM, 0, GetUnicastKey());
	}
}

static void Use_Ball(gentity_t *ent, gitem_t *item) {

}

static void Drop_Ball(gentity_t *ent, gitem_t *item) {

}

//======================================================================

// clang-format off
gitem_t	itemlist[] =
{
	{ },	// leave index 0 alone

	//
	// ARMOR
	//

/*QUAKED item_armor_body (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/armor/body/tris.md2"
*/
	{
		/* id */ IT_ARMOR_BODY,
		/* classname */ "item_armor_body",
		/* pickup */ Pickup_Armor,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/ar3_pkup.wav",
		/* world_model */ "models/items/armor/body/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_bodyarmor",
		/* use_name */   "Body Armor",
		/* pickup_name */   "$item_body_armor",
		/* pickup_name_definite */ "$item_body_armor_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR,
		/* vwep_model */ nullptr,
		/* armor_info */ &bodyarmor_info
	},

/*QUAKED item_armor_combat (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_ARMOR_COMBAT,
		/* classname */ "item_armor_combat",
		/* pickup */ Pickup_Armor,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/ar1_pkup.wav",
		/* world_model */ "models/items/armor/combat/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_combatarmor",
		/* use_name */  "Combat Armor",
		/* pickup_name */  "$item_combat_armor",
		/* pickup_name_definite */ "$item_combat_armor_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR,
		/* vwep_model */ nullptr,
		/* armor_info */ &combatarmor_info
	},

/*QUAKED item_armor_jacket (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_ARMOR_JACKET,
		/* classname */ "item_armor_jacket",
		/* pickup */ Pickup_Armor,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/ar1_pkup.wav",
		/* world_model */ "models/items/armor/jacket/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_jacketarmor",
		/* use_name */  "Jacket Armor",
		/* pickup_name */  "$item_jacket_armor",
		/* pickup_name_definite */ "$item_jacket_armor_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR,
		/* vwep_model */ nullptr,
		/* armor_info */ &jacketarmor_info
	},

/*QUAKED item_armor_shard (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_ARMOR_SHARD,
		/* classname */ "item_armor_shard",
		/* pickup */ Pickup_Armor,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/ar2_pkup.wav",
		/* world_model */ "models/items/armor/shard/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_armor_shard",
		/* use_name */  "Armor Shard",
		/* pickup_name */  "$item_armor_shard",
		/* pickup_name_definite */ "$item_armor_shard_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR
	},

/*QUAKED item_power_screen (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_POWER_SCREEN,
		/* classname */ "item_power_screen",
		/* pickup */ Pickup_PowerArmor,
		/* use */ Use_PowerArmor,
		/* drop */ Drop_PowerArmor,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/ar3_pkup.wav",
		/* world_model */ "models/items/armor/screen/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_powerscreen",
		/* use_name */  "Power Screen",
		/* pickup_name */  "$item_power_screen",
		/* pickup_name_definite */ "$item_power_screen_def",
		/* quantity */ 60,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_SCREEN,
		/* precaches */ "misc/power2.wav misc/power1.wav"
	},

/*QUAKED item_power_shield (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_POWER_SHIELD,
		/* classname */ "item_power_shield",
		/* pickup */ Pickup_PowerArmor,
		/* use */ Use_PowerArmor,
		/* drop */ Drop_PowerArmor,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/ar3_pkup.wav",
		/* world_model */ "models/items/armor/shield/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_powershield",
		/* use_name */  "Power Shield",
		/* pickup_name */  "$item_power_shield",
		/* pickup_name_definite */ "$item_power_shield_def",
		/* quantity */ 60,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_NULL,
		/* flags */ IF_ARMOR | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_SHIELD,
		/* precaches */ "misc/power2.wav misc/power1.wav"
	},

	//
	// WEAPONS 
	//

/* weapon_grapple (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_WEAPON_GRAPPLE,
		/* classname */ "weapon_grapple",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Grapple,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_flareg/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/grapple/tris.md2",
		/* icon */ "w_grapple",
		/* use_name */  "Grapple",
		/* pickup_name */  "$item_grapple",
		/* pickup_name_definite */ "$item_grapple_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_WEAPON_BLASTER,
		/* flags */ IF_WEAPON | IF_NO_HASTE | IF_POWERUP_WHEEL | IF_NOT_RANDOM,
		/* vwep_model */ "#w_grapple.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/grapple/grfire.wav weapons/grapple/grpull.wav weapons/grapple/grhang.wav weapons/grapple/grreset.wav weapons/grapple/grhit.wav weapons/grapple/grfly.wav"
	},

/* weapon_blaster (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_WEAPON_BLASTER,
		/* classname */ "weapon_blaster",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Blaster,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_blast/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_blast/tris.md2",
		/* icon */ "w_blaster",
		/* use_name */  "Blaster",
		/* pickup_name */  "$item_blaster",
		/* pickup_name_definite */ "$item_blaster_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_WEAPON_BLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP | IF_NOT_RANDOM,
		/* vwep_model */ "#w_blaster.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/blastf1a.wav misc/lasfly.wav"
	},

/*QUAKED weapon_chainfist (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_chainf/tris.md2"
*/
	{
		/* id */ IT_WEAPON_CHAINFIST,
		/* classname */ "weapon_chainfist",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_ChainFist,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_chainf/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_chainf/tris.md2",
		/* icon */ "w_chainfist",
		/* use_name */  "Chainfist",
		/* pickup_name */  "$item_chainfist",
		/* pickup_name_definite */ "$item_chainfist_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_WEAPON_BLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP | IF_NO_HASTE,
		/* vwep_model */ "#w_chainfist.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/sawidle.wav weapons/sawhit.wav weapons/sawslice.wav",
	},

/*QUAKED weapon_shotgun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_shotg/tris.md2"
*/
	{
		/* id */ IT_WEAPON_SHOTGUN,
		/* classname */ "weapon_shotgun",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Shotgun,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_shotg/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_shotg/tris.md2",
		/* icon */ "w_shotgun",
		/* use_name */  "Shotgun",
		/* pickup_name */  "$item_shotgun",
		/* pickup_name_definite */ "$item_shotgun_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_SHELLS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_shotgun.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_SHELLS,
		/* precaches */ "weapons/shotgf1b.wav weapons/shotgr1b.wav"
	},

/*QUAKED weapon_supershotgun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_shotg2/tris.md2"
*/
	{
		/* id */ IT_WEAPON_SSHOTGUN,
		/* classname */ "weapon_supershotgun",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_SuperShotgun,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_shotg2/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_shotg2/tris.md2",
		/* icon */ "w_sshotgun",
		/* use_name */  "Super Shotgun",
		/* pickup_name */  "$item_super_shotgun",
		/* pickup_name_definite */ "$item_super_shotgun_def",
		/* quantity */ 2,
		/* ammo */ IT_AMMO_SHELLS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_sshotgun.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_SHELLS,
		/* precaches */ "weapons/sshotf1b.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 10
	},

/*QUAKED weapon_machinegun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_machn/tris.md2"
*/
	{
		/* id */ IT_WEAPON_MACHINEGUN,
		/* classname */ "weapon_machinegun",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Machinegun,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_machn/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_machn/tris.md2",
		/* icon */ "w_machinegun",
		/* use_name */  "Machinegun",
		/* pickup_name */  "$item_machinegun",
		/* pickup_name_definite */ "$item_machinegun_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_BULLETS,
		/* chain */ IT_WEAPON_MACHINEGUN,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_machinegun.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_BULLETS,
		/* precaches */ "weapons/machgf1b.wav weapons/machgf2b.wav weapons/machgf3b.wav weapons/machgf4b.wav weapons/machgf5b.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 30
	},

/*QUAKED weapon_etf_rifle (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_etf_rifle/tris.md2"
*/
	{
		/* id */ IT_WEAPON_ETF_RIFLE,
		/* classname */ "weapon_etf_rifle",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_ETF_Rifle,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_etf_rifle/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_etf_rifle/tris.md2",
		/* icon */ "w_etf_rifle",
		/* use_name */  "ETF Rifle",
		/* pickup_name */  "$item_etf_rifle",
		/* pickup_name_definite */ "$item_etf_rifle_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_FLECHETTES,
		/* chain */ IT_WEAPON_MACHINEGUN,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_etfrifle.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_FLECHETTES,
		/* precaches */ "weapons/nail1.wav models/proj/flechette/tris.md2",
		/* sort_id */ 0,
		/* quantity_warn */ 30
	},

/*QUAKED weapon_chaingun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_chain/tris.md2"
*/
	{
		/* id */ IT_WEAPON_CHAINGUN,
		/* classname */ "weapon_chaingun",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Chaingun,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_chain/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_chain/tris.md2",
		/* icon */ "w_chaingun",
		/* use_name */  "Chaingun",
		/* pickup_name */  "$item_chaingun",
		/* pickup_name_definite */ "$item_chaingun_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_BULLETS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_chaingun.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_BULLETS,
		/* precaches */ "weapons/chngnu1a.wav weapons/chngnl1a.wav weapons/machgf3b.wav weapons/chngnd1a.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 60
	},

/*QUAKED ammo_grenades (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
*/
	{
		/* id */ IT_AMMO_GRENADES,
		/* classname */ "ammo_grenades",
		/* pickup */ Pickup_Ammo,
		/* use */ Use_Weapon,
		/* drop */ Drop_Ammo,
		/* weaponthink */ Weapon_HandGrenade,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/items/ammo/grenades/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ "models/weapons/v_handgr/tris.md2",
		/* icon */ "a_grenades",
		/* use_name */  "Grenades",
		/* pickup_name */  "$item_grenades",
		/* pickup_name_definite */ "$item_grenades_def",
		/* quantity */ 5,
		/* ammo */ IT_AMMO_GRENADES,
		/* chain */ IT_AMMO_GRENADES,
		/* flags */ IF_AMMO | IF_WEAPON,
		/* vwep_model */ "#a_grenades.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_GRENADES,
		/* precaches */ "weapons/hgrent1a.wav weapons/hgrena1b.wav weapons/hgrenc1b.wav weapons/hgrenb1a.wav weapons/hgrenb2a.wav models/objects/grenade3/tris.md2",
		/* sort_id */ 0,
		/* quantity_warn */ 2
	},

/*QUAKED ammo_trap (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/weapons/g_trap/tris.md2"
*/
	{
		/* id */ IT_AMMO_TRAP,
		/* classname */ "ammo_trap",
		/* pickup */ Pickup_Ammo,
		/* use */ Use_Weapon,
		/* drop */ Drop_Ammo,
		/* weaponthink */ Weapon_Trap,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/weapons/g_trap/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_trap/tris.md2",
		/* icon */ "a_trap",
		/* use_name */  "Trap",
		/* pickup_name */  "$item_trap",
		/* pickup_name_definite */ "$item_trap_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_TRAP,
		/* chain */ IT_AMMO_GRENADES,
		/* flags */ IF_AMMO | IF_WEAPON | IF_NO_INFINITE_AMMO,
		/* vwep_model */ "#a_trap.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_TRAP,
		/* precaches */ "misc/fhit3.wav weapons/trapcock.wav weapons/traploop.wav weapons/trapsuck.wav weapons/trapdown.wav items/s_health.wav items/n_health.wav items/l_health.wav items/m_health.wav models/weapons/z_trap/tris.md2",
		/* sort_id */ 0,
		/* quantity_warn */ 1
	},

/*QUAKED ammo_tesla (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/ammo/am_tesl/tris.md2"
*/
	{
		/* id */ IT_AMMO_TESLA,
		/* classname */ "ammo_tesla",
		/* pickup */ Pickup_Ammo,
		/* use */ Use_Weapon,
		/* drop */ Drop_Ammo,
		/* weaponthink */ Weapon_Tesla,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/ammo/am_tesl/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ "models/weapons/v_tesla/tris.md2",
		/* icon */ "a_tesla",
		/* use_name */  "Tesla",
		/* pickup_name */  "$item_tesla",
		/* pickup_name_definite */ "$item_tesla_def",
		/* quantity */ 3,
		/* ammo */ IT_AMMO_TESLA,
		/* chain */ IT_AMMO_GRENADES,
		/* flags */ IF_AMMO | IF_WEAPON | IF_NO_INFINITE_AMMO,
		/* vwep_model */ "#a_tesla.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_TESLA,
		/* precaches */ "weapons/teslaopen.wav weapons/hgrenb1a.wav weapons/hgrenb2a.wav models/weapons/g_tesla/tris.md2",
		/* sort_id */ 0,
		/* quantity_warn */ 1
	},

/*QUAKED weapon_grenadelauncher (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_launch/tris.md2"
*/
	{
		/* id */ IT_WEAPON_GLAUNCHER,
		/* classname */ "weapon_grenadelauncher",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_GrenadeLauncher,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_launch/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_launch/tris.md2",
		/* icon */ "w_glauncher",
		/* use_name */  "Grenade Launcher",
		/* pickup_name */  "$item_grenade_launcher",
		/* pickup_name_definite */ "$item_grenade_launcher_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_GRENADES,
		/* chain */ IT_WEAPON_GLAUNCHER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_glauncher.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_GRENADES,
		/* precaches */ "models/objects/grenade4/tris.md2 weapons/grenlf1a.wav weapons/grenlr1b.wav weapons/grenlb1b.wav"
	},

/*QUAKED weapon_proxlauncher (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_plaunch/tris.md2"
*/
	{
		/* id */ IT_WEAPON_PROXLAUNCHER,
		/* classname */ "weapon_proxlauncher",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_ProxLauncher,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_plaunch/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_plaunch/tris.md2",
		/* icon */ "w_proxlaunch",
		/* use_name */  "Prox Launcher",
		/* pickup_name */  "$item_prox_launcher",
		/* pickup_name_definite */ "$item_prox_launcher_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_PROX,
		/* chain */ IT_WEAPON_GLAUNCHER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_plauncher.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_PROX,
		/* precaches */ "weapons/grenlf1a.wav weapons/grenlr1b.wav weapons/grenlb1b.wav weapons/proxwarn.wav weapons/proxopen.wav",
	},

/*QUAKED weapon_rocketlauncher (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_rocket/tris.md2"
*/
	{
		/* id */ IT_WEAPON_RLAUNCHER,
		/* classname */ "weapon_rocketlauncher",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_RocketLauncher,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_rocket/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_rocket/tris.md2",
		/* icon */ "w_rlauncher",
		/* use_name */  "Rocket Launcher",
		/* pickup_name */  "$item_rocket_launcher",
		/* pickup_name_definite */ "$item_rocket_launcher_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_ROCKETS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_rlauncher.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_ROCKETS,
		/* precaches */ "models/objects/rocket/tris.md2 weapons/rockfly.wav weapons/rocklf1a.wav weapons/rocklr1b.wav models/objects/debris2/tris.md2"
	},

/*QUAKED weapon_hyperblaster (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_hyperb/tris.md2"
*/
	{
		/* id */ IT_WEAPON_HYPERBLASTER,
		/* classname */ "weapon_hyperblaster",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_HyperBlaster,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_hyperb/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_hyperb/tris.md2",
		/* icon */ "w_hyperblaster",
		/* use_name */  "HyperBlaster",
		/* pickup_name */  "$item_hyperblaster",
		/* pickup_name_definite */ "$item_hyperblaster_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_WEAPON_HYPERBLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_hyperblaster.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_CELLS,
		/* precaches */ "weapons/hyprbu1a.wav weapons/hyprbl1a.wav weapons/hyprbf1a.wav weapons/hyprbd1a.wav misc/lasfly.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 30
	},

/*QUAKED weapon_boomer (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_boom/tris.md2"
*/
	{
		/* id */ IT_WEAPON_IONRIPPER,
		/* classname */ "weapon_boomer",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_IonRipper,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_boom/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_boomer/tris.md2",
		/* icon */ "w_ripper",
		/* use_name */  "Ionripper",
		/* pickup_name */  "$item_ionripper",
		/* pickup_name_definite */ "$item_ionripper_def",
		/* quantity */ 2,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_WEAPON_HYPERBLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_ripper.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_CELLS,
		/* precaches */ "weapons/rippfire.wav models/objects/boomrang/tris.md2 misc/lasfly.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 30
	},

/*QUAKED weapon_plasmabeam (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_beamer/tris.md2"
*/
	{
		/* id */ IT_WEAPON_PLASMABEAM,
		/* classname */ "weapon_plasmabeam",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_PlasmaBeam,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_beamer/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_beamer/tris.md2",
		/* icon */ "w_heatbeam",
		/* use_name */  "Plasma Beam",
		/* pickup_name */  "$item_plasma_beam",
		/* pickup_name_definite */ "$item_plasma_beam_def",
		/* quantity */ 2,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_WEAPON_HYPERBLASTER,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_plasma.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_CELLS,
		/* precaches */ "weapons/bfg__l1a.wav weapons/bfg_hum.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 50
	},

/*QUAKED weapon_railgun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_rail/tris.md2"
*/
	{
		/* id */ IT_WEAPON_RAILGUN,
		/* classname */ "weapon_railgun",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Railgun,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_rail/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_rail/tris.md2",
		/* icon */ "w_railgun",
		/* use_name */  "Railgun",
		/* pickup_name */  "$item_railgun",
		/* pickup_name_definite */ "$item_railgun_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_SLUGS,
		/* chain */ IT_WEAPON_RAILGUN,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_railgun.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_SLUGS,
		/* precaches */ "weapons/rg_hum.wav"
	},

/*QUAKED weapon_phalanx (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_shotx/tris.md2"
*/
	{
		/* id */ IT_WEAPON_PHALANX,
		/* classname */ "weapon_phalanx",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Phalanx,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_shotx/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_shotx/tris.md2",
		/* icon */ "w_phallanx",
		/* use_name */  "Phalanx",
		/* pickup_name */  "$item_phalanx",
		/* pickup_name_definite */ "$item_phalanx_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_MAGSLUG,
		/* chain */ IT_WEAPON_RAILGUN,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_phalanx.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_MAGSLUG,
		/* precaches */ "weapons/plasshot.wav sprites/s_photon.sp2 weapons/rockfly.wav"
	},

/*QUAKED weapon_bfg (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_bfg/tris.md2"
*/
	{
		/* id */ IT_WEAPON_BFG,
		/* classname */ "weapon_bfg",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_BFG,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_bfg/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_bfg/tris.md2",
		/* icon */ "w_bfg",
		/* use_name */  "BFG10K",
		/* pickup_name */  "$item_bfg10k",
		/* pickup_name_definite */ "$item_bfg10k_def",
		/* quantity */ 50,
		/* ammo */ IT_AMMO_CELLS,
		/* chain */ IT_WEAPON_BFG,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_bfg.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_CELLS,
		/* precaches */ "sprites/s_bfg1.sp2 sprites/s_bfg2.sp2 sprites/s_bfg3.sp2 weapons/bfg__f1y.wav weapons/bfg__l1a.wav weapons/bfg__x1b.wav weapons/bfg_hum.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 50
	},

/*QUAKED weapon_disintegrator (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_dist/tris.md2"
*/
	{
		/* id */ IT_WEAPON_DISRUPTOR,
		/* classname */ "weapon_disintegrator",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Disruptor,
		/* pickup_sound */ "misc/w_pkup.wav",
		/* world_model */ "models/weapons/g_dist/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ "models/weapons/v_dist/tris.md2",
		/* icon */ "w_disintegrator",
		/* use_name */  "Disruptor",
		/* pickup_name */  "$item_disruptor",
		/* pickup_name_definite */ "$item_disruptor_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_ROUNDS,
		/* chain */ IT_WEAPON_BFG,
		/* flags */ IF_WEAPON | IF_STAY_COOP,
		/* vwep_model */ "#w_disrupt.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_DISRUPTOR,
		/* precaches */ "models/proj/disintegrator/tris.md2 weapons/disrupt.wav weapons/disint2.wav weapons/disrupthit.wav",
	},

	//
	// AMMO ITEMS
	//

/*QUAKED ammo_shells (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/ammo/shells/medium/tris.md2"
*/
	{
		/* id */ IT_AMMO_SHELLS,
		/* classname */ "ammo_shells",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/items/ammo/shells/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_shells",
		/* use_name */  "Shells",
		/* pickup_name */  "$item_shells",
		/* pickup_name_definite */ "$item_shells_def",
		/* quantity */ 10,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_SHELLS
	},

/*QUAKED ammo_bullets (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/ammo/bullets/medium/tris.md2"
*/
	{
		/* id */ IT_AMMO_BULLETS,
		/* classname */ "ammo_bullets",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/items/ammo/bullets/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_bullets",
		/* use_name */  "Bullets",
		/* pickup_name */  "$item_bullets",
		/* pickup_name_definite */ "$item_bullets_def",
		/* quantity */ 50,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_BULLETS
	},

/*QUAKED ammo_cells (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/ammo/cells/medium/tris.md2"
*/
	{
		/* id */ IT_AMMO_CELLS,
		/* classname */ "ammo_cells",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/items/ammo/cells/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_cells",
		/* use_name */  "Cells",
		/* pickup_name */  "$item_cells",
		/* pickup_name_definite */ "$item_cells_def",
		/* quantity */ 50,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_CELLS
	},

/*QUAKED ammo_rockets (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/ammo/rockets/medium/tris.md2"
*/
	{
		/* id */ IT_AMMO_ROCKETS,
		/* classname */ "ammo_rockets",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/items/ammo/rockets/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_rockets",
		/* use_name */  "Rockets",
		/* pickup_name */  "$item_rockets",
		/* pickup_name_definite */ "$item_rockets_def",
		/* quantity */ 5,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_ROCKETS
	},

/*QUAKED ammo_slugs (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/ammo/slugs/medium/tris.md2"
*/
	{
		/* id */ IT_AMMO_SLUGS,
		/* classname */ "ammo_slugs",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/items/ammo/slugs/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_slugs",
		/* use_name */  "Slugs",
		/* pickup_name */  "$item_slugs",
		/* pickup_name_definite */ "$item_slugs_def",
		/* quantity */ 5,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_SLUGS
	},

/*QUAKED ammo_magslug (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/objects/ammo/tris.md2"
*/
	{
		/* id */ IT_AMMO_MAGSLUG,
		/* classname */ "ammo_magslug",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/objects/ammo/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_mslugs",
		/* use_name */  "Mag Slug",
		/* pickup_name */  "$item_mag_slug",
		/* pickup_name_definite */ "$item_mag_slug_def",
		/* quantity */ 10,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_MAGSLUG
	},

/*QUAKED ammo_flechettes (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/ammo/am_flechette/tris.md2"
*/
	{
		/* id */ IT_AMMO_FLECHETTES,
		/* classname */ "ammo_flechettes",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/ammo/am_flechette/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_flechettes",
		/* use_name */  "Flechettes",
		/* pickup_name */  "$item_flechettes",
		/* pickup_name_definite */ "$item_flechettes_def",
		/* quantity */ 50,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_FLECHETTES
	},

/*QUAKED ammo_prox (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/ammo/am_prox/tris.md2"
*/
	{
		/* id */ IT_AMMO_PROX,
		/* classname */ "ammo_prox",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/ammo/am_prox/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_prox",
		/* use_name */  "Prox",
		/* pickup_name */  "$item_prox",
		/* pickup_name_definite */ "$item_prox_def",
		/* quantity */ 5,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_PROX,
		/* precaches */ "models/weapons/g_prox/tris.md2 weapons/proxwarn.wav"
	},

/*QUAKED ammo_nuke (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/ammo/g_nuke/tris.md2"
*/
	{
		/* id */ IT_AMMO_NUKE,
		/* classname */ "ammo_nuke",
		/* pickup */ Pickup_Nuke,
		/* use */ Use_Nuke,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/weapons/g_nuke/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_nuke",
		/* use_name */  "A-M Bomb",
		/* pickup_name */  "$item_am_bomb",
		/* pickup_name_definite */ "$item_am_bomb_def",
		/* quantity */ 300,
		/* ammo */ IT_AMMO_NUKE,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_AM_BOMB,
		/* precaches */ "weapons/nukewarn2.wav world/rumble.wav"
	},

/*QUAKED ammo_disruptor (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/ammo/am_disr/tris.md2"
*/
	{
		/* id */ IT_AMMO_ROUNDS,
		/* classname */ "ammo_disruptor",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/ammo/am_disr/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_disruptor",
		/* use_name */  "Rounds",
		/* pickup_name */  "$item_rounds",
		/* pickup_name_definite */ "$item_rounds_def",
		/* quantity */ 3,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_DISRUPTOR
	},

//
// POWERUP ITEMS
//
/*QUAKED item_quad (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/quaddama/tris.md2"
*/
	{
		/* id */ IT_POWERUP_QUAD,
		/* classname */ "item_quad",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Quad,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/quaddama/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_quad",
		/* use_name */  "Quad Damage",
		/* pickup_name */  "$item_quad_damage",
		/* pickup_name_definite */ "$item_quad_damage_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_QUAD,
		/* precaches */ "items/damage.wav items/damage2.wav items/damage3.wav ctf/tech2x.wav"
	},

/*QUAKED item_quadfire (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/quadfire/tris.md2"
*/
	{
		/* id */ IT_POWERUP_HASTE,
		/* classname */ "item_quadfire",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Haste,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/quadfire/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_quadfire",
		/* use_name */  "Haste",
		/* pickup_name */  "Haste",
		/* pickup_name_definite */ "Haste",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_HASTE,
		/* precaches */ "items/quadfire1.wav items/quadfire2.wav items/quadfire3.wav"
	},

/*QUAKED item_invulnerability (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/invulner/tris.md2"
*/
	{
		/* id */ IT_POWERUP_PROTECTION,
		/* classname */ "item_invulnerability",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Protection,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/invulner/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_invulnerability",
		/* use_name */  "Protection",
		/* pickup_name */  "Protection",
		/* pickup_name_definite */ "Protection",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_PROTECTION,
		/* precaches */ "items/protect.wav items/protect2.wav items/protect4.wav"
	},

/*QUAKED item_invisibility (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/cloaker/tris.md2"
*/
	{
		/* id */ IT_POWERUP_INVISIBILITY,
		/* classname */ "item_invisibility",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Invisibility,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/cloaker/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_cloaker",
		/* use_name */  "Invisibility",
		/* pickup_name */  "$item_invisibility",
		/* pickup_name_definite */ "$item_invisibility_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_INVISIBILITY,
	},

/*QUAKED item_silencer (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/silencer/tris.md2"
*/
	{
		/* id */ IT_POWERUP_SILENCER,
		/* classname */ "item_silencer",
		/* pickup */ Pickup_TimedItem,
		/* use */ Use_Silencer,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/silencer/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_silencer",
		/* use_name */  "Silencer",
		/* pickup_name */  "$item_silencer",
		/* pickup_name_definite */ "$item_silencer_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_SILENCER,
	},

/*QUAKED item_breather (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/breather/tris.md2"
*/
	{
		/* id */ IT_POWERUP_REBREATHER,
		/* classname */ "item_breather",
		/* pickup */ Pickup_TimedItem,
		/* use */ Use_Breather,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/breather/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_rebreather",
		/* use_name */  "Rebreather",
		/* pickup_name */  "$item_rebreather",
		/* pickup_name_definite */ "$item_rebreather_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_TIMED | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_REBREATHER,
		/* precaches */ "items/airout.wav"
	},

/*QUAKED item_enviro (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/enviro/tris.md2"
*/
	{
		/* id */ IT_POWERUP_ENVIROSUIT,
		/* classname */ "item_enviro",
		/* pickup */ Pickup_TimedItem,
		/* use */ Use_Envirosuit,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/enviro/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_envirosuit",
		/* use_name */  "Environment Suit",
		/* pickup_name */  "$item_environment_suit",
		/* pickup_name_definite */ "$item_environment_suit_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_TIMED | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_ENVIROSUIT,
		/* precaches */ "items/airout.wav"
	},

/*QUAKED item_ancient_head (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Special item that gives +2 to maximum health
model="models/items/c_head/tris.md2"
*/
	{
		/* id */ IT_ANCIENT_HEAD,
		/* classname */ "item_ancient_head",
		/* pickup */ Pickup_LegacyHead,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/c_head/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_fixme",
		/* use_name */  "Ancient Head",
		/* pickup_name */  "$item_ancient_head",
		/* pickup_name_definite */ "$item_ancient_head_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH | IF_NOT_RANDOM,
	},

/*QUAKED item_legacy_head (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Special item that gives +5 to maximum health.
model="models/items/legacyhead/tris.md2"
*/
	{
		/* id */ IT_LEGACY_HEAD,
		/* classname */ "item_legacy_head",
		/* pickup */ Pickup_LegacyHead,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/legacyhead/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_fixme",
		/* use_name */  "Ranger's Head",
		/* pickup_name */  "Ranger's Head",
		/* pickup_name_definite */ "Ranger's Head",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH | IF_NOT_RANDOM,
	},

/*QUAKED item_adrenaline (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Gives +1 to maximum health, +5 in deathmatch.
model="models/items/adrenal/tris.md2"
*/
	{
		/* id */ IT_ADRENALINE,
		/* classname */ "item_adrenaline",
		/* pickup */ Pickup_TimedItem,
		/* use */ Use_Adrenaline,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/adrenal/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_adrenaline",
		/* use_name */  "Adrenaline",
		/* pickup_name */  "$item_adrenaline",
		/* pickup_name_definite */ "$item_adrenaline_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_ADRENALINE,
		/* precache */ "items/n_health.wav"
	},

/*QUAKED item_bandolier (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/band/tris.md2"
*/
	{
		/* id */ IT_BANDOLIER,
		/* classname */ "item_bandolier",
		/* pickup */ Pickup_Bandolier,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/band/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_bandolier",
		/* use_name */  "Bandolier",
		/* pickup_name */  "$item_bandolier",
		/* pickup_name_definite */ "$item_bandolier_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED
	},

/*QUAKED item_pack (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/pack/tris.md2"
*/
	{
		/* id */ IT_PACK,
		/* classname */ "item_pack",
		/* pickup */ Pickup_Pack,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/pack/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_pack",
		/* use_name */  "Ammo Pack",
		/* pickup_name */  "$item_ammo_pack",
		/* pickup_name_definite */ "$item_ammo_pack_def",
		/* quantity */ 180,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED
	},

/*QUAKED item_ir_goggles (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Infrared vision.
model="models/items/goggles/tris.md2"
*/
	{
		/* id */ IT_IR_GOGGLES,
		/* classname */ "item_ir_goggles",
		/* pickup */ Pickup_TimedItem,
		/* use */ Use_IR,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/goggles/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_ir",
		/* use_name */  "IR Goggles",
		/* pickup_name */  "$item_ir_goggles",
		/* pickup_name_definite */ "$item_ir_goggles_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_IR_GOGGLES,
		/* precaches */ "misc/ir_start.wav"
	},

/*QUAKED item_double (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/ddamage/tris.md2"
*/
	{
		/* id */ IT_POWERUP_DOUBLE,
		/* classname */ "item_double",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Double,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/ddamage/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_double",
		/* use_name */  "Double Damage",
		/* pickup_name */  "$item_double_damage",
		/* pickup_name_definite */ "$item_double_damage_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_DOUBLE,
		/* precaches */ "misc/ddamage1.wav misc/ddamage2.wav misc/ddamage3.wav ctf/tech2x.wav"
	},

/*QUAKED item_sphere_vengeance (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/vengnce/tris.md2"
*/
	{
		/* id */ IT_POWERUP_SPHERE_VENGEANCE,
		/* classname */ "item_sphere_vengeance",
		/* pickup */ Pickup_Sphere,
		/* use */ Use_Vengeance,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/vengnce/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_vengeance",
		/* use_name */  "vengeance sphere",
		/* pickup_name */  "$item_vengeance_sphere",
		/* pickup_name_definite */ "$item_vengeance_sphere_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_SPHERE | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_SPHERE_VENGEANCE,
		/* precaches */ "spheres/v_idle.wav"
	},

/*QUAKED item_sphere_hunter (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/hunter/tris.md2"
*/
	{
		/* id */ IT_POWERUP_SPHERE_HUNTER,
		/* classname */ "item_sphere_hunter",
		/* pickup */ Pickup_Sphere,
		/* use */ Use_Hunter,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/hunter/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_hunter",
		/* use_name */  "hunter sphere",
		/* pickup_name */  "$item_hunter_sphere",
		/* pickup_name_definite */ "$item_hunter_sphere_def",
		/* quantity */ 120,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_SPHERE | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_SPHERE_HUNTER,
		/* precaches */ "spheres/h_idle.wav spheres/h_active.wav spheres/h_lurk.wav"
	},

/*QUAKED item_sphere_defender (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/defender/tris.md2"
*/
	{
		/* id */ IT_POWERUP_SPHERE_DEFENDER,
		/* classname */ "item_sphere_defender",
		/* pickup */ Pickup_Sphere,
		/* use */ Use_Defender,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/defender/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_defender",
		/* use_name */  "defender sphere",
		/* pickup_name */  "$item_defender_sphere",
		/* pickup_name_definite */ "$item_defender_sphere_def",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_SPHERE | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_SPHERE_DEFENDER,
		/* precaches */ "models/objects/laser/tris.md2 models/items/shell/tris.md2 spheres/d_idle.wav"
	},

/*QUAKED item_doppleganger (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/dopple/tris.md2"
*/
	{
		/* id */ IT_DOPPELGANGER,
		/* classname */ "item_doppleganger",
		/* pickup */ Pickup_Doppelganger,
		/* use */ Use_Doppelganger,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/dopple/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_doppleganger",
		/* use_name */  "Doppelganger",
		/* pickup_name */  "$item_doppleganger",
		/* pickup_name_definite */ "$item_doppleganger_def",
		/* quantity */ 90,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_DOPPELGANGER,
		/* precaches */ "models/objects/dopplebase/tris.md2 models/items/spawngro3/tris.md2 medic_commander/monsterspawn1.wav models/items/hunter/tris.md2 models/items/vengnce/tris.md2",
	},

/* Tag Token */
	{
		/* id */ IT_TAG_TOKEN,
		/* classname */ nullptr,
		/* pickup */ nullptr,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/tagtoken/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB | EF_TAGTRAIL,
		/* view_model */ nullptr,
		/* icon */ "i_tagtoken",
		/* use_name */  "Tag Token",
		/* pickup_name */  "$item_tag_token",
		/* pickup_name_definite */ "$item_tag_token_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED | IF_NOT_GIVEABLE
	},

	//
	// KEYS
	//
/*QUAKED key_data_cd (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Key for computer centers.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/data_cd/tris.md2"
*/
	{
		/* id */ IT_KEY_DATA_CD,
		/* classname */ "key_data_cd",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/data_cd/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_datacd",
		/* use_name */  "Data CD",
		/* pickup_name */  "$item_data_cd",
		/* pickup_name_definite */ "$item_data_cd_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_power_cube (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN NO_TOUCH x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Power Cubes for warehouse.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/power/tris.md2"
*/
	{
		/* id */ IT_KEY_POWER_CUBE,
		/* classname */ "key_power_cube",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/power/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_powercube",
		/* use_name */  "Power Cube",
		/* pickup_name */  "$item_power_cube",
		/* pickup_name_definite */ "$item_power_cube_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_explosive_charges (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN NO_TOUCH x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Explosive Charges - for N64.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/n64/charge/tris.md2"
*/
	{
		/* id */ IT_KEY_EXPLOSIVE_CHARGES,
		/* classname */ "key_explosive_charges",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/n64/charge/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "n64/i_charges",
		/* use_name */  "Explosive Charges",
		/* pickup_name */  "$item_explosive_charges",
		/* pickup_name_definite */ "$item_explosive_charges_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_yellow_key (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Normal door key - Yellow - for N64.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/n64/yellow_key/tris.md2"
*/
	{
		/* id */ IT_KEY_YELLOW,
		/* classname */ "key_yellow_key",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/n64/yellow_key/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "n64/i_yellow_key",
		/* use_name */  "Yellow Key",
		/* pickup_name */  "$item_yellow_key",
		/* pickup_name_definite */ "$item_yellow_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_power_core (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Power Core key - for N64.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/n64/power_core/tris.md2"
*/
	{
		/* id */ IT_KEY_POWER_CORE,
		/* classname */ "key_power_core",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/n64/power_core/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_pyramid",
		/* use_name */  "Power Core",
		/* pickup_name */  "$item_power_core",
		/* pickup_name_definite */ "$item_power_core_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_pyramid (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Key for the entrance of jail3.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/pyramid/tris.md2"
*/
	{
		/* id */ IT_KEY_PYRAMID,
		/* classname */ "key_pyramid",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/pyramid/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_pyramid",
		/* use_name */  "Pyramid Key",
		/* pickup_name */  "$item_pyramid_key",
		/* pickup_name_definite */ "$item_pyramid_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_data_spinner (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Key for the city computer.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/spinner/tris.md2"
*/
	{
		/* id */ IT_KEY_DATA_SPINNER,
		/* classname */ "key_data_spinner",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/spinner/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_dataspin",
		/* use_name */  "Data Spinner",
		/* pickup_name */  "$item_data_spinner",
		/* pickup_name_definite */ "$item_data_spinner_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_pass (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Security pass for the security level.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/pass/tris.md2"
*/
	{
		/* id */ IT_KEY_PASS,
		/* classname */ "key_pass",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/pass/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_security",
		/* use_name */  "Security Pass",
		/* pickup_name */  "$item_security_pass",
		/* pickup_name_definite */ "$item_security_pass_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_blue_key (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Normal door key - Blue.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/key/tris.md2"
*/
	{
		/* id */ IT_KEY_BLUE_KEY,
		/* classname */ "key_blue_key",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/key/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_bluekey",
		/* use_name */  "Blue Key",
		/* pickup_name */  "$item_blue_key",
		/* pickup_name_definite */ "$item_blue_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_red_key (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Normal door key - Red.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/red_key/tris.md2"
*/
	{
		/* id */ IT_KEY_RED_KEY,
		/* classname */ "key_red_key",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/red_key/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_redkey",
		/* use_name */  "Red Key",
		/* pickup_name */  "$item_red_key",
		/* pickup_name_definite */ "$item_red_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_green_key (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Normal door key - Green.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/keys/green_key/tris.md2"
*/
	{
		/* id */ IT_KEY_GREEN_KEY,
		/* classname */ "key_green_key",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/green_key/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "k_green",
		/* use_name */  "Green Key",
		/* pickup_name */  "$item_green_key",
		/* pickup_name_definite */ "$item_green_key_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_commander_head (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Key - Tank Commander's Head.
model="models/monsters/commandr/head/tris.md2"
*/
	{
		/* id */ IT_KEY_COMMANDER_HEAD,
		/* classname */ "key_commander_head",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/monsters/commandr/head/tris.md2",
		/* world_model_flags */ EF_GIB,
		/* view_model */ nullptr,
		/* icon */ "k_comhead",
		/* use_name */  "Commander's Head",
		/* pickup_name */  "$item_commanders_head",
		/* pickup_name_definite */ "$item_commanders_head_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_airstrike_target (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Key - Airstrike Target for strike.
model="models/items/keys/target/tris.md2"
*/
	{
		/* id */ IT_KEY_AIRSTRIKE,
		/* classname */ "key_airstrike_target",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/keys/target/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_airstrike",
		/* use_name */  "Airstrike Marker",
		/* pickup_name */  "$item_airstrike_marker",
		/* pickup_name_definite */ "$item_airstrike_marker_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY
	},

/*QUAKED key_nuke_container (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_nuke/tris.md2"
*/
	{
		/* id */ IT_KEY_NUKE_CONTAINER,
		/* classname */ "key_nuke_container",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/weapons/g_nuke/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_contain",
		/* use_name */  "Antimatter Pod",
		/* pickup_name */  "$item_antimatter_pod",
		/* pickup_name_definite */ "$item_antimatter_pod_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY,
	},

/*QUAKED key_nuke (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_nuke/tris.md2"
*/
	{
		/* id */ IT_KEY_NUKE,
		/* classname */ "key_nuke",
		/* pickup */ Pickup_Key,
		/* use */ nullptr,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/weapons/g_nuke/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_nuke",
		/* use_name */  "Antimatter Bomb",
		/* pickup_name */  "$item_antimatter_bomb",
		/* pickup_name_definite */ "$item_antimatter_bomb_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_KEY,
	},

/*QUAKED item_health_small (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Health - Stimpack.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/healing/stimpack/tris.md2"
*/
	// Paril: split the healths up so they are always valid classnames
	{
		/* id */ IT_HEALTH_SMALL,
		/* classname */ "item_health_small",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/s_health.wav",
		/* world_model */ "models/items/healing/stimpack/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "i_health",
		/* use_name */  "Health",
		/* pickup_name */  "$item_stimpack",
		/* pickup_name_definite */ "$item_stimpack_def",
		/* quantity */ 2,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ HEALTH_IGNORE_MAX
	},

/*QUAKED item_health (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Health - First Aid.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/healing/medium/tris.md2"
*/
	{
		/* id */ IT_HEALTH_MEDIUM,
		/* classname */ "item_health",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/n_health.wav",
		/* world_model */ "models/items/healing/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "i_health",
		/* use_name */  "Health",
		/* pickup_name */  "$item_small_medkit",
		/* pickup_name_definite */ "$item_small_medkit_def",
		/* quantity */ 10,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH
	},

/*QUAKED item_health_large (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Health - Medkit.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/healing/large/tris.md2"
*/
	{
		/* id */ IT_HEALTH_LARGE,
		/* classname */ "item_health_large",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/l_health.wav",
		/* world_model */ "models/items/healing/large/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "i_health",
		/* use_name */  "Health",
		/* pickup_name */  "$item_large_medkit",
		/* pickup_name_definite */ "$item_large_medkit",
		/* quantity */ 25,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH
	},

/*QUAKED item_health_mega (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Health - Mega Health.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/items/mega_h/tris.md2"
*/
	{
		/* id */ IT_HEALTH_MEGA,
		/* classname */ "item_health_mega",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/m_health.wav",
		/* world_model */ "models/items/mega_h/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "p_megahealth",
		/* use_name */  "Mega Health",
		/* pickup_name */  "$item_mega_health",
		/* pickup_name_definite */ "$item_mega_health_def",
		/* quantity */ 100,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ HEALTH_IGNORE_MAX | HEALTH_TIMED
	},

/*QUAKED item_flag_team_red (1 0.2 0) (-16 -16 -24) (16 16 32) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Red Flag for CTF.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="players/male/flag1.md2"
*/
	{
		/* id */ IT_FLAG_RED,
		/* classname */ ITEM_CTF_FLAG_RED,
		/* pickup */ CTF_PickupFlag,
		/* use */ nullptr,
		/* drop */ CTF_DropFlag, //Should this be null if we don't want players to drop it manually?
		/* weaponthink */ nullptr,
		/* pickup_sound */ "ctf/flagtk.wav",
		/* world_model */ "players/male/flag1.md2",
		/* world_model_flags */ EF_FLAG_RED,
		/* view_model */ nullptr,
		/* icon */ "i_ctf1",
		/* use_name */  "Red Flag",
		/* pickup_name */  "$item_red_flag",
		/* pickup_name_definite */ "$item_red_flag_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_NONE,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "ctf/flagcap.wav"
	},

/*QUAKED item_flag_team_blue (1 0.2 0) (-16 -16 -24) (16 16 32) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Blue Flag for CTF.
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="players/male/flag2.md2"
*/
	{
		/* id */ IT_FLAG_BLUE,
		/* classname */ ITEM_CTF_FLAG_BLUE,
		/* pickup */ CTF_PickupFlag,
		/* use */ nullptr,
		/* drop */ CTF_DropFlag,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "ctf/flagtk.wav",
		/* world_model */ "players/male/flag2.md2",
		/* world_model_flags */ EF_FLAG_BLUE,
		/* view_model */ nullptr,
		/* icon */ "i_ctf2",
		/* use_name */  "Blue Flag",
		/* pickup_name */  "$item_blue_flag",
		/* pickup_name_definite */ "$item_blue_flag_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_NONE,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "ctf/flagcap.wav"
	},

/* Disruptor Shield Tech */
	{
		/* id */ IT_TECH_DISRUPTOR_SHIELD,
		/* classname */ "item_tech1",
		/* pickup */ Tech_Pickup,
		/* use */ nullptr,
		/* drop */ Tech_Drop,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/ctf/resistance/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "tech1",
		/* use_name */  "Disruptor Shield",
		/* pickup_name */  "$item_disruptor_shield",
		/* pickup_name_definite */ "$item_disruptor_shield_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TECH | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_TECH_DISRUPTOR_SHIELD,
		/* precaches */ "ctf/tech1.wav"
	},

/* Power Amplifier Tech */
	{
		/* id */ IT_TECH_POWER_AMP,
		/* classname */ "item_tech2",
		/* pickup */ Tech_Pickup,
		/* use */ nullptr,
		/* drop */ Tech_Drop,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/ctf/strength/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "tech2",
		/* use_name */  "Power Amplifier",
		/* pickup_name */  "$item_power_amplifier",
		/* pickup_name_definite */ "$item_power_amplifier_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TECH | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_TECH_POWER_AMP,
		/* precaches */ "ctf/tech2.wav ctf/tech2x.wav"
	},

/* Time Accel Tech */
	{
		/* id */ IT_TECH_TIME_ACCEL,
		/* classname */ "item_tech3",
		/* pickup */ Tech_Pickup,
		/* use */ nullptr,
		/* drop */ Tech_Drop,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/ctf/haste/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "tech3",
		/* use_name */  "Time Accel",
		/* pickup_name */  "$item_time_accel",
		/* pickup_name_definite */ "$item_time_accel_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TECH | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_TECH_TIME_ACCEL,
		/* precaches */ "ctf/tech3.wav"
	},

/* AutoDoc Tech */
	{
		/* id */ IT_TECH_AUTODOC,
		/* classname */ "item_tech4",
		/* pickup */ Tech_Pickup,
		/* use */ nullptr,
		/* drop */ Tech_Drop,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/ctf/regeneration/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "tech4",
		/* use_name */  "AutoDoc",
		/* pickup_name */  "$item_autodoc",
		/* pickup_name_definite */ "$item_autodoc_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TECH | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_TECH_AUTODOC,
		/* precaches */ "ctf/tech4.wav"
	},

/*QUAKED ammo_shells_large (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/shells/large/tris.md2"
*/
	{
		/* id */ IT_AMMO_SHELLS_LARGE ,
		/* classname */ "ammo_shells_large",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/vault/items/ammo/shells/large/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_shells",
		/* use_name */  "Large Shells",
		/* pickup_name */  "Large Shells",
		/* pickup_name_definite */ "Large Shells",
		/* quantity */ 20,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_SHELLS
	},

/*QUAKED ammo_shells_small (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/shells/small/tris.md2"
*/
	{
		/* id */ IT_AMMO_SHELLS_SMALL,
		/* classname */ "ammo_shells_small",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/vault/items/ammo/shells/small/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_shells",
		/* use_name */  "Small Shells",
		/* pickup_name */  "Small Shells",
		/* pickup_name_definite */ "Small Shells",
		/* quantity */ 6,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_SHELLS
	},

/*QUAKED ammo_bullets_large (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/bullets/large/tris.md2"
*/
	{
		/* id */ IT_AMMO_BULLETS_LARGE,
		/* classname */ "ammo_bullets_large",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/vault/items/ammo/bullets/large/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_bullets",
		/* use_name */  "Large Bullets",
		/* pickup_name */  "Large Bullets",
		/* pickup_name_definite */ "Large Bullets",
		/* quantity */ 100,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_BULLETS
	},

/*QUAKED ammo_bullets_small (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/bullets/small/tris.md2"
*/
	{
		/* id */ IT_AMMO_BULLETS_SMALL,
		/* classname */ "ammo_bullets_small",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/vault/items/ammo/bullets/small/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_bullets",
		/* use_name */  "Small Bullets",
		/* pickup_name */  "Small Bullets",
		/* pickup_name_definite */ "Small Bullets",
		/* quantity */ 16,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_BULLETS
	},

/*QUAKED ammo_cells_large (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/cells/large/tris.md2"
*/
	{
		/* id */ IT_AMMO_CELLS_LARGE,
		/* classname */ "ammo_cells_large",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/vault/items/ammo/cells/large/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_cells",
		/* use_name */  "Large Cells",
		/* pickup_name */  "Large Cells",
		/* pickup_name_definite */ "Large Cells",
		/* quantity */ 100,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_CELLS
	},

/*QUAKED ammo_cells_small (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/cells/small/tris.md2"
*/
	{
		/* id */ IT_AMMO_CELLS_SMALL,
		/* classname */ "ammo_cells_small",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/vault/items/ammo/cells/small/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_cells",
		/* use_name */  "Small Cells",
		/* pickup_name */  "Small Cells",
		/* pickup_name_definite */ "Small Cells",
		/* quantity */ 20,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_CELLS
	},

/*QUAKED ammo_rockets_small (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/rockets/small/tris.md2"
*/
	{
		/* id */ IT_AMMO_ROCKETS_SMALL,
		/* classname */ "ammo_rockets_small",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/vault/items/ammo/rockets/small/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_rockets",
		/* use_name */  "Small Rockets",
		/* pickup_name */  "Small Rockets",
		/* pickup_name_definite */ "Small Rockets",
		/* quantity */ 2,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_ROCKETS
	},

/*QUAKED ammo_slugs_large (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/slugs/large/tris.md2"
*/
	{
		/* id */ IT_AMMO_SLUGS_LARGE,
		/* classname */ "ammo_slugs_large",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/vault/items/ammo/slugs/large/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_slugs",
		/* use_name */  "Large Slugs",
		/* pickup_name */  "Large Slugs",
		/* pickup_name_definite */ "Large Slugs",
		/* quantity */ 20,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_SLUGS
	},

/*QUAKED ammo_slugs_small (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/slugs/small/tris.md2"
*/
	{
		/* id */ IT_AMMO_SLUGS_SMALL,
		/* classname */ "ammo_slugs_small",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/vault/items/ammo/slugs/small/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_slugs",
		/* use_name */  "Small Slugs",
		/* pickup_name */  "Small Slugs",
		/* pickup_name_definite */ "Small Slugs",
		/* quantity */ 3,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_SLUGS
	},

/*QUAKED item_teleporter (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/vault/items/ammo/nuke/tris.md2"
*/
	{
		/* id */ IT_TELEPORTER,
		/* classname */ "item_teleporter",
		/* pickup */ Pickup_Teleporter,
		/* use */ Use_Teleporter,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/vault/items/ammo/nuke/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_fixme",
		/* use_name */  "Personal Teleporter",
		/* pickup_name */  "Personal Teleporter",
		/* pickup_name_definite */ "Personal Teleporter",
		/* quantity */ 120,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_TIMED | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF
	},

/*QUAKED item_regen (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
model="models/items/invulner/tris.md2"
*/
	{
		/* id */ IT_POWERUP_REGEN,
		/* classname */ "item_regen",
		/* pickup */ Pickup_Powerup,
		/* use */ Use_Regeneration,
		/* drop */ Drop_General,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/invulner/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_fixme",
		/* use_name */  "Regeneration",
		/* pickup_name */  "Regeneration",
		/* pickup_name_definite */ "Regeneration",
		/* quantity */ 60,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP | IF_POWERUP_WHEEL,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_REGEN,
		/* precaches */ "items/protect.wav"
	},

/*QUAKED item_foodcube (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Meaty cube o' health
model="models/objects/trapfx/tris.md2"
*/
	{
		/* id */ IT_FOODCUBE,
		/* classname */ "item_foodcube",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/n_health.wav",
		/* world_model */ "models/objects/trapfx/tris.md2",
		/* world_model_flags */ EF_GIB,
		/* view_model */ nullptr,
		/* icon */ "i_health",
		/* use_name */  "Meaty Cube",
		/* pickup_name */  "Meaty Cube",
		/* pickup_name_definite */ "Meaty Cube",
		/* quantity */ 50,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ HEALTH_IGNORE_MAX
	},

/*QUAKED item_ball (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN x x SUSPENDED x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Big ol' ball
models/items/ammo/grenades/medium/tris.md2"
*/
	{
		/* id */ IT_BALL,
		/* classname */ "item_ball",
		/* pickup */ Pickup_Ball,
		/* use */ Use_Ball,
		/* drop */ Drop_Ball,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/ammo/grenades/medium/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "i_help",
		/* use_name */  "Ball",
		/* pickup_name */  "Ball",
		/* pickup_name_definite */ "Ball",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_POWERUP| IF_POWERUP_WHEEL | IF_NOT_RANDOM,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_BALL,
		/* precaches */ "",
		/* sort_id */ -1
	},
	
/* Flashlight */
	{
		/* id */ IT_FLASHLIGHT,
		/* classname */ "item_flashlight",
		/* pickup */ Pickup_General,
		/* use */ Use_Flashlight,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/flashlight/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_torch",
		/* use_name */  "Flashlight",
		/* pickup_name */  "$item_flashlight",
		/* pickup_name_definite */ "$item_flashlight_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF | IF_NOT_RANDOM,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_FLASHLIGHT,
		/* precaches */ "items/flashlight_on.wav items/flashlight_off.wav",
		/* sort_id */ -1
	},

/* Compass */
	{
		/* id */ IT_COMPASS,
		/* classname */ "item_compass",
		/* pickup */ nullptr,
		/* use */ Use_Compass,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ nullptr,
		/* world_model */ nullptr,
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "p_compass",
		/* use_name */  "Compass",
		/* pickup_name */  "$item_compass",
		/* pickup_name_definite */ "$item_compass_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_COMPASS,
		/* precaches */ "misc/help_marker.wav",
		/* sort_id */ -2
	},
};
// clang-format on

void InitItems() {
	// validate item integrity
	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1))
		if (itemlist[i].id != i)
			gi.Com_ErrorFmt("Item {} has wrong enum ID {} (should be {})", itemlist[i].pickup_name, (int32_t)itemlist[i].id, (int32_t)i);

	// set up weapon chains
	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1)) {
		if (!itemlist[i].chain)
			continue;

		gitem_t *item = &itemlist[i];

		// already initialized
		if (item->chain_next)
			continue;

		gitem_t *chain_item = &itemlist[item->chain];

		if (!chain_item)
			gi.Com_ErrorFmt("Invalid item chain {} for {}", (int32_t)item->chain, item->pickup_name);

		// set up initial chain
		if (!chain_item->chain_next)
			chain_item->chain_next = chain_item;

		// if we're not the first in chain, add us now
		if (chain_item != item) {
			gitem_t *c;

			// end of chain is one whose chain_next points to chain_item
			for (c = chain_item; c->chain_next != chain_item; c = c->chain_next)
				continue;

			// splice us in
			item->chain_next = chain_item;
			c->chain_next = item;
		}
	}

	// set up ammo
	for (auto &it : itemlist) {
		if ((it.flags & IF_AMMO) && it.tag >= AMMO_BULLETS && it.tag < AMMO_MAX) {
			if (it.id <= IT_AMMO_ROUNDS)
				ammolist[it.tag] = &it;
		}
		else if ((it.flags & IF_POWERUP_WHEEL) && !(it.flags & IF_WEAPON) && it.tag >= POWERUP_SCREEN && it.tag < POWERUP_MAX)
			poweruplist[it.tag] = &it;
	}

	// in coop or DM with Weapons' Stay, remove drop ptr
	for (auto &it : itemlist) {
		if (coop->integer)
			if (!P_UseCoopInstancedItems() && (it.flags & IF_STAY_COOP))
				it.drop = nullptr;
	}
}

/*
===============
G_CanDropItem
===============
*/
static inline bool G_CanDropItem(const gitem_t &item) {
	if (!item.drop)
		return false;
	else if ((item.flags & IF_WEAPON) && !(item.flags & IF_AMMO) && deathmatch->integer && g_dm_weapons_stay->integer)
		return false;

	return true;
}

/*
===============
SetItemNames

Called by worldspawn
===============
*/
void SetItemNames() {
	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1))
		gi.configstring(CS_ITEMS + i, itemlist[i].pickup_name);

	// [Paril-KEX] set ammo wheel indices first
	int32_t cs_index = 0;

	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1)) {
		if (!(itemlist[i].flags & IF_AMMO))
			continue;

		if (cs_index >= MAX_WHEEL_ITEMS)
			gi.Com_Error("out of wheel indices");

		gi.configstring(CS_WHEEL_AMMO + cs_index, G_Fmt("{}|{}", (int32_t)i, gi.imageindex(itemlist[i].icon)).data());
		itemlist[i].ammo_wheel_index = cs_index;
		cs_index++;
	}

	// set weapon wheel indices
	cs_index = 0;

	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1)) {
		if (!(itemlist[i].flags & IF_WEAPON))
			continue;

		if (cs_index >= MAX_WHEEL_ITEMS)
			gi.Com_Error("out of wheel indices");

		int32_t min_ammo = (itemlist[i].flags & IF_AMMO) ? 1 : itemlist[i].quantity;

		gi.configstring(CS_WHEEL_WEAPONS + cs_index, G_Fmt("{}|{}|{}|{}|{}|{}|{}|{}",
			(int32_t)i,
			gi.imageindex(itemlist[i].icon),
			itemlist[i].ammo ? GetItemByIndex(itemlist[i].ammo)->ammo_wheel_index : -1,
			min_ammo,
			(itemlist[i].flags & IF_POWERUP_WHEEL) ? 1 : 0,
			itemlist[i].sort_id,
			itemlist[i].quantity_warn,
			G_CanDropItem(itemlist[i]) ? 1 : 0
		).data());
		itemlist[i].weapon_wheel_index = cs_index;
		cs_index++;
	}

	// set powerup wheel indices
	cs_index = 0;

	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1)) {
		if (!(itemlist[i].flags & IF_POWERUP_WHEEL) || (itemlist[i].flags & IF_WEAPON))
			continue;

		if (cs_index >= MAX_WHEEL_ITEMS)
			gi.Com_Error("out of wheel indices");

		gi.configstring(CS_WHEEL_POWERUPS + cs_index, G_Fmt("{}|{}|{}|{}|{}|{}",
			(int32_t)i,
			gi.imageindex(itemlist[i].icon),
			(itemlist[i].flags & IF_POWERUP_ONOFF) ? 1 : 0,
			itemlist[i].sort_id,
			G_CanDropItem(itemlist[i]) ? 1 : 0,
			itemlist[i].ammo ? GetItemByIndex(itemlist[i].ammo)->ammo_wheel_index : -1
		).data());
		itemlist[i].powerup_wheel_index = cs_index;
		cs_index++;
	}
}
