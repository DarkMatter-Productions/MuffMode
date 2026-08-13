// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_command_contracts.h"
#include "muffmode/mm_map_pool.h"
#include "muffmode/mm_maps.h"
#include "muffmode/mm_util.h"

#include "json/json.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <unordered_map>
#include <utility>

namespace muffmode::map_pool {
namespace {

using steady_clock_t = std::chrono::steady_clock;

struct filesystem_api_v1_t {
	int64_t (*OpenFile)(const char *path, int *handle, unsigned mode);
	int (*CloseFile)(int handle);
	int (*LoadFile)(const char *path, void **buffer, unsigned flags, unsigned tag);
	int (*ReadFile)(void *buffer, size_t length, int handle);
	int (*WriteFile)(const void *buffer, size_t length, int handle);
	int (*FlushFile)(int handle);
	int64_t (*TellFile)(int handle);
	int (*SeekFile)(int handle, int64_t offset, int whence);
	int (*ReadLine)(int handle, char *buffer, size_t size);
	void **(*ListFiles)(const char *path, const char *filter, unsigned flags, int *count);
	void (*FreeFileList)(void **list);
	const char *(*ErrorString)(int error);
};

struct map_snapshot_t {
	bool pool_loaded = false;
	bool cycle_loaded = false;
	std::string pool_filename;
	std::string cycle_filename;
	std::vector<map_entry_t> pool;
	std::unordered_map<std::string, size_t> index;
	std::vector<size_t> cycle;
};

struct map_runtime_state_t {
	bool initialized = false;
	bool has_seen_spawn = false;
	bool pool_reload_pending = false;
	map_snapshot_t snapshot;
	std::unordered_map<std::string, int64_t> last_played_seconds;
	uint64_t revision = 0;
	int pool_file_modified = -1;
	int cycle_file_modified = -1;
	std::string cached_context;
	std::string cached_selection;
};

map_runtime_state_t s_state;
uint64_t s_process_revision = 0;

enum class load_outcome_t : uint8_t {
	disabled,
	success,
	failure
};

struct file_read_result_t {
	bool attempted = false;
	bool success = false;
	std::string text;
	std::string source;
	std::string error;
};

struct pool_load_result_t {
	load_outcome_t outcome = load_outcome_t::failure;
	map_snapshot_t snapshot;
	size_t ignored_non_multiplayer = 0;
	size_t invalid_entries = 0;
	size_t duplicate_entries = 0;
	std::vector<std::string> diagnostics;
	std::string error;
};

struct cycle_load_result_t {
	load_outcome_t outcome = load_outcome_t::failure;
	std::vector<size_t> cycle;
	size_t unmatched_tokens = 0;
	size_t invalid_tokens = 0;
	size_t duplicate_tokens = 0;
	std::string filename;
	std::string error;
};

int64_t SteadySeconds()
{
	return std::chrono::duration_cast<std::chrono::seconds>(
		steady_clock_t::now().time_since_epoch()).count();
}

uint64_t NextRevision(uint64_t current) noexcept
{
	return current == std::numeric_limits<uint64_t>::max() ? 1 : current + 1;
}

bool IsUsableClient(const gentity_t *ent)
{
	return ent && ent->inuse && ent->client;
}

std::string OneLine(std::string_view text, size_t max_length = 320)
{
	std::string output;
	output.reserve(std::min(text.size(), max_length));
	for (char ch : text) {
		if (output.size() >= max_length)
			break;
		const unsigned char uch = static_cast<unsigned char>(ch);
		output.push_back(uch < 0x20 || uch >= 0x7f ? ' ' : ch);
	}
	if (text.size() > output.size() && output.size() >= 3) {
		output.resize(output.size() - 3);
		output += "...";
	}
	return output;
}

void ReportToClient(gentity_t *ent, std::string_view message)
{
	if (IsUsableClient(ent))
		gi.LocClient_Print(ent, PRINT_HIGH, "{}\n", std::string(message).c_str());
}

const filesystem_api_v1_t *GetFilesystemApi()
{
	if (!gi.GetExtension)
		return nullptr;
	return static_cast<const filesystem_api_v1_t *>(
		gi.GetExtension("FILESYSTEM_API_V1"));
}

file_read_result_t ReadFromEngineFilesystem(
	std::string_view filename,
	size_t max_bytes)
{
	file_read_result_t result;
	const filesystem_api_v1_t *fs = GetFilesystemApi();
	if (!fs)
		return result;
	result.attempted = true;
	if (!fs->OpenFile || !fs->CloseFile || !fs->ReadFile) {
		result.error = "engine filesystem extension is incomplete";
		return result;
	}

	const std::string owned_filename(filename);
	int handle = 0;
	const int64_t length = fs->OpenFile(owned_filename.c_str(), &handle, 0);
	if (length < 0 || !handle) {
		if (handle)
			fs->CloseFile(handle);
		const char *detail = nullptr;
		if (length < 0 && fs->ErrorString &&
			length >= std::numeric_limits<int>::min() &&
			length <= std::numeric_limits<int>::max()) {
			detail = fs->ErrorString(static_cast<int>(length));
		}
		result.error = detail && detail[0]
			? fmt::format("could not open {} through the engine filesystem: {}",
				filename, OneLine(detail))
			: fmt::format("could not open {} through the engine filesystem", filename);
		return result;
	}

	auto close_file = [&]() {
		if (handle) {
			fs->CloseFile(handle);
			handle = 0;
		}
	};

	if (static_cast<uint64_t>(length) > max_bytes) {
		close_file();
		result.error = fmt::format(
			"{} is too large ({} bytes; limit is {})",
			filename, length, max_bytes);
		return result;
	}

	try {
		result.text.resize(static_cast<size_t>(length));
	} catch (const std::exception &exception) {
		close_file();
		result.error = fmt::format(
			"could not allocate storage for {}: {}",
			filename, OneLine(exception.what()));
		return result;
	}

	size_t total = 0;
	while (total < result.text.size()) {
		const int count = fs->ReadFile(
			result.text.data() + total,
			result.text.size() - total,
			handle);
		if (count <= 0 ||
			static_cast<size_t>(count) > result.text.size() - total) {
			close_file();
			result.text.clear();
			result.error = fmt::format(
				"could not completely read {} through the engine filesystem",
				filename);
			return result;
		}
		total += static_cast<size_t>(count);
	}

	close_file();
	result.success = true;
	result.source = fmt::format("{} (engine filesystem)", filename);
	return result;
}

file_read_result_t ReadFromLooseBaseq2(
	std::string_view filename,
	size_t max_bytes)
{
	file_read_result_t result;
	result.attempted = true;

	try {
		cvar_t *basedir_cvar = gi.cvar("basedir", ".", CVAR_NOFLAGS);
		const char *basedir =
			basedir_cvar && basedir_cvar->string && basedir_cvar->string[0]
			? basedir_cvar->string
			: ".";
		const std::filesystem::path path =
			std::filesystem::path(basedir) / "baseq2" / std::string(filename);

		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file.is_open()) {
			result.error = fmt::format(
				"could not open {}",
				OneLine(path.string()));
			return result;
		}

		const std::streamoff length = file.tellg();
		if (length < 0 || static_cast<uint64_t>(length) > max_bytes) {
			result.error = length < 0
				? fmt::format("could not determine the size of {}", OneLine(path.string()))
				: fmt::format(
					"{} is too large ({} bytes; limit is {})",
					OneLine(path.string()), length, max_bytes);
			return result;
		}

		file.seekg(0, std::ios::beg);
		if (!file) {
			result.error = fmt::format("could not seek {}", OneLine(path.string()));
			return result;
		}

		result.text.resize(static_cast<size_t>(length));
		if (!result.text.empty()) {
			file.read(result.text.data(), static_cast<std::streamsize>(result.text.size()));
			if (!file || file.gcount() != static_cast<std::streamsize>(result.text.size())) {
				result.text.clear();
				result.error = fmt::format(
					"could not completely read {}",
					OneLine(path.string()));
				return result;
			}
		}

		result.success = true;
		result.source = path.string();
		return result;
	} catch (const std::exception &exception) {
		result.error = fmt::format(
			"loose-file read failed: {}",
			OneLine(exception.what()));
		return result;
	}
}

file_read_result_t ReadConfiguredFile(
	std::string_view filename,
	size_t max_bytes)
{
	file_read_result_t vfs_result =
		ReadFromEngineFilesystem(filename, max_bytes);
	if (vfs_result.attempted)
		return vfs_result;
	return ReadFromLooseBaseq2(filename, max_bytes);
}

constexpr std::array<std::string_view, 14> SUPPORTED_ENTRY_PROPERTIES = {{
	"bsp", "title", "episode", "min", "max",
	"dm", "tdm", "ctf", "duel", "arena", "popular",
	"custom", "custom_textures", "custom_sounds"
}};

std::optional<std::string> FirstUnsupportedEntryProperty(
	const Json::Value &object)
{
	for (const std::string &name : object.getMemberNames()) {
		if (std::find(
				SUPPORTED_ENTRY_PROPERTIES.begin(),
				SUPPORTED_ENTRY_PROPERTIES.end(),
				name) == SUPPORTED_ENTRY_PROPERTIES.end()) {
			return name;
		}
	}
	return std::nullopt;
}

bool ReadOptionalBool(
	const Json::Value &object,
	const char *name,
	bool &value,
	std::string &error)
{
	if (!object.isMember(name))
		return true;
	const Json::Value &field = object[name];
	if (!field.isBool()) {
		error = fmt::format("'{}' must be a boolean", name);
		return false;
	}
	value = field.asBool();
	return true;
}

bool ReadOptionalInt(
	const Json::Value &object,
	const char *name,
	int minimum,
	int maximum,
	int &value,
	std::string &error)
{
	if (!object.isMember(name))
		return true;
	const Json::Value &field = object[name];
	if (!field.isInt()) {
		error = fmt::format("'{}' must be an integer", name);
		return false;
	}
	const int parsed = field.asInt();
	if (parsed < minimum || parsed > maximum) {
		error = fmt::format(
			"'{}' must be in the range {}..{}",
			name, minimum, maximum);
		return false;
	}
	value = parsed;
	return true;
}

bool ReadOptionalDisplayString(
	const Json::Value &object,
	const char *name,
	size_t max_bytes,
	std::string &value,
	std::string &error)
{
	if (!object.isMember(name))
		return true;
	const Json::Value &field = object[name];
	if (!field.isString()) {
		error = fmt::format("'{}' must be a string", name);
		return false;
	}
	const std::string parsed = field.asString();
	if (!IsSafeDisplayText(parsed, max_bytes)) {
		error = fmt::format(
			"'{}' is malformed UTF-8, contains unsafe display controls, or exceeds {} bytes",
			name, max_bytes);
		return false;
	}
	value = parsed;
	return true;
}

void AddDiagnostic(
	pool_load_result_t &result,
	size_t entry_index,
	std::string_view mapname,
	std::string_view reason)
{
	result.invalid_entries++;
	constexpr size_t MAX_DIAGNOSTICS = 16;
	if (result.diagnostics.size() >= MAX_DIAGNOSTICS)
		return;

	if (mapname.empty()) {
		result.diagnostics.push_back(fmt::format(
			"entry {}: {}",
			entry_index + 1, OneLine(reason)));
	} else {
		result.diagnostics.push_back(fmt::format(
			"entry {} ('{}'): {}",
			entry_index + 1, OneLine(mapname, MAX_QPATH), OneLine(reason)));
	}
}

pool_load_result_t LoadPoolCandidate()
{
	pool_load_result_t result;
	const char *configured =
		g_maps_pool_file && g_maps_pool_file->string
		? g_maps_pool_file->string
		: "";
	if (!configured[0]) {
		result.outcome = load_outcome_t::disabled;
		return result;
	}

	const std::string_view filename(configured);
	if (!IsSafeConfigLeaf(filename)) {
		result.error = fmt::format(
			"g_maps_pool_file '{}' is not a safe leaf filename",
			OneLine(filename));
		return result;
	}

	const file_read_result_t file =
		ReadConfiguredFile(filename, MAX_POOL_FILE_BYTES);
	if (!file.success) {
		result.error = file.error;
		return result;
	}
	if (file.text.empty()) {
		result.error = fmt::format("{} is empty", OneLine(file.source));
		return result;
	}
	if (file.text.find('\0') != std::string::npos) {
		result.error = fmt::format("{} contains an embedded NUL byte", OneLine(file.source));
		return result;
	}

	Json::CharReaderBuilder builder;
	builder["collectComments"] = false;
	builder["allowComments"] = false;
	builder["allowTrailingCommas"] = false;
	builder["strictRoot"] = true;
	builder["allowDroppedNullPlaceholders"] = false;
	builder["allowNumericKeys"] = false;
	builder["allowSingleQuotes"] = false;
	builder["stackLimit"] = 64;
	builder["failIfExtra"] = true;
	builder["rejectDupKeys"] = true;
	builder["allowSpecialFloats"] = false;
	builder["skipBom"] = true;

	Json::Value root;
	std::string parse_errors;
	try {
		const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		if (!reader ||
			!reader->parse(
				file.text.data(),
				file.text.data() + file.text.size(),
				&root,
				&parse_errors)) {
			result.error = fmt::format(
				"could not parse {}: {}",
				OneLine(file.source),
				OneLine(parse_errors));
			return result;
		}
	} catch (const std::exception &exception) {
		result.error = fmt::format(
			"could not parse {}: {}",
			OneLine(file.source),
			OneLine(exception.what()));
		return result;
	}

	if (!root.isObject() || root.size() != 1 ||
		!root.isMember("maps") || !root["maps"].isArray()) {
		result.error = fmt::format(
			"{} must contain only a root 'maps' array",
			OneLine(file.source));
		return result;
	}

	const Json::Value &maps = root["maps"];
	if (maps.size() > MAX_POOL_ENTRIES) {
		result.error = fmt::format(
			"{} contains {} map entries; limit is {}",
			OneLine(file.source), maps.size(), MAX_POOL_ENTRIES);
		return result;
	}

	map_snapshot_t candidate;
	candidate.pool_filename = std::string(filename);
	candidate.pool.reserve(maps.size());
	candidate.index.reserve(maps.size());

	for (Json::ArrayIndex i = 0; i < maps.size(); i++) {
		const Json::Value &object = maps[i];
		if (!object.isObject()) {
			AddDiagnostic(result, i, {}, "entry must be an object");
			continue;
		}
		if (const std::optional<std::string> unsupported =
			FirstUnsupportedEntryProperty(object)) {
			AddDiagnostic(
				result, i, {},
				fmt::format("unsupported property '{}'", OneLine(*unsupported, 80)));
			continue;
		}
		if (!object.isMember("bsp") || !object["bsp"].isString()) {
			AddDiagnostic(result, i, {}, "missing required string 'bsp'");
			continue;
		}

		std::string bsp = object["bsp"].asString();
		if (!IsCanonicalStructuredMapIdentifier(bsp, MAX_QPATH)) {
			AddDiagnostic(
				result, i, bsp,
				"identifier must be a canonical lowercase ASCII BSP stem without engine map-command syntax");
			continue;
		}

		std::string error;
		map_entry_t entry;
		entry.bsp = bsp;
		entry.title = bsp;

		bool dm = false;
		bool tdm = false;
		bool ctf = false;
		bool duel = false;
		bool arena = false;
		if (!ReadOptionalBool(object, "dm", dm, error) ||
			!ReadOptionalBool(object, "tdm", tdm, error) ||
			!ReadOptionalBool(object, "ctf", ctf, error) ||
			!ReadOptionalBool(object, "duel", duel, error) ||
			!ReadOptionalBool(object, "arena", arena, error) ||
			!ReadOptionalBool(object, "popular", entry.popular, error) ||
			!ReadOptionalBool(object, "custom", entry.custom, error) ||
			!ReadOptionalBool(object, "custom_textures", entry.custom_textures, error) ||
			!ReadOptionalBool(object, "custom_sounds", entry.custom_sounds, error) ||
			!ReadOptionalInt(
				object, "min", 0, static_cast<int>(MAX_LOBBY_PLAYERS),
				entry.min_players, error) ||
			!ReadOptionalInt(
				object, "max", 0, static_cast<int>(MAX_LOBBY_PLAYERS),
				entry.max_players, error) ||
			!ReadOptionalDisplayString(
				object, "title", MAX_TITLE_BYTES,
				entry.title, error) ||
			!ReadOptionalDisplayString(
				object, "episode", MAX_EPISODE_BYTES,
				entry.episode, error)) {
			AddDiagnostic(result, i, bsp, error);
			continue;
		}

		if (entry.min_players > 0 && entry.max_players > 0 &&
			entry.min_players > entry.max_players) {
			AddDiagnostic(result, i, bsp, "'min' exceeds 'max'");
			continue;
		}

		if (dm)
			entry.modes |= MAP_MODE_DM;
		if (tdm)
			entry.modes |= MAP_MODE_TDM;
		if (ctf)
			entry.modes |= MAP_MODE_CTF;
		if (duel)
			entry.modes |= MAP_MODE_DUEL;
		if (arena)
			entry.modes |= MAP_MODE_ARENA;

		if (entry.modes == MAP_MODE_NONE) {
			result.ignored_non_multiplayer++;
			continue;
		}

		entry.custom =
			entry.custom || entry.custom_textures || entry.custom_sounds;
		if (entry.title.empty())
			entry.title = entry.bsp;
		entry.search_text_folded.reserve(
			entry.bsp.size() + entry.title.size() + entry.episode.size() + 2);
		for (std::string_view field : {
			std::string_view(entry.bsp),
			std::string_view(entry.title),
			std::string_view(entry.episode)}) {
			if (!entry.search_text_folded.empty())
				entry.search_text_folded.push_back('\0');
			for (char ch : field)
				entry.search_text_folded.push_back(AsciiLower(ch));
		}

		entry.canonical_bsp = CanonicalMapKey(entry.bsp);
		if (candidate.index.find(entry.canonical_bsp) != candidate.index.end()) {
			result.duplicate_entries++;
			continue;
		}

		const size_t index = candidate.pool.size();
		candidate.index.emplace(entry.canonical_bsp, index);
		candidate.pool.push_back(std::move(entry));
	}

	if (candidate.pool.empty()) {
		result.error = fmt::format(
			"{} contains no usable unique multiplayer maps ({} non-multiplayer, {} invalid, {} duplicates){}",
			OneLine(file.source),
			result.ignored_non_multiplayer,
			result.invalid_entries,
			result.duplicate_entries,
			result.diagnostics.empty()
				? ""
				: fmt::format("; first issue: {}", result.diagnostics.front()));
		return result;
	}

	candidate.pool_loaded = true;
	result.snapshot = std::move(candidate);
	result.outcome = load_outcome_t::success;
	return result;
}

cycle_load_result_t LoadCycleCandidate(const map_snapshot_t &pool)
{
	cycle_load_result_t result;
	const char *configured =
		g_maps_cycle_file && g_maps_cycle_file->string
		? g_maps_cycle_file->string
		: "";
	if (!configured[0]) {
		result.outcome = load_outcome_t::disabled;
		return result;
	}

	const std::string_view filename(configured);
	if (!IsSafeConfigLeaf(filename)) {
		result.error = fmt::format(
			"g_maps_cycle_file '{}' is not a safe leaf filename",
			OneLine(filename));
		return result;
	}
	if (!pool.pool_loaded || pool.pool.empty()) {
		result.error = "a map cycle cannot load without a valid structured pool";
		return result;
	}

	const file_read_result_t file =
		ReadConfiguredFile(filename, MAX_CYCLE_FILE_BYTES);
	if (!file.success) {
		result.error = file.error;
		return result;
	}

	const cycle_parse_result_t parsed =
		ParseCycleText(file.text, MAX_QPATH, MAX_CYCLE_TOKENS);
	if (!parsed.valid) {
		result.error = fmt::format(
			"could not parse {}: {}",
			OneLine(file.source), OneLine(parsed.error));
		return result;
	}

	result.invalid_tokens = parsed.invalid_tokens;
	result.duplicate_tokens = parsed.duplicate_tokens;
	result.filename = std::string(filename);
	result.cycle.reserve(parsed.maps.size());
	std::vector<bool> selected(pool.pool.size(), false);

	for (const std::string &mapname : parsed.maps) {
		const auto found = pool.index.find(CanonicalMapKey(mapname));
		if (found == pool.index.end()) {
			result.unmatched_tokens++;
			continue;
		}
		if (selected[found->second]) {
			result.duplicate_tokens++;
			continue;
		}
		selected[found->second] = true;
		result.cycle.push_back(found->second);
	}

	if (result.cycle.empty()) {
		result.error = fmt::format(
			"{} contains no maps recognized by the structured pool ({} unmatched, {} invalid, {} duplicates)",
			OneLine(file.source),
			result.unmatched_tokens,
			result.invalid_tokens,
			result.duplicate_tokens);
		return result;
	}

	result.outcome = load_outcome_t::success;
	return result;
}

void UpdateModifiedSnapshots()
{
	s_state.pool_file_modified =
		g_maps_pool_file ? g_maps_pool_file->modified_count : -1;
	s_state.cycle_file_modified =
		g_maps_cycle_file ? g_maps_cycle_file->modified_count : -1;
}

void PruneRuntimeHistory()
{
	for (auto it = s_state.last_played_seconds.begin();
		it != s_state.last_played_seconds.end();) {
		if (s_state.snapshot.index.find(it->first) == s_state.snapshot.index.end())
			it = s_state.last_played_seconds.erase(it);
		else
			++it;
	}
}

void PublishSnapshot(map_snapshot_t snapshot)
{
	s_state.snapshot = std::move(snapshot);
	s_process_revision = NextRevision(s_process_revision);
	s_state.revision = s_process_revision;
	s_state.cached_context.clear();
	s_state.cached_selection.clear();
	PruneRuntimeHistory();
	MM_PruneMapQueueToConfiguredMaps();
}

bool DisablePublishedCycle() noexcept
{
	if (!s_state.snapshot.cycle_loaded &&
		s_state.snapshot.cycle_filename.empty() &&
		s_state.snapshot.cycle.empty()) {
		return false;
	}

	s_state.snapshot.cycle_loaded = false;
	s_state.snapshot.cycle_filename.clear();
	s_state.snapshot.cycle.clear();
	s_process_revision = NextRevision(s_process_revision);
	s_state.revision = s_process_revision;
	s_state.cached_context.clear();
	s_state.cached_selection.clear();
	return true;
}

void LogPoolSuccess(
	const pool_load_result_t &pool_result,
	const cycle_load_result_t *cycle_result,
	gentity_t *ent)
{
	for (const std::string &diagnostic : pool_result.diagnostics)
		gi.Com_PrintFmt("MuffMode map pool: skipped {}.\n", diagnostic);
	if (pool_result.invalid_entries > pool_result.diagnostics.size()) {
		gi.Com_PrintFmt(
			"MuffMode map pool: {} additional invalid entries omitted from diagnostics.\n",
			pool_result.invalid_entries - pool_result.diagnostics.size());
	}

	gi.Com_PrintFmt(
		"MuffMode map pool: loaded {} maps from '{}' ({} non-multiplayer ignored, {} invalid, {} duplicates).\n",
		pool_result.snapshot.pool.size(),
		pool_result.snapshot.pool_filename,
		pool_result.ignored_non_multiplayer,
		pool_result.invalid_entries,
		pool_result.duplicate_entries);

	std::string message = fmt::format(
		"Loaded {} structured maps from '{}'.",
		pool_result.snapshot.pool.size(),
		pool_result.snapshot.pool_filename);

	if (cycle_result && cycle_result->outcome == load_outcome_t::success) {
		gi.Com_PrintFmt(
			"MuffMode map cycle: loaded {} maps from '{}' ({} unmatched, {} invalid, {} duplicates).\n",
			cycle_result->cycle.size(),
			cycle_result->filename,
			cycle_result->unmatched_tokens,
			cycle_result->invalid_tokens,
			cycle_result->duplicate_tokens);
		message += fmt::format(
			" Cycle '{}' has {} eligible maps.",
			cycle_result->filename,
			cycle_result->cycle.size());
	} else {
		message += " Legacy g_map_list rotation remains active.";
	}
	ReportToClient(ent, message);
}

int RequestedGametype()
{
	return clamp(
		g_gametype ? g_gametype->integer : static_cast<int>(GT_FFA),
		static_cast<int>(GT_FIRST),
		static_cast<int>(GT_LAST));
}

mode_selection_t ModesForGametype(int gametype)
{
	auto cycle_has_mode = [](map_mode_flags_t mode) {
		return std::any_of(
			s_state.snapshot.cycle.begin(),
			s_state.snapshot.cycle.end(),
			[mode](size_t index) {
				return index < s_state.snapshot.pool.size() &&
					HasMode(s_state.snapshot.pool[index].modes, mode);
			});
	};

	const bool arena = gametype == GT_ARENA;
	const bool ctf = (_gt[gametype] & GTF_CTF) != 0;
	const bool duel = gametype == GT_DUEL;
	const bool teams = (_gt[gametype] & GTF_TEAMS) != 0;
	return ResolveModeSelection(
		arena,
		ctf,
		duel,
		teams,
		!arena && !ctf && duel && cycle_has_mode(MAP_MODE_DUEL),
		!arena && !ctf && teams && cycle_has_mode(MAP_MODE_TDM));
}

std::vector<size_t> BuildCandidates(
	map_mode_flags_t required_mode,
	int player_count,
	int64_t now,
	int repeat_delay,
	std::string_view current_key,
	bool enforce_player_bounds,
	bool enforce_cooldown,
	bool exclude_current)
{
	std::vector<size_t> candidates;
	candidates.reserve(s_state.snapshot.cycle.size());

	for (size_t index : s_state.snapshot.cycle) {
		if (index >= s_state.snapshot.pool.size())
			continue;
		const map_entry_t &entry = s_state.snapshot.pool[index];
		if (!HasMode(entry.modes, required_mode))
			continue;
		if (exclude_current && entry.canonical_bsp == current_key)
			continue;
		if (enforce_player_bounds &&
			((entry.min_players > 0 && player_count < entry.min_players) ||
				(entry.max_players > 0 && player_count > entry.max_players))) {
			continue;
		}
		if (enforce_cooldown && repeat_delay > 0) {
			const auto played =
				s_state.last_played_seconds.find(entry.canonical_bsp);
			if (played != s_state.last_played_seconds.end() &&
				now - played->second < repeat_delay) {
				continue;
			}
		}
		candidates.push_back(index);
	}

	return candidates;
}

std::vector<size_t> BuildPreferredCandidates(
	mode_selection_t modes,
	int player_count,
	int64_t now,
	int repeat_delay,
	std::string_view current_key,
	bool enforce_player_bounds,
	bool enforce_cooldown,
	bool exclude_current)
{
	std::vector<size_t> candidates = BuildCandidates(
		modes.preferred,
		player_count,
		now,
		repeat_delay,
		current_key,
		enforce_player_bounds,
		enforce_cooldown,
		exclude_current);
	if (candidates.empty() && modes.fallback != MAP_MODE_NONE) {
		candidates = BuildCandidates(
			modes.fallback,
			player_count,
			now,
			repeat_delay,
			current_key,
			enforce_player_bounds,
			enforce_cooldown,
			exclude_current);
	}
	return candidates;
}

size_t ChooseSequentialCandidate(
	const std::vector<size_t> &candidates,
	std::string_view current_key)
{
	std::vector<bool> eligible(s_state.snapshot.pool.size(), false);
	for (size_t index : candidates)
		if (index < eligible.size())
			eligible[index] = true;

	size_t current_position = s_state.snapshot.cycle.size();
	for (size_t i = 0; i < s_state.snapshot.cycle.size(); i++) {
		const size_t index = s_state.snapshot.cycle[i];
		if (index < s_state.snapshot.pool.size() &&
			s_state.snapshot.pool[index].canonical_bsp == current_key) {
			current_position = i;
			break;
		}
	}

	const size_t start = current_position < s_state.snapshot.cycle.size()
		? current_position + 1
		: 0;
	for (size_t offset = 0; offset < s_state.snapshot.cycle.size(); offset++) {
		const size_t position = (start + offset) % s_state.snapshot.cycle.size();
		const size_t index = s_state.snapshot.cycle[position];
		if (index < eligible.size() && eligible[index])
			return index;
	}

	return candidates.front();
}

size_t ChooseRandomCandidate(const std::vector<size_t> &candidates)
{
	size_t total_weight = candidates.size();
	for (size_t index : candidates) {
		if (index < s_state.snapshot.pool.size() &&
			s_state.snapshot.pool[index].popular) {
			total_weight++;
		}
	}

	std::uniform_int_distribution<size_t> distribution(0, total_weight - 1);
	size_t choice = distribution(mt_rand);
	for (size_t index : candidates) {
		const size_t weight =
			index < s_state.snapshot.pool.size() &&
				s_state.snapshot.pool[index].popular
			? 2
			: 1;
		if (choice < weight)
			return index;
		choice -= weight;
	}
	return candidates.back();
}

struct command_filter_t {
	std::string display;
	std::string folded;
	std::array<size_t, MAX_FILTER_BYTES> prefix = {};
};

std::optional<command_filter_t> CommandFilter(gentity_t *ent)
{
	std::string filter;
	for (int i = 1; i < gi.argc(); i++) {
		const char *raw_arg = gi.argv(i);
		if (!raw_arg || !raw_arg[0])
			continue;
		const std::string_view arg(raw_arg);
		const size_t separator = filter.empty() ? 0 : 1;
		const size_t remaining = MAX_FILTER_BYTES - filter.size();
		if (separator > remaining || arg.size() > remaining - separator) {
			gi.LocClient_Print(
				ent, PRINT_HIGH,
				"Map filter is too long (maximum {} bytes).\n",
				MAX_FILTER_BYTES);
			return std::nullopt;
		}
		if (separator)
			filter.push_back(' ');
		filter.append(arg);
	}
	if (!IsSafeDisplayText(filter, MAX_FILTER_BYTES)) {
		gi.LocClient_Print(
			ent, PRINT_HIGH,
			"Map filter contains malformed or unsafe display text.\n");
		return std::nullopt;
	}

	command_filter_t result;
	result.display = std::move(filter);
	result.folded = AsciiFold(result.display);
	for (size_t i = 1, matched = 0; i < result.folded.size(); i++) {
		while (matched > 0 && result.folded[i] != result.folded[matched])
			matched = result.prefix[matched - 1];
		if (result.folded[i] == result.folded[matched])
			matched++;
		result.prefix[i] = matched;
	}
	return result;
}

bool ContainsFilter(
	std::string_view folded_haystack,
	const command_filter_t &filter) noexcept
{
	if (filter.folded.empty())
		return true;

	size_t matched = 0;
	for (char ch : folded_haystack) {
		while (matched > 0 && ch != filter.folded[matched])
			matched = filter.prefix[matched - 1];
		if (ch == filter.folded[matched])
			matched++;
		if (matched == filter.folded.size())
			return true;
	}
	return false;
}

bool EntryMatchesFilter(
	const map_entry_t &entry,
	const command_filter_t &filter)
{
	if (filter.folded.empty())
		return true;

	if (filter.folded == "dm")
		return HasMode(entry.modes, MAP_MODE_DM);
	if (filter.folded == "tdm")
		return HasMode(entry.modes, MAP_MODE_TDM);
	if (filter.folded == "ctf")
		return HasMode(entry.modes, MAP_MODE_CTF);
	if (filter.folded == "duel")
		return HasMode(entry.modes, MAP_MODE_DUEL);
	if (filter.folded == "arena")
		return HasMode(entry.modes, MAP_MODE_ARENA);
	if (filter.folded == "popular")
		return entry.popular;
	if (filter.folded == "custom")
		return entry.custom;

	return ContainsFilter(entry.search_text_folded, filter);
}

std::string ModeLabels(map_mode_flags_t modes)
{
	std::string labels;
	auto append = [&](map_mode_flags_t mode, const char *label) {
		if (!HasMode(modes, mode))
			return;
		if (!labels.empty())
			labels.push_back(',');
		labels += label;
	};
	append(MAP_MODE_DM, "dm");
	append(MAP_MODE_TDM, "tdm");
	append(MAP_MODE_CTF, "ctf");
	append(MAP_MODE_DUEL, "duel");
	append(MAP_MODE_ARENA, "arena");
	return labels;
}

void FlushPrintChunk(gentity_t *ent, std::string &chunk)
{
	if (chunk.empty())
		return;
	gi.LocClient_Print(ent, PRINT_HIGH, "{}", chunk.c_str());
	chunk.clear();
}

void PrintEntries(
	gentity_t *ent,
	const std::vector<size_t> &indices,
	const command_filter_t &filter,
	std::string_view heading)
{
	if (!IsUsableClient(ent))
		return;

	gi.LocClient_Print(ent, PRINT_HIGH, "{}\n", std::string(heading).c_str());
	std::string chunk;
	size_t matches = 0;
	size_t printed = 0;
	constexpr size_t MAX_PRINT_CHUNK = 900;
	constexpr size_t MAX_PRINTED_ENTRIES = 32;

	for (size_t index : indices) {
		if (index >= s_state.snapshot.pool.size())
			continue;
		const map_entry_t &entry = s_state.snapshot.pool[index];
		if (!EntryMatchesFilter(entry, filter))
			continue;
		matches++;
		if (printed >= MAX_PRINTED_ENTRIES)
			continue;

		std::string line = fmt::format(
			"  {:<16} - {} [{}]",
			entry.bsp, entry.title, ModeLabels(entry.modes));
		if (entry.min_players > 0 || entry.max_players > 0) {
			line += fmt::format(
				" (players {}..{})",
				entry.min_players > 0 ? entry.min_players : 0,
				entry.max_players > 0 ? entry.max_players : static_cast<int>(MAX_LOBBY_PLAYERS));
		}
		if (entry.popular)
			line += " popular";
		line.push_back('\n');

		if (chunk.size() + line.size() > MAX_PRINT_CHUNK)
			FlushPrintChunk(ent, chunk);
		chunk += line;
		printed++;
	}

	FlushPrintChunk(ent, chunk);
	if (matches > printed) {
		gi.LocClient_Print(
			ent, PRINT_HIGH,
			"Showing the first {} of {} matching maps; use a narrower filter.\n",
			printed, matches);
		return;
	}
	if (!filter.display.empty()) {
		gi.LocClient_Print(
			ent, PRINT_HIGH,
			"{} map{} matched '{}'.\n",
			matches, matches == 1 ? "" : "s",
			filter.display.c_str());
	} else {
		gi.LocClient_Print(
			ent, PRINT_HIGH,
			"{} map{} listed.\n",
			matches, matches == 1 ? "" : "s");
	}
}

} // namespace
} // namespace muffmode::map_pool

