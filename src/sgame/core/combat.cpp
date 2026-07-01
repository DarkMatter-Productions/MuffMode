// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Damage, death and combat rule helpers.

#include "g_local.h"
#include "muffmode/mm_combat_heatmap.h"
#include "muffmode/mm_freezetag.h"

/*
============
CanDamage

Returns true if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
bool CanDamage(gentity_t *targ, gentity_t *inflictor) {
	vec3_t	dest;
	trace_t trace;

	// bmodels need special checking because their origin is 0,0,0
	vec3_t inflictor_center;

	if (inflictor->linked)
		inflictor_center = (inflictor->absmin + inflictor->absmax) * 0.5f;
	else
		inflictor_center = inflictor->s.origin;

	if (targ->solid == SOLID_BSP) {
		dest = closest_point_to_box(inflictor_center, targ->absmin, targ->absmax);

		trace = gi.traceline(inflictor_center, dest, inflictor, MASK_SOLID);
		if (trace.fraction == 1.0f)
			return true;
	}

	vec3_t targ_center;

	if (targ->linked)
		targ_center = (targ->absmin + targ->absmax) * 0.5f;
	else
		targ_center = targ->s.origin;

	trace = gi.traceline(inflictor_center, targ_center, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0f)
		return true;

	dest = targ_center;
	dest[0] += 15.0f;
	dest[1] += 15.0f;
	trace = gi.traceline(inflictor_center, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0f)
		return true;

	dest = targ_center;
	dest[0] += 15.0f;
	dest[1] -= 15.0f;
	trace = gi.traceline(inflictor_center, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0f)
		return true;

	dest = targ_center;
	dest[0] -= 15.0f;
	dest[1] += 15.0f;
	trace = gi.traceline(inflictor_center, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0f)
		return true;

	dest = targ_center;
	dest[0] -= 15.0f;
	dest[1] -= 15.0f;
	trace = gi.traceline(inflictor_center, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0f)
		return true;

	return false;
}

/*
============
Killed
============
*/
void Killed(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, mod_t mod) {
	if (targ->health < -999)
		targ->health = -999;

	// [Paril-KEX]
	if ((targ->svflags & SVF_MONSTER) && targ->monsterinfo.aiflags & AI_MEDIC) {
		if (targ->enemy && targ->enemy->inuse && (targ->enemy->svflags & SVF_MONSTER)) // god, I hope so
			cleanupHealTarget(targ->enemy);

		// clean up self
		targ->monsterinfo.aiflags &= ~AI_MEDIC;
	}

	targ->enemy = attacker;
	targ->lastMOD = mod;

	// [Paril-KEX] monsters call die in their damage handler
	if (targ->svflags & SVF_MONSTER)
		return;

	targ->die(targ, inflictor, attacker, damage, point, mod);

	if (targ->monsterinfo.setskin)
		targ->monsterinfo.setskin(targ);
}

/*
================
SpawnDamage
================
*/
void SpawnDamage(int type, const vec3_t &origin, const vec3_t &normal, int damage) {
	if (damage > 255)
		damage = 255;
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(type);
	gi.WritePosition(origin);
	gi.WriteDir(normal);
	gi.multicast(origin, MULTICAST_PVS, false);
}

