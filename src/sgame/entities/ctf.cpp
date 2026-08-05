// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include "g_local.h"
// [MuffMode] Round lifecycle lives in muffmode/mm_match
#include "muffmode/mm_match.h"
#include "muffmode/mm_match_stats.h"
#include "muffmode/mm_strike.h"

constexpr int32_t CTF_CAPTURE_BONUS = 15;	  // what you get for capture
constexpr int32_t CTF_TEAM_BONUS = 10;   // what your team gets for capture
constexpr int32_t CTF_RECOVERY_BONUS = 1;	  // what you get for recovery
constexpr int32_t CTF_FLAG_BONUS = 0;   // what you get for picking up enemy flag
constexpr int32_t CTF_FRAG_CARRIER_BONUS = 2; // what you get for fragging enemy flag carrier
constexpr gtime_t CTF_FLAG_RETURN_TIME = 40_sec;  // seconds until auto return

constexpr int32_t CTF_CARRIER_DANGER_PROTECT_BONUS = 2; // bonus for fraggin someone who has recently hurt your flag carrier
constexpr int32_t CTF_CARRIER_PROTECT_BONUS = 1; // bonus for fraggin someone while either you or your target are near your flag carrier
constexpr int32_t CTF_FLAG_DEFENSE_BONUS = 1; 	// bonus for fraggin someone while either you or your target are near your flag
constexpr int32_t CTF_RETURN_FLAG_ASSIST_BONUS = 1; // awarded for returning a flag that causes a capture to happen almost immediately
constexpr int32_t CTF_FRAG_CARRIER_ASSIST_BONUS = 2;	// award for fragging a flag carrier if a capture happens almost immediately

constexpr float CTF_TARGET_PROTECT_RADIUS = 400;   // the radius around an object being defended where a target will be worth extra frags
constexpr float CTF_ATTACKER_PROTECT_RADIUS = 400; // the radius around an object being defended where an attacker will get extra frags when making kills

constexpr gtime_t CTF_CARRIER_DANGER_PROTECT_TIMEOUT = 8_sec;
constexpr gtime_t CTF_FRAG_CARRIER_ASSIST_TIMEOUT = 10_sec;
constexpr gtime_t CTF_RETURN_FLAG_ASSIST_TIMEOUT = 10_sec;

constexpr gtime_t CTF_AUTO_FLAG_RETURN_TIMEOUT = 30_sec; // number of seconds before dropped flag auto-returns

static uint64_t CTF_FlagCarrierTimeMsec(const gclient_t *client) {
	if (!client)
		return 0;

	const gtime_t start_time = client->resp.ctf_flagsince
		? client->resp.ctf_flagsince
		: client->pers.team_state.flag_pickup_time;
	if (!start_time || level.time <= start_time)
		return 0;

	return static_cast<uint64_t>(
		(level.time - start_time).milliseconds());
}

static void CTF_AwardMedal(gclient_t *client, medal_t medal) {
	if (!client)
		return;

	client->pers.medal_time = level.time;
	client->pers.medal_type = medal;
	client->pers.medal_count[medal]++;
	MM_MatchStats_RecordMedal(client, medal);
}

