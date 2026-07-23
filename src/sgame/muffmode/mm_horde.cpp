// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_announcer.h"
#include "muffmode/mm_horde.h"
#include "muffmode/mm_horde_ai.h"
#include "muffmode/mm_horde_ai_rules.h"
#include "muffmode/mm_horde_tables.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <limits>
#include <vector>

// Late-wave tuning cvars are referenced by helpers defined before the main extern block below.
extern cvar_t *g_horde_content_peak_wave;
extern cvar_t *g_horde_late_wave_factor;
extern cvar_t *g_horde_late_escalation;
extern cvar_t *g_horde_late_budget_factor;
extern cvar_t *g_horde_late_max_alive_per_wave;
extern cvar_t *g_horde_late_max_alive_cap;
extern cvar_t *g_horde_theme_min_monsters;
extern cvar_t *g_horde_enhanced_ai;

namespace horde = muffmode::horde;

extern cvar_t *g_horde_starting_wave;
extern cvar_t *g_horde_points_base;
extern cvar_t *g_horde_points_per_wave;
extern cvar_t *g_horde_points_min;
extern cvar_t *g_horde_points_max;
extern cvar_t *g_horde_spawn_interval_min;
extern cvar_t *g_horde_spawn_interval_max;
extern cvar_t *g_horde_spawn_burst_count;
extern cvar_t *g_horde_spawn_burst_rest;
extern cvar_t *g_horde_warmup_cap;
extern cvar_t *g_horde_max_alive;
extern cvar_t *g_horde_wave_spawn_delay_ms;
extern cvar_t *g_horde_player_scale;
extern cvar_t *g_horde_player_scale_factor;
extern cvar_t *g_horde_player_scale_max;
extern cvar_t *g_horde_lives;
extern cvar_t *g_horde_mark_monsters_threshold;
extern cvar_t *g_horde_mark_monsters_max;
extern cvar_t *g_horde_map_scale;
extern cvar_t *g_horde_map_scale_ref;
extern cvar_t *g_horde_map_scale_factor;
extern cvar_t *g_horde_champions;
extern cvar_t *g_horde_champion_max_per_run;
extern cvar_t *g_horde_champion_chance;
extern cvar_t *g_horde_champion_min_wave;
extern cvar_t *g_horde_champion_health_mult;
extern cvar_t *g_horde_champion_health_floor;
extern cvar_t *g_horde_champion_health_per_wave;
extern cvar_t *g_horde_champion_damage_mult;
extern cvar_t *g_horde_champion_speed_mult;
extern cvar_t *g_horde_champion_strong_ratio;
extern cvar_t *g_horde_champion_force; // DEBUG/TEST: force a champion every wave
extern cvar_t *g_horde_boss_waves;
extern cvar_t *g_horde_boss_min_wave;
extern cvar_t *g_horde_boss_interval;
extern cvar_t *g_horde_boss_budget_mult;
extern cvar_t *g_horde_boss_health_mult;
extern cvar_t *g_horde_boss_damage_mult;
extern cvar_t *g_horde_boss_tier_window;
extern cvar_t *g_horde_boss_powerup_chance;
extern cvar_t *g_horde_boss_machinegames;
extern cvar_t *g_horde_boss_pairs;
extern cvar_t *g_horde_boss_repeat_window;
extern cvar_t *g_horde_boss_force;
extern cvar_t *g_horde_boss_scale_limit;
extern cvar_t *g_horde_boss_health_per_wave;
extern cvar_t *g_horde_boss_damage_per_wave;
extern cvar_t *g_horde_boss_pair_health_mult;
extern cvar_t *g_horde_boss_armor_mult;
extern cvar_t *g_horde_themed_waves;
extern cvar_t *g_horde_theme_chance;
extern cvar_t *g_horde_theme_min_wave;
extern cvar_t *g_horde_wave_variety;
extern cvar_t *g_horde_wave_min_types;
extern cvar_t *g_horde_wave_type_ramp;
extern cvar_t *g_horde_featured_spawns;
extern cvar_t *g_horde_water_spawns;
extern cvar_t *g_horde_water_spawn_chance;
extern cvar_t *g_horde_water_max_alive;
extern cvar_t *g_horde_map_monster_spawns;
extern cvar_t *g_horde_map_spawn_chance;
extern cvar_t *g_horde_map_spawn_cooldown;
extern cvar_t *g_horde_map_spawn_min_dist;
extern cvar_t *g_horde_drop_chance;
extern cvar_t *g_horde_drop_profile_bias;
extern cvar_t *g_horde_champion_drop_chance;
extern cvar_t *g_horde_streak_step;
extern cvar_t *g_horde_streak_max_tier;
extern cvar_t *g_horde_streak_score_bonus;
extern cvar_t *g_horde_streak_drop_bonus;
extern cvar_t *g_horde_streak_upgrade_chance;
extern cvar_t *g_horde_wave_survival_bonus;
extern cvar_t *g_horde_reinforcement_kills;
extern cvar_t *g_horde_reinforcements_per_wave;
extern cvar_t *g_horde_reinforcement_protection;
extern cvar_t *g_horde_stall_timeout;
extern cvar_t *g_horde_monster_edge_drops;
extern cvar_t *g_horde_preset_allow_boss_waves;
extern cvar_t *g_horde_preset_chance;
extern cvar_t *g_horde_preset_weight_clone_army;
extern cvar_t *g_horde_preset_weight_funhouse_horde;
extern cvar_t *g_horde_preset_weight_get_over_here;
extern cvar_t *g_horde_preset_weight_giant_horde;
extern cvar_t *g_horde_preset_weight_glass_cannon;
extern cvar_t *g_horde_preset_weight_low_gravity;
extern cvar_t *g_horde_preset_weight_tiny_shamblers;
extern cvar_t *g_horde_preset_weight_tiny_terror;
extern cvar_t *g_horde_preset_weight_pinball_night;
extern cvar_t *g_horde_preset_weight_sawstorm;

