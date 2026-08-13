// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_horde.h"
#include "muffmode/mm_horde_ai.h"
#include "muffmode/mm_horde_ai_rules.h"
#include "muffmode/mm_horde_tables.h"
#include "muffmode/mm_profile.h"
#include "muffmode/mm_ruleset_weapons.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

extern cvar_t *g_horde_enhanced_ai;
extern cvar_t *g_horde_target_spread_weight;
extern cvar_t *g_horde_retarget_interval;
extern cvar_t *g_horde_pursuit;
extern cvar_t *g_horde_pursuit_repath_time;
extern cvar_t *g_horde_target_model;
extern cvar_t *g_horde_target_aggression;
extern cvar_t *g_horde_target_opportunism;
extern cvar_t *g_horde_reach_probe_budget;

namespace muffmode::horde {

namespace {

// Rough average spawn_points per monster body; used to turn wave point budget into an expected kill rate.
constexpr float kAdaptivePointsPerMonster = 2.0f;
// Seconds over which we expect a wave's monster bodies to be cleared at a "normal" pace.
constexpr float kAdaptiveWaveSeconds = 90.0f;
// Tactical spawns never pick a spot closer than this to any living fighter (independent of theme).
constexpr float kSpawnMinPlayerDistance = 192.0f;
constexpr float kTargetIsolationWeightFraction = 0.50f;
constexpr float kTargetHealthWeightFraction = 0.375f;
constexpr float kMaxRetargetIntervalSeconds = 300.0f;
constexpr float kMaxTargetSpreadWeight = 1'000'000.0f;
constexpr float kMaxPursuitRepathSeconds = 60.0f;
// How long a blind pursuer gets to prove it is covering ground before we call it wedged.
constexpr gtime_t kPursuitSampleWindow = 1500_ms;
// Ground a blind pursuer must cover per sample window to count as making progress.
constexpr float kPursuitMinProgress = 24.0f;
// How far the cached pursuit goal may drift from the fighter before it is re-pinned.
constexpr float kPursuitGoalSlack = 128.0f;
// Renewed every think so ai_run never reaches its 20-second abandon check.
constexpr gtime_t kPursuitSearchLease = 5_sec;

// --- strategy targeting tuning ---------------------------------------------------------
constexpr float kLoadSpan = 3.0f;                 // attackers before a fighter reads as saturated
constexpr float kIsolationSpanUnits = 1024.0f;    // matches the legacy isolation normalizer
constexpr float kStreakSpanKills = 6.0f;
constexpr float kDefenseReference = 200.0f;       // 100 armor + 100 power-armor cells
constexpr float kNimbleWidthUnits = 40.0f;
constexpr float kBulkWidthUnits = 96.0f;
constexpr float kStepBudgetUnits = 18.0f;         // STEPSIZE
constexpr float kAgilityBudgetUnits = 256.0f;
constexpr float kBaseClimbSlope = 1.0f;           // 45 degrees for a nimble body
constexpr float kVerticalSpanUnits = 512.0f;
constexpr float kAreaDisconnectFactor = 0.30f;
// Not zero: PHS fails through a merely closed door, which opens again.
constexpr float kPhsBlockedFactor = 0.45f;
constexpr float kHabitatDryFactor = 0.15f;
constexpr float kHabitatDrownFactor = 0.35f;
constexpr float kEvidenceFloorFactor = 0.12f;
constexpr float kContactAccessFloor = 0.90f;
constexpr float kAbandonAccess = 0.25f;
constexpr float kRescueAccess = 0.55f;
constexpr float kSwitchMarginBase = 0.05f;
constexpr float kSpreadWeightReference = 512.0f;
constexpr float kUnknownHeavyBulk = 0.40f;        // an unclassified body this wide acts like a heavy
constexpr float kStickyEngagementUnits = 256.0f;
constexpr int   kMaxReachProbeBudget = 4096;
constexpr gtime_t kFighterFieldWindow = 100_ms;
constexpr gtime_t kEvidenceSampleWindow = 500_ms;
constexpr gtime_t kUnreachWindow = 12_sec;
constexpr gtime_t kEvidenceBlockedFloor = 3_sec;

struct AdaptiveState {
	int     wave = -1;
	int     waveBudgetStart = 0;
	int     playerDeathsWave = 0;
	int     monstersKilledAtStart = 0;
	gtime_t waveStartTime = 0_ms;
};

AdaptiveState adaptive_state;
float         adaptive_last_wave_pressure = 1.0f;

// Single-threaded reuse buffer (game DLL is not multi-threaded).
gtime_t                              target_load_frame = gtime_t::from_ms(-1);
std::array<int, MAX_CLIENTS + 1>     target_load = {};

// Per-fighter derived scalars that are expensive relative to a field read but change slowly.
// Only these three are cached: origin, area, water level, ground state and health are always
// read live, so a candidate cannot be scored against a stale corpse.
struct FighterField {
	gentity_t *ent = nullptr;   // identity guard against client slot reuse
	gtime_t    stamp = 0_ms;    // 0 means never built
	float      threat01 = 0.f;
	float      vuln01 = 0.f;
	float      isolation01 = 0.f;
};

std::array<FighterField, MAX_CLIENTS + 1> fighter_field = {};

// PHS is the only non-free engine call in the scorer, so it runs on a per-frame budget that
// is claimed all-or-nothing per evaluation: a shortfall shifts every candidate identically
// and can never reorder them, which would otherwise be a source of target thrashing.
gtime_t phs_budget_frame = gtime_t::from_ms(-1);
int     phs_budget_left = 0;

bool ClaimPhsBudget(int count)
{
	if (phs_budget_frame != level.time) {
		phs_budget_frame = level.time;
		phs_budget_left = MM_Horde_ClampFiniteInt(g_horde_reach_probe_budget->value,
			512, 0, kMaxReachProbeBudget);
	}

	const int needed = max(0, count);
	if (needed > phs_budget_left)
		return false;

	phs_budget_left -= needed;
	return true;
}

gentity_t *ClosestPlayerToPoint(vec3_t point)
{
	float      best_player_distance = std::numeric_limits<float>::max();
	gentity_t *closest = nullptr;

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client) || ec->health <= 0 || ec->client->eliminated)
			continue;

		const vec3_t delta = point - ec->s.origin;
		const float  player_distance = delta.length();

		if (player_distance < best_player_distance) {
			best_player_distance = player_distance;
			closest = ec;
		}
	}

	return closest;
}

void CountMonstersTargetingPlayers(int *counts, int count_capacity)
{
	if (!counts || count_capacity <= 0)
		return;

	std::fill_n(counts, count_capacity, 0);

	for (size_t i = 1; i < globals.num_entities; i++) {
		gentity_t *ent = &g_entities[i];

		if (!ent->inuse || !(ent->svflags & SVF_MONSTER))
			continue;
		if (ent->health <= 0 || ent->deadflag || (ent->svflags & SVF_DEADMONSTER))
			continue;
		if (ent->monsterinfo.aiflags & AI_GOOD_GUY)
			continue;
		if (!ent->enemy || !ent->enemy->client)
			continue;

		const int slot = static_cast<int>(ent->enemy - g_entities);
		if (slot < 1 || slot >= count_capacity)
			continue;

		counts[slot] = MM_Horde_SaturatingIncrement(counts[slot]);
	}
}

