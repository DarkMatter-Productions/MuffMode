// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "shared/gameplay.h"

#include <cstddef>
#include <string_view>

// [MuffMode] Deathmatch centerprints are sent with a marker character so MuffMode clients can tell
// them apart from ordinary ones: CG_ParseCenterPrint strips the marker and skips the console echo,
// keeping match chatter out of the console. Stock clients have no such rule — they render and echo
// the message as-is, so the marker is a space rather than a visible glyph.
inline constexpr char MM_CENTERPRINT_MARKER = ' ';

// print_type_t packs PRINT_BROADCAST / PRINT_NO_NOTIFY as high bitflags on top of the message type,
// so the type has to be masked out before comparing.
inline constexpr int MM_PRINT_TYPE_MASK = 0x7;

inline bool MM_IsCenterPrintLevel(print_type_t level)
{
	return (static_cast<int>(level) & MM_PRINT_TYPE_MASK) == static_cast<int>(PRINT_CENTER);
}

// Offset just past any leading "%bind:...%" run — where the marker belongs. The marker cannot go in
// front of the run: stock clients only recognise binds at offset 0, so a leading marker would make
// them render the raw "%bind:inven:Open menu%" text. Mirrors CG_ParseCenterPrint's bind grammar.
inline size_t MM_CenterPrintMarkerOffset(std::string_view text)
{
	constexpr std::string_view kBindPrefix = "%bind:";

	size_t offset = 0;
	while (text.size() - offset >= kBindPrefix.size() &&
		text.compare(offset, kBindPrefix.size(), kBindPrefix) == 0) {
		const size_t end_of_bind = text.find('%', offset + 1);
		if (end_of_bind == std::string_view::npos)
			break;
		offset = end_of_bind + 1;
	}

	return offset;
}

// Rejects an empty message, one that is nothing but binds, and a '$' localization key — the client
// looks a key up whole, so a marker would break the lookup. A message that already starts with a
// space is still marked: the client strips exactly one, so the original text round-trips intact.
inline bool MM_CenterPrintBaseAcceptsMarker(std::string_view base)
{
	const size_t offset = MM_CenterPrintMarkerOffset(base);
	if (offset >= base.size())
		return false;

	return base[offset] != '$';
}

// Returns base with the marker inserted, or base unchanged when no marker applies (not deathmatch,
// not a centerprint, unmarkable base, or the marked form would not fit). The returned pointer is
// owned by the call and stays valid until the next call, matching the G_Fmt idiom.
const char *MM_MarkCenterPrint(print_type_t level, const char *base);