namespace muffmode::horde {

constexpr int kMaxMonsterMarkerSlots = POI_HORDE_MONSTER_END - POI_HORDE_MONSTER_0 + 1;
static_assert(kMaxMonsterMarkerSlots > 0, "Horde monster marker POI range must not be empty");
constexpr float kMaxSpawnAnchorWeight = 1'000'000.f;
constexpr gtime_t kEliminatedReminderInterval = 20_sec;

enum class SpawnAnchorKind : int32_t {
	Ground = 1,
	Flying,
	Water,
	Boss,
};

constexpr int     kConvertedAnchorMarker = -1701;
constexpr uint8_t kHordeRewardRegular = 1;
constexpr uint8_t kHordeRewardChampion = 2;
constexpr uint8_t kHordeRewardBoss = 3;
constexpr size_t  kBossHistorySize = 8;
constexpr int     kHordeMaxSummonSlots = 64;
constexpr int     kHordeMaxPowerArmor = 10'000'000;
constexpr int     kHordeMaxMonsterHealth = 10'000'000;

// Wildcard Waves deliberately omit mechanics that can recursively create entities,
// revive without a bounded cost, or leave persistent global state behind.
enum class WildcardPreset : uint8_t {
	None,
	CloneArmy,
	FunhouseHorde,
	GetOverHere,
	GiantHorde,
	GlassCannon,
	LowGravity,
	TinyShamblers,
	TinyTerror,
	PinballNight,
	Sawstorm,
};

constexpr std::array<WildcardPreset, 10> kWildcardPresets = {
	WildcardPreset::CloneArmy,
	WildcardPreset::FunhouseHorde,
	WildcardPreset::GetOverHere,
	WildcardPreset::GiantHorde,
	WildcardPreset::GlassCannon,
	WildcardPreset::LowGravity,
	WildcardPreset::TinyShamblers,
	WildcardPreset::TinyTerror,
	WildcardPreset::PinballNight,
	WildcardPreset::Sawstorm,
};

const char *PresetName(WildcardPreset preset)
{
	switch (preset) {
	case WildcardPreset::CloneArmy:       return "CLONE ARMY";
	case WildcardPreset::FunhouseHorde:   return "FUNHOUSE HORDE";
	case WildcardPreset::GetOverHere:     return "GET OVER HERE!";
	case WildcardPreset::GiantHorde:      return "GIANT HORDE";
	case WildcardPreset::GlassCannon:     return "GLASS CANNON";
	case WildcardPreset::LowGravity:      return "LOW GRAVITY";
	case WildcardPreset::TinyShamblers:   return "TINY SHAMBLERS";
	case WildcardPreset::TinyTerror:      return "TINY TERROR";
	case WildcardPreset::PinballNight:    return "PINBALL NIGHT";
	case WildcardPreset::Sawstorm:        return "SAWSTORM";
	default:                              return "";
	}
}

cvar_t *PresetWeightCvar(WildcardPreset preset)
{
	switch (preset) {
	case WildcardPreset::CloneArmy:       return g_horde_preset_weight_clone_army;
	case WildcardPreset::FunhouseHorde:   return g_horde_preset_weight_funhouse_horde;
	case WildcardPreset::GetOverHere:     return g_horde_preset_weight_get_over_here;
	case WildcardPreset::GiantHorde:      return g_horde_preset_weight_giant_horde;
	case WildcardPreset::GlassCannon:     return g_horde_preset_weight_glass_cannon;
	case WildcardPreset::LowGravity:      return g_horde_preset_weight_low_gravity;
	case WildcardPreset::TinyShamblers:   return g_horde_preset_weight_tiny_shamblers;
	case WildcardPreset::TinyTerror:      return g_horde_preset_weight_tiny_terror;
	case WildcardPreset::PinballNight:    return g_horde_preset_weight_pinball_night;
	case WildcardPreset::Sawstorm:        return g_horde_preset_weight_sawstorm;
	default:                              return nullptr;
	}
}

bool PresetEligible(WildcardPreset preset, int wave)
{
	if (preset == WildcardPreset::TinyShamblers)
		return wave >= 10 && FindMonsterRow("monster_shambler");

	return preset != WildcardPreset::None;
}

struct PresetSpawnTuning {
	float scale = 1.f;
	float health = 1.f;
	float damage = 1.f;
	float movement = 1.f;
	float gravity = 1.f;
};

// Shared by MapMonsterAnchorKind (converted campaign placements) and MonsterHabitat (director
// roster spawns) so the two classifiers can't drift apart. monster_boss2/monster_carrier are
// flyers here too; MapMonsterAnchorKind still resolves them to Boss because its own boss check
// runs first and returns before this list is consulted.
constexpr std::array<const char *, 8> kFlyingMonsterClasses = {
	"monster_flyer", "monster_floater", "monster_hover", "monster_daedalus",
	"monster_fixbot", "monster_kamikaze", "monster_boss2", "monster_carrier",
};

struct DirectorState {
	const BossDefinition                    *boss = nullptr;
	std::array<gentity_t *, MAX_HEALTH_BARS> bossEntities = {};
	std::array<gentity_t *, MAX_HEALTH_BARS> bossHealthBars = {};
	bool                                     bossPending = false;
	int                                      bossUnitsTarget = 0;
	int                                      bossUnitsSpawned = 0;
	int                                      bossSpawnFailures = 0;
	int                                      spawnsInBurst = 0;
	int                                      reinforcementKills = 0;
	int                                      reinforcementsUsed = 0;
	bool                                     reinforcementPending = false;
	std::array<const WeightedItem *, kHordeMonsterCount> featuredRows = {};
	int                     featuredCount = 0;
	int                     featuredCursor = 0;
	int                     featuredRemaining = 0;
	WildcardPreset          preset = WildcardPreset::None;
	const WeightedItem     *presetMonster = nullptr;
	bool                    powerupsPaused = false;
	gtime_t                 lastCombatProgressTime = 0_ms;
	int                     stallRecoveryAttempts = 0;
};

struct BossMapPlacement {
	size_t                   authoredSpots = 0;
	std::vector<gentity_t *> fallbackSpots;
};

DirectorState director;
int           reinforcementCursor = 0;
std::array<const BossDefinition *, kBossHistorySize> bossHistory = {};
size_t        bossHistoryCount = 0;
std::array<BossMapPlacement, kHordeBossCount + 1> bossMapPlacements = {};
std::array<bool, kHordeBossCount> bossMapEligible = {};
bool          mapBossesAvailable = false;
gtime_t       livingCountCacheTime = gtime_t::from_ms(-1);
int           livingThreatCacheCount = 0;
int           livingPressureCacheCount = 0;
bool          livingCountCacheValid = false;

const WeightedItem *PickPresetMonster(WildcardPreset preset)
{
	if (preset == WildcardPreset::TinyShamblers)
		return FindMonsterRow("monster_shambler");
	if (preset != WildcardPreset::CloneArmy)
		return nullptr;

	std::array<const WeightedItem *, kHordeMonsterCount> eligible = {};
	size_t count = 0;
	const int budget = MM_Horde_WavePointBudget();

	for (const WeightedItem &monster : kMonsters) {
		if (monster.min_level != -1 && level.round_number < monster.min_level)
			continue;
		if (monster.max_level != -1 && level.round_number > monster.max_level)
			continue;
		if (monster.spawn_points > budget)
			continue;

		// An all-medic wave can recursively extend itself through corpse revival.
		// Keep Clone Army deterministic in size by excluding both medic roles.
		if (!strcmp(monster.classname, "monster_medic") ||
			!strcmp(monster.classname, "monster_medic_commander"))
			continue;

		eligible[count++] = &monster;
	}

	return count ? eligible[irandom(static_cast<int32_t>(count))] : nullptr;
}

WildcardPreset SelectPreset(bool boss_wave)
{
	if (!MM_Horde_ShouldSelectPreset(boss_wave,
			g_horde_preset_allow_boss_waves->integer != 0,
			g_horde_preset_chance->value, frandom()))
		return WildcardPreset::None;

	std::array<int, kWildcardPresets.size()> cumulative = {};
	int total = 0;
	for (size_t index = 0; index < kWildcardPresets.size(); index++) {
		const WildcardPreset preset = kWildcardPresets[index];
		cvar_t *weight_cvar = PresetWeightCvar(preset);
		const int weight = PresetEligible(preset, level.round_number) && weight_cvar
			? MM_Horde_PresetWeight(weight_cvar->integer)
			: 0;
		total += weight;
		cumulative[index] = total;
	}

	if (total <= 0)
		return WildcardPreset::None;

	const int pick = irandom(total);
	for (size_t index = 0; index < kWildcardPresets.size(); index++)
		if (pick < cumulative[index])
			return kWildcardPresets[index];

	return WildcardPreset::None;
}

PresetSpawnTuning SpawnTuning(WildcardPreset preset)
{
	PresetSpawnTuning tuning;

	switch (preset) {
	case WildcardPreset::FunhouseHorde:
		if (brandom()) {
			tuning.scale = 0.6f;
			tuning.health = 0.55f;
			tuning.damage = 0.75f;
			tuning.movement = 1.2f;
		} else {
			tuning.scale = 1.45f;
			tuning.health = 1.5f;
			tuning.damage = 1.2f;
			tuning.movement = 0.8f;
		}
		break;
	case WildcardPreset::GiantHorde:
		tuning.scale = 1.35f;
		tuning.health = 1.6f;
		tuning.damage = 1.1f;
		tuning.movement = 0.75f;
		break;
	case WildcardPreset::GlassCannon:
		tuning.health = 0.4f;
		tuning.damage = 1.8f;
		break;
	case WildcardPreset::LowGravity:
		tuning.gravity = 0.55f;
		break;
	case WildcardPreset::TinyShamblers:
		tuning.scale = 0.55f;
		tuning.health = 0.4f;
		tuning.damage = 0.65f;
		tuning.movement = 1.25f;
		break;
	case WildcardPreset::TinyTerror:
		tuning.scale = 0.6f;
		tuning.health = 0.35f;
		tuning.damage = 0.7f;
		tuning.movement = 1.5f;
		break;
	case WildcardPreset::PinballNight:
		tuning.gravity = 0.65f;
		break;
	default:
		break;
	}

	tuning.scale = MM_Horde_PresetEntityScale(tuning.scale);
	return tuning;
}

float FiniteSeconds(float value, float fallback, float maximum)
{
	return MM_Horde_ClampFiniteFloat(value, fallback, 0.f, maximum);
}

bool Active()
{
	return g_gametype->integer == static_cast<int>(GT_HORDE);
}

const BossDefinition *SelectBossForWave(int wave);
void BuildBossPlacementCatalog();
bool BossHasMapPlacement(const BossDefinition *boss);
const std::vector<gentity_t *> &BossFallbackSpots(const BossDefinition *boss);
void RecoverStalledCombat();

void RecordCombatProgress()
{
	director.lastCombatProgressTime = level.time;
	director.stallRecoveryAttempts = 0;
}

bool BossAlive()
{
	for (gentity_t *boss : director.bossEntities)
		if (boss && boss->inuse && boss->health > 0 && !boss->deadflag)
			return true;

	return false;
}

int LivingBossCount()
{
	int count = 0;
	for (gentity_t *boss : director.bossEntities)
		if (boss && boss->inuse && boss->health > 0 && !boss->deadflag)
			count++;
	return count;
}

int BossEntitySlot(const gentity_t *ent)
{
	if (!ent)
		return -1;

	for (size_t i = 0; i < director.bossEntities.size(); i++)
		if (director.bossEntities[i] == ent)
			return static_cast<int>(i);

	return -1;
}

bool BossEncounterDefeated()
{
	return MM_Horde_BossEncounterDefeated(director.bossPending, LivingBossCount());
}

void ClearBossHealthBars()
{
	for (gentity_t *&bar : director.bossHealthBars) {
		if (!bar)
			continue;

		for (gentity_t *&active_bar : level.health_bar_entities)
			if (active_bar == bar)
				active_bar = nullptr;

		if (bar->inuse)
			G_FreeEntity(bar);
		bar = nullptr;
	}

	// A map-authored target_health_bar may still occupy the other slot; only clear the
	// shared name configstring when nothing is left to display, and restore whatever
	// bar is still active otherwise (mirrors vanilla use_target_healthbar's occupancy scan).
	for (gentity_t *active_bar : level.health_bar_entities) {
		if (active_bar) {
			gi.configstring(CONFIG_HEALTH_BAR_NAME, active_bar->message);
			return;
		}
	}
	gi.configstring(CONFIG_HEALTH_BAR_NAME, "");
}

void AttachBossHealthBar(gentity_t *boss, int slot)
{
	if (!boss || !boss->inuse || slot < 0 || slot >= static_cast<int>(MAX_HEALTH_BARS))
		return;

	// level.health_bar_entities is the shared global slot array (also used by map-authored
	// target_health_bar entities); the boss-unit slot passed in only indexes this director's
	// own bookkeeping arrays, so find a free global slot rather than assuming they match.
	int global_slot = -1;
	for (size_t i = 0; i < level.health_bar_entities.size(); i++) {
		if (!level.health_bar_entities[i]) {
			global_slot = static_cast<int>(i);
			break;
		}
	}
	if (global_slot < 0)
		return; // every health bar slot is already occupied

	gentity_t *bar = G_Spawn();
	bar->classname = "horde_boss_healthbar";
	bar->enemy = boss;
	bar->message = director.boss ? director.boss->display_name : "Boss";
	bar->delay = 2.0f;
	bar->svflags |= SVF_NOCLIENT;

	director.bossHealthBars[slot] = bar;
	level.health_bar_entities[global_slot] = bar;
	gi.configstring(CONFIG_HEALTH_BAR_NAME, bar->message);
}

void RetireBossHealthBar(int slot)
{
	if (slot < 0 || slot >= static_cast<int>(director.bossHealthBars.size()))
		return;

	gentity_t *bar = director.bossHealthBars[slot];
	if (!bar || !bar->inuse)
		return;

	// The HUD keeps a defeated bar visible briefly. Stop it from following the raw
	// boss pointer immediately so a freed/reused entity slot cannot become its target.
	bar->enemy = nullptr;
	bar->timestamp = level.time + 2_sec;
}

void RecordBossHistory(const BossDefinition *boss)
{
	if (!boss || boss == &kFallbackBoss)
		return;

	const size_t move_count = min(bossHistoryCount, bossHistory.size() - 1);
	for (size_t i = move_count; i > 0; i--)
		bossHistory[i] = bossHistory[i - 1];
	bossHistory[0] = boss;
	bossHistoryCount = min(bossHistoryCount + 1, bossHistory.size());
}

int PerformanceTier(const gclient_t *client)
{
	if (!client)
		return 0;

	return MM_Horde_PerformanceTier(client->pers.horde_kill_streak, g_horde_streak_step->integer,
		g_horde_streak_max_tier->integer);
}

void ResetWavePerformance()
{
	for (auto ec : active_clients()) {
		if (!ec->client || !ec->client->pers.connected)
			continue;

		ec->client->pers.horde_wave_kills = 0;
		ec->client->pers.horde_kill_streak = 0;
		ec->client->pers.horde_wave_deaths = 0;
	}
}

void AwardWavePerformance()
{
	const int survival_bonus = max(0, g_horde_wave_survival_bonus->integer);
	if (survival_bonus <= 0 || IsScoringDisabled())
		return;

	for (auto ec : active_clients()) {
		if (!ec->client || !ClientIsPlaying(ec->client) || ec->client->eliminated)
			continue;
		if (ec->client->pers.horde_wave_kills <= 0 || ec->client->pers.horde_wave_deaths > 0)
			continue;

		G_AdjustPlayerScore(ec->client, survival_bonus, false, 0);
		gi.LocClient_Print(ec, PRINT_HIGH, "Flawless wave: +{} score.\n", survival_bonus);
	}
}

void BuildFeaturedRoster()
{
	director.featuredRows.fill(nullptr);
	director.featuredCount = 0;
	director.featuredCursor = 0;
	director.featuredRemaining = 0;

	const int requested = max(0, g_horde_featured_spawns->integer);
	if (requested <= 0)
		return;

	const HordeCategory category = ActiveThemeCategory();
	for (const WeightedItem &monster : kMonsters) {
		if (MM_Horde_MonsterUnlockWave(monster.min_level) != level.round_number)
			continue;
		if (monster.max_level != -1 && level.round_number > monster.max_level)
			continue;
		if (category != HordeCategory::None && (monster.categories & category) == HordeCategory::None)
			continue;

		director.featuredRows[director.featuredCount++] = &monster;
		if (level.horde_wave_roster) {
			const size_t index = static_cast<size_t>(&monster - kMonsters.data());
			level.horde_wave_roster |= 1u << index;
		}
	}

	if (director.featuredCount <= 0)
		return;

	std::shuffle(director.featuredRows.begin(),
		director.featuredRows.begin() + director.featuredCount, mt_rand);
	director.featuredRemaining = requested;
}

const WeightedItem *PeekFeaturedMonster(int remaining_points)
{
	if (director.featuredRemaining <= 0 || director.featuredCount <= 0)
		return nullptr;

	for (int offset = 0; offset < director.featuredCount; offset++) {
		const int index = (director.featuredCursor + offset) % director.featuredCount;
		const WeightedItem *monster = director.featuredRows[index];
		if (monster && monster->spawn_points <= remaining_points)
			return monster;
	}

	return nullptr;
}

void ConsumeFeaturedMonster(const WeightedItem *monster)
{
	if (!monster || director.featuredRemaining <= 0)
		return;

	for (int index = 0; index < director.featuredCount; index++) {
		if (director.featuredRows[index] != monster)
			continue;

		director.featuredCursor = (index + 1) % director.featuredCount;
		director.featuredRemaining--;
		return;
	}
}

bool HasEliminatedFighter()
{
	for (auto ec : active_clients())
		if (ec->client && ClientIsPlaying(ec->client) && ec->client->eliminated &&
			ec->client->sess.team != TEAM_SPECTATOR)
			return true;

	return false;
}

bool ReinforcementAvailable()
{
	return g_horde_reinforcements_per_wave->integer > 0 &&
		director.reinforcementsUsed < g_horde_reinforcements_per_wave->integer;
}

gclient_t *SortedConnectedClient(size_t slot)
{
	if (slot >= q_countof(level.sorted_clients))
		return nullptr;

	const int client_num = level.sorted_clients[slot];
	if (client_num < 0 || client_num >= static_cast<int>(game.maxclients))
		return nullptr;

	gclient_t *client = &game.clients[client_num];
	if (!client->pers.connected)
		return nullptr;

	return client;
}

int MonsterMarkerSlots()
{
	return clamp(g_horde_mark_monsters_max->integer, 1, kMaxMonsterMarkerSlots);
}

bool ClientWantsMonsterMarkers(gclient_t *cl)
{
	if (!cl || !cl->pers.connected)
		return false;
	if (ClientIsPlaying(cl))
		return true;

	return cl->eliminated && cl->sess.team != TEAM_SPECTATOR;
}

bool IsLivingHostileMonster(const gentity_t *ent)
{
	if (!ent->inuse || !(ent->svflags & SVF_MONSTER))
		return false;
	if (ent->health <= 0 || ent->deadflag || (ent->svflags & SVF_DEADMONSTER))
		return false;
	if (ent->monsterinfo.aiflags & AI_GOOD_GUY)
		return false;

	return true;
}

bool IsLivingThreat(const gentity_t *ent)
{
	return IsLivingHostileMonster(ent) && !(ent->monsterinfo.aiflags & AI_DO_NOT_COUNT);
}

void InvalidateLivingCounts()
{
	livingCountCacheValid = false;
}

void RefreshLivingCounts()
{
	// Memoized per server frame: this is a full entity-list scan, and it is legitimately
	// queried from several independent places within the same frame (spawning, monster
	// markers, wave-clear check, and once per connected player for the HUD stat). Spawn,
	// death, and cleanup paths explicitly invalidate it so same-frame mutations are visible.
	if (livingCountCacheValid && livingCountCacheTime == level.time)
		return;

	int threats = 0;
	int pressure = 0;
	for (size_t i = 1; i < globals.num_entities; i++) {
		const gentity_t *ent = &g_entities[i];
		if (!IsLivingHostileMonster(ent))
			continue;

		pressure++;
		if (!(ent->monsterinfo.aiflags & AI_DO_NOT_COUNT))
			threats++;
	}

	livingCountCacheTime = level.time;
	livingThreatCacheCount = threats;
	livingPressureCacheCount = pressure;
	livingCountCacheValid = true;
}

int LivingThreatCount()
{
	RefreshLivingCounts();
	return livingThreatCacheCount;
}

int LivingPressureCount()
{
	RefreshLivingCounts();
	return livingPressureCacheCount;
}

void SendMonsterPoi(gentity_t *player, int slot, const vec3_t &pos)
{
	gi.WriteByte(svc_poi);
	gi.WriteShort(static_cast<uint16_t>(POI_HORDE_MONSTER_0 + slot));
	gi.WriteShort(600);
	gi.WritePosition(pos);
	gi.WriteShort(level.pic_ping);
	gi.WriteByte(208);
	gi.WriteByte(POI_FLAG_NONE);
	gi.unicast(player, false);
}

void ClearMonsterPoi(gentity_t *player, int slot)
{
	gi.WriteByte(svc_poi);
	gi.WriteShort(static_cast<uint16_t>(POI_HORDE_MONSTER_0 + slot));
	gi.WriteShort(0xFFFF);
	gi.WritePosition(vec3_origin);
	gi.WriteShort(0);
	gi.WriteByte(0);
	gi.WriteByte(POI_FLAG_NONE);
	gi.unicast(player, false);
}

void ClearMonsterPoisForClient(gentity_t *player)
{
	const int slots = MonsterMarkerSlots();

	for (int slot = 0; slot < slots; slot++)
		ClearMonsterPoi(player, slot);
}

void ClearMonsterPoisForAll()
{
	for (auto ec : active_clients()) {
		if (!ec->client || !ClientWantsMonsterMarkers(ec->client))
			continue;

		ClearMonsterPoisForClient(ec);
	}

	level.horde_mark_living = -1;
}

void UpdateMonsterMarkers()
{
	if (!Active())
		return;
	if (level.round_state != roundst_t::ROUND_IN_PROGRESS)
		return;

	const int threshold = g_horde_mark_monsters_threshold->integer;
	const int living = LivingThreatCount();
	const bool boss_alive = BossAlive();
	const bool mark_remaining = threshold >= 1 && living <= threshold;

	if (!boss_alive && !mark_remaining) {
		if (level.horde_mark_living >= 0 && level.horde_mark_living <= threshold)
			ClearMonsterPoisForAll();

		level.horde_mark_living = static_cast<int16_t>(living);
		return;
	}

	const bool newly_marking = mark_remaining &&
		(level.horde_mark_living > threshold || level.horde_mark_living < 0);
	const bool count_changed = level.horde_mark_living != living;
	const bool throttle = level.horde_mark_time > level.time && !count_changed;

	if (throttle)
		return;

	level.horde_mark_time = level.time + 500_ms;
	level.horde_mark_living = static_cast<int16_t>(living);

	if (newly_marking) {
		for (auto ec : active_clients()) {
			if (!ec->client || !ClientWantsMonsterMarkers(ec->client))
				continue;

			gi.local_sound(ec, CHAN_AUTO, gi.soundindex("misc/help_marker.wav"), 1.f, ATTN_NORM, 0, GetUnicastKey());
		}
	}

	const int max_slots = MonsterMarkerSlots();
	std::array<gentity_t *, kMaxMonsterMarkerSlots> marked = {};
	int                                             num_marked = 0;

	for (gentity_t *boss : director.bossEntities) {
		if (num_marked >= max_slots)
			break;
		if (boss && boss->inuse && boss->health > 0 && !boss->deadflag)
			marked[num_marked++] = boss;
	}

	if (mark_remaining) {
		for (size_t i = 1; i < globals.num_entities && num_marked < max_slots; i++) {
			gentity_t *ent = &g_entities[i];

			if (!IsLivingThreat(ent))
				continue;

			bool already_marked = false;
			for (int slot = 0; slot < num_marked; slot++)
				if (marked[slot] == ent) {
					already_marked = true;
					break;
				}
			if (already_marked)
				continue;

			marked[num_marked++] = ent;
		}
	}

	for (auto ec : active_clients()) {
		if (!ec->client || !ClientWantsMonsterMarkers(ec->client))
			continue;

		for (int slot = 0; slot < max_slots; slot++) {
			if (slot < num_marked) {
				vec3_t pos = marked[slot]->s.origin;
				pos[2] += marked[slot]->maxs[2] * 0.5f;
				SendMonsterPoi(ec, slot, pos);
			} else {
				ClearMonsterPoi(ec, slot);
			}
		}
	}
}

int LivesPerWave()
{
	return max(1, g_horde_lives->integer);
}

bool ClientIsActiveFighter(gentity_t *ec)
{
	if (!ec->client || !ClientIsPlaying(ec->client))
		return false;
	if (ec->client->eliminated)
		return false;
	if (ec->health > 0)
		return true;

	return ec->client->pers.lives > 0;
}

bool HasActiveFighter()
{
	for (auto ec : active_clients()) {
		if (ClientIsActiveFighter(ec))
			return true;
	}

	return false;
}

void GrantWaveLives()
{
	const int lives = LivesPerWave();

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client))
			continue;

		const bool was_eliminated = ec->client->eliminated;

		ec->client->pers.lives = lives;
		ec->client->eliminated = false;
		ec->client->horde_elim_msg_next = 0_sec;

		// Eliminated fighters spectate in freecam with deadflag cleared and health restored.
		if (was_eliminated || ec->deadflag || ec->health <= 0)
			ClientRespawn(ec);
	}
}

gentity_t *PickEliminatedFighter()
{
	const int max_clients = static_cast<int>(game.maxclients);
	if (max_clients <= 0)
		return nullptr;

	for (int offset = 0; offset < max_clients; offset++) {
		const int slot = (reinforcementCursor + offset) % max_clients;
		gentity_t *ent = &g_entities[slot + 1];

		if (!ent->inuse || !ent->client || !ent->client->pers.connected)
			continue;
		if (!ClientIsPlaying(ent->client) || !ent->client->eliminated ||
			ent->client->sess.team == TEAM_SPECTATOR)
			continue;

		reinforcementCursor = (slot + 1) % max_clients;
		return ent;
	}

	return nullptr;
}

void ProcessReinforcement()
{
	if (!director.reinforcementPending)
		return;

	director.reinforcementPending = false;
	if (!ReinforcementAvailable())
		return;

	gentity_t *fighter = PickEliminatedFighter();
	if (!fighter)
		return;

	fighter->client->pers.lives = 1;
	fighter->client->eliminated = false;
	fighter->client->horde_elim_msg_next = 0_sec;
	fighter->client->respawn_time = level.time;

	const float protection_sec = FiniteSeconds(g_horde_reinforcement_protection->value, 2.f, 60.f);
	fighter->client->pers.horde_reinforcement_protection = gtime_t::from_sec(protection_sec);
	ClientRespawn(fighter);

	director.reinforcementsUsed = MM_Horde_SaturatingIncrement(director.reinforcementsUsed);
	director.reinforcementKills = 0;
	gi.LocBroadcast_Print(PRINT_CENTER, "{} has rallied back into the fight!", fighter->client->resp.netname);
	gi.positioned_sound(fighter->s.origin, fighter, CHAN_AUTO | CHAN_RELIABLE,
		gi.soundindex("items/protect.wav"), 1, ATTN_NONE, 0);
}

