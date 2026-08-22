// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_bans.h"
#include "muffmode/mm_bans_rules.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

constexpr const char *k_ban_file = "baseq2/bans.cfg";

std::vector<muffmode::bans::BanRecord> g_ban_list;

bool ReadFileText(const std::filesystem::path &path, std::string &out) {
	std::error_code ec;
	if (!std::filesystem::exists(path, ec))
		return false;
	std::ifstream in(path, std::ios::binary);
	if (!in)
		return false;
	out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	return true;
}

// Atomic-ish write: write a temp file then rename over the target.
bool WriteBanFile() {
	const std::filesystem::path path(k_ban_file);
	const std::filesystem::path tmp = std::filesystem::path(std::string(k_ban_file) + ".tmp");
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	{
		std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
		if (!out)
			return false;
		const std::string text = muffmode::bans::FormatBanFile(g_ban_list);
		out.write(text.data(), static_cast<std::streamsize>(text.size()));
		if (!out.good())
			return false;
	}
	std::filesystem::rename(tmp, path, ec);
	if (ec) {
		// Defensive retry: drop the target and rename again.
		std::filesystem::remove(path, ec);
		std::filesystem::rename(tmp, path, ec);
	}
	if (ec) {
		std::error_code cleanup;
		std::filesystem::remove(tmp, cleanup);
		return false;
	}
	return true;
}

} // namespace

void MM_Bans_Load() {
	g_ban_list.clear();
	std::string text;
	if (ReadFileText(k_ban_file, text))
		g_ban_list = muffmode::bans::ParseBanFile(text);
	gi.Com_PrintFmt("MuffMode: loaded {} ban(s) from {}\n", g_ban_list.size(), k_ban_file);
}

bool MM_IsBanned(const char *social_id) {
	if (!social_id || !*social_id)
		return false;
	return muffmode::bans::IsBanned(g_ban_list, social_id);
}

bool MM_Bans_Add(std::string_view social_id, std::string_view name, std::string_view reason) {
	if (social_id.empty())
		return false;
	muffmode::bans::BanRecord rec{ std::string(social_id), std::string(name), std::string(reason) };
	const bool added = muffmode::bans::AddOrUpdateBan(g_ban_list, rec);
	if (!WriteBanFile())
		gi.Com_PrintFmt("WARNING: could not write {}; this ban will not survive a restart.\n", k_ban_file);
	return added;
}

bool MM_Bans_Remove(std::string_view social_id) {
	const bool removed = muffmode::bans::RemoveBan(g_ban_list, social_id);
	if (removed && !WriteBanFile())
		gi.Com_PrintFmt("WARNING: could not write {}; this ban will return after a restart.\n", k_ban_file);
	return removed;
}

void MM_Bans_Print(gentity_t *ent) {
	if (g_ban_list.empty()) {
		if (ent)
			gi.LocClient_Print(ent, PRINT_HIGH, "Ban list is empty.\n");
		else
			gi.Com_Print("Ban list is empty.\n");
		return;
	}
	if (ent)
		gi.LocClient_Print(ent, PRINT_HIGH, "-- {} ban(s): social_id | name | reason --\n", g_ban_list.size());
	else
		gi.Com_PrintFmt("-- {} ban(s): social_id | name | reason --\n", g_ban_list.size());
	for (const auto &rec : g_ban_list) {
		if (ent)
			gi.LocClient_Print(ent, PRINT_HIGH, "{} | {} | {}\n", rec.social_id.c_str(), rec.name.c_str(), rec.reason.c_str());
		else
			gi.Com_PrintFmt("{} | {} | {}\n", rec.social_id, rec.name, rec.reason);
	}
}
