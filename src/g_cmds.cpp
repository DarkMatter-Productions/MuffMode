// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include "g_local.h"
#include "g_debug_log.h"
#include "muffmode/mm_vote.h"
#include "monsters/m_player.h"
/*freeze*/
#if 0
#include "freeze.h"
#endif
/*freeze*/

enum cmd_flags_t : uint32_t {
	CF_NONE				= 0,
	CF_ALLOW_DEAD		= bit_v<0>,
	CF_ALLOW_INT		= bit_v<1>,
	CF_ALLOW_SPEC		= bit_v<2>,
	CF_MATCH_ONLY		= bit_v<3>,
	CF_ADMIN_ONLY		= bit_v<4>,
	CF_CHEAT_PROTECT	= bit_v<5>,
};

struct cmds_t {
	const		char *name;
	void		(*func)(gentity_t *ent);
	uint32_t	flags;
};

// forward declarations for captain system
void SetCaptain(team_t team, gentity_t *ent);
void VacateCaptain(team_t team, gentity_t *leaving);

static void Cmd_Print_State(gentity_t *ent, bool on_state) {
	const char *s = gi.argv(0);
	if (s)
		gi.LocClient_Print(ent, PRINT_HIGH, "{} {}\n", s, on_state ? "ON" : "OFF");
}

static inline bool CheatsOk(gentity_t *ent) {
	if (!deathmatch->integer && !coop->integer)
		return true;
	
	if (!g_cheats->integer) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Cheats must be enabled to use this command.\n");
		return false;
	}

	return true;
}

static inline bool AliveOk(gentity_t *ent) {
	if (ent->health <= 0 || ent->deadflag) {
		//gi.LocClient_Print(ent, PRINT_HIGH, "You must be alive to use this command.\n");
		return false;
	}

	return true;
}

static inline bool SpectatorOk(gentity_t *ent) {
	if (!ClientIsPlaying(ent->client)) {
		//gi.LocClient_Print(ent, PRINT_HIGH, "Spectators cannot use this command.\n");
		return false;
	}

	return true;
}

static inline bool AdminOk(gentity_t *ent) {
	if (!g_allow_admin->integer || !ent->client->sess.admin) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Only admins can use this command.\n");
		return false;
	}

	return true;
}

//=================================================================================

static void SelectNextItem(gentity_t *ent, item_flags_t itflags, bool menu = true) {
	gclient_t *cl;
	item_id_t  i, index;
	gitem_t *it;

	cl = ent->client;

	if (menu && cl->menu) {
		P_Menu_Next(ent);
		return;
	} else if (menu && cl->follow_target) {
		FollowNext(ent);
		return;
	}

	// scan for the next valid one
	for (i = static_cast<item_id_t>(IT_NULL + 1); i <= IT_TOTAL; i = static_cast<item_id_t>(i + 1)) {
		index = static_cast<item_id_t>((cl->pers.selected_item + i) % IT_TOTAL);
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (!(it->flags & itflags))
			continue;

		cl->pers.selected_item = index;
		cl->pers.selected_item_time = level.time + SELECTED_ITEM_TIME;
		cl->ps.stats[STAT_SELECTED_ITEM_NAME] = CS_ITEMS + index;
		return;
	}

	cl->pers.selected_item = IT_NULL;
}

static void Cmd_InvNextP_f(gentity_t *ent) {
	SelectNextItem(ent, IF_TIMED | IF_POWERUP | IF_SPHERE);
}

static void Cmd_InvNextW_f(gentity_t *ent) {
	SelectNextItem(ent, IF_WEAPON);
}

static void Cmd_InvNext_f(gentity_t *ent) {
	SelectNextItem(ent, IF_ANY);
}

static void SelectPrevItem(gentity_t *ent, item_flags_t itflags) {
	gclient_t *cl = ent->client;
	item_id_t  i, index;
	gitem_t *it;

	if (cl->menu) {
		P_Menu_Prev(ent);
		return;
	} else if (cl->follow_target) {
		FollowPrev(ent);
		return;
	}

	// scan for the previous valid one
	for (i = static_cast<item_id_t>(IT_NULL + 1); i <= IT_TOTAL; i = static_cast<item_id_t>(i + 1)) {
		index = static_cast<item_id_t>((cl->pers.selected_item + IT_TOTAL - i) % IT_TOTAL);
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (!(it->flags & itflags))
			continue;

		cl->pers.selected_item = index;
		cl->pers.selected_item_time = level.time + SELECTED_ITEM_TIME;
		cl->ps.stats[STAT_SELECTED_ITEM_NAME] = CS_ITEMS + index;
		return;
	}

	cl->pers.selected_item = IT_NULL;
}

static void Cmd_InvPrevP_f(gentity_t *ent) {
	SelectPrevItem(ent, IF_TIMED | IF_POWERUP | IF_SPHERE);
}

static void Cmd_InvPrevW_f(gentity_t *ent) {
	SelectPrevItem(ent, IF_WEAPON);
}

static void Cmd_InvPrev_f(gentity_t *ent) {
	SelectPrevItem(ent, IF_ANY);
}

void ValidateSelectedItem(gentity_t *ent) {
	gclient_t *cl = ent->client;

	if (cl->pers.inventory[cl->pers.selected_item])
		return; // valid

	SelectNextItem(ent, IF_ANY, false);
}

//=================================================================================

static void SpawnAndGiveItem(gentity_t *ent, item_id_t id) {
	gitem_t *it = GetItemByIndex(id);

	if (!it)
		return;

	gentity_t *it_ent = G_Spawn();
	it_ent->classname = it->classname;
	SpawnItem(it_ent, it);

	if (it_ent->inuse) {
		Touch_Item(it_ent, ent, null_trace, true);
		if (it_ent->inuse)
			G_FreeEntity(it_ent);
	}
}

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
static void Cmd_Give_f(gentity_t *ent) {
	const char	*name = gi.args();
	gitem_t		*it;
	size_t		i;
	bool		give_all;
	gentity_t		*it_ent;

	if (Q_strcasecmp(name, "all") == 0)
		give_all = true;
	else
		give_all = false;

	if (give_all || Q_strcasecmp(gi.argv(1), "health") == 0) {
		if (gi.argc() == 3)
			ent->health = atoi(gi.argv(2));
		else
			ent->health = ent->max_health;
		if (!give_all)
			return;
	}

	if (give_all || Q_strcasecmp(name, "weapons") == 0) {
		for (i = 0; i < IT_TOTAL; i++) {
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (!(it->flags & IF_WEAPON))
				continue;
			ent->client->pers.inventory[i] += 1;
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_strcasecmp(name, "ammo") == 0) {
		if (give_all)
			SpawnAndGiveItem(ent, IT_PACK);

		for (i = 0; i < IT_TOTAL; i++) {
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (!(it->flags & IF_AMMO))
				continue;
			Add_Ammo(ent, it, AMMO_INFINITE);
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_strcasecmp(name, "armor") == 0) {
		ent->client->pers.inventory[IT_ARMOR_JACKET] = 0;
		ent->client->pers.inventory[IT_ARMOR_COMBAT] = 0;
		ent->client->pers.inventory[IT_ARMOR_BODY] = GetItemByIndex(IT_ARMOR_BODY)->armor_info->max_count;

		if (!give_all)
			return;
	}

	if (give_all || Q_strcasecmp(name, "keys") == 0) {
		for (i = 0; i < IT_TOTAL; i++) {
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (!(it->flags & IF_KEY))
				continue;
			ent->client->pers.inventory[i]++;
		}
		ent->client->pers.power_cubes = 0xFF;

		if (!give_all)
			return;
	}

	if (give_all) {
		SpawnAndGiveItem(ent, IT_POWER_SHIELD);

		if (!give_all)
			return;
	}

	if (give_all) {
		for (i = 0; i < IT_TOTAL; i++) {
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (it->flags & (IF_ARMOR | IF_POWER_ARMOR | IF_WEAPON | IF_AMMO | IF_NOT_GIVEABLE | IF_TECH))
				continue;
			else if (it->pickup == CTF_PickupFlag)
				continue;
			else if ((it->flags & IF_HEALTH) && !it->use)
				continue;
			ent->client->pers.inventory[i] = (it->flags & IF_KEY) ? 8 : 1;
		}

		G_CheckPowerArmor(ent);
		ent->client->pers.power_cubes = 0xFF;
		return;
	}

	it = FindItem(name);
	if (!it) {
		name = gi.argv(1);
		it = FindItem(name);
	}
	if (!it)
		it = FindItemByClassname(name);

	if (!it) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_unknown_item");
		return;
	}

	if (it->flags & IF_NOT_GIVEABLE) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_not_giveable");
		return;
	}

	if (!it->pickup) {
		ent->client->pers.inventory[it->id] = 1;
		return;
	}

	it_ent = G_Spawn();
	it_ent->classname = it->classname;
	SpawnItem(it_ent, it);
	if (it->flags & IF_AMMO && gi.argc() == 3)
		it_ent->count = atoi(gi.argv(2));

	// since some items don't actually spawn when you say to ..
	if (!it_ent->inuse)
		return;

	Touch_Item(it_ent, ent, null_trace, true);
	if (it_ent->inuse)
		G_FreeEntity(it_ent);
}

static void Cmd_SetPOI_f(gentity_t *self) {
	level.current_poi = self->s.origin;
	level.valid_poi = true;
}

static void Cmd_CheckPOI_f(gentity_t *self) {
	if (!level.valid_poi)
		return;

	char visible_pvs = gi.inPVS(self->s.origin, level.current_poi, false) ? 'y' : 'n';
	char visible_pvs_portals = gi.inPVS(self->s.origin, level.current_poi, true) ? 'y' : 'n';
	char visible_phs = gi.inPHS(self->s.origin, level.current_poi, false) ? 'y' : 'n';
	char visible_phs_portals = gi.inPHS(self->s.origin, level.current_poi, true) ? 'y' : 'n';

	gi.Com_PrintFmt("pvs {} + portals {}, phs {} + portals {}\n", visible_pvs, visible_pvs_portals, visible_phs, visible_phs_portals);
}

// [Paril-KEX]
static void Cmd_Target_f(gentity_t *ent) {
	ent->target = gi.argv(1);
	G_UseTargets(ent, ent);
	ent->target = nullptr;
}

/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
static void Cmd_God_f(gentity_t *ent) {
	ent->flags ^= FL_GODMODE;
	Cmd_Print_State(ent, ent->flags & FL_GODMODE);
}

/*
==================
Cmd_Immortal_f

Sets client to immortal - take damage but never go below 1 hp

argv(0) immortal
==================
*/
static void Cmd_Immortal_f(gentity_t *ent) {
	ent->flags ^= FL_IMMORTAL;
	Cmd_Print_State(ent, ent->flags & FL_IMMORTAL);
}

void ED_ParseField(const char *key, const char *value, gentity_t *ent);
/*
=================
Cmd_Spawn_f

Spawn class name

argv(0) spawn
argv(1) <classname>
argv(2+n) "key"...
argv(3+n) "value"...
=================
*/
static void Cmd_Spawn_f(gentity_t *ent) {
	solid_t backup = ent->solid;
	ent->solid = SOLID_NOT;
	gi.linkentity(ent);

	gentity_t *other = G_Spawn();
	other->classname = gi.argv(1);

	other->s.origin = ent->s.origin + (AngleVectors(ent->s.angles).forward * 24.f);
	other->s.angles[YAW] = ent->s.angles[YAW];

	st = {};

	if (gi.argc() > 3) {
		for (int i = 2; i < gi.argc(); i += 2)
			ED_ParseField(gi.argv(i), gi.argv(i + 1), other);
	}

	ED_CallSpawn(other);

	if (other->inuse) {
		vec3_t forward, end;
		AngleVectors(ent->client->v_angle, forward, nullptr, nullptr);
		end = ent->s.origin;
		end[2] += ent->viewheight;
		end += (forward * 8192);

		trace_t tr = gi.traceline(ent->s.origin + vec3_t{ 0.f, 0.f, (float)ent->viewheight }, end, other, MASK_SHOT | CONTENTS_MONSTERCLIP);
		other->s.origin = tr.endpos;

		for (size_t i = 0; i < 3; i++) {
			if (tr.plane.normal[i] > 0)
				other->s.origin[i] -= other->mins[i] * tr.plane.normal[i];
			else
				other->s.origin[i] += other->maxs[i] * -tr.plane.normal[i];
		}

		while (gi.trace(other->s.origin, other->mins, other->maxs, other->s.origin, other,
			MASK_SHOT | CONTENTS_MONSTERCLIP).startsolid) {
			float dx = other->mins[0] - other->maxs[0];
			float dy = other->mins[1] - other->maxs[1];
			other->s.origin += forward * -sqrtf(dx * dx + dy * dy);

			if ((other->s.origin - ent->s.origin).dot(forward) < 0) {
				gi.Client_Print(ent, PRINT_HIGH, "Couldn't find a suitable spawn location.\n");
				G_FreeEntity(other);
				break;
			}
		}

		if (other->inuse)
			gi.linkentity(other);

		if ((other->svflags & SVF_MONSTER) && other->think)
			other->think(other);
	}

	ent->solid = backup;
	gi.linkentity(ent);
}

/*
=================
Cmd_Teleport_f

argv(0) teleport
argv(1) x
argv(2) y
argv(3) z
argv(4) pitch
argv(5) yaw
argv(6) roll
=================
*/
static void Cmd_Teleport_f(gentity_t *ent) {
	if (gi.argc() < 4) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <x> <y> <z> <pitch> <yaw> <roll>\n", gi.argv(0));
		return;
	}

	ent->s.origin[0] = (float)atof(gi.argv(1));
	ent->s.origin[1] = (float)atof(gi.argv(2));
	ent->s.origin[2] = (float)atof(gi.argv(3));

	if (gi.argc() >= 4) {
		float pitch = (float)atof(gi.argv(4));
		float yaw = (float)atof(gi.argv(5));
		float roll = (float)atof(gi.argv(6));
		vec3_t ang{ pitch, yaw, roll };

		ent->client->ps.pmove.delta_angles = (ang - ent->client->resp.cmd_angles);
		ent->client->ps.viewangles = {};
		ent->client->v_angle = {};
	}

	gi.linkentity(ent);
}

/*
==================
TimeoutEnd
==================
*/
void TimeoutEnd() {
	level.timeout_in_place = 0_ms;
	level.timeout_ent = nullptr;
	gi.Broadcast_Print(PRINT_CENTER, "Timeout has ended.\n");
	gi.positioned_sound(world->s.origin, world, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex("misc/tele_up.wav"), 1, ATTN_NONE, 0);
}

/*
==================
Cmd_TimeIn_f

Ends a timeout session.
==================
*/
static void Cmd_TimeIn_f(gentity_t *ent) {
	if (!level.timeout_in_place) {
		gi.Client_Print(ent, PRINT_HIGH, "A timeout is not currently in effect.\n");
		return;
	}
	if (!ent->client->sess.admin && level.timeout_ent != ent) {
		gi.Client_Print(ent, PRINT_HIGH, "The timeout can only be ended by the timeout caller or an admin.\n");
		return;
	}

	gi.LocBroadcast_Print(PRINT_HIGH, "{} is resuming the match.\n", ent->client->pers.netname);
	level.timeout_in_place = 3_sec;
}

/*
==================
Cmd_TimeOut_f

Calls a timeout session.
==================
*/
static void Cmd_TimeOut_f(gentity_t *ent) {
	if (g_dm_timeout_length->integer <= 0) {
		gi.Client_Print(ent, PRINT_HIGH, "Server has disabled timeouts.\n");
		return;
	}
	if (level.match_state != MATCH_IN_PROGRESS) {
		gi.Client_Print(ent, PRINT_HIGH, "Timeouts can only be issued during a match.\n");
		return;
	}
	if (ent->client->pers.timeout_used && !ent->client->sess.admin) {
		gi.Client_Print(ent, PRINT_HIGH, "You have already used your timeout.\n");
		return;
	}
	if (level.timeout_in_place > 0_ms) {
		gi.Client_Print(ent, PRINT_HIGH, "A timeout is already in progress.\n");
		return;
	}

	level.timeout_ent = ent;
	level.timeout_in_place = gtime_t::from_sec(g_dm_timeout_length->integer);
	gi.LocBroadcast_Print(PRINT_CENTER, "{} called a timeout!\n{} has been granted.", ent->client->resp.netname, G_TimeString(g_dm_timeout_length->integer * 1000, false));
	gi.positioned_sound(world->s.origin, world, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex("world/klaxon2.wav"), 1, ATTN_NONE, 0);
	ent->client->pers.timeout_used = true;
}

/*
==================
Cmd_NoTarget_f

Sets client to notarget

argv(0) notarget
==================
*/
static void Cmd_NoTarget_f(gentity_t *ent) {
	ent->flags ^= FL_NOTARGET;
	Cmd_Print_State(ent, ent->flags & FL_NOTARGET);
}

/*
==================
Cmd_NoVisible_f

Sets client to "super notarget"

argv(0) novisible
==================
*/
static void Cmd_NoVisible_f(gentity_t *ent) {
	ent->flags ^= FL_NOVISIBLE;
	Cmd_Print_State(ent, ent->flags & FL_NOVISIBLE);
}

/*
==================
Cmd_AlertAll_f

argv(0) alertall
==================
*/
static void Cmd_AlertAll_f(gentity_t *ent) {
	for (size_t i = 0; i < globals.num_entities; i++) {
		gentity_t *t = &g_entities[i];

		if (!t->inuse || t->health <= 0 || !(t->svflags & SVF_MONSTER))
			continue;

		t->enemy = ent;
		FoundTarget(t);
	}
}

/*
==================
Cmd_NoClip_f

argv(0) noclip
==================
*/
static void Cmd_NoClip_f(gentity_t *ent) {
	ent->movetype = ent->movetype == MOVETYPE_NOCLIP ? MOVETYPE_WALK : MOVETYPE_NOCLIP;
	Cmd_Print_State(ent, ent->movetype == MOVETYPE_NOCLIP);
}

