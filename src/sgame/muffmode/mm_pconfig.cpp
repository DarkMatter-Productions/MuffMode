// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_pconfig.h"
#include "muffmode/mm_parse.h"
#include "muffmode/mm_util.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <string>

//=======================================================================
// PLAYER CONFIGS
//=======================================================================

namespace muffmode::pconfig {

constexpr long k_max_player_config_file_length = 0x40000;

bool IsSafeSocialIdChar(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		(c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
}

int ClientSlotNumber(const gentity_t *ent)
{
	const uint32_t max_clients = static_cast<uint32_t>(game.maxclients);
	if (!ent || ent->s.number < 1 || ent->s.number > max_clients)
		return 0;

	return static_cast<int>(ent->s.number - 1);
}

std::string FallbackSocialId(const char *prefix, const gentity_t *ent)
{
	return fmt::format("{}_{}", prefix && *prefix ? prefix : "client", ClientSlotNumber(ent));
}

bool IsReservedWindowsDeviceName(const char *name)
{
	if (!name || !*name)
		return false;

	std::string stem;
	stem.reserve(MAX_INFO_VALUE - 1);
	for (const char *p = name; *p && *p != '.' && stem.size() < MAX_INFO_VALUE - 1; p++)
		stem += *p;

	if (stem.empty())
		return false;

	static constexpr std::array<const char *, 22> reserved_names = {{
		"CON", "PRN", "AUX", "NUL",
		"COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
		"LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
	}};

	for (const char *reserved : reserved_names) {
		if (muffmode::CStringEqualsI(stem.c_str(), reserved))
			return true;
	}

	return false;
}

std::string SanitizeSocialId(const char *src)
{
	if (!src)
		return {};

	std::string out;
	out.reserve(MAX_INFO_VALUE - 1);

	for (const char *cursor = src; *cursor && out.size() < MAX_INFO_VALUE - 1; cursor++) {
		const char c = *cursor;
		// Allow alphanumeric, dash, underscore, and common ID characters.
		// Skip leading dots to avoid ambiguous Windows device/path spellings.
		if (IsSafeSocialIdChar(c) && (c != '.' || !out.empty()))
			out += c;
	}

	while (!out.empty() && out.back() == '.')
		out.pop_back();

	return out;
}

std::string SanitizeConfigCommentText(const char *text)
{
	if (!text || !*text)
		return "Player";

	std::string out;
	for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p && out.size() < MAX_INFO_VALUE - 1; p++) {
		if (*p < ' ' || *p == 0x7F) {
			if (!out.empty() && out.back() != ' ')
				out += ' ';
			continue;
		}

		out += static_cast<char>(*p);
	}

	while (!out.empty() && out.back() == ' ')
		out.pop_back();

	return out.empty() ? "Player" : out;
}

bool RequireCommandArgc(gentity_t *ent, int min_expected, int max_expected, const char *usage)
{
	if (!ent || !ent->client)
		return false;

	if (MM_IsArgcInRangeValid(gi.argc(), min_expected, max_expected))
		return true;

	gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {}\n", usage);
	return false;
}

bool RequireNoCommandArgs(gentity_t *ent)
{
	return RequireCommandArgc(ent, 1, 1, gi.argv(0));
}

bool EnsureDirectory()
{
	const std::filesystem::path dir("baseq2/pcfg");
	std::error_code error;

	std::filesystem::create_directories(dir, error);
	if (error) {
		gi.Com_PrintFmt("{}: Cannot create player config directory \"{}\": {}\n", __FUNCTION__, dir.string(), error.message());
		return false;
	}

	error.clear();
	if (!std::filesystem::is_directory(dir, error)) {
		gi.Com_PrintFmt("{}: Player config path is not a directory: \"{}\"\n", __FUNCTION__, dir.string());
		return false;
	}

	return true;
}

bool ValidateReadableConfig(FILE *file)
{
	if (std::fseek(file, 0, SEEK_END) != 0)
		return false;

	const long file_length = std::ftell(file);
	if (file_length < 0 || file_length > k_max_player_config_file_length)
		return false;

	if (std::fseek(file, 0, SEEK_SET) != 0)
		return false;

	std::string buffer(static_cast<size_t>(file_length), '\0');
	if (!buffer.empty()) {
		const size_t read_length = std::fread(&buffer[0], 1, buffer.size(), file);
		if (read_length != buffer.size())
			return false;
	}

	return true;
}

} // namespace muffmode::pconfig

