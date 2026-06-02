// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_admin.h"
#include "muffmode/mm_vote.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace {

void Handicap_SanitizeString(const char *in, char *out)
{
	while (*in)
	{
		if (*in < ' ')
		{
			in++;
			continue;
		}
		*out = (char)tolower(*in);
		out++;
		in++;
	}
	*out = '\0';
}

int Handicap_ClientNumberFromName(gentity_t *to, const char *name)
{
	char s2[MAX_STRING_CHARS];
	char n2[MAX_STRING_CHARS];

	// Numeric values are slot numbers.
	if (name[0] >= '0' && name[0] <= '9')
	{
		int idnum = atoi(name);
		if (idnum >= 0 && (unsigned)idnum < (unsigned)game.maxclients)
		{
			gclient_t *cl = &game.clients[idnum];
			if (cl->pers.connected)
				return idnum;
		}
		gi.LocClient_Print(to, PRINT_HIGH, "Bad client slot: {}\n", idnum);
		return -1;
	}

	Handicap_SanitizeString(name, s2);
	for (size_t idnum = 0; idnum < game.maxclients; idnum++)
	{
		gclient_t *cl = &game.clients[idnum];
		if (!cl->pers.connected)
			continue;
		Handicap_SanitizeString(cl->resp.netname, n2);
		if (!strcmp(n2, s2))
			return (int)idnum;
	}

	gi.LocClient_Print(to, PRINT_HIGH, "User {} is not on the server.\n", name);
	return -1;
}

item_id_t Handicap_WeaponIDFromName(const char *name)
{
	if (!Q_strcasecmp(name, "railgun") || !Q_strcasecmp(name, "rail"))
		return IT_WEAPON_RAILGUN;
	if (!Q_strcasecmp(name, "chaingun") || !Q_strcasecmp(name, "chain"))
		return IT_WEAPON_CHAINGUN;
	if (!Q_strcasecmp(name, "rlauncher") || !Q_strcasecmp(name, "rocket") || !Q_strcasecmp(name, "rl"))
		return IT_WEAPON_RLAUNCHER;
	if (!Q_strcasecmp(name, "all"))
		return IT_NULL; // Special case for "all" (restricts railgun, chaingun, rlauncher).
	return IT_NULL;
}

void Handicap_ApplyWeaponRestriction(gentity_t *target, item_id_t weapon_id, bool restrict)
{
	if (!target || !target->client)
		return;

	if (weapon_id == IT_NULL)
	{
		constexpr uint32_t HANDICAP_ALL_WEAPONS =
			(1U << (IT_WEAPON_RAILGUN - FIRST_WEAPON)) |
			(1U << (IT_WEAPON_CHAINGUN - FIRST_WEAPON)) |
			(1U << (IT_WEAPON_RLAUNCHER - FIRST_WEAPON));

		if (restrict)
			target->client->handicap.restricted_weapons |= HANDICAP_ALL_WEAPONS;
		else
			target->client->handicap.restricted_weapons &= ~HANDICAP_ALL_WEAPONS;
	}
	else
	{
		uint32_t weapon_bit = 1U << (weapon_id - FIRST_WEAPON);

		if (restrict)
			target->client->handicap.restricted_weapons |= weapon_bit;
		else
			target->client->handicap.restricted_weapons &= ~weapon_bit;
	}

	if (target->client->pers.weapon && (target->client->pers.weapon->flags & IF_WEAPON))
	{
		uint32_t weapon_bit = 1U << (target->client->pers.weapon->id - FIRST_WEAPON);
		if (target->client->handicap.restricted_weapons & weapon_bit)
			NoAmmoWeaponChange(target, false);
	}
}

} // namespace

