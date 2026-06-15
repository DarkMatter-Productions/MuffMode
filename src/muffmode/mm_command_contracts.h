// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

inline bool MM_IsTeleportArgcValid(int argc) {
	return argc == 4 || argc == 7;
}

inline bool MM_IsSpawnArgcValid(int argc) {
	return argc >= 2 && ((argc - 2) % 2) == 0;
}
