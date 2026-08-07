// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

// [MuffMode] The one resolved view of "what gametype is in force right now".
//
// GT(), GTF() and notGT() used to expand, at every one of their ~630 call
// sites, into a cvar dereference, a clamp, an arena-fallback branch and a
// cross-translation-unit call to MM_Arena_Active(). T_Damage alone expands them
// 23 times per damage event; G_RunEntity three times per entity per frame.
// Publishing the resolution once and reading it everywhere makes each of those
// a single load of DLL-local data, and -- more importantly -- makes it
// impossible for two call sites in the same frame to disagree.
//
// The cache is constant-initialised, so it holds effective Deathmatch before
// PreInitGame has registered g_gametype. That is exactly what the old
// `g_gametype ? ... : GT_FFA` null guard answered, with no lazy-init branch on
// the hot path.

#include "muffmode/mm_gametype.h"
#include "muffmode/mm_gametype_table.h"

#include <cstdint>

namespace muffmode {
namespace gametype {

struct mm_gt_live_t {
	int			configured;					// availability-sanitised, in range
	int			effective;					// configured, with Arena failed closed
	int			flags;						// MM_GT_TABLE[effective].flags
	bool		arena_active;
	int32_t		gametype_modified_count;	// -1 when never observed
};

// Defined in mm_gt_session.cpp.
extern mm_gt_live_t g_gt_live;

// Recompute from the live cvar and arena activation. The only writer.
void MM_GT_PublishLive();

// Two integer compares; republishes only on drift. Runs once per frame from
// G_RunFrame_, above the timeout gate, so a publish site missed elsewhere
// degrades to at most one frame of staleness rather than permanent divergence.
// Note that G_RunFrame skips the whole frame body when no client is spawned, so
// this is a safety net for a running server, not a guarantee for an idle one --
// the explicit publish sites are what make it correct.
void MM_GT_ValidateLive();

#ifdef _DEBUG
// Recompute independently and complain on mismatch, so a missed publish site is
// a loud development failure rather than a silent one. Compiled out in Release.
void MM_GT_AssertLivePublished();
#endif

} // namespace gametype
} // namespace muffmode

// [MuffMode] Selecting MuffMode Arena is not enough to activate its gameplay.
// Until the current entity lump passes the RA2 map contract, all normal
// gameplay queries see Deathmatch, while registration and configuration code
// can use GT_RAW to inspect the requested cvar value.
//
// This is the only definition. The previous out-of-line body in mm_arena.cpp
// had two separate declarations (g_local.h and mm_arena.h) that had to be kept
// in sync by hand.
inline bool MM_Arena_Active()
{
	return muffmode::gametype::g_gt_live.arena_active;
}
