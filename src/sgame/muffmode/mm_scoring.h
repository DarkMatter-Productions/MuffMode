// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

struct gclient_t;

// Shared player-score policy used by arena-style modes.
int MM_AwardDamageScore(gclient_t *client, int damage);
void MM_ResetClientScoring(gclient_t *client);
