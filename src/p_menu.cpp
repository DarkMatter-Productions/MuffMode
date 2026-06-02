// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include "g_local.h"
#include "g_debug_log.h"
#include "muffmode/mm_vote_menu.h"

/*
============
P_Menu_Dirty
============
*/
void P_Menu_Dirty() {
	for (auto player : active_clients())
		if (player->client->menu) {
			player->client->menudirty = true;
			player->client->menutime = level.time;
		}
}

// Note that the pmenu entries are duplicated
// this is so that a static set of pmenu entries can be used
// for multiple clients and changed without interference
// note that arg will be freed when the menu is closed, it must be allocated memory
menu_hnd_t *P_Menu_Open(gentity_t *ent, const menu_t *entries, int cur, int num, void *arg, UpdateFunc_t UpdateFunc) {
	menu_hnd_t		*hnd;
	const menu_t	*p;
	size_t			i;

	if (!ent->client)
		return nullptr;

	int ci = (int)(ent->client - game.clients);
	MuffModeLog("DEBUG", "P_Menu_Open: client %d, existing menu=%p, inmenu=%d, num_entries=%d",
	           ci, (void*)ent->client->menu, (int)ent->client->inmenu, num);

	if (ent->client->menu) {
		gi.Com_Print("Warning: client already has a menu.\n");
		if (!Vote_Menu_Active(ent))
			P_Menu_Close(ent);
	}

	hnd = (menu_hnd_t *)gi.TagMalloc(sizeof(*hnd), TAG_LEVEL);
	hnd->UpdateFunc = UpdateFunc;

	hnd->arg = arg;
	hnd->entries = (menu_t *)gi.TagMalloc(sizeof(menu_t) * num, TAG_LEVEL);
	memcpy(hnd->entries, entries, sizeof(menu_t) * num);
	// duplicate the strings since they may be from static memory
	for (i = 0; i < num; i++)
		Q_strlcpy(hnd->entries[i].text, entries[i].text, sizeof(entries[i].text));

	hnd->num = num;

	if (cur < 0 || !entries[cur].SelectFunc) {
		for (i = 0, p = entries; i < num; i++, p++)
			if (p->SelectFunc)
				break;
	} else
		i = cur;

	if (i >= num)
		hnd->cur = -1;
	else
		hnd->cur = i;

	ent->client->showscores = true;
	ent->client->inmenu = true;
	ent->client->menu = hnd;
	ent->client->ps.stats[STAT_SHOW_STATUSBAR] = 0;

	MuffModeLog("DEBUG", "P_Menu_Open: client %d, new menu=%p, entries=%p, calling UpdateFunc=%p",
	           ci, (void*)hnd, (void*)hnd->entries, (void*)UpdateFunc);

	if (UpdateFunc)
		UpdateFunc(ent);

	MuffModeLog("DEBUG", "P_Menu_Open: client %d, UpdateFunc done, calling P_Menu_Do_Update", ci);
	P_Menu_Do_Update(ent);
	MuffModeLog("DEBUG", "P_Menu_Open: client %d, calling unicast", ci);
	gi.unicast(ent, true);

	MuffModeLog("DEBUG", "P_Menu_Open: client %d complete", ci);
	return hnd;
}

void P_Menu_Close(gentity_t *ent) {
	menu_hnd_t *hnd;

	if (!ent->client->menu)
		return;

	int ci = (int)(ent->client - game.clients);
	hnd = ent->client->menu;
	MuffModeLog("DEBUG", "P_Menu_Close: client %d, menu=%p, entries=%p",
	           ci, (void*)hnd, (void*)hnd->entries);
	gi.TagFree(hnd->entries);
	if (hnd->arg)
		gi.TagFree(hnd->arg);
	gi.TagFree(hnd);
	ent->client->menu = nullptr;
	ent->client->inmenu = false;
	ent->client->showscores = false;
	
	gentity_t *e = ent->client->follow_target ? ent->client->follow_target : ent;
	ent->client->ps.stats[STAT_SHOW_STATUSBAR] = !ClientIsPlaying(e->client) ? 0 : 1;
}

// only use on pmenu's that have been called with P_Menu_Open
void P_Menu_UpdateEntry(menu_t *entry, const char *text, int align, SelectFunc_t SelectFunc) {
	Q_strlcpy(entry->text, text, sizeof(entry->text));
	entry->align = align;
	entry->SelectFunc = SelectFunc;
}

#include "g_statusbar.h"

