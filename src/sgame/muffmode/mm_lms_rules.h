// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

// Pure, host-testable round-end predicates for Last Man Standing (GT_LMS).
//
// LMS is free-for-all elimination: an "active fighter" is a playing client who is
// not eliminated and either still alive or still holding a life (Horde semantics).
// A round needs at least two participants so a lone player (everyone else
// disconnected) never trips an instant round-end - mirrors the Red Rover guard.

// The round resolves to a single winner: exactly one active fighter remains.
inline bool MM_LMSRoundHasWinner(int active_fighters, int participants) {
	return participants >= 2 && active_fighters == 1;
}

// The round resolves to a draw: every fighter was eliminated (mutual kill / lava).
inline bool MM_LMSRoundIsDraw(int active_fighters, int participants) {
	return participants >= 2 && active_fighters == 0;
}