/*
==================
Cmd_Use_f

Use an inventory item
==================
*/
static void Cmd_Use_f(gentity_t *ent) {
	item_id_t	index;
	gitem_t		*it = nullptr;
	const char	*s = gi.args();
	const char	*cmd = gi.argv(0);

	if (!Q_strcasecmp(cmd, "use_index") || !Q_strcasecmp(cmd, "use_index_only")) {
		it = GetItemByIndex((item_id_t)atoi(s));
	} else {
		if (!strcmp(s, "holdable")) {
			if (ent->client->pers.inventory[IT_AMMO_NUKE])
				it = GetItemByIndex(IT_AMMO_NUKE);
			else if (ent->client->pers.inventory[IT_DOPPELGANGER])
				it = GetItemByIndex(IT_DOPPELGANGER);
			else if (ent->client->pers.inventory[IT_TELEPORTER])
				it = GetItemByIndex(IT_TELEPORTER);
			else if (ent->client->pers.inventory[IT_ADRENALINE])
				it = GetItemByIndex(IT_ADRENALINE);
			else if (ent->client->pers.inventory[IT_COMPASS])
				it = GetItemByIndex(IT_COMPASS);
			else return;
		}

		if (!it)
			it = FindItem(s);
	}

	if (!it) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_unknown_item_name", s);
		return;
	}
	if (!it->use) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_item_not_usable");
		return;
	}
	index = it->id;

	if (IsCombatDisabled() && !(it->flags & IF_WEAPON))
		return;

	// Paril: Use_Weapon handles weapon availability
	if (!(it->flags & IF_WEAPON) && !ent->client->pers.inventory[index]) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_out_of_item", it->pickup_name);
		return;
	}

	// allow weapon chains for use
	ent->client->no_weapon_chains = !!strcmp(gi.argv(0), "use") && !!strcmp(gi.argv(0), "use_index");

	it->use(ent, it);

	ValidateSelectedItem(ent);
}

/*
==================
Cmd_Drop_f

Drop an inventory item
==================
*/
static void Cmd_Drop_f(gentity_t *ent) {
	item_id_t	index;
	gitem_t		*it;
	const char	*s;

	// don't drop anything when combat is disabled
	if (IsCombatDisabled())
		return;

	s = gi.args();

	// Handle special cases first, before item lookup
	if (!Q_strcasecmp(s, "tech")) {
		it = Tech_Held(ent);

		if (it) {
			it->drop(ent, it);
			ValidateSelectedItem(ent);
		}

		return;
	}

	if (!Q_strcasecmp(s, "weapon")) {
		it = ent->client->pers.weapon;

		if (it) {
			it->drop(ent, it);
			ValidateSelectedItem(ent);
		}

		return;
	}

	const char *cmd = gi.argv(0);

	if (!Q_strcasecmp(cmd, "drop_index")) {
		it = GetItemByIndex((item_id_t)atoi(s));
	} else {
		it = FindItem(s);
	}

	if (!it) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Unknown item : {}\n", s);
		return;
	}
	if (!it->drop) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_item_not_droppable");
		return;
	}

	const char *t = nullptr;
	if (it->id == IT_FLAG_RED || it->id == IT_FLAG_BLUE) {
		if (!(g_drop_cmds->integer & 1))
			t = "Flag";
	} else if (it->flags & IF_POWERUP) {
		if (!(g_drop_cmds->integer & 2))
			t = "Powerup";
	} else if (it->flags & IF_WEAPON || it->flags & IF_AMMO) {
		if (!(g_drop_cmds->integer & 4))
			t = "Weapon and ammo";
		else if (!ItemSpawnsEnabled()) {
			gi.Client_Print(ent, PRINT_HIGH, "Weapon and ammo dropping is not available in this mode.\n");
			return;
		}
	}

	if (t != nullptr) {
		gi.LocClient_Print(ent, PRINT_HIGH, "{} dropping has been disabled on this server.\n", t);
		return;
	}

	index = it->id;
	if (!ent->client->pers.inventory[index]) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_out_of_item", it->pickup_name);
		return;
	}

	it->drop(ent, it);

	if (Teams() && g_teamplay_item_drop_notice->integer) {
		// add drop notice to all team mates
		//BroadcastTeamMessage(ent->client->sess.team, PRINT_CHAT, G_Fmt("[TEAM]: {} drops {}\n", ent->client->resp.netname, it->use_name).data());

		uint32_t key = GetUnicastKey();

		for (auto ec : active_clients()) {
			if (ent == ec)
				continue;
			if (ClientIsPlaying(ec->client) && !OnSameTeam(ent, ec))
				continue;
			if (!ClientIsPlaying(ec->client) && !ec->client->follow_target)
				continue;
			if (!ClientIsPlaying(ec->client) && ec->client->follow_target && !OnSameTeam(ent, ec->client->follow_target))
				continue;
			if (!ClientIsPlaying(ec->client) && ec->client->follow_target && ent == ec->client->follow_target)
				continue;
			
			gi.WriteByte(svc_poi);
			gi.WriteShort(POI_PING + (ent->s.number - 1));
			gi.WriteShort(5000);
			gi.WritePosition(ent->s.origin);
			gi.WriteShort(gi.imageindex(it->icon));
			gi.WriteByte(215);
			gi.WriteByte(POI_FLAG_NONE);
			gi.unicast(ec, false);
			gi.local_sound(ec, CHAN_AUTO, gi.soundindex("misc/help_marker.wav"), 1.0f, ATTN_NONE, 0.0f, key);
			
			gi.LocClient_Print(ec, PRINT_TTS, G_Fmt("[TEAM]: {} drops {}\n", ent->client->resp.netname, it->use_name).data(), ent->client->resp.netname);
		}
	}

	ValidateSelectedItem(ent);
}

/*
=================
Cmd_Inven_f
=================
*/
static void Cmd_Inven_f(gentity_t *ent) {
	size_t		i;
	gclient_t	*cl;

	cl = ent->client;

	cl->showscores = false;
	cl->showhelp = false;

	globals.server_flags &= ~SERVER_FLAG_SLOW_TIME;

	if (deathmatch->integer && ent->client->menu) {
		if (Vote_Menu_Active(ent))
			return;
		//gi.Client_Print(ent, PRINT_HIGH, "ARGH!\n");
		P_Menu_Close(ent);
		ent->client->follow_update = true;
		if (!ent->client->initial_menu_closure) {
			gi.LocClient_Print(ent, PRINT_CENTER, "%bind:inven:Toggles Menu%{}", " ");
			ent->client->initial_menu_closure = true;
		}
		return;
	}

	if (cl->showinventory) {
		cl->showinventory = false;
		return;
	}

	if (deathmatch->integer) {
		if (Vote_Menu_Active(ent))
			return;

		G_Menu_Join_Open(ent);
		return;
	}
	globals.server_flags |= SERVER_FLAG_SLOW_TIME;

	cl->showinventory = true;

	gi.WriteByte(svc_inventory);
	for (i = 0; i < IT_TOTAL; i++)
		gi.WriteShort(cl->pers.inventory[i]);
	for (; i < MAX_ITEMS; i++)
		gi.WriteShort(0);
	gi.unicast(ent, true);
}

/*
=================
Cmd_InvUse_f
=================
*/
static void Cmd_InvUse_f(gentity_t *ent) {
	gitem_t *it;

	if (deathmatch->integer && ent->client->menu) {
		P_Menu_Select(ent);
		return;
	}

	if (!ClientIsPlaying(ent->client))
		return;

	if (ent->health <= 0 || ent->deadflag)
		return;

	ValidateSelectedItem(ent);

	if (ent->client->pers.selected_item == IT_NULL) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_no_item_to_use");
		return;
	}

	it = &itemlist[ent->client->pers.selected_item];
	if (!it->use) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_item_not_usable");
		return;
	}

	// don't allow weapon chains for invuse
	ent->client->no_weapon_chains = true;
	it->use(ent, it);

	ValidateSelectedItem(ent);
}

/*
=================
Cmd_WeapPrev_f
=================
*/
static void Cmd_WeapPrev_f(gentity_t *ent) {
	gclient_t	*cl = ent->client;
	item_id_t	i, index;
	gitem_t		*it;
	item_id_t	selected_weapon;

	if (!cl->pers.weapon)
		return;

	// don't allow weapon chains for weapprev
	cl->no_weapon_chains = true;

	selected_weapon = cl->pers.weapon->id;

	// scan  for the next valid one
	for (i = static_cast<item_id_t>(IT_NULL + 1); i <= IT_TOTAL; i = static_cast<item_id_t>(i + 1)) {
		// PMM - prevent scrolling through ALL weapons
		index = static_cast<item_id_t>((selected_weapon + IT_TOTAL - i) % IT_TOTAL);
		if (!cl->pers.inventory[index])
			continue;

		it = &itemlist[index];
		if (!it->use)
			continue;

		if (!(it->flags & IF_WEAPON))
			continue;

		it->use(ent, it);
		if (cl->newweapon == it)
			return; // successful
	}
}

/*
=================
Cmd_WeapNext_f
=================
*/
static void Cmd_WeapNext_f(gentity_t *ent) {
	gclient_t	*cl = ent->client;
	item_id_t	i, index;
	gitem_t		*it;
	item_id_t	selected_weapon;

	if (!cl->pers.weapon)
		return;

	// don't allow weapon chains for weapnext
	cl->no_weapon_chains = true;

	selected_weapon = cl->pers.weapon->id;

	// scan  for the next valid one
	for (i = static_cast<item_id_t>(IT_NULL + 1); i <= IT_TOTAL; i = static_cast<item_id_t>(i + 1)) {
		// PMM - prevent scrolling through ALL weapons
		index = static_cast<item_id_t>((selected_weapon + i) % IT_TOTAL);
		if (!cl->pers.inventory[index])
			continue;

		it = &itemlist[index];
		if (!it->use)
			continue;

		if (!(it->flags & IF_WEAPON))
			continue;

		it->use(ent, it);
		// PMM - prevent scrolling through ALL weapons

		if (cl->newweapon == it)
			return;
	}
}

/*
=================
Cmd_WeapLast_f
=================
*/
static void Cmd_WeapLast_f(gentity_t *ent) {
	gclient_t	*cl = ent->client;
	int			index;
	gitem_t		*it;

	if (!cl->pers.weapon || !cl->pers.lastweapon)
		return;

	// don't allow weapon chains for weaplast
	cl->no_weapon_chains = true;

	index = cl->pers.lastweapon->id;
	if (!cl->pers.inventory[index])
		return;

	it = &itemlist[index];
	if (!it->use)
		return;

	if (!(it->flags & IF_WEAPON))
		return;

	it->use(ent, it);
}

/*
=================
Cmd_InvDrop_f
=================
*/
static void Cmd_InvDrop_f(gentity_t *ent) {
	gitem_t *it;

	ValidateSelectedItem(ent);

	if (ent->client->pers.selected_item == IT_NULL) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_no_item_to_drop");
		return;
	}

	it = &itemlist[ent->client->pers.selected_item];
	if (!it->drop) {
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_item_not_droppable");
		return;
	}
	it->drop(ent, it);

	ValidateSelectedItem(ent);
}

/*
=================
Cmd_Forfeit_f
=================
*/
static void Cmd_Forfeit_f(gentity_t *ent) {
	if (notGT(GT_DUEL)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Forfeit is only available in a duel.\n");
		return;
	}
	if (level.match_state < matchst_t::MATCH_IN_PROGRESS) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Forfeit is not available during warmup.\n");
		return;
	}
	if (ent->client != &game.clients[level.sorted_clients[1]]) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Forfeit is only available to the losing player.\n");
		return;
	}
	if (!g_allow_forfeit->integer) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Forfeits are not enabled on this server.\n");
		return;
	}

	QueueIntermission(G_Fmt("{} forfeits the match.", ent->client->resp.netname).data(), true, false);
}

/*
=================
Cmd_Kill_f
=================
*/
static void Cmd_Kill_f(gentity_t *ent) {
	if ((level.time - ent->client->respawn_time) < 5_sec)
		return;

	if (IsCombatDisabled())
		return;

	if (false) { // Race mode removed
		ClientSpawn(ent);
		G_PostRespawn(ent);
		return;
	}

	ent->flags &= ~FL_GODMODE;
	ent->health = 0;

	//  make sure no trackers are still hurting us.
	if (ent->client->tracker_pain_time)
		RemoveAttackingPainDaemons(ent);

	if (ent->client->owned_sphere) {
		G_FreeEntity(ent->client->owned_sphere);
		ent->client->owned_sphere = nullptr;
	}

	// [Paril-KEX] don't allow kill to take points away in TDM
	player_die(ent, ent, ent, 100000, vec3_origin, { MOD_SUICIDE, GT(GT_TDM) });
}

/*
=================
Cmd_Kill_AI_f
=================
*/
static void Cmd_Kill_AI_f(gentity_t *ent) {
	// except the one we're looking at...
	gentity_t *looked_at = nullptr;

	vec3_t start = ent->s.origin + vec3_t{ 0.f, 0.f, (float)ent->viewheight };
	vec3_t end = start + ent->client->v_forward * 1024.f;

	looked_at = gi.traceline(start, end, ent, MASK_SHOT).ent;

	const int numEntities = globals.num_entities;
	for (int entnum = 1; entnum < numEntities; ++entnum) {
		gentity_t *entity = &g_entities[entnum];
		if (!entity->inuse || entity == looked_at) {
			continue;
		}

		if ((entity->svflags & SVF_MONSTER) == 0) {
			continue;
		}

		G_FreeEntity(entity);
	}

	gi.LocClient_Print(ent, PRINT_HIGH, "{}: All AI Are Dead...\n", __FUNCTION__);
}

/*
=================
Cmd_Where_f
=================
*/
static void Cmd_Where_f(gentity_t *ent) {
	if (ent == nullptr || ent->client == nullptr)
		return;

	const vec3_t &origin = ent->s.origin;
	
	std::string location;
	fmt::format_to(std::back_inserter(location), FMT_STRING("{:.1f} {:.1f} {:.1f} {:.1f} {:.1f} {:.1f}\n"), origin[0], origin[1], origin[2], ent->client->ps.viewangles[PITCH], ent->client->ps.viewangles[YAW], ent->client->ps.viewangles[ROLL]);
	gi.LocClient_Print(ent, PRINT_HIGH, "Location: {}\n", location.c_str());
	gi.SendToClipBoard(location.c_str());
}

/*
=================
Cmd_Clear_AI_Enemy_f
=================
*/
static void Cmd_Clear_AI_Enemy_f(gentity_t *ent) {
	for (size_t i = 1; i < globals.num_entities; i++) {
		gentity_t *entity = &g_entities[i];
		if (!entity->inuse)
			continue;
		if ((entity->svflags & SVF_MONSTER) == 0)
			continue;

		entity->monsterinfo.aiflags |= AI_FORGET_ENEMY;
	}

	gi.LocClient_Print(ent, PRINT_HIGH, "{}: Clear All AI Enemies...\n", __FUNCTION__);
}

/*
=================
Cmd_PutAway_f
=================
*/
static void Cmd_PutAway_f(gentity_t *ent) {
	ent->client->showscores = false;
	ent->client->showhelp = false;
	ent->client->showinventory = false;

	gentity_t *e = ent->client->follow_target ? ent->client->follow_target : ent;
	ent->client->ps.stats[STAT_SHOW_STATUSBAR] = !ClientIsPlaying(e->client) || e->client->eliminated ? 0 : 1;

	globals.server_flags &= ~SERVER_FLAG_SLOW_TIME;

	ent->client->follow_update = true;

	if (deathmatch->integer && ent->client->menu) {
		if (Vote_Menu_Active(ent))
			return;
		//gi.Client_Print(ent, PRINT_HIGH, "ARGH! 2\n");
		P_Menu_Close(ent);
	}
}

static int PlayerSortByScore(const void *a, const void *b) {
	int anum, bnum;

	anum = *(const int *)a;
	bnum = *(const int *)b;

	anum = game.clients[anum].resp.score;
	bnum = game.clients[bnum].resp.score;

	if (anum < bnum)
		return -1;
	if (anum > bnum)
		return 1;
	return 0;
}

/*
=================
PlayersList
=================
*/
static void PlayersList(gentity_t *ent, bool ranked) {
	size_t	i, count;
	static std::string	small, large;
	int		index[MAX_CLIENTS_KEX] = { 0 };

	small.clear();
	large.clear();

	count = 0;
	for (auto ec : active_clients()) {
		index[count] = ec - g_entities - 1;
		count++;
	}

	// sort by score
	if (ranked)
		qsort(index, count, sizeof(index[0]), PlayerSortByScore);

	// print information
	large[0] = 0;

	if (count) {
		for (i = 0; i < count; i++) {
			gclient_t *cl = &game.clients[index[i]];

			char value[MAX_INFO_VALUE] = { 0 };
			gi.Info_ValueForKey(cl->pers.userinfo, "name", value, sizeof(value));

			fmt::format_to(std::back_inserter(small), FMT_STRING("{:9} {:32} {:32} {:02}:{:02} {:4} {:5} {}{}\n"), index[i], cl->pers.social_id, value, (level.time - cl->resp.entertime).milliseconds() / 60000,
				((level.time - cl->resp.entertime).milliseconds() % 60000) / 1000, cl->ping,
				cl->resp.score, cl->sess.duel_queued ? "QUEUE" : Teams_TeamName(cl->sess.team), cl->sess.admin ? " (admin)" : cl->sess.inactive ? " (inactive)" : "");

			if (small.length() + large.length() > MAX_IDEAL_PACKET_SIZE - 50) { // can't print all of them in one packet
				large += "...\n";
				break;
			}

			large += small;
			small.clear();
		}

		// remove the last newline
		large.pop_back();
	}

	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "\nclientnum id                               name                             time  ping score team\n");
	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "--------------------------------------------------------------------------------------------------------------\n");
	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, large.c_str());
	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "\n--------------------------------------------------------------------------------------------------------------\n");
	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "total players: {}\n", count);
	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "\n");
}

/*
=================
Cmd_Players_f
=================
*/
static void Cmd_Players_f(gentity_t *ent) {
	PlayersList(ent, false);
}

/*
=================
Cmd_PlayersRanked_f
=================
*/
static void Cmd_PlayersRanked_f(gentity_t *ent) {
	PlayersList(ent, true);
}

