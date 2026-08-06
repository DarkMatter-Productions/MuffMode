// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "mm_awards_rules.h"

#include <string>
#include <vector>

struct gentity_t;

// [MuffMode] Post-match awards. The end of a ranked match reads as three screens
// in a row: the scoreboard, then the awards reel, then the next-map pick, and
// only then does the level change. The reel replaces the scoreboard with an
// award name in green and the player who earned it in white underneath.
//
// Each screen hands on the same way the one before it did -- a minimum hold so
// nobody presses straight through it, then any key, with a timeout so an
// unattended server still moves. The reel follows the next-map pick's contract
// rather than inventing a second one: it is offered the exit by every
// intermission path that was about to call ExitLevel(), holds the intermission
// while it plays, and then offers that same exit on to the pick.
//
// Only the presentation and the stage machine live in the .cpp. Which titles
// exist and who wins them is pure arithmetic in mm_awards_rules.h, and the
// arithmetic itself is driven from mm_match_stats, which owns the frozen
// participant snapshots the reel is computed from.

// One decided award, resolved to a named player. Names are captured at freeze
// time so a reel entry survives the winner disconnecting before it is shown.
struct mm_award_entry_t {
	mm_match_award_t award = mm_match_award_t::none;
	std::string player_name;
	std::string social_id;
	uint32_t value = 0;
};

// Registers g_match_awards and g_match_awards_time.
void MM_Awards_RegisterCvars();

// Drops the published reel and any in-flight display. Called wherever a match or
// a level is torn down, since the reel keeps its state module-side.
void MM_Awards_Reset();

// Publishes the decided reel. mm_match_stats owns the participant snapshots, so
// it does the selection and hands the finished list over. `ranked` records
// whether the match met the ranked bar; an unranked match still exports its
// awards but never shows the reel.
void MM_Awards_Publish(std::vector<mm_award_entry_t> entries, bool ranked);

// The published reel, in display order. Empty until a match has ended, and kept
// after the level changes so `awards` can still answer.
const std::vector<mm_award_entry_t> &MM_Awards_Entries();
bool MM_Awards_MatchWasRanked();

// True only between Publish and the next Reset, i.e. while the published reel
// belongs to the match that is settling right now. Career award tallies are
// permanent, so the profile write must not be able to credit a reel left over
// from an earlier match on the same level.
bool MM_Awards_BelongsToSettlingMatch();

// True while the reel owns the intermission, from the moment it opens until it
// calls ExitLevel(). Vanilla intermission handling defers to it in that window.
bool MM_Awards_Active();

// Offered the exit by an intermission path that was about to call ExitLevel().
// Returns true when the reel has taken it over, false to exit as normal.
bool MM_Awards_ClaimIntermissionExit();

// True once an open reel has been up long enough to accept a key press. Client
// input consults this so the press that dismissed the scoreboard cannot also
// dismiss the reel that replaced it.
bool MM_Awards_AcceptsSkip();

// Advances an open reel, and once it has run its course hands the intermission
// exit on to the next-map pick, or takes it itself when there is no pick.
void MM_Awards_RunFrame();

// Sends ent the reel in place of the intermission scoreboard, on the reel's own
// refresh cadence. Returns true when the reel owned the client's layout this
// frame, in which case the caller must not send the scoreboard over the top.
bool MM_Awards_DrawIntermissionLayout(gentity_t *ent);

// Player command body: `awards`. Prints the last match's reel to the console,
// which is the only way to read it back once the level has changed.
void MM_CmdAwards(gentity_t *ent);
