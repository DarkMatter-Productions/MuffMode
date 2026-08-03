// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// g_turret.c

#include "g_local.h"

constexpr spawnflags_t SPAWNFLAG_TURRET_BREACH_FIRE = 65536_spawnflag;

static void AnglesNormalize(vec3_t &vec) {
	while (vec[0] > 360)
		vec[0] -= 360;
	while (vec[0] < 0)
		vec[0] += 360;
	while (vec[1] > 360)
		vec[1] -= 360;
	while (vec[1] < 0)
		vec[1] += 360;
}

// [MuffMode] Map killtargets can free or recycle any turret part without
// invoking monster death cleanup, so controller links are live references.
static bool turret_is_controller(const gentity_t *entity) noexcept
{
	return entity && entity->classname &&
		(strcmp(entity->classname, "turret_driver") == 0 ||
			strcmp(entity->classname, "turret_invisible_brain") == 0);
}

static bool turret_is_graph_part(const gentity_t *entity) noexcept
{
	return entity && entity->inuse && entity->classname &&
		(strcmp(entity->classname, "turret_base") == 0 ||
			strcmp(entity->classname, "turret_breach") == 0 ||
			turret_is_controller(entity));
}

static size_t turret_team_search_limit() noexcept
{
	return std::min(static_cast<size_t>(globals.num_entities),
		static_cast<size_t>(game.maxentities));
}

static bool turret_entity_lifetime_matches(
	const gentity_t *entity, int32_t generation) noexcept
{
	return entity && entity->inuse && entity->spawn_count == generation;
}

static bool turret_reference_is_active(
	const gentity_t *entity, int32_t generation) noexcept
{
	return turret_entity_lifetime_matches(entity, generation) &&
		(!entity->client || entity->client->pers.connected);
}

static bool turret_is_team_master(const gentity_t *master) noexcept
{
	return master && master->inuse && master->teammaster == master &&
		(master->flags & FL_TEAMMASTER) && !(master->flags & FL_TEAMSLAVE);
}

static bool turret_is_reciprocal_team_member(
	const gentity_t *member, const gentity_t *master) noexcept
{
	if (!member || !member->inuse || !master)
		return false;
	if (member == master)
		return turret_is_team_master(master);
	return member->teammaster == master &&
		(member->flags & FL_TEAMSLAVE) &&
		!(member->flags & FL_TEAMMASTER);
}

// A damaged chain is not authoritative for gameplay, but its current,
// reciprocal prefix is still safe to stop. This scan is deliberately bounded:
// corrupt save data or a re-entrant callback can leave a cycle behind.
static gentity_t *turret_find_reciprocal_breach(gentity_t *master) noexcept
{
	if (!turret_is_team_master(master))
		return nullptr;

	gentity_t *member = master;
	size_t remaining = turret_team_search_limit();
	while (member && remaining > 0) {
		--remaining;
		if (!turret_is_reciprocal_team_member(member, master))
			break;
		if (member->classname &&
			strcmp(member->classname, "turret_breach") == 0) {
			return member;
		}
		member = member->teamchain;
	}
	return nullptr;
}

static bool turret_validate_team_chain(gentity_t *master,
	const gentity_t *required_member, gentity_t **tail_out = nullptr) noexcept
{
	if (!turret_is_team_master(master))
		return false;

	bool found_required = required_member == nullptr;
	gentity_t *tail = nullptr;
	gentity_t *member = master;
	size_t remaining = turret_team_search_limit();
	while (member && remaining > 0) {
		--remaining;
		if (!turret_is_reciprocal_team_member(member, master))
			return false;
		if (member == required_member)
			found_required = true;
		tail = member;
		member = member->teamchain;
	}

	if (member || !found_required)
		return false;
	if (tail_out)
		*tail_out = tail;
	return true;
}

static void turret_quiesce_team(gentity_t *master) noexcept
{
	if (!turret_is_team_master(master))
		return;
	gentity_t *member = master;
	size_t remaining = turret_team_search_limit();
	while (member && remaining > 0) {
		--remaining;
		if (!turret_is_reciprocal_team_member(member, master))
			return;
		member->velocity = {};
		member->avelocity = {};
		member->s.sound = 0;
		member = member->teamchain;
	}
}

static bool turret_team_master_is_trusted(
	const gentity_t *entity, const gentity_t *master) noexcept
{
	if (!turret_is_team_master(master))
		return false;
	return entity == master || turret_is_graph_part(master) ||
		(entity && entity->turret_master_generation != 0 &&
			turret_entity_lifetime_matches(
				master, entity->turret_master_generation));
}

static bool turret_has_structural_breach_target(
	const gentity_t *breach) noexcept
{
	return breach && breach->inuse && breach->classname &&
		strcmp(breach->classname, "turret_breach") == 0 &&
		turret_is_team_master(breach->teammaster);
}

static bool turret_has_structural_breach_team(gentity_t *breach) noexcept
{
	return turret_has_structural_breach_target(breach) &&
		turret_validate_team_chain(breach->teammaster, breach);
}

static bool turret_capture_breach_team(gentity_t *breach) noexcept
{
	if (!turret_has_structural_breach_team(breach))
		return false;
	breach->turret_master_generation = breach->teammaster->spawn_count;
	return true;
}

static bool turret_has_valid_breach_target(const gentity_t *breach) noexcept
{
	return turret_has_structural_breach_team(
		const_cast<gentity_t *>(breach)) &&
		turret_entity_lifetime_matches(breach->teammaster,
			breach->turret_master_generation);
}

