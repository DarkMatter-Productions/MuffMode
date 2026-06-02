// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gentity_t;
struct menu_hnd_t;

// [MuffMode] Vote menu entry points. Implemented in muffmode/mm_vote_menu.cpp.
bool Vote_Menu_Active(gentity_t *ent);
void G_Menu_CallVote(gentity_t *ent, menu_hnd_t *p);
void G_Menu_ReturnToCallVote(gentity_t *ent, menu_hnd_t *p);
void G_Menu_Vote_Open(gentity_t *ent);
