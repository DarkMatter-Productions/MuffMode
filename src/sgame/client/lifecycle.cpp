// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include "g_local.h"
#include "userinfo.h"
#include "monsters/m_player.h"
#include "bots/bot_includes.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_awards.h"
#include "muffmode/mm_captain.h"
#include "muffmode/mm_client_profile.h"
#include "muffmode/mm_client_refs.h"
#include "muffmode/mm_freezetag.h"
#include "muffmode/mm_ghost.h"
#include "muffmode/mm_horde.h"
#include "muffmode/mm_horde_ai_rules.h"
#include "muffmode/mm_lms.h"
#include "muffmode/mm_map_pick.h"
#include "muffmode/mm_match_stats.h"
#include "muffmode/mm_menu.h"
#include "muffmode/mm_motd.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_pconfig.h"
#include "muffmode/mm_player_stats.h"
#include "muffmode/mm_ruleset.h"
#include "muffmode/mm_spawn_rules.h"
#include "muffmode/mm_spawn_loadout.h"
#include "muffmode/mm_team.h"
#include "muffmode/mm_vote.h"

bool ClientArenaEliminationCorpse(const gclient_t *client);

//=======================================================================
// Client death, obituary and item-drop handling lives in sgame/client/death.cpp.

// [Paril-KEX]
static void Player_GiveStartItems(gentity_t *ent, const char *ptr) {
	char token_copy[MAX_TOKEN_CHARS];
	const char *token;

	while ((token = COM_ParseEx(&ptr, ";")) && *token) {
		Q_strlcpy(token_copy, token, sizeof(token_copy));
		const char *ptr_copy = token_copy;

		const char *item_name = COM_Parse(&ptr_copy);
		gitem_t *item = FindItemByClassname(item_name);

		if (!item || !item->pickup)
			continue;
			//gi.Com_ErrorFmt("Invalid g_start_item entry: {}\n", item_name);

		int32_t count = 1;

		if (*ptr_copy) {
			const auto parsed_count = MM_ParseNonNegativeIntArg(COM_Parse(&ptr_copy));
			if (!parsed_count) {
				gi.Com_PrintFmt("{}: ignoring invalid g_start_item count for {}\n", __FUNCTION__, item_name);
				continue;
			}
			count = *parsed_count;
		}

		if (count == 0) {
			ent->client->pers.inventory[item->id] = 0;
			continue;
		}

		gentity_t *dummy = G_Spawn();
		dummy->item = item;
		dummy->count = count;
		dummy->spawnflags |= SPAWNFLAG_ITEM_DROPPED;
		item->pickup(dummy, ent);
		G_FreeEntity(dummy);
	}
}

/*
==============
InitClientPersistant

This is only called when the game first initializes in single player,
but is called after each death and level change in deathmatch
==============
*/
namespace {
bool ghost_abort_restart_in_progress = false;

bool GhostAbortSpawnUsesDeferredPresentation(gentity_t *ent)
{
	return MM_GhostSpawnUsesDeferredPresentation(
		ghost_abort_restart_in_progress,
		MM_Ghost_IsAbortSpawnPending(ent));
}
}

void InitClientPersistant(gentity_t *ent, gclient_t *client) {
	// backup & restore userinfo
	char userinfo[MAX_INFO_STRING];
	Q_strlcpy(userinfo, client->pers.userinfo, sizeof(userinfo));

	// [MuffMode] Deathmatch respawns and FreezeTag thaws rebuild the
	// persistent client block. Keep the current match counters for an
	// already-connected participant; fresh connections still start clean.
	const bool preserve_match_stats = MM_MatchStats_IsCollecting() && client->pers.connected;
	const mm_match_player_stats_t match_stats =
		preserve_match_stats ? client->pers.match : mm_match_player_stats_t{};

	memset(&client->pers, 0, sizeof(client->pers));
	if (preserve_match_stats)
		client->pers.match = match_stats;
	if (GhostAbortSpawnUsesDeferredPresentation(ent))
		ClientUserinfoChangedForRestore(ent, userinfo);
	else
		ClientUserinfoChanged(ent, userinfo);

	client->pers.health = 100;
	client->pers.max_health = 100;

	client->pers.medal_time = 0_sec;
	client->pers.medal_type = MEDAL_NONE;
	client->pers.medal_count[MEDAL_EXCELLENT] = 0;
	client->pers.medal_count[MEDAL_HUMILIATION] = 0;
	client->pers.medal_count[MEDAL_IMPRESSIVE] = 0;

	// don't give us weapons if we shouldn't have any
	if (ClientIsPlaying(client)) {
		bool taken_loadout = false;

		MM_ApplyStartingHealthArmor(ent, client);

		if (coop->integer) {
			for (auto player : active_clients()) {
				if (player == ent || !player->client->pers.spawned ||
						!ClientIsPlaying(player->client) ||
						player->movetype == MOVETYPE_NOCLIP || player->movetype == MOVETYPE_FREECAM)
					continue;

				client->pers.inventory = player->client->pers.inventory;
				client->pers.max_ammo = player->client->pers.max_ammo;
				client->pers.power_cubes = player->client->pers.power_cubes;
				taken_loadout = true;
				break;
			}
		}

		MM_ApplySpawnLoadout(ent, client, taken_loadout);

		if (!taken_loadout) {
			// [MuffMode] A multi-arena room's resolved loadout is authoritative.
			// Global/world start items and the selectable global grapple must not
			// add equipment that this arena disabled.
			if (notGT(GT_ARENA)) {
				if (*g_start_items->string)
					Player_GiveStartItems(ent, g_start_items->string);
				if (level.start_items && *level.start_items)
					Player_GiveStartItems(ent, level.start_items);
			}

			if (false) // Race mode removed
				client->pers.inventory[IT_COMPASS] = 1;
			else if (!deathmatch->integer || level.match_state < match_state_t::MATCH_IN_PROGRESS)
				// compass also used for ready status toggling in deathmatch
				client->pers.inventory[IT_COMPASS] = 1;

			if (notGT(GT_ARENA)) {
				bool give_grapple = (!strcmp(g_allow_grapple->string, "auto")) ?
					(GTF(GTF_CTF) ? !level.no_grapple : 0) :
					(g_allow_grapple->integer && !g_grapple_offhand->integer);
				if (give_grapple)
					client->pers.inventory[IT_WEAPON_GRAPPLE] = 1;
			}
		}

		MM_ClampClientPersistHealthArmor(client);

		NoAmmoWeaponChange(ent, false);

		client->pers.weapon = client->newweapon;
		if (client->newweapon)
			client->pers.selected_item = client->newweapon->id;
		client->newweapon = nullptr;
		client->pers.lastweapon = client->pers.weapon;
	}

	if (InCoopStyle() && g_coop_enable_lives->integer)
		client->pers.lives = g_coop_num_lives->integer + 1;

	if (ent->client->pers.autoshield >= AUTO_SHIELD_AUTO)
		ent->flags |= FL_WANTS_POWER_ARMOR;

	client->pers.connected = true;
	client->pers.spawned = true;
}

static void InitClientResp(gclient_t *cl) {
	bool showed_help = cl->resp.showed_help;
	team_t team = cl->sess.team;
	int motd_mod_count = cl->resp.motd_mod_count;

	char netname[MAX_NETNAME];
	Q_strlcpy(netname, cl->resp.netname, sizeof(netname));

	memset(&cl->resp, 0, sizeof(cl->resp));

	cl->resp.showed_help = showed_help;

	Q_strlcpy(cl->resp.netname, netname, sizeof(cl->resp.netname));

	cl->resp.entertime = level.time;
	cl->resp.coop_respawn = cl->pers;

	cl->resp.motd_mod_count = motd_mod_count;

	cl->sess.team = team;
}

/*
==================
SaveClientData

Some information that should be persistant, like health,
is still stored in the entity structure, so it needs to
be mirrored out to the client structure before all the
gentities are wiped.
==================
*/
void SaveClientData() {
	gentity_t *ent;

	for (size_t i = 0; i < game.maxclients; i++) {
		ent = &g_entities[1 + i];
		if (!ent->inuse)
			continue;
		game.clients[i].pers.health = ent->health;
		game.clients[i].pers.max_health = ent->max_health;
		game.clients[i].pers.saved_flags = (ent->flags & (FL_FLASHLIGHT | FL_GODMODE | FL_NOTARGET | FL_POWER_ARMOR | FL_WANTS_POWER_ARMOR));
		if (coop->integer)
			game.clients[i].pers.score = ent->client->resp.score;
	}
}

void FetchClientEntData(gentity_t *ent) {
	ent->health = ent->client->pers.health;
	ent->max_health = ent->client->pers.max_health;
	ent->flags |= ent->client->pers.saved_flags;
	if (coop->integer)
		G_SetPlayerScore(ent->client, ent->client->pers.score);
}

/*
=======================================================================

  SelectSpawnPoint

=======================================================================
*/

// Player spawn entities and spawn-point selection live in sgame/client/spawn_points.cpp.

//======================================================================

void InitBodyQue() {
	gentity_t *ent;

	level.body_que = 0;
	for (size_t i = 0; i < BODY_QUEUE_SIZE; i++) {
		ent = G_Spawn();
		ent->classname = "bodyque";
	}
}

static DIE(body_die) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod) -> void {
	if (self->s.modelindex == MODELINDEX_PLAYER &&
		self->health < self->gib_health) {
		gi.sound(self, CHAN_BODY, gi.soundindex("misc/udeath.wav"), 1, ATTN_NORM, 0);
		ThrowGibs(self, damage, { { 4, "models/objects/gibs/sm_meat/tris.md2" } });
		self->s.origin[2] -= 48;
		ThrowClientHead(self, damage);
	}

	if (mod.id == MOD_CRUSH) {
		// prevent explosion singularities
		self->svflags = SVF_NOCLIENT;
		self->takedamage = false;
		self->solid = SOLID_NOT;
		self->movetype = MOVETYPE_NOCLIP;
		gi.linkentity(self);
	}
}

/*
=============
BodySink

After sitting around for x seconds, fall into the ground and disappear
=============
*/
static THINK(BodySink) (gentity_t *ent) -> void {
	if (!ent->linked)
		return;

	if (level.time > ent->timestamp) {
		ent->svflags = SVF_NOCLIENT;
		ent->takedamage = false;
		ent->solid = SOLID_NOT;
		ent->movetype = MOVETYPE_NOCLIP;

		// the body ques are never actually freed, they are just unlinked
		gi.unlinkentity(ent);
		return;
	}
	ent->nextthink = level.time + 50_ms;
	ent->s.origin[2] -= 0.5;
	gi.linkentity(ent);
}

void CopyToBodyQue(gentity_t *ent) {
	// if we were completely removed, don't bother with a body
	if (!ent->s.modelindex)
		return;

	gentity_t *body;

	// grab a body que and cycle to the next one
	if (level.body_que < 0 || level.body_que >= BODY_QUEUE_SIZE)
		level.body_que = 0;
	body = &g_entities[game.maxclients + level.body_que + 1];
	level.body_que = (level.body_que + 1) % BODY_QUEUE_SIZE;

	// FIXME: send an effect on the removed body

	gi.unlinkentity(ent);

	gi.unlinkentity(body);
	body->s = ent->s;
	body->s.number = body - g_entities;
	body->s.skinnum = ent->s.skinnum & 0xFF; // only copy the client #

	body->s.effects = EF_NONE;
	body->s.renderfx = RF_NONE;

	body->svflags = ent->svflags;
	body->absmin = ent->absmin;
	body->absmax = ent->absmax;
	body->size = ent->size;
	body->solid = ent->solid;
	body->clipmask = ent->clipmask;
	body->owner = ent->owner;
	body->movetype = ent->movetype;
	body->health = ent->health;
	body->gib_health = ent->gib_health;
	body->s.event = EV_OTHER_TELEPORT;
	body->velocity = ent->velocity;
	body->avelocity = ent->avelocity;
	body->groundentity = ent->groundentity;
	body->groundentity_linkcount = ent->groundentity_linkcount;
	// Body-queue entities are reused. Always replace their spatial room so a
	// corpse cannot block, receive damage from, or pause with another arena.
	body->arena = GT(GT_ARENA) ? MM_Arena_Id(ent) : ent->arena;

	if (ent->takedamage) {
		body->mins = ent->mins;
		body->maxs = ent->maxs;
	} else
		body->mins = body->maxs = {};

	if (g_corpse_sink_time->value > 0) {
		body->timestamp = level.time + gtime_t::from_sec(g_corpse_sink_time->value + 1.5);
		body->nextthink = level.time + gtime_t::from_sec(g_corpse_sink_time->value);
		body->think = BodySink;
	}

	body->die = body_die;
	body->takedamage = true;

	if (ClientArenaEliminationCorpse(ent->client)) {
		body->takedamage = false;
		body->solid = SOLID_NOT;
		body->movetype = MOVETYPE_NONE;
		body->mins = body->maxs = {};
	}

	gi.linkentity(body);
}

void G_PostRespawn(gentity_t *self) {
	if (self->svflags & SVF_NOCLIENT)
		return;

	// add a teleportation effect
	self->s.event = EV_PLAYER_TELEPORT;

	// hold in place briefly
	self->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
	self->client->ps.pmove.pm_time = 112;
	
	self->client->respawn_min_time = 0_ms;
	self->client->respawn_time = level.time;
	
	if (deathmatch->integer && level.match_state == match_state_t::MATCH_WARMUP_READYUP)
		BroadcastReadyReminderMessage();
}

static bool ClientArenaEliminationRound(const gclient_t *client) {
	if (!client || !client->eliminated)
		return false;
	if (GT(GT_ARENA))
		return MM_Arena_IsEliminated(client);
	return GTF(GTF_ARENA) && GTF(GTF_ELIMINATION) && GTF(GTF_ROUNDS) &&
		level.match_state == match_state_t::MATCH_IN_PROGRESS &&
		level.round_state == round_state_t::ROUND_IN_PROGRESS;
}

static bool ClientHordeEliminatedRound(const gclient_t *client) {
	return client && client->eliminated && GT(GT_HORDE) &&
		level.match_state == match_state_t::MATCH_IN_PROGRESS &&
		(level.round_state == round_state_t::ROUND_IN_PROGRESS || level.round_state == round_state_t::ROUND_ENDED) &&
		client->sess.team != TEAM_SPECTATOR;
}

static bool ClientArenaBotMidRoundRespawnBlocked(const gclient_t *client) {
	return client && GTF(GTF_ARENA) && GTF(GTF_ELIMINATION) && GTF(GTF_ROUNDS) &&
		level.match_state == match_state_t::MATCH_IN_PROGRESS &&
		level.round_state == round_state_t::ROUND_IN_PROGRESS &&
		notGT(GT_HORDE) && notGT(GT_LMS);
}

void ClientSetEliminated(gentity_t *self) {
	self->client->eliminated = true;
}