/*
=================
PlayerSortByJoinTime
=================
*/
static int PlayerSortByJoinTime(const void *a, const void *b) {
	int anum, bnum;

	anum = *(const int *)a;
	bnum = *(const int *)b;

	anum = game.clients[anum].sess.team_join_time.milliseconds();
	bnum = game.clients[bnum].sess.team_join_time.milliseconds();

	if (anum > bnum)
		return -1;
	if (anum < bnum)
		return 1;
	return 0;
}

/*
=================
Cmd_PlayersJoinTime_f
=================
*/
static void Cmd_PlayersJoinTime_f(gentity_t *ent) {
	size_t	i, count;
	static std::string	small, large;
	int		index[MAX_CLIENTS_KEX] = { 0 };

	small.clear();
	large.clear();

	count = 0;
	for (auto ec : active_clients()) {
		index[count] = ec - g_entities - 1;
		count++;
	}

	// sort by score
	qsort(index, count, sizeof(index[0]), PlayerSortByJoinTime);

	// print information
	large[0] = 0;

	if (count) {
		for (i = 0; i < count; i++) {
			gclient_t *cl = &game.clients[index[i]];

			char value[MAX_INFO_VALUE] = { 0 };
			gi.Info_ValueForKey(cl->pers.userinfo, "name", value, sizeof(value));

			fmt::format_to(std::back_inserter(small), FMT_STRING("{:32} {:32}\n"), cl->sess.team_join_time.milliseconds(), value);

			if (small.length() + large.length() > MAX_IDEAL_PACKET_SIZE - 50) { // can't print all of them in one packet
				large += "...\n";
				break;
			}

			large += small;
			small.clear();
		}

		// remove the last newline
		large.pop_back();
	}

	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "\nclientnum id                               name                             time  ping score team\n");
	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "--------------------------------------------------------------------------------------------------------------\n");
	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, large.c_str());
	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "\n--------------------------------------------------------------------------------------------------------------\n");
	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "total players: {}\n", count);
	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "\n");
}

bool CheckFlood(gentity_t *ent) {
	int		   i;
	gclient_t *cl;

	if (flood_msgs->integer) {
		cl = ent->client;

		if (level.time < cl->flood_locktill) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_flood_cant_talk",
				(cl->flood_locktill - level.time).seconds<int32_t>());
			return true;
		}
		i = cl->flood_whenhead - flood_msgs->integer + 1;
		if (i < 0)
			i = (sizeof(cl->flood_when) / sizeof(cl->flood_when[0])) + i;
		if (i >= q_countof(cl->flood_when))
			i = 0;
		if (cl->flood_when[i] && level.time - cl->flood_when[i] < gtime_t::from_sec(flood_persecond->value)) {
			cl->flood_locktill = level.time + gtime_t::from_sec(flood_waitdelay->value);
			gi.LocClient_Print(ent, PRINT_CHAT, "$g_flood_cant_talk",
				flood_waitdelay->integer);
			return true;
		}
		cl->flood_whenhead = (cl->flood_whenhead + 1) % (sizeof(cl->flood_when) / sizeof(cl->flood_when[0]));
		cl->flood_when[cl->flood_whenhead] = level.time;
	}
	return false;
}

/*
=================
Cmd_Wave_f
=================
*/
static void Cmd_Wave_f(gentity_t *ent) {
	int i = atoi(gi.argv(1));

	// no dead or noclip waving
	if (ent->deadflag || ent->movetype == MOVETYPE_NOCLIP)
		return;

	// can't wave when ducked
	bool do_animate = ent->client->anim_priority <= ANIM_WAVE && !(ent->client->ps.pmove.pm_flags & PMF_DUCKED);

	if (do_animate)
		ent->client->anim_priority = ANIM_WAVE;

	const char *other_notify_msg = nullptr, *other_notify_none_msg = nullptr;

	vec3_t start, dir;
	P_ProjectSource(ent, ent->client->v_angle, { 0, 0, 0 }, start, dir);

	// see who we're aiming at
	gentity_t *aiming_at = nullptr;
	float best_dist = -9999;

	for (auto player : active_clients()) {
		if (player == ent)
			continue;

		vec3_t cdir = player->s.origin - start;
		float dist = cdir.normalize();

		float dot = ent->client->v_forward.dot(cdir);

		if (dot < 0.97)
			continue;
		else if (dist < best_dist)
			continue;

		best_dist = dist;
		aiming_at = player;
	}

	switch (i) {
	case GESTURE_FLIP_OFF:
		other_notify_msg = "$g_flipoff_other";
		other_notify_none_msg = "$g_flipoff_none";
		if (do_animate) {
			ent->s.frame = FRAME_flip01 - 1;
			ent->client->anim_end = FRAME_flip12;
		}
		break;
	case GESTURE_SALUTE:
		other_notify_msg = "$g_salute_other";
		other_notify_none_msg = "$g_salute_none";
		if (do_animate) {
			ent->s.frame = FRAME_salute01 - 1;
			ent->client->anim_end = FRAME_salute11;
		}
		break;
	case GESTURE_TAUNT:
		other_notify_msg = "$g_taunt_other";
		other_notify_none_msg = "$g_taunt_none";
		if (do_animate) {
			ent->s.frame = FRAME_taunt01 - 1;
			ent->client->anim_end = FRAME_taunt17;
		}
		break;
	case GESTURE_WAVE:
		other_notify_msg = "$g_wave_other";
		other_notify_none_msg = "$g_wave_none";
		if (do_animate) {
			ent->s.frame = FRAME_wave01 - 1;
			ent->client->anim_end = FRAME_wave11;
		}
		break;
	case GESTURE_POINT:
	default:
		other_notify_msg = "$g_point_other";
		other_notify_none_msg = "$g_point_none";
		if (do_animate) {
			ent->s.frame = FRAME_point01 - 1;
			ent->client->anim_end = FRAME_point12;
		}
		break;
	}

	bool has_a_target = false;

	if (i == GESTURE_POINT) {
		for (auto player : active_clients()) {
			if (player == ent)
				continue;
			else if (!OnSameTeam(ent, player))
				continue;

			has_a_target = true;
			break;
		}
	}

	if (i == GESTURE_POINT && has_a_target) {
		// don't do this stuff if we're flooding
		if (CheckFlood(ent))
			return;

		trace_t tr = gi.traceline(start, start + (ent->client->v_forward * 2048), ent, MASK_SHOT & ~CONTENTS_WINDOW);
		other_notify_msg = "$g_point_other_ping";

		uint32_t key = GetUnicastKey();

		if (tr.fraction != 1.0f) {
			// send to all teammates
			for (auto player : active_clients()) {
				if (player != ent && !OnSameTeam(ent, player))
					continue;

				gi.WriteByte(svc_poi);
				gi.WriteShort(POI_PING + (ent->s.number - 1));
				gi.WriteShort(5000);
				gi.WritePosition(tr.endpos);
				gi.WriteShort(level.pic_ping);
				gi.WriteByte(208);
				gi.WriteByte(POI_FLAG_NONE);
				gi.unicast(player, false);

				gi.local_sound(player, CHAN_AUTO, gi.soundindex("misc/help_marker.wav"), 1.0f, ATTN_NONE, 0.0f, key);
				gi.LocClient_Print(player, PRINT_HIGH, other_notify_msg, ent->client->resp.netname);
			}
		}
	} else {
		if (CheckFlood(ent))
			return;

		gentity_t *targ = nullptr;
		while ((targ = findradius(targ, ent->s.origin, 1024)) != nullptr) {
			if (ent == targ) continue;
			if (!targ->client) continue;
			if (!gi.inPVS(ent->s.origin, targ->s.origin, false)) continue;

			if (aiming_at && other_notify_msg)
				gi.LocClient_Print(targ, PRINT_TTS, other_notify_msg, ent->client->resp.netname, aiming_at->client->resp.netname);
			else if (other_notify_none_msg)
				gi.LocClient_Print(targ, PRINT_TTS, other_notify_none_msg, ent->client->resp.netname);
		}

		if (aiming_at && other_notify_msg)
			gi.LocClient_Print(ent, PRINT_TTS, other_notify_msg, ent->client->resp.netname, aiming_at->client->resp.netname);
		else if (other_notify_none_msg)
			gi.LocClient_Print(ent, PRINT_TTS, other_notify_none_msg, ent->client->resp.netname);
	}

	ent->client->anim_time = 0_ms;
}

#ifndef KEX_Q2_GAME
/*
==================
Cmd_Say_f

NB: only used for non-Playfab stuff
==================
*/
static void Cmd_Say_f(gentity_t *ent, bool arg0) {
	gentity_t *other;
	const char *p_in;
	static std::string text;

	if (gi.argc() < 2 && !arg0)
		return;
	else if (CheckFlood(ent))
		return;

	text.clear();
	fmt::format_to(std::back_inserter(text), FMT_STRING("{}: "), ent->client->resp.netname);

	if (arg0) {
		text += gi.argv(0);
		text += " ";
		text += gi.args();
	} else {
		p_in = gi.args();
		size_t in_len = strlen(p_in);

		if (p_in[0] == '\"' && p_in[in_len - 1] == '\"')
			text += std::string_view(p_in + 1, in_len - 2);
		else
			text += p_in;
	}

	// don't let text be too long for malicious reasons
	if (text.length() > 150)
		text.resize(150);

	if (text.back() != '\n')
		text.push_back('\n');

	if (g_dedicated->integer)
		gi.Client_Print(nullptr, PRINT_CHAT, text.c_str());

	for (uint32_t j = 1; j <= game.maxclients; j++) {
		other = &g_entities[j];
		if (!other->inuse)
			continue;
		if (!other->client)
			continue;
		gi.Client_Print(other, PRINT_CHAT, text.c_str());
	}
}

/*
=================
Cmd_Say_Team_f

NB: only used for non-Playfab stuff
=================
*/
static void Cmd_Say_Team_f(gentity_t *who, const char *msg_in) {
	gentity_t *cl_ent;
	char outmsg[256];

	if (CheckFlood(who))
		return;

	Q_strlcpy(outmsg, msg_in, sizeof(outmsg));

	char *msg = outmsg;

	if (*msg == '\"') {
		msg[strlen(msg) - 1] = 0;
		msg++;
	}

	for (size_t i = 0; i < game.maxclients; i++) {
		cl_ent = g_entities + 1 + i;
		if (!cl_ent->inuse)
			continue;
		if (cl_ent->client->sess.team == who->client->sess.team)
			gi.LocClient_Print(cl_ent, PRINT_CHAT, "({}): {}\n",
				who->client->resp.netname, msg);
	}
}
#endif

/*
=================
Cmd_ListEntities_f
=================
*/
static void Cmd_ListEntities_f(gentity_t *ent) {
	int count = 0;

	for (size_t i = 1; i < game.maxentities; i++) {
		gentity_t *e = &g_entities[i];

		if (!e || !e->inuse)
			continue;
		
		if (gi.argc() > 1) {
			if (!strstr(e->classname, gi.argv(1)))
				continue;
		}
		if (gi.argc() > 2) {
			float num = atof(gi.argv(3));
			if (e->s.origin[0] != num)
				continue;
		}
		if (gi.argc() > 3) {
			float num = atof(gi.argv(4));
			if (e->s.origin[1] != num)
				continue;
		}
		if (gi.argc() > 4) {
			float num = atof(gi.argv(5));
			if (e->s.origin[2] != num)
				continue;
		}

		gi.Com_PrintFmt("{}: {}", i, *e);
//#if 0
		if (e->target)
			gi.Com_PrintFmt(", target={}", e->target);
		if (e->targetname)
			gi.Com_PrintFmt(", targetname={}", e->targetname);
//#endif
		gi.Com_Print("\n");

		count++;
	}
	gi.Com_PrintFmt("\ntotal valid entities={}\n", count);
}

/*
=================
Cmd_ListMonsters_f
=================
*/
static void Cmd_ListMonsters_f(gentity_t *ent) {
	if (!g_debug_monster_kills->integer)
		return;

	for (size_t i = 0; i < level.total_monsters; i++) {
		gentity_t *e = level.monsters_registered[i];

		if (!e || !e->inuse)
			continue;
		else if (!(e->svflags & SVF_MONSTER) || (e->monsterinfo.aiflags & AI_DO_NOT_COUNT))
			continue;
		else if (e->deadflag)
			continue;

		gi.Com_PrintFmt("{}\n", *e);
	}
}

// =========================================
// TEAMPLAY - MOSTLY PORTED FROM QUAKE III
// =========================================

/*
================
PickTeam
================
*/
team_t PickTeam(int ignore_client_num) {
	if (!Teams())
		return TEAM_FREE;

	if (level.num_playing_blue > level.num_playing_red)
		return TEAM_RED;

	if (level.num_playing_red > level.num_playing_blue)
		return TEAM_BLUE;

	// equal team count, so join the team with the lowest score
	if (level.team_scores[TEAM_BLUE] > level.team_scores[TEAM_RED])
		return TEAM_RED;
	if (level.team_scores[TEAM_RED] > level.team_scores[TEAM_BLUE])
		return TEAM_BLUE;

	// equal team scores, so join team with lowest total individual scores
	// skip in tdm as it's redundant
	if (notGT(GT_TDM)) {
		int iscore_red = 0, iscore_blue = 0;

		for (size_t i = 0; i < game.maxclients; i++) {
			if (i == ignore_client_num)
				continue;
			if (!game.clients[i].pers.connected)
				continue;

			if (game.clients[i].sess.team == TEAM_RED) {
				iscore_red += game.clients[i].resp.score;
				continue;
			}
			if (game.clients[i].sess.team == TEAM_BLUE) {
				iscore_blue += game.clients[i].resp.score;
				continue;
			}
		}

		if (iscore_blue > iscore_red)
			return TEAM_RED;
		if (iscore_red > iscore_blue)
			return TEAM_BLUE;
	}

	// otherwise just randomly select a team
	return brandom() ? TEAM_RED : TEAM_BLUE;
}

/*
=================
BroadcastTeamChange

Let everyone know about a team change
=================
*/
void BroadcastTeamChange(gentity_t *ent, int old_team, bool inactive, bool silent) {

	if (!deathmatch->integer)
		return;

	if (!ent->client)
		return;

	if (notGT(GT_DUEL) && ent->client->sess.team == old_team)
		return;

	if (silent)
		return;

	const char *s = nullptr, *t = nullptr;
	char		name[MAX_INFO_VALUE] = { 0 };
	int32_t		client_num;

	client_num = ent - g_entities - 1;
	gi.Info_ValueForKey(ent->client->pers.userinfo, "name", name, sizeof(name));

	switch (ent->client->sess.team) {
	case TEAM_FREE:
		s = G_Fmt("{} joined the battle.\n", name).data();
		//t = "%bind:inven:Toggles Menu%You have joined the game.";
		t = "You have joined the game.";
		break;
	case TEAM_SPECTATOR:
		if (inactive) {
			s = G_Fmt("{} is inactive,\nmoved to spectators.\n", name).data();
			t = "You are inactive and have been\nmoved to spectators.";
		} else {
			if (GT(GT_DUEL) && ent->client->sess.duel_queued) {
				s = G_Fmt("{} is in the queue to play.\n", name).data();
				t = "You are in the queue to play.";
			} else {
				s = G_Fmt("{} joined the spectators.\n", name).data();
				t = "You are now spectating.";
			}
		}
		break;
	case TEAM_RED:
	case TEAM_BLUE:
		s = G_Fmt("{} joined the {} Team.\n", name, Teams_TeamName(ent->client->sess.team)).data();
		t = G_Fmt("You have joined the {} Team.\n", Teams_TeamName(ent->client->sess.team)).data();
		break;
	}

	if (s) {
		for (auto ec : active_clients()) {
			if (ec == ent)
				continue;
			if (ec->svflags & SVF_BOT)
				continue;
			gi.LocClient_Print(ec, PRINT_CENTER, s);
		}
		//gi.Com_Print(s);
	}

	if (g_dm_do_readyup->integer && level.match_state == matchst_t::MATCH_WARMUP_READYUP) {
		BroadcastReadyReminderMessage();
	} else if (t) {
		gi.LocClient_Print(ent, PRINT_CENTER, G_Fmt("%bind:inven:Toggles Menu%{}", t).data() );
	}
}

/*
=================
AllowTeamSwitch
=================
*/
static bool AllowTeamSwitch(gentity_t *ent, team_t desired_team) {
	/*
	if (desired_team != ent->client->sess.team && GT(GT_RR) && level.match_state == matchst_t::MATCH_IN_PROGRESS) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You cannot change teams during a Red Rover match.\n");
		return false;
	}
	*/
	if (desired_team != TEAM_SPECTATOR && maxplayers->integer && level.num_playing_human_clients >= maxplayers->integer) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Maximum player count has been reached.\n");
		return false; // ignore the request
	}

	if (level.locked[desired_team]) {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} is locked.\n", Teams_TeamName(desired_team));
		return false; // ignore the request
	}

	if (Teams()) {
		if (g_teamplay_force_balance->integer) {
			// We allow a spread of two
			if ((desired_team == TEAM_RED && (level.num_playing_red - level.num_playing_blue > 1)) ||
				(desired_team == TEAM_BLUE && (level.num_playing_blue - level.num_playing_red > 1))) {
				gi.LocClient_Print(ent, PRINT_HIGH, "{} has too many players.\n", Teams_TeamName(desired_team));
				return false; // ignore the request
			}

			// It's ok, the team we are switching to has less or same number of players
		}
	}

	return true;
}

/*
=================
AllowClientTeamSwitch
=================
*/
bool AllowClientTeamSwitch(gentity_t *ent) {
	if (!deathmatch->integer)
		return false;

	if (g_dm_force_join->integer || !g_teamplay_allow_team_pick->integer) {
		if (!(ent->svflags & SVF_BOT)) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Team picks are disabled.");
			return false;
		}
	}
	
	if (ent->client->resp.team_delay_time > level.time) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You may not switch teams more than once per 5 seconds.\n");
		return false;
	}

	return true;
}

