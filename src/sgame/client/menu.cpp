// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include "g_local.h"
#include "core/debug_log.h"
#include "muffmode/mm_util.h"
#include "muffmode/mm_vote_menu.h"

#include <algorithm>
#include <string>

namespace {

constexpr std::size_t kMenuMaxSourceBytes = 256;
constexpr std::size_t kMenuMaxArgumentChars = 63;

std::size_t ValidUtf8SequenceLength(std::string_view text, std::size_t offset)
{
	const std::size_t limit = text.size();
	const auto byte = [&](std::size_t index) {
		return static_cast<unsigned char>(text[index]);
	};
	const auto continuation = [&](std::size_t index) {
		return index < limit && (byte(index) & 0xc0) == 0x80;
	};

	const unsigned char lead = byte(offset);
	if (lead >= 0xc2 && lead <= 0xdf)
		return continuation(offset + 1) ? 2 : 1;

	if (lead >= 0xe0 && lead <= 0xef) {
		if (!continuation(offset + 1) || !continuation(offset + 2))
			return 1;
		const unsigned char second = byte(offset + 1);
		if ((lead == 0xe0 && second < 0xa0) ||
			(lead == 0xed && second >= 0xa0))
			return 1;
		return 3;
	}

	if (lead >= 0xf0 && lead <= 0xf4) {
		if (!continuation(offset + 1) || !continuation(offset + 2) ||
			!continuation(offset + 3))
			return 1;
		const unsigned char second = byte(offset + 1);
		if ((lead == 0xf0 && second < 0x90) ||
			(lead == 0xf4 && second >= 0x90))
			return 1;
		return 4;
	}

	// Preserve non-UTF-8 high bytes as single legacy Quake glyphs.
	return 1;
}

template <std::size_t N>
std::string_view BoundedMenuStringView(const char (&text)[N])
{
	std::size_t length = 0;
	while (length < N && text[length])
		length++;
	return std::string_view(&text[0], length);
}

std::string SanitizeMenuField(std::string_view text, std::size_t max_chars)
{
	if (max_chars == 0)
		return {};

	const std::size_t scan_length =
		std::min(text.size(), kMenuMaxSourceBytes);
	const std::string_view scan_text = text.substr(0, scan_length);
	std::string cleaned;
	cleaned.reserve(std::min(scan_length, max_chars * 2));

	bool pending_space = false;
	bool truncated = scan_length < text.size();
	std::size_t character_count = 0;
	std::size_t ellipsis_boundary = 0;
	const std::size_t characters_before_ellipsis =
		max_chars > 3 ? max_chars - 3 : 0;
	auto append = [&](std::string_view glyph) {
		if (character_count >= max_chars) {
			truncated = true;
			return false;
		}
		cleaned.append(glyph);
		character_count++;
		if (character_count == characters_before_ellipsis)
			ellipsis_boundary = cleaned.size();
		return true;
	};
	auto append_ascii = [&](char c) {
		return append(std::string_view(&c, 1));
	};

	for (std::size_t i = 0; i < scan_length;) {
		const unsigned char c = static_cast<unsigned char>(scan_text[i]);
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
			c == '\v' || c == '\f') {
			pending_space = !cleaned.empty();
			i++;
			continue;
		}

		if (c >= 0x80) {
			const std::size_t sequence_length =
				ValidUtf8SequenceLength(scan_text, i);
			if (pending_space) {
				if (!append_ascii(' '))
					break;
				pending_space = false;
			}
			if (!append(scan_text.substr(i, sequence_length)))
				break;
			i += sequence_length;
			continue;
		}

		char safe = '\0';
		if (c == 0x1d || c == 0x1e || c == 0x1f)
			safe = '-'; // Preserve legacy menu divider glyphs without C0 bytes.
		else if (c < 0x20 || c == 0x7f) {
			i++;
			continue;
		} else if (c == '"')
			safe = '\'';
		else if (c == '\\')
			safe = '/';
		else
			safe = static_cast<char>(c);

		if (pending_space) {
			if (!append_ascii(' '))
				break;
			pending_space = false;
		}
		if (!append_ascii(safe))
			break;
		i++;
	}

	if (!truncated)
		return cleaned;

	if (max_chars <= 3)
		return std::string(max_chars, '.');

	if (character_count > characters_before_ellipsis)
		cleaned.resize(ellipsis_boundary);
	cleaned += "...";
	return cleaned;
}