/*
=============
MM_ClientInitPConfig

Load or create the player's configuration file on connect.
=============
*/
void MM_ClientInitPConfig(gentity_t *ent)
{
	bool file_exists = false;

	if (!ent || !ent->client)
		return;
	if (ent->svflags & SVF_BOT)
		return;

	// Validate and sanitize social_id for filesystem use
	// This prevents crashes from empty or malicious social_id values
	std::string safe_social_id = ent->client->pers.social_id[0]
		? muffmode::pconfig::SanitizeSocialId(ent->client->pers.social_id)
		: muffmode::pconfig::FallbackSocialId("unknown", ent);

	if (safe_social_id.empty() || muffmode::pconfig::IsReservedWindowsDeviceName(safe_social_id.c_str()))
		safe_social_id = muffmode::pconfig::FallbackSocialId("invalid", ent);

	const std::string path = fmt::format("baseq2/pcfg/{}.cfg", safe_social_id);
	const char *name = path.c_str();

	if (!muffmode::pconfig::EnsureDirectory())
		return;

	auto existing_file = muffmode::OpenFile(name, "rb");
	if (existing_file) {
		file_exists = true;

		if (!muffmode::pconfig::ValidateReadableConfig(existing_file.get())) {
			gi.Com_PrintFmt("{}: Player config load error for \"{}\", discarding.\n", __FUNCTION__, name);
			return;
		}
	}

	if (!file_exists) {
		auto new_file = muffmode::OpenFile(name, "wb");
		if (new_file) {
			const std::string header = fmt::format("// {}'s Player Config\n// Generated by Muff Mode\n",
				muffmode::pconfig::SanitizeConfigCommentText(ent->client->resp.netname));

			if (std::fwrite(header.data(), 1, header.size(), new_file.get()) == header.size())
				gi.Com_PrintFmt("{}: Player config written to: \"{}\"\n", __FUNCTION__, name);
			else
				gi.Com_PrintFmt("{}: Player config write error: \"{}\"\n", __FUNCTION__, name);
		} else {
			gi.Com_PrintFmt("{}: Cannot save player config: {}\n", __FUNCTION__, name);
		}
	} else {
		//gi.Com_PrintFmt("{}: Player config not saved as file already exists: \"{}\"\n", __FUNCTION__, name);
	}
}

/*
=================
MM_CmdCrosshairID
=================
*/
void MM_CmdCrosshairID(gentity_t *ent)
{
	if (!muffmode::pconfig::RequireNoCommandArgs(ent))
		return;

	ent->client->sess.pc.show_id ^= true;
	gi.LocClient_Print(ent, PRINT_HIGH, "Player identification display: {}\n", ent->client->sess.pc.show_id ? "ON" : "OFF");
}

/*
=================
MM_CmdTimer
=================
*/
void MM_CmdTimer(gentity_t *ent)
{
	if (!muffmode::pconfig::RequireNoCommandArgs(ent))
		return;

	ent->client->sess.pc.show_timer ^= true;
	gi.LocClient_Print(ent, PRINT_HIGH, "Match timer display: {}\n", ent->client->sess.pc.show_timer ? "ON" : "OFF");
}

/*
=================
MM_CmdInfoHud
=================
*/
void MM_CmdInfoHud(gentity_t *ent)
{
	if (!muffmode::pconfig::RequireNoCommandArgs(ent))
		return;

	ent->client->sess.pc.show_match_info ^= true;
	gi.LocClient_Print(ent, PRINT_HIGH, "Info HUD: {}\n", ent->client->sess.pc.show_match_info ? "ON" : "OFF");
}

/*
=================
MM_CmdFragMessages
=================
*/
void MM_CmdFragMessages(gentity_t *ent)
{
	if (!muffmode::pconfig::RequireNoCommandArgs(ent))
		return;

	ent->client->sess.pc.show_fragmessages ^= true;
	gi.LocClient_Print(ent, PRINT_HIGH, "{} frag messages.\n", ent->client->sess.pc.show_fragmessages ? "Activating" : "Disabling");
}

/*
=================
MM_CmdAnnouncer
=================
*/
void MM_CmdAnnouncer(gentity_t *ent)
{
	if (!muffmode::pconfig::RequireNoCommandArgs(ent))
		return;

	ent->client->sess.pc.use_expanded ^= true;
	gi.LocClient_Print(ent, PRINT_HIGH, "Match announcer: {}\n", ent->client->sess.pc.use_expanded ? "ON" : "OFF");
}

/*
=================
MM_CmdKillBeep
=================
*/
void MM_CmdKillBeep(gentity_t *ent)
{
	if (!muffmode::pconfig::RequireCommandArgc(ent, 1, 2, G_Fmt("{} [0-4]", gi.argv(0)).data()))
		return;

	int num = 0;
	if (gi.argc() > 1) {
		const auto parsed = MM_ParseNonNegativeIntArg(gi.argv(1));
		if (!parsed || *parsed > 4) {
			gi.LocClient_Print(ent, PRINT_HIGH, "Invalid kill beep value. Use 0 through 4.\n");
			return;
		}
		num = *parsed;
	} else {
		num = (ent->client->sess.pc.killbeep_num + 1) % 5;
	}
	constexpr std::array<const char *, 5> kill_beep_names = { "off", "clang", "beep-boop", "insane", "tang-tang" };
	ent->client->sess.pc.killbeep_num = num;
	gi.LocClient_Print(ent, PRINT_HIGH, "Kill beep changed to: {}\n", kill_beep_names[num]);
}
