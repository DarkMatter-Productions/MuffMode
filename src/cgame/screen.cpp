// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include "screen.h"
#include "hud_text.h"
#include "messages.h"
#include "mm_hud_enhancements.h"
#include "muffmode/mm_hud_stat_contracts.h"
#include "muffmode/mm_parse.h"

#include <array>
#include <charconv>

constexpr int32_t STAT_MINUS = 10;  // num frame for '-' stats digit
constexpr const char *sb_nums[2][11] =
{
	{   "num_0", "num_1", "num_2", "num_3", "num_4", "num_5",
		"num_6", "num_7", "num_8", "num_9", "num_minus"
	},
	{   "anum_0", "anum_1", "anum_2", "anum_3", "anum_4", "anum_5",
		"anum_6", "anum_7", "anum_8", "anum_9", "anum_minus"
	}
};

constexpr int32_t CHAR_WIDTH = 16;
constexpr int32_t CONCHAR_WIDTH = 8;

static int32_t font_y_offset;

constexpr rgba_t alt_color{ 112, 255, 52, 255 };

static cvar_t *scr_usekfont;

static cvar_t *ui_acc_alttypeface;

// team border cvars
static cvar_t *cl_teamBorder;
static cvar_t *cl_teamBorderWidth;
static cvar_t *cl_teamBorderAlpha;

// static temp data used for hud
static struct {
	struct {
		struct {
			char    text[24];
		} table_cells[6];
	} table_rows[11]; // just enough to store 8 levels + header + total (+ one slack)

	size_t column_widths[6];
	int32_t num_rows = 0;
	int32_t num_columns = 0;
} hud_temp;

layout_flags_t CG_LayoutFlags(const player_state_t *ps) {
	return (layout_flags_t)ps->stats[STAT_LAYOUTS];
}

static bool CG_TryParseLayoutInt(const char *token, int32_t &value) {
	const auto parsed = MM_ParseIntArg(token);
	if (!parsed)
		return false;

	value = *parsed;
	return true;
}

static int32_t CG_ParseLayoutInt(const char *token, const char *error_message = "Bad layout integer") {
	int32_t value = 0;
	if (!CG_TryParseLayoutInt(token, value))
		cgi.Com_Error(error_message);

	return value;
}

static bool CG_TryLayoutStatIndex(const char *token, int32_t &stat) {
	return CG_TryParseLayoutInt(token, stat) && stat >= 0 && stat < MAX_STATS;
}

static int32_t CG_ParseLayoutStatIndex(const char *token, const char *error_message = "Bad stat_string index") {
	int32_t stat = 0;
	if (!CG_TryLayoutStatIndex(token, stat))
		cgi.Com_Error(error_message);

	return stat;
}

static int32_t CG_ParseLayoutClientIndex(const char *token) {
	const int32_t client_index = CG_ParseLayoutInt(token, "Bad client index");
	if (client_index < 0 || client_index >= MAX_CLIENTS)
		cgi.Com_Error("client >= MAX_CLIENTS");

	return client_index;
}

static int32_t CG_ParseLayoutLocalizationArgCount(const char *token) {
	const int32_t num_args = CG_ParseLayoutInt(token, "Bad loc string");
	if (num_args < 0 || num_args >= static_cast<int32_t>(MAX_LOCALIZATION_ARGS))
		cgi.Com_Error("Bad loc string");

	return num_args;
}

bool CG_UseKFont() {
	return scr_usekfont && scr_usekfont->integer != 0;
}

/*
==============
CG_DrawHUDString
==============
*/
int CG_DrawHUDString(const char *string, int x, int y, int centerwidth, int xor_mask, int scale, bool shadow) {
	int     margin;
	char    line[1024];
	int     width;
	int     i;

	margin = x;

	while (*string) {
		width = 0;
		while (*string && *string != '\n')
			line[width++] = *string++;
		line[width] = 0;

		vec2_t size;

		if (CG_UseKFont())
			size = cgi.SCR_MeasureFontString(line, scale);

		if (centerwidth) {
			if (!CG_UseKFont())
				x = margin + ((centerwidth - width * CONCHAR_WIDTH * scale)) / 2;
			else
				x = margin + ((centerwidth - size.x)) / 2;
		} else {
			x = margin;
		}

		if (!CG_UseKFont()) {
			for (i = 0; i < width; i++) {
				cgi.SCR_DrawChar(x, y, scale, line[i] ^ xor_mask, shadow);
				x += CONCHAR_WIDTH * scale;
			}
		} else {
			cgi.SCR_DrawFontString(line, x, y - (font_y_offset * scale), scale, xor_mask ? alt_color : rgba_white, true, text_align_t::LEFT);
			x += size.x;
		}

		if (*string) {
			string++;
			x = margin;
			y += (CG_UseKFont() ? 10 : CONCHAR_WIDTH) * scale;
		}
	}

	return x;
}

/*
==============
CG_DrawString
==============
*/
static void CG_DrawString(int x, int y, int scale, const char *s, bool alt = false, bool shadow = true) {
	while (*s) {
		cgi.SCR_DrawChar(x, y, scale, *s ^ (alt ? 0x80 : 0), shadow);
		x += 8 * scale;
		s++;
	}
}

/*
==============
CG_DrawFieldHudCentered
==============
*/
static void CG_DrawFieldHudCentered(vrect_t hud_vrect, int y, int color, int width, int value, int scale) {
	char    num[16], *ptr;
	int     l;
	int     frame;

	if (width < 1)
		return;

	if (width > 5)
		width = 5;

	auto result = std::to_chars(num, num + sizeof(num) - 1, value);
	*(result.ptr) = '\0';

	l = (result.ptr - num);

	if (l > width)
		l = width;

	int32_t x = ((hud_vrect.x + hud_vrect.width / 2) * scale) - ((l * CHAR_WIDTH) / 2) * scale;

	ptr = num;
	while (*ptr && l) {
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr - '0';
		int w, h;
		cgi.Draw_GetPicSize(&w, &h, sb_nums[color][frame]);
		cgi.SCR_DrawPic(x, y, w * scale, h * scale, sb_nums[color][frame]);
		x += CHAR_WIDTH * scale;
		ptr++;
		l--;
	}
}

/*
==============
CG_DrawField
==============
*/
static void CG_DrawField(int x, int y, int color, int width, int value, int scale) {
	char    num[16], *ptr;
	int     l;
	int     frame;

	if (width < 1)
		return;

	// draw number string
	if (width > 5)
		width = 5;

	auto result = std::to_chars(num, num + sizeof(num) - 1, value);
	*(result.ptr) = '\0';

	l = (result.ptr - num);

	if (l > width)
		l = width;

	x += (2 + CHAR_WIDTH * (width - l)) * scale;

	ptr = num;
	while (*ptr && l) {
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr - '0';
		int w, h;
		cgi.Draw_GetPicSize(&w, &h, sb_nums[color][frame]);
		cgi.SCR_DrawPic(x, y, w * scale, h * scale, sb_nums[color][frame]);
		x += CHAR_WIDTH * scale;
		ptr++;
		l--;
	}
}

