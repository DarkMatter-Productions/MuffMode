// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "messages.h"
#include "hud_text.h"

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr rgba_t kAltColor{ 112, 255, 52, 255 };
constexpr int32_t kConCharWidth = 8;
constexpr size_t kMaxCenterPrints = 4;
constexpr size_t kMaxNotify = 8;

cvar_t *scr_centertime;
cvar_t *scr_printspeed;
cvar_t *cl_notifytime;
cvar_t *scr_maxlines;
cvar_t *ui_acc_contrast;
cvar_t *ui_acc_alttypeface;

struct CenterPrintBind {
	std::string bind;
	std::string purpose;
};

struct CenterPrint {
	std::vector<CenterPrintBind> binds;
	std::vector<std::string> lines;
	bool        instant = false;
	size_t      current_line = 0;
	size_t      line_count = 0;
	bool        finished = false;
	uint64_t    time_tick = 0;
	uint64_t    time_off = 0;
};

struct NotifyLine {
	std::string message;
	bool        is_active = false;
	bool        is_chat = false;
	uint64_t    time = 0;
};

struct HudMessageState {
	std::array<CenterPrint, kMaxCenterPrints> centers;
	std::optional<size_t> center_index;
	std::array<NotifyLine, kMaxNotify> notify;
};

std::array<HudMessageState, MAX_SPLIT_PLAYERS> hud_messages;

bool ViewingLayout(const player_state_t *ps)
{
	return ps->stats[STAT_LAYOUTS] & (LAYOUTS_LAYOUT | LAYOUTS_INVENTORY);
}

bool InIntermission(const player_state_t *ps)
{
	return ps->stats[STAT_LAYOUTS] & LAYOUTS_INTERMISSION;
}

size_t FindEndOfUTF8Codepoint(const std::string &str, size_t pos)
{
	if (pos >= str.size())
		return std::string::npos;

	for (size_t i = pos; i < str.size(); i++) {
		const char &ch = str[i];

		if ((ch & 0x80) == 0) {
			return i;
		}
		if ((ch & 0xC0) == 0x80) {
			continue;
		}
		return i;
	}

	return std::string::npos;
}

void CheckNotifyExpire(HudMessageState &data)
{
	while (data.notify[0].is_active && data.notify[0].time < cgi.CL_ClientTime()) {
		data.notify[0].is_active = false;

		for (size_t i = 1; i < kMaxNotify; i++)
			if (data.notify[i].is_active)
				std::swap(data.notify[i], data.notify[i - 1]);
	}
}

void AddNotify(HudMessageState &data, const char *msg, bool is_chat)
{
	if (scr_maxlines->integer <= 0)
		return;

	const size_t max_lines = min(kMaxNotify, static_cast<size_t>(scr_maxlines->integer));
	size_t index = 0;

	for (; index < max_lines; index++)
		if (!data.notify[index].is_active)
			break;

	if (index == max_lines) {
		data.notify[0].time = 0;
		CheckNotifyExpire(data);
		index = max_lines - 1;
	}

	data.notify[index].message.assign(msg);
	data.notify[index].is_active = true;
	data.notify[index].is_chat = is_chat;
	data.notify[index].time = cgi.CL_ClientTime() + static_cast<uint64_t>(cl_notifytime->value * 1000);
}

CenterPrint &QueueCenterPrint(int isplit, bool instant)
{
	auto &state = hud_messages[isplit];

	if (!state.center_index.has_value() || instant) {
		state.center_index = 0;

		for (size_t i = 1; i < kMaxCenterPrints; i++)
			state.centers[i].lines.clear();

		return state.centers[0];
	}

	for (size_t i = 1; i < kMaxCenterPrints; i++) {
		auto &center = state.centers[(state.center_index.value() + i) % kMaxCenterPrints];

		if (center.lines.empty())
			return center;
	}

	auto &center = state.centers[state.center_index.value()];
	state.center_index = (state.center_index.value() + 1) % kMaxCenterPrints;
	return center;
}

