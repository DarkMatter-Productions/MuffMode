// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_loc.h"
#include "muffmode/mm_loc_parse.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr size_t MM_MAX_LOC_ENTRIES = 4096;
constexpr long MM_MAX_LOC_FILE_LINES = 16384;
constexpr float MM_LOC_COORD_SCALE = 8.0f;

struct loc_entry_t {
	vec3_t origin;
	std::string label;
};

std::vector<loc_entry_t> g_loc_entries;
std::string g_loc_mapname;

bool MM_IsSafeMapname(const char *name)
{
	if (!name || !*name)
		return false;

	if (!std::strcmp(name, ".") || !std::strcmp(name, ".."))
		return false;

	size_t length = 0;
	for (const unsigned char *p = reinterpret_cast<const unsigned char *>(name); *p; p++) {
		if (++length >= MAX_QPATH)
			return false;

		if (*p <= ' ' || *p == '/' || *p == '\\' || *p == ':' || *p == '"' || *p == ';')
			return false;
	}

	return true;
}

bool MM_LocVerbose()
{
	return g_verbose && g_verbose->integer;
}

void MM_LoadLocFile(const char *mapname)
{
	g_loc_entries.clear();

	if (!MM_IsSafeMapname(mapname)) {
		gi.Com_PrintFmt("{}: rejecting unsafe map name for loc load: \"{}\"\n", __FUNCTION__, mapname ? mapname : "");
		return;
	}

	std::string path = "baseq2/locs/";
	path += mapname;
	path += ".loc";

	FILE *f = std::fopen(path.c_str(), "rb");
	if (!f) {
		if (MM_LocVerbose())
			gi.Com_PrintFmt("{}: no loc file for map \"{}\" ({})\n", __FUNCTION__, mapname, path.c_str());
		return;
	}

	char line_buf[1024];
	long lines_read = 0;
	while (std::fgets(line_buf, sizeof(line_buf), f)) {
		if (++lines_read > MM_MAX_LOC_FILE_LINES) {
			gi.Com_PrintFmt("{}: stopping loc load after {} lines: {}\n", __FUNCTION__, MM_MAX_LOC_FILE_LINES, path.c_str());
			break;
		}
		if (g_loc_entries.size() >= MM_MAX_LOC_ENTRIES) {
			gi.Com_PrintFmt("{}: stopping loc load after {} entries: {}\n", __FUNCTION__, MM_MAX_LOC_ENTRIES, path.c_str());
			break;
		}

		float xyz[3];
		std::string label;
		if (!MM_ParseLocLine(line_buf, xyz, label))
			continue;

		loc_entry_t entry;
		entry.origin = { xyz[0] / MM_LOC_COORD_SCALE, xyz[1] / MM_LOC_COORD_SCALE, xyz[2] / MM_LOC_COORD_SCALE };
		entry.label = std::move(label);
		g_loc_entries.push_back(std::move(entry));
	}

	std::fclose(f);

	if (MM_LocVerbose())
		gi.Com_PrintFmt("{}: loaded {} loc entries for map \"{}\"\n", __FUNCTION__, g_loc_entries.size(), mapname);
}

const loc_entry_t *MM_NearestLoc(const vec3_t &origin)
{
	const loc_entry_t *best = nullptr;
	float best_dist = 0.f;

	for (const auto &entry : g_loc_entries) {
		const float dist = (entry.origin - origin).lengthSquared();
		if (!best || dist < best_dist) {
			best = &entry;
			best_dist = dist;
		}
	}

	return best;
}

struct loc_item_name_t {
	item_id_t id;
	const char *name;
};

