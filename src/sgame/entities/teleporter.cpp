// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "teleporter.h"
#include "muffmode/mm_arena.h"

namespace {

constexpr spawnflags_t SPAWNFLAG_TELEPORTER_NO_SOUND = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAG_TELEPORTER_NO_TELEPORT_EFFECT = 2_spawnflag;
constexpr spawnflags_t SPAWNFLAG_TELEPORTER_N64_EFFECT = 4_spawnflag;

struct TeleporterTriggerBounds {
	vec3_t mins = { -8, -8, 8 };
	vec3_t maxs = { 8, 8, 24 };
	bool show_spawnpad = true;
};

TeleporterTriggerBounds ResolveTriggerBounds(gentity_t *ent)
{
	TeleporterTriggerBounds bounds;

	if (!ent->target)
		return bounds;

	if (st.was_key_specified("mins"))
		bounds.mins = ent->mins;

	if (st.was_key_specified("maxs")) {
		bounds.maxs = ent->maxs;
		if (bounds.mins)
			bounds.show_spawnpad = false;
	}

	return bounds;
}

void SetupTeleporterSpawnPad(gentity_t *ent)
{
	gi.setmodel(ent, "models/objects/dmspot/tris.md2");
	ent->s.skinnum = 1;

	if (level.is_n64 || ent->spawnflags.has(SPAWNFLAG_TELEPORTER_N64_EFFECT))
		ent->s.effects = EF_TELEPORTER2;
	else
		ent->s.effects = EF_TELEPORTER;

	if (!ent->spawnflags.has(SPAWNFLAG_TELEPORTER_NO_SOUND))
		ent->s.sound = gi.soundindex("world/amb10.wav");

	ent->solid = SOLID_BBOX;
	ent->mins = { -32, -32, -24 };
	ent->maxs = { 32, 32, -16 };

	gi.linkentity(ent);
}

static TOUCH(teleporter_touch) (gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void
{
	if (!other->client)
		return;

	// [MuffMode] Positive RA2 arena teleporters are selectors, not spatial
	// teleporters. Enter the arena as an observer; its menu owns joining.
	if (GT(GT_ARENA) && self->arena > 0) {
		MM_Arena_MoveTo(other, self->arena, true);
		return;
	}

	gentity_t *dest = G_FindByString<&gentity_t::targetname>(nullptr, self->target);
	if (!dest) {
		gi.Com_PrintFmt("{}: Couldn't find destination, removing.\n", *self);
		G_FreeEntity(self);
		return;
	}

	TeleportPlayer(other, dest->s.origin, dest->s.angles);

	const bool draw_teleport_effect = !self->spawnflags.has(SPAWNFLAG_TELEPORTER_NO_TELEPORT_EFFECT);
	self->owner->s.event = draw_teleport_effect ? EV_PLAYER_TELEPORT : EV_OTHER_TELEPORT;
	other->s.event = draw_teleport_effect ? EV_PLAYER_TELEPORT : EV_OTHER_TELEPORT;
}

void SpawnTeleporterTrigger(gentity_t *ent, const TeleporterTriggerBounds &bounds)
{
	gentity_t *trigger = G_Spawn();
	trigger->classname = "teleporter_touch";
	trigger->touch = teleporter_touch;
	trigger->solid = SOLID_TRIGGER;
	trigger->target = ent->target;
	trigger->owner = ent;
	trigger->arena = ent->arena;
	trigger->s.origin = ent->s.origin;
	trigger->mins = bounds.mins;
	trigger->maxs = bounds.maxs;

	gi.linkentity(trigger);
}

} // namespace

void TeleportPlayer(gentity_t *player, vec3_t origin, vec3_t angles)
{
	Weapon_Grapple_DoReset(player->client);

	gi.unlinkentity(player);

	player->s.origin = origin;
	player->s.old_origin = origin;
	player->s.origin[2] += 10;

	TeleporterVelocity(player, angles);

	player->client->ps.pmove.delta_angles = angles - player->client->resp.cmd_angles;

	player->s.angles = {};
	player->client->ps.viewangles = {};
	player->client->v_angle = {};
	AngleVectors(player->client->v_angle, player->client->v_forward, nullptr, nullptr);

	gi.linkentity(player);

	KillBox(player, !!player->client);

	if (player->client->owned_sphere) {
		gentity_t *sphere = player->client->owned_sphere;
		sphere->s.origin = player->s.origin;
		sphere->s.origin[2] = player->absmax[2];
		sphere->s.angles[YAW] = player->s.angles[YAW];
		gi.linkentity(sphere);
	}
}

/*QUAKED misc_teleporter (1 0 0) (-32 -32 -24) (32 32 -16) NO_SOUND NO_TELEPORT_EFFECT N64_EFFECT x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Stepping onto this disc will teleport players to the targeted misc_teleporter_dest object.
*/
void SP_misc_teleporter(gentity_t *ent)
{
	const TeleporterTriggerBounds bounds = ResolveTriggerBounds(ent);

	if (bounds.show_spawnpad)
		SetupTeleporterSpawnPad(ent);

	// N64 has some of these for visual effects only. RA2 selector pads often
	// omit target entirely; their positive arena key is the destination.
	if (!ent->target && !(GT(GT_ARENA) && ent->arena > 0))
		return;

	SpawnTeleporterTrigger(ent, bounds);
}

/*QUAKED misc_teleporter_dest (1 0 0) (-32 -32 -24) (32 32 -16) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Point teleporters at these.
*/
void SP_misc_teleporter_dest(gentity_t *ent)
{
	if (level.is_n64)
		return;

	if (g_dm_spawnpads->integer > 1 || (g_dm_spawnpads->integer == 1 && ItemSpawnsEnabled() && notGT(GT_HORDE))) {
		if (!level.no_dm_spawnpads) {
			gi.setmodel(ent, "models/objects/dmspot/tris.md2");
			ent->s.skinnum = 0;
			ent->solid = SOLID_BBOX;
			ent->clipmask |= MASK_SOLID;

			ent->mins = { -32, -32, -24 };
			ent->maxs = { 32, 32, -16 };
			gi.linkentity(ent);
		}
	}
}
