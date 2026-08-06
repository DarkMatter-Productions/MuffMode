// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_announcer.h"
#include "muffmode/mm_announcer_rules.h"

#include <iterator>

namespace {

struct mm_announce_def_t {
	const char *stem;
	const char *backup_sound;
	bool        use_backup;
	bool        backup_alongside_vo = false;
};

static bool s_first_blood_announced = false;

constexpr mm_announce_def_t k_announce_defs[] = {
	{ "vote_failed", nullptr, false },
	{ "vote_passed", nullptr, false },
	{ "vote_now", "misc/pc_up.wav", true },
	{ "round_begins_in", nullptr, false },
	{ "fight", "misc/tele_up.wav", true, true },
	{ "blue_wins_round", "ctf/flagcap.wav", true },
	{ "red_wins_round", "ctf/flagcap.wav", false },
	{ "round_won", "ctf/flagcap.wav", true },
	{ "prepare_your_team", nullptr, false },
	{ "prepare_to_fight", nullptr, false },
	{ "overtime", "world/klaxon2.wav", true },
	{ "sudden_death", "world/klaxon2.wav", true },
	{ "one", nullptr, false },
	{ "two", nullptr, false },
	{ "three", nullptr, false },
	{ "5_minute", nullptr, false },
	{ "1_minute", nullptr, false },
	{ "1_frag", nullptr, false },
	{ "2_frags", nullptr, false },
	{ "3_frags", nullptr, false },
	{ "lead_taken", nullptr, false },
	{ "lead_tied", nullptr, false },
	{ "lead_lost", nullptr, false },
	{ "blue_leads", nullptr, false },
	{ "red_leads", nullptr, false },
	{ "teams_tied", nullptr, false },
	{ "blue_scores", nullptr, false },
	{ "red_scores", nullptr, false },
	{ "red_wins", nullptr, false },
	{ "blue_wins", nullptr, false },
	{ "you_win", nullptr, false },
	{ "you_lose", nullptr, false },
	{ "rampage1", nullptr, false },
	{ "first_excellent", nullptr, false },
	{ "excellent1", nullptr, false },
	{ "humiliation1", nullptr, false },
	{ "quad_damage", nullptr, false },
	{ "haste", nullptr, false },
	{ "battlesuit", nullptr, false },
	{ "regeneration", nullptr, false },
	{ "invisibility", nullptr, false },
	{ "first_blood", nullptr, false },
	{ "double_kill", nullptr, false },
	{ "triple_kill", nullptr, false },
	{ "flag_captured", "ctf/flagcap.wav", true },
};

static_assert(std::size(k_announce_defs) == static_cast<size_t>(mm_announce_event_t::FlagCaptured) + 1,
	"mm_announce_event_t table mismatch");

} // namespace

void MM_AnnounceRaw(gentity_t *focus, const char *stem, const char *backup_sound, bool use_backup, bool backup_alongside_vo)
{
	AnnouncerSound(focus, stem, backup_sound, use_backup, backup_alongside_vo);
}

void MM_Announce(mm_announce_event_t event, gentity_t *focus)
{
	const auto index = static_cast<size_t>(event);
	if (index >= std::size(k_announce_defs))
		return;

	const mm_announce_def_t &def = k_announce_defs[index];
	MM_AnnounceRaw(focus, def.stem, def.backup_sound, def.use_backup, def.backup_alongside_vo);
}

void MM_AnnounceRaw_ToClient(gentity_t *listener, const char *stem, const char *backup_sound, bool use_backup, bool backup_alongside_vo)
{
	if (!listener || !listener->inuse || !listener->client || !listener->client->pers.connected)
		return;
	if (listener->client->sess.is_a_bot || (listener->svflags & SVF_BOT))
		return;

	const bool has_stem = stem && stem[0];
	const bool has_backup = backup_sound && backup_sound[0];
	const announce_action_t action = MM_AnnounceDecision(
		listener->client->sess.pc.announcer_enabled, has_stem, use_backup, has_backup, backup_alongside_vo);

	// Channel flags mirror AnnouncerSound exactly so a room announcement is indistinguishable
	// from a level-wide one on the client.
	if (action.play_sting)
		gi.local_sound(listener, CHAN_RELIABLE | CHAN_AUTO, gi.soundindex(backup_sound), 1, ATTN_NONE, 0);
	if (action.play_backup)
		gi.local_sound(listener, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex(backup_sound), 1, ATTN_NONE, 0);
	if (action.play_vo)
		gi.local_sound(listener, CHAN_RELIABLE | CHAN_NO_PHS_ADD | CHAN_AUX, gi.soundindex(G_Fmt("vo_evil/{}.wav", stem).data()), 1, ATTN_NONE, 0);
}

void MM_Announce_ToClient(gentity_t *listener, mm_announce_event_t event)
{
	const auto index = static_cast<size_t>(event);
	if (index >= std::size(k_announce_defs))
		return;

	const mm_announce_def_t &def = k_announce_defs[index];
	MM_AnnounceRaw_ToClient(listener, def.stem, def.backup_sound, def.use_backup, def.backup_alongside_vo);
}

void MM_Announcer_OnMatchReset()
{
	s_first_blood_announced = false;
}

void MM_Announcer_OnPlayerFrag(gentity_t *attacker, gentity_t *victim)
{
	if (!attacker || !victim || attacker == victim)
		return;
	if (!attacker->client || !victim->client)
		return;
	if (level.match_state != match_state_t::MATCH_IN_PROGRESS)
		return;
	if (s_first_blood_announced)
		return;

	s_first_blood_announced = true;
	MM_Announce(mm_announce_event_t::FirstBlood, world);
}