static gentity_t *turret_find_structural_breach(gentity_t *master) noexcept
{
	if (!turret_validate_team_chain(master, nullptr))
		return nullptr;

	gentity_t *member = master;
	size_t remaining = turret_team_search_limit();
	while (member && remaining > 0) {
		--remaining;
		if (member->classname && strcmp(member->classname, "turret_breach") == 0)
			return member;
		member = member->teamchain;
	}
	return nullptr;
}

static gentity_t *turret_find_valid_breach(gentity_t *master) noexcept
{
	gentity_t *const breach = turret_find_structural_breach(master);
	return turret_has_valid_breach_target(breach) ? breach : nullptr;
}

static bool turret_controller_structurally_owns_breach(
	const gentity_t *controller, const gentity_t *breach) noexcept
{
	if (!controller || !controller->inuse || !turret_is_controller(controller) ||
		!breach || !breach->inuse || !breach->classname ||
		strcmp(breach->classname, "turret_breach") != 0)
		return false;

	gentity_t *tail = nullptr;
	return turret_has_structural_breach_team(
			const_cast<gentity_t *>(breach)) &&
		turret_validate_team_chain(breach->teammaster, breach, &tail) &&
		tail == controller && controller->teamchain == nullptr &&
		controller->target_ent == breach &&
		controller->teammaster == breach->teammaster &&
		breach->owner == controller &&
		breach->teammaster->owner == controller;
}

static gentity_t *turret_find_attached_controller(gentity_t *breach) noexcept
{
	if (!turret_has_structural_breach_team(breach))
		return nullptr;

	gentity_t *tail = nullptr;
	if (!turret_validate_team_chain(breach->teammaster, breach, &tail) ||
		!tail || !turret_is_controller(tail) || tail->target_ent != breach ||
		tail->teammaster != breach->teammaster)
		return nullptr;
	return tail;
}

static bool turret_controller_owns_breach(
	const gentity_t *controller, const gentity_t *breach) noexcept
{
	return controller && breach &&
		turret_controller_structurally_owns_breach(controller, breach) &&
		turret_entity_lifetime_matches(breach,
			controller->turret_breach_generation) &&
		turret_entity_lifetime_matches(breach->teammaster,
			controller->turret_master_generation) &&
		turret_entity_lifetime_matches(controller,
			breach->turret_controller_generation) &&
		turret_has_valid_breach_target(breach);
}

static void turret_stamp_controller_relationship(
	gentity_t *controller, gentity_t *breach) noexcept
{
	breach->turret_master_generation = breach->teammaster->spawn_count;
	breach->turret_controller_generation = controller->spawn_count;
	controller->turret_breach_generation = breach->spawn_count;
	controller->turret_master_generation = breach->teammaster->spawn_count;
}

static gentity_t *turret_resolve_controller(gentity_t *breach) noexcept
{
	if (!turret_has_valid_breach_target(breach))
		return nullptr;
	if (!breach->owner && !breach->teammaster->owner)
		return nullptr;

	gentity_t *controller = breach->owner;
	if (turret_controller_owns_breach(controller, breach))
		return controller;

	controller = breach->teammaster->owner;
	if (turret_controller_owns_breach(controller, breach))
		return controller;

	// Conflicting or stale ownership is never authoritative. The generation
	// stamp remains available until after both links have been discarded.
	breach->owner = nullptr;
	breach->teammaster->owner = nullptr;
	breach->turret_controller_generation = 0;
	breach->moveinfo.blocked = nullptr;
	breach->spawnflags &= ~SPAWNFLAG_TURRET_BREACH_FIRE;
	return nullptr;
}

static void turret_detach_controller(gentity_t *controller) noexcept
{
	if (!controller)
		return;

	gentity_t *const breach = controller->target_ent;
	gentity_t *const teammaster = controller->teammaster;
	const int32_t controller_generation = controller->spawn_count;
	const bool breach_is_current =
		turret_entity_lifetime_matches(
			breach, controller->turret_breach_generation) &&
		breach->classname && strcmp(breach->classname, "turret_breach") == 0;
	const bool teammaster_is_current =
		turret_entity_lifetime_matches(
			teammaster, controller->turret_master_generation) &&
		turret_is_team_master(teammaster);

	if (teammaster && teammaster_is_current) {
		turret_quiesce_team(teammaster);
		gentity_t *predecessor = teammaster;
		size_t remaining = turret_team_search_limit();
		while (predecessor && remaining > 0) {
			--remaining;
			if (!turret_is_reciprocal_team_member(predecessor, teammaster))
				break;

			gentity_t *const next = predecessor->teamchain;
			if (next == controller) {
				predecessor->teamchain = nullptr;
				break;
			}
			if (!next)
				break;
			if (!turret_is_reciprocal_team_member(next, teammaster) ||
				remaining == 0) {
				predecessor->teamchain = nullptr;
				break;
			}
			predecessor = next;
		}
		if (teammaster->owner == controller)
			teammaster->owner = nullptr;
	}

	if (breach_is_current && breach->owner == controller &&
		breach->turret_controller_generation == controller_generation) {
		breach->owner = nullptr;
		breach->turret_controller_generation = 0;
		breach->moveinfo.blocked = nullptr;
		breach->spawnflags &= ~SPAWNFLAG_TURRET_BREACH_FIRE;
	}

	controller->teammaster = nullptr;
	controller->teamchain = nullptr;
	controller->flags &= ~FL_TEAMSLAVE;
	controller->target_ent = nullptr;
	controller->velocity = {};
	controller->avelocity = {};
	controller->s.sound = 0;
	controller->turret_breach_generation = 0;
	controller->turret_master_generation = 0;
	controller->turret_controller_generation = 0;
	controller->activator = nullptr;
	controller->turret_activator_generation = 0;
	controller->enemy = nullptr;
	controller->turret_enemy_generation = 0;
}