/*
============
CTF_ScoreBonuses

Calculate the bonuses for flag defense, flag carrier defense, etc.
Note that bonuses are not cumaltive.  You get one, they are in importance
order.
============
*/
void CTF_ScoreBonuses(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker) {
	item_id_t	flag_item, enemy_flag_item;
	team_t		otherteam;
	gentity_t	*flag, *carrier = nullptr;
	const char	*c;
	vec3_t		v1, v2;

	if (!(GTF(GTF_CTF)))
		return;

	if (targ->client && attacker->client) {
		if (attacker->client->resp.ghost)
			if (attacker != targ)
				attacker->client->resp.ghost->kills++;
		if (targ->client->resp.ghost)
			targ->client->resp.ghost->deaths++;
	}

	// no bonus for fragging yourself
	if (!targ->client || !attacker->client || targ == attacker)
		return;

	otherteam = Teams_OtherTeam(targ->client->sess.team);
	if (otherteam < 0)
		return; // whoever died isn't on a team

	// same team, if the flag at base, check to he has the enemy flag
	if (targ->client->sess.team == TEAM_RED) {
		flag_item = IT_FLAG_RED;
		enemy_flag_item = IT_FLAG_BLUE;
	} else {
		flag_item = IT_FLAG_BLUE;
		enemy_flag_item = IT_FLAG_RED;
	}

	// did the attacker frag the flag carrier?
	if (targ->client->pers.inventory[enemy_flag_item]) {
		attacker->client->resp.ctf_lastfraggedcarrier = level.time;
		G_AdjustPlayerScore(attacker->client, CTF_FRAG_CARRIER_BONUS, false, 0);
		gi.LocClient_Print(attacker, PRINT_MEDIUM, "$g_bonus_enemy_carrier",
			CTF_FRAG_CARRIER_BONUS);

		// the target had the flag, clear the hurt carrier
		// field on the other team
		for (auto ec : active_clients()) {
			if (ec->inuse && ec->client->sess.team == otherteam)
				ec->client->resp.ctf_lasthurtcarrier = 0_ms;
		}
		return;
	}

	if (targ->client->resp.ctf_lasthurtcarrier &&
		level.time - targ->client->resp.ctf_lasthurtcarrier < CTF_CARRIER_DANGER_PROTECT_TIMEOUT &&
		!attacker->client->pers.inventory[flag_item]) {
		// attacker is on the same team as the flag carrier and
		// fragged a guy who hurt our flag carrier
		G_AdjustPlayerScore(attacker->client, CTF_CARRIER_DANGER_PROTECT_BONUS, false, 0);
		gi.LocBroadcast_Print(PRINT_MEDIUM, "$g_bonus_flag_defense",
			attacker->client->resp.netname,
			Teams_TeamName(attacker->client->sess.team));
		if (attacker->client->resp.ghost)
			attacker->client->resp.ghost->carrierdef++;
		MM_MatchStats_RecordCtfFlagDefence(
			attacker->client, attacker->client->sess.team);
		CTF_AwardMedal(attacker->client, MEDAL_DEFENCE);
		return;
	}

	// flag and flag carrier area defense bonuses

	// we have to find the flag and carrier entities

	// find the flag
	switch (attacker->client->sess.team) {
	case TEAM_RED:
		c = ITEM_CTF_FLAG_RED;
		break;
	case TEAM_BLUE:
		c = ITEM_CTF_FLAG_BLUE;
		break;
	default:
		return;
	}

	flag = nullptr;
	while ((flag = G_FindByString<&gentity_t::classname>(flag, c)) != nullptr) {
		if (!(flag->spawnflags & SPAWNFLAG_ITEM_DROPPED))
			break;
	}

	if (!flag)
		return; // can't find attacker's flag

	// find attacker's team's flag carrier
	for (auto ec : active_clients()) {
		if (ec->client->pers.inventory[flag_item]) {
			carrier = ec;
			break;
		}
	}

	// ok we have the attackers flag and a pointer to the carrier

	// check to see if we are defending the base's flag
	v1 = targ->s.origin - flag->s.origin;
	v2 = attacker->s.origin - flag->s.origin;

	if ((v1.length() < CTF_TARGET_PROTECT_RADIUS ||
		v2.length() < CTF_TARGET_PROTECT_RADIUS ||
		loc_CanSee(flag, targ) || loc_CanSee(flag, attacker)) &&
		attacker->client->sess.team != targ->client->sess.team) {
		// we defended the base flag
		G_AdjustPlayerScore(attacker->client, CTF_FLAG_DEFENSE_BONUS, false, 0);
		if (flag->solid == SOLID_NOT)
			gi.LocBroadcast_Print(PRINT_MEDIUM, "$g_bonus_defend_base",
				attacker->client->resp.netname,
				Teams_TeamName(attacker->client->sess.team));
		else
			gi.LocBroadcast_Print(PRINT_MEDIUM, "$g_bonus_defend_flag",
				attacker->client->resp.netname,
				Teams_TeamName(attacker->client->sess.team));
		if (attacker->client->resp.ghost)
			attacker->client->resp.ghost->basedef++;
		MM_MatchStats_RecordCtfFlagDefence(
			attacker->client, attacker->client->sess.team);
		CTF_AwardMedal(attacker->client, MEDAL_DEFENCE);
		return;
	}

	if (carrier && carrier != attacker) {
		v1 = targ->s.origin - carrier->s.origin;
		v2 = attacker->s.origin - carrier->s.origin;

		if (v1.length() < CTF_ATTACKER_PROTECT_RADIUS ||
			v2.length() < CTF_ATTACKER_PROTECT_RADIUS ||
			loc_CanSee(carrier, targ) || loc_CanSee(carrier, attacker)) {
			G_AdjustPlayerScore(attacker->client, CTF_CARRIER_PROTECT_BONUS, false, 0);
			gi.LocBroadcast_Print(PRINT_MEDIUM, "$g_bonus_defend_carrier",
				attacker->client->resp.netname,
				Teams_TeamName(attacker->client->sess.team));
			if (attacker->client->resp.ghost)
				attacker->client->resp.ghost->carrierdef++;
			MM_MatchStats_RecordCtfFlagDefence(
				attacker->client, attacker->client->sess.team);
			return;
		}
	}
}

