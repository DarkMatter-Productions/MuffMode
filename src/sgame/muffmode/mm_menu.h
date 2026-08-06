// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;
struct menu_hnd_t;

enum class mm_menu_client_state_t {
	Playing,
	Spectator
};

// Server menus use movement/select buttons as UI controls regardless of the
// client's gameplay role. The role argument makes that contract explicit and
// prevents callers from restoring the old spectator-only routing by accident.
constexpr bool MM_MenuCapturesUsercmd(bool menu_open, mm_menu_client_state_t) noexcept
{
	return menu_open;
}

// [MuffMode] Join/admin/info menu entry points. Implemented in muffmode/mm_menu.cpp.
void G_Menu_Join_Open(gentity_t *ent);
void G_Menu_ReturnToMain(gentity_t *ent, menu_hnd_t *p);
