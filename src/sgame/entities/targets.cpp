// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include <limits>

#include "g_local.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_player_name.h"

static bool TargetParseFiniteFloat(const char *token, float &out) {
	const auto value = MM_ParseFloatArg(token);
	if (!value)
		return false;

	out = *value;
	return true;
}

static bool TargetParseUInt32(const char *token, uint32_t &out) {
	const auto value = MM_ParseUInt32Arg(token);
	if (!value)
		return false;

	out = *value;
	return true;
}

static size_t TargetLightStyleIndex(float delay, size_t style_length) {
	if (style_length == 0 || !std::isfinite(delay)) {
		return 0;
	}

	float wrapped = fmod(delay, static_cast<float>(style_length));
	if (wrapped < 0.0f) {
		wrapped += static_cast<float>(style_length);
	}

	return static_cast<size_t>(wrapped);
}

static float TargetLightDelayFraction(float delay) {
	if (!std::isfinite(delay)) {
		return 0.0f;
	}

	float fraction = fmod(delay, 1.0f);
	if (fraction < 0.0f) {
		fraction += 1.0f;
	}

	return fraction;
}

static uint32_t TargetEntityLimit() {
	return min(globals.num_entities, game.maxentities);
}

static uint32_t TargetClientEntityLimit() {
	return min(TargetEntityLimit(), game.maxclients + 1);
}

/*QUAKED target_temp_entity (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Fire an origin based temp entity event to the clients.
"style"		type byte
*/
static USE(Use_Target_Tent) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(ent->style);
	gi.WritePosition(ent->s.origin);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);
}

void SP_target_temp_entity(gentity_t *ent) {
	if (level.is_n64 && ent->style == 27)
		ent->style = TE_TELEPORT_EFFECT;

	ent->use = Use_Target_Tent;
}

//==========================================================
//==========================================================

/*QUAKED target_speaker (1 0 0) (-8 -8 -8) (8 8 8) LOOPED-ON LOOPED-OFF RELIABLE x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
"noise"		wav file to play
"attenuation"
-1 = none, send to whole level
1 = normal fighting sounds
2 = idle sound level
3 = ambient sound level
"volume"	0.0 to 1.0

Normal sounds play each time the target is used.  The reliable flag can be set for crucial voiceovers.

[Paril-KEX] looped sounds are by default atten 3 / vol 1, and the use function toggles it on/off.
*/

constexpr spawnflags_t SPAWNFLAG_SPEAKER_LOOPED_ON = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAG_SPEAKER_LOOPED_OFF = 2_spawnflag;
constexpr spawnflags_t SPAWNFLAG_SPEAKER_RELIABLE = 4_spawnflag;
constexpr spawnflags_t SPAWNFLAG_SPEAKER_NO_STEREO = 8_spawnflag;

static USE(Use_Target_Speaker) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	soundchan_t chan;

	if (ent->spawnflags.has(SPAWNFLAG_SPEAKER_LOOPED_ON | SPAWNFLAG_SPEAKER_LOOPED_OFF)) { // looping sound toggles
		if (ent->s.sound)
			ent->s.sound = 0; // turn it off
		else
			ent->s.sound = ent->noise_index; // start it
	} else { // normal sound
		if (ent->spawnflags.has(SPAWNFLAG_SPEAKER_RELIABLE))
			chan = CHAN_VOICE | CHAN_RELIABLE;
		else
			chan = CHAN_VOICE;
		// use a positioned_sound, because this entity won't normally be
		// sent to any clients because it is invisible
		gi.positioned_sound(ent->s.origin, ent, chan, ent->noise_index, ent->volume, ent->attenuation, 0);
	}
}

void SP_target_speaker(gentity_t *ent) {
	if (!st.noise) {
		gi.Com_PrintFmt("{}: no noise set\n", *ent);
		return;
	}

	if (!strstr(st.noise, ".wav"))
		ent->noise_index = gi.soundindex(G_Fmt("{}.wav", st.noise).data());
	else
		ent->noise_index = gi.soundindex(st.noise);

	if (!ent->volume)
		ent->volume = ent->s.loop_volume = 1.0;

	if (!ent->attenuation) {
		if (ent->spawnflags.has(SPAWNFLAG_SPEAKER_LOOPED_OFF | SPAWNFLAG_SPEAKER_LOOPED_ON))
			ent->attenuation = ATTN_STATIC;
		else
			ent->attenuation = ATTN_NORM;
	} else if (ent->attenuation == -1) // use -1 so 0 defaults to 1
	{
		if (ent->spawnflags.has(SPAWNFLAG_SPEAKER_LOOPED_OFF | SPAWNFLAG_SPEAKER_LOOPED_ON)) {
			ent->attenuation = ATTN_LOOP_NONE;
			ent->svflags |= SVF_NOCULL;
		} else
			ent->attenuation = ATTN_NONE;
	}

	ent->s.loop_attenuation = ent->attenuation;

	// check for prestarted looping sound
	if (ent->spawnflags.has(SPAWNFLAG_SPEAKER_LOOPED_ON))
		ent->s.sound = ent->noise_index;

	if (ent->spawnflags.has(SPAWNFLAG_SPEAKER_NO_STEREO))
		ent->s.renderfx |= RF_NO_STEREO;

	ent->use = Use_Target_Speaker;

	// must link the entity so we get areas and clusters so
	// the server can determine who to send updates to
	gi.linkentity(ent);
}

//==========================================================

constexpr spawnflags_t SPAWNFLAG_HELP_HELP1 = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAG_SET_POI = 2_spawnflag;

extern void target_poi_use(gentity_t *ent, gentity_t *other, gentity_t *activator);
static USE(Use_Target_Help) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	if (ent->spawnflags.has(SPAWNFLAG_HELP_HELP1)) {
		if (strcmp(game.helpmessage1, ent->message)) {
			Q_strlcpy(game.helpmessage1, ent->message, sizeof(game.helpmessage1));
			game.help1changed++;
		}
	} else {
		if (strcmp(game.helpmessage2, ent->message)) {
			Q_strlcpy(game.helpmessage2, ent->message, sizeof(game.helpmessage2));
			game.help2changed++;
		}
	}

	if (ent->spawnflags.has(SPAWNFLAG_SET_POI)) {
		target_poi_use(ent, other, activator);
	}
}

/*QUAKED target_help (1 0 1) (-16 -16 -24) (16 16 24) HELP1 SETPOI x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
When fired, the "message" key becomes the current personal computer string, and the message light will be set on all clients status bars.
*/
void SP_target_help(gentity_t *ent) {
	if (deathmatch->integer) { // auto-remove for deathmatch
		G_FreeEntity(ent);
		return;
	}

	if (!ent->message) {
		gi.Com_PrintFmt("{}: no message\n", *ent);
		G_FreeEntity(ent);
		return;
	}

	ent->use = Use_Target_Help;

	if (ent->spawnflags.has(SPAWNFLAG_SET_POI)) {
		if (st.image)
			ent->noise_index = gi.imageindex(st.image);
		else
			ent->noise_index = gi.imageindex("friend");
	}
}

//==========================================================

/*QUAKED target_secret (1 0 1) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Counts a secret found.
These are single use targets.
*/
static USE(use_target_secret) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	gi.sound(ent, CHAN_VOICE, ent->noise_index, 1, ATTN_NORM, 0);

	level.found_secrets++;

	if (G_UseTargets(ent, activator))
		G_FreeEntity(ent);
}

static THINK(G_VerifyTargetted) (gentity_t *ent) -> void {
	if (!ent->targetname || !*ent->targetname)
		gi.Com_PrintFmt("WARNING: missing targetname on {}\n", *ent);
	else if (!G_FindByString<&gentity_t::target>(nullptr, ent->targetname))
		gi.Com_PrintFmt("WARNING: doesn't appear to be anything targeting {}\n", *ent);
}

void SP_target_secret(gentity_t *ent) {
	if (deathmatch->integer) { // auto-remove for deathmatch
		G_FreeEntity(ent);
		return;
	}

	ent->think = G_VerifyTargetted;
	ent->nextthink = level.time + 10_ms;

	ent->use = use_target_secret;
	if (!st.noise)
		st.noise = "misc/secret.wav";
	ent->noise_index = gi.soundindex(st.noise);
	ent->svflags = SVF_NOCLIENT;
	level.total_secrets++;
}

//==========================================================
// [Paril-KEX] notify this player of a goal change
void G_PlayerNotifyGoal(gentity_t *player) {
	// no goals in DM
	if (deathmatch->integer)
		return;

	if (!player->client->pers.spawned)
		return;
	else if ((level.time - player->client->resp.entertime) < 300_ms)
		return;

	// N64 goals
	if (level.goals) {
		// if the goal has updated, commit it first
		if (game.help1changed != game.help2changed) {
			const char *current_goal = level.goals;

			// skip ahead by the number of goals we've finished
			for (size_t i = 0; i < level.goal_num; i++) {
				while (*current_goal && *current_goal != '\t')
					current_goal++;

				if (!*current_goal)
					gi.Com_Error("invalid n64 goals; tell Paril\n");

				current_goal++;
			}

			// find the end of this goal
			const char *goal_end = current_goal;

			while (*goal_end && *goal_end != '\t')
				goal_end++;

			Q_strlcpy(game.helpmessage1, current_goal, min((size_t)(goal_end - current_goal + 1), sizeof(game.helpmessage1)));

			game.help2changed = game.help1changed;
		}

		if (player->client->pers.game_help1changed != game.help1changed) {
			gi.LocClient_Print(player, PRINT_TYPEWRITER, game.helpmessage1);
			gi.local_sound(player, player, CHAN_AUTO | CHAN_RELIABLE, gi.soundindex("misc/talk.wav"), 1.0f, ATTN_NONE, 0.0f, GetUnicastKey());

			player->client->pers.game_help1changed = game.help1changed;
		}

		// no regular goals
		return;
	}

	if (player->client->pers.game_help1changed != game.help1changed) {
		player->client->pers.game_help1changed = game.help1changed;
		player->client->pers.helpchanged = 1;
		player->client->pers.help_time = level.time + 5_sec;

		if (*game.helpmessage1)
			// [Sam-KEX] Print objective to screen
			gi.LocClient_Print(player, PRINT_TYPEWRITER, "$g_primary_mission_objective", game.helpmessage1);
	}

	if (player->client->pers.game_help2changed != game.help2changed) {
		player->client->pers.game_help2changed = game.help2changed;
		player->client->pers.helpchanged = 1;
		player->client->pers.help_time = level.time + 5_sec;

		if (*game.helpmessage2)
			// [Sam-KEX] Print objective to screen
			gi.LocClient_Print(player, PRINT_TYPEWRITER, "$g_secondary_mission_objective", game.helpmessage2);
	}
}

