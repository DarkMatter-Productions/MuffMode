// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include "g_local.h"
#include "monsters/m_player.h"
#include "bots/bot_includes.h"
#include "muffmode/mm_ruleset.h"
#include "muffmode/mm_vote.h"

void SP_misc_teleporter_dest(gentity_t *ent);

static bool ShouldShowRampageMessages();

static THINK(info_player_start_drop) (gentity_t *self) -> void {
	// allow them to drop
	self->solid = SOLID_TRIGGER;
	self->movetype = MOVETYPE_TOSS;
	self->mins = PLAYER_MINS;
	self->maxs = PLAYER_MAXS;
	gi.linkentity(self);
}

static inline void deathmatch_spawn_flags(gentity_t *self) {
	if (st.nobots)
		self->flags = FL_NO_BOTS;
	if (st.nohumans)
		self->flags = FL_NO_HUMANS;
}

/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
The normal starting point for a level.

"nobots" will prevent bots from using this spot.
"nohumans" will prevent humans from using this spot.
*/
void SP_info_player_start(gentity_t *self) {
	// fix stuck spawn points
	if (gi.trace(self->s.origin, PLAYER_MINS, PLAYER_MAXS, self->s.origin, self, MASK_SOLID).startsolid)
		G_FixStuckObject(self, self->s.origin);

	// [Paril-KEX] on n64, since these can spawn riding elevators,
	// allow them to "ride" the elevators so respawning works
	if (level.is_n64) {
		self->think = info_player_start_drop;
		self->nextthink = level.time + FRAME_TIME_S;
	}

	deathmatch_spawn_flags(self);
}

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32) INITIAL x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
A potential spawning position for deathmatch games.

The first time a player enters the game, they will be at an 'INITIAL' spot.
Targets will be fired when someone spawns in on them.
"nobots" will prevent bots from using this spot.
"nohumans" will prevent humans from using this spot.
*/
void SP_info_player_deathmatch(gentity_t *self) {
	if (!deathmatch->integer) {
		G_FreeEntity(self);
		return;
	}
	SP_misc_teleporter_dest(self);

	deathmatch_spawn_flags(self);
}

/*QUAKED info_player_team_red (1 0 0) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
A potential Red Team spawning position for CTF games.
*/
void SP_info_player_team_red(gentity_t *self) {}

/*QUAKED info_player_team_blue (0 0 1) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
A potential Blue Team spawning position for CTF games.
*/
void SP_info_player_team_blue(gentity_t *self) {}

/*QUAKED info_player_coop (1 0 1) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
A potential spawning position for coop games.
*/
void SP_info_player_coop(gentity_t *self) {
	if (!coop->integer) {
		G_FreeEntity(self);
		return;
	}

	SP_info_player_start(self);
}

/*QUAKED info_player_coop_lava (1 0 1) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
A potential spawning position for coop games on rmine2 where lava level
needs to be checked.
*/
void SP_info_player_coop_lava(gentity_t *self) {
	if (!coop->integer) {
		G_FreeEntity(self);
		return;
	}

	// fix stuck spawn points
	if (gi.trace(self->s.origin, PLAYER_MINS, PLAYER_MAXS, self->s.origin, self, MASK_SOLID).startsolid)
		G_FixStuckObject(self, self->s.origin);
}

/*QUAKED info_player_intermission (1 0 1) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
The deathmatch intermission point will be at one of these
Use 'angles' instead of 'angle', so you can set pitch or roll as well as yaw.  'pitch yaw roll'
*/
void SP_info_player_intermission(gentity_t *ent) {}

/*QUAKED info_ctf_teleport_destination (0.5 0.5 0.5) (-16 -16 -24) (16 16 32) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Point trigger_teleports at these.
*/
void SP_info_ctf_teleport_destination(gentity_t *ent) {
	ent->s.origin[2] += 16;
}

// [Paril-KEX] whether instanced items should be used or not
bool P_UseCoopInstancedItems() {
	// squad respawn forces instanced items on, since we don't
	// want players to need to backtrack just to get their stuff.
	return g_coop_instanced_items->integer || g_coop_squad_respawn->integer;
}

//=======================================================================

constexpr int8_t MAX_PLAYER_STOCK_MODELS = 3;
constexpr int8_t MAX_PLAYER_STOCK_SKINS = 24;

struct p_mods_skins_t {
	const char *mname;		// first model will be default model
	const char *sname[MAX_PLAYER_STOCK_SKINS];	//index 0 will be default skin
};

p_mods_skins_t original_models[MAX_PLAYER_STOCK_MODELS] = {
	{
		"male",
		{
			"grunt",

			"cipher",	"claymore",	"ctf_b",
			"ctf_r",	"deaddude",	"disguise",
			"flak",		"howitzer",	"insane1",
			"insane2",	"insane3",	"major",
			"nightops",	"pointman",	"psycho",
			"rampage",	"razor",	"recon",
			"rogue_b",	"rogue_r",	"scout",
			"sniper",	"viper"
		}
	},
	{
		"female",
		{
			"athena",

			"brianna",	"cobalt",	"ctf_b",
			"ctf_r",	"disguise",	"ensign",
			"jezebel",	"jungle",	"lotus",
			"rogue_b",	"rogue_r",	"stiletto",
			"venus",	"voodoo"
		}
	},
	{
		"cyborg",
		{
			"oni911",

			"ctf_b",	"ctf_r",	"disguise",
			"ps9000",	"tyr574"
		}
	}
};

/*
=============
ClientSkinOverride

Return an allowed skin path, falling back to stock defaults when needed.
=============
*/
static const char *ClientSkinOverride(const char *s) {
	if (g_allow_custom_skins->integer) {
		return s;
	}

	static char skin_buffer[MAX_QPATH];
	size_t i;
	std::string pm(s);
	std::string ps(s);
	std::string_view t(s);
	if (i = t.find_first_of('/'); i != std::string_view::npos) {
		pm = t.substr(0, i);
		ps = t.substr(i + 1, strlen(s) - i - 1);
	}

	if (!pm.length()) {
		pm = "male";
		ps = "grunt";
	}

	for (i = 0; i < MAX_PLAYER_STOCK_MODELS; i++) {
		if (pm == original_models[i].mname) {
			for (size_t j = 0; j < MAX_PLAYER_STOCK_SKINS; j++) {
				if (ps == original_models[i].sname[j]) {
					return s;
				}
			}

			gi.Com_PrintFmt("{}: reverting to default skin for model: {} -> {}\n", __FUNCTION__, s, original_models[i].mname, original_models[i].sname[0]);
			G_FmtTo(skin_buffer, "{}/{}", original_models[i].mname, original_models[i].sname[0]);
			return skin_buffer;
		}
	}

	gi.Com_PrintFmt("{}: reverting to default model: {} -> male/grunt\n", __FUNCTION__, s);
	Q_strlcpy(skin_buffer, "male/grunt", sizeof(skin_buffer));
	return skin_buffer;
}

//=======================================================================
// PLAYER CONFIGS
//=======================================================================
/*
static void PCfg_WriteConfig(gentity_t *ent) {
	if (!std::strcmp(ent->client->pers.social_id, "me_a_bot"))
		return;

	const char *name = G_Fmt("baseq2/pcfg/wtest/{}.cfg", ent->client->pers.social_id).data();
	char *buffer = nullptr;
	std::string string;
	string.clear();

	FILE *f = std::fopen(name, "wb");
	if (!f)
		return;

	string = G_Fmt("// {}'s Player Config\n// Generated by Muff Mode\n", ent->client->resp.netname).data();
	string += G_Fmt("name {}\n", ent->client->resp.netname).data();
	string += G_Fmt("show_id {}\n", ent->client->sess.pc.show_id ? 1 : 0).data();
	string += G_Fmt("show_fragmessages {}\n", ent->client->sess.pc.show_fragmessages ? 1 : 0).data();
	string += G_Fmt("show_timer {}\n", ent->client->sess.pc.show_timer ? 1 : 0).data();
	string += G_Fmt("killbeep_num {}\n", (int)ent->client->sess.pc.killbeep_num).data();

	std::fwrite(buffer, 1, strlen(buffer), f);
	std::fclose(f);
	gi.Com_PrintFmt("Player config written to: \"{}\"\n", name);
}
*/
/*
=============
PCfg_ClientInitPConfig

Load or create the player's configuration file on connect.
=============
*/
static void PCfg_ClientInitPConfig(gentity_t *ent) {
	bool file_exists = false;
	bool cfg_valid = true;
	
	if (!ent->client) return;
	if (ent->svflags & SVF_BOT) return;

	// Validate and sanitize social_id for filesystem use
	// This prevents crashes from empty or malicious social_id values
	char safe_social_id[MAX_INFO_VALUE] = {0};
	if (!ent->client->pers.social_id[0]) {
		// Empty social_id - use a fallback based on client number
		Q_strlcpy(safe_social_id, G_Fmt("unknown_{}", ent - g_entities - 1).data(), sizeof(safe_social_id));
	} else {
		// Sanitize: remove path separators and other dangerous characters
		const char *src = ent->client->pers.social_id;
		char *dst = safe_social_id;
		size_t remaining = sizeof(safe_social_id) - 1;
		
		while (*src && remaining > 0) {
			char c = *src++;
			// Allow alphanumeric, dash, underscore, and common ID characters
			// Block path separators, null bytes, and other filesystem-unsafe chars
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || 
			    (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
				*dst++ = c;
				remaining--;
			}
		}
		*dst = '\0';
		
		// If sanitization removed everything, use fallback
		if (!safe_social_id[0]) {
			Q_strlcpy(safe_social_id, G_Fmt("invalid_{}", ent - g_entities - 1).data(), sizeof(safe_social_id));
		}
	}

	const std::string path = std::string(G_Fmt("baseq2/pcfg/{}.cfg", safe_social_id));
	const char *name = path.c_str();

	FILE *f = fopen(name, "rb");
	char *buffer = nullptr;
	if (f != NULL) {
		size_t length;
		size_t read_length;

		fseek(f, 0, SEEK_END);
		length = ftell(f);
		fseek(f, 0, SEEK_SET);

		if (length > 0x40000) {
			cfg_valid = false;
		}
		if (cfg_valid) {
			buffer = (char *)gi.TagMalloc(length + 1, '\0');
			if (length) {
				read_length = fread(buffer, 1, length, f);

				if (length != read_length) {
					cfg_valid = false;
				}
			}
		}
		file_exists = true;
		fclose(f);

		if (!cfg_valid) {
			if (buffer) {
				gi.TagFree(buffer);
			}
			gi.Com_PrintFmt("{}: Player config load error for \"{}\", discarding.\n", __FUNCTION__, name);
			return;
		}

		if (buffer) {
			gi.TagFree(buffer);
			buffer = nullptr;
		}
	}

	if (!file_exists) {
		f = fopen(name, "w");
		if (f) {
			const std::string header = std::string(G_Fmt("// {}'s Player Config\n// Generated by Muff Mode\n", ent->client->resp.netname));

			fwrite(header.c_str(), 1, header.length(), f);
			gi.Com_PrintFmt("{}: Player config written to: \"{}\"\n", __FUNCTION__, name);
			fclose(f);
		} else {
			gi.Com_PrintFmt("{}: Cannot save player config: {}\n", __FUNCTION__, name);
		}
	} else {
		//gi.Com_PrintFmt("{}: Player config not saved as file already exists: \"{}\"\n", __FUNCTION__, name);
	}
}

//=======================================================================
struct mon_name_t {
	const char *classname;
	const char *longname;
};
mon_name_t monname[] = {
	{ "monster_arachnid", "Arachnid" },
	{ "monster_berserk", "Berserker" },
	{ "monster_boss2", "Hornet" },
	{ "monster_boss5", "Super Tank" },
	{ "monster_brain", "Brains" },
	{ "monster_carrier", "Carrier" },
	{ "monster_chick", "Iron Maiden" },
	{ "monster_chick_heat", "Iron Maiden" },
	{ "monster_daedalus", "Daedalus" },
	{ "monster_fixbot", "Fixbot" },
	{ "monster_flipper", "Barracuda Shark" },
	{ "monster_floater", "Technician" },
	{ "monster_flyer", "Flyer" },
	{ "monster_gekk", "Gekk" },
	{ "monster_gladb", "Gladiator" },
	{ "monster_gladiator", "Gladiator" },
	{ "monster_guardian", "Guardian" },
	{ "monster_guncmdr", "Gunner Commander" },
	{ "monster_gunner", "Gunner" },
	{ "monster_hover", "Icarus" },
	{ "monster_infantry", "Infantry" },
	{ "monster_jorg", "Jorg" },
	{ "monster_kamikaze", "Kamikaze" },
	{ "monster_makron", "Makron" },
	{ "monster_medic", "Medic" },
	{ "monster_medic_commander", "Medic Commander" },
	{ "monster_mutant", "Mutant" },
	{ "monster_parasite", "Parasite" },
	{ "monster_shambler", "Shambler" },
	{ "monster_soldier", "Machinegun Guard" },
	{ "monster_soldier_hypergun", "Hypergun Guard" },
	{ "monster_soldier_lasergun", "Laser Guard" },
	{ "monster_soldier_light", "Light Guard" },
	{ "monster_soldier_ripper", "Ripper Guard" },
	{ "monster_soldier_ss", "Shotgun Guard" },
	{ "monster_stalker", "Stalker" },
	{ "monster_supertank", "Super Tank" },
	{ "monster_tank", "Tank" },
	{ "monster_tank_commander", "Tank Commander" },
	{ "monster_turret", "Turret" },
	{ "monster_widow", "Black Widow" },
	{ "monster_widow2", "Black Widow" },
};

/*
=============
MonsterName

Look up a friendly monster name, falling back to the classname when not found.
=============
*/
static const char *MonsterName(const char *classname) {
	if (!classname)
		return nullptr;
	for (size_t i = 0; i < ARRAY_LEN(monname); i++) {
		if (!Q_strncasecmp(classname, monname[i].classname, strlen(classname)))
			return monname[i].longname;
	}
	return classname;
}

static bool IsVowel(const char c) {
	if (c == 'A' || c == 'a' ||
		c == 'E' || c == 'e' ||
		c == 'I' || c == 'i' ||
		c == 'O' || c == 'o' ||
		c == 'U' || c == 'u')
		return true;
	return false;
}