// [Paril-KEX]
static void CG_DrawTable(int x, int y, uint32_t width, uint32_t height, int32_t scale) {
	// half left
	int32_t width_pixels = width;
	x -= width_pixels / 2;
	y += CONCHAR_WIDTH * scale;
	// use Y as top though

	int32_t height_pixels = height;

	// draw border
	// KEX_FIXME method that requires less chars
	cgi.SCR_DrawChar(x - (CONCHAR_WIDTH * scale), y - (CONCHAR_WIDTH * scale), scale, 18, false);
	cgi.SCR_DrawChar((x + width_pixels), y - (CONCHAR_WIDTH * scale), scale, 20, false);
	cgi.SCR_DrawChar(x - (CONCHAR_WIDTH * scale), y + height_pixels, scale, 24, false);
	cgi.SCR_DrawChar((x + width_pixels), y + height_pixels, scale, 26, false);

	for (int cx = x; cx < x + width_pixels; cx += CONCHAR_WIDTH * scale) {
		cgi.SCR_DrawChar(cx, y - (CONCHAR_WIDTH * scale), scale, 19, false);
		cgi.SCR_DrawChar(cx, y + height_pixels, scale, 25, false);
	}

	for (int cy = y; cy < y + height_pixels; cy += CONCHAR_WIDTH * scale) {
		cgi.SCR_DrawChar(x - (CONCHAR_WIDTH * scale), cy, scale, 21, false);
		cgi.SCR_DrawChar((x + width_pixels), cy, scale, 23, false);
	}

	cgi.SCR_DrawColorPic(x, y, width_pixels, height_pixels, "_white", { 0, 0, 0, 255 });

	// draw in columns
	for (int i = 0; i < hud_temp.num_columns; i++) {
		for (int r = 0, ry = y; r < hud_temp.num_rows; r++, ry += (CONCHAR_WIDTH + font_y_offset) * scale) {
			int x_offset = 0;

			// center 
			if (r == 0) {
				x_offset = ((hud_temp.column_widths[i]) / 2) -
					((cgi.SCR_MeasureFontString(hud_temp.table_rows[r].table_cells[i].text, scale).x) / 2);
			}
			// right align
			else if (i != 0) {
				x_offset = (hud_temp.column_widths[i] - cgi.SCR_MeasureFontString(hud_temp.table_rows[r].table_cells[i].text, scale).x);
			}

			//CG_DrawString(x + x_offset, ry, scale, hud_temp.table_rows[r].table_cells[i].text, r == 0, true);
			cgi.SCR_DrawFontString(hud_temp.table_rows[r].table_cells[i].text, x + x_offset, ry - (font_y_offset * scale), scale, r == 0 ? alt_color : rgba_white, true, text_align_t::LEFT);
		}

		x += (hud_temp.column_widths[i] + cgi.SCR_MeasureFontString(" ", 1).x);
	}
}

/*
=============
CG_TimeStringMs

Format a client-visible timer string with millisecond precision.
=============
*/
static const char *CG_TimeStringMs(const int msec) {
	static char buffer[32];
	int hours, mins, seconds, ms = msec;

	seconds = ms / 1000;
	ms -= seconds * 1000;
	mins = seconds / 60;
	seconds -= mins * 60;
	hours = mins / 60;
	mins -= hours * 60;

	if (hours > 0) {
		G_FmtTo(buffer, "{}:{:02}:{:02}.{}", hours, mins, seconds, ms);
	} else {
		G_FmtTo(buffer, "{:02}:{:02}.{}", mins, seconds, ms);
	}

	return buffer;
}