/*
============
T_Damage

targ		entity that is being damaged
inflictor	entity that is causing the damage
attacker	entity that caused the inflictor to damage targ
	example: targ=monster, inflictor=rocket, attacker=player

dir			direction of the attack
point		point at which the damage is being inflicted
normal		normal vector from that point
damage		amount of damage being inflicted
knockback	force to be applied against targ as a result of the damage

dflags		these flags are used to control how T_Damage works
	DAMAGE_RADIUS			damage was indirect (from a nearby explosion)
	DAMAGE_NO_ARMOR			armor does not protect from this damage
	DAMAGE_ENERGY			damage is from an energy based weapon
	DAMAGE_NO_KNOCKBACK		do not affect velocity, just view angles
	DAMAGE_BULLET			damage is from a bullet (used for ricochets)
	DAMAGE_NO_PROTECTION	kills godmode, armor, everything
============
*/
static int CheckPowerArmor(gentity_t *ent, const vec3_t &point, const vec3_t &normal, int damage, damageflags_t dflags) {
	gclient_t *client;
	int		   save;
	item_id_t  power_armor_type;
	int		   damagePerCell;
	int		   pa_te_type;
	int *power;
	int		   power_used;

	if (ent->health <= 0)
		return 0;

	if (!damage)
		return 0;

	client = ent->client;

	if (dflags & (DAMAGE_NO_ARMOR | DAMAGE_NO_POWER_ARMOR))
		return 0;

	if (client) {
		power_armor_type = PowerArmorType(ent);
		power = &client->pers.inventory[IT_AMMO_CELLS];
	} else if (ent->svflags & SVF_MONSTER) {
		power_armor_type = ent->monsterinfo.power_armor_type;
		power = &ent->monsterinfo.power_armor_power;
	} else
		return 0;

	if (power_armor_type == IT_NULL)
		return 0;
	if (!*power)
		return 0;

	if (power_armor_type == IT_POWER_SCREEN) {
		vec3_t vec;
		float  dot;
		vec3_t forward;

		// only works if damage point is in front
		AngleVectors(ent->s.angles, forward, nullptr, nullptr);
		vec = point - ent->s.origin;
		vec.normalize();
		dot = vec.dot(forward);
		if (dot <= 0.3f)
			return 0;

		damagePerCell = 1;
		pa_te_type = TE_SCREEN_SPARKS;
		damage = damage / 3;
	} else {
		if (deathmatch->integer)
			damagePerCell = !!(RS(RS_MM)) ? 1 : GTF(GTF_CTF) ? 1 : 2; // power armor is weaker in DM
		else
			damagePerCell = 2;
		pa_te_type = TE_SCREEN_SPARKS;
		damage = (2 * damage) / 3;
	}

	// Paril: fix small amounts of damage not
	// being absorbed
	damage = max(1, damage);

	save = *power * damagePerCell;

	if (!save)
		return 0;

	// [Paril-KEX] energy damage should do more to power armor, not ETF Rifle shots.
	if (dflags & DAMAGE_ENERGY)
		save = max(1, save / 2);

	if (save > damage)
		save = damage;

	// [Paril-KEX] energy damage should do more to power armor, not ETF Rifle shots.
	if (dflags & DAMAGE_ENERGY)
		power_used = (save / damagePerCell) * 2;
	else
		power_used = save / damagePerCell;

	power_used = max(1, power_used);

	SpawnDamage(pa_te_type, point, normal, save);
	ent->powerarmor_time = level.time + 200_ms;

	// Paril: adjustment so that power armor
	// always uses damagePerCell even if it does
	// only a single point of damage
	*power = max(0, *power - max(damagePerCell, power_used));

	// check power armor turn-off states
	if (ent->client)
		G_CheckPowerArmor(ent);
	else if (!*power) {
		gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/mon_power2.wav"), 1.f, ATTN_NORM, 0.f);

		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_POWER_SPLASH);
		gi.WriteEntity(ent);
		gi.WriteByte((power_armor_type == IT_POWER_SCREEN) ? 1 : 0);
		gi.multicast(ent->s.origin, MULTICAST_PHS, false);
	}

	return save;
}

static int CheckArmor(gentity_t *ent, const vec3_t &point, const vec3_t &normal, int damage, int te_sparks,
	damageflags_t dflags) {
	gclient_t *client;
	int		   save;
	item_id_t  index;
	gitem_t *armor;
	int *power;

	if (!damage)
		return 0;

	if (dflags & (DAMAGE_NO_ARMOR | DAMAGE_NO_REG_ARMOR))
		return 0;

	client = ent->client;
	index = ArmorIndex(ent);

	if (!index)
		return 0;

	armor = GetItemByIndex(index);

	if (dflags & DAMAGE_ENERGY)
		save = (int)ceilf(RS(RS_Q3A) ? 0.66f * damage : armor->armor_info->energy_protection * damage);
	else
		save = (int)ceilf(RS(RS_Q3A) ? 0.66f * damage : armor->armor_info->normal_protection * damage);

	if (client)
		power = &client->pers.inventory[index];
	else
		power = &ent->monsterinfo.armor_power;

	if (save >= *power)
		save = *power;

	if (!save)
		return 0;

	*power -= save;

	if (!client && !ent->monsterinfo.armor_power)
		ent->monsterinfo.armor_type = IT_NULL;

	SpawnDamage(te_sparks, point, normal, save);

	return save;
}

