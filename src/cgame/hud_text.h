// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

// Small, engine-independent helpers kept here so client input and rendering
// boundaries can be covered by the host test executable.
constexpr bool CG_TryPackedStatId(int32_t id, size_t id_count, uint8_t &packed_id) noexcept
{
	if (id < 0 || static_cast<size_t>(id) >= id_count ||
		static_cast<uint64_t>(id) > std::numeric_limits<uint8_t>::max())
		return false;

	packed_id = static_cast<uint8_t>(id);
	return true;
}

constexpr int64_t CG_HUDViewportCenter(int32_t origin, int32_t extent) noexcept
{
	return static_cast<int64_t>(origin) + (static_cast<int64_t>(extent) / 2);
}

// Converts a logical HUD coordinate to renderer pixels without allowing any
// signed arithmetic to overflow. Invalid values leave pixel_coordinate alone.
constexpr bool CG_TryScaleHUDCoordinate(int64_t logical_coordinate, int32_t scale,
	int64_t pixel_adjustment, int32_t &pixel_coordinate) noexcept
{
	if (logical_coordinate < std::numeric_limits<int32_t>::min() ||
		logical_coordinate > std::numeric_limits<int32_t>::max())
		return false;

	// Both operands now fit int32_t, so their product always fits int64_t.
	const int64_t scaled = logical_coordinate * static_cast<int64_t>(scale);
	if ((pixel_adjustment > 0 && scaled > std::numeric_limits<int64_t>::max() - pixel_adjustment) ||
		(pixel_adjustment < 0 && scaled < std::numeric_limits<int64_t>::min() - pixel_adjustment))
		return false;

	const int64_t adjusted = scaled + pixel_adjustment;
	if (adjusted < std::numeric_limits<int32_t>::min() ||
		adjusted > std::numeric_limits<int32_t>::max())
		return false;

	pixel_coordinate = static_cast<int32_t>(adjusted);
	return true;
}

// Applies a renderer-space adjustment to an already scaled coordinate.  Layout
// commands are allowed to select any int32 coordinate, so every later glyph,
// icon, alignment, and line-spacing offset must remain checked as well.
constexpr bool CG_TryAddHUDPixels(int32_t coordinate, int64_t adjustment,
	int32_t &adjusted_coordinate) noexcept
{
	const int64_t minimum_adjustment =
		static_cast<int64_t>(std::numeric_limits<int32_t>::min()) - coordinate;
	const int64_t maximum_adjustment =
		static_cast<int64_t>(std::numeric_limits<int32_t>::max()) - coordinate;
	if (adjustment < minimum_adjustment || adjustment > maximum_adjustment)
		return false;

	adjusted_coordinate = static_cast<int32_t>(
		static_cast<int64_t>(coordinate) + adjustment);
	return true;
}

constexpr bool CG_TryHUDTextExtent(size_t glyph_count, int32_t glyph_width,
	int32_t scale, int32_t &pixel_extent) noexcept
{
	if (glyph_width < 0 || scale <= 0)
		return false;
	if (glyph_count > static_cast<size_t>(std::numeric_limits<int64_t>::max()))
		return false;

	const int64_t count = static_cast<int64_t>(glyph_count);
	if (glyph_width != 0 &&
		count > std::numeric_limits<int64_t>::max() / glyph_width)
		return false;
	const int64_t logical_extent = count * glyph_width;
	if (logical_extent != 0 &&
		scale > std::numeric_limits<int64_t>::max() / logical_extent)
		return false;

	const int64_t extent = logical_extent * scale;
	if (extent > std::numeric_limits<int32_t>::max())
		return false;

	pixel_extent = static_cast<int32_t>(extent);
	return true;
}

constexpr bool CG_HUDBlinkVisible(uint64_t time_ms, uint64_t half_period_ms = 256) noexcept
{
	return half_period_ms != 0 && ((time_ms / half_period_ms) & 1u) != 0;
}

constexpr bool CG_IsValidHUDLocalizationArgCount(int32_t count, size_t maximum) noexcept
{
	return count >= 0 && static_cast<size_t>(count) <= maximum;
}

constexpr bool CG_IsValidHUDNumericWidth(int32_t width) noexcept
{
	// Numeric fields render from a 16-byte local decimal buffer. Wider values
	// cannot add visible precision and would make offset arithmetic unbounded.
	return width >= 0 && width <= 15;
}

constexpr bool CG_HUDLayoutConditionsBalanced(int32_t depth, bool skipping) noexcept
{
	return depth == 0 && !skipping;
}

constexpr size_t CG_FindEndOfUTF8Codepoint(std::string_view text, size_t position) noexcept
{
	if (position >= text.size())
		return std::string_view::npos;

	for (size_t i = position; i < text.size(); i++) {
		const uint8_t ch = static_cast<uint8_t>(text[i]);
		if ((ch & 0x80) == 0 || (ch & 0xC0) != 0x80)
			return i;
	}

	return std::string_view::npos;
}

// Returns the largest prefix that does not end midway through a UTF-8 sequence.
// Invalid complete bytes are preserved; this helper only removes an incomplete
// trailing sequence produced by a bounded renderer copy.
constexpr size_t CG_CompleteUTF8PrefixLength(std::string_view text) noexcept
{
	size_t position = 0;
	size_t complete = 0;
	while (position < text.size()) {
		const uint8_t lead = static_cast<uint8_t>(text[position]);
		size_t sequence_length = 1;
		if ((lead & 0xE0) == 0xC0)
			sequence_length = 2;
		else if ((lead & 0xF0) == 0xE0)
			sequence_length = 3;
		else if ((lead & 0xF8) == 0xF0)
			sequence_length = 4;

		if (sequence_length == 1) {
			position++;
			complete = position;
			continue;
		}
		if (sequence_length > text.size() - position)
			break;

		bool continuation_bytes_valid = true;
		for (size_t i = 1; i < sequence_length; i++) {
			const uint8_t byte = static_cast<uint8_t>(text[position + i]);
			if ((byte & 0xC0) != 0x80) {
				continuation_bytes_valid = false;
				break;
			}
		}
		if (!continuation_bytes_valid) {
			position++;
			complete = position;
			continue;
		}

		position += sequence_length;
		complete = position;
	}
	return complete;
}

constexpr std::string_view CG_CenterPrintVisiblePrefix(
	std::string_view line, size_t revealed_length, bool complete) noexcept
{
	if (complete)
		return line;

	return line.substr(0, revealed_length < line.size() ? revealed_length : line.size());
}

struct cg_hud_line_t {
	const char *next = nullptr;
	size_t length = 0;
};

// Copies one newline-delimited HUD line while always consuming the complete
// source line. Long server/localization text is truncated to the destination
// instead of overflowing the renderer's stack buffer.
template<size_t N>
cg_hud_line_t CG_CopyHUDLine(const char *input, char (&output)[N]) noexcept
{
	static_assert(N > 0);

	size_t length = 0;
	if (!input) {
		output[0] = '\0';
		return {};
	}

	while (*input && *input != '\n') {
		if (length + 1 < N)
			output[length++] = *input;
		input++;
	}

	output[length] = '\0';
	return { input, length };
}

bool CG_UseKFont();

int CG_DrawHUDString(const char *string, int x, int y, int centerwidth, int xor_mask, int scale, bool shadow = true);