void RevalidateMenuCursor(menu_hnd_t *hnd)
{
	if (!hnd || !hnd->entries || hnd->num <= 0) {
		if (hnd)
			hnd->cur = -1;
		return;
	}

	if (hnd->cur >= 0 && hnd->cur < hnd->num &&
		hnd->entries[hnd->cur].SelectFunc)
		return;

	hnd->cur = -1;
	for (int row = 0; row < hnd->num; row++) {
		if (!hnd->entries[row].SelectFunc)
			continue;
		hnd->cur = row;
		break;
	}
}

} // namespace

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
// A successful open adopts arg and frees it when the menu closes. On nullptr,
// arg remains caller-owned, including when the initial updater closes the menu.
menu_hnd_t *P_Menu_Open(gentity_t *ent, const menu_t *entries, int cur, int num, void *arg, UpdateFunc_t UpdateFunc) {
	menu_hnd_t		*hnd;
	const menu_t	*p;
	size_t			i;

	if (!ent || !ent->client || !entries || num <= 0)
		return nullptr;

	int ci = (int)(ent->client - game.clients);
	if (num > MENU_MAX_ROWS) {
		gi.Com_PrintFmt("Warning: menu for client {} has {} rows; limiting it to {}.\n",
			ci, num, MENU_MAX_ROWS);
		num = MENU_MAX_ROWS;
	}
	MuffModeLog("DEBUG", "P_Menu_Open: client %d, existing menu=%p, inmenu=%d, num_entries=%d",
	           ci, (void*)ent->client->menu, (int)ent->client->inmenu, num);

	if (ent->client->menu) {
		gi.Com_Print("Warning: client already has a menu.\n");
		if (Vote_Menu_Active(ent)) {
			MuffModeLog("DEBUG",
				"P_Menu_Open: client %d retaining active vote menu", ci);
			return nullptr;
		}
		P_Menu_Close(ent);
	}

	hnd = (menu_hnd_t *)gi.TagMalloc(sizeof(*hnd), TAG_LEVEL);
	if (!hnd)
		return nullptr;
	hnd->UpdateFunc = UpdateFunc;

	hnd->arg = arg;
	hnd->arg_owned = false;
	hnd->entries = (menu_t *)gi.TagMalloc(sizeof(menu_t) * num, TAG_LEVEL);
	if (!hnd->entries) {
		gi.TagFree(hnd);
		return nullptr;
	}
	memcpy(hnd->entries, entries, sizeof(menu_t) * num);
	// duplicate the strings since they may be from static memory
	for (i = 0; i < num; i++)
		P_Menu_SetText(&hnd->entries[i],
			BoundedMenuStringView(entries[i].text));

	hnd->num = num;

	if (cur < 0 || cur >= num || !entries[cur].SelectFunc) {
		for (i = 0, p = entries; i < num; i++, p++)
			if (p->SelectFunc)
				break;
	} else
		i = cur;

	if (i >= num)
		hnd->cur = -1;
	else
		hnd->cur = static_cast<int>(i);

	ent->client->showscores = true;
	ent->client->inmenu = true;
	ent->client->menu = hnd;
	ent->client->ps.stats[STAT_SHOW_STATUSBAR] = 0;

	MuffModeLog("DEBUG", "P_Menu_Open: client %d, new menu=%p, entries=%p, calling UpdateFunc=%p",
	           ci, (void*)hnd, (void*)hnd->entries, (void*)UpdateFunc);

	MuffModeLog("DEBUG", "P_Menu_Open: client %d, calling P_Menu_Do_Update", ci);
	P_Menu_Do_Update(ent);
	if (ent->client->menu != hnd)
		return nullptr;
	hnd->arg_owned = true;
	MuffModeLog("DEBUG", "P_Menu_Open: client %d, calling unicast", ci);
	gi.unicast(ent, true);
	ent->client->menutime = level.time + 3_sec;
	ent->client->menudirty = false;

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
	if (hnd->arg && hnd->arg_owned)
		gi.TagFree(hnd->arg);
	gi.TagFree(hnd);
	ent->client->menu = nullptr;
	ent->client->inmenu = false;
	ent->client->showscores = false;
	
	gentity_t *e = ent->client->follow_target ? ent->client->follow_target : ent;
	ent->client->ps.stats[STAT_SHOW_STATUSBAR] = !ClientIsPlaying(e->client) ? 0 : 1;
}

void P_Menu_SetText(menu_t *entry, std::string_view text) {
	if (!entry)
		return;

	const bool highlighted = !text.empty() && text.front() == '*';
	if (highlighted)
		text.remove_prefix(1);

	std::string cleaned = SanitizeMenuField(text, MENU_MAX_LINE_CHARS);
	if (highlighted)
		cleaned.insert(cleaned.begin(), '*');
	muffmode::CopyString(entry->text, cleaned);
}

// only use on pmenu's that have been called with P_Menu_Open
void P_Menu_UpdateEntry(menu_t *entry, const char *text, int align, SelectFunc_t SelectFunc) {
	P_Menu_SetText(entry, text ? std::string_view(text) : std::string_view());
	entry->align = align;
	entry->SelectFunc = SelectFunc;
}

#include "core/statusbar.h"

void P_Menu_Do_Update(gentity_t *ent) {
	int			i;
	menu_t		*p;
	int			x;
	menu_hnd_t	*hnd;

	if (!ent || !ent->client)
		return;

	int ci = (int)(ent->client - game.clients);

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
	if (ent->client->menu != hnd)
		return;

	RevalidateMenuCursor(hnd);

	statusbar_t sb;

	sb.xv(32).yv(8).picn("inventory");

	for (i = 0, p = hnd->entries; i < hnd->num; i++, p++) {
		std::string_view text = BoundedMenuStringView(p->text);
		if (text.empty())
			continue; // blank line

		bool alt = false;
		if (text.front() == '*') {
			alt = true;
			text.remove_prefix(1);
		}
		const std::string safe_text =
			SanitizeMenuField(text, MENU_MAX_LINE_CHARS);
		if (safe_text.empty())
			continue;
		const std::string safe_arg = SanitizeMenuField(
			BoundedMenuStringView(p->text_arg1), kMenuMaxArgumentChars);

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

		sb.sb << " 1 \"" << safe_text << "\" \"" << safe_arg << "\" ";

		if (hnd->cur == i) {
			sb.xv(56);
			sb.string2("\">\"");
		}

	}

	gi.WriteByte(svc_layout);
	gi.WriteString(sb.sb.str().c_str());
}

void P_Menu_Update(gentity_t *ent) {
	if (!ent->client->menu) {
		gi.Com_Print("Warning: ent has no menu\n");
		return;
	}

	menu_hnd_t *hnd = ent->client->menu;
	P_Menu_Do_Update(ent);
	if (ent->client->menu == hnd) {
		gi.unicast(ent, true);
		ent->client->menutime = level.time + 3_sec;
		ent->client->menudirty = false;
	}

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