/*
================
CG_ExecuteLayoutString

================
*/
static void CG_ExecuteLayoutString(const char *s, vrect_t hud_vrect, vrect_t hud_safe, int32_t scale, int32_t playernum, const player_state_t *ps) {
	int			x, y;
	int			w, h;
	int			hx, hy;
	int			value;
	const char *token;
	int			width;
	int			index;

	if (!s[0])
		return;

	x = hud_vrect.x;
	y = hud_vrect.y;
	width = 3;

	hx = 320 / 2;
	hy = 240 / 2;

	bool flash_frame = (cgi.CL_ClientTime() % 1000) < 500;

	// if non-zero, parse but don't affect state
	int32_t if_depth = 0; // current if statement depth
	int32_t endif_depth = 0; // at this depth, toggle skip_depth
	bool skip_depth = false; // whether we're in a dead stmt or not

	while (s) {
		token = COM_Parse(&s);
		if (!strcmp(token, "xl")) {
			token = COM_Parse(&s);
			if (!skip_depth)
				x = ((hud_vrect.x + CG_ParseLayoutInt(token)) * scale) + hud_safe.x;
			continue;
		}
		if (!strcmp(token, "xr")) {
			token = COM_Parse(&s);
			if (!skip_depth)
				x = ((hud_vrect.x + hud_vrect.width + CG_ParseLayoutInt(token)) * scale) - hud_safe.x;
			continue;
		}
		if (!strcmp(token, "xv")) {
			token = COM_Parse(&s);
			if (!skip_depth)
				x = (hud_vrect.x + hud_vrect.width / 2 + (CG_ParseLayoutInt(token) - hx)) * scale;
			continue;
		}

		if (!strcmp(token, "yt")) {
			token = COM_Parse(&s);
			if (!skip_depth)
				y = ((hud_vrect.y + CG_ParseLayoutInt(token)) * scale) + hud_safe.y;
			continue;
		}
		if (!strcmp(token, "yb")) {
			token = COM_Parse(&s);
			if (!skip_depth)
				y = ((hud_vrect.y + hud_vrect.height + CG_ParseLayoutInt(token)) * scale) - hud_safe.y;
			continue;
		}
		if (!strcmp(token, "yv")) {
			token = COM_Parse(&s);
			if (!skip_depth)
				y = (hud_vrect.y + hud_vrect.height / 2 + (CG_ParseLayoutInt(token) - hy)) * scale;
			continue;
		}

		if (!strcmp(token, "pic")) {   // draw a pic from a stat number
			token = COM_Parse(&s);
			if (!skip_depth) {
				int32_t stat = 0;
				bool skip = false;

				if (!CG_TryLayoutStatIndex(token, stat))
					cgi.Com_Error("Bad pic stat index");

				value = ps->stats[stat];
				if (value < 0 || value >= MAX_IMAGES)
					cgi.Com_Error("Pic outside image range");

				//muff: client-side hacky hacks - don't show vitals if spectating
				if ((ps->stats[STAT_SPECTATOR] && !ps->stats[STAT_CHASE]) && (stat == STAT_HEALTH_ICON || stat == STAT_AMMO_ICON || stat == STAT_ARMOR_ICON))
					skip = true;

				const char *const pic = cgi.get_configstring(CS_IMAGES + value);

				if (pic && *pic && !skip) {
					//muff: little hacky hack! resize the player pics on miniscores for clients rockin' muffmode
					if (stat == STAT_MINISCORE_FIRST_PIC || stat == STAT_MINISCORE_SECOND_PIC) {
						w = 24;
						h = 24;
					} else if (stat == STAT_CTF_FLAG_PIC) {
						cgi.Draw_GetPicSize(&w, &h, pic);
					} else {
						cgi.Draw_GetPicSize(&w, &h, pic);
					}
					cgi.SCR_DrawPic(x, y, w * scale, h * scale, pic);
				}
			}

			continue;
		}

		if (!strcmp(token, "client")) {   // draw a deathmatch client block
			token = COM_Parse(&s);
			if (!skip_depth) {
				x = (hud_vrect.x + hud_vrect.width / 2 + (CG_ParseLayoutInt(token) - hx)) * scale;
				x += 8 * scale;
			}
			token = COM_Parse(&s);
			if (!skip_depth) {
				y = (hud_vrect.y + hud_vrect.height / 2 + (CG_ParseLayoutInt(token) - hy)) * scale;
				y += 7 * scale;
			}

			token = COM_Parse(&s);

			if (!skip_depth)
				value = CG_ParseLayoutClientIndex(token);

			int score = 0, ping = 0, time = 0;

			token = COM_Parse(&s);
			if (!skip_depth)
				score = CG_ParseLayoutInt(token);

			token = COM_Parse(&s);
			if (!skip_depth)
				ping = CG_ParseLayoutInt(token);

			token = COM_Parse(&s);
			if (!skip_depth) {
				time = CG_ParseLayoutInt(token);

				// Race mode removed - always show score, never format as time
				const char *scr = G_Fmt("{}", score).data();

				cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
				if (!scr_usekfont->integer)
					CG_DrawString(x + 32 * scale, y, scale, cgi.CL_GetClientName(value));
				else
					cgi.SCR_DrawFontString(cgi.CL_GetClientName(value), x + 32 * scale, y - (font_y_offset * scale), scale, rgba_white, true, text_align_t::LEFT);

				if (!scr_usekfont->integer)
					CG_DrawString(x + 32 * scale, y + 10 * scale, scale, scr, true);
				else
					cgi.SCR_DrawFontString(scr, x + 32 * scale, y + (10 - font_y_offset) * scale, scale, rgba_white, true, text_align_t::LEFT);

				cgi.SCR_DrawPic(x + 32 + 96 * scale, y + 10 * scale, 9 * scale, 9 * scale, "ping");
				if (!scr_usekfont->integer)
					CG_DrawString(x + 32 + 73 * scale + 32 * scale, y + 10 * scale, scale, G_Fmt("{}", ping).data());
				else
					cgi.SCR_DrawFontString(G_Fmt("{}", ping).data(), x + 32 + 107 * scale, y + (10 - font_y_offset) * scale, scale, rgba_white, true, text_align_t::LEFT);

				cgi.SCR_SetAltTypeface(false);
			}
			continue;
		}

		if (!strcmp(token, "ctf")) {   // draw a ctf client block
			int     score, ping;

			token = COM_Parse(&s);
			if (!skip_depth)
				x = (hud_vrect.x + hud_vrect.width / 2 - hx + CG_ParseLayoutInt(token)) * scale;
			token = COM_Parse(&s);
			if (!skip_depth)
				y = (hud_vrect.y + hud_vrect.height / 2 - hy + CG_ParseLayoutInt(token)) * scale;

			token = COM_Parse(&s);
			if (!skip_depth)
				value = CG_ParseLayoutClientIndex(token);

			token = COM_Parse(&s);
			if (!skip_depth)
				score = CG_ParseLayoutInt(token);

			token = COM_Parse(&s);
			if (!skip_depth) {
				ping = CG_ParseLayoutInt(token);
				if (ping > 999)
					ping = 999;
			}

			token = COM_Parse(&s);

			if (!skip_depth) {

				cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
				cgi.SCR_DrawFontString(G_Fmt("{}", score).data(), x, y - (font_y_offset * scale), scale, value == playernum ? alt_color : rgba_white, true, text_align_t::LEFT);
				x += 3 * 9 * scale;
				cgi.SCR_DrawFontString(G_Fmt("{}", ping).data(), x, y - (font_y_offset * scale), scale, value == playernum ? alt_color : rgba_white, true, text_align_t::LEFT);
				x += 3 * 9 * scale;
				cgi.SCR_DrawFontString(cgi.CL_GetClientName(value), x, y - (font_y_offset * scale), scale, value == playernum ? alt_color : rgba_white, true, text_align_t::LEFT);
				cgi.SCR_SetAltTypeface(false);

				if (*token) {
					cgi.Draw_GetPicSize(&w, &h, token);
					cgi.SCR_DrawPic(x - ((w + 2) * scale), y, w * scale, h * scale, token);
				}
			}
			continue;
		}

		if (!strcmp(token, "picn")) {   // draw a pic from a name
			token = COM_Parse(&s);
			if (!skip_depth) {
				//muff: hoo boy, another little hacky hack
				if (strstr(token, "/players/")) {
					w = h = 32;
					
				} else if (!strcmp(token, "wheel/p_compass_selected")) {
					w = h = 12;
					
				} else {
					cgi.Draw_GetPicSize(&w, &h, token);
				}
				cgi.SCR_DrawPic(x, y, w * scale, h * scale, token);
			}
			continue;
		}

		if (!strcmp(token, "num")) {   // draw a number
			token = COM_Parse(&s);
			if (!skip_depth)
				width = CG_ParseLayoutInt(token);
			token = COM_Parse(&s);
			if (!skip_depth) {
				int32_t stat = 0;
				if (!CG_TryLayoutStatIndex(token, stat))
					cgi.Com_Error("Bad num stat index");

				value = ps->stats[stat];
				if (value != -999) {
					if (stat == STAT_COUNTDOWN)
						CG_DrawFieldHudCentered(hud_vrect, y, 0, width, value, scale);
					else
						CG_DrawField(x, y, 0, width, value, scale);
				}
			}
			continue;
		}
		// [Paril-KEX] special handling for the lives number
		else if (!strcmp(token, "lives_num")) {
			token = COM_Parse(&s);
			if (!skip_depth) {
				int32_t stat = 0;
				if (!CG_TryLayoutStatIndex(token, stat))
					cgi.Com_Error("Bad lives_num stat index");

				value = ps->stats[stat];
				CG_DrawField(x, y, value <= 2 ? flash_frame : 0, 1, max(0, value - 2), scale);
			}
			continue;
		}

		//muff: client-side hacky hacks - don't show vitals if spectating
		if (!ps->stats[STAT_SPECTATOR] || ps->stats[STAT_CHASE]) {
			if (!strcmp(token, "hnum")) {
				// health number
				if (!skip_depth) {
					int     color;

					value = ps->stats[STAT_HEALTH];
					width = value > 999 ? 4 : 3;
					if (value > 25)
						color = 0;  // green
					else if (value > 0)
						color = flash_frame;      // flash
					else
						color = 1;
					if (ps->stats[STAT_FLASHES] & 1) {
						int delta = (width - 3) * 16;
						//cgi.Draw_GetPicSize(&w, &h, "field_3");
						w = 48;
						h = 24;
						w += delta;
						cgi.SCR_DrawPic(x - delta, y, w * scale, h * scale, "field_3");
					}

					CG_DrawField(x, y, color, width, value, scale);
				}
				continue;
			}

			if (!strcmp(token, "anum")) {
				// ammo number
				if (!skip_depth) {
					int     color;

					width = 3;
					value = ps->stats[STAT_AMMO];

					int32_t min_ammo = cgi.CL_GetWarnAmmoCount(ps->stats[STAT_ACTIVE_WEAPON]);

					if (!min_ammo)
						min_ammo = 5; // back compat

					if (value > min_ammo)
						color = 0;  // green
					else if (value >= 0)
						color = flash_frame;      // flash
					else
						continue;   // negative number = don't show
					if (ps->stats[STAT_FLASHES] & 4) {
						cgi.Draw_GetPicSize(&w, &h, "field_3");
						cgi.SCR_DrawPic(x, y, w * scale, h * scale, "field_3");
					}

					CG_DrawField(x, y, color, width, value, scale);
				}
				continue;
			}

			if (!strcmp(token, "rnum")) {
				// armor number
				if (!skip_depth) {
					int     color;

					width = 3;
					value = ps->stats[STAT_ARMOR];
					if (value < 0)
						continue;

					color = 0;  // green
					if (ps->stats[STAT_FLASHES] & 2) {
						cgi.Draw_GetPicSize(&w, &h, "field_3");
						cgi.SCR_DrawPic(x, y, w * scale, h * scale, "field_3");
					}

					CG_DrawField(x, y, color, width, value, scale);
				}
				continue;
			}
		}
		if (!strcmp(token, "stat_string")) {
			token = COM_Parse(&s);

			if (!skip_depth) {
				int32_t stat = 0;
				if (!CG_TryLayoutStatIndex(token, stat))
					cgi.Com_Error("Bad stat_string index");
				index = ps->stats[stat];

				if (cgi.CL_ServerProtocol() <= PROTOCOL_VERSION_3XX)
					index = CS_REMAP(index).start / CS_MAX_STRING_LENGTH;

				if (index < 0 || index >= MAX_CONFIGSTRINGS)
					cgi.Com_Error("Bad stat_string index");

				const char *str = cgi.get_configstring(index);

				if (!scr_usekfont->integer)
					CG_DrawString(x, y, scale, str);
				else {
					cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
					cgi.SCR_DrawFontString(str, x, y - (font_y_offset * scale), scale, rgba_white, true, text_align_t::LEFT);
					cgi.SCR_SetAltTypeface(false);
				}
			}
			continue;
		}

		// Q2Eaks alt color stat string
		if (!strcmp(token, "stat_string2")) {
			token = COM_Parse(&s);

			if (!skip_depth) {
				index = CG_ParseLayoutStatIndex(token);
				index = ps->stats[index];

				if (cgi.CL_ServerProtocol() <= PROTOCOL_VERSION_3XX)
					index = CS_REMAP(index).start / CS_MAX_STRING_LENGTH;

				if (index < 0 || index >= MAX_CONFIGSTRINGS)
					cgi.Com_Error("Bad stat_string index");
				if (!scr_usekfont->integer)
					CG_DrawString(x, y, scale, cgi.get_configstring(index));
				else {
					cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
					cgi.SCR_DrawFontString(cgi.get_configstring(index), x, y - (font_y_offset * scale), scale, alt_color, true, text_align_t::LEFT);
					cgi.SCR_SetAltTypeface(false);
				}
			}
			continue;
		}

		if (!strcmp(token, "cstring")) {
			token = COM_Parse(&s);
			if (!skip_depth) {
				cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
				CG_DrawHUDString(token, x, y, hx * 2 * scale, 0, scale);
				cgi.SCR_SetAltTypeface(false);
			}
			continue;
		}

		if (!strcmp(token, "string")) {
			token = COM_Parse(&s);
			if (!skip_depth) {
				if (!scr_usekfont->integer)
					CG_DrawString(x, y, scale, token);
				else {
					cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
					cgi.SCR_DrawFontString(token, x, y - (font_y_offset * scale), scale, rgba_white, true, text_align_t::LEFT);
					cgi.SCR_SetAltTypeface(false);
				}
			}
			continue;
		}

		if (!strcmp(token, "cstring2")) {
			token = COM_Parse(&s);
			if (!skip_depth) {
				cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
				CG_DrawHUDString(token, x, y, hx * 2 * scale, 0x80, scale);
				cgi.SCR_SetAltTypeface(false);
			}
			continue;
		}

		if (!strcmp(token, "string2")) {
			token = COM_Parse(&s);
			if (!skip_depth) {
				if (!scr_usekfont->integer)
					CG_DrawString(x, y, scale, token, true);
				else {
					cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
					cgi.SCR_DrawFontString(token, x, y - (font_y_offset * scale), scale, alt_color, true, text_align_t::LEFT);
					cgi.SCR_SetAltTypeface(false);
				}
			}
			continue;
		}

		if (!strcmp(token, "if")) {
			// if stmt
			token = COM_Parse(&s);

			if_depth++;

			// skip to endif
			int32_t stat = 0;
			if (!skip_depth && (!CG_TryLayoutStatIndex(token, stat) || !ps->stats[stat])) {
				skip_depth = true;
				endif_depth = if_depth;
			}

			continue;
		}

		if (!strcmp(token, "ifgef")) {
			// if stmt
			token = COM_Parse(&s);

			if_depth++;

			// skip to endif
			if (!skip_depth && cgi.CL_ServerFrame() < CG_ParseLayoutInt(token)) {
				skip_depth = true;
				endif_depth = if_depth;
			}

			continue;
		}

		if (!strcmp(token, "endif")) {
			if (skip_depth && (if_depth == endif_depth))
				skip_depth = false;

			if_depth--;

			if (if_depth < 0)
				cgi.Com_Error("endif without matching if");

			continue;
		}

		// localization stuff
		if (!strcmp(token, "loc_stat_string")) {
			token = COM_Parse(&s);

			if (!skip_depth) {
				index = CG_ParseLayoutStatIndex(token);
				index = ps->stats[index];

				if (cgi.CL_ServerProtocol() <= PROTOCOL_VERSION_3XX)
					index = CS_REMAP(index).start / CS_MAX_STRING_LENGTH;

				if (index < 0 || index >= MAX_CONFIGSTRINGS)
					cgi.Com_Error("Bad stat_string index");
				if (!scr_usekfont->integer)
					CG_DrawString(x, y, scale, cgi.Localize(cgi.get_configstring(index), nullptr, 0));
				else {
					cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
					cgi.SCR_DrawFontString(cgi.Localize(cgi.get_configstring(index), nullptr, 0), x, y - (font_y_offset * scale), scale, rgba_white, true, text_align_t::LEFT);
					cgi.SCR_SetAltTypeface(false);
				}
			}
			continue;
		}

		if (!strcmp(token, "loc_stat_rstring")) {
			token = COM_Parse(&s);

			if (!skip_depth) {
				index = CG_ParseLayoutStatIndex(token);
				index = ps->stats[index];

				if (cgi.CL_ServerProtocol() <= PROTOCOL_VERSION_3XX)
					index = CS_REMAP(index).start / CS_MAX_STRING_LENGTH;

				if (index < 0 || index >= MAX_CONFIGSTRINGS)
					cgi.Com_Error("Bad stat_string index");
				const char *s = cgi.Localize(cgi.get_configstring(index), nullptr, 0);
				if (!scr_usekfont->integer)
					CG_DrawString(x - (strlen(s) * CONCHAR_WIDTH * scale), y, scale, s);
				else {
					cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
					vec2_t size = cgi.SCR_MeasureFontString(s, scale);
					cgi.SCR_DrawFontString(s, x - size.x, y - (font_y_offset * scale), scale, rgba_white, true, text_align_t::LEFT);
					cgi.SCR_SetAltTypeface(false);
				}
			}
			continue;
		}

		if (!strcmp(token, "loc_stat_cstring")) {
			token = COM_Parse(&s);

			if (!skip_depth) {
				index = CG_ParseLayoutStatIndex(token);
				index = ps->stats[index];

				if (cgi.CL_ServerProtocol() <= PROTOCOL_VERSION_3XX)
					index = CS_REMAP(index).start / CS_MAX_STRING_LENGTH;

				if (index < 0 || index >= MAX_CONFIGSTRINGS)
					cgi.Com_Error("Bad stat_string index");
				cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
				CG_DrawHUDString(cgi.Localize(cgi.get_configstring(index), nullptr, 0), x, y, hx * 2 * scale, 0, scale);
				cgi.SCR_SetAltTypeface(false);
			}
			continue;
		}

		if (!strcmp(token, "loc_stat_cstring2")) {
			token = COM_Parse(&s);

			if (!skip_depth) {
				index = CG_ParseLayoutStatIndex(token);
				index = ps->stats[index];

				if (cgi.CL_ServerProtocol() <= PROTOCOL_VERSION_3XX)
					index = CS_REMAP(index).start / CS_MAX_STRING_LENGTH;

				if (index < 0 || index >= MAX_CONFIGSTRINGS)
					cgi.Com_Error("Bad stat_string index");
				cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
				CG_DrawHUDString(cgi.Localize(cgi.get_configstring(index), nullptr, 0), x, y, hx * 2 * scale, 0x80, scale);
				cgi.SCR_SetAltTypeface(false);
			}
			continue;
		}

		static char arg_tokens[MAX_LOCALIZATION_ARGS + 1][MAX_TOKEN_CHARS];
		static const char *arg_buffers[MAX_LOCALIZATION_ARGS];

		if (!strcmp(token, "loc_cstring")) {
			int32_t num_args = CG_ParseLayoutLocalizationArgCount(COM_Parse(&s));

			// parse base
			token = COM_Parse(&s);
			Q_strlcpy(arg_tokens[0], token, sizeof(arg_tokens[0]));

			// parse args
			for (size_t i = 0; i < num_args; i++) {
				token = COM_Parse(&s);
				Q_strlcpy(arg_tokens[1 + i], token, sizeof(arg_tokens[0]));
				arg_buffers[i] = arg_tokens[1 + i];
			}

			if (!skip_depth) {
				cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
				CG_DrawHUDString(cgi.Localize(arg_tokens[0], arg_buffers, num_args), x, y, hx * 2 * scale, 0, scale);
				cgi.SCR_SetAltTypeface(false);
			}
			continue;
		}

		if (!strcmp(token, "loc_string")) {
			int32_t num_args = CG_ParseLayoutLocalizationArgCount(COM_Parse(&s));

			// parse base
			token = COM_Parse(&s);
			Q_strlcpy(arg_tokens[0], token, sizeof(arg_tokens[0]));

			// parse args
			for (size_t i = 0; i < num_args; i++) {
				token = COM_Parse(&s);
				Q_strlcpy(arg_tokens[1 + i], token, sizeof(arg_tokens[0]));
				arg_buffers[i] = arg_tokens[1 + i];
			}

			if (!skip_depth) {
				if (!scr_usekfont->integer)
					CG_DrawString(x, y, scale, cgi.Localize(arg_tokens[0], arg_buffers, num_args));
				else {
					cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
					cgi.SCR_DrawFontString(cgi.Localize(arg_tokens[0], arg_buffers, num_args), x, y - (font_y_offset * scale), scale, rgba_white, true, text_align_t::LEFT);
					cgi.SCR_SetAltTypeface(false);
				}
			}
			continue;
		}

		if (!strcmp(token, "loc_cstring2")) {
			int32_t num_args = CG_ParseLayoutLocalizationArgCount(COM_Parse(&s));

			// parse base
			token = COM_Parse(&s);
			Q_strlcpy(arg_tokens[0], token, sizeof(arg_tokens[0]));

			// parse args
			for (size_t i = 0; i < num_args; i++) {
				token = COM_Parse(&s);
				Q_strlcpy(arg_tokens[1 + i], token, sizeof(arg_tokens[0]));
				arg_buffers[i] = arg_tokens[1 + i];
			}

			if (!skip_depth) {
				cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
				CG_DrawHUDString(cgi.Localize(arg_tokens[0], arg_buffers, num_args), x, y, hx * 2 * scale, 0x80, scale);
				cgi.SCR_SetAltTypeface(false);
			}
			continue;
		}

		if (!strcmp(token, "loc_string2") || !strcmp(token, "loc_rstring2") ||
			!strcmp(token, "loc_string") || !strcmp(token, "loc_rstring")) {
			bool green = token[strlen(token) - 1] == '2';
			bool rightAlign = !Q_strncasecmp(token, "loc_rstring", strlen("loc_rstring"));
			int32_t num_args = CG_ParseLayoutLocalizationArgCount(COM_Parse(&s));

			// parse base
			token = COM_Parse(&s);
			Q_strlcpy(arg_tokens[0], token, sizeof(arg_tokens[0]));

			// parse args
			for (size_t i = 0; i < num_args; i++) {
				token = COM_Parse(&s);
				Q_strlcpy(arg_tokens[1 + i], token, sizeof(arg_tokens[0]));
				arg_buffers[i] = arg_tokens[1 + i];
			}

			if (!skip_depth) {
				const char *locStr = cgi.Localize(arg_tokens[0], arg_buffers, num_args);
				int xOffs = 0;
				if (rightAlign) {
					xOffs = scr_usekfont->integer ? cgi.SCR_MeasureFontString(locStr, scale).x : (strlen(locStr) * CONCHAR_WIDTH * scale);
				}

				if (!scr_usekfont->integer)
					CG_DrawString(x - xOffs, y, scale, locStr, green);
				else {
					cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
					cgi.SCR_DrawFontString(locStr, x - xOffs, y - (font_y_offset * scale), scale, green ? alt_color : rgba_white, true, text_align_t::LEFT);
					cgi.SCR_SetAltTypeface(false);
				}
			}
			continue;
		}

		// draw time remaining
		if (!strcmp(token, "time_limit")) {
			// end frame
			token = COM_Parse(&s);

			if (!skip_depth) {
				int32_t end_frame = CG_ParseLayoutInt(token);

				if (end_frame < cgi.CL_ServerFrame())
					continue;

				uint64_t remaining_ms = (end_frame - cgi.CL_ServerFrame()) * cgi.frame_time_ms;

				const bool green = true;
				arg_buffers[0] = G_Fmt("{:02}:{:02}", (remaining_ms / 1000) / 60, (remaining_ms / 1000) % 60).data();

				const char *locStr = cgi.Localize("$g_score_time", arg_buffers, 1);
				int xOffs = scr_usekfont->integer ? cgi.SCR_MeasureFontString(locStr, scale).x : (strlen(locStr) * CONCHAR_WIDTH * scale);
				if (!scr_usekfont->integer)
					CG_DrawString(x - xOffs, y, scale, locStr, green);
				else {
					cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
					cgi.SCR_DrawFontString(locStr, x - xOffs, y - (font_y_offset * scale), scale, green ? alt_color : rgba_white, true, text_align_t::LEFT);
					cgi.SCR_SetAltTypeface(false);
				}
			}
		}

		// draw client dogtag
		if (!strcmp(token, "dogtag")) {
			token = COM_Parse(&s);

			if (!skip_depth) {
				value = CG_ParseLayoutClientIndex(token);

				const std::string_view path = G_Fmt("/tags/{}", cgi.CL_GetClientDogtag(value));
				cgi.SCR_DrawPic(x, y, 198 * scale, 32 * scale, path.data());
			}
		}

		if (!strcmp(token, "start_table")) {
			token = COM_Parse(&s);
			value = CG_ParseLayoutInt(token);

			if (value < 0 || value > static_cast<int32_t>(q_countof(hud_temp.table_rows[0].table_cells)))
				cgi.Com_Error("table too big");

			if (!skip_depth) {
				hud_temp.num_columns = value;
				hud_temp.num_rows = 1;

				for (int i = 0; i < value; i++)
					hud_temp.column_widths[i] = 0;
			}

			for (int i = 0; i < value; i++) {
				token = COM_Parse(&s);
				if (!skip_depth) {
					token = cgi.Localize(token, nullptr, 0);
					Q_strlcpy(hud_temp.table_rows[0].table_cells[i].text, token, sizeof(hud_temp.table_rows[0].table_cells[i].text));
					hud_temp.column_widths[i] = max(hud_temp.column_widths[i], (size_t)cgi.SCR_MeasureFontString(hud_temp.table_rows[0].table_cells[i].text, scale).x);
				}
			}
		}

		if (!strcmp(token, "table_row")) {
			token = COM_Parse(&s);
			value = CG_ParseLayoutInt(token);

			if (value < 0 || value > static_cast<int32_t>(q_countof(hud_temp.table_rows[0].table_cells)))
				cgi.Com_Error("table too big");

			if (!skip_depth) {
				if (value > hud_temp.num_columns || hud_temp.num_rows >= q_countof(hud_temp.table_rows)) {
					cgi.Com_Error("table too big");
					return;
				}
			}

			auto *row = skip_depth ? nullptr : &hud_temp.table_rows[hud_temp.num_rows];

			for (int i = 0; i < value; i++) {
				token = COM_Parse(&s);
				if (!skip_depth) {
					Q_strlcpy(row->table_cells[i].text, token, sizeof(row->table_cells[i].text));
					hud_temp.column_widths[i] = max(hud_temp.column_widths[i], (size_t)cgi.SCR_MeasureFontString(row->table_cells[i].text, scale).x);
				}
			}

			if (!skip_depth) {
				for (int i = value; i < hud_temp.num_columns; i++)
					row->table_cells[i].text[0] = '\0';

				hud_temp.num_rows++;
			}
		}

		if (!strcmp(token, "draw_table")) {
			if (!skip_depth) {
				// in scaled pixels, incl padding between elements
				uint32_t total_inner_table_width = 0;

				for (int i = 0; i < hud_temp.num_columns; i++) {
					if (i != 0)
						total_inner_table_width += cgi.SCR_MeasureFontString(" ", scale).x;

					total_inner_table_width += hud_temp.column_widths[i];
				}

				// in scaled pixels
				uint32_t total_table_height = hud_temp.num_rows * (CONCHAR_WIDTH + font_y_offset) * scale;

				CG_DrawTable(x, y, total_inner_table_width, total_table_height, scale);
			}
		}

		if (!strcmp(token, "stat_pname")) {
			token = COM_Parse(&s);

			if (!skip_depth) {
				text_align_t align = text_align_t::LEFT;

				index = CG_ParseLayoutStatIndex(token);

				//muff: hacky hacks - move crosshair id text to 160, align centrally
				if (index == STAT_CROSSHAIR_ID_VIEW) {
					x = (hud_vrect.x + hud_vrect.width / 2 + 160 - hx) * scale;
					align = text_align_t::CENTER;
				}

				index = ps->stats[index] - 1;
				if (index < 0 || index >= MAX_CLIENTS)
					continue;

				if (!scr_usekfont->integer)
					CG_DrawString(x, y, scale, cgi.CL_GetClientName(index));
				else {
					cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
					cgi.SCR_DrawFontString(cgi.CL_GetClientName(index), x, y - (font_y_offset * scale), scale, rgba_white, true, align);
					cgi.SCR_SetAltTypeface(false);
				}
			}
			continue;
		}

		if (!strcmp(token, "health_bars")) {
			if (skip_depth)
				continue;

			const byte *stat = reinterpret_cast<const byte *>(&ps->stats[STAT_HEALTH_BARS]);
			const char *name = cgi.Localize(cgi.get_configstring(CONFIG_HEALTH_BAR_NAME), nullptr, 0);
			cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
			CG_DrawHUDString(name, (hud_vrect.x + hud_vrect.width / 2 + -160) * scale, y, (320 / 2) * 2 * scale, 0, scale);
			cgi.SCR_SetAltTypeface(false);
			float bar_width = ((hud_vrect.width * scale) - (hud_safe.x * 2)) * 0.50f;
			float bar_height = 4 * scale;

			y += cgi.SCR_FontLineHeight(scale);

			float x = ((hud_vrect.x + (hud_vrect.width * 0.5f)) * scale) - (bar_width * 0.5f);

			// 2 health bars, hardcoded
			for (size_t i = 0; i < 2; i++, stat++) {
				if (!(*stat & 0b10000000))
					continue;

				float percent = (*stat & 0b01111111) / 127.f;

				cgi.SCR_DrawColorPic(x, y, bar_width + scale, bar_height + scale, "_white", rgba_black);

				if (percent > 0)
					cgi.SCR_DrawColorPic(x, y, bar_width * percent, bar_height, "_white", rgba_red);
				if (percent < 1)
					cgi.SCR_DrawColorPic(x + (bar_width * percent), y, bar_width * (1.f - percent), bar_height, "_white", { 80, 80, 80, 255 });

				y += bar_height * 3;
			}
		}

		if (!strcmp(token, "story")) {
			const char *story_str = cgi.get_configstring(CONFIG_STORY_SCORELIMIT);

			if (!*story_str)
				continue;

			const char *localized = cgi.Localize(story_str, nullptr, 0);
			vec2_t size = cgi.SCR_MeasureFontString(localized, scale);
			float centerx = ((hud_vrect.x + (hud_vrect.width * 0.5f)) * scale);
			float centery = ((hud_vrect.y + (hud_vrect.height * 0.5f)) * scale) - (size.y * 0.5f);

			cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);
			cgi.SCR_DrawFontString(localized, centerx, centery, scale, rgba_white, true, text_align_t::CENTER);
			cgi.SCR_SetAltTypeface(false);
		}
	}

	if (skip_depth)
		cgi.Com_Error("if with no matching endif");
}