void ClientRespawn(gentity_t *ent) {
	if (deathmatch->integer || coop->integer) {
		// spectators don't leave bodies; arena elimination corpses are queued at death
		if (ClientIsPlaying(ent->client) &&
				(!ClientArenaEliminationCorpse(ent->client) || ent->s.modelindex))
			CopyToBodyQue(ent);
		ent->svflags &= ~SVF_NOCLIENT;

		bool rr_defected = false;
		if (GT(GT_RR) && level.match_state == match_state_t::MATCH_IN_PROGRESS) {
			team_t cur = ent->client->sess.team;
			team_t other = Teams_OtherTeam(cur);
			int teammates_left = 0, opponents = 0;

			for (auto ec : active_clients()) {
				if (ec == ent || !ClientIsPlaying(ec->client))
					continue;
				if (ec->client->sess.team == cur)
					teammates_left++;
				else if (ec->client->sess.team == other)
					opponents++;
			}

			// The final player on a side can defect too; the emptied team is what ends
			// the Red Rover round. Keep the team guard so corrupt state never lands on spectator.
			if ((cur == TEAM_RED || cur == TEAM_BLUE) && (teammates_left > 0 || opponents > 0)) {
				ent->client->sess.team = other;
				P_PublishEngineTeam(ent);
				G_AssignPlayerSkin(ent, ent->client->pers.skin);
				rr_defected = true;
			}
		}

		ClientSpawn(ent);
		G_PostRespawn(ent);

		// announce the new team right after spawning so the player knows where they landed
		if (rr_defected)
			gi.LocClient_Print(ent, PRINT_CENTER, "You are now on the {} team!", Teams_TeamName(ent->client->sess.team));
		return;
	}

	// restart the entire server
	gi.AddCommandString("menu_loadgame\n");
}

//==============================================================

// [Paril-KEX]
// skinnum was historically used to pack data
// so we're going to build onto that.
static uint8_t P_CurrentEngineTeamIndex(gentity_t *ent) {
	if (!ent || !ent->client)
		return 0;

	if (InCoopStyle())
		return 1; // all players are teamed in coop
	if (GT(GT_ARENA))
		// KEX team IDs are a red/blue/none presentation contract. Arena's
		// persistent logical-team identity remains in resp.arena_team_id.
		return P_EngineTeamIndex(ent->client->resp.arena_side);
	if (Teams())
		return P_EngineTeamIndex(ent->client->sess.team);

	return 0;
}

static uint8_t P_CurrentPackedTeamIndex(gentity_t *ent) {
	const uint8_t team_index = P_CurrentEngineTeamIndex(ent);
	// player_skinnum_t has only four team bits. Keep the bounded projection
	// explicit so future engine-side team identifiers cannot wrap.
	return team_index <= 0x0f ? team_index : 0;
}

void P_PublishEngineTeam(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	const uint8_t team_index = P_CurrentEngineTeamIndex(ent);

	ent->client->ps.team_id = team_index;
	ent->sv.team = team_index;

	if (ent->s.modelindex == MODELINDEX_PLAYER) {
		player_skinnum_t packed;
		packed.skinnum = ent->s.skinnum;
		packed.team_index = P_CurrentPackedTeamIndex(ent);
		ent->s.skinnum = packed.skinnum;
	}
}

void P_AssignClientSkinnum(gentity_t *ent) {
	if (ent->s.modelindex != MODELINDEX_PLAYER) {
		P_PublishEngineTeam(ent);
		return;
	}

	player_skinnum_t packed;

	packed.client_num = ent->client - game.clients;
	if (ent->client->pers.weapon)
		packed.vwep_index = ent->client->pers.weapon->vwep_index - level.vwep_offset + 1;
	else
		packed.vwep_index = 0;
	packed.viewheight = ent->client->ps.viewoffset.z + ent->client->ps.pmove.viewheight;

	packed.team_index = P_CurrentPackedTeamIndex(ent);

	if (ent->deadflag)
		packed.poi_icon = 1;
	else
		packed.poi_icon = 0;

	ent->s.skinnum = packed.skinnum;
	P_PublishEngineTeam(ent);
}

// [Paril-KEX] send player level POI
void P_SendLevelPOI(gentity_t *ent) {
	if (!level.valid_poi)
		return;

	gi.WriteByte(svc_poi);
	gi.WriteShort(POI_OBJECTIVE);
	gi.WriteShort(10000);
	gi.WritePosition(ent->client->help_poi_location);
	gi.WriteShort(ent->client->help_poi_image);
	gi.WriteByte(208);
	gi.WriteByte(POI_FLAG_NONE);
	gi.unicast(ent, true);
}

// [Paril-KEX] force the fog transition on the given player,
// optionally instantaneously (ignore any transition time). A forced publish
// also sends values that compare equal, for a newly connected client.
void P_ForceFogTransition(gentity_t *ent, bool instant, bool force) {
	// sanity check; if we're not changing the values, don't bother
	if (!force && ent->client->fog == ent->client->pers.wanted_fog &&
		ent->client->heightfog == ent->client->pers.wanted_heightfog)
		return;

	svc_fog_data_t fog{};

	// check regular fog
	if (force || ent->client->pers.wanted_fog[0] != ent->client->fog[0] ||
		ent->client->pers.wanted_fog[4] != ent->client->fog[4]) {
		fog.bits |= svc_fog_data_t::BIT_DENSITY;
		fog.density = ent->client->pers.wanted_fog[0];
		fog.skyfactor = ent->client->pers.wanted_fog[4] * 255.f;
	}
	if (force || ent->client->pers.wanted_fog[1] != ent->client->fog[1]) {
		fog.bits |= svc_fog_data_t::BIT_R;
		fog.red = ent->client->pers.wanted_fog[1] * 255.f;
	}
	if (force || ent->client->pers.wanted_fog[2] != ent->client->fog[2]) {
		fog.bits |= svc_fog_data_t::BIT_G;
		fog.green = ent->client->pers.wanted_fog[2] * 255.f;
	}
	if (force || ent->client->pers.wanted_fog[3] != ent->client->fog[3]) {
		fog.bits |= svc_fog_data_t::BIT_B;
		fog.blue = ent->client->pers.wanted_fog[3] * 255.f;
	}

	if (!instant && ent->client->pers.fog_transition_time) {
		fog.bits |= svc_fog_data_t::BIT_TIME;
		fog.time = clamp(ent->client->pers.fog_transition_time.milliseconds(), (int64_t)0, (int64_t)std::numeric_limits<uint16_t>::max());
	}

	// check heightfog stuff
	auto &hf = ent->client->heightfog;
	const auto &wanted_hf = ent->client->pers.wanted_heightfog;

	if (force || hf.falloff != wanted_hf.falloff) {
		fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_FALLOFF;
		if (!wanted_hf.falloff)
			fog.hf_falloff = 0;
		else
			fog.hf_falloff = wanted_hf.falloff;
	}
	if (force || hf.density != wanted_hf.density) {
		fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_DENSITY;

		if (!wanted_hf.density)
			fog.hf_density = 0;
		else
			fog.hf_density = wanted_hf.density;
	}

	if (force || hf.start[0] != wanted_hf.start[0]) {
		fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_START_R;
		fog.hf_start_r = wanted_hf.start[0] * 255.f;
	}
	if (force || hf.start[1] != wanted_hf.start[1]) {
		fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_START_G;
		fog.hf_start_g = wanted_hf.start[1] * 255.f;
	}
	if (force || hf.start[2] != wanted_hf.start[2]) {
		fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_START_B;
		fog.hf_start_b = wanted_hf.start[2] * 255.f;
	}
	if (force || hf.start[3] != wanted_hf.start[3]) {
		fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_START_DIST;
		fog.hf_start_dist = wanted_hf.start[3];
	}

	if (force || hf.end[0] != wanted_hf.end[0]) {
		fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_END_R;
		fog.hf_end_r = wanted_hf.end[0] * 255.f;
	}
	if (force || hf.end[1] != wanted_hf.end[1]) {
		fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_END_G;
		fog.hf_end_g = wanted_hf.end[1] * 255.f;
	}
	if (force || hf.end[2] != wanted_hf.end[2]) {
		fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_END_B;
		fog.hf_end_b = wanted_hf.end[2] * 255.f;
	}
	if (force || hf.end[3] != wanted_hf.end[3]) {
		fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_END_DIST;
		fog.hf_end_dist = wanted_hf.end[3];
	}

	if (fog.bits & 0xFF00)
		fog.bits |= svc_fog_data_t::BIT_MORE_BITS;

	gi.WriteByte(svc_fog);

	if (fog.bits & svc_fog_data_t::BIT_MORE_BITS)
		gi.WriteShort(fog.bits);
	else
		gi.WriteByte(fog.bits);

	if (fog.bits & svc_fog_data_t::BIT_DENSITY) {
		gi.WriteFloat(fog.density);
		gi.WriteByte(fog.skyfactor);
	}
	if (fog.bits & svc_fog_data_t::BIT_R)
		gi.WriteByte(fog.red);
	if (fog.bits & svc_fog_data_t::BIT_G)
		gi.WriteByte(fog.green);
	if (fog.bits & svc_fog_data_t::BIT_B)
		gi.WriteByte(fog.blue);
	if (fog.bits & svc_fog_data_t::BIT_TIME)
		gi.WriteShort(fog.time);

	if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_FALLOFF)
		gi.WriteFloat(fog.hf_falloff);
	if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_DENSITY)
		gi.WriteFloat(fog.hf_density);

	if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_START_R)
		gi.WriteByte(fog.hf_start_r);
	if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_START_G)
		gi.WriteByte(fog.hf_start_g);
	if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_START_B)
		gi.WriteByte(fog.hf_start_b);
	if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_START_DIST)
		gi.WriteLong(fog.hf_start_dist);

	if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_END_R)
		gi.WriteByte(fog.hf_end_r);
	if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_END_G)
		gi.WriteByte(fog.hf_end_g);
	if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_END_B)
		gi.WriteByte(fog.hf_end_b);
	if (fog.bits & svc_fog_data_t::BIT_HEIGHTFOG_END_DIST)
		gi.WriteLong(fog.hf_end_dist);

	gi.unicast(ent, true);

	ent->client->fog = ent->client->pers.wanted_fog;
	hf = wanted_hf;
}

static void MoveClientToFreeCam(gentity_t *ent) {
	ent->movetype = MOVETYPE_FREECAM;
	ent->solid = SOLID_NOT;
	ent->svflags |= SVF_NOCLIENT;
	ent->client->sess.spectator_state = SPECTATOR_FREE;
	ent->client->ps.gunindex = 0;
	ent->client->ps.gunskin = 0;

	ent->client->ps.stats[STAT_SHOW_STATUSBAR] = 0;

	ent->takedamage = false;
	ent->s.modelindex = 0;
	ent->s.modelindex2 = 0;
	ent->s.modelindex3 = 0;
	ent->s.effects = EF_NONE;
	ent->client->ps.damage_blend[3] = ent->client->ps.screen_blend[3] = 0;
	ent->client->ps.rdflags = RDF_NONE;
	ent->s.sound = 0;
	P_PublishEngineTeam(ent);

	gi.linkentity(ent);
}

/*
===========
InitPlayerTeam
============
*/
static bool InitPlayerTeam(gentity_t *ent) {
	if (!deathmatch->integer) {
		ent->client->sess.team = TEAM_FREE;
		P_PublishEngineTeam(ent);
		ent->client->ps.stats[STAT_SHOW_STATUSBAR] = 1;
		return true;
	}

	// already initialised. ClientConnect pre-sets fresh deathmatch clients to
	// TEAM_SPECTATOR before first spawn, which made this early-return swallow
	// every human before the g_dm_force_join / g_dm_auto_join block below could
	// run (the cvars were dead for humans). An uninitialised spectator is really
	// a brand-new client awaiting first join, so let them fall through. [MuffMode]
	if (ent->client->sess.team != TEAM_NONE &&
		(ent->client->sess.initialised || ent->client->sess.team != TEAM_SPECTATOR))
		return true;

	ent->client->sess.team = TEAM_SPECTATOR;
	P_PublishEngineTeam(ent);
	MoveClientToFreeCam(ent);
	
	const bool may_auto_join = MM_GhostSpawnMayAutoJoin(
		GhostAbortSpawnUsesDeferredPresentation(ent));
	if (may_auto_join &&
		(ent->client->sess.is_a_bot || (ent->svflags & SVF_BOT) ||
			g_dm_force_join->integer || g_dm_auto_join->integer)) {
		if (ent != &g_entities[1] || (ent == &g_entities[1] && g_owner_auto_join->integer)) {
			team_t pick = PickTeam(-1);
			if (!level.locked[pick]) {
				SetTeam(ent, pick, false, false, false);
				return true;
			}
		}
	}

	if (!ent->client->initial_menu_shown)
		ent->client->initial_menu_delay = level.time + 10_hz;

	return false;
}

// [Paril-KEX] ugly global to handle squad respawn origin
static bool use_squad_respawn = false;
static bool spawn_from_begin = false;
static vec3_t squad_respawn_position, squad_respawn_angles;

static inline void PutClientOnSpawnPoint(gentity_t *ent, const vec3_t &spawn_origin, const vec3_t &spawn_angles) {
	gclient_t *client = ent->client;

	client->spawn_origin = spawn_origin;
	client->ps.pmove.origin = spawn_origin;

	ent->s.origin = spawn_origin;
	if (!use_squad_respawn)
		ent->s.origin[2] += 1; // make sure off ground
	ent->s.old_origin = ent->s.origin;

	// set the delta angle
	client->ps.pmove.delta_angles = spawn_angles - client->resp.cmd_angles;

	ent->s.angles = spawn_angles;
	//ent->s.angles[PITCH] /= 3;		//muff: why??

	client->ps.viewangles = ent->s.angles;
	client->v_angle = ent->s.angles;

	AngleVectors(client->v_angle, client->v_forward, nullptr, nullptr);
}