/*QUAKED target_goal (1 0 1) (-8 -8 -8) (8 8 8) KEEP_MUSIC x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Counts a goal completed.
These are single use targets.
*/
constexpr spawnflags_t SPAWNFLAG_GOAL_KEEP_MUSIC = 1_spawnflag;

static USE(use_target_goal) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	gi.sound(ent, CHAN_VOICE, ent->noise_index, 1, ATTN_NORM, 0);

	level.found_goals++;

	if (level.found_goals == level.total_goals && !ent->spawnflags.has(SPAWNFLAG_GOAL_KEEP_MUSIC)) {
		if (ent->sounds)
			gi.configstring(CS_CDTRACK, G_Fmt("{}", ent->sounds).data());
		else
			gi.configstring(CS_CDTRACK, "0");
	}

	// [Paril-KEX] n64 goals
	if (level.goals) {
		level.goal_num++;
		game.help1changed++;

		for (auto player : active_clients())
			G_PlayerNotifyGoal(player);
	}

	if (G_UseTargets(ent, activator))
		G_FreeEntity(ent);
}

void SP_target_goal(gentity_t *ent) {
	if (deathmatch->integer) { // auto-remove for deathmatch
		G_FreeEntity(ent);
		return;
	}

	ent->use = use_target_goal;
	if (!st.noise)
		st.noise = "misc/secret.wav";
	ent->noise_index = gi.soundindex(st.noise);
	ent->svflags = SVF_NOCLIENT;
	level.total_goals++;
}

//==========================================================

/*QUAKED target_explosion (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Spawns an explosion temporary entity when used.

"delay"		wait this long before going off
"dmg"		how much radius damage should be done, defaults to 0
*/
static THINK(target_explosion_explode) (gentity_t *self) -> void {
	float save;
	const int32_t self_generation = self->spawn_count;
	gentity_t *activator = self->activator;
	if (!activator || !activator->inuse ||
		activator->spawn_count != self->count ||
		(activator->client && !activator->client->pers.connected))
		activator = world;
	const int32_t activator_generation = activator->spawn_count;

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_EXPLOSION1);
	gi.WritePosition(self->s.origin);
	gi.multicast(self->s.origin, MULTICAST_PHS, false);

	T_RadiusDamage(self, activator, (float)self->dmg, nullptr, (float)self->dmg + 40, DAMAGE_NONE, MOD_EXPLOSIVE);
	if (!self->inuse || self->spawn_count != self_generation)
		return;
	if (activator && (!activator->inuse ||
		activator->spawn_count != activator_generation))
		activator = world;

	save = self->delay;
	self->delay = 0;
	if (!G_UseTargets(self, activator) || !self->inuse ||
		self->spawn_count != self_generation)
		return;
	self->delay = save;
}

static USE(use_target_explosion) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	self->activator = activator;
	self->count = activator ? activator->spawn_count : 0;

	if (!self->delay) {
		target_explosion_explode(self);
		return;
	}

	self->think = target_explosion_explode;
	self->nextthink = level.time + gtime_t::from_sec(self->delay);
}

void SP_target_explosion(gentity_t *ent) {
	ent->use = use_target_explosion;
	ent->svflags = SVF_NOCLIENT;
}

//==========================================================

/*QUAKED target_changelevel (1 0 0) (-8 -8 -8) (8 8 8) END_OF_UNIT x x CLEAR_INVENTORY NO_END_OF_UNIT FADE_OUT IMMEDIATE_LEAVE x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Changes level to "map" when fired
*/
static USE(use_target_changelevel) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	if (level.intermission_time)
		return; // already activated

	if (!deathmatch->integer && !coop->integer)
		if (g_entities[1].health <= 0)
			return;

	// if noexit, do a ton of damage to other
	if (deathmatch->integer && !g_dm_allow_exit->integer && other != world) {
		T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, 10 * other->max_health, 1000, DAMAGE_NONE, MOD_EXIT);
		return;
	}

	// if multiplayer, let everyone know who hit the exit
	if (deathmatch->integer) {
		if (false) { // Race mode removed
			if (!IsCombatDisabled()) {
				if (level.match_state == MATCH_IN_PROGRESS) {
					int old_score = activator->client->resp.score;
					int new_score = (level.time - activator->client->respawn_time).milliseconds();
					bool pb = !old_score || new_score < old_score;
					if (pb) {
						G_SetPlayerScore(activator->client, (level.time - activator->client->respawn_time).milliseconds());
						gi.sound(activator, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex("ctf/flagcap.wav"), 1, ATTN_NONE, 0);
					}
					gi.LocClient_Print(activator, PRINT_CENTER, "{}{}", G_TimeStringMs(new_score, false), pb ? " (PB)" : "");
				} else {
					gi.LocClient_Print(activator, PRINT_CENTER, "{}", G_TimeStringMs((level.time - activator->client->respawn_time).milliseconds(), false));
				}
				ClientSpawn(activator);
				G_PostRespawn(activator);

				activator->client->pers.last_spawn_time = level.time;

				gtime_t clock = timelimit->value ? (level.match_time + gtime_t::from_min(timelimit->value) + level.overtime - level.time) : level.time - level.match_time;
				int	t = clock.milliseconds();
				const char *s, *s1, *s2 = "";

				int t2 = (level.time - activator->client->pers.last_spawn_time).milliseconds();
				s1 = G_Fmt("{} ({})", G_TimeString(t, false), G_TimeStringMs(t2, false)).data();

				s = G_Fmt("{}{}", s1, s2).data();

				activator->client->ps.stats[STAT_MATCH_STATE] = CONFIG_MATCH_STATE;
				gi.configstring(CONFIG_MATCH_STATE, s);
			}
			return;
		} else {
			if (level.time < 10_sec)
				return;

			if (activator && activator->client)
				gi.LocBroadcast_Print(PRINT_HIGH, "$g_exited_level", MM_PlayerLocalizationName(activator->client));
		}
	}

	// if going to a new unit, clear cross triggers
	if (strstr(self->map, "*"))
		game.cross_level_flags &= ~(SFL_CROSS_TRIGGER_MASK);

	// if map has a landmark, store position instead of using spawn next map
	if (activator && activator->client && !deathmatch->integer) {
		activator->client->landmark_name = nullptr;
		activator->client->landmark_rel_pos = vec3_origin;

		self->target_ent = G_PickTarget(self->target);
		if (self->target_ent && activator && activator->client) {
			activator->client->landmark_name = G_CopyString(self->target_ent->targetname, TAG_GAME);

			// get relative vector to landmark pos, and unrotate by the landmark angles in preparation to be
			// rotated by the next map
			activator->client->landmark_rel_pos = activator->s.origin - self->target_ent->s.origin;

			activator->client->landmark_rel_pos = RotatePointAroundVector({ 1, 0, 0 }, activator->client->landmark_rel_pos, -self->target_ent->s.angles[PITCH]);
			activator->client->landmark_rel_pos = RotatePointAroundVector({ 0, 1, 0 }, activator->client->landmark_rel_pos, -self->target_ent->s.angles[ROLL]);
			activator->client->landmark_rel_pos = RotatePointAroundVector({ 0, 0, 1 }, activator->client->landmark_rel_pos, -self->target_ent->s.angles[YAW]);

			activator->client->oldvelocity = RotatePointAroundVector({ 1, 0, 0 }, activator->client->oldvelocity, -self->target_ent->s.angles[PITCH]);
			activator->client->oldvelocity = RotatePointAroundVector({ 0, 1, 0 }, activator->client->oldvelocity, -self->target_ent->s.angles[ROLL]);
			activator->client->oldvelocity = RotatePointAroundVector({ 0, 0, 1 }, activator->client->oldvelocity, -self->target_ent->s.angles[YAW]);

			// unrotate our view angles for the next map too
			activator->client->oldviewangles = activator->client->ps.viewangles - self->target_ent->s.angles;
		}
	}

	BeginIntermission(self);
}

void SP_target_changelevel(gentity_t *ent) {
	if (!ent->map) {
		gi.Com_PrintFmt("{}: no map\n", *ent);
		G_FreeEntity(ent);
		return;
	}

	ent->use = use_target_changelevel;
	ent->svflags = SVF_NOCLIENT;
}

//==========================================================

/*QUAKED target_splash (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Creates a particle splash effect when used.

Set "sounds" to one of the following:
  1) sparks
  2) blue water
  3) brown water
  4) slime
  5) lava
  6) blood

"count"	how many pixels in the splash
"dmg"	if set, does a radius damage at this location when it splashes
		useful for lava/sparks
*/

static USE(use_target_splash) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_SPLASH);
	gi.WriteByte(self->count);
	gi.WritePosition(self->s.origin);
	gi.WriteDir(self->movedir);
	gi.WriteByte(self->sounds);
	gi.multicast(self->s.origin, MULTICAST_PVS, false);

	if (self->dmg)
		T_RadiusDamage(self, activator, (float)self->dmg, nullptr, (float)self->dmg + 40, DAMAGE_NONE, MOD_SPLASH);
}

void SP_target_splash(gentity_t *self) {
	self->use = use_target_splash;
	G_SetMovedir(self->s.angles, self->movedir);

	if (!self->count)
		self->count = 32;

	// N64 "sparks" are blue, not yellow.
	if (level.is_n64 && self->sounds == 1)
		self->sounds = 7;

	self->svflags = SVF_NOCLIENT;
}

//==========================================================

/*QUAKED target_spawner (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Set target to the type of entity you want spawned.
Useful for spawning monsters and gibs in the factory levels.

For monsters:
	Set direction to the facing you want it to have.

For gibs:
	Set direction if you want it moving and
	speed how fast it should be moving otherwise it
	will just be dropped
*/
void ED_CallSpawn(gentity_t *ent);

