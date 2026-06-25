// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

// [MuffMode] Offline-ish bot nav generator.
//
// Grid-samples the current map's walkable floor using live gi.trace, builds a
// walk-only node graph, and writes a valid KEX "NAV3" v6 .nav file to
// baseq2/bots/navigation/<mapname>.nav. The result is the Walk backbone; special
// links (teleporters, jumps, lifts) are added afterwards in QuakeNavEditor.
//
// Driven by the "sv nav_bake [grid]" server command (see sgame/core/server_commands.cpp).
// Format spec: docs-dev/nav-file-format.md
void MM_NavBake(float gridArg);