/*
===========
ClientSpawn

Previously known as 'PutClientInServer'

Called when a player connects to a server or respawns in
a deathmatch.
============
*/
void ClientSpawn(gentity_t *ent) {
	int						index = ent - g_entities - 1;
	vec3_t					spawn_origin, spawn_angles;
	gclient_t				*client = ent->client;
	client_persistant_t		saved;
	client_respawn_t		resp{};
	client_session_t		sess = client->sess;
	const gtime_t			horde_reinforcement_protection = client->pers.horde_reinforcement_protection;
	const gtime_t			horde_elim_msg_next = client->horde_elim_msg_next;
	const bool				defer_ghost_presentation =
		GhostAbortSpawnUsesDeferredPresentation(ent);

	if (GTF(GTF_ROUNDS) && level.match_state == match_state_t::MATCH_IN_PROGRESS && notGT(GT_HORDE)) {
		const bool round_locked = level.round_state == round_state_t::ROUND_IN_PROGRESS ||
			level.round_state == round_state_t::ROUND_ENDED;
		if (round_locked) {
			const bool freeze_thaw_respawn = MM_FreezeTag_IsFrozen(ent);

			// LMS uses the Horde-style lives model: a mid-round (re)spawn with lives left is a
			// living respawn, not an elimination. Only auto-eliminate LMS spawners who are out
			// of lives (latecomers/round-joiners get no lives, so they spectate until next round).
			if (!freeze_thaw_respawn && GTF(GTF_ELIMINATION) && (notGT(GT_LMS) || ent->client->pers.lives <= 0))
				ClientSetEliminated(ent);
			else if (ClientIsPlaying(ent->client) && !freeze_thaw_respawn && MM_FreezeTag_ShouldHoldSpawnForRound())
				ClientSetEliminated(ent);
		}
	}

	G_ClearLagCompensationHistory(ent);
	MM_FreezeTag_ClearClient(ent);

	if (GT(GT_HORDE) && level.match_state == match_state_t::MATCH_IN_PROGRESS &&
		MM_Horde_ShouldEliminateMidWaveSpawn(level.round_state == round_state_t::ROUND_IN_PROGRESS,
			ent->client->eliminated, ent->client->pers.lives))
		ClientSetEliminated(ent);
	bool eliminated = ent->client->eliminated;
	int lives = 0;
	if (InCoopStyle() && notGT(GT_HORDE) && g_coop_enable_lives->integer)
		lives = ent->client->pers.spawned ? ent->client->pers.lives : g_coop_enable_lives->integer + 1;
	else if (GT(GT_HORDE) && ClientIsPlaying(ent->client))
		lives = ent->client->pers.lives;
	
	// clear velocity now, since landmark may change it
	ent->velocity = {};

	if (client->landmark_name != nullptr)
		ent->velocity = client->oldvelocity;

	// find a spawn point
	// do it before setting health back up, so farthest
	// ranging doesn't count this client
	bool valid_spawn = false;
	bool force_spawn = client->awaiting_respawn && level.time > client->respawn_timeout;
	bool is_landmark = false;

	InitPlayerTeam(ent);

	if (!ClientIsPlaying(ent->client) || eliminated)
		ent->flags |= FL_NOTARGET;
	else
		ent->flags &= ~FL_NOTARGET;

	if (use_squad_respawn) {
		spawn_origin = squad_respawn_position;
		spawn_angles = squad_respawn_angles;
		valid_spawn = true;
	} else
		valid_spawn = SelectSpawnPoint(ent, spawn_origin, spawn_angles, force_spawn, is_landmark);

	// [Paril-KEX] if we didn't get a valid spawn, hold us in
	// limbo for a while until we do get one
	if (!valid_spawn) {
		// only do this once per spawn
		if (!client->awaiting_respawn) {
			char userinfo[MAX_INFO_STRING];
			memcpy(userinfo, client->pers.userinfo, sizeof(userinfo));
			if (defer_ghost_presentation)
				ClientUserinfoChangedForRestore(ent, userinfo);
			else
				ClientUserinfoChanged(ent, userinfo);

			client->respawn_timeout = level.time + 3_sec;
		}

		// find a spot to place us
		if (!level.intermission_spot && !level.level_intermission_set)
			SetIntermissionPoint();

		ent->s.origin = level.intermission_origin;
		ent->client->ps.pmove.origin = level.intermission_origin;
		ent->client->ps.viewangles = level.intermission_angle;

		client->awaiting_respawn = true;
		client->ps.pmove.pm_type = PM_FREEZE;
		client->ps.rdflags = RDF_NONE;
		ent->deadflag = false;

		MoveClientToFreeCam(ent);
		if (GT(GT_ARENA) && MM_Arena_Id(ent) > 0)
			client->ps.rdflags = RDF_NOWORLDMODEL;
		gi.linkentity(ent);

		return;
	}
	
	client->resp.ctf_state++;

	bool was_waiting_for_respawn = client->awaiting_respawn;

	if (client->awaiting_respawn)
		ent->svflags &= ~SVF_NOCLIENT;

	client->awaiting_respawn = false;
	client->respawn_timeout = 0_ms;

	char social_id[MAX_INFO_VALUE];
	Q_strlcpy(social_id, ent->client->pers.social_id, sizeof(social_id));

	// deathmatch wipes most client data every spawn
	if (deathmatch->integer) {
		client->pers.health = 0;
		resp = client->resp;
		sess = client->sess;
	} else {
		// [Kex] Maintain user info in singleplayer to keep the player skin. 
		char userinfo[MAX_INFO_STRING];
		memcpy(userinfo, client->pers.userinfo, sizeof(userinfo));

		if (coop->integer) {
			resp = client->resp;
			sess = client->sess;

			if (!P_UseCoopInstancedItems()) {
				resp.coop_respawn.game_help1changed = client->pers.game_help1changed;
				resp.coop_respawn.game_help2changed = client->pers.game_help2changed;
				resp.coop_respawn.helpchanged = client->pers.helpchanged;
				client->pers = resp.coop_respawn;
			} else {
				// fix weapon
				if (!client->pers.weapon)
					client->pers.weapon = client->pers.lastweapon;
			}
		}

		ClientUserinfoChanged(ent, userinfo);

		if (coop->integer) {
			if (resp.score > client->pers.score)
				client->pers.score = resp.score;
		} else {
			resp = {};
			sess.team = TEAM_FREE;
		}
	}

	// clear everything but the persistant data
	saved = client->pers;
	memset(client, 0, sizeof(*client));
	client->pers = saved;
	client->resp = resp;
	client->sess = sess;
	if (GT(GT_HORDE) && eliminated)
		client->horde_elim_msg_next = horde_elim_msg_next;
	client->chaingun_shots = 1; // first shot deals 5 damage (4 + (1 & 1))

	// on a new, fresh spawn (always in DM, clear inventory
	// or new spawns in SP/coop)
	const bool horde_elim_spectator = GT(GT_HORDE) && eliminated && ClientIsPlaying(client);
	const bool horde_wave_rejoin = GT(GT_HORDE) && ClientIsPlaying(client) && !eliminated &&
		level.round_state == round_state_t::ROUND_COUNTDOWN && level.round_number > 0 &&
		client->pers.weapon != nullptr;
	const bool ghost_abort_needs_persistent_initialization =
		MM_GhostSpawnNeedsPersistentInitialization(defer_ghost_presentation,
			client->pers.spawned);

	// A restore abort begins from a deliberately zeroed client. Horde normally
	// preserves an eliminated fighter's existing loadout, but there is no prior
	// loadout to preserve here. Initialize it once so this valid freecam spawn is
	// complete and the next wave cannot inherit one health with no weapon.
	if (ghost_abort_needs_persistent_initialization ||
		(client->pers.health <= 0 && !horde_elim_spectator && !horde_wave_rejoin))
		InitClientPersistant(ent, client);
	else if (horde_wave_rejoin) {
		if (client->pers.max_health < 1)
			MM_ApplyStartingHealthArmor(ent, client);
		else
			client->pers.health = client->pers.max_health;
	}

	// InitClientPersistant clears pers on a successful deathmatch spawn; carry the
	// rally duration across that reset so delayed and immediate spawns behave alike.
	if (GT(GT_HORDE) && horde_reinforcement_protection > 0_ms)
		client->pers.horde_reinforcement_protection = horde_reinforcement_protection;

	// restore social ID
	Q_strlcpy(ent->client->pers.social_id, social_id, sizeof(social_id));

	// fix level switch issue
	ent->client->pers.connected = true;

	// slow time will be unset here
	globals.server_flags &= ~SERVER_FLAG_SLOW_TIME;

	// copy some data from the client to the entity
	FetchClientEntData(ent);

	// clear entity values
	ent->groundentity = nullptr;
	ent->client = &game.clients[index];
	ent->takedamage = true;
	ent->movetype = MOVETYPE_WALK;
	ent->viewheight = 22;
	ent->inuse = true;
	ent->classname = "player";
	ent->mass = 200;
	ent->solid = SOLID_BBOX;
	ent->deadflag = false;
	ent->air_finished = level.time + 12_sec;
	ent->clipmask = MASK_PLAYERSOLID;
	ent->model = "players/male/tris.md2";
	ent->die = player_die;
	ent->waterlevel = WATER_NONE;
	ent->watertype = CONTENTS_NONE;
	ent->flags &= ~(FL_NO_KNOCKBACK | FL_ALIVE_KNOCKBACK_ONLY | FL_NO_DAMAGE_EFFECTS | FL_SAM_RAIMI);
	ent->svflags &= ~SVF_DEADMONSTER;
	ent->svflags |= SVF_PLAYER;
	ent->client->pers.last_spawn_time = level.time;

	ent->mins = PLAYER_MINS;
	ent->maxs = PLAYER_MAXS;

	ent->client->pers.lives = lives;

	// clear playerstate values
	memset(&ent->client->ps, 0, sizeof(client->ps));

	char val[MAX_INFO_VALUE];
	if (gi.Info_ValueForKey(ent->client->pers.userinfo, "fov", val, sizeof(val)))
		ent->client->ps.fov = muffmode::player::ParseUserinfoFov(val);
	else
		ent->client->ps.fov = muffmode::player::kDefaultUserinfoFov;

	ent->client->ps.pmove.viewheight = ent->viewheight;

	// [MuffMode] Restore engine team identity right after clearing playerstate,
	// matching stock q2re's timing and keeping KEX lobby team-chat metadata fresh.
	P_PublishEngineTeam(ent);

	if (!G_ShouldPlayersCollide(false))
		ent->clipmask &= ~CONTENTS_PLAYER;

	if (client->pers.weapon)
		client->ps.gunindex = gi.modelindex(client->pers.weapon->view_model);
	else
		client->ps.gunindex = 0;
	client->ps.gunskin = 0;

	// clear entity state values
	ent->s.effects = EF_NONE;
	ent->s.modelindex = MODELINDEX_PLAYER;	// will use the skin specified model
	ent->s.modelindex2 = MODELINDEX_PLAYER; // custom gun model
	// sknum is player num and weapon number
	// weapon number will be added in changeweapon
	P_AssignClientSkinnum(ent);
	MM_Ghost_CompleteAbortSpawn(ent);

	CalculateRanks();

	ent->s.frame = 0;

	PutClientOnSpawnPoint(ent, spawn_origin, spawn_angles);

	// [Paril-KEX] set up world fog & send it instantly
	ent->client->pers.wanted_fog = {
		world->fog.density,
		world->fog.color[0],
		world->fog.color[1],
		world->fog.color[2],
		world->fog.sky_factor
	};
	ent->client->pers.wanted_heightfog = {
		{ world->heightfog.start_color[0], world->heightfog.start_color[1], world->heightfog.start_color[2], world->heightfog.start_dist },
		{ world->heightfog.end_color[0], world->heightfog.end_color[1], world->heightfog.end_color[2], world->heightfog.end_dist },
		world->heightfog.falloff,
		world->heightfog.density
	};
	P_ForceFogTransition(ent, true);
	
	// spawn as spectator
	if (!ClientIsPlaying(client) || eliminated) {
		FreeFollower(ent);

		MoveClientToFreeCam(ent);
		ent->client->ps.stats[STAT_SHOW_STATUSBAR] = 0;
		if (!ent->client->initial_menu_shown)
			ent->client->initial_menu_delay = level.time + 10_hz;
		ent->client->eliminated = eliminated;
		const bool may_auto_follow = MM_GhostSpawnMayAutoFollow(
			defer_ghost_presentation);
		if (eliminated &&
			(GT(GT_ARENA) || (GTF(GTF_ARENA) && GTF(GTF_ELIMINATION)))) {
			if (may_auto_follow)
				GetFollowTarget(ent);
		}
		else if (eliminated && GT(GT_HORDE)) {
			if (may_auto_follow)
				GetFollowTarget(ent);
			MM_Horde_NotifyEliminatedSpectator(ent);
			ent->client->pers.health = 1; // prevent pers.health <= 0 from forcing scoreboard layout in freecam
		}
		gi.linkentity(ent);
		return;
	}

	if (GT(GT_HORDE) && client->pers.horde_reinforcement_protection > 0_ms) {
		client->pu_time_protection = level.time + client->pers.horde_reinforcement_protection;
		client->pers.horde_reinforcement_protection = 0_ms;
	}

	ent->client->ps.stats[STAT_SHOW_STATUSBAR] = 1;

	// ensure playing players are visible (clear SVF_NOCLIENT that may have been set by Entities_Reset)
	ent->svflags &= ~SVF_NOCLIENT;

	// [Paril-KEX] a bit of a hack, but landmark spawns can sometimes cause
	// intersecting spawns, so we'll do a sanity check here...
	if (spawn_from_begin) {
		if (coop->integer) {
			gentity_t *collision = G_UnsafeSpawnPosition(ent->s.origin, true, ent);

			if (collision) {
				gi.linkentity(ent);

				if (collision->client) {
					// we spawned in somebody else, so we're going to change their spawn position
					bool lm = false;
					SelectSpawnPoint(collision, spawn_origin, spawn_angles, true, lm);
					PutClientOnSpawnPoint(collision, spawn_origin, spawn_angles);
				}
				// else, no choice but to accept where ever we spawned :(
			}
		}

		// give us one (1) free fall ticket even if
		// we didn't spawn from landmark
		ent->client->landmark_free_fall = true;
	}

	gi.linkentity(ent);

	if (!KillBox(ent, true, MOD_TELEFRAG_SPAWN)) // couldn't spawn in?
		return;

	// my tribute to cash's level-specific hacks. I hope I live
	// up to his trailblazing cheese.
	if (Q_strcasecmp(level.mapname, "rboss") == 0) {
		// if you get on to rboss in single player or coop, ensure
		// the player has the nuke key. (not in DM)
		if (!deathmatch->integer)
			client->pers.inventory[IT_KEY_NUKE] = 1;
	}
	
	// force the current weapon up
	if (MM_UsesArenaSpawnLoadout() && client->pers.inventory[IT_WEAPON_RLAUNCHER])
		client->newweapon = &itemlist[IT_WEAPON_RLAUNCHER];
	else
		client->newweapon = client->pers.weapon;
	Change_Weapon(ent);

	if (was_waiting_for_respawn)
		G_PostRespawn(ent);

	MM_MatchStats_RecordSpawn(client);
}