static void ClientObituary(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, mod_t mod) {
	const char *base = nullptr;

	if (InCoopStyle() && attacker->client)
		mod.friendly_fire = true;

	if (mod.id == MOD_CHANGE_TEAM)
		return;

	int kill_count = self->client->resp.kill_count;
	self->client->resp.kill_count = 0;

	switch (mod.id) {
	case MOD_SUICIDE:
		base = "$g_mod_generic_suicide";
		break;
	case MOD_EXPIRE:
		base = "{0} ran out of blood.\n";
		break;
	case MOD_FALLING:
		base = "$g_mod_generic_falling";
		break;
	case MOD_CRUSH:
		base = "$g_mod_generic_crush";
		break;
	case MOD_WATER:
		base = "$g_mod_generic_water";
		break;
	case MOD_SLIME:
		base = "$g_mod_generic_slime";
		break;
	case MOD_LAVA:
		base = "$g_mod_generic_lava";
		break;
	case MOD_EXPLOSIVE:
	case MOD_BARREL:
		base = "$g_mod_generic_explosive";
		break;
	case MOD_EXIT:
		base = "$g_mod_generic_exit";
		break;
	case MOD_TARGET_LASER:
		base = "$g_mod_generic_laser";
		break;
	case MOD_TARGET_BLASTER:
		base = "$g_mod_generic_blaster";
		break;
	case MOD_BOMB:
	case MOD_SPLASH:
	case MOD_TRIGGER_HURT:
		base = "$g_mod_generic_hurt";
		break;
		/*
	case MOD_GEKK:
	case MOD_BRAINTENTACLE:
		base = "$g_mod_generic_gekk";
		break;
		*/
	default:
		base = nullptr;
		break;
	}

	if (attacker == self) {
		switch (mod.id) {
		case MOD_HELD_GRENADE:
			base = "$g_mod_self_held_grenade";
			break;
		case MOD_HG_SPLASH:
		case MOD_G_SPLASH:
			base = "$g_mod_self_grenade_splash";
			break;
		case MOD_R_SPLASH:
			base = "$g_mod_self_rocket_splash";
			break;
		case MOD_BFG_BLAST:
			base = "$g_mod_self_bfg_blast";
			break;
		case MOD_TRAP:
			base = "$g_mod_self_trap";
			break;
		case MOD_DOPPEL_EXPLODE:
			base = "$g_mod_self_dopple_explode";
			break;
		case MOD_EXPIRE:
			base = "{0} ran out of blood.\n";
			break;
		default:
			base = "$g_mod_self_default";
			break;
		}
	}

	// send generic/self
	if (base) {
		gi.LocBroadcast_Print(PRINT_MEDIUM, base, self->client->resp.netname);
		self->enemy = nullptr;
		return;
	}

	// has a killer
	self->enemy = attacker;

	if (!attacker)
		return;

	if (attacker->svflags & SVF_MONSTER) {
		const char *monname = MonsterName(attacker->classname);

		if (monname)
			gi.LocBroadcast_Print(PRINT_MEDIUM, "{} was killed by a{} {}\n", self->client->resp.netname, IsVowel(monname[0]) ? "n" : "", monname);
		return;
	}

	if (!attacker->client)
		return;
	
	switch (mod.id) {
	case MOD_BLASTER:
		base = "$g_mod_kill_blaster";
		break;
	case MOD_SHOTGUN:
		base = "$g_mod_kill_shotgun";
		break;
	case MOD_SSHOTGUN:
		base = "$g_mod_kill_sshotgun";
		break;
	case MOD_MACHINEGUN:
		base = "$g_mod_kill_machinegun";
		break;
	case MOD_CHAINGUN:
		base = "$g_mod_kill_chaingun";
		break;
	case MOD_GRENADE:
		base = "$g_mod_kill_grenade";
		break;
	case MOD_G_SPLASH:
		base = "$g_mod_kill_grenade_splash";
		break;
	case MOD_ROCKET:
		base = "$g_mod_kill_rocket";
		break;
	case MOD_R_SPLASH:
		base = "$g_mod_kill_rocket_splash";
		break;
	case MOD_HYPERBLASTER:
		base = "$g_mod_kill_hyperblaster";
		break;
	case MOD_RAILGUN:
		base = "$g_mod_kill_railgun";
		break;
	case MOD_BFG_LASER:
		base = "$g_mod_kill_bfg_laser";
		break;
	case MOD_BFG_BLAST:
		base = "$g_mod_kill_bfg_blast";
		break;
	case MOD_BFG_EFFECT:
		base = "$g_mod_kill_bfg_effect";
		break;
	case MOD_HANDGRENADE:
		base = "$g_mod_kill_handgrenade";
		break;
	case MOD_HG_SPLASH:
		base = "$g_mod_kill_handgrenade_splash";
		break;
	case MOD_HELD_GRENADE:
		base = "$g_mod_kill_held_grenade";
		break;
	case MOD_TELEFRAG:
	case MOD_TELEFRAG_SPAWN:
		base = "$g_mod_kill_telefrag";
		break;
	case MOD_RIPPER:
		base = "$g_mod_kill_ripper";
		break;
	case MOD_PHALANX:
		base = "$g_mod_kill_phalanx";
		break;
	case MOD_TRAP:
		base = "$g_mod_kill_trap";
		break;
	case MOD_CHAINFIST:
		base = "$g_mod_kill_chainfist";
		break;
	case MOD_DISINTEGRATOR:
		base = "$g_mod_kill_disintegrator";
		break;
	case MOD_ETF_RIFLE:
		base = "$g_mod_kill_etf_rifle";
		break;
	case MOD_PLASMABEAM:
		base = "$g_mod_kill_heatbeam";
		break;
	case MOD_TESLA:
		base = "$g_mod_kill_tesla";
		break;
	case MOD_PROX:
		base = "$g_mod_kill_prox";
		break;
	case MOD_NUKE:
		base = "$g_mod_kill_nuke";
		break;
	case MOD_VENGEANCE_SPHERE:
		base = "$g_mod_kill_vengeance_sphere";
		break;
	case MOD_DEFENDER_SPHERE:
		base = "$g_mod_kill_defender_sphere";
		break;
	case MOD_HUNTER_SPHERE:
		base = "$g_mod_kill_hunter_sphere";
		break;
	case MOD_TRACKER:
		base = "$g_mod_kill_tracker";
		break;
	case MOD_DOPPEL_EXPLODE:
		base = "$g_mod_kill_dopple_explode";
		break;
	case MOD_DOPPEL_VENGEANCE:
		base = "$g_mod_kill_dopple_vengeance";
		break;
	case MOD_DOPPEL_HUNTER:
		base = "$g_mod_kill_dopple_hunter";
		break;
	case MOD_GRAPPLE:
		base = "$g_mod_kill_grapple";
		break;
	default:
		base = "$g_mod_kill_generic";
		break;
	}

	gi.LocBroadcast_Print(PRINT_MEDIUM, base, self->client->resp.netname, attacker->client->resp.netname);

	if (Teams()) {
		// if at start and same team, clear.
		// [Paril-KEX] moved here so it's not an outlier in player_die.
		if (mod.id == MOD_TELEFRAG_SPAWN &&
				self->client->resp.ctf_state < 2 &&
				self->client->sess.team == attacker->client->sess.team) {
			self->client->resp.ctf_state = 0;
			return;
		}
	}

	// frag messages
	if (deathmatch->integer && self != attacker && self->client && attacker->client) {
		if (!(self->svflags & SVF_BOT)) {
			if (level.match_state == matchst_t::MATCH_WARMUP_READYUP) {
				BroadcastReadyReminderMessage();
			} else {
				if (GTF(GTF_ROUNDS) && GTF(GTF_ELIMINATION) && level.round_state == roundst_t::ROUND_IN_PROGRESS) {
					gi.LocClient_Print(self, PRINT_CENTER, "You were fragged by {}\nYou will respawn next round.", attacker->client->resp.netname);
				} else if (GT(GT_FREEZE) && level.round_state == roundst_t::ROUND_IN_PROGRESS) {
					bool last_standing = true;
					if (self->client->sess.team == TEAM_RED && level.num_living_red > 1 ||
						self->client->sess.team == TEAM_BLUE && level.num_living_blue > 1)
						last_standing = false;
					gi.LocClient_Print(self, PRINT_CENTER, "You were frozen by {}{}",
						attacker->client->resp.netname,
						last_standing ? "" : "\nYou will respawn once thawed.");
				} else {
					gi.LocClient_Print(self, PRINT_CENTER, "You were {} by {}", GT(GT_FREEZE) ? "frozen" : "fragged", attacker->client->resp.netname);
				}
			}
		}
		if (!(attacker->svflags & SVF_BOT)) {
			if (Teams() && OnSameTeam(self, attacker)) {
				gi.LocClient_Print(attacker, PRINT_CENTER, "You fragged {}, your team mate :(", self->client->resp.netname);
			} else {
				if (level.match_state == matchst_t::MATCH_WARMUP_READYUP) {
					BroadcastReadyReminderMessage();
				} else if (attacker->client->resp.kill_count && !(attacker->client->resp.kill_count % 10)) {
					if (ShouldShowRampageMessages()) {
						gi.LocBroadcast_Print(PRINT_CENTER, "{} is on a rampage\nwith {} frags!", attacker->client->resp.netname, attacker->client->resp.kill_count);
						AnnouncerSound(attacker, "rampage1", nullptr, false);
						attacker->client->pers.medal_time = level.time;
						attacker->client->pers.medal_type = MEDAL_RAMPAGE;
						attacker->client->pers.medal_count[MEDAL_RAMPAGE]++;
					}
				} else if (kill_count >= 10) {
					if (ShouldShowRampageMessages()) {
						gi.LocBroadcast_Print(PRINT_CENTER, "{} put an end to {}'s\nrampage!", attacker->client->resp.netname, self->client->resp.netname);
					}
				} else if (Teams() || level.match_state != matchst_t::MATCH_IN_PROGRESS) {
					if (attacker->client->sess.pc.show_fragmessages)
						gi.LocClient_Print(attacker, PRINT_CENTER, "You {} {}", GT(GT_FREEZE) ? "froze" : "fragged", self->client->resp.netname);
				} else {
					if (attacker->client->sess.pc.show_fragmessages)
						gi.LocClient_Print(attacker, PRINT_CENTER, "You {} {}\n{} place with {}", GT(GT_FREEZE) ? "froze" : "fragged",
							self->client->resp.netname, G_PlaceString(attacker->client->resp.rank + 1), attacker->client->resp.score);
				}
			}
			if (attacker->client->sess.pc.killbeep_num > 0 && attacker->client->sess.pc.killbeep_num < 5) {
				const char *sb[5] = { "", "nav_editor/select_node.wav", "misc/comp_up.wav", "insane/insane7.wav", "nav_editor/finish_node_move.wav" };
				gi.local_sound(attacker, CHAN_AUTO, gi.soundindex(sb[attacker->client->sess.pc.killbeep_num]), 1, ATTN_NONE, 0);
			}
		}
	}

	if (base)
		return;

	gi.LocBroadcast_Print(PRINT_MEDIUM, "$g_mod_generic_died", self->client->resp.netname);
}

/*
=================
TossClientItems

Toss the weapon, tech, CTF flag and powerups for the killed player
=================
*/
static void TossClientItems(gentity_t *self) {
	if (!deathmatch->integer)
		return;

	if (GTF(GTF_ARENA))
		return;

	// don't drop anything when combat is disabled
	if (IsCombatDisabled())
		return;

	gitem_t *wp;
	gentity_t *drop;
	bool	quad, doubled, haste, protection, invis, regen;

	// drop weapon
	wp = self->client->pers.weapon;
	if (wp) {
		if (g_instagib->integer || GT(GT_INSTAGIB))
			wp = nullptr;
		else if (g_nadefest->integer || GT(GT_NADEFEST))
			wp = nullptr;
		else if (!self->client->pers.inventory[self->client->pers.weapon->ammo])
			wp = nullptr;
		else if (!wp->drop)
			wp = nullptr;
		else if (RS(RS_Q3A) && wp->id == IT_WEAPON_MACHINEGUN)
			wp = nullptr;
		else if (RS(RS_Q1) && wp->id == IT_WEAPON_SHOTGUN)
			wp = nullptr;

		if (wp) {
			self->client->v_angle[YAW] = 0.0;
			drop = Drop_Item(self, wp);
			drop->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
			drop->spawnflags &= ~SPAWNFLAG_ITEM_DROPPED;
			drop->svflags &= ~SVF_INSTANCED;
		}
	}

	//drop tech
	Tech_DeadDrop(self);

	// drop CTF flags
	CTF_DeadDropFlag(self);

	// drop powerup
	quad = g_dm_no_quad_drop->integer ? false : (self->client->pu_time_quad > (level.time + 1_sec));
	haste = (self->client->pu_time_haste > (level.time + 1_sec));
	doubled = (self->client->pu_time_double > (level.time + 1_sec));
	protection = (self->client->pu_time_protection > (level.time + 1_sec));
	invis = (self->client->pu_time_invisibility > (level.time + 1_sec));
	regen = (self->client->pu_time_regeneration > (level.time + 1_sec));

	if (!g_dm_powerup_drop->integer) {
		quad = doubled = haste = protection = invis = regen = false;
	}

	if (quad) {
		self->client->v_angle[YAW] += 45.0;
		drop = Drop_Item(self, GetItemByIndex(IT_POWERUP_QUAD));
		drop->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
		drop->spawnflags &= ~SPAWNFLAG_ITEM_DROPPED;
		drop->svflags &= ~SVF_INSTANCED;

		drop->touch = Touch_Item;
		drop->nextthink = self->client->pu_time_quad;
		drop->think = g_quadhog->integer ? QuadHog_DoReset : G_FreeEntity;

		if (g_quadhog->integer) {
			drop->s.renderfx |= RF_SHELL_BLUE;
			drop->s.effects |= EF_COLOR_SHELL;
		}

		// decide how many seconds it has left
		drop->count = self->client->pu_time_quad.seconds<int>() - level.time.seconds<int>();
		if (drop->count < 1) {
			drop->count = 1;
		}
	}

	if (haste) {
		self->client->v_angle[YAW] += 45;
		drop = Drop_Item(self, GetItemByIndex(IT_POWERUP_HASTE));
		drop->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
		drop->spawnflags &= ~SPAWNFLAG_ITEM_DROPPED;
		drop->svflags &= ~SVF_INSTANCED;

		drop->touch = Touch_Item;
		drop->nextthink = self->client->pu_time_haste;
		drop->think = G_FreeEntity;

		// decide how many seconds it has left
		drop->count = self->client->pu_time_haste.seconds<int>() - level.time.seconds<int>();
		if (drop->count < 1) {
			drop->count = 1;
		}
	}

	if (protection) {
		self->client->v_angle[YAW] += 45;
		drop = Drop_Item(self, GetItemByIndex(IT_POWERUP_PROTECTION));
		drop->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
		drop->spawnflags &= ~SPAWNFLAG_ITEM_DROPPED;
		drop->svflags &= ~SVF_INSTANCED;

		drop->touch = Touch_Item;
		drop->nextthink = self->client->pu_time_protection;
		drop->think = G_FreeEntity;

		// decide how many seconds it has left
		drop->count = self->client->pu_time_protection.seconds<int>() - level.time.seconds<int>();
		if (drop->count < 1) {
			drop->count = 1;
		}
	}

	if (regen) {
		self->client->v_angle[YAW] += 45;
		drop = Drop_Item(self, GetItemByIndex(IT_POWERUP_REGEN));
		drop->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
		drop->spawnflags &= ~SPAWNFLAG_ITEM_DROPPED;
		drop->svflags &= ~SVF_INSTANCED;

		drop->touch = Touch_Item;
		drop->nextthink = self->client->pu_time_regeneration;
		drop->think = G_FreeEntity;

		// decide how many seconds it has left
		drop->count = self->client->pu_time_regeneration.seconds<int>() - level.time.seconds<int>();
		if (drop->count < 1) {
			drop->count = 1;
		}
	}

	if (invis) {
		self->client->v_angle[YAW] += 45;
		drop = Drop_Item(self, GetItemByIndex(IT_POWERUP_INVISIBILITY));
		drop->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
		drop->spawnflags &= ~SPAWNFLAG_ITEM_DROPPED;
		drop->svflags &= ~SVF_INSTANCED;

		drop->touch = Touch_Item;
		drop->nextthink = self->client->pu_time_invisibility;
		drop->think = G_FreeEntity;

		// decide how many seconds it has left
		drop->count = self->client->pu_time_invisibility.seconds<int>() - level.time.seconds<int>();
		if (drop->count < 1) {
			drop->count = 1;
		}
	}

	if (doubled) {
		self->client->v_angle[YAW] += 45;
		drop = Drop_Item(self, GetItemByIndex(IT_POWERUP_DOUBLE));
		drop->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
		drop->spawnflags &= ~SPAWNFLAG_ITEM_DROPPED;
		drop->svflags &= ~SVF_INSTANCED;

		drop->touch = Touch_Item;
		drop->nextthink = self->client->pu_time_double;
		drop->think = G_FreeEntity;

		// decide how many seconds it has left
		drop->count = self->client->pu_time_double.seconds<int>() - level.time.seconds<int>();
		if (drop->count < 1) {
			drop->count = 1;
		}
	}

	self->client->v_angle[YAW] = 0.0;
}