static void M_ReactToDamage(gentity_t *targ, gentity_t *attacker, gentity_t *inflictor) {
	if (!(attacker->client) && !(attacker->svflags & SVF_MONSTER))
		return;

	// logic for tesla - if you are hit by a tesla, and can't see who you should be mad at (attacker)
	// attack the tesla
	// also, target the tesla if it's a "new" tesla
	if ((inflictor) && (!strcmp(inflictor->classname, "tesla_mine"))) {
		if ((MarkTeslaArea(targ, inflictor) || brandom()) && (!targ->enemy || !targ->enemy->classname || strcmp(targ->enemy->classname, "tesla_mine")))
			TargetTesla(targ, inflictor);
		return;
	}

	if (attacker == targ || attacker == targ->enemy)
		return;

	// if we are a good guy monster and our attacker is a player
	// or another good guy, do not get mad at them
	if (targ->monsterinfo.aiflags & AI_GOOD_GUY) {
		if (attacker->client || (attacker->monsterinfo.aiflags & AI_GOOD_GUY))
			return;
	}

	//  if we're currently mad at something a target_anger made us mad at, ignore
	//  damage
	if (targ->enemy && targ->monsterinfo.aiflags & AI_TARGET_ANGER) {
		float percentHealth;

		// make sure whatever we were pissed at is still around.
		if (targ->enemy->inuse) {
			percentHealth = (float)(targ->health) / (float)(targ->max_health);
			if (targ->enemy->inuse && percentHealth > 0.33f)
				return;
		}

		// remove the target anger flag
		targ->monsterinfo.aiflags &= ~AI_TARGET_ANGER;
	}

	// we recently switched from reacting to damage, don't do it
	if (targ->monsterinfo.react_to_damage_time > level.time)
		return;

	// if we're healing someone, do like above and try to stay with them
	if ((targ->enemy) && (targ->monsterinfo.aiflags & AI_MEDIC)) {
		float percentHealth;

		percentHealth = (float)(targ->health) / (float)(targ->max_health);
		// ignore it some of the time
		if (targ->enemy->inuse && percentHealth > 0.25f)
			return;

		// remove the medic flag
		cleanupHealTarget(targ->enemy);
		targ->monsterinfo.aiflags &= ~AI_MEDIC;
	}

	// we now know that we are not both good guys
	targ->monsterinfo.react_to_damage_time = level.time + random_time(3_sec, 5_sec);

	// if attacker is a client, get mad at them because he's good and we're not
	if (attacker->client) {
		targ->monsterinfo.aiflags &= ~AI_SOUND_TARGET;

		// this can only happen in coop (both new and old enemies are clients)
		// only switch if can't see the current enemy
		if (targ->enemy != attacker) {
			if (targ->enemy && targ->enemy->client) {
				if (visible(targ, targ->enemy)) {
					targ->oldenemy = attacker;
					return;
				}
				targ->oldenemy = targ->enemy;
			}

			// [Paril-KEX]
			if ((targ->svflags & SVF_MONSTER) && targ->monsterinfo.aiflags & AI_MEDIC) {
				if (targ->enemy && targ->enemy->inuse && (targ->enemy->svflags & SVF_MONSTER)) // god, I hope so
				{
					cleanupHealTarget(targ->enemy);
				}

				// clean up self
				targ->monsterinfo.aiflags &= ~AI_MEDIC;
			}

			targ->enemy = attacker;
			if (!(targ->monsterinfo.aiflags & AI_DUCKED))
				FoundTarget(targ);
		}
		return;
	}

	if (attacker->enemy == targ // if they *meant* to shoot us, then shoot back
		// it's the same base (walk/swim/fly) type and both don't ignore shots,
		// get mad at them
		|| (((targ->flags & (FL_FLY | FL_SWIM)) == (attacker->flags & (FL_FLY | FL_SWIM))) &&
			(strcmp(targ->classname, attacker->classname) != 0) && !(attacker->monsterinfo.aiflags & AI_IGNORE_SHOTS) &&
			!(targ->monsterinfo.aiflags & AI_IGNORE_SHOTS))) {
		if (targ->enemy != attacker) {
			// [Paril-KEX]
			if ((targ->svflags & SVF_MONSTER) && targ->monsterinfo.aiflags & AI_MEDIC) {
				if (targ->enemy && targ->enemy->inuse && (targ->enemy->svflags & SVF_MONSTER)) { // god, I hope so
					cleanupHealTarget(targ->enemy);
				}

				// clean up self
				targ->monsterinfo.aiflags &= ~AI_MEDIC;
			}

			if (targ->enemy && targ->enemy->client)
				targ->oldenemy = targ->enemy;
			targ->enemy = attacker;
			if (!(targ->monsterinfo.aiflags & AI_DUCKED))
				FoundTarget(targ);
		}
	}
	// otherwise get mad at whoever they are mad at (help our buddy) unless it is us!
	else if (attacker->enemy && attacker->enemy != targ && targ->enemy != attacker->enemy) {
		if (targ->enemy != attacker->enemy) {
			// [Paril-KEX]
			if ((targ->svflags & SVF_MONSTER) && targ->monsterinfo.aiflags & AI_MEDIC) {
				if (targ->enemy && targ->enemy->inuse && (targ->enemy->svflags & SVF_MONSTER)) // god, I hope so
				{
					cleanupHealTarget(targ->enemy);
				}

				// clean up self
				targ->monsterinfo.aiflags &= ~AI_MEDIC;
			}

			if (targ->enemy && targ->enemy->client)
				targ->oldenemy = targ->enemy;
			targ->enemy = attacker->enemy;
			if (!(targ->monsterinfo.aiflags & AI_DUCKED))
				FoundTarget(targ);
		}
	}
}

