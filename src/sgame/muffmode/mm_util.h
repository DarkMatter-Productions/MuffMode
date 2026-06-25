// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace muffmode {

inline bool IsNullOrEmpty(const char *text) noexcept
{
	return !text || !*text;
}

inline bool CStringEquals(const char *lhs, const char *rhs) noexcept
{
	if (!lhs || !rhs)
		return lhs == rhs;

	return std::strcmp(lhs, rhs) == 0;
}

inline bool CStringEqualsI(const char *lhs, const char *rhs) noexcept
{
	if (!lhs || !rhs)
		return lhs == rhs;

	return Q_strcasecmp(lhs, rhs) == 0;
}

inline bool CStringLessI(const std::string &lhs, const std::string &rhs) noexcept
{
	return Q_strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
}

inline const char *CvarString(const cvar_t *cvar) noexcept
{
	return (cvar && cvar->string) ? cvar->string : "";
}

inline std::string_view CvarStringView(const cvar_t *cvar) noexcept
{
	return CvarString(cvar);
}

inline int CvarInteger(const cvar_t *cvar) noexcept
{
	return cvar ? cvar->integer : 0;
}

inline float CvarValue(const cvar_t *cvar) noexcept
{
	return cvar ? cvar->value : 0.f;
}

inline bool CvarEnabled(const cvar_t *cvar) noexcept
{
	return CvarInteger(cvar) != 0;
}

inline bool LocalTimeSnapshot(std::time_t value, std::tm &out) noexcept
{
#ifdef _WIN32
	return localtime_s(&out, &value) == 0;
#else
	return localtime_r(&value, &out) != nullptr;
#endif
}

struct FileCloser {
	void operator()(FILE *file) const noexcept
	{
		if (file)
			std::fclose(file);
	}
};

using unique_file = std::unique_ptr<FILE, FileCloser>;

inline unique_file OpenFile(const char *path, const char *mode)
{
	return unique_file(std::fopen(path, mode));
}

template <typename T, size_t N>
constexpr int CountAsInt(const T (&)[N]) noexcept
{
	static_assert(N <= static_cast<size_t>(std::numeric_limits<int>::max()), "array is too large for int count");
	return static_cast<int>(N);
}

template <size_t N>
inline void CopyString(char (&dest)[N], std::string_view value)
{
	static_assert(N > 0);

	const size_t count = std::min(value.size(), N - 1);
	if (count > 0)
		std::copy_n(value.data(), count, dest);
	dest[count] = '\0';
}

inline std::vector<std::string> SplitTokens(std::string_view text, char delimiter)
{
	std::vector<std::string> tokens;
	size_t start = 0;

	while (true) {
		start = text.find_first_not_of(delimiter, start);
		if (start == std::string_view::npos)
			break;

		const size_t end = text.find(delimiter, start);
		tokens.emplace_back(text.substr(start, end == std::string_view::npos ? end : end - start));

		if (end == std::string_view::npos)
			break;

		start = end + 1;
	}

	return tokens;
}

} // namespace muffmode