static USE(use_target_spawner) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	// don't trigger spawn monsters in horde mode
	if (GT(GT_HORDE) && !Q_strncasecmp("monster_", self->target, 8))
		return;

	gentity_t *ent;

	ent = G_Spawn();
	ent->classname = self->target;
	ent->flags = self->flags;
	ent->s.origin = self->s.origin;
	ent->s.angles = self->s.angles;
	st = {};

	// [Paril-KEX] although I fixed these in our maps, this is just
	// in case anybody else does this by accident. Don't count these monsters
	// so they don't inflate the monster count.
	ent->monsterinfo.aiflags |= AI_DO_NOT_COUNT;

	ED_CallSpawn(ent);
	gi.linkentity(ent);

	if (!KillBox(ent, false))
		return;
	if (self->speed)
		ent->velocity = self->movedir;

	ent->s.renderfx |= RF_IR_VISIBLE;
}

void SP_target_spawner(gentity_t *self) {
	self->use = use_target_spawner;
	self->svflags = SVF_NOCLIENT;
	if (self->speed) {
		G_SetMovedir(self->s.angles, self->movedir);
		self->movedir *= self->speed;
	}
}

//==========================================================

/*QUAKED target_blaster (1 0 0) (-8 -8 -8) (8 8 8) NOTRAIL NOEFFECTS x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Fires a blaster bolt in the set direction when triggered.

dmg		default is 15
speed	default is 1000
*/

constexpr spawnflags_t SPAWNFLAG_BLASTER_NOTRAIL = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAG_BLASTER_NOEFFECTS = 2_spawnflag;

static USE(use_target_blaster) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	effects_t effect;

	if (self->spawnflags.has(SPAWNFLAG_BLASTER_NOEFFECTS))
		effect = EF_NONE;
	else if (self->spawnflags.has(SPAWNFLAG_BLASTER_NOTRAIL))
		effect = EF_HYPERBLASTER;
	else
		effect = EF_BLASTER;

	fire_blaster(self, self->s.origin, self->movedir, self->dmg, (int)self->speed, effect, MOD_TARGET_BLASTER);
	gi.sound(self, CHAN_VOICE, self->noise_index, 1, ATTN_NORM, 0);
}

void SP_target_blaster(gentity_t *self) {
	self->use = use_target_blaster;
	G_SetMovedir(self->s.angles, self->movedir);
	self->noise_index = gi.soundindex("weapons/laser2.wav");

	if (!self->dmg)
		self->dmg = 15;
	if (!self->speed)
		self->speed = 1000;

	self->svflags = SVF_NOCLIENT;
}

//==========================================================

/*QUAKED target_crosslevel_trigger (.5 .5 .5) (-8 -8 -8) (8 8 8) TRIGGER1 TRIGGER2 TRIGGER3 TRIGGER4 TRIGGER5 TRIGGER6 TRIGGER7 TRIGGER8 NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Once this trigger is touched/used, any trigger_crosslevel_target with the same trigger number is automatically used when a level is started within the same unit.  It is OK to check multiple triggers.  Message, delay, target, and killtarget also work.
*/
static USE(trigger_crosslevel_trigger_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	game.cross_level_flags |= self->spawnflags.value;
	G_FreeEntity(self);
}

void SP_target_crosslevel_trigger(gentity_t *self) {
	self->svflags = SVF_NOCLIENT;
	self->use = trigger_crosslevel_trigger_use;
}

/*QUAKED target_crosslevel_target (.5 .5 .5) (-8 -8 -8) (8 8 8) TRIGGER1 TRIGGER2 TRIGGER3 TRIGGER4 TRIGGER5 TRIGGER6 TRIGGER7 TRIGGER8 x x x x x x x x TRIGGER9 TRIGGER10 TRIGGER11 TRIGGER12 TRIGGER13 TRIGGER14 TRIGGER15 TRIGGER16
Triggered by a trigger_crosslevel elsewhere within a unit.  If multiple triggers are checked, all must be true.  Delay, target and
killtarget also work.

"delay"		delay before using targets if the trigger has been activated (default 1)
*/
static THINK(target_crosslevel_target_think) (gentity_t *self) -> void {
	if (self->spawnflags.value == (game.cross_level_flags & SFL_CROSS_TRIGGER_MASK & self->spawnflags.value)) {
		if (G_UseTargets(self, self))
			G_FreeEntity(self);
	}
}

void SP_target_crosslevel_target(gentity_t *self) {
	if (!self->delay)
		self->delay = 1;
	self->svflags = SVF_NOCLIENT;

	self->think = target_crosslevel_target_think;
	self->nextthink = level.time + gtime_t::from_sec(self->delay);
}

//==========================================================

/*QUAKED target_laser (0 .5 .8) (-8 -8 -8) (8 8 8) START_ON RED GREEN BLUE YELLOW ORANGE FAT WINDOWSTOP NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
When triggered, fires a laser.  You can either set a target or a direction.

WINDOWSTOP - stops at CONTENTS_WINDOW
*/

constexpr spawnflags_t SPAWNFLAG_LASER_STOPWINDOW = 0x0080_spawnflag;

struct laser_pierce_t : pierce_args_t {
	gentity_t *self;
	int32_t count;
	bool damaged_thing = false;

	inline laser_pierce_t(gentity_t *self, int32_t count) :
		pierce_args_t(),
		self(self),
		count(count) {}

	// we hit an entity; return false to stop the piercing.
	// you can adjust the mask for the re-trace (for water, etc).
	virtual bool hit(contents_t &mask, vec3_t &end) override {
		// hurt it if we can
		if (self->dmg > 0 && (tr.ent->takedamage) && !(tr.ent->flags & FL_IMMUNE_LASER) && self->damage_debounce_time <= level.time) {
			damaged_thing = true;
			T_Damage(tr.ent, self, self->activator, self->movedir, tr.endpos, vec3_origin, self->dmg, 1, DAMAGE_ENERGY, MOD_TARGET_LASER);
		}

		// if we hit something that's not a monster or player or is immune to lasers, we're done
		if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client) && !(tr.ent->flags & FL_DAMAGEABLE)) {
			if (self->spawnflags.has(SPAWNFLAG_LASER_ZAP)) {
				self->spawnflags &= ~SPAWNFLAG_LASER_ZAP;
				gi.WriteByte(svc_temp_entity);
				gi.WriteByte(TE_LASER_SPARKS);
				gi.WriteByte(count);
				gi.WritePosition(tr.endpos);
				gi.WriteDir(tr.plane.normal);
				gi.WriteByte(self->s.skinnum);
				gi.multicast(tr.endpos, MULTICAST_PVS, false);
			}

			return false;
		}

		if (!mark(tr.ent))
			return false;

		return true;
	}
};

static THINK(target_laser_think) (gentity_t *self) -> void {
	int32_t count;

	if (self->spawnflags.has(SPAWNFLAG_LASER_ZAP))
		count = 8;
	else
		count = 4;

	if (self->enemy) {
		vec3_t last_movedir = self->movedir;
		vec3_t point = (self->enemy->absmin + self->enemy->absmax) * 0.5f;
		self->movedir = point - self->s.origin;
		self->movedir.normalize();
		if (self->movedir != last_movedir)
			self->spawnflags |= SPAWNFLAG_LASER_ZAP;
	}

	vec3_t start = self->s.origin;
	vec3_t end = start + (self->movedir * 2048);

	laser_pierce_t args{
		self,
		count
	};

	contents_t mask = self->spawnflags.has(SPAWNFLAG_LASER_STOPWINDOW) ? MASK_SHOT : (CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_PLAYER | CONTENTS_DEADMONSTER);

	pierce_trace(start, end, self, args, mask);

	self->s.old_origin = args.tr.endpos;

	if (args.damaged_thing)
		self->damage_debounce_time = level.time + 10_hz;

	self->nextthink = level.time + FRAME_TIME_S;
	gi.linkentity(self);
}

static void target_laser_on(gentity_t *self) {
	if (!self->activator)
		self->activator = self;
	self->spawnflags |= SPAWNFLAG_LASER_ZAP | SPAWNFLAG_LASER_ON;
	self->svflags &= ~SVF_NOCLIENT;
	self->flags |= FL_TRAP;
	target_laser_think(self);
}

void target_laser_off(gentity_t *self) {
	self->spawnflags &= ~SPAWNFLAG_LASER_ON;
	self->svflags |= SVF_NOCLIENT;
	self->flags &= ~FL_TRAP;
	self->nextthink = 0_ms;
}

static USE(target_laser_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	self->activator = activator;
	if (self->spawnflags.has(SPAWNFLAG_LASER_ON))
		target_laser_off(self);
	else
		target_laser_on(self);
}