// check if the two given entities are on the same team
bool OnSameTeam(gentity_t *ent1, gentity_t *ent2) {
	if (!ent1 || !ent2)
		return false;

	// monsters are never on the same team (except Horde: no monster-vs-monster damage)
	if (!ent1->client || !ent2->client) {
		if (GT(GT_HORDE) && (ent1->svflags & SVF_MONSTER) && (ent2->svflags & SVF_MONSTER))
			return true;
		return false;
	}

	// we're never on our own team
	else if (ent1 == ent2)
		return false;

	if (!ent1->client || !ent2->client)
		return false;

	if (g_quadhog->integer) {
		if (ent1->client->pu_time_quad > level.time || ent2->client->pu_time_quad > level.time)
			return false;
		return true;
	}

	// [Paril-KEX] coop 'team' support
	if (InCoopStyle())
		return ent1->client && ent2->client;
	else if (Teams() && ent1->client && ent2->client) {
		if (ent1->client->sess.team == ent2->client->sess.team)
			return true;
	}

	return false;
}

// check if the two entities are on a team and that
// they wouldn't damage each other
bool CheckTeamDamage(gentity_t *targ, gentity_t *attacker) {
	// always damage teammates if friendly fire is enabled
	if (g_friendly_fire->integer)
		return false;

	return OnSameTeam(targ, attacker);
}

static bool ShouldRecordCombatHeat(const gentity_t *attacker, const gclient_t *target_client) {
	return target_client || (attacker && attacker->client);
}

static vec3_t CombatHeatOrigin(const gentity_t *target, const gentity_t *inflictor, const vec3_t &point) {
	if (point)
		return point;
	if (target && target->client)
		return target->s.origin;
	if (inflictor && inflictor->inuse)
		return inflictor->s.origin;
	return target ? target->s.origin : vec3_origin;
}