static bool turret_attach_controller(
	gentity_t *controller, gentity_t *breach) noexcept
{
	if (!controller || !controller->inuse || !turret_is_controller(controller) ||
		controller->target_ent || controller->teammaster ||
		controller->teamchain || (controller->flags & FL_TEAMSLAVE) ||
		!turret_capture_breach_team(breach) || breach->owner ||
		breach->teammaster->owner) {
		return false;
	}

	gentity_t *tail = nullptr;
	if (!turret_validate_team_chain(breach->teammaster, breach, &tail) || !tail)
		return false;

	tail->teamchain = controller;
	controller->target_ent = breach;
	controller->teammaster = breach->teammaster;
	controller->flags |= FL_TEAMSLAVE;
	turret_stamp_controller_relationship(controller, breach);
	breach->owner = controller;
	breach->teammaster->owner = controller;
	return true;
}

static void turret_disable_breach(gentity_t *breach) noexcept
{
	if (!breach)
		return;
	breach->think = nullptr;
	breach->nextthink = 0_ms;
	breach->avelocity = {};
	breach->s.sound = 0;
	breach->moveinfo.blocked = nullptr;
	breach->spawnflags &= ~SPAWNFLAG_TURRET_BREACH_FIRE;
}

void G_TurretPrepareEntityFree(gentity_t *entity)
{
	if (!entity || !entity->inuse || !entity->teammaster)
		return;
	gentity_t *const master = entity->teammaster;
	gentity_t *breach = turret_find_reciprocal_breach(master);
	if (!breach && entity->classname &&
		strcmp(entity->classname, "turret_breach") == 0) {
		breach = entity;
	}
	const bool known_turret_graph = turret_is_graph_part(entity) ||
		turret_is_graph_part(master) || breach;
	if (!known_turret_graph)
		return;

	// Stop every current reciprocal member we can safely identify before full
	// validation. Otherwise freeing a breach from a cyclic/damaged graph can
	// strand the surviving pushers with angular velocity and a looping sound.
	if (turret_team_master_is_trusted(entity, master) ||
		(breach && turret_team_master_is_trusted(breach, master))) {
		turret_quiesce_team(master);
	}
	if (breach)
		turret_disable_breach(breach);

	if (!turret_validate_team_chain(master, entity))
		return;
	breach = turret_find_structural_breach(master);
	if (!breach)
		return;

	gentity_t *controller = nullptr;
	if (turret_controller_structurally_owns_breach(breach->owner, breach))
		controller = breach->owner;
	else if (turret_controller_structurally_owns_breach(master->owner, breach))
		controller = master->owner;

	if (controller) {
		// A structurally reciprocal graph is authoritative during teardown even
		// when a prior fault invalidated one of its cached generation stamps.
		turret_stamp_controller_relationship(controller, breach);
		turret_detach_controller(controller);
	} else {
		breach->owner = nullptr;
		master->owner = nullptr;
		breach->turret_controller_generation = 0;
	}
}

MOVEINFO_BLOCKED(turret_blocked) (gentity_t *self, gentity_t *other) -> void {
	gentity_t *const teammaster = self->teammaster;
	if (!other || !other->takedamage ||
		!turret_validate_team_chain(teammaster, self))
		return;

	gentity_t *attacker = teammaster;
	if (gentity_t *const breach = turret_find_valid_breach(teammaster)) {
		if (gentity_t *const controller = turret_resolve_controller(breach))
			attacker = controller;
	}
	T_Damage(other, self, attacker, vec3_origin, other->s.origin,
		vec3_origin, teammaster->dmg, 10, DAMAGE_NONE, MOD_CRUSH);
}

/*QUAKED turret_breach (0 0 0) ? x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This portion of the turret can change both pitch and yaw.
The model  should be made with a flat pitch.
It (and the associated base) need to be oriented towards 0.
Use "angle" to set the starting angle.

"speed"		default 50
"dmg"		default 10
"angle"		point this forward
"target"	point this at an info_notnull at the muzzle tip
"minpitch"	min acceptable pitch angle : default -30
"maxpitch"	max acceptable pitch angle : default 30
"minyaw"	min acceptable yaw angle   : default 0
"maxyaw"	max acceptable yaw angle   : default 360
*/

static void turret_breach_fire(gentity_t *self, gentity_t *controller,
	gentity_t *teammaster) {
	vec3_t f, r, u;
	vec3_t start;
	int	   damage;
	int	   speed;
	if (!turret_controller_owns_breach(controller, self) ||
		self->teammaster != teammaster)
		return;

	AngleVectors(self->s.angles, f, r, u);
	start = self->s.origin + (f * self->move_origin[0]);
	start += (r * self->move_origin[1]);
	start += (u * self->move_origin[2]);

	if (self->count)
		damage = self->count;
	else
		damage = (int)frandom(100, 150);
	speed = 550 + 50 * skill->integer;
	gentity_t *attacker = controller;
	if (turret_reference_is_active(controller->activator,
			controller->turret_activator_generation))
		attacker = controller->activator;
	else if (controller->activator &&
		!turret_entity_lifetime_matches(controller->activator,
			controller->turret_activator_generation)) {
		controller->activator = nullptr;
		controller->turret_activator_generation = 0;
	}
	gentity_t *const rocket = fire_rocket(attacker, start, f, damage, speed, 150, damage);
	if (rocket)
		rocket->s.scale = teammaster->splash_radius;

	gi.positioned_sound(start, self, CHAN_WEAPON, gi.soundindex("weapons/rocklf1a.wav"), 1, ATTN_NORM, 0);
}