constexpr loc_item_name_t MM_LOC_ITEM_NAMES[] = {
	{ IT_WEAPON_SHOTGUN, "SG" },
	{ IT_WEAPON_SSHOTGUN, "SSG" },
	{ IT_WEAPON_MACHINEGUN, "MG" },
	{ IT_WEAPON_CHAINGUN, "CG" },
	{ IT_WEAPON_GLAUNCHER, "GL" },
	{ IT_WEAPON_PROXLAUNCHER, "PROX" },
	{ IT_WEAPON_RLAUNCHER, "RL" },
	{ IT_WEAPON_HYPERBLASTER, "HB" },
	{ IT_WEAPON_IONRIPPER, "RIPPER" },
	{ IT_WEAPON_PLASMABEAM, "PB" },
	{ IT_WEAPON_RAILGUN, "RG" },
	{ IT_WEAPON_PHALANX, "PHALANX" },
	{ IT_WEAPON_ETF_RIFLE, "ETF" },
	{ IT_WEAPON_DISRUPTOR, "DISRUPTOR" },
	{ IT_WEAPON_BFG, "BFG" },
	{ IT_POWERUP_QUAD, "QUAD" },
	{ IT_POWERUP_DOUBLE, "DD" },
	{ IT_POWERUP_PROTECTION, "INVUL" },
	{ IT_POWERUP_INVISIBILITY, "INVIS" },
	{ IT_POWERUP_HASTE, "HASTE" },
	{ IT_POWERUP_REGEN, "REGEN" },
	{ IT_POWERUP_ENVIROSUIT, "ENVIRO" },
	{ IT_POWERUP_REBREATHER, "REBREATHER" },
	{ IT_POWERUP_SILENCER, "SILENCER" },
	{ IT_HEALTH_MEGA, "MEGA" },
};

const char *MM_LocItemShortName(const gitem_t *item)
{
	if (!item)
		return "?";

	for (const auto &entry : MM_LOC_ITEM_NAMES) {
		if (entry.id == item->id)
			return entry.name;
	}
	return (item->pickup_name && *item->pickup_name) ? item->pickup_name : "?";
}

bool MM_IsLocLandmark(const gentity_t *ent)
{
	if (!ent || !ent->item)
		return false;

	const item_flags_t flags = ent->item->flags;
	if ((flags & IF_WEAPON) && !(flags & IF_AMMO))
		return true;
	if (flags & IF_POWERUP)
		return true;
	if (ent->item->id == IT_HEALTH_MEGA)
		return true;

	return false;
}

bool MM_NearestItemLoc(const gentity_t *ent, std::string &out_label)
{
	if (!ent)
		return false;

	const gentity_t *best = nullptr;
	float best_dist = 0.f;

	for (uint32_t i = 1; i < globals.num_entities; i++) {
		const gentity_t *candidate = &g_entities[i];
		if (!candidate->inuse || !MM_IsLocLandmark(candidate))
			continue;

		const float dist = (ent->s.origin - candidate->s.origin).lengthSquared();
		if ((best && dist >= best_dist) || !gi.inPVS(ent->s.origin, candidate->s.origin, false))
			continue;

		best = candidate;
		best_dist = dist;
	}

	if (!best) {
		out_label = (ent->waterlevel > WATER_NONE) ? "water" : level.mapname;
		return true;
	}

	const char *modifier = "";
	for (uint32_t i = 1; i < globals.num_entities; i++) {
		const gentity_t *candidate = &g_entities[i];
		if (candidate == best || !candidate->inuse || !MM_IsLocLandmark(candidate))
			continue;
		if (candidate->item->id == best->item->id) {
			modifier = (candidate->s.origin.z < best->s.origin.z) ? "upper " : "lower ";
			break;
		}
	}

	out_label = std::string(modifier) + MM_LocItemShortName(best->item);
	return true;
}

void MM_LocReplaceAll(std::string &text, const char *token, const std::string &replacement)
{
	const size_t token_len = std::strlen(token);
	size_t pos = 0;
	while ((pos = text.find(token, pos)) != std::string::npos) {
		text.replace(pos, token_len, replacement);
		pos += replacement.size();
	}
}

std::string MM_LocNearby(gentity_t *ent, bool team_only)
{
	std::string out;
	for (auto other : active_clients()) {
		if (other == ent || !other->client || other->health <= 0)
			continue;
		if (team_only && other->client->sess.team != ent->client->sess.team)
			continue;
		if (!visible(ent, other))
			continue;

		if (!out.empty())
			out += ", ";
		out += other->client->resp.netname;

		if (out.size() > 96)
			break;
	}
	return out;
}