void T_Damage(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker, const vec3_t &dir, const vec3_t &point,
	const vec3_t &normal, int damage, int knockback, damageflags_t dflags, mod_t mod) {
	gclient_t *client;
	int			take, save;
	int			asave = 0, psave = 0;
	int			te_sparks;
	bool		sphere_notified;

	if (!targ->takedamage)
		return;

	if (!attacker)
		attacker = world;

	if ((g_instagib->integer || GT(GT_INSTAGIB)) && attacker->client && targ->client) {
		// [Kex] always kill no matter what on instagib
		damage = 9999;
	}
	sphere_notified = false;

	// friendly fire avoidance
	// if enabled you can't hurt teammates (but you can hurt yourself)
	// knockback still occurs
	if ((targ != attacker) && !(dflags & DAMAGE_NO_PROTECTION)) {
		// mark as friendly fire
		if (OnSameTeam(targ, attacker)) {
			mod.friendly_fire = true;

			// if we're not a nuke & friendly fire is disabled, just kill the damage
			if (!g_friendly_fire->integer && (mod.id != MOD_NUKE)) {
				damage = 0;
			}
		}
	}

	// easy mode takes half damage
	if (skill->integer == 0 && !deathmatch->integer && targ->client && damage) {
		damage /= 2;
		if (!damage)
			damage = 1;
	}

	if ((targ->svflags & SVF_MONSTER) != 0) {
		damage *= ai_damage_scale->integer;
	} else {
		damage *= g_damage_scale->integer;
	} // mal: just for debugging...

	// horde champion: tapered outgoing-damage buff (weak champions hit hard, heavy ones store <= 1 = no buff)
	if ((attacker->svflags & SVF_MONSTER) && attacker->monsterinfo.champion_damage_scale > 1.0f)
		damage = (int)ceil(damage * attacker->monsterinfo.champion_damage_scale);

	client = targ->client;

	// PMM - defender sphere takes half damage
	if (damage && (client) && (client->owned_sphere) && (client->owned_sphere->spawnflags == SF_SPHERE_DEFENDER)) {
		damage /= 2;
		if (!damage)
			damage = 1;
	}

	if (dflags & DAMAGE_BULLET)
		te_sparks = TE_BULLET_SPARKS;
	else
		te_sparks = TE_SPARKS;

	// bonus damage for surprising a monster
	if (!(dflags & DAMAGE_RADIUS) && (targ->svflags & SVF_MONSTER) && (attacker->client) &&
		(!targ->enemy || targ->monsterinfo.surprise_time == level.time) && (targ->health > 0)) {
		damage *= 2;
		targ->monsterinfo.surprise_time = level.time;
	}

	// power amplifier tech
	damage = Tech_ApplyPowerAmp(attacker, damage);

	if (RS(RS_Q3A)) {
		knockback = damage;
		if (knockback > 200)
			knockback = 200;
	}

	if ((targ->flags & FL_NO_KNOCKBACK) ||
			((targ->flags & FL_ALIVE_KNOCKBACK_ONLY) && (!targ->deadflag || targ->dead_time != level.time)))
			knockback = 0;

	if (MM_FreezeTag_IsFrozen(targ))
		knockback = static_cast<int>(ceilf(knockback * max(0.0f, g_freezetag_frozen_knockback_scale->value)));

	// figure momentum add
	if (!(dflags & DAMAGE_NO_KNOCKBACK)) {
		if ((knockback) && (targ->movetype != MOVETYPE_NONE) && (targ->movetype != MOVETYPE_BOUNCE) &&
			(targ->movetype != MOVETYPE_PUSH) && (targ->movetype != MOVETYPE_STOP)) {
			vec3_t normalized = dir.normalized();
			vec3_t kvel = {};

			if (RS(RS_Q3A)) {
				if (targ->client)
					kvel = normalized * (1000.0f * knockback / 200.0f);
			} else {
				float  mass;

				if (targ->mass < 50)
					mass = 50;
				else
					mass = (float)targ->mass;

				if (targ->client && attacker == targ)
					kvel = normalized * (1600.0f * knockback / mass); // the rocket jump hack...
				else
					kvel = normalized * (500.0f * knockback / mass);
			}

			kvel *= g_knockback_scale->value;

			// arena gives a bit more knockback
			if (!RS(RS_Q3A) && GTF(GTF_ARENA))
				kvel *= 1.25f;

			targ->velocity += kvel;
		}
	}

	if (MM_FreezeTag_IsFrozen(targ)) {
		if (damage > 0 && !(targ->flags & FL_NO_DAMAGE_EFFECTS))
			SpawnDamage(te_sparks, point, normal, damage);
		MM_FreezeTag_OnFrozenDamage(targ, attacker);
		return;
	}

	if (RS(RS_Q3A) && targ == attacker && damage > 0)
		damage = max(1, (int)(damage * 0.5f));

	if (targ != attacker && attacker->client && !OnSameTeam(targ, attacker)) {
		if ((!inflictor->skip && (dflags & DAMAGE_STAT_ONCE)) || !(dflags & DAMAGE_STAT_ONCE)) {
			MS_Adjust(attacker->client, MSTAT_HITS, 1);
			inflictor->skip = true;
		}
	}

	take = damage;
	save = 0;

	if (!(dflags & DAMAGE_NO_PROTECTION)) {
		if (IsCombatDisabled()) {
			take = 0;
			save = damage;
		}

		// instagib railgun splash never inflicts damage
		if (mod.id == MOD_RAILGUN_SPLASH) {
			take = 0;
			save = damage;
		}

		// protection does not take splash damage
		if (client && client->pu_time_protection > level.time && (dflags & DAMAGE_RADIUS)) {
			gi.sound(targ, CHAN_AUX, gi.soundindex("items/protect3.wav"), 1, ATTN_NORM, 0);
			take = 0;
			save = damage;
		}

		// check for godmode
		if (targ->flags & FL_GODMODE) {
			take = 0;
			save = damage;
			SpawnDamage(te_sparks, point, normal, save);
		}

		// check for monster invincibility
		if ((targ->svflags & SVF_MONSTER) && targ->monsterinfo.invincibility_time > level.time) {
			if (targ->pain_debounce_time < level.time) {
				gi.sound(targ, CHAN_ITEM, gi.soundindex("items/protect4.wav"), 1, ATTN_NORM, 0);
				targ->pain_debounce_time = level.time + 2_sec;
			}
			take = 0;
			save = damage;
		}
	}

	// check for getting out of self damage
	if (targ == attacker && deathmatch->integer && g_dm_no_self_damage->integer) {
		take = 0;
		save = damage;
	}

	if (g_vampiric_damage->integer && targ->health > 0 && attacker != targ && !OnSameTeam(targ, attacker) && take > 0) {
		int vtake = take;
		int hmax = clamp(g_vampiric_health_max->integer, 100, 9999);

		if (vtake > targ->health)
			vtake = targ->health;

		vtake = max(1, (int)ceil((float)vtake * g_vampiric_percentile->value));

		if (attacker->health < hmax)
			attacker->health += vtake;
		if (attacker->health > hmax)
			attacker->health = hmax;
	}

	// team armor protect
	if (Teams() && targ->client && attacker->client &&
			targ->client->sess.team == attacker->client->sess.team && targ != attacker &&
			g_teamplay_armor_protect->integer) {
		psave = asave = 0;
	} else {
		if (targ == attacker && GTF(GTF_ARENA) && !g_arena_dmg_armor->integer) {
			take = 0;
			save = damage;
		} else {
			psave = CheckPowerArmor(targ, point, normal, take, dflags);
			take -= psave;

			asave = CheckArmor(targ, point, normal, take, te_sparks, dflags);
			take -= asave;
		}
	}

	// treat cheat/powerup savings the same as armor
	asave += save;

	if (!(dflags & DAMAGE_NO_PROTECTION)) {
		// Arena modes always block self-health damage; g_arena_dmg_armor only
		// affects whether self-hit damage can be absorbed by armor.
		if (targ == attacker && GTF(GTF_ARENA)) {
			take = 0;
			save = 0;	// damage;
		}

		// check for protection powerup
		if (take && client && client->pu_time_protection > level.time) {
			gi.sound(targ, CHAN_AUX, gi.soundindex("items/protect3.wav"), 1, ATTN_NORM, 0);
			take = RS(RS_Q2RE) ? 0 : ceil(take/2);
		}
	}

	// resistance tech
	take = Tech_ApplyDisruptorShield(targ, take);

	CTF_CheckHurtCarrier(targ, attacker);

	// this option will do damage both to the armor and person. originally for DPU rounds
	if ((dflags & DAMAGE_DESTROY_ARMOR)) {
		if (!(targ->flags & FL_GODMODE) && !(dflags & DAMAGE_NO_PROTECTION) &&
				!(client && client->pu_time_protection > level.time)) {
			take = damage;
		}
	}

	if (targ != attacker && attacker->client && targ->health > 0) {
		int stat_take = take;
		if (stat_take > targ->health)
			stat_take = targ->health;

		// arena player scoring: 1 point per 100 damage dealt to opponents, capped to 0 health
		// (Red Rover and LMS score by frags / round-wins only, so they opt out of arena damage-scoring)
		if (GTF(GTF_ARENA) && notGT(GT_RR) && notGT(GT_LMS) && !OnSameTeam(targ, attacker)) {
			attacker->client->pers.dmg_scorer += stat_take + psave + asave;

			if (attacker->client->pers.dmg_scorer >= 100) {
				int32_t score_add = floor(attacker->client->pers.dmg_scorer / 100);
				attacker->client->pers.dmg_scorer -= score_add * 100;

				G_AdjustPlayerScore(attacker->client, score_add, false, 0);
			}
		}

		// Red Rover: tally enemy damage dealt this round for the round-winner tie-break and
		// display. Tracked here rather than via MS_*/match-stats so it counts bots too and
		// works regardless of g_matchstats. Reset each round in CheckDMRoundState.
		if (GT(GT_RR) && !OnSameTeam(targ, attacker))
			attacker->client->resp.round_dmg += stat_take + psave + asave;

		// team damage checks and warnings
		if (OnSameTeam(targ, attacker)) {
			attacker->client->pers.dmg_team += take + psave + asave;

			if (attacker->client->pers.dmg_team >= 100) {
				int32_t score_add = floor(attacker->client->pers.dmg_team / 100);
				attacker->client->pers.dmg_team -= score_add * 100;

				gi.LocClient_Print(attacker, PRINT_CENTER, "You are on {} Team,\nstop attacking your team mates!\n", Teams_TeamName(attacker->client->sess.team));
			}
		}

		// [Paril-KEX] player hit markers
		if (!((targ->svflags & SVF_DEADMONSTER) || (targ->flags & FL_NO_DAMAGE_EFFECTS)) && mod.id != MOD_TARGET_LASER) {
			attacker->client->ps.stats[STAT_HIT_MARKER] += take + psave + asave;
		}
		MS_Adjust(attacker->client, MSTAT_DMG_DEALT, stat_take + psave + asave);

		if (targ->client)
			MS_Adjust(targ->client, MSTAT_DMG_RECEIVED, stat_take + psave + asave);
	}

	// do the damage
	if (take) {
		if (!(targ->flags & FL_NO_DAMAGE_EFFECTS)) {
			if (targ->flags & FL_MECHANICAL)
				SpawnDamage(TE_ELECTRIC_SPARKS, point, normal, take);
			else if ((targ->svflags & SVF_MONSTER) || (client)) {
				if (strcmp(targ->classname, "monster_gekk") == 0)
					SpawnDamage(TE_GREENBLOOD, point, normal, take);
				else if (mod.id == MOD_CHAINFIST)
					SpawnDamage(TE_MOREBLOOD, point, normal, 255);
				else
					SpawnDamage(TE_BLOOD, point, normal, take);
			} else
				SpawnDamage(te_sparks, point, normal, take);
		}

		if (!IsCombatDisabled()) {
			if (ShouldRecordCombatHeat(attacker, client))
				muffmode::combat_heatmap::AddEvent(CombatHeatOrigin(targ, inflictor, point), static_cast<float>(take));
			targ->health = targ->health - take;
		}

		if ((targ->flags & FL_IMMORTAL) && targ->health <= 0)
			targ->health = 1;

		// spheres need to know who to shoot at
		if (client && client->owned_sphere) {
			sphere_notified = true;
			if (client->owned_sphere->pain)
				client->owned_sphere->pain(client->owned_sphere, attacker, 0, 0, mod);
		}

		if (targ->health <= 0) {
			if ((targ->svflags & SVF_MONSTER) || (client)) {
				targ->flags |= FL_ALIVE_KNOCKBACK_ONLY;
				targ->dead_time = level.time;

			}
			targ->monsterinfo.damage_blood += take;
			targ->monsterinfo.damage_attacker = attacker;
			targ->monsterinfo.damage_inflictor = inflictor;
			targ->monsterinfo.damage_from = point;
			targ->monsterinfo.damage_mod = mod;
			targ->monsterinfo.damage_knockback += knockback;
			Killed(targ, inflictor, attacker, take, point, mod);
			return;
		}
	}

	// spheres need to know who to shoot at
	if (!sphere_notified) {
		if (client && client->owned_sphere) {
			sphere_notified = true;
			if (client->owned_sphere->pain)
				client->owned_sphere->pain(client->owned_sphere, attacker, 0, 0, mod);
		}
	}

	if (targ->client)
		targ->client->last_attacker_time = level.time;

	if (targ->svflags & SVF_MONSTER) {
		if (damage > 0) {
			M_ReactToDamage(targ, attacker, inflictor);

			targ->monsterinfo.damage_attacker = attacker;
			targ->monsterinfo.damage_inflictor = inflictor;
			targ->monsterinfo.damage_blood += take;
			targ->monsterinfo.damage_from = point;
			targ->monsterinfo.damage_mod = mod;
			targ->monsterinfo.damage_knockback += knockback;
		}

		if (targ->monsterinfo.setskin)
			targ->monsterinfo.setskin(targ);
	} else if (take && targ->pain)
		targ->pain(targ, attacker, (float)knockback, take, mod);

	// add to the damage inflicted on a player this frame
	// the total will be turned into screen blends and view angle kicks
	// at the end of the frame
	if (client) {
		client->damage_parmor += psave;
		client->damage_armor += asave;
		client->damage_blood += take;
		client->damage_knockback += knockback;
		client->damage_from = point;
		client->last_damage_time = level.time + COOP_DAMAGE_RESPAWN_TIME;

		if (!(dflags & DAMAGE_NO_INDICATOR) && inflictor != world && attacker != world && (take || psave || asave)) {
			damage_indicator_t *indicator = nullptr;
			size_t i;

			for (i = 0; i < client->num_damage_indicators; i++) {
				if ((point - client->damage_indicators[i].from).length() < 32.f) {
					indicator = &client->damage_indicators[i];
					break;
				}
			}

			if (!indicator && i != MAX_DAMAGE_INDICATORS) {
				indicator = &client->damage_indicators[i];
				// for projectile direct hits, use the attacker; otherwise
				// use the inflictor (rocket splash should point to the rocket)
				indicator->from = (dflags & DAMAGE_RADIUS) ? inflictor->s.origin : attacker->s.origin;
				indicator->health = indicator->armor = indicator->power = 0;
				client->num_damage_indicators++;
			}

			if (indicator) {
				indicator->health += take;
				indicator->power += psave;
				indicator->armor += asave;
			}
		}
	}
}