static THINK(turret_breach_think) (gentity_t *self) -> void {
	vec3_t	 current_angles;
	vec3_t	 delta;
	if (!turret_has_valid_breach_target(self)) {
		gi.Com_Print("WARNING: turret_breach lost its turret team; disabling entity.\n");
		if (turret_team_master_is_trusted(self, self->teammaster))
			turret_quiesce_team(self->teammaster);
		turret_disable_breach(self);
		return;
	}

	gentity_t *const teammaster = self->teammaster;
	gentity_t *const controller = turret_resolve_controller(self);

	current_angles = self->s.angles;
	AnglesNormalize(current_angles);

	AnglesNormalize(self->move_angles);
	if (self->move_angles[PITCH] > 180)
		self->move_angles[PITCH] -= 360;

	// clamp angles to mins & maxs
	if (self->move_angles[PITCH] > self->pos1[PITCH])
		self->move_angles[PITCH] = self->pos1[PITCH];
	else if (self->move_angles[PITCH] < self->pos2[PITCH])
		self->move_angles[PITCH] = self->pos2[PITCH];

	if ((self->move_angles[YAW] < self->pos1[YAW]) || (self->move_angles[YAW] > self->pos2[YAW])) {
		float dmin, dmax;

		dmin = fabsf(self->pos1[YAW] - self->move_angles[YAW]);
		if (dmin < -180)
			dmin += 360;
		else if (dmin > 180)
			dmin -= 360;
		dmax = fabsf(self->pos2[YAW] - self->move_angles[YAW]);
		if (dmax < -180)
			dmax += 360;
		else if (dmax > 180)
			dmax -= 360;
		if (fabsf(dmin) < fabsf(dmax))
			self->move_angles[YAW] = self->pos1[YAW];
		else
			self->move_angles[YAW] = self->pos2[YAW];
	}

	delta = self->move_angles - current_angles;
	if (delta[0] < -180)
		delta[0] += 360;
	else if (delta[0] > 180)
		delta[0] -= 360;
	if (delta[1] < -180)
		delta[1] += 360;
	else if (delta[1] > 180)
		delta[1] -= 360;
	delta[2] = 0;

	if (delta[0] > self->speed * gi.frame_time_s)
		delta[0] = self->speed * gi.frame_time_s;
	if (delta[0] < -1 * self->speed * gi.frame_time_s)
		delta[0] = -1 * self->speed * gi.frame_time_s;
	if (delta[1] > self->speed * gi.frame_time_s)
		delta[1] = self->speed * gi.frame_time_s;
	if (delta[1] < -1 * self->speed * gi.frame_time_s)
		delta[1] = -1 * self->speed * gi.frame_time_s;

	gentity_t *ent = teammaster;
	size_t remaining = turret_team_search_limit();
	while (ent && remaining > 0) {
		--remaining;
		if (!turret_is_reciprocal_team_member(ent, teammaster)) {
			turret_disable_breach(self);
			return;
		}
		if (ent->noise_index) {
			if (delta[0] || delta[1]) {
				ent->s.sound = ent->noise_index;
				ent->s.loop_attenuation = ATTN_NORM;
			} else
				ent->s.sound = 0;
		}
		ent = ent->teamchain;
	}
	if (ent) {
		turret_disable_breach(self);
		return;
	}

	self->avelocity = delta * (1.0f / gi.frame_time_s);

	self->nextthink = level.time + FRAME_TIME_S;

	ent = teammaster;
	remaining = turret_team_search_limit();
	while (ent && remaining > 0) {
		--remaining;
		if (!turret_is_reciprocal_team_member(ent, teammaster)) {
			turret_disable_breach(self);
			return;
		}
		ent->avelocity[1] = self->avelocity[1];
		ent = ent->teamchain;
	}
	if (ent) {
		turret_disable_breach(self);
		return;
	}

	// if we have a driver, adjust his velocities
	if (controller) {
		float  angle;
		float  target_z;
		float  diff;
		vec3_t target;
		vec3_t dir;

		// angular is easy, just copy ours
		controller->avelocity[0] = self->avelocity[0];
		controller->avelocity[1] = self->avelocity[1];

		// x & y
		angle = self->s.angles[YAW] + controller->move_origin[1];
		angle *= (float)(PI * 2 / 360);
		target[0] = self->s.origin[0] + cosf(angle) * controller->move_origin[0];
		target[1] = self->s.origin[1] + sinf(angle) * controller->move_origin[0];
		target[2] = controller->s.origin[2];

		dir = target - controller->s.origin;
		controller->velocity[0] = dir[0] * 1.0f / gi.frame_time_s;
		controller->velocity[1] = dir[1] * 1.0f / gi.frame_time_s;

		// z
		angle = self->s.angles[PITCH] * (float)(PI * 2 / 360);
		target_z = self->s.origin[2] + controller->move_origin[0] * tan(angle) + controller->move_origin[2];

		diff = target_z - controller->s.origin[2];
		controller->velocity[2] = diff * 1.0f / gi.frame_time_s;

		if (self->spawnflags.has(SPAWNFLAG_TURRET_BREACH_FIRE)) {
			turret_breach_fire(self, controller, teammaster);
			self->spawnflags &= ~SPAWNFLAG_TURRET_BREACH_FIRE;
		}
	}
}

