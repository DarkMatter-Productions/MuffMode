// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <string>

enum gametype_t;
enum ruleset_t;
enum class VoteState;
struct gclient_t;
struct gentity_t;

// [MuffMode] Vote state machine implementation hooks.
void MM_TransitionVoteState(VoteState new_state);
void MM_ClearVote();
void MM_VotePassed();
void MM_VotePassGametype();
void MM_VotePassRuleset();
void MM_VotePassUnlagged();
bool MM_VoteValUnlagged(gentity_t *ent);
void MM_VotePassTimelimit();
bool MM_VoteValTimelimit(gentity_t *ent);
void MM_VotePassScorelimit();
bool MM_VoteValScorelimit(gentity_t *ent);
void MM_VotePassPowerups();
bool MM_VoteValPowerups(gentity_t *ent);
void MM_VotePassFriendlyFire();
bool MM_VoteValFriendlyFire(gentity_t *ent);
void MM_VotePassShuffleTeams();
bool MM_VoteValShuffleTeams(gentity_t *ent);
void MM_VotePassBalanceTeams();
bool MM_VoteValBalanceTeams(gentity_t *ent);
void MM_VotePassReadyAll();
bool MM_VoteValReadyAll(gentity_t *ent);
bool MM_IsGametypeVotable(gametype_t gt);
bool MM_IsRulesetVotable(ruleset_t rs);
std::string MM_GetVotableGametypesList();
std::string MM_GetVotableRulesetsList();
bool MM_ValidVoteCommand(gentity_t *ent);
void MM_VoteCommandStore(gentity_t *ent);
void MM_CmdCallVote(gentity_t *ent);
void MM_CmdVote(gentity_t *ent);
void MM_RevertVote(gclient_t *client);