namespace map_pool = muffmode::map_pool;

void MM_InitMapPoolSystem()
{
	map_pool::s_state = {};
	map_pool::s_state.initialized = true;

	map_pool::pool_load_result_t pool_result =
		map_pool::LoadPoolCandidate();
	if (pool_result.outcome == map_pool::load_outcome_t::disabled) {
		map_pool::s_state.pool_reload_pending = false;
		map_pool::UpdateModifiedSnapshots();
		return;
	}
	if (pool_result.outcome == map_pool::load_outcome_t::failure) {
		map_pool::s_state.pool_reload_pending = true;
		gi.Com_PrintFmt(
			"MuffMode map pool unavailable: {}. Using legacy g_map_pool/g_map_list.\n",
			map_pool::OneLine(pool_result.error));
		map_pool::UpdateModifiedSnapshots();
		return;
	}

	map_pool::cycle_load_result_t cycle_result =
		map_pool::LoadCycleCandidate(pool_result.snapshot);
	if (cycle_result.outcome == map_pool::load_outcome_t::success) {
		pool_result.snapshot.cycle_loaded = true;
		pool_result.snapshot.cycle_filename = cycle_result.filename;
		pool_result.snapshot.cycle = cycle_result.cycle;
	} else if (cycle_result.outcome == map_pool::load_outcome_t::failure) {
		gi.Com_PrintFmt(
			"MuffMode map cycle unavailable: {}. Legacy g_map_list rotation remains active.\n",
			map_pool::OneLine(cycle_result.error));
	}

	map_pool::LogPoolSuccess(
		pool_result,
		cycle_result.outcome == map_pool::load_outcome_t::success
			? &cycle_result
			: nullptr,
		nullptr);
	map_pool::PublishSnapshot(std::move(pool_result.snapshot));
	map_pool::s_state.pool_reload_pending = false;
	map_pool::UpdateModifiedSnapshots();
}

