// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "muffmode/mm_arena.h"

#include <string_view>

struct gentity_t;

// True when the current engine delegates built-in say/say_team commands to the
// game DLL. Official KEX owns those commands in its lobby layer; Q2PRO-family
// KEX hosts advertise a server extension and expect the game to handle them.
bool MM_ChatCommandsUseGameDispatch();

// Sends already-parsed chat text through MuffMode's shared flood, formatting,
// logging, and recipient policy. Arena contributes only room/team visibility.
void MM_SendScopedChat(gentity_t *sender, mm_arena_chat_scope_t scope,
	std::string_view message, bool team_prefix);