float FighterHealthFraction()
{
	float health_sum = 0.0f;
	float max_sum = 0.0f;
	int   fighters = 0;

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client) || ec->health <= 0 || ec->client->eliminated)
			continue;

		health_sum += static_cast<float>(max(ec->health, 0));
		max_sum += static_cast<float>(max(ec->max_health, 1));
		fighters++;
	}

	if (fighters <= 0 || max_sum <= 0.0f)
		return 1.0f;

	return health_sum / max_sum;
}

bool SpawnPointClear(gentity_t *spot, const vec3_t &check_mins, const vec3_t &check_maxs)
{
	const float lift = max(1.0f, -check_mins[2]);
	const vec3_t point = spot->s.origin + vec3_t{ 0.0f, 0.0f, lift };
	const trace_t tr = gi.trace(point, check_mins, check_maxs, point, spot, MASK_MONSTERSOLID);
	return !tr.startsolid && !tr.allsolid;
}

bool SpawnFarEnoughFromFighters(gentity_t *spot, float min_dist)
{
	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client) || ec->health <= 0 || ec->client->eliminated)
			continue;

		if ((spot->s.origin - ec->s.origin).length() < min_dist)
			return false;
	}

	return true;
}

bool SpawnSpotUsable(gentity_t *spot, vec3_t avoid_point)
{
	if (!spot || !spot->inuse)
		return false;

	float cv_dist = g_dm_respawn_point_min_dist->value;
	if (cv_dist > 512.0f)
		cv_dist = 512.0f;
	else if (cv_dist < 0.0f)
		cv_dist = 0.0f;

	if (avoid_point && cv_dist > 0.0f) {
		const vec3_t delta = spot->s.origin - avoid_point;
		if (delta.length() <= cv_dist)
			return false;
	}

	return true;
}

bool TacticalSpawnSpotValid(gentity_t *spot, vec3_t avoid_point, float min_player_dist,
	const vec3_t &check_mins, const vec3_t &check_maxs)
{
	if (!SpawnSpotUsable(spot, avoid_point))
		return false;
	if (!SpawnFarEnoughFromFighters(spot, min_player_dist))
		return false;
	if (!SpawnPointClear(spot, check_mins, check_maxs))
		return false;

	return true;
}

bool IsRangedGruntClassname(const char *classname)
{
	if (!classname)
		return false;

	return !Q_strcasecmp(classname, "monster_soldier") ||
		!Q_strcasecmp(classname, "monster_soldier_light") ||
		!Q_strcasecmp(classname, "monster_soldier_ss") ||
		!Q_strcasecmp(classname, "monster_soldier_hypergun") ||
		!Q_strcasecmp(classname, "monster_soldier_lasergun") ||
		!Q_strcasecmp(classname, "monster_soldier_ripper") ||
		!Q_strcasecmp(classname, "monster_infantry") ||
		!Q_strcasecmp(classname, "monster_gunner");
}

bool SupportsBlindfireClassname(const char *classname)
{
	if (!classname)
		return false;

	return !Q_strcasecmp(classname, "monster_gunner") ||
		!Q_strcasecmp(classname, "monster_chick") ||
		!Q_strcasecmp(classname, "monster_chick_heat") ||
		!Q_strcasecmp(classname, "monster_guncmdr") ||
		!Q_strcasecmp(classname, "monster_infantry") ||
		!Q_strcasecmp(classname, "monster_soldier_hypergun") ||
		!Q_strcasecmp(classname, "monster_soldier_lasergun") ||
		!Q_strcasecmp(classname, "monster_soldier_ripper");
}

bool IsHunterClassname(const char *classname)
{
	if (!classname)
		return false;

	return !Q_strcasecmp(classname, "monster_gekk") ||
		!Q_strcasecmp(classname, "monster_berserk") ||
		!Q_strcasecmp(classname, "monster_parasite") ||
		!Q_strcasecmp(classname, "monster_brain") ||
		!Q_strcasecmp(classname, "monster_mutant") ||
		!Q_strcasecmp(classname, "monster_stalker") ||
		!Q_strcasecmp(classname, "monster_flipper");
}

bool IsBulwarkClassname(const char *classname)
{
	if (!classname)
		return false;

	return !Q_strcasecmp(classname, "monster_gladb") ||
		!Q_strcasecmp(classname, "monster_gladiator") ||
		!Q_strcasecmp(classname, "monster_guncmdr") ||
		!Q_strcasecmp(classname, "monster_tank") ||
		!Q_strcasecmp(classname, "monster_tank_commander") ||
		!Q_strcasecmp(classname, "monster_shambler") ||
		!Q_strcasecmp(classname, "monster_supertank") ||
		!Q_strcasecmp(classname, "monster_boss2") ||
		!Q_strcasecmp(classname, "monster_carrier") ||
		!Q_strcasecmp(classname, "monster_makron");
}

bool IsRangedAnchorClassname(const char *classname)
{
	if (!classname)
		return false;

	return !Q_strcasecmp(classname, "monster_gladb") ||
		!Q_strcasecmp(classname, "monster_gladiator") ||
		!Q_strcasecmp(classname, "monster_chick") ||
		!Q_strcasecmp(classname, "monster_chick_heat") ||
		!Q_strcasecmp(classname, "monster_boss2") ||
		!Q_strcasecmp(classname, "monster_carrier") ||
		!Q_strcasecmp(classname, "monster_makron");
}

mm_horde_target_role_t TargetRole(const gentity_t *from)
{
	const char *classname = from ? from->classname : nullptr;

	if (IsHunterClassname(classname))
		return mm_horde_target_role_t::Hunter;
	if (IsBulwarkClassname(classname))
		return mm_horde_target_role_t::Bulwark;
	return mm_horde_target_role_t::Balanced;
}

bool IsAquaticMonster(const gentity_t *from)
{
	return from && ((from->flags & FL_SWIM) ||
		(from->classname && !Q_strcasecmp(from->classname, "monster_flipper")));
}

bool HasLivingWaterFighter()
{
	for (auto ec : active_clients())
		if (ClientIsPlaying(ec->client) && ec->health > 0 && !ec->client->eliminated &&
			ec->waterlevel >= WATER_WAIST)
			return true;

	return false;
}

float NearestLivingAllyDistance(const gentity_t *candidate)
{
	float nearest = std::numeric_limits<float>::max();
	int   allies = 0;

	for (auto ec : active_clients()) {
		if (ec == candidate || !ClientIsPlaying(ec->client) || ec->health <= 0 || ec->client->eliminated)
			continue;

		nearest = min(nearest, (ec->s.origin - candidate->s.origin).length());
		allies++;
	}

	return allies > 0 ? nearest : 0.0f;
}

