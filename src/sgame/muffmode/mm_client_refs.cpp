// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_client_refs.h"

namespace {

gentity_t *OwnedPlayerNoise(gentity_t *candidate, gentity_t *departing)
{
	if (!candidate || candidate == departing || !candidate->inuse ||
		candidate->owner != departing || !candidate->classname ||
		strcmp(candidate->classname, "player_noise") != 0) {
		return nullptr;
	}

	return candidate;
}

bool IsUsableMonsterTarget(gentity_t *target)
{
	return target && target->inuse && target->health > 0 &&
		(!target->client || target->client->pers.connected);
}

void TransitionMonsterAfterTargetDeparture(gentity_t *monster)
{
	monster->monsterinfo.aiflags &= ~(
		AI_SOUND_TARGET | AI_MEDIC | AI_FORGET_ENEMY |
		AI_TARGET_ANGER | AI_CHARGING);
	monster->monsterinfo.close_sight_tripped = false;
	monster->monsterinfo.attack_state = AS_STRAIGHT;

	if (monster->health <= 0 || monster->deadflag)
		return;

	if (IsUsableMonsterTarget(monster->oldenemy)) {
		monster->enemy = monster->oldenemy;
		monster->oldenemy = nullptr;
		HuntTarget(monster);
		return;
	}
	monster->oldenemy = nullptr;

	if (IsUsableMonsterTarget(monster->monsterinfo.last_player_enemy)) {
		monster->enemy = monster->monsterinfo.last_player_enemy;
		monster->monsterinfo.last_player_enemy = nullptr;
		HuntTarget(monster);
		return;
	}
	monster->monsterinfo.last_player_enemy = nullptr;

	// A combat point or hint path is independent of the departed player. Keep
	// following it when its entity survived the reference cleanup.
	if (!monster->goalentity && monster->movetarget)
		monster->goalentity = monster->movetarget;
	if (monster->goalentity) {
		if ((monster->monsterinfo.aiflags & (AI_COMBAT_POINT | AI_HINT_PATH)) &&
			monster->monsterinfo.run) {
			monster->monsterinfo.run(monster);
		} else if (monster->monsterinfo.walk) {
			monster->monsterinfo.walk(monster);
		} else if (monster->monsterinfo.run) {
			monster->monsterinfo.run(monster);
		}
		return;
	}

	monster->monsterinfo.pausetime = HOLD_FOREVER;
	if (monster->monsterinfo.aiflags & AI_TEMP_STAND_GROUND)
		monster->monsterinfo.aiflags &=
			~(AI_STAND_GROUND | AI_TEMP_STAND_GROUND);
	// The animation callback is engine-owned code and may synchronously change
	// entity lifetime; leave no state access after invoking it.
	if (monster->monsterinfo.stand)
		monster->monsterinfo.stand(monster);
}

void ClearMonsterClientLifetimeReferences(
	gentity_t *monster,
	gentity_t *departing,
	gentity_t *noise_primary,
	gentity_t *noise_secondary)
{
	const auto clear = MM_MonsterClientReferenceClearPolicy(
		mm_monster_client_reference_set_t<gentity_t> {
			monster->enemy,
			monster->oldenemy,
			monster->goalentity,
			monster->movetarget,
			monster->monsterinfo.last_player_enemy,
			monster->monsterinfo.damage_attacker,
			monster->monsterinfo.damage_inflictor,
			monster->groundentity
		},
		departing,
		noise_primary,
		noise_secondary);

	if (clear.old_enemy)
		monster->oldenemy = nullptr;
	if (clear.last_player_enemy)
		monster->monsterinfo.last_player_enemy = nullptr;
	if (clear.goal_entity)
		monster->goalentity = nullptr;
	if (clear.move_target) {
		monster->movetarget = nullptr;
		monster->monsterinfo.aiflags &= ~AI_COMBAT_POINT;
		if (monster->monsterinfo.aiflags & AI_HINT_PATH) {
			monster->monsterinfo.aiflags &= ~AI_HINT_PATH;
			monster->monsterinfo.goal_hint = nullptr;
		}
	}
	if (clear.damage_attacker)
		monster->monsterinfo.damage_attacker = world;
	if (clear.damage_inflictor)
		monster->monsterinfo.damage_inflictor = world;
	if (clear.ground_entity) {
		monster->groundentity = nullptr;
		monster->groundentity_linkcount = 0;
	}

	if (clear.current_enemy) {
		monster->enemy = nullptr;
		TransitionMonsterAfterTargetDeparture(monster);
	} else if (clear.goal_entity && IsUsableMonsterTarget(monster->enemy)) {
		// A malformed mixed state can retain a valid enemy while its goal points
		// at the departing slot. Re-establish the normal enemy/goal pairing.
		HuntTarget(monster);
	}
}

void ClearDepartingNoiseClientReferences(
	gentity_t *departing,
	gentity_t *noise_primary,
	gentity_t *noise_secondary)
{
	if (!departing->client)
		return;

	if (MM_ClientLifetimeReferenceMatches(
			departing->client->sound_entity,
			departing,
			noise_primary,
			noise_secondary)) {
		departing->client->sound_entity = nullptr;
		departing->client->sound_entity_time = 0_ms;
	}
	if (MM_ClientLifetimeReferenceMatches(
			departing->client->sound2_entity,
			departing,
			noise_primary,
			noise_secondary)) {
		departing->client->sound2_entity = nullptr;
		departing->client->sound2_entity_time = 0_ms;
	}
}

void RetirePlayerNoise(gentity_t *noise, gentity_t *departing)
{
	if (!noise || !noise->inuse || noise->owner != departing)
		return;

	noise->owner = nullptr;
	G_FreeEntity(noise);
}

} // namespace

