// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct gentity_t;

namespace muffmode::maps {

inline char CanonicalMapTokenChar(char value) noexcept
{
	if (value == '\\')
		return '/';
	if (value >= 'A' && value <= 'Z')
		return static_cast<char>(value + ('a' - 'A'));
	return value;
}

inline bool MapTokensEqual(
	std::string_view lhs,
	std::string_view rhs) noexcept
{
	if (lhs.size() != rhs.size())
		return false;
	for (size_t i = 0; i < lhs.size(); i++)
		if (CanonicalMapTokenChar(lhs[i]) != CanonicalMapTokenChar(rhs[i]))
			return false;
	return true;
}

enum class mymap_modifier_id_t : size_t {
	Powerups,
	PowerArmor,
	Health,
	Armor,
	Ammo,
	Weapons,
	Count
};

inline constexpr size_t MyMapModifierIndex(mymap_modifier_id_t id) noexcept
{
	return static_cast<size_t>(id);
}

inline constexpr std::array<std::string_view,
	MyMapModifierIndex(mymap_modifier_id_t::Count)> kMyMapModifierNames = {
	"pu", "pa", "ht", "ar", "am", "wp"
};

using mymap_modifier_modes_t =
	std::array<int8_t, kMyMapModifierNames.size()>;

struct mymap_queue_entry_t {
	std::string map_name;
	// One-shot spawn filters belong to this map, not to mutable queue-global state.
	mymap_modifier_modes_t modifier_modes {};

	friend bool operator==(
		const mymap_queue_entry_t &lhs,
		const mymap_queue_entry_t &rhs) noexcept
	{
		return lhs.map_name == rhs.map_name &&
			lhs.modifier_modes == rhs.modifier_modes;
	}

	friend bool operator!=(
		const mymap_queue_entry_t &lhs,
		const mymap_queue_entry_t &rhs) noexcept
	{
		return !(lhs == rhs);
	}
};

enum class mymap_pending_load_result_t : uint8_t {
	none,
	matched,
	cancelled
};

// Holds a selected queue entry until the engine identifies the map it is
// actually loading. MapTokensEqual supplies the canonical (case- and
// separator-insensitive) identity check without changing the map spelling sent
// to the retail engine.
class mymap_pending_selection_t {
public:
	[[nodiscard]] bool Arm(mymap_queue_entry_t entry)
	{
		if (active())
			return false;

		entry_ = std::move(entry);
		return true;
	}

	[[nodiscard]] bool active() const noexcept
	{
		return !entry_.map_name.empty();
	}

	[[nodiscard]] const std::string &map_name() const noexcept
	{
		return entry_.map_name;
	}

	[[nodiscard]] mymap_pending_load_result_t TakeForMap(
		std::string_view loaded_map,
		mymap_modifier_modes_t &matched_modes) noexcept
	{
		if (!active())
			return mymap_pending_load_result_t::none;

		const bool matches = MapTokensEqual(entry_.map_name, loaded_map);
		if (matches)
			matched_modes = entry_.modifier_modes;
		Cancel();
		return matches
			? mymap_pending_load_result_t::matched
			: mymap_pending_load_result_t::cancelled;
	}