void MM_ShutdownMapPoolSystem()
{
	map_pool::s_state = {};
}

bool MM_ReloadMapPool(gentity_t *ent)
{
	map_pool::pool_load_result_t pool_result =
		map_pool::LoadPoolCandidate();
	map_pool::UpdateModifiedSnapshots();

	if (pool_result.outcome == map_pool::load_outcome_t::disabled) {
		map_pool::PublishSnapshot({});
		map_pool::s_state.pool_reload_pending = false;
		gi.Com_Print(
			"MuffMode structured map pool disabled; legacy map configuration is active.\n");
		map_pool::ReportToClient(
			ent,
			"Structured map pool disabled; legacy g_map_pool/g_map_list is active.");
		return true;
	}
	if (pool_result.outcome == map_pool::load_outcome_t::failure) {
		map_pool::s_state.pool_reload_pending = true;
		const bool cycle_disabled_by_config =
			!g_maps_cycle_file || !g_maps_cycle_file->string ||
			!g_maps_cycle_file->string[0];
		const bool cycle_was_disabled =
			cycle_disabled_by_config && map_pool::DisablePublishedCycle();
		if (cycle_was_disabled) {
			gi.Com_Print(
				"MuffMode structured map cycle disabled despite the failed pool reload; legacy g_map_list rotation is active.\n");
		}
		std::string_view retained_state =
			"Legacy map configuration remains active.";
		if (map_pool::s_state.snapshot.pool_loaded) {
			retained_state = cycle_disabled_by_config
				? "The last-known-good pool remains active with its structured cycle disabled."
				: "The last-known-good pool remains active.";
		}
		const std::string message = fmt::format(
			"Map-pool reload failed: {}. {}",
			map_pool::OneLine(pool_result.error),
			retained_state);
		gi.Com_PrintFmt("MuffMode {}.\n", message);
		map_pool::ReportToClient(ent, message);
		return false;
	}

	map_pool::cycle_load_result_t cycle_result =
		map_pool::LoadCycleCandidate(pool_result.snapshot);
	if (cycle_result.outcome == map_pool::load_outcome_t::failure) {
		if (!map_pool::s_state.snapshot.pool_loaded) {
			gi.Com_PrintFmt(
				"MuffMode map cycle unavailable: {}. Publishing the valid pool while legacy g_map_list rotation remains active.\n",
				map_pool::OneLine(cycle_result.error));
			map_pool::LogPoolSuccess(pool_result, nullptr, ent);
			map_pool::PublishSnapshot(std::move(pool_result.snapshot));
			map_pool::s_state.pool_reload_pending = false;
			return true;
		}
		map_pool::s_state.pool_reload_pending = true;

		const std::string message = fmt::format(
			"Map-pool reload was not published because its cycle failed: {}. {}",
			map_pool::OneLine(cycle_result.error),
			"The last-known-good snapshot remains active.");
		gi.Com_PrintFmt("MuffMode {}.\n", message);
		map_pool::ReportToClient(ent, message);
		return false;
	}

	if (cycle_result.outcome == map_pool::load_outcome_t::success) {
		pool_result.snapshot.cycle_loaded = true;
		pool_result.snapshot.cycle_filename = cycle_result.filename;
		pool_result.snapshot.cycle = cycle_result.cycle;
	}

	map_pool::LogPoolSuccess(
		pool_result,
		cycle_result.outcome == map_pool::load_outcome_t::success
			? &cycle_result
			: nullptr,
		ent);
	map_pool::PublishSnapshot(std::move(pool_result.snapshot));
	map_pool::s_state.pool_reload_pending = false;
	return true;
}

