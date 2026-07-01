// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_horde.h"
#include "muffmode/mm_horde_ai.h"
#include "muffmode/mm_horde_ai_rules.h"
#include "muffmode/mm_horde_tables.h"

#include <algorithm>
#include <array>
#include <climits>

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
extern cvar_t *g_horde_themed_waves;
extern cvar_t *g_horde_theme_chance;
extern cvar_t *g_horde_theme_min_wave;
extern cvar_t *g_horde_wave_variety;
extern cvar_t *g_horde_wave_min_types;

namespace muffmode::horde {

constexpr int kMaxMonsterMarkerSlots = POI_HORDE_MONSTER_END - POI_HORDE_MONSTER_0 + 1;
static_assert(kMaxMonsterMarkerSlots > 0, "Horde monster marker POI range must not be empty");

bool Active()
{
	return g_gametype->integer == static_cast<int>(GT_HORDE);
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

bool IsLivingMonster(const gentity_t *ent)
{
	if (!ent->inuse || !(ent->svflags & SVF_MONSTER))
		return false;
	if (ent->health <= 0 || ent->deadflag || (ent->svflags & SVF_DEADMONSTER))
		return false;
	if (ent->monsterinfo.aiflags & AI_DO_NOT_COUNT)
		return false;

	return true;
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
	const int living = level.total_monsters - level.killed_monsters;

	if (threshold < 1 || living > threshold) {
		if (level.horde_mark_living >= 0 && level.horde_mark_living <= threshold)
			ClearMonsterPoisForAll();

		level.horde_mark_living = static_cast<int16_t>(living);
		return;
	}

	const bool newly_marking = level.horde_mark_living > threshold || level.horde_mark_living < 0;
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

	for (size_t i = 1; i < globals.num_entities && num_marked < max_slots; i++) {
		gentity_t *ent = &g_entities[i];

		if (!IsLivingMonster(ent))
			continue;

		marked[num_marked++] = ent;
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
		ec->client->horde_elim_msg_wave = 0;

		// Eliminated fighters spectate in freecam with deadflag cleared and health restored.
		if (was_eliminated || ec->deadflag || ec->health <= 0)
			ClientRespawn(ec);
	}
}

float MultiplierFromFighters(int fighters)
{
	if (!g_horde_player_scale->integer)
		return 1.f;

	float factor = g_horde_player_scale_factor->value;
	if (factor < 0.f)
		factor = 0.f;

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
	const float ref      = max(1.f, g_horde_map_scale_ref->value);
	const float factor   = clamp(g_horde_map_scale_factor->value, 0.f, 10.f);
	const float ratio    = diagonal / ref;
	const float mult     = 1.f + (ratio - 1.f) * factor;

	level.horde_map_scale_mult = max(0.1f, mult);
	return level.horde_map_scale_mult;
}

} // namespace muffmode::horde

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

	// Linear up to the tuned content peak (wave 12). Beyond it - reached via endless (roundlimit 0)
	// or a high finite roundlimit - growth tapers by the effective late factor (escalation on:
	// g_horde_late_budget_factor; off: g_horde_late_wave_factor) so late waves stay playable
	// instead of piling up 190+ points of commanders. Continuous at the peak.
	int budget;
	if (level.round_number <= peak)
		budget = base + level.round_number * per_wave;
	else
		budget = base + peak * per_wave +
			static_cast<int>((level.round_number - peak) * per_wave * EffectiveLateWaveFactor());

	if (min_pts > 0)
		budget = max(budget, min_pts);
	if (max_pts > 0)
		budget = min(budget, max_pts);

	return max(1, static_cast<int>(budget * pmult * msmult));
}

namespace muffmode::horde {

gtime_t SpawnInterval(bool warmup, float adaptive_mult)
{
	if (warmup)
		return 5_sec;

	const float min_sec = max(0.05f, g_horde_spawn_interval_min->value);
	const float max_sec = max(min_sec, g_horde_spawn_interval_max->value);
	gtime_t interval = random_time(gtime_t::from_sec(min_sec), gtime_t::from_sec(max_sec));

	if (!warmup && g_horde_enhanced_ai->integer && adaptive_mult != 1.f) {
		interval = gtime_t::from_ms(static_cast<int64_t>(interval.milliseconds() / max(adaptive_mult, 0.1f)));
		interval = max(interval, gtime_t::from_ms(100));
	}

	return interval;
}

} // namespace muffmode::horde

bool MM_Horde_ShouldSkipEntitiesReset()
{
	return horde::Active();
}

