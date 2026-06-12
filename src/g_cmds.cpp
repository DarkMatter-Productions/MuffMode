// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include "g_local.h"
#include "g_debug_log.h"
#include "muffmode/mm_admin.h"
#include "muffmode/mm_captain.h"
#include "muffmode/mm_duel.h"
#include "muffmode/mm_ghost.h"
#include "muffmode/mm_maps.h"
#include "muffmode/mm_match.h"
#include "muffmode/mm_menu.h"
#include "muffmode/mm_motd.h"
#include "muffmode/mm_pconfig.h"
#include "muffmode/mm_team.h"
#include "muffmode/mm_vote.h"
#include "muffmode/mm_vote_menu.h"
#include "monsters/m_player.h"
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

// [MuffMode] Timeout bodies live in muffmode/mm_match
static void Cmd_TimeIn_f(gentity_t *ent) {
	MM_CmdTimeIn(ent);
}

static void Cmd_TimeOut_f(gentity_t *ent) {
	MM_CmdTimeOut(ent);
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
// [MuffMode] Forfeit body lives in muffmode/mm_duel
static void Cmd_Forfeit_f(gentity_t *ent) {
	MM_Duel_CmdForfeit(ent);
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

// [MuffMode] Team command body lives in muffmode/mm_team
static void Cmd_Team_f(gentity_t *ent) {
	MM_CmdTeam(ent);
}

// [MuffMode] Client preference command bodies live in muffmode/mm_pconfig
static void Cmd_CrosshairID_f(gentity_t *ent) {
	MM_CmdCrosshairID(ent);
}

static void Cmd_Timer_f(gentity_t *ent) {
	MM_CmdTimer(ent);
}

static void Cmd_FragMessages_f(gentity_t *ent) {
	MM_CmdFragMessages(ent);
}

static void Cmd_Announcer_f(gentity_t *ent) {
	MM_CmdAnnouncer(ent);
}

static void Cmd_KillBeep_f(gentity_t *ent) {
	MM_CmdKillBeep(ent);
}

// [MuffMode] Ghost rejoin body lives in muffmode/mm_ghost
static void Cmd_Ghost_f(gentity_t *ent) {
	MM_CmdGhost(ent);
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
	MM_CmdDoctor(ent);
}

/*----------------------------------------------------------------*/

// NEW VOTING CODE

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
	MM_CmdVote(ent);
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

// [MuffMode] Captain and team lock bodies live in muffmode/mm_captain
static void Cmd_Captain_f(gentity_t *ent) {
	MM_CmdCaptain(ent);
}

static void Cmd_LockTeam_f(gentity_t *ent) {
	MM_CmdLockTeam(ent);
}

static void Cmd_UnlockTeam_f(gentity_t *ent) {
	MM_CmdUnlockTeam(ent);
}

// [MuffMode] Admin team command bodies live in muffmode/mm_team
static void Cmd_SetTeam_f(gentity_t *ent) {
	MM_CmdSetTeam(ent);
}

static void Cmd_Shuffle_f(gentity_t *ent) {
	MM_CmdShuffle(ent);
}

static void Cmd_BalanceTeams_f(gentity_t *ent) {
	MM_CmdBalanceTeams(ent);
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
	MM_CmdGametype(ent);
}

/*
=================
Cmd_Ruleset_f
=================
*/
static void Cmd_Ruleset_f(gentity_t *ent) {
	MM_CmdRuleset(ent);
}

static void Cmd_SetMap_f(gentity_t *ent) {
	MM_CmdSetMap(ent);
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

// [MuffMode] Readyup command bodies live in muffmode/mm_captain
static void Cmd_ReadyAll_f(gentity_t *ent) {
	MM_CmdReadyAll(ent);
}

static void Cmd_UnReadyAll_f(gentity_t *ent) {
	MM_CmdUnReadyAll(ent);
}

static void Cmd_ReadyTeam_f(gentity_t *ent) {
	MM_CmdReadyTeam(ent);
}

static void Cmd_Ready_f(gentity_t *ent) {
	MM_CmdReady(ent);
}

static void Cmd_NotReady_f(gentity_t *ent) {
	MM_CmdNotReady(ent);
}

static void Cmd_ReadyUp_f(gentity_t *ent) {
	MM_CmdReadyUp(ent);
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
// MAP QUEUE
// ======================================================

static void Cmd_MapList_f(gentity_t *ent) {
	MM_CmdMapList(ent);
}

static void Cmd_MyMap_f(gentity_t *ent) {
	MM_CmdMyMap(ent);
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

static void Cmd_LoadMotd_f(gentity_t *ent) {
	MM_CmdLoadMotd(ent);
}

static void Cmd_Motd_f(gentity_t *ent) {
	MM_CmdMotd(ent);
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
	{"invnext",			Cmd_InvNext_f,			CF_ALLOW_SPEC | CF_ALLOW_DEAD},	//spec for menu up/down, dead for horde spectators
	{"invnextp",		Cmd_InvNextP_f,			CF_NONE},
	{"invnextw",		Cmd_InvNextW_f,			CF_NONE},
	{"invprev",			Cmd_InvPrev_f,			CF_ALLOW_SPEC | CF_ALLOW_DEAD},	//spec for menu up/down, dead for horde spectators
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