static THINK(target_laser_start) (gentity_t *self) -> void {
	gentity_t *ent;

	self->movetype = MOVETYPE_NONE;
	self->solid = SOLID_NOT;
	self->s.renderfx |= RF_BEAM;
	self->s.modelindex = MODELINDEX_WORLD; // must be non-zero

	// [Sam-KEX] On Q2N64, spawnflag of 128 turns it into a lightning bolt
	if (level.is_n64) {
		// Paril: fix for N64
		if (self->spawnflags.has(SPAWNFLAG_LASER_STOPWINDOW)) {
			self->spawnflags &= ~SPAWNFLAG_LASER_STOPWINDOW;
			self->spawnflags |= SPAWNFLAG_LASER_LIGHTNING;
		}
	}

	if (self->spawnflags.has(SPAWNFLAG_LASER_LIGHTNING)) {
		self->s.renderfx |= RF_BEAM_LIGHTNING; // tell renderer it is lightning

		if (!self->s.skinnum)
			self->s.skinnum = 0xf3f3f1f1; // default lightning color
	}

	// set the beam diameter
	// [Paril-KEX] lab has this set prob before lightning was implemented
	if (!level.is_n64 && self->spawnflags.has(SPAWNFLAG_LASER_FAT))
		self->s.frame = 16;
	else
		self->s.frame = 4;

	// set the color
	if (!self->s.skinnum) {
		if (self->spawnflags.has(SPAWNFLAG_LASER_RED))
			self->s.skinnum = 0xf2f2f0f0;
		else if (self->spawnflags.has(SPAWNFLAG_LASER_GREEN))
			self->s.skinnum = 0xd0d1d2d3;
		else if (self->spawnflags.has(SPAWNFLAG_LASER_BLUE))
			self->s.skinnum = 0xf3f3f1f1;
		else if (self->spawnflags.has(SPAWNFLAG_LASER_YELLOW))
			self->s.skinnum = 0xdcdddedf;
		else if (self->spawnflags.has(SPAWNFLAG_LASER_ORANGE))
			self->s.skinnum = 0xe0e1e2e3;
	}

	if (!self->enemy) {
		if (self->target) {
			ent = G_FindByString<&gentity_t::targetname>(nullptr, self->target);
			if (!ent)
				gi.Com_PrintFmt("{}: {} is a bad target\n", *self, self->target);
			else {
				self->enemy = ent;

				// N64 fix
				// FIXME: which map was this for again? oops
				if (level.is_n64 && !strcmp(self->enemy->classname, "func_train") && !(self->enemy->spawnflags & SPAWNFLAG_TRAIN_START_ON))
					self->enemy->use(self->enemy, self, self);
			}
		} else {
			G_SetMovedir(self->s.angles, self->movedir);
		}
	}
	self->use = target_laser_use;
	self->think = target_laser_think;

	if (!self->dmg)
		self->dmg = 1;

	self->mins = { -8, -8, -8 };
	self->maxs = { 8, 8, 8 };
	gi.linkentity(self);

	if (self->spawnflags.has(SPAWNFLAG_LASER_ON))
		target_laser_on(self);
	else
		target_laser_off(self);
}

void SP_target_laser(gentity_t *self) {
	// let everything else get spawned before we start firing
	self->think = target_laser_start;
	self->flags |= FL_TRAP_LASER_FIELD;
	self->nextthink = level.time + 1_sec;
}

//==========================================================

/*QUAKED target_lightramp (0 .5 .8) (-8 -8 -8) (8 8 8) TOGGLE x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
speed		How many seconds the ramping will take
message		two letters; starting lightlevel and ending lightlevel
*/

constexpr spawnflags_t SPAWNFLAG_LIGHTRAMP_TOGGLE = 1_spawnflag;

static bool TargetLightrampStyleIsValid(int32_t style) {
	return style >= 0 && style < static_cast<int32_t>(MAX_LIGHTSTYLES);
}

static THINK(target_lightramp_think) (gentity_t *self) -> void {
	char style[2];

	if (!self->enemy || !TargetLightrampStyleIsValid(self->enemy->style)) {
		self->think = nullptr;
		self->nextthink = 0_ms;
		return;
	}

	style[0] = (char)('a' + self->movedir[0] + ((level.time - self->timestamp) / gi.frame_time_s).seconds() * self->movedir[2]);
	style[1] = 0;

	gi.configstring(CS_LIGHTS + self->enemy->style, style);

	if ((level.time - self->timestamp).seconds() < self->speed) {
		self->nextthink = level.time + FRAME_TIME_S;
	} else if (self->spawnflags.has(SPAWNFLAG_LIGHTRAMP_TOGGLE)) {
		char temp;

		temp = (char)self->movedir[0];
		self->movedir[0] = self->movedir[1];
		self->movedir[1] = temp;
		self->movedir[2] *= -1;
	}
}

static USE(target_lightramp_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	if (!self->enemy) {
		gentity_t *e;

		// check all the targets
		e = nullptr;
		while (1) {
			e = G_FindByString<&gentity_t::targetname>(e, self->target);
			if (!e)
				break;
			if (strcmp(e->classname, "light") != 0) {
				gi.Com_PrintFmt("{}: target {} ({}) is not a light\n", *self, self->target, *e);
			} else if (!TargetLightrampStyleIsValid(e->style)) {
				gi.Com_PrintFmt("{}: target {} ({}) has invalid lightstyle {}\n", *self, self->target, *e, e->style);
			} else {
				self->enemy = e;
			}
		}

		if (!self->enemy) {
			gi.Com_PrintFmt("{}: target {} not found\n", *self, self->target);
			G_FreeEntity(self);
			return;
		}
	}

	self->timestamp = level.time;
	target_lightramp_think(self);
}

void SP_target_lightramp(gentity_t *self) {
	if (!self->message || strlen(self->message) != 2 || self->message[0] < 'a' || self->message[0] > 'z' || self->message[1] < 'a' || self->message[1] > 'z' || self->message[0] == self->message[1]) {
		gi.Com_PrintFmt("{}: bad ramp ({})\n", *self, self->message ? self->message : "null string");
		G_FreeEntity(self);
		return;
	}

	if (deathmatch->integer) {
		G_FreeEntity(self);
		return;
	}

	if (!self->target) {
		gi.Com_PrintFmt("{}: no target\n", *self);
		G_FreeEntity(self);
		return;
	}

	if (self->speed <= 0.0f || !std::isfinite(self->speed))
		self->speed = 1.0f;

	self->svflags |= SVF_NOCLIENT;
	self->use = target_lightramp_use;
	self->think = target_lightramp_think;

	self->movedir[0] = (float)(self->message[0] - 'a');
	self->movedir[1] = (float)(self->message[1] - 'a');
	self->movedir[2] = (self->movedir[1] - self->movedir[0]) / (self->speed / gi.frame_time_s);
}

//==========================================================

/*QUAKED target_earthquake (1 0 0) (-8 -8 -8) (8 8 8) SILENT TOGGLE UNKNOWN_ROGUE ONE_SHOT x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
When triggered, this initiates a level-wide earthquake.
All players are affected with a screen shake.
"speed"		severity of the quake (default:200)
"count"		duration of the quake (default:5)
*/

constexpr spawnflags_t SPAWNFLAGS_EARTHQUAKE_SILENT = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAGS_EARTHQUAKE_TOGGLE = 2_spawnflag;
[[maybe_unused]] constexpr spawnflags_t SPAWNFLAGS_EARTHQUAKE_UNKNOWN_ROGUE = 4_spawnflag;
constexpr spawnflags_t SPAWNFLAGS_EARTHQUAKE_ONE_SHOT = 8_spawnflag;

static bool target_earthquake_affects(const gentity_t *self, const gentity_t *player) {
	if (notGT(GT_ARENA))
		return true;

	const int arena_id = MM_Arena_Id(self);
	return arena_id <= 0 || MM_Arena_Id(player) == arena_id;
}

static void target_earthquake_sound(gentity_t *self) {
	if (notGT(GT_ARENA)) {
		gi.positioned_sound(self->s.origin, self, CHAN_VOICE, self->noise_index, 1.0, ATTN_NONE, 0);
		return;
	}

	for (gentity_t *player : active_clients()) {
		if (target_earthquake_affects(self, player))
			gi.local_sound(player, self, CHAN_VOICE, self->noise_index, 1.0, ATTN_NONE, 0);
	}
}

static THINK(target_earthquake_think) (gentity_t *self) -> void {
	if (!(self->spawnflags & SPAWNFLAGS_EARTHQUAKE_SILENT)) {
		if (self->last_move_time < level.time) {
			target_earthquake_sound(self);
			self->last_move_time = level.time + 6.5_sec;
		}
	}

	for (gentity_t *player : active_clients()) {
		if (!target_earthquake_affects(self, player))
			continue;
		player->client->quake_time = level.time + 1000_ms;
	}

	if (level.time < self->timestamp)
		self->nextthink = level.time + 10_hz;
}

static USE(target_earthquake_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	if (self->spawnflags.has(SPAWNFLAGS_EARTHQUAKE_ONE_SHOT)) {
		for (gentity_t *player : active_clients()) {
			if (!target_earthquake_affects(self, player))
				continue;
			player->client->v_dmg_pitch = -self->speed * 0.1f;
			player->client->v_dmg_time = level.time + DAMAGE_TIME();
		}

		return;
	}

	self->timestamp = level.time + gtime_t::from_sec(self->count);

	if (self->spawnflags.has(SPAWNFLAGS_EARTHQUAKE_TOGGLE)) {
		if (self->style)
			self->nextthink = 0_ms;
		else
			self->nextthink = level.time + FRAME_TIME_S;

		self->style = !self->style;
	} else {
		self->nextthink = level.time + FRAME_TIME_S;
		self->last_move_time = 0_ms;
	}

	self->activator = activator;
}

void SP_target_earthquake(gentity_t *self) {
	if (!self->targetname)
		gi.Com_PrintFmt("{}: untargeted\n", *self);

	if (level.is_n64) {
		self->spawnflags |= SPAWNFLAGS_EARTHQUAKE_TOGGLE;
		self->speed = 5;
	}

	if (!self->count)
		self->count = 5;

	if (!self->speed)
		self->speed = 200;

	self->svflags |= SVF_NOCLIENT;
	self->think = target_earthquake_think;
	self->use = target_earthquake_use;

	if (!(self->spawnflags & SPAWNFLAGS_EARTHQUAKE_SILENT))
		self->noise_index = gi.soundindex("world/quake.wav");
}

/*QUAKED target_camera (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
[Sam-KEX] Creates a camera path as seen in the N64 version.
*/

constexpr size_t HACKFLAG_TELEPORT_OUT = 2;
constexpr size_t HACKFLAG_SKIPPABLE = 64;
constexpr size_t HACKFLAG_END_OF_UNIT = 128;

static void camera_lookat_pathtarget(gentity_t *self, vec3_t origin, vec3_t *dest) {
	if (self->pathtarget) {
		gentity_t *pt = nullptr;
		pt = G_FindByString<&gentity_t::targetname>(pt, self->pathtarget);
		if (pt) {
			float yaw, pitch;
			vec3_t delta = pt->s.origin - origin;

			float d = delta[0] * delta[0] + delta[1] * delta[1];
			if (d == 0.0f) {
				yaw = 0.0f;
				pitch = (delta[2] > 0.0f) ? 90.0f : -90.0f;
			} else {
				yaw = atan2(delta[1], delta[0]) * (180.0f / PIf);
				pitch = atan2(delta[2], sqrt(d)) * (180.0f / PIf);
			}

			(*dest)[YAW] = yaw;
			(*dest)[PITCH] = -pitch;
			(*dest)[ROLL] = 0;
		}
	}
}