/*
============
T_RadiusDamage
============
*/
void T_RadiusDamage(gentity_t *inflictor, gentity_t *attacker, float damage, gentity_t *ignore, float radius, damageflags_t dflags, mod_t mod) {
	float	 points;
	gentity_t *ent = nullptr;
	vec3_t	 v;
	vec3_t	 dir;
	vec3_t   inflictor_center;

	inflictor_center = inflictor->linked ? ((inflictor->absmax + inflictor->absmin) * 0.5f) : inflictor->s.origin;

	if (RS(RS_Q3A)) {
		size_t entity_count = min(static_cast<size_t>(globals.num_entities), static_cast<size_t>(game.maxentities));
		for (size_t entnum = 1; entnum < entity_count; entnum++) {
			ent = &g_entities[entnum];
			if (!ent->inuse)
				continue;
			if (ent == ignore)
				continue;
			if (!ent->takedamage)
				continue;

			for (int i = 0; i < 3; i++) {
				if (inflictor_center[i] < ent->absmin[i])
					v[i] = ent->absmin[i] - inflictor_center[i];
				else if (inflictor_center[i] > ent->absmax[i])
					v[i] = inflictor_center[i] - ent->absmax[i];
				else
					v[i] = 0;
			}

			float dist = v.length();
			if (dist >= radius)
				continue;

			points = damage * (1.0f - (dist / radius));
			if (points <= 0)
				continue;

			if (CanDamage(ent, inflictor)) {
				dir = ent->s.origin - inflictor_center;
				dir[2] += 24;
				vec3_t normal = dir.normalized();
				T_Damage(ent, inflictor, attacker, dir, closest_point_to_box(inflictor_center, ent->absmin, ent->absmax), normal, (int)points, (int)points, dflags | DAMAGE_RADIUS, mod);
			}
		}

		return;
	}

	while ((ent = findradius(ent, inflictor_center, radius)) != nullptr) {
		if (ent == ignore)
			continue;
		if (!ent->takedamage)
			continue;

		if (ent->solid == SOLID_BSP && ent->linked)
			v = closest_point_to_box(inflictor_center, ent->absmin, ent->absmax);
		else {
			v = ent->mins + ent->maxs;
			v = ent->s.origin + (v * 0.5f);
		}
		v = inflictor_center - v;
		points = damage - 0.5f * v.length();
		if (ent == attacker)
			points *= 0.5f;
		if (points > 0) {
			if (CanDamage(ent, inflictor)) {
				dir = (ent->s.origin - inflictor_center).normalized();
				// [Paril-KEX] use closest point on bbox to explosion position
				// to spawn damage effect

				float kb = points;
				if (mod.id == MOD_HYPERBLASTER)
					kb *= 5;
				// [MuffMode] self rocket-jump: use the ruleset's dedicated knockback base
				// (RS_Q2RE-equivalent) so RJ height matches q2re while self damage stays lower.
				else if (ent == attacker && inflictor->splash_knockback > 0 &&
						 inflictor->owner && inflictor->owner->client)
					kb = ((float)inflictor->splash_knockback - 0.5f * v.length()) * 0.5f;

				T_Damage(ent, inflictor, attacker, dir, closest_point_to_box(inflictor_center, ent->absmin, ent->absmax), dir, (int)points, (int)kb, dflags | DAMAGE_RADIUS, mod);
			}
		}
	}
}