static THINK(turret_breach_finish_init) (gentity_t *self) -> void {
	const int32_t self_generation = self->spawn_count;
	if (!turret_capture_breach_team(self)) {
		gi.Com_PrintFmt(
			"WARNING: turret_breach is not part of a complete turret team; disabling entity.\n");
		turret_disable_breach(self);
		return;
	}

	// get and save info for muzzle location
	if (!self->target) {
		gi.Com_PrintFmt("{}: needs a target\n", *self);
	} else {
		self->target_ent = G_PickTarget(self->target);
		if (self->target_ent) {
			self->move_origin = self->target_ent->s.origin - self->s.origin;
			G_FreeEntity(self->target_ent);
			if (!turret_entity_lifetime_matches(self, self_generation))
				return;
		} else
			gi.Com_PrintFmt("{}: could not find target entity \"{}\"\n", *self, self->target);
	}
	if (!turret_has_valid_breach_target(self)) {
		turret_disable_breach(self);
		return;
	}

	self->teammaster->dmg = self->dmg;
	self->teammaster->splash_radius = self->splash_radius; // scale
	self->think = turret_breach_think;
	self->think(self);
}

void SP_turret_breach(gentity_t *self) {
	self->solid = SOLID_BSP;
	self->movetype = MOVETYPE_PUSH;

	if (st.noise)
		self->noise_index = gi.soundindex(st.noise);

	gi.setmodel(self, self->model);

	if (!self->speed)
		self->speed = 50;
	if (!self->dmg)
		self->dmg = 10;

	if (!st.minpitch)
		st.minpitch = -30;
	if (!st.maxpitch)
		st.maxpitch = 30;
	if (!st.maxyaw)
		st.maxyaw = 360;

	self->pos1[PITCH] = -1 * st.minpitch;
	self->pos1[YAW] = st.minyaw;
	self->pos2[PITCH] = -1 * st.maxpitch;
	self->pos2[YAW] = st.maxyaw;

	// scale used for rocket scale
	self->splash_radius = self->s.scale;
	self->s.scale = 0;

	self->ideal_yaw = self->s.angles[YAW];
	self->move_angles[YAW] = self->ideal_yaw;

	self->moveinfo.blocked = turret_blocked;

	self->think = turret_breach_finish_init;
	self->nextthink = level.time + FRAME_TIME_S;
	gi.linkentity(self);
}

/*QUAKED turret_base (0 0 0) ? x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This portion of the turret changes yaw only.
MUST be teamed with a turret_breach.
*/

void SP_turret_base(gentity_t *self) {
	self->solid = SOLID_BSP;
	self->movetype = MOVETYPE_PUSH;

	if (st.noise)
		self->noise_index = gi.soundindex(st.noise);

	gi.setmodel(self, self->model);
	self->moveinfo.blocked = turret_blocked;
	gi.linkentity(self);
}

/*QUAKED turret_driver (1 .5 0) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Must NOT be on the team with the rest of the turret parts.
Instead it must target the turret_breach.
*/

void infantry_die(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod);
void infantry_stand(gentity_t *self);
void infantry_pain(gentity_t *self, gentity_t *other, float kick, int damage, const mod_t &mod);
void infantry_setskin(gentity_t *self);

static DIE(turret_driver_die) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod) -> void {
	if (!self->deadflag) {
		gentity_t *const target_ent = self->target_ent;
		if (turret_controller_owns_breach(self, target_ent)) {
			// level the gun
			target_ent->move_angles[0] = 0;
		}
		turret_detach_controller(self);

		// clear pitch
		self->s.angles[PITCH] = 0;
		self->movetype = MOVETYPE_STEP;

		self->think = monster_think;
	}

	infantry_die(self, inflictor, attacker, damage, point, mod);

	G_FixStuckObject(self, self->s.origin);
	AngleVectors(self->s.angles, self->velocity, nullptr, nullptr);
	self->velocity *= -50;
	self->velocity.z += 110.f;
}

bool FindTarget(gentity_t *self);

static void turret_driver_rollback_registration(gentity_t *self) noexcept
{
	if (level.total_monsters <= 0)
		return;

	const size_t registered_count = std::min(
		static_cast<size_t>(level.total_monsters),
		level.monsters_registered.size());
	const auto end = level.monsters_registered.begin() + registered_count;
	const auto found = std::find(
		level.monsters_registered.begin(), end, self);
	if (found != end) {
		std::move(found + 1, end, found);
		*(end - 1) = nullptr;
	}
	level.total_monsters--;
}

static USE(turret_driver_use) (gentity_t *self, gentity_t *other,
	gentity_t *activator) -> void
{
	if (!self || !self->inuse)
		return;
	const int32_t self_generation = self->spawn_count;
	if (self->enemy && !turret_reference_is_active(
		self->enemy, self->turret_enemy_generation)) {
		self->enemy = nullptr;
		self->turret_enemy_generation = 0;
	}

	monster_use(self, other, activator);
	if (!turret_entity_lifetime_matches(self, self_generation))
		return;
	self->turret_enemy_generation = self->enemy && self->enemy->inuse
		? self->enemy->spawn_count : 0;
}