void DrawCenterString(
	const player_state_t *ps,
	const vrect_t &hud_vrect,
	const vrect_t &hud_safe,
	int isplit,
	int scale,
	CenterPrint &center)
{
	int32_t y = hud_vrect.y * scale;

	if (ViewingLayout(ps))
		y += hud_safe.y;
	else if (center.lines.size() <= 4)
		y += static_cast<int32_t>((hud_vrect.height * 0.2f) * scale);
	else
		y += 48 * scale;

	int line_height = (CG_UseKFont() ? 10 : kConCharWidth) * scale;
	if (ui_acc_alttypeface->integer)
		line_height = static_cast<int>(line_height * 1.5f);

	if (center.instant) {
		for (const auto &line : center.lines) {
			cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer != 0);

			if (ui_acc_contrast->integer && !line.empty()) {
				vec2_t size = cgi.SCR_MeasureFontString(line.c_str(), scale);
				size.x += 10;
				const int bar_y = ui_acc_alttypeface->integer ? y - 8 : y;
				cgi.SCR_DrawColorPic((hud_vrect.x + hud_vrect.width / 2) * scale - (size.x / 2), bar_y, size.x, line_height, "_white", rgba_black);
			}

			CG_DrawHUDString(line.c_str(), (hud_vrect.x + hud_vrect.width / 2 - 160) * scale, y, 320 * scale, 0, scale);
			cgi.SCR_SetAltTypeface(false);

			y += line_height;
		}

		for (const auto &bind : center.binds) {
			y += line_height * 2;
			cgi.SCR_DrawBind(isplit, bind.bind.c_str(), bind.purpose.c_str(), (hud_vrect.x + (hud_vrect.width / 2)) * scale, y, scale);
		}

		if (!center.finished) {
			center.finished = true;
			center.time_off = cgi.CL_ClientRealTime() + static_cast<uint64_t>(scr_centertime->value * 1000);
		}

		return;
	}

	const uint64_t now = cgi.CL_ClientRealTime();

	if (!center.finished && center.time_tick < now) {
		center.time_tick = now + static_cast<uint64_t>(scr_printspeed->value * 1000);
		center.line_count = FindEndOfUTF8Codepoint(center.lines[center.current_line], center.line_count + 1);

		if (center.line_count == std::string::npos) {
			center.current_line++;
			center.line_count = 0;

			if (center.current_line == center.lines.size()) {
				center.current_line--;
				center.finished = true;
				center.time_off = now + static_cast<uint64_t>(scr_centertime->value * 1000);
			}
		}
	}

	char buffer[256] = {};

	for (size_t i = 0; i < center.lines.size(); i++) {
		cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer != 0);

		const auto &line = center.lines[i];

		if (center.finished || i != center.current_line)
			Q_strlcpy(buffer, line.c_str(), sizeof(buffer));
		else
			Q_strlcpy(buffer, line.c_str(), min(center.line_count + 1, sizeof(buffer)));

		int cursor_x = 0;

		if (ui_acc_contrast->integer && !line.empty()) {
			vec2_t size = cgi.SCR_MeasureFontString(line.c_str(), scale);
			size.x += 10;
			const int bar_y = ui_acc_alttypeface->integer ? y - 8 : y;
			cgi.SCR_DrawColorPic((hud_vrect.x + hud_vrect.width / 2) * scale - (size.x / 2), bar_y, size.x, line_height, "_white", rgba_black);
		}

		if (buffer[0])
			cursor_x = CG_DrawHUDString(buffer, (hud_vrect.x + hud_vrect.width / 2 - 160) * scale, y, 320 * scale, 0, scale);
		else
			cursor_x = (hud_vrect.width / 2) * scale;

		cgi.SCR_SetAltTypeface(false);

		if (i == center.current_line && !ui_acc_alttypeface->integer)
			cgi.SCR_DrawChar(cursor_x, y, scale, 10 + ((cgi.CL_ClientRealTime() >> 8) & 1), true);

		y += line_height;

		if (i == center.current_line)
			break;
	}
}

} // namespace

void CG_InitMessages()
{
	scr_centertime = cgi.cvar("scr_centertime", "5.0", CVAR_ARCHIVE);
	scr_printspeed = cgi.cvar("scr_printspeed", "0.04", CVAR_NOFLAGS);
	cl_notifytime = cgi.cvar("cl_notifytime", "5.0", CVAR_ARCHIVE);
	scr_maxlines = cgi.cvar("scr_maxlines", "4", CVAR_ARCHIVE);
	ui_acc_contrast = cgi.cvar("ui_acc_contrast", "0", CVAR_NOFLAGS);
	ui_acc_alttypeface = cgi.cvar("ui_acc_alttypeface", "0", CVAR_NOFLAGS);

	hud_messages = {};
}

void CG_ClearCenterprint(int32_t isplit)
{
	hud_messages[isplit].center_index = {};
}

void CG_ClearNotify(int32_t isplit)
{
	for (auto &msg : hud_messages[isplit].notify)
		msg.is_active = false;
}

void CG_NotifyMessage(int32_t isplit, const char *msg, bool is_chat)
{
	AddNotify(hud_messages[isplit], msg, is_chat);
}