static THINK(update_target_camera) (gentity_t *self) -> void {
	bool do_skip = false;

	// only allow skipping after 2 seconds
	if ((self->hackflags & HACKFLAG_SKIPPABLE) && level.time > 2_sec) {
		for (auto ce : active_clients()) {
			if (ce->client->buttons & BUTTON_ANY) {
				do_skip = true;
				break;
			}
		}
	}

	if (!do_skip && self->movetarget) {
		self->moveinfo.remaining_distance -= (self->moveinfo.move_speed * gi.frame_time_s) * 0.8f;

		if (self->moveinfo.remaining_distance <= 0) {
			if (self->movetarget->hackflags & HACKFLAG_TELEPORT_OUT) {
				if (self->enemy) {
					self->enemy->s.event = EV_PLAYER_TELEPORT;
					self->enemy->hackflags = HACKFLAG_TELEPORT_OUT;
					self->enemy->pain_debounce_time = self->enemy->timestamp = gtime_t::from_sec(self->movetarget->wait);
				}
			}

			self->s.origin = self->movetarget->s.origin;
			self->nextthink = level.time + gtime_t::from_sec(self->movetarget->wait);
			if (self->movetarget->target) {
				self->movetarget = G_PickTarget(self->movetarget->target);

				if (self->movetarget) {
					self->moveinfo.move_speed = self->movetarget->speed ? self->movetarget->speed : 55;
					self->moveinfo.remaining_distance = (self->movetarget->s.origin - self->s.origin).normalize();
					self->moveinfo.distance = self->moveinfo.remaining_distance;
				}
			} else
				self->movetarget = nullptr;

			return;
		} else {
			float frac = 1.0f - (self->moveinfo.remaining_distance / self->moveinfo.distance);

			if (self->enemy && (self->enemy->hackflags & HACKFLAG_TELEPORT_OUT))
				self->enemy->s.alpha = max(1.f / 255.f, frac);

			vec3_t delta = self->movetarget->s.origin - self->s.origin;
			delta *= frac;
			vec3_t newpos = self->s.origin + delta;

			camera_lookat_pathtarget(self, newpos, &level.intermission_angle);
			level.intermission_origin = newpos;
			level.spawn_spots[SPAWN_SPOT_INTERMISSION] = self;
			level.spawn_spots[SPAWN_SPOT_INTERMISSION]->s.origin += delta;

			// move all clients to the intermission point
			for (auto ce : active_clients())
				MoveClientToIntermission(ce);
		}
	} else {
		if (self->killtarget) {
			// destroy dummy player
			if (self->enemy)
				G_FreeEntity(self->enemy);

			gentity_t *t = nullptr;
			level.intermission_time = 0_ms;
			level.level_intermission_set = true;

			while ((t = G_FindByString<&gentity_t::targetname>(t, self->killtarget))) {
				t->use(t, self, self->activator);
			}

			level.intermission_time = level.time;
			level.intermission_server_frame = gi.ServerFrame();

			// end of unit requires a wait
			if (level.changemap && !strchr(level.changemap, '*'))
				level.intermission_exit = true;
		}

		self->think = nullptr;
		return;
	}

	self->nextthink = level.time + FRAME_TIME_S;
}

void G_SetClientFrame(gentity_t *ent);

extern float xyspeed;

static THINK(target_camera_dummy_think) (gentity_t *self) -> void {
	// bit of a hack, but this will let the dummy
	// move like a player
	self->client = self->owner->client;
	xyspeed = sqrtf(self->velocity[0] * self->velocity[0] + self->velocity[1] * self->velocity[1]);
	G_SetClientFrame(self);
	self->client = nullptr;

	// alpha fade out for voops
	if (self->hackflags & HACKFLAG_TELEPORT_OUT) {
		self->timestamp = max(0_ms, self->timestamp - 10_hz);
		self->s.alpha = max(1.f / 255.f, (self->timestamp.seconds() / self->pain_debounce_time.seconds()));
	}

	self->nextthink = level.time + 10_hz;
}

static USE(use_target_camera) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	if (self->sounds)
		gi.configstring(CS_CDTRACK, G_Fmt("{}", self->sounds).data());

	if (!self->target)
		return;

	self->movetarget = G_PickTarget(self->target);

	if (!self->movetarget)
		return;

	level.intermission_time = level.time;
	level.intermission_server_frame = gi.ServerFrame();
	level.intermission_exit = false;

	// spawn fake player dummy where we were
	if (activator->client) {
		gentity_t *dummy = self->enemy = G_Spawn();
		dummy->owner = activator;
		dummy->clipmask = activator->clipmask;
		dummy->s.origin = activator->s.origin;
		dummy->s.angles = activator->s.angles;
		dummy->groundentity = activator->groundentity;
		dummy->groundentity_linkcount = dummy->groundentity ? dummy->groundentity->linkcount : 0;
		dummy->think = target_camera_dummy_think;
		dummy->nextthink = level.time + 10_hz;
		dummy->solid = SOLID_BBOX;
		dummy->movetype = MOVETYPE_STEP;
		dummy->mins = activator->mins;
		dummy->maxs = activator->maxs;
		dummy->s.modelindex = dummy->s.modelindex2 = MODELINDEX_PLAYER;
		dummy->s.skinnum = activator->s.skinnum;
		dummy->velocity = activator->velocity;
		dummy->s.renderfx = RF_MINLIGHT;
		dummy->s.frame = activator->s.frame;
		gi.linkentity(dummy);
	}

	camera_lookat_pathtarget(self, self->s.origin, &level.intermission_angle);
	level.intermission_origin = self->s.origin;
	level.spawn_spots[SPAWN_SPOT_INTERMISSION] = self;

	// move all clients to the intermission point
	for (auto ce : active_clients()) {
		// respawn any dead clients
		if (ce->health <= 0 || ce->client->eliminated) {
			// give us our max health back since it will reset
			// to pers.health; in instanced items we'd lose the items
			// we touched so we always want to respawn with our max.
			if (P_UseCoopInstancedItems())
				ce->client->pers.health = ce->client->pers.max_health = ce->max_health;

			ClientRespawn(ce);
		}

		MoveClientToIntermission(ce);
	}

	self->activator = activator;
	self->think = update_target_camera;
	self->nextthink = level.time + gtime_t::from_sec(self->wait);
	self->moveinfo.move_speed = self->speed;

	self->moveinfo.remaining_distance = (self->movetarget->s.origin - self->s.origin).normalize();
	self->moveinfo.distance = self->moveinfo.remaining_distance;

	if (self->hackflags & HACKFLAG_END_OF_UNIT)
		G_EndOfUnitMessage();
}

void SP_target_camera(gentity_t *self) {
	if (deathmatch->integer) { // auto-remove for deathmatch
		G_FreeEntity(self);
		return;
	}

	self->use = use_target_camera;
	self->svflags = SVF_NOCLIENT;
}

/*QUAKED target_gravity (1 0 0) (-8 -8 -8) (8 8 8) NOTRAIL NOEFFECTS x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
[Sam-KEX] Changes gravity, as seen in the N64 version
*/

static USE(use_target_gravity) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	gi.cvar_set("g_gravity", G_Fmt("{}", self->gravity).data());
	level.gravity = self->gravity;
}

void SP_target_gravity(gentity_t *self) {
	if (!TargetParseFiniteFloat(st.gravity, self->gravity)) {
		gi.Com_PrintFmt("{}: invalid gravity '{}'\n", *self, st.gravity ? st.gravity : "");
		G_FreeEntity(self);
		return;
	}

	self->use = use_target_gravity;
}

/*QUAKED target_soundfx (1 0 0) (-8 -8 -8) (8 8 8) NOTRAIL NOEFFECTS x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
[Sam-KEX] Plays a sound fx, as seen in the N64 version
*/

static THINK(update_target_soundfx) (gentity_t *self) -> void {
	gi.positioned_sound(self->s.origin, self, CHAN_VOICE, self->noise_index, self->volume, self->attenuation, 0);
}

static USE(use_target_soundfx) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	self->think = update_target_soundfx;
	self->nextthink = level.time + gtime_t::from_sec(self->delay);
}

void SP_target_soundfx(gentity_t *self) {
	if (!self->volume)
		self->volume = 1.0;

	if (!self->attenuation)
		self->attenuation = 1.0;
	else if (self->attenuation == -1) // use -1 so 0 defaults to 1
		self->attenuation = 0;

	uint32_t noise = 0;
	if (!TargetParseUInt32(st.noise, noise) || noise > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
		gi.Com_PrintFmt("{}: invalid noise '{}'\n", *self, st.noise ? st.noise : "");
		G_FreeEntity(self);
		return;
	}

	self->noise_index = static_cast<int32_t>(noise);

	switch (self->noise_index) {
	case 1:
		self->noise_index = gi.soundindex("world/x_alarm.wav");
		break;
	case 2:
		self->noise_index = gi.soundindex("world/flyby1.wav");
		break;
	case 4:
		self->noise_index = gi.soundindex("world/amb12.wav");
		break;
	case 5:
		self->noise_index = gi.soundindex("world/amb17.wav");
		break;
	case 7:
		self->noise_index = gi.soundindex("world/bigpump2.wav");
		break;
	default:
		gi.Com_PrintFmt("{}: unknown noise {}\n", *self, self->noise_index);
		G_FreeEntity(self);
		return;
	}

	self->use = use_target_soundfx;
}

/*QUAKED target_light (1 0 0) (-8 -8 -8) (8 8 8) START_ON NO_LERP FLICKER x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
[Paril-KEX] dynamic light entity that follows a lightstyle.
*/

constexpr spawnflags_t SPAWNFLAG_TARGET_LIGHT_START_ON = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAG_TARGET_LIGHT_NO_LERP = 2_spawnflag; // not used in N64, but I'll use it for this
constexpr spawnflags_t SPAWNFLAG_TARGET_LIGHT_FLICKER = 4_spawnflag;

static THINK(target_light_flicker_think) (gentity_t *self) -> void {
	if (brandom())
		self->svflags ^= SVF_NOCLIENT;

	self->nextthink = level.time + 10_hz;
}