bool MM_ReloadMapCycle(gentity_t *ent)
{
	map_pool::s_state.cycle_file_modified =
		g_maps_cycle_file ? g_maps_cycle_file->modified_count : -1;
	const int pool_modified =
		g_maps_pool_file ? g_maps_pool_file->modified_count : -1;
	const bool pool_disabled =
		!g_maps_pool_file || !g_maps_pool_file->string ||
		!g_maps_pool_file->string[0];
	if (pool_disabled &&
		pool_modified != map_pool::s_state.pool_file_modified) {
		return MM_ReloadMapPool(ent);
	}

	const bool cycle_disabled =
		!g_maps_cycle_file || !g_maps_cycle_file->string ||
		!g_maps_cycle_file->string[0];
	if (cycle_disabled) {
		map_pool::DisablePublishedCycle();
		gi.Com_Print(
			"MuffMode structured map cycle disabled; legacy g_map_list rotation is active.\n");
		map_pool::ReportToClient(
			ent,
			"Structured map cycle disabled; legacy g_map_list rotation is active.");
		if (pool_modified != map_pool::s_state.pool_file_modified)
			return MM_ReloadMapPool(ent);
		return true;
	}

	if (map_pool::s_state.pool_reload_pending ||
		pool_modified != map_pool::s_state.pool_file_modified) {
		// A cycle is resolved to indices in the currently published pool.
		// Process a pending pool change transactionally before attempting a
		// cycle-only reload so this command cannot acknowledge and strand it.
		return MM_ReloadMapPool(ent);
	}
	if (!map_pool::s_state.snapshot.pool_loaded) {
		gi.Com_Print(
			"MuffMode map-cycle reload skipped: no structured map pool is loaded.\n");
		map_pool::ReportToClient(
			ent,
			"No structured map pool is loaded; reload the pool first.");
		return false;
	}

	map_pool::cycle_load_result_t cycle_result =
		map_pool::LoadCycleCandidate(map_pool::s_state.snapshot);
	if (cycle_result.outcome == map_pool::load_outcome_t::disabled) {
		map_pool::DisablePublishedCycle();
		return true;
	}
	if (cycle_result.outcome == map_pool::load_outcome_t::failure) {
		const std::string message = fmt::format(
			"Map-cycle reload failed: {}. {}",
			map_pool::OneLine(cycle_result.error),
			map_pool::s_state.snapshot.cycle_loaded
				? "The last-known-good cycle remains active."
				: "Legacy g_map_list rotation remains active.");
		gi.Com_PrintFmt("MuffMode {}.\n", message);
		map_pool::ReportToClient(ent, message);
		return false;
	}

	map_pool::map_snapshot_t snapshot = map_pool::s_state.snapshot;
	snapshot.cycle_loaded = true;
	snapshot.cycle_filename = cycle_result.filename;
	snapshot.cycle = cycle_result.cycle;
	map_pool::PublishSnapshot(std::move(snapshot));

	gi.Com_PrintFmt(
		"MuffMode map cycle: loaded {} maps from '{}' ({} unmatched, {} invalid, {} duplicates).\n",
		cycle_result.cycle.size(),
		cycle_result.filename,
		cycle_result.unmatched_tokens,
		cycle_result.invalid_tokens,
		cycle_result.duplicate_tokens);
	map_pool::ReportToClient(
		ent,
		fmt::format(
			"Loaded {} cycle maps from '{}'.",
			cycle_result.cycle.size(),
			cycle_result.filename));
	return true;
}