float MultiplierFromFighters(int fighters)
{
	if (!g_horde_player_scale->integer)
		return 1.f;

	const float factor = MM_Horde_ClampFiniteFloat(g_horde_player_scale_factor->value,
		0.4f, 0.f, 10.f);

	return 1.f + (fighters - 1) * factor;
}

float MapScaleMultiplier()
{
	if (!g_horde_map_scale->integer)
		return 1.f;

	if (level.horde_map_scale_mult != 0.f)
		return level.horde_map_scale_mult;

	if (level.num_spawn_spots < 2)
	{
		level.horde_map_scale_mult = 1.f;
		return 1.f;
	}

	vec3_t bmin = level.spawn_spots[0]->s.origin;
	vec3_t bmax = bmin;
	for (int i = 1; i < level.num_spawn_spots; i++)
	{
		const vec3_t &o = level.spawn_spots[i]->s.origin;
		bmin.x = min(bmin.x, o.x);
		bmin.y = min(bmin.y, o.y);
		bmin.z = min(bmin.z, o.z);
		bmax.x = max(bmax.x, o.x);
		bmax.y = max(bmax.y, o.y);
		bmax.z = max(bmax.z, o.z);
	}

	const float diagonal = (bmax - bmin).length();
	const float ref      = MM_Horde_ClampFiniteFloat(g_horde_map_scale_ref->value, 4000.f, 1.f, 1'000'000.f);
	const float factor   = MM_Horde_ClampFiniteFloat(g_horde_map_scale_factor->value, 0.5f, 0.f, 10.f);
	const float ratio    = diagonal / ref;
	const float mult     = 1.f + (ratio - 1.f) * factor;

	level.horde_map_scale_mult = MM_Horde_ClampFiniteFloat(mult, 1.f, 0.1f, 100.f);
	return level.horde_map_scale_mult;
}

bool ClassnameMatches(const char *classname, const char *candidate)
{
	return classname && candidate && !Q_strcasecmp(classname, candidate);
}

bool IsExcludedMapMonster(const char *classname)
{
	static constexpr std::array<const char *, 4> excluded = {
		"monster_commander_body",
		"monster_boss3_stand",
		"monster_tank_stand",
		"monster_turret",
	};

	for (const char *candidate : excluded)
		if (ClassnameMatches(classname, candidate))
			return true;

	return false;
}

const BossDefinition *MachineGamesBossProfile(const gentity_t *ent)
{
	if (!ent || !ent->classname || !ent->targetname || !*level.mapname)
		return nullptr;

	struct MapBossIdentity {
		const char *map;
		const char *targetname;
		const char *classname;
		const char *profile;
	};

	static constexpr std::array<MapBossIdentity, 19> identities = {{
		{ "mgu1m3", "boss2", "monster_boss2", "gate_warden" },
		{ "mgu1m5", "makron1", "monster_makron", "makron" },
		{ "mgu1m5", "makron2", "monster_makron", "children_of_makron" },
		{ "mgu1m5", "makron3", "monster_makron", "children_of_makron" },
		{ "mgu2m2", "beast", "monster_mutant", "bloodstarved_mutant" },
		{ "mgu3m4", "boss1", "monster_supertank", "strogg_supertank" },
		{ "mgu3m4", "boss_2", "monster_carrier", "strogg_carrier" },
		{ "mgu3m4", "boss3", "monster_boss5", "strogg_megatank" },
		{ "mgu3secret", "boss", "monster_carrier", "ancient_carrier" },
		{ "mgu4m1", "boss1", "monster_tank_commander", "commander" },
		{ "mgu4m3", "boss", "monster_carrier", "garbage_carrier" },
		{ "mgu5m2", "arachnid_mini_boss", "monster_arachnid", "arachnid" },
		{ "mgu5m3", "enemies_boss", "monster_makron", "system_administrator" },
		{ "mgu5m3", "small_guy", "monster_supertank", "janitor" },
		{ "mgu6m1", "boss_room_plat_boss", "monster_supertank", "overburden" },
		{ "mgu6m2", "silverkey_boss", "monster_supertank", "underminer" },
		{ "mgu6m3", "mother", "monster_shambler", "modir" },
		{ "mguboss", "boss1", "monster_boss2", "servitor_of_creation" },
		{ "mguboss", "boss2", "monster_supertank", "servitors_of_creation" },
	}};

	for (const auto &identity : identities)
		if (!Q_strcasecmp(level.mapname, identity.map) &&
			!Q_strcasecmp(ent->targetname, identity.targetname) &&
			!Q_strcasecmp(ent->classname, identity.classname))
			return FindBossDefinition(identity.profile);

	if (!Q_strcasecmp(level.mapname, "mguboss") &&
		(!Q_strcasecmp(ent->targetname, "boss3a") || !Q_strcasecmp(ent->targetname, "boss3b")) &&
		!Q_strcasecmp(ent->classname, "monster_shambler"))
		return FindBossDefinition("masters_of_the_machine");

	return nullptr;
}

SpawnAnchorKind MapMonsterAnchorKind(const gentity_t *ent)
{
	if (!ent || !ent->classname)
		return SpawnAnchorKind::Ground;

	const char *classname = ent->classname;

	if (MachineGamesBossProfile(ent))
		return SpawnAnchorKind::Boss;

	if (ClassnameMatches(classname, "monster_flipper"))
		return SpawnAnchorKind::Water;
	if (ClassnameMatches(classname, "monster_gekk") &&
		(gi.pointcontents(ent->s.origin) & CONTENTS_WATER))
		return SpawnAnchorKind::Water;

	static constexpr std::array<const char *, 9> bosses = {
		"monster_supertank", "monster_boss2", "monster_jorg",
		"monster_makron", "monster_guardian", "monster_boss5",
		"monster_carrier", "monster_widow", "monster_widow2",
	};
	for (const char *candidate : bosses)
		if (ClassnameMatches(classname, candidate))
			return SpawnAnchorKind::Boss;

	for (const char *candidate : kFlyingMonsterClasses)
		if (ClassnameMatches(classname, candidate))
			return SpawnAnchorKind::Flying;

	return SpawnAnchorKind::Ground;
}

const char *AnchorClassname(SpawnAnchorKind kind)
{
	switch (kind) {
	case SpawnAnchorKind::Flying: return "info_horde_flying_spawn";
	case SpawnAnchorKind::Water: return "info_horde_water_spawn";
	case SpawnAnchorKind::Boss: return "info_horde_boss_spawn";
	default: return "info_horde_spawn";
	}
}

void SpawnAnchor(gentity_t *ent, SpawnAnchorKind kind)
{
	if (!Active()) {
		G_FreeEntity(ent);
		return;
	}

	const bool converted = ent->mass == kConvertedAnchorMarker;
	ent->mass = 0;
	if (converted && kind == SpawnAnchorKind::Boss) {
		ent->sounds = static_cast<uint32_t>(ent->spawnflags);
		ent->noise_index2 = 1;
	}
	// Difficulty/gametype editor flags have already been consumed by G_InhibitEntity.
	// No source-monster behavior flags should leak onto the inert anchor.
	ent->spawnflags = SPAWNFLAG_NONE;
	ent->style = static_cast<int32_t>(kind);
	ent->count = max(0, ent->count);
	ent->health = max(0, ent->health);
	if (ent->health > 0 && ent->count > ent->health)
		std::swap(ent->count, ent->health);
	ent->random = MM_Horde_ClampFiniteFloat(ent->random, 1.f, 0.01f,
		kMaxSpawnAnchorWeight);
	if (!std::isfinite(ent->wait) || ent->wait < 0.f)
		ent->wait = 0.f;

	if (ent->map && strncmp(ent->map, "monster_", 8)) {
		gi.Com_PrintFmt("{}: horde_monster must be a monster_* classname\n", *ent);
		G_FreeEntity(ent);
		return;
	}

	if (kind == SpawnAnchorKind::Boss && ent->message && *ent->message) {
		const BossDefinition *boss = FindBossDefinition(ent->message);
		if (!boss) {
			gi.Com_PrintFmt("{}: unknown horde_boss profile '{}'\n", *ent, ent->message);
			G_FreeEntity(ent);
			return;
		}
		if (ent->map && Q_strcasecmp(ent->map, boss->classname)) {
			gi.Com_PrintFmt("{}: horde_boss '{}' requires {}, not {}\n",
				*ent, boss->id, boss->classname, ent->map);
			G_FreeEntity(ent);
			return;
		}
		if (!ent->map)
			ent->map = boss->classname;
	}

	if (kind == SpawnAnchorKind::Boss && ent->noise_index2 > 0) {
		const uint32_t raw_flags = ent->sounds &
			~static_cast<uint32_t>(SPAWNFLAG_EDITOR_MASK);
		ent->sounds = raw_flags & ~7u;
		// Common monster bits 1/2/4 can leave a director boss dormant. Shambler
		// bit 1 is the sole class-specific overlap and retains its precise attack.
		if (ClassnameMatches(ent->map, "monster_shambler") && (raw_flags & 1u))
			ent->sounds |= 1u;
	}

	// Converted campaign placements express affinity; explicit horde_monster
	// keys are strict so bespoke encounters remain deterministic.
	ent->dmg = (!converted && ent->map && *ent->map) ? 1 : 0;
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_NOT;
	ent->svflags |= SVF_NOCLIENT;
	gi.linkentity(ent);
}

} // namespace muffmode::horde

bool MM_Horde_ConvertMapMonsterSpawn(gentity_t *ent)
{
	if (!horde::Active() || !ent || !ent->classname ||
		strncmp(ent->classname, "monster_", 8) || horde::IsExcludedMapMonster(ent->classname))
		return false;

	const char *source_classname = ent->classname;
	const horde::BossDefinition *boss_profile = horde::MachineGamesBossProfile(ent);
	const horde::SpawnAnchorKind kind = horde::MapMonsterAnchorKind(ent);
	const float authored_health_multiplier = st.was_key_specified("health_multiplier")
		? st.health_multiplier
		: 0.f;
	const bool authored_power_armor_type = st.was_key_specified("power_armor_type");
	const bool authored_power_armor_power = st.was_key_specified("power_armor_power");
	const bool authored_monster_slots = st.was_key_specified("monster_slots");
	const char *authored_reinforcements = st.was_key_specified("reinforcements")
		? st.reinforcements
		: nullptr;

	ent->classname = horde::AnchorClassname(kind);
	ent->map = source_classname;
	// Boss anchors retain map-authored tuning in otherwise-unused generic fields.
	ent->model = kind == horde::SpawnAnchorKind::Boss ? authored_reinforcements : nullptr;
	ent->message = boss_profile ? boss_profile->id : nullptr;
	ent->target = nullptr;
	ent->targetname = nullptr;
	ent->killtarget = nullptr;
	ent->pathtarget = nullptr;
	ent->deathtarget = nullptr;
	ent->healthtarget = nullptr;
	ent->itemtarget = nullptr;
	ent->combattarget = nullptr;
	ent->count = 0;
	ent->health = 0;
	ent->random = 1.f;
	ent->wait = 0.f;
	ent->speed = kind == horde::SpawnAnchorKind::Boss ? authored_health_multiplier : 0.f;
	ent->accel = 0.f;
	ent->decel = kind == horde::SpawnAnchorKind::Boss && authored_power_armor_type ? 1.f : 0.f;
	ent->volume = kind == horde::SpawnAnchorKind::Boss && authored_power_armor_power ? 1.f : 0.f;
	ent->attenuation = kind == horde::SpawnAnchorKind::Boss && authored_monster_slots ? 1.f : 0.f;
	ent->noise_index = kind == horde::SpawnAnchorKind::Boss && authored_reinforcements ? 1 : 0;
	ent->dmg = 0;
	ent->mass = horde::kConvertedAnchorMarker;
	// Horde always runs with deathmatch 1, so G_InhibitEntity (called right after this by the
	// SpawnEntities loop) would otherwise free every converted anchor whose source monster was
	// authored NOT_DEATHMATCH -- true of virtually all campaign monster_* placements. Preserve
	// the source monster's skill inhibition by translating a mismatch into that DM filter.
	const bool skill_inhibited = MM_Horde_SourceMonsterInhibitedBySkill(skill->integer,
		ent->spawnflags.has(SPAWNFLAG_NOT_EASY),
		ent->spawnflags.has(SPAWNFLAG_NOT_MEDIUM),
		ent->spawnflags.has(SPAWNFLAG_NOT_HARD));
	if (skill_inhibited)
		ent->spawnflags |= SPAWNFLAG_NOT_DEATHMATCH;
	else
		ent->spawnflags &= ~SPAWNFLAG_NOT_DEATHMATCH;
	return true;
}

void SP_info_horde_spawn(gentity_t *ent)
{
	horde::SpawnAnchor(ent, horde::SpawnAnchorKind::Ground);
}

void SP_info_horde_flying_spawn(gentity_t *ent)
{
	horde::SpawnAnchor(ent, horde::SpawnAnchorKind::Flying);
}

void SP_info_horde_water_spawn(gentity_t *ent)
{
	horde::SpawnAnchor(ent, horde::SpawnAnchorKind::Water);
}

void SP_info_horde_boss_spawn(gentity_t *ent)
{
	horde::SpawnAnchor(ent, horde::SpawnAnchorKind::Boss);
}

int MM_Horde_LivingThreatCount()
{
	return horde::Active() ? horde::LivingThreatCount() : 0;
}

int MM_Horde_CountFighters()
{
	int fighters = 0;

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client) || ec->health <= 0 || ec->client->eliminated)
			continue;
		fighters++;
	}

	const int max_fighters = clamp(g_horde_player_scale_max->integer, 1, 32);
	return clamp(max(fighters, 1), 1, max_fighters);
}

namespace {

float EffectiveLateWaveFactor()
{
	const bool escalation = g_horde_late_escalation->integer != 0;
	return MM_Horde_EffectiveLateWaveFactor(escalation, g_horde_late_wave_factor->value,
		g_horde_late_budget_factor->value);
}

int MaxAliveCap()
{
	const bool escalation = g_horde_late_escalation->integer != 0;
	return MM_Horde_LateMaxAlive(g_horde_max_alive->integer, level.round_number,
		g_horde_content_peak_wave->integer, g_horde_late_max_alive_per_wave->integer,
		g_horde_late_max_alive_cap->integer, escalation);
}

} // namespace

int MM_Horde_WavePointBudget()
{
	const int   fighters = MM_Horde_CountFighters();
	const float pmult    = horde::MultiplierFromFighters(fighters);
	const float msmult   = horde::MapScaleMultiplier();
	const int   base     = g_horde_points_base->integer;
	const int   per_wave = g_horde_points_per_wave->integer;
	const int   min_pts  = g_horde_points_min->integer;
	const int   max_pts  = g_horde_points_max->integer;
	const int   peak     = g_horde_content_peak_wave->integer;

	// Linear up to the tuned content peak, then tapered. The host-tested helper keeps
	// extreme cvar/round values from overflowing or invoking undefined float-to-int casts.
	return MM_Horde_ComputeWaveBudget(level.round_number, base, per_wave, min_pts, max_pts,
		peak, EffectiveLateWaveFactor(), pmult, msmult);
}

namespace muffmode::horde {

gtime_t SpawnInterval(bool warmup, float adaptive_mult)
{
	if (warmup)
		return 5_sec;

	const float min_sec = MM_Horde_ClampFiniteFloat(g_horde_spawn_interval_min->value,
		0.3f, 0.05f, 60.f);
	const float max_sec = MM_Horde_ClampFiniteFloat(g_horde_spawn_interval_max->value,
		0.5f, min_sec, 60.f);
	gtime_t interval = random_time(gtime_t::from_sec(min_sec), gtime_t::from_sec(max_sec));

	if (!warmup && g_horde_enhanced_ai->integer && adaptive_mult != 1.f) {
		interval = gtime_t::from_ms(static_cast<int64_t>(interval.milliseconds() / max(adaptive_mult, 0.1f)));
		interval = max(interval, gtime_t::from_ms(100));
	}

	return interval;
}

gtime_t SpawnDelayAfterSuccess(bool warmup, float adaptive_mult, bool boss)
{
	if (warmup)
		return 5_sec;
	if (boss) {
		director.spawnsInBurst = 0;
		return 2500_ms;
	}

	gtime_t delay = SpawnInterval(false, adaptive_mult);
	director.spawnsInBurst = MM_Horde_SaturatingIncrement(director.spawnsInBurst);

	const int burst_size = max(0, g_horde_spawn_burst_count->integer);
	if (MM_Horde_ShouldRestAfterSpawn(director.spawnsInBurst, burst_size)) {
		director.spawnsInBurst = 0;
		delay += gtime_t::from_sec(FiniteSeconds(g_horde_spawn_burst_rest->value, 2.f, 300.f));
	}

	return delay;
}

} // namespace muffmode::horde