/*
=====================
ClientBeginDeathmatch

A client has just connected to the server in
deathmatch mode, so clear everything out before starting them.
=====================
*/
static void ClientBeginDeathmatch(gentity_t *ent, bool finish_server_frame = true) {
	// SpawnEntities marks retained clients unspawned before the engine calls
	// ClientBegin for the new map. Preserve that distinction across ClientSpawn,
	// which initializes pers.spawned again.
	const bool fresh_connection = ent->client->pers.spawned;

	G_InitGentity(ent);

	// make sure we have a known default
	ent->svflags |= SVF_PLAYER;

	InitClientResp(ent->client);
	MM_Arena_OnClientBegin(ent);

	// locate ent at a spawn point
	ClientSpawn(ent);

	if (level.intermission_time) {
		MoveClientToIntermission(ent);
	} else {
		if (fresh_connection && !(ent->svflags & SVF_NOCLIENT)) {
			// send effect
			gi.WriteByte(svc_muzzleflash);
			gi.WriteEntity(ent);
			gi.WriteByte(MZ_LOGIN);
			gi.multicast(ent->s.origin, MULTICAST_PVS, false);
		}
	}

	//gi.LocBroadcast_Print(PRINT_HIGH, "$g_entered_game", ent->client->resp.netname);

	// make sure all view stuff is valid. Ghost aborts run at the start of a live
	// game frame and will be finalized by the ordinary end-frame pass.
	if (finish_server_frame)
		ClientEndServerFrame(ent);
}

void ClientRestartAfterGhostRestoreAbort(gentity_t *ent)
{
	if (!ent || !ent->client || !ent->client->pers.connected)
		return;

	if (ent->linked)
		gi.unlinkentity(ent);

	const bool previous_restart_state = ghost_abort_restart_in_progress;
	ghost_abort_restart_in_progress = true;
	struct ghost_abort_restart_scope_t {
		bool previous;
		~ghost_abort_restart_scope_t() { ghost_abort_restart_in_progress = previous; }
	} restart_scope { previous_restart_state };

	ClientBeginDeathmatch(ent, false);
	CalculateRanks();
}

static void G_SetLevelEntry() {
	if (deathmatch->integer)
		return;
	// map is a hub map, so we shouldn't bother tracking any of this.
	// the next map will pick up as the start.
	if (level.hub_map)
		return;

	level_entry_t *found_entry = nullptr;
	int32_t highest_order = 0;

	for (size_t i = 0; i < MAX_LEVELS_PER_UNIT; i++) {
		level_entry_t *entry = &game.level_entries[i];

		highest_order = max(highest_order, entry->visit_order);

		if (!strcmp(entry->map_name, level.mapname) || !*entry->map_name) {
			found_entry = entry;
			break;
		}
	}

	if (!found_entry) {
		gi.Com_PrintFmt("WARNING: more than {} maps in unit, can't track the rest\n", MAX_LEVELS_PER_UNIT);
		return;
	}

	level.entry = found_entry;
	Q_strlcpy(level.entry->map_name, level.mapname, sizeof(level.entry->map_name));

	// we're visiting this map for the first time, so
	// mark it in our order as being recent
	if (!*level.entry->pretty_name) {
		Q_strlcpy(level.entry->pretty_name, level.level_name, sizeof(level.entry->pretty_name));
		level.entry->visit_order = highest_order + 1;

		// give all of the clients an extra life back
		if (g_coop_enable_lives->integer)
			for (auto ec : active_clients())
				ec->client->pers.lives = min(g_coop_num_lives->integer + 1, ec->client->pers.lives + 1);
	}

	// scan for all new maps we can go to, for secret levels
	gentity_t *changelevel = nullptr;
	while ((changelevel = G_FindByString<&gentity_t::classname>(changelevel, "target_changelevel"))) {
		if (!changelevel->map || !*changelevel->map)
			continue;

		// next unit map, don't count it
		if (strchr(changelevel->map, '*'))
			continue;

		const char *level = strchr(changelevel->map, '+');

		if (level)
			level++;
		else
			level = changelevel->map;

		// don't include end screen levels
		if (strstr(level, ".cin") || strstr(level, ".pcx"))
			continue;

		size_t level_length;

		const char *spawnpoint = strchr(level, '$');

		if (spawnpoint)
			level_length = spawnpoint - level;
		else
			level_length = strlen(level);

		// make an entry for this level that we may or may not visit
		level_entry_t *found_entry = nullptr;

		for (size_t i = 0; i < MAX_LEVELS_PER_UNIT; i++) {
			level_entry_t *entry = &game.level_entries[i];

			if (!*entry->map_name || !strncmp(entry->map_name, level, level_length)) {
				found_entry = entry;
				break;
			}
		}

		if (!found_entry) {
			gi.Com_PrintFmt("WARNING: more than {} maps in unit, can't track the rest\n", MAX_LEVELS_PER_UNIT);
			return;
		}

		Q_strlcpy(found_entry->map_name, level, min(level_length + 1, sizeof(found_entry->map_name)));
	}
}

/*
=================
ClientIsPlaying
=================
*/
bool ClientIsPlaying(const gclient_t *cl) {
	if (!cl) return false;

	if (!deathmatch->integer)
		return true;
	if (GT(GT_ARENA))
		return MM_Arena_IsFighter(cl);

	return !(cl->sess.team == TEAM_NONE || cl->sess.team == TEAM_SPECTATOR);
}

/*
=================
ClientCanVote

Returns true if the client is eligible to participate in votes.
This includes active players and duel-queued players (who are
technically spectators but are active participants).
=================
*/
bool ClientCanVote(gclient_t *cl) {
	if (!cl) return false;

	if (ClientIsPlaying(cl))
		return true;

	// Duel-queued players should always be able to vote
	if (GT(GT_DUEL) && cl->sess.duel_queued)
		return true;
	if (MM_Arena_ClientCanVote(cl))
		return true;

	// Spectators can vote if g_allow_spec_vote is enabled
	if (g_allow_spec_vote->integer)
		return true;

	return false;
}

/*
===========
ClientBegin

called when a client has finished connecting, and is ready
to be placed into the game.  This will happen every level load.
============
*/
void ClientBegin(gentity_t *ent) {
	// Null-ent guard; same engine teardown class as ClientThink/ClientDisconnect.
	if (!ent)
		return;

	ent->client = game.clients + (ent - g_entities - 1);
	const bool retained_level_client = !ent->client->pers.spawned &&
		!ent->client->sess.is_a_bot &&
		MM_ClientProfileCanPersistIdentity(ent->client->pers.social_id) &&
		!MM_Ghost_ReservedClientState(ent);
	ent->client->awaiting_respawn = false;
	ent->client->respawn_timeout = 0_ms;

	// SpawnEntities clears client edicts between maps, while ClientConnect is not
	// called again for retained clients. Restore engine-visible identity before
	// begin/spawn work so retained bots cannot fall through the human-client path.
	ent->svflags |= SVF_PLAYER;
	if (ent->client->sess.is_a_bot)
		ent->svflags |= SVF_BOT;
	else
		ent->svflags &= ~SVF_BOT;

	// set inactivity timer
	gtime_t cv = gtime_t::from_sec(g_inactivity->integer);
	if (cv) {
		if (cv < 15_sec) cv = 15_sec;
		ent->client->sess.inactivity_time = level.time + cv;
		ent->client->sess.inactivity_warning = false;
	}

	// [Paril-KEX] we're always connected by this point...
	ent->client->pers.connected = true;

	// ClientConnect is not repeated for retained clients. Reload a persistent
	// profile so a map that changes gametype selects the correct rating. An
	// identity that cannot form a canonical profile remains session-only and
	// deliberately keeps its preferences across the map change.
	if (retained_level_client) {
		MM_ClientInitPConfig(ent);
		MM_PlayerStats_OnProfileLoaded(ent);
	}

	// A reconnect reservation already has current connection identity and loaded
	// preferences from ClientConnect. Enter the restore delay before ordinary
	// ClientBegin republishes canonical skins or applies viewer-wide overrides;
	// the bounded restore scheduler owns those presentation writes.
	if (deathmatch->integer && notGT(GT_ARENA) && MM_Ghost_TryRestore(ent))
		return;

	// Map loads clear engine configstrings without calling ClientConnect again.
	// Republish retained name/skin data before exposing MODELINDEX_PLAYER so the
	// renderer cannot observe a player backed by a stale map model handle.
	if (ent->client->pers.userinfo[0]) {
		char userinfo[MAX_INFO_STRING];
		Q_strlcpy(userinfo, ent->client->pers.userinfo, sizeof(userinfo));
		ClientUserinfoChanged(ent, userinfo);
	}

	if (deathmatch->integer) {
		ClientBeginDeathmatch(ent);

		// count current clients and rank for scoreboard
		CalculateRanks();
		return;
	}

	// [Paril-KEX] set enter time now, so we can send messages slightly
	// after somebody first joins
	ent->client->resp.entertime = level.time;
	ent->client->pers.spawned = true;

	// if there is already a body waiting for us (a loadgame), just
	// take it, otherwise spawn one from scratch
	if (ent->inuse) {
		// the client has cleared the client side viewangles upon
		// connecting to the server, which is different than the
		// state when the game is saved, so we need to compensate
		// with deltaangles
		ent->client->ps.pmove.delta_angles = ent->client->ps.viewangles;
	} else {
		// a spawn point will completely reinitialize the entity
		// except for the persistant data that was initialized at
		// ClientConnect() time
		G_InitGentity(ent);
		ent->classname = "player";
		InitClientResp(ent->client);
		spawn_from_begin = true;
		ClientSpawn(ent);
		spawn_from_begin = false;

		if (!ent->client->pers.ingame)
			BroadcastTeamChange(ent, -1, false, false);
	}

	// make sure we have a known default
	ent->svflags |= SVF_PLAYER;

	if (level.intermission_time) {
		MoveClientToIntermission(ent);
	} else {
		// send effect if in a multiplayer game
		if (game.maxclients > 1 && !(ent->svflags & SVF_NOCLIENT))
			gi.LocBroadcast_Print(PRINT_HIGH, "$g_entered_game", ent->client->resp.netname);
	}

	level.coop_scale_players++;
	G_Monster_CheckCoopHealthScaling();

	// make sure all view stuff is valid
	ClientEndServerFrame(ent);

	// [Paril-KEX] send them goal, if needed
	G_PlayerNotifyGoal(ent);

	// [Paril-KEX] we're going to set this here just to be certain
	// that the level entry timer only starts when a player is actually
	// *in* the level
	G_SetLevelEntry();

	ent->client->pers.ingame = true;
}

static inline bool IsSlotIgnored(gentity_t *slot, gentity_t **ignore, size_t num_ignore) {
	for (size_t i = 0; i < num_ignore; i++)
		if (slot == ignore[i])
			return true;

	return false;
}

static inline gentity_t *ClientChooseSlot_Any(gentity_t **ignore, size_t num_ignore) {
	for (size_t i = 0; i < game.maxclients; i++)
		if (!IsSlotIgnored(globals.gentities + i + 1, ignore, num_ignore) &&
			!MM_Ghost_IsReservedSlot(globals.gentities + i + 1) &&
			!game.clients[i].pers.connected)
			return globals.gentities + i + 1;

	return nullptr;
}

static inline gentity_t *ClientChooseSlot_Coop(const char *userinfo, const char *social_id, bool is_bot, gentity_t **ignore, size_t num_ignore) {
	char name[MAX_INFO_VALUE] = { 0 };
	gi.Info_ValueForKey(userinfo, "name", name, sizeof(name));

	// the host should always occupy slot 0, some systems rely on this
	// (CHECK: is this true? is it just bots?)
	{
		size_t num_players = 0;

		for (size_t i = 0; i < game.maxclients; i++)
			if (IsSlotIgnored(globals.gentities + i + 1, ignore, num_ignore) || game.clients[i].pers.connected)
				num_players++;

		if (!num_players) {
			gi.Com_PrintFmt("coop slot {} is host {}+{}\n", 1, name, social_id);
			return globals.gentities + 1;
		}
	}

	// grab matches from players that we have connected
	using match_type_t = int32_t;
	enum {
		MATCH_USERNAME,
		MATCH_SOCIAL,
		MATCH_BOTH,

		MATCH_TYPES
	};

	struct {
		gentity_t *slot = nullptr;
		size_t total = 0;
	} matches[MATCH_TYPES];

	for (size_t i = 0; i < game.maxclients; i++) {
		if (IsSlotIgnored(globals.gentities + i + 1, ignore, num_ignore) || game.clients[i].pers.connected)
			continue;

		char check_name[MAX_INFO_VALUE] = { 0 };
		gi.Info_ValueForKey(game.clients[i].pers.userinfo, "name", check_name, sizeof(check_name));

		bool username_match = game.clients[i].pers.userinfo[0] &&
			!strcmp(check_name, name);

		bool social_match = social_id && game.clients[i].pers.social_id[0] &&
			!strcmp(game.clients[i].pers.social_id, social_id);

		match_type_t type = (match_type_t)0;

		if (username_match)
			type |= MATCH_USERNAME;
		if (social_match)
			type |= MATCH_SOCIAL;

		if (!type)
			continue;

		matches[type].slot = globals.gentities + i + 1;
		matches[type].total++;
	}

	// pick matches in descending order, only if the total matches
	// is 1 in the particular set; this will prefer to pick
	// social+username matches first, then social, then username last.
	for (int32_t i = 2; i >= 0; i--) {
		if (matches[i].total == 1) {
			gi.Com_PrintFmt("coop slot {} restored for {}+{}\n", (ptrdiff_t)(matches[i].slot - globals.gentities), name, social_id);

			// spawn us a ghost now since we're gonna spawn eventually
			if (!matches[i].slot->inuse) {
				matches[i].slot->s.modelindex = MODELINDEX_PLAYER;
				matches[i].slot->solid = SOLID_BBOX;

				G_InitGentity(matches[i].slot);
				matches[i].slot->classname = "player";
				InitClientResp(matches[i].slot->client);
				spawn_from_begin = true;
				ClientSpawn(matches[i].slot);
				spawn_from_begin = false;

				// make sure we have a known default
				matches[i].slot->svflags |= SVF_PLAYER;

				matches[i].slot->sv.init = false;
				matches[i].slot->classname = "player";
				matches[i].slot->client->pers.connected = true;
				matches[i].slot->client->pers.spawned = true;
				P_AssignClientSkinnum(matches[i].slot);
				gi.linkentity(matches[i].slot);
			}

			return matches[i].slot;
		}
	}

	// in the case where we can't find a match, we're probably a new
	// player, so pick a slot that hasn't been occupied yet
	for (size_t i = 0; i < game.maxclients; i++)
		if (!IsSlotIgnored(globals.gentities + i + 1, ignore, num_ignore) && !game.clients[i].pers.userinfo[0]) {
			gi.Com_PrintFmt("coop slot {} issuing new for {}+{}\n", i + 1, name, social_id);
			return globals.gentities + i + 1;
		}

	// all slots have some player data in them, we're forced to replace one.
	gentity_t *any_slot = ClientChooseSlot_Any(ignore, num_ignore);

	gi.Com_PrintFmt("coop slot {} any slot for {}+{}\n", !any_slot ? -1 : (ptrdiff_t)(any_slot - globals.gentities), name, social_id);

	return any_slot;
}

