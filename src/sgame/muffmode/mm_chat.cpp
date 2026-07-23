// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_arena.h"
#include "muffmode/mm_chat.h"

#include <string>

bool MM_ChatCommandsUseGameDispatch()
{
#ifndef KEX_Q2_GAME
	return true;
#else
	// Q2REPRO/Q2PRO expose this documented engine extension. Using capability
	// detection keeps official KEX lobby chat on its native path and prevents
	// the game DLL from producing a duplicate message there.
	return gi.GetExtension &&
		gi.GetExtension("FILESYSTEM_API_V1") != nullptr;
#endif
}

void MM_SendScopedChat(gentity_t *sender, mm_arena_chat_scope_t scope,
	std::string_view message, bool team_prefix)
{
	if (!sender || !sender->inuse || !sender->client || message.empty())
		return;
	if (GT(GT_ARENA) && !MM_Arena_CanSendChat(sender, scope))
		return;
	if (!MM_Arena_BypassChatFlood(sender, scope) && CheckFlood(sender))
		return;

	std::string text;
	if (team_prefix)
		fmt::format_to(std::back_inserter(text), FMT_STRING("({}): "),
			sender->client->resp.netname);
	else if (GT(GT_ARENA) && scope == mm_arena_chat_scope_t::World)
		fmt::format_to(std::back_inserter(text), FMT_STRING("[WORLD] {}: "),
			sender->client->resp.netname);
	else
		fmt::format_to(std::back_inserter(text), FMT_STRING("{}: "),
			sender->client->resp.netname);
	text.append(message);

	if (text.length() > 150)
		text.resize(150);
	if (text.empty())
		return;
	if (text.back() != '\n')
		text.push_back('\n');

	if (g_dedicated->integer)
		gi.Client_Print(nullptr, PRINT_CHAT, text.c_str());

	for (gentity_t *recipient : active_clients()) {
		if (!recipient || !recipient->inuse || !recipient->client)
			continue;
		if (GT(GT_ARENA) &&
			!MM_Arena_ChatRecipient(sender, recipient, scope))
			continue;
		if (notGT(GT_ARENA) && team_prefix &&
			recipient->client->sess.team != sender->client->sess.team)
			continue;
		gi.Client_Print(recipient, PRINT_CHAT, text.c_str());
	}
}
