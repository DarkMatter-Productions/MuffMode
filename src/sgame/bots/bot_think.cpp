// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "../g_local.h"
#include "bot_think.h"
#include "muffmode/mm_freezetag.h"

/*
================
Bot_BeginFrame
================
*/
void Bot_BeginFrame( gentity_t * bot ) {
	MM_FreezeTag_BotBeginFrame(bot);
}

/*
================
Bot_EndFrame
================
*/
void Bot_EndFrame( gentity_t * bot ) {

}
