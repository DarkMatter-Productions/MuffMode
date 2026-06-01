// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "g_local.h"
#include <string>

// [MuffMode] Vote state machine implementation hooks.
void MM_TransitionVoteState(VoteState new_state);
void MM_ClearVote();
void MM_BeginVoteValidationContext(vcmds_t *cc, const char *arg);
void MM_EndVoteValidationContext();
void MM_VotePassed();
void MM_VotePassGametype();
bool MM_VoteValGametype(gentity_t *ent);
void MM_VotePassRuleset();
bool MM_VoteValRuleset(gentity_t *ent);
void MM_VotePassMap();
bool MM_VoteValMap(gentity_t *ent);
bool MM_IsMapValid(const char *mapname);
void MM_VotePassRestartMatch();
void MM_VotePassNextMap();
bool MM_VoteValRandom(gentity_t *ent);
void MM_VotePassCointoss();
void MM_VotePassRandom();
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
void MM_VotePassHandicap();
bool MM_VoteValHandicap(gentity_t *ent);
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

