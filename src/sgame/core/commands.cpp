// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include "g_local.h"
#include "muffmode/mm_client_profile.h"
#include "muffmode/mm_player_stats.h"
#include "debug_log.h"
#include "muffmode/mm_admin.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_awards.h"
#include "muffmode/mm_captain.h"
#include "muffmode/mm_chat.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_duel.h"
#include "muffmode/mm_freezetag.h"
#include "muffmode/mm_ghost.h"
#include "muffmode/mm_items_rules.h"
#include "muffmode/mm_loc.h"
#include "muffmode/mm_map_pick.h"
#include "muffmode/mm_map_pool.h"
#include "muffmode/mm_maps.h"
#include "muffmode/mm_match.h"
#include "muffmode/mm_menu.h"
#include "muffmode/mm_motd.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_pconfig.h"
#include "muffmode/mm_skin.h"
#include "muffmode/mm_statusbar.h"
#include "muffmode/mm_team.h"
#include "muffmode/mm_vote.h"
#include "muffmode/mm_vote_menu.h"
#include "monsters/m_player.h"
enum class cmd_flags_t : uint32_t {
	none			= 0,
	allow_dead		= bit_v<0>,
	allow_int		= bit_v<1>,
	allow_spec		= bit_v<2>,
	match_only		= bit_v<3>,
	admin_only		= bit_v<4>,
	cheat_protect	= bit_v<5>,
};
MAKE_ENUM_BITFLAGS(cmd_flags_t);

constexpr cmd_flags_t CF_NONE = cmd_flags_t::none;
constexpr cmd_flags_t CF_ALLOW_DEAD = cmd_flags_t::allow_dead;
constexpr cmd_flags_t CF_ALLOW_INT = cmd_flags_t::allow_int;
constexpr cmd_flags_t CF_ALLOW_SPEC = cmd_flags_t::allow_spec;
constexpr cmd_flags_t CF_MATCH_ONLY = cmd_flags_t::match_only;
constexpr cmd_flags_t CF_ADMIN_ONLY = cmd_flags_t::admin_only;
constexpr cmd_flags_t CF_CHEAT_PROTECT = cmd_flags_t::cheat_protect;

constexpr bool HasCmdFlag(cmd_flags_t flags, cmd_flags_t flag)
{
	return (flags & flag) != cmd_flags_t::none;
}

struct cmds_t {
	const		char *name;
	void		(*func)(gentity_t *ent);
	cmd_flags_t	flags;
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