float TargetScore(gentity_t *from, gentity_t *candidate, int monsters_targeting)
{
	const float distance = (candidate->s.origin - (from ? from->s.origin : vec3_origin)).length();
	const float nearest_ally = NearestLivingAllyDistance(candidate);
	const float health_frac = static_cast<float>(max(candidate->health, 0)) /
		static_cast<float>(max(candidate->max_health, 1));
	const float spread_weight = MM_Horde_ClampFiniteFloat(g_horde_target_spread_weight->value,
		512.f, 0.f, kMaxTargetSpreadWeight);

	return MM_Horde_ComputeRoleTargetScore(monsters_targeting, distance, nearest_ally, health_frac,
		spread_weight, spread_weight * kTargetIsolationWeightFraction,
		spread_weight * kTargetHealthWeightFraction, TargetRole(from));
}

gentity_t *FindBestTarget(gentity_t *from, gentity_t *current)
{
	const bool water_only = IsAquaticMonster(from) && HasLivingWaterFighter();
	gentity_t *best = nullptr;
	float      best_score = std::numeric_limits<float>::max();

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client) || ec->health <= 0 || ec->client->eliminated)
			continue;
		if (water_only && ec->waterlevel < WATER_WAIST)
			continue;

		const int slot = static_cast<int>(ec - g_entities);
		if (slot < 0 || slot >= static_cast<int>(target_load.size()))
			continue;

		int load = target_load[slot];
		if (current && ec != current)
			load = MM_Horde_SaturatingIncrement(load);

		const float score = TargetScore(from, ec, load);
		if (score < best_score) {
			best_score = score;
			best = ec;
		}
	}

	return best;
}

// Threats the pursuit driver may steer. Scripted goals, escorts, medics chasing a corpse,
// and genuinely immobile monsters keep the agenda vanilla gave them.
bool PursuitEligible(const gentity_t *monster)
{
	if (!monster || !monster->inuse || !(monster->svflags & SVF_MONSTER))
		return false;
	if (monster->health <= 0 || monster->deadflag || (monster->svflags & SVF_DEADMONSTER))
		return false;
	if (monster->flags & FL_STATIONARY)
		return false;
	if (monster->monsterinfo.aiflags &
		(AI_GOOD_GUY | AI_MEDIC | AI_COMBAT_POINT | AI_SOUND_TARGET | AI_HINT_PATH))
		return false;

	return true;
}

bool PursuitTargetAlive(const gentity_t *enemy)
{
	return enemy && enemy->inuse && enemy->client && enemy->health > 0 &&
		ClientIsPlaying(enemy->client) && !enemy->client->eliminated;
}

// ---------------------------------------------------------------------------------------
// Strategy classification
// ---------------------------------------------------------------------------------------

mm_horde_strategy_t StrategyForClassname(const char *classname)
{
	if (!classname)
		return mm_horde_strategy_t::Balanced;

	auto is = [classname](const char *name) { return !Q_strcasecmp(classname, name); };

	if (is("monster_soldier") || is("monster_soldier_light") || is("monster_soldier_ss") ||
		is("monster_soldier_hypergun") || is("monster_soldier_lasergun") ||
		is("monster_soldier_ripper") || is("monster_infantry"))
		return mm_horde_strategy_t::Swarm;

	if (is("monster_gekk") || is("monster_berserk") || is("monster_mutant") || is("monster_brain"))
		return mm_horde_strategy_t::Hunter;

	if (is("monster_stalker") || is("monster_parasite"))
		return mm_horde_strategy_t::Ambusher;

	if (is("monster_flyer") || is("monster_hover") || is("monster_daedalus") ||
		is("monster_floater") || is("monster_fixbot"))
		return mm_horde_strategy_t::Skirmisher;

	if (is("monster_gunner") || is("monster_chick") || is("monster_chick_heat") ||
		is("monster_boss2") || is("monster_carrier") || is("monster_makron") ||
		is("monster_arachnid"))
		return mm_horde_strategy_t::Artillery;

	if (is("monster_gladb") || is("monster_gladiator") || is("monster_guncmdr") ||
		is("monster_tank") || is("monster_tank_commander") || is("monster_shambler") ||
		is("monster_supertank") || is("monster_boss5") || is("monster_widow") ||
		is("monster_widow2") || is("monster_guardian"))
		return mm_horde_strategy_t::Bruiser;

	if (is("monster_medic") || is("monster_medic_commander"))
		return mm_horde_strategy_t::Support;

	if (is("monster_flipper"))
		return mm_horde_strategy_t::Aquatic;

	return mm_horde_strategy_t::Balanced;
}

float HullBulkOf(const gentity_t *from)
{
	if (!from)
		return 0.f;

	// size is maxs - mins, maintained by gi.linkentity after every rescale, so this tracks
	// s.scale, boss scaling, and Wildcard preset scaling without a second source of truth.
	return MM_Horde_HullBulk(max(from->size[0], from->size[1]), kNimbleWidthUnits, kBulkWidthUnits);
}

mm_horde_strategy_t StrategyForEntity(gentity_t *from)
{
	if (!from)
		return mm_horde_strategy_t::Balanced;

	// A gekk that entered water swaps its whole movement model; FL_SWIM is itself the stable
	// state, so this needs no hysteresis of its own.
	if (from->flags & FL_SWIM)
		return mm_horde_strategy_t::Aquatic;

	uint8_t &cached = from->monsterinfo.horde_strategy;
	if (!cached || cached > static_cast<uint8_t>(mm_horde_strategy_t::Count))
		cached = static_cast<uint8_t>(StrategyForClassname(from->classname)) + 1;

	mm_horde_strategy_t strategy = static_cast<mm_horde_strategy_t>(cached - 1);

	// A modded or map-authored heavy still behaves like a heavy.
	if (strategy == mm_horde_strategy_t::Balanced && HullBulkOf(from) >= kUnknownHeavyBulk)
		strategy = mm_horde_strategy_t::Bruiser;

	return strategy;
}

// ---------------------------------------------------------------------------------------
// Per-fighter threat and vulnerability signals
// ---------------------------------------------------------------------------------------

