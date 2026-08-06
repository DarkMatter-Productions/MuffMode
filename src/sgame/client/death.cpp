// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Client death, obituary and item-drop handling.
#include "g_local.h"
#include "monsters/m_player.h"
#include "muffmode/mm_announcer.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_captain.h"
#include "muffmode/mm_freezetag.h"
#include "muffmode/mm_freezetag_rules.h"
#include "muffmode/mm_horde.h"
#include "muffmode/mm_lms.h"
#include "muffmode/mm_match_stats.h"
#include "muffmode/mm_spawn_loadout.h"

static bool ShouldShowRampageMessages();

template<typename... Args>
static void ClientObituaryBroadcast(gentity_t *scope, print_type_t level,
	const char *base, Args &&...args)
{
	if (!GT(GT_ARENA)) {
		gi.LocBroadcast_Print(level, base, std::forward<Args>(args)...);
		return;
	}

	for (auto recipient : active_clients()) {
		if (MM_Arena_ChatRecipient(scope, recipient, mm_arena_chat_scope_t::Arena))
			gi.LocClient_Print(recipient, level, base, args...);
	}
}

struct monster_name_t {
	const char *classname;
	const char *longname;
};

static constexpr monster_name_t MONSTER_NAMES[] = {
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
	for (const monster_name_t &name : MONSTER_NAMES) {
		if (!Q_strncasecmp(classname, name.classname, strlen(classname)))
			return name.longname;
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

static std::string ObituaryColorResetAfter(const char *text) {
	if (!text || !*text)
		return {};

	std::string result(text);
	if (result.size() < 2 || result[result.size() - 2] != '^' || result.back() != '7')
		result.append("^7");
	return result;
}

static const char *PlayerObituaryLogFormat(mod_id_t mod) {
	switch (mod) {
	case MOD_BLASTER:
		return "{} was blasted by {}.\n";
	case MOD_SHOTGUN:
		return "{} was gunned down by {}.\n";
	case MOD_SSHOTGUN:
		return "{} was blown away by {}'s Super Shotgun.\n";
	case MOD_MACHINEGUN:
		return "{} was machinegunned by {}.\n";
	case MOD_CHAINGUN:
		return "{} was cut in half by {}'s Chaingun.\n";
	case MOD_GRENADE:
		return "{} was popped by {}'s grenade.\n";
	case MOD_G_SPLASH:
		return "{} was shredded by {}'s shrapnel.\n";
	case MOD_ROCKET:
		return "{} ate {}'s rocket.\n";
	case MOD_R_SPLASH:
		return "{} almost dodged {}'s rocket.\n";
	case MOD_HYPERBLASTER:
		return "{} was melted by {}'s HyperBlaster.\n";
	case MOD_RAILGUN:
		return "{} was railed by {}.\n";
	case MOD_BFG_LASER:
		return "{} saw the pretty lights from {}'s BFG.\n";
	case MOD_BFG_BLAST:
		return "{} was disintegrated by {}'s BFG blast.\n";
	case MOD_BFG_EFFECT:
		return "{} couldn't hide from {}'s BFG.\n";
	case MOD_HANDGRENADE:
		return "{} caught {}'s handgrenade.\n";
	case MOD_HG_SPLASH:
		return "{} didn't see {}'s handgrenade.\n";
	case MOD_HELD_GRENADE:
		return "{} feels {}'s pain.\n";
	case MOD_TELEFRAG:
	case MOD_TELEFRAG_SPAWN:
		return "{} tried to invade {}'s personal space.\n";
	case MOD_RIPPER:
		return "{} ripped to shreds by {}'s ripper gun.\n";
	case MOD_PHALANX:
		return "{} was evaporated by {}.\n";
	case MOD_TRAP:
		return "{} was caught in {}'s trap.\n";
	case MOD_CHAINFIST:
		return "{} was shredded by {}'s ripsaw.\n";
	case MOD_DISINTEGRATOR:
		return "{} lost his grip courtesy of {}'s Disintegrator.\n";
	case MOD_ETF_RIFLE:
		return "{} was perforated by {}.\n";
	case MOD_PLASMABEAM:
		return "{} was scorched by {}'s Plasma Beam.\n";
	case MOD_TESLA:
		return "{} was enlightened by {}'s tesla mine.\n";
	case MOD_PROX:
		return "{} got too close to {}'s proximity mine.\n";
	case MOD_NUKE:
		return "{} was nuked by {}'s antimatter bomb.\n";
	case MOD_VENGEANCE_SPHERE:
		return "{} was purged by {}'s Vengeance Sphere.\n";
	case MOD_DEFENDER_SPHERE:
		return "{} had a blast with {}'s Defender Sphere.\n";
	case MOD_HUNTER_SPHERE:
		return "{} was hunted down by {}'s Hunter Sphere.\n";
	case MOD_TRACKER:
		return "{} was annihilated by {}'s Disruptor.\n";
	case MOD_DOPPEL_EXPLODE:
		return "{} was tricked by {}'s Doppelganger.\n";
	case MOD_DOPPEL_VENGEANCE:
		return "{} was purged by {}'s Doppelganger.\n";
	case MOD_DOPPEL_HUNTER:
		return "{} was hunted down by {}'s Doppelganger.\n";
	case MOD_GRAPPLE:
		return "{} was caught by {}'s grapple.\n";
	default:
		return "{} was killed by {}.\n";
	}
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
		ClientObituaryBroadcast(self, PRINT_MEDIUM, base, self->client->resp.netname);
		MM_MatchStats_LogEvent(fmt::format("{} died by {}{}.",
			ObituaryColorResetAfter(self->client->resp.netname),
			MM_MatchStats_ModName(static_cast<uint8_t>(mod.id)),
			attacker == self ? " (self)" : ""));
		self->enemy = nullptr;
		return;
	}

	// has a killer
	self->enemy = attacker;

	if (!attacker)
		return;

	if (attacker->svflags & SVF_MONSTER) {
		const char *monname = MonsterName(attacker->classname);

		if (monname) {
			ClientObituaryBroadcast(self, PRINT_MEDIUM, "{} was killed by a{} {}\n",
				self->client->resp.netname, IsVowel(monname[0]) ? "n" : "", monname);
			MM_MatchStats_LogEvent(fmt::format("{} was killed by a {}.\n",
				ObituaryColorResetAfter(self->client->resp.netname), monname));
		}
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

	ClientObituaryBroadcast(self, PRINT_MEDIUM, base,
		self->client->resp.netname, attacker->client->resp.netname);
	MM_MatchStats_LogEvent(fmt::format(PlayerObituaryLogFormat(mod.id),
		self->client->resp.netname, attacker->client->resp.netname));

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
			if (level.match_state == match_state_t::MATCH_WARMUP_READYUP) {
				BroadcastReadyReminderMessage();
			} else {
				if (GT(GT_HORDE) && level.round_state == round_state_t::ROUND_IN_PROGRESS && ClientIsPlaying(self->client)) {
					const int remaining = max(0, self->client->pers.lives - 1);
					if (remaining > 0) {
						gi.LocClient_Print(self, PRINT_CENTER, "You were killed by {}\n{} {} remaining.",
							attacker->client->resp.netname, remaining, remaining == 1 ? "life" : "lives");
					} else {
						gi.LocClient_Print(self, PRINT_CENTER, "You were killed by {}",
							attacker->client->resp.netname);
					}
				} else if (GTF(GTF_ROUNDS) && GTF(GTF_ELIMINATION) && level.round_state == round_state_t::ROUND_IN_PROGRESS) {
					gi.LocClient_Print(self, PRINT_CENTER, "You were fragged by {}\nYou will respawn next round.", attacker->client->resp.netname);
				} else {
					gi.LocClient_Print(self, PRINT_CENTER, "You were fragged by {}", attacker->client->resp.netname);
				}
			}
		}
		if (!(attacker->svflags & SVF_BOT)) {
			if ((Teams() || GT(GT_ARENA)) && OnSameTeam(self, attacker)) {
				gi.LocClient_Print(attacker, PRINT_CENTER, "You fragged {}, your team mate :(", self->client->resp.netname);
			} else {
				if (level.match_state == match_state_t::MATCH_WARMUP_READYUP) {
					BroadcastReadyReminderMessage();
				} else if (attacker->client->resp.kill_count && !(attacker->client->resp.kill_count % 10)) {
					if (ShouldShowRampageMessages()) {
						ClientObituaryBroadcast(self, PRINT_CENTER, "{} is on a rampage\nwith {} frags!",
							attacker->client->resp.netname, attacker->client->resp.kill_count);
						MM_Announce(mm_announce_event_t::Rampage1, attacker);
						attacker->client->pers.medal_time = level.time;
						attacker->client->pers.medal_type = MEDAL_RAMPAGE;
					}
					// Message and announcer preferences are presentation-only. The
					// earned award must remain present in native and exported stats.
					attacker->client->pers.medal_count[MEDAL_RAMPAGE]++;
					MM_MatchStats_RecordMedal(attacker->client, MEDAL_RAMPAGE);
				} else if (kill_count >= 10) {
					if (ShouldShowRampageMessages()) {
						ClientObituaryBroadcast(self, PRINT_CENTER, "{} put an end to {}'s\nrampage!",
							attacker->client->resp.netname, self->client->resp.netname);
					}
				} else if (Teams() || level.match_state != match_state_t::MATCH_IN_PROGRESS) {
					if (attacker->client->sess.pc.show_fragmessages)
						gi.LocClient_Print(attacker, PRINT_CENTER, "You fragged {}", self->client->resp.netname);
				} else {
					if (attacker->client->sess.pc.show_fragmessages)
						gi.LocClient_Print(attacker, PRINT_CENTER, "You fragged {}\n{} place with {}",
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

	ClientObituaryBroadcast(self, PRINT_MEDIUM, "$g_mod_generic_died", self->client->resp.netname);
}

static void DropPlayerTimedPowerup(gentity_t *self, item_id_t item_id, gtime_t expiration_time, void (*expire_think)(gentity_t *), bool quad_hog_visuals = false) {
	self->client->v_angle[YAW] += 45;

	gentity_t *drop = Drop_Item(self, GetItemByIndex(item_id));
	drop->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
	drop->spawnflags &= ~SPAWNFLAG_ITEM_DROPPED;
	drop->svflags &= ~SVF_INSTANCED;

	drop->touch = Touch_Item;
	drop->nextthink = expiration_time;
	drop->think = expire_think;

	if (quad_hog_visuals) {
		drop->s.renderfx |= RF_SHELL_BLUE;
		drop->s.effects |= EF_COLOR_SHELL;
	}

	drop->count = expiration_time.seconds<int>() - level.time.seconds<int>();
	if (drop->count < 1)
		drop->count = 1;
}

/*
=================
TossClientItems

Toss the weapon, tech, CTF flag and powerups for the killed player
=================
*/
static void TossClientItemsInternal(gentity_t *self, bool drop_weapon) {
	if (!deathmatch->integer)
		return;

	if (GTF(GTF_ARENA))
		return;

	// don't drop anything when combat is disabled
	if (IsCombatDisabled())
		return;

	gitem_t *wp;
	bool	quad, doubled, haste, protection, invis, regen;

	// drop weapon
	wp = drop_weapon ? self->client->pers.weapon : nullptr;
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
			gentity_t *drop = Drop_Item(self, wp);
			drop->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
			drop->spawnflags &= ~SPAWNFLAG_ITEM_DROPPED;
			drop->svflags &= ~SVF_INSTANCED;
		}
	}

	//drop tech
	// [MuffMode] Horde can opt out of dropping techs on death via g_horde_tech_drop_on_death.
	if (!GT(GT_HORDE) || g_horde_tech_drop_on_death->integer)
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
		DropPlayerTimedPowerup(self, IT_POWERUP_QUAD, self->client->pu_time_quad, g_quadhog->integer ? QuadHog_DoReset : G_FreeEntity, g_quadhog->integer);
	}

	if (haste) {
		DropPlayerTimedPowerup(self, IT_POWERUP_HASTE, self->client->pu_time_haste, G_FreeEntity);
	}

	if (protection) {
		DropPlayerTimedPowerup(self, IT_POWERUP_PROTECTION, self->client->pu_time_protection, G_FreeEntity);
	}

	if (regen) {
		DropPlayerTimedPowerup(self, IT_POWERUP_REGEN, self->client->pu_time_regeneration, G_FreeEntity);
	}

	if (invis) {
		DropPlayerTimedPowerup(self, IT_POWERUP_INVISIBILITY, self->client->pu_time_invisibility, G_FreeEntity);
	}

	if (doubled) {
		DropPlayerTimedPowerup(self, IT_POWERUP_DOUBLE, self->client->pu_time_double, G_FreeEntity);
	}

	self->client->v_angle[YAW] = 0.0;
}

void TossClientItems(gentity_t *self) {
	TossClientItemsInternal(self, true);
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
	case match_state_t::MATCH_WARMUP_DELAYED:
	case match_state_t::MATCH_WARMUP_DEFAULT:
	case match_state_t::MATCH_WARMUP_READYUP:
	case match_state_t::MATCH_COUNTDOWN:
	case match_state_t::MATCH_ENDED:
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

bool ClientArenaEliminationCorpse(const gclient_t *client) {
	if (!client)
		return false;
	if (GT(GT_ARENA))
		return MM_Arena_IsEliminated(client);
	return client->eliminated && GTF(GTF_ARENA) && GTF(GTF_ELIMINATION);
}

static void ClientFinalizeArenaEliminationCorpse(gentity_t *self) {
	if (!ClientArenaEliminationCorpse(self->client) || !self->s.modelindex)
		return;

	// Snapshot after death pose is chosen (see early CopyToBodyQue in player_die history).
	if (self->client->anim_priority == ANIM_DEATH && self->client->anim_end > 0)
		self->s.frame = self->client->anim_end;

	if (self->client->anim_end > 0)
		CopyToBodyQue(self);

	// The body queue owns the visible, collidable corpse. Keep the eliminated
	// player entity as a hidden freecam anchor only.
	self->solid = SOLID_NOT;
	self->movetype = MOVETYPE_NONE;
	self->mins = {};
	self->maxs = {};
	self->svflags |= SVF_NOCLIENT;
	self->s.modelindex = 0;
	self->s.modelindex2 = 0;
	self->s.modelindex3 = 0;
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

	if (GT(GT_ARENA)) {
		// [MuffMode] Logical teams and scores are owned by the victim's
		// arena. The singleton frag/team score path below must stay untouched.
	} else if (attacker && attacker->client && level.match_state == match_state_t::MATCH_IN_PROGRESS) {
		if (attacker == self || mod.friendly_fire) {
			// LMS: resp.score is the round-win tally and the match-win quantity, so no
			// kill/suicide frag adjustments may touch it - only round wins move the score.
			if (!mod.no_point_loss && notGT(GT_LMS))
				G_AdjustPlayerScore(attacker->client, -1, GT(GT_TDM), -1);
			attacker->client->resp.kill_count = 0;
		} else {
			MM_Announcer_OnPlayerFrag(attacker, self);
			if (notGT(GT_LMS))
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
				MM_MatchStats_RecordMedal(attacker->client, MEDAL_EXCELLENT);

				if (attacker->client->pers.medal_count[MEDAL_EXCELLENT] == 1)
					MM_Announce(mm_announce_event_t::FirstExcellent, attacker);
				else
					MM_Announce(mm_announce_event_t::Excellent1, attacker);
			}
			attacker->client->pers.kill_time = level.time;

			if (mod.id == MOD_BLASTER || mod.id == MOD_CHAINFIST) {
				attacker->client->pers.medal_time = level.time;
				attacker->client->pers.medal_type = MEDAL_HUMILIATION;
				attacker->client->pers.medal_count[MEDAL_HUMILIATION]++;
				MM_MatchStats_RecordMedal(attacker->client, MEDAL_HUMILIATION);

				MM_Announce(mm_announce_event_t::Humiliation1, attacker);
			}

			for (auto ec : active_clients()) {
				if ((!ClientIsPlaying(ec->client) || ec->client->eliminated) && ec->client->sess.pc.follow_killer) {
					ec->client->follow_queued_target = attacker;
					ec->client->follow_queued_time = level.time;
				}
			}
		}
	} else {
		// LMS: never dock round-win score for an environmental / no-attacker death.
		if (!mod.no_point_loss && notGT(GT_LMS))
			G_AdjustPlayerScore(self->client, -1, GT(GT_TDM), -1);
	}
	MS_Adjust(self->client, MSTAT_DEATHS_TOTAL, 1);

	if (self == attacker)
		MS_Adjust(self->client, MSTAT_DEATHS_SUICIDES, 1);
	else if (!attacker)
		MS_Adjust(self->client, MSTAT_DEATHS_ENVIRO, 1);
	else if (1_sec > (level.time - self->client->respawn_time))
		MS_Adjust(self->client, MSTAT_DEATHS_SPAWN, 1);

	const bool spawn_death = self != attacker &&
		1_sec > (level.time - self->client->respawn_time);
	const bool team_kill = attacker && attacker != self && attacker->client &&
		OnSameTeam(self, attacker);
	MM_MatchStats_RecordDeath(self, attacker, mod, spawn_death, team_kill);

	if (MM_FreezeTag_ShouldFreezeDeath(self, attacker, mod)) {
		LookAtKiller(self, inflictor, attacker);
		ClientObituary(self, inflictor, attacker, mod);
		TossClientItemsInternal(self, MM_FreezeTagDropWeaponOnFreeze(MM_UsesArenaSpawnLoadout()));
		Weapon_Grapple_DoReset(self->client);
		MM_FreezeTag_FreezePlayer(self, attacker, mod);
		return;
	}

	self->svflags |= SVF_DEADMONSTER;

	if (GTF(GTF_ROUNDS) && GTF(GTF_ELIMINATION) &&
			level.match_state == match_state_t::MATCH_IN_PROGRESS &&
			level.round_state == round_state_t::ROUND_IN_PROGRESS &&
			notGT(GT_HORDE) && notGT(GT_LMS) && ClientIsPlaying(self->client) &&
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

		if (GT(GT_HORDE))
			MM_Horde_OnPlayerDeath(self);
		if (GT(GT_LMS))
			MM_LMS_OnPlayerDeath(self);
		if ((GT(GT_HORDE) || GT(GT_LMS)) && self->client->eliminated)
			self->client->respawn_time = level.time + 1_sec;

		CTF_ScoreBonuses(self, inflictor, attacker);
		// Arena loadout modes (Horde/LMS) don't scatter a full kit when the fighter is
		// eliminated; they keep spectating until the next round/wave.
		if (notGT(GT_ARENA) &&
			!((GT(GT_HORDE) || GT(GT_LMS)) && self->client->eliminated))
			TossClientItems(self);
		Weapon_Grapple_DoReset(self->client);

		if (deathmatch->integer && g_dm_death_scoreboard->integer && !self->client->showscores)
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
	if (Teams() || GT(GT_ARENA))
		self->client->pers.inventory.fill(0);

	// if there's a sphere around, let it know the player died.
	// vengeance and hunter will die if they're not attacking,
	// defender should always die
	if (gentity_t *sphere = G_ResolveOwnedSphere(self->client)) {
		if (sphere->die)
			sphere->die(sphere, self, self, 0, vec3_origin, mod);
		else
			G_FreeEntity(sphere);
	}

	// if we've been killed by the tracker, GIB!
	if (mod.id == MOD_TRACKER) {
		self->health = -100;
		damage = 400;
	}

	self->s.effects = EF_NONE;
	self->s.renderfx = RF_NONE;

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
		if (InCoopStyle() && notGT(GT_HORDE) && (g_coop_squad_respawn->integer || g_coop_enable_lives->integer)) {
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

	if (ClientArenaEliminationCorpse(self->client))
		self->takedamage = false;

	ClientFinalizeArenaEliminationCorpse(self);

	// holster view weapon (Think_Weapon skips eliminated players before it can)
	self->client->newweapon = nullptr;
	self->client->pers.weapon = nullptr;
	self->client->ps.gunindex = 0;
	self->client->ps.gunskin = 0;
	self->client->ps.gunframe = 0;

	self->deadflag = true;
	MM_Arena_OnDeath(self, attacker);
	if (GT(GT_ARENA) && MM_Arena_IsEliminated(self->client)) {
		self->takedamage = false;
		ClientFinalizeArenaEliminationCorpse(self);
	}

	gi.linkentity(self);
}

//=======================================================================