	if (MM_FreezeTag_IsFrozen(ent)) {
		MM_FreezeTag_BlockFrozenCommand(ent);
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

static size_t CmdEntityCount() {
	return min(static_cast<size_t>(globals.num_entities), static_cast<size_t>(game.maxentities));
}

static size_t CmdClientEntityCount() {
	return min(CmdEntityCount(), static_cast<size_t>(game.maxclients) + 1);
}

static bool CmdClientIndexIsValid(int client_index) {
	return client_index >= 0 && static_cast<size_t>(client_index) < game.maxclients;
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
		if (gi.argc() == 3) {
			const auto health = MM_ParseIntArg(gi.argv(2));
			if (!health) {
				gi.LocClient_Print(ent, PRINT_HIGH, "Invalid health value.\n");
				return;
			}
			ent->health = *health;
		} else {
			ent->health = ent->max_health;
		}
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
	if (it->flags & IF_AMMO && gi.argc() == 3) {
		const auto count = MM_ParseIntArg(gi.argv(2));
		if (!count) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Invalid item count.\n");
			G_FreeEntity(it_ent);
			return;
		}
		it_ent->count = *count;
	}

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
	if (!G_UseTargets(ent, ent))
		return;
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
	const int argc = gi.argc();
	if (!MM_IsSpawnArgcValid(argc)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <classname> [<key> <value>]...\n", gi.argv(0));
		if (argc >= 2)
			gi.LocClient_Print(ent, PRINT_HIGH, "Spawn arguments must be key/value pairs.\n");
		return;
	}

	const int tail_args = argc - 2;

	solid_t backup = ent->solid;
	ent->solid = SOLID_NOT;
	gi.linkentity(ent);

	gentity_t *other = G_Spawn();
	other->classname = gi.argv(1);

	other->s.origin = ent->s.origin + (AngleVectors(ent->s.angles).forward * 24.f);
	other->s.angles[YAW] = ent->s.angles[YAW];

	st = {};

	if (tail_args > 0) {
		for (int i = 2; i < argc; i += 2)
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
argv(4) optional pitch
argv(5) optional yaw
argv(6) optional roll
=================
*/
static void Cmd_Teleport_f(gentity_t *ent) {
	const int argc = gi.argc();
	if (!MM_IsTeleportArgcValid(argc)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <x> <y> <z> [<pitch> <yaw> <roll>]\n", gi.argv(0));
		return;
	}

	const auto x = MM_ParseFloatArg(gi.argv(1));
	const auto y = MM_ParseFloatArg(gi.argv(2));
	const auto z = MM_ParseFloatArg(gi.argv(3));
	if (!x || !y || !z) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid coordinate(s).\n");
		return;
	}

	ent->s.origin = vec3_t{ *x, *y, *z };

	if (argc == 7) {
		const auto pitch = MM_ParseFloatArg(gi.argv(4));
		const auto yaw = MM_ParseFloatArg(gi.argv(5));
		const auto roll = MM_ParseFloatArg(gi.argv(6));
		if (!pitch || !yaw || !roll) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Invalid angle(s).\n");
			return;
		}

		const vec3_t ang{ *pitch, *yaw, *roll };
		ent->client->ps.pmove.delta_angles = (ang - ent->client->resp.cmd_angles);
		ent->client->ps.viewangles = {};
		ent->client->v_angle = {};
	}

	gi.linkentity(ent);
}

// [MuffMode] Timeout bodies live in muffmode/mm_match
static void Cmd_TimeIn_f(gentity_t *ent) {
	if (!MM_Arena_CallTimein(ent))
		MM_CmdTimeIn(ent);
}

static void Cmd_TimeOut_f(gentity_t *ent) {
	if (!MM_Arena_CallTimeout(ent))
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
	const size_t entity_count = CmdEntityCount();

	for (size_t i = 0; i < entity_count; i++) {
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
		const auto item_index = MM_ParseIntArg(s);
		if (!item_index) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Invalid item index.\n");
			return;
		}
		it = GetItemByIndex((item_id_t)*item_index);
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

	const bool arena_ready_compass =
		GT(GT_ARENA) && index == IT_COMPASS;
	if ((GT(GT_ARENA) ? !MM_Arena_CombatEnabled(ent) : IsCombatDisabled()) &&
		!(it->flags & IF_WEAPON) && !arena_ready_compass)
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

	// Arena loadouts are round-owned and cannot be transferred or discarded.
	if (GT(GT_ARENA)) {
		gi.Client_Print(ent, PRINT_HIGH,
			"Arena loadouts and ammunition cannot be dropped.\n");
		return;
	}

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
		const auto item_index = MM_ParseIntArg(s);
		if (!item_index) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Invalid item index.\n");
			return;
		}
		it = GetItemByIndex((item_id_t)*item_index);
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

static bool ShouldShowMenuBindHint(const gentity_t *ent)
{
	if (!ent || !ent->client)
		return false;
	if (level.match_state >= match_state_t::MATCH_COUNTDOWN)
		return false;
	if (level.round_state == round_state_t::ROUND_COUNTDOWN)
		return false;
	if (level.match_state == match_state_t::MATCH_WARMUP_READYUP && ent->client->resp.ready)
		return false;
	return true;
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

	// [MuffMode] The post-match screens own the intermission layout: the next-map
	// pick draws its menu where the scoreboard was and the awards reel paints over
	// it. This is the one intermission-allowed command that closes or replaces a
	// menu, and an inven press here would take the pick away from the player who
	// pressed it -- permanently once the winner reveal has started, since the pick
	// stops reopening menus then -- or leave a join menu over the reel that
	// P_Menu_Select refuses to act on during intermission anyway. The bind does
	// nothing while either screen is up.
	if (MM_MapPick_Active() || MM_Awards_Active())
		return;

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
			if (ShouldShowMenuBindHint(ent))
				gi.LocClient_Print(ent, PRINT_CENTER, "%bind:inven:Open menu%{}", " ");
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

	if (GT(GT_ARENA) ? !MM_Arena_CombatEnabled(ent) : IsCombatDisabled())
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

	if (gentity_t *sphere = G_ResolveOwnedSphere(ent->client)) {
		G_FreeEntity(sphere);
		G_ClearOwnedSphere(ent->client);
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

	const size_t num_entities = CmdEntityCount();
	for (size_t entnum = 1; entnum < num_entities; ++entnum) {
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
	// only write the listen-server host's own clipboard (entity 1); otherwise any
	// remote client running "where" would clobber the host's clipboard.
	if (ent == &g_entities[1])
		gi.SendToClipBoard(location.c_str());
}

/*
=================
Cmd_HudDump_f

[MuffMode] Dev tooling: write the current gametype's CS_STATUSBAR layout to hud_dump.json
in the working dir, for loading into the offline HUD previewer (docs-dev/hud-editor).
=================
*/
static void Cmd_HudDump_f(gentity_t *ent) {
	std::string path;
	if (MM_DumpStatusbar(&path))
		gi.LocClient_Print(ent, PRINT_HIGH, "HUD layout dumped to {}\n", path.c_str());
	else
		gi.LocClient_Print(ent, PRINT_HIGH, "HUD layout dump failed.\n");
}

/*
=================
Cmd_Clear_AI_Enemy_f
=================
*/
static void Cmd_Clear_AI_Enemy_f(gentity_t *ent) {
	const size_t entity_count = CmdEntityCount();

	for (size_t i = 1; i < entity_count; i++) {
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
	const int anum = *(const int *)a;
	const int bnum = *(const int *)b;

	const bool a_valid = CmdClientIndexIsValid(anum);
	const bool b_valid = CmdClientIndexIsValid(bnum);
	if (!a_valid && !b_valid)
		return 0;
	if (!a_valid)
		return 1;
	if (!b_valid)
		return -1;

	const int a_score = game.clients[anum].resp.score;
	const int b_score = game.clients[bnum].resp.score;

	if (a_score < b_score)
		return -1;
	if (a_score > b_score)
		return 1;
	return 0;
}

static int PlayerSortByJoinTimeCmd(const void *a, const void *b) {
	const int anum = *(const int *)a;
	const int bnum = *(const int *)b;

	const bool a_valid = CmdClientIndexIsValid(anum);
	const bool b_valid = CmdClientIndexIsValid(bnum);
	if (!a_valid && !b_valid)
		return 0;
	if (!a_valid)
		return 1;
	if (!b_valid)
		return -1;

	const auto a_join_time = game.clients[anum].sess.team_join_time.milliseconds();
	const auto b_join_time = game.clients[bnum].sess.team_join_time.milliseconds();

	if (a_join_time > b_join_time)
		return -1;
	if (a_join_time < b_join_time)
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
	int		index[MAX_LOBBY_PLAYERS] = { 0 };

	small.clear();
	large.clear();

	count = 0;
	for (auto ec : active_clients()) {
		if (count >= q_countof(index))
			break;

		const ptrdiff_t client_index = ec - g_entities - 1;
		if (client_index < 0 || static_cast<size_t>(client_index) >= game.maxclients)
			continue;

		index[count++] = static_cast<int>(client_index);
	}

	// sort by score
	if (ranked)
		qsort(index, count, sizeof(index[0]), PlayerSortByScore);

	if (count) {
		for (i = 0; i < count; i++) {
			if (!CmdClientIndexIsValid(index[i]))
				continue;

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
		if (!large.empty())
			large.pop_back();
	}

	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "\nclientnum id                               name                             time  ping score team\n");
	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "--------------------------------------------------------------------------------------------------------------\n");
	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "{}", large.c_str());
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
	int		index[MAX_LOBBY_PLAYERS] = { 0 };

	small.clear();
	large.clear();

	count = 0;
	for (auto ec : active_clients()) {
		if (count >= q_countof(index))
			break;

		const ptrdiff_t client_index = ec - g_entities - 1;
		if (client_index < 0 || static_cast<size_t>(client_index) >= game.maxclients)
			continue;

		index[count++] = static_cast<int>(client_index);
	}

	// sort by join time
	qsort(index, count, sizeof(index[0]), PlayerSortByJoinTimeCmd);

	if (count) {
		for (i = 0; i < count; i++) {
			if (!CmdClientIndexIsValid(index[i]))
				continue;

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
		if (!large.empty())
			large.pop_back();
	}

	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "\nclientnum id                               name                             time  ping score team\n");
	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "--------------------------------------------------------------------------------------------------------------\n");
	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "{}", large.c_str());
	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "\n--------------------------------------------------------------------------------------------------------------\n");
	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "total players: {}\n", count);
	gi.LocClient_Print(ent, PRINT_HIGH | PRINT_NO_NOTIFY, "\n");
}

