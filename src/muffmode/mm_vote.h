// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <string>

enum gametype_t;
enum ruleset_t;
enum class VoteState;
struct gentity_t;

// [MuffMode] Vote state machine implementation hooks.
void MM_TransitionVoteState(VoteState new_state);
void MM_ClearVote();
void MM_VotePassed();
void MM_VotePassGametype();
void MM_VotePassRuleset();
bool MM_IsGametypeVotable(gametype_t gt);
bool MM_IsRulesetVotable(ruleset_t rs);
std::string MM_GetVotableGametypesList();
std::string MM_GetVotableRulesetsList();
bool MM_ValidVoteCommand(gentity_t *ent);

