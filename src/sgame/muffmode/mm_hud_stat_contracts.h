// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "shared/gameplay.h"

#include <cstddef>
#include <cstdint>

// Server <-> client HUD stat contract (ps.stats[] slots consumed by CS_STATUSBAR layout).
//
// STAT_GAMETYPE_HUD      — warmup (not countdown): "Gametype: …"; in-progress: Fraglimit / Capturelimit(CTF) / Round x of y (CA, RR, Strike, LMS) / Wave x of y (Horde)
// STAT_RULESET_HUD       — warmup: ruleset; Strike in-progress: Capturelimit
// STAT_DUEL_HEADER       — duel: header pic; CA/RR in-progress: CONFIG_CA_ALIVE_HUD + client (POV "X vs Y"); LMS: POV "N enemy/enemies remaining"
// STAT_WARMUP_NOTICE     — CONFIG_WARMUP_NOTICE; WARMUP_SPLASH_DURATION after warmup_notice_time while requisite active
// STAT_ROUND_NUMBER      — CONFIG_ROUND_PROGRESS; display uses HudRoundDisplayNumber() (+1 during countdown); not used for Horde / round-on-gametype HUD modes
// STAT_MINISCORE_*       — writer: SetMiniScoreStats / CTF_SetStats; visible from MATCH_WARMUP_DELAYED through MATCH_IN_PROGRESS
// STAT_MONSTER_COUNT    — Horde: remaining monsters (big num); Strike/CA: arena_hud_role_t (ifbit)
// STAT_LIVES            — Horde/LMS: right stack num(1) at yt 42; coop: lives_num stack at yt 2

inline constexpr size_t MM_STATUSBAR_LAYOUT_MAX_CHARS = 5280; // CS_SIZE(CS_STATUSBAR)

inline bool MM_StatusbarLayoutLengthWithinBudget(size_t len)
{
	return len <= MM_STATUSBAR_LAYOUT_MAX_CHARS;
}

inline bool MM_MiniscoreValVisible(int16_t stat_value)
{
	return stat_value != 0;
}

inline uint16_t MM_EncodeStrikeArenaRole(bool attacking)
{
	return attacking ? static_cast<uint16_t>(ARENA_ROLE_ATTACKING) : static_cast<uint16_t>(ARENA_ROLE_DEFENDING);
}

inline uint16_t MM_EncodeArenaEliminatedRole()
{
	return static_cast<uint16_t>(ARENA_ROLE_ELIMINATED);
}

inline uint16_t MM_EncodeArenaRoleForClient(bool strike_mode, bool attacking, bool eliminated)
{
	if (strike_mode) {
		if (eliminated)
			return 0;
		return MM_EncodeStrikeArenaRole(attacking);
	}

	if (eliminated)
		return MM_EncodeArenaEliminatedRole();

	return 0;
}

inline bool MM_ArenaRoleHasMask(uint16_t role, arena_hud_role_t mask)
{
	return (role & static_cast<uint16_t>(mask)) != 0;
}

// Per-client CA alive strings (allies vs enemies from local POV). CONFIG_CA_ALIVE_HUD is the pool base.
// CONFIG_CA_ALIVE_HUD is an *absolute* configstring index (CS_GENERAL + offset), so the number of pool
// slots is what remains in the general region after the base - NOT MAX_GENERAL - CONFIG_CA_ALIVE_HUD,
// which underflows size_t and makes the callers' bounds checks dead.
static_assert(CONFIG_CA_ALIVE_HUD >= CS_GENERAL && CONFIG_CA_ALIVE_HUD < CS_GENERAL + MAX_GENERAL,
	"CONFIG_CA_ALIVE_HUD pool base must live inside the general configstring region");
inline constexpr size_t CONFIG_CA_ALIVE_HUD_SLOTS = (CS_GENERAL + MAX_GENERAL) - CONFIG_CA_ALIVE_HUD;
