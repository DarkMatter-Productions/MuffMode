// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "bot_think.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_freezetag.h"

/*
================
Bot_BeginFrame
================
*/
void Bot_BeginFrame( gentity_t * bot ) {
	MM_FreezeTag_BotBeginFrame(bot);
	// [MuffMode] Arena rooms are selected in-game, not by the engine, so a bot has to choose its
	// room and an opposing logical team itself. No-op outside GT_ARENA.
	MM_Arena_BotBeginFrame(bot);
}

/*
================
Bot_EndFrame
================
*/
void Bot_EndFrame( gentity_t * bot ) {

}