// think function handles interpolation from start to finish.
static THINK(target_light_think) (gentity_t *self) -> void {
	if (self->spawnflags.has(SPAWNFLAG_TARGET_LIGHT_FLICKER))
		target_light_flicker_think(self);

	const char *style = gi.get_configstring(CS_LIGHTS + self->style);
	const size_t style_length = style ? strlen(style) : 0;
	if (style_length == 0) {
		self->nextthink = level.time + 10_hz;
		return;
	}

	self->delay += self->speed;

	size_t index = TargetLightStyleIndex(self->delay, style_length);
	char style_value = style[index];
	float current_lerp = (float)(style_value - 'a') / (float)('z' - 'a');
	float lerp;

	if (!(self->spawnflags & SPAWNFLAG_TARGET_LIGHT_NO_LERP)) {
		size_t next_index = (index + 1) % style_length;
		char next_style_value = style[next_index];

		float next_lerp = (float)(next_style_value - 'a') / (float)('z' - 'a');

		float mod_lerp = TargetLightDelayFraction(self->delay);
		lerp = (next_lerp * mod_lerp) + (current_lerp * (1.f - mod_lerp));
	} else
		lerp = current_lerp;

	int my_rgb = self->count;
	int target_rgb = self->chain->s.skinnum;

	int my_b = ((my_rgb >> 8) & 0xff);
	int my_g = ((my_rgb >> 16) & 0xff);
	int my_r = ((my_rgb >> 24) & 0xff);

	int target_b = ((target_rgb >> 8) & 0xff);
	int target_g = ((target_rgb >> 16) & 0xff);
	int target_r = ((target_rgb >> 24) & 0xff);

	float backlerp = 1.0f - lerp;

	int b = (target_b * lerp) + (my_b * backlerp);
	int g = (target_g * lerp) + (my_g * backlerp);
	int r = (target_r * lerp) + (my_r * backlerp);

	self->s.skinnum = (b << 8) | (g << 16) | (r << 24);

	self->nextthink = level.time + 10_hz;
}

static USE(target_light_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	self->health = !self->health;

	if (self->health)
		self->svflags &= ~SVF_NOCLIENT;
	else
		self->svflags |= SVF_NOCLIENT;

	if (!self->health) {
		self->think = nullptr;
		self->nextthink = 0_ms;
		return;
	}

	// has dynamic light "target"
	if (self->chain) {
		self->think = target_light_think;
		self->nextthink = level.time + 10_hz;
	} else if (self->spawnflags.has(SPAWNFLAG_TARGET_LIGHT_FLICKER)) {
		self->think = target_light_flicker_think;
		self->nextthink = level.time + 10_hz;
	}
}

void SP_target_light(gentity_t *self) {
	self->s.modelindex = 1;
	self->s.renderfx = RF_CUSTOM_LIGHT;
	self->s.frame = st.radius ? st.radius : 150;
	self->count = self->s.skinnum;
	self->svflags |= SVF_NOCLIENT;
	self->health = 0;

	if (self->target)
		self->chain = G_PickTarget(self->target);

	if (!self->speed)
		self->speed = 1.0f;
	else
		self->speed = 0.1f / self->speed;

	if (level.is_n64)
		self->style += 10;

	if (self->style < 0 || self->style >= static_cast<int32_t>(MAX_LIGHTSTYLES)) {
		gi.Com_PrintFmt("{}: invalid lightstyle {}\n", *self, self->style);
		G_FreeEntity(self);
		return;
	}

	self->use = target_light_use;

	if (self->spawnflags.has(SPAWNFLAG_TARGET_LIGHT_START_ON))
		target_light_use(self, self, self);

	gi.linkentity(self);
}

/*QUAKED target_poi (1 0 0) (-4 -4 -4) (4 4 4) NEAREST DUMMY DYNAMIC x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
[Paril-KEX] point of interest for help in player navigation.
Without any additional setup, targeting this entity will switch
the current POI in the level to the one this is linked to.

"count": if set, this value is the 'stage' linked to this POI. A POI
with this set that is activated will only take effect if the current
level's stage value is <= this value, and if it is, will also set
the current level's stage value to this value.

"style": only used for teamed POIs; the POI with the lowest style will
be activated when checking for which POI to activate. This is mainly
useful during development, to easily insert or change the order of teamed
POIs without needing to manually move the entity definitions around.

"team": if set, this will create a team of POIs. Teamed POIs act like
a single unit; activating any of them will do the same thing. When activated,
it will filter through all of the POIs on the team selecting the one that
best fits the current situation. This includes checking "count" and "style"
values. You can also set the NEAREST spawnflag on any of the teamed POIs,
which will additionally cause activation to prefer the nearest one to the player.
Killing a POI via killtarget will remove it from the chain, allowing you to
adjust valid POIs at runtime.

The DUMMY spawnflag is to allow you to use a single POI as a team member
that can be activated, if you're using killtargets to remove POIs.

The DYNAMIC spawnflag is for very specific circumstances where you want
to direct the player to the nearest teamed POI, but want the path to pick
the nearest at any given time rather than only when activated.

The DISABLED flag is mainly intended to work with DYNAMIC & teams; the POI
will be disabled until it is targeted, and afterwards will be enabled until
it is killed.
*/

constexpr spawnflags_t SPAWNFLAG_POI_NEAREST = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAG_POI_DUMMY = 2_spawnflag;
constexpr spawnflags_t SPAWNFLAG_POI_DYNAMIC = 4_spawnflag;
constexpr spawnflags_t SPAWNFLAG_POI_DISABLED = 8_spawnflag;

static float distance_to_poi(vec3_t start, vec3_t end) {
	PathRequest request;
	request.start = start;
	request.goal = end;
	request.moveDist = 64.f;
	request.pathFlags = PathFlags::All;
	request.nodeSearch.ignoreNodeFlags = true;
	request.nodeSearch.minHeight = 128.0f;
	request.nodeSearch.maxHeight = 128.0f;
	request.nodeSearch.radius = 1024.0f;
	request.pathPoints.count = 0;

	PathInfo info;

	if (gi.GetPathToGoal(request, info))
		return info.pathDistSqr;

	if (info.returnCode == PathReturnCode::NoNavAvailable)
		return (end - start).lengthSquared();

	return std::numeric_limits<float>::infinity();
}

USE(target_poi_use) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	// we were disabled, so remove the disable check
	if (ent->spawnflags.has(SPAWNFLAG_POI_DISABLED))
		ent->spawnflags &= ~SPAWNFLAG_POI_DISABLED;

	// early stage check
	if (ent->count && level.current_poi_stage > ent->count)
		return;

	// teamed POIs work a bit differently
	if (ent->team) {
		gentity_t *poi_master = ent->teammaster;

		// unset ent, since we need to find one that matches
		ent = nullptr;

		float best_distance = std::numeric_limits<float>::infinity();
		int32_t best_style = std::numeric_limits<int32_t>::max();

		gentity_t *dummy_fallback = nullptr;

		for (gentity_t *poi = poi_master; poi; poi = poi->teamchain) {
			// currently disabled
			if (poi->spawnflags.has(SPAWNFLAG_POI_DISABLED))
				continue;

			// ignore dummy POI
			if (poi->spawnflags.has(SPAWNFLAG_POI_DUMMY)) {
				dummy_fallback = poi;
				continue;
			}
			// POI is not part of current stage
			else if (poi->count && level.current_poi_stage > poi->count)
				continue;
			// POI isn't the right style
			else if (poi->style > best_style)
				continue;

			float dist = distance_to_poi(activator->s.origin, poi->s.origin);

			// we have one already and it's farther away, don't bother
			if (poi_master->spawnflags.has(SPAWNFLAG_POI_NEAREST) &&
				ent &&
				dist > best_distance)
				continue;

			// found a better style; overwrite dist
			if (poi->style < best_style) {
				// unless we weren't reachable...
				if (poi_master->spawnflags.has(SPAWNFLAG_POI_NEAREST) && std::isinf(dist))
					continue;

				best_style = poi->style;
				if (poi_master->spawnflags.has(SPAWNFLAG_POI_NEAREST))
					best_distance = dist;
				ent = poi;
				continue;
			}

			// if we're picking by nearest, check distance
			if (poi_master->spawnflags.has(SPAWNFLAG_POI_NEAREST)) {
				if (dist < best_distance) {
					best_distance = dist;
					ent = poi;
					continue;
				}
			} else {
				// not picking by distance, so it's order of appearance
				ent = poi;
			}
		}

		// no valid POI found; this isn't always an error,
		// some valid techniques may require this to happen.
		if (!ent) {
			if (dummy_fallback && dummy_fallback->spawnflags.has(SPAWNFLAG_POI_DYNAMIC))
				ent = dummy_fallback;
			else
				return;
		}

		// copy over POI stage value
		if (ent->count) {
			if (level.current_poi_stage <= ent->count)
				level.current_poi_stage = ent->count;
		}
	} else {
		if (ent->count) {
			if (level.current_poi_stage <= ent->count)
				level.current_poi_stage = ent->count;
			else
				return; // this POI is not part of our current stage
		}
	}

	// dummy POI; not valid
	if (!strcmp(ent->classname, "target_poi") && ent->spawnflags.has(SPAWNFLAG_POI_DUMMY) && !ent->spawnflags.has(SPAWNFLAG_POI_DYNAMIC))
		return;

	level.valid_poi = true;
	level.current_poi = ent->s.origin;
	level.current_poi_image = ent->noise_index;

	if (!strcmp(ent->classname, "target_poi") && ent->spawnflags.has(SPAWNFLAG_POI_DYNAMIC)) {
		level.current_dynamic_poi = nullptr;

		// pick the dummy POI, since it isn't supposed to get freed
		// FIXME maybe store the team string instead?

		for (gentity_t *m = ent->teammaster; m; m = m->teamchain)
			if (m->spawnflags.has(SPAWNFLAG_POI_DUMMY)) {
				level.current_dynamic_poi = m;
				break;
			}

		if (!level.current_dynamic_poi)
			gi.Com_PrintFmt("can't activate poi for {}; need DUMMY in chain\n", *ent);
	} else
		level.current_dynamic_poi = nullptr;
}

