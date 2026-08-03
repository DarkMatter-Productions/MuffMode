// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace muffmode::reliable_text {

// Match the established structured-map-list print budget. This is the maximum
// caller-controlled text argument supplied to one localized reliable print.
inline constexpr size_t kMaxPrintPayloadBytes = 900;
inline constexpr size_t kUtf8BoundarySlackBytes = 3;
static_assert(kMaxPrintPayloadBytes > kUtf8BoundarySlackBytes);
inline constexpr size_t kGuaranteedPrintContentBytes =
	kMaxPrintPayloadBytes - kUtf8BoundarySlackBytes;
// Conservatively account for the localized-print opcode, level, base string,
// argument metadata, and terminators in the game-side fan-out scope.
inline constexpr size_t kReliablePrintAccountingOverheadBytes = 32;
inline constexpr size_t kMaxMotdCommandMessages = 8;
inline constexpr size_t kMaxMyMapQueueMessages = 4;
inline constexpr std::string_view kTruncatedNotice =
	"\n[Output truncated by server.]\n";

struct chunk_plan_t {
	std::vector<std::string_view> chunks;
	size_t planned_bytes = 0;
	bool truncated = false;
};

inline size_t Utf8CodePointLength(
	std::string_view text, size_t offset) noexcept
{
	if (offset >= text.size())
		return 0;

	const uint8_t lead = static_cast<uint8_t>(text[offset]);
	if (lead <= 0x7f)
		return 1;

	size_t length = 0;
	uint32_t codepoint = 0;
	if (lead >= 0xc2 && lead <= 0xdf) {
		length = 2;
		codepoint = lead & 0x1f;
	} else if (lead >= 0xe0 && lead <= 0xef) {
		length = 3;
		codepoint = lead & 0x0f;
	} else if (lead >= 0xf0 && lead <= 0xf4) {
		length = 4;
		codepoint = lead & 0x07;
	} else {
		return 1;
	}

	if (length > text.size() - offset)
		return 1;
	for (size_t i = 1; i < length; i++) {
		const uint8_t continuation =
			static_cast<uint8_t>(text[offset + i]);
		if ((continuation & 0xc0) != 0x80)
			return 1;
		codepoint = (codepoint << 6) | (continuation & 0x3f);
	}

	const uint32_t minimum = length == 2 ? 0x80u :
		length == 3 ? 0x800u : 0x10000u;
	if (codepoint < minimum || codepoint > 0x10ffffu ||
		(codepoint >= 0xd800u && codepoint <= 0xdfffu)) {
		return 1;
	}
	return length;
}

inline size_t Utf8PrefixLength(
	std::string_view text, size_t max_bytes) noexcept
{
	size_t length = 0;
	while (length < text.size()) {
		const size_t codepoint_bytes = Utf8CodePointLength(text, length);
		if (!codepoint_bytes || length > max_bytes ||
			codepoint_bytes > max_bytes - length) {
			break;
		}
		length += codepoint_bytes;
	}
	return length;
}

inline chunk_plan_t PlanChunks(
	std::string_view text,
	size_t max_chunk_bytes = kMaxPrintPayloadBytes,
	size_t max_chunks = 1)
{
	chunk_plan_t plan;
	if (text.empty())
		return plan;
	if (!max_chunk_bytes || !max_chunks) {
		plan.truncated = true;
		return plan;
	}

	const size_t chunks_for_size =
		text.size() / max_chunk_bytes +
		(text.size() % max_chunk_bytes != 0 ? 1u : 0u);
	plan.chunks.reserve(std::min(max_chunks, chunks_for_size));

	size_t offset = 0;
	while (offset < text.size() && plan.chunks.size() < max_chunks) {
		const size_t length = Utf8PrefixLength(
			text.substr(offset), max_chunk_bytes);
		if (!length)
			break;
		plan.chunks.emplace_back(text.data() + offset, length);
		plan.planned_bytes += length;
		offset += length;
	}

	plan.truncated = offset < text.size();
	return plan;
}

inline std::string MakeBoundedPreview(
	std::string_view text,
	size_t max_bytes = kMaxPrintPayloadBytes,
	std::string_view truncated_notice = kTruncatedNotice)
{
	if (text.size() <= max_bytes)
		return std::string(text);
	if (!max_bytes)
		return {};

	const size_t notice_bytes = Utf8PrefixLength(
		truncated_notice, max_bytes);
	const size_t prefix_bytes = Utf8PrefixLength(
		text, max_bytes - notice_bytes);

	std::string preview;
	preview.reserve(prefix_bytes + notice_bytes);
	preview.append(text.data(), prefix_bytes);
	preview.append(truncated_notice.data(), notice_bytes);
	return preview;
}

} // namespace muffmode::reliable_text