bool CheckFlood(gentity_t *ent) {
	constexpr int flood_history_capacity = 10;
	static_assert(q_countof(gclient_t::flood_when) == flood_history_capacity);

	const int message_count = MM_ClampFloodMessageCount(
		flood_msgs->integer, flood_history_capacity);
	if (message_count) {
		gclient_t *cl = ent->client;

		if (level.time < cl->flood_locktill) {
			gi.LocClient_Print(ent, PRINT_HIGH, "$g_flood_cant_talk",
				(cl->flood_locktill - level.time).seconds<int32_t>());
			return true;
		}

		cl->flood_whenhead = MM_NormalizeRingIndex(
			cl->flood_whenhead, flood_history_capacity);
		const int i = MM_FloodHistoryIndex({
			cl->flood_whenhead, message_count, flood_history_capacity });
		if (cl->flood_when[i] && level.time - cl->flood_when[i] < gtime_t::from_sec(flood_persecond->value)) {
			cl->flood_locktill = level.time + gtime_t::from_sec(flood_waitdelay->value);
			gi.LocClient_Print(ent, PRINT_CHAT, "$g_flood_cant_talk",
				flood_waitdelay->integer);
			return true;
		}
		cl->flood_whenhead = (cl->flood_whenhead + 1) % flood_history_capacity;
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
	int i = GESTURE_POINT;
	if (gi.argc() > 1) {
		const auto gesture = MM_ParseIntArg(gi.argv(1));
		if (!gesture) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Invalid gesture value.\n");
			return;
		}
		i = *gesture;
	}

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

/*
==================
Cmd_SayScoped_f

Arena chat is routed by the Arena module so players in one RA2 arena cannot
receive another arena's conversation.  KEX owns the built-in say/say_team
commands, but this helper also backs the explicit say_world command.
==================
*/
static void Cmd_SayScoped_f(gentity_t *ent, bool arg0,
	mm_arena_chat_scope_t scope, bool team_prefix) {
	const char *p_in;
	std::string message;

	if (gi.argc() < 2 && !arg0)
		return;

	if (arg0) {
		message += gi.argv(0);
		message += " ";
		message += gi.args();
	} else {
		p_in = gi.args();
		size_t in_len = strlen(p_in);

		if (in_len >= 2 && p_in[0] == '\"' && p_in[in_len - 1] == '\"')
			message += std::string_view(p_in + 1, in_len - 2);
		else
			message += p_in;
	}

	MM_SendScopedChat(ent, scope, message, team_prefix);
}

/*
==================
Cmd_Say_f

Used directly by engines that forward console chat to ClientCommand. The
official KEX lobby consumes these commands before the game DLL; Q2PRO-family
KEX hosts intentionally forward them for game-side handling.
==================
*/
static void Cmd_Say_f(gentity_t *ent, bool arg0) {
	const mm_arena_chat_scope_t scope =
		GT(GT_ARENA) && MM_Arena_Id(ent) > 0
			? mm_arena_chat_scope_t::Arena
			: mm_arena_chat_scope_t::World;
	Cmd_SayScoped_f(ent, arg0, scope, false);
}

/*
=================
Cmd_Say_Team_f

See Cmd_Say_f.
=================
*/
static void Cmd_Say_Team_f(gentity_t *who, const char *) {
	Cmd_SayScoped_f(who, false,
		GT(GT_ARENA) ? mm_arena_chat_scope_t::Team : mm_arena_chat_scope_t::World,
		true);
}

static void Cmd_SayArena_f(gentity_t *ent) {
	const mm_arena_chat_scope_t scope =
		GT(GT_ARENA) && MM_Arena_Id(ent) > 0
			? mm_arena_chat_scope_t::Arena
			: mm_arena_chat_scope_t::World;
	Cmd_SayScoped_f(ent, false, scope, false);
}

static void Cmd_SayWorld_f(gentity_t *ent) {
	Cmd_SayScoped_f(ent, false, mm_arena_chat_scope_t::World, false);
}

/*
=================
Cmd_ListEntities_f
=================
*/
static void Cmd_ListEntities_f(gentity_t *ent) {
	int count = 0;
	const size_t entity_count = CmdEntityCount();

	for (size_t i = 1; i < entity_count; i++) {
		gentity_t *e = &g_entities[i];

		if (!e || !e->inuse)
			continue;
		
		if (gi.argc() > 1) {
			if (!e->classname || !strstr(e->classname, gi.argv(1)))
				continue;
		}
		if (gi.argc() > 3) {
			const auto num = MM_ParseFloatArg(gi.argv(3));
			if (!num) {
				gi.LocClient_Print(ent, PRINT_HIGH, "Invalid X filter.\n");
				return;
			}
			if (e->s.origin[0] != *num)
				continue;
		}
		if (gi.argc() > 4) {
			const auto num = MM_ParseFloatArg(gi.argv(4));
			if (!num) {
				gi.LocClient_Print(ent, PRINT_HIGH, "Invalid Y filter.\n");
				return;
			}
			if (e->s.origin[1] != *num)
				continue;
		}
		if (gi.argc() > 5) {
			const auto num = MM_ParseFloatArg(gi.argv(5));
			if (!num) {
				gi.LocClient_Print(ent, PRINT_HIGH, "Invalid Z filter.\n");
				return;
			}
			if (e->s.origin[2] != *num)
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

	const size_t registered_count = std::min(
		static_cast<size_t>(std::max(0, level.total_monsters)),
		level.monsters_registered.size());
	for (size_t i = 0; i < registered_count; i++) {
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
	P_PublishEngineTeam(ent);
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

static void Cmd_InfoHud_f(gentity_t *ent) {
	MM_CmdInfoHud(ent);
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

static void Cmd_SetWeaponPref_f(gentity_t *ent) {
	MM_CmdSetWeaponPref(ent);
}

static void Cmd_SkillRating_f(gentity_t *ent) {
	MM_CmdSkillRating(ent);
}

// [MuffMode] Per-viewer skin override bodies live in muffmode/mm_skin
static void Cmd_EnemySkin_f(gentity_t *ent) {
	MM_CmdEnemySkin(ent);
}

static void Cmd_TeamSkin_f(gentity_t *ent) {
	MM_CmdTeamSkin(ent);
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

	if (level.match_state == match_state_t::MATCH_WARMUP_READYUP) {
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

	const uint32_t ghost_capacity = min(
		game.maxclients, static_cast<uint32_t>(q_countof(level.ghosts)));
	uint32_t i;
	for (i = 0, g = level.ghosts; i < ghost_capacity; i++, g++)
		if (g->ent)
			break;

	if (i == ghost_capacity) {
		if (!text.length())
			text = "No statistics available.\n";

		gi.Client_Print(ent, PRINT_HIGH, text.c_str());
		return;
	}

	text += "  #|Name            |Score|Kills|Death|BasDf|CarDf|Effcy|\n";

	for (i = 0, g = level.ghosts; i < ghost_capacity; i++, g++) {
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

	if (!targ->inuse || !targ->client) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid client number.\n");
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
	if (ClientIsPlaying(ent->client) && !ent->client->eliminated) {
		gi.Client_Print(ent, PRINT_HIGH, "You must spectate before you can follow.\n");
		return;
	}
	if (gi.argc() < 2) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <client name/num|next|prev|stop|view>\n", gi.argv(0));
		return;
	}

	const char *arg = gi.argv(1);

	if (!Q_strcasecmp(arg, "next")) {
		if (ent->client->follow_target)
			FollowNext(ent);
		else
			GetFollowTarget(ent);
		return;
	}

	if (!Q_strcasecmp(arg, "prev") || !Q_strcasecmp(arg, "previous")) {
		if (ent->client->follow_target)
			FollowPrev(ent);
		else
			FollowCycle(ent, -1);
		return;
	}

	if (!Q_strcasecmp(arg, "stop") || !Q_strcasecmp(arg, "off") || !Q_strcasecmp(arg, "free")) {
		if (ent->client->follow_target)
			FreeFollower(ent);
		else
			gi.Client_Print(ent, PRINT_HIGH, "You are not following anyone.\n");
		return;
	}

	if (!Q_strcasecmp(arg, "view")) {
		ToggleFollowViewMode(ent);
		return;
	}

	gentity_t *follow_ent = ClientEntFromString(arg);

	if (!follow_ent || !follow_ent->inuse || !follow_ent->client) {
		gi.Client_Print(ent, PRINT_HIGH, "Invalid client specified.\n");
		return;
	}

	if (!ClientIsPlaying(follow_ent->client) || follow_ent->client->eliminated) {
		gi.Client_Print(ent, PRINT_HIGH, "Specified client is not playing.\n");
		return;
	}

	if (!FollowTargetAllowed(ent, follow_ent)) {
		gi.Client_Print(ent, PRINT_HIGH, "You can only follow teammates while eliminated.\n");
		return;
	}

	SetFollowTarget(ent, follow_ent);
	UpdateFollowCamera(ent);
}

/*
=================
Cmd_FollowKiller_f
=================
*/
static void Cmd_FollowKiller_f(gentity_t *ent) {
	MM_CmdFollowKiller(ent);
}

/*
=================
Cmd_FollowLeader_f
=================
*/
static void Cmd_FollowLeader_f(gentity_t *ent) {
	MM_CmdFollowLeader(ent);
}

/*
=================
Cmd_FollowPowerup_f
=================
*/
static void Cmd_FollowPowerup_f(gentity_t *ent) {
	MM_CmdFollowPowerup(ent);
}

/*
=================
Cmd_FollowView_f
=================
*/
static void Cmd_FollowView_f(gentity_t *ent) {
	MM_CmdFollowView(ent);
}

static void Cmd_FollowNext_f(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (ent->client->follow_target)
		FollowNext(ent);
	else
		GetFollowTarget(ent);
}

static void Cmd_FollowPrev_f(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	if (ent->client->follow_target)
		FollowPrev(ent);
	else
		FollowCycle(ent, -1);
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

// [MuffMode] These legacy RA commands remain registered for compatibility.
// Their arena bodies return false outside MuffMode Arena, so make that state
// explicit instead of silently accepting a command with no effect.
static void Cmd_ArenaOnlyUnavailable_f(gentity_t *ent) {
	if (!ent || !ent->client)
		return;
	gi.LocClient_Print(ent, PRINT_HIGH,
		"{} is only available in MuffMode Arena.\n", gi.argv(0));
}

static void Cmd_TeamName_f(gentity_t *ent) {
	if (!MM_Arena_TeamNameCommand(ent))
		Cmd_ArenaOnlyUnavailable_f(ent);
}

static void Cmd_TeamKick_f(gentity_t *ent) {
	if (!MM_Arena_TeamKickCommand(ent))
		Cmd_ArenaOnlyUnavailable_f(ent);
}

static void Cmd_TeamMute_f(gentity_t *ent) {
	if (!MM_Arena_TeamMuteCommand(ent, true))
		Cmd_ArenaOnlyUnavailable_f(ent);
}

static void Cmd_TeamUnmute_f(gentity_t *ent) {
	if (!MM_Arena_TeamMuteCommand(ent, false))
		Cmd_ArenaOnlyUnavailable_f(ent);
}

static void Cmd_SpecInvite_f(gentity_t *ent) {
	if (!MM_Arena_SpectatorInviteCommand(ent, true))
		Cmd_ArenaOnlyUnavailable_f(ent);
}

static void Cmd_SpecRevoke_f(gentity_t *ent) {
	if (!MM_Arena_SpectatorInviteCommand(ent, false))
		Cmd_ArenaOnlyUnavailable_f(ent);
}

static void Cmd_SpecWho_f(gentity_t *ent) {
	if (!MM_Arena_SpecWhoCommand(ent))
		Cmd_ArenaOnlyUnavailable_f(ent);
}

// [MuffMode] Admin team command bodies live in muffmode/mm_team
static void Cmd_SetTeam_f(gentity_t *ent) {
	MM_CmdSetTeam(ent);
}

static void Cmd_Arena_f(gentity_t *ent) {
	MM_Arena_Cmd(ent);
}

static void Cmd_Shuffle_f(gentity_t *ent) {
	MM_CmdShuffle(ent);
}

static void Cmd_BalanceTeams_f(gentity_t *ent) {
	MM_CmdBalanceTeams(ent);
}

// [MuffMode] Admin match-control command bodies live in muffmode/mm_admin
static void Cmd_StartMatch_f(gentity_t *ent) {
	MM_CmdStartMatch(ent);
}

static void Cmd_EndMatch_f(gentity_t *ent) {
	MM_CmdEndMatch(ent);
}

static void Cmd_ResetMatch_f(gentity_t *ent) {
	MM_CmdResetMatch(ent);
}

static void Cmd_ForceVote_f(gentity_t *ent) {
	MM_CmdForceVote(ent);
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

static void Cmd_MapRestart_f(gentity_t *ent) {
	MM_CmdMapRestart(ent);
}

static void Cmd_NextMap_f(gentity_t *ent) {
	MM_CmdNextMap(ent);
}

static void Cmd_Admin_f(gentity_t *ent) {
	MM_CmdAdmin(ent);
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
	if (!MM_Arena_ReadyCommand(ent))
		MM_CmdReady(ent);
}

static void Cmd_NotReady_f(gentity_t *ent) {
	if (!MM_Arena_SetReady(ent, false))
		MM_CmdNotReady(ent);
}

static void Cmd_ReadyUp_f(gentity_t *ent) {
	if (!MM_Arena_ToggleReady(ent))
		MM_CmdReadyUp(ent);
}

static void Cmd_Hook_f(gentity_t *ent) {
	const bool global_offhand =
		g_allow_grapple->integer && g_grapple_offhand->integer;
	const bool hook_allowed = GT(GT_ARENA)
		? MM_Arena_GrappleEnabled(ent)
		: global_offhand;
	if (!hook_allowed)
		return;
	if (GT(GT_ARENA) && !MM_Arena_CombatEnabled(ent))
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

static void Cmd_MapPick_f(gentity_t *ent) {
	MM_CmdMapPick(ent);
}

// [MuffMode] Post-match awards reel body lives in muffmode/mm_awards
static void Cmd_Awards_f(gentity_t *ent) {
	MM_CmdAwards(ent);
}

static void Cmd_MapPool_f(gentity_t *ent) {
	MM_CmdMapPool(ent);
}

static void Cmd_MapCycle_f(gentity_t *ent) {
	MM_CmdMapCycle(ent);
}

static void Cmd_LoadMapPool_f(gentity_t *ent) {
	MM_CmdLoadMapPool(ent);
}

static void Cmd_LoadMapCycle_f(gentity_t *ent) {
	MM_CmdLoadMapCycle(ent);
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

static void Cmd_Loc_f(gentity_t *ent) {
	MM_CmdLoc(ent);
}

static void Cmd_Motd_f(gentity_t *ent) {
	MM_CmdMotd(ent);
}

// =========================================

// [MuffMode] Command table mixes vanilla and MuffMode commands; MuffMode
// bodies live in muffmode/ modules behind thin Cmd_*_f wrappers above.
cmds_t client_cmds[] = {
	{"admin",			Cmd_Admin_f,			CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"alertall",		Cmd_AlertAll_f,			CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"announcer",		Cmd_Announcer_f,		CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"arena",			Cmd_Arena_f,			CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"awards",			Cmd_Awards_f,			CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"balance",			Cmd_BalanceTeams_f,		CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"boot",			Cmd_Boot_f,				CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"callvote",		Cmd_CallVote_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"captain",			Cmd_Captain_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"checkpoi",		Cmd_CheckPOI_f,			CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"clear_ai_enemy",	Cmd_Clear_AI_Enemy_f,	CF_CHEAT_PROTECT},
	{"cv",				Cmd_CallVote_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"drop", 			Cmd_Drop_f,				CF_NONE},
	{"drop_index", 		Cmd_Drop_f,				CF_NONE},
	{"doctor", 			Cmd_Doctor_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"endmatch", 		Cmd_EndMatch_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"enemyskin",		Cmd_EnemySkin_f,		CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"eskin",			Cmd_EnemySkin_f,		CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"fm", 				Cmd_FragMessages_f,		CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"follow",			Cmd_Follow_f,			CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"followkiller",	Cmd_FollowKiller_f,		CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"followleader",	Cmd_FollowLeader_f,		CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"follownext",		Cmd_FollowNext_f,		CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"followprev",		Cmd_FollowPrev_f,		CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"followpowerup",	Cmd_FollowPowerup_f,	CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"followview",		Cmd_FollowView_f,		CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"forcevote",		Cmd_ForceVote_f,		CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"forfeit",			Cmd_Forfeit_f,			CF_ALLOW_DEAD},
	{"gametype",		Cmd_Gametype_f,			CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC},	// listing is open to all; changing is gated to admins inside the handler
	{"ghost",			Cmd_Ghost_f,			CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"give",			Cmd_Give_f,				CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"god",				Cmd_God_f,				CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"help",			Cmd_Help_f,				CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"hook",			Cmd_Hook_f,				CF_NONE},
	{"hud_dump",		Cmd_HudDump_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"id",				Cmd_CrosshairID_f,		CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"immortal",		Cmd_Immortal_f,			CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"invdrop",			Cmd_InvDrop_f,			CF_NONE},
	{"inven",			Cmd_Inven_f,			CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"invnext",			Cmd_InvNext_f,			CF_ALLOW_SPEC | CF_ALLOW_DEAD | CF_ALLOW_INT},	//spec for menu up/down, dead for horde spectators
	{"invnextp",		Cmd_InvNextP_f,			CF_NONE},
	{"invnextw",		Cmd_InvNextW_f,			CF_NONE},
	{"invprev",			Cmd_InvPrev_f,			CF_ALLOW_SPEC | CF_ALLOW_DEAD | CF_ALLOW_INT},	//spec for menu up/down, dead for horde spectators
	{"invprevp",		Cmd_InvPrevP_f,			CF_NONE},
	{"invprevw",		Cmd_InvPrevW_f,			CF_NONE},
	{"invuse",			Cmd_InvUse_f,			CF_ALLOW_SPEC | CF_ALLOW_INT},	//spec for menu up/down
	{"kb",				Cmd_KillBeep_f,			CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"kill",			Cmd_Kill_f,				CF_NONE},
	{"kill_ai",			Cmd_Kill_AI_f,			CF_CHEAT_PROTECT},
	{"listentities",	Cmd_ListEntities_f,		CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"listmonsters",	Cmd_ListMonsters_f,		CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"load_mapcycle",	Cmd_LoadMapCycle_f,		CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"load_mappool",	Cmd_LoadMapPool_f,		CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"loadmotd",		Cmd_LoadMotd_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"loc",				Cmd_Loc_f,				CF_NONE},
	{"lockteam",		Cmd_LockTeam_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"map_restart",		Cmd_MapRestart_f,		CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"mapcycle",		Cmd_MapCycle_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"mapinfo",			Cmd_MapInfo_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"maplist",			Cmd_MapList_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"mappick",			Cmd_MapPick_f,			CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"mappool",			Cmd_MapPool_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"motd",			Cmd_Motd_f,				CF_ALLOW_SPEC | CF_ALLOW_INT},
	{"mymap",			Cmd_MyMap_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"nextmap",			Cmd_NextMap_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"noclip",			Cmd_NoClip_f,			CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"notarget",		Cmd_NoTarget_f,			CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"notready",		Cmd_NotReady_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"novisible",		Cmd_NoVisible_f,		CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"players",			Cmd_Players_f,			CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"playtime",		Cmd_PlayersJoinTime_f,	CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"playrank",		Cmd_PlayersRanked_f,	CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"putaway",			Cmd_PutAway_f,			CF_ALLOW_SPEC},	//spec for menu close
	{"ready",			Cmd_Ready_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"readyall",		Cmd_ReadyAll_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"readyteam",		Cmd_ReadyTeam_f,		CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"readyup",			Cmd_ReadyUp_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"resetmatch",		Cmd_ResetMatch_f,		CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"ruleset",			Cmd_Ruleset_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"say_arena",		Cmd_SayArena_f,			CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"say_world",		Cmd_SayWorld_f,			CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"score",			Cmd_Score_f,			CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"setpoi",			Cmd_SetPOI_f,			CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"setmap",			Cmd_SetMap_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"setteam",			Cmd_SetTeam_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"setweaponpref",	Cmd_SetWeaponPref_f,	CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"shuffle",			Cmd_Shuffle_f,			CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"spawn",			Cmd_Spawn_f,			CF_ADMIN_ONLY | CF_ALLOW_SPEC},
	{"specinvite",		Cmd_SpecInvite_f,		CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"specrevoke",		Cmd_SpecRevoke_f,		CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"specwho",			Cmd_SpecWho_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"sr",				Cmd_SkillRating_f,		CF_ALLOW_DEAD | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"startmatch",		Cmd_StartMatch_f,		CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"stats",			Cmd_Stats_f,			CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"target",			Cmd_Target_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"team",			Cmd_Team_f,				CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"teamcaptain",		Cmd_Captain_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"teamkick",		Cmd_TeamKick_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"teamlock",		Cmd_LockTeam_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"teammute",		Cmd_TeamMute_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"teamname",		Cmd_TeamName_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"teamskin",		Cmd_TeamSkin_f,			CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"teamunlock",		Cmd_UnlockTeam_f,		CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"teamunmute",		Cmd_TeamUnmute_f,		CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"tskin",			Cmd_TeamSkin_f,			CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"teleport",		Cmd_Teleport_f,			CF_ALLOW_SPEC | CF_CHEAT_PROTECT},
	{"time-out",		Cmd_TimeOut_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"time-in",			Cmd_TimeIn_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"timein",			Cmd_TimeIn_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"timeout",			Cmd_TimeOut_f,			CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"timer",			Cmd_Timer_f,			CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"infohud",			Cmd_InfoHud_f,			CF_ALLOW_SPEC | CF_ALLOW_DEAD},
	{"unhook",			Cmd_UnHook_f,			CF_NONE},
	{"unlockteam",		Cmd_UnlockTeam_f,		CF_ALLOW_DEAD | CF_ALLOW_SPEC},
	{"unreadyall",		Cmd_UnReadyAll_f,		CF_ADMIN_ONLY | CF_ALLOW_INT | CF_ALLOW_SPEC},
	{"use",				Cmd_Use_f,				CF_NONE},
	{"use_index",		Cmd_Use_f,				CF_NONE},
	{"use_index_only",	Cmd_Use_f,				CF_NONE},
	{"use_only",		Cmd_Use_f,				CF_NONE},
	{"vote",			Cmd_Vote_f,				CF_ALLOW_DEAD | CF_ALLOW_SPEC},
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

	// Official KEX consumes built-in chat in its lobby layer and never sends it
	// here. Q2PRO-family KEX hosts expose their filesystem extension and forward
	// chat to ClientCommand so mods can supply the recipient policy.
	if (MM_ChatCommandsUseGameDispatch()) {
		if (!Q_strcasecmp(cmd, "say")) {
			Cmd_Say_f(ent, false);
			return;
		}
		if (!Q_strcasecmp(cmd, "say_team") || !Q_strcasecmp(cmd, "steam")) {
			if (Teams() || GT(GT_ARENA))
				Cmd_Say_Team_f(ent, gi.args());
			else
				Cmd_Say_f(ent, false);
			return;
		}
	}

	// The reconnect snapshot is not committed yet. Do not let team, vote,
	// forfeit, or admin commands mutate external match state during the bounded
	// reinstatement delay. Chat above remains available where the host forwards it.
	if (MM_Ghost_IsPendingRestore(ent))
		return;

	if (!cc) {
		// [MuffMode] Only authenticated admins may change exact, known item overrides.
		if (MM_IsExactArgcValid(gi.argc(), 2) &&
			MM_IsKnownItemOverrideCvarName(cmd,
				std::string_view(&level.mapname[0]))) {
			if (MM_ClientMaySetItemOverride(
					g_allow_admin->integer != 0, ent->client->sess.admin))
				gi.cvar_forceset(cmd, gi.argv(1));
			else
				gi.LocClient_Print(ent, PRINT_HIGH, "Only admins can use this command.\n");
		} else {
			gi.LocClient_Print(ent, PRINT_HIGH, "Invalid client command: \"{}\"\n", cmd);
		}
		return;
	}

	if (HasCmdFlag(cc->flags, CF_ADMIN_ONLY))
		if (!AdminOk(ent))
			return;

	if (HasCmdFlag(cc->flags, CF_CHEAT_PROTECT))
		if (!CheatsOk(ent))
			return;

	if (!HasCmdFlag(cc->flags, CF_ALLOW_DEAD))
		if (!AliveOk(ent))
			return;

	if (!HasCmdFlag(cc->flags, CF_ALLOW_SPEC))
		if (!SpectatorOk(ent))
			return;

	if (HasCmdFlag(cc->flags, CF_MATCH_ONLY))
		return;

	if (!HasCmdFlag(cc->flags, CF_ALLOW_INT))
		if (level.intermission_time)
			return;

	cc->func(ent);
}