/*
============
CTF_CheckHurtCarrier
============
*/
void CTF_CheckHurtCarrier(gentity_t *targ, gentity_t *attacker) {
	if (!(GTF(GTF_CTF)))
		return;

	if (!targ->client || !attacker->client)
		return;

	item_id_t flag_item = targ->client->sess.team == TEAM_RED ? IT_FLAG_BLUE : IT_FLAG_RED;

	if (targ->client->pers.inventory[flag_item] &&
		targ->client->sess.team != attacker->client->sess.team)
		attacker->client->resp.ctf_lasthurtcarrier = level.time;
}

/*
============
CTF_ResetTeamFlag
============
*/
void CTF_ResetTeamFlag(team_t team) {
	// Always attempt to clean up flags, even when transitioning AWAY from CTF
	// This prevents crashes when changing from CTF to other gametypes
	
	gentity_t *ent;
	const char *c = team == TEAM_RED ? ITEM_CTF_FLAG_RED : ITEM_CTF_FLAG_BLUE;

	ent = nullptr;
	while ((ent = G_FindByString<&gentity_t::classname>(ent, c)) != nullptr) {
		if (ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED)) {
			G_FreeEntity(ent);
		} else {
			// Only restore flags if we're in a CTF gametype
			if (GTF(GTF_CTF)) {
				ent->svflags &= ~SVF_NOCLIENT;
				ent->solid = SOLID_TRIGGER;
				gi.linkentity(ent);
				ent->s.event = EV_ITEM_RESPAWN;
			} else {
				// Transitioning away from CTF, free the flag entity
				G_FreeEntity(ent);
			}
		}
	}
}

/*
============
CTF_ResetFlags
============
*/
void CTF_ResetFlags() {
	// Always attempt to clean up flags, even when transitioning AWAY from CTF
	// CTF_ResetTeamFlag() will handle both restoration (if in CTF) and cleanup (if not)
	
	CTF_ResetTeamFlag(TEAM_RED);
	CTF_ResetTeamFlag(TEAM_BLUE);
}

