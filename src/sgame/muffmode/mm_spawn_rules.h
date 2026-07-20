// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstddef>
#include <cstring>
#include <string>
#include <utility>

// Host-testable helpers for map-load ownership of non-trivial level state.
// SpawnEntities must destroy prior std::string members on every gamemap; memset
// alone leaks their heap blocks and is undefined for non-trivial types.

struct mm_level_cpp_state_t {
	std::string vote_arg;
	std::string entstring;
	std::string match_id;
};

// Destroy any prior dynamic members, then adopt a single owned entity-lump copy.
inline void MM_ResetLevelCppState(mm_level_cpp_state_t &state, std::string &&entity_string)
{
	state = {};
	state.entstring = std::move(entity_string);
}

inline const char *MM_EntityStringCStr(const mm_level_cpp_state_t &state) noexcept
{
	return state.entstring.c_str();
}

// Replace the final path component of an absolute/relative file path in-place.
// Returns false when the result would not fit in `capacity` (including NUL).
inline bool MM_ReplacePathFilename(char *path, size_t capacity, const char *filename) noexcept
{
	if (!path || !filename || capacity == 0)
		return false;

	char *slash = nullptr;
	for (char *p = path; *p; ++p) {
		if (*p == '\\' || *p == '/')
			slash = p;
	}

	const size_t dir_len = slash ? static_cast<size_t>(slash - path + 1) : 0;
	const size_t file_len = std::strlen(filename);
	if (dir_len + file_len + 1 > capacity)
		return false;

	std::memcpy(path + dir_len, filename, file_len + 1);
	return true;
}

// Join a directory and filename into `out`. Appends a separator when needed.
inline bool MM_JoinDirectoryFile(char *out, size_t capacity, const char *directory,
	const char *filename) noexcept
{
	if (!out || !directory || !filename || capacity == 0)
		return false;

	const size_t dir_len = std::strlen(directory);
	const size_t file_len = std::strlen(filename);
	const bool needs_sep = dir_len > 0 && directory[dir_len - 1] != '\\' &&
		directory[dir_len - 1] != '/';
	const size_t sep_len = needs_sep ? 1u : 0u;

	if (dir_len + sep_len + file_len + 1 > capacity)
		return false;

	std::memcpy(out, directory, dir_len);
	size_t cursor = dir_len;
	if (needs_sep)
		out[cursor++] = '\\';
	std::memcpy(out + cursor, filename, file_len + 1);
	return true;
}