/*
==================
LookAtKiller
==================
*/
void LookAtKiller(gentity_t *self, gentity_t *inflictor, gentity_t *attacker) {
	vec3_t dir;

	if (attacker && attacker != world && attacker != self) {
		dir = attacker->s.origin - self->s.origin;
	} else if (inflictor && inflictor != world && inflictor != self) {
		dir = inflictor->s.origin - self->s.origin;
	} else {
		self->client->killer_yaw = self->s.angles[YAW];
		return;
	}

	// PMM - fixed to correct for pitch of 0
	if (dir[0])
		self->client->killer_yaw = 180 / PIf * atan2f(dir[1], dir[0]);
	else if (dir[1] > 0)
		self->client->killer_yaw = 90;
	else if (dir[1] < 0)
		self->client->killer_yaw = 270;
	else
		self->client->killer_yaw = 0;
}

/*
================
Match_CanScore
================
*/
static bool Match_CanScore() {
	if (level.intermission_queued)
		return false;

	switch (level.match_state) {
	case matchst_t::MATCH_WARMUP_DELAYED:
	case matchst_t::MATCH_WARMUP_DEFAULT:
	case matchst_t::MATCH_WARMUP_READYUP:
	case matchst_t::MATCH_COUNTDOWN:
	case matchst_t::MATCH_ENDED:
		return false;
	}

	return true;
}

/*
==================
ShouldShowRampageMessages
==================
Helper function to check if rampage messages should be shown.
0 = disabled for all gametypes
1 = defaults (enabled for FFA, disabled for TDM/CTF)
2 = enabled for all gametypes
==================
*/
static bool ShouldShowRampageMessages() {
	int value = g_frag_messages->integer;
	
	if (value == 0)
		return false;  // Disabled for all gametypes
	if (value == 2)
		return true;   // Enabled for all gametypes
	
	// value == 1: Use gametype-specific defaults
	// Enabled for FFA, disabled for TDM/CTF
	if (GT(GT_FFA))
		return true;
	if (GT(GT_TDM) || GT(GT_CTF))
		return false;
	
	// For other gametypes, default to enabled
	return true;
}

/*
==================
player_die
==================
*/
DIE(player_die) (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, const vec3_t &point, const mod_t &mod) -> void {
	if (self->client->ps.pmove.pm_type == PM_DEAD)
		return;

	PlayerTrail_Destroy(self);

	self->avelocity = {};

	self->takedamage = true;
	self->movetype = MOVETYPE_TOSS;

	self->s.modelindex2 = 0; // remove linked weapon model
	self->s.modelindex3 = 0; // remove linked ctf flag

	self->s.angles[PITCH] = 0;
	self->s.angles[ROLL] = 0;

	self->s.sound = 0;
	self->client->weapon_sound = 0;

	self->maxs[2] = -8;

	if (attacker && attacker->client && level.match_state == matchst_t::MATCH_IN_PROGRESS) {
		if (attacker == self || mod.friendly_fire) {
			if (!mod.no_point_loss)
				G_AdjustPlayerScore(attacker->client, -1, GT(GT_TDM), -1);
			attacker->client->resp.kill_count = 0;
		} else {
			G_AdjustPlayerScore(attacker->client, 1, GT(GT_TDM), 1);
			if (attacker->health > 0)
				attacker->client->resp.kill_count++;

			MS_Adjust(attacker->client, MSTAT_KILLS_TOTAL, 1);
			if (1_sec > (level.time - self->client->respawn_time))
				MS_Adjust(attacker->client, MSTAT_KILLS_SPAWN, 1);

			if (attacker->client->pers.kill_time && (attacker->client->pers.kill_time + 2_sec > level.time)) {
				attacker->client->pers.medal_time = level.time;
				attacker->client->pers.medal_type = MEDAL_EXCELLENT;
				attacker->client->pers.medal_count[MEDAL_EXCELLENT]++;

				if (attacker->client->pers.medal_count[MEDAL_EXCELLENT] == 1)
					AnnouncerSound(attacker, "first_excellent", nullptr, false);
				else
					AnnouncerSound(attacker, "excellent1", nullptr, false);
			}
			attacker->client->pers.kill_time = level.time;

			if (mod.id == MOD_BLASTER || mod.id == MOD_CHAINFIST) {
				attacker->client->pers.medal_time = level.time;
				attacker->client->pers.medal_type = MEDAL_HUMILIATION;
				attacker->client->pers.medal_count[MEDAL_HUMILIATION]++;

				AnnouncerSound(attacker, "humiliation1", nullptr, false);
			}

			for (auto ec : active_clients()) {
				if ((!ClientIsPlaying(ec->client) || ec->client->eliminated) && ec->client->sess.pc.follow_killer) {
					ec->client->follow_queued_target = attacker;
					ec->client->follow_queued_time = level.time;
				}
			}
		}
	} else {
		if (!mod.no_point_loss)
			G_AdjustPlayerScore(self->client, -1, GT(GT_TDM), -1);
	}
	MS_Adjust(self->client, MSTAT_DEATHS_TOTAL, 1);

	if (self == attacker)
		MS_Adjust(self->client, MSTAT_DEATHS_SUICIDES, 1);
	else if (!attacker)
		MS_Adjust(self->client, MSTAT_DEATHS_ENVIRO, 1);
	else if (1_sec > (level.time - self->client->respawn_time))
		MS_Adjust(self->client, MSTAT_DEATHS_SPAWN, 1);

	self->svflags |= SVF_DEADMONSTER;

	if (GTF(GTF_ROUNDS) && GTF(GTF_ELIMINATION) &&
			level.match_state == matchst_t::MATCH_IN_PROGRESS &&
			level.round_state == roundst_t::ROUND_IN_PROGRESS &&
			notGT(GT_HORDE) && ClientIsPlaying(self->client) &&
			!self->client->eliminated) {
		ClientSetEliminated(self);
		CalculateRanks();
	}

	if (!self->deadflag) {
		self->client->respawn_time = (level.time + 1_sec);

		if (false) { // Race mode removed
			self->client->respawn_min_time = self->client->respawn_time = level.time;
		} else {
			self->client->respawn_min_time = (level.time + gtime_t::from_sec(g_dm_respawn_delay_min->value));
			if (deathmatch->integer && g_dm_force_respawn_time->integer) {
				self->client->respawn_time = (level.time + gtime_t::from_sec(g_dm_force_respawn_time->value));
			}
		}

		LookAtKiller(self, inflictor, attacker);
		self->client->ps.pmove.pm_type = PM_DEAD;
		ClientObituary(self, inflictor, attacker, mod);

		CTF_ScoreBonuses(self, inflictor, attacker);
		TossClientItems(self);
		Weapon_Grapple_DoReset(self->client);

		if (deathmatch->integer && !self->client->showscores)
			Cmd_Help_f(self); // show scores

		if (coop->integer && !P_UseCoopInstancedItems()) {
			// clear inventory
			// this is kind of ugly, but it's how we want to handle keys in coop
			for (int n = 0; n < IT_TOTAL; n++) {
				if (itemlist[n].flags & IF_KEY)
					self->client->resp.coop_respawn.inventory[n] = self->client->pers.inventory[n];
				self->client->pers.inventory[n] = 0;
			}
		}
	}

	// remove powerups
	self->client->pu_time_quad = 0_ms;
	self->client->pu_time_haste = 0_ms;
	self->client->pu_time_double = 0_ms;
	self->client->pu_time_protection = 0_ms;
	self->client->pu_time_invisibility = 0_ms;
	self->client->pu_time_regeneration = 0_ms;
	self->client->pu_time_rebreather = 0_ms;
	self->client->pu_time_enviro = 0_ms;
	self->flags &= ~FL_POWER_ARMOR;

	// clear inventory
	if (Teams())
		self->client->pers.inventory.fill(0);

	// if there's a sphere around, let it know the player died.
	// vengeance and hunter will die if they're not attacking,
	// defender should always die
	if (self->client->owned_sphere) {
		gentity_t *sphere;

		sphere = self->client->owned_sphere;
		sphere->die(sphere, self, self, 0, vec3_origin, mod);
	}

	// if we've been killed by the tracker, GIB!
	if (mod.id == MOD_TRACKER) {
		self->health = -100;
		damage = 400;
	}

	if (GT(GT_FREEZE) && !level.intermission_time && self->client->eliminated && !self->client->resp.thawer) {
		self->s.effects |= EF_COLOR_SHELL;
		self->s.renderfx |= (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE);
	} else {
		self->s.effects = EF_NONE;
		self->s.renderfx = RF_NONE;
	}

	// make sure no trackers are still hurting us.
	if (self->client->tracker_pain_time) {
		RemoveAttackingPainDaemons(self);
	}

	// if we got obliterated by the nuke, don't gib
	if ((self->health < -80) && (mod.id == MOD_NUKE))
		self->flags |= FL_NOGIB;

	if (self->health < GIB_HEALTH) {
		// don't toss gibs if we got vaped by the nuke
		if (!(self->flags & FL_NOGIB)) {
			// gib
			gi.sound(self, CHAN_BODY, gi.soundindex("misc/udeath.wav"), 1, ATTN_NORM, 0);

			// more meaty gibs for your dollar!
			if (deathmatch->integer) {
				if (self->health < -160)
					ThrowGibs(self, damage, { { 16, "models/objects/gibs/sm_meat/tris.md2", GIB_METALLIC } });
				if (self->health < -120)
					ThrowGibs(self, damage, { { 12, "models/objects/gibs/sm_meat/tris.md2", GIB_METALLIC } });
				if (self->health < -80)
					ThrowGibs(self, damage, { { 10, "models/objects/gibs/sm_meat/tris.md2", GIB_METALLIC } });
			}
			ThrowGibs(self, damage, { { 8, "models/objects/gibs/sm_meat/tris.md2", GIB_METALLIC } });
		}
		self->flags &= ~FL_NOGIB;

		ThrowClientHead(self, damage);
		
		self->client->anim_priority = ANIM_DEATH;
		self->client->anim_end = 0;
		
		self->takedamage = false;
	} else { // normal death
		if (!self->deadflag) {
			if (GT(GT_FREEZE)) {
				self->s.frame = FRAME_crstnd01 - 1;
				self->client->anim_end = self->s.frame;
			} else {
				// start a death animation
				self->client->anim_priority = ANIM_DEATH;
				if (self->client->ps.pmove.pm_flags & PMF_DUCKED) {
					self->s.frame = FRAME_crdeath1 - 1;
					self->client->anim_end = FRAME_crdeath5;
				} else {
					switch (irandom(3)) {
					case 0:
						self->s.frame = FRAME_death101 - 1;
						self->client->anim_end = FRAME_death106;
						break;
					case 1:
						self->s.frame = FRAME_death201 - 1;
						self->client->anim_end = FRAME_death206;
						break;
					case 2:
						self->s.frame = FRAME_death301 - 1;
						self->client->anim_end = FRAME_death308;
						break;
					}
				}
			}
			static constexpr const char *death_sounds[] = {
				"*death1.wav",
				"*death2.wav",
				"*death3.wav",
				"*death4.wav"
			};
			gi.sound(self, CHAN_VOICE, gi.soundindex(random_element(death_sounds)), 1, ATTN_NORM, 0);
			self->client->anim_time = 0_ms;
		}
	}

	if (!self->deadflag) {
		if (InCoopStyle() && (g_coop_squad_respawn->integer || g_coop_enable_lives->integer)) {
			if (g_coop_enable_lives->integer && self->client->pers.lives) {
				self->client->pers.lives--;
				self->client->resp.coop_respawn.lives--;
			}

			bool allPlayersDead = true;

			for (auto player : active_clients())
				if (player->health > 0 || (!level.deadly_kill_box && g_coop_enable_lives->integer && player->client->pers.lives > 0)) {
					allPlayersDead = false;
					break;
				}

			if (allPlayersDead) // allow respawns for telefrags and weird shit
			{
				level.coop_level_restart_time = level.time + 5_sec;

				for (auto player : active_clients())
					gi.LocCenter_Print(player, "$g_coop_lose");
			}

			// in 3 seconds, attempt a respawn or put us into
			// spectator mode
			if (!level.coop_level_restart_time)
				self->client->respawn_time = level.time + 3_sec;
		}
	}

	level.total_player_deaths++;

	self->deadflag = true;

	gi.linkentity(self);
}

