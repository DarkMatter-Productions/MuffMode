// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_message_budget.h"
#include "muffmode/mm_motd.h"
#include "muffmode/mm_reliable_text.h"
#include "muffmode/mm_util.h"

#include <cstdio>
#include <optional>
#include <string>
#include <utility>

namespace muffmode::motd {

constexpr long k_max_motd_file_length = 0x40000;

bool IsSafeFilename(const char *filename)
{
	return filename && IsSafeFilenameText(filename, MAX_QPATH);
}

void Clear()
{
	if (game.motd.empty())
		return;

	game.motd.clear();
	game.motd_mod_count++;
}

bool IsVerboseLoggingEnabled()
{
	return g_verbose && g_verbose->integer;
}

std::optional<std::string> ReadTextFile(FILE *file, const char *name)
{
	if (std::fseek(file, 0, SEEK_END) != 0) {
		gi.Com_PrintFmt("{}: MoTD file seek error: \"{}\"\n", __FUNCTION__, name);
		return std::nullopt;
	}

	const long file_length = std::ftell(file);
	if (file_length < 0) {
		gi.Com_PrintFmt("{}: MoTD file length error: \"{}\"\n", __FUNCTION__, name);
		return std::nullopt;
	}

	if (file_length > k_max_motd_file_length) {
		gi.Com_PrintFmt("{}: MoTD file length exceeds maximum: \"{}\"\n", __FUNCTION__, name);
		return std::nullopt;
	}

	if (std::fseek(file, 0, SEEK_SET) != 0) {
		gi.Com_PrintFmt("{}: MoTD file rewind error: \"{}\"\n", __FUNCTION__, name);
		return std::nullopt;
	}

	std::string contents(static_cast<size_t>(file_length), '\0');
	if (!contents.empty()) {
		const size_t read_length = std::fread(&contents[0], 1, contents.size(), file);
		if (read_length != contents.size()) {
			gi.Com_PrintFmt("{}: MoTD file read error: \"{}\"\n", __FUNCTION__, name);
			return std::nullopt;
		}
	}

	return contents;
}

} // namespace muffmode::motd

namespace {

void PrintBoundedMOTDResponse(gentity_t *ent, std::string_view text)
{
	using namespace muffmode::reliable_text;
	constexpr size_t max_total_bytes =
		kMaxMotdCommandMessages * kGuaranteedPrintContentBytes;
	const std::string bounded = MakeBoundedPreview(text, max_total_bytes);
	const chunk_plan_t plan = PlanChunks(
		bounded, kMaxPrintPayloadBytes, kMaxMotdCommandMessages);

	mm_reliable_fanout_scope_t budget(
		kMaxMotdCommandMessages,
		kMaxMotdCommandMessages *
			(kMaxPrintPayloadBytes + kReliablePrintAccountingOverheadBytes),
		"MOTD client response");
	for (std::string_view chunk : plan.chunks) {
		if (!MM_ReserveReliableFanoutMessage(
			chunk.size() + kReliablePrintAccountingOverheadBytes)) {
			break;
		}
		const std::string terminated(chunk);
		gi.LocClient_Print(ent, PRINT_HIGH, "{}", terminated.c_str());
	}
}

} // namespace

void MM_LoadMOTD()
{
	const char *configured_filename = (g_motd_filename && g_motd_filename->string) ? g_motd_filename->string : "";
	const char *filename = configured_filename[0] ? configured_filename : "motd.txt";
	if (!muffmode::motd::IsSafeFilename(filename)) {
		muffmode::motd::Clear();
		gi.Com_PrintFmt("{}: rejecting unsafe MoTD filename: \"{}\"\n", __FUNCTION__, filename);
		return;
	}

	std::string path = "baseq2/";
	path += filename;
	const char *name = path.c_str();
	auto file = muffmode::OpenFile(name, "rb");

	if (!file) {
		muffmode::motd::Clear();
		if (muffmode::motd::IsVerboseLoggingEnabled())
			gi.Com_PrintFmt("{}: MoTD file not found, cleared current message: \"{}\"\n", __FUNCTION__, name);
		return;
	}

	if (auto contents = muffmode::motd::ReadTextFile(file.get(), name)) {
		game.motd = std::move(*contents);
		game.motd_mod_count++;
		if (muffmode::motd::IsVerboseLoggingEnabled())
			gi.Com_PrintFmt("{}: MotD file verified and loaded: \"{}\"\n", __FUNCTION__, name);
	} else {
		muffmode::motd::Clear();
		gi.Com_PrintFmt("{}: MotD file load error for \"{}\", discarding.\n", __FUNCTION__, name);
	}
}

void MM_ShowAutomaticMOTD(gentity_t *ent)
{
	if (!ent || !ent->client || game.motd.empty())
		return;

	using namespace muffmode::reliable_text;
	const std::string preview = MakeBoundedPreview(
		game.motd, kMaxPrintPayloadBytes);
	mm_reliable_fanout_scope_t budget(
		1,
		kMaxPrintPayloadBytes + kReliablePrintAccountingOverheadBytes,
		"automatic MOTD preview");
	if (!MM_ReserveReliableFanoutMessage(
		preview.size() + kReliablePrintAccountingOverheadBytes)) {
		return;
	}

	gi.LocCenter_Print(ent, "{}", preview.c_str());
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
	if (CheckFlood(ent))
		return;

	if (!MM_IsExactArgcValid(gi.argc(), 1)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {}\n", gi.argv(0));
		return;
	}

	if (game.motd.empty()) {
		gi.LocClient_Print(ent, PRINT_HIGH,
			"No Message of the Day set.\n");
		return;
	}

	std::string motd_message = "Message of the Day:\n";
	motd_message += game.motd;
	motd_message += "\n";
	PrintBoundedMOTDResponse(ent, motd_message);
}
