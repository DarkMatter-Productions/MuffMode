// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

// Pure, host-testable ban-list rules.
//
// Bans are keyed by the engine-provided social_id (stable per platform account,
// unlike name or IP). The persisted form is one record per line:
//     social_id|name|reason
// Fields are pipe-delimited; '|' and newlines are stripped from name/reason on
// write so a record is always exactly one line. Engine-free by design: the live
// file I/O, connect-time enforcement, and admin commands live in mm_bans.cpp.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace muffmode::bans {

struct BanRecord {
	std::string social_id;
	std::string name;   // last-seen display name (informational)
	std::string reason; // admin-supplied, may be empty
};

// Replace field separators / newlines so a value never breaks the line format.
inline std::string SanitizeField(std::string_view in) {
	std::string out;
	out.reserve(in.size());
	for (char c : in) {
		if (c == '|' || c == '\n' || c == '\r')
			out.push_back(' ');
		else
			out.push_back(c);
	}
	return out;
}

// Serialize one record to its single-line persisted form.
inline std::string FormatBanLine(const BanRecord &rec) {
	return SanitizeField(rec.social_id) + "|" + SanitizeField(rec.name) + "|" + SanitizeField(rec.reason);
}

// Parse one persisted line. Returns false for blank/comment lines or a record
// with an empty social_id (the required key). Extra '|' beyond the third field
// are folded into the reason.
inline bool ParseBanLine(std::string_view line, BanRecord &out) {
	// trim leading whitespace
	size_t start = 0;
	while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
		++start;
	line.remove_prefix(start);
	if (line.empty() || line[0] == '#')
		return false;

	const size_t p1 = line.find('|');
	std::string_view id = line.substr(0, p1);
	if (id.empty())
		return false;

	out = BanRecord{};
	out.social_id = std::string(id);
	if (p1 == std::string_view::npos)
		return true;

	std::string_view rest = line.substr(p1 + 1);
	const size_t p2 = rest.find('|');
	out.name = std::string(rest.substr(0, p2));
	if (p2 != std::string_view::npos)
		out.reason = std::string(rest.substr(p2 + 1));
	return true;
}

// social_id comparison is exact (platform ids are opaque, case-sensitive).
inline size_t FindBanIndex(const std::vector<BanRecord> &list, std::string_view social_id) {
	for (size_t i = 0; i < list.size(); ++i)
		if (list[i].social_id == social_id)
			return i;
	return static_cast<size_t>(-1);
}

inline bool IsBanned(const std::vector<BanRecord> &list, std::string_view social_id) {
	return !social_id.empty() && FindBanIndex(list, social_id) != static_cast<size_t>(-1);
}

// Add a ban, or update the name/reason of an existing one (dedupe by social_id).
// Returns true if a new record was added, false if an existing one was updated.
// An empty social_id is rejected (returns false, list unchanged).
inline bool AddOrUpdateBan(std::vector<BanRecord> &list, const BanRecord &rec) {
	if (rec.social_id.empty())
		return false;
	const size_t idx = FindBanIndex(list, rec.social_id);
	if (idx != static_cast<size_t>(-1)) {
		list[idx].name = rec.name;
		list[idx].reason = rec.reason;
		return false;
	}
	list.push_back(rec);
	return true;
}

// Remove a ban by social_id. Returns true if a record was removed.
inline bool RemoveBan(std::vector<BanRecord> &list, std::string_view social_id) {
	const size_t idx = FindBanIndex(list, social_id);
	if (idx == static_cast<size_t>(-1))
		return false;
	list.erase(list.begin() + static_cast<std::ptrdiff_t>(idx));
	return true;
}

// Serialize the whole list to file text (one record per line, trailing newline).
inline std::string FormatBanFile(const std::vector<BanRecord> &list) {
	std::string out;
	for (const BanRecord &rec : list) {
		out += FormatBanLine(rec);
		out.push_back('\n');
	}
	return out;
}

// Parse whole-file text into records (skips blank/comment/invalid lines).
inline std::vector<BanRecord> ParseBanFile(std::string_view text) {
	std::vector<BanRecord> list;
	size_t pos = 0;
	while (pos <= text.size()) {
		size_t nl = text.find('\n', pos);
		std::string_view line = text.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
		if (!line.empty() && line.back() == '\r')
			line.remove_suffix(1);
		BanRecord rec;
		if (ParseBanLine(line, rec))
			AddOrUpdateBan(list, rec); // dedupe on load
		if (nl == std::string_view::npos)
			break;
		pos = nl + 1;
	}
	return list;
}

} // namespace muffmode::bans