static THINK(turret_driver_think) (gentity_t *self) -> void {
	vec3_t target;
	vec3_t dir;

	self->nextthink = level.time + FRAME_TIME_S;
	if (!turret_controller_owns_breach(self, self->target_ent)) {
		gi.Com_Print("WARNING: turret_driver lost its turret breach; removing entity.\n");
		turret_detach_controller(self);
		turret_driver_rollback_registration(self);
		G_FreeEntity(self);
		return;
	}

	if (self->enemy && (!turret_reference_is_active(self->enemy,
		self->turret_enemy_generation) || self->enemy->health <= 0)) {
		self->enemy = nullptr;
		self->turret_enemy_generation = 0;
	}

	if (!self->enemy) {
		const int32_t self_generation = self->spawn_count;
		if (!FindTarget(self) ||
			!turret_entity_lifetime_matches(self, self_generation) ||
			!self->enemy || !self->enemy->inuse ||
			(self->enemy->client &&
				!self->enemy->client->pers.connected)) {
			if (turret_entity_lifetime_matches(self, self_generation)) {
				self->enemy = nullptr;
				self->turret_enemy_generation = 0;
			}
			return;
		}
		self->turret_enemy_generation = self->enemy->spawn_count;
		self->monsterinfo.trail_time = level.time;
		self->monsterinfo.aiflags &= ~AI_LOST_SIGHT;
	} else {
		if (visible(self, self->enemy)) {
			if (self->monsterinfo.aiflags & AI_LOST_SIGHT) {
				self->monsterinfo.trail_time = level.time;
				self->monsterinfo.aiflags &= ~AI_LOST_SIGHT;
			}
		} else {
			self->monsterinfo.aiflags |= AI_LOST_SIGHT;
			return;
		}
	}

	gentity_t *const enemy = self->enemy;
	if (!enemy)
		return;

	// let the turret know where we want it to aim
	target = enemy->s.origin;
	target[2] += enemy->viewheight;
	dir = target - self->target_ent->s.origin;
	self->target_ent->move_angles = vectoangles(dir);

	// decide if we should shoot
	if (level.time < self->monsterinfo.attack_finished)
		return;

	gtime_t reaction_time = gtime_t::from_sec(3 - skill->integer);
	if ((level.time - self->monsterinfo.trail_time) < reaction_time)
		return;

	self->monsterinfo.attack_finished = level.time + reaction_time + 1_sec;
	// FIXME how do we really want to pass this along?
	self->target_ent->spawnflags |= SPAWNFLAG_TURRET_BREACH_FIRE;
}

static THINK(turret_driver_link) (gentity_t *self) -> void {
	vec3_t	 vec;

	self->think = turret_driver_think;
	self->nextthink = level.time + FRAME_TIME_S;

	gentity_t *const target_ent = G_PickTarget(self->target);
	if (!turret_attach_controller(self, target_ent)) {
		gi.Com_PrintFmt(
			"WARNING: {} has no available turret breach for target '{}'; removing entity.\n",
			self->classname ? self->classname : "turret_driver",
			self->target ? self->target : "");
		turret_driver_rollback_registration(self);
		G_FreeEntity(self);
		return;
	}
	self->target_ent->moveinfo.blocked = turret_blocked;
	self->takedamage = true;
	self->s.angles = self->target_ent->s.angles;

	vec[0] = self->target_ent->s.origin[0] - self->s.origin[0];
	vec[1] = self->target_ent->s.origin[1] - self->s.origin[1];
	vec[2] = 0;
	self->move_origin[0] = vec.length();

	vec = self->s.origin - self->target_ent->s.origin;
	vec = vectoangles(vec);
	AnglesNormalize(vec);
	self->move_origin[1] = vec[1];

	self->move_origin[2] = self->s.origin[2] - self->target_ent->s.origin[2];

}

void InfantryPrecache();

void SP_turret_driver(gentity_t *self) {
	if (deathmatch->integer) {
		G_FreeEntity(self);
		return;
	}
	if (self->team) {
		gi.Com_PrintFmt(
			"WARNING: turret_driver must not be part of team '{}'; removing entity.\n",
			self->team);
		G_FreeEntity(self);
		return;
	}

	InfantryPrecache();

	self->movetype = MOVETYPE_PUSH;
	self->solid = SOLID_BBOX;
	self->s.modelindex = gi.modelindex("models/monsters/infantry/tris.md2");
	self->mins = { -16, -16, -24 };
	self->maxs = { 16, 16, 32 };

	self->health = self->max_health = 100;
	self->gib_health = GIB_HEALTH;
	self->mass = 200;
	self->viewheight = 24;

	self->pain = infantry_pain;
	self->die = turret_driver_die;
	self->monsterinfo.stand = infantry_stand;

	self->flags |= FL_NO_KNOCKBACK;
	const int monster_index = max(0, level.total_monsters);
	if (g_debug_monster_kills->integer) {
		if (static_cast<size_t>(monster_index) <
			level.monsters_registered.size())
			level.monsters_registered[monster_index] = self;
		else if (static_cast<size_t>(monster_index) ==
			level.monsters_registered.size())
			gi.Com_Print("turret_driver: debug monster registry full; later monsters will not be tracked.\n");
	}
	if (level.total_monsters < std::numeric_limits<int>::max())
		level.total_monsters = monster_index + 1;

	self->svflags |= SVF_MONSTER;
	self->use = turret_driver_use;
	self->clipmask = MASK_MONSTERSOLID;
	self->s.old_origin = self->s.origin;
	self->monsterinfo.aiflags |= AI_STAND_GROUND;
	self->monsterinfo.setskin = infantry_setskin;

	if (st.item) {
		self->item = FindItemByClassname(st.item);
		if (!self->item)
			gi.Com_PrintFmt("{}: bad item: {}\n", *self, st.item);
	}

	self->think = turret_driver_link;
	self->nextthink = level.time + FRAME_TIME_S;

	gi.linkentity(self);
}

// invisible turret drivers so we can have unmanned turrets.
// originally designed to shoot at func_trains and such, so they
// fire at the center of the bounding box, rather than the entity's
// origin.

constexpr spawnflags_t SPAWNFLAG_TURRET_BRAIN_IGNORE_SIGHT = 1_spawnflag;

