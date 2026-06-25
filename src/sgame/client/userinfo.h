// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "g_local.h"

namespace muffmode::player {

constexpr float kDefaultUserinfoFov = 90.0f;

float ParseUserinfoFov(const char *value);

} // namespace muffmode::player