void MM_HandleMapPoolCvarChanges()
{
	if (!map_pool::s_state.initialized)
		return;

	const int pool_modified =
		g_maps_pool_file ? g_maps_pool_file->modified_count : -1;
	const int cycle_modified =
		g_maps_cycle_file ? g_maps_cycle_file->modified_count : -1;
	map_pool::reload_context_t reload_context;
	reload_context.pool_reload_pending =
		map_pool::s_state.pool_reload_pending;
	reload_context.pool_disabled =
		!g_maps_pool_file || !g_maps_pool_file->string ||
		!g_maps_pool_file->string[0];
	reload_context.cycle_disabled =
		!g_maps_cycle_file || !g_maps_cycle_file->string ||
		!g_maps_cycle_file->string[0];
	switch (map_pool::ResolveReloadAction(
		pool_modified,
		map_pool::s_state.pool_file_modified,
		cycle_modified,
		map_pool::s_state.cycle_file_modified,
		reload_context)) {
	case map_pool::reload_action_t::pool:
		MM_ReloadMapPool(nullptr);
		break;
	case map_pool::reload_action_t::cycle:
		MM_ReloadMapCycle(nullptr);
		break;
	case map_pool::reload_action_t::none:
		break;
	}
}

void MM_RecordStructuredMapPlayed()
{
	// SpawnEntities calls this before replacing level.mapname. Clearing the
	// decision cache here also makes same-map reloads start a fresh rotation.
	map_pool::s_state.cached_context.clear();
	map_pool::s_state.cached_selection.clear();

	if (!map_pool::s_state.initialized)
		return;
	if (!map_pool::s_state.has_seen_spawn) {
		map_pool::s_state.has_seen_spawn = true;
		return;
	}
	if (
		!map_pool::s_state.snapshot.pool_loaded ||
		!MM_IsSafeMapToken(level.mapname)) {
		return;
	}

	const std::string key =
		map_pool::CanonicalMapKey(level.mapname);
	if (map_pool::s_state.snapshot.index.find(key) !=
		map_pool::s_state.snapshot.index.end()) {
		map_pool::s_state.last_played_seconds[key] =
			map_pool::SteadySeconds();
	}
}

