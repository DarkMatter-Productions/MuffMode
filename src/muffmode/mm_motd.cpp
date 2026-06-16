// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_motd.h"

#include <cstdio>
#include <string>

namespace {
constexpr long MM_MAX_MOTD_FILE_LENGTH = 0x40000;

bool MM_IsSafeMotdFilename(const char *filename)
{
	if (!filename || !*filename)
		return false;

	if (!strcmp(filename, ".") || !strcmp(filename, ".."))
		return false;

	size_t length = 0;
	for (const unsigned char *p = reinterpret_cast<const unsigned char *>(filename); *p; p++) {
		length++;
		if (length >= MAX_QPATH)
			return false;

		if (*p <= ' ' || *p == '/' || *p == '\\' || *p == ':' || *p == '"' || *p == ';')
			return false;
	}

	return true;
}

void MM_ClearMOTD()
{
	if (game.motd.empty())
		return;

	game.motd.clear();
	game.motd_mod_count++;
}

bool MM_IsVerboseLoggingEnabled()
{
	return g_verbose && g_verbose->integer;
}
} // namespace

void MM_LoadMOTD()
{
	const char *configured_filename = (g_motd_filename && g_motd_filename->string) ? g_motd_filename->string : "";
	const char *filename = configured_filename[0] ? configured_filename : "motd.txt";
	if (!MM_IsSafeMotdFilename(filename)) {
		MM_ClearMOTD();
		gi.Com_PrintFmt("{}: rejecting unsafe MoTD filename: \"{}\"\n", __FUNCTION__, filename);
		return;
	}

	std::string path = "baseq2/";
	path += filename;
	const char *name = path.c_str();
	FILE *f = fopen(name, "rb");
	bool valid = true;

	if (f == nullptr) {
		MM_ClearMOTD();
		if (MM_IsVerboseLoggingEnabled())
			gi.Com_PrintFmt("{}: MoTD file not found, cleared current message: \"{}\"\n", __FUNCTION__, name);
		return;
	}

	char *buffer = nullptr;
	size_t length = 0;

	if (fseek(f, 0, SEEK_END) != 0) {
		gi.Com_PrintFmt("{}: MoTD file seek error: \"{}\"\n", __FUNCTION__, name);
		valid = false;
	}

	const long file_length = valid ? ftell(f) : -1;
	if (valid && file_length < 0) {
		gi.Com_PrintFmt("{}: MoTD file length error: \"{}\"\n", __FUNCTION__, name);
		valid = false;
	} else if (valid && file_length > MM_MAX_MOTD_FILE_LENGTH) {
		gi.Com_PrintFmt("{}: MoTD file length exceeds maximum: \"{}\"\n", __FUNCTION__, name);
		valid = false;
	}

	if (valid) {
		length = static_cast<size_t>(file_length);
		if (fseek(f, 0, SEEK_SET) != 0) {
			gi.Com_PrintFmt("{}: MoTD file rewind error: \"{}\"\n", __FUNCTION__, name);
			valid = false;
		}
	}

	if (valid) {
		buffer = (char *)gi.TagMalloc(length + 1, TAG_LEVEL);
		if (!buffer) {
			gi.Com_PrintFmt("{}: MoTD allocation failed: \"{}\"\n", __FUNCTION__, name);
			valid = false;
		}
	}

	if (valid) {
		if (length) {
			const size_t read_length = fread(buffer, 1, length, f);

			if (length != read_length) {
				gi.Com_PrintFmt("{}: MoTD file read error: \"{}\"\n", __FUNCTION__, name);
				valid = false;
			}
		}

		buffer[length] = '\0';
	}
	fclose(f);

	if (valid) {
		game.motd = (const char *)buffer;
		game.motd_mod_count++;
		if (MM_IsVerboseLoggingEnabled())
			gi.Com_PrintFmt("{}: MotD file verified and loaded: \"{}\"\n", __FUNCTION__, name);
	} else {
		MM_ClearMOTD();
		gi.Com_PrintFmt("{}: MotD file load error for \"{}\", discarding.\n", __FUNCTION__, name);
	}

	// game.motd is a std::string and copied the contents above; free the scratch buffer.
	if (buffer)
		gi.TagFree(buffer);
}

void MM_CmdLoadMotd(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	if (!MM_IsExactArgcValid(gi.argc(), 1)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {}\n", gi.argv(0));
		return;
	}

	MM_LoadMOTD();
}

void MM_CmdMotd(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	if (!MM_IsExactArgcValid(gi.argc(), 1)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {}\n", gi.argv(0));
		return;
	}

	std::string motd_message;
	const char *s = nullptr;

	if (game.motd.size()) {
		motd_message = "Message of the Day:\n";
		motd_message += game.motd;
		motd_message += "\n";
		s = motd_message.c_str();
	} else {
		s = "No Message of the Day set.\n";
	}

	gi.LocClient_Print(ent, PRINT_HIGH, "{}", s);
}