// An objective danger tier per weapon. Deliberately not the per-player preference rank,
// which is a switch order rather than a measure of how much damage the fighter can output.
float WeaponDanger01(const gitem_t *weapon)
{
	switch (weapon ? weapon->id : IT_NULL) {
	case IT_WEAPON_BFG:			return 1.00f;
	case IT_WEAPON_DISRUPTOR:	return 0.95f;
	case IT_WEAPON_RAILGUN:		return 0.90f;
	case IT_WEAPON_PLASMABEAM:	return 0.85f;
	case IT_WEAPON_RLAUNCHER:	return 0.80f;
	case IT_WEAPON_PHALANX:		return 0.72f;
	case IT_WEAPON_IONRIPPER:	return 0.68f;
	case IT_WEAPON_HYPERBLASTER:return 0.66f;
	case IT_WEAPON_CHAINGUN:	return 0.64f;
	case IT_WEAPON_SSHOTGUN:	return 0.58f;
	case IT_WEAPON_ETF_RIFLE:	return 0.52f;
	case IT_WEAPON_GLAUNCHER:	return 0.48f;
	case IT_WEAPON_MACHINEGUN:	return 0.44f;
	case IT_WEAPON_PROXLAUNCHER:return 0.40f;
	case IT_AMMO_TESLA:			return 0.34f;
	case IT_AMMO_TRAP:			return 0.32f;
	// Hand grenades and the shotgun sit on the same tier deliberately.
	case IT_AMMO_GRENADES:
	case IT_WEAPON_SHOTGUN:		return 0.30f;
	case IT_WEAPON_CHAINFIST:	return 0.18f;
	case IT_WEAPON_BLASTER:		return 0.10f;
	case IT_WEAPON_GRAPPLE:		return 0.05f;
	default:					return 0.30f;
	}
}

// Mirrors the engine's own out-of-ammo predicate so "dry" means what the weapon code means.
bool DryWeapon(gentity_t *ent)
{
	if (!ent || !ent->client)
		return false;

	gitem_t *weapon = ent->client->pers.weapon;
	if (!weapon)
		return false;

	const item_id_t ammo_id = MM_Ruleset_WeaponAmmoId(weapon);
	if (!ammo_id)
		return false;
	if (InfiniteAmmoOn(weapon) || MM_Arena_InfiniteAmmoEnabled(ent))
		return false;

	return ent->client->pers.inventory[ammo_id] < MM_Ruleset_WeaponAmmoRequired(weapon);
}

float ThreatOf(gentity_t *ec)
{
	const gclient_t *cl = ec->client;

	float powerup = 0.f;
	if (cl->pu_time_quad > level.time)
		powerup = 1.00f;
	else if (cl->pu_time_double > level.time)
		powerup = 0.70f;
	else if (cl->pu_time_haste > level.time)
		powerup = 0.45f;

	const int streak = max(cl->resp.kill_count, cl->pers.horde_kill_streak);
	const float streak01 = std::clamp(static_cast<float>(max(0, streak)) / kStreakSpanKills, 0.f, 1.f);
	// Both of these are future deadlines (level.time + window), not timestamps.
	const float firing01 = cl->last_firing_time >= level.time ? 1.f : 0.f;
	const float amp01 = cl->pers.inventory[IT_TECH_POWER_AMP] > 0 ? 1.f : 0.f;

	return MM_Horde_ThreatUtility(powerup, WeaponDanger01(cl->pers.weapon), streak01, firing01, amp01);
}

float VulnerabilityOf(gentity_t *ec)
{
	gclient_t *cl = ec->client;

	const float health_frac = std::clamp(static_cast<float>(max(ec->health, 0)) /
		static_cast<float>(max(ec->max_health, 1)), 0.f, 1.f);

	const item_id_t armor_id = ArmorIndex(ec);
	const float armor = armor_id ? static_cast<float>(max(0, cl->pers.inventory[armor_id])) : 0.f;
	const float power_armor = PowerArmorType(ec) != IT_NULL
		? static_cast<float>(min(max(0, cl->pers.inventory[IT_AMMO_CELLS]), 100))
		: 0.f;
	const float defense_frac = std::clamp((armor + power_armor) / kDefenseReference, 0.f, 1.f);

	const float hurt01 = cl->last_damage_time > level.time ? 1.f : 0.f;
	const float dry01 = DryWeapon(ec) ? 1.f : 0.f;
	const bool  helpless = cl->inmenu || cl->grapple_state != 0 ||
		ec->groundentity == nullptr || cl->pers.lives == 1;
	const bool  protected_now = cl->pu_time_protection > level.time ||
		cl->pu_time_invisibility > level.time;

	return MM_Horde_VulnerabilityUtility(1.f - health_frac, 1.f - defense_frac, hurt01, dry01,
		helpless ? 1.f : 0.f, protected_now);
}

const FighterField &FighterFieldFor(gentity_t *ec)
{
	static FighterField scratch;

	const int slot = static_cast<int>(ec - g_entities);
	const bool in_range = slot >= 1 && slot < static_cast<int>(fighter_field.size());
	FighterField &entry = in_range ? fighter_field[slot] : scratch;

	if (in_range && entry.ent == ec && entry.stamp != 0_ms &&
		(level.time - entry.stamp) < kFighterFieldWindow)
		return entry;

	entry.ent = ec;
	entry.stamp = level.time;
	entry.threat01 = ThreatOf(ec);
	entry.vuln01 = VulnerabilityOf(ec);

	float nearest_sq = std::numeric_limits<float>::max();
	int   allies = 0;
	for (auto other : active_clients()) {
		if (other == ec || !ClientIsPlaying(other->client) || other->health <= 0 ||
			other->client->eliminated)
			continue;

		nearest_sq = min(nearest_sq, (other->s.origin - ec->s.origin).lengthSquared());
		allies++;
	}

	entry.isolation01 = MM_Horde_IsolationUtility(
		allies > 0 ? std::sqrt(nearest_sq) : 0.f, allies, kIsolationSpanUnits);

	return entry;
}

// ---------------------------------------------------------------------------------------
// Reachability and the strategy scorer
// ---------------------------------------------------------------------------------------

// Everything the scorer needs about the monster, resolved once per evaluation rather than
// once per candidate. This is what retires the old per-candidate classname compare chain.
struct TargetContext {
	gentity_t                  *self = nullptr;
	mm_horde_strategy_weights_t weights = {};
	vec3_t   origin = {};
	int32_t  areanum = 0;
	int32_t  areanum2 = 0;
	float    climb_budget = kStepBudgetUnits;
	float    slope = kBaseClimbSlope;
	float    sharpen = 0.f;
	float    switch_margin = kSwitchMarginBase;
	bool     water_only = false;
	bool     phs_enabled = false;
	uint64_t unreach_lo = 0;
	uint64_t unreach_hi = 0;
	int64_t  unreach_age_ms = 0;
};

