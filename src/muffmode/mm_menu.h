// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;
struct menu_hnd_t;

// [MuffMode] Join/admin/info menu entry points. Implemented in muffmode/mm_menu.cpp.
void G_Menu_Join_Open(gentity_t *ent);
void G_Menu_ReturnToMain(gentity_t *ent, menu_hnd_t *p);
