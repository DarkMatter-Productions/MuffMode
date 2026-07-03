// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;

enum class mm_announce_event_t : uint16_t {
	VoteFailed,
	VotePassed,
	VoteNow,
	RoundBeginsIn,
	FightWithBackup,
	BlueWinsRound,
	RedWinsRound,
	RoundWon,
	PrepareYourTeam,
	PrepareToFight,
	Overtime,
	SuddenDeath,
	One,
	Two,
	Three,
	FiveMinute,
	OneMinute,
	OneFrag,
	TwoFrags,
	ThreeFrags,
	LeadTaken,
	LeadTied,
	LeadLost,
	BlueLeads,
	RedLeads,
	TeamsTied,
	BlueScores,
	RedScores,
	RedWins,
	BlueWins,
	YouWin,
	YouLose,
	Rampage1,
	FirstExcellent,
	Excellent1,
	Humiliation1,
	QuadDamage,
	Haste,
	BattleSuit,
	Regeneration,
	Invisibility,
	// Phase 6+ reserved stems (assets optional; hooks added incrementally).
	FirstBlood,
	DoubleKill,
	TripleKill,
	FlagCaptured,
};

void MM_Announce(mm_announce_event_t event, gentity_t *focus);
void MM_AnnounceRaw(gentity_t *focus, const char *stem, const char *backup_sound, bool use_backup, bool backup_alongside_vo = false);

void MM_Announcer_OnMatchReset();
void MM_Announcer_OnPlayerFrag(gentity_t *attacker, gentity_t *victim);