//=======================================================================

#include <string>
#include <sstream>

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

		if (*ptr_copy)
			count = strtoul(COM_Parse(&ptr_copy), nullptr, 10);

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
void InitClientPersistant(gentity_t *ent, gclient_t *client) {
	// backup & restore userinfo
	char userinfo[MAX_INFO_STRING];
	Q_strlcpy(userinfo, client->pers.userinfo, sizeof(userinfo));

	memset(&client->pers, 0, sizeof(client->pers));
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
		// in coop, if there's already a player in the game and we're new,
		// steal their loadout. this would fix a potential softlock where a new
		// player may not have weapons at all.
		bool taken_loadout = false;

		int health, armor;
		gitem_armor_t armor_type = jacketarmor_info;

		if (GTF(GTF_ARENA)) {
			health = clamp(g_arena_start_health->integer, 1, 9999);
			armor = clamp(g_arena_start_armor->integer, 0, 999);
		} else {
			health = clamp(g_starting_health->integer, 1, 9999);
			armor = clamp(g_starting_armor->integer, 0, 999);
		}

		if (armor > jacketarmor_info.max_count)
			if (armor > combatarmor_info.max_count)
				armor_type = bodyarmor_info;
			else armor_type = combatarmor_info;

		client->pers.health = client->pers.max_health = health;

		int bonus = RS(RS_Q3A) ? 25 : g_starting_health_bonus->integer;
		if (!(GTF(GTF_ARENA)) && bonus > 0) {
			client->pers.health += bonus;
			if (!(RS(RS_Q3A))) {
				client->pers.health_bonus = bonus;
				ent->client->pers.health_bonus_timer = level.time + 1_sec;
			}
			ent->client->time_residual = level.time;
		}

		if (armor_type.base_count == jacketarmor_info.base_count)
			client->pers.inventory[IT_ARMOR_JACKET] = armor;
		else if (armor_type.base_count == combatarmor_info.base_count)
			client->pers.inventory[IT_ARMOR_COMBAT] = armor;
		else if (armor_type.base_count == bodyarmor_info.base_count)
			client->pers.inventory[IT_ARMOR_BODY] = armor;

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

		if (GT(GT_BALL)) {
			client->pers.inventory[IT_WEAPON_CHAINFIST] = 1;
		} else if (!taken_loadout) {
			if (g_instagib->integer || GT(GT_INSTAGIB)) {
				client->pers.inventory[IT_WEAPON_RAILGUN] = 1;
				client->pers.inventory[IT_AMMO_SLUGS] = AMMO_INFINITE;
			} else if (g_nadefest->integer || GT(GT_NADEFEST)) {
				client->pers.inventory[IT_AMMO_GRENADES] = AMMO_INFINITE;
			} else if (GTF(GTF_ARENA)) {
				client->pers.max_ammo.fill(50);
				client->pers.max_ammo[AMMO_SHELLS] = 50;
				client->pers.max_ammo[AMMO_BULLETS] = 300;
				client->pers.max_ammo[AMMO_GRENADES] = 50;
				client->pers.max_ammo[AMMO_ROCKETS] = 50;
				client->pers.max_ammo[AMMO_CELLS] = 200;
				client->pers.max_ammo[AMMO_SLUGS] = 25;
				/*
				client->pers.max_ammo[AMMO_TRAP] = 5;
				client->pers.max_ammo[AMMO_FLECHETTES] = 200;
				client->pers.max_ammo[AMMO_DISRUPTOR] = 12;
				client->pers.max_ammo[AMMO_TESLA] = 5;
				*/
				client->pers.inventory[IT_AMMO_SHELLS] = 50;
				if (!(RS(RS_Q1))) {
					client->pers.inventory[IT_AMMO_BULLETS] = 200;
					client->pers.inventory[IT_AMMO_GRENADES] = 50;
				}
				client->pers.inventory[IT_AMMO_ROCKETS] = 50;
				client->pers.inventory[IT_AMMO_CELLS] = 200;
				if (!(RS(RS_Q1)))
					client->pers.inventory[IT_AMMO_SLUGS] = 50;

				client->pers.inventory[IT_WEAPON_BLASTER] = 1;
				client->pers.inventory[IT_WEAPON_SHOTGUN] = 1;
				if (!(RS(RS_Q3A)))
					client->pers.inventory[IT_WEAPON_SSHOTGUN] = 1;
				if (!(RS(RS_Q1))) {
					client->pers.inventory[IT_WEAPON_MACHINEGUN] = 1;
					client->pers.inventory[IT_WEAPON_CHAINGUN] = 1;
				}
				client->pers.inventory[IT_WEAPON_GLAUNCHER] = 1;
				client->pers.inventory[IT_WEAPON_RLAUNCHER] = 1;
				client->pers.inventory[IT_WEAPON_HYPERBLASTER] = 1;
				if (notGT(GT_CA))
					client->pers.inventory[IT_WEAPON_PLASMABEAM] = 1;
				if (!(RS(RS_Q1)))
					client->pers.inventory[IT_WEAPON_RAILGUN] = 1;
			} else {
				if (RS(RS_Q3A)) {
					client->pers.max_ammo.fill(200);
					client->pers.max_ammo[AMMO_BULLETS] = 200;
					client->pers.max_ammo[AMMO_SHELLS] = 200;
					client->pers.max_ammo[AMMO_CELLS] = 200;

					client->pers.max_ammo[AMMO_TRAP] = 200;
					client->pers.max_ammo[AMMO_FLECHETTES] = 200;
					client->pers.max_ammo[AMMO_DISRUPTOR] = 200;
					client->pers.max_ammo[AMMO_TESLA] = 200;

					client->pers.inventory[IT_WEAPON_CHAINFIST] = 1;
					client->pers.inventory[IT_WEAPON_MACHINEGUN] = 1;
					client->pers.inventory[IT_AMMO_BULLETS] = (GT(GT_TDM)) ? 50 : 100;
				} else if (RS(RS_Q1)) {
					client->pers.max_ammo.fill(200);
					client->pers.max_ammo[AMMO_BULLETS] = 200;
					client->pers.max_ammo[AMMO_SHELLS] = 200;
					client->pers.max_ammo[AMMO_CELLS] = 200;

					client->pers.max_ammo[AMMO_TRAP] = 200;
					client->pers.max_ammo[AMMO_FLECHETTES] = 200;
					client->pers.max_ammo[AMMO_DISRUPTOR] = 200;
					client->pers.max_ammo[AMMO_TESLA] = 200;

					client->pers.inventory[IT_WEAPON_CHAINFIST] = 1;
					client->pers.inventory[IT_WEAPON_SHOTGUN] = 1;
					client->pers.inventory[IT_AMMO_SHELLS] = 10;
				} else if (RS(RS_QC)) {
					// Quake Champions style: random starting weapon (Shotgun, Machinegun, or Hyperblaster)
					client->pers.max_ammo.fill(50);
					client->pers.max_ammo[AMMO_BULLETS] = 200;
					client->pers.max_ammo[AMMO_SHELLS] = 100;
					client->pers.max_ammo[AMMO_CELLS] = 200;

					client->pers.max_ammo[AMMO_TRAP] = 5;
					client->pers.max_ammo[AMMO_FLECHETTES] = 200;
					client->pers.max_ammo[AMMO_DISRUPTOR] = 12;
					client->pers.max_ammo[AMMO_TESLA] = 5;

					int weapon_choice = irandom(3); // 0 = Shotgun, 1 = Machinegun, 2 = Hyperblaster
					if (weapon_choice == 0) {
						client->pers.inventory[IT_WEAPON_SHOTGUN] = 1;
						client->pers.inventory[IT_AMMO_SHELLS] = 50;
					} else if (weapon_choice == 1) {
						client->pers.inventory[IT_WEAPON_MACHINEGUN] = 1;
						client->pers.inventory[IT_AMMO_BULLETS] = 200;
					} else {
						client->pers.inventory[IT_WEAPON_HYPERBLASTER] = 1;
						client->pers.inventory[IT_AMMO_CELLS] = 200;
					}
				} else {
					// fill with 50s, since it's our most common value
					client->pers.max_ammo.fill(50);
					client->pers.max_ammo[AMMO_BULLETS] = 200;
					client->pers.max_ammo[AMMO_SHELLS] = 100;
					client->pers.max_ammo[AMMO_CELLS] = 200;

					client->pers.max_ammo[AMMO_TRAP] = 5;
					client->pers.max_ammo[AMMO_FLECHETTES] = 200;
					client->pers.max_ammo[AMMO_DISRUPTOR] = 12;
					client->pers.max_ammo[AMMO_TESLA] = 5;

					client->pers.inventory[IT_WEAPON_BLASTER] = 1;
				}

				if (deathmatch->integer && game.ruleset != RS_QC) {
					if (level.match_state < matchst_t::MATCH_IN_PROGRESS) {
						for (size_t i = FIRST_WEAPON; i < LAST_WEAPON; i++) {
							if (!level.weapon_count[i - FIRST_WEAPON])
								continue;

							if (!itemlist[i].ammo)
								continue;

							client->pers.inventory[i] = 1;

							gitem_t *ammo = GetItemByIndex(itemlist[i].ammo);
							if (ammo)
								Add_Ammo(&g_entities[client - game.clients + 1], ammo, InfiniteAmmoOn(ammo) ? AMMO_INFINITE : ammo->quantity * 2);

							//gi.Com_PrintFmt("wp={} wc={} am={} q={}\n", i, level.weapon_count[i - FIRST_WEAPON], itemlist[i].ammo, InfiniteAmmoOn(ammo) ? AMMO_INFINITE : ammo->quantity * 2);
						}
					}
				}
			}

			if (*g_start_items->string)
				Player_GiveStartItems(ent, g_start_items->string);
			if (level.start_items && *level.start_items)
				Player_GiveStartItems(ent, level.start_items);

			if (false) // Race mode removed
				client->pers.inventory[IT_COMPASS] = 1;
			else if (!deathmatch->integer || level.match_state < matchst_t::MATCH_IN_PROGRESS)
				// compass also used for ready status toggling in deathmatch
				client->pers.inventory[IT_COMPASS] = 1;

			bool give_grapple = (!strcmp(g_allow_grapple->string, "auto")) ?
				(GTF(GTF_CTF) ? !level.no_grapple : 0) :
				(g_allow_grapple->integer && !g_grapple_offhand->integer);
			if (give_grapple)
				client->pers.inventory[IT_WEAPON_GRAPPLE] = 1;
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

/*
================
PlayersRangeFromSpot

Returns the distance to the nearest player from the given spot
muffmode: excludes current client
================
*/
static float PlayersRangeFromSpot(gentity_t *ent, gentity_t *spot) {
	float	bestplayerdistance;
	vec3_t	v;
	float	playerdistance;

	bestplayerdistance = 9999999;

	for (auto ec : active_clients()) {
		if (ec->health <= 0 || ec->client->eliminated)
			continue;
#if 0
		if (ent != nullptr)
			if (ec == ent)
				continue;
#endif
		v = spot->s.origin - ec->s.origin;
		playerdistance = v.length();

		if (playerdistance < bestplayerdistance)
			bestplayerdistance = playerdistance;
	}

	return bestplayerdistance;
}

static bool SpawnPointClear(gentity_t *spot) {
	vec3_t p = spot->s.origin + vec3_t{ 0, 0, 9.f };
	return !gi.trace(p, PLAYER_MINS, PLAYER_MAXS, p, spot, CONTENTS_PLAYER | CONTENTS_MONSTER).startsolid;
}

select_spawn_result_t SelectDeathmatchSpawnPoint(gentity_t *ent, vec3_t avoid_point, playerspawn_t mode, bool force_spawn, bool fallback_to_ctf_or_start, bool intermission, bool initial) {
	float cv_dist = g_dm_respawn_point_min_dist->value;
	struct spawn_point_t {
		gentity_t *point;
		float dist;
	};

	static std::vector<spawn_point_t> spawn_points;

	spawn_points.clear();

	// gather all spawn points 
	gentity_t *spot = nullptr;

	if (cv_dist > 512) cv_dist = 512;
	else if (cv_dist < 0) cv_dist = 0;

	if (intermission)
		while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_intermission")) != nullptr)
			spawn_points.push_back({ spot, PlayersRangeFromSpot(ent, spot) });

	if (spawn_points.size() == 0) {
		spot = nullptr;
		while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_deathmatch")) != nullptr)
			spawn_points.push_back({ spot, PlayersRangeFromSpot(ent, spot) });

		// no points
		if (spawn_points.size() == 0) {
			// try CTF spawns...
			if (fallback_to_ctf_or_start) {
				spot = nullptr;
				while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_team_red")) != nullptr)
					spawn_points.push_back({ spot, PlayersRangeFromSpot(ent, spot) });
				spot = nullptr;
				while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_team_blue")) != nullptr)
					spawn_points.push_back({ spot, PlayersRangeFromSpot(ent, spot) });

				// we only have an info_player_start then
				if (spawn_points.size() == 0) {
					spot = G_FindByString<&gentity_t::classname>(nullptr, "info_player_start");

					if (spot)
						spawn_points.push_back({ spot, PlayersRangeFromSpot(ent, spot) });

					// map is malformed
					if (spawn_points.size() == 0)
						return { nullptr, false };
				}
			} else
				return { nullptr, false };
		}
	}

	// if there's only one spawn point, that's the one.
	if (spawn_points.size() == 1) {
		if (force_spawn || SpawnPointClear(spawn_points[0].point))
			return { spawn_points[0].point, true };

		return { nullptr, true };
	}

	// order by distances ascending (top of list has closest players to point)
	std::sort(spawn_points.begin(), spawn_points.end(), [](const spawn_point_t &a, const spawn_point_t &b) { return a.dist < b.dist; });

	switch (mode) {
	default:	// high random
	case playerspawn_t::SPAWN_FAR_HALF:		// farthest half
		{
			size_t margin = spawn_points.size() / 2;

			// for random, select a random point other than the two
			// that are closest to the player if possible.
			// shuffle the non-distance-related spawn points
			std::shuffle(spawn_points.begin() + margin, spawn_points.end(), mt_rand);

			// run down the list and pick the first one that we can use
			for (auto it = spawn_points.begin() + margin; it != spawn_points.end(); ++it) {
				auto spot = it->point;

				if (avoid_point == spot->s.origin)
					continue;
				//muff: avoid respawning at or close to last spawn point
				if (avoid_point && cv_dist) {
					vec3_t v = spot->s.origin - avoid_point;
					float d = v.length();

					if (d <= cv_dist) {
						if (g_dm_respawn_point_min_dist_debug->integer)
							gi.Com_PrintFmt("{}: avoiding spawn point\n", *spot);
						continue;
					}
				}

				if (ent && ent->client) {
					if (ent->client->sess.is_a_bot)
						if (spot->flags & FL_NO_BOTS)
							continue;
					if (!ent->client->sess.is_a_bot)
						if (spot->flags & FL_NO_HUMANS)
							continue;
				}

				if (SpawnPointClear(spot))
					return { spot, true };
			}

			// none clear, so we have to pick one of the other two
			if (SpawnPointClear(spawn_points[1].point))
				return { spawn_points[1].point, true };
			else if (SpawnPointClear(spawn_points[0].point))
				return { spawn_points[0].point, true };

			break;
		}
	case playerspawn_t::SPAWN_FARTHEST:		// farthest
		{
			size_t count = spawn_points.end() - spawn_points.begin();
			size_t size = spawn_points.size();
			//gi.Com_PrintFmt("count:{} size:{}\n", count, size);
			for (int32_t i = spawn_points.size() - 1; i >= 0; --i) {
				//muff: avoid respawning at or close to last spawn point
				if (avoid_point && cv_dist) {
					vec3_t v = spawn_points[i].point->s.origin - avoid_point;
					float d = v.length();

					if (d <= cv_dist) {
						if (g_dm_respawn_point_min_dist_debug->integer)
							gi.Com_PrintFmt("{}: avoiding spawn point\n", *spawn_points[i].point);
						continue;
					}
				}

				if (ent->client->sess.is_a_bot)
					if (spot->flags & FL_NO_BOTS)
						continue;
				if (!ent->client->sess.is_a_bot)
					if (spot->flags & FL_NO_HUMANS)
						continue;

				if (SpawnPointClear(spawn_points[i].point))
					return { spawn_points[i].point, true };
			}
			// none clear, so we have to pick one of the other two
			if (SpawnPointClear(spawn_points[1].point))
				return { spawn_points[1].point, true };
			else if (SpawnPointClear(spawn_points[0].point))
				return { spawn_points[0].point, true };

			break;
		}
	case playerspawn_t::SPAWN_NEAREST:		// nearest
		{
			size_t count = spawn_points.end() - spawn_points.begin();
			size_t size = spawn_points.size();
			//gi.Com_PrintFmt("count:{} size:{}\n", count, size);
			for (int32_t i = 0; i < spawn_points.size(); i++) {
				//muff: avoid respawning at or close to last spawn point
				if (avoid_point && cv_dist) {
					vec3_t v = spawn_points[i].point->s.origin - avoid_point;
					float d = v.length();

					if (d <= cv_dist) {
						if (g_dm_respawn_point_min_dist_debug->integer)
							gi.Com_PrintFmt("{}: avoiding spawn point.\n", *spawn_points[i].point);
						continue;
					}
				}

				if (ent->client->sess.is_a_bot)
					if (spot->flags & FL_NO_BOTS)
						continue;
				if (!ent->client->sess.is_a_bot)
					if (spot->flags & FL_NO_HUMANS)
						continue;

				if (SpawnPointClear(spawn_points[i].point))
					return { spawn_points[i].point, true };
			}
			// none clear
			break;
		}
	}

	if (force_spawn)
		return { random_element(spawn_points).point, true };

	return { nullptr, true };
}

static vec3_t TeamCentralPoint(team_t team) {
	vec3_t	team_origin = { 0, 0, 0 };
	uint8_t team_count = 0;
	for (auto ec : active_players()) {
		if (ec->client->sess.team != team)
			continue;

		team_origin += ec->s.origin;
		team_count++;
	}
	if (team_origin)
		return team_origin / team_count;
	else
		return team_origin;
}

/*
================
SelectTeamSpawnPoint

Go to a team point, but NOT the two points closest
to other players
================
*/
static gentity_t *SelectTeamSpawnPoint(gentity_t *ent, bool force_spawn) {
	if (ent->client->resp.ctf_state) {
		select_spawn_result_t result = SelectDeathmatchSpawnPoint(ent, ent->client->spawn_origin, (playerspawn_t)clamp(g_dm_spawn_farthest->integer, 0, 3), force_spawn, false, false, false);	// !ClientIsPlaying(ent->client));

		if (result.any_valid)
			return result.spot;
	}
	/*
	vec3_t	team_origin = TeamCentralPoint(ent->client->sess.team);
	gi.LocClient_Print(ent, PRINT_HIGH, "team central point= {} {} {}\n", team_origin[0], team_origin[1], team_origin[2]);
	if (ent->client->resp.ctf_state) {
		select_spawn_result_t result = SelectDeathmatchSpawnPoint(ent, team_origin, SPAWN_NEAREST, force_spawn, false, !ClientIsPlaying(ent->client) || ent->client->eliminated, false);

		if (result.any_valid)
			return result.spot;
	}
	*/
	const char *cname;

	switch (ent->client->sess.team) {
		case TEAM_RED:
			cname = "info_player_team_red";
			break;
		case TEAM_BLUE:
			cname = "info_player_team_blue";
			break;
		default:
		{
			select_spawn_result_t result = SelectDeathmatchSpawnPoint(ent, ent->client->spawn_origin, (playerspawn_t)clamp(g_dm_spawn_farthest->integer, 0, 3), force_spawn, true, false, false);

			if (result.any_valid)
				return result.spot;

			gi.Com_Error("Can't find suitable spectator spawn point.");
			return nullptr;
		}
	}

	static std::vector<gentity_t *> spawn_points;
	gentity_t *spot = nullptr;

	spawn_points.clear();

	while ((spot = G_FindByString<&gentity_t::classname>(spot, cname)) != nullptr)
		spawn_points.push_back(spot);

	if (!spawn_points.size()) {
		select_spawn_result_t result = SelectDeathmatchSpawnPoint(ent, ent->client->spawn_origin, (playerspawn_t)clamp(g_dm_spawn_farthest->integer, 0, 3), force_spawn, true, false, false);

		if (!result.any_valid)
			gi.Com_Error("Can't find suitable team spawn point.");

		return result.spot;
	}

	std::shuffle(spawn_points.begin(), spawn_points.end(), mt_rand);

	for (auto &point : spawn_points)
		if (SpawnPointClear(point))
			return point;

	if (force_spawn)
		return random_element(spawn_points);

	return nullptr;
}

static gentity_t *SelectLavaCoopSpawnPoint(gentity_t *ent) {
	int		 index;
	gentity_t *spot = nullptr;
	float	 lavatop;
	gentity_t *lava;
	gentity_t *pointWithLeastLava;
	float	 lowest;
	gentity_t *spawnPoints[64];
	vec3_t	 center;
	int		 numPoints;
	gentity_t *highestlava;

	lavatop = -99999;
	highestlava = nullptr;

	// first, find the highest lava
	// remember that some will stop moving when they've filled their
	// areas...
	lava = nullptr;
	while (1) {
		lava = G_FindByString<&gentity_t::classname>(lava, "func_water");
		if (!lava)
			break;

		center = lava->absmax + lava->absmin;
		center *= 0.5f;

		if (lava->spawnflags.has(SPAWNFLAG_WATER_SMART) && (gi.pointcontents(center) & MASK_WATER)) {
			if (lava->absmax[2] > lavatop) {
				lavatop = lava->absmax[2];
				highestlava = lava;
			}
		}
	}

	// if we didn't find ANY lava, then return nullptr
	if (!highestlava)
		return nullptr;

	// find the top of the lava and include a small margin of error (plus bbox size)
	lavatop = highestlava->absmax[2] + 64;

	// find all the lava spawn points and store them in spawnPoints[]
	spot = nullptr;
	numPoints = 0;
	while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_coop_lava"))) {
		if (numPoints == 64)
			break;

		spawnPoints[numPoints++] = spot;
	}

	// walk up the sorted list and return the lowest, open, non-lava spawn point
	spot = nullptr;
	lowest = 999999;
	pointWithLeastLava = nullptr;
	for (index = 0; index < numPoints; index++) {
		if (spawnPoints[index]->s.origin[2] < lavatop)
			continue;

		if (PlayersRangeFromSpot(ent, spawnPoints[index]) > 32) {
			if (spawnPoints[index]->s.origin[2] < lowest) {
				// save the last point
				pointWithLeastLava = spawnPoints[index];
				lowest = spawnPoints[index]->s.origin[2];
			}
		}
	}

	return pointWithLeastLava;
}

// [Paril-KEX]
static gentity_t *SelectSingleSpawnPoint(gentity_t *ent) {
	gentity_t *spot = nullptr;

	while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_start")) != nullptr) {
		if (!game.spawnpoint[0] && !spot->targetname)
			break;

		if (!game.spawnpoint[0] || !spot->targetname)
			continue;

		if (Q_strcasecmp(game.spawnpoint, spot->targetname) == 0)
			break;
	}

	if (!spot) {
		// there wasn't a matching targeted spawnpoint, use one that has no targetname
		while ((spot = G_FindByString<&gentity_t::classname>(spot, "info_player_start")) != nullptr)
			if (!spot->targetname)
				return spot;
	}

	// none at all, so just pick any
	if (!spot)
		return G_FindByString<&gentity_t::classname>(spot, "info_player_start");

	return spot;
}

