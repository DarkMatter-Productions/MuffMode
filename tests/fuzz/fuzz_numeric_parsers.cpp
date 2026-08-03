// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "muffmode/mm_parse.h"
#include "muffmode/mm_gametype_cfg_rules.h"
#include "muffmode/mm_spawn_rules.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	std::string input(reinterpret_cast<const char *>(data), size);
	input.push_back('\0');

	(void)MM_ParseIntArg(input.c_str());
	(void)MM_ParseNonNegativeIntArg(input.c_str());
	(void)MM_ParseFloatArg(input.c_str());
	(void)MM_ParseCfgIntArg(input.c_str());
	(void)muffmode::gametype::MM_GtCfgLineViolatesSlotCap(
		std::string_view(input.data(), size), 4);

	// Entity-string field values reach the game module straight from a BSP or a
	// .ent override. Expansion must stay inside the caller's buffer, always
	// terminate, and never claim more bytes than the source could produce.
	const std::string_view value(input.data(), size);
	char expanded[64];
	const size_t written = MM_UnescapeEntityValue(value, expanded, sizeof(expanded));
	if (written >= sizeof(expanded) || expanded[written] != '\0')
		std::abort();
	if (MM_UnescapedEntityValueLength(value) > value.size())
		std::abort();

	// Colour components arrive as arbitrary authored floats; packing them must be
	// total, so drive it with raw bit patterns including NaN and infinities.
	if (size >= sizeof(std::array<float, 4>)) {
		std::array<float, 4> components{};
		std::memcpy(components.data(), data, sizeof(components));
		(void)MM_PackEntityColorRgba(components);
	}

	return 0;
}