bool MM_StructuredMapPoolLoaded()
{
	return map_pool::s_state.snapshot.pool_loaded &&
		!map_pool::s_state.snapshot.pool.empty();
}

bool MM_StructuredMapCycleActive()
{
	return MM_StructuredMapPoolLoaded() &&
		map_pool::s_state.snapshot.cycle_loaded &&
		!map_pool::s_state.snapshot.cycle.empty();
}

size_t MM_StructuredMapPoolCount()
{
	return map_pool::s_state.snapshot.pool.size();
}

size_t MM_StructuredMapCycleCount()
{
	return map_pool::s_state.snapshot.cycle.size();
}

uint64_t MM_MapPoolRevision()
{
	return map_pool::s_state.revision;
}

bool MM_StructuredMapPoolContains(const char *mapname)
{
	if (!mapname || !MM_IsSafeMapToken(mapname) ||
		!MM_StructuredMapPoolLoaded()) {
		return false;
	}
	return map_pool::s_state.snapshot.index.find(
		map_pool::CanonicalMapKey(mapname)) !=
		map_pool::s_state.snapshot.index.end();
}

bool MM_ResolveStructuredMapName(
	const char *mapname,
	std::string &resolved)
{
	if (!mapname || !MM_IsSafeMapToken(mapname) ||
		!MM_StructuredMapPoolLoaded()) {
		resolved.clear();
		return false;
	}

	const std::string key = map_pool::CanonicalMapKey(mapname);
	resolved.clear();
	const auto found = map_pool::s_state.snapshot.index.find(key);
	if (found == map_pool::s_state.snapshot.index.end() ||
		found->second >= map_pool::s_state.snapshot.pool.size()) {
		return false;
	}

	resolved = map_pool::s_state.snapshot.pool[found->second].bsp;
	return true;
}