bool MM_Horde_ShouldSkipEntitiesReset()
{
	return horde::Active();
}

int MM_Horde_CountdownWaveNumber()
{
	if (notGT(GT_HORDE))
		return MM_Horde_SaturatingIncrement(level.round_number);

	if (!level.round_number && g_horde_starting_wave->integer > 0)
		return g_horde_starting_wave->integer;

	return MM_Horde_SaturatingIncrement(level.round_number);
}

void MM_Horde_AdvanceRoundNumber()
{
	if (notGT(GT_HORDE))
		return;

	if (!level.round_number && g_horde_starting_wave->integer > 0)
		level.round_number = g_horde_starting_wave->integer;
	else
		level.round_number = MM_Horde_SaturatingIncrement(level.round_number);
}

void MM_Horde_OnRoundCountdown()
{
	if (notGT(GT_HORDE))
		return;

	horde::GrantWaveLives();

	// [MuffMode] Clear techs at the countdown to the next wave so none linger during downtime;
	// a fresh set is spawned at wave start (MM_Horde_BeginWave).
	if (g_horde_tech_reset_each_wave->integer)
		Tech_HordeClear();
}

void MM_Horde_OnRoundStarted()
{
	if (notGT(GT_HORDE))
		return;

	// Begin the wave first so the theme is chosen before we announce it.
	MM_Horde_BeginWave();

	gi.LocBroadcast_Print(PRINT_CHAT, "Wave {} has begun!\n", level.round_number);
	if (horde::director.boss && horde::director.preset != horde::WildcardPreset::None)
		gi.LocBroadcast_Print(PRINT_CENTER, "BOSS WAVE\n{}\nWILDCARD: {}",
			horde::director.boss->display_name, horde::PresetName(horde::director.preset));
	else if (horde::director.boss)
		gi.LocBroadcast_Print(PRINT_CENTER, "BOSS WAVE\n{}", horde::director.boss->display_name);
	else if (horde::director.preset != horde::WildcardPreset::None)
		gi.LocBroadcast_Print(PRINT_CENTER, "WILDCARD WAVE\n{}",
			horde::PresetName(horde::director.preset));
	else if (const horde::ThemeDefinition *theme = horde::FindTheme(static_cast<horde::Theme>(level.horde_wave_theme)))
		gi.LocBroadcast_Print(PRINT_CENTER, "{}", theme->announce);
	else
		gi.LocBroadcast_Print(PRINT_CENTER, brandom() ? "INCOMING!" : "LOCK AND LOAD!");
	MM_Announce(mm_announce_event_t::FightWithBackup, world);
}

void MM_Horde_NotifyEliminatedSpectator(gentity_t *ent)
{
	if (!horde::Active())
		return;
	if (level.round_state != roundst_t::ROUND_IN_PROGRESS)
		return;
	if (!ent || !ent->client || !ent->client->eliminated)
		return;
	if (ent->client->sess.team == TEAM_SPECTATOR)
		return;
	if (level.time < ent->client->horde_elim_msg_next)
		return;

	ent->client->horde_elim_msg_next = level.time + horde::kEliminatedReminderInterval;
	if (horde::ReinforcementAvailable()) {
		const int needed = max(1, g_horde_reinforcement_kills->integer) -
			horde::director.reinforcementKills;
		gi.LocClient_Print(ent, PRINT_CENTER,
			"Your squad can rally you back.\n{} monster {} needed.",
			max(1, needed), needed == 1 ? "kill" : "kills");
	} else {
		gi.LocClient_Print(ent, PRINT_CENTER, "You will rejoin when the next wave countdown begins.");
	}
}

void MM_Horde_OnPlayerDeath(gentity_t *ent)
{
	if (!horde::Active())
		return;
	if (level.round_state != roundst_t::ROUND_IN_PROGRESS)
		return;
	if (!ent || !ent->client || !ClientIsPlaying(ent->client))
		return;

	const bool had_eliminated_fighter = horde::HasEliminatedFighter();

	ent->client->pers.horde_wave_deaths =
		MM_Horde_SaturatingIncrement(ent->client->pers.horde_wave_deaths);
	ent->client->pers.horde_kill_streak = 0;

	if (ent->client->pers.lives > 0)
		ent->client->pers.lives--;

	horde::Adaptive_RecordPlayerDeath();

	if (ent->client->pers.lives <= 0) {
		ClientSetEliminated(ent);
		if (!had_eliminated_fighter)
			horde::director.reinforcementKills = 0;
		ent->client->respawn_time = level.time + 1_sec;
		MM_Horde_NotifyEliminatedSpectator(ent);
	}
}

void MM_Horde_OnMonsterKilled(gentity_t *ent)
{
	if (!horde::Active() || level.round_state != roundst_t::ROUND_IN_PROGRESS || !ent)
		return;

	horde::InvalidateLivingCounts();
	if (!(ent->monsterinfo.aiflags & AI_DO_NOT_COUNT))
		horde::RecordCombatProgress();

	const bool counted_reward = ent->monsterinfo.horde_reward_class != 0 &&
		!(ent->monsterinfo.aiflags & AI_DO_NOT_COUNT);
	if (!counted_reward)
		ent->monsterinfo.horde_reward_class = 0;
	gentity_t *killer = ent->monsterinfo.damage_attacker;
	gclient_t *killer_client = killer && killer->client && killer->client->pers.connected
		? killer->client
		: nullptr;
	int performance_tier = 0;

	if (counted_reward && killer_client) {
		const int old_tier = horde::PerformanceTier(killer_client);
		killer_client->pers.horde_wave_kills =
			MM_Horde_SaturatingIncrement(killer_client->pers.horde_wave_kills);
		killer_client->pers.horde_kill_streak =
			MM_Horde_SaturatingIncrement(killer_client->pers.horde_kill_streak);
		performance_tier = horde::PerformanceTier(killer_client);

		if (performance_tier > old_tier)
			gi.LocClient_Print(killer, PRINT_HIGH,
				"Horde momentum tier {}: bonus score and improved drops.\n", performance_tier);
	}

	if (counted_reward) {
		gitem_t *drop = nullptr;

		if (ent->monsterinfo.horde_reward_class == horde::kHordeRewardBoss) {
			drop = horde::PickBossDrop(g_horde_boss_powerup_chance->value);
		} else if (ent->monsterinfo.horde_reward_class == horde::kHordeRewardChampion) {
			const float chance = MM_Horde_DropChance(g_horde_champion_drop_chance->value,
				g_horde_streak_drop_bonus->value, performance_tier);

			if (frandom() < chance) {
				// When techs are enabled, roll a random tech instead of the champion's
				// strong-item pool -- no other Horde monster drops techs.
				drop = AllowTechs()
					? GetItemByIndex(tech_ids[irandom(static_cast<int32_t>(q_countof(tech_ids)))])
					: horde::PickChampionDrop();

				const float upgrade_chance = MM_Horde_DropChance(0.f,
					g_horde_streak_upgrade_chance->value, performance_tier);
				if (drop && frandom() < upgrade_chance)
					drop = horde::UpgradeDrop(drop);
			}
		} else {
			const float chance = MM_Horde_DropChance(g_horde_drop_chance->value,
				g_horde_streak_drop_bonus->value, performance_tier);

			if (frandom() < chance) {
				const horde::WeightedItem *row = horde::FindMonsterRow(ent->classname);
				const std::array<item_id_t, 8> *drops = row ? &row->drops : nullptr;
				if (!drops) {
					if (const horde::DirectorMonster *aquatic = horde::FindAquaticRow(ent->classname))
						drops = &aquatic->drops;
				}
				drop = horde::PickDropItem(drops);

				const float upgrade_chance = MM_Horde_DropChance(0.f,
					g_horde_streak_upgrade_chance->value, performance_tier);
				if (drop && frandom() < upgrade_chance)
					drop = horde::UpgradeDrop(drop);
			}
		}

		ent->item = drop;
	}

	const int boss_slot = horde::BossEntitySlot(ent);
	const bool boss_unit_killed = boss_slot >= 0;
	bool boss_defeated = false;
	if (boss_unit_killed) {
		horde::director.bossEntities[boss_slot] = nullptr;
		horde::RetireBossHealthBar(boss_slot);
		boss_defeated = horde::BossEncounterDefeated();
		horde::ClearMonsterPoisForAll();

		if (boss_defeated) {
			gi.LocBroadcast_Print(PRINT_CENTER, "{} defeated!", horde::director.boss ?
				horde::director.boss->display_name : "Boss");
		} else if (!horde::director.bossPending && horde::LivingBossCount() > 0) {
			gi.LocBroadcast_Print(PRINT_CENTER, "{}\n{} remaining",
				horde::director.boss ? horde::director.boss->display_name : "Bosses",
				horde::LivingBossCount());
		}
	}

	if (!counted_reward || !horde::HasEliminatedFighter() || !horde::ReinforcementAvailable() ||
		horde::director.reinforcementPending)
		return;

	if (boss_defeated) {
		horde::director.reinforcementPending = true;
		return;
	}

	horde::director.reinforcementKills =
		MM_Horde_SaturatingIncrement(horde::director.reinforcementKills);
	if (MM_Horde_ReinforcementReady(horde::director.reinforcementKills,
			g_horde_reinforcement_kills->integer, horde::director.reinforcementsUsed,
			g_horde_reinforcements_per_wave->integer))
		horde::director.reinforcementPending = true;
}

void MM_Horde_OnMonsterDamaged(gentity_t *ent, int damage)
{
	if (!horde::Active() || level.round_state != roundst_t::ROUND_IN_PROGRESS ||
		!ent || damage <= 0 || !(ent->svflags & SVF_MONSTER) ||
		(ent->monsterinfo.aiflags & (AI_GOOD_GUY | AI_DO_NOT_COUNT)))
		return;

	horde::RecordCombatProgress();
}

bool MM_Horde_CheckAllFightersLost()
{
	if (!horde::Active())
		return false;
	if (level.round_state != roundst_t::ROUND_IN_PROGRESS)
		return false;
	if (level.num_playing_clients < 1)
		return false;
	if (horde::HasActiveFighter())
		return false;

	gi.Broadcast_Print(PRINT_CENTER, "DEFEATED!");
	QueueIntermission("ALL FIGHTERS LOST!", true, false);
	return true;
}

bool MM_Horde_CheckDesertionDefeat()
{
	if (!horde::Active())
		return false;
	if (level.match_state != matchst_t::MATCH_IN_PROGRESS)
		return false;
	if (level.intermission_queued || level.intermission_time)
		return false;

	gi.Broadcast_Print(PRINT_CENTER, "DEFEATED!");
	QueueIntermission("ALL FIGHTERS LOST!", true, false);
	return true;
}

void MM_Horde_CleanWaveTransition()
{
	if (!horde::Active())
		return;

	horde::ClearBossHealthBars();

	// Remove dead corpses plus any live non-counted summons/resurrections between
	// waves (Horde skips Entities_Reset).
	for (size_t i = globals.num_entities; i > 1; i--) {
		gentity_t *ent = &g_entities[i - 1];

		if (!ent->inuse)
			continue;
		if (!(ent->svflags & SVF_MONSTER))
			continue;
		const bool auxiliary = (ent->monsterinfo.aiflags & AI_DO_NOT_COUNT) &&
			!(ent->monsterinfo.aiflags & AI_GOOD_GUY);
		if (!auxiliary && ent->health > 0 && !ent->deadflag && !(ent->svflags & SVF_DEADMONSTER))
			continue;

		G_FreeEntity(ent);
	}

	level.total_monsters = 0;
	level.killed_monsters = 0;

	if (g_debug_monster_kills->integer)
		level.monsters_registered.fill(nullptr);

	horde::InvalidateLivingCounts();

	horde::ClearMonsterPoisForAll();
	level.horde_mark_time = 0_ms;
}

void MM_Horde_OnRoundEnd()
{
	if (notGT(GT_HORDE))
		return;

	horde::AwardWavePerformance();
	horde::director.powerupsPaused = true;
	horde::Adaptive_RecordWaveEnd();
	level.horde_all_spawned = false;
	MM_Horde_CleanWaveTransition();
}

bool MM_Horde_PowerupsPaused()
{
	return horde::Active() && horde::director.powerupsPaused;
}

void MM_Horde_PauseClientPowerups(gentity_t *ent)
{
	if (!MM_Horde_PowerupsPaused() || !ent || !ent->client)
		return;

	const int64_t frame_ms = static_cast<int64_t>(gi.frame_time_ms);
	if (frame_ms <= 0)
		return;
	const gtime_t frame = gtime_t::from_ms(frame_ms);

	auto pause_deadline = [frame](gtime_t &deadline) {
		if (deadline <= level.time)
			return;

		const int64_t deadline_ms = deadline.milliseconds();
		const int64_t frame_ms = frame.milliseconds();
		if (deadline_ms > std::numeric_limits<int64_t>::max() - frame_ms)
			deadline = gtime_t::from_ms(std::numeric_limits<int64_t>::max());
		else
			deadline += frame;
	};

	pause_deadline(ent->client->pu_time_quad);
	pause_deadline(ent->client->pu_time_haste);
	pause_deadline(ent->client->pu_time_double);
	pause_deadline(ent->client->pu_time_protection);
	pause_deadline(ent->client->pu_time_invisibility);
	pause_deadline(ent->client->pu_time_regeneration);
	pause_deadline(ent->client->pu_time_rebreather);
	pause_deadline(ent->client->pu_time_enviro);
	pause_deadline(ent->client->ir_time);
	pause_deadline(ent->client->tech_expire_time);
	pause_deadline(ent->client->pu_regen_time_regen);
	pause_deadline(ent->client->tech_regen_time);
}

bool MM_Horde_UpdateRoundInProgress()
{
	if (notGT(GT_HORDE))
		return false;

	horde::ProcessReinforcement();

	if (MM_Horde_CheckAllFightersLost())
		return false;

	MM_Horde_RunSpawning();
	horde::UpdateMonsterMarkers();
	horde::RecoverStalledCombat();

	if (MM_Horde_WaveCleared(level.horde_all_spawned, horde::LivingThreatCount())) {
		gi.LocBroadcast_Print(PRINT_CENTER, "Monsters eliminated!\n");
		gi.positioned_sound(world->s.origin, world, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex("ctf/flagcap.wav"), 1, ATTN_NONE, 0);
		return true;
	}

	return false;
}

bool MM_Horde_CheckMatchEnd()
{
	if (notGT(GT_HORDE))
		return false;

	if (roundlimit->integer <= 0 || level.round_number < roundlimit->integer)
		return false;

	gclient_t *winner = horde::SortedConnectedClient(0);
	if (!winner)
		QueueIntermission("MATCH ENDED", false, false);
	else
		QueueIntermission(G_Fmt("{} WINS with a final score of {}.", winner->resp.netname,
			winner->resp.score).data(),
			false, false);
	return true;
}

bool MM_Horde_SkipFragScoreLimit()
{
	return horde::Active();
}

bool MM_Horde_SkipMercyLimit()
{
	return horde::Active();
}

void MM_Horde_Init()
{
	if (notGT(GT_HORDE))
		return;

	horde::director = {};
	horde::reinforcementCursor = 0;
	horde::bossHistory = {};
	horde::bossHistoryCount = 0;
	horde::InvalidateLivingCounts();
	horde::ResetRuntimeState();

	// The monster table's content curve peaks at waves 11-12; the global default
	// roundlimit of 8 would end the match before heavies and commanders appear.
	// Apply a horde default of 12 once per load, only when still at the global default.
	static bool roundlimit_defaulted = false;
	if (!roundlimit_defaulted) {
		roundlimit_defaulted = true;
		if (roundlimit->integer == 8) {
			gi.cvar_forceset("roundlimit", "12");
			gi.Com_PrintFmt("MM_Horde: roundlimit at global default (8), using horde default of 12.\n");
		}
	}

	horde::PrecacheTableMonsters();
	horde::PrecacheDirectorMonsters();
	horde::PrecacheRewardItems();

	// [MuffMode] Expiry cue for timed techs (g_horde_tech_duration); precache here since the
	// power-armor items that normally register it may not be present in a Horde match.
	gi.soundindex("misc/power2.wav");
	gi.soundindex("items/protect.wav");
	gi.modelindex("models/items/spawngro3/tris.md2");
}

void MM_Horde_FinalizeLevelSpawns()
{
	if (GT(GT_HORDE))
		horde::BuildBossPlacementCatalog();
}

