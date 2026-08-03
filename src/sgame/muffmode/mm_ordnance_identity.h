// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include <cstdint>

struct gentity_t;

// Entity slots remain address-stable but are reusable. Keep the base lifetime
// predicate independent from the game DLL so synchronous damage/think paths can
// reject a freed or recycled slot before consulting any other entity state.
inline constexpr bool MM_OrdnanceGenerationMatches(
	std::int32_t captured_generation, bool current_inuse,
	std::int32_t current_generation) noexcept
{
	return current_inuse && captured_generation == current_generation;
}

// Delayed ordnance keeps a pointer to an owner or target entity, but entity
// slots are reusable. A pointer is authoritative only while its generation
// still matches and, in multi-arena play, it remains in the room captured when
// the ordnance was created.
inline constexpr bool MM_OrdnanceIdentityMatches(
	std::int32_t captured_generation, int captured_arena,
	bool current_inuse, bool current_connected,
	std::int32_t current_generation, bool arena_active,
	int current_arena) noexcept
{
	return MM_OrdnanceGenerationMatches(captured_generation, current_inuse,
		current_generation) && current_connected &&
		(!arena_active || captured_arena == current_arena);
}

// Entity-aware delayed-ordnance helpers are implemented with the game module
// so every persistent projectile uses the same generation/arena contract.
void MM_CaptureOrdnanceOwner(gentity_t *ordnance, gentity_t *owner);
gentity_t *MM_ResolveOrdnanceOwner(const gentity_t *ordnance);
gentity_t *MM_ResolveOrdnanceOwner(const gentity_t *ordnance,
	gentity_t *owner);
bool MM_DiscardOrphan(gentity_t *ordnance);
bool MM_DiscardOrphan(gentity_t *ordnance, gentity_t *owner);
bool MM_OrdnanceEntityIdentityMatches(const gentity_t *entity,
	std::int32_t generation, int arena_id);
