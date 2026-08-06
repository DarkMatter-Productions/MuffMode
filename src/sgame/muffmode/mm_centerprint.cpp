// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_centerprint.h"

#include <cstring>

const char *MM_MarkCenterPrint(print_type_t level, const char *base)
{
	// Single shared buffer, consumed by the Loc_Print call in the same statement (G_Fmt idiom).
	static char marked[MAX_STRING_CHARS];

	if (!base || !deathmatch || !deathmatch->integer)
		return base;

	if (!MM_IsCenterPrintLevel(level))
		return base;

	const std::string_view text(base);
	if (!MM_CenterPrintBaseAcceptsMarker(text))
		return base;

	// marker + text + NUL
	if (text.size() + 2 > sizeof(marked))
		return base;

	const size_t offset = MM_CenterPrintMarkerOffset(text);

	memcpy(marked, text.data(), offset);
	marked[offset] = MM_CENTERPRINT_MARKER;
	memcpy(marked + offset + 1, text.data() + offset, text.size() - offset);
	marked[text.size() + 1] = '\0';

	return marked;
}