int MM_Horde_CountdownWaveNumber()
{
	if (notGT(GT_HORDE))
		return level.round_number + 1;

	if (!level.round_number && g_horde_starting_wave->integer > 0)
		return g_horde_starting_wave->integer;

	return level.round_number + 1;
}

void MM_Horde_AdvanceRoundNumber()
{
	if (notGT(GT_HORDE))
		return;

	if (!level.round_number && g_horde_starting_wave->integer > 0)
		level.round_number = g_horde_starting_wave->integer;
	else
		level.round_number++;
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
	if (const horde::ThemeDefinition *theme = horde::FindTheme(static_cast<horde::Theme>(level.horde_wave_theme)))
		gi.LocBroadcast_Print(PRINT_CENTER, "{}", theme->announce);
	else
		gi.LocBroadcast_Print(PRINT_CENTER, brandom() ? "INCOMING!" : "LOCK AND LOAD!");
	AnnouncerSound(world, "fight", nullptr, false);
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
	if (ent->client->horde_elim_msg_wave == level.round_number)
		return;

	ent->client->horde_elim_msg_wave = static_cast<int16_t>(level.round_number);
	gi.LocClient_Print(ent, PRINT_CENTER, "You will rejoin when the next wave countdown begins.");
}

void MM_Horde_OnPlayerDeath(gentity_t *ent)
{
	if (!horde::Active())
		return;
	if (level.round_state != roundst_t::ROUND_IN_PROGRESS)
		return;
	if (!ent || !ent->client || !ClientIsPlaying(ent->client))
		return;

	if (ent->client->pers.lives > 0)
		ent->client->pers.lives--;

	horde::Adaptive_RecordPlayerDeath();

	if (ent->client->pers.lives <= 0) {
		ClientSetEliminated(ent);
		ent->client->respawn_time = level.time + 1_sec;
		MM_Horde_NotifyEliminatedSpectator(ent);
	}
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

	// Remove dead monster corpses between waves (Horde skips Entities_Reset).
	for (size_t i = globals.num_entities; i > 1; i--) {
		gentity_t *ent = &g_entities[i - 1];

		if (!ent->inuse)
			continue;
		if (!(ent->svflags & SVF_MONSTER))
			continue;
		if (ent->health > 0 && !ent->deadflag && !(ent->svflags & SVF_DEADMONSTER))
			continue;

		G_FreeEntity(ent);
	}

	level.total_monsters = 0;
	level.killed_monsters = 0;

	if (g_debug_monster_kills->integer)
		level.monsters_registered.fill(nullptr);

	horde::ClearMonsterPoisForAll();
	level.horde_mark_time = 0_ms;
}

void MM_Horde_OnRoundEnd()
{
	if (notGT(GT_HORDE))
		return;

	horde::Adaptive_RecordWaveEnd();
	level.horde_all_spawned = false;
	MM_Horde_CleanWaveTransition();
}

