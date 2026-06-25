// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstdint>

#if defined(MM_USE_TRACY)
#include <tracy/Tracy.hpp>
#define MM_PROFILE_ZONE(name) ZoneScopedN(name)
#define MM_PROFILE_FRAME_MARK(name) FrameMarkNamed(name)
#else
#define MM_PROFILE_ZONE(name) do { (void)sizeof(name); } while (false)
#define MM_PROFILE_FRAME_MARK(name) do { (void)sizeof(name); } while (false)
#endif

#if defined(MM_ENABLE_PROFILE_COUNTERS)
#include <atomic>

struct mm_profile_counters_t {
	std::atomic<uint64_t> frame_runs{ 0 };
	std::atomic<uint64_t> frame_main_loop_runs{ 0 };
	std::atomic<uint64_t> frame_non_main_loop_runs{ 0 };
	std::atomic<uint64_t> frame_entities_visited{ 0 };
	std::atomic<uint64_t> frame_clients_visited{ 0 };
	std::atomic<uint64_t> frame_nonclients_visited{ 0 };
	std::atomic<uint64_t> frame_no_client_skips{ 0 };

	std::atomic<uint64_t> trigger_touch_calls{ 0 };
	std::atomic<uint64_t> trigger_box_entities{ 0 };
	std::atomic<uint64_t> trigger_touch_dispatches{ 0 };

	std::atomic<uint64_t> projectile_touch_calls{ 0 };
	std::atomic<uint64_t> projectile_traces{ 0 };
	std::atomic<uint64_t> projectile_skipped{ 0 };
	std::atomic<uint64_t> projectile_impacts{ 0 };
	std::atomic<uint64_t> projectile_skip_overflows{ 0 };
	std::atomic<uint64_t> projectile_max_skipped_per_call{ 0 };

	std::atomic<uint64_t> killbox_calls{ 0 };
	std::atomic<uint64_t> killbox_box_entities{ 0 };
	std::atomic<uint64_t> killbox_damage_events{ 0 };

	std::atomic<uint64_t> los_visible_calls{ 0 };
	std::atomic<uint64_t> los_visible_traces{ 0 };
};

inline mm_profile_counters_t &MM_ProfileCounters() noexcept {
	static mm_profile_counters_t counters;
	return counters;
}

inline void MM_ProfileAdd(std::atomic<uint64_t> &counter, uint64_t value) noexcept {
	counter.fetch_add(value, std::memory_order_relaxed);
}

inline void MM_ProfileMax(std::atomic<uint64_t> &counter, uint64_t value) noexcept {
	uint64_t current = counter.load(std::memory_order_relaxed);

	while (current < value &&
		!counter.compare_exchange_weak(current, value, std::memory_order_relaxed, std::memory_order_relaxed)) {
	}
}

#define MM_PROFILE_INC(field) MM_ProfileAdd(MM_ProfileCounters().field, 1)
#define MM_PROFILE_ADD(field, value) MM_ProfileAdd(MM_ProfileCounters().field, static_cast<uint64_t>(value))
#define MM_PROFILE_MAX(field, value) MM_ProfileMax(MM_ProfileCounters().field, static_cast<uint64_t>(value))
#else
#define MM_PROFILE_INC(field) do { } while (false)
#define MM_PROFILE_ADD(field, value) do { (void)(value); } while (false)
#define MM_PROFILE_MAX(field, value) do { (void)(value); } while (false)
#endif