/*
================
TeamBalance

Balance the teams without shuffling.
Switch last joined player(s) from stacked team.
================
*/
int TeamBalance(bool force) {
	if (!Teams())
		return 0;

	if (GT(GT_RR))
		return 0;

	int delta = abs(level.num_playing_red - level.num_playing_blue);

	if (delta < 2)
		return level.num_playing_red - level.num_playing_blue;

	team_t stack_team = level.num_playing_red > level.num_playing_blue ? TEAM_RED : TEAM_BLUE;

	size_t	count = 0;
	int		index[MAX_CLIENTS_KEX/2];
	memset(index, 0, sizeof(index));

	// assemble list of client nums of everyone on stacked team
	for (auto ec : active_clients()) {
		if (ec->client->sess.team != stack_team)
			continue;
		index[count] = ec - g_entities;
		count++;
	}

	// sort client num list by join time
	qsort(index, count, sizeof(index[0]), PlayerSortByJoinTime);

	//run through sort list, switching from stack_team until teams are even
	if (count) {
		size_t	i;
		int switched = 0;
		gclient_t *cl = nullptr;
		for (i = 0; i < count, delta > 1; i++) {
			cl = &game.clients[index[i]];

			if (!cl)
				continue;

			if (!cl->pers.connected)
				continue;

			if (cl->sess.team != stack_team)
				continue;

			cl->sess.team = stack_team == TEAM_RED ? TEAM_BLUE : TEAM_RED;

			//TODO: queue this change in round-based games
			ClientRespawn(&g_entities[cl - game.clients + 1]);
			gi.LocClient_Print(&g_entities[cl - game.clients + 1], PRINT_CENTER, "You have changed teams to rebalance the game.\n");

			delta--;
			switched++;
		}

		if (switched) {
			gi.LocBroadcast_Print(PRINT_HIGH, "Teams have been balanced.\n");
			return switched;
		}
	}
	return 0;
}

/*
================
TeamShuffle

Randomly shuffles all players in teamplay
================
*/
bool TeamShuffle() {
	if (!Teams())
		return false;
	/*
	if (level.num_playing_clients < 3)
		return false;
		*/
	bool join_red = brandom();
	gentity_t *ent;
	int32_t index[MAX_CLIENTS_KEX] = { 0 };

	memset(index, -1, sizeof(index));

	// determine max team size based from active players
	int maxteam = ceil(level.num_playing_clients / 2);
	int count_red = 0, count_blue = 0;
	team_t setteam = join_red ? TEAM_RED : TEAM_BLUE;
	
	// create random array
	for (size_t i = 0; i < MAX_CLIENTS_KEX; i++) {
		if (index[i] >= 0)
			continue;

		int rnd = irandom(0, MAX_CLIENTS_KEX);
		while (index[rnd] >= 0)
			rnd = irandom(0, MAX_CLIENTS_KEX);

		index[i] = rnd;
		index[rnd] = i;
	}
#if 0
	for (size_t i = 0; i < MAX_CLIENTS_KEX; i++) {
		gi.Com_PrintFmt("{}={}\n", i, index[i]);
	}
#endif

	// set teams
	for (size_t i = 1; i <= MAX_CLIENTS_KEX; i++) {
		ent = &g_entities[index[i-1]];
		if (!ent)
			continue;
		if (!ent->inuse)
			continue;
		if (!ent->client)
			continue;
		if (!ent->client->pers.connected)
			continue;
		if (!ClientIsPlaying(ent->client))
			continue;

		if (count_red >= maxteam || count_red > count_blue)
			setteam = TEAM_BLUE;
		else if (count_blue >= maxteam || count_blue > count_red)
			setteam = TEAM_RED;
		
		ent->client->sess.team = setteam;

		if (setteam == TEAM_RED)
			count_red++;
		else count_blue++;

		join_red ^= true;
		setteam = join_red ? TEAM_RED : TEAM_BLUE;
	}

	return true;
}

/*
=================
StopFollowing

If the client being followed leaves the game, or you just want to drop
to free floating spectator mode
=================
*/
static void StopFollowing(gentity_t *ent, bool release) {
	gclient_t *client;

	if (ent->svflags & SVF_BOT || !ent->inuse)
		return;

	client = ent->client;

	client->sess.team = TEAM_SPECTATOR;
	client->sess.spectator_state = SPECTATOR_FREE;
	if (release) {
		client->ps.stats[STAT_HEALTH] = ent->health = 1;
		ent->client->ps.stats[STAT_SHOW_STATUSBAR] = 0;
	}
	//SetClientViewAngle(ent, client->ps.viewangles);

	//client->ps.pm_flags &= ~PMF_FOLLOW;
	ent->svflags &= SVF_BOT;

	//client->ps.clientnum = ent - g_entities;


	//-------------

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
}

/*
=================
SetTeam
=================
*/
bool SetTeam(gentity_t *ent, team_t desired_team, bool inactive, bool force, bool silent) {
	team_t old_team = ent->client->sess.team;
	bool queue = false;
	
	if (!force) {
		// Check if this would be a duel queue join (spectator with queue flag)
		bool would_be_duel_queue = GT(GT_DUEL) && desired_team != TEAM_SPECTATOR && level.num_playing_clients >= 2;
		
		if (!ClientIsPlaying(ent->client) && desired_team != TEAM_SPECTATOR) {
			bool revoke = false;
			// Check if the desired team is locked (covers both captain lock and g_match_lock)
			if (level.locked[desired_team] && !would_be_duel_queue) {
				gi.LocClient_Print(ent, PRINT_HIGH, "{} is locked.\n", Teams_TeamName(desired_team));
				revoke = true;
			} else if (level.num_playing_human_clients >= maxplayers->integer) {
				gi.LocClient_Print(ent, PRINT_HIGH, "Maximum player load reached.\n");
				revoke = true;
			}
			if (revoke) {
				P_Menu_Close(ent);
				return false;
			}
		}

		if (desired_team != TEAM_SPECTATOR && desired_team == ent->client->sess.team) {
			P_Menu_Close(ent);
			return false;
		}

		if (GT(GT_DUEL)) {
			if (desired_team != TEAM_SPECTATOR && level.num_playing_clients >= 2) {
				desired_team = TEAM_SPECTATOR;
				queue = true;
				P_Menu_Close(ent);
			}
		}

		if (!AllowTeamSwitch(ent, desired_team))
			return false;

		// Don't rate limit switching TO spectator - allow players to spectate immediately
		// Only rate limit switches between playing teams
		if (!inactive && desired_team != TEAM_SPECTATOR && ent->client->resp.team_delay_time > level.time) {
			gi.LocClient_Print(ent, PRINT_HIGH, "You may not switch teams more than once per 5 seconds.\n");
			P_Menu_Close(ent);
			return false;
		}
	} else {
		if (GT(GT_DUEL)) {
			if (desired_team == TEAM_NONE) {
				desired_team = TEAM_SPECTATOR;
				queue = true;
			}
		}
	}

	// allow the change...

	P_Menu_Close(ent);

	// vacate captain if leaving a team
	if ((old_team == TEAM_RED || old_team == TEAM_BLUE) && old_team != desired_team) {
		if (level.captain[old_team] == ent)
			VacateCaptain(old_team, ent);
	}

	// start as spectator
	if (ent->movetype == MOVETYPE_NOCLIP)
		Weapon_Grapple_DoReset(ent->client);

	CTF_DeadDropFlag(ent);
	Tech_DeadDrop(ent);

	FreeFollower(ent);

	ent->svflags &= ~SVF_NOCLIENT;
	ent->client->resp.score = 0;
	ent->client->sess.team = desired_team;
	if (desired_team == TEAM_SPECTATOR)
		ent->client->eliminated = false;
	ent->client->resp.ctf_state = 0;
	ent->client->sess.inactive = inactive;
	ent->client->sess.inactivity_time = level.time + 1_min;
	// If queued for duel, record when they joined the queue for proper ordering.
	// Otherwise, non-queued spectators get 0_sec, playing players get current time.
	if (desired_team == TEAM_SPECTATOR) {
		ent->client->sess.team_join_time = queue ? level.time : 0_sec;
	} else {
		ent->client->sess.team_join_time = level.time;
	}
	ent->client->resp.team_delay_time = force || !ent->client->sess.initialised ? level.time : level.time + 5_sec;
	ent->client->sess.spectator_state = desired_team == TEAM_SPECTATOR ? SPECTATOR_FREE : SPECTATOR_NOT;
	ent->client->sess.spectator_client = 0;
	ent->client->sess.duel_queued = queue;

	if (desired_team != TEAM_SPECTATOR) {
		if (Teams())
			G_AssignPlayerSkin(ent, ent->client->pers.skin);

		G_RevertVote(ent->client);

		// assign a ghost code
		Match_Ghost_DoAssign(ent);

		// free any followers
		FreeClientFollowers(ent);

		// auto-assign captain if team has none
		if ((desired_team == TEAM_RED || desired_team == TEAM_BLUE) && !level.captain[desired_team])
			SetCaptain(desired_team, ent);
	}

	ent->client->sess.initialised = true;

	// if they are playing a duel, count as a loss
	if (GT(GT_DUEL) && old_team == TEAM_FREE)
		ent->client->sess.losses++;

	ClientSpawn(ent);
	G_PostRespawn(ent);

	BroadcastTeamChange(ent, old_team, inactive, silent);

	ent->client->ps.stats[STAT_SHOW_STATUSBAR] = desired_team == TEAM_SPECTATOR || ent->client->eliminated ? 0 : 1;

	// if anybody has a menu open, update it immediately
	P_Menu_Dirty();

	return true;
}

/*
=================
Cmd_Team_f
=================
*/
static void Cmd_Team_f(gentity_t *ent) {
	if (gi.argc() == 1) {
		switch (ent->client->sess.team) {
		case TEAM_SPECTATOR:
			gi.LocClient_Print(ent, PRINT_HIGH, "You are spectating.\n");
			break;
		case TEAM_FREE:
			gi.LocClient_Print(ent, PRINT_HIGH, "You are in the match.\n");
			break;
		case TEAM_RED:
		case TEAM_BLUE:
			gi.LocClient_Print(ent, PRINT_HIGH, "Your team: {}\n", Teams_TeamName(ent->client->sess.team));
			break;
		default:
			break;
		}
		return;
	}

	const char *s = gi.argv(1);
	team_t team = StringToTeamNum(s);
	if (team == TEAM_NONE)
		return;

	SetTeam(ent, team, false, false, false);
}

/*
=================
Cmd_CrosshairID_f
=================
*/
static void Cmd_CrosshairID_f(gentity_t *ent) {
	ent->client->sess.pc.show_id ^= true;
	gi.LocClient_Print(ent, PRINT_HIGH, "Player identication display: {}\n", ent->client->sess.pc.show_id ? "ON" : "OFF");
}

/*
=================
Cmd_Timer_f
=================
*/
static void Cmd_Timer_f(gentity_t *ent) {
	ent->client->sess.pc.show_timer ^= true;
	gi.LocClient_Print(ent, PRINT_HIGH, "Match timer display: {}\n", ent->client->sess.pc.show_timer ? "ON" : "OFF");
}

/*
=================
Cmd_FragMessages_f
=================
*/
static void Cmd_FragMessages_f(gentity_t *ent) {
	ent->client->sess.pc.show_fragmessages ^= true;
	gi.LocClient_Print(ent, PRINT_HIGH, "{} frag messages.\n", ent->client->sess.pc.show_fragmessages ? "Activating" : "Disabling");
}

/*
=================
Cmd_Announcer_f
=================
*/
static void Cmd_Announcer_f(gentity_t *ent) {
	ent->client->sess.pc.use_expanded ^= true;
	gi.LocClient_Print(ent, PRINT_HIGH, "Match announcer: {}\n", ent->client->sess.pc.use_expanded ? "ON" : "OFF");
}

/*
=================
Cmd_KillBeep_f
=================
*/
static void Cmd_KillBeep_f(gentity_t *ent) {
	int num = 0;
	if (gi.argc() > 1) {
		num = atoi(gi.argv(1));
		if (num < 0)
			num = 0;
		else if (num > 4)
			num = 4;
	} else {
		num = (ent->client->sess.pc.killbeep_num + 1) % 5;
	}
	const char *sb[5] = { "off", "clang", "beep-boop", "insane", "tang-tang" };
	ent->client->sess.pc.killbeep_num = num;
	gi.LocClient_Print(ent, PRINT_HIGH, "Kill beep changed to: {}\n", sb[num]);
}


/*
=================
Cmd_Ghost_f
=================
*/
static void Cmd_Ghost_f(gentity_t *ent) {
	int i, n;

	if (gi.argc() < 2) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <code>\n", gi.argv(0));
		return;
	}

	if (ClientIsPlaying(ent->client)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You are already in the game.\n");
		return;
	}
	if (level.match_state != matchst_t::MATCH_IN_PROGRESS) {
		gi.LocClient_Print(ent, PRINT_HIGH, "No match is in progress.\n");
		return;
	}

	n = atoi(gi.argv(1));

	for (i = 0; i < MAX_CLIENTS_KEX; i++) {
		if (level.ghosts[i].code && level.ghosts[i].code == n) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Ghost code accepted, your position has been reinstated.\n");
			level.ghosts[i].ent->client->resp.ghost = nullptr;
			ent->client->sess.team = level.ghosts[i].team;
			ent->client->resp.ghost = level.ghosts + i;
			ent->client->resp.score = level.ghosts[i].score;
			ent->client->resp.ctf_state = 0;
			level.ghosts[i].ent = ent;
			ent->svflags = SVF_NONE;
			ent->flags &= ~FL_GODMODE;
			ClientSpawn(ent);
			gi.LocBroadcast_Print(PRINT_HIGH, "{} has been reinstated to {} team.\n",
				ent->client->resp.netname, Teams_TeamName(ent->client->sess.team));
			return;
		}
	}
	gi.LocClient_Print(ent, PRINT_HIGH, "Invalid ghost code.\n");
}


static void Cmd_Stats_f(gentity_t *ent) {
	if (!(GTF(GTF_CTF)))
		return;

	ghost_t *g;
	static std::string text;

	text.clear();

	if (level.match_state == matchst_t::MATCH_WARMUP_READYUP) {
		for (auto ec : active_clients()) {
			if (!ClientIsPlaying(ec->client))
				continue;
			if (ec->client->resp.ready)
				continue;

			std::string_view str = G_Fmt("{} is not ready.\n", ec->client->resp.netname);
			if (text.length() + str.length() < MAX_STRING_CHARS - 50)
				text += str;
		}
	}

	uint32_t i;
	for (i = 0, g = level.ghosts; i < MAX_CLIENTS_KEX; i++, g++)
		if (g->ent)
			break;

	if (i == MAX_CLIENTS_KEX) {
		if (!text.length())
			text = "No statistics available.\n";

		gi.Client_Print(ent, PRINT_HIGH, text.c_str());
		return;
	}

	text += "  #|Name            |Score|Kills|Death|BasDf|CarDf|Effcy|\n";

	for (i = 0, g = level.ghosts; i < MAX_CLIENTS_KEX; i++, g++) {
		if (!*g->netname)
			continue;

		int32_t e;

		if (g->deaths + g->kills == 0)
			e = 50;
		else
			e = g->kills * 100 / (g->kills + g->deaths);
		std::string_view str = G_Fmt("{:3}|{:<16.16}|{:5}|{:5}|{:5}|{:5}|{:5}|{:4}%|\n",
			g->number,
			g->netname,
			g->score,
			g->kills,
			g->deaths,
			g->basedef,
			g->carrierdef,
			e);

		if (text.length() + str.length() > MAX_STRING_CHARS - 50) {
			text += "And more...\n";
			break;
		}

		text += str;
	}

	gi.Client_Print(ent, PRINT_HIGH, text.c_str());
}

static void Cmd_Boot_f(gentity_t *ent) {
	if (gi.argc() < 2) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} [client name/num]\n", gi.argv(0));
		return;
	}

	if (*gi.argv(1) < '0' || *gi.argv(1) > '9') {
		gi.LocClient_Print(ent, PRINT_HIGH, "Specify the player number to kick.\n");
		return;
	}

	gentity_t *targ = ClientEntFromString(gi.argv(1));

	if (targ == nullptr) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid client number.\n");
		return;
	}

	if (targ == &g_entities[1]) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You cannot kick the lobby owner.\n");
		return;
	}
	
	if (targ->client->sess.admin) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You cannot kick an admin.\n");
		return;
	}

	gi.AddCommandString(G_Fmt("kick {}\n", targ - g_entities).data());
}

