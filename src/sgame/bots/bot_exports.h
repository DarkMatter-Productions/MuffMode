// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

void	Bot_SetWeapon( gentity_t * bot, const int weaponIndex, const bool instantSwitch );
void	Bot_TriggerEntity( gentity_t * bot, gentity_t * entity );
int32_t Bot_GetItemID( const char * classname );
void	Bot_UseItem( gentity_t * bot, const int32_t itemID );
void    Entity_ForceLookAtPoint( gentity_t * entity, gvec3_cref_t point );
bool    Bot_PickedUpItem( gentity_t * bot, gentity_t * item );
