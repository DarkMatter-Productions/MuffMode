// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "muffmode/mm_factory_rules.h"
#include "muffmode/mm_parse.h"
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

	// factories.cfg is operator-authored text the game module parses at level
	// start. Parsing must terminate on anything, honour its own bounds, and
	// never emit a factory it would refuse to name -- a rejected document has to
	// stay rejected rather than half-loading.
	{
		using namespace muffmode::factory;

		const factory_parse_result_t parsed =
			MM_ParseFactoryDocument(input.c_str(), "fuzz.cfg");
		if (parsed.factories.size() > MM_FACTORY_MAX_FACTORIES)
			std::abort();
		for (const parsed_factory_t &factory : parsed.factories) {
			if (!MM_IsSafeFactoryId(factory.id))
				std::abort();
			if (!MM_IsSafeFactoryTitle(factory.title))
				std::abort();
			if (factory.overrides.size() > MM_FACTORY_MAX_OVERRIDES)
				std::abort();
			if (factory.scope.maps.size() > MM_FACTORY_MAX_MAPS ||
				factory.scope.mappool.size() > MM_FACTORY_MAX_MAPS)
				std::abort();
		}
	}

	return 0;
}