static void Cmd_Doctor_f(gentity_t *ent) {
	if (gi.argc() > 1) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {}\n", gi.argv(0));
		return;
	}

	int errors = 0;
	int warnings = 0;
	int infos = 0;

	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "\n[MuffMode Doctor] Running diagnostics...\n");

	auto report = [&](const char *severity, const char *problem, const char *fix) {
		if (!Q_strcasecmp(severity, "ERROR"))
			errors++;
		else if (!Q_strcasecmp(severity, "WARN"))
			warnings++;
		else
			infos++;

		gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "[{}] {}\n", severity, problem);
		if (fix && fix[0])
			gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "      Suggestion: {}\n", fix);
	};

	// Deterministic dependency checks
	if (g_dm_do_readyup->integer && !g_dm_do_warmup->integer) {
		report("ERROR",
			"g_dm_do_readyup is enabled while g_dm_do_warmup is disabled.",
			"set g_dm_do_warmup 1");
	}

	if (g_dm_do_readyup->integer && (g_warmup_ready_percentage->value <= 0.f || g_warmup_ready_percentage->value > 1.f)) {
		report("ERROR",
			"g_warmup_ready_percentage must be in range (0.0, 1.0] when readyup is enabled.",
			"set g_warmup_ready_percentage 0.51");
	}

	if (minplayers->integer > maxplayers->integer) {
		report("ERROR",
			"minplayers is greater than maxplayers.",
			"set minplayers <= maxplayers");
	}

	if (maxplayers->integer <= 0) {
		report("ERROR",
			"maxplayers must be greater than zero.",
			"set maxplayers 2");
	}

	if (g_vote_limit->integer < 0) {
		report("ERROR",
			"g_vote_limit cannot be negative.",
			"set g_vote_limit 0");
	}

	// Context and no-op checks
	if (!g_allow_voting->integer && g_allow_spec_vote->integer) {
		report("WARN",
			"g_allow_spec_vote is enabled while voting is globally disabled.",
			"set g_allow_voting 1 or set g_allow_spec_vote 0");
	}

	if (!g_allow_voting->integer && g_allow_vote_midgame->integer) {
		report("WARN",
			"g_allow_vote_midgame is enabled while voting is globally disabled.",
			"set g_allow_voting 1 or set g_allow_vote_midgame 0");
	}

	if (g_dm_do_readyup->integer && g_warmup_ready_percentage->value >= 0.99f) {
		report("WARN",
			"g_warmup_ready_percentage is very high; matches may stall waiting for nearly all players.",
			"set g_warmup_ready_percentage between 0.50 and 0.80");
	}

	if (g_dm_overtime->integer > 0 && (GT(GT_DUEL) == 0)) {
		report("INFO",
			"g_dm_overtime is set but currently only applies to Duel.",
			"Switch to Duel or leave as a preset for later.");
	}

	if ((GTF(GTF_ROUNDS) & GTF_ROUNDS) == 0) {
		if (roundlimit->integer != 8) {
			report("INFO",
				"roundlimit is non-default in a non-round gametype.",
				"No action needed unless this was unintended.");
		}

		if (roundtimelimit->integer != 2) {
			report("INFO",
				"roundtimelimit is non-default in a non-round gametype.",
				"No action needed unless this was unintended.");
		}

		if (g_round_countdown->integer != 10) {
			report("INFO",
				"g_round_countdown is non-default in a non-round gametype.",
				"No action needed unless this was unintended.");
		}
	} else {
		if (roundlimit->integer <= 0) {
			report("WARN",
				"roundlimit is <= 0 in a round-based gametype.",
				"set roundlimit 8");
		}

		if (roundtimelimit->value <= 0.f) {
			report("WARN",
				"roundtimelimit is <= 0 in a round-based gametype.",
				"set roundtimelimit 2");
		}

		if (g_round_countdown->integer < 0) {
			report("WARN",
				"g_round_countdown is negative.",
				"set g_round_countdown 10");
		}
	}

	if (!g_allow_voting->integer && (g_vote_flags->integer != 0 || g_vote_limit->integer != 3)) {
		report("INFO",
			"Vote restriction cvars are customized while voting is disabled.",
			"No action needed unless you expected votes to be active.");
	}

	if (!errors && !warnings && !infos)
		gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "[OK] No issues found.\n");

	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY,
		"[MuffMode Doctor] Summary: {} error(s), {} warning(s), {} info message(s).\n\n",
		errors, warnings, infos);
}

/*----------------------------------------------------------------*/

// NEW VOTING CODE

static bool Vote_Val_None(gentity_t *ent) {
	return true;
}

// Validation helpers so menu votes can run the same val_func logic as cmd votes.
// In menu context, val_func calls that read gi.argc/gi.argv are fed from the
// selected menu command/arg pair.
static bool s_menu_vote_validation_active = false;
static vcmds_t *s_menu_vote_validation_cmd = nullptr;
static std::string s_menu_vote_validation_arg;

static inline int VoteVal_Argc() {
	if (!s_menu_vote_validation_active)
		return gi.argc();
	return s_menu_vote_validation_arg.empty() ? 2 : 3;
}

static inline const char *VoteVal_Argv(int index) {
	if (!s_menu_vote_validation_active)
		return gi.argv(index);
	if (index == 0)
		return "callvote";
	if (index == 1)
		return (s_menu_vote_validation_cmd && s_menu_vote_validation_cmd->name) ? s_menu_vote_validation_cmd->name : "";
	if (index == 2)
		return s_menu_vote_validation_arg.c_str();
	return "";
}

bool ValidateMenuVoteCommand(gentity_t *ent, vcmds_t *cc, const char *arg) {
	if (!ent || !ent->client || !cc)
		return false;

	const char *menu_arg = arg ? arg : "";
	const int provided_argc = menu_arg[0] ? 3 : 2;

	if (cc->args && provided_argc < (1 + cc->min_args)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "{}: {}\nUsage: {} {}\n", cc->name, cc->help, cc->name, cc->args);
		return false;
	}

	s_menu_vote_validation_active = true;
	s_menu_vote_validation_cmd = cc;
	s_menu_vote_validation_arg = menu_arg;
	const bool ok = cc->val_func(ent);
	s_menu_vote_validation_active = false;
	s_menu_vote_validation_cmd = nullptr;
	s_menu_vote_validation_arg.clear();
	return ok;
}

void Vote_Pass_Map() {
	MuffModeLog("DEBUG", "Vote_Pass_Map: enter, arg='%s' (len=%d, ptr=%p)",
	           level.vote_state.arg.c_str(), (int)level.vote_state.arg.length(),
	           (void*)level.vote_state.arg.c_str());

	if (level.vote_state.arg.empty() || level.vote_state.arg.length() >= sizeof(level.nextmap)) {
		gi.LocBroadcast_Print(PRINT_HIGH, "Map vote failed: invalid map name.\n");
		return;
	}

	Q_strlcpy(level.nextmap, level.vote_state.arg.c_str(), sizeof(level.nextmap));
	MuffModeLog("DEBUG", "Vote_Pass_Map: queuing gamemap for '%s'", level.nextmap);
	gi.AddCommandString(G_Fmt("gamemap \"{}\"\n", level.nextmap).data());
}

// Helper function to validate a map name using two-tier system:
// First checks g_map_pool (all available maps), then falls back to g_map_list (rotation maps)
static bool IsMapValid(const char *mapname) {
	if (!mapname || !mapname[0])
		return false;

	char *token;

	// First check g_map_pool if it exists and is non-empty
	if (g_map_pool->string[0]) {
		const char *pool = g_map_pool->string;

		while ((token = COM_Parse(&pool)) && *token) {
			if (!Q_strcasecmp(token, mapname))
				return true;
		}
	}

	// Fall back to g_map_list if pool didn't have it (or pool was empty)
	if (g_map_list->string[0]) {
		const char *mlist = g_map_list->string;

		while ((token = COM_Parse(&mlist)) && *token) {
			if (!Q_strcasecmp(token, mapname))
				return true;
		}
	}

	return false;
}

static void PrintAvailableMaps(gentity_t *ent) {
	std::vector<std::string> all_maps;
	char *token;

	auto map_exists = [&all_maps](const char *map) -> bool {
		for (const auto &existing : all_maps)
			if (!Q_strcasecmp(existing.c_str(), map))
				return true;
		return false;
	};

	if (g_map_pool->string[0]) {
		const char *pool = g_map_pool->string;
		while ((token = COM_Parse(&pool)) && *token)
			if (!map_exists(token))
				all_maps.push_back(token);
	}
	if (g_map_list->string[0]) {
		const char *mlist = g_map_list->string;
		while ((token = COM_Parse(&mlist)) && *token)
			if (!map_exists(token))
				all_maps.push_back(token);
	}

	if (all_maps.empty()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "No map list or pool configured.\n");
		return;
	}

	std::sort(all_maps.begin(), all_maps.end(), [](const std::string &a, const std::string &b) {
		return Q_strcasecmp(a.c_str(), b.c_str()) < 0;
	});

	std::string display = join_strings(all_maps, " ");
	if (display.length() > 256)
		display = display.substr(0, 256) + "...";
	gi.LocClient_Print(ent, PRINT_HIGH, "Valid maps are: {}\n", display.c_str());
}

static bool Vote_Val_Map(gentity_t *ent) {
	if (VoteVal_Argc() < 3 || !VoteVal_Argv(2)[0]) {
		PrintAvailableMaps(ent);
		return false;
	}

	if (!IsMapValid(VoteVal_Argv(2))) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Unknown map.\n");
		PrintAvailableMaps(ent);
		return false;
	}

	return true;
}

void Vote_Pass_RestartMatch() {
	Match_Reset();
}

void Vote_Pass_Gametype() {
	MM_VotePassGametype();
}

// Helper function to check if a gametype is votable
bool IsGametypeVotable(gametype_t gt) {
	return MM_IsGametypeVotable(gt);
}

// Helper function to build list of votable gametypes for help text
static std::string GetVotableGametypesList() {
	return MM_GetVotableGametypesList();
}

// Helper function to check if a ruleset is votable
bool IsRulesetVotable(ruleset_t rs) {
	return MM_IsRulesetVotable(rs);
}

// Helper function to build list of votable rulesets for help text
static std::string GetVotableRulesetsList() {
	return MM_GetVotableRulesetsList();
}

static bool Vote_Val_Gametype(gentity_t *ent) {
	// Ensure exactly 3 arguments: callvote, gametype, <gametype_name>
	if (VoteVal_Argc() != 3) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: callvote gametype <gametype_name>\n");
		std::string votable_list = GetVotableGametypesList();
		if (!votable_list.empty()) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Valid gametypes are: {}\n", votable_list.c_str());
		}
		return false;
	}
	
	gametype_t gt = GT_IndexFromString(VoteVal_Argv(2));
	
	if (gt == GT_NONE) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid gametype: '{}'\n", VoteVal_Argv(2));
		std::string votable_list = GetVotableGametypesList();
		if (!votable_list.empty()) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Valid gametypes are: {}\n", votable_list.c_str());
		}
		return false;
	}

	// Check if gametype is votable
	if (!IsGametypeVotable(gt)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "This gametype is not available for voting.\n");
		std::string votable_list = GetVotableGametypesList();
		if (!votable_list.empty()) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Valid gametypes are: {}\n", votable_list.c_str());
		}
		return false;
	}

	return true;
}

static void Vote_Pass_Ruleset() {
	MM_VotePassRuleset();
}

static bool Vote_Val_Ruleset(gentity_t *ent) {
	ruleset_t desired_rs = RS_IndexFromString(VoteVal_Argv(2));
	if (desired_rs == ruleset_t::RS_NONE) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid ruleset: '{}'\n", VoteVal_Argv(2));
		std::string votable_list = GetVotableRulesetsList();
		if (!votable_list.empty()) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Valid rulesets are: {}\n", votable_list.c_str());
		}
		return false;
	}
	if ((int)desired_rs == game.ruleset) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Ruleset currently active.\n");
		return false;
	}

	// Check if ruleset is votable
	if (!IsRulesetVotable(desired_rs)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "This ruleset is not available for voting.\n");
		std::string votable_list = GetVotableRulesetsList();
		if (!votable_list.empty()) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Valid rulesets are: {}\n", votable_list.c_str());
		}
		return false;
	}

	return true;
}

void Vote_Pass_NextMap() {
	Match_End();
	level.intermission_exit = true;
}

void Vote_Pass_ShuffleTeams() {
	if (!Teams()) {
		gi.LocBroadcast_Print(PRINT_HIGH, "Shuffle vote failed: not a team gametype.\n");
		return;
	}
	TeamShuffle();
	Match_Reset();
	gi.LocBroadcast_Print(PRINT_HIGH, "Teams have been shuffled.\n");
}

static bool Vote_Val_ShuffleTeams(gentity_t *ent) {
	if (!Teams()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Shuffle teams is only available in team gametypes.\n");
		return false;
	}
	return true;
}

static void Vote_Pass_Unlagged() {
	int argi = strtoul(level.vote_state.arg.data(), nullptr, 10);
	
	gi.LocBroadcast_Print(PRINT_HIGH, "Lag compensation has been {}.\n", argi ? "ENABLED" : "DISABLED");

	gi.cvar_forceset("g_lag_compensation", argi ? "1" : "0");
}

static bool Vote_Val_Unlagged(gentity_t *ent) {
	int arg = strtoul(VoteVal_Argv(2), nullptr, 10);
	
	if ((g_lag_compensation->integer && arg)
			|| (!g_lag_compensation->integer && !arg)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Lag compensation is already {}.\n", arg ? "ENABLED" : "DISABLED");
		return false;
	}

	return true;
}

static bool Vote_Val_Random(gentity_t *ent) {
	int arg = strtoul(VoteVal_Argv(2), nullptr, 10);

	if (arg > 100 || arg < 2)
		return false;

	return true;
}

void Vote_Pass_Cointoss() {
	gi.LocBroadcast_Print(PRINT_HIGH, "The coin is: {}\n", brandom() ? "HEADS" : "TAILS");
}

void Vote_Pass_Random() {
	gi.LocBroadcast_Print(PRINT_HIGH, "The random number is: {}\n", irandom(2, atoi(level.vote_state.arg.data())));
}

void Vote_Pass_Timelimit() {
	const char *s = level.vote_state.arg.data();
	int argi = strtoul(s, nullptr, 10);
	
	if (!argi)
		gi.LocBroadcast_Print(PRINT_HIGH, "Time limit has been DISABLED.\n");
	else
		gi.LocBroadcast_Print(PRINT_HIGH, "Time limit has been set to {}.\n", G_TimeString(argi * 60000, false));

	gi.cvar_forceset("timelimit", s);
}

static bool Vote_Val_Timelimit(gentity_t *ent) {
	int argi = strtoul(VoteVal_Argv(2), nullptr, 10);
	
	if (argi < 0 || argi > 1440) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid time limit value.\n");
		return false;
	}
	
	if (argi == timelimit->integer) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Time limit is already set to {}.\n", G_TimeString(argi * 60000, false));
		return false;
	}
	return true;
}

void Vote_Pass_Scorelimit() {
	int argi = strtoul(level.vote_state.arg.data(), nullptr, 10);
	
	if (argi)
		gi.LocBroadcast_Print(PRINT_HIGH, "Score limit has been set to {}.\n", argi);
	else
		gi.LocBroadcast_Print(PRINT_HIGH, "Score limit has been DISABLED.\n");

	gi.cvar_forceset(G_Fmt("{}limit", GT_ScoreLimitString()).data(), level.vote_state.arg.data());
}

static bool Vote_Val_Scorelimit(gentity_t *ent) {
	int argi = strtoul(VoteVal_Argv(2), nullptr, 10);
	
	if (argi < 0) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid score limit value.\n");
		return false;
	}

	if (argi == GT_ScoreLimit()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Score limit is already set to {}.\n", argi);
		return false;
	}

	return true;
}

static void Vote_Pass_BalanceTeams() {
	TeamBalance(true);
}

void Vote_Pass_ReadyAll() {
	if (!g_dm_do_readyup->integer || level.match_state != matchst_t::MATCH_WARMUP_READYUP) {
		gi.LocBroadcast_Print(PRINT_HIGH, "Ready all vote failed: not in ready-up warmup.\n");
		return;
	}
	ReadyAll();
	gi.LocBroadcast_Print(PRINT_HIGH, "All players have been readied.\n");
}

static bool Vote_Val_ReadyAll(gentity_t *ent) {
	if (!g_dm_do_readyup->integer || level.match_state != matchst_t::MATCH_WARMUP_READYUP) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Ready all is only available during ready-up warmup.\n");
		return false;
	}
	return true;
}

static bool Vote_Val_BalanceTeams(gentity_t *ent) {
	if (!Teams())
		return false;

	return true;
}

static void Vote_Pass_Powerups() {
	int argi = strtoul(level.vote_state.arg.data(), nullptr, 10);
	
	gi.LocBroadcast_Print(PRINT_HIGH, "Powerups have been {}.\n", argi ? "ENABLED" : "DISABLED");

	gi.cvar_forceset("g_no_powerups", argi ? "0" : "1");
	
	// Restart the map so powerup changes take effect immediately
	gi.AddCommandString(G_Fmt("gamemap {}\n", level.mapname).data());
}

static void Vote_Pass_FriendlyFire() {
	int argi = strtoul(level.vote_state.arg.data(), nullptr, 10);
	
	gi.LocBroadcast_Print(PRINT_HIGH, "Friendly fire has been {}.\n", argi ? "ENABLED" : "DISABLED");

	gi.cvar_forceset("g_friendly_fire", argi ? "1" : "0");
	
	// Friendly fire changes take effect immediately, no map restart needed
}

static bool Vote_Val_FriendlyFire(gentity_t *ent) {
	if (notGT(GT_TDM) && notGT(GT_CTF)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Friendly fire can only be changed in TDM or CTF gametypes.\n");
		return false;
	}

	int arg = strtoul(VoteVal_Argv(2), nullptr, 10);
	
	if (arg != 0 && arg != 1) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid argument. Use 0 to disable or 1 to enable friendly fire.\n");
		return false;
	}
	
	bool currently_enabled = g_friendly_fire->integer != 0;
	bool will_be_enabled = (arg == 1);
	
	if (currently_enabled == will_be_enabled) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Friendly fire is already {}.\n", will_be_enabled ? "ENABLED" : "DISABLED");
		return false;
	}

	return true;
}

static bool Vote_Val_Powerups(gentity_t *ent) {
	int arg = strtoul(VoteVal_Argv(2), nullptr, 10);
	
	if (arg != 0 && arg != 1) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid argument. Use 0 to disable or 1 to enable powerups.\n");
		return false;
	}
	
	bool currently_disabled = g_no_powerups->integer != 0;
	bool will_be_disabled = (arg == 0);
	
	if (currently_disabled == will_be_disabled) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Powerups are already {}.\n", will_be_disabled ? "DISABLED" : "ENABLED");
		return false;
	}

	return true;
}

// Forward declarations for handicap helper functions
static int Handicap_ClientNumberFromName(gentity_t *to, const char *name);
static item_id_t Handicap_WeaponIDFromName(const char *name);
static void Handicap_ApplyWeaponRestriction(gentity_t *target, item_id_t weapon_id, bool restrict);