// [Paril-KEX]
static gentity_t *G_UnsafeSpawnPosition(vec3_t spot, bool check_players) {
	contents_t mask = MASK_PLAYERSOLID;

	if (!check_players)
		mask &= ~CONTENTS_PLAYER;

	trace_t tr = gi.trace(spot, PLAYER_MINS, PLAYER_MAXS, spot, nullptr, mask);

	// sometimes the spot is too close to the ground, give it a bit of slack
	if (tr.startsolid && !tr.ent->client) {
		spot[2] += 1;
		tr = gi.trace(spot, PLAYER_MINS, PLAYER_MAXS, spot, nullptr, mask);
	}

	// no idea why this happens in some maps..
	if (tr.startsolid && !tr.ent->client) {
		// try a nudge
		if (G_FixStuckObject_Generic(spot, PLAYER_MINS, PLAYER_MAXS, [mask](const vec3_t &start, const vec3_t &mins, const vec3_t &maxs, const vec3_t &end) {
			return gi.trace(start, mins, maxs, end, nullptr, mask);
			}) == stuck_result_t::NO_GOOD_POSITION)
			return tr.ent; // what do we do here...?

			trace_t tr = gi.trace(spot, PLAYER_MINS, PLAYER_MAXS, spot, nullptr, mask);

			if (tr.startsolid && !tr.ent->client)
				return tr.ent; // what do we do here...?
	}

	if (tr.fraction == 1.f)
		return nullptr;
	else if (check_players && tr.ent && tr.ent->client)
		return tr.ent;

	return nullptr;
}

static gentity_t *SelectCoopSpawnPoint(gentity_t *ent, bool force_spawn, bool check_players) {
	gentity_t *spot = nullptr;
	const char *target;

	//  rogue hack, but not too gross...
	if (!Q_strcasecmp(level.mapname, "rmine2"))
		return SelectLavaCoopSpawnPoint(ent);

	// try the main spawn point first
	spot = SelectSingleSpawnPoint(ent);

	if (spot && !G_UnsafeSpawnPosition(spot->s.origin, check_players))
		return spot;

	spot = nullptr;

	// assume there are four coop spots at each spawnpoint
	int32_t num_valid_spots = 0;

	while (1) {
		spot = G_FindByString<&gentity_t::classname>(spot, "info_player_coop");
		if (!spot)
			break; // we didn't have enough...

		target = spot->targetname;
		if (!target)
			target = "";
		if (Q_strcasecmp(game.spawnpoint, target) == 0) { // this is a coop spawn point for one of the clients here
			num_valid_spots++;

			if (!G_UnsafeSpawnPosition(spot->s.origin, check_players))
				return spot; // this is it
		}
	}

	bool use_targetname = true;

	// if we didn't find any spots, map is probably set up wrong.
	// use empty targetname ones.
	if (!num_valid_spots) {
		use_targetname = false;

		while (1) {
			spot = G_FindByString<&gentity_t::classname>(spot, "info_player_coop");
			if (!spot)
				break; // we didn't have enough...

			target = spot->targetname;
			if (!target) {
				// this is a coop spawn point for one of the clients here
				num_valid_spots++;

				if (!G_UnsafeSpawnPosition(spot->s.origin, check_players))
					return spot; // this is it
			}
		}
	}

	// if player collision is disabled, just pick a random spot
	if (!g_coop_player_collision->integer) {
		spot = nullptr;

		num_valid_spots = irandom(num_valid_spots);

		while (1) {
			spot = G_FindByString<&gentity_t::classname>(spot, "info_player_coop");

			if (!spot)
				break; // we didn't have enough...

			target = spot->targetname;
			if (use_targetname && !target)
				target = "";
			if (use_targetname ? (Q_strcasecmp(game.spawnpoint, target) == 0) : !target) { // this is a coop spawn point for one of the clients here
				num_valid_spots++;

				if (!num_valid_spots)
					return spot;

				--num_valid_spots;
			}
		}

		// if this fails, just fall through to some other spawn.
	}

	// no safe spots..?
	if (force_spawn || !g_coop_player_collision->integer)
		return SelectSingleSpawnPoint(spot);

	return nullptr;
}

static bool TryLandmarkSpawn(gentity_t *ent, vec3_t &origin, vec3_t &angles) {
	// if transitioning from another level with a landmark seamless transition
	// just set the location here
	if (!ent->client->landmark_name || !strlen(ent->client->landmark_name)) {
		return false;
	}

	gentity_t *landmark = G_PickTarget(ent->client->landmark_name);
	if (!landmark) {
		return false;
	}

	vec3_t old_origin = origin;
	vec3_t spot_origin = origin;
	origin = ent->client->landmark_rel_pos;

	// rotate our relative landmark into our new landmark's frame of reference
	origin = RotatePointAroundVector({ 1, 0, 0 }, origin, landmark->s.angles[PITCH]);
	origin = RotatePointAroundVector({ 0, 1, 0 }, origin, landmark->s.angles[ROLL]);
	origin = RotatePointAroundVector({ 0, 0, 1 }, origin, landmark->s.angles[YAW]);

	origin += landmark->s.origin;

	angles = ent->client->oldviewangles + landmark->s.angles;

	if (landmark->spawnflags.has(SPAWNFLAG_LANDMARK_KEEP_Z))
		origin[2] = spot_origin[2];

	// sometimes, landmark spawns can cause slight inconsistencies in collision;
	// we'll do a bit of tracing to make sure the bbox is clear
	if (G_FixStuckObject_Generic(origin, PLAYER_MINS, PLAYER_MAXS, [ent](const vec3_t &start, const vec3_t &mins, const vec3_t &maxs, const vec3_t &end) {
		return gi.trace(start, mins, maxs, end, ent, MASK_PLAYERSOLID & ~CONTENTS_PLAYER);
		}) == stuck_result_t::NO_GOOD_POSITION) {
		origin = old_origin;
		return false;
	}

	ent->s.origin = origin;

	// rotate the velocity that we grabbed from the map
	if (ent->velocity) {
		ent->velocity = RotatePointAroundVector({ 1, 0, 0 }, ent->velocity, landmark->s.angles[PITCH]);
		ent->velocity = RotatePointAroundVector({ 0, 1, 0 }, ent->velocity, landmark->s.angles[ROLL]);
		ent->velocity = RotatePointAroundVector({ 0, 0, 1 }, ent->velocity, landmark->s.angles[YAW]);
	}

	return true;
}