void MM_CmdDoctor(gentity_t *ent)
{
	if (gi.argc() > 1)
	{
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

	if (g_dm_do_readyup->integer && !g_dm_do_warmup->integer)
	{
		report("ERROR",
			"g_dm_do_readyup is enabled while g_dm_do_warmup is disabled.",
			"set g_dm_do_warmup 1");
	}

	if (g_dm_do_readyup->integer && (g_warmup_ready_percentage->value <= 0.f || g_warmup_ready_percentage->value > 1.f))
	{
		report("ERROR",
			"g_warmup_ready_percentage must be in range (0.0, 1.0] when readyup is enabled.",
			"set g_warmup_ready_percentage 0.51");
	}

	if (minplayers->integer > maxplayers->integer)
	{
		report("ERROR",
			"minplayers is greater than maxplayers.",
			"set minplayers <= maxplayers");
	}

	if (maxplayers->integer <= 0)
	{
		report("ERROR",
			"maxplayers must be greater than zero.",
			"set maxplayers 2");
	}

	if (g_vote_limit->integer < 0)
	{
		report("ERROR",
			"g_vote_limit cannot be negative.",
			"set g_vote_limit 0");
	}

	if (!g_allow_voting->integer && g_allow_spec_vote->integer)
	{
		report("WARN",
			"g_allow_spec_vote is enabled while voting is globally disabled.",
			"set g_allow_voting 1 or set g_allow_spec_vote 0");
	}

	if (!g_allow_voting->integer && g_allow_vote_midgame->integer)
	{
		report("WARN",
			"g_allow_vote_midgame is enabled while voting is globally disabled.",
			"set g_allow_voting 1 or set g_allow_vote_midgame 0");
	}

	if (g_dm_do_readyup->integer && g_warmup_ready_percentage->value >= 0.99f)
	{
		report("WARN",
			"g_warmup_ready_percentage is very high; matches may stall waiting for nearly all players.",
			"set g_warmup_ready_percentage between 0.50 and 0.80");
	}

	if (g_dm_overtime->integer > 0 && (GT(GT_DUEL) == 0))
	{
		report("INFO",
			"g_dm_overtime is set but currently only applies to Duel.",
			"Switch to Duel or leave as a preset for later.");
	}

	if ((GTF(GTF_ROUNDS) & GTF_ROUNDS) == 0)
	{
		if (roundlimit->integer != 8)
		{
			report("INFO",
				"roundlimit is non-default in a non-round gametype.",
				"No action needed unless this was unintended.");
		}

		if (roundtimelimit->integer != 2)
		{
			report("INFO",
				"roundtimelimit is non-default in a non-round gametype.",
				"No action needed unless this was unintended.");
		}

		if (g_round_countdown->integer != 10)
		{
			report("INFO",
				"g_round_countdown is non-default in a non-round gametype.",
				"No action needed unless this was unintended.");
		}
	}
	else
	{
		if (roundlimit->integer <= 0)
		{
			report("WARN",
				"roundlimit is <= 0 in a round-based gametype.",
				"set roundlimit 8");
		}

		if (roundtimelimit->value <= 0.f)
		{
			report("WARN",
				"roundtimelimit is <= 0 in a round-based gametype.",
				"set roundtimelimit 2");
		}

		if (g_round_countdown->integer < 0)
		{
			report("WARN",
				"g_round_countdown is negative.",
				"set g_round_countdown 10");
		}
	}

	if (!g_allow_voting->integer && (g_vote_flags->integer != 0 || g_vote_limit->integer != 3))
	{
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

void MM_CmdGametype(gentity_t *ent)
{
	if (!deathmatch->integer)
		return;

	if (gi.argc() < 2)
	{
		std::string votable_list = MM_GetVotableGametypesList();
		if (!votable_list.empty())
			gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <{}>\nChanges current gametype. Current gametype is {} ({}).\n", gi.argv(0), votable_list.c_str(), gt_long_name[g_gametype->integer], g_gametype->integer);
		else
			gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <gametype>\nChanges current gametype. Current gametype is {} ({}).\n", gi.argv(0), gt_long_name[g_gametype->integer], g_gametype->integer);
		return;
	}

	gametype_t gt = GT_IndexFromString(gi.argv(1));
	if (gt == GT_NONE)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid gametype.\n");
		std::string votable_list = MM_GetVotableGametypesList();
		if (!votable_list.empty())
			gi.LocClient_Print(ent, PRINT_HIGH, "Valid gametypes are: {}\n", votable_list.c_str());
		return;
	}

	if (g_votable_gametypes->string[0] && !MM_IsGametypeVotable(gt))
		gi.LocClient_Print(ent, PRINT_HIGH, "Warning: This gametype is not in the votable list, but setting it anyway (admin override).\n");

	ChangeGametype(gt);
}

void MM_CmdRuleset(gentity_t *ent)
{
	if (!deathmatch->integer)
		return;

	if (gi.argc() < 2)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <q2re|mm|q3a|q2reb|qc>\nChanges current ruleset. Current ruleset is {} ({}).\n", gi.argv(0), rs_long_name[(int)game.ruleset], (int)game.ruleset);
		return;
	}

	ruleset_t rs = RS_IndexFromString(gi.argv(1));
	if (rs == RS_NONE)
	{
		gi.Client_Print(ent, PRINT_HIGH, "Invalid ruleset.\n");
		return;
	}

	gi.cvar_forceset("g_ruleset", G_Fmt("{}", (int)rs).data());
}

void MM_CmdSetMap(gentity_t *ent)
{
	if (gi.argc() < 2)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} [mapname]\nChanges to a map within the map pool or list.", gi.argv(0));
		return;
	}

	if (!MM_IsMapValid(gi.argv(1)))
	{
		gi.Client_Print(ent, PRINT_HIGH, "Map name is not valid.\n");
		return;
	}

	gi.LocBroadcast_Print(PRINT_HIGH, "[ADMIN]: Changing map to {}\n", gi.argv(1));
	gi.AddCommandString(G_Fmt("gamemap \"{}\"\n", gi.argv(1)).data());
}