static cvar_t *cl_skipHud;
static cvar_t *cl_paused;

/*
================
CL_DrawInventory
================
*/
constexpr size_t DISPLAY_ITEMS = 19;

static void CG_DrawInventory(const player_state_t *ps, const std::array<int16_t, MAX_ITEMS> &inventory, vrect_t hud_vrect, int32_t scale) {
	int     i;
	int     num, selected_num, item;
	int     index[MAX_ITEMS];
	int     x, y;
	int     width, height;
	int     selected;
	int     top;

	selected = ps->stats[STAT_SELECTED_ITEM];

	num = 0;
	selected_num = 0;
	for (i = 0; i < MAX_ITEMS; i++) {
		if (i == selected) {
			selected_num = num;
		}
		if (inventory[i]) {
			index[num] = i;
			num++;
		}
	}

	// determine scroll point
	top = selected_num - DISPLAY_ITEMS / 2;
	if (num - top < DISPLAY_ITEMS)
		top = num - DISPLAY_ITEMS;
	if (top < 0)
		top = 0;

	x = hud_vrect.x * scale;
	y = hud_vrect.y * scale;
	width = hud_vrect.width;
	height = hud_vrect.height;

	x += ((width / 2) - (256 / 2)) * scale;
	y += ((height / 2) - (216 / 2)) * scale;

	int pich, picw;
	cgi.Draw_GetPicSize(&picw, &pich, "inventory");
	cgi.SCR_DrawPic(x, y + 8 * scale, picw * scale, pich * scale, "inventory");

	y += 27 * scale;
	x += 22 * scale;

	for (i = top; i < num && i < top + DISPLAY_ITEMS; i++) {
		item = index[i];
		if (item == selected) // draw a blinky cursor by the selected item
		{
			if ((cgi.CL_ClientRealTime() * 10) & 1)
				cgi.SCR_DrawChar(x - 8, y, scale, 15, false);
		}

		if (!scr_usekfont->integer) {
			CG_DrawString(x, y, scale,
				G_Fmt("{:3} {}", inventory[item],
					cgi.Localize(cgi.get_configstring(CS_ITEMS + item), nullptr, 0)).data(),
				item == selected, false);
		} else {
			const char *string = G_Fmt("{}", inventory[item]).data();
			cgi.SCR_DrawFontString(string, x + (216 * scale) - (16 * scale), y - (font_y_offset * scale), scale, (item == selected) ? alt_color : rgba_white, true, text_align_t::RIGHT);

			string = cgi.Localize(cgi.get_configstring(CS_ITEMS + item), nullptr, 0);
			cgi.SCR_DrawFontString(string, x + (16 * scale), y - (font_y_offset * scale), scale, (item == selected) ? alt_color : rgba_white, true, text_align_t::LEFT);
		}

		y += 8 * scale;
	}
}