std::vector<std::string> MM_CollectStructuredMapPool()
{
	std::vector<std::string> maps;
	maps.reserve(map_pool::s_state.snapshot.pool.size());
	for (const map_pool::map_entry_t &entry : map_pool::s_state.snapshot.pool)
		maps.push_back(entry.bsp);
	return maps;
}

std::vector<std::string> MM_CollectStructuredMapCycle()
{
	std::vector<std::string> maps;
	maps.reserve(map_pool::s_state.snapshot.cycle.size());
	for (size_t index : map_pool::s_state.snapshot.cycle) {
		if (index < map_pool::s_state.snapshot.pool.size())
			maps.push_back(map_pool::s_state.snapshot.pool[index].bsp);
	}
	return maps;
}

static bool MM_SelectStructuredMap(
	std::string &mapname,
	bool cycle_start)
{
	mapname.clear();
	MM_HandleMapPoolCvarChanges();
	if (!MM_StructuredMapCycleActive())
		return false;
	const bool current_map_safe = MM_IsSafeMapToken(level.mapname);
	if (!cycle_start && !current_map_safe)
		return false;

	const int gametype = map_pool::RequestedGametype();
	const int repeat_delay = clamp(
		g_maps_repeat_delay ? g_maps_repeat_delay->integer : 1800,
		0, 86400);
	const int player_count = map_pool::ResolveSelectionPlayerCount(
		static_cast<int>(level.num_playing_human_clients),
		minplayers ? minplayers->integer : 0,
		maxplayers ? maxplayers->integer : 0,
		cycle_start);
	const bool random_selection =
		g_maps_random && g_maps_random->integer != 0;
	const std::string current_key = current_map_safe
		? map_pool::CanonicalMapKey(level.mapname)
		: std::string();
	const std::string context = fmt::format(
		"{}:{}:{}:{}:{}:{}:{}",
		cycle_start ? "start" : "next",
		current_key,
		gametype,
		map_pool::s_state.revision,
		player_count,
		repeat_delay,
		random_selection ? 1 : 0);
	if (map_pool::s_state.cached_context == context &&
		!map_pool::s_state.cached_selection.empty()) {
		mapname = map_pool::s_state.cached_selection;
		return true;
	}

	const int64_t now = map_pool::SteadySeconds();

	const map_pool::mode_selection_t modes =
		map_pool::ModesForGametype(gametype);

	std::vector<size_t> candidates;
	for (const map_pool::selection_relaxation_t relaxation :
		map_pool::SELECTION_RELAXATIONS) {
		candidates = map_pool::BuildPreferredCandidates(
			modes, player_count, now, repeat_delay, current_key,
			relaxation.enforce_player_bounds,
			relaxation.enforce_cooldown,
			!cycle_start);
		if (!candidates.empty())
			break;
	}
	if (candidates.empty())
		return false;

	const size_t selected = random_selection
		? map_pool::ChooseRandomCandidate(candidates)
		: cycle_start
			? candidates.front()
			: map_pool::ChooseSequentialCandidate(candidates, current_key);
	if (selected >= map_pool::s_state.snapshot.pool.size())
		return false;

	const std::string &selected_map =
		map_pool::s_state.snapshot.pool[selected].bsp;
	if (!MM_IsSafeMapToken(selected_map.c_str()))
		return false;

	map_pool::s_state.cached_context = context;
	map_pool::s_state.cached_selection = selected_map;
	mapname = selected_map;
	return true;
}