void MM_CmdHandicap(gentity_t *ent)
{
	if (notGT(GT_DUEL))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Handicap system only works in duel mode.\n");
		return;
	}

	if (gi.argc() < 4)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <player> <weapon> <on|off>\n", gi.argv(0));
		gi.LocClient_Print(ent, PRINT_HIGH, "Weapons: railgun, chaingun, rlauncher, all\n");
		return;
	}

	const char *player_name = gi.argv(1);
	const char *weapon_name = gi.argv(2);
	const char *onoff = gi.argv(3);

	int clientnum = Handicap_ClientNumberFromName(ent, player_name);
	if (clientnum < 0)
		return;

	gentity_t *target = &g_entities[1 + clientnum];
	if (!target->inuse || !target->client || !ClientIsPlaying(target->client))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Player '{}' is not playing.\n", player_name);
		return;
	}

	item_id_t weapon_id = Handicap_WeaponIDFromName(weapon_name);
	if (weapon_id == IT_NULL && Q_strcasecmp(weapon_name, "all"))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid weapon '{}'. Valid: railgun, chaingun, rlauncher, all\n", weapon_name);
		return;
	}

	bool restrict = false;
	if (!Q_strcasecmp(onoff, "on") || !Q_strcasecmp(onoff, "1"))
		restrict = true;
	else if (!Q_strcasecmp(onoff, "off") || !Q_strcasecmp(onoff, "0"))
		restrict = false;
	else
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Invalid value '{}'. Use 'on' or 'off'.\n", onoff);
		return;
	}

	Handicap_ApplyWeaponRestriction(target, weapon_id, restrict);

	const char *weapon_display = weapon_name;
	if (weapon_id != IT_NULL)
	{
		gitem_t *weapon_item = GetItemByIndex(weapon_id);
		if (weapon_item)
			weapon_display = weapon_item->pickup_name;
	}
	else
	{
		weapon_display = "all handicap weapons (railgun, chaingun, rlauncher)";
	}

	gi.LocBroadcast_Print(PRINT_HIGH, "[ADMIN]: {} {} for {}.\n",
		weapon_display, restrict ? "restricted" : "unrestricted",
		target->client->resp.netname);
}

void MM_CmdHandicapClear(gentity_t *ent)
{
	if (notGT(GT_DUEL))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Handicap system only works in duel mode.\n");
		return;
	}

	if (gi.argc() < 2)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {} <player>\n", gi.argv(0));
		return;
	}

	const char *player_name = gi.argv(1);

	int clientnum = Handicap_ClientNumberFromName(ent, player_name);
	if (clientnum < 0)
		return;

	gentity_t *target = &g_entities[1 + clientnum];
	if (!target->inuse || !target->client)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Player '{}' is not active.\n", player_name);
		return;
	}

	target->client->handicap.restricted_weapons = 0;
	target->client->handicap.damage_dealt_multiplier = 1.0f;
	target->client->handicap.damage_received_multiplier = 1.0f;
	target->client->handicap.health_multiplier = 1.0f;

	if (target->client->pers.weapon && (target->client->pers.weapon->flags & IF_WEAPON))
	{
		uint32_t weapon_bit = 1U << (target->client->pers.weapon->id - FIRST_WEAPON);
		if (target->client->handicap.restricted_weapons & weapon_bit)
			NoAmmoWeaponChange(target, false);
	}

	gi.LocBroadcast_Print(PRINT_HIGH, "[ADMIN]: All handicaps cleared for {}.\n",
		target->client->resp.netname);
}