void MM_Horde_BeginWave()
{
	if (notGT(GT_HORDE))
		return;

	MM_Horde_CleanWaveTransition();

	// [MuffMode] Horde spawns a fresh set of techs at the start of each wave (cleared at the
	// previous countdown). Persistence (g_horde_tech_reset_each_wave 0) uses the map-load spawn.
	if (g_horde_tech_reset_each_wave->integer)
		Tech_HordeSpawnWave();

	horde::director = {};
	horde::RecordCombatProgress();
	horde::ResetWavePerformance();
	const bool forced_boss_fits = g_horde_boss_force->string && *g_horde_boss_force->string &&
		horde::BossHasMapPlacement(horde::FindBossDefinition(g_horde_boss_force->string));
	if (g_horde_boss_waves->integer && (horde::mapBossesAvailable || forced_boss_fits) &&
		MM_Horde_IsBossWave(level.round_number, g_horde_boss_min_wave->integer,
			g_horde_boss_interval->integer)) {
		horde::director.boss = horde::SelectBossForWave(level.round_number);
		if (horde::director.boss) {
			horde::director.bossUnitsTarget =
				horde::EffectiveBossUnits(*horde::director.boss);
			horde::director.bossPending = horde::director.bossUnitsTarget > 0;
			horde::RecordBossHistory(horde::director.boss);
		}
	}

	horde::director.preset = horde::SelectPreset(horde::director.bossPending);
	horde::director.presetMonster = horde::PickPresetMonster(horde::director.preset);
	if ((horde::director.preset == horde::WildcardPreset::CloneArmy ||
			horde::director.preset == horde::WildcardPreset::TinyShamblers) &&
		!horde::director.presetMonster)
		horde::director.preset = horde::WildcardPreset::None;

	// Pick this wave's theme. Rare (g_horde_theme_chance), never the same as the previous
	// themed wave, and only themes whose monsters exist by this wave are eligible.
	{
		const horde::Theme prev = static_cast<horde::Theme>(level.horde_wave_theme);
		horde::Theme chosen = horde::Theme::None;

		if (!horde::director.bossPending &&
			horde::director.preset == horde::WildcardPreset::None &&
			g_horde_themed_waves->integer &&
			level.round_number >= g_horde_theme_min_wave->integer &&
			frandom() < MM_Horde_Probability(g_horde_theme_chance->value, 0.2f)) {
			std::array<const horde::ThemeDefinition *, horde::kHordeThemeCount> eligible = {};
			size_t num_eligible = 0;

			for (const auto &def : horde::kThemes) {
				if (level.round_number < def.min_wave || def.theme == prev)
					continue;
				// Skip themes that can't field enough on-category bodies at this wave, so a banner
				// never shows for a theme that would spawn off-category fillers (or nothing).
				if (horde::CountThemeCandidates(def.category, level.round_number) < g_horde_theme_min_monsters->integer)
					continue;
				eligible[num_eligible++] = &def;
			}

			if (num_eligible > 0)
				chosen = eligible[irandom(static_cast<int32_t>(num_eligible))]->theme;
		}

		level.horde_wave_theme = static_cast<int8_t>(chosen);
	}

	// Build this wave's monster roster: a random subset of the eligible types so runs vary.
	// Non-themed waves only (a themed wave is already a category subset). 0 = unrestricted.
	level.horde_wave_roster = 0;
	if (horde::director.preset == horde::WildcardPreset::None &&
		g_horde_wave_variety->integer &&
		static_cast<horde::Theme>(level.horde_wave_theme) == horde::Theme::None) {
		std::array<int, horde::kHordeMonsterCount> eligible = {};
		std::array<int, horde::kHordeMonsterCount> cheap = {};
		size_t num_eligible = 0;
		size_t num_cheap = 0;
		int min_cost = INT_MAX;

		for (size_t i = 0; i < horde::kMonsters.size(); i++) {
			const horde::WeightedItem &m = horde::kMonsters[i];
			if (m.min_level != -1 && level.round_number < m.min_level)
				continue;
			if (m.max_level != -1 && level.round_number > m.max_level)
				continue;
			eligible[num_eligible++] = static_cast<int>(i);
			min_cost = min(min_cost, m.spawn_points);
		}

		const int min_types = MM_Horde_EffectiveMinTypes(g_horde_wave_min_types->integer,
			level.round_number, g_horde_wave_type_ramp->integer, static_cast<int>(num_eligible));
		if (num_eligible > static_cast<size_t>(min_types)) {
			for (size_t k = 0; k < num_eligible; k++)
				if (horde::kMonsters[eligible[k]].spawn_points == min_cost)
					cheap[num_cheap++] = eligible[k];

			const int roster_size = irandom(min_types, static_cast<int32_t>(num_eligible) + 1);	// inclusive max
			uint32_t  mask = 0;
			int       picked = 0;

			// Guarantee one random cheap grunt so the budget always spends down cleanly.
			if (num_cheap > 0) {
				mask |= 1u << cheap[irandom(static_cast<int32_t>(num_cheap))];
				picked = 1;
			}

			// Shuffle the eligible list, then fill the remaining roster slots from it.
			std::shuffle(eligible.begin(), eligible.begin() + num_eligible, mt_rand);

			for (size_t k = 0; k < num_eligible && picked < roster_size; k++) {
				if (mask & (1u << eligible[k]))
					continue;	// already seeded the grunt
				mask |= 1u << eligible[k];
				picked++;
			}

			level.horde_wave_roster = mask;
		}
	}

	// Make each newly unlocked role visible instead of leaving progression entirely
	// to weighted chance. The featured picks are folded into random rosters too.
	if (horde::director.preset == horde::WildcardPreset::None)
		horde::BuildFeaturedRoster();

	// Decide whether this wave hosts a champion.
	// Up to the content peak: spend the per-run budget (mm_match seeds 0-2), spread across the waves
	// remaining until the peak. Past the peak the budget is gone, so switch to a steady per-wave rate
	// derived from the same knobs (max_per_run * champion_chance champions per peak-length span) so
	// champions keep appearing at the tuned cadence for any wave count.
	level.horde_champion_pending = false;
	if (!horde::director.bossPending &&
		horde::director.preset == horde::WildcardPreset::None &&
		g_horde_champions->integer &&
		level.round_number >= g_horde_champion_min_wave->integer) {
		if (horde::IsLateWave()) {
			const int64_t span64 = static_cast<int64_t>(g_horde_content_peak_wave->integer) -
				static_cast<int64_t>(g_horde_champion_min_wave->integer) + 1;
			const int span = static_cast<int>(std::clamp<int64_t>(span64, 1,
				std::numeric_limits<int>::max()));
			const float champions_per_run = MM_Horde_ClampFiniteFloat(
				g_horde_champion_max_per_run->value, 2.f, 0.f, 127.f);
			const float chance = MM_Horde_Probability(g_horde_champion_chance->value, 0.6f);
			const float rate = champions_per_run * chance / span;

			if (frandom() < MM_Horde_Probability(rate))
				level.horde_champion_pending = true;
		} else if (level.horde_champions_remaining > 0) {
			// Spread the run's budget across the waves left until the peak (== roundlimit for the
			// standard 12-wave run, so that case is unchanged).
			const int last_budget_wave = roundlimit->integer > 0
				? min(roundlimit->integer, g_horde_content_peak_wave->integer)
				: g_horde_content_peak_wave->integer;
			const int64_t waves_left64 = static_cast<int64_t>(last_budget_wave) -
				static_cast<int64_t>(level.round_number) + 1;
			const int waves_left = static_cast<int>(std::clamp<int64_t>(waves_left64, 1,
				std::numeric_limits<int>::max()));

			if (frandom() < static_cast<float>(level.horde_champions_remaining) / waves_left) {
				level.horde_champion_pending = true;
				level.horde_champions_remaining--;
			}
		}
	}

	// DEBUG/TEST: force a champion every wave (overrides the roll above), regardless of min_wave.
	if (!horde::director.bossPending &&
		horde::director.preset == horde::WildcardPreset::None &&
		g_horde_champion_force->integer)
		level.horde_champion_pending = true;

	const int fighters = MM_Horde_CountFighters();
	level.horde_fighters_snapshotted = static_cast<int8_t>(fighters);
	level.horde_spawn_points_remaining = MM_Horde_WavePointBudget();

	if (horde::director.bossPending)
		level.horde_spawn_points_remaining = MM_Horde_ScaleInt(
			level.horde_spawn_points_remaining,
			MM_Horde_ClampFiniteFloat(g_horde_boss_budget_mult->value, 0.8f, 0.1f, 10.f),
			level.horde_spawn_points_remaining, 1);
	else if (const horde::ThemeDefinition *theme = horde::FindTheme(static_cast<horde::Theme>(level.horde_wave_theme)))
		level.horde_spawn_points_remaining = MM_Horde_ScaleInt(
			level.horde_spawn_points_remaining, theme->budget_mult,
			level.horde_spawn_points_remaining, 1);

	horde::Adaptive_BeginWave();

	const int delay_ms = max(0, g_horde_wave_spawn_delay_ms->integer);
	level.horde_monster_spawn_time = level.time + gtime_t::from_ms(delay_ms);
}

// Horde spawn points are deathmatch player spawns; their origins are placed for
// the player hull (which gets a +9 lift and stuck-fixing in client spawn code) and
// can sit low enough that monster hulls start embedded in the floor — on bloodrun
// every spawn origin is only 15u above its floor. A monster spawned embedded in a
// thin floor gets teleported through it by M_droptofloor (a trace does not clip
// against a brush it starts inside), e.g. into the blood pool under the walkway at
// 1104 208 -633. Lift the origin clear before validating, and nudge as a fallback.
// Also rejects spots whose ground is liquid. Returns false if the spot is unusable.
namespace muffmode::horde {

constexpr float kMaxValidatedCoordinate = 1'000'000.f;
constexpr float kMaxValidatedHullOffset = 8192.f;

bool OriginFinite(const vec3_t &origin)
{
	for (size_t axis = 0; axis < 3; axis++)
		if (!std::isfinite(origin[axis]) || std::abs(origin[axis]) > kMaxValidatedCoordinate)
			return false;

	return true;
}

bool HullBoundsValid(const vec3_t &mins, const vec3_t &maxs)
{
	for (size_t axis = 0; axis < 3; axis++) {
		if (!std::isfinite(mins[axis]) || !std::isfinite(maxs[axis]) ||
			std::abs(mins[axis]) > kMaxValidatedHullOffset ||
			std::abs(maxs[axis]) > kMaxValidatedHullOffset ||
			mins[axis] > maxs[axis])
			return false;
	}

	return true;
}

bool ValidateSpawnOrigin(vec3_t &origin, const vec3_t &check_mins, const vec3_t &check_maxs)
{
	if (!OriginFinite(origin) || !HullBoundsValid(check_mins, check_maxs))
		return false;

	// Lift only enough to put the hull bottom above the spawn plane. Tall bosses
	// commonly have mins.z == 0, so a fixed 16-unit lift needlessly rejects them
	// beneath ceilings they would fit after M_droptofloor.
	origin[2] += max(1.f, -check_mins[2]);
	if (!OriginFinite(origin))
		return false;

	if (!CheckSpawnPoint(origin, check_mins, check_maxs)) {
		if (G_FixStuckObject_Generic(origin, check_mins, check_maxs,
				[](const vec3_t &start, const vec3_t &mins, const vec3_t &maxs, const vec3_t &end) {
					return gi.trace(start, mins, maxs, end, nullptr, MASK_MONSTERSOLID);
				}) == stuck_result_t::NO_GOOD_POSITION)
			return false;
		if (!CheckSpawnPoint(origin, check_mins, check_maxs))
			return false;
	}

	trace_t tr = gi.trace(origin, check_mins, check_maxs, origin - vec3_t{ 0.f, 0.f, 64.f }, nullptr, MASK_MONSTERSOLID);
	if (gi.pointcontents(tr.endpos) & (CONTENTS_LAVA | CONTENTS_SLIME))
		return false;

	return true;
}

constexpr vec3_t kDefaultSpawnMins = { -32.f, -32.f, -16.f };
constexpr vec3_t kDefaultSpawnMaxs = { 32.f, 32.f, 64.f };
constexpr float  kTau = 6.28318530717958647692f;
constexpr float  kWaterSpawnMinRadius = 128.0f;
constexpr float  kWaterSpawnMaxRadius = 512.0f;

int CountLivingAquatics()
{
	int count = 0;

	for (size_t i = 1; i < globals.num_entities; i++) {
		gentity_t *ent = &g_entities[i];
		if (!IsLivingHostileMonster(ent))
			continue;
		if ((ent->flags & FL_SWIM) || ClassnameMatches(ent->classname, "monster_flipper"))
			count++;
	}

	return count;
}

bool MonsterRequiresWater(const gentity_t *monster)
{
	return monster && ((monster->flags & FL_SWIM) ||
		ClassnameMatches(monster->classname, "monster_flipper"));
}

bool WaterSpawnFarEnoughFromFighters(const vec3_t &origin)
{
	if (!OriginFinite(origin))
		return false;

	for (auto ec : active_clients())
		if (ClientIsPlaying(ec->client) && ec->health > 0 && !ec->client->eliminated &&
			(origin - ec->s.origin).length() < kWaterSpawnMinRadius)
			return false;

	return true;
}

bool FullySubmerged(const vec3_t &origin, const vec3_t &mins, const vec3_t &maxs)
{
	if (!OriginFinite(origin) || !HullBoundsValid(mins, maxs))
		return false;

	auto is_safe_water = [](const vec3_t &sample) {
		if (!OriginFinite(sample))
			return false;
		const contents_t contents = gi.pointcontents(sample);
		return (contents & CONTENTS_WATER) && !(contents & (CONTENTS_LAVA | CONTENTS_SLIME));
	};

	if (!is_safe_water(origin))
		return false;

	const std::array<float, 2> x_offsets = { mins[0] + 2.f, maxs[0] - 2.f };
	const std::array<float, 2> y_offsets = { mins[1] + 2.f, maxs[1] - 2.f };
	const std::array<float, 2> z_offsets = { mins[2] + 2.f, maxs[2] - 2.f };

	for (float x : x_offsets) {
		for (float y : y_offsets) {
			for (float z : z_offsets) {
				if (!is_safe_water(origin + vec3_t{ x, y, z }))
					return false;
			}
		}
	}

	const trace_t tr = gi.trace(origin, mins, maxs, origin, nullptr, MASK_MONSTERSOLID);
	return !tr.startsolid && !tr.allsolid;
}

bool ValidateAuthoredOrigin(vec3_t &origin, const vec3_t &mins, const vec3_t &maxs,
	SpawnAnchorKind kind)
{
	if (!OriginFinite(origin) || !HullBoundsValid(mins, maxs))
		return false;

	if (!CheckSpawnPoint(origin, mins, maxs)) {
		if (G_FixStuckObject_Generic(origin, mins, maxs,
				[](const vec3_t &start, const vec3_t &trace_mins, const vec3_t &trace_maxs,
					const vec3_t &end) {
					return gi.trace(start, trace_mins, trace_maxs, end, nullptr, MASK_MONSTERSOLID);
				}) == stuck_result_t::NO_GOOD_POSITION)
			return false;
		if (!CheckSpawnPoint(origin, mins, maxs))
			return false;
	}

	if (gi.pointcontents(origin) & (CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME))
		return false;

	if (kind == SpawnAnchorKind::Flying)
		return true;

	const trace_t floor = gi.trace(origin, mins, maxs, origin - vec3_t{ 0.f, 0.f, 64.f },
		nullptr, MASK_MONSTERSOLID);
	return !(gi.pointcontents(floor.endpos) & (CONTENTS_LAVA | CONTENTS_SLIME));
}

bool OriginSharesFighterPHS(const vec3_t &origin)
{
	if (!OriginFinite(origin))
		return false;

	for (auto ec : active_clients())
		if (ClientIsPlaying(ec->client) && ec->health > 0 && !ec->client->eliminated &&
			gi.inPHS(origin, ec->s.origin, true))
			return true;

	return false;
}

bool ThreatInsideWorld(const gentity_t *monster)
{
	if (!monster || !OriginFinite(monster->s.origin) ||
		!HullBoundsValid(monster->mins, monster->maxs) ||
		(gi.pointcontents(monster->s.origin) & CONTENTS_SOLID))
		return false;

	// World containment must ignore overlapping actors; a zero-length
	// MASK_MONSTERSOLID trace would misclassify a valid crowded fight as BSP escape.
	const trace_t hull = gi.trace(monster->s.origin, monster->mins, monster->maxs,
		monster->s.origin, monster, MASK_SOLID);
	return !hull.startsolid && !hull.allsolid;
}

bool IsAnchorEntity(const gentity_t *ent, SpawnAnchorKind kind)
{
	if (!ent || !ent->inuse || ent->style != static_cast<int32_t>(kind) || !ent->classname)
		return false;

	return !Q_strcasecmp(ent->classname, AnchorClassname(kind));
}

size_t BossPlacementIndex(const BossDefinition *boss)
{
	if (!boss)
		return bossMapPlacements.size();
	for (size_t i = 0; i < kBosses.size(); i++)
		if (boss == &kBosses[i])
			return i;
	if (boss == &kFallbackBoss)
		return kHordeBossCount;
	return bossMapPlacements.size();
}

bool BossAnchorSupportsProfile(gentity_t *anchor, const BossDefinition &boss)
{
	if (!IsAnchorEntity(anchor, SpawnAnchorKind::Boss))
		return false;
	if (anchor->message && *anchor->message && Q_strcasecmp(anchor->message, boss.id))
		return false;

	const bool exact_affinity = anchor->map && boss.classname &&
		!Q_strcasecmp(anchor->map, boss.classname);
	if (anchor->dmg && !exact_affinity)
		return false;

	vec3_t mins = boss.mins * EffectiveBossScale(boss);
	vec3_t maxs = boss.maxs * EffectiveBossScale(boss);
	if (anchor->s.scale > 0.f) {
		const float base_scale = EffectiveBossScale(boss);
		const float anchor_scale = EffectiveBossScale(boss, anchor->s.scale);
		const float ratio = anchor_scale / max(0.05f, base_scale);
		mins *= ratio;
		maxs *= ratio;
	}

	vec3_t origin = anchor->s.origin;
	return ValidateAuthoredOrigin(origin, mins, maxs, SpawnAnchorKind::Boss);
}

void BuildBossPlacementCatalog()
{
	for (BossMapPlacement &placement : bossMapPlacements) {
		placement.authoredSpots = 0;
		placement.fallbackSpots.clear();
	}
	bossMapEligible = {};
	mapBossesAvailable = false;

	auto inspect_profile = [](const BossDefinition &boss, size_t slot) {
		BossMapPlacement &placement = bossMapPlacements[slot];

		for (size_t i = 1; i < globals.num_entities; i++)
			if (BossAnchorSupportsProfile(&g_entities[i], boss))
				placement.authoredSpots++;

		const float scale = EffectiveBossScale(boss);
		const vec3_t mins = boss.mins * scale;
		const vec3_t maxs = boss.maxs * scale;
		for (int i = 0; i < level.num_spawn_spots; i++) {
			gentity_t *spot = level.spawn_spots[i];
			if (!spot || !spot->inuse)
				continue;

			vec3_t origin = spot->s.origin;
			if (ValidateSpawnOrigin(origin, mins, maxs))
				placement.fallbackSpots.push_back(spot);
		}

		return MM_Horde_BossPlacementSufficient(placement.authoredSpots,
			placement.fallbackSpots.size(), EffectiveBossUnits(boss));
	};

	for (size_t i = 0; i < kBosses.size(); i++) {
		bossMapEligible[i] = inspect_profile(kBosses[i], i);
		mapBossesAvailable = mapBossesAvailable || bossMapEligible[i];
	}
	inspect_profile(kFallbackBoss, kHordeBossCount);

	size_t compatible_profiles = 0;
	size_t marked_fallbacks = 0;
	for (size_t i = 0; i < kBosses.size(); i++) {
		if (!bossMapEligible[i])
			continue;
		compatible_profiles++;
		marked_fallbacks += bossMapPlacements[i].fallbackSpots.size();
	}

	if (mapBossesAvailable)
		gi.Com_PrintFmt("MM_Horde: level-load boss placement catalog marked {} fallback uses across {} compatible profiles.\n",
			marked_fallbacks, compatible_profiles);
	else
		gi.Com_Print("MM_Horde: no boss profile fits authored anchors or player-spawn fallbacks; boss waves disabled for this map.\n");
}

bool BossHasMapPlacement(const BossDefinition *boss)
{
	const size_t slot = BossPlacementIndex(boss);
	if (slot >= bossMapPlacements.size())
		return false;
	if (slot < kHordeBossCount)
		return bossMapEligible[slot];

	const BossMapPlacement &placement = bossMapPlacements[slot];
	return MM_Horde_BossPlacementSufficient(placement.authoredSpots,
		placement.fallbackSpots.size(), EffectiveBossUnits(*boss));
}

const std::vector<gentity_t *> &BossFallbackSpots(const BossDefinition *boss)
{
	static const std::vector<gentity_t *> empty;
	const size_t slot = BossPlacementIndex(boss);
	return slot < bossMapPlacements.size() ? bossMapPlacements[slot].fallbackSpots : empty;
}

bool AnchorHasFighterContext(const vec3_t &origin)
{
	if (!OriginFinite(origin))
		return false;

	const float min_distance = MM_Horde_ClampFiniteFloat(g_horde_map_spawn_min_dist->value,
		192.f, 0.f, 1'000'000.f);
	bool any_fighter = false;
	bool in_phs = false;

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client) || ec->health <= 0 || ec->client->eliminated)
			continue;

		any_fighter = true;
		if ((origin - ec->s.origin).length() < min_distance)
			return false;
		if (gi.inPHS(origin, ec->s.origin, true))
			in_phs = true;
	}

	return any_fighter && in_phs;
}