TargetContext BuildTargetContext(gentity_t *from, bool allow_phs)
{
	TargetContext ctx;
	ctx.self = from;

	const mm_horde_strategy_t strategy = StrategyForEntity(from);
	const float bulk = HullBulkOf(from);

	mm_horde_strategy_weights_t weights = MM_Horde_ApplySizeToWeights(
		MM_Horde_StrategyWeights(strategy), bulk);
	ctx.weights = MM_Horde_ApplyWeightTuning(weights,
		MM_Horde_NormalizedSpreadWeight(g_horde_target_spread_weight->value,
			kSpreadWeightReference, MM_HORDE_MAX_TARGET_WEIGHT),
		g_horde_target_aggression->value, g_horde_target_opportunism->value);
	ctx.switch_margin = MM_Horde_TargetSwitchMargin(kSwitchMarginBase, weights.switch_scale);

	if (from) {
		ctx.origin = from->s.origin;
		ctx.areanum = from->areanum;
		ctx.areanum2 = from->areanum2;

		const bool flyer = (from->flags & FL_FLY) != 0;
		const bool swimmer = (from->flags & FL_SWIM) != 0;
		ctx.climb_budget = MM_Horde_ClimbBudget(flyer, swimmer, from->monsterinfo.can_jump,
			from->monsterinfo.jump_height, from->monsterinfo.drop_height, kStepBudgetUnits);
		ctx.slope = MM_Horde_ClimbSlope(kBaseClimbSlope, bulk);
		ctx.sharpen = MM_Horde_GateSharpen(weights.gate_bias, bulk,
			std::clamp(ctx.climb_budget / kAgilityBudgetUnits, 0.f, 1.f));

		ctx.unreach_lo = from->monsterinfo.horde_unreach_lo;
		ctx.unreach_hi = from->monsterinfo.horde_unreach_hi;
		ctx.unreach_age_ms = (level.time - from->monsterinfo.horde_unreach_time).milliseconds();
	}

	ctx.water_only = IsAquaticMonster(from) && HasLivingWaterFighter();
	// Claimed all-or-nothing: either every candidate this evaluation gets a PHS factor or
	// none does, so a budget shortfall can never reorder candidates.
	ctx.phs_enabled = allow_phs && ClaimPhsBudget(level.num_playing_clients);

	return ctx;
}

bool AreasReachable(int32_t a1, int32_t a2, int32_t b1, int32_t b2)
{
	if (a1 == b1 || (a2 && (a2 == b1 || a2 == b2)) || (b2 && b2 == a1))
		return true;
	if (gi.AreasConnected(a1, b1))
		return true;
	if (a2 && gi.AreasConnected(a2, b1))
		return true;
	if (b2 && gi.AreasConnected(a1, b2))
		return true;

	return false;
}

float CandidateAccess(const TargetContext &ctx, gentity_t *candidate, int slot)
{
	gentity_t *from = ctx.self;

	// Areas that cannot be connected are usually genuinely separated, but areaportals open,
	// so this is a heavy discount rather than a hard zero.
	const float area = AreasReachable(ctx.areanum, ctx.areanum2,
		candidate->areanum, candidate->areanum2) ? 1.f : kAreaDisconnectFactor;

	const vec3_t delta = candidate->s.origin - ctx.origin;
	const float horizontal = std::sqrt(delta.x * delta.x + delta.y * delta.y);
	const float vertical = MM_Horde_VerticalAccess(delta.z, horizontal, ctx.climb_budget,
		ctx.slope, kVerticalSpanUnits);

	float habitat = 1.f;
	if (from) {
		if ((from->flags & FL_SWIM) && !(from->flags & FL_FLY) &&
			candidate->waterlevel < WATER_WAIST)
			habitat = kHabitatDryFactor;
		else if (!(from->flags & (FL_SWIM | FL_FLY)) && candidate->waterlevel == WATER_UNDER)
			habitat = kHabitatDrownFactor;
	}

	const float phs = (!ctx.phs_enabled || !from ||
		gi.inPHS(ctx.origin, candidate->s.origin, true)) ? 1.f : kPhsBlockedFactor;

	const float evidence = MM_Horde_UnreachFactor(
		MM_Horde_UnreachMaskTest(ctx.unreach_lo, ctx.unreach_hi, slot),
		ctx.unreach_age_ms, kUnreachWindow.milliseconds(), kEvidenceFloorFactor);

	// An enemy we are currently in contact with is reachable by definition.
	const bool in_contact = from && from->enemy == candidate &&
		!(from->monsterinfo.aiflags & AI_LOST_SIGHT);

	return MM_Horde_ComputeAccess(area, vertical, habitat, phs, evidence, in_contact,
		kContactAccessFloor);
}

float StrategyTargetScore(const TargetContext &ctx, gentity_t *candidate, int slot, int load,
	float *out_access)
{
	const FighterField &field = FighterFieldFor(candidate);

	mm_horde_target_terms_t terms;
	terms.prox = MM_Horde_ProximityUtility((candidate->s.origin - ctx.origin).length(),
		ctx.weights.prox_half);
	terms.free = MM_Horde_LoadUtility(load, kLoadSpan);
	terms.threat = field.threat01;
	terms.vuln = field.vuln01;
	terms.isolation = field.isolation01;

	const float access = CandidateAccess(ctx, candidate, slot);
	if (out_access)
		*out_access = access;

	return MM_Horde_ComputeGatedTargetScore(terms, ctx.weights, access, ctx.sharpen,
		MM_HORDE_GATE_FLOOR);
}

// Highest score wins; ties resolve to the lowest entity slot, matching the legacy scorer.
// Reports the incumbent's score and access in the same pass so the caller never has to
// re-evaluate it asymmetrically.
gentity_t *FindBestTargetStrategy(gentity_t *current, const TargetContext &ctx,
	float *out_best_score, float *out_best_access, float *out_current_score,
	float *out_current_access)
{
	MM_PROFILE_ZONE("MM_Horde_FindBestTarget");

	gentity_t *best = nullptr;
	float      best_score = -1.f;
	float      best_access = 1.f;

	if (out_current_score)
		*out_current_score = 0.f;
	if (out_current_access)
		*out_current_access = 1.f;

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client) || ec->health <= 0 || ec->client->eliminated)
			continue;
		if (ctx.water_only && ec->waterlevel < WATER_WAIST)
			continue;

		const int slot = static_cast<int>(ec - g_entities);
		if (slot < 0 || slot >= static_cast<int>(target_load.size()))
			continue;

		// Normalize the load so the incumbent and every challenger are compared as they would
		// read after a switch; the raw counts already include this monster on its incumbent.
		const int load = MM_Horde_ComparableTargetLoad(target_load[slot], ec == current,
			current != nullptr);

		float access = 1.f;
		const float score = StrategyTargetScore(ctx, ec, slot, load, &access);

		if (ec == current) {
			if (out_current_score)
				*out_current_score = score;
			if (out_current_access)
				*out_current_access = access;
		}

		if (score > best_score) {
			best_score = score;
			best_access = access;
			best = ec;
		}
	}

	if (out_best_score)
		*out_best_score = max(best_score, 0.f);
	if (out_best_access)
		*out_best_access = best_access;

	return best;
}

} // namespace

void ResetRuntimeState()
{
	adaptive_state = {};
	adaptive_last_wave_pressure = 1.0f;
	target_load_frame = gtime_t::from_ms(-1);
	target_load.fill(0);
	fighter_field.fill({});
	phs_budget_frame = gtime_t::from_ms(-1);
	phs_budget_left = 0;
}