static THINK(turret_brain_think) (gentity_t *self) -> void {
	vec3_t	target;
	vec3_t	dir;
	vec3_t	endpos;
	trace_t trace;

	self->nextthink = level.time + FRAME_TIME_S;
	if (!turret_controller_owns_breach(self, self->target_ent)) {
		gi.Com_Print(
			"WARNING: turret_invisible_brain lost its turret breach; removing entity.\n");
		turret_detach_controller(self);
		G_FreeEntity(self);
		return;
	}

	if (self->enemy) {
		if (!turret_entity_lifetime_matches(
				self->enemy, self->turret_enemy_generation)) {
			self->enemy = nullptr;
			self->turret_enemy_generation = 0;
		} else if (self->enemy->client &&
			!self->enemy->client->pers.connected) {
			// Loadgame clients are temporarily disconnected. Preserve the stamped
			// target so the turret can resume after that same client reconnects.
			return;
		} else if (self->enemy->takedamage && self->enemy->health <= 0) {
			self->enemy = nullptr;
			self->turret_enemy_generation = 0;
		}
	}

	// Invisible turret brains are wired to one fixed killtarget and intentionally
	// do not run monster target acquisition.  FindTarget can call HuntTarget,
	// which requires movement callbacks this stationary brain does not have.
	if (!self->enemy)
		return;

	endpos = self->enemy->absmax + self->enemy->absmin;
	endpos *= 0.5f;

	if (!self->spawnflags.has(SPAWNFLAG_TURRET_BRAIN_IGNORE_SIGHT)) {
		trace = gi.traceline(self->target_ent->s.origin, endpos, self->target_ent, MASK_SHOT);
		if (trace.fraction == 1 || trace.ent == self->enemy) {
			if (self->monsterinfo.aiflags & AI_LOST_SIGHT) {
				self->monsterinfo.trail_time = level.time;
				self->monsterinfo.aiflags &= ~AI_LOST_SIGHT;
			}
		} else {
			self->monsterinfo.aiflags |= AI_LOST_SIGHT;
			return;
		}
	}

	// let the turret know where we want it to aim
	target = endpos;
	dir = target - self->target_ent->s.origin;
	self->target_ent->move_angles = vectoangles(dir);

	// decide if we should shoot
	if (level.time < self->monsterinfo.attack_finished)
		return;

	gtime_t reaction_time;

	if (self->delay)
		reaction_time = gtime_t::from_sec(self->delay);
	else
		reaction_time = gtime_t::from_sec(3 - skill->integer);

	if ((level.time - self->monsterinfo.trail_time) < reaction_time)
		return;

	self->monsterinfo.attack_finished = level.time + reaction_time + 1_sec;
	// FIXME how do we really want to pass this along?
	self->target_ent->spawnflags |= SPAWNFLAG_TURRET_BREACH_FIRE;
}

// =================
// =================
static THINK(turret_brain_link) (gentity_t *self) -> void {
	vec3_t	 vec;

	if (self->killtarget) {
		self->enemy = G_PickTarget(self->killtarget);
		self->turret_enemy_generation = self->enemy
			? self->enemy->spawn_count : 0;
	}

	self->think = turret_brain_think;
	self->nextthink = level.time + FRAME_TIME_S;

	gentity_t *const target_ent = G_PickTarget(self->target);
	if (!turret_attach_controller(self, target_ent)) {
		gi.Com_PrintFmt(
			"WARNING: {} has no available turret breach for target '{}'; removing entity.\n",
			self->classname ? self->classname : "turret_invisible_brain",
			self->target ? self->target : "");
		G_FreeEntity(self);
		return;
	}
	self->target_ent->moveinfo.blocked = turret_blocked;
	self->s.angles = self->target_ent->s.angles;

	vec[0] = self->target_ent->s.origin[0] - self->s.origin[0];
	vec[1] = self->target_ent->s.origin[1] - self->s.origin[1];
	vec[2] = 0;
	self->move_origin[0] = vec.length();

	vec = self->s.origin - self->target_ent->s.origin;
	vec = vectoangles(vec);
	AnglesNormalize(vec);
	self->move_origin[1] = vec[1];

	self->move_origin[2] = self->s.origin[2] - self->target_ent->s.origin[2];

	// Pass the activator along to the linked turret parts.
	gentity_t *ent = self->teammaster;
	size_t remaining = turret_team_search_limit();
	bool found_controller = false;
	while (ent && remaining > 0) {
		--remaining;
		if (!turret_entity_lifetime_matches(self->teammaster,
				self->turret_master_generation) ||
			!turret_is_reciprocal_team_member(ent, self->teammaster))
			break;
		if (ent == self) {
			found_controller = true;
			break;
		}
		ent->activator = self->activator;
		ent->turret_activator_generation = self->turret_activator_generation;
		ent = ent->teamchain;
	}
	if (!found_controller) {
		gi.Com_Print("WARNING: turret_invisible_brain found a corrupt turret team; removing entity.\n");
		turret_detach_controller(self);
		G_FreeEntity(self);
	}
}

// =================
// =================
static USE(turret_brain_deactivate) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	self->think = nullptr;
	self->nextthink = 0_ms;
	self->activator = nullptr;
	self->turret_activator_generation = 0;
}

// =================
// =================
static USE(turret_brain_activate) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	if (!self->enemy) {
		self->enemy = activator;
		self->turret_enemy_generation = activator
			? activator->spawn_count : 0;
	}

	// wait at least 3 seconds to fire.
	if (self->wait)
		self->monsterinfo.attack_finished = level.time + gtime_t::from_sec(self->wait);
	else
		self->monsterinfo.attack_finished = level.time + 3_sec;
	self->use = turret_brain_deactivate;

	// Paril NOTE: rhangar1 has a turret_invisible_brain that breaks the
	// hangar ceiling; once the final rocket explodes the barrier,
	// it attempts to print "Barrier neutralized." to the rocket owner
	// who happens to be this brain rather than the player that activated
	// the turret. this resolves this by passing it along to fire_rocket.
	self->activator = activator;
	self->turret_activator_generation = activator
		? activator->spawn_count : 0;

	self->think = turret_brain_link;
	self->nextthink = level.time + FRAME_TIME_S;
}