bool PickAuthoredSpawn(SpawnAnchorKind kind, const char *monster_class,
	const vec3_t &mins, const vec3_t &maxs, vec3_t &out_origin, vec3_t &out_angles,
	gentity_t *&out_anchor, const BossDefinition *boss = nullptr)
{
	out_anchor = nullptr;
	if (!g_horde_map_monster_spawns->integer)
		return false;

	float total_weight = 0.f;
	for (size_t i = 1; i < globals.num_entities; i++) {
		gentity_t *anchor = &g_entities[i];
		if (!IsAnchorEntity(anchor, kind))
			continue;
		if (anchor->count > 0 && level.round_number < anchor->count)
			continue;
		if (anchor->health > 0 && level.round_number > anchor->health)
			continue;
		if (anchor->timestamp > level.time)
			continue;
		if (kind == SpawnAnchorKind::Boss && anchor->message && *anchor->message &&
			(!boss || Q_strcasecmp(anchor->message, boss->id)))
			continue;

		const bool exact_affinity = anchor->map && monster_class &&
			!Q_strcasecmp(anchor->map, monster_class);
		if (anchor->dmg && !exact_affinity)
			continue;

		vec3_t origin = anchor->s.origin;
		vec3_t candidate_mins = mins;
		vec3_t candidate_maxs = maxs;
		if (boss && anchor->s.scale > 0.f) {
			const float base_scale = EffectiveBossScale(*boss);
			const float anchor_scale = EffectiveBossScale(*boss, anchor->s.scale);
			const float ratio = anchor_scale / max(0.05f, base_scale);
			candidate_mins *= ratio;
			candidate_maxs *= ratio;
		}
		const bool valid = kind == SpawnAnchorKind::Water
			? FullySubmerged(origin, candidate_mins, candidate_maxs)
			: ValidateAuthoredOrigin(origin, candidate_mins, candidate_maxs, kind);
		if (!valid || !AnchorHasFighterContext(origin))
			continue;

		float weight = max(0.01f, anchor->random);
		if (exact_affinity)
			weight *= 4.f;

		total_weight += weight;
		if (!out_anchor || frandom() * total_weight < weight) {
			out_anchor = anchor;
			out_origin = origin;
			out_angles = anchor->s.angles;
		}
	}

	return out_anchor != nullptr;
}

const BossDefinition *PickAuthoredBossProfile(int wave)
{
	if (!g_horde_map_monster_spawns->integer)
		return nullptr;

	const BossDefinition *chosen = nullptr;
	float total_weight = 0.f;

	for (size_t i = 1; i < globals.num_entities; i++) {
		gentity_t *anchor = &g_entities[i];
		if (!IsAnchorEntity(anchor, SpawnAnchorKind::Boss) ||
			!anchor->message || !*anchor->message)
			continue;
		if (anchor->count > 0 && wave < anchor->count)
			continue;
		if (anchor->health > 0 && wave > anchor->health)
			continue;

		const BossDefinition *boss = FindBossDefinition(anchor->message);
		if (!boss || !BossHasMapPlacement(boss) || !BossAvailableForWave(*boss, wave))
			continue;

		const float weight = max(0.01f, anchor->random);
		total_weight += weight;
		if (!chosen || frandom() * total_weight < weight)
			chosen = boss;
	}

	return chosen;
}

const BossDefinition *SelectBossForWave(int wave)
{
	if (g_horde_boss_force->string && *g_horde_boss_force->string) {
		if (const BossDefinition *forced = FindBossDefinition(g_horde_boss_force->string)) {
			if (!BossHasMapPlacement(forced)) {
				gi.Com_PrintFmt("MM_Horde: forced boss '{}' has no level-load-validated placement on this map; boss wave skipped.\n",
					forced->id);
				return nullptr;
			}
			if (forced->units <= 1 || g_horde_boss_pairs->integer)
				return forced;

			gi.Com_PrintFmt("MM_Horde: forced boss '{}' is a pair but g_horde_boss_pairs is disabled.\n",
				forced->id);
		} else {
			gi.Com_PrintFmt("MM_Horde: unknown g_horde_boss_force profile '{}'.\n",
				g_horde_boss_force->string);
		}
	}

	if (const BossDefinition *authored = PickAuthoredBossProfile(wave))
		return authored;

	return PickBossForWave(wave, bossHistory.data(), bossHistoryCount, &bossMapEligible);
}

SpawnAnchorKind MonsterHabitat(const char *classname)
{
	for (const char *candidate : kFlyingMonsterClasses)
		if (ClassnameMatches(classname, candidate))
			return SpawnAnchorKind::Flying;

	return SpawnAnchorKind::Ground;
}

bool PickWaterSpawn(const DirectorMonster *&out_monster, int remaining_points, vec3_t &out_origin,
	vec3_t &out_angles, gentity_t *&out_anchor)
{
	out_monster = nullptr;
	out_anchor = nullptr;

	if (!g_horde_water_spawns->integer ||
		frandom() >= MM_Horde_Probability(g_horde_water_spawn_chance->value, 0.3f))
		return false;

	const DirectorMonster *monster = PickAquaticForWave(level.round_number, remaining_points);
	if (!monster)
		return false;

	const int max_alive = max(0, g_horde_water_max_alive->integer);
	if (max_alive > 0 && CountLivingAquatics() >= max_alive)
		return false;

	if (PickAuthoredSpawn(SpawnAnchorKind::Water, monster->classname, monster->mins, monster->maxs,
			out_origin, out_angles, out_anchor)) {
		out_monster = monster;
		return true;
	}

	std::array<gentity_t *, MAX_CLIENTS> water_fighters = {};
	int num_water_fighters = 0;

	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client) || ec->health <= 0 || ec->client->eliminated ||
			ec->waterlevel < WATER_WAIST)
			continue;
		if (num_water_fighters < static_cast<int>(water_fighters.size()))
			water_fighters[num_water_fighters++] = ec;
	}

	if (num_water_fighters == 0)
		return false;

	for (int attempt = 0; attempt < 20; attempt++) {
		gentity_t *target = water_fighters[irandom(num_water_fighters)];
		const float angle = frandom() * kTau;
		const float radius = kWaterSpawnMinRadius +
			frandom() * (kWaterSpawnMaxRadius - kWaterSpawnMinRadius);
		vec3_t origin = target->s.origin + vec3_t{
			std::cos(angle) * radius,
			std::sin(angle) * radius,
			crandom() * 64.0f,
		};

		if (!gi.inPHS(origin, target->s.origin, true))
			continue;
		if (!WaterSpawnFarEnoughFromFighters(origin) ||
			!FullySubmerged(origin, monster->mins, monster->maxs))
			continue;

		out_monster = monster;
		out_origin = origin;
		out_angles = { 0.f, vectoyaw(target->s.origin - origin), 0.f };
		return true;
	}

	return false;
}

void CommitAuthoredSpawn(gentity_t *anchor, gentity_t *monster)
{
	if (!anchor || !anchor->inuse || !monster || !monster->inuse)
		return;

	const float cooldown = anchor->wait > 0.f
		? FiniteSeconds(anchor->wait, 0.f, 3600.f)
		: FiniteSeconds(g_horde_map_spawn_cooldown->value, 3.f, 3600.f);
	anchor->timestamp = level.time + gtime_t::from_sec(cooldown);

	monster->deathtarget = anchor->deathtarget;
	monster->healthtarget = anchor->healthtarget;
	monster->itemtarget = anchor->itemtarget;

	if (anchor->target || anchor->killtarget)
		G_UseTargets(anchor, monster);
}

void RecordSpawnFailure(bool warmup, bool boss)
{
	if (boss) {
		director.bossSpawnFailures = MM_Horde_SaturatingIncrement(director.bossSpawnFailures);
		if (director.bossSpawnFailures >= 3) {
			if (director.bossUnitsSpawned > 0) {
				gi.Com_PrintFmt("MM_Horde: {} deployed {}/{} encounter units; continuing with the active boss set.\n",
					director.boss ? director.boss->display_name : "boss",
					director.bossUnitsSpawned, director.bossUnitsTarget);
				director.bossUnitsTarget = director.bossUnitsSpawned;
				director.bossPending = false;
				director.bossSpawnFailures = 0;
				if (BossEncounterDefeated()) {
					gi.LocBroadcast_Print(PRINT_CENTER, "{} defeated!", director.boss ?
						director.boss->display_name : "Boss");
					if (HasEliminatedFighter() && ReinforcementAvailable() &&
						!director.reinforcementPending)
						director.reinforcementPending = true;
				} else {
					gi.LocBroadcast_Print(PRINT_CENTER, "BOSS DEPLOYMENT INCOMPLETE\nFIGHT ON");
				}
			} else {
				gi.Com_PrintFmt("MM_Horde: {} cannot fit available spawn points; continuing without a boss.\n",
					director.boss ? director.boss->display_name : "boss");
				director.boss = nullptr;
				director.bossPending = false;
				director.bossUnitsTarget = 0;
				director.bossSpawnFailures = 0;
				gi.LocBroadcast_Print(PRINT_CENTER, "BOSS DEPLOYMENT FAILED\nESCORT WAVE");
			}
		}
	}

	level.horde_monster_spawn_time = warmup ? level.time + 5_sec : level.time + 1_sec;
}

int RetargetLivingThreats()
{
	int retargeted = 0;

	for (size_t i = 1; i < globals.num_entities; i++) {
		gentity_t *monster = &g_entities[i];
		if (!IsLivingThreat(monster))
			continue;

		gentity_t *target = PickTarget(monster);
		if (!target)
			continue;

		monster->enemy = target;
		monster->monsterinfo.aiflags &=
			~(AI_LOST_SIGHT | AI_PURSUIT_LAST_SEEN | AI_PURSUE_NEXT | AI_PURSUE_TEMP);
		FoundTarget(monster);
		retargeted++;
	}

	return retargeted;
}

gentity_t *StalledThreatCandidate()
{
	for (size_t i = 1; i < globals.num_entities; i++) {
		gentity_t *monster = &g_entities[i];
		if (IsLivingThreat(monster) && !OriginSharesFighterPHS(monster->s.origin))
			return monster;
	}

	for (gentity_t *boss : director.bossEntities)
		if (boss && IsLivingThreat(boss))
			return boss;

	for (size_t i = 1; i < globals.num_entities; i++)
		if (IsLivingThreat(&g_entities[i]))
			return &g_entities[i];

	return nullptr;
}

bool RelocateStalledThreat(gentity_t *monster)
{
	if (!monster || !IsLivingHostileMonster(monster))
		return false;
	// A water-only monster must never be recovered onto a dry player spawn.
	// If it actually escapes world space, fail closed and let the caller retire it.
	if (MonsterRequiresWater(monster))
		return false;

	const bool old_origin_valid = ThreatInsideWorld(monster);
	const vec3_t avoid_origin = OriginFinite(monster->s.origin) ? monster->s.origin : vec3_origin;
	select_spawn_result_t result = SelectSpawnPoint(avoid_origin, monster->mins, monster->maxs);
	if (!result.any_valid || !result.spot)
		return false;

	vec3_t origin = result.spot->s.origin;
	if (!ValidateSpawnOrigin(origin, monster->mins, monster->maxs) ||
		!OriginSharesFighterPHS(origin))
		return false;

	const vec3_t old_origin = monster->s.origin;
	const vec3_t old_old_origin = monster->s.old_origin;
	const vec3_t old_velocity = monster->velocity;
	gentity_t *const old_groundentity = monster->groundentity;
	gi.unlinkentity(monster);
	monster->s.origin = origin;
	monster->s.old_origin = origin;
	monster->velocity = {};
	monster->groundentity = nullptr;
	gi.linkentity(monster);

	// Monster spawn functions can replace the provisional hull. Verify the
	// linked entity itself and roll back atomically if the destination is no
	// longer contained or connected to the active play space.
	if (!ThreatInsideWorld(monster) || !OriginSharesFighterPHS(monster->s.origin)) {
		gi.unlinkentity(monster);
		monster->s.origin = old_origin;
		monster->s.old_origin = old_old_origin;
		monster->velocity = old_velocity;
		monster->groundentity = old_groundentity;
		gi.linkentity(monster);
		return false;
	}

	const vec3_t size = monster->maxs - monster->mins;
	const float radius = max(24.0f, std::max({ size.x, size.y, size.z }) * 0.4f);
	if (old_origin_valid)
		SpawnGrow_Spawn(old_origin + (monster->mins + monster->maxs) * 0.5f, radius, radius * 2.0f);
	SpawnGrow_Spawn(origin + (monster->mins + monster->maxs) * 0.5f, radius, radius * 2.0f);

	if (gentity_t *target = PickTarget(monster)) {
		monster->enemy = target;
		monster->monsterinfo.aiflags &=
			~(AI_LOST_SIGHT | AI_PURSUIT_LAST_SEEN | AI_PURSUE_NEXT | AI_PURSUE_TEMP);
		FoundTarget(monster);
	}

	return true;
}