#if defined(MM_GHOST_RUNTIME_TESTING)
// Q2REPRO deliberately supplies no platform identity. Runtime soak builds may
// derive an ephemeral identity from a unique test-client name so the real
// disconnect/reconnect path can be exercised. Production builds never compile
// this fallback and continue to require the engine-authenticated social ID.
static const char *RuntimeTestSocialId(const char *userinfo, const char *social_id,
	bool is_bot, char (&derived)[MAX_INFO_VALUE])
{
	if (is_bot || (social_id && social_id[0]))
		return social_id;

	char name[MAX_INFO_VALUE]{};
	gi.Info_ValueForKey(userinfo, "name", name, sizeof(name));
	if (!name[0])
		return social_id;

	Q_strlcpy(derived, "runtime-soak:", sizeof(derived));
	Q_strlcat(derived, name, sizeof(derived));
	return derived;
}
#endif

// [Paril-KEX] for coop, we want to try to ensure that players will always get their
// proper slot back when they connect.
gentity_t *ClientChooseSlot(const char *userinfo, const char *social_id, bool is_bot, gentity_t **ignore, size_t num_ignore, bool cinematic) {
#if defined(MM_GHOST_RUNTIME_TESTING)
	char derived_social_id[MAX_INFO_VALUE]{};
	social_id = RuntimeTestSocialId(userinfo, social_id, is_bot, derived_social_id);
#endif
	// Retail providers may supply no account identity. Slot selection logs and
	// reconnect helpers still require a valid C string.
	social_id = social_id && social_id[0] ? social_id : "";
	if (!cinematic && deathmatch->integer && !is_bot)
		if (gentity_t *slot = MM_Ghost_ChooseReconnectSlot(social_id, ignore, num_ignore))
			return slot;

	// coop and non-bots is the only thing that we need to do special behavior on
	if (!cinematic && coop->integer && !is_bot)
		return ClientChooseSlot_Coop(userinfo, social_id, is_bot, ignore, num_ignore);

	// just find any free slot
	return ClientChooseSlot_Any(ignore, num_ignore);
}

/*
===========
ClientConnect

Called when a player begins connecting to the server.
The game can refuse entrance to a client by returning false.
If the client is allowed, the connection process will continue
and eventually get to ClientBegin()
Changing levels will NOT cause this to be called again, but
loadgames will.
============
*/
bool ClientConnect(gentity_t *ent, char *userinfo, const char *social_id, bool is_bot) {
	// Null-ent guard; same engine teardown class as ClientThink/ClientDisconnect.
	if (!ent)
		return false;

#if defined(MM_GHOST_RUNTIME_TESTING)
	char derived_social_id[MAX_INFO_VALUE]{};
	social_id = RuntimeTestSocialId(userinfo, social_id, is_bot, derived_social_id);
#endif
	// Retail providers may represent an unavailable account identity with a null
	// pointer. Normalize it once before any C-string consumer sees it.
	social_id = social_id && social_id[0] ? social_id : "";
	// they can connect
	ent->client = game.clients + (ent - g_entities - 1);
	MM_Ghost_CancelAbortSpawn(ent);
	ent->client->sess.team = deathmatch->integer ? TEAM_NONE : TEAM_FREE;
	// Authentication belongs to a network connection, not a reusable client
	// slot or ghost snapshot. Real auth may grant this again after connect.
	ent->client->sess.admin = ent == &g_entities[1];
	ent->client->pers.voted = 0;
	P_PublishEngineTeam(ent);

	// set up userinfo early
	ClientUserinfoChanged(ent, userinfo);

	// if there is already a body waiting for us (a loadgame), just
	// take it, otherwise spawn one from scratch
	if (ent->inuse == false) {
		// clear the respawning variables

		if (!ent->client->sess.initialised && !ent->client->sess.team) {
			//gi.Com_PrintFmt_("ClientConnect: {} q={}\n", ent->client->resp.netname, ent->client->sess.duel_queued);
			// force team join
			ent->client->sess.team = deathmatch->integer ? TEAM_SPECTATOR : TEAM_FREE;
			P_PublishEngineTeam(ent);
			//InitPlayerTeam(ent);
			ent->client->sess.pc = MM_DefaultClientConfig();

			InitClientResp(ent->client);
		}

		if (!game.autosaved || !ent->client->pers.weapon)
			InitClientPersistant(ent, ent->client);
	}

	// make sure we start with known default(s)
	ent->svflags = SVF_PLAYER;

	if (is_bot) {
		ent->svflags |= SVF_BOT;
		ent->client->sess.is_a_bot = true;

		// On level change the engine reconnects bots with a default/placeholder name
		// (typically "0" or empty). Restore the previously-stored name so it is not lost.
		char engine_name[MAX_INFO_VALUE] = { 0 };
		gi.Info_ValueForKey(userinfo, "name", engine_name, sizeof(engine_name));
		if ((!engine_name[0] || !strcmp(engine_name, "0")) && ent->client->resp.netname[0]) {
			gi.Info_SetValueForKey(userinfo, "name", ent->client->resp.netname);
			ClientUserinfoChanged(ent, userinfo);
		}

		if (bot_name_prefix->string[0] && *bot_name_prefix->string) {
			char oldname[MAX_INFO_VALUE];
			char newname[MAX_NETNAME];

			gi.Info_ValueForKey(userinfo, "name", oldname, sizeof(oldname));
			Q_strlcpy(newname, bot_name_prefix->string, sizeof(newname));
			Q_strlcat(newname, oldname, sizeof(newname));
			gi.Info_SetValueForKey(userinfo, "name", newname);
			ClientUserinfoChanged(ent, userinfo);
		}
	} else {
		// Clear bot flag for human clients - sess persists across map loads (TAG_GAME),
		// so if a bot previously occupied this slot, is_a_bot would still be true
		ent->client->sess.is_a_bot = false;
	}

	Q_strlcpy(ent->client->pers.social_id, social_id, sizeof(ent->client->pers.social_id));

	if (game.maxclients > 1) {
		char value[MAX_INFO_VALUE] = { 0 };
		// [Paril-KEX] fetch name because now netname is kinda unsuitable
		gi.Info_ValueForKey(userinfo, "name", value, sizeof(value));
		gi.LocClient_Print(nullptr, PRINT_HIGH, "$g_player_connected", value);
	}
	ent->client->pers.connected = true;

	ent->client->pers.ingame = true;

	// count current clients and rank for scoreboard
	CalculateRanks();

	// [MuffMode] Player config persistence lives in muffmode/mm_pconfig
	// Client slots are reusable, so a fresh connection starts from known profile
	// defaults. Retained map-change clients bypass ClientConnect; their existing
	// state is therefore available to the transactional reload in ClientBegin.
	ent->client->sess.pc = MM_DefaultClientConfig();
	ent->client->sess.profile_persistence_ready = false;
	ent->client->sess.skill_rating = MM_ClientProfileDefaultRating();
	ent->client->sess.skill_rating_change = 0;
	ent->client->sess.stats_matches_played = 0;
	MM_ClientProfileClearWeaponPreferences(ent->client);
	MM_ClientInitPConfig(ent);
	MM_PlayerStats_OnProfileLoaded(ent);

	// [Paril-KEX] force a state update
	ent->sv.init = false;

	gi.WriteByte(svc_stufftext);
	gi.WriteString("bind F3 readyup\n");
	//gi.WriteString("say hello\n");
	gi.unicast(ent, true);

	return true;
}

/*
===========
ClientDisconnect

Called when a player drops from the server.
Will not be called between levels.
============
*/
void ClientDisconnect(gentity_t *ent) {
	// The engine calls this with a null ent when a kick lands on an empty or
	// still-connecting slot (crash observed at ClientDisconnect+0x1E: access
	// violation reading offset 0x78, rcx = 0).
	if (!ent || !ent->client)
		return;

	// Raw references to this reusable client slot must end with this lifetime.
	MM_ClearDepartingClientReferences(ent);
	// A departed player must not still be deciding the next map.
	MM_MapPick_ClearClientVote(ent);
	// A grapple entity is owned by this exact client generation and must not
	// remain allocated after either a normal disconnect or snapshot capture.
	Weapon_Grapple_DoReset(ent->client);

	// Pause clocks before a reconnect ghost copies the session. A successful
	// reservation remains unsettled until it expires or the match ends.
	MM_PlayerStats_OnClientPause(ent);
	MM_MatchStats_ClientEnd(ent);

	const bool abort_spawn_pending = MM_Ghost_IsAbortSpawnPending(ent);
	MM_Ghost_CancelAbortSpawn(ent);

	G_ClearLagCompensationHistory(ent);

	const bool auto_ghosted =
		MM_GhostDisconnectMayCaptureSnapshot(abort_spawn_pending) &&
		notGT(GT_ARENA) && MM_Ghost_CaptureDisconnect(ent);
	if (!auto_ghosted)
		MM_PlayerStats_OnClientDisconnect(ent);
	MM_Arena_OnClientDisconnect(ent);
	if (!auto_ghosted) {
		TossClientItems(ent);
		// Item drops can add final CTF statistics after participation stopped.
		MM_MatchStats_ClientEnd(ent);
	}
	PlayerTrail_Destroy(ent);

	// make sure no trackers are still hurting us.
	if (ent->client->tracker_pain_time)
		RemoveAttackingPainDaemons(ent);

	if (gentity_t *sphere = G_ResolveOwnedSphere(ent->client)) {
		G_FreeEntity(sphere);
		G_ClearOwnedSphere(ent->client);
	}

	// send effect
	if (!auto_ghosted && !(ent->svflags & SVF_NOCLIENT)) {
		gi.WriteByte(svc_muzzleflash);
		gi.WriteEntity(ent);
		gi.WriteByte(MZ_LOGOUT);
		gi.multicast(ent->s.origin, MULTICAST_PVS, false);
	}

	// free any followers
	FreeClientFollowers(ent);

	// vacate captain if this player was one
	team_t dc_team = ent->client->sess.team;
	if ((dc_team == TEAM_RED || dc_team == TEAM_BLUE) && level.captain[dc_team] == ent)
		VacateCaptain(dc_team, ent);

	MM_RevertVote(ent->client);

	// If the disconnected client called the vote, cancel it
	if (level.vote_state.caller == ent->client) {
		if (level.vote_state.state == VoteState::ACTIVE || level.vote_state.state == VoteState::PASSED) {
			TransitionVoteState(VoteState::FAILED);
		}
		// EXECUTING state: let it finish (or fail if you prefer)
	}

	if (auto_ghosted) {
		MM_Ghost_MakeDisconnectPlaceholder(ent);
	} else {
		gi.unlinkentity(ent);
		ent->s.modelindex = 0;
		ent->solid = SOLID_NOT;
		// End this client-entity lifetime explicitly. Client slots are not passed
		// through G_FreeEntity, so delayed callbacks need the same generation
		// change before a later connection can reuse the slot.
		ent->spawn_count = MM_NextEntityGeneration(ent->spawn_count);
		ent->inuse = false;
		ent->sv.init = false;
		ent->classname = "disconnected";
		ent->client->pers.connected = false;
		ent->client->pers.spawned = false;
		ent->timestamp = level.time + 1_sec;
	}

	// update active scoreboards
	if (deathmatch->integer) {
		CalculateRanks();

		for (auto ec : active_clients())
			if (ec->client->showscores)
				ec->client->menutime = level.time;
	}

	if (ent->client->pers.connected)
		if (ent->client->resp.netname && ent->client->sess.initialised)
			gi.LocBroadcast_Print(PRINT_CENTER, "{} has left the server.",
				ent->client->resp.netname);
}

//==============================================================

static trace_t G_PM_Clip(const vec3_t &start, const vec3_t *mins, const vec3_t *maxs, const vec3_t &end, contents_t mask) {
	return gi.game_import_t::clip(world, start, mins, maxs, end, mask);
}

static trace_t G_PM_Trace(const vec3_t &start, const vec3_t *mins,
	const vec3_t *maxs, const vec3_t &end, const gentity_t *passent,
	contents_t mask) {
	trace_t trace = gi.game_import_t::trace(
		start, mins, maxs, end, passent, mask);
	if (notGT(GT_ARENA) || !passent || !passent->client)
		return trace;

	struct skipped_entity_t {
		gentity_t *ent;
		int32_t spawn_count;
	};
	constexpr size_t MAX_ARENA_TRACE_SKIPS = 64;
	std::array<skipped_entity_t, MAX_ARENA_TRACE_SKIPS> skipped {};
	size_t skipped_count = 0;

	// Room tags remain authoritative when dynamic bounds meet near shared map
	// geometry. Preserve same-room collision while allowing Pmove traces to
	// pass through dynamic entities that belong to another room.
	while (trace.ent && trace.ent != world &&
		!MM_Arena_CanInteract(passent, trace.ent) &&
		skipped_count < skipped.size()) {
		skipped[skipped_count++] = {
			trace.ent, trace.ent->spawn_count
		};
		gi.unlinkentity(trace.ent);
		trace = gi.game_import_t::trace(
			start, mins, maxs, end, passent, mask);
	}

	while (skipped_count > 0) {
		const skipped_entity_t entry = skipped[--skipped_count];
		if (entry.ent->inuse && entry.ent->spawn_count == entry.spawn_count)
			gi.linkentity(entry.ent);
	}
	return trace;
}