void P_Menu_Do_Update(gentity_t *ent) {
	int			i;
	menu_t		*p;
	int			x;
	menu_hnd_t	*hnd;
	const char	*t;
	bool		alt = false;

	int ci = ent->client ? (int)(ent->client - game.clients) : -1;

	if (!ent->client->menu) {
		gi.Com_Print("Warning: ent has no menu\n");
		return;
	}

	hnd = ent->client->menu;
	MuffModeLog("DEBUG", "P_Menu_Do_Update: client %d, menu=%p, entries=%p, num=%d, UpdateFunc=%p",
	           ci, (void*)hnd, (void*)hnd->entries, hnd->num, (void*)hnd->UpdateFunc);

	if (hnd->UpdateFunc)
		hnd->UpdateFunc(ent);

	// Guard: UpdateFunc may have closed/freed the menu (e.g. vote menu when vote passes)
	if (!ent->client->menu)
		return;

	statusbar_t sb;

	sb.xv(32).yv(8).picn("inventory");

	for (i = 0, p = hnd->entries; i < hnd->num; i++, p++) {
		if (!*(p->text))
			continue; // blank line

		t = p->text;

		if (*t == '*') {
			alt = true;
			t++;
		}

		sb.yv(32 + i * 8);

		const char *loc_func = "loc_string";

		if (p->align == MENU_ALIGN_CENTER) {
			x = 0;
			loc_func = "loc_cstring";
		} else if (p->align == MENU_ALIGN_RIGHT) {
			x = 260;
			loc_func = "loc_rstring";
		} else
			x = 64;

		sb.xv(x);

		sb.sb << loc_func;

		if (hnd->cur == i || alt)
			sb.sb << '2';

		sb.sb << " 1 \"" << t << "\" \"" << p->text_arg1 << "\" ";

		if (hnd->cur == i) {
			sb.xv(56);
			sb.string2("\">\"");
		}

		alt = false;
	}

	gi.WriteByte(svc_layout);
	gi.WriteString(sb.sb.str().c_str());
}

void P_Menu_Update(gentity_t *ent) {
	if (!ent->client->menu) {
		gi.Com_Print("Warning: ent has no menu\n");
		return;
	}

	if (level.time - ent->client->menutime >= 1_sec) {
		// been a second or more since last update, update now
		P_Menu_Do_Update(ent);
		gi.unicast(ent, true);
		ent->client->menutime = level.time + 1_sec;
		ent->client->menudirty = false;
	}

	ent->client->menutime = level.time;
	ent->client->menudirty = true;
	gi.local_sound(ent, CHAN_AUTO, gi.soundindex("misc/menu2.wav"), 1, ATTN_NONE, 0);
}

void P_Menu_Next(gentity_t *ent) {
	menu_hnd_t	*hnd;
	int			i;
	menu_t		*p;

	if (!ent->client->menu) {
		gi.Com_Print("Warning: ent has no menu\n");
		return;
	}

	hnd = ent->client->menu;

	if (hnd->cur < 0)
		return; // no selectable entries

	i = hnd->cur;
	p = hnd->entries + hnd->cur;
	do {
		i++;
		p++;
		if (i == hnd->num) {
			i = 0;
			p = hnd->entries;
		}
		if (p->SelectFunc)
			break;
	} while (i != hnd->cur);

	hnd->cur = i;

	P_Menu_Update(ent);
}

void P_Menu_Prev(gentity_t *ent) {
	menu_hnd_t	*hnd;
	int			i;
	menu_t		*p;

	if (!ent->client->menu) {
		gi.Com_Print("Warning: ent has no menu\n");
		return;
	}

	hnd = ent->client->menu;

	if (hnd->cur < 0)
		return; // no selectable entries

	i = hnd->cur;
	p = hnd->entries + hnd->cur;
	do {
		if (i == 0) {
			i = hnd->num - 1;
			p = hnd->entries + i;
		} else {
			i--;
			p--;
		}
		if (p->SelectFunc)
			break;
	} while (i != hnd->cur);

	hnd->cur = i;

	P_Menu_Update(ent);
}

void P_Menu_Select(gentity_t *ent) {
	menu_hnd_t	*hnd;
	menu_t		*p;

	if (!ent->client->menu) {
		gi.Com_Print("Warning: ent has no menu\n");
		return;
	}

	// no selecting during intermission
	if (level.intermission_queued || level.intermission_time)
		return;

	hnd = ent->client->menu;

	if (hnd->cur < 0)
		return; // no selectable entries

	p = hnd->entries + hnd->cur;

	if (p->SelectFunc)
		p->SelectFunc(ent, hnd);
	//gi.local_sound(ent, CHAN_AUTO, gi.soundindex("misc/menu1.wav"), 1, ATTN_NONE, 0);
}