/*
============
CTF_PickupFlag
============
*/
bool CTF_PickupFlag(gentity_t *ent, gentity_t *other) {
	if (!(GTF(GTF_CTF)))
		return false;

	// Capture Strike: no flag interaction once the turn is over (e.g. after a defender wipe).
	if (GT(GT_STRIKE) && level.round_state != round_state_t::ROUND_IN_PROGRESS)
		return false;

	team_t		team;
	item_id_t	flag_item, enemy_flag_item;

	// figure out what team this flag is
	if (ent->item->id == IT_FLAG_RED)
		team = TEAM_RED;
	else if (ent->item->id == IT_FLAG_BLUE)
		team = TEAM_BLUE;
	else {
		gi.LocClient_Print(other, PRINT_HIGH, "Don't know what team the flag is on, removing.\n");
		G_FreeEntity(ent);
		return false;
	}

	// same team, if the flag at base, check to he has the enemy flag
	if (team == TEAM_RED) {
		flag_item = IT_FLAG_RED;
		enemy_flag_item = IT_FLAG_BLUE;
	} else {
		flag_item = IT_FLAG_BLUE;
		enemy_flag_item = IT_FLAG_RED;
	}

	if (team == other->client->sess.team) {

		if (!(ent->spawnflags & SPAWNFLAG_ITEM_DROPPED)) {
			// the flag is at home base.  if the player has the enemy
			// flag, he's just scored a capture!

			if (other->client->pers.inventory[enemy_flag_item]) {
				const uint64_t carrier_time_msec =
					CTF_FlagCarrierTimeMsec(other->client);
				if (other->client->pers.team_state.flag_pickup_time) {
					gi.LocBroadcast_Print(PRINT_HIGH, "{} TEAM CAPTURED the flag! ({} captured in {})\n",
						Teams_TeamName(team), other->client->resp.netname, G_TimeStringMs((level.time - other->client->pers.team_state.flag_pickup_time).milliseconds(), false));
				} else {
					gi.LocBroadcast_Print(PRINT_HIGH, "{} TEAM CAPTURED the flag! (captured by {})\n",
						Teams_TeamName(team), other->client->resp.netname);
				}
				other->client->pers.inventory[enemy_flag_item] = 0;

				level.ctf_last_flag_capture = level.time;
				level.ctf_last_capture_team = team;
				if (!GT(GT_STRIKE))
					G_AdjustTeamScore(team, 1);
				MM_MatchStats_RecordCtfFlagCapture(other->client, team,
					Teams_OtherTeam(team), carrier_time_msec);
				CTF_AwardMedal(other->client, MEDAL_CAPTURE);

				gi.sound(ent, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex("ctf/flagcap.wav"), 1, ATTN_NONE, 0);

				if (!GT(GT_STRIKE)) {
					// other gets capture bonus
					G_AdjustPlayerScore(other->client, CTF_CAPTURE_BONUS, false, 0);
					if (other->client->resp.ghost)
						other->client->resp.ghost->caps++;

					// Ok, let's do the player loop, hand out the bonuses
					for (auto ec : active_clients()) {
						if (ec->client->sess.team != other->client->sess.team)
							ec->client->resp.ctf_lasthurtcarrier = -5_sec;
						else if (ec->client->sess.team == other->client->sess.team) {
							if (ec != other)
								G_AdjustPlayerScore(ec->client, CTF_TEAM_BONUS, false, 0);
							// award extra points for capture assists
							if (ec->client->resp.ctf_lastreturnedflag && ec->client->resp.ctf_lastreturnedflag + CTF_RETURN_FLAG_ASSIST_TIMEOUT > level.time) {
								gi.LocBroadcast_Print(PRINT_HIGH, "$g_bonus_assist_return", ec->client->resp.netname);
								G_AdjustPlayerScore(ec->client, CTF_RETURN_FLAG_ASSIST_BONUS, false, 0);
								MM_MatchStats_RecordCtfFlagAssist(ec->client, team);
								CTF_AwardMedal(ec->client, MEDAL_ASSIST);
							}
							if (ec->client->resp.ctf_lastfraggedcarrier && ec->client->resp.ctf_lastfraggedcarrier + CTF_FRAG_CARRIER_ASSIST_TIMEOUT > level.time) {
								gi.LocBroadcast_Print(PRINT_HIGH, "$g_bonus_assist_frag_carrier", ec->client->resp.netname);
								G_AdjustPlayerScore(ec->client, CTF_FRAG_CARRIER_ASSIST_BONUS, false, 0);
								MM_MatchStats_RecordCtfFlagAssist(ec->client, team);
								CTF_AwardMedal(ec->client, MEDAL_ASSIST);
							}
						}
					}
				}

				CTF_ResetFlags();

				if (GT(GT_STRIKE))
					MM_Strike_AwardTurnWin(true);

				return false;
			}
			return false; // its at home base already
		}
		// hey, its not home.  return it by teleporting it back
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_returned_flag",
			other->client->resp.netname, Teams_TeamName(team));
		G_AdjustPlayerScore(other->client, CTF_RECOVERY_BONUS, false, 0);
		other->client->resp.ctf_lastreturnedflag = level.time;
		MM_MatchStats_RecordCtfFlagReturn(other->client, team);
		gi.sound(ent, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex("ctf/flagret.wav"), 1, ATTN_NONE, 0);
		// CTF_ResetTeamFlag will remove this entity!  We must return false
		CTF_ResetTeamFlag((team_t)team);
		return false;
	}

	// capturestrike: can't pick up enemy flag if defending
	if (GT(GT_STRIKE)) {
		if ((level.strike_red_attacks && other->client->sess.team != TEAM_RED) ||
				(!level.strike_red_attacks && other->client->sess.team != TEAM_BLUE)) {
			//gi.LocClient_Print(other, PRINT_CENTER, "Your team is defending!\n");
			return false;
		}
	}

	// hey, its not our flag, pick it up
	if (!(ent->spawnflags & SPAWNFLAG_ITEM_DROPPED)) {
		other->client->pers.team_state.flag_pickup_time = level.time;
	}
	gi.LocBroadcast_Print(PRINT_HIGH, "$g_got_flag",
		other->client->resp.netname, Teams_TeamName(team));
	G_AdjustPlayerScore(other->client, CTF_FLAG_BONUS, false, 0);
	if (GT(GT_STRIKE))
		MM_Strike_AwardTouchBonus();

	other->client->pers.inventory[flag_item] = 1;
	other->client->resp.ctf_flagsince = level.time;
	MM_MatchStats_RecordCtfFlagPickup(other->client, team);


	// pick up the flag
	// if it's not a dropped flag, we just make is disappear
	// if it's dropped, it will be removed by the pickup caller
	if (!(ent->spawnflags & SPAWNFLAG_ITEM_DROPPED)) {
		ent->flags |= FL_RESPAWN;
		ent->svflags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;
	}
	return true;
}

