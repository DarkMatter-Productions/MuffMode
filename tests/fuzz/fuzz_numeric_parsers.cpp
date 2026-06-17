// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "muffmode/mm_parse.h"

#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	std::string input(reinterpret_cast<const char *>(data), size);
	input.push_back('\0');

	(void)MM_ParseIntArg(input.c_str());
	(void)MM_ParseNonNegativeIntArg(input.c_str());
	(void)MM_ParseFloatArg(input.c_str());
	(void)MM_ParseCfgIntArg(input.c_str());

	return 0;
}