bool MM_Horde_UpdateRoundInProgress()
{
	if (notGT(GT_HORDE))
		return false;

	if (MM_Horde_CheckAllFightersLost())
		return false;

	MM_Horde_RunSpawning();
	horde::UpdateMonsterMarkers();

	if (level.horde_all_spawned && !(level.total_monsters - level.killed_monsters)) {
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

	// [MuffMode] Expiry cue for timed techs (g_horde_tech_duration); precache here since the
	// power-armor items that normally register it may not be present in a Horde match.
	gi.soundindex("misc/power2.wav");
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

	// Pick this wave's theme. Rare (g_horde_theme_chance), never the same as the previous
	// themed wave, and only themes whose monsters exist by this wave are eligible.
	{
		const horde::Theme prev = static_cast<horde::Theme>(level.horde_wave_theme);
		horde::Theme chosen = horde::Theme::None;

		if (g_horde_themed_waves->integer &&
			level.round_number >= g_horde_theme_min_wave->integer &&
			frandom() < g_horde_theme_chance->value) {
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
	if (g_horde_wave_variety->integer &&
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

		const int min_types = max(1, g_horde_wave_min_types->integer);
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

	// Decide whether this wave hosts a champion.
	// Up to the content peak: spend the per-run budget (mm_match seeds 0-2), spread across the waves
	// remaining until the peak. Past the peak the budget is gone, so switch to a steady per-wave rate
	// derived from the same knobs (max_per_run * champion_chance champions per peak-length span) so
	// champions keep appearing at the tuned cadence for any wave count.
	level.horde_champion_pending = false;
	if (g_horde_champions->integer &&
		level.round_number >= g_horde_champion_min_wave->integer) {
		if (horde::IsLateWave()) {
			const int   span = max(1, g_horde_content_peak_wave->integer - g_horde_champion_min_wave->integer + 1);
			const float rate = g_horde_champion_max_per_run->value * g_horde_champion_chance->value / span;

			if (frandom() < min(rate, 1.0f))
				level.horde_champion_pending = true;
		} else if (level.horde_champions_remaining > 0) {
			// Spread the run's budget across the waves left until the peak (== roundlimit for the
			// standard 12-wave run, so that case is unchanged).
			const int last_budget_wave = roundlimit->integer > 0
				? min(roundlimit->integer, g_horde_content_peak_wave->integer)
				: g_horde_content_peak_wave->integer;
			const int waves_left = max(1, last_budget_wave - level.round_number + 1);

			if (frandom() < static_cast<float>(level.horde_champions_remaining) / waves_left) {
				level.horde_champion_pending = true;
				level.horde_champions_remaining--;
			}
		}
	}

	// DEBUG/TEST: force a champion every wave (overrides the roll above), regardless of min_wave.
	if (g_horde_champion_force->integer)
		level.horde_champion_pending = true;

	const int fighters = MM_Horde_CountFighters();
	level.horde_fighters_snapshotted = static_cast<int8_t>(fighters);
	level.horde_spawn_points_remaining = MM_Horde_WavePointBudget();

	if (const horde::ThemeDefinition *theme = horde::FindTheme(static_cast<horde::Theme>(level.horde_wave_theme)))
		level.horde_spawn_points_remaining =
			max(1, static_cast<int>(level.horde_spawn_points_remaining * theme->budget_mult));

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

bool ValidateSpawnOrigin(vec3_t &origin, const vec3_t &check_mins, const vec3_t &check_maxs)
{
	origin[2] += 16.f;

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

} // namespace muffmode::horde

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

	const int warmup_cap = max(1, g_horde_warmup_cap->integer);
	if (warmup && (level.total_monsters - level.killed_monsters >= warmup_cap))
		return;

	// Cap concurrently-alive monsters during live waves. Without this, a high-budget
	// swarm wave (many cheap monsters) can pile up hundreds of homing entities on a
	// single player and overflow that client's network message buffer (SZ_GetSpace).
	// Spawning pauses while at the cap and resumes as monsters die, so the wave still
	// spawns its full budget over time - only peak concurrency is bounded. 0 disables.
	const int alive_cap = MaxAliveCap();
	int effective_cap = alive_cap;
	if (!warmup && alive_cap > 0 && g_horde_enhanced_ai->integer)
		effective_cap = max(1, static_cast<int>(alive_cap * min(adaptive_mult, 1.f)));
	if (!warmup && alive_cap > 0 && (level.total_monsters - level.killed_monsters >= effective_cap))
		return;

	if (level.horde_all_spawned)
		return;

	if (!warmup && level.horde_spawn_points_remaining <= 0) {
		level.horde_all_spawned = true;
		return;
	}

	if (level.horde_monster_spawn_time <= level.time) {
		const int                 remaining = warmup ? INT_MAX : level.horde_spawn_points_remaining;
		const horde::WeightedItem *monster_row = nullptr;
		const char               *monster_class = horde::PickMonsterForWave(&monster_row, remaining);
		if (!monster_class) {
			if (!warmup)
				level.horde_all_spawned = true;
			else
				level.horde_monster_spawn_time = level.time + 5_sec;
			return;
		}

		gentity_t *e = G_Spawn();
		e->classname = monster_class;
		select_spawn_result_t result = horde::SelectSpawnPoint(vec3_origin);

		if (result.any_valid && result.spot) {
			// Validate spawn point fits a large monster (tank commander is the worst-case hull).
			// CheckSpawnPoint also rejects non-world solids (doors, movers) unlike a raw startsolid check.
			constexpr vec3_t horde_check_mins = { -32.f, -32.f, -16.f };
			constexpr vec3_t horde_check_maxs = {  32.f,  32.f,  64.f };
			vec3_t spawn_origin = result.spot->s.origin;
			if (!horde::ValidateSpawnOrigin(spawn_origin, horde_check_mins, horde_check_maxs)) {
				// Try a different candidate by excluding the failed spot from selection.
				// avoid_point is honoured when g_dm_respawn_point_min_dist > 0 (default 256).
				select_spawn_result_t retry = horde::SelectSpawnPoint(result.spot->s.origin);
				bool retry_ok = false;
				if (retry.any_valid && retry.spot && retry.spot != result.spot) {
					spawn_origin = retry.spot->s.origin;
					if (horde::ValidateSpawnOrigin(spawn_origin, horde_check_mins, horde_check_maxs)) {
						result = retry;
						retry_ok = true;
					}
				}
				if (!retry_ok) {
					// No spot can safely hold a large monster right now. Spawning anyway would
					// place it embedded and let monster_start_go's stuck-fixing relocate it
					// through thin floors or into walls; skip this attempt and retry shortly.
					G_FreeEntity(e);
					level.horde_monster_spawn_time = warmup ? level.time + 5_sec : level.time + 1_sec;
					return;
				}
			}

			e->s.origin = spawn_origin;
			e->s.angles = result.spot->s.angles;

			// The first valid spawn of a champion-pending wave becomes the champion. Base health is
			// scaled 3x via st before spawn; after spawn we apply a health floor + wave scaling so even
			// a weak monster (e.g. light soldier) becomes a real threat, plus tapered damage/speed buffs
			// and the EF_DOUBLE shell (applied after spawn, since monster_start zeroes the powerup timers).
			// The buffs taper by base strength: weak monsters get the full punch, heavy ones (tank etc.)
			// stay beefy bullet-sponges without becoming one-shot deleters.
			const bool is_champion = level.horde_champion_pending && !warmup;

			// [MuffMode] Champions always drop a random tech when techs are enabled; otherwise
			// they drop their usual strong item.
			if (is_champion)
				e->item = AllowTechs() ? GetItemByIndex(tech_ids[irandom(static_cast<int32_t>(q_countof(tech_ids)))])
									   : horde::PickChampionDrop();
			else
				e->item = horde::PickDropItem(monster_row);
			st = {};
			st.health_multiplier = is_champion ? g_horde_champion_health_mult->value : 1.0f;
			ED_CallSpawn(e);

			if (!e->inuse || !(e->svflags & SVF_MONSTER)) {
				if (e->inuse)
					G_FreeEntity(e);
				level.horde_monster_spawn_time = warmup ? level.time + 5_sec : level.time + 1_sec;
				return;
			}

			horde::ApplySpawnRoleTuning(e, monster_class);

			if (is_champion) {
				// natural = full health after spawn (already includes the 3x mult and any co-op scaling).
				const int natural = e->health;

				// Put the floor on the same footing as the (possibly co-op-scaled) natural health by
				// mirroring whatever multiplier co-op applied. base_health is the pre-co-op health set by
				// G_Monster_ScaleCoopHealth; it stays 0 when no co-op scaling happened (pure DM/horde).
				const float coop_mult = (e->monsterinfo.base_health > 0)
					? (float)natural / (float)e->monsterinfo.base_health
					: 1.0f;

				const float floor_base = g_horde_champion_health_floor->value +
					g_horde_champion_health_per_wave->value * (float)level.round_number;
				const float floor_hp = floor_base * coop_mult;

				// Weakness signal drives the taper: 1.0 for a sub-floor monster (full help), 0.0 once its
				// natural health reaches strong_ratio x floor.
				const float strong_hp = floor_hp * g_horde_champion_strong_ratio->value;
				const float denom = max(1.0f, strong_hp - floor_hp);
				const float weakness = clamp((strong_hp - (float)natural) / denom, 0.0f, 1.0f);

				// Health: lift weak monsters to the floor; leave naturally-tough ones at their 3x. Co-op
				// re-scaling on later joins only adds health, so the floor is never undercut.
				const int champ_hp = max(natural, (int)floor_hp);
				e->health = e->max_health = champ_hp;

				// Tapered offensive buffs. Damage scale is stored for T_Damage; speed folds into the
				// frame-distance multiplier (already = MODEL_SCALE * s.scale at this point).
				e->monsterinfo.champion_damage_scale = lerp(1.0f, g_horde_champion_damage_mult->value, weakness);
				e->monsterinfo.scale *= lerp(1.0f, g_horde_champion_speed_mult->value, weakness);

				// EF_DOUBLE shell marks every champion regardless of tier.
				e->monsterinfo.double_time = HOLD_FOREVER;
				level.horde_champion_pending = false;
			}

			level.horde_monster_spawn_time = level.time + horde::SpawnInterval(warmup, adaptive_mult);

			e->enemy = MM_Horde_PickTarget(e);
			if (e->enemy)
				FoundTarget(e);

			if (!warmup && monster_row) {
				level.horde_spawn_points_remaining -= monster_row->spawn_points;

				if (level.horde_spawn_points_remaining <= 0)
					level.horde_all_spawned = true;
			}
		} else {
			G_FreeEntity(e);
			level.horde_monster_spawn_time = warmup ? level.time + 5_sec : level.time + 1_sec;
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

	G_AdjustPlayerScore(cl, offset, false, 0);
}