static THINK(target_poi_setup) (gentity_t *self) -> void {
	if (self->team) {
		// copy dynamic/nearest over to all teammates
		if (self->spawnflags.has((SPAWNFLAG_POI_NEAREST | SPAWNFLAG_POI_DYNAMIC)))
			for (gentity_t *m = self->teammaster; m; m = m->teamchain)
				m->spawnflags |= self->spawnflags & (SPAWNFLAG_POI_NEAREST | SPAWNFLAG_POI_DYNAMIC);

		for (gentity_t *m = self->teammaster; m; m = m->teamchain) {
			if (strcmp(m->classname, "target_poi"))
				gi.Com_PrintFmt("WARNING: {} is teamed with target_poi's; unintentional\n", *m);
		}
	}
}

void SP_target_poi(gentity_t *self) {
	if (deathmatch->integer) { // auto-remove for deathmatch
		G_FreeEntity(self);
		return;
	}

	if (st.image)
		self->noise_index = gi.imageindex(st.image);
	else
		self->noise_index = gi.imageindex("friend");

	self->use = target_poi_use;
	self->svflags |= SVF_NOCLIENT;
	self->think = target_poi_setup;
	self->nextthink = level.time + 1_ms;

	if (!self->team) {
		if (self->spawnflags.has(SPAWNFLAG_POI_NEAREST))
			gi.Com_PrintFmt("{} has useless spawnflag 'NEAREST'\n", *self);
		if (self->spawnflags.has(SPAWNFLAG_POI_DYNAMIC))
			gi.Com_PrintFmt("{} has useless spawnflag 'DYNAMIC'\n", *self);
	}
}

/*QUAKED target_music (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Change music when used
"sounds" set music track number to change to
*/

static USE(use_target_music) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	gi.configstring(CS_CDTRACK, G_Fmt("{}", ent->sounds).data());
}

void SP_target_music(gentity_t *self) {
	self->use = use_target_music;
}

/*QUAKED target_healthbar (0 1 0) (-8 -8 -8) (8 8 8) PVS_ONLY x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Hook up health bars to monsters.
"delay" is how long to show the health bar for after death.
"message" is their name
*/

static USE(use_target_healthbar) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	gentity_t *target = G_PickTarget(ent->target);

	if (!target || ent->health != target->spawn_count) {
		if (target)
			gi.Com_PrintFmt("{}: target {} changed from what it used to be\n", *ent, *target);
		else
			gi.Com_PrintFmt("{}: no target\n", *ent);
		G_FreeEntity(ent);
		return;
	}

	for (size_t i = 0; i < MAX_HEALTH_BARS; i++) {
		if (level.health_bar_entities[i])
			continue;

		ent->enemy = target;
		level.health_bar_entities[i] = ent;
		gi.configstring(CONFIG_HEALTH_BAR_NAME, ent->message);
		return;
	}

	gi.Com_PrintFmt("{}: too many health bars\n", *ent);
	G_FreeEntity(ent);
}

static THINK(check_target_healthbar) (gentity_t *ent) -> void {
	gentity_t *target = G_PickTarget(ent->target);
	if (!target || !(target->svflags & SVF_MONSTER)) {
		if (target != nullptr) {
			gi.Com_PrintFmt("{}: target {} does not appear to be a monster\n", *ent, *target);
		}
		G_FreeEntity(ent);
		return;
	}

	// just for sanity check
	ent->health = target->spawn_count;
}

void SP_target_healthbar(gentity_t *self) {
	if (deathmatch->integer) {
		G_FreeEntity(self);
		return;
	}

	if (!self->target || !*self->target) {
		gi.Com_PrintFmt("{}: missing target\n", *self);
		G_FreeEntity(self);
		return;
	}

	if (!self->message) {
		gi.Com_PrintFmt("{}: missing message\n", *self);
		G_FreeEntity(self);
		return;
	}

	self->use = use_target_healthbar;
	self->think = check_target_healthbar;
	self->nextthink = level.time + 25_ms;
}

/*QUAKED target_autosave (0 1 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Auto save on command.
*/

static USE(use_target_autosave) (gentity_t *ent, gentity_t *other, gentity_t *activator) -> void {
	gtime_t save_time = gtime_t::from_sec(gi.cvar("g_athena_auto_save_min_time", "60", CVAR_NOSET)->value);

	if (level.time - level.next_auto_save > save_time) {
		gi.AddCommandString("autosave\n");
		level.next_auto_save = level.time;
	}
}

void SP_target_autosave(gentity_t *self) {
	if (deathmatch->integer) {
		G_FreeEntity(self);
		return;
	}

	self->use = use_target_autosave;
}

/*QUAKED target_sky (0 1 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Change sky parameters.
"sky"	environment map name
"skyaxis"	vector axis for rotating sky
"skyrotate"	speed of rotation in degrees/second
*/

static USE(use_target_sky) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	if (self->map)
		gi.configstring(CS_SKY, self->map);

	if (self->count & 3) {
		float rotate = 0.0f;
		int32_t autorotate = 1;

		const char *const skyrotate = gi.get_configstring(CS_SKYROTATE);
		if (!skyrotate || sscanf(skyrotate, "%f %i", &rotate, &autorotate) != 2) {
			// [MuffMode] A malformed engine configstring must not leave either
			// preserved value uninitialized.
			rotate = 0.0f;
			autorotate = 1;
		}

		if (self->count & 1)
			rotate = self->accel;

		if (self->count & 2)
			autorotate = self->style;

		gi.configstring(CS_SKYROTATE, G_Fmt("{} {}", rotate, autorotate).data());
	}

	if (self->count & 4)
		gi.configstring(CS_SKYAXIS, G_Fmt("{}", self->movedir).data());
}

void SP_target_sky(gentity_t *self) {
	self->use = use_target_sky;
	if (st.was_key_specified("sky"))
		self->map = st.sky;
	if (st.was_key_specified("skyaxis")) {
		self->count |= 4;
		self->movedir = st.skyaxis;
	}
	if (st.was_key_specified("skyrotate")) {
		self->count |= 1;
		self->accel = st.skyrotate;
	}
	if (st.was_key_specified("skyautorotate")) {
		self->count |= 2;
		self->style = st.skyautorotate;
	}
}

//==========================================================

/*QUAKED target_crossunit_trigger (.5 .5 .5) (-8 -8 -8) (8 8 8) TRIGGER1 TRIGGER2 TRIGGER3 TRIGGER4 TRIGGER5 TRIGGER6 TRIGGER7 TRIGGER8
Once this trigger is touched/used, any trigger_crossunit_target with the same trigger number is automatically used when a level is started within the same unit.  It is OK to check multiple triggers.  Message, delay, target, and killtarget also work.
*/
static USE(trigger_crossunit_trigger_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	game.cross_unit_flags |= self->spawnflags.value;
	G_FreeEntity(self);
}

void SP_target_crossunit_trigger(gentity_t *self) {
	if (deathmatch->integer) {
		G_FreeEntity(self);
		return;
	}

	self->svflags = SVF_NOCLIENT;
	self->use = trigger_crossunit_trigger_use;
}

/*QUAKED target_crossunit_target (.5 .5 .5) (-8 -8 -8) (8 8 8) TRIGGER1 TRIGGER2 TRIGGER3 TRIGGER4 TRIGGER5 TRIGGER6 TRIGGER7 TRIGGER8 - - - - - - - - TRIGGER9 TRIGGER10 TRIGGER11 TRIGGER12 TRIGGER13 TRIGGER14 TRIGGER15 TRIGGER16
Triggered by a trigger_crossunit elsewhere within a unit.
If multiple triggers are checked, all must be true. Delay, target and killtarget also work.

"delay"		delay before using targets if the trigger has been activated (default 1)
*/
static THINK(target_crossunit_target_think) (gentity_t *self) -> void {
	if (self->spawnflags.value == (game.cross_unit_flags & SFL_CROSS_TRIGGER_MASK & self->spawnflags.value)) {
		if (G_UseTargets(self, self))
			G_FreeEntity(self);
	}
}

void SP_target_crossunit_target(gentity_t *self) {
	if (deathmatch->integer) {
		G_FreeEntity(self);
		return;
	}

	if (!self->delay)
		self->delay = 1;
	self->svflags = SVF_NOCLIENT;

	self->think = target_crossunit_target_think;
	self->nextthink = level.time + gtime_t::from_sec(self->delay);
}

/*QUAKED target_achievement (.5 .5 .5) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Give an achievement.

"achievement"		cheevo to give
*/
static USE(use_target_achievement) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	gi.WriteByte(svc_achievement);
	gi.WriteString(self->map);
	gi.multicast(vec3_origin, MULTICAST_ALL, true);
}

void SP_target_achievement(gentity_t *self) {
	if (deathmatch->integer) {
		G_FreeEntity(self);
		return;
	}

	self->map = st.achievement;
	self->use = use_target_achievement;
}

static USE(use_target_story) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	level.story_active = !!(self->message && *self->message);
	gi.configstring(CONFIG_STORY_SCORELIMIT, self->message ? self->message : "");
}

void SP_target_story(gentity_t *self) {
	if (deathmatch->integer) {
		G_FreeEntity(self);
		return;
	}

	self->use = use_target_story;
}

/*QUAKED target_mal_laser (1 0 0) (-4 -4 -4) (4 4 4) START_ON RED GREEN BLUE YELLOW ORANGE FAT x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Mal's laser
*/
static void target_mal_laser_on(gentity_t *self) {
	if (!self->activator)
		self->activator = self;
	self->spawnflags |= SPAWNFLAG_LASER_ZAP | SPAWNFLAG_LASER_ON;
	self->svflags &= ~SVF_NOCLIENT;
	self->flags |= FL_TRAP;
	// target_laser_think (self);
	self->nextthink = level.time + gtime_t::from_sec(self->wait + self->delay);
}

static USE(target_mal_laser_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	self->activator = activator;
	if (self->spawnflags.has(SPAWNFLAG_LASER_ON))
		target_laser_off(self);
	else
		target_mal_laser_on(self);
}

void mal_laser_think(gentity_t *self);