extern uint64_t cgame_init_time;

// Engine player_state.team_id values (matches vanilla ctf_team / P_EngineTeamIndex).
constexpr uint8_t ENGINE_TEAM_RED = 1;
constexpr uint8_t ENGINE_TEAM_BLUE = 2;

// Game type constants (matching g_local.h enum gametype_t)
constexpr int GT_TDM = 3;
constexpr int GT_CTF = 4;

/*
================
CG_DrawTeamBorder

Draws a colored border around the screen to indicate team membership
in TDM/CTF modes.
================
*/
static void CG_DrawTeamBorder(const player_state_t *ps, vrect_t hud_vrect, int32_t scale) {
	// Check if feature is enabled
	if (!cl_teamBorder->integer)
		return;

	// Only draw if player is on a team
	if (ps->team_id != ENGINE_TEAM_RED && ps->team_id != ENGINE_TEAM_BLUE)
		return;

	// Don't draw if HUD is hidden
	if (ps->stats[STAT_LAYOUTS] & LAYOUTS_HIDE_HUD)
		return;

	// Get border settings
	int32_t border_width = std::clamp(cl_teamBorderWidth->integer, 1, 20) * scale;
	uint8_t alpha = (uint8_t)std::clamp(cl_teamBorderAlpha->integer, 0, 255);

	// Define team colors with configurable alpha
	rgba_t team_red_color{ 255, 50, 50, alpha };
	rgba_t team_blue_color{ 50, 100, 255, alpha };

	rgba_t border_color = (ps->team_id == ENGINE_TEAM_RED) ? team_red_color : team_blue_color;

	// Calculate screen dimensions
	int32_t x = hud_vrect.x * scale;
	int32_t y = hud_vrect.y * scale;
	int32_t w = hud_vrect.width * scale;
	int32_t h = hud_vrect.height * scale;

	// Draw bottom border only
	cgi.SCR_DrawColorPic(x, y + h - border_width, w, border_width, "_white", border_color);
}

