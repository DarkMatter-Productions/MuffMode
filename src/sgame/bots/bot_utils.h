// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

void Entity_UpdateState( gentity_t * entity );
const gentity_t * FindLocalPlayer();
const gentity_t * FindFirstBot();
const gentity_t * FindFirstMonster();
const gentity_t * FindActorUnderCrosshair( const gentity_t * player );