bool MM_SelectStructuredNextMap(std::string &mapname)
{
	return MM_SelectStructuredMap(mapname, false);
}

bool MM_SelectStructuredCycleStartMap(std::string &mapname)
{
	return MM_SelectStructuredMap(mapname, true);
}

std::vector<map_pool::map_choice_t> MM_CollectStructuredMapChoices(
	size_t max_choices)
{
	std::vector<map_pool::map_choice_t> choices;
	if (!max_choices)
		return choices;

	MM_HandleMapPoolCvarChanges();
	if (!MM_StructuredMapCycleActive() || !MM_IsSafeMapToken(level.mapname))
		return choices;

	const int gametype = map_pool::RequestedGametype();
	const int repeat_delay = clamp(
		g_maps_repeat_delay ? g_maps_repeat_delay->integer : 1800,
		0, 86400);
	const int player_count =
		static_cast<int>(level.num_playing_human_clients);
	const std::string current_key =
		map_pool::CanonicalMapKey(level.mapname);
	const int64_t now = map_pool::SteadySeconds();
	const map_pool::mode_selection_t modes =
		map_pool::ModesForGametype(gametype);

	// Relax cooldown first. If any player-count-valid map remains after that,
	// keep the bounds: a one-map "choice" is not worth an intermission pause,
	// and the caller drops the pick instead of padding it with unsuitable maps.
	std::vector<size_t> candidates;
	for (const map_pool::selection_relaxation_t relaxation :
		map_pool::SELECTION_RELAXATIONS) {
		candidates = map_pool::BuildPreferredCandidates(
			modes, player_count, now, repeat_delay, current_key,
			relaxation.enforce_player_bounds,
			relaxation.enforce_cooldown,
			true);
		if (!map_pool::ShouldContinueMapPickRelaxation(
			candidates.size(), relaxation))
			break;
	}
	if (candidates.empty())
		return choices;

	std::shuffle(candidates.begin(), candidates.end(), mt_rand);

	choices.reserve(std::min(candidates.size(), max_choices));
	for (size_t index : candidates) {
		if (choices.size() >= max_choices)
			break;
		if (index >= map_pool::s_state.snapshot.pool.size())
			continue;

		const map_pool::map_entry_t &entry =
			map_pool::s_state.snapshot.pool[index];
		if (!MM_IsSafeMapToken(entry.bsp.c_str()))
			continue;

		choices.push_back({
			entry.bsp,
			entry.title.empty() ? entry.bsp : entry.title
		});
	}

	return choices;
}

void MM_CmdMapPool(gentity_t *ent)
{
	if (!map_pool::IsUsableClient(ent))
		return;
	MM_HandleMapPoolCvarChanges();
	const auto filter = map_pool::CommandFilter(ent);
	if (!filter)
		return;
	if (!MM_StructuredMapPoolLoaded()) {
		gi.LocClient_Print(
			ent, PRINT_HIGH,
			"No structured map pool is loaded; map voting and MyMap use legacy g_map_pool/g_map_list.\n");
		return;
	}

	std::vector<size_t> indices(map_pool::s_state.snapshot.pool.size());
	for (size_t i = 0; i < indices.size(); i++)
		indices[i] = i;
	map_pool::PrintEntries(
		ent, indices, *filter,
		fmt::format(
			"Structured map pool '{}' ({} maps):",
			map_pool::s_state.snapshot.pool_filename,
			map_pool::s_state.snapshot.pool.size()));
}

void MM_CmdMapCycle(gentity_t *ent)
{
	if (!map_pool::IsUsableClient(ent))
		return;
	MM_HandleMapPoolCvarChanges();
	const auto filter = map_pool::CommandFilter(ent);
	if (!filter)
		return;
	if (!MM_StructuredMapCycleActive()) {
		gi.LocClient_Print(
			ent, PRINT_HIGH,
			"No usable structured map cycle is active; automatic rotation falls back to legacy g_map_list.\n");
		return;
	}

	map_pool::PrintEntries(
		ent,
		map_pool::s_state.snapshot.cycle,
		*filter,
		fmt::format(
			"Structured map cycle '{}' ({} maps; {} selection):",
			map_pool::s_state.snapshot.cycle_filename,
			map_pool::s_state.snapshot.cycle.size(),
			g_maps_random && g_maps_random->integer
				? "weighted random"
				: "ordered"));
}

void MM_CmdLoadMapPool(gentity_t *ent)
{
	if (!map_pool::IsUsableClient(ent))
		return;
	if (!MM_IsExactArgcValid(gi.argc(), 1)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {}\n", gi.argv(0));
		return;
	}
	MM_ReloadMapPool(ent);
}

void MM_CmdLoadMapCycle(gentity_t *ent)
{
	if (!map_pool::IsUsableClient(ent))
		return;
	if (!MM_IsExactArgcValid(gi.argc(), 1)) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Usage: {}\n", gi.argv(0));
		return;
	}
	MM_ReloadMapCycle(ent);
}
