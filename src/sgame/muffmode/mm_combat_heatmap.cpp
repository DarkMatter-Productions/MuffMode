// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_combat_heatmap.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <unordered_map>

namespace muffmode::combat_heatmap {

namespace {

constexpr float kCellSize = 256.0f;
constexpr float kEventRadius = 512.0f;
constexpr float kDecayPerSecond = 4.0f;
constexpr float kMinCellHeat = 0.01f;
constexpr float kDefaultQueryRadius = 320.0f;
constexpr float kMaxQueryRadius = 4096.0f;
constexpr float kMaxEventHeat = 120.0f;
constexpr float kMaxCellHeat = 1000.0f;
constexpr float kFullDangerHeat = 75.0f;
constexpr size_t kMaxPruneChecksPerFrame = 128;

struct Cell {
	float heat = 0.0f;
	gtime_t touched = 0_ms;
	bool initialized = false;
};

struct Key {
	int32_t x = 0;
	int32_t y = 0;

	constexpr bool operator==(const Key &other) const noexcept
	{
		return x == other.x && y == other.y;
	}
};

struct KeyHash {
	size_t operator()(const Key &key) const noexcept
	{
		uint32_t a = static_cast<uint32_t>(key.x);
		const uint32_t b = static_cast<uint32_t>(key.y);
		a ^= b + 0x9e3779b9u + (a << 6) + (a >> 2);
		return static_cast<size_t>(a);
	}
};

std::unordered_map<Key, Cell, KeyHash> s_cells;
size_t s_prune_cursor = 0;

bool IsFiniteVec3(const vec3_t &origin)
{
	return std::isfinite(origin[0]) && std::isfinite(origin[1]) && std::isfinite(origin[2]);
}

bool CellCoordinate(float value, int32_t &out)
{
	const double cell = std::floor(static_cast<double>(value) / kCellSize);
	if (!std::isfinite(cell) ||
		cell < static_cast<double>(std::numeric_limits<int32_t>::min()) ||
		cell > static_cast<double>(std::numeric_limits<int32_t>::max()))
		return false;

	out = static_cast<int32_t>(cell);
	return true;
}

bool CellFromOrigin(const vec3_t &origin, int32_t &x, int32_t &y)
{
	return CellCoordinate(origin[0], x) && CellCoordinate(origin[1], y);
}

bool OffsetKey(int32_t x, int32_t y, int dx, int dy, Key &out)
{
	const int64_t ox = static_cast<int64_t>(x) + dx;
	const int64_t oy = static_cast<int64_t>(y) + dy;
	if (ox < std::numeric_limits<int32_t>::min() || ox > std::numeric_limits<int32_t>::max() ||
		oy < std::numeric_limits<int32_t>::min() || oy > std::numeric_limits<int32_t>::max())
		return false;

	out = { static_cast<int32_t>(ox), static_cast<int32_t>(oy) };
	return true;
}

float NormalizeQueryRadius(float radius)
{
	if (!std::isfinite(radius) || radius <= 0.0f)
		return kDefaultQueryRadius;

	return std::clamp(radius, 1.0f, kMaxQueryRadius);
}

void ApplyDecay(Cell &cell, const gtime_t &now)
{
	if (!cell.initialized) {
		cell.touched = now;
		cell.initialized = true;
		return;
	}

	const float dt = (now - cell.touched).seconds<float>();
	if (!std::isfinite(dt) || dt < 0.0f) {
		cell.touched = now;
		return;
	}

	if (dt > 0.0f) {
		if (cell.heat > 0.0f)
			cell.heat = std::max(0.0f, cell.heat - (kDecayPerSecond * dt));
		cell.touched = now;
	}
}

void Deposit(const Key &key, float amount, const gtime_t &now)
{
	Cell &cell = s_cells[key];
	ApplyDecay(cell, now);
	cell.heat = std::min(kMaxCellHeat, cell.heat + amount);
}

float RadialFalloff(float distance)
{
	if (distance >= kEventRadius)
		return 0.0f;

	const float t = 1.0f - (distance / kEventRadius);
	return 0.5f - 0.5f * cosf(PIf * t);
}

} // namespace

void Init()
{
	s_cells.clear();
	s_prune_cursor = 0;
}

void ResetForNewLevel()
{
	Init();
}

void AddEvent(const vec3_t &origin, float amount)
{
	if (!deathmatch->integer || amount <= 0.0f || !std::isfinite(amount) || !IsFiniteVec3(origin))
		return;

	amount = std::min(amount, kMaxEventHeat);

	const gtime_t now = level.time;
	int32_t cx = 0;
	int32_t cy = 0;
	if (!CellFromOrigin(origin, cx, cy))
		return;

	const int reach = static_cast<int>(ceilf(kEventRadius / kCellSize)) + 1;

	for (int dy = -reach; dy <= reach; ++dy) {
		for (int dx = -reach; dx <= reach; ++dx) {
			Key key;
			if (!OffsetKey(cx, cy, dx, dy, key))
				continue;

			const vec3_t center = {
				(key.x + 0.5f) * kCellSize,
				(key.y + 0.5f) * kCellSize,
				origin[2]
			};

			const float weight = RadialFalloff((center - origin).length());
			if (weight > 0.0f)
				Deposit(key, amount * weight, now);
		}
	}
}

float Query(const vec3_t &origin, float radius)
{
	if (!deathmatch->integer || !IsFiniteVec3(origin))
		return 0.0f;

	const gtime_t now = level.time;
	const float query_radius = NormalizeQueryRadius(radius);
	int32_t cx = 0;
	int32_t cy = 0;
	if (!CellFromOrigin(origin, cx, cy))
		return 0.0f;

	const int reach = static_cast<int>(ceilf(query_radius / kCellSize)) + 1;

	float sum = 0.0f;
	for (int dy = -reach; dy <= reach; ++dy) {
		for (int dx = -reach; dx <= reach; ++dx) {
			Key key;
			if (!OffsetKey(cx, cy, dx, dy, key))
				continue;

			auto it = s_cells.find(key);
			if (it == s_cells.end())
				continue;

			Cell &cell = it->second;
			ApplyDecay(cell, now);

			const vec3_t center = {
				(key.x + 0.5f) * kCellSize,
				(key.y + 0.5f) * kCellSize,
				origin[2]
			};

			const float distance = (center - origin).length();
			if (distance > query_radius)
				continue;

			const float weight = 1.0f - (distance / query_radius);
			sum += cell.heat * weight;
		}
	}

	return sum;
}

float DangerAt(const vec3_t &origin)
{
	if (!deathmatch->integer)
		return 0.0f;

	const float raw = Query(origin, kDefaultQueryRadius);
	if (!std::isfinite(raw))
		return 1.0f;

	return std::clamp(raw / (raw + kFullDangerHeat), 0.0f, 1.0f);
}

void RunFrame()
{
	if (!deathmatch->integer || s_cells.empty())
		return;

	const gtime_t now = level.time;
	if (s_prune_cursor >= s_cells.size())
		s_prune_cursor = 0;

	auto it = s_cells.begin();
	std::advance(it, static_cast<std::ptrdiff_t>(s_prune_cursor));

	for (size_t checked = 0; checked < kMaxPruneChecksPerFrame && !s_cells.empty(); ++checked) {
		if (it == s_cells.end()) {
			it = s_cells.begin();
			s_prune_cursor = 0;
		}

		auto current = it++;
		ApplyDecay(current->second, now);
		if (current->second.heat <= kMinCellHeat) {
			s_cells.erase(current);
		} else {
			++s_prune_cursor;
		}

		if (!s_cells.empty() && s_prune_cursor >= s_cells.size())
			s_prune_cursor = 0;
	}
}

} // namespace muffmode::combat_heatmap