void CG_DrawNotify(int32_t isplit, vrect_t hud_vrect, vrect_t hud_safe, int32_t scale)
{
	auto &data = hud_messages[isplit];

	CheckNotifyExpire(data);

	int y = (hud_vrect.y * scale) + hud_safe.y;

	cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer != 0);

	if (ui_acc_contrast->integer) {
		for (const auto &msg : data.notify) {
			if (!msg.is_active || msg.message.empty())
				break;

			vec2_t size = cgi.SCR_MeasureFontString(msg.message.c_str(), scale);
			size.x += 10;
			cgi.SCR_DrawColorPic((hud_vrect.x * scale) + hud_safe.x - 5, y, size.x, 15 * scale, "_white", rgba_black);
			y += 10 * scale;
		}
	}

	y = (hud_vrect.y * scale) + hud_safe.y;
	for (const auto &msg : data.notify) {
		if (!msg.is_active)
			break;

		cgi.SCR_DrawFontString(
			msg.message.c_str(),
			(hud_vrect.x * scale) + hud_safe.x,
			y,
			scale,
			msg.is_chat ? kAltColor : rgba_white,
			true,
			text_align_t::LEFT);
		y += 10 * scale;
	}

	cgi.SCR_SetAltTypeface(false);

	if (isplit == 0) {
		const char *input_msg = nullptr;
		bool input_team = false;

		if (cgi.CL_GetTextInput(&input_msg, &input_team))
			cgi.SCR_DrawFontString(
				G_Fmt("{}: {}", input_team ? "say_team" : "say", input_msg).data(),
				(hud_vrect.x * scale) + hud_safe.x,
				y,
				scale,
				rgba_white,
				true,
				text_align_t::LEFT);
	}
}

void CG_ParseCenterPrint(const char *str, int isplit, bool instant)
{
	const char *s = nullptr;
	char line[64];
	int i = 0;
	int j = 0;
	int length = 0;

	CenterPrint &center = QueueCenterPrint(isplit, instant);

	center.lines.clear();

	size_t line_start = 0;
	std::string text(str);

	center.binds.clear();

	while (text.compare(0, 6, "%bind:") == 0) {
		const size_t end_of_bind = text.find_first_of('%', 1);

		if (end_of_bind == std::string::npos)
			break;

		const std::string bind = text.substr(6, end_of_bind - 6);

		if (const size_t purpose_index = bind.find_first_of(':'); purpose_index != std::string::npos)
			center.binds.emplace_back(CenterPrintBind{ bind.substr(0, purpose_index), bind.substr(purpose_index + 1) });
		else
			center.binds.emplace_back(CenterPrintBind{ bind });

		text = text.substr(end_of_bind + 1);
	}

	cgi.Com_Print("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");

	s = text.c_str();
	do {
		for (length = 0; length < 40; length++)
			if (s[length] == '\n' || !s[length])
				break;
		for (i = 0; i < (40 - length) / 2; i++)
			line[i] = ' ';

		for (j = 0; j < length; j++)
			line[i++] = s[j];

		line[i] = '\n';
		line[i + 1] = 0;

		cgi.Com_Print(line);

		while (*s && *s != '\n')
			s++;

		if (!*s)
			break;
		s++;
	} while (true);

	cgi.Com_Print("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	CG_ClearNotify(isplit);

	for (size_t line_end = 0;;) {
		line_end = FindEndOfUTF8Codepoint(text, line_end);

		if (line_end == std::string::npos) {
			if (line_start < text.size())
				center.lines.emplace_back(text.c_str() + line_start);
			break;
		}

		const char &ch = text[line_end];

		if (ch == '\n') {
			if (line_end > line_start)
				center.lines.emplace_back(text.c_str() + line_start, line_end - line_start);
			else
				center.lines.emplace_back();
			line_start = line_end + 1;
			line_end++;
			continue;
		}

		line_end++;
	}

	if (center.lines.empty()) {
		center.finished = true;
		return;
	}

	center.time_tick = cgi.CL_ClientRealTime() + static_cast<uint64_t>(scr_printspeed->value * 1000);
	center.instant = instant;
	center.finished = false;
	center.current_line = 0;
	center.line_count = 0;
}

void CG_CheckDrawCenterString(const player_state_t *ps, const vrect_t &hud_vrect, const vrect_t &hud_safe, int isplit, int scale)
{
	if (InIntermission(ps))
		return;

	// Scoreboard (and inventory) use LAYOUTS_LAYOUT / LAYOUTS_INVENTORY; hide centerprint so it
	// does not overlap the overlay (statusbar overlay HUD is already gated by STAT_SHOW_STATUSBAR).
	if (ViewingLayout(ps))
		return;

	auto &data = hud_messages[isplit];
	if (!data.center_index.has_value())
		return;

	auto &center = data.centers[data.center_index.value()];

	if (center.finished && center.time_off < cgi.CL_ClientRealTime()) {
		if (ps->stats[STAT_COUNTDOWN] > 0) {
			center.time_off = cgi.CL_ClientRealTime() + static_cast<uint64_t>(scr_centertime->value * 1000);
		} else {
			center.lines.clear();

			const size_t next_index = (data.center_index.value() + 1) % kMaxCenterPrints;
			auto &next_center = data.centers[next_index];

			if (next_center.lines.empty()) {
				data.center_index.reset();
				return;
			}

			data.center_index = next_index;
			next_center.current_line = next_center.line_count = 0;
		}
	}

	if (!data.center_index.has_value())
		return;

	DrawCenterString(ps, hud_vrect, hud_safe, isplit, scale, data.centers[data.center_index.value()]);
}