void MM_ClearDepartingClientReferences(gentity_t *departing)
{
	if (!departing)
		return;

	// Player-noise entities are aliases of the owning client lifetime for AI
	// targeting. Only accept the live, correctly-owned entities so a stale
	// mynoise pointer can never retire an unrelated reused slot.
	gentity_t *noise_primary = OwnedPlayerNoise(departing->mynoise, departing);
	gentity_t *noise_secondary = OwnedPlayerNoise(departing->mynoise2, departing);
	if (noise_secondary == noise_primary)
		noise_secondary = nullptr;

	for (gentity_t *viewer : active_clients()) {
		const auto clear = MM_ClientReferenceClearPolicy(
			viewer->client->follow_target,
			viewer->client->follow_queued_target,
			departing);

		if (clear.queued_follow) {
			viewer->client->follow_queued_target = nullptr;
			viewer->client->follow_queued_time = 0_sec;
		}

		if (clear.active_follow)
			FreeFollower(viewer);

		if (viewer->groundentity == departing) {
			viewer->groundentity = nullptr;
			viewer->groundentity_linkcount = 0;
		}
		if (viewer->client->oldgroundentity == departing)
			viewer->client->oldgroundentity = nullptr;
	}

	const size_t entity_limit = std::min(
		static_cast<size_t>(globals.num_entities),
		static_cast<size_t>(game.maxentities));
	const size_t first_non_client = std::min(
		entity_limit,
		static_cast<size_t>(game.maxclients) + 1);
	for (size_t i = first_non_client; i < entity_limit; i++) {
		gentity_t *candidate = &g_entities[i];
		if (!candidate->inuse)
			continue;
		// Map entities such as clocks and buttons can retain their use activator
		// for an arbitrary delay. Clear that alias before the client slot can be
		// reconnected or reused; world preserves running/on sentinels and gives
		// direct use callbacks a safe neutral source.
		if (candidate->activator == departing)
			candidate->activator = world;
		if (candidate->owner == departing && candidate->classname &&
			strcmp(candidate->classname, "item_health_mega") == 0) {
			candidate->owner = nullptr;
			candidate->megahealth_owner_generation = 0;
		}
		if (!(candidate->svflags & SVF_MONSTER))
			continue;
		ClearMonsterClientLifetimeReferences(
			candidate, departing, noise_primary, noise_secondary);
	}

	ClearDepartingNoiseClientReferences(
		departing, noise_primary, noise_secondary);
	departing->mynoise = nullptr;
	departing->mynoise2 = nullptr;
	RetirePlayerNoise(noise_primary, departing);
	RetirePlayerNoise(noise_secondary, departing);

	// The disguised-player noise grace period is another delayed client-slot
	// reference. Do not let a replacement client inherit the old noise event.
	if (level.disguise_violator == departing) {
		level.disguise_violator = nullptr;
		level.disguise_violation_time = 0_ms;
	}
}