static THINK(mal_laser_think2) (gentity_t *self) -> void {
	self->svflags |= SVF_NOCLIENT;
	self->think = mal_laser_think;
	self->nextthink = level.time + gtime_t::from_sec(self->wait);
	self->spawnflags |= SPAWNFLAG_LASER_ZAP;
}

THINK(mal_laser_think) (gentity_t *self) -> void {
	self->svflags &= ~SVF_NOCLIENT;
	target_laser_think(self);
	self->think = mal_laser_think2;
	self->nextthink = level.time + 100_ms;
}

void SP_target_mal_laser(gentity_t *self) {
	self->movetype = MOVETYPE_NONE;
	self->solid = SOLID_NOT;
	self->s.renderfx |= RF_BEAM;
	self->s.modelindex = MODELINDEX_WORLD; // must be non-zero
	self->flags |= FL_TRAP_LASER_FIELD;

	// set the beam diameter
	if (self->spawnflags.has(SPAWNFLAG_LASER_FAT))
		self->s.frame = 16;
	else
		self->s.frame = 4;

	// set the color
	if (self->spawnflags.has(SPAWNFLAG_LASER_RED))
		self->s.skinnum = 0xf2f2f0f0;
	else if (self->spawnflags.has(SPAWNFLAG_LASER_GREEN))
		self->s.skinnum = 0xd0d1d2d3;
	else if (self->spawnflags.has(SPAWNFLAG_LASER_BLUE))
		self->s.skinnum = 0xf3f3f1f1;
	else if (self->spawnflags.has(SPAWNFLAG_LASER_YELLOW))
		self->s.skinnum = 0xdcdddedf;
	else if (self->spawnflags.has(SPAWNFLAG_LASER_ORANGE))
		self->s.skinnum = 0xe0e1e2e3;

	G_SetMovedir(self->s.angles, self->movedir);

	if (!self->delay)
		self->delay = 0.1f;

	if (!self->wait)
		self->wait = 0.1f;

	if (!self->dmg)
		self->dmg = 5;

	self->mins = { -8, -8, -8 };
	self->maxs = { 8, 8, 8 };

	self->nextthink = level.time + gtime_t::from_sec(self->delay);
	self->think = mal_laser_think;

	self->use = target_mal_laser_use;

	gi.linkentity(self);

	if (self->spawnflags.has(SPAWNFLAG_LASER_ON))
		target_mal_laser_on(self);
	else
		target_laser_off(self);
}


//==========================================================

/*QUAKED target_steam (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Creates a steam effect (particles w/ velocity in a line).

speed = velocity of particles (default 50)
count = number of particles (default 32)
sounds = color of particles (default 8 for steam)
	the color range is from this color to this color + 6
wait = seconds to run before stopping (overrides default
	value derived from func_timer)

best way to use this is to tie it to a func_timer that "pokes"
it every second (or however long you set the wait time, above)

note that the width of the base is proportional to the speed
good colors to use:
6-9 - varying whites (darker to brighter)
224 - sparks
176 - blue water
80  - brown water
208 - slime
232 - blood
*/

static USE(use_target_steam) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	// FIXME - this needs to be a global
	static int nextid;
	vec3_t	   point;

	if (nextid > 20000)
		nextid = nextid % 20000;

	nextid++;

	// automagically set wait from func_timer unless they set it already, or
	// default to 1000 if not called by a func_timer (eek!)
	if (!self->wait) {
		if (other)
			self->wait = other->wait * 1000;
		else
			self->wait = 1000;
	}

	if (self->enemy) {
		point = (self->enemy->absmin + self->enemy->absmax) * 0.5f;
		self->movedir = point - self->s.origin;
		self->movedir.normalize();
	}

	point = self->s.origin + (self->movedir * (self->style * 0.5f));
	if (self->wait > 100) {
		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_STEAM);
		gi.WriteShort(nextid);
		gi.WriteByte(self->count);
		gi.WritePosition(self->s.origin);
		gi.WriteDir(self->movedir);
		gi.WriteByte(self->sounds & 0xff);
		gi.WriteShort((short int)(self->style));
		gi.WriteLong((int)(self->wait));
		gi.multicast(self->s.origin, MULTICAST_PVS, false);
	} else {
		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_STEAM);
		gi.WriteShort((short int)-1);
		gi.WriteByte(self->count);
		gi.WritePosition(self->s.origin);
		gi.WriteDir(self->movedir);
		gi.WriteByte(self->sounds & 0xff);
		gi.WriteShort((short int)(self->style));
		gi.multicast(self->s.origin, MULTICAST_PVS, false);
	}
}

static THINK(target_steam_start) (gentity_t *self) -> void {
	gentity_t *ent;

	self->use = use_target_steam;

	if (self->target) {
		ent = G_FindByString<&gentity_t::targetname>(nullptr, self->target);
		if (!ent)
			gi.Com_PrintFmt("{}: target {} not found\n", *self, self->target);
		self->enemy = ent;
	} else {
		G_SetMovedir(self->s.angles, self->movedir);
	}

	if (!self->count)
		self->count = 32;
	if (!self->style)
		self->style = 75;
	if (!self->sounds)
		self->sounds = 8;
	if (self->wait)
		self->wait *= 1000; // we want it in milliseconds, not seconds

	// paranoia is good
	self->sounds &= 0xff;
	self->count &= 0xff;

	self->svflags = SVF_NOCLIENT;

	gi.linkentity(self);
}

void SP_target_steam(gentity_t *self) {
	self->style = (int)self->speed;

	if (self->target) {
		self->think = target_steam_start;
		self->nextthink = level.time + 1_sec;
	} else
		target_steam_start(self);
}

//==========================================================
// target_anger
//==========================================================

static USE(target_anger_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	gentity_t *target;
	gentity_t *t;

	t = nullptr;
	target = G_FindByString<&gentity_t::targetname>(t, self->killtarget);

	if (target && self->target) {
		// Make whatever a "good guy" so the monster will try to kill it!
		if (!(target->svflags & SVF_MONSTER)) {
			target->monsterinfo.aiflags |= AI_GOOD_GUY | AI_DO_NOT_COUNT;
			target->svflags |= SVF_MONSTER;
			target->health = 300;
		}

		t = nullptr;
		while ((t = G_FindByString<&gentity_t::targetname>(t, self->target))) {
			if (t == self) {
				gi.Com_Print("WARNING: entity used itself.\n");
			} else {
				if (t->use) {
					if (t->health <= 0)
						return;

					t->enemy = target;
					t->monsterinfo.aiflags |= AI_TARGET_ANGER;
					FoundTarget(t);
				}
			}
			if (!self->inuse) {
				gi.Com_Print("entity was removed while using targets\n");
				return;
			}
		}
	}
}

/*QUAKED target_anger (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
This trigger will cause an entity to be angry at another entity when a player touches it. Target the
entity you want to anger, and killtarget the entity you want it to be angry at.

target - entity to piss off
killtarget - entity to be pissed off at
*/
void SP_target_anger(gentity_t *self) {
	if (!self->target) {
		gi.Com_Print("target_anger without target!\n");
		G_FreeEntity(self);
		return;
	}
	if (!self->killtarget) {
		gi.Com_Print("target_anger without killtarget!\n");
		G_FreeEntity(self);
		return;
	}

	self->use = target_anger_use;
	self->svflags = SVF_NOCLIENT;
}

// ***********************************
// target_killplayers
// ***********************************

USE(target_killplayers_use) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	gentity_t *ent;
	level.deadly_kill_box = true;

	// kill any visible monsters
	gentity_t *end = &g_entities[TargetEntityLimit()];
	for (ent = g_entities; ent < end; ent++) {
		if (!ent->inuse)
			continue;
		if (ent->health < 1)
			continue;
		if (!ent->takedamage)
			continue;

		for (auto ce : active_clients()) {
			if (gi.inPVS(ce->s.origin, ent->s.origin, false)) {
				T_Damage(ent, self, self, vec3_origin, ent->s.origin, vec3_origin,
					ent->health, 0, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);
				break;
			}
		}
	}

	// kill the players
	for (auto ce : active_clients())
		T_Damage(ce, self, self, vec3_origin, self->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);

	level.deadly_kill_box = false;
}

/*QUAKED target_killplayers (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
When triggered, this will kill all the players on the map.
*/
void SP_target_killplayers(gentity_t *self) {
	self->use = target_killplayers_use;
	self->svflags = SVF_NOCLIENT;
}

/*QUAKED target_blacklight (1 0 1) (-16 -16 -24) (16 16 24) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Pulsing black light with sphere in the center
*/
static THINK(blacklight_think) (gentity_t *self) -> void {
	self->s.angles[PITCH] += frandom(10);
	self->s.angles[YAW] += frandom(10);
	self->s.angles[ROLL] += frandom(10);
	self->nextthink = level.time + FRAME_TIME_MS;
}

void SP_target_blacklight(gentity_t *ent) {
	if (deathmatch->integer) { // auto-remove for deathmatch
		G_FreeEntity(ent);
		return;
	}

	ent->mins = {};
	ent->maxs = {};

	ent->s.effects |= (EF_TRACKERTRAIL | EF_TRACKER);
	ent->think = blacklight_think;
	ent->s.modelindex = gi.modelindex("models/items/spawngro3/tris.md2");
	ent->s.scale = 6.f;
	ent->s.skinnum = 0;
	ent->nextthink = level.time + FRAME_TIME_MS;
	gi.linkentity(ent);
}

/*QUAKED target_orb (1 0 1) (-16 -16 -24) (16 16 24) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Translucent pulsing orb with speckles
*/
void SP_target_orb(gentity_t *ent) {
	if (deathmatch->integer) { // auto-remove for deathmatch
		G_FreeEntity(ent);
		return;
	}

	ent->mins = {};
	ent->maxs = {};

	//	ent->s.effects |= EF_TRACKERTRAIL;
	ent->think = blacklight_think;
	ent->nextthink = level.time + 10_hz;
	ent->s.skinnum = 1;
	ent->s.modelindex = gi.modelindex("models/items/spawngro3/tris.md2");
	ent->s.frame = 2;
	ent->s.scale = 8.f;
	ent->s.effects |= EF_SPHERETRANS;
	gi.linkentity(ent);
}

//==========================================================