static bool Vote_Val_Handicap(gentity_t *ent) {
	// Must be in duel mode
	if (notGT(GT_DUEL)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Handicap system only works in duel mode.\n");
		return false;
	}

	// Check argument count
	if (VoteVal_Argc() < 5) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: callvote handicap <player> <weapon> <on|off>\n");
		gi.LocClient_Print(ent, PRINT_HIGH, "Weapons: railgun, chaingun, rlauncher, all\n");
		return false;
	}

	const char *player_name = VoteVal_Argv(2);
	const char *weapon_name = VoteVal_Argv(3);
	const char *onoff = VoteVal_Argv(4);

	// Find target player
	int clientnum = Handicap_ClientNumberFromName(ent, player_name);
	if (clientnum < 0) {
		return false;
	}

	gentity_t *target = &g_entities[1 + clientnum];
	if (!target->inuse || !target->client || !ClientIsPlaying(target->client)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Player '{}' is not playing.\n", player_name);
		return false;
	}

	// Validate weapon
	item_id_t weapon_id = Handicap_WeaponIDFromName(weapon_name);
	if (weapon_id == IT_NULL && Q_strcasecmp(weapon_name, "all")) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid weapon '{}'. Valid: railgun, chaingun, rlauncher, all\n", weapon_name);
		return false;
	}

	// Validate on/off
	bool restrict = false;
	if (!Q_strcasecmp(onoff, "on") || !Q_strcasecmp(onoff, "1")) {
		restrict = true;
	} else if (!Q_strcasecmp(onoff, "off") || !Q_strcasecmp(onoff, "0")) {
		restrict = false;
	} else {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid value '{}'. Use 'on' or 'off'.\n", onoff);
		return false;
	}

	// Check if restriction would change anything
	if (weapon_id == IT_NULL) {
		// "all" case
		constexpr uint32_t HANDICAP_ALL_WEAPONS = 
			(1U << (IT_WEAPON_RAILGUN - FIRST_WEAPON)) |
			(1U << (IT_WEAPON_CHAINGUN - FIRST_WEAPON)) |
			(1U << (IT_WEAPON_RLAUNCHER - FIRST_WEAPON));
		
		bool currently_restricted = (target->client->handicap.restricted_weapons & HANDICAP_ALL_WEAPONS) == HANDICAP_ALL_WEAPONS;
		if (currently_restricted == restrict) {
			gi.LocClient_Print(ent, PRINT_HIGH, "All handicap weapons are already {} for {}.\n", 
				restrict ? "restricted" : "unrestricted", target->client->resp.netname);
			return false;
		}
	} else {
		uint32_t weapon_bit = 1U << (weapon_id - FIRST_WEAPON);
		bool currently_restricted = (target->client->handicap.restricted_weapons & weapon_bit) != 0;
		if (currently_restricted == restrict) {
			gitem_t *weapon_item = GetItemByIndex(weapon_id);
			gi.LocClient_Print(ent, PRINT_HIGH, "{} is already {} for {}.\n", 
				weapon_item ? weapon_item->pickup_name : weapon_name,
				restrict ? "restricted" : "unrestricted", 
				target->client->resp.netname);
			return false;
		}
	}

	return true;
}

static void Vote_Pass_Handicap() {
	// Parse the stored argument string
	// Format: "playername weaponname on|off" or "player name" weaponname on|off
	std::string arg = level.vote_state.arg;
	
	std::string player_name, weapon_name, onoff;
	
	// Handle quoted player names
	if (arg.length() > 0 && arg[0] == '"') {
		// Find closing quote
		size_t quote_end = arg.find('"', 1);
		if (quote_end == std::string::npos) {
			gi.LocBroadcast_Print(PRINT_HIGH, "Handicap vote failed: invalid argument format (unclosed quote).\n");
			return;
		}
		
		// Extract player name (without quotes)
		player_name = arg.substr(1, quote_end - 1);
		
		// Find next space after closing quote
		size_t space_after_quote = arg.find(' ', quote_end + 1);
		if (space_after_quote == std::string::npos) {
			gi.LocBroadcast_Print(PRINT_HIGH, "Handicap vote failed: invalid argument format.\n");
			return;
		}
		
		// Find space after weapon name
		size_t space_after_weapon = arg.find(' ', space_after_quote + 1);
		if (space_after_weapon == std::string::npos) {
			gi.LocBroadcast_Print(PRINT_HIGH, "Handicap vote failed: invalid argument format.\n");
			return;
		}
		
		weapon_name = arg.substr(space_after_quote + 1, space_after_weapon - space_after_quote - 1);
		onoff = arg.substr(space_after_weapon + 1);
	} else {
		// No quotes - split by spaces normally
		size_t space1 = arg.find(' ');
		if (space1 == std::string::npos) {
			gi.LocBroadcast_Print(PRINT_HIGH, "Handicap vote failed: invalid argument format.\n");
			return;
		}
		
		size_t space2 = arg.find(' ', space1 + 1);
		if (space2 == std::string::npos) {
			gi.LocBroadcast_Print(PRINT_HIGH, "Handicap vote failed: invalid argument format.\n");
			return;
		}
		
		player_name = arg.substr(0, space1);
		weapon_name = arg.substr(space1 + 1, space2 - space1 - 1);
		onoff = arg.substr(space2 + 1);
	}

	// Find target player (use a dummy entity for the helper function)
	gentity_t *dummy_ent = nullptr;
	for (auto ec : active_clients()) {
		dummy_ent = ec;
		break;
	}
	if (!dummy_ent) {
		gi.LocBroadcast_Print(PRINT_HIGH, "Handicap vote failed: no players found.\n");
		return;
	}

	int clientnum = Handicap_ClientNumberFromName(dummy_ent, player_name.c_str());
	if (clientnum < 0) {
		gi.LocBroadcast_Print(PRINT_HIGH, "Handicap vote failed: player not found.\n");
		return;
	}

	gentity_t *target = &g_entities[1 + clientnum];
	if (!target->inuse || !target->client) {
		gi.LocBroadcast_Print(PRINT_HIGH, "Handicap vote failed: player not active.\n");
		return;
	}

	item_id_t weapon_id = Handicap_WeaponIDFromName(weapon_name.c_str());
	bool restrict = (!Q_strcasecmp(onoff.c_str(), "on") || !Q_strcasecmp(onoff.c_str(), "1"));

	// Apply restriction
	Handicap_ApplyWeaponRestriction(target, weapon_id, restrict);

	// Broadcast result
	const char *weapon_display = weapon_name.c_str();
	if (weapon_id != IT_NULL) {
		gitem_t *weapon_item = GetItemByIndex(weapon_id);
		if (weapon_item)
			weapon_display = weapon_item->pickup_name;
	} else {
		weapon_display = "all handicap weapons (railgun, chaingun, rlauncher)";
	}

	gi.LocBroadcast_Print(PRINT_HIGH, "[VOTE]: {} {} for {}.\n", 
		weapon_display, restrict ? "restricted" : "unrestricted", 
		target->client->resp.netname);
}

vcmds_t vote_cmds[] = {
	{"map",					Vote_Val_Map,			Vote_Pass_Map,			1,		2,	"[mapname]",						"changes to the specified map"},
	{"nextmap",				Vote_Val_None,			Vote_Pass_NextMap,		2,		1,	"",									"move to the next map in the rotation"},
	{"restart",				Vote_Val_None,			Vote_Pass_RestartMatch,	4,		1,	"",									"restarts the current match"},
	{"gametype",			Vote_Val_Gametype,		Vote_Pass_Gametype,		8,		2,	"<gametype>",	"changes the current gametype"},
	{"timelimit",			Vote_Val_Timelimit,		Vote_Pass_Timelimit,	16,		2,	"<0..$>",							"alters the match time limit, 0 for no time limit"},
	{"scorelimit",			Vote_Val_Scorelimit,	Vote_Pass_Scorelimit,	32,		2,	"<0..$>",							"alters the match score limit, 0 for no score limit"},
	{"fraglimit",			Vote_Val_Scorelimit,	Vote_Pass_Scorelimit,	32,		2,	"<0..$>",							"alters the match score limit, 0 for no score limit (alias for scorelimit)"},
	{"shuffle",				Vote_Val_ShuffleTeams,	Vote_Pass_ShuffleTeams,	64,		2,	"",									"shuffles teams"},
	{"unlagged",			Vote_Val_Unlagged,		Vote_Pass_Unlagged,		128,	2,	"<0/1>",							"enables or disables lag compensation"},
	{"cointoss",			Vote_Val_None,			Vote_Pass_Cointoss,		256,	1,	"",									"invokes a HEADS or TAILS cointoss"},
	{"random",				Vote_Val_Random,		Vote_Pass_Random,		512,	1,	"<2-100>",							"randomly selects a number from 2 to specified value"},
	{"balance",				Vote_Val_BalanceTeams,	Vote_Pass_BalanceTeams,	1024,	1,	"",									"balance teams without shuffling"},
	{"ruleset",				Vote_Val_Ruleset,		Vote_Pass_Ruleset,		2048,	2,	"<q2re|mm|q3a|q2reb|qc>",					"changes the current ruleset"},
	{"powerups",				Vote_Val_Powerups,		Vote_Pass_Powerups,		4096,	2,	"<0/1>",							"enables or disables powerups"},
	{"friendlyfire",			Vote_Val_FriendlyFire,	Vote_Pass_FriendlyFire,	8192,	2,	"<0/1>",							"enables or disables friendly fire (team modes only)"},
	{"handicap",				Vote_Val_Handicap,		Vote_Pass_Handicap,		16384,	4,	"<player> <weapon> <on|off>",		"restricts weapons for a player in duel mode"},
	{"readyall",				Vote_Val_ReadyAll,		Vote_Pass_ReadyAll,		32768,	1,	"",									"ready all players (during ready-up warmup)"},
	{nullptr,					nullptr,				nullptr,				0,		0,	nullptr,								nullptr},
};

/*
===============
FindVoteCmdByName

===============
*/
vcmds_t *FindVoteCmdByName(const char *name) {
	vcmds_t *cc = vote_cmds;

	for (size_t i = 0; i < ARRAY_LEN(vote_cmds); i++, cc++) {
		if (!cc->name)
			continue;
		if (!Q_strcasecmp(cc->name, name))
			return cc;
	}

	return nullptr;
}

/*
===============
TransitionVoteState

Thin vanilla hook for MuffMode vote state machine
===============
*/
void TransitionVoteState(VoteState new_state) {
	MM_TransitionVoteState(new_state);
}

/*
===============
ClearVote

Convenience function to clear vote state
===============
*/
void ClearVote() {
	MM_ClearVote();
}

/*
==================
Vote_Passed
==================
*/
void Vote_Passed() {
	MM_VotePassed();
}

/*
=================
ValidVoteCommand
=================
*/
static bool ValidVoteCommand(gentity_t *ent) {
	return MM_ValidVoteCommand(ent);
}

/*
=================
VoteCommandStore
=================
*/
void VoteCommandStore(gentity_t *ent) {
	MM_VoteCommandStore(ent);
}

/*
==================
Cmd_CallVote_f
==================
*/
static void Cmd_CallVote_f(gentity_t *ent) {
	MM_CmdCallVote(ent);
}

/*
==================
Cmd_Vote_f
==================
*/
static void Cmd_Vote_f(gentity_t *ent) {
	if (!deathmatch->integer)
		return;

	if (!ClientCanVote(ent->client)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Not allowed to vote as spectator.\n");
		return;
	}
	
	if (gi.argc() < 2) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} [yes/no]\nCasts your vote in current voting session.\n", gi.argv(0));
		return;
	}

	if (level.vote_state.state != VoteState::ACTIVE) {
		gi.LocClient_Print(ent, PRINT_HIGH, "No vote in progress.\n");
		return;
	}

	if (ent->client->pers.voted != 0) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Vote already cast.\n");
		return;
	}

	const char *arg = gi.argv(1);

	if (arg[0] == 'y' || arg[0] == 'Y' || arg[0] == '1') {
		ent->client->pers.voted = 1;
	} else {
		ent->client->pers.voted = -1;
	}

	gi.LocClient_Print(ent, PRINT_HIGH, "Vote cast.\n");
	
	// a majority will be determined in CheckVote, which will also account
	// for players entering or leaving
}

void G_RevertVote(gclient_t *client) {
	if (level.vote_state.state != VoteState::ACTIVE)
		return;

	if (!level.vote_state.caller)
		return;

	if (client->pers.voted != 0) {
		client->pers.voted = 0;
	}
}

/*
=================
Cmd_Follow_f
=================
*/
static void Cmd_Follow_f(gentity_t *ent) {
	if (ClientIsPlaying(ent->client)) {
		gi.Client_Print(ent, PRINT_HIGH, "You must spectate before you can follow.\n");
		return;
	}
	if (gi.argc() < 2) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} [client name/num]\nFollows the specified player.", gi.argv(0));
		return;
	}

	gentity_t *follow_ent = ClientEntFromString(gi.argv(1));

	if (!follow_ent || !follow_ent->inuse) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid client specified.\n");
		return;
	}

	if (!ClientIsPlaying(follow_ent->client)) {
		gi.Client_Print(ent, PRINT_HIGH, "Specified client is not playing.\n");
		return;
	}

	ent->client->follow_target = follow_ent;
	ent->client->follow_update = true;
	UpdateChaseCam(ent);
}

/*
=================
Cmd_FollowKiller_f
=================
*/
static void Cmd_FollowKiller_f(gentity_t *ent) {
	ent->client->sess.pc.follow_killer ^= true;
	gi.LocClient_Print(ent, PRINT_HIGH, "Auto-follow killer: {}\n", ent->client->sess.pc.follow_killer ? "ON" : "OFF");
}

/*
=================
Cmd_FollowLeader_f
=================
*/
static void Cmd_FollowLeader_f(gentity_t *ent) {
	ent->client->sess.pc.follow_leader ^= true;

	if (ent->client->sess.pc.follow_leader) {
		if (!level.num_playing_clients || level.sorted_clients[0] < 0) {
			ent->client->sess.pc.follow_leader = false;
			gi.Client_Print(ent, PRINT_HIGH, "No leader available to follow.\n");
			gi.LocClient_Print(ent, PRINT_HIGH, "Auto-follow leader: OFF\n");
			return;
		}
	}

	gentity_t *leader = &g_entities[level.sorted_clients[0] + 1];
	gi.LocClient_Print(ent, PRINT_HIGH, "Auto-follow leader: {}\n", ent->client->sess.pc.follow_leader ? "ON" : "OFF");

	if (!ClientIsPlaying(ent->client) && ent->client->sess.pc.follow_leader && ent->client->follow_target != leader) {
		ent->client->follow_target = leader;
		ent->client->follow_update = true;
		UpdateChaseCam(ent);
	}
}

/*
=================
Cmd_FollowPowerup_f
=================
*/
static void Cmd_FollowPowerup_f(gentity_t *ent) {
	ent->client->sess.pc.follow_powerup ^= true;
	gi.LocClient_Print(ent, PRINT_HIGH, "Auto-follow powerup pick-ups: {}\n", ent->client->sess.pc.follow_powerup ? "ON" : "OFF");
}

/*----------------------------------------------------------------*/

/*
=================
SetCaptain

Sets ent as captain of team. Pass nullptr to remove captain.
=================
*/
void SetCaptain(team_t team, gentity_t *ent) {
	level.captain[team] = ent;

	if (ent) {
		gi.LocBroadcast_Print(PRINT_HIGH, "{} became captain of {}.\n",
			ent->client->resp.netname, Teams_TeamName(team));
	}
}

/*
=================
FindNewCaptain

Finds the longest-tenured teammate to auto-promote as captain.
Returns nullptr if no eligible player found.
=================
*/
static gentity_t *FindNewCaptain(team_t team, gentity_t *exclude = nullptr) {
	gentity_t *best = nullptr;
	gtime_t earliest = {};

	for (auto ec : active_clients()) {
		if (ec == exclude)
			continue;
		if (ec->client->sess.team != team)
			continue;
		if (ec->svflags & SVF_BOT)
			continue;
		if (!best || ec->client->sess.team_join_time < earliest) {
			best = ec;
			earliest = ec->client->sess.team_join_time;
		}
	}

	return best;
}

/*
=================
VacateCaptain

Called when a captain leaves their team. Auto-promotes the
longest-tenured teammate, or clears captain if team is empty.
=================
*/
void VacateCaptain(team_t team, gentity_t *leaving) {
	level.captain[team] = nullptr;

	gentity_t *replacement = FindNewCaptain(team, leaving);
	if (replacement)
		SetCaptain(team, replacement);
}

/*
=================
ValidateCaptains

Called after match start / reset to ensure captain pointers
are still valid. Keeps existing captains if valid, otherwise
auto-promotes the longest-tenured teammate.
=================
*/
void ValidateCaptains() {
	if (!Teams())
		return;

	for (team_t t : { TEAM_RED, TEAM_BLUE }) {
		gentity_t *cap = level.captain[t];
		if (cap && cap->inuse && cap->client && cap->client->pers.connected && cap->client->sess.team == t)
			continue; // captain is still valid
		level.captain[t] = nullptr;
		gentity_t *replacement = FindNewCaptain(t);
		if (replacement)
			SetCaptain(t, replacement);
	}
}

/*
=================
IsCaptainOrAdmin

Returns true if ent is captain of the given team, or is an admin.
=================
*/
static bool IsCaptainOrAdmin(gentity_t *ent, team_t team) {
	if (ent->client->sess.admin)
		return true;
	if (level.captain[team] == ent)
		return true;
	return false;
}

/*
=================
Cmd_Captain_f

Usage:
  captain          - claim captain (if none) or show current captain
  captain <player> - transfer captain to a teammate (must be captain)
=================
*/
static void Cmd_Captain_f(gentity_t *ent) {
	if (!Teams()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Captain is only available in team modes.\n");
		return;
	}

	team_t team = ent->client->sess.team;

	if (team != TEAM_RED && team != TEAM_BLUE) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You must be on a team to use this command.\n");
		return;
	}

	if (gi.argc() == 1) {
		if (level.captain[team] == ent) {
			gi.LocClient_Print(ent, PRINT_HIGH, "You are the captain of {}.\n", Teams_TeamName(team));
		} else if (level.captain[team]) {
			gi.LocClient_Print(ent, PRINT_HIGH, "{} is the captain of {}.\n",
				level.captain[team]->client->resp.netname, Teams_TeamName(team));
		} else {
			SetCaptain(team, ent);
		}
		return;
	}

	// transfer captain to another player
	if (level.captain[team] != ent) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You must be captain to transfer it to another player.\n");
		return;
	}

	const char *args = gi.args();
	size_t args_len = strlen(args);
	char name_buf[MAX_NETNAME];
	if (args_len >= 2 && args[0] == '"' && args[args_len - 1] == '"') {
		size_t copy_len = std::min(args_len - 2, sizeof(name_buf) - 1);
		memcpy(name_buf, args + 1, copy_len);
		name_buf[copy_len] = '\0';
		args = name_buf;
	}

	gentity_t *target = ClientEntFromString(args);

	if (!target || !target->inuse || !target->client) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid player.\n");
		return;
	}

	if (target == ent) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You can't transfer captain to yourself.\n");
		return;
	}

	if (target->client->sess.team != team) {
		gi.LocClient_Print(ent, PRINT_HIGH, "{} is not on your team.\n", target->client->resp.netname);
		return;
	}

	gi.LocClient_Print(target, PRINT_HIGH, "{} transferred captain status to you.\n",
		ent->client->resp.netname);
	SetCaptain(team, target);
}