void RefreshTargetLoadCache()
{
	if (target_load_frame == level.time)
		return;

	target_load_frame = level.time;
	CountMonstersTargetingPlayers(target_load.data(), static_cast<int>(target_load.size()));
}

float AdaptivePressureMult()
{
	if (!g_horde_enhanced_ai->integer)
		return 1.0f;

	const float health_frac = FighterHealthFraction();
	const int   fighters = max(1, MM_Horde_CountFighters());
	const float death_pressure = static_cast<float>(adaptive_state.playerDeathsWave) / static_cast<float>(fighters);

	const int64_t killed_delta = static_cast<int64_t>(level.killed_monsters) -
		static_cast<int64_t>(adaptive_state.monstersKilledAtStart);
	const int killed = static_cast<int>(std::clamp<int64_t>(killed_delta, 0,
		std::numeric_limits<int>::max()));
	const float elapsed = (level.time - adaptive_state.waveStartTime).seconds();
	const float clear_rate = elapsed > 0.0f ? static_cast<float>(killed) / elapsed : 0.0f;
	// Compare monsters/sec cleared against an expected monsters/sec derived from budget, not raw points.
	const float expected_monsters = adaptive_state.waveBudgetStart > 0
		? static_cast<float>(adaptive_state.waveBudgetStart) / kAdaptivePointsPerMonster
		: 0.0f;
	const float expected_rate = expected_monsters / kAdaptiveWaveSeconds;
	const float clear_ratio = expected_rate > 0.0f ? clear_rate / expected_rate : 1.0f;

	return MM_Horde_ComputeAdaptiveSpawnMult(health_frac, death_pressure, clear_ratio);
}

void Adaptive_BeginWave()
{
	if (!g_horde_enhanced_ai->integer) {
		adaptive_state = {};
		adaptive_last_wave_pressure = 1.0f;
		return;
	}

	const float budget_mult = MM_Horde_ComputeAdaptiveBudgetMult(adaptive_last_wave_pressure);
	if (budget_mult != 1.0f)
		level.horde_spawn_points_remaining = MM_Horde_ScaleInt(
			level.horde_spawn_points_remaining, budget_mult,
			level.horde_spawn_points_remaining, 1);

	adaptive_state.wave = level.round_number;
	adaptive_state.waveBudgetStart = level.horde_spawn_points_remaining;
	adaptive_state.playerDeathsWave = 0;
	adaptive_state.monstersKilledAtStart = level.killed_monsters;
	adaptive_state.waveStartTime = level.time;
	adaptive_last_wave_pressure = 1.0f;
}

void Adaptive_RecordWaveEnd()
{
	if (!g_horde_enhanced_ai->integer)
		return;

	adaptive_last_wave_pressure = AdaptivePressureMult();
}

void Adaptive_RecordPlayerDeath()
{
	if (g_horde_enhanced_ai->integer)
		adaptive_state.playerDeathsWave = MM_Horde_SaturatingIncrement(adaptive_state.playerDeathsWave);
}

select_spawn_result_t SelectSpawnPoint(vec3_t avoid_point, const vec3_t &check_mins,
	const vec3_t &check_maxs, gentity_t *const *allowed_spots, size_t allowed_count,
	bool restrict_to_allowed)
{
	struct Candidate {
		gentity_t *spot;
		float      dist;
		float      bearing;
		float      z;
	};

	// Single-threaded reuse buffer (game DLL is not multi-threaded).
	static std::vector<Candidate> candidates;
	static std::vector<Candidate> close_candidates;
	candidates.clear();
	close_candidates.clear();

	vec3_t cluster = vec3_origin;
	int    fighters = 0;

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client) || ec->health <= 0 || ec->client->eliminated)
			continue;

		cluster += ec->s.origin;
		fighters++;
	}

	if (fighters < 1 && !restrict_to_allowed)
		return SelectDeathmatchSpawnPoint(nullptr, avoid_point, SPAWN_FARTHEST, false, true, false, false);

	if (fighters > 0)
		cluster /= static_cast<float>(fighters);

	float cv_dist = g_dm_respawn_point_min_dist->value;
	if (cv_dist > 512.0f)
		cv_dist = 512.0f;
	else if (cv_dist < 0.0f)
		cv_dist = 0.0f;
	const float min_player_dist = max(kSpawnMinPlayerDistance, cv_dist);

	auto try_add_spot = [&](gentity_t *spot) {
		if (!SpawnSpotUsable(spot, avoid_point) || !SpawnPointClear(spot, check_mins, check_maxs))
			return;
		// Reject spots a stationary fighter can never validate downstream: MM_Horde_RunSpawning
		// fails the whole attempt post-selection if the chosen spot isn't in any fighter's PHS
		// (e.g. behind a closed door), and with few remaining candidates that can keep
		// reselecting the same blocked spot forever. Filtering here up front — rather than only
		// deprioritizing into close_candidates — lets a different, reachable candidate win instead.
		if (!OriginSharesFighterPHS(spot->s.origin))
			return;

		const vec3_t delta = spot->s.origin - cluster;
		Candidate candidate = {
			spot,
			delta.length(),
			std::atan2(delta.y, delta.x),
			spot->s.origin[2],
		};

		if (TacticalSpawnSpotValid(spot, avoid_point, min_player_dist, check_mins, check_maxs))
			candidates.push_back(candidate);
		else
			close_candidates.push_back(candidate);
	};

	if (restrict_to_allowed) {
		for (size_t i = 0; i < allowed_count; i++)
			try_add_spot(allowed_spots ? allowed_spots[i] : nullptr);
	} else {
		gentity_t *spot = nullptr;
		while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_deathmatch")) != nullptr)
			try_add_spot(spot);

		// If every deathmatch spot is too close, still give team spawns a chance to
		// provide a tactically safe location before falling back to the close set.
		if (candidates.empty()) {
			spot = nullptr;
			while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_team_red")) != nullptr)
				try_add_spot(spot);
			spot = nullptr;
			while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_team_blue")) != nullptr)
				try_add_spot(spot);
		}
	}

	if (candidates.empty())
		candidates.swap(close_candidates);

	if (candidates.empty())
		return restrict_to_allowed
			? select_spawn_result_t {}
			: SelectDeathmatchSpawnPoint(nullptr, avoid_point, SPAWN_FARTHEST, false, true, false, false);
	if (candidates.size() == 1)
		return { candidates.front().spot, true };

	std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
		return a.dist < b.dist;
	});

	if (!g_horde_enhanced_ai->integer)
		return { candidates.back().spot, true };

	const float min_dist = candidates.front().dist;
	const float max_dist = candidates.back().dist;
	const float dist_span = max(1.0f, max_dist - min_dist);

	const HordeCategory theme = ActiveThemeCategory();
	const bool          prefer_close = (theme & (HCAT_MELEE | HCAT_INFEST)) != HordeCategory::None;
	const bool          prefer_aerial = (theme & HCAT_AERIAL) != HordeCategory::None;

	const float ideal_t = prefer_close ? 0.25f : (prefer_aerial ? 0.75f : 0.55f);
	const float ideal_dist = min_dist + dist_span * ideal_t;
	const float cluster_z = cluster[2];

	gentity_t *best = nullptr;
	float      best_score = std::numeric_limits<float>::lowest();

	for (const auto &candidate : candidates) {
		const float dist_penalty = -std::fabs(candidate.dist - ideal_dist);
		const float aerial_bonus = prefer_aerial ? (candidate.z - cluster_z) * 0.25f : 0.0f;
		const float flank_bonus = (1.0f - std::fabs(candidate.dist - ideal_dist) / dist_span) * 128.0f;
		const float bearing_variety = std::fabs(std::sin(candidate.bearing)) * 32.0f;
		const float score = dist_penalty + aerial_bonus + flank_bonus + bearing_variety + frandom() * 16.0f;

		if (score > best_score) {
			best_score = score;
			best = candidate.spot;
		}
	}

	if (best)
		return { best, true };

	return SelectDeathmatchSpawnPoint(nullptr, avoid_point, SPAWN_FARTHEST, false, true, false, false);
}