	void Cancel() noexcept
	{
		entry_.map_name.clear();
		entry_.modifier_modes.fill(0);
	}

private:
	mymap_queue_entry_t entry_;
};

inline constexpr size_t kMaxMyMapQueueEntries = 32;

inline std::string FormatMyMapQueueEntry(const mymap_queue_entry_t &entry)
{
	if (entry.map_name.empty())
		return {};

	std::string text = entry.map_name;
	bool has_modifier = false;
	for (size_t modifier_index = 0;
		modifier_index < entry.modifier_modes.size(); modifier_index++) {
		const int8_t mode = entry.modifier_modes[modifier_index];
		if (!mode)
			continue;

		text += has_modifier ? " " : " (";
		text += mode > 0 ? "+" : "-";
		text += kMyMapModifierNames[modifier_index];
		has_modifier = true;
	}
	if (has_modifier)
		text += ")";
	return text;
}

inline std::string FormatMyMapQueueEntries(
	const std::vector<mymap_queue_entry_t> &queue)
{
	std::string text;
	const size_t entry_count = std::min(
		queue.size(), kMaxMyMapQueueEntries);

	for (size_t i = 0; i < entry_count; i++) {
		const auto &entry = queue[i];
		const std::string formatted_entry = FormatMyMapQueueEntry(entry);
		if (formatted_entry.empty())
			continue;
		if (!text.empty())
			text += " ";
		text += formatted_entry;
	}

	return text;
}

inline bool ApplyMyMapModifierToken(
	mymap_modifier_modes_t &modes,
	std::string_view token) noexcept
{
	if (token.size() != 3 || (token.front() != '+' && token.front() != '-'))
		return false;

	const std::string_view name = token.substr(1);
	for (size_t i = 0; i < kMyMapModifierNames.size(); i++) {
		if (!MapTokensEqual(name, kMyMapModifierNames[i]))
			continue;

		modes[i] = token.front() == '+' ? int8_t { 1 } : int8_t { -1 };
		return true;
	}

	return false;
}

inline bool PopMyMapQueueFront(
	std::vector<mymap_queue_entry_t> &queue,
	mymap_queue_entry_t &entry)
{
	if (queue.empty())
		return false;

	entry = std::move(queue.front());
	queue.erase(queue.begin());
	return true;
}

inline bool IsSafeMapTokenText(std::string_view mapname, size_t max_qpath) noexcept
{
	if (mapname.empty() || mapname.size() >= max_qpath)
		return false;

	auto is_separator = [](char c) {
		return c == '/' || c == '\\';
	};

	auto is_safe_segment = [](std::string_view segment) {
		return !segment.empty() && segment != "." && segment != "..";
	};

	size_t segment_start = 0;
	for (size_t i = 0; i < mapname.size(); i++) {
		const unsigned char c = static_cast<unsigned char>(mapname[i]);

		if (c <= ' ' || c == 0x7F || c == '"' || c == '\'' || c == ';' || c == ':' ||
			c == '*' || c == '?' || c == '<' || c == '>' || c == '|') {
			return false;
		}

		if (mapname[i] == '.' && i + 1 < mapname.size() && mapname[i + 1] == '.')
			return false;

		if (!is_separator(mapname[i]))
			continue;

		if (!is_safe_segment(mapname.substr(segment_start, i - segment_start)))
			return false;
		segment_start = i + 1;
	}

	return is_safe_segment(mapname.substr(segment_start));
}

inline bool IsSafeChangeLevelTokenText(std::string_view changemap, size_t max_qpath) noexcept
{
	// A leading '*' is the engine's legitimate end-of-unit marker. Keep it
	// out of the map token itself so no other '*' can reach AddCommandString.
	if (changemap.empty() || changemap.size() >= max_qpath)
		return false;
	if (changemap.front() == '*')
		changemap.remove_prefix(1);

	return IsSafeMapTokenText(changemap, max_qpath);
}

// Typed helpers for internal MuffMode systems that need the sanitized map pool/list.
std::vector<std::string> CollectConfiguredMaps();
bool ContainsConfiguredMap(const char *mapname);
bool ResolveConfiguredMap(const char *mapname, std::string &resolved);

} // namespace muffmode::maps

// [MuffMode] Map-list module entry points.
bool MM_IsSafeMapToken(const char *mapname);
void MM_ShuffleMapList();
void MM_GametypeChangeMapFirst();
bool MM_TryBeginIntermissionFromMapList();
void MM_HandleMapShuffleCvarChange();
void MM_PruneMapQueueToConfiguredMaps();

// [MuffMode] MyMap queue (game.mapqueue state remains in game_locals_t).
int MM_MQ_Count();
bool MM_MQ_Add(
	gentity_t *ent,
	const char *mapname,
	muffmode::maps::mymap_modifier_modes_t modifier_modes = {});
const char *MM_MQ_Go_Next();
// The first full map load after a queue selection consumes the transaction.
// A matching canonical map receives its modes; a different map cancels them.
bool MM_MQ_TakePendingModifiersForMap(
	const char *mapname,
	muffmode::maps::mymap_modifier_modes_t &modifier_modes) noexcept;
void MM_MQ_CancelPendingMapLoad() noexcept;
void MM_CmdMapList(gentity_t *ent);
void MM_CmdMyMap(gentity_t *ent);
