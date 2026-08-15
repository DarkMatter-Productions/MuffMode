// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_client_profile.h"
#include "muffmode/mm_gametype.h"
#include "muffmode/mm_pconfig.h"
#include "muffmode/mm_pconfig_rules.h"
#include "muffmode/mm_player_name.h"
#include "muffmode/mm_util.h"

#include "json/json.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <io.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

// Some engine/platform include paths expose the legacy Windows min/max macros
// before this translation unit reaches windows.h.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace muffmode::client_profile {
namespace {

constexpr int k_schema_version = 2;
constexpr float k_default_skill_rating = 1500.0f;
constexpr float k_max_skill_rating = 65535.0f;
constexpr size_t k_max_profile_bytes = 0x40000;
constexpr size_t k_max_aliases = 16;
constexpr size_t k_max_gametype_key_bytes = 31;
// Keep the complete lock/temp/quarantine paths below classic Windows path
// limits for normal install roots. Longer identities remain session-only.
constexpr size_t k_max_profile_stem_bytes = 80;
constexpr size_t k_max_preference_snapshot_bytes = 0x4000;
constexpr size_t k_max_command_tokens = 128;
constexpr size_t k_max_pending_preference_snapshots = MAX_CLIENTS;
constexpr int32_t k_max_skill_rating_change = 32;
constexpr const char *k_profile_directory = "baseq2/pcfg/profiles";
constexpr const char *k_legacy_profile_directory = "baseq2/pcfg";
constexpr auto k_profile_lock_timeout = std::chrono::milliseconds(20);
constexpr auto k_profile_lock_retry = std::chrono::milliseconds(2);
constexpr auto k_preference_retry_initial_delay = std::chrono::milliseconds(5000);
constexpr auto k_preference_retry_max_delay = std::chrono::milliseconds(60000);
constexpr auto k_preference_debounce_delay = std::chrono::milliseconds(500);
constexpr auto k_preference_max_flush_delay = std::chrono::milliseconds(2000);

struct weapon_token_t {
	item_id_t id;
	std::string_view token;
};

constexpr std::array<weapon_token_t, 21> k_weapon_tokens = {{
	{ IT_WEAPON_GRAPPLE, "GP" },
	{ IT_WEAPON_BLASTER, "BL" },
	{ IT_WEAPON_CHAINFIST, "CF" },
	{ IT_WEAPON_SHOTGUN, "SG" },
	{ IT_WEAPON_SSHOTGUN, "SSG" },
	{ IT_WEAPON_MACHINEGUN, "MG" },
	{ IT_WEAPON_ETF_RIFLE, "ETF" },
	{ IT_WEAPON_CHAINGUN, "CG" },
	{ IT_AMMO_GRENADES, "HG" },
	{ IT_AMMO_TRAP, "TP" },
	{ IT_AMMO_TESLA, "TM" },
	{ IT_WEAPON_GLAUNCHER, "GL" },
	{ IT_WEAPON_PROXLAUNCHER, "PL" },
	{ IT_WEAPON_RLAUNCHER, "RL" },
	{ IT_WEAPON_HYPERBLASTER, "HB" },
	{ IT_WEAPON_IONRIPPER, "IR" },
	{ IT_WEAPON_PLASMABEAM, "PB" },
	{ IT_WEAPON_RAILGUN, "RG" },
	{ IT_WEAPON_PHALANX, "PX" },
	{ IT_WEAPON_BFG, "BFG" },
	{ IT_WEAPON_DISRUPTOR, "DTR" }
}};

// This is the existing MuffMode no-ammo order, extended with the mission-pack
// selectable explosives present in WORR. Explicit preferences are prepended.
constexpr std::array<item_id_t, 20> k_default_weapon_order = {{
	IT_WEAPON_DISRUPTOR,
	IT_WEAPON_BFG,
	IT_WEAPON_RAILGUN,
	IT_WEAPON_PLASMABEAM,
	IT_WEAPON_IONRIPPER,
	IT_WEAPON_HYPERBLASTER,
	IT_WEAPON_ETF_RIFLE,
	IT_WEAPON_CHAINGUN,
	IT_WEAPON_MACHINEGUN,
	IT_WEAPON_SSHOTGUN,
	IT_WEAPON_SHOTGUN,
	IT_WEAPON_PHALANX,
	IT_WEAPON_RLAUNCHER,
	IT_WEAPON_GLAUNCHER,
	IT_WEAPON_PROXLAUNCHER,
	IT_AMMO_GRENADES,
	IT_AMMO_TRAP,
	IT_AMMO_TESLA,
	IT_WEAPON_BLASTER,
	IT_WEAPON_CHAINFIST
}};

enum class read_status_t : uint8_t {
	ok,
	missing,
	oversized,
	failed
};

struct read_result_t {
	read_status_t status = read_status_t::failed;
	std::string text;
	std::string error;
};

enum class document_status_t : uint8_t {
	ok,
	missing,
	corrupt,
	failed
};

struct document_result_t {
	document_status_t status = document_status_t::failed;
	Json::Value root;
	std::string error;
};

struct repair_result_t {
	bool valid = true;
	bool modified = false;
	bool schema_repaired = false;
	std::string error;
	std::string gametype;
	float rating = k_default_skill_rating;
	int32_t rating_change = 0;
};

bool IsValidItemId(item_id_t id) noexcept
{
	return id > IT_NULL && id < IT_TOTAL;
}

bool IsWeaponPreferenceId(item_id_t id) noexcept
{
	return std::any_of(k_weapon_tokens.begin(), k_weapon_tokens.end(),
		[id](const weapon_token_t &entry) { return entry.id == id; });
}

std::string UpperAscii(std::string_view text)
{
	std::string result;
	result.reserve(text.size());
	for (char ch : text) {
		if (ch >= 'a' && ch <= 'z')
			ch = static_cast<char>(ch - ('a' - 'A'));
		result.push_back(ch);
	}
	return result;
}

std::string OneLine(std::string_view text, size_t max_length = 320)
{
	std::string result;
	result.reserve(std::min(text.size(), max_length));
	for (const unsigned char ch : text) {
		if (result.size() >= max_length)
			break;
		result.push_back(ch >= 0x20 && ch < 0x7f ? static_cast<char>(ch) : ' ');
	}
	if (text.size() > result.size() && result.size() >= 3) {
		result.resize(result.size() - 3);
		result += "...";
	}
	return result;
}

std::optional<item_id_t> WeaponIdForToken(std::string_view token) noexcept
{
	if (token.empty() || token.size() > 24)
		return std::nullopt;

	std::array<char, 25> normalized{};
	for (size_t i = 0; i < token.size(); i++) {
		char ch = token[i];
		if (ch >= 'a' && ch <= 'z')
			ch = static_cast<char>(ch - ('a' - 'A'));
		normalized[i] = ch;
	}
	const std::string_view value(normalized.data(), token.size());

	for (const weapon_token_t &entry : k_weapon_tokens) {
		if (value == entry.token)
			return entry.id;
	}

	// Friendly aliases are accepted, but JSON is always rewritten using the
	// compact target abbreviation. MuffMode represents WORR's ruleset-specific
	// Plasma Gun and Thunderbolt with the HyperBlaster and Plasma Beam item IDs,
	// so legacy PG/TB preferences collapse safely onto those usable weapons.
	if (value == "GRAPPLE" || value == "HOOK")
		return IT_WEAPON_GRAPPLE;
	if (value == "BLASTER")
		return IT_WEAPON_BLASTER;
	if (value == "CHAINFIST")
		return IT_WEAPON_CHAINFIST;
	if (value == "SHOTGUN")
		return IT_WEAPON_SHOTGUN;
	if (value == "SUPERSHOTGUN" || value == "SUPER-SHOTGUN")
		return IT_WEAPON_SSHOTGUN;
	if (value == "MACHINEGUN")
		return IT_WEAPON_MACHINEGUN;
	if (value == "ETFRIFLE" || value == "ETF-RIFLE")
		return IT_WEAPON_ETF_RIFLE;
	if (value == "CHAINGUN")
		return IT_WEAPON_CHAINGUN;
	if (value == "GRENADE" || value == "GRENADES" || value == "HANDGRENADES")
		return IT_AMMO_GRENADES;
	if (value == "TRAP")
		return IT_AMMO_TRAP;
	if (value == "TESLA")
		return IT_AMMO_TESLA;
	if (value == "GRENADELAUNCHER" || value == "GRENADE-LAUNCHER")
		return IT_WEAPON_GLAUNCHER;
	if (value == "PROX" || value == "PROXLAUNCHER" || value == "PROX-LAUNCHER")
		return IT_WEAPON_PROXLAUNCHER;
	if (value == "ROCKETLAUNCHER" || value == "ROCKET-LAUNCHER")
		return IT_WEAPON_RLAUNCHER;
	if (value == "HYPERBLASTER")
		return IT_WEAPON_HYPERBLASTER;
	if (value == "PG" || value == "PLASMAGUN" || value == "PLASMA-GUN")
		return IT_WEAPON_HYPERBLASTER;
	if (value == "IONRIPPER" || value == "RIPPER")
		return IT_WEAPON_IONRIPPER;
	if (value == "PLASMABEAM" || value == "PLASMA-BEAM")
		return IT_WEAPON_PLASMABEAM;
	if (value == "TB" || value == "THUNDERBOLT")
		return IT_WEAPON_PLASMABEAM;
	if (value == "RAILGUN")
		return IT_WEAPON_RAILGUN;
	if (value == "PHALANX")
		return IT_WEAPON_PHALANX;
	if (value == "BFG10K")
		return IT_WEAPON_BFG;
	if (value == "DISRUPTOR")
		return IT_WEAPON_DISRUPTOR;

	return std::nullopt;
}

std::string_view WeaponToken(item_id_t id) noexcept
{
	for (const weapon_token_t &entry : k_weapon_tokens) {
		if (entry.id == id)
			return entry.token;
	}
	return {};
}

std::optional<std::string> NormalizeGametype(std::string_view gametype)
{
	if (gametype.empty() || gametype.size() > k_max_gametype_key_bytes)
		return std::nullopt;

	std::string result;
	result.reserve(gametype.size());
	for (char ch : gametype) {
		if (ch >= 'a' && ch <= 'z')
			ch = static_cast<char>(ch - ('a' - 'A'));
		const bool valid =
			(ch >= 'A' && ch <= 'Z') ||
			(ch >= '0' && ch <= '9') ||
			ch == '_' || ch == '-';
		if (!valid)
			return std::nullopt;
		result.push_back(ch);
	}
	return result;
}

std::string CurrentGametypeKey()
{
	const gametype_t gametype = MM_CurrentGametype();
	const int index = static_cast<int>(gametype);
	if (index >= 0 && index < GT_NUM_GAMETYPES)
		return gt_short_name_upper[index];
	return gt_short_name_upper[GT_FFA];
}

bool IsSafeStoredName(const Json::Value &value)
{
	if (!value.isString())
		return false;
	const std::string name = value.asString();
	return name.size() < MAX_NETNAME &&
		MM_PlayerNameForStorage(name) == name;
}

bool IsSafeTimestamp(const Json::Value &value)
{
	if (!value.isString())
		return false;
	const std::string timestamp = value.asString();
	if (timestamp.empty() || timestamp.size() > 64)
		return false;
	return std::all_of(timestamp.begin(), timestamp.end(), [](unsigned char ch) {
		return ch >= 0x20 && ch < 0x7f;
	});
}

template<size_t N>
std::optional<std::string_view> FixedCStringView(const char (&text)[N]) noexcept
{
	size_t length = 0;
	while (length < N && text[length])
		length++;
	if (length == N)
		return std::nullopt;
	return std::string_view(text, length);
}

client_config_t SanitizeConfigSeed(const client_config_t &seed)
{
	client_config_t sanitized = seed;
	if (sanitized.killbeep_num < 0 || sanitized.killbeep_num > 4)
		sanitized.killbeep_num = MM_DefaultClientConfig().killbeep_num;

	const auto enemy_skin = FixedCStringView(sanitized.enemy_skin);
	if (!enemy_skin || (!enemy_skin->empty() && !pconfig::IsStorableSkinPath(
			*enemy_skin, MAX_QPATH, MAX_NETNAME - 1, CS_SIZE(CS_PLAYERSKINS)))) {
		sanitized.enemy_skin[0] = 0;
	}
	const auto team_skin = FixedCStringView(sanitized.team_skin);
	if (!team_skin || (!team_skin->empty() && !pconfig::IsStorableSkinPath(
			*team_skin, MAX_QPATH, MAX_NETNAME - 1, CS_SIZE(CS_PLAYERSKINS)))) {
		sanitized.team_skin[0] = 0;
	}
	return sanitized;
}

std::string TimestampNow()
{
	const std::time_t now = std::chrono::system_clock::to_time_t(
		std::chrono::system_clock::now());
	std::tm utc{};
#ifdef _WIN32
	if (gmtime_s(&utc, &now) != 0)
		return "1970-01-01T00:00:00Z";
#else
	if (!gmtime_r(&now, &utc))
		return "1970-01-01T00:00:00Z";
#endif
	std::array<char, 32> buffer{};
	if (std::strftime(buffer.data(), buffer.size(), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0)
		return "1970-01-01T00:00:00Z";
	return buffer.data();
}

std::optional<std::filesystem::path> ProfilePath(std::string_view social_id)
{
	const auto stem = pconfig::EncodeSocialIdConfigStem(
		social_id,
		MAX_INFO_VALUE - 1,
		k_max_profile_stem_bytes);
	if (!stem)
		return std::nullopt;
	return std::filesystem::path(k_profile_directory) / (*stem + ".json");
}

std::optional<std::filesystem::path> LegacyWorrProfilePath(
	std::string_view social_id)
{
	const auto stem = pconfig::LegacyWorrSocialIdConfigStem(
		social_id,
		MAX_INFO_VALUE - 1,
		k_max_profile_stem_bytes);
	if (!stem)
		return std::nullopt;
	return std::filesystem::path(k_legacy_profile_directory) /
		(*stem + ".json");
}

std::optional<std::filesystem::path> PreviousCanonicalProfilePath(
	std::string_view social_id)
{
	const auto stem = pconfig::EncodeSocialIdConfigStem(
		social_id,
		MAX_INFO_VALUE - 1,
		k_max_profile_stem_bytes);
	if (!stem)
		return std::nullopt;
	return std::filesystem::path(k_legacy_profile_directory) /
		(*stem + ".json");
}

bool EnsureProfileDirectory()
{
	std::error_code error;
	const std::filesystem::path directory(k_profile_directory);
	std::filesystem::create_directories(directory, error);
	if (error) {
		gi.Com_PrintFmt("{}: cannot create player profile directory \"{}\": {}\n",
			__FUNCTION__, directory.string(), error.message());
		return false;
	}

	error.clear();
	if (!std::filesystem::is_directory(directory, error) || error) {
		gi.Com_PrintFmt("{}: player profile path is not a directory: \"{}\"\n",
			__FUNCTION__, directory.string());
		return false;
	}
	return true;
}

class profile_file_lock_t {
public:
	profile_file_lock_t() = default;
	profile_file_lock_t(const profile_file_lock_t &) = delete;
	profile_file_lock_t &operator=(const profile_file_lock_t &) = delete;

	~profile_file_lock_t() noexcept
	{
#ifdef _WIN32
		if (handle_ != INVALID_HANDLE_VALUE) {
			if (locked_)
				UnlockFileEx(handle_, 0, MAXDWORD, MAXDWORD, &overlapped_);
			CloseHandle(handle_);
		}
#else
		if (descriptor_ >= 0) {
			if (locked_)
				flock(descriptor_, LOCK_UN);
			close(descriptor_);
		}
#endif
	}

	bool Acquire(const std::filesystem::path &profile, std::string &error)
	{
		if (!EnsureProfileDirectory()) {
			error = "profile directory is unavailable";
			return false;
		}

		const std::filesystem::path lock_directory =
			profile.parent_path() / ".locks";
		std::error_code directory_error;
		std::filesystem::create_directories(lock_directory, directory_error);
		if (directory_error) {
			error = fmt::format("profile lock directory could not be created: {}",
				directory_error.message());
			return false;
		}
		const std::filesystem::path lock_path =
			lock_directory / (profile.stem().string() + ".lck");

#ifdef _WIN32
		handle_ = CreateFileW(lock_path.c_str(),
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (handle_ == INVALID_HANDLE_VALUE) {
			error = fmt::format("profile lock file could not be opened (Windows error {})",
				GetLastError());
			return false;
		}
#else
		descriptor_ = open(lock_path.c_str(), O_CREAT | O_RDWR, 0600);
		if (descriptor_ < 0) {
			error = fmt::format("profile lock file could not be opened (errno {})", errno);
			return false;
		}
#endif

		const auto deadline = std::chrono::steady_clock::now() +
			k_profile_lock_timeout;
		for (;;) {
#ifdef _WIN32
			if (LockFileEx(handle_,
					LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
					0, MAXDWORD, MAXDWORD, &overlapped_)) {
				locked_ = true;
				return true;
			}
			const DWORD lock_error = GetLastError();
			if (lock_error != ERROR_LOCK_VIOLATION &&
				lock_error != ERROR_IO_PENDING) {
				error = fmt::format("profile lock failed (Windows error {})", lock_error);
				return false;
			}
#else
			if (flock(descriptor_, LOCK_EX | LOCK_NB) == 0) {
				locked_ = true;
				return true;
			}
			if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR) {
				error = fmt::format("profile lock failed (errno {})", errno);
				return false;
			}
#endif
			if (std::chrono::steady_clock::now() >= deadline) {
				error = fmt::format("profile lock acquisition exceeded {} ms",
					k_profile_lock_timeout.count());
				return false;
			}
			std::this_thread::sleep_for(k_profile_lock_retry);
		}
	}

private:
	bool locked_ = false;
#ifdef _WIN32
	HANDLE handle_ = INVALID_HANDLE_VALUE;
	OVERLAPPED overlapped_{};
#else
	int descriptor_ = -1;
#endif
};

bool AcquireProfileLock(const std::filesystem::path &path,
	profile_file_lock_t &lock)
{
	std::string error;
	if (lock.Acquire(path, error))
		return true;
	gi.Com_PrintFmt("{}: player profile lock failed for \"{}\": {}\n",
		__FUNCTION__, path.string(), error);
	return false;
}

read_result_t ReadBoundedFile(const std::filesystem::path &path)
{
	read_result_t result;
	const std::string filename = path.string();
	auto file = muffmode::OpenFile(filename.c_str(), "rb");
	if (!file) {
		std::error_code error;
		const bool exists = std::filesystem::exists(path, error);
		result.status = !error && !exists ? read_status_t::missing : read_status_t::failed;
		result.error = result.status == read_status_t::missing
			? "file does not exist"
			: "file could not be opened";
		return result;
	}

	if (std::fseek(file.get(), 0, SEEK_END) != 0) {
		result.error = "file length could not be read";
		return result;
	}
	const long length = std::ftell(file.get());
	if (length < 0) {
		result.error = "file length could not be read";
		return result;
	}
	if (static_cast<unsigned long>(length) > k_max_profile_bytes) {
		result.status = read_status_t::oversized;
		result.error = fmt::format("file is too large ({} bytes; limit is {})",
			length, k_max_profile_bytes);
		return result;
	}
	if (std::fseek(file.get(), 0, SEEK_SET) != 0) {
		result.error = "file could not be rewound";
		return result;
	}

	try {
		result.text.resize(static_cast<size_t>(length));
	} catch (const std::exception &exception) {
		result.error = fmt::format("could not allocate read buffer: {}", exception.what());
		return result;
	}
	if (!result.text.empty() &&
		std::fread(result.text.data(), 1, result.text.size(), file.get()) != result.text.size()) {
		result.text.clear();
		result.error = "file could not be read completely";
		return result;
	}
	if (result.text.find('\0') != std::string::npos) {
		result.text.clear();
		result.error = "file contains an embedded NUL byte";
		return result;
	}

	result.status = read_status_t::ok;
	return result;
}

document_result_t ReadDocument(const std::filesystem::path &path)
{
	document_result_t result;
	read_result_t file = ReadBoundedFile(path);
	if (file.status == read_status_t::missing) {
		result.status = document_status_t::missing;
		return result;
	}
	if (file.status == read_status_t::oversized) {
		result.status = document_status_t::corrupt;
		result.error = std::move(file.error);
		return result;
	}
	if (file.status != read_status_t::ok) {
		result.status = document_status_t::failed;
		result.error = std::move(file.error);
		return result;
	}
	if (file.text.empty()) {
		result.status = document_status_t::corrupt;
		result.error = "file is empty";
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

	std::string parse_error;
	try {
		const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		if (!reader || !reader->parse(
				file.text.data(),
				file.text.data() + file.text.size(),
				&result.root,
				&parse_error)) {
			result.status = document_status_t::corrupt;
			result.error = fmt::format("JSON parse failed: {}", parse_error);
			return result;
		}
	} catch (const std::exception &exception) {
		result.status = document_status_t::corrupt;
		result.error = fmt::format("JSON parse failed: {}", exception.what());
		return result;
	}

	if (!result.root.isObject()) {
		result.status = document_status_t::corrupt;
		result.error = "JSON root must be an object";
		return result;
	}

	result.status = document_status_t::ok;
	return result;
}

bool FlushFileToDisk(FILE *file) noexcept
{
	if (!file || std::fflush(file) != 0)
		return false;
#ifdef _WIN32
	return _commit(_fileno(file)) == 0;
#else
	return fsync(fileno(file)) == 0;
#endif
}

bool AtomicReplace(
	const std::filesystem::path &temporary,
	const std::filesystem::path &destination,
	std::string &error)
{
#ifdef _WIN32
	if (MoveFileExW(
			temporary.c_str(),
			destination.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		return true;
	}
	error = fmt::format("atomic replace failed with Windows error {}", GetLastError());
	return false;
#else
	std::error_code rename_error;
	std::filesystem::rename(temporary, destination, rename_error);
	if (!rename_error)
		return true;
	error = fmt::format("atomic replace failed: {}", rename_error.message());
	return false;
#endif
}

#ifndef _WIN32
bool FlushParentDirectory(
	const std::filesystem::path &directory,
	std::string &warning)
{
	int flags = O_RDONLY;
#ifdef O_DIRECTORY
	flags |= O_DIRECTORY;
#endif
#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif
	const int descriptor = open(directory.c_str(), flags);
	if (descriptor < 0) {
		const int open_error = errno;
		warning = fmt::format("parent directory could not be opened for sync: {}",
			std::error_code(open_error, std::generic_category()).message());
		return false;
	}

	if (fsync(descriptor) != 0) {
		const int sync_error = errno;
		close(descriptor);
		warning = fmt::format("parent directory sync failed: {}",
			std::error_code(sync_error, std::generic_category()).message());
		return false;
	}
	if (close(descriptor) != 0) {
		const int close_error = errno;
		warning = fmt::format("parent directory close after sync failed: {}",
			std::error_code(close_error, std::generic_category()).message());
		return false;
	}
	return true;
}
#endif

uint64_t CurrentProcessIdValue() noexcept
{
#ifdef _WIN32
	return static_cast<uint64_t>(GetCurrentProcessId());
#else
	return static_cast<uint64_t>(getpid());
#endif
}

std::optional<std::filesystem::path> CreateUniqueTemporaryDirectory(
	const std::filesystem::path &profile_directory,
	std::string &error)
{
	const std::filesystem::path temporary_root = profile_directory / ".tmp";
	std::error_code directory_error;
	std::filesystem::create_directories(temporary_root, directory_error);
	if (directory_error) {
		error = fmt::format("temporary directory could not be created: {}",
			directory_error.message());
		return std::nullopt;
	}

	static std::atomic<uint64_t> sequence{0};
	const uint64_t process_id = CurrentProcessIdValue();
	const uint64_t timestamp = static_cast<uint64_t>(
		std::chrono::steady_clock::now().time_since_epoch().count());
	for (unsigned int attempt = 0; attempt < 32; attempt++) {
		const uint64_t operation = sequence.fetch_add(1,
			std::memory_order_relaxed);
		std::filesystem::path candidate = temporary_root /
			fmt::format("{}-{:x}-{:x}", process_id, timestamp, operation);
		directory_error.clear();
		if (std::filesystem::create_directory(candidate, directory_error))
			return candidate;
		if (directory_error &&
			directory_error != std::errc::file_exists) {
			error = fmt::format("unique temporary directory could not be created: {}",
				directory_error.message());
			return std::nullopt;
		}
	}

	error = "unique temporary directory name attempts were exhausted";
	return std::nullopt;
}

struct document_write_result_t {
	bool committed = false;
	std::string error;
	std::string durability_warning;
};

document_write_result_t WriteDocumentAtomically(
	const std::filesystem::path &path,
	const Json::Value &root)
{
	document_write_result_t result;
	const std::filesystem::path profile_directory = path.parent_path();
	if (!EnsureProfileDirectory()) {
		result.error = "profile directory is unavailable";
		return result;
	}

	std::string text;
	try {
		Json::StreamWriterBuilder builder;
		builder["commentStyle"] = "None";
		builder["indentation"] = "\t";
		text = Json::writeString(builder, root);
	} catch (const std::exception &exception) {
		result.error = fmt::format(
			"JSON serialization failed: {}", exception.what());
		return result;
	}
	if (text.size() > k_max_profile_bytes) {
		result.error = fmt::format(
			"serialized profile is too large ({} bytes; limit is {})",
			text.size(), k_max_profile_bytes);
		return result;
	}

	const auto temporary_directory = CreateUniqueTemporaryDirectory(
		profile_directory, result.error);
	if (!temporary_directory)
		return result;
	const std::filesystem::path temporary =
		*temporary_directory / path.filename();

	auto remove_temporary = [&temporary, &temporary_directory]() {
		std::error_code ignored;
		std::filesystem::remove(temporary, ignored);
		ignored.clear();
		std::filesystem::remove(*temporary_directory, ignored);
	};

	const std::string temporary_name = temporary.string();
	auto file = muffmode::OpenFile(temporary_name.c_str(), "wb");
	if (!file) {
		remove_temporary();
		result.error = "temporary file could not be opened";
		return result;
	}
	if (!text.empty() &&
		std::fwrite(text.data(), 1, text.size(), file.get()) != text.size()) {
		file.reset();
		remove_temporary();
		result.error = "temporary file could not be written completely";
		return result;
	}
	if (!FlushFileToDisk(file.get())) {
		file.reset();
		remove_temporary();
		result.error = "temporary file could not be flushed";
		return result;
	}
	file.reset();

	if (!AtomicReplace(temporary, path, result.error)) {
		remove_temporary();
		return result;
	}
	result.committed = true;
#ifndef _WIN32
	// The rename has committed at this point. A directory-sync failure reduces
	// crash durability, but reporting the write as failed would retry an already
	// committed match result and duplicate its counters.
	try {
		FlushParentDirectory(
			profile_directory, result.durability_warning);
	} catch (...) {
		// Post-commit diagnostics must never turn this operation back into a
		// failure. Keep the best-effort warning allocation equally contained.
		try {
			result.durability_warning =
				"parent directory durability sync could not be reported";
		} catch (...) {
		}
	}
#endif
	remove_temporary();
	return result;
}

bool QuarantineProfile(
	const std::filesystem::path &path,
	std::string_view reason)
{
	const int64_t epoch_seconds = std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
	for (int suffix = 0; suffix < 100; suffix++) {
		const std::string directory_name = fmt::format("{}{}", epoch_seconds,
			suffix == 0 ? std::string() : fmt::format("-{}", suffix));
		const std::filesystem::path quarantine_directory =
			path.parent_path() / "quarantine" / directory_name;
		const std::filesystem::path quarantine =
			quarantine_directory / path.filename();
		std::error_code directory_error;
		std::filesystem::create_directories(quarantine_directory, directory_error);
		if (directory_error)
			continue;
		std::error_code exists_error;
		if (std::filesystem::exists(quarantine, exists_error) || exists_error)
			continue;


#ifdef _WIN32
		if (!MoveFileExW(path.c_str(), quarantine.c_str(),
				MOVEFILE_WRITE_THROUGH)) {
			continue;
		}
#else
		std::error_code rename_error;
		std::filesystem::rename(path, quarantine, rename_error);
		if (rename_error)
			continue;

		// The source has moved, so a post-rename sync failure is a durability
		// warning rather than a failed quarantine operation.
		std::string destination_warning;
		std::string quarantine_parent_warning;
		std::string source_warning;
		const bool destination_synced = FlushParentDirectory(
			quarantine_directory, destination_warning);
		const bool quarantine_parent_synced = FlushParentDirectory(
			quarantine_directory.parent_path(), quarantine_parent_warning);
		const bool source_synced = FlushParentDirectory(
			path.parent_path(), source_warning);
		if (!destination_synced || !quarantine_parent_synced ||
			!source_synced) {
			const auto log_sync_warning = [](const std::string &warning) {
				if (!warning.empty()) {
					gi.Com_PrintFmt(
						"QuarantineProfile: invalid player profile quarantine committed, but directory sync failed: {}\n",
						warning);
				}
			};
			log_sync_warning(destination_warning);
			log_sync_warning(quarantine_parent_warning);
			log_sync_warning(source_warning);
		}
#endif

		gi.Com_PrintFmt("{}: preserved invalid player profile \"{}\" as \"{}\" ({})\n",
			__FUNCTION__, path.string(), quarantine.string(), OneLine(reason));
		return true;
	}

	gi.Com_PrintFmt("{}: could not preserve invalid player profile \"{}\" ({})\n",
		__FUNCTION__, path.string(), OneLine(reason));
	return false;
}

bool JsonCounter(const Json::Value &value, uint64_t &out) noexcept
{
	if (value.type() == Json::uintValue) {
		out = value.asUInt64();
		return true;
	}
	if (value.type() == Json::intValue && value.asInt64() >= 0) {
		out = static_cast<uint64_t>(value.asInt64());
		return true;
	}
	return false;
}

Json::Value CounterValue(uint64_t value)
{
	return Json::Value(static_cast<Json::Value::UInt64>(value));
}

uint64_t SaturatingAdd(uint64_t lhs, uint64_t rhs) noexcept
{
	const uint64_t maximum = std::numeric_limits<uint64_t>::max();
	return lhs > maximum - rhs ? maximum : lhs + rhs;
}

bool JsonRating(const Json::Value &value, float &out) noexcept
{
	if (!value.isNumeric())
		return false;
	const double parsed = value.asDouble();
	if (!std::isfinite(parsed) || parsed < 0.0 || parsed > k_max_skill_rating)
		return false;
	out = static_cast<float>(parsed);
	return true;
}

enum class skill_change_parse_t : uint8_t {
	valid,
	wrapped_uint16,
	invalid
};

skill_change_parse_t JsonSkillRatingChange(
	const Json::Value &value, int32_t &out) noexcept
{
	if (value.type() != Json::intValue && value.type() != Json::uintValue)
		return skill_change_parse_t::invalid;

	int64_t parsed = 0;
	if (value.type() == Json::uintValue) {
		const Json::Value::UInt64 unsigned_value = value.asUInt64();
		if (unsigned_value >
			static_cast<Json::Value::UInt64>(std::numeric_limits<int64_t>::max())) {
			return skill_change_parse_t::invalid;
		}
		parsed = static_cast<int64_t>(unsigned_value);
	} else {
		parsed = value.asInt64();
	}

	if (parsed >= -k_max_skill_rating_change &&
		parsed <= k_max_skill_rating_change) {
		out = static_cast<int32_t>(parsed);
		return skill_change_parse_t::valid;
	}

	constexpr int64_t uint16_modulus =
		static_cast<int64_t>(std::numeric_limits<uint16_t>::max()) + 1;
	const int64_t wrapped_minimum = uint16_modulus - k_max_skill_rating_change;
	if (parsed >= wrapped_minimum && parsed < uint16_modulus) {
		out = static_cast<int32_t>(parsed - uint16_modulus);
		return skill_change_parse_t::wrapped_uint16;
	}
	return skill_change_parse_t::invalid;
}

void EnsureBool(
	Json::Value &object,
	const char *key,
	bool default_value,
	bool &modified,
	bool &schema_repaired)
{
	if (object.isMember(key) && object[key].isBool())
		return;
	object[key] = default_value;
	modified = true;
	schema_repaired = true;
}

void EnsureCounter(
	Json::Value &object,
	const char *key,
	bool &modified,
	bool &schema_repaired)
{
	uint64_t ignored = 0;
	if (object.isMember(key) && JsonCounter(object[key], ignored))
		return;
	object[key] = CounterValue(0);
	modified = true;
	schema_repaired = true;
}

void RepairAliases(
	Json::Value &root,
	std::string_view previous_name,
	std::string_view current_name,
	bool name_changed,
	bool &modified,
	bool &schema_repaired)
{
	std::vector<std::string> aliases;
	aliases.reserve(k_max_aliases);

	const auto append = [&](std::string_view alias) {
		if (aliases.size() >= k_max_aliases || alias.empty() || alias == current_name)
			return;
		const std::string safe = MM_PlayerNameForStorage(alias);
		if (safe != alias)
			return;
		if (std::find(aliases.begin(), aliases.end(), safe) != aliases.end())
			return;
		aliases.push_back(safe);
	};

	if (root.isMember("playerAliases") && root["playerAliases"].isArray()) {
		for (const Json::Value &alias : root["playerAliases"]) {
			if (alias.isString())
				append(alias.asString());
		}
	} else {
		schema_repaired = true;
	}
	if (name_changed &&
		std::find(aliases.begin(), aliases.end(), previous_name) == aliases.end()) {
		if (aliases.size() >= k_max_aliases)
			aliases.erase(aliases.begin());
		append(previous_name);
	}

	Json::Value canonical(Json::arrayValue);
	for (const std::string &alias : aliases)
		canonical.append(alias);

	if (!root.isMember("playerAliases") || root["playerAliases"] != canonical) {
		root["playerAliases"] = std::move(canonical);
		modified = true;
		schema_repaired = true;
	}
}

Json::Value CanonicalWeaponPreferences(const Json::Value &value)
{
	Json::Value canonical(Json::arrayValue);
	std::array<bool, IT_TOTAL> seen{};
	if (!value.isArray())
		return canonical;

	for (const Json::Value &entry : value) {
		if (!entry.isString())
			continue;
		const auto id = WeaponIdForToken(entry.asString());
		if (!id || !IsValidItemId(*id))
			continue;
		const size_t index = static_cast<size_t>(*id);
		if (seen[index])
			continue;
		seen[index] = true;
		canonical.append(std::string(WeaponToken(*id)));
		if (canonical.size() >= k_weapon_tokens.size())
			break;
	}
	return canonical;
}

void RepairConfig(
	Json::Value &root,
	const client_config_t &defaults,
	bool &modified,
	bool &schema_repaired)
{
	if (!root.isMember("config") || !root["config"].isObject()) {
		root["config"] = Json::Value(Json::objectValue);
		modified = true;
		schema_repaired = true;
	}
	Json::Value &config = root["config"];

	EnsureBool(config, "drawCrosshairID", defaults.show_id, modified, schema_repaired);
	EnsureBool(config, "drawTimer", defaults.show_timer, modified, schema_repaired);
	EnsureBool(config, "drawMatchInfo", defaults.show_match_info, modified, schema_repaired);
	EnsureBool(config, "drawFragMessages", defaults.show_fragmessages, modified, schema_repaired);
	EnsureBool(config, "followKiller", defaults.follow_killer, modified, schema_repaired);
	EnsureBool(config, "followLeader", defaults.follow_leader, modified, schema_repaired);
	EnsureBool(config, "followPowerup", defaults.follow_powerup, modified, schema_repaired);
	EnsureBool(config, "eyeCam", defaults.follow_first_person, modified, schema_repaired);
	EnsureBool(config, "announcerEnabled", defaults.announcer_enabled, modified, schema_repaired);

	if (!config.isMember("killBeep") || !config["killBeep"].isInt() ||
		config["killBeep"].asInt() < 0 || config["killBeep"].asInt() > 4) {
		config["killBeep"] = defaults.killbeep_num;
		modified = true;
		schema_repaired = true;
	}

	const auto repair_skin = [&](const char *key, const char *default_skin) {
		bool valid = config.isMember(key) && config[key].isString();
		if (valid) {
			const std::string skin = config[key].asString();
			valid = skin.empty() || pconfig::IsStorableSkinPath(
				skin, MAX_QPATH, MAX_NETNAME - 1, CS_SIZE(CS_PLAYERSKINS));
		}
		if (!valid) {
			config[key] = default_skin;
			modified = true;
			schema_repaired = true;
		}
	};
	repair_skin("enemySkin", defaults.enemy_skin);
	repair_skin("teamSkin", defaults.team_skin);

	const Json::Value canonical_prefs = CanonicalWeaponPreferences(config["weaponPrefs"]);
	if (!config.isMember("weaponPrefs") || config["weaponPrefs"] != canonical_prefs) {
		config["weaponPrefs"] = canonical_prefs;
		modified = true;
		schema_repaired = true;
	}
}

void RepairRatings(
	Json::Value &root,
	std::string_view current_gametype,
	float &current_rating,
	bool &modified,
	bool &schema_repaired)
{
	Json::Value canonical(Json::objectValue);
	float inherited_rating = k_default_skill_rating;
	if (root.isMember("ratings") && root["ratings"].isObject()) {
		for (const std::string &stored_key : root["ratings"].getMemberNames()) {
			const auto key = NormalizeGametype(stored_key);
			float rating = 0.0f;
			if (!key || !JsonRating(root["ratings"][stored_key], rating))
				continue;
			inherited_rating = std::max(inherited_rating, rating);
			if (canonical.size() >= GT_NUM_GAMETYPES && !canonical.isMember(*key))
				continue;
			if (!canonical.isMember(*key) || canonical[*key].asDouble() < rating)
				canonical[*key] = rating;
		}
	} else {
		schema_repaired = true;
	}

	if (!canonical.isMember(std::string(current_gametype)))
		canonical[std::string(current_gametype)] = inherited_rating;
	current_rating = canonical[std::string(current_gametype)].asFloat();

	if (!root.isMember("ratings") || root["ratings"] != canonical) {
		root["ratings"] = std::move(canonical);
		modified = true;
		schema_repaired = true;
	}
}

void RepairStats(
	Json::Value &root,
	float current_rating,
	bool &modified,
	bool &schema_repaired)
{
	if (!root.isMember("stats") || !root["stats"].isObject()) {
		root["stats"] = Json::Value(Json::objectValue);
		modified = true;
		schema_repaired = true;
	}
	Json::Value &stats = root["stats"];
	EnsureCounter(stats, "totalMatches", modified, schema_repaired);
	EnsureCounter(stats, "totalWins", modified, schema_repaired);
	EnsureCounter(stats, "totalLosses", modified, schema_repaired);
	EnsureCounter(stats, "totalDraws", modified, schema_repaired);
	EnsureCounter(stats, "totalAbandons", modified, schema_repaired);
	EnsureCounter(stats, "totalTimePlayedMs", modified, schema_repaired);

	// [MuffMode] Career post-match award tallies. Deliberately not back-filled
	// when absent: most profiles will never earn one, and creating an empty
	// object in every stored document would rewrite the whole profile directory
	// for a key nobody reads. It is only normalized when it exists but is the
	// wrong shape, so the increment path can index it safely.
	if (stats.isMember("awards") && !stats["awards"].isObject()) {
		stats["awards"] = Json::Value(Json::objectValue);
		modified = true;
		schema_repaired = true;
	}

	float ignored_rating = 0.0f;
	if (!stats.isMember("bestSkillRating") ||
		!JsonRating(stats["bestSkillRating"], ignored_rating)) {
		stats["bestSkillRating"] = 0.0f;
		modified = true;
		schema_repaired = true;
	}
	if (!stats.isMember("lastSkillRating") ||
		!JsonRating(stats["lastSkillRating"], ignored_rating)) {
		stats["lastSkillRating"] = current_rating;
		modified = true;
		schema_repaired = true;
	}
	int32_t ignored_change = 0;
	const skill_change_parse_t change_status = stats.isMember("lastSkillChange")
		? JsonSkillRatingChange(stats["lastSkillChange"], ignored_change)
		: skill_change_parse_t::invalid;
	if (change_status == skill_change_parse_t::invalid) {
		stats["lastSkillChange"] = 0;
		modified = true;
		schema_repaired = true;
	} else if (change_status == skill_change_parse_t::wrapped_uint16) {
		stats["lastSkillChange"] = ignored_change;
		modified = true;
		schema_repaired = true;
	}

	// Migrate WORR's misleading legacy field name. Its value was milliseconds.
	if (stats.isMember("totalTimePlayed")) {
		uint64_t legacy_time = 0;
		uint64_t current_time = 0;
		JsonCounter(stats["totalTimePlayedMs"], current_time);
		if (JsonCounter(stats["totalTimePlayed"], legacy_time))
			stats["totalTimePlayedMs"] = CounterValue(std::max(current_time, legacy_time));
		stats.removeMember("totalTimePlayed");
		modified = true;
		schema_repaired = true;
	}
}

void RepairRatingChanges(
	Json::Value &root,
	const Json::Value &legacy_ratings,
	std::string_view current_gametype,
	int32_t &current_change,
	bool &modified,
	bool &schema_repaired)
{
	const Json::Value &ratings = root["ratings"];
	const bool had_change_object = root.isMember("ratingChanges") &&
		root["ratingChanges"].isObject();
	Json::Value canonical(Json::objectValue);
	if (had_change_object) {
		for (const std::string &stored_key :
				root["ratingChanges"].getMemberNames()) {
			const auto key = NormalizeGametype(stored_key);
			int32_t change = 0;
			const skill_change_parse_t change_status =
				JsonSkillRatingChange(
					root["ratingChanges"][stored_key], change);
			if (!key || !ratings.isMember(*key) ||
				change_status == skill_change_parse_t::invalid) {
				schema_repaired = true;
				continue;
			}
			if (change_status == skill_change_parse_t::wrapped_uint16)
				schema_repaired = true;
			// Prefer an already-canonical spelling if a legacy document contains
			// multiple keys that normalize to the same gametype.
			if (!canonical.isMember(*key) || stored_key == *key)
				canonical[*key] = change;
		}
	} else {
		schema_repaired = true;
	}

	for (const std::string &key : ratings.getMemberNames()) {
		if (!canonical.isMember(key))
			canonical[key] = 0;
	}

	Json::Value &stats = root["stats"];
	std::optional<std::string> last_gametype;
	if (stats.isMember("lastSkillGametype")) {
		if (stats["lastSkillGametype"].isString()) {
			const auto normalized = NormalizeGametype(
				stats["lastSkillGametype"].asString());
			if (normalized && ratings.isMember(*normalized)) {
				last_gametype = *normalized;
				if (stats["lastSkillGametype"].asString() != *normalized) {
					stats["lastSkillGametype"] = *normalized;
					modified = true;
					schema_repaired = true;
				}
			}
		}
		if (!last_gametype) {
			stats.removeMember("lastSkillGametype");
			modified = true;
			schema_repaired = true;
		}
	}

	// Schema v1 only recorded one global delta. Recover it when the matching
	// rating identifies exactly one gametype; otherwise zero is safer than
	// attributing a previous mode's result to the mode being loaded now.
	if (!had_change_object) {
		if (!last_gametype) {
			float last_rating = 0.0f;
			if (legacy_ratings.isObject() &&
				JsonRating(stats["lastSkillRating"], last_rating)) {
				for (const std::string &stored_key : legacy_ratings.getMemberNames()) {
					const auto key = NormalizeGametype(stored_key);
					float candidate = 0.0f;
					if (!key || !ratings.isMember(*key) ||
						!JsonRating(legacy_ratings[stored_key], candidate) ||
						std::fabs(candidate - last_rating) > 0.0001f) {
						continue;
					}
					if (last_gametype == key)
						continue;
					if (last_gametype) {
						last_gametype.reset();
						break;
					}
					last_gametype = *key;
				}
			}
		}

		int32_t legacy_change = 0;
		if (last_gametype && JsonSkillRatingChange(
				stats["lastSkillChange"], legacy_change) !=
				skill_change_parse_t::invalid) {
			canonical[*last_gametype] = legacy_change;
			stats["lastSkillGametype"] = *last_gametype;
			modified = true;
		}
	}

	if (!root.isMember("ratingChanges") ||
		root["ratingChanges"] != canonical) {
		root["ratingChanges"] = canonical;
		modified = true;
		schema_repaired = true;
	}

	current_change = 0;
	JsonSkillRatingChange(
		canonical[std::string(current_gametype)], current_change);
}

repair_result_t RepairProfile(
	Json::Value &root,
	std::string_view social_id,
	std::string_view requested_name,
	std::string_view requested_gametype,
	const client_config_t &defaults,
	bool touch_last_seen)
{
	repair_result_t result;
	const auto gametype = NormalizeGametype(requested_gametype);
	if (!gametype) {
		result.valid = false;
		result.error = "invalid gametype key";
		return result;
	}
	result.gametype = *gametype;

	const Json::Value &stored_version = root["schemaVersion"];
	const bool version_is_nonnegative_integer =
		stored_version.type() == Json::uintValue ||
		(stored_version.type() == Json::intValue && stored_version.asInt64() >= 0);
	if (version_is_nonnegative_integer &&
		stored_version.asUInt64() > static_cast<uint64_t>(k_schema_version)) {
		result.valid = false;
		result.error = fmt::format(
			"profile schema version {} is newer than supported version {}",
			stored_version.asUInt64(), k_schema_version);
		return result;
	}
	if (!version_is_nonnegative_integer ||
		stored_version.asUInt64() != static_cast<uint64_t>(k_schema_version)) {
		root["schemaVersion"] = k_schema_version;
		result.modified = true;
		result.schema_repaired = true;
	}

	if (!root.isMember("socialID") || !root["socialID"].isString() ||
		root["socialID"].asString() != social_id) {
		root["socialID"] = std::string(social_id);
		result.modified = true;
		result.schema_repaired = true;
	}

	std::string previous_name;
	if (IsSafeStoredName(root["playerName"]))
		previous_name = root["playerName"].asString();
	const std::string current_name = requested_name.empty()
		? (previous_name.empty() ? std::string("Player") : previous_name)
		: MM_PlayerNameForStorage(requested_name);
	const bool name_changed = !previous_name.empty() && previous_name != current_name;
	if (previous_name != current_name) {
		root["playerName"] = current_name;
		result.modified = true;
		if (previous_name.empty())
			result.schema_repaired = true;
	}

	if (!IsSafeStoredName(root["originalPlayerName"])) {
		root["originalPlayerName"] = previous_name.empty() ? current_name : previous_name;
		result.modified = true;
		result.schema_repaired = true;
	}
	RepairAliases(root, previous_name, current_name, name_changed,
		result.modified, result.schema_repaired);

	RepairConfig(root, defaults, result.modified, result.schema_repaired);
	const Json::Value legacy_ratings = root["ratings"];
	RepairRatings(root, result.gametype, result.rating,
		result.modified, result.schema_repaired);
	RepairStats(root, result.rating, result.modified, result.schema_repaired);
	RepairRatingChanges(root, legacy_ratings, result.gametype, result.rating_change,
		result.modified, result.schema_repaired);

	// Authority comes exclusively from MuffMode's authenticated admin system.
	if (root.isMember("admin")) {
		root.removeMember("admin");
		result.modified = true;
		result.schema_repaired = true;
	}
	if (root.isMember("banned")) {
		root.removeMember("banned");
		result.modified = true;
		result.schema_repaired = true;
	}

	const std::string now = TimestampNow();
	if (!IsSafeTimestamp(root["firstSeen"])) {
		root["firstSeen"] = now;
		result.modified = true;
		result.schema_repaired = true;
	}
	if (touch_last_seen &&
		(!IsSafeTimestamp(root["lastSeen"]) || root["lastSeen"].asString() != now)) {
		root["lastSeen"] = now;
		result.modified = true;
	}
	if (!IsSafeTimestamp(root["lastUpdated"])) {
		root["lastUpdated"] = now;
		result.modified = true;
		result.schema_repaired = true;
	}
	return result;
}

void SetUpdatedTimestamp(Json::Value &root)
{
	root["lastUpdated"] = TimestampNow();
}

Json::Value CreateProfile(
	std::string_view social_id,
	std::string_view player_name,
	std::string_view gametype,
	const client_config_t &defaults)
{
	Json::Value root(Json::objectValue);
	repair_result_t repaired = RepairProfile(
		root, social_id, player_name, gametype, defaults, true);
	(void)repaired;
	SetUpdatedTimestamp(root);
	return root;
}

void LoadConfigIntoClient(gclient_t *client, const Json::Value &root)
{
	const Json::Value &config = root["config"];
	client_config_t loaded = MM_DefaultClientConfig();
	loaded.show_id = config["drawCrosshairID"].asBool();
	loaded.show_timer = config["drawTimer"].asBool();
	loaded.show_match_info = config["drawMatchInfo"].asBool();
	loaded.show_fragmessages = config["drawFragMessages"].asBool();
	loaded.killbeep_num = config["killBeep"].asInt();
	loaded.follow_killer = config["followKiller"].asBool();
	loaded.follow_leader = config["followLeader"].asBool();
	loaded.follow_powerup = config["followPowerup"].asBool();
	loaded.follow_first_person = config["eyeCam"].asBool();
	loaded.announcer_enabled = config["announcerEnabled"].asBool();
	muffmode::CopyString(loaded.enemy_skin, config["enemySkin"].asString());
	muffmode::CopyString(loaded.team_skin, config["teamSkin"].asString());
	client->sess.pc = loaded;

	MM_ClientProfileClearWeaponPreferences(client);
	for (const Json::Value &preference : config["weaponPrefs"])
		MM_ClientProfileAppendWeaponPreference(client, preference.asString());
}

void ApplyProfileToClient(gclient_t *client, const Json::Value &root,
	std::string_view gametype)
{
	LoadConfigIntoClient(client, root);
	float rating = k_default_skill_rating;
	JsonRating(root["ratings"][std::string(gametype)], rating);
	client->sess.skill_rating = rating;
	int32_t change = 0;
	JsonSkillRatingChange(
		root["ratingChanges"][std::string(gametype)], change);
	client->sess.skill_rating_change = change;
}

Json::Value ConfigFromClient(const gclient_t *client)
{
	Json::Value config(Json::objectValue);
	const client_config_t &pc = client->sess.pc;
	config["drawCrosshairID"] = pc.show_id;
	config["drawTimer"] = pc.show_timer;
	config["drawMatchInfo"] = pc.show_match_info;
	config["drawFragMessages"] = pc.show_fragmessages;
	config["killBeep"] = std::clamp(pc.killbeep_num, 0, 4);
	config["followKiller"] = pc.follow_killer;
	config["followLeader"] = pc.follow_leader;
	config["followPowerup"] = pc.follow_powerup;
	config["eyeCam"] = pc.follow_first_person;
	config["announcerEnabled"] = pc.announcer_enabled;

	const auto enemy_skin = FixedCStringView(pc.enemy_skin);
	config["enemySkin"] = enemy_skin && pconfig::IsStorableSkinPath(
		*enemy_skin, MAX_QPATH, MAX_NETNAME - 1, CS_SIZE(CS_PLAYERSKINS))
		? std::string(*enemy_skin)
		: std::string();
	const auto team_skin = FixedCStringView(pc.team_skin);
	config["teamSkin"] = team_skin && pconfig::IsStorableSkinPath(
		*team_skin, MAX_QPATH, MAX_NETNAME - 1, CS_SIZE(CS_PLAYERSKINS))
		? std::string(*team_skin)
		: std::string();

	Json::Value preferences(Json::arrayValue);
	std::array<bool, IT_TOTAL> seen{};
	const size_t count = std::min(
		static_cast<size_t>(client->sess.weapon_pref_count),
		client->sess.weapon_prefs.size());
	for (size_t i = 0; i < count; i++) {
		const item_id_t id = client->sess.weapon_prefs[i];
		if (!IsWeaponPreferenceId(id))
			continue;
		const size_t index = static_cast<size_t>(id);
		if (seen[index])
			continue;
		seen[index] = true;
		preferences.append(std::string(WeaponToken(id)));
	}
	config["weaponPrefs"] = std::move(preferences);
	return config;
}

bool ParseRecoveryConfigSnapshot(
	std::string_view text,
	const client_config_t &defaults,
	Json::Value &config,
	std::string &error)
{
	if (text.empty()) {
		error = "snapshot is empty";
		return false;
	}
	if (text.size() > k_max_preference_snapshot_bytes) {
		error = fmt::format("snapshot is too large ({} bytes; limit is {})",
			text.size(), k_max_preference_snapshot_bytes);
		return false;
	}
	if (text.find('\0') != std::string_view::npos) {
		error = "snapshot contains an embedded NUL byte";
		return false;
	}

	Json::CharReaderBuilder builder;
	builder["collectComments"] = false;
	builder["allowComments"] = false;
	builder["allowTrailingCommas"] = false;
	builder["strictRoot"] = true;
	builder["allowDroppedNullPlaceholders"] = false;
	builder["allowNumericKeys"] = false;
	builder["allowSingleQuotes"] = false;
	builder["stackLimit"] = 32;
	builder["failIfExtra"] = true;
	builder["rejectDupKeys"] = true;
	builder["allowSpecialFloats"] = false;
	builder["skipBom"] = false;

	Json::Value parsed;
	try {
		const std::unique_ptr<Json::CharReader> reader(
			builder.newCharReader());
		if (!reader || !reader->parse(text.data(), text.data() + text.size(),
				&parsed, &error)) {
			if (error.empty())
				error = "JSON parse failed";
			return false;
		}
		if (!parsed.isObject()) {
			error = "snapshot root is not an object";
			return false;
		}

		Json::Value wrapper(Json::objectValue);
		wrapper["config"] = std::move(parsed);
		bool modified = false;
		bool schema_repaired = false;
		RepairConfig(wrapper, defaults, modified, schema_repaired);
		config = std::move(wrapper["config"]);
		return true;
	} catch (const std::exception &exception) {
		error = fmt::format("snapshot parse failed: {}", exception.what());
		return false;
	}
}

struct owned_config_field_t {
	mm_client_profile_preference_mask_t mask;
	const char *name;
};

constexpr std::array<owned_config_field_t, 13> k_owned_config_fields{{
	{ MM_CLIENT_PROFILE_PREFERENCE_SHOW_ID, "drawCrosshairID" },
	{ MM_CLIENT_PROFILE_PREFERENCE_SHOW_TIMER, "drawTimer" },
	{ MM_CLIENT_PROFILE_PREFERENCE_SHOW_MATCH_INFO, "drawMatchInfo" },
	{ MM_CLIENT_PROFILE_PREFERENCE_SHOW_FRAG_MESSAGES, "drawFragMessages" },
	{ MM_CLIENT_PROFILE_PREFERENCE_KILL_BEEP, "killBeep" },
	{ MM_CLIENT_PROFILE_PREFERENCE_FOLLOW_KILLER, "followKiller" },
	{ MM_CLIENT_PROFILE_PREFERENCE_FOLLOW_LEADER, "followLeader" },
	{ MM_CLIENT_PROFILE_PREFERENCE_FOLLOW_POWERUP, "followPowerup" },
	{ MM_CLIENT_PROFILE_PREFERENCE_FOLLOW_VIEW, "eyeCam" },
	{ MM_CLIENT_PROFILE_PREFERENCE_ANNOUNCER, "announcerEnabled" },
	{ MM_CLIENT_PROFILE_PREFERENCE_ENEMY_SKIN, "enemySkin" },
	{ MM_CLIENT_PROFILE_PREFERENCE_TEAM_SKIN, "teamSkin" },
	{ MM_CLIENT_PROFILE_PREFERENCE_WEAPONS, "weaponPrefs" }
}};

bool StoreConfigSnapshot(Json::Value &root, const Json::Value &config,
	mm_client_profile_preference_mask_t dirty_mask)
{
	Json::Value &stored = root["config"];
	if (!stored.isObject())
		stored = Json::Value(Json::objectValue);
	bool modified = false;
	for (const owned_config_field_t &field : k_owned_config_fields) {
		if (!(dirty_mask & field.mask) || !config.isMember(field.name) ||
			stored[field.name] == config[field.name]) {
			continue;
		}
		stored[field.name] = config[field.name];
		modified = true;
	}
	if (!modified)
		return false;
	SetUpdatedTimestamp(root);
	return true;
}

struct pending_preference_state_t {
	Json::Value config;
	std::string player_name;
	std::string gametype;
	mm_client_profile_preference_mask_t dirty_mask = 0;
	uint64_t generation = 0;
	std::chrono::steady_clock::time_point pending_since{};
	std::chrono::steady_clock::time_point next_retry{};
	std::chrono::milliseconds retry_delay =
		k_preference_retry_initial_delay;
	bool pending = false;
	bool retry_in_progress = false;
};

struct pending_preference_snapshot_t {
	Json::Value config;
	std::string player_name;
	std::string gametype;
	mm_client_profile_preference_mask_t dirty_mask = 0;
	uint64_t generation = 0;
};

struct preference_retry_candidate_t {
	std::string social_id;
	uint64_t generation = 0;
};

struct preference_persist_result_t {
	bool succeeded = false;
	std::optional<uint64_t> generation;
};

std::mutex s_pending_preference_mutex;
std::unordered_map<std::string, pending_preference_state_t>
	s_pending_preferences;
uint64_t s_pending_preference_generation = 0;

uint64_t NextPreferenceGeneration() noexcept
{
	// Zero remains the invalid/default token. Wrapping would require more
	// successful save requests than the process can practically execute.
	if (++s_pending_preference_generation == 0)
		++s_pending_preference_generation;
	return s_pending_preference_generation;
}

bool QueuePreferenceSave(
	std::string_view social_id,
	Json::Value config,
	std::string player_name,
	std::string gametype,
	mm_client_profile_preference_mask_t dirty_mask)
{
	dirty_mask &= MM_CLIENT_PROFILE_PREFERENCE_ALL;
	if (!dirty_mask)
		return true;
	std::lock_guard<std::mutex> guard(s_pending_preference_mutex);
	const std::string identity(social_id);
	auto state = s_pending_preferences.find(identity);
	if (state == s_pending_preferences.end()) {
		if (s_pending_preferences.size() >=
			k_max_pending_preference_snapshots) {
			return false;
		}
		state = s_pending_preferences.emplace(
			identity, pending_preference_state_t{}).first;
	}

	const auto now = std::chrono::steady_clock::now();
	const uint64_t generation = NextPreferenceGeneration();
	const bool was_pending = state->second.pending;
	if (!was_pending) {
		state->second.pending_since = now;
		state->second.retry_delay = k_preference_retry_initial_delay;
	}
	state->second.config = std::move(config);
	state->second.player_name = std::move(player_name);
	state->second.gametype = std::move(gametype);
	state->second.dirty_mask |= dirty_mask;
	state->second.generation = generation;
	state->second.pending = true;
	const auto debounce_due = std::min(
		now + k_preference_debounce_delay,
		state->second.pending_since + k_preference_max_flush_delay);
	state->second.next_retry = was_pending
		? std::max(state->second.next_retry, debounce_due)
		: debounce_due;
	return true;
}

std::optional<pending_preference_snapshot_t> PendingPreferenceSnapshot(
	std::string_view social_id)
{
	std::lock_guard<std::mutex> guard(s_pending_preference_mutex);
	const auto state = s_pending_preferences.find(std::string(social_id));
	if (state == s_pending_preferences.end() || !state->second.pending)
		return std::nullopt;
	return pending_preference_snapshot_t{
		state->second.config,
		state->second.player_name,
		state->second.gametype,
		state->second.dirty_mask,
		state->second.generation
	};
}

bool CanErasePendingPreference(
	const pending_preference_state_t &state) noexcept
{
	return !state.pending && !state.retry_in_progress;
}

void MarkPendingPreferencePersisted(
	std::string_view social_id, uint64_t generation)
{
	std::lock_guard<std::mutex> guard(s_pending_preference_mutex);
	const auto state = s_pending_preferences.find(std::string(social_id));
	if (state == s_pending_preferences.end())
		return;
	if (state->second.pending && state->second.generation == generation) {
		state->second.dirty_mask = 0;
		state->second.pending = false;
	}
	if (CanErasePendingPreference(state->second))
		s_pending_preferences.erase(state);
}

std::optional<preference_retry_candidate_t> BeginPreferenceRetry(
	std::chrono::steady_clock::time_point now,
	bool ignore_deadline,
	const std::vector<std::string> &already_attempted)
{
	std::lock_guard<std::mutex> guard(s_pending_preference_mutex);
	auto selected = s_pending_preferences.end();
	for (auto candidate = s_pending_preferences.begin();
		candidate != s_pending_preferences.end(); ++candidate) {
		const pending_preference_state_t &state = candidate->second;
		if (!state.pending || state.retry_in_progress ||
			(!ignore_deadline && state.next_retry > now) ||
			std::find(already_attempted.begin(), already_attempted.end(),
				candidate->first) != already_attempted.end()) {
			continue;
		}
		if (selected == s_pending_preferences.end() ||
			state.generation < selected->second.generation) {
			selected = candidate;
		}
	}
	if (selected == s_pending_preferences.end())
		return std::nullopt;

	selected->second.retry_in_progress = true;
	return preference_retry_candidate_t{
		selected->first,
		selected->second.generation
	};
}

void FinishPreferenceRetry(
	const preference_retry_candidate_t &candidate,
	const preference_persist_result_t &result,
	std::chrono::steady_clock::time_point now)
{
	std::lock_guard<std::mutex> guard(s_pending_preference_mutex);
	const auto state = s_pending_preferences.find(candidate.social_id);
	if (state == s_pending_preferences.end())
		return;

	state->second.retry_in_progress = false;
	if (result.succeeded && result.generation && state->second.pending &&
		state->second.generation == *result.generation) {
		state->second.dirty_mask = 0;
		state->second.pending = false;
	} else if (!result.succeeded && result.generation &&
		state->second.pending &&
		state->second.generation == *result.generation) {
		state->second.next_retry = now + state->second.retry_delay;
		state->second.retry_delay = std::min(
			state->second.retry_delay * 2,
			k_preference_retry_max_delay);
	}

	if (CanErasePendingPreference(state->second))
		s_pending_preferences.erase(state);
}

size_t PendingPreferenceCount()
{
	std::lock_guard<std::mutex> guard(s_pending_preference_mutex);
	return static_cast<size_t>(std::count_if(
		s_pending_preferences.begin(), s_pending_preferences.end(),
		[](const auto &entry) { return entry.second.pending; }));
}

void ApplyLatestPendingPreferencesToClient(
	gclient_t *client, std::string_view social_id)
{
	const auto snapshot = PendingPreferenceSnapshot(social_id);
	if (!client || !snapshot)
		return;
	Json::Value root(Json::objectValue);
	// A pending save owns only its dirty fields. Preserve unrelated values that
	// may have just been loaded from a newer on-disk profile.
	root["config"] = ConfigFromClient(client);
	StoreConfigSnapshot(root, snapshot->config, snapshot->dirty_mask);
	LoadConfigIntoClient(client, root);
}

bool WriteOrLog(const std::filesystem::path &path, const Json::Value &root)
{
	const document_write_result_t write =
		WriteDocumentAtomically(path, root);
	if (write.committed) {
		if (!write.durability_warning.empty()) {
			gi.Com_PrintFmt(
				"{}: player profile write committed for \"{}\", but durability sync failed: {}\n",
				__FUNCTION__, path.string(), write.durability_warning);
		}
		return true;
	}
	gi.Com_PrintFmt("{}: player profile write failed for \"{}\": {}\n",
		__FUNCTION__, path.string(), write.error);
	return false;
}

bool ProfileMatchesIdentity(
	const Json::Value &root, std::string_view social_id)
{
	return root["socialID"].isString() &&
		root["socialID"].asString() == social_id;
}

bool PrepareRecoveredDocument(
	const std::filesystem::path &path,
	std::string_view reason,
	std::string_view social_id,
	std::string_view player_name,
	std::string_view gametype,
	const client_config_t &defaults,
	Json::Value &root)
{
	if (!QuarantineProfile(path, reason))
		return false;
	root = CreateProfile(social_id, player_name, gametype, defaults);
	return true;
}

bool LoadDocumentForUpdate(
	const std::filesystem::path &path,
	std::string_view social_id,
	std::string_view player_name,
	std::string_view gametype,
	const client_config_t &defaults,
	Json::Value &root,
	bool &document_modified,
	bool &document_recreated)
{
	document_modified = false;
	document_recreated = false;
	document_result_t document = ReadDocument(path);
	if (document.status == document_status_t::missing) {
		root = CreateProfile(social_id, player_name, gametype, defaults);
		document_modified = true;
		document_recreated = true;
		return true;
	}
	if (document.status == document_status_t::corrupt) {
		if (!PrepareRecoveredDocument(path, document.error, social_id, player_name,
				gametype, defaults, root)) {
			return false;
		}
		document_modified = true;
		document_recreated = true;
		return true;
	}
	if (document.status != document_status_t::ok) {
		gi.Com_PrintFmt("{}: player profile read failed for \"{}\": {}\n",
			__FUNCTION__, path.string(), document.error);
		return false;
	}

	if (!ProfileMatchesIdentity(document.root, social_id)) {
		if (!PrepareRecoveredDocument(path,
			"profile socialID does not match its canonical filename",
			social_id, player_name, gametype, defaults, root)) {
			return false;
		}
		document_modified = true;
		document_recreated = true;
		return true;
	}

	root = std::move(document.root);
	repair_result_t repair = RepairProfile(
		root, social_id, player_name, gametype, defaults, false);
	if (!repair.valid) {
		gi.Com_PrintFmt("{}: refusing to update player profile \"{}\": {}\n",
			__FUNCTION__, path.string(), repair.error);
		return false;
	}
	document_modified = repair.modified;
	return true;
}

preference_persist_result_t PersistPendingPreference(
	const preference_retry_candidate_t &candidate)
{
	preference_persist_result_t result;
	result.generation = candidate.generation;
	const auto path = ProfilePath(candidate.social_id);
	if (!path)
		return result;

	profile_file_lock_t profile_lock;
	if (!AcquireProfileLock(*path, profile_lock))
		return result;

	// Re-read the cache only after taking the file lock. A newer command may
	// have coalesced into this identity while this retry was waiting.
	const auto pending = PendingPreferenceSnapshot(candidate.social_id);
	if (!pending) {
		result.succeeded = true;
		result.generation.reset();
		return result;
	}
	result.generation = pending->generation;
	const auto gametype = NormalizeGametype(pending->gametype);
	if (!gametype)
		return result;

	const client_config_t defaults = MM_DefaultClientConfig();
	Json::Value root;
	bool document_modified = false;
	bool document_recreated = false;
	if (!LoadDocumentForUpdate(
			*path,
			candidate.social_id,
			pending->player_name,
			*gametype,
			defaults,
			root,
			document_modified,
			document_recreated)) {
		return result;
	}
	const bool config_modified =
		StoreConfigSnapshot(root, pending->config,
			MM_ClientProfilePreferenceMergeMask(
				document_recreated, pending->dirty_mask));
	if (!document_modified && !config_modified) {
		result.succeeded = true;
		return result;
	}
	if (document_modified && !config_modified)
		SetUpdatedTimestamp(root);
	result.succeeded = WriteOrLog(*path, root);
	return result;
}

uint64_t Counter(const Json::Value &stats, const char *key) noexcept
{
	uint64_t value = 0;
	JsonCounter(stats[key], value);
	return value;
}

void IncrementCounter(Json::Value &stats, const char *key, uint64_t amount = 1)
{
	stats[key] = CounterValue(SaturatingAdd(Counter(stats, key), amount));
}

std::string BoundedCommandToken(const char *token)
{
	if (!token)
		return {};
	return muffmode::TruncateWithEllipsis(token, 32);
}

} // namespace
} // namespace muffmode::client_profile

float MM_ClientProfileDefaultRating() noexcept
{
	return muffmode::client_profile::k_default_skill_rating;
}

bool MM_ClientProfileCanPersistIdentity(std::string_view social_id)
{
	return muffmode::client_profile::ProfilePath(social_id).has_value();
}

bool MM_ClientProfileCanonicalExists(std::string_view social_id)
{
	using namespace muffmode::client_profile;
	const auto path = ProfilePath(social_id);
	if (!path)
		return false;
	std::error_code error;
	const bool exists = std::filesystem::exists(*path, error);
	// This query only controls whether a stale line-based seed may be used.
	// Treat an indeterminate canonical path as present and fail closed.
	return exists || static_cast<bool>(error);
}

mm_client_profile_load_status_t MM_ClientProfileLoad(
	gclient_t *client,
	std::string_view social_id,
	std::string_view player_name,
	std::string_view gametype,
	const client_config_t *missing_profile_seed)
{
	using namespace muffmode::client_profile;
	if (!client)
		return mm_client_profile_load_status_t::skipped;

	const client_config_t defaults = MM_DefaultClientConfig();
	const client_config_t create_seed = missing_profile_seed
		? SanitizeConfigSeed(*missing_profile_seed)
		: defaults;
	if (client->sess.is_a_bot || social_id.empty())
		return mm_client_profile_load_status_t::skipped;

	const auto path = ProfilePath(social_id);
	const auto normalized_gametype = NormalizeGametype(gametype);
	if (!path || !normalized_gametype) {
		gi.Com_PrintFmt("{}: cannot map player identity or gametype to a profile\n", __FUNCTION__);
		ApplyLatestPendingPreferencesToClient(client, social_id);
		return mm_client_profile_load_status_t::failed;
	}
	profile_file_lock_t profile_lock;
	if (!AcquireProfileLock(*path, profile_lock)) {
		ApplyLatestPendingPreferencesToClient(client, social_id);
		return mm_client_profile_load_status_t::failed;
	}

	document_result_t document = ReadDocument(*path);
	std::optional<std::filesystem::path> migrated_from;
	std::string migrated_profile_kind;
	if (document.status == document_status_t::missing) {
		const auto try_migration_candidate = [&document, &migrated_from,
				&migrated_profile_kind, &path, social_id](
			const std::optional<std::filesystem::path> &candidate,
			std::string_view kind) {
			if (document.status != document_status_t::missing || !candidate ||
				*candidate == *path ||
				(migrated_from && *candidate == *migrated_from)) {
				return;
			}
			document_result_t legacy = ReadDocument(*candidate);
			if (legacy.status == document_status_t::ok) {
				// Root-level canonical files and WORR's lossy sanitizer can
				// occupy the same filename. Trust only an exact authenticated
				// identity match and never quarantine a foreign legacy file.
				if (ProfileMatchesIdentity(legacy.root, social_id)) {
					document = std::move(legacy);
					migrated_from = *candidate;
					migrated_profile_kind.assign(kind);
				} else {
					gi.Com_PrintFmt(
						"{}: ignored colliding {} player profile \"{}\"\n",
						"MM_ClientProfileLoad", kind, candidate->string());
				}
			} else if (legacy.status != document_status_t::missing) {
				gi.Com_PrintFmt(
					"{}: ignored unreadable {} player profile \"{}\": {}\n",
					"MM_ClientProfileLoad", kind, candidate->string(), legacy.error);
			}
		};

		const auto previous_canonical =
			PreviousCanonicalProfilePath(social_id);
		try_migration_candidate(previous_canonical, "root canonical");
		const auto worr_legacy = LegacyWorrProfilePath(social_id);
		if (!previous_canonical || !worr_legacy ||
			*worr_legacy != *previous_canonical) {
			try_migration_candidate(worr_legacy, "WORR legacy");
		}
	}
	if (document.status == document_status_t::missing) {
		Json::Value root = CreateProfile(
			social_id, player_name, *normalized_gametype, create_seed);
		const auto pending = PendingPreferenceSnapshot(social_id);
		if (pending)
			StoreConfigSnapshot(root, pending->config,
				MM_ClientProfilePreferenceMergeMask(
					true, pending->dirty_mask));
		if (!WriteOrLog(*path, root)) {
			ApplyLatestPendingPreferencesToClient(client, social_id);
			return mm_client_profile_load_status_t::failed;
		}
		if (pending)
			MarkPendingPreferencePersisted(social_id, pending->generation);
		ApplyProfileToClient(client, root, *normalized_gametype);
		ApplyLatestPendingPreferencesToClient(client, social_id);
		return mm_client_profile_load_status_t::created;
	}
	if (document.status == document_status_t::corrupt) {
		Json::Value root;
		if (!PrepareRecoveredDocument(*path, document.error, social_id,
				player_name, *normalized_gametype, create_seed, root)) {
			ApplyLatestPendingPreferencesToClient(client, social_id);
			return mm_client_profile_load_status_t::failed;
		}
		const auto pending = PendingPreferenceSnapshot(social_id);
		if (pending)
			StoreConfigSnapshot(root, pending->config,
				MM_ClientProfilePreferenceMergeMask(
					true, pending->dirty_mask));
		if (!WriteOrLog(*path, root)) {
			ApplyLatestPendingPreferencesToClient(client, social_id);
			return mm_client_profile_load_status_t::failed;
		}
		if (pending)
			MarkPendingPreferencePersisted(social_id, pending->generation);
		ApplyProfileToClient(client, root, *normalized_gametype);
		ApplyLatestPendingPreferencesToClient(client, social_id);
		return mm_client_profile_load_status_t::recovered;
	}
	if (document.status != document_status_t::ok) {
		gi.Com_PrintFmt("{}: player profile read failed for \"{}\": {}\n",
			__FUNCTION__, path->string(), document.error);
		ApplyLatestPendingPreferencesToClient(client, social_id);
		return mm_client_profile_load_status_t::failed;
	}
	if (!ProfileMatchesIdentity(document.root, social_id)) {
		Json::Value recovered;
		if (!PrepareRecoveredDocument(*path,
				"profile socialID does not match its canonical filename",
				social_id, player_name, *normalized_gametype,
				create_seed, recovered)) {
			ApplyLatestPendingPreferencesToClient(client, social_id);
			return mm_client_profile_load_status_t::failed;
		}
		const auto pending = PendingPreferenceSnapshot(social_id);
		if (pending)
			StoreConfigSnapshot(
				recovered, pending->config,
				MM_ClientProfilePreferenceMergeMask(
					true, pending->dirty_mask));
		if (!WriteOrLog(*path, recovered)) {
			ApplyLatestPendingPreferencesToClient(client, social_id);
			return mm_client_profile_load_status_t::failed;
		}
		if (pending)
			MarkPendingPreferencePersisted(social_id, pending->generation);
		ApplyProfileToClient(client, recovered, *normalized_gametype);
		ApplyLatestPendingPreferencesToClient(client, social_id);
		return mm_client_profile_load_status_t::recovered;
	}

	Json::Value &root = document.root;
	repair_result_t repair = RepairProfile(
		root,
		social_id,
		player_name,
		*normalized_gametype,
		defaults,
		true);
	if (!repair.valid) {
		gi.Com_PrintFmt("{}: refusing to load player profile \"{}\": {}\n",
			__FUNCTION__, path->string(), repair.error);
		ApplyLatestPendingPreferencesToClient(client, social_id);
		return mm_client_profile_load_status_t::failed;
	}

	const auto pending = PendingPreferenceSnapshot(social_id);
	const bool preference_modified = pending &&
		StoreConfigSnapshot(root, pending->config, pending->dirty_mask);
	if (repair.modified || migrated_from || preference_modified) {
		SetUpdatedTimestamp(root);
		if (!WriteOrLog(*path, root)) {
			ApplyLatestPendingPreferencesToClient(client, social_id);
			return mm_client_profile_load_status_t::failed;
		}
	}
	if (pending)
		MarkPendingPreferencePersisted(social_id, pending->generation);
	ApplyProfileToClient(client, root, *normalized_gametype);
	ApplyLatestPendingPreferencesToClient(client, social_id);
	if (migrated_from) {
		gi.Com_PrintFmt(
			"{}: migrated {} player profile \"{}\" to \"{}\"\n",
			__FUNCTION__, migrated_profile_kind, migrated_from->string(),
			path->string());
	}
	return repair.schema_repaired || migrated_from
		? mm_client_profile_load_status_t::repaired
		: mm_client_profile_load_status_t::loaded;
}

bool MM_ClientProfileSavePreferences(
	const gclient_t *client,
	std::string_view social_id,
	mm_client_profile_preference_mask_t dirty_mask)
{
	using namespace muffmode::client_profile;
	if (!client || client->sess.is_a_bot || social_id.empty() ||
		!client->sess.profile_persistence_ready) {
		return false;
	}
	const auto path = ProfilePath(social_id);
	if (!path)
		return false;

	const auto gametype = NormalizeGametype(CurrentGametypeKey());
	if (!gametype)
		return false;
	const std::string player_name =
		MM_PlayerNameForStorage(MM_PlayerDisplayName(client));
	if (!QueuePreferenceSave(
			social_id,
			ConfigFromClient(client),
			player_name,
			*gametype,
			dirty_mask)) {
		gi.Com_PrintFmt(
			"{}: pending player preference cache is full; preferences for \"{}\" remain session-only\n",
			__FUNCTION__, path->string());
		return false;
	}
	return true;
}

void MM_ClientProfile_RunFrame()
{
	using namespace muffmode::client_profile;
	const auto now = std::chrono::steady_clock::now();
	const std::vector<std::string> none_attempted;
	const auto candidate = BeginPreferenceRetry(
		now, false, none_attempted);
	if (!candidate)
		return;

	const preference_persist_result_t result =
		PersistPendingPreference(*candidate);
	FinishPreferenceRetry(*candidate, result,
		std::chrono::steady_clock::now());
}

void MM_ClientProfile_Shutdown()
{
	using namespace muffmode::client_profile;
	std::vector<std::string> attempted;
	attempted.reserve(k_max_pending_preference_snapshots);
	for (size_t attempt = 0;
		attempt < k_max_pending_preference_snapshots; ++attempt) {
		const auto candidate = BeginPreferenceRetry(
			std::chrono::steady_clock::now(), true, attempted);
		if (!candidate)
			break;
		attempted.push_back(candidate->social_id);
		const preference_persist_result_t result =
			PersistPendingPreference(*candidate);
		FinishPreferenceRetry(*candidate, result,
			std::chrono::steady_clock::now());
	}

	const size_t remaining = PendingPreferenceCount();
	if (remaining > 0) {
		gi.Com_PrintFmt(
			"MM_ClientProfile: {} pending preference snapshot{} could not be persisted before shutdown.\n",
			remaining, remaining == 1 ? "" : "s");
	}
}

std::string MM_ClientProfileSerializePreferences(const gclient_t *client)
{
	using namespace muffmode::client_profile;
	if (!client)
		return {};
	try {
		Json::StreamWriterBuilder builder;
		builder["commentStyle"] = "None";
		builder["indentation"] = "";
		std::string serialized = Json::writeString(
			builder, ConfigFromClient(client));
		if (serialized.empty() ||
			serialized.size() > k_max_preference_snapshot_bytes) {
			return {};
		}
		return serialized;
	} catch (const std::exception &) {
		return {};
	}
}

bool MM_ClientProfilePersistMatchResult(
	const mm_client_profile_match_result_t &result)
{
	using namespace muffmode::client_profile;
	if (result.social_id.empty() || !std::isfinite(result.skill_rating) ||
		result.skill_rating < 0.0f || result.skill_rating > k_max_skill_rating ||
		result.skill_rating_change < -k_max_skill_rating_change ||
		result.skill_rating_change > k_max_skill_rating_change) {
		return false;
	}
	switch (result.outcome) {
	case mm_client_profile_outcome_t::win:
	case mm_client_profile_outcome_t::loss:
	case mm_client_profile_outcome_t::draw:
	case mm_client_profile_outcome_t::abandon:
		break;
	default:
		return false;
	}
	const auto path = ProfilePath(result.social_id);
	const auto gametype = NormalizeGametype(result.gametype);
	if (!path || !gametype)
		return false;
	profile_file_lock_t profile_lock;
	if (!AcquireProfileLock(*path, profile_lock))
		return false;

	const client_config_t defaults = MM_DefaultClientConfig();
	Json::Value root;
	bool document_modified = false;
	bool document_recreated = false;
	if (!LoadDocumentForUpdate(
			*path,
			result.social_id,
			result.player_name,
			*gametype,
			defaults,
			root,
			document_modified,
			document_recreated)) {
		return false;
	}
	if (document_recreated) {
		Json::Value recovery_config;
		std::string recovery_error;
		if (!ParseRecoveryConfigSnapshot(
				result.recovery_preferences_json,
				defaults,
				recovery_config,
				recovery_error)) {
			gi.Com_PrintFmt(
				"{}: refusing to recreate player profile \"{}\" without a valid queued preference snapshot: {}\n",
				__FUNCTION__, path->string(), OneLine(recovery_error));
			return false;
		}
		StoreConfigSnapshot(root, recovery_config,
			MM_CLIENT_PROFILE_PREFERENCE_ALL);
	}

	Json::Value &stats = root["stats"];
	IncrementCounter(stats, "totalMatches");
	switch (result.outcome) {
	case mm_client_profile_outcome_t::win:
		IncrementCounter(stats, "totalWins");
		break;
	case mm_client_profile_outcome_t::loss:
		IncrementCounter(stats, "totalLosses");
		break;
	case mm_client_profile_outcome_t::draw:
		IncrementCounter(stats, "totalDraws");
		break;
	case mm_client_profile_outcome_t::abandon:
		IncrementCounter(stats, "totalAbandons");
		break;
	default:
		return false;
	}
	const auto pending = PendingPreferenceSnapshot(result.social_id);
	if (pending)
		StoreConfigSnapshot(root, pending->config, pending->dirty_mask);

	const uint64_t duration = result.duration_ms > 0
		? static_cast<uint64_t>(result.duration_ms)
		: 0;
	IncrementCounter(stats, "totalTimePlayedMs", duration);

	// [MuffMode] Career award tallies, one counter per catalog key. RepairStats
	// only normalizes a malformed object, so the first award a profile ever
	// earns is what creates it.
	if (result.match_awards && !result.match_awards->empty()) {
		Json::Value &awards = stats["awards"];
		if (!awards.isObject())
			awards = Json::Value(Json::objectValue);
		for (const std::string &key : *result.match_awards) {
			if (!key.empty())
				IncrementCounter(awards, key.c_str());
		}
	}

	root["ratings"][*gametype] = result.skill_rating;
	root["ratingChanges"][*gametype] = result.skill_rating_change;

	float best_rating = 0.0f;
	if (!JsonRating(stats["bestSkillRating"], best_rating))
		best_rating = 0.0f;
	stats["bestSkillRating"] = std::max(best_rating, result.skill_rating);
	stats["lastSkillRating"] = result.skill_rating;
	stats["lastSkillChange"] = result.skill_rating_change;
	stats["lastSkillGametype"] = *gametype;
	SetUpdatedTimestamp(root);
	const bool saved = WriteOrLog(*path, root);
	if (saved && pending)
		MarkPendingPreferencePersisted(
			result.social_id, pending->generation);
	return saved;
}

void MM_ClientProfileClearWeaponPreferences(gclient_t *client) noexcept
{
	if (!client)
		return;
	client->sess.weapon_prefs.fill(IT_NULL);
	client->sess.weapon_pref_count = 0;
}

mm_weapon_preference_result_t MM_ClientProfileAppendWeaponPreference(
	gclient_t *client,
	std::string_view token) noexcept
{
	using namespace muffmode::client_profile;
	if (!client)
		return mm_weapon_preference_result_t::invalid;
	const auto id = WeaponIdForToken(token);
	if (!id)
		return mm_weapon_preference_result_t::invalid;

	const size_t count = std::min(
		static_cast<size_t>(client->sess.weapon_pref_count),
		client->sess.weapon_prefs.size());
	for (size_t i = 0; i < count; i++) {
		if (client->sess.weapon_prefs[i] == *id)
			return mm_weapon_preference_result_t::duplicate;
	}
	if (count >= client->sess.weapon_prefs.size() ||
		count >= k_weapon_tokens.size() ||
		count >= std::numeric_limits<uint8_t>::max()) {
		return mm_weapon_preference_result_t::full;
	}
	client->sess.weapon_prefs[count] = *id;
	client->sess.weapon_pref_count = static_cast<uint8_t>(count + 1);
	return mm_weapon_preference_result_t::added;
}

std::string_view MM_ClientProfileWeaponPreferenceToken(item_id_t weapon) noexcept
{
	return muffmode::client_profile::WeaponToken(weapon);
}

std::string MM_ClientProfileWeaponPreferenceList(const gclient_t *client)
{
	using namespace muffmode::client_profile;
	if (!client)
		return {};
	std::string list;
	const size_t count = std::min(
		static_cast<size_t>(client->sess.weapon_pref_count),
		client->sess.weapon_prefs.size());
	for (size_t i = 0; i < count; i++) {
		const std::string_view token = WeaponToken(client->sess.weapon_prefs[i]);
		if (token.empty())
			continue;
		if (!list.empty())
			list.push_back(' ');
		list.append(token.data(), token.size());
	}
	return list;
}

size_t MM_ClientProfileBuildWeaponOrder(
	const gclient_t *client,
	item_id_t *output,
	size_t output_capacity) noexcept
{
	using namespace muffmode::client_profile;
	std::array<bool, IT_TOTAL> seen{};
	size_t total = 0;
	auto append = [&](item_id_t id) {
		if (!IsWeaponPreferenceId(id))
			return;
		const size_t index = static_cast<size_t>(id);
		if (seen[index])
			return;
		seen[index] = true;
		if (output && total < output_capacity)
			output[total] = id;
		total++;
	};

	if (client) {
		const size_t count = std::min(
			static_cast<size_t>(client->sess.weapon_pref_count),
			client->sess.weapon_prefs.size());
		for (size_t i = 0; i < count; i++)
			append(client->sess.weapon_prefs[i]);
	}
	for (item_id_t id : k_default_weapon_order)
		append(id);
	return output ? std::min(total, output_capacity) : total;
}

size_t MM_ClientProfileWeaponPreferenceRank(
	const gclient_t *client,
	item_id_t weapon) noexcept
{
	if (!muffmode::client_profile::IsWeaponPreferenceId(weapon))
		return std::numeric_limits<size_t>::max();
	std::array<item_id_t, IT_TOTAL> order{};
	const size_t count = MM_ClientProfileBuildWeaponOrder(
		client, order.data(), order.size());
	for (size_t i = 0; i < count; i++) {
		if (order[i] == weapon)
			return i;
	}
	return std::numeric_limits<size_t>::max();
}

bool MM_ClientProfilePrefersWeapon(
	const gclient_t *client,
	item_id_t candidate,
	item_id_t current) noexcept
{
	return MM_ClientProfileWeaponPreferenceRank(client, candidate) <
		MM_ClientProfileWeaponPreferenceRank(client, current);
}

void MM_CmdSetWeaponPref(gentity_t *ent)
{
	using namespace muffmode::client_profile;
	if (!ent || !ent->client)
		return;
	if (CheckFlood(ent))
		return;
	if ((ent->svflags & SVF_BOT) || ent->client->sess.is_a_bot) {
		gi.LocClient_Print(ent, PRINT_HIGH, "Weapon preferences are unavailable for bots.\n");
		return;
	}

	MM_ClientProfileClearWeaponPreferences(ent->client);
	std::vector<std::string> invalid;
	invalid.reserve(4);
	size_t duplicates = 0;
	bool full = false;
	const int argc = std::max(gi.argc(), 0);
	const size_t token_count = argc > 1
		? std::min(static_cast<size_t>(argc - 1), k_max_command_tokens)
		: 0;
	for (size_t i = 0; i < token_count; i++) {
		const char *token = gi.argv(static_cast<int>(i + 1));
		switch (MM_ClientProfileAppendWeaponPreference(
			ent->client, token ? std::string_view(token) : std::string_view())) {
		case mm_weapon_preference_result_t::added:
			break;
		case mm_weapon_preference_result_t::duplicate:
			duplicates++;
			break;
		case mm_weapon_preference_result_t::invalid:
			if (invalid.size() < 16)
				invalid.push_back(BoundedCommandToken(token));
			break;
		case mm_weapon_preference_result_t::full:
			full = true;
			break;
		}
	}
	if (static_cast<size_t>(std::max(argc - 1, 0)) > token_count)
		full = true;

	const bool persistable = MM_ClientProfileCanPersistIdentity(
		ent->client->pers.social_id);
	const bool saved = persistable && MM_ClientProfileSavePreferences(
		ent->client, ent->client->pers.social_id,
		MM_CLIENT_PROFILE_PREFERENCE_WEAPONS);
	if (!invalid.empty()) {
		std::string joined;
		for (const std::string &token : invalid) {
			if (!joined.empty())
				joined += ", ";
			joined += token;
		}
		gi.LocClient_Print(ent, PRINT_HIGH,
			"Ignored unsupported weapon preference{}: {}.\n",
			invalid.size() == 1 ? "" : "s", joined.c_str());
	}
	if (duplicates > 0)
		gi.LocClient_Print(ent, PRINT_HIGH,
			"Ignored {} duplicate weapon preference{}.\n",
			duplicates, duplicates == 1 ? "" : "s");
	if (full)
		gi.LocClient_Print(ent, PRINT_HIGH,
			"Weapon preference list was truncated to {} entries.\n",
			static_cast<int>(ent->client->sess.weapon_pref_count));

	const std::string list = MM_ClientProfileWeaponPreferenceList(ent->client);
	if (list.empty())
		gi.LocClient_Print(ent, PRINT_HIGH, "Weapon preference order cleared; using the default order.\n");
	else
		gi.LocClient_Print(ent, PRINT_HIGH,
			"Weapon preference order: {} (unlisted weapons use the default order).\n",
			list.c_str());
	if (!persistable)
		gi.LocClient_Print(ent, PRINT_HIGH,
			"Weapon preferences changed for this session only.\n");
	else if (!saved)
		gi.LocClient_Print(ent, PRINT_HIGH,
			"Weapon preferences changed for this session only; they could not be saved.\n");
}
