// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstdint>

struct gentity_t;

// [MuffMode] Vote command table entry. Registry lives in mm_vote.cpp.
struct vcmds_t {
	const		char *name;
	bool		(*val_func)(gentity_t *ent);
	void		(*func)();
	int32_t		flag;
	int8_t		min_args;
	const		char *args;
	const		char *help;
};
extern vcmds_t vote_cmds[];

enum class VoteState {
	IDLE,			// No vote in progress
	ACTIVE,			// Vote is being voted on
	PASSED,			// Vote passed, waiting to execute
	EXECUTING,		// Currently executing the vote command
	FAILED,			// Vote failed or timed out
	COMPLETE		// Vote executed successfully
};
