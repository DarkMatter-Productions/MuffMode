// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
/*
==============================================================================

boss3

==============================================================================
*/

#include "../g_local.h"
#include "m_boss32.h"

USE(Use_Boss3) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_BOSSTPORT);
	gi.WritePosition(self->s.origin);
	gi.multicast(self->s.origin, MULTICAST_PHS, false);

	// just hide, don't kill ent so we can trigger it again
	self->svflags |= SVF_NOCLIENT;
	self->solid = SOLID_NOT;
}

static THINK(Think_Boss3Stand) (gentity_t *self) -> void {
	if (self->s.frame == FRAME_stand260)
		self->s.frame = FRAME_stand201;
	else
		self->s.frame++;
	self->nextthink = level.time + 10_hz;
}

/*QUAKED monster_boss3_stand (1 .5 0) (-32 -32 0) (32 32 90) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP

Just stands and cycles in one place until targeted, then teleports away.
*/
void SP_monster_boss3_stand(gentity_t *self) {
	if (!M_AllowSpawn(self)) {
		G_FreeEntity(self);
		return;
	}

	self->movetype = MOVETYPE_STEP;
	self->solid = SOLID_BBOX;
	self->model = "models/monsters/boss3/rider/tris.md2";
	self->s.modelindex = gi.modelindex(self->model);
	self->s.frame = FRAME_stand201;

	gi.soundindex("misc/bigtele.wav");

	self->mins = { -32, -32, 0 };
	self->maxs = { 32, 32, 90 };

	self->use = Use_Boss3;
	self->think = Think_Boss3Stand;
	self->nextthink = level.time + FRAME_TIME_S;
	gi.linkentity(self);
}