/*
===========
SelectSpawnPoint

Chooses a player start, deathmatch start, coop start, etc
============
*/
bool SelectSpawnPoint(gentity_t *ent, vec3_t &origin, vec3_t &angles, bool force_spawn, bool &landmark) {
	gentity_t *spot = nullptr;

	// DM spots are simple
	if (deathmatch->integer) {
		if (Teams() && ClientIsPlaying(ent->client))
			spot = SelectTeamSpawnPoint(ent, force_spawn);
		else {
			if (false) // Race mode removed
				spot = SelectSingleSpawnPoint(ent);

			if (!spot) {
				select_spawn_result_t result = SelectDeathmatchSpawnPoint(ent, ent->client->spawn_origin, (playerspawn_t)clamp(g_dm_spawn_farthest->integer, 0, 3), force_spawn, true, !ClientIsPlaying(ent->client) || ent->client->eliminated, false);

				if (!result.any_valid)
					gi.Com_Error("No valid spawn points found.");

				spot = result.spot;
			}
		}

		if (spot) {
			origin = spot->s.origin + vec3_t{ 0, 0, (float)(g_dm_spawnpads->integer ? 9 : 1) };
			angles = spot->s.angles;

			//muff mode: we just want yaw really, definitely no roll!
			//if (ClientIsPlaying(ent->client))
				//angles[PITCH] = 0;
			angles[ROLL] = 0;

			return true;
		}

		return false;
	}

	if (coop->integer) {
		spot = SelectCoopSpawnPoint(ent, force_spawn, true);

		if (!spot)
			spot = SelectCoopSpawnPoint(ent, force_spawn, false);

		// no open spot yet
		if (!spot) {
			// in worst case scenario in coop during intermission, just spawn us at intermission
			// spot. this only happens for a single frame, and won't break
			// anything if they come back.
			if (level.intermission_time) {
				origin = level.intermission_origin;
				angles = level.intermission_angle;
				return true;
			}

			return false;
		}
	} else {
		spot = SelectSingleSpawnPoint(ent);

		// in SP, just put us at the origin if spawn fails
		if (!spot) {
			gi.Com_PrintFmt("Couldn't find spawn point {}\n", game.spawnpoint);

			origin = { 0, 0, 0 };
			angles = { 0, 0, 0 };

			return true;
		}
	}

	// spot should always be non-null here

	origin = spot->s.origin;
	angles = spot->s.angles;

	// check landmark
	if (TryLandmarkSpawn(ent, origin, angles))
		landmark = true;

	return true;
}

/*
===========
SelectSpectatorSpawnPoint

============
*/
static gentity_t *SelectSpectatorSpawnPoint(vec3_t origin, vec3_t angles) {
	//FindIntermissionPoint();
	SetIntermissionPoint();
	origin = level.intermission_origin;
	angles = level.intermission_angle;

	return level.spawn_spots[SPAWN_SPOT_INTERMISSION]; // was NULL
}

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
	bool frozen = !!(GT(GT_FREEZE) && !level.intermission_time && ent->client->eliminated && !ent->client->resp.thawer);

	// grab a body que and cycle to the next one
	body = &g_entities[game.maxclients + level.body_que + 1];
	level.body_que = (level.body_que + 1) % BODY_QUEUE_SIZE;

	// FIXME: send an effect on the removed body

	gi.unlinkentity(ent);

	gi.unlinkentity(body);
	body->s = ent->s;
	body->s.number = body - g_entities;
	body->s.skinnum = ent->s.skinnum & 0xFF; // only copy the client #

	if (frozen) {
		body->s.effects |= EF_COLOR_SHELL;
		body->s.renderfx |= (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE);
	} else {
		body->s.effects = EF_NONE;
		body->s.renderfx = RF_NONE;
	}

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

	if (ent->takedamage) {
		body->mins = ent->mins;
		body->maxs = ent->maxs;
	} else
		body->mins = body->maxs = {};

	if (g_corpse_sink_time->value > 0 && notGT(GT_FREEZE)) {
		body->timestamp = level.time + gtime_t::from_sec(g_corpse_sink_time->value + 1.5);
		body->nextthink = level.time + gtime_t::from_sec(g_corpse_sink_time->value);
		body->think = BodySink;
	}

	body->die = body_die;
	body->takedamage = true;

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
	
	if (deathmatch->integer && level.match_state == matchst_t::MATCH_WARMUP_READYUP)
		BroadcastReadyReminderMessage();
}

void ClientSetEliminated(gentity_t *self) {
	self->client->eliminated = true;
	//MoveClientToFreeCam(self);
}

void ClientRespawn(gentity_t *ent) {
	if (deathmatch->integer || coop->integer) {
		// spectators don't leave bodies
		if (ClientIsPlaying(ent->client))
			CopyToBodyQue(ent);
		ent->svflags &= ~SVF_NOCLIENT;

		if (GT(GT_RR) && level.match_state == matchst_t::MATCH_IN_PROGRESS) {
			ent->client->sess.team = Teams_OtherTeam(ent->client->sess.team);
			G_AssignPlayerSkin(ent, ent->client->pers.skin);
		}

		ClientSpawn(ent);
		G_PostRespawn(ent);
		return;
	}

	// restart the entire server
	gi.AddCommandString("menu_loadgame\n");
}

//==============================================================