static int32_t CG_HudLayoutRightX(const vrect_t &hud_vrect, int32_t xr, int32_t scale, int32_t safe_x)
{
	return (hud_vrect.x + hud_vrect.width + xr) * scale - safe_x;
}

static int32_t CG_HudLayoutBottomY(const vrect_t &hud_vrect, int32_t yb, int32_t scale, int32_t safe_y)
{
	return (hud_vrect.y + hud_vrect.height + yb) * scale - safe_y;
}

static bool CG_MiniscorePicIsPlayerSkin(int32_t pic_index)
{
	if (pic_index <= 0)
		return false;

	const char *const pic = cgi.get_configstring(CS_IMAGES + pic_index);
	// Player head icons from SetMiniScoreStats: /players/{skin}_i
	return pic && strncmp(pic, "/players/", 9) == 0;
}

static void CG_DrawMiniscorePicSized(int32_t x, int32_t y, int32_t pic_index, int32_t scale)
{
	const char *const pic = cgi.get_configstring(CS_IMAGES + pic_index);

	if (!pic || !*pic)
		return;

	const int32_t size = muffmode::hud::kMiniscorePicSize * scale;
	cgi.SCR_DrawPic(x, y, size, size, pic);
}

static void CG_DrawFfaMiniscoreRow(const player_state_t *ps, const vrect_t &hud_vrect, int32_t yb, int32_t scale, int32_t safe_x, int32_t safe_y,
	player_stat_t pic_stat, player_stat_t score_stat, player_stat_t highlight_stat)
{
	const int32_t pic_index = ps->stats[pic_stat];
	const int32_t score = ps->stats[score_stat];
	const int32_t highlight_index = ps->stats[highlight_stat];

	if (!CG_MiniscorePicIsPlayerSkin(pic_index))
		return;

	if (pic_index <= 0 || score == -999)
		return;

	const int32_t y = CG_HudLayoutBottomY(hud_vrect, yb, scale, safe_y);
	const int32_t pic_x = CG_HudLayoutRightX(hud_vrect, muffmode::hud::kMiniscorePicXr, scale, safe_x);
	const int32_t num_x = CG_HudLayoutRightX(hud_vrect, muffmode::hud::kMiniscoreNumXr, scale, safe_x);

	CG_DrawMiniscorePicSized(pic_x, y, pic_index, scale);
	CG_DrawField(num_x, y, 0, muffmode::hud::kMiniscoreNumFieldWidth, score, scale);

	if (highlight_index <= 0)
		return;

	// i_ctfj frame: native size, centred on the 24×24 skin icon (layout xr/yb offsets target full-size pics).
	const char *const highlight_pic = cgi.get_configstring(CS_IMAGES + highlight_index);
	if (!highlight_pic || !*highlight_pic)
		return;

	const int32_t pic_size = muffmode::hud::kMiniscorePicSize * scale;
	int32_t hw = 0, hh = 0;
	cgi.Draw_GetPicSize(&hw, &hh, highlight_pic);
	cgi.SCR_DrawPic(
		pic_x + (pic_size - hw * scale) / 2,
		y + (pic_size - hh * scale) / 2,
		hw * scale,
		hh * scale,
		highlight_pic);
}