/*
=================
Cmd_LockTeam_f

Locks a team. Captains lock their own team; admins can specify a team.
=================
*/
static void Cmd_LockTeam_f(gentity_t *ent) {
	if (!Teams()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Team lock is only available in team modes.\n");
		return;
	}

	team_t team;

	if (ent->client->sess.admin && gi.argc() >= 2) {
		team = StringToTeamNum(gi.argv(1));
	} else {
		team = ent->client->sess.team;
	}

	if (team != TEAM_RED && team != TEAM_BLUE) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid team.\n");
		return;
	}

	if (!IsCaptainOrAdmin(ent, team)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Only team captains or admins can lock teams.\n");
		return;
	}

	if (level.locked[team]) {
		gi.LocClient_Print(ent, PRINT_HIGH, "{} is already locked.\n", Teams_TeamName(team));
		return;
	}

	level.locked[team] = true;
	gi.LocBroadcast_Print(PRINT_HIGH, "{} has been locked.\n", Teams_TeamName(team));
	P_Menu_Dirty();
}

/*
=================
Cmd_UnlockTeam_f

Unlocks a team. Captains unlock their own team; admins can specify a team.
=================
*/
static void Cmd_UnlockTeam_f(gentity_t *ent) {
	if (!Teams()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Team unlock is only available in team modes.\n");
		return;
	}

	team_t team;

	if (ent->client->sess.admin && gi.argc() >= 2) {
		team = StringToTeamNum(gi.argv(1));
	} else {
		team = ent->client->sess.team;
	}

	if (team != TEAM_RED && team != TEAM_BLUE) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid team.\n");
		return;
	}

	if (!IsCaptainOrAdmin(ent, team)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Only team captains or admins can unlock teams.\n");
		return;
	}

	if (!level.locked[team]) {
		gi.LocClient_Print(ent, PRINT_HIGH, "{} is already unlocked.\n", Teams_TeamName(team));
		return;
	}

	level.locked[team] = false;
	gi.LocBroadcast_Print(PRINT_HIGH, "{} has been unlocked.\n", Teams_TeamName(team));
	P_Menu_Dirty();
}

/*
=================
Cmd_SetTeam_f
=================
*/
static void Cmd_SetTeam_f(gentity_t *ent) {
	if (gi.argc() < 2) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} [client name/num] [team]\n", gi.argv(0));
		return;
	}

	gentity_t *targ = ClientEntFromString(gi.argv(1));

	if (!targ || !targ->inuse || !targ->client) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid client name or number.\n");
		return;
	}

	if (gi.argc() == 2) {
		gi.LocClient_Print(ent, PRINT_HIGH, "{} is on {} team.\n", targ->client->resp.netname, gi.argv(0));
		return;
	}

	team_t team = StringToTeamNum(gi.argv(2));
	if (team == TEAM_NONE) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid team.\n");
		return;
	}

	if (targ->client->sess.team == team) {
		gi.LocClient_Print(ent, PRINT_HIGH, "{} is already on {} team.\n", targ->client->resp.netname, Teams_TeamName(team));
		return;
	}

	if ((Teams() && team == TEAM_FREE) || (!Teams() && team != TEAM_SPECTATOR && team != TEAM_FREE)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid team.\n");
		return;
	}

	gi.LocBroadcast_Print(PRINT_HIGH, "[ADMIN]: Moved {} to {} team.\n", targ->client->resp.netname, Teams_TeamName(team));
	SetTeam(targ, team, false, true, false);
}

/*
=================
Cmd_Shuffle_f
=================
*/
static void Cmd_Shuffle_f(gentity_t *ent) {
	gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Forced team shuffle.\n");
	TeamShuffle();
	Match_Reset();
}

/*
=================
Cmd_BalanceTeams_f
=================
*/
static void Cmd_BalanceTeams_f(gentity_t *ent) {
	gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Forced team balancing.\n");
	TeamBalance(true);
}

/*
=================
Cmd_StartMatch_f
=================
*/
static void Cmd_StartMatch_f(gentity_t *ent) {
	if (level.match_state > matchst_t::MATCH_WARMUP_READYUP) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Match has already started.\n");
		return;
	}

	gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Forced match start.\n");
	Match_Start();
}

/*
=================
Cmd_EndMatch_f
=================
*/
static void Cmd_EndMatch_f(gentity_t *ent) {
	if (level.match_state < matchst_t::MATCH_IN_PROGRESS) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Match has not yet begun.\n");
		return;
	}
	if (level.intermission_time) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Match has already ended.\n");
		return;
	}
	QueueIntermission("[ADMIN]: Forced match end.", true, false);
}

/*
=================
Cmd_ResetMatch_f
=================
*/
static void Cmd_ResetMatch_f(gentity_t *ent) {
	if (level.match_state < matchst_t::MATCH_IN_PROGRESS) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Match has not yet begun.\n");
		return;
	}
	if (level.intermission_time) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Match has already ended.\n");
		return;
	}
	
	gi.LocBroadcast_Print(PRINT_HIGH, "[ADMIN]: Forced match reset.\n");
	Match_Reset();
}

/*
=================
Cmd_ForceVote_f
=================
*/
static void Cmd_ForceVote_f(gentity_t *ent) {
	if (!deathmatch->integer)
		return;

	if (gi.argc() < 2) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <yes|no>\n", gi.argv(0));
		return;
	}

	if (level.vote_state.state == VoteState::IDLE) {
		gi.LocClient_Print(ent, PRINT_HIGH, "No vote in progress.\n");
		return;
	}

	const char *arg = gi.argv(1);

	if (arg[0] == 'y' || arg[0] == 'Y' || arg[0] == '1') {
		gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Passed the vote.\n");
		if (level.vote_state.state == VoteState::ACTIVE) {
			TransitionVoteState(VoteState::PASSED);
		} else if (level.vote_state.state == VoteState::PASSED) {
			// Already passed, just execute immediately
			TransitionVoteState(VoteState::EXECUTING);
		}
	} else {
		gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Failed the vote.\n");
		TransitionVoteState(VoteState::FAILED);
	}
}

/*
=================
Cmd_Gametype_f
=================
*/
static void Cmd_Gametype_f(gentity_t *ent) {
	if (!deathmatch->integer)
		return;

	if (gi.argc() < 2) {
		std::string votable_list = GetVotableGametypesList();
		if (!votable_list.empty()) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <{}>\nChanges current gametype. Current gametype is {} ({}).\n", gi.argv(0), votable_list.c_str(), gt_long_name[g_gametype->integer], g_gametype->integer);
		} else {
			gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <gametype>\nChanges current gametype. Current gametype is {} ({}).\n", gi.argv(0), gt_long_name[g_gametype->integer], g_gametype->integer);
		}
		return;
	}

	gametype_t gt = GT_IndexFromString(gi.argv(1));
	if (gt == GT_NONE) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid gametype.\n");
		std::string votable_list = GetVotableGametypesList();
		if (!votable_list.empty()) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Valid gametypes are: {}\n", votable_list.c_str());
		}
		return;
	}

	// Admins can set any gametype, but warn if it's not votable
	if (g_votable_gametypes->string[0] && !IsGametypeVotable(gt)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Warning: This gametype is not in the votable list, but setting it anyway (admin override).\n");
	}

	ChangeGametype(gt);
}

/*
=================
Cmd_Ruleset_f
=================
*/
static void Cmd_Ruleset_f(gentity_t *ent) {
	if (!deathmatch->integer)
		return;

	if (gi.argc() < 2) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <q2re|mm|q3a|q2reb|qc>\nChanges current ruleset. Current ruleset is {} ({}).\n", gi.argv(0), rs_long_name[(int)game.ruleset], (int)game.ruleset);
		return;
	}

	ruleset_t rs = RS_IndexFromString(gi.argv(1));
	if (rs == RS_NONE) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid ruleset.\n");
		return;
	}

	gi.cvar_forceset("g_ruleset", G_Fmt("{}", (int)rs).data());
}

static void Cmd_SetMap_f(gentity_t *ent) {
	if (gi.argc() < 2) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} [mapname]\nChanges to a map within the map pool or list.", gi.argv(0));
		return;
	}

	if (!IsMapValid(gi.argv(1))) {
		gi.Client_Print(ent, PRINT_HIGH, "Map name is not valid.\n");
		return;
	}
	gi.LocBroadcast_Print(PRINT_HIGH, "[ADMIN]: Changing map to {}\n", gi.argv(1));
	gi.AddCommandString(G_Fmt("gamemap \"{}\"\n", gi.argv(1)).data());
}

extern void ClearWorldEntities();
static void Cmd_MapRestart_f(gentity_t *ent) {
	gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Session reset.\n");

	//TODO: reset match variables, clear world entities, reload world entities
	//SpawnEntities(level.mapname, level.entstring.c_str(), nullptr);
	//Match_Reset();
	//ClearWorldEntities();
	gi.AddCommandString(G_Fmt("gamemap {}\n", level.mapname).data());
}

static void Cmd_NextMap_f(gentity_t *ent) {
	gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Changing to next map.\n");
	Match_End();
	level.intermission_exit = true;
}

static void Cmd_Admin_f(gentity_t *ent) {
	if (!g_allow_admin->integer) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Administration is disabled\n");
		return;
	}
	
	if (gi.argc() > 1) {
		if (ent->client->sess.admin) {
			gi.Client_Print(ent, PRINT_HIGH, "You already have administrative rights.\n");
			return;
		}
		if (admin_password->string && *admin_password->string && Q_strcasecmp(admin_password->string, gi.argv(1)) == 0) {
			if (!ent->client->sess.admin) {
				ent->client->sess.admin = true;
				gi.LocBroadcast_Print(PRINT_HIGH, "{} has become an admin.\n", ent->client->resp.netname);
			}
			return;
		}
	}
	
	// run command if valid...

}

/*----------------------------------------------------------------*/

static bool ReadyConditions(gentity_t *ent, bool desired_status, bool admin_cmd) {
	if (level.match_state == matchst_t::MATCH_WARMUP_READYUP)
		return true;

	const char *s = nullptr;
	if (admin_cmd) {
		s = "You cannot force ready status until ";
	} else {
		s = "You cannot change your ready status until ";
	}

	switch (level.warmup_requisite) {
	case warmupreq_t::WARMUP_REQ_MORE_PLAYERS:
	{
		int minp = GT(GT_DUEL) ? 2 : minplayers->integer;
		int req = minp - level.num_playing_clients;
		gi.LocClient_Print(ent, PRINT_HIGH, "{}{} more player{} present.\n", s, req, req > 1 ? "s are" : " is");
		break;
	}
	case warmupreq_t::WARMUP_REQ_BALANCE:
		gi.LocClient_Print(ent, PRINT_HIGH, "{}teams are balanced.\n", s);
		break;
	default:
		gi.LocClient_Print(ent, PRINT_HIGH, "You cannot {}ready at this stage of the match.\n", desired_status ? "" : "un");
		break;
	}
	return false;
}

static void Cmd_ReadyAll_f(gentity_t *ent) {
	if (!ReadyConditions(ent, true, true))
		return;

	ReadyAll();

	gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Forced all players to ready status\n");
}

static void Cmd_UnReadyAll_f(gentity_t *ent) {
	if (!ReadyConditions(ent, false, true))
		return;

	UnReadyAll();

	gi.Broadcast_Print(PRINT_HIGH, "[ADMIN]: Forced all players to NOT ready status\n");
}

/*
=================
Cmd_ReadyTeam_f

Captain readies up all players on their team.
=================
*/
static void Cmd_ReadyTeam_f(gentity_t *ent) {
	if (!Teams()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "This command is only available in team modes.\n");
		return;
	}

	if (!ReadyConditions(ent, true, false))
		return;

	team_t team = ent->client->sess.team;

	if (team != TEAM_RED && team != TEAM_BLUE) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You must be on a team to use this command.\n");
		return;
	}

	if (!IsCaptainOrAdmin(ent, team)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Only team captains or admins can ready the team.\n");
		return;
	}

	int count = 0;
	for (auto ec : active_clients()) {
		if (!ClientIsPlaying(ec->client))
			continue;
		if (ec->client->sess.team != team)
			continue;
		if (!ec->client->resp.ready) {
			ec->client->resp.ready = true;
			count++;
		}
	}

	if (count > 0)
		gi.LocBroadcast_Print(PRINT_HIGH, "{} readied up {} ({} player{}).\n",
			ent->client->resp.netname, Teams_TeamName(team), count, count > 1 ? "s" : "");
	else
		gi.LocClient_Print(ent, PRINT_HIGH, "All players on {} are already ready.\n", Teams_TeamName(team));
}

static void BroadcastReadyStatus(gentity_t *ent) {
	gi.LocBroadcast_Print(PRINT_CENTER, "%bind:+wheel2:Use Compass to toggle your ready status.%MATCH IS IN WARMUP\n{} is {}ready.", ent->client->resp.netname, ent->client->resp.ready ? "" : "NOT ");
}

static void Cmd_Ready_f(gentity_t *ent) {
	if (!ReadyConditions(ent, true, false))
		return;

	if (level.match_state != matchst_t::MATCH_WARMUP_READYUP) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You cannot ready at this stage of the match.\n");
		return;
	}

	if (ent->client->resp.ready) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You have already committed.\n");
		return;
	}

	ent->client->resp.ready = true;
	BroadcastReadyStatus(ent);
}

static void Cmd_NotReady_f(gentity_t *ent) {
	if (!ReadyConditions(ent, false, false))
		return;

	if (!ent->client->resp.ready) {
		gi.LocClient_Print(ent, PRINT_HIGH, "You haven't committed.\n");
		return;
	}

	ent->client->resp.ready = false;
	BroadcastReadyStatus(ent);
}

void Cmd_ReadyUp_f(gentity_t *ent) {
	if (!ReadyConditions(ent, !ent->client->resp.ready, false))
		return;

	ent->client->resp.ready ^= true;
	BroadcastReadyStatus(ent);
}

static void Cmd_Hook_f(gentity_t *ent) {
	if (!g_allow_grapple->integer || !g_grapple_offhand->integer)
		return;

	Weapon_Hook(ent);
}

static void Cmd_UnHook_f(gentity_t *ent) {
	Weapon_Grapple_DoReset(ent->client);
}

// ======================================================
// DUEL HANDICAP SYSTEM
// ======================================================

// Helper: Sanitize string for name matching (remove case and control characters)
static void Handicap_SanitizeString(const char *in, char *out) {
	while (*in) {
		if (*in < ' ') {
			in++;
			continue;
		}
		*out = tolower(*in);
		out++;
		in++;
	}
	*out = '\0';
}

// Helper: Get client number from name or slot number
static int Handicap_ClientNumberFromName(gentity_t *to, const char *name) {
	char s2[MAX_STRING_CHARS];
	char n2[MAX_STRING_CHARS];
	
	// Numeric values are slot numbers
	if (name[0] >= '0' && name[0] <= '9') {
		int idnum = atoi(name);
		if (idnum >= 0 && (unsigned)idnum < (unsigned)game.maxclients) {
			gclient_t *cl = &game.clients[idnum];
			if (cl->pers.connected)
				return idnum;
		}
		gi.LocClient_Print(to, PRINT_HIGH, "Bad client slot: {}\n", idnum);
		return -1;
	}
	
	// Check for name match
	Handicap_SanitizeString(name, s2);
	for (size_t idnum = 0; idnum < game.maxclients; idnum++) {
		gclient_t *cl = &game.clients[idnum];
		if (!cl->pers.connected)
			continue;
		Handicap_SanitizeString(cl->resp.netname, n2);
		if (!strcmp(n2, s2))
			return idnum;
	}
	
	gi.LocClient_Print(to, PRINT_HIGH, "User {} is not on the server.\n", name);
	return -1;
}

// Helper: Get weapon ID from weapon name string
static item_id_t Handicap_WeaponIDFromName(const char *name) {
	if (!Q_strcasecmp(name, "railgun") || !Q_strcasecmp(name, "rail"))
		return IT_WEAPON_RAILGUN;
	if (!Q_strcasecmp(name, "chaingun") || !Q_strcasecmp(name, "chain"))
		return IT_WEAPON_CHAINGUN;
	if (!Q_strcasecmp(name, "rlauncher") || !Q_strcasecmp(name, "rocket") || !Q_strcasecmp(name, "rl"))
		return IT_WEAPON_RLAUNCHER;
	if (!Q_strcasecmp(name, "all"))
		return IT_NULL; // Special case for "all" (restricts all three: railgun, chaingun, rlauncher)
	return IT_NULL;
}

// Helper: Apply weapon restriction to a player
static void Handicap_ApplyWeaponRestriction(gentity_t *target, item_id_t weapon_id, bool restrict) {
	if (!target || !target->client)
		return;

	if (weapon_id == IT_NULL) {
		// "all" - restrict all three handicap weapons (railgun, chaingun, rlauncher)
		constexpr uint32_t HANDICAP_ALL_WEAPONS = 
			(1U << (IT_WEAPON_RAILGUN - FIRST_WEAPON)) |
			(1U << (IT_WEAPON_CHAINGUN - FIRST_WEAPON)) |
			(1U << (IT_WEAPON_RLAUNCHER - FIRST_WEAPON));
		
		if (restrict) {
			target->client->handicap.restricted_weapons |= HANDICAP_ALL_WEAPONS;
		} else {
			target->client->handicap.restricted_weapons &= ~HANDICAP_ALL_WEAPONS;
		}
	} else {
		uint32_t weapon_bit = 1U << (weapon_id - FIRST_WEAPON);
		
		if (restrict) {
			target->client->handicap.restricted_weapons |= weapon_bit;
		} else {
			target->client->handicap.restricted_weapons &= ~weapon_bit;
		}
	}
	
	// If currently using a restricted weapon, force switch
	if (target->client->pers.weapon && (target->client->pers.weapon->flags & IF_WEAPON)) {
		uint32_t weapon_bit = 1U << (target->client->pers.weapon->id - FIRST_WEAPON);
		if (target->client->handicap.restricted_weapons & weapon_bit) {
			NoAmmoWeaponChange(target, false);
		}
	}
}