// [Paril-KEX]
// skinnum was historically used to pack data
// so we're going to build onto that.
void P_AssignClientSkinnum(gentity_t *ent) {
	if (ent->s.modelindex != 255)
		return;

	player_skinnum_t packed;

	packed.client_num = ent->client - game.clients;
	if (ent->client->pers.weapon)
		packed.vwep_index = ent->client->pers.weapon->vwep_index - level.vwep_offset + 1;
	else
		packed.vwep_index = 0;
	packed.viewheight = ent->client->ps.viewoffset.z + ent->client->ps.pmove.viewheight;

	if (InCoopStyle())
		packed.team_index = 1; // all players are teamed in coop
	else if (Teams())
		packed.team_index = ent->client->sess.team;
	else
		packed.team_index = 0;

	if (ent->deadflag)
		packed.poi_icon = 1;
	else
		packed.poi_icon = 0;

	ent->s.skinnum = packed.skinnum;
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
// optionally instantaneously (ignore any transition time)
void P_ForceFogTransition(gentity_t *ent, bool instant) {
	// sanity check; if we're not changing the values, don't bother
	if (ent->client->fog == ent->client->pers.wanted_fog &&
		ent->client->heightfog == ent->client->pers.wanted_heightfog)
		return;

	svc_fog_data_t fog{};

	// check regular fog
	if (ent->client->pers.wanted_fog[0] != ent->client->fog[0] ||
		ent->client->pers.wanted_fog[4] != ent->client->fog[4]) {
		fog.bits |= svc_fog_data_t::BIT_DENSITY;
		fog.density = ent->client->pers.wanted_fog[0];
		fog.skyfactor = ent->client->pers.wanted_fog[4] * 255.f;
	}
	if (ent->client->pers.wanted_fog[1] != ent->client->fog[1]) {
		fog.bits |= svc_fog_data_t::BIT_R;
		fog.red = ent->client->pers.wanted_fog[1] * 255.f;
	}
	if (ent->client->pers.wanted_fog[2] != ent->client->fog[2]) {
		fog.bits |= svc_fog_data_t::BIT_G;
		fog.green = ent->client->pers.wanted_fog[2] * 255.f;
	}
	if (ent->client->pers.wanted_fog[3] != ent->client->fog[3]) {
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

	if (hf.falloff != wanted_hf.falloff) {
		fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_FALLOFF;
		if (!wanted_hf.falloff)
			fog.hf_falloff = 0;
		else
			fog.hf_falloff = wanted_hf.falloff;
	}
	if (hf.density != wanted_hf.density) {
		fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_DENSITY;

		if (!wanted_hf.density)
			fog.hf_density = 0;
		else
			fog.hf_density = wanted_hf.density;
	}

	if (hf.start[0] != wanted_hf.start[0]) {
		fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_START_R;
		fog.hf_start_r = wanted_hf.start[0] * 255.f;
	}
	if (hf.start[1] != wanted_hf.start[1]) {
		fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_START_G;
		fog.hf_start_g = wanted_hf.start[1] * 255.f;
	}
	if (hf.start[2] != wanted_hf.start[2]) {
		fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_START_B;
		fog.hf_start_b = wanted_hf.start[2] * 255.f;
	}
	if (hf.start[3] != wanted_hf.start[3]) {
		fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_START_DIST;
		fog.hf_start_dist = wanted_hf.start[3];
	}

	if (hf.end[0] != wanted_hf.end[0]) {
		fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_END_R;
		fog.hf_end_r = wanted_hf.end[0] * 255.f;
	}
	if (hf.end[1] != wanted_hf.end[1]) {
		fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_END_G;
		fog.hf_end_g = wanted_hf.end[1] * 255.f;
	}
	if (hf.end[2] != wanted_hf.end[2]) {
		fog.bits |= svc_fog_data_t::BIT_HEIGHTFOG_END_B;
		fog.hf_end_b = wanted_hf.end[2] * 255.f;
	}
	if (hf.end[3] != wanted_hf.end[3]) {
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
		ent->client->ps.stats[STAT_SHOW_STATUSBAR] = 1;
		return true;
	}

	// already initialised
	if (ent->client->sess.team != TEAM_NONE)
		return true;

	ent->client->sess.team = TEAM_SPECTATOR;
	MoveClientToFreeCam(ent);
	
	if (ent->client->sess.is_a_bot || (ent->svflags & SVF_BOT) || g_dm_force_join->integer || g_dm_auto_join->integer) {
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
	client_respawn_t		resp;
	client_session_t		sess;

	if (GTF(GTF_ROUNDS) && GTF(GTF_ELIMINATION) && level.match_state == matchst_t::MATCH_IN_PROGRESS && notGT(GT_HORDE))
		if (level.round_state == roundst_t::ROUND_IN_PROGRESS || level.round_state == roundst_t::ROUND_ENDED)
			ClientSetEliminated(ent);
	bool eliminated = ent->client->eliminated;
	int lives = 0;
	if (InCoopStyle() && g_coop_enable_lives->integer)
		lives = ent->client->pers.spawned ? ent->client->pers.lives : g_coop_enable_lives->integer + 1;
	
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
			ClientUserinfoChanged(ent, userinfo);

			client->respawn_timeout = level.time + 3_sec;
		}

		// find a spot to place us
		//SetIntermissionPoint();

		ent->s.origin = level.intermission_origin;
		ent->client->ps.pmove.origin = level.intermission_origin;
		ent->client->ps.viewangles = level.intermission_angle;

		client->awaiting_respawn = true;
		client->ps.pmove.pm_type = PM_FREEZE;
		client->ps.rdflags = RDF_NONE;
		ent->deadflag = false;

		MoveClientToFreeCam(ent);
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
			memset(&resp, 0, sizeof(resp));
			client->sess.team = TEAM_FREE;
		}
	}

	// clear everything but the persistant data
	saved = client->pers;
	memset(client, 0, sizeof(*client));
	client->pers = saved;
	client->resp = resp;
	client->sess = sess;
	client->chaingun_shots = 1; // first shot deals 5 damage (4 + (1 & 1))

	// on a new, fresh spawn (always in DM, clear inventory
	// or new spawns in SP/coop)
	if (client->pers.health <= 0)
		InitClientPersistant(ent, client);

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
	gi.Info_ValueForKey(ent->client->pers.userinfo, "fov", val, sizeof(val));
	ent->client->ps.fov = clamp((float)strtoul(val, nullptr, 10), 1.f, 160.f);

	ent->client->ps.pmove.viewheight = ent->viewheight;

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
		gi.linkentity(ent);
		return;
	}
	ent->client->ps.stats[STAT_SHOW_STATUSBAR] = 1;

	// ensure playing players are visible (clear SVF_NOCLIENT that may have been set by Entities_Reset)
	ent->svflags &= ~SVF_NOCLIENT;

	// [Paril-KEX] a bit of a hack, but landmark spawns can sometimes cause
	// intersecting spawns, so we'll do a sanity check here...
	if (spawn_from_begin) {
		if (coop->integer) {
			gentity_t *collision = G_UnsafeSpawnPosition(ent->s.origin, true);

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

	if (!KillBox(ent, true, MOD_TELEFRAG_SPAWN)) { // could't spawn in?
	}

	// my tribute to cash's level-specific hacks. I hope I live
	// up to his trailblazing cheese.
	if (Q_strcasecmp(level.mapname, "rboss") == 0) {
		// if you get on to rboss in single player or coop, ensure
		// the player has the nuke key. (not in DM)
		if (!deathmatch->integer)
			client->pers.inventory[IT_KEY_NUKE] = 1;
	}
	
	// force the current weapon up
	if (GTF(GTF_ARENA) && client->pers.inventory[IT_WEAPON_RLAUNCHER])
		client->newweapon = &itemlist[IT_WEAPON_RLAUNCHER];
	else
		client->newweapon = client->pers.weapon;
	Change_Weapon(ent);

	if (was_waiting_for_respawn)
		G_PostRespawn(ent);
}

/*
=====================
ClientBeginDeathmatch

A client has just connected to the server in
deathmatch mode, so clear everything out before starting them.
=====================
*/
static void ClientBeginDeathmatch(gentity_t *ent) {
	G_InitGentity(ent);

	// make sure we have a known default
	ent->svflags |= SVF_PLAYER;

	InitClientResp(ent->client);

	// locate ent at a spawn point
	ClientSpawn(ent);

	if (level.intermission_time) {
		MoveClientToIntermission(ent);
	} else {
		if (!(ent->svflags & SVF_NOCLIENT)) {
			// send effect
			gi.WriteByte(svc_muzzleflash);
			gi.WriteEntity(ent);
			gi.WriteByte(MZ_LOGIN);
			gi.multicast(ent->s.origin, MULTICAST_PVS, false);
		}
	}

	//gi.LocBroadcast_Print(PRINT_HIGH, "$g_entered_game", ent->client->resp.netname);

	// make sure all view stuff is valid
	ClientEndServerFrame(ent);
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
bool ClientIsPlaying(gclient_t *cl) {
	if (!cl) return false;

	if (!deathmatch->integer)
		return true;

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
	ent->client = game.clients + (ent - g_entities - 1);
	ent->client->awaiting_respawn = false;
	ent->client->respawn_timeout = 0_ms;

	// set inactivity timer
	gtime_t cv = gtime_t::from_sec(g_inactivity->integer);
	if (cv) {
		if (cv < 15_sec) cv = 15_sec;
		ent->client->sess.inactivity_time = level.time + cv;
		ent->client->sess.inactivity_warning = false;
	}

	// [Paril-KEX] we're always connected by this point...
	ent->client->pers.connected = true;

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

/*
================
P_GetLobbyUserNum
================
*/
unsigned int P_GetLobbyUserNum(const gentity_t *player) {
	unsigned int playerNum = 0;
	if (player > g_entities && player < g_entities + MAX_ENTITIES) {
		playerNum = (player - g_entities) - 1;
		if (playerNum >= MAX_CLIENTS) {
			playerNum = 0;
		}
	}
	return playerNum;
}

/*
================
G_EncodedPlayerName

Gets a token version of the players "name" to be decoded on the client.
================
*/
static std::string G_EncodedPlayerName(gentity_t *player) {
	unsigned int playernum = P_GetLobbyUserNum(player);
	return std::string("##P") + std::to_string(playernum);
}

/*
================
Match_Ghost_Assign
================
*/
void Match_Ghost_Assign(gentity_t *ent) {
	int ghost, i;

	for (ghost = 0; ghost < MAX_CLIENTS; ghost++)
		if (!level.ghosts[ghost].code)
			break;
	if (ghost == MAX_CLIENTS)
		return;
	level.ghosts[ghost].team = ent->client->sess.team;
	level.ghosts[ghost].score = 0;
	for (;;) {
		level.ghosts[ghost].code = irandom(10000, 100000);
		for (i = 0; i < MAX_CLIENTS; i++)
			if (i != ghost && level.ghosts[i].code == level.ghosts[ghost].code)
				break;
		if (i == MAX_CLIENTS)
			break;
	}
	level.ghosts[ghost].ent = ent;
	Q_strlcpy(level.ghosts[ghost].netname, ent->client->resp.netname, sizeof(level.ghosts[ghost].netname));
	ent->client->resp.ghost = level.ghosts + ghost;
	gi.LocClient_Print(ent, PRINT_CHAT, "Your ghost code is **** {} ****\n", level.ghosts[ghost].code);
	gi.LocClient_Print(ent, PRINT_HIGH, "If you lose connection, you can rejoin with your score intact by typing \"ghost {}\".\n",
		level.ghosts[ghost].code);
}

/*
================
Match_Ghost_DoAssign
================
*/
void Match_Ghost_DoAssign(gentity_t *ent) {
	// assign a ghost code
	if (level.match_state == matchst_t::MATCH_IN_PROGRESS) {
		if (ent->client->resp.ghost)
			ent->client->resp.ghost->code = 0;
		ent->client->resp.ghost = nullptr;
		Match_Ghost_Assign(ent);
	}
}

/*
===========
ClientUserInfoChanged

called whenever the player updates a userinfo variable.
============
*/
void ClientUserinfoChanged(gentity_t *ent, const char *userinfo) {
	char val[MAX_INFO_VALUE] = { 0 };

	// set name
	if (!gi.Info_ValueForKey(userinfo, "name", ent->client->pers.netname, sizeof(ent->client->pers.netname)))
		Q_strlcpy(ent->client->pers.netname, "badinfo", sizeof(ent->client->pers.netname));

	Q_strlcpy(ent->client->resp.netname, ent->client->pers.netname, sizeof(ent->client->resp.netname));

	// set skin
	if (!gi.Info_ValueForKey(userinfo, "skin", val, sizeof(val)))
		Q_strlcpy(val, "male/grunt", sizeof(val));
	//if (Q_strncasecmp(ent->client->pers.skin, val, sizeof(ent->client->pers.skin))) {
		Q_strlcpy(ent->client->pers.skin, ClientSkinOverride(val), sizeof(ent->client->pers.skin));
		ent->client->pers.skin_icon_index = gi.imageindex(G_Fmt("/players/{}_i", ent->client->pers.skin).data());
	//}

	int playernum = ent - g_entities - 1;

	// combine name and skin into a configstring
	if (Teams())
		G_AssignPlayerSkin(ent, ent->client->pers.skin);

	else {
#if 0
		// set dogtag
		char dogtag[MAX_INFO_VALUE] = { 0 };
		gi.Info_ValueForKey(userinfo, "dogtag", dogtag, sizeof(dogtag));
#endif
		gi.configstring(CS_PLAYERSKINS + playernum, G_Fmt("{}\\{}", ent->client->pers.netname, ent->client->pers.skin).data());
	}

	//  set player name field (used in id_state view)
	gi.configstring(CONFIG_CHASE_PLAYER_NAME + playernum, ent->client->pers.netname);

	// [Kex] netname is used for a couple of other things, so we update this after those.
	if (!(ent->svflags & SVF_BOT)) {
		Q_strlcpy(ent->client->pers.netname, G_EncodedPlayerName(ent).c_str(), sizeof(ent->client->pers.netname));
	}

	// fov
	gi.Info_ValueForKey(userinfo, "fov", val, sizeof(val));
	ent->client->ps.fov = clamp((float)strtoul(val, nullptr, 10), 1.f, 160.f);

	// handedness
	if (gi.Info_ValueForKey(userinfo, "hand", val, sizeof(val))) {
		ent->client->pers.hand = static_cast<handedness_t>(clamp(atoi(val), (int32_t)RIGHT_HANDED, (int32_t)CENTER_HANDED));
	} else {
		ent->client->pers.hand = RIGHT_HANDED;
	}

	// [Paril-KEX] auto-switch
	if (gi.Info_ValueForKey(userinfo, "autoswitch", val, sizeof(val))) {
		ent->client->pers.autoswitch = static_cast<auto_switch_t>(clamp(atoi(val), (int32_t)auto_switch_t::SMART, (int32_t)auto_switch_t::NEVER));
	} else {
		ent->client->pers.autoswitch = auto_switch_t::SMART;
	}

	if (gi.Info_ValueForKey(userinfo, "autoshield", val, sizeof(val))) {
		ent->client->pers.autoshield = atoi(val);
	} else {
		ent->client->pers.autoshield = -1;
	}

	// [Paril-KEX] wants bob
	if (gi.Info_ValueForKey(userinfo, "bobskip", val, sizeof(val))) {
		ent->client->pers.bob_skip = val[0] == '1';
	} else {
		ent->client->pers.bob_skip = false;
	}

	// save off the userinfo in case we want to check something later
	Q_strlcpy(ent->client->pers.userinfo, userinfo, sizeof(ent->client->pers.userinfo));
}

static inline bool IsSlotIgnored(gentity_t *slot, gentity_t **ignore, size_t num_ignore) {
	for (size_t i = 0; i < num_ignore; i++)
		if (slot == ignore[i])
			return true;

	return false;
}

static inline gentity_t *ClientChooseSlot_Any(gentity_t **ignore, size_t num_ignore) {
	for (size_t i = 0; i < game.maxclients; i++)
		if (!IsSlotIgnored(globals.gentities + i + 1, ignore, num_ignore) && !game.clients[i].pers.connected)
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

// [Paril-KEX] for coop, we want to try to ensure that players will always get their
// proper slot back when they connect.
gentity_t *ClientChooseSlot(const char *userinfo, const char *social_id, bool is_bot, gentity_t **ignore, size_t num_ignore, bool cinematic) {
	// coop and non-bots is the only thing that we need to do special behavior on
	if (!cinematic && coop->integer && !is_bot)
		return ClientChooseSlot_Coop(userinfo, social_id, is_bot, ignore, num_ignore);

	// just find any free slot
	return ClientChooseSlot_Any(ignore, num_ignore);
}

static inline bool CheckBanned(gentity_t *ent, char *userinfo, const char *social_id) {
	// currently all bans are in Steamworks, don't bother if not from there
	if (social_id[0] != 'S')
		return false;

	// Israel
	if (!Q_strcasecmp(social_id, "Steamworks-76561198026297488")) {
		gi.Info_SetValueForKey(userinfo, "rejmsg", "Antisemite detected!\n");

		gentity_t *host = &g_entities[1];
		if (host && host->client) {
			if (level.time > host->client->last_banned_message_time + 10_sec) {

				char name[MAX_INFO_VALUE] = { 0 };
				gi.Info_ValueForKey(userinfo, "name", name, sizeof(name));

				gi.LocClient_Print(host, PRINT_TTS, "ANTISEMITE DETECTED ({})!\n", name);
				host->client->last_banned_message_time = level.time;
				gi.LocBroadcast_Print(PRINT_CHAT, "{}: God Bless Palestine\n", name);
			}
		}

		gi.local_sound(ent, CHAN_AUTO, gi.soundindex("world/klaxon3.wav"), 1, ATTN_NONE, 0);
		gi.AddCommandString(G_Fmt("kick {}\n", ent - g_entities - 1).data());
		return true;
	}

	// Kirlomax
	if (!Q_strcasecmp(social_id, "Steamworks-76561198001774610")) {
		gi.Info_SetValueForKey(userinfo, "rejmsg", "WARNING! KNOWN CHEATER DETECTED\n");

		gentity_t *host = &g_entities[1];
		if (host && host->client) {
			if (level.time > host->client->last_banned_message_time + 10_sec) {

				char name[MAX_INFO_VALUE] = { 0 };
				gi.Info_ValueForKey(userinfo, "name", name, sizeof(name));

				gi.LocClient_Print(host, PRINT_TTS, "WARNING! KNOWN CHEATER DETECTED ({})!\n", name);
				host->client->last_banned_message_time = level.time;
				gi.LocBroadcast_Print(PRINT_CHAT, "{}: I am a known cheater, banned from all servers.\n", name);
			}
		}

		gi.local_sound(ent, CHAN_AUTO, gi.soundindex("world/klaxon3.wav"), 1, ATTN_NONE, 0);
		gi.AddCommandString(G_Fmt("kick {}\n", ent - g_entities - 1).data());
		G_StuffCmd(ent, "disconnect\n");
		return true;
	}

	// Model192
	if (!Q_strcasecmp(social_id, "Steamworks-76561197972296343")) {
		gi.Info_SetValueForKey(userinfo, "rejmsg", "WARNING! MOANERTONE DETECTED\n");

		gentity_t *host = &g_entities[1];
		if (host && host->client) {
			if (level.time > host->client->last_banned_message_time + 10_sec) {

				char name[MAX_INFO_VALUE] = { 0 };
				gi.Info_ValueForKey(userinfo, "name", name, sizeof(name));

				gi.LocClient_Print(host, PRINT_TTS, "WARNING! MOANERTONE DETECTED ({})!\n", name);
				host->client->last_banned_message_time = level.time;
				gi.LocBroadcast_Print(PRINT_CHAT, "{}: Listen up, I have something to moan about.\n", name);
			}
		}

		gi.local_sound(ent, CHAN_AUTO, gi.soundindex("world/klaxon3.wav"), 1, ATTN_NONE, 0);
		gi.AddCommandString(G_Fmt("kick {}\n", ent - g_entities - 1).data());
		G_StuffCmd(ent, "disconnect\n");
		return true;
	}

	// Dalude
	if (!Q_strcasecmp(social_id, "Steamworks-76561199001991246") || !Q_strcasecmp(social_id, "EOS-07e230c273be4248bbf26c89033923c1")) {
		ent->client->sess.is_888 = true;
		gi.Info_SetValueForKey(userinfo, "rejmsg", "Fake 888 Agent detected!\n");
		gi.Info_SetValueForKey(userinfo, "name", "Fake 888 Agent");

		gentity_t *host = &g_entities[1];
		if (host && host->client) {
			if (level.time > host->client->last_banned_message_time + 10_sec) {

				char name[MAX_INFO_VALUE] = { 0 };
				gi.Info_ValueForKey(userinfo, "name", name, sizeof(name));

				gi.LocClient_Print(host, PRINT_TTS, "FAKE 888 AGENT DETECTED ({})!\n", name);
				host->client->last_banned_message_time = level.time;
				gi.LocBroadcast_Print(PRINT_CHAT, "{}: bejesus, what a lovely lobby! certainly better than 888's!\n", name);
			}
		}
		gi.local_sound(ent, CHAN_AUTO, gi.soundindex("world/klaxon3.wav"), 1, ATTN_NONE, 0);
		gi.AddCommandString(G_Fmt("kick {}\n", ent - g_entities - 1).data());
		G_StuffCmd(ent, "disconnect\n");
		return true;
	}
	return false;
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
#if 0
	// check to see if they are on the banned IP list
	char value[MAX_INFO_VALUE] = { 0 };
	gi.Info_ValueForKey(userinfo, "ip", value, sizeof(value));
	if (G_FilterPacket(value)) {
		gi.Info_SetValueForKey(userinfo, "rejmsg", "Banned.");
		return false;
	}
#endif
	
#if 0
	if (!is_bot && CheckBanned(ent, userinfo, social_id))
		return false;
#endif

	ent->client->sess.team = deathmatch->integer ? TEAM_NONE : TEAM_FREE;

	// they can connect
	ent->client = game.clients + (ent - g_entities - 1);

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
			//InitPlayerTeam(ent);
			ent->client->sess.pc.show_id = true;
			ent->client->sess.pc.show_timer = true;
			ent->client->sess.pc.show_fragmessages = true;
			ent->client->sess.pc.killbeep_num = 1;
			ent->client->sess.pc.follow_killer = false;
			ent->client->sess.pc.follow_powerup = false;

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

		if (bot_name_prefix->string[0] && *bot_name_prefix->string) {
			char oldname[MAX_INFO_VALUE];
			char newname[MAX_NETNAME];

			gi.Info_ValueForKey(userinfo, "name", oldname, sizeof(oldname));
			Q_strlcpy(newname, bot_name_prefix->string, sizeof(newname));
			Q_strlcat(newname, oldname, sizeof(newname));
			gi.Info_SetValueForKey(userinfo, "name", newname);
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
#if 0
	// set skin
	char val[MAX_INFO_VALUE] = { 0 };
	if (!gi.Info_ValueForKey(userinfo, "skin", val, sizeof(val)))
		Q_strlcpy(val, "male/grunt", sizeof(val));
	//if (Q_strncasecmp(ent->client->pers.skin, val, sizeof(ent->client->pers.skin))) {
	Q_strlcpy(ent->client->pers.skin, ClientSkinOverride(val), sizeof(ent->client->pers.skin));
	ent->client->pers.skin_icon_index = gi.imageindex(G_Fmt("/players/{}_i", ent->client->pers.skin).data());
#endif
	ent->client->pers.connected = true;

	ent->client->pers.ingame = true;

	// entity 1 is always server host, so make admin
	if (ent == &g_entities[1])
		ent->client->sess.admin = true;
	else {
		//TODO: check admins.txt for social_id

	}

	// count current clients and rank for scoreboard
	CalculateRanks();

	//PCfg_WriteConfig(ent);
	PCfg_ClientInitPConfig(ent);

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
	if (!ent->client)
		return;

	TossClientItems(ent);
	PlayerTrail_Destroy(ent);

	// make sure no trackers are still hurting us.
	if (ent->client->tracker_pain_time)
		RemoveAttackingPainDaemons(ent);

	if (ent->client->owned_sphere) {
		if (ent->client->owned_sphere->inuse)
			G_FreeEntity(ent->client->owned_sphere);
		ent->client->owned_sphere = nullptr;
	}

	// send effect
	if (!(ent->svflags & SVF_NOCLIENT)) {
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

	gi.unlinkentity(ent);
	ent->s.modelindex = 0;
	ent->solid = SOLID_NOT;
	ent->inuse = false;
	ent->sv.init = false;
	ent->classname = "disconnected";
	ent->client->pers.connected = false;
	ent->client->pers.spawned = false;
	ent->timestamp = level.time + 1_sec;

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

	int med_min = RS(RS_Q3A) ? 40 : 30;
	int far_min = RS(RS_Q3A) ? 61 : 55;

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
		if (!deathmatch->integer || !(g_dm_no_fall_damage->integer || GTF(GTF_ARENA))) {
			ent->pain_debounce_time = level.time + FRAME_TIME_S; // no normal pain sound
			if (RS(RS_Q3A))
				damage = ent->s.event == EV_FALL_FAR ? 10 : 5;
			else {
				damage = (int)((delta - 30) / 2);
				if (damage < 1)
					damage = 1;
			}
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

	if (ent->client->latched_buttons & (BUTTON_ATTACK | BUTTON_JUMP)) {
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
	if (!ent->client->sess.inactivity_time) {
		ent->client->sess.inactivity_time = level.time + cv;
		ent->client->sess.inactivity_warning = false;
		return true;
	}
	if (!deathmatch->integer || !cv || !ClientIsPlaying(ent->client) || ent->client->eliminated || ent->client->sess.is_a_bot || ent->s.number == 0) {
		// give everyone some time, so if the operator sets g_inactivity during
		// gameplay, everyone isn't kicked
		ent->client->sess.inactivity_time = level.time + 1_min;
		ent->client->sess.inactivity_warning = false;
	} else {
		// Check for ANY input activity, not just newly pressed buttons
		// This prevents false positives when players are actively playing but
		// only holding movement keys or moving the mouse
		bool has_input = false;
		
		// Check for any button currently pressed
		if (ent->client->buttons)
			has_input = true;
		
		// Check for movement input (forward/backward or strafe left/right)
		if (ent->client->cmd.forwardmove != 0 || ent->client->cmd.sidemove != 0)
			has_input = true;
		
		// Check for newly pressed buttons (existing check)
		if (ent->client->latched_buttons)
			has_input = true;
		
		// Check for view angle changes (mouse movement)
		// Compare current command angles with previous frame's angles
		// Use a threshold of 1.0 degree to avoid false positives from tiny movements
		if (!has_input) {
			const float angle_threshold = 1.0f;
			const gvec3_t &current_angles = ent->client->cmd.angles;
			const vec3_t &prev_angles = ent->client->resp.cmd_angles;
			
			// Calculate angular difference for pitch and yaw
			// Handle angle wrapping by normalizing to -180 to 180 range
			auto angle_diff = [](float a, float b) -> float {
				float diff = a - b;
				// Normalize to -180 to 180 range
				while (diff > 180.0f)
					diff -= 360.0f;
				while (diff < -180.0f)
					diff += 360.0f;
				return fabsf(diff);
			};
			
			// Check pitch (index 0) and yaw (index 1) for significant changes
			if (angle_diff(current_angles[0], prev_angles[0]) > angle_threshold ||
				angle_diff(current_angles[1], prev_angles[1]) > angle_threshold) {
				has_input = true;
			}
		}
		
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

	if (RS(RS_Q3A)) {
		// count down health when over max
		if (ent->health > ent->client->pers.max_health)
			ent->health--;

		// count down armor when over max
		if (ent->client->pers.inventory[IT_ARMOR_COMBAT] > ent->client->pers.max_health)
			ent->client->pers.inventory[IT_ARMOR_COMBAT]--;
	}

	MM_ClampEntityHealthArmor(ent);

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

	// [Paril-KEX] pass buttons through even if we are in intermission or
	// chasing.
	client->oldbuttons = client->buttons;
	client->buttons = ucmd->buttons;
	client->latched_buttons |= client->buttons & ~client->oldbuttons;
	client->cmd = *ucmd;

	if (!client->initial_menu_shown && client->initial_menu_delay && level.time > client->initial_menu_delay) {
		if (!ClientIsPlaying(client) && (!client->sess.initialised || client->sess.inactive)) {
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
			bool queued_allowed = true;
			if (client->eliminated && Teams() && client->follow_queued_target->client)
				queued_allowed = (client->follow_queued_target->client->sess.team == client->sess.team);

			if (queued_allowed) {
				client->follow_target = client->follow_queued_target;
				client->follow_update = true;
				UpdateChaseCam(ent);
			}
			client->follow_queued_target = nullptr;
			client->follow_queued_time = 0_sec;
		}
	}
	
	// check for inactivity timer
	if (!ClientInactivityTimer(ent))
		return;

	if (g_quadhog->integer)
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
					gi.LocCenter_Print(ent, "{}", game.motd.c_str());
					delay += 5_sec;
					ent->client->resp.motd_mod_count = game.motd_mod_count;
				}
			}
		}
		if (!ent->client->resp.showed_help && g_showhelp->integer) {
			if (level.time >= ent->client->sess.team_join_time + delay) {
				if (g_quadhog->integer) {
					gi.LocClient_Print(ent, PRINT_CENTER, "QUAD HOG\nFind the Quad Damage to become the Quad Hog!\nScore by fragging the Quad Hog or fragging while Quad Hog.");
				} else if (g_vampiric_damage->integer) {
					gi.LocClient_Print(ent, PRINT_CENTER, "VAMPIRIC DAMAGE\nSurvive by inflicting damage on your foes,\ntheir pain makes you stronger!");
				} else if (g_frenzy->integer) {
					gi.LocClient_Print(ent, PRINT_CENTER, "WEAPONS FRENZY\nWeapons fire faster, rockets move faster, ammo regenerates.");
				} else if (g_nadefest->integer || GT(GT_NADEFEST)) {
					gi.LocClient_Print(ent, PRINT_CENTER, "NADE FEST\nOnly grenades, nothing else!");
				} else if (g_instagib->integer || GT(GT_INSTAGIB)) {
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

			// can exit intermission after five seconds
			// Paril: except in N64. the camera handles it.
			// Paril again: except on unit exits, we can leave immediately after camera finishes
			if (level.changemap && (!n64_sp || level.level_intermission_set) && level.time > level.intermission_time + 5_sec && (ucmd->buttons & BUTTON_ANY))
				level.intermission_exit = true;
		}

		if (!n64_sp)
			client->ps.pmove.viewheight = ent->viewheight = 22;
		else
			client->ps.pmove.viewheight = ent->viewheight = 0;
		ent->movetype = MOVETYPE_FREECAM;
		return;
	}

	if (ent->client->follow_target) {
		client->resp.cmd_angles = ucmd->angles;
		ent->movetype = MOVETYPE_FREECAM;
	} else {

		// set up for pmove
		memset(&pm, 0, sizeof(pm));

		if (ent->movetype == MOVETYPE_FREECAM) {
			if (ent->client->menu) {
				client->ps.pmove.pm_type = PM_FREEZE;

				// [Paril-KEX] handle menu movement
				HandleMenuMovement(ent, ucmd);
			} else if (ent->client->awaiting_respawn)
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
		else if (ent->client->grapple_state >= GRAPPLE_STATE_PULL)
			client->ps.pmove.pm_type = PM_GRAPPLE;
		else
			client->ps.pmove.pm_type = PM_NORMAL;

		// [Paril-KEX]
		if (!G_ShouldPlayersCollide(false) ||
			(InCoopStyle() && !(ent->clipmask & CONTENTS_PLAYER)) // if player collision is on and we're temporarily ghostly...
			)
			client->ps.pmove.pm_flags |= PMF_IGNORE_PLAYER_COLLISION;
		else
			client->ps.pmove.pm_flags &= ~PMF_IGNORE_PLAYER_COLLISION;

		// haste support
		client->ps.pmove.haste = client->pu_time_haste > level.time;

		// trigger_gravity support
		client->ps.pmove.gravity = (short)(level.gravity * ent->gravity);
		pm.s = client->ps.pmove;

		pm.s.origin = ent->s.origin;
		pm.s.velocity = ent->velocity;

		if (memcmp(&client->old_pmove, &pm.s, sizeof(pm.s)))
			pm.snapinitial = true;

		pm.cmd = *ucmd;
		pm.player = ent;
		pm.trace = gi.game_import_t::trace;
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

		if (client->grapple_ent)
			Weapon_Grapple_Pull(client->grapple_ent);

		gi.linkentity(ent);

		ent->gravity = 1.0;

		if (ent->movetype != MOVETYPE_NOCLIP) {
			G_TouchTriggers(ent);
			if (ent->movetype != MOVETYPE_FREECAM)
				G_TouchProjectiles(ent, old_origin);
		}

		// touch other objects
		for (i = 0; i < pm.touch.num; i++) {
			trace_t &tr = pm.touch.traces[i];
			other = tr.ent;

			if (other->touch)
				other->touch(other, ent, tr, true);
		}
	}

	// fire weapon from final position if needed
	if (client->latched_buttons & BUTTON_ATTACK) {
		if (!ClientIsPlaying(client) || (client->eliminated && !client->sess.is_a_bot)) {
			client->latched_buttons = BUTTON_NONE;

			if (client->follow_target) {
				FreeFollower(ent);
			} else
				GetFollowTarget(ent);
		} else if (!ent->client->weapon_thunk) {
			// we can only do this during a ready state and
			// if enough time has passed from last fire
			if (ent->client->weaponstate == WEAPON_READY && !IsCombatDisabled()) {
				ent->client->weapon_fire_buffered = true;

				if (ent->client->weapon_fire_finished <= level.time) {
					ent->client->weapon_thunk = true;
					Think_Weapon(ent);
				}
			}
		}
	}

	if (!ClientIsPlaying(client) || (client->eliminated && !client->sess.is_a_bot)) {
		if (!HandleMenuMovement(ent, ucmd)) {
			if (ucmd->buttons & BUTTON_JUMP) {
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

	// update chase cam if being followed
	for (auto ec : active_clients())
		if (ec->client->follow_target == ent)
			UpdateChaseCam(ec);

	// perform once-a-second actions
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

enum respawn_state_t {
	RESPAWN_NONE,     // invalid state
	RESPAWN_SPECTATE, // move to spectator
	RESPAWN_SQUAD,    // move to good squad point
	RESPAWN_START     // move to start of map
};

// [Paril-KEX] return false to fall back to click-to-respawn behavior.
// note that this is only called if they are allowed to respawn (not
// restarting the level due to all being dead)
static bool G_CoopRespawn(gentity_t *ent) {
	// don't do this in non-coop
	if (!InCoopStyle())
		return false;
	// if we don't have squad or lives, it doesn't matter
	if (!g_coop_squad_respawn->integer && !g_coop_enable_lives->integer)
		return false;

	respawn_state_t state = RESPAWN_NONE;

	// first pass: if we have no lives left, just move to spectator
	if (g_coop_enable_lives->integer) {
		if (ent->client->pers.lives == 0) {
			state = RESPAWN_SPECTATE;
			ent->client->coop_respawn_state = COOP_RESPAWN_NO_LIVES;
		}
	}

	// second pass: check for where to spawn
	if (state == RESPAWN_NONE) {
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
				state = RESPAWN_START;
			else {
				auto [good_player, good_spot] = G_FindSquadRespawnTarget();

				if (good_player) {
					state = RESPAWN_SQUAD;

					squad_respawn_position = good_spot;
					squad_respawn_angles = good_player->s.angles;
					squad_respawn_angles[2] = 0;

					use_squad_respawn = true;
				} else {
					state = RESPAWN_SPECTATE;
				}
			}
		} else
			state = RESPAWN_START;
	}

	if (state == RESPAWN_SQUAD || state == RESPAWN_START) {
		// give us our max health back since it will reset
		// to pers.health; in instanced items we'd lose the items
		// we touched so we always want to respawn with our max.
		if (P_UseCoopInstancedItems())
			ent->client->pers.health = ent->client->pers.max_health = ent->max_health;

		ClientRespawn(ent);

		ent->client->latched_buttons = BUTTON_NONE;
		use_squad_respawn = false;
	} else if (state == RESPAWN_SPECTATE) {
		if (!ent->client->coop_respawn_state)
			ent->client->coop_respawn_state = COOP_RESPAWN_WAITING;

		if (ClientIsPlaying(ent->client)) {
			// move us to spectate just so we don't have to twiddle
			// our thumbs forever
			CopyToBodyQue(ent);
			ent->client->sess.team = TEAM_SPECTATOR;
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
void ClientBeginServerFrame(gentity_t *ent) {
	gclient_t *client;
	int		   buttonMask;

	if (gi.ServerFrame() != ent->client->step_frame)
		ent->s.renderfx &= ~RF_STAIR_STEP;

	if (level.intermission_time)
		return;

	client = ent->client;

	if (client->awaiting_respawn) {
		if ((level.time.milliseconds() % 500) == 0)
			ClientSpawn(ent);
		return;
	}

	if ((ent->svflags & SVF_BOT) != 0) {
		Bot_BeginFrame(ent);
	}

	// run weapon animations if it hasn't been done by a ucmd_t
	if (!client->weapon_thunk && ClientIsPlaying(client) && !client->eliminated)
		Think_Weapon(ent);
	else
		client->weapon_thunk = false;

	if (ent->deadflag) {
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