void ApplySpawnRoleTuning(gentity_t *ent, const char *classname)
{
	if (!g_horde_enhanced_ai->integer || !ent || !classname)
		return;

	if (IsHunterClassname(classname))
		ent->monsterinfo.combat_style = COMBAT_MELEE;
	else if (IsRangedAnchorClassname(classname))
		ent->monsterinfo.combat_style = COMBAT_RANGED;
	else if (IsRangedGruntClassname(classname))
		ent->monsterinfo.combat_style = COMBAT_MIXED;

	if (SupportsBlindfireClassname(classname))
		ent->monsterinfo.blindfire = true;

	// Strategy is orthogonal to combat_style: strategy chooses which fighter, combat_style
	// still governs how the monster approaches once it has one.
	ent->monsterinfo.horde_strategy = static_cast<uint8_t>(StrategyForClassname(classname)) + 1;

	const float retarget_sec = MM_Horde_ClampFiniteFloat(g_horde_retarget_interval->value,
		8.f, 0.f, kMaxRetargetIntervalSeconds);
	if (retarget_sec > 0.0f) {
		ent->monsterinfo.horde_retarget_time = level.time +
			random_time(gtime_t::from_sec(retarget_sec * 0.75f), gtime_t::from_sec(retarget_sec * 1.25f));
	}
}

gentity_t *PickTarget(gentity_t *from)
{
	const vec3_t origin = from ? from->s.origin : vec3_origin;

	if (!g_horde_enhanced_ai->integer)
		return ClosestPlayerToPoint(origin);

	RefreshTargetLoadCache();

	gentity_t *best = nullptr;
	if (g_horde_target_model->integer) {
		const TargetContext ctx = BuildTargetContext(from, true);
		best = FindBestTargetStrategy(nullptr, ctx, nullptr, nullptr, nullptr, nullptr);
	} else {
		best = FindBestTarget(from, nullptr);
	}

	// Same-frame retarget/spawn callers share the frozen load cache; bump the chosen slot so the
	// next picker sees this assignment (e.g. mass re-acquire on death).
	if (best) {
		const int best_slot = static_cast<int>(best - g_entities);
		if (best_slot >= 0 && best_slot < static_cast<int>(target_load.size()))
			target_load[best_slot] = MM_Horde_SaturatingIncrement(target_load[best_slot]);
	}

	return best ? best : ClosestPlayerToPoint(origin);
}

bool MaybeRetarget(gentity_t *monster)
{
	if (!g_horde_enhanced_ai->integer || !monster || monster->health <= 0)
		return false;
	const float retarget_sec = MM_Horde_ClampFiniteFloat(g_horde_retarget_interval->value,
		8.f, 0.f, kMaxRetargetIntervalSeconds);
	if (retarget_sec <= 0.0f || monster->monsterinfo.horde_retarget_time > level.time)
		return false;
	if (monster->monsterinfo.aiflags & (AI_MEDIC | AI_COMBAT_POINT | AI_SOUND_TARGET))
		return false;
	if (!monster->enemy || !monster->enemy->client || monster->enemy->health <= 0 ||
		monster->enemy->client->eliminated)
		return false;

	monster->monsterinfo.horde_retarget_time = level.time +
		random_time(gtime_t::from_sec(retarget_sec * 0.75f), gtime_t::from_sec(retarget_sec * 1.25f));

	// An enemy we are in contact with at knife range stays ours. Under the strategy model we
	// reuse vanilla's own sight bookkeeping instead of paying for another traceline; contact
	// also floors that enemy's access, so no separate reachability check is needed here.
	if (g_horde_target_model->integer) {
		if (!(monster->monsterinfo.aiflags & AI_LOST_SIGHT) &&
			range_to(monster, monster->enemy) < kStickyEngagementUnits)
			return false;
	} else if (visible(monster, monster->enemy) && range_to(monster, monster->enemy) < 256.0f) {
		return false;
	}

	RefreshTargetLoadCache();

	gentity_t *current = monster->enemy;
	gentity_t *best = nullptr;
	float      best_score = 0.f, best_access = 1.f;
	float      current_score = 0.f, current_access = 1.f;

	if (g_horde_target_model->integer) {
		const TargetContext ctx = BuildTargetContext(monster, true);
		best = FindBestTargetStrategy(current, ctx, &best_score, &best_access,
			&current_score, &current_access);
		if (!best || best == current)
			return false;

		const int slot_a = static_cast<int>(current - g_entities);
		const int slot_b = static_cast<int>(best - g_entities);
		if (slot_a < 0 || slot_a >= static_cast<int>(target_load.size()) ||
			slot_b < 0 || slot_b >= static_cast<int>(target_load.size()))
			return false;

		if (!MM_Horde_ShouldSwitchTarget(best_score, current_score, ctx.switch_margin,
				current_access, best_access, kAbandonAccess, kRescueAccess))
			return false;
	} else {
		best = FindBestTarget(monster, current);
		if (!best || best == current)
			return false;

		const int slot_a = static_cast<int>(current - g_entities);
		const int slot_b = static_cast<int>(best - g_entities);
		if (slot_a < 0 || slot_a >= static_cast<int>(target_load.size()) ||
			slot_b < 0 || slot_b >= static_cast<int>(target_load.size()))
			return false;

		const float legacy_current = TargetScore(monster, current, target_load[slot_a]);
		const float legacy_best = TargetScore(monster, best,
			MM_Horde_SaturatingIncrement(target_load[slot_b]));
		const float spread_weight = MM_Horde_ClampFiniteFloat(g_horde_target_spread_weight->value,
			512.f, 0.f, kMaxTargetSpreadWeight);
		const float switch_margin = max(32.0f, spread_weight * 0.25f);
		if (legacy_best + switch_margin >= legacy_current)
			return false;
	}

	const int current_slot = static_cast<int>(current - g_entities);
	const int best_slot = static_cast<int>(best - g_entities);

	target_load[current_slot] = max(0, target_load[current_slot] - 1);
	target_load[best_slot] = MM_Horde_SaturatingIncrement(target_load[best_slot]);
	monster->oldenemy = nullptr;
	monster->enemy = best;
	monster->goalentity = best;
	FoundTarget(monster);
	return true;
}