void MM_ExpandStatusMacros(gentity_t *ent, std::string &body)
{
	gclient_t *client = ent->client;

	if (body.find("%h") != std::string::npos)
		MM_LocReplaceAll(body, "%h", std::string(G_Fmt("H:{}", ent->health)));

	if (body.find("%a") != std::string::npos) {
		const item_id_t armor_index = ArmorIndex(ent);
		const int armor = armor_index ? client->pers.inventory[armor_index] : 0;
		const bool power_armor = client->pers.inventory[IT_POWER_SCREEN] > 0 || client->pers.inventory[IT_POWER_SHIELD] > 0;
		const int power = power_armor ? client->pers.inventory[IT_AMMO_CELLS] : 0;
		const std::string armor_text = power > 0
			? std::string(G_Fmt("A:{} P:{}", armor, power))
			: std::string(G_Fmt("A:{}", armor));
		MM_LocReplaceAll(body, "%a", armor_text);
	}

	if (body.find("%w") != std::string::npos) {
		std::string weapon = "none";
		if (client->pers.weapon) {
			const gitem_t *weapon_item = client->pers.weapon;
			weapon = (weapon_item->ammo != IT_NULL)
				? std::string(G_Fmt("{}:{}", MM_LocItemShortName(weapon_item), client->pers.inventory[weapon_item->ammo]))
				: std::string(MM_LocItemShortName(weapon_item));
		}
		MM_LocReplaceAll(body, "%w", weapon);
	}

	if (body.find("%m") != std::string::npos) {
		std::string ammo = "none";
		if (client->pers.weapon && client->pers.weapon->ammo != IT_NULL) {
			const gitem_t *ammo_item = GetItemByIndex(client->pers.weapon->ammo);
			if (ammo_item && ammo_item->pickup_name && *ammo_item->pickup_name)
				ammo = ammo_item->pickup_name;
		}
		MM_LocReplaceAll(body, "%m", ammo);
	}

	if (body.find("%n") != std::string::npos)
		MM_LocReplaceAll(body, "%n", MM_LocNearby(ent, true));
	if (body.find("%N") != std::string::npos)
		MM_LocReplaceAll(body, "%N", MM_LocNearby(ent, false));
}

} // namespace

void MM_ClearLocCache()
{
	g_loc_entries.clear();
	g_loc_mapname.clear();
}

bool MM_LocLabelForEntity(gentity_t *ent, std::string &out_label)
{
	out_label.clear();

	if (!ent || !ent->client || !g_loc || !g_loc->integer)
		return false;

	if (g_loc_mapname != level.mapname) {
		MM_LoadLocFile(level.mapname);
		g_loc_mapname = level.mapname;
	}

	if (const loc_entry_t *loc = MM_NearestLoc(ent->s.origin)) {
		out_label = loc->label;
		return true;
	}

	return g_loc_items && g_loc_items->integer && MM_NearestItemLoc(ent, out_label);
}

void MM_CmdLoc(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	if (!g_loc || !g_loc->integer) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Location callouts are disabled.\n");
		return;
	}

	std::string label;
	if (!MM_LocLabelForEntity(ent, label)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "No location data is available for this map.\n");
		return;
	}

	std::string body = MM_BuildLocBody(gi.args(), label.c_str());
	if (body.empty()) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: loc <message containing a macro, e.g. bind x \"cmd loc at %l\">.\n");
		return;
	}

	MM_ExpandStatusMacros(ent, body);
	constexpr size_t MM_MAX_LOC_FINAL = 240;
	if (body.size() > MM_MAX_LOC_FINAL)
		body.resize(MM_MAX_LOC_FINAL);

	const bool team_game = Teams();
	const std::string text = team_game
		? std::string(G_Fmt("(TEAM) {}: {}\n", ent->client->resp.netname, body.c_str()))
		: std::string(G_Fmt("{}: {}\n", ent->client->resp.netname, body.c_str()));

	for (auto recipient : active_clients()) {
		if (team_game && recipient->client->sess.team != ent->client->sess.team)
			continue;

		gi.WriteByte(svc_print);
		gi.WriteByte(PRINT_CHAT);
		gi.WriteString(text.c_str());
		gi.unicast(recipient, true);
	}
}