/*
============
CTF_DropFlagTouch
============
*/
static TOUCH(CTF_DropFlagTouch) (gentity_t *ent, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	if (!(GTF(GTF_CTF)))
		return;

	// owner (who dropped us) can't touch for two secs
	if (other == ent->owner &&
		ent->nextthink - level.time > CTF_AUTO_FLAG_RETURN_TIMEOUT - 2_sec)
		return;

	Touch_Item(ent, other, tr, other_touching_self);
}

/*
============
CTF_DropFlagThink
============
*/
static THINK(CTF_DropFlagThink) (gentity_t *ent) -> void {
	if (!(GTF(GTF_CTF)))
		return;

	// auto return the flag
	// reset flag will remove ourselves
	if (ent->item->id == IT_FLAG_RED) {
		CTF_ResetTeamFlag(TEAM_RED);
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_flag_returned",
			Teams_TeamName(TEAM_RED));
	} else if (ent->item->id == IT_FLAG_BLUE) {
		CTF_ResetTeamFlag(TEAM_BLUE);
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_flag_returned",
			Teams_TeamName(TEAM_BLUE));
	}

	gi.sound(ent, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex("ctf/flagret.wav"), 1, ATTN_NONE, 0);
}

/*
============
CTF_DeadDropFlag

Called from PlayerDie, to drop the flag from a dying player
============
*/
void CTF_DeadDropFlag(gentity_t *self) {
	if (!(GTF(GTF_CTF)))
		return;

	gentity_t *dropped = nullptr;
	team_t dropped_flag_team = TEAM_NONE;

	if (self->client->pers.inventory[IT_FLAG_RED]) {
		dropped = Drop_Item(self, GetItemByIndex(IT_FLAG_RED));
		dropped_flag_team = TEAM_RED;
		self->client->pers.inventory[IT_FLAG_RED] = 0;
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_lost_flag",
			self->client->resp.netname, Teams_TeamName(TEAM_RED));
	} else if (self->client->pers.inventory[IT_FLAG_BLUE]) {
		dropped = Drop_Item(self, GetItemByIndex(IT_FLAG_BLUE));
		dropped_flag_team = TEAM_BLUE;
		self->client->pers.inventory[IT_FLAG_BLUE] = 0;
		gi.LocBroadcast_Print(PRINT_HIGH, "$g_lost_flag",
			self->client->resp.netname, Teams_TeamName(TEAM_BLUE));
	}
	if (dropped_flag_team != TEAM_NONE)
		MM_MatchStats_RecordCtfFlagDrop(self->client, dropped_flag_team,
			CTF_FlagCarrierTimeMsec(self->client));

	self->client->pers.team_state.flag_pickup_time = 0_ms;
	self->client->resp.ctf_flagsince = 0_ms;

	if (dropped) {
		dropped->think = CTF_DropFlagThink;
		dropped->nextthink = level.time + CTF_AUTO_FLAG_RETURN_TIMEOUT;
		dropped->touch = CTF_DropFlagTouch;
	}
}