void RetireUnrecoverableThreat(gentity_t *monster)
{
	const int boss_slot = BossEntitySlot(monster);
	if (boss_slot >= 0) {
		director.bossEntities[boss_slot] = nullptr;
		RetireBossHealthBar(boss_slot);
	}

	G_FreeEntity(monster);
	InvalidateLivingCounts();
}

void RecoverStalledCombat()
{
	const int living = LivingThreatCount();
	bool recovered_placement = false;

	// A malformed map, auxiliary summon, or movement edge must never leave any
	// live hostile monster outside BSP world space. Recover every escaped
	// placement in this pass; if no habitat-safe destination exists, retire it
	// without reward. PHS is enforced when monsters spawn, while later PHS
	// separation is handled by the stall watchdog so normal room transitions do
	// not teleport valid aquatic or temporarily occluded monsters.
	const size_t entity_limit = globals.num_entities;
	for (size_t i = 1; i < entity_limit; i++) {
		gentity_t *monster = &g_entities[i];
		if (!IsLivingHostileMonster(monster) || ThreatInsideWorld(monster))
			continue;

		const char *classname = monster->classname ? monster->classname : "monster";
		if (RelocateStalledThreat(monster))
			gi.Com_PrintFmt("MM_Horde: relocated escaped {} back into playable world space.\n",
				classname);
		else {
			gi.Com_PrintFmt("MM_Horde: retired escaped {} with no valid recovery spawn.\n",
				classname);
			RetireUnrecoverableThreat(monster);
		}
		recovered_placement = true;
	}

	if (recovered_placement) {
		RecordCombatProgress();
		return;
	}

	if (!MM_Horde_StallRecoveryDue(level.horde_all_spawned, living,
			level.time.milliseconds(), director.lastCombatProgressTime.milliseconds(),
			g_horde_stall_timeout->value))
		return;

	if (director.stallRecoveryAttempts == 0) {
		const int retargeted = RetargetLivingThreats();
		gi.Com_PrintFmt("MM_Horde: no monster damage for {} seconds; retargeted {} threats.\n",
			FiniteSeconds(g_horde_stall_timeout->value, 90.f, 3600.f), retargeted);
	} else {
		gentity_t *monster = StalledThreatCandidate();
		const bool relocated = RelocateStalledThreat(monster);
		const char *classname = monster && monster->classname
			? monster->classname : "a remaining threat";
		if (!relocated && MonsterRequiresWater(monster)) {
			gi.Com_PrintFmt("MM_Horde: persistent combat stall; retired habitat-bound {} with no safe water recovery.\n",
				classname);
			RetireUnrecoverableThreat(monster);
		} else {
			gi.Com_PrintFmt("MM_Horde: persistent combat stall; {} {}.\n",
				relocated ? "relocated" : "could not relocate", classname);
		}
	}

	director.lastCombatProgressTime = level.time;
	director.stallRecoveryAttempts = MM_Horde_SaturatingIncrement(director.stallRecoveryAttempts);
}

} // namespace muffmode::horde

bool MM_Horde_MonsterEdgeDropsEnabled(const gentity_t *ent)
{
	if (!muffmode::horde::Active() || !g_horde_monster_edge_drops->integer ||
		level.round_state != roundst_t::ROUND_IN_PROGRESS ||
		!ent || !ent->inuse || !(ent->svflags & SVF_MONSTER) ||
		ent->health <= 0 || ent->deadflag || !ent->groundentity ||
		(ent->flags & (FL_FLY | FL_SWIM)) ||
		(ent->monsterinfo.aiflags & AI_STAND_GROUND) ||
		ent->monsterinfo.horde_reward_class == muffmode::horde::kHordeRewardBoss)
		return false;

	const gentity_t *enemy = ent->enemy;
	return enemy && enemy->inuse && enemy->client && enemy->health > 0 &&
		!enemy->deadflag && !enemy->client->eliminated &&
		ClientIsPlaying(enemy->client);
}

int MM_Horde_ModifyDamage(const gentity_t *target, const gentity_t *attacker,
	int damage, int means_of_death)
{
	if (!muffmode::horde::Active() ||
		level.round_state != roundst_t::ROUND_IN_PROGRESS || damage <= 0 ||
		!target || !attacker)
		return damage;

	const muffmode::horde::WildcardPreset preset = muffmode::horde::director.preset;
	if (preset == muffmode::horde::WildcardPreset::Sawstorm &&
		attacker->client && (target->svflags & SVF_MONSTER) &&
		target->monsterinfo.horde_reward_class != muffmode::horde::kHordeRewardBoss &&
		means_of_death == MOD_CHAINFIST)
		return MM_Horde_ScaleInt(damage, 4.f, damage, 0);

	if ((attacker->svflags & SVF_MONSTER) &&
		target->client &&
		attacker->monsterinfo.horde_reward_class != muffmode::horde::kHordeRewardBoss &&
		(preset == muffmode::horde::WildcardPreset::GetOverHere ||
			preset == muffmode::horde::WildcardPreset::PinballNight))
		return MM_Horde_ScaleInt(damage, 0.85f, damage, 0);

	return damage;
}

int MM_Horde_ModifyKnockback(const gentity_t *target, const gentity_t *attacker,
	int knockback)
{
	if (!muffmode::horde::Active() ||
		level.round_state != roundst_t::ROUND_IN_PROGRESS ||
		muffmode::horde::director.preset != muffmode::horde::WildcardPreset::PinballNight ||
		!target || !target->client || knockback <= 0 ||
		(attacker && (attacker->svflags & SVF_MONSTER) &&
			attacker->monsterinfo.horde_reward_class == muffmode::horde::kHordeRewardBoss))
		return knockback;

	return MM_Horde_ScaleInt(knockback, 5.f, knockback, 0);
}

void MM_Horde_ApplyDamagePull(gentity_t *target, const gentity_t *attacker, int damage)
{
	if (!muffmode::horde::Active() ||
		level.round_state != roundst_t::ROUND_IN_PROGRESS ||
		muffmode::horde::director.preset != muffmode::horde::WildcardPreset::GetOverHere ||
		!target || !target->client || !attacker || attacker == target ||
		!(attacker->svflags & SVF_MONSTER) ||
		attacker->monsterinfo.horde_reward_class == muffmode::horde::kHordeRewardBoss ||
		damage <= 0)
		return;

	vec3_t direction = attacker->s.origin - target->s.origin;
	const float distance = direction.length();
	if (!std::isfinite(distance) || distance < 1.f)
		return;

	direction *= 1.f / distance;
	const float impulse = std::clamp(30.f + static_cast<float>(damage), 30.f, 160.f);
	const vec3_t addition = direction * impulse;
	if (std::isfinite(addition.x) && std::isfinite(addition.y) && std::isfinite(addition.z))
		target->velocity += addition;
}

float MM_Horde_PlayerGravityScale()
{
	if (!muffmode::horde::Active() ||
		level.round_state != roundst_t::ROUND_IN_PROGRESS)
		return 1.f;

	switch (muffmode::horde::director.preset) {
	case muffmode::horde::WildcardPreset::LowGravity:
		return 0.55f;
	case muffmode::horde::WildcardPreset::PinballNight:
		return 0.65f;
	default:
		return 1.f;
	}
}

bool MM_Horde_ValidateMonsterPlacement(gentity_t *ent)
{
	if (!muffmode::horde::Active())
		return true;
	if (!ent || !ent->inuse || !(ent->svflags & SVF_MONSTER) ||
		!muffmode::horde::ThreatInsideWorld(ent))
		return false;

	// Warmup can run before a fighter is established. During a live wave,
	// connectivity to at least one living fighter is mandatory, which rejects
	// sealed voids and disconnected map leaves even when their contents are not
	// reported as solid by a malformed BSP.
	if (level.round_state == roundst_t::ROUND_IN_PROGRESS &&
		MM_Horde_CountFighters() > 0 &&
		!muffmode::horde::OriginSharesFighterPHS(ent->s.origin))
		return false;

	return true;
}

bool MM_Horde_CountAuxiliaryMonster(gentity_t *ent)
{
	if (!muffmode::horde::Active() || level.round_state != roundst_t::ROUND_IN_PROGRESS)
		return true;
	if (!ent || !ent->inuse || !(ent->svflags & SVF_MONSTER) ||
		ent->health <= 0 || ent->deadflag || (ent->svflags & SVF_DEADMONSTER) ||
		(ent->monsterinfo.aiflags & AI_GOOD_GUY) ||
		!MM_Horde_ValidateMonsterPlacement(ent))
		return false;
	if (!(ent->monsterinfo.aiflags & AI_DO_NOT_COUNT))
		return true;

	// CreateMonster and the medic resurrection path deliberately enter monster_start
	// as AI_DO_NOT_COUNT. Balance the engine's total/killed counters when Horde elects
	// to make one of those monsters a real wave threat. Keep reward class zero so
	// repeated summons or revivals cannot produce score, drops, or rally progress.
	const int monster_index = max(0, level.total_monsters);
	if (g_debug_monster_kills->integer) {
		if (static_cast<size_t>(monster_index) < level.monsters_registered.size())
			level.monsters_registered[monster_index] = ent;
		else if (static_cast<size_t>(monster_index) == level.monsters_registered.size())
			gi.Com_PrintFmt("MM_Horde: debug monster registry full; auxiliary threat not tracked.\n");
	}

	ent->monsterinfo.aiflags &= ~AI_DO_NOT_COUNT;
	ent->monsterinfo.horde_reward_class = 0;
	level.total_monsters = MM_Horde_SaturatingIncrement(monster_index);
	muffmode::horde::InvalidateLivingCounts();
	muffmode::horde::RecordCombatProgress();
	return true;
}

// [MuffMode] Pick a random validated floor position anywhere within the play area (the AABB of
// the deathmatch spawn spots) for scattering Horde techs, rather than placing them on the spawn
// points themselves. Samples a random XY, drops a downward trace to the floor, and reuses
// horde::ValidateSpawnOrigin (rejects solids/stuck spots and lava/slime floors). Returns false
// when no valid spot is found in a bounded number of tries (caller falls back to a spawn point).
bool MM_Horde_PickTechSpawnPos(vec3_t &out)
{
	if (notGT(GT_HORDE) || level.num_spawn_spots < 2)
		return false;

	vec3_t bmin = level.spawn_spots[0]->s.origin;
	vec3_t bmax = bmin;
	for (int i = 1; i < level.num_spawn_spots; i++) {
		const vec3_t &o = level.spawn_spots[i]->s.origin;
		bmin.x = min(bmin.x, o.x); bmin.y = min(bmin.y, o.y); bmin.z = min(bmin.z, o.z);
		bmax.x = max(bmax.x, o.x); bmax.y = max(bmax.y, o.y); bmax.z = max(bmax.z, o.z);
	}

	constexpr vec3_t tech_mins = { -15.f, -15.f, -15.f };
	constexpr vec3_t tech_maxs = {  15.f,  15.f,  15.f };
	const float trace_top    = bmax.z + 64.f;
	const float trace_bottom = bmin.z - 256.f;

	for (int attempt = 0; attempt < 24; attempt++) {
		vec3_t start = { bmin.x + frandom() * (bmax.x - bmin.x),
						 bmin.y + frandom() * (bmax.y - bmin.y),
						 trace_top };
		vec3_t end = { start.x, start.y, trace_bottom };

		trace_t tr = gi.trace(start, tech_mins, tech_maxs, end, nullptr, MASK_MONSTERSOLID);
		if (tr.startsolid || tr.allsolid || tr.fraction == 1.0f)
			continue; // started embedded, or never reached a floor

		vec3_t origin = tr.endpos;
		if (!horde::ValidateSpawnOrigin(origin, tech_mins, tech_maxs))
			continue;

		out = origin;
		return true;
	}

	return false;
}