/*QUAKED turret_invisible_brain (1 .5 0) (-16 -16 -16) (16 16 16) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Invisible brain to drive the turret.

Does not search for targets. If targeted, can only be turned on once
and then off once. After that they are completely disabled.

"delay" the delay between firing (default ramps for skill level)
"Target" the turret breach
"Killtarget" the item you want it to attack.
Target the brain if you want it activated later, instead of immediately. It will wait 3 seconds
before firing to acquire the target.
*/
void SP_turret_invisible_brain(gentity_t *self) {
	if (self->team) {
		gi.Com_PrintFmt(
			"WARNING: turret_invisible_brain must not be part of team '{}'; removing entity.\n",
			self->team);
		G_FreeEntity(self);
		return;
	}
	if (!self->killtarget) {
		gi.Com_Print("turret_invisible_brain with no killtarget!\n");
		G_FreeEntity(self);
		return;
	}
	if (!self->target) {
		gi.Com_Print("turret_invisible_brain with no target!\n");
		G_FreeEntity(self);
		return;
	}

	if (self->targetname) {
		self->use = turret_brain_activate;
	} else {
		self->think = turret_brain_link;
		self->nextthink = level.time + FRAME_TIME_S;
	}

	self->movetype = MOVETYPE_PUSH;
	gi.linkentity(self);
}

static bool turret_restore_lifetime_reference(gentity_t *&reference,
	int32_t &generation, bool migrate_legacy_stamp) noexcept
{
	if (!reference) {
		generation = 0;
		return true;
	}
	if (!reference->inuse) {
		reference = nullptr;
		generation = 0;
		return false;
	}
	if (migrate_legacy_stamp) {
		generation = reference->spawn_count;
		return true;
	}
	if (turret_entity_lifetime_matches(reference, generation))
		return true;
	reference = nullptr;
	generation = 0;
	return false;
}

static bool turret_restore_breach_graph(
	gentity_t *breach, bool migrate_legacy_stamps) noexcept
{
	if (migrate_legacy_stamps) {
		if (!turret_capture_breach_team(breach))
			return false;
	} else if (!turret_has_valid_breach_target(breach)) {
		return false;
	}

	gentity_t *const attached = turret_find_attached_controller(breach);
	gentity_t *const breach_owner = breach->owner;
	gentity_t *const master_owner = breach->teammaster->owner;
	if (!breach_owner && !master_owner)
		return attached == nullptr;
	if (!breach_owner || breach_owner != master_owner ||
		!turret_controller_structurally_owns_breach(breach_owner, breach))
		return false;

	if (migrate_legacy_stamps) {
		turret_stamp_controller_relationship(breach_owner, breach);
		return true;
	}
	return turret_controller_owns_breach(breach_owner, breach);
}

static void turret_fail_closed_restored_graph(
	gentity_t *breach, bool trust_saved_stamps) noexcept
{
	if (!breach)
		return;

	gentity_t *const master = breach->teammaster;
	const bool structural_master = turret_has_structural_breach_team(breach);
	const bool stamped_master = trust_saved_stamps &&
		turret_entity_lifetime_matches(
			master, breach ? breach->turret_master_generation : 0);
	const bool may_mutate_master = structural_master || stamped_master;
	if (may_mutate_master && turret_is_team_master(master))
		turret_quiesce_team(master);
	turret_disable_breach(breach);

	// A fully reciprocal tail is safe to use only for teardown. Stamp that
	// structural relationship long enough to remove the controller cleanly;
	// never make it authoritative for gameplay after a failed V2 validation.
	gentity_t *const controller = turret_find_attached_controller(breach);
	if (controller && master && may_mutate_master) {
		breach->owner = controller;
		master->owner = controller;
		turret_stamp_controller_relationship(controller, breach);
		turret_detach_controller(controller);
	} else {
		breach->owner = nullptr;
		if (master && may_mutate_master && turret_is_team_master(master))
			master->owner = nullptr;
		breach->turret_controller_generation = 0;
	}
	breach->turret_master_generation = 0;
}

void G_TurretRestoreEntityReferences(bool migrate_legacy_stamps)
{
	const size_t entity_count = turret_team_search_limit();
	for (size_t index = 0; index < entity_count; ++index) {
		gentity_t *const entity = &g_entities[index];
		if (!entity->inuse || !entity->classname)
			continue;
		const bool turret_part = strcmp(entity->classname, "turret_base") == 0 ||
			strcmp(entity->classname, "turret_breach") == 0 ||
			turret_is_controller(entity);
		if (!turret_part)
			continue;

		(void) turret_restore_lifetime_reference(entity->activator,
			entity->turret_activator_generation, migrate_legacy_stamps);
		if (turret_is_controller(entity))
			(void) turret_restore_lifetime_reference(entity->enemy,
				entity->turret_enemy_generation, migrate_legacy_stamps);
	}

	for (size_t index = 0; index < entity_count; ++index) {
		gentity_t *const breach = &g_entities[index];
		if (!breach->inuse || !breach->classname ||
			strcmp(breach->classname, "turret_breach") != 0)
			continue;
		if (!turret_restore_breach_graph(breach, migrate_legacy_stamps)) {
			gi.Com_Print(
				"WARNING: restored turret graph failed identity validation; disabling it.\n");
			turret_fail_closed_restored_graph(
				breach, !migrate_legacy_stamps);
		}
	}
}