// Reachability evidence is harvested from what the engine already computed for this monster
// on its last path attempt, so the scorer never has to call gi.GetPathToGoal itself. Runs for
// every horde monster, independent of the pursuit driver's own gating.
void HarvestReachEvidence(gentity_t *monster)
{
	if (!g_horde_enhanced_ai->integer || !g_horde_target_model->integer)
		return;
	if (!monster || !monster->inuse || monster->health <= 0)
		return;

	monsterinfo_t &info = monster->monsterinfo;
	if (info.horde_reach_sample_time > level.time)
		return;
	info.horde_reach_sample_time = level.time + kEvidenceSampleWindow;

	gentity_t *enemy = monster->enemy;
	if (!enemy || !enemy->client)
		return;

	const int slot = static_cast<int>(enemy - g_entities);
	if (slot < 1 || slot > 128)
		return;

	// AI_NO_PATH_FINDING means the map has no nav mesh at all, so a nav verdict says nothing
	// about this particular fighter.
	const bool nav_dead = !(info.aiflags & AI_NO_PATH_FINDING) &&
		(info.nav_path.returnCode == PathReturnCode::NoPathFound ||
			info.nav_path.returnCode == PathReturnCode::NoGoalNode);
	const bool wedged = info.path_blocked_counter >= kEvidenceBlockedFloor;
	const bool contact = !(info.aiflags & AI_LOST_SIGHT);
	const bool routed = info.nav_path.returnCode <= PathReturnCode::InProgress &&
		info.nav_path_cache_time > level.time;

	if (contact || routed) {
		// Positive evidence clears only this fighter's bit, never the whole mask.
		MM_Horde_UnreachMaskClear(info.horde_unreach_lo, info.horde_unreach_hi, slot);
	} else if (nav_dead || wedged) {
		// The clock is shared, so a fully decayed mask must be dropped before it is refreshed
		// or old bits would silently revive.
		if ((level.time - info.horde_unreach_time) >= kUnreachWindow)
			info.horde_unreach_lo = info.horde_unreach_hi = 0;

		MM_Horde_UnreachMaskSet(info.horde_unreach_lo, info.horde_unreach_hi, slot);
		info.horde_unreach_time = level.time;
	}
}

void DrivePursuit(gentity_t *monster)
{
	if (!g_horde_enhanced_ai->integer || !g_horde_pursuit->integer)
		return;
	if (!PursuitEligible(monster))
		return;

	gentity_t *enemy = monster->enemy;
	if (!PursuitTargetAlive(enemy))
		return;

	monsterinfo_t &info = monster->monsterinfo;

	// A live fighter assignment outranks any hold order: Horde threats advance.
	if (info.aiflags & AI_STAND_GROUND) {
		info.aiflags &= ~(AI_STAND_GROUND | AI_TEMP_STAND_GROUND);
		info.pausetime = 0_ms;
	}

	// ai_run abandons the hunt 20 seconds past search_time; renew the lease so a threat
	// with a living target never stops looking for it.
	info.search_time = level.time + kPursuitSearchLease;

	// AI_LOST_SIGHT is vanilla's own "can't see them" bookkeeping, so we reuse it rather
	// than paying for another line-of-sight trace per monster per frame. While in contact,
	// vanilla combat movement is already aggressive and the progress sample below would
	// misread deliberate strafing or a ranged hold as being wedged.
	if (!(info.aiflags & AI_LOST_SIGHT)) {
		info.horde_pursuit_sample_time = 0_ms;
		return;
	}

	// Out of contact. Vanilla walks the player trail, which strands a monster wherever the
	// trail went cold; pin the fallback goal to where the fighter actually is instead.
	if (MM_Horde_ShouldRepinPursuitGoal((info.last_sighting - enemy->s.origin).length(),
			kPursuitGoalSlack)) {
		info.last_sighting = info.saved_goal = enemy->s.origin;
		info.aiflags &= ~(AI_PURSUIT_LAST_SEEN | AI_PURSUE_NEXT | AI_PURSUE_TEMP);
	}

	// Cap the nav lockout a failed path may impose (vanilla holds it for 5-10 seconds).
	const float repath_sec = MM_Horde_ClampFiniteFloat(g_horde_pursuit_repath_time->value,
		2.f, 0.f, kMaxPursuitRepathSeconds);
	info.path_wait_time = gtime_t::from_ms(MM_Horde_ClampPursuitLockoutMs(
		info.path_wait_time.milliseconds(), level.time.milliseconds(), repath_sec));

	if (info.horde_pursuit_sample_time > level.time)
		return;

	const bool  sample_valid = info.horde_pursuit_sample_time != 0_ms;
	const float moved = (monster->s.origin - info.horde_pursuit_last_origin).length();

	info.horde_pursuit_last_origin = monster->s.origin;
	info.horde_pursuit_sample_time = level.time + kPursuitSampleWindow;

	if (!MM_Horde_PursuitStalled(sample_valid, moved, kPursuitMinProgress))
		return;

	// Wedged on geometry: drop every movement penalty so the next frame re-paths and picks
	// a fresh direction instead of serving out a bad-move or blocked cooldown in place.
	info.bad_move_time = 0_ms;
	info.random_change_time = 0_ms;
	info.path_wait_time = 0_ms;
	info.nav_path_cache_time = 0_ms;
	info.path_blocked_counter = 0_ms;
	info.move_block_counter = 0;
	info.aiflags &= ~AI_BLOCKED;
	info.last_sighting = info.saved_goal = enemy->s.origin;
	monster->ideal_yaw = vectoyaw(enemy->s.origin - monster->s.origin);
}

} // namespace muffmode::horde

gentity_t *MM_Horde_PickTarget(gentity_t *from)
{
	return muffmode::horde::PickTarget(from);
}

bool MM_Horde_MaybeRetarget(gentity_t *monster)
{
	return muffmode::horde::MaybeRetarget(monster);
}

void MM_Horde_DrivePursuit(gentity_t *monster)
{
	// Harvested before (and outside) the pursuit driver's own gating so reachability memory
	// still accrues for medics, stationary and scripted monsters, and is not disabled by
	// g_horde_pursuit 0.
	muffmode::horde::HarvestReachEvidence(monster);
	muffmode::horde::DrivePursuit(monster);
}