/*
ROGUE
clean up heal targets for medic
*/
void M_SetEffects(gentity_t *self);
void cleanupHealTarget(gentity_t *ent) {
	ent->monsterinfo.healer = nullptr;
	ent->takedamage = true;
	ent->monsterinfo.aiflags &= ~AI_RESURRECTING;
	M_SetEffects(ent);
}

/*
============
T_RadiusNukeDamage

Like T_RadiusDamage, but ignores walls (skips CanDamage check, among others)
// up to KILLZONE radius, do 10,000 points
// after that, do damage linearly out to KILLZONE2 radius
============
*/

void T_RadiusNukeDamage(gentity_t *inflictor, gentity_t *attacker, float damage, gentity_t *ignore, float radius, mod_t mod) {
	float	 points;
	gentity_t *ent = nullptr;
	vec3_t	 v;
	vec3_t	 dir;
	float	 len;
	float	 killzone, killzone2;
	trace_t	 tr;
	float	 dist;

	killzone = radius;
	killzone2 = radius * 2.0f;

	while ((ent = findradius(ent, inflictor->s.origin, killzone2)) != nullptr) {
		// ignore nobody
		if (ent == ignore)
			continue;
		if (!ent->takedamage)
			continue;
		if (!ent->inuse)
			continue;
		if (!(ent->client || (ent->svflags & SVF_MONSTER) || (ent->flags & FL_DAMAGEABLE)))
			continue;

		v = ent->mins + ent->maxs;
		v = ent->s.origin + (v * 0.5f);
		v = inflictor->s.origin - v;
		len = v.length();
		if (len <= killzone) {
			if (ent->client)
				ent->flags |= FL_NOGIB;
			points = 10000;
		} else if (len <= killzone2)
			points = (damage / killzone) * (killzone2 - len);
		else
			points = 0;

		if (points > 0) {
			if (ent->client)
				ent->client->nuke_time = level.time + 2_sec;
			dir = ent->s.origin - inflictor->s.origin;
			T_Damage(ent, inflictor, attacker, dir, inflictor->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, mod);
		}
	}
	ent = g_entities + 1; // skip the worldspawn
	// cycle through players
	while (ent) {
		if ((ent->client) && (ent->client->nuke_time != level.time + 2_sec) && (ent->inuse)) {
			tr = gi.traceline(inflictor->s.origin, ent->s.origin, inflictor, MASK_SOLID);
			if (tr.fraction == 1.0f)
				ent->client->nuke_time = level.time + 2_sec;
			else {
				dist = realrange(ent, inflictor);
				if (dist < 2048)
					ent->client->nuke_time = max(ent->client->nuke_time, level.time + 1.5_sec);
				else
					ent->client->nuke_time = max(ent->client->nuke_time, level.time + 1_sec);
			}
			ent++;
		} else
			ent = nullptr;
	}
}

/*
============
T_RadiusClassDamage

Like T_RadiusDamage, but ignores anything with classname=ignoreClass
============
*/
void T_RadiusClassDamage(gentity_t *inflictor, gentity_t *attacker, float damage, char *ignoreClass, float radius, mod_t mod) {
	float	 points;
	gentity_t *ent = nullptr;
	vec3_t	 v;
	vec3_t	 dir;

	while ((ent = findradius(ent, inflictor->s.origin, radius)) != nullptr) {
		if (ent->classname && !strcmp(ent->classname, ignoreClass))
			continue;
		if (!ent->takedamage)
			continue;

		v = ent->mins + ent->maxs;
		v = ent->s.origin + (v * 0.5f);
		v = inflictor->s.origin - v;
		points = damage - 0.5f * v.length();
		if (ent == attacker)
			points = points * 0.5f;
		if (points > 0) {
			if (CanDamage(ent, inflictor)) {
				dir = ent->s.origin - inflictor->s.origin;
				T_Damage(ent, inflictor, attacker, dir, inflictor->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, mod);
			}
		}
	}
}