/*
============
CTF_DropFlag
============
*/
void CTF_DropFlag(gentity_t *ent, gitem_t *item) {
	if (!(GTF(GTF_CTF)))
		return;

	ent->client->pers.team_state.flag_pickup_time = 0_ms;

	if (brandom())
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_lusers_drop_flags");
	else
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_winners_drop_flags");
}

/*
============
CTF_FlagThink
============
*/
static THINK(CTF_FlagThink) (gentity_t *ent) -> void {
	if (!(GTF(GTF_CTF)))
		return;

	if (ent->solid != SOLID_NOT)
		ent->s.frame = 173 + (((ent->s.frame - 173) + 1) % 16);
	ent->nextthink = level.time + 10_hz;
}

/*
============
CTF_FlagSetup
============
*/
THINK(CTF_FlagSetup) (gentity_t *ent) -> void {
	if (!(GTF(GTF_CTF)))
		return;

	trace_t tr;
	vec3_t	dest;

	ent->mins = { -15, -15, -15 };
	ent->maxs = { 15, 15, 15 };

	if (ent->model)
		gi.setmodel(ent, ent->model);
	else
		gi.setmodel(ent, ent->item->world_model);
	ent->solid = SOLID_TRIGGER;
	ent->movetype = MOVETYPE_TOSS;
	ent->touch = Touch_Item;
	ent->s.frame = 173;

	dest = ent->s.origin + vec3_t{ 0, 0, -128 };

	tr = gi.trace(ent->s.origin, ent->mins, ent->maxs, dest, ent, MASK_SOLID);
	if (tr.startsolid) {
		gi.Com_PrintFmt("{}: {} startsolid\n", __FUNCTION__, *ent);
		G_FreeEntity(ent);
		return;
	}

	ent->s.origin = tr.endpos;

	gi.linkentity(ent);

	ent->nextthink = level.time + 10_hz;
	ent->think = CTF_FlagThink;
}

/*
============
CTF_ClientEffects
============
*/
void CTF_ClientEffects(gentity_t *player) {
	if (!(GTF(GTF_CTF)))
		return;

	player->s.effects &= ~(EF_FLAG_RED | EF_FLAG_BLUE);
	if (player->health > 0) {
		if (player->client->pers.inventory[IT_FLAG_RED])
			player->s.effects |= EF_FLAG_RED;
		if (player->client->pers.inventory[IT_FLAG_BLUE])
			player->s.effects |= EF_FLAG_BLUE;
	}

	if (player->client->pers.inventory[IT_FLAG_RED])
		player->s.modelindex3 = mi_ctf_red_flag;
	else if (player->client->pers.inventory[IT_FLAG_BLUE])
		player->s.modelindex3 = mi_ctf_blue_flag;
	else
		player->s.modelindex3 = 0;
}