static void Cmd_Handicap_f(gentity_t *ent) {
	if (!AdminOk(ent))
		return;

	// Must be in duel mode
	if (notGT(GT_DUEL)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Handicap system only works in duel mode.\n");
		return;
	}

	if (gi.argc() < 4) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <player> <weapon> <on|off>\n", gi.argv(0));
		gi.LocClient_Print(ent, PRINT_HIGH, "Weapons: railgun, chaingun, rlauncher, all\n");
		return;
	}

	const char *player_name = gi.argv(1);
	const char *weapon_name = gi.argv(2);
	const char *onoff = gi.argv(3);

	// Find target player
	int clientnum = Handicap_ClientNumberFromName(ent, player_name);
	if (clientnum < 0) {
		return;
	}

	gentity_t *target = &g_entities[1 + clientnum];
	if (!target->inuse || !target->client || !ClientIsPlaying(target->client)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Player '{}' is not playing.\n", player_name);
		return;
	}

	// Validate weapon
	item_id_t weapon_id = Handicap_WeaponIDFromName(weapon_name);
	if (weapon_id == IT_NULL && Q_strcasecmp(weapon_name, "all")) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid weapon '{}'. Valid: railgun, chaingun, rlauncher, all\n", weapon_name);
		return;
	}

	// Validate on/off
	bool restrict = false;
	if (!Q_strcasecmp(onoff, "on") || !Q_strcasecmp(onoff, "1")) {
		restrict = true;
	} else if (!Q_strcasecmp(onoff, "off") || !Q_strcasecmp(onoff, "0")) {
		restrict = false;
	} else {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid value '{}'. Use 'on' or 'off'.\n", onoff);
		return;
	}

	// Apply restriction
	Handicap_ApplyWeaponRestriction(target, weapon_id, restrict);

	// Broadcast result
	const char *weapon_display = weapon_name;
	if (weapon_id != IT_NULL) {
		gitem_t *weapon_item = GetItemByIndex(weapon_id);
		if (weapon_item)
			weapon_display = weapon_item->pickup_name;
	} else {
		weapon_display = "all handicap weapons (railgun, chaingun, rlauncher)";
	}

	gi.LocBroadcast_Print(PRINT_HIGH, "[ADMIN]: {} {} for {}.\n", 
		weapon_display, restrict ? "restricted" : "unrestricted", 
		target->client->resp.netname);
}

static void Cmd_HandicapClear_f(gentity_t *ent) {
	if (!AdminOk(ent))
		return;

	// Must be in duel mode
	if (notGT(GT_DUEL)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Handicap system only works in duel mode.\n");
		return;
	}

	if (gi.argc() < 2) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <player>\n", gi.argv(0));
		return;
	}

	const char *player_name = gi.argv(1);

	// Find target player
	int clientnum = Handicap_ClientNumberFromName(ent, player_name);
	if (clientnum < 0) {
		return;
	}

	gentity_t *target = &g_entities[1 + clientnum];
	if (!target->inuse || !target->client) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Player '{}' is not active.\n", player_name);
		return;
	}

	// Clear all restrictions
	target->client->handicap.restricted_weapons = 0;
	target->client->handicap.damage_dealt_multiplier = 1.0f;
	target->client->handicap.damage_received_multiplier = 1.0f;
	target->client->handicap.health_multiplier = 1.0f;

	// Force weapon switch if currently using a restricted weapon
	if (target->client->pers.weapon && (target->client->pers.weapon->flags & IF_WEAPON)) {
		uint32_t weapon_bit = 1U << (target->client->pers.weapon->id - FIRST_WEAPON);
		if (target->client->handicap.restricted_weapons & weapon_bit) {
			NoAmmoWeaponChange(target, false);
		}
	}

	gi.LocBroadcast_Print(PRINT_HIGH, "[ADMIN]: All handicaps cleared for {}.\n", 
		target->client->resp.netname);
}

// ======================================================
// MAP QUEUE
// ======================================================

static void MQ_PrintList(gentity_t *ent) {
	std::string text = "";
	for (size_t i = 0; i < game.mapqueue.size(); i++) {
		if (!game.mapqueue[i].empty())
			text += game.mapqueue[i] + " ";	// G_Fmt("{} \n", game.mapqueue[i].data()).data();
	}
	
	gi.LocClient_Print(ent, PRINT_HIGH, G_Fmt("{}\n", text).data());
}

static void Cmd_MapList_f(gentity_t *ent) {
	if (g_map_list->string[0]) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Current map list:\n");
		// Truncate map list if too long to prevent message overflow
		constexpr size_t MAX_MAP_LIST_DISPLAY = 512;
		std::string map_list_display = g_map_list->string;
		if (map_list_display.length() > MAX_MAP_LIST_DISPLAY) {
			map_list_display = map_list_display.substr(0, MAX_MAP_LIST_DISPLAY) + "...";
		}
		gi.LocClient_Print(ent, PRINT_HIGH, "{}\n", map_list_display.c_str());
		if (MQ_Count()) {
			gi.LocClient_Print(ent, PRINT_HIGH, "\nCurrent MyMap Queue:\n");
			MQ_PrintList(ent);
		}
	} else {
		gi.LocClient_Print(ent, PRINT_HIGH, "No Map List set.\n");
	}
}

static void Cmd_MapInfo_f(gentity_t *ent) {
	if (level.mapname[0])
		gi.LocClient_Print(ent, PRINT_HIGH, "MAP INFO:\nfilename: {}\n", level.mapname);
	else return;
	if (level.level_name[0])
		gi.LocClient_Print(ent, PRINT_HIGH, "longname: {}\n", level.level_name);
	if (level.author[0])
		gi.LocClient_Print(ent, PRINT_HIGH, "author{}: {}{}{}\n", level.author2[0] ? "s" : "", level.author, level.author2[0] ? ", " : "", level.author2[0] ? level.author2 : "");
}

static const char *MyMap_FlagString() {

}

static void Cmd_MyMap_f(gentity_t *ent) {
	if (!g_allow_mymap->integer) {
		gi.LocClient_Print(ent, PRINT_HIGH, "MyMap is disabled.\n");
		return;
	}

	if (!g_map_list->string[0]) {
		gi.LocClient_Print(ent, PRINT_HIGH, "No maps are queued as no map list is present.\n");
		return;
	}

	if (gi.argc() < 2) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Add a map to the MyMap Queue.\nRecognized maps are:\n");
		// Truncate map list if too long to prevent message overflow
		constexpr size_t MAX_MAP_LIST_DISPLAY = 512;
		std::string map_list_display = g_map_list->string;
		if (map_list_display.length() > MAX_MAP_LIST_DISPLAY) {
			map_list_display = map_list_display.substr(0, MAX_MAP_LIST_DISPLAY) + "...";
		}
		gi.LocClient_Print(ent, PRINT_HIGH, "{}\n", map_list_display.c_str());

		if (MQ_Count()) {
			gi.LocClient_Print(ent, PRINT_HIGH, "MyMap Queue => ");
			MQ_PrintList(ent);
		}
		return;
	}

	if (!strcmp(gi.argv(1), level.mapname)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Cannot add current map to MyMap Queue.\n");
		return;
	}
	
	MQ_Add(ent, gi.argv(1));
	
	std::string text = "";

	for (size_t i = 0; i < game.mapqueue.size(); i++) {
		if (!game.mapqueue[i].empty()) {
			text += game.mapqueue[i];
			
			text += " ";
		}
	}

	game.item_inhibit_pu = 0;
	game.item_inhibit_pa = 0;
	game.item_inhibit_ht = 0;
	game.item_inhibit_ar = 0;
	game.item_inhibit_am = 0;
	game.item_inhibit_wp = 0;

	// flags: "+pu", "-pa", "+ht", "+ar", "+am", "+wp"
	// argv indices: 0=cmd, 1=mapname, 2..argc-1=flags
	for (int i = 2; i < gi.argc(); i++) {
		const char *s = gi.argv(i);
		if (!s || !s[0])
			continue;
		int num = 0;
		if (s[0] == '+') { num = 1; s++; }
		else if (s[0] == '-') { num = -1; s++; }
		else continue;
		if (!Q_strcasecmp(s, "pu"))      game.item_inhibit_pu = num;
		else if (!Q_strcasecmp(s, "pa")) game.item_inhibit_pa = num;
		else if (!Q_strcasecmp(s, "ht")) game.item_inhibit_ht = num;
		else if (!Q_strcasecmp(s, "ar")) game.item_inhibit_ar = num;
		else if (!Q_strcasecmp(s, "am")) game.item_inhibit_am = num;
		else if (!Q_strcasecmp(s, "wp")) game.item_inhibit_wp = num;
	}
	
	if (text.size())
		gi.LocBroadcast_Print(PRINT_HIGH, "MyMap Queue => {}\n", text.data());
}

static void Cmd_LoadMotd_f(gentity_t *ent) {
	G_LoadMOTD();
}

static void Cmd_Motd_f(gentity_t *ent) {
	const char *s = nullptr;

	if (game.motd.size())
		s = G_Fmt("Message of the Day:\n{}\n", game.motd.c_str()).data();
	else
		s = "No Message of the Day set.\n";
	gi.LocClient_Print(ent, PRINT_HIGH, "{}", s);
}

// =========================================

cmds_t client_cmds[] = {
	{"admin",			Cmd_Admin_f,			CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"alertall",		Cmd_AlertAll_f,			CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"announcer",		Cmd_Announcer_f,		CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"balance",			Cmd_BalanceTeams_f,		CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"boot",			Cmd_Boot_f,				CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"callvote",		Cmd_CallVote_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"captain",			Cmd_Captain_f,			CF_ALLOW_DEAD},
	{"checkpoi",		Cmd_CheckPOI_f,			CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"clear_ai_enemy",	Cmd_Clear_AI_Enemy_f,	CF_CHEAT_PROTECT},
	{"cv",				Cmd_CallVote_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"drop", 			Cmd_Drop_f,				CF_NONE},
	{"drop_index", 		Cmd_Drop_f,				CF_NONE},
	{"doctor", 			Cmd_Doctor_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"endmatch", 		Cmd_EndMatch_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"fm", 				Cmd_FragMessages_f,		CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"follow",			Cmd_Follow_f,			CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"followkiller",	Cmd_FollowKiller_f,		CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"followleader",	Cmd_FollowLeader_f,		CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"followpowerup",	Cmd_FollowPowerup_f,	CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"forcevote",		Cmd_ForceVote_f,		CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"forfeit",			Cmd_Forfeit_f,			CF_ALLOW_DEAD},
	{"gametype",		Cmd_Gametype_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"ghost",			Cmd_Ghost_f,			CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"give",			Cmd_Give_f,				CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"god",				Cmd_God_f,				CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"help",			Cmd_Help_f,				CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"hook",			Cmd_Hook_f,				CF_NONE},
	{"id",				Cmd_CrosshairID_f,		CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"immortal",		Cmd_Immortal_f,			CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"invdrop",			Cmd_InvDrop_f,			CF_NONE},
	{"inven",			Cmd_Inven_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"invnext",			Cmd_InvNext_f,			CF_ALLOW_SPEC},	//spec for menu up/down
	{"invnextp",		Cmd_InvNextP_f,			CF_NONE},
	{"invnextw",		Cmd_InvNextW_f,			CF_NONE},
	{"invprev",			Cmd_InvPrev_f,			CF_ALLOW_SPEC},	//spec for menu up/down
	{"invprevp",		Cmd_InvPrevP_f,			CF_NONE},
	{"invprevw",		Cmd_InvPrevW_f,			CF_NONE},
	{"invuse",			Cmd_InvUse_f,			CF_ALLOW_SPEC},	//spec for menu up/down
	{"kb",				Cmd_KillBeep_f,			CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"kill",			Cmd_Kill_f,				CF_NONE},
	{"kill_ai",			Cmd_Kill_AI_f,			CF_CHEAT_PROTECT},
	{"listentities",	Cmd_ListEntities_f,		CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"listmonsters",	Cmd_ListMonsters_f,		CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"loadmotd",		Cmd_LoadMotd_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"lockteam",		Cmd_LockTeam_f,			CF_ALLOW_DEAD},
	{"map_restart",		Cmd_MapRestart_f,		CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"mapinfo",			Cmd_MapInfo_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"maplist",			Cmd_MapList_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"motd",			Cmd_Motd_f,				CF_ALLOW_SPEC | CF_ALLOW_INT},
	{"mymap",			Cmd_MyMap_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"nextmap",			Cmd_NextMap_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"noclip",			Cmd_NoClip_f,			CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"notarget",		Cmd_NoTarget_f,			CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"notready",		Cmd_NotReady_f,			CF_ALLOW_DEAD},
	{"novisible",		Cmd_NoVisible_f,		CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"players",			Cmd_Players_f,			CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"playtime",		Cmd_PlayersJoinTime_f,	CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"playrank",		Cmd_PlayersRanked_f,	CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"putaway",			Cmd_PutAway_f,			CF_ALLOW_SPEC},	//spec for menu close
	{"ready",			Cmd_Ready_f,			CF_ALLOW_DEAD},
	{"readyall",		Cmd_ReadyAll_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"readyteam",		Cmd_ReadyTeam_f,		CF_ALLOW_DEAD},
	{"readyup",			Cmd_ReadyUp_f,			CF_ALLOW_DEAD},
	{"resetmatch",		Cmd_ResetMatch_f,		CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"ruleset",			Cmd_Ruleset_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"score",			Cmd_Score_f,			CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"setpoi",			Cmd_SetPOI_f,			CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"setmap",			Cmd_SetMap_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"setteam",			Cmd_SetTeam_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"shuffle",			Cmd_Shuffle_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"spawn",			Cmd_Spawn_f,			CF_ADMIN_ONLY | CF_ALLOW_SPEC},
	{"startmatch",		Cmd_StartMatch_f,		CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"stats",			Cmd_Stats_f,			CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"target",			Cmd_Target_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"team",			Cmd_Team_f,				CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"teleport",		Cmd_Teleport_f,			CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"time-out",		Cmd_TimeOut_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"time-in",			Cmd_TimeIn_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"timer",			Cmd_Timer_f,			CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"unhook",			Cmd_UnHook_f,			CF_NONE},
	{"unlockteam",		Cmd_UnlockTeam_f,		CF_ALLOW_DEAD},
	{"unreadyall",		Cmd_UnReadyAll_f,		CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"use",				Cmd_Use_f,				CF_NONE},
	{"use_index",		Cmd_Use_f,				CF_NONE},
	{"use_index_only",	Cmd_Use_f,				CF_NONE},
	{"use_only",		Cmd_Use_f,				CF_NONE},
	{"vote",			Cmd_Vote_f,				CF_ALLOW_DEAD},
	{"wave",			Cmd_Wave_f,				CF_NONE},
	{"weaplast",		Cmd_WeapLast_f,			CF_NONE},
	{"weapnext",		Cmd_WeapNext_f,			CF_NONE},
	{"weapprev",		Cmd_WeapPrev_f,			CF_NONE},
	{"where",			Cmd_Where_f,			CF_ALLOW_SPEC},
	{"handicap",			Cmd_Handicap_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"handicap_clear",	Cmd_HandicapClear_f,	CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
};

/*
===============
FindClientCmdByName
===============
*/
static cmds_t *FindClientCmdByName(const char *name) {
	cmds_t	*cc = client_cmds;

	for (size_t i = 0; i < (sizeof(client_cmds) / sizeof(client_cmds[0])); i++, cc++) {
		if (!cc->name)
			continue;
		if (!Q_strcasecmp(cc->name, name))
			return cc;
	}

	return nullptr;
}

/*
=================
ClientCommand
=================
*/
void ClientCommand(gentity_t *ent) {
	cmds_t		*cc;
	const char	*cmd;

	if (!ent->client)
		return; // not fully in game yet

#if 0
	// check if client is 888, print what is being sent and prevent any further processing
	if (ent->client->sess.is_888) {
		gi.Com_PrintFmt("Sneaky little snake Dalude/888 (%s) sent the following command:\n{}\n", ent->client->pers.netname, gi.args());
		return;
	}
#endif

	cmd = gi.argv(0);
	cc = FindClientCmdByName(cmd);

	// [Paril-KEX] these have to go through the lobby system
#ifndef KEX_Q2_GAME
	if (!Q_strcasecmp(cmd, "say")) {
		Cmd_Say_f(ent, false);
		return;
	}
	if (!Q_strcasecmp(cmd, "say_team") == 0 || !Q_strcasecmp(cmd, "steam")) {
		if (Teams())
			Cmd_Say_Team_f(ent, gi.args());
		else
			Cmd_Say_f(ent, false);
		return;
	}
#endif

	if (!cc) {
		// always allow replace_/disable_ item cvars
		if (gi.argc() > 1 && strstr(cmd, "replace_") || strstr(cmd, "disable_")) {
			gi.cvar_forceset(cmd, gi.argv(1));
		} else
			gi.LocClient_Print(ent, PRINT_HIGH, "Invalid client command: \"{}\"\n", cmd);
		return;
	}

	if (cc->flags & CF_ADMIN_ONLY)
		if (!AdminOk(ent))
			return;

	if (cc->flags & CF_CHEAT_PROTECT)
		if (!CheatsOk(ent))
			return;

	if (!(cc->flags & CF_ALLOW_DEAD))
		if (!AliveOk(ent))
			return;

	if (!(cc->flags & CF_ALLOW_SPEC))
		if (!SpectatorOk(ent))
			return;

	if (cc->flags & CF_MATCH_ONLY)
		return;

	if (!(cc->flags & CF_ALLOW_INT))
		if (level.intermission_time)
			return;

	cc->func(ent);
}