bool G_ShouldPlayersCollide(bool weaponry) {
	if (false) // Race mode removed
		return false;

	if (g_disable_player_collision->integer)
		return false; // only for debugging.

	// always collide on dm
	if (!InCoopStyle())
		return true;

	// weaponry collides if friendly fire is enabled
	if (weaponry && g_friendly_fire->integer)
		return true;

	// check collision cvar
	return g_coop_player_collision->integer;
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
=================
P_FallingDamage

Paril-KEX: this is moved here and now reacts directly
to ClientThink rather than being delayed.
=================
*/
static void P_FallingDamage(gentity_t *ent, const pmove_t &pm) {
	int	   damage;
	vec3_t dir;

	// dead stuff can't crater
	if (ent->health <= 0 || ent->deadflag)
		return;

	if (ent->s.modelindex != MODELINDEX_PLAYER)
		return; // not in the player model

	if (ent->movetype == MOVETYPE_NOCLIP || ent->movetype == MOVETYPE_FREECAM)
		return;

	// never take falling damage if completely underwater
	if (pm.waterlevel == WATER_UNDER)
		return;

	//  never take damage if just release grapple or on grapple
	if (ent->client->grapple_release_time >= level.time ||
		(ent->client->grapple_ent &&
			ent->client->grapple_state > GRAPPLE_STATE_FLY))
		return;

	float delta = pm.impact_delta;

	delta = delta * delta * 0.0001f;

	if (pm.waterlevel == WATER_WAIST)
		delta *= 0.25f;
	if (pm.waterlevel == WATER_FEET)
		delta *= 0.5f;

	if (delta < 1)
		return;

	// restart footstep timer
	ent->client->bobtime = 0;

	if (ent->client->landmark_free_fall) {
		delta = min(30.f, delta);
		ent->client->landmark_free_fall = false;
		ent->client->landmark_noise_time = level.time + 100_ms;
	}

	if (delta < 15) {
		if (!(pm.s.pm_flags & PMF_ON_LADDER))
			ent->s.event = EV_FOOTSTEP;
		return;
	}

	ent->client->fall_value = delta * 0.5f;
	if (ent->client->fall_value > 40)
		ent->client->fall_value = 40;
	ent->client->fall_time = level.time + FALL_TIME();

	// [MuffMode] Ruleset-specific fall thresholds live in muffmode/mm_ruleset
	int med_min = MM_RulesetFallDamageMedMin();
	int far_min = MM_RulesetFallDamageFarMin();

	if (delta > med_min) {
		if (delta >= far_min) {
			ent->s.event = EV_FALL_FAR;
			// Send positioned sound for spectators only
			if (HasSpectators(ent)) {
				const char *fall_sound = brandom() ? "*fall1.wav" : "*fall2.wav";
				gi.positioned_sound(ent->s.origin, ent, CHAN_VOICE, gi.soundindex(fall_sound), 1, ATTN_NORM, 0);
			}
		} else {
			ent->s.event = EV_FALL_MEDIUM;
			// Send positioned sound for spectators only
			if (HasSpectators(ent)) {
				gi.positioned_sound(ent->s.origin, ent, CHAN_VOICE, gi.soundindex("player/land1.wav"), 1, ATTN_NORM, 0);
			}
		}
		const bool suppresses_falling = GT(GT_ARENA)
			? !MM_Arena_FallingDamageEnabled(ent)
			: (g_dm_no_fall_damage->integer || GTF(GTF_ARENA));
		if (!deathmatch->integer || !suppresses_falling) {
			ent->pain_debounce_time = level.time + FRAME_TIME_S; // no normal pain sound
			damage = MM_RulesetFallDamage(ent->s.event == EV_FALL_FAR, delta);
			dir = { 0, 0, 1 };

			T_Damage(ent, world, world, dir, ent->s.origin, vec3_origin, damage, 0, DAMAGE_NONE, MOD_FALLING);
		}
	} else {
		ent->s.event = EV_FALL_SHORT;
		// Send positioned sound for spectators only
		if (HasSpectators(ent)) {
			gi.positioned_sound(ent->s.origin, ent, CHAN_VOICE, gi.soundindex("player/land1.wav"), 1, ATTN_NORM, 0);
		}
	}

	// Paril: falling damage noises alert monsters
	if (ent->health)
		PlayerNoise(ent, pm.s.origin, PNOISE_SELF);
}

static bool HandleMenuMovement(gentity_t *ent, usercmd_t *ucmd) {
	if (!ent->client->menu)
		return false;

	// [Paril-KEX] handle menu movement
	int32_t menu_sign = ucmd->forwardmove > 0 ? 1 : ucmd->forwardmove < 0 ? -1 : 0;

	if (ent->client->menu_sign != menu_sign) {
		ent->client->menu_sign = menu_sign;

		if (menu_sign > 0) {
			P_Menu_Prev(ent);
			return true;
		} else if (menu_sign < 0) {
			P_Menu_Next(ent);
			return true;
		}
	}

	const button_t selection_buttons =
		ent->client->latched_buttons & (BUTTON_ATTACK | BUTTON_JUMP);
	if (selection_buttons) {
		// The menu owns these presses. Consume them before a selection callback
		// can close the menu, and keep suppressing them until the player releases
		// the buttons so they cannot also fire, jump, or change follow target.
		ent->client->menu_captured_buttons |= selection_buttons;
		ent->client->latched_buttons &= ~(BUTTON_ATTACK | BUTTON_JUMP);
		ent->client->buttons &= ~selection_buttons;
		ent->client->cmd.buttons &= ~selection_buttons;
		P_Menu_Select(ent);
		return true;
	}

	return false;
}

/*
=================
ClientInactivityTimer

Returns false if the client is dropped
=================
*/
static bool ClientInactivityTimer(gentity_t *ent) {
	gtime_t cv = gtime_t::from_sec(g_inactivity->integer);

	if (!ent->client)
		return true;
	
	if (cv && cv < 15_sec) cv = 15_sec;
	if (GT(GT_ARENA) && MM_Arena_IsFighter(ent->client) &&
		MM_Arena_IsPaused(MM_Arena_Id(ent))) {
		// A room-local timeout is deliberate inactivity. Give the fighter a
		// fresh window after time-in instead of warning or removing them.
		ent->client->sess.inactivity_time =
			level.time + (cv ? cv : 1_min);
		ent->client->sess.inactivity_warning = false;
		return true;
	}
	if (!ent->client->sess.inactivity_time) {
		ent->client->sess.inactivity_time = level.time + cv;
		ent->client->sess.inactivity_warning = false;
		return true;
	}

	// Any input activity this frame: held buttons, movement, freshly pressed buttons,
	// or a meaningful view-angle change (>1 degree, to ignore tiny jitter).
	auto client_has_input = [](gentity_t *e) -> bool {
		if (e->client->buttons)
			return true;
		if (e->client->cmd.forwardmove != 0 || e->client->cmd.sidemove != 0)
			return true;
		if (e->client->latched_buttons)
			return true;
		const float angle_threshold = 1.0f;
		const gvec3_t &current_angles = e->client->cmd.angles;
		const vec3_t &prev_angles = e->client->resp.cmd_angles;
		auto angle_diff = [](float a, float b) -> float {
			float diff = a - b;
			while (diff > 180.0f)
				diff -= 360.0f;
			while (diff < -180.0f)
				diff += 360.0f;
			return fabsf(diff);
		};
		return angle_diff(current_angles[0], prev_angles[0]) > angle_threshold ||
			angle_diff(current_angles[1], prev_angles[1]) > angle_threshold;
	};

	// Red Rover: an inactivity drop sends a player to spectator with sess.inactive set.
	// The moment they're active again, fold them back onto a team automatically so they
	// aren't stranded off every team (grey tag, missing from the scoreboard) for the rest
	// of the match. A deliberate spectator has sess.inactive == false and is left alone.
	if (GT(GT_RR) && level.match_state == match_state_t::MATCH_IN_PROGRESS && deathmatch->integer &&
		!ent->client->sess.is_a_bot && ent->client->sess.team == TEAM_SPECTATOR &&
		ent->client->sess.inactive && client_has_input(ent)) {
		SetTeam(ent, PickTeam(-1), false, false, false);
		return true;
	}

	if (!deathmatch->integer || !cv || !ClientIsPlaying(ent->client) || ent->client->eliminated || ent->client->sess.is_a_bot || ent->s.number == 0) {
		// give everyone some time, so if the operator sets g_inactivity during
		// gameplay, everyone isn't kicked
		ent->client->sess.inactivity_time = level.time + 1_min;
		ent->client->sess.inactivity_warning = false;
	} else {
		// Check for ANY input activity, not just newly pressed buttons. This prevents
		// false positives when players are actively playing but only holding movement
		// keys or moving the mouse.
		bool has_input = client_has_input(ent);

		if (has_input) {
			ent->client->sess.inactivity_time = level.time + cv;
			ent->client->sess.inactivity_warning = false;
		} else {
			if (level.time > ent->client->sess.inactivity_time) {
				gi.LocClient_Print(ent, PRINT_CENTER, "You have been removed from the match\ndue to inactivity.\n");
				SetTeam(ent, TEAM_SPECTATOR, true, true, false);
				return false;
			}
			if (level.time > ent->client->sess.inactivity_time - gtime_t::from_sec(10) && !ent->client->sess.inactivity_warning) {
				ent->client->sess.inactivity_warning = true;
				gi.LocClient_Print(ent, PRINT_CENTER, "Ten seconds until inactivity trigger!\n");	//TODO: "$g_ten_sec_until_drop");
				gi.local_sound(ent, CHAN_AUTO, gi.soundindex("world/fish.wav"), 1, ATTN_NONE, 0);
			}
		}
	}

	return true;
}

/*
==================
ClientTimerActions

Actions that happen once a second
==================
*/
static void ClientTimerActions(gentity_t *ent) {
	if (ent->client->time_residual > level.time)
		return;

	MS_Adjust(ent->client, MSTAT_HEALTH_TRACKER, ent->health);
	MS_Adjust(ent->client, MSTAT_HEALTH_TICKS, 1);
	if (ent->health > MS_Value(ent->client, MSTAT_HEALTH_PEAK))
		MS_Set(ent->client, MSTAT_HEALTH_PEAK, ent->health);

	MS_Adjust(ent->client, MSTAT_PING_TRACKER, ent->client->ping);
	MS_Adjust(ent->client, MSTAT_PING_TICKS, 1);
	if (ent->client->ping > MS_Value(ent->client, MSTAT_PING_PEAK))
		MS_Set(ent->client, MSTAT_HEALTH_PEAK, ent->client->ping);

	MM_RulesetQ3AHealthArmorDecay(ent);

	MM_ClampEntityHealthArmor(ent);

	if (GT(GT_HORDE) && ent->client->eliminated && ent->client->sess.team != TEAM_SPECTATOR &&
		level.round_state == round_state_t::ROUND_IN_PROGRESS)
		MM_Horde_NotifyEliminatedSpectator(ent);

	ent->client->time_residual = level.time + 1_sec;
}

/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame.
==============
*/
void ClientThink(gentity_t *ent, usercmd_t *ucmd) {
	gclient_t *client;
	gentity_t *other;
	uint32_t   i;
	pmove_t	   pm;

	// The engine can dispatch a think for a client slot whose entity is
	// already torn down (kick/disconnect race), passing a null ent.
	if (!ent || !ent->client)
		return;

	level.current_entity = ent;
	client = ent->client;

	//no movement during map or match intermission
	if (level.timeout_in_place > 0_ms) {
		client->resp.cmd_angles[0] = ucmd->angles[0];
		client->resp.cmd_angles[1] = ucmd->angles[1];
		client->resp.cmd_angles[2] = ucmd->angles[2];
		client->ps.pmove.pm_type = PM_FREEZE;
		return;
	}

	if (MM_Ghost_ClientThink(ent, ucmd))
		return;

	// [Paril-KEX] pass buttons through even if we are in intermission or
	// following.
	client->menu_captured_buttons &= ucmd->buttons;
	client->oldbuttons = client->buttons;
	client->buttons = ucmd->buttons & ~client->menu_captured_buttons;
	client->latched_buttons |= client->buttons & ~client->oldbuttons;
	client->cmd = *ucmd;
	client->cmd.buttons = client->buttons;

	if (!client->initial_menu_shown && client->initial_menu_delay && level.time > client->initial_menu_delay) {
		// [MuffMode] Never open the join menu on a bot. Arena bots never route through SetTeam
		// (the only sess.initialised setter), so without this guard the menu opens on them and is
		// never closed, which suppresses Think_Weapon forever and unicasts a layout every 3s.
		const bool is_a_bot = (ent->svflags & SVF_BOT) || client->sess.is_a_bot;
		if (!is_a_bot && !ClientIsPlaying(client) && (!client->sess.initialised || client->sess.inactive)) {
			if (ent->client->sess.admin && g_owner_push_scores->integer)
				Cmd_Score_f(ent);
			else
				G_Menu_Join_Open(ent);
			//if (!client->initial_menu_shown)
			//	gi.LocClient_Print(ent, PRINT_CHAT, "Welcome to {} v{}.\n", GAMEMOD_TITLE, GAMEMOD_VERSION);
			client->initial_menu_delay = 0_sec;
			client->initial_menu_shown = true;
		}
	}

	// check for queued follow targets
	if (!ClientIsPlaying(client) || client->eliminated) {
		if (client->follow_queued_target && level.time > client->follow_queued_time + 500_ms) {
			// eliminated players in team elimination modes may only follow their own team
			if (FollowTargetAllowed(ent, client->follow_queued_target)) {
				SetFollowTarget(ent, client->follow_queued_target);
				UpdateFollowCamera(ent);
			}
			client->follow_queued_target = nullptr;
			client->follow_queued_time = 0_sec;
		}
	}
	
	// check for inactivity timer
	if (!ClientInactivityTimer(ent))
		return;

	if (notGT(GT_ARENA) && g_quadhog->integer)
		if (ent->client->pu_time_quad > 0_sec && level.time >= ent->client->pu_time_quad)
			QuadHog_SetupSpawn(0_ms);

	if (ent->client->pers.health_bonus > 0) {
		if (ent->client->pers.health <= ent->client->pers.max_health) {
			ent->client->pers.health_bonus = 0;
		} else {
			if (level.time > ent->client->pers.health_bonus_timer) {
				ent->client->pers.health_bonus--;
				ent->health--;
				ent->client->pers.health_bonus_timer = level.time + 1_sec;
			}
		}
	}
	
	if (ent->client->sess.team_join_time) {
		gtime_t delay = 5_sec;
		if (ent->client->resp.motd_mod_count != game.motd_mod_count) {
			if (level.time >= ent->client->sess.team_join_time + delay) {
				if (g_showmotd->integer && game.motd.size()) {
					MM_ShowAutomaticMOTD(ent);
					delay += 5_sec;
					ent->client->resp.motd_mod_count = game.motd_mod_count;
				}
			}
		}
		if (!ent->client->resp.showed_help && g_showhelp->integer &&
			notGT(GT_ARENA)) {
			if (level.time >= ent->client->sess.team_join_time + delay) {
				if (g_quadhog->integer) {
					gi.LocClient_Print(ent, PRINT_CENTER, "QUAD HOG\nFind the Quad Damage to become the Quad Hog!\nScore by fragging the Quad Hog or fragging while Quad Hog.");
				} else if (g_vampiric_damage->integer) {
					gi.LocClient_Print(ent, PRINT_CENTER, "VAMPIRIC DAMAGE\nSurvive by inflicting damage on your foes,\ntheir pain makes you stronger!");
				} else if (g_frenzy->integer) {
					gi.LocClient_Print(ent, PRINT_CENTER, "WEAPONS FRENZY\nWeapons fire faster, rockets move faster, ammo regenerates.");
				} else if (g_nadefest->integer) {
					gi.LocClient_Print(ent, PRINT_CENTER, "NADE FEST\nOnly grenades, nothing else!");
				} else if (g_instagib->integer) {
					gi.LocClient_Print(ent, PRINT_CENTER, "INSTAGIB\nA rail-y good time!");
				}

				ent->client->resp.showed_help = true;
				//ent->client->sess.team_join_time = 0_sec;
			}
		}
	}

	if ((ucmd->buttons & BUTTON_CROUCH) && pm_config.n64_physics) {
		if (client->pers.n64_crouch_warn_times < 12 &&
			client->pers.n64_crouch_warning < level.time &&
			(++client->pers.n64_crouch_warn_times % 3) == 0) {
			client->pers.n64_crouch_warning = level.time + 10_sec;
			gi.LocClient_Print(ent, PRINT_CENTER, "$g_n64_crouching");
		}
	}

	if (level.intermission_time || ent->client->awaiting_respawn) {
		client->ps.pmove.pm_type = PM_FREEZE;

		bool n64_sp = false;

		if (level.intermission_time) {
			n64_sp = !deathmatch->integer && level.is_n64;

			// [MuffMode] While the next-map pick is up it owns the intermission:
			// its menu takes the buttons to move the cursor and cast the pick, and
			// nobody -- menu open or not -- can press past it to the exit.
			if (MM_MapPick_Active()) {
				if (MM_MapPick_MenuOpen(ent))
					HandleMenuMovement(ent, ucmd);
			}
			// [MuffMode] The awards reel sits between the scoreboard and the
			// pick, and takes the skip on its own clock rather than the
			// scoreboard's: its hold has to expire before a key means anything,
			// or the press that dismissed the scoreboard would carry through it.
			else if (MM_Awards_Active()) {
				if (MM_Awards_AcceptsSkip() && (ucmd->buttons & BUTTON_ANY))
					level.intermission_exit = true;
			}
			// can exit intermission after five seconds
			// Paril: except in N64. the camera handles it.
			// Paril again: except on unit exits, we can leave immediately after camera finishes
			else if (level.changemap && (!n64_sp || level.level_intermission_set) && level.time > level.intermission_time + 5_sec && (ucmd->buttons & BUTTON_ANY))
				level.intermission_exit = true;
		}

		if (!n64_sp)
			client->ps.pmove.viewheight = ent->viewheight = 22;
		else
			client->ps.pmove.viewheight = ent->viewheight = 0;
		ent->movetype = MOVETYPE_FREECAM;
		return;
	}

	if (MM_FreezeTag_ClientThink(ent, ucmd))
		return;

	// [MuffMode] The rerelease only routed usercmd menu controls for spectators,
	// but MuffMode exposes the join/vote/config menus to live players too.
	const bool menu_captures_usercmd = MM_MenuCapturesUsercmd(
		client->menu != nullptr,
		ClientIsPlaying(client) ? mm_menu_client_state_t::Playing : mm_menu_client_state_t::Spectator);
	const bool menu_captures_attack = menu_captures_usercmd &&
		(client->latched_buttons & BUTTON_ATTACK);
	if (menu_captures_usercmd) {
		HandleMenuMovement(ent, ucmd);
		if (menu_captures_attack)
			client->weapon_thunk = true;
	}

	if (ent->client->follow_target) {
		client->resp.cmd_angles = ucmd->angles;
		ent->movetype = MOVETYPE_FREECAM;
	} else {

		// set up for pmove
		memset(&pm, 0, sizeof(pm));

		if (ent->movetype == MOVETYPE_FREECAM) {
			if (menu_captures_usercmd) {
				client->ps.pmove.pm_type = PM_FREEZE;
			} else if (ent->client->awaiting_respawn)
				client->ps.pmove.pm_type = PM_FREEZE;
			else if (!MM_Arena_FreecamAllowed(ent))
				client->ps.pmove.pm_type = PM_FREEZE;
			else if (!ClientIsPlaying(ent->client) || client->eliminated)
				client->ps.pmove.pm_type = PM_SPECTATOR;
			else
				client->ps.pmove.pm_type = PM_NOCLIP;
		} else if (ent->movetype == MOVETYPE_NOCLIP) {
			client->ps.pmove.pm_type = PM_NOCLIP;
		} else if (ent->s.modelindex != MODELINDEX_PLAYER)
			client->ps.pmove.pm_type = PM_GIB;
		else if (ent->deadflag)
			client->ps.pmove.pm_type = PM_DEAD;
		else if (GT(GT_ARENA) && MM_Arena_IsFighter(client) &&
			!MM_Arena_IsEliminated(client) && !MM_Arena_CombatEnabled(ent))
			client->ps.pmove.pm_type = PM_FREEZE;
		else if (ent->client->grapple_state >= GRAPPLE_STATE_PULL)
			client->ps.pmove.pm_type = PM_GRAPPLE;
		else
			client->ps.pmove.pm_type = PM_NORMAL;

		// [Paril-KEX]
		if (!G_ShouldPlayersCollide(false) ||
			((InCoopStyle() || GT(GT_ARENA)) && !(ent->clipmask & CONTENTS_PLAYER)) // if player collision is on and we're temporarily ghostly...
			)
			client->ps.pmove.pm_flags |= PMF_IGNORE_PLAYER_COLLISION;
		else
			client->ps.pmove.pm_flags &= ~PMF_IGNORE_PLAYER_COLLISION;

		// haste support — q2re's DualFire is fire-rate only; movement boost is ruleset-gated
		client->ps.pmove.haste = (client->pu_time_haste > level.time) && MM_RulesetHasteBoostsMovement();

		// trigger_gravity support
		client->ps.pmove.gravity = static_cast<int16_t>(MM_Horde_ClampFiniteInt(
			static_cast<double>(level.gravity) * static_cast<double>(ent->gravity) *
				MM_Horde_PlayerGravityScale(),
			level.gravity, std::numeric_limits<int16_t>::min(),
			std::numeric_limits<int16_t>::max()));
		pm.s = client->ps.pmove;

		pm.s.origin = ent->s.origin;
		pm.s.velocity = ent->velocity;

		if (memcmp(&client->old_pmove, &pm.s, sizeof(pm.s)))
			pm.snapinitial = true;

		pm.cmd = *ucmd;
		pm.cmd.buttons = client->buttons;
		if (menu_captures_usercmd) {
			// Forward/back navigates the menu, while attack/jump selects. Do not
			// replay those same inputs into live-player movement on this frame.
			pm.cmd.forwardmove = 0;
			pm.cmd.buttons &= ~(BUTTON_ATTACK | BUTTON_JUMP);
		}
		pm.player = ent;
		pm.trace = G_PM_Trace;
		pm.clip = G_PM_Clip;
		pm.pointcontents = gi.pointcontents;
		pm.viewoffset = ent->client->ps.viewoffset;

		// perform a pmove
		Pmove(&pm);

		if (pm.groundentity && ent->groundentity) {
			float stepsize = fabs(ent->s.origin[2] - pm.s.origin[2]);

			if (stepsize > 4.f && stepsize < (ent->s.origin[2] < 0 ? STEPSIZE_BELOW : STEPSIZE)) {
				ent->s.renderfx |= RF_STAIR_STEP;
				ent->client->step_frame = gi.ServerFrame() + 1;
			}
		}

		P_FallingDamage(ent, pm);

		if (ent->client->landmark_free_fall && pm.groundentity) {
			ent->client->landmark_free_fall = false;
			ent->client->landmark_noise_time = level.time + 100_ms;
		}

		// [Paril-KEX] save old position for G_TouchProjectiles
		vec3_t old_origin = ent->s.origin;

		ent->s.origin = pm.s.origin;
		ent->velocity = pm.s.velocity;

		// [Paril-KEX] if we stepped onto/off of a ladder, reset the
		// last ladder pos
		if ((pm.s.pm_flags & PMF_ON_LADDER) != (client->ps.pmove.pm_flags & PMF_ON_LADDER)) {
			client->last_ladder_pos = ent->s.origin;

			if (pm.s.pm_flags & PMF_ON_LADDER) {
				if (!deathmatch->integer &&
					client->last_ladder_sound < level.time) {
					ent->s.event = EV_LADDER_STEP;
					// Send positioned sound for spectators only
					if (HasSpectators(ent)) {
						gi.positioned_sound(ent->s.origin, ent, CHAN_FOOTSTEP, gi.soundindex("player/stepl.wav"), 1, ATTN_NORM, 0);
					}
					client->last_ladder_sound = level.time + LADDER_SOUND_TIME;
				}
			}
		}

		// save results of pmove
		client->ps.pmove = pm.s;
		client->old_pmove = pm.s;

		ent->mins = pm.mins;
		ent->maxs = pm.maxs;

		if (!ent->client->menu)
			client->resp.cmd_angles = ucmd->angles;

		if (pm.jump_sound && !(pm.s.pm_flags & PMF_ON_LADDER)) {
			gi.sound(ent, CHAN_VOICE, gi.soundindex("*jump1.wav"), 1, ATTN_NORM, 0);
			// Paril: removed to make ambushes more effective and to
			// not have monsters around corners come to jumps
			// PlayerNoise(ent, ent->s.origin, PNOISE_SELF);
		}

		// sam raimi cam support
		if (ent->flags & FL_SAM_RAIMI)
			ent->viewheight = 8;
		else
			ent->viewheight = (int)pm.s.viewheight;

		ent->waterlevel = pm.waterlevel;
		ent->watertype = pm.watertype;
		ent->groundentity = pm.groundentity;
		if (pm.groundentity)
			ent->groundentity_linkcount = pm.groundentity->linkcount;

		if (ent->deadflag) {
			client->ps.viewangles[ROLL] = 40;
			client->ps.viewangles[PITCH] = -15;
			client->ps.viewangles[YAW] = client->killer_yaw;
		} else if (!ent->client->menu) {
			client->v_angle = pm.viewangles;
			client->ps.viewangles = pm.viewangles;
			AngleVectors(client->v_angle, client->v_forward, nullptr, nullptr);
		}

		if (client->grapple_ent &&
			(notGT(GT_ARENA) || MM_Arena_CombatEnabled(ent)))
			Weapon_Grapple_Pull(client->grapple_ent);

		gi.linkentity(ent);

		ent->gravity = 1.0;

		if (ent->movetype != MOVETYPE_NOCLIP) {
			const int32_t touch_generation = ent->spawn_count;
			G_TouchTriggers(ent);
			if (!ent->inuse || ent->spawn_count != touch_generation)
				return;
			if (ent->movetype != MOVETYPE_FREECAM)
				if (!G_TouchProjectiles(ent, old_origin))
					return;
		}

		// touch other objects
		for (i = 0; i < pm.touch.num; i++) {
			trace_t &tr = pm.touch.traces[i];
			other = tr.ent;

			if (GT(GT_ARENA) &&
				(!MM_Arena_CanInteract(ent, other) ||
				 !MM_Arena_CanUseEntity(ent, other)))
				continue;
			if (other->touch)
				other->touch(other, ent, tr, true);
		}
	}

	// fire weapon from final position if needed
	if ((client->latched_buttons & BUTTON_USE) && client->follow_target && (!ClientIsPlaying(client) || (client->eliminated && !client->sess.is_a_bot))) {
		if (!client->menu) {
			client->latched_buttons &= ~BUTTON_USE;
			ToggleFollowViewMode(ent);
		}
	}

	if ((client->latched_buttons & BUTTON_CROUCH) && client->follow_target && (!ClientIsPlaying(client) || (client->eliminated && !client->sess.is_a_bot))) {
		if (!client->menu) {
			client->latched_buttons &= ~BUTTON_CROUCH;
			FollowPrev(ent);
		}
	}

	if (client->latched_buttons & BUTTON_ATTACK) {
		if (!ClientIsPlaying(client) || (client->eliminated && !client->sess.is_a_bot)) {
			if (!client->menu) {
				client->latched_buttons = BUTTON_NONE;

				if (client->follow_target) {
					FreeFollower(ent);
				} else
					GetFollowTarget(ent);
			}
		} else if (!ent->client->weapon_thunk) {
			// we can only do this during a ready state and
			// if enough time has passed from last fire
			if (ent->client->weaponstate == WEAPON_READY &&
				(GT(GT_ARENA) ? MM_Arena_CombatEnabled(ent) : !IsCombatDisabled())) {
				ent->client->weapon_fire_buffered = true;

				if (ent->client->weapon_fire_finished <= level.time) {
					ent->client->weapon_thunk = true;
					Think_Weapon(ent);
				}
			}
		}
	}

	if (!ClientIsPlaying(client) || (client->eliminated && !client->sess.is_a_bot)) {
		if (!menu_captures_usercmd) {
			if (client->buttons & BUTTON_JUMP) {
				if (!(client->ps.pmove.pm_flags & PMF_JUMP_HELD)) {
					client->ps.pmove.pm_flags |= PMF_JUMP_HELD;
					if (client->follow_target)
						FollowNext(ent);
					else
						GetFollowTarget(ent);
				}
			} else
				client->ps.pmove.pm_flags &= ~PMF_JUMP_HELD;
		}
	}

	// update follow camera if being followed
	for (auto ec : active_clients())
		if (ec->client->follow_target == ent)
			UpdateFollowCamera(ec);

	// perform once-a-second actions. A room-local timeout freezes health/armor
	// decay and match-stat sampling along with the rest of that arena.
	if (notGT(GT_ARENA) || !MM_Arena_IsFighter(ent->client) ||
		!MM_Arena_IsPaused(MM_Arena_Id(ent)))
		ClientTimerActions(ent);
}

// active monsters
struct active_monsters_filter_t {
	inline bool operator()(gentity_t *ent) const {
		return (ent->inuse && (ent->svflags & SVF_MONSTER) && ent->health > 0);
	}
};

inline entity_iterable_t<active_monsters_filter_t> active_monsters() {
	return entity_iterable_t<active_monsters_filter_t> { game.maxclients + (uint32_t)BODY_QUEUE_SIZE + 1U };
}

static inline bool G_MonstersSearchingFor(gentity_t *player) {
	for (auto ent : active_monsters()) {
		// check for *any* player target
		if (player == nullptr && ent->enemy && !ent->enemy->client)
			continue;
		// they're not targeting us, so who cares
		else if (player != nullptr && ent->enemy != player)
			continue;

		// they lost sight of us
		if ((ent->monsterinfo.aiflags & AI_LOST_SIGHT) && level.time > ent->monsterinfo.trail_time + 5_sec)
			continue;

		// no sir
		return true;
	}

	// yes sir
	return false;
}

// [Paril-KEX] from the given player, find a good spot to
// spawn a player
static inline bool G_FindRespawnSpot(gentity_t *player, vec3_t &spot) {
	// sanity check; make sure there's enough room for ourselves.
	// (crouching in a small area, etc)
	trace_t tr = gi.trace(player->s.origin, PLAYER_MINS, PLAYER_MAXS, player->s.origin, player, MASK_PLAYERSOLID);

	if (tr.startsolid || tr.allsolid)
		return false;

	// throw five boxes a short-ish distance from the player and see if they land in a good, visible spot
	constexpr float yaw_spread[] = { 0, 90, 45, -45, -90 };
	constexpr float back_distance = 128.f;
	constexpr float up_distance = 128.f;
	constexpr float player_viewheight = 22.f;

	// we don't want to spawn inside of these
	contents_t mask = MASK_PLAYERSOLID | CONTENTS_LAVA | CONTENTS_SLIME;

	for (auto &yaw : yaw_spread) {
		vec3_t angles = { 0, (player->s.angles[YAW] + 180) + yaw, 0 };

		// throw the box three times:
		// one up & back
		// one back
		// one up, then back
		// pick the one that went the farthest
		vec3_t start = player->s.origin;
		vec3_t end = start + vec3_t{ 0, 0, up_distance };

		tr = gi.trace(start, PLAYER_MINS, PLAYER_MAXS, end, player, mask);

		// stuck
		if (tr.startsolid || tr.allsolid || (tr.contents & (CONTENTS_LAVA | CONTENTS_SLIME)))
			continue;

		vec3_t fwd;
		AngleVectors(angles, fwd, nullptr, nullptr);

		start = tr.endpos;
		end = start + fwd * back_distance;

		tr = gi.trace(start, PLAYER_MINS, PLAYER_MAXS, end, player, mask);

		// stuck
		if (tr.startsolid || tr.allsolid || (tr.contents & (CONTENTS_LAVA | CONTENTS_SLIME)))
			continue;

		// plop us down now
		start = tr.endpos;
		end = tr.endpos - vec3_t{ 0, 0, up_distance * 4 };

		tr = gi.trace(start, PLAYER_MINS, PLAYER_MAXS, end, player, mask);

		// stuck, or floating, or touching some other entity
		if (tr.startsolid || tr.allsolid || (tr.contents & (CONTENTS_LAVA | CONTENTS_SLIME)) || tr.fraction == 1.0f || tr.ent != world)
			continue;

		// don't spawn us *inside* liquids
		if (gi.pointcontents(tr.endpos + vec3_t{ 0, 0, player_viewheight }) & MASK_WATER)
			continue;

		// don't spawn us on steep slopes
		if (tr.plane.normal.z < 0.7f)
			continue;

		spot = tr.endpos;

		float z_diff = fabsf(player->s.origin[2] - tr.endpos[2]);

		// 5 steps is way too many steps
		if (z_diff > (player->s.origin[2] < 0 ? STEPSIZE_BELOW : STEPSIZE) * 4.f)
			continue;

		// if we went up or down 1 step, make sure we can still see their origin and their head
		if (z_diff > (player->s.origin[2] < 0 ? STEPSIZE_BELOW : STEPSIZE)) {
			tr = gi.traceline(player->s.origin, tr.endpos, player, mask);

			if (tr.fraction != 1.0f)
				continue;

			tr = gi.traceline(player->s.origin + vec3_t{ 0, 0, player_viewheight }, tr.endpos + vec3_t{ 0, 0, player_viewheight }, player, mask);

			if (tr.fraction != 1.0f)
				continue;
		}

		// good spot!
		return true;
	}

	return false;
}

// [Paril-KEX] check each player to find a good
// respawn target & position
inline std::tuple<gentity_t *, vec3_t> G_FindSquadRespawnTarget() {
	bool monsters_searching_for_anybody = G_MonstersSearchingFor(nullptr);

	for (auto player : active_clients()) {
		// no dead players
		if (player->deadflag)
			continue;

		// check combat state; we can't have taken damage recently
		if (player->client->last_damage_time >= level.time) {
			player->client->coop_respawn_state = COOP_RESPAWN_IN_COMBAT;
			continue;
		}

		// check if any monsters are currently targeting us
		// or searching for us
		if (G_MonstersSearchingFor(player)) {
			player->client->coop_respawn_state = COOP_RESPAWN_IN_COMBAT;
			continue;
		}

		// check firing state; if any enemies are mad at any players,
		// don't respawn until everybody has cooled down
		if (monsters_searching_for_anybody && player->client->last_firing_time >= level.time) {
			player->client->coop_respawn_state = COOP_RESPAWN_IN_COMBAT;
			continue;
		}

		// check positioning; we must be on world ground
		if (player->groundentity != world) {
			player->client->coop_respawn_state = COOP_RESPAWN_BAD_AREA;
			continue;
		}

		// can't be in liquid
		if (player->waterlevel >= WATER_UNDER) {
			player->client->coop_respawn_state = COOP_RESPAWN_BAD_AREA;
			continue;
		}

		// good player; pick a spot
		vec3_t spot;

		if (!G_FindRespawnSpot(player, spot)) {
			player->client->coop_respawn_state = COOP_RESPAWN_BLOCKED;
			continue;
		}

		// good player most likely
		return { player, spot };
	}

	// no good player
	return { nullptr, {} };
}

enum class respawn_state_t {
	none,     // invalid state
	spectate, // move to spectator
	squad,    // move to good squad point
	start     // move to start of map
};

// [Paril-KEX] return false to fall back to click-to-respawn behavior.
// note that this is only called if they are allowed to respawn (not
// restarting the level due to all being dead)
static bool G_CoopRespawn(gentity_t *ent) {
	// don't do this in non-coop
	if (!InCoopStyle())
		return false;
	if (GT(GT_HORDE))
		return false;
	// if we don't have squad or lives, it doesn't matter
	if (!g_coop_squad_respawn->integer && !g_coop_enable_lives->integer)
		return false;

	respawn_state_t state = respawn_state_t::none;

	// first pass: if we have no lives left, just move to spectator
	if (g_coop_enable_lives->integer) {
		if (ent->client->pers.lives == 0) {
			state = respawn_state_t::spectate;
			ent->client->coop_respawn_state = COOP_RESPAWN_NO_LIVES;
		}
	}

	// second pass: check for where to spawn
	if (state == respawn_state_t::none) {
		// if squad respawn, don't respawn until we can find a good player to spawn on.
		if (coop->integer && g_coop_squad_respawn->integer) {
			bool allDead = true;

			for (auto player : active_clients()) {
				if (player->health > 0) {
					allDead = false;
					break;
				}
			}

			// all dead, so if we ever get here we have lives enabled;
			// we should just respawn at the start of the level
			if (allDead)
				state = respawn_state_t::start;
			else {
				auto [good_player, good_spot] = G_FindSquadRespawnTarget();

				if (good_player) {
					state = respawn_state_t::squad;

					squad_respawn_position = good_spot;
					squad_respawn_angles = good_player->s.angles;
					squad_respawn_angles[2] = 0;

					use_squad_respawn = true;
				} else {
					state = respawn_state_t::spectate;
				}
			}
		} else
			state = respawn_state_t::start;
	}

	if (state == respawn_state_t::squad || state == respawn_state_t::start) {
		// give us our max health back since it will reset
		// to pers.health; in instanced items we'd lose the items
		// we touched so we always want to respawn with our max.
		if (P_UseCoopInstancedItems())
			ent->client->pers.health = ent->client->pers.max_health = ent->max_health;

		ClientRespawn(ent);

		ent->client->latched_buttons = BUTTON_NONE;
		use_squad_respawn = false;
	} else if (state == respawn_state_t::spectate) {
		if (!ent->client->coop_respawn_state)
			ent->client->coop_respawn_state = COOP_RESPAWN_WAITING;

		if (ClientIsPlaying(ent->client)) {
			// move us to spectate just so we don't have to twiddle
			// our thumbs forever
			CopyToBodyQue(ent);
			ent->client->sess.team = TEAM_SPECTATOR;
			P_PublishEngineTeam(ent);
			MoveClientToFreeCam(ent);
			gi.linkentity(ent);
			GetFollowTarget(ent);
		}
	}

	return true;
}

/*
==============
ClientBeginServerFrame

This will be called once for each server frame, before running
any other entities in the world.
==============
*/
static void PauseArenaClientClock(gtime_t &clock) {
	if (!clock || clock == HOLD_FOREVER)
		return;
	const int64_t frame_ms = static_cast<int64_t>(gi.frame_time_ms);
	if (frame_ms <= 0)
		return;
	const int64_t clock_ms = clock.milliseconds();
	if (clock_ms > std::numeric_limits<int64_t>::max() - frame_ms)
		clock = HOLD_FOREVER;
	else
		clock += gtime_t::from_ms(frame_ms);
}

static bool PauseArenaClientFrame(gentity_t *ent) {
	if (!ent || !ent->client || notGT(GT_ARENA) ||
		!MM_Arena_IsFighter(ent->client) ||
		!MM_Arena_IsPaused(MM_Arena_Id(ent)))
		return false;

	gclient_t *client = ent->client;
	PauseArenaClientClock(client->weapon_fire_finished);
	PauseArenaClientClock(client->weapon_think_time);
	PauseArenaClientClock(client->grenade_time);
	PauseArenaClientClock(client->grenade_finished_time);
	PauseArenaClientClock(client->next_drown_time);
	PauseArenaClientClock(client->pu_time_quad);
	PauseArenaClientClock(client->pu_time_haste);
	PauseArenaClientClock(client->pu_time_double);
	PauseArenaClientClock(client->pu_time_protection);
	PauseArenaClientClock(client->pu_time_invisibility);
	PauseArenaClientClock(client->pu_time_regeneration);
	PauseArenaClientClock(client->pu_time_rebreather);
	PauseArenaClientClock(client->pu_time_enviro);
	PauseArenaClientClock(client->pu_regen_time_regen);
	PauseArenaClientClock(client->pu_regen_time_blip);
	PauseArenaClientClock(client->grapple_release_time);
	PauseArenaClientClock(client->ir_time);
	PauseArenaClientClock(client->tracker_pain_time);
	PauseArenaClientClock(client->tech_regen_time);
	PauseArenaClientClock(client->tech_sound_time);
	PauseArenaClientClock(client->tech_expire_time);
	PauseArenaClientClock(client->frenzy_ammoregentime);
	PauseArenaClientClock(client->vampire_expiretime);
	PauseArenaClientClock(ent->air_finished);
	PauseArenaClientClock(ent->touch_debounce_time);
	PauseArenaClientClock(ent->pain_debounce_time);
	PauseArenaClientClock(ent->damage_debounce_time);
	return true;
}

void ClientBeginServerFrame(gentity_t *ent) {
	gclient_t *client;
	int		   buttonMask;

	if (gi.ServerFrame() != ent->client->step_frame)
		ent->s.renderfx &= ~RF_STAIR_STEP;

	if (!ent->client->pers.connected)
		return;

	if (level.intermission_time)
		return;

	client = ent->client;
	if (PauseArenaClientFrame(ent))
		return;
	MM_Horde_PauseClientPowerups(ent);

	if (client->awaiting_respawn) {
		int32_t retry_frames = gi.frame_time_ms > 0 ? 500 / gi.frame_time_ms : 5;
		if (retry_frames < 1)
			retry_frames = 1;

		if ((gi.ServerFrame() % retry_frames) == 0 || level.time > client->respawn_timeout)
			ClientSpawn(ent);
		return;
	}

	if (MM_Ghost_RunPendingRestoreFrame(ent))
		return;

	if (MM_FreezeTag_RunClientFrame(ent))
		return;

	if ((ent->svflags & SVF_BOT) != 0) {
		Bot_BeginFrame(ent);
	}

	// run weapon animations if it hasn't been done by a ucmd_t
	if (!client->weapon_thunk && ClientIsPlaying(client) && !client->eliminated && !client->menu)
		Think_Weapon(ent);
	else
		client->weapon_thunk = false;

	if (ent->deadflag) {
		if (deathmatch->integer && MM_FreezeTag_ShouldRespawnForRoundCountdown(ent) &&
				level.time > client->respawn_time && !level.coop_level_restart_time) {
			ClientRespawn(ent);
			return;
		}

		if (deathmatch->integer && ClientArenaEliminationRound(client) &&
				level.time > client->respawn_time && !level.coop_level_restart_time) {
			ClientRespawn(ent);
			return;
		}

		if (deathmatch->integer && ClientHordeEliminatedRound(client) &&
				level.time > client->respawn_time && !level.coop_level_restart_time) {
			ClientRespawn(ent);
			return;
		}

		if (deathmatch->integer && GT(GT_HORDE) && ClientIsPlaying(client) && client->eliminated &&
				level.round_state == round_state_t::ROUND_IN_PROGRESS) {
			return;
		}

		if (deathmatch->integer && GT(GT_HORDE) && ClientIsPlaying(client) && !client->eliminated &&
				level.round_state == round_state_t::ROUND_IN_PROGRESS &&
				level.time > client->respawn_time && !level.coop_level_restart_time) {
			ClientRespawn(ent);
			return;
		}

		// LMS: a fighter who died but still has a life left respawns mid-round with the arena
		// loadout (eliminated fighters are handled above by ClientArenaEliminationRound, which
		// transitions them to a spectator until the next round).
		if (deathmatch->integer && GT(GT_LMS) && ClientIsPlaying(client) && !client->eliminated &&
				level.round_state == round_state_t::ROUND_IN_PROGRESS &&
				level.time > client->respawn_time && !level.coop_level_restart_time) {
			ClientRespawn(ent);
			return;
		}

		// Arena elimination (CA/Strike): bots must not auto-respawn mid-round like FFA;
		// eliminated fighters use ClientArenaEliminationRound above to enter spectator.
		if (deathmatch->integer && (ent->svflags & SVF_BOT) && ClientIsPlaying(client) && !client->eliminated &&
				!ClientArenaBotMidRoundRespawnBlocked(client) &&
				level.time > client->respawn_time && !level.coop_level_restart_time) {
			ClientRespawn(ent);
			client->latched_buttons = BUTTON_NONE;
			return;
		}

		//muff mode: add minimum delay in dm
		if (deathmatch->integer && client->respawn_min_time && level.time > client->respawn_min_time && level.time <= client->respawn_time) {
			if ((client->latched_buttons & BUTTON_ATTACK)) {
				ClientRespawn(ent);
				client->latched_buttons = BUTTON_NONE;
			}
		} else if (level.time > client->respawn_time && !level.coop_level_restart_time) {
			// don't respawn if level is waiting to restart
			// check for coop handling
			if (!G_CoopRespawn(ent)) {
				// in deathmatch, only wait for attack button
				if (deathmatch->integer)
					buttonMask = BUTTON_ATTACK;
				else
					buttonMask = -1;

				if ((client->latched_buttons & buttonMask) ||
					(deathmatch->integer && g_dm_force_respawn->integer)) {
					ClientRespawn(ent);
					client->latched_buttons = BUTTON_NONE;
				}
			}
		}
		return;
	}

	// add player trail so monsters can follow
	if (!deathmatch->integer)
		PlayerTrail_Add(ent);

	client->latched_buttons = BUTTON_NONE;
}
/*
==============
RemoveAttackingPainDaemons

This is called to clean up the pain daemons that the disruptor attaches
to clients to damage them.
==============
*/
void RemoveAttackingPainDaemons(gentity_t *self) {
	gentity_t *tracker;

	tracker = G_FindByString<&gentity_t::classname>(nullptr, "pain daemon");
	while (tracker) {
		if (tracker->enemy == self)
			G_FreeEntity(tracker);
		tracker = G_FindByString<&gentity_t::classname>(tracker, "pain daemon");
	}

	if (self->client)
		self->client->tracker_pain_time = 0_ms;
}