void CG_DrawMuffModeHudEnhancements(const player_state_t *ps, vrect_t hud_vrect, vrect_t hud_safe, int32_t scale, int32_t playernum)
{
	(void)playernum;

	if (ps->stats[STAT_LAYOUTS] & LAYOUTS_HIDE_HUD)
		return;

	// FFA/Duel/RR skin-icon miniscore: omitted from CS_STATUSBAR for vanilla clients.
	CG_DrawFfaMiniscoreRow(ps, hud_vrect, muffmode::hud::kBottomMiniscoreRow1Yb, scale, hud_safe.x, hud_safe.y,
		STAT_MINISCORE_FIRST_PIC, STAT_MINISCORE_FIRST_SCORE, STAT_MINISCORE_FIRST_POS);
	CG_DrawFfaMiniscoreRow(ps, hud_vrect, muffmode::hud::kBottomMiniscoreRow2Yb, scale, hud_safe.x, hud_safe.y,
		STAT_MINISCORE_SECOND_PIC, STAT_MINISCORE_SECOND_SCORE, STAT_MINISCORE_SECOND_POS);
}

void CG_DrawHUD(int32_t isplit, const cg_server_data_t *data, vrect_t hud_vrect, vrect_t hud_safe, int32_t scale, int32_t playernum, const player_state_t *ps) {
	if (cgi.CL_InAutoDemoLoop()) {
		if (cl_paused->integer) return; // demo is paused, menu is open

		uint64_t time = cgi.CL_ClientRealTime() - cgame_init_time;
		if (time < 20000 &&
			(time % 4000) < 2000)
			cgi.SCR_DrawFontString(cgi.Localize("$m_eou_press_button", nullptr, 0), hud_vrect.width * 0.5f * scale, (hud_vrect.height - 64.f) * scale, scale, rgba_green, true, text_align_t::CENTER);
		return;
	}

	// draw HUD
	if (!cl_skipHud->integer && !(ps->stats[STAT_LAYOUTS] & LAYOUTS_HIDE_HUD))
		CG_ExecuteLayoutString(cgi.get_configstring(CS_STATUSBAR), hud_vrect, hud_safe, scale, playernum, ps);

	// draw centerprint string
	CG_CheckDrawCenterString(ps, hud_vrect, hud_safe, isplit, scale);

	// draw notify
	CG_DrawNotify(isplit, hud_vrect, hud_safe, scale);

	if (!cl_skipHud->integer && !(ps->stats[STAT_LAYOUTS] & LAYOUTS_HIDE_HUD))
		CG_DrawMuffModeHudEnhancements(ps, hud_vrect, hud_safe, scale, playernum);

	// svc_layout still drawn with hud off
	if (ps->stats[STAT_LAYOUTS] & LAYOUTS_LAYOUT)
		CG_ExecuteLayoutString(data->layout, hud_vrect, hud_safe, scale, playernum, ps);

	// inventory too
	if (ps->stats[STAT_LAYOUTS] & LAYOUTS_INVENTORY)
		CG_DrawInventory(ps, data->inventory, hud_vrect, scale);

	// draw team border for TDM/CTF modes
	if (!cl_skipHud->integer)
		CG_DrawTeamBorder(ps, hud_vrect, scale);
}

/*
================
CG_TouchPics

================
*/
void CG_TouchPics() {
	for (auto &nums : sb_nums)
		for (auto &str : nums)
			cgi.Draw_RegisterPic(str);

	cgi.Draw_RegisterPic("inventory");

	font_y_offset = (cgi.SCR_FontLineHeight(1) - CONCHAR_WIDTH) / 2;
}

void CG_InitScreen() {
	cl_paused = cgi.cvar("paused", "0", CVAR_NOFLAGS);
	cl_skipHud = cgi.cvar("cl_skipHud", "0", CVAR_ARCHIVE);
	scr_usekfont = cgi.cvar("scr_usekfont", "1", CVAR_NOFLAGS);

	ui_acc_alttypeface = cgi.cvar("ui_acc_alttypeface", "0", CVAR_NOFLAGS);
	CG_InitMessages();

	// team border cvars
	cl_teamBorder = cgi.cvar("cl_teamBorder", "1", CVAR_ARCHIVE);
	cl_teamBorderWidth = cgi.cvar("cl_teamBorderWidth", "1", CVAR_ARCHIVE);
	cl_teamBorderAlpha = cgi.cvar("cl_teamBorderAlpha", "150", CVAR_ARCHIVE);
}