void MM_Horde_RunSpawning()
{
	if (notGT(GT_HORDE))
		return;

	bool warmup = level.match_state == MATCH_WARMUP_DEFAULT || level.match_state == MATCH_WARMUP_READYUP;

	if (!warmup && level.round_state != ROUND_IN_PROGRESS)
		return;

	horde::RefreshTargetLoadCache();

	const float adaptive_mult = (!warmup && g_horde_enhanced_ai->integer) ? horde::AdaptivePressureMult() : 1.f;
	const int living_pressure = horde::LivingPressureCount();

	const int warmup_cap = max(1, g_horde_warmup_cap->integer);
	if (warmup && living_pressure >= warmup_cap)
		return;

	// Cap concurrently-alive monsters during live waves. Without this, a high-budget
	// swarm wave (many cheap monsters) can pile up hundreds of homing entities on a
	// single player and overflow that client's network message buffer (SZ_GetSpace).
	// Auxiliary summons and resurrections do not block wave completion, but do contribute
	// to this pressure cap so they cannot bypass its safety purpose.
	// Spawning pauses while at the cap and resumes as monsters die, so the wave still
	// spawns its full budget over time - only peak concurrency is bounded. 0 disables.
	const int alive_cap = MaxAliveCap();
	int effective_cap = alive_cap;
	if (!warmup && alive_cap > 0 && g_horde_enhanced_ai->integer)
		effective_cap = MM_Horde_ScaleInt(alive_cap, min(adaptive_mult, 1.f), alive_cap, 1);
	if (!warmup && alive_cap > 0 && living_pressure >= effective_cap &&
		!horde::director.bossPending)
		return;

	if (level.horde_all_spawned)
		return;

	if (!warmup && level.horde_spawn_points_remaining <= 0 &&
		!horde::director.bossPending) {
		level.horde_all_spawned = true;
		return;
	}

	if (level.horde_monster_spawn_time <= level.time) {
		const int                     remaining = warmup ? INT_MAX : level.horde_spawn_points_remaining;
		const horde::WeightedItem    *monster_row = nullptr;
		const horde::WeightedItem    *featured_row = nullptr;
		const horde::DirectorMonster *director_monster = nullptr;
		const horde::BossDefinition *boss_definition = nullptr;
		const bool                    is_boss = !warmup && horde::director.bossPending &&
			horde::director.boss;
		bool                          location_ready = false;
		gentity_t                    *spawn_anchor = nullptr;
		vec3_t                        spawn_origin = {};
		vec3_t                        spawn_angles = {};

		if (is_boss) {
			boss_definition = horde::director.boss;
		} else if (!warmup && horde::director.presetMonster &&
			horde::director.presetMonster->spawn_points <= remaining) {
			monster_row = horde::director.presetMonster;
		} else if (!warmup &&
			horde::director.preset == horde::WildcardPreset::None &&
			(featured_row = horde::PeekFeaturedMonster(remaining))) {
			monster_row = featured_row;
		} else if (!warmup &&
			horde::director.preset == horde::WildcardPreset::None &&
			horde::PickWaterSpawn(director_monster, remaining, spawn_origin,
				spawn_angles, spawn_anchor)) {
			location_ready = true;
		}

		const char *monster_class = nullptr;
		if (boss_definition)
			monster_class = boss_definition->classname;
		else if (director_monster)
			monster_class = director_monster->classname;
		else
			monster_class = horde::director.presetMonster
				? (monster_row ? monster_row->classname : nullptr)
				: monster_row
				? monster_row->classname
				: horde::PickMonsterForWave(&monster_row, remaining);
		if (!monster_class) {
			if (!warmup)
				level.horde_all_spawned = true;
			else
				level.horde_monster_spawn_time = level.time + 5_sec;
			return;
		}

		const horde::PresetSpawnTuning preset_tuning = is_boss
			? horde::PresetSpawnTuning{}
			: horde::SpawnTuning(horde::director.preset);
		vec3_t check_mins = director_monster ? director_monster->mins : horde::kDefaultSpawnMins;
		vec3_t check_maxs = director_monster ? director_monster->maxs : horde::kDefaultSpawnMaxs;
		if (boss_definition) {
			const float scale = horde::EffectiveBossScale(*boss_definition);
			check_mins = boss_definition->mins * scale;
			check_maxs = boss_definition->maxs * scale;
		} else {
			check_mins *= preset_tuning.scale;
			check_maxs *= preset_tuning.scale;
		}

		if (!location_ready && !warmup) {
			const bool try_authored = is_boss ||
				frandom() < MM_Horde_Probability(g_horde_map_spawn_chance->value, 0.75f);
			if (try_authored) {
				const horde::SpawnAnchorKind preferred = is_boss
					? horde::SpawnAnchorKind::Boss
					: horde::MonsterHabitat(monster_class);
				location_ready = horde::PickAuthoredSpawn(preferred, monster_class,
					check_mins, check_maxs, spawn_origin, spawn_angles, spawn_anchor,
					boss_definition);

				// A boss-specific placement is preferred, but a compatible authored
				// monster location is still better than discarding the map's encounter layout.
				if (!location_ready && is_boss) {
					const horde::SpawnAnchorKind habitat = horde::MonsterHabitat(monster_class);
					location_ready = horde::PickAuthoredSpawn(habitat, monster_class,
						check_mins, check_maxs, spawn_origin, spawn_angles, spawn_anchor);
				}
			}
		}

		if (!location_ready) {
			const std::vector<gentity_t *> &boss_fallbacks =
				is_boss ? horde::BossFallbackSpots(boss_definition) :
					horde::BossFallbackSpots(nullptr);
			gentity_t *const *allowed_spots = is_boss ? boss_fallbacks.data() : nullptr;
			const size_t allowed_count = is_boss ? boss_fallbacks.size() : 0;
			select_spawn_result_t result = horde::SelectSpawnPoint(vec3_origin,
				check_mins, check_maxs, allowed_spots, allowed_count, is_boss);
			if (result.any_valid && result.spot) {
				spawn_origin = result.spot->s.origin;
				if (!horde::ValidateSpawnOrigin(spawn_origin, check_mins, check_maxs)) {
					// Try a different candidate by excluding the failed spot from selection.
					// avoid_point is honoured when g_dm_respawn_point_min_dist > 0 (default 256).
					select_spawn_result_t retry = horde::SelectSpawnPoint(result.spot->s.origin,
						check_mins, check_maxs, allowed_spots, allowed_count, is_boss);
					bool retry_ok = false;
					if (retry.any_valid && retry.spot && retry.spot != result.spot) {
						spawn_origin = retry.spot->s.origin;
						if (horde::ValidateSpawnOrigin(spawn_origin, check_mins, check_maxs)) {
							result = retry;
							retry_ok = true;
						}
					}
					if (!retry_ok) {
						horde::RecordSpawnFailure(warmup, is_boss);
						return;
					}
				}

				spawn_angles = result.spot->s.angles;
				location_ready = true;
			}
		}

		if (!location_ready) {
			horde::RecordSpawnFailure(warmup, is_boss);
			return;
		}

		if (!warmup && !horde::OriginSharesFighterPHS(spawn_origin)) {
			horde::RecordSpawnFailure(false, is_boss);
			return;
		}

		gentity_t *e = G_Spawn();
		e->classname = monster_class;
		e->s.origin = spawn_origin;
		e->s.angles = spawn_angles;
		if (!is_boss && preset_tuning.scale != 1.f)
			e->s.scale = preset_tuning.scale;

		// The first valid regular spawn of a champion-pending wave becomes the champion.
		const bool is_champion = level.horde_champion_pending && !warmup && !is_boss;

		st = {};
		if (is_boss) {
			const int snapshotted_fighters = max(1, static_cast<int>(level.horde_fighters_snapshotted));
			const float player_health_mult = 1.0f + (snapshotted_fighters - 1) * 0.20f;
			const float wave_health_mult = MM_Horde_BossWaveMultiplier(level.round_number,
				boss_definition->min_level, g_horde_boss_health_per_wave->value);
			const bool boss_anchor = spawn_anchor &&
				spawn_anchor->style == static_cast<int32_t>(horde::SpawnAnchorKind::Boss);
			const float authored_health_mult = boss_anchor && spawn_anchor->speed > 0.f
				? spawn_anchor->speed
				: boss_definition->health_multiplier;
			const float profile_health_mult = MM_Horde_ClampFiniteFloat(authored_health_mult,
				boss_definition->health_multiplier, 0.05f, MM_HORDE_MAX_COMBAT_MULTIPLIER);
			const float pair_health_mult = horde::director.bossUnitsTarget > 1
				? MM_Horde_ClampFiniteFloat(g_horde_boss_pair_health_mult->value,
					1.f, 0.05f, MM_HORDE_MAX_COMBAT_MULTIPLIER)
				: 1.f;
			const float global_health_mult = MM_Horde_ClampFiniteFloat(
				g_horde_boss_health_mult->value, 1.f, 0.1f, MM_HORDE_MAX_COMBAT_MULTIPLIER);
			const double combined_health_mult = static_cast<double>(global_health_mult) *
				player_health_mult * wave_health_mult * profile_health_mult * pair_health_mult;
			st.health_multiplier = static_cast<float>(std::clamp(combined_health_mult,
				0.1, static_cast<double>(MM_HORDE_MAX_COMBAT_MULTIPLIER)));

			const uint32_t profile_spawnflags = boss_anchor && spawn_anchor->noise_index2 > 0
				? static_cast<uint32_t>(spawn_anchor->sounds)
				: boss_definition->spawnflags;
			e->spawnflags = spawnflags_t(profile_spawnflags);
			e->s.scale = horde::EffectiveBossScale(*boss_definition,
				boss_anchor ? spawn_anchor->s.scale : 0.f);

			int32_t power_armor_type = boss_definition->power_armor_type;
			int32_t power_armor_power = boss_definition->power_armor_power;
			int32_t monster_slots = boss_definition->monster_slots;
			const char *reinforcements = boss_definition->reinforcements;

			if (boss_anchor) {
				if (spawn_anchor->decel > 0.f)
					power_armor_type = spawn_anchor->monsterinfo.power_armor_type;
				if (spawn_anchor->volume > 0.f)
					power_armor_power = spawn_anchor->monsterinfo.power_armor_power;
				if (spawn_anchor->attenuation > 0.f)
					monster_slots = spawn_anchor->monsterinfo.monster_slots;
				if (spawn_anchor->noise_index > 0)
					reinforcements = spawn_anchor->model;
			}

			if (power_armor_type >= 0) {
				e->monsterinfo.power_armor_type = static_cast<item_id_t>(power_armor_type);
				st.keys_specified.emplace("power_armor_type");
			}
			if (power_armor_power >= 0) {
				e->monsterinfo.power_armor_power = min(power_armor_power, horde::kHordeMaxPowerArmor);
				st.keys_specified.emplace("power_armor_power");
			}
			if (monster_slots >= 0) {
				e->monsterinfo.monster_slots = min(monster_slots, horde::kHordeMaxSummonSlots);
				st.keys_specified.emplace("monster_slots");
			}
			if (reinforcements) {
				st.reinforcements = reinforcements;
				st.keys_specified.emplace("reinforcements");
			}
		} else {
			st.health_multiplier = is_champion
				? MM_Horde_ClampFiniteFloat(g_horde_champion_health_mult->value,
					3.f, 0.1f, MM_HORDE_MAX_COMBAT_MULTIPLIER)
				: preset_tuning.health;
		}
		ED_CallSpawn(e);

		if (!e->inuse || !(e->svflags & SVF_MONSTER)) {
			if (e->inuse)
				G_FreeEntity(e);
			horde::RecordSpawnFailure(warmup, is_boss);
			return;
		}
		if (!MM_Horde_ValidateMonsterPlacement(e) ||
			(!warmup && !horde::OriginSharesFighterPHS(e->s.origin))) {
			gi.Com_PrintFmt("MM_Horde: rejected out-of-world or disconnected spawn for {}.\n",
				e->classname ? e->classname : "monster");
			G_FreeEntity(e);
			horde::RecordSpawnFailure(false, is_boss);
			return;
		}

		horde::ApplySpawnRoleTuning(e, monster_class);
		if (!is_boss && !is_champion) {
			e->monsterinfo.champion_damage_scale = preset_tuning.damage;
			float movement_scale = preset_tuning.movement;
			if (preset_tuning.scale != 1.f) {
				const float applied_scale = std::isfinite(e->s.scale) && e->s.scale > 0.f
					? MM_Horde_PresetEntityScale(e->s.scale)
					: 1.f;
				movement_scale /= applied_scale;
			}
			e->monsterinfo.scale *= movement_scale;
			e->gravity *= preset_tuning.gravity;
		}
		e->monsterinfo.horde_reward_class = warmup ? 0 :
			(is_boss ? horde::kHordeRewardBoss :
				(is_champion ? horde::kHordeRewardChampion : horde::kHordeRewardRegular));
		horde::InvalidateLivingCounts();

		if (is_boss) {
			const float wave_damage_mult = MM_Horde_BossWaveMultiplier(level.round_number,
				boss_definition->min_level, g_horde_boss_damage_per_wave->value);
			const bool boss_anchor = spawn_anchor &&
				spawn_anchor->style == static_cast<int32_t>(horde::SpawnAnchorKind::Boss);
			const float authored_damage_mult = boss_anchor && spawn_anchor->accel > 0.f
				? spawn_anchor->accel
				: boss_definition->damage_multiplier;
			const float profile_damage_mult = MM_Horde_ClampFiniteFloat(authored_damage_mult,
				boss_definition->damage_multiplier, 0.05f, MM_HORDE_MAX_COMBAT_MULTIPLIER);
			const float global_damage_mult = MM_Horde_ClampFiniteFloat(
				g_horde_boss_damage_mult->value, 1.15f, 0.1f, MM_HORDE_MAX_COMBAT_MULTIPLIER);
			const double combined_damage_mult = static_cast<double>(global_damage_mult) *
				wave_damage_mult * profile_damage_mult;
			e->monsterinfo.champion_damage_scale = static_cast<float>(std::clamp(
				combined_damage_mult, 0.1, static_cast<double>(MM_HORDE_MAX_COMBAT_MULTIPLIER)));
			e->monsterinfo.double_time = HOLD_FOREVER;

			const float armor_mult = MM_Horde_ClampFiniteFloat(g_horde_boss_armor_mult->value,
				1.f, 0.f, MM_HORDE_MAX_COMBAT_MULTIPLIER);
			e->monsterinfo.power_armor_power = MM_Horde_ScaleInt(
				e->monsterinfo.power_armor_power, armor_mult,
				e->monsterinfo.power_armor_power, 0, horde::kHordeMaxPowerArmor);
			e->monsterinfo.max_power_armor_power = e->monsterinfo.power_armor_power;
			e->monsterinfo.initial_power_armor_type = e->monsterinfo.power_armor_type;

			const int boss_slot = horde::director.bossUnitsSpawned++;
			horde::director.bossEntities[boss_slot] = e;
			horde::director.bossPending =
				horde::director.bossUnitsSpawned < horde::director.bossUnitsTarget;
			horde::director.bossSpawnFailures = 0;
			horde::AttachBossHealthBar(e, boss_slot);

			const vec3_t size = e->maxs - e->mins;
			const float radius = max(32.0f, std::max({ size.x, size.y, size.z }) * 0.5f);
			SpawnGrow_Spawn(e->s.origin + (e->mins + e->maxs) * 0.5f, radius, radius * 2.0f);
		} else if (is_champion) {
			// natural = full health after spawn (already includes the 3x mult and any co-op scaling).
			const int natural = e->health;

			// Put the floor on the same footing as the (possibly co-op-scaled) natural health by
			// mirroring whatever multiplier co-op applied. base_health is the pre-co-op health set by
			// G_Monster_ScaleCoopHealth; it stays 0 when no co-op scaling happened (pure DM/horde).
			const float coop_mult = (e->monsterinfo.base_health > 0)
				? (float)natural / (float)e->monsterinfo.base_health
				: 1.0f;

			const float floor_start = MM_Horde_ClampFiniteFloat(g_horde_champion_health_floor->value,
				400.f, 0.f, static_cast<float>(horde::kHordeMaxMonsterHealth));
			const float floor_per_wave = MM_Horde_ClampFiniteFloat(
				g_horde_champion_health_per_wave->value, 25.f,
				-static_cast<float>(horde::kHordeMaxMonsterHealth),
				static_cast<float>(horde::kHordeMaxMonsterHealth));
			const float safe_coop_mult = MM_Horde_ClampFiniteFloat(coop_mult, 1.f, 0.f,
				MM_HORDE_MAX_COMBAT_MULTIPLIER);
			const double floor_base = static_cast<double>(floor_start) +
				static_cast<double>(floor_per_wave) * level.round_number;
			const int floor_hp = MM_Horde_ClampFiniteInt(
				std::max(0.0, floor_base) * safe_coop_mult, natural, 0,
				horde::kHordeMaxMonsterHealth);

			// Weakness signal drives the taper: 1.0 for a sub-floor monster (full help), 0.0 once its
			// natural health reaches strong_ratio x floor.
			const float strong_ratio = MM_Horde_ClampFiniteFloat(
				g_horde_champion_strong_ratio->value, 4.f, 0.1f, MM_HORDE_MAX_COMBAT_MULTIPLIER);
			const float strong_hp = floor_hp * strong_ratio;
			const float denom = max(1.0f, strong_hp - static_cast<float>(floor_hp));
			const float weakness = clamp((strong_hp - (float)natural) / denom, 0.0f, 1.0f);

			// Health: lift weak monsters to the floor; leave naturally-tough ones at their 3x. Co-op
			// re-scaling on later joins only adds health, so the floor is never undercut.
			const int champ_hp = max(natural, floor_hp);
			e->health = e->max_health = champ_hp;

			// Tapered offensive buffs. Damage scale is stored for T_Damage; speed folds into the
			// frame-distance multiplier (already = MODEL_SCALE * s.scale at this point).
			const float champion_damage_mult = MM_Horde_ClampFiniteFloat(
				g_horde_champion_damage_mult->value, 2.f, 0.1f, MM_HORDE_MAX_COMBAT_MULTIPLIER);
			const float champion_speed_mult = MM_Horde_ClampFiniteFloat(
				g_horde_champion_speed_mult->value, 1.25f, 0.1f, 4.f);
			e->monsterinfo.champion_damage_scale = lerp(1.0f, champion_damage_mult, weakness);
			e->monsterinfo.scale *= lerp(1.0f, champion_speed_mult, weakness);

			// EF_DOUBLE shell marks every champion regardless of tier.
			e->monsterinfo.double_time = HOLD_FOREVER;
			level.horde_champion_pending = false;
		}

		if (featured_row)
			horde::ConsumeFeaturedMonster(featured_row);

		level.horde_monster_spawn_time = level.time +
			horde::SpawnDelayAfterSuccess(warmup, adaptive_mult, is_boss);

		e->enemy = MM_Horde_PickTarget(e);
		if (e->enemy)
			FoundTarget(e);

		if (!warmup) {
			horde::RecordCombatProgress();
			const int spawn_cost = boss_definition ? boss_definition->spawn_points :
				(director_monster ? director_monster->spawn_points :
				(monster_row ? monster_row->spawn_points : 0));
			level.horde_spawn_points_remaining -= spawn_cost;

			if (level.horde_spawn_points_remaining <= 0 &&
				!horde::director.bossPending)
				level.horde_all_spawned = true;
		}

		if (spawn_anchor) {
			if (!is_boss) {
				const vec3_t size = e->maxs - e->mins;
				const float radius = max(24.0f, std::max({ size.x, size.y, size.z }) * 0.4f);
				SpawnGrow_Spawn(e->s.origin + (e->mins + e->maxs) * 0.5f, radius, radius * 2.0f);
			}
			horde::CommitAuthoredSpawn(spawn_anchor, e);
		}
	}
}

void MM_Horde_AdjustPlayerScore(gclient_t *cl, int32_t offset)
{
	if (notGT(GT_HORDE))
		return;
	if (!cl || !cl->pers.connected)
		return;

	if (IsScoringDisabled())
		return;

	const int tier = horde::PerformanceTier(cl);
	const int bonus = MM_Horde_ClampFiniteInt(
		static_cast<double>(tier) * max(0, g_horde_streak_score_bonus->integer),
		0, 0, std::numeric_limits<int>::max());
	G_AdjustPlayerScore(cl, MM_Horde_SaturatingAdd(offset, bonus), false, 0);
}
