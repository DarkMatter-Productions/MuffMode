// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

inline bool MM_IsTeleportArgcValid(int argc) {
	return argc == 4 || argc == 7;
}

inline bool MM_IsSpawnArgcValid(int argc) {
	return argc >= 2 && ((argc - 2) % 2) == 0;
}

inline bool MM_IsExactArgcValid(int argc, int expected) {
	return argc == expected;
}

inline bool MM_IsArgcInRangeValid(int argc, int min_expected, int max_expected) {
	if (min_expected > max_expected)
		return false;
	return argc >= min_expected && argc <= max_expected;
}

inline int MM_ClampTimeoutSeconds(int seconds) {
	constexpr int MM_MAX_TIMEOUT_SECONDS = 3600;

	if (seconds < 0)
		return 0;
	if (seconds > MM_MAX_TIMEOUT_SECONDS)
		return MM_MAX_TIMEOUT_SECONDS;
	return seconds;
}
