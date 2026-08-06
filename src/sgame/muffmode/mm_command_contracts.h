// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstdint>

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

inline int MM_ClampResumeCountdownSeconds(int seconds) {
	constexpr int MM_MAX_RESUME_COUNTDOWN_SECONDS = 120;

	if (seconds < 0)
		return 0;
	if (seconds > MM_MAX_RESUME_COUNTDOWN_SECONDS)
		return MM_MAX_RESUME_COUNTDOWN_SECONDS;
	return seconds;
}

inline bool MM_ClientMaySetItemOverride(bool admin_commands_enabled, bool authenticated_admin) {
	return admin_commands_enabled && authenticated_admin;
}

inline int MM_ClampFloodMessageCount(int configured_count, int history_capacity) {
	if (configured_count <= 0 || history_capacity <= 0)
		return 0;
	if (configured_count > history_capacity)
		return history_capacity;
	return configured_count;
}

inline int MM_NormalizeRingIndex(int index, int capacity) {
	if (capacity <= 0)
		return 0;

	const int remainder = index % capacity;
	return remainder < 0 ? remainder + capacity : remainder;
}

struct mm_flood_history_query_t {
	int head = 0;
	int configured_count = 0;
	int history_capacity = 0;
};

inline int MM_FloodHistoryIndex(const mm_flood_history_query_t &query) {
	if (query.history_capacity <= 0)
		return 0;

	const int normalized_head = MM_NormalizeRingIndex(query.head, query.history_capacity);
	const int message_count = MM_ClampFloodMessageCount(
		query.configured_count, query.history_capacity);
	if (!message_count)
		return normalized_head;

	const int64_t index = static_cast<int64_t>(normalized_head) +
		query.history_capacity - message_count + 1;
	return static_cast<int>(index % query.history_capacity);
}

inline constexpr int MM_MAX_FRAMES_PER_SERVER_FRAME = 64;

inline int MM_ClampFramesPerServerFrame(int configured_count) {
	if (configured_count < 0)
		return 0;
	if (configured_count > MM_MAX_FRAMES_PER_SERVER_FRAME)
		return MM_MAX_FRAMES_PER_SERVER_FRAME;
	return configured_count;
}
