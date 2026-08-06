// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "muffmode/mm_gibs.h"
#include "muffmode/mm_gibs_rules.h"

#include <cmath>

static_assert(MM_GIBS_HEALTH_THRESHOLD == GIB_HEALTH,
	"mm_gibs_rules.h must track GIB_HEALTH");

namespace {

// Stock rerelease assets. Every path below is present in baseq2's pak0.pak; the
// impact sounds and the leg model are simply never referenced by vanilla code.
constexpr const char *kImpactSounds[] = {
	"player/gibimp1.wav",
	"player/gibimp2.wav",
	"player/gibimp3.wav"
};

constexpr const char *kGibMeat = "models/objects/gibs/sm_meat/tris.md2";
constexpr const char *kGibLeg = "models/objects/gibs/leg/tris.md2";
constexpr const char *kGibBone = "models/objects/gibs/bone/tris.md2";
constexpr const char *kGibBone2 = "models/objects/gibs/bone2/tris.md2";
constexpr const char *kGibArm = "models/objects/gibs/arm/tris.md2";
constexpr const char *kGibChest = "models/objects/gibs/chest/tris.md2";

// Per-gib state. Both fields are unused by gibs in vanilla, and both are reset
// in MM_Gibs_Finalize because a head gib is a recycled corpse rather than a
// fresh G_Spawn and can arrive carrying whatever the player left behind.
//   ent->count -> impact effects already spent by this gib
//   ent->style -> kState* bits below
constexpr int32_t kStateUpright = 1 << 0;
constexpr int32_t kStateOrganic = 1 << 1;
constexpr int32_t kStateSubmerged = 1 << 2;

// A gib gets at most this many impact effects over its whole life. Without it a
// single deep overkill can put fifty gibs into a corridor and every wall bounce
// asks for a sound channel and a temp entity.
constexpr int32_t kMaxImpactsPerGib = 2;

// Impacts below this speed are the tail of a bounce settling out, not a hit
// worth announcing.
constexpr float kMinImpactSpeed = 140.0f;

constexpr gtime_t kImpactDebounce = 350_ms;

// Server-wide ceiling on impact effects, so that simultaneous deaths across a
// large server cannot flood the temp-entity stream.
constexpr gtime_t kEffectWindow = 100_ms;
constexpr int32_t kEffectsPerWindow = 3;

// Gibs are flat-lit in vanilla, which reads as a glowing chunk in a dark room.
// Losing RF_FULLBRIGHT lets them take map lighting instead.
constexpr float kScaleJitterMin = 0.85f;
constexpr float kScaleJitterMax = 1.15f;

// Guarantees every gib tumbles. Vanilla's frandom(600) can hand out a near-zero
// spin, which leaves a chunk sliding through the air perfectly rigid.
constexpr float kSpinMin = 200.0f;
constexpr float kSpinMax = 600.0f;

// Launch envelope. Vanilla clamps each axis to 300 independently, which both
// squares off the spray and discards most of the killing blow's knockback;
// scaling to a single higher ceiling keeps the direction intact.
constexpr mm_gib_launch_bounds_t kLaunchBounds{ 600.0f, 150.0f, 600.0f };

// Fraction of a gib's speed retained after one second underwater, and the share
// of gravity that buoyancy cancels. Vanilla runs gibs through G_Physics_Toss,
// which applies no water handling at all, so they fall through a pool exactly as
// they would through air.
constexpr float kWaterSpeedRetainedPerSecond = 0.15f;
constexpr float kWaterBuoyancy = 0.6f;

// How long a gib spends fading and sinking once its lifetime runs out.
constexpr gtime_t kSinkDuration = 1500_ms;

// s.alpha of 0 means "use the default", so the fade has to stop just above it.
constexpr float kMinFadeAlpha = 0.04f;

// Live-gib budget. The ring is a hard bound on tracked references; g_gib_max
// picks the working cap inside it.
constexpr size_t kMaxTracked = 1024;

struct tracked_gib_t {
	int32_t entnum = 0;
	int32_t spawn_count = 0;
};

tracked_gib_t tracked[kMaxTracked];
size_t tracked_head = 0;
size_t tracked_count = 0;

gtime_t effect_window_start = 0_ms;
int32_t effects_this_window = 0;

// G_FreeEntity refuses to memset client and body-queue slots; it only unlinks
// them. Anything written to entity_state_t on those slots therefore survives into
// the owner's next life, and a player would respawn wearing whatever scale or
// alpha the head gib was left at. The two effects that persist are limited to
// gibs holding an ordinary, recyclable slot.
bool IsRecyclableSlot(const gentity_t *ent) {
	return (ent - g_entities) > static_cast<ptrdiff_t>(game.maxclients + BODY_QUEUE_SIZE);
}

gentity_t *ResolveTracked(const tracked_gib_t &entry) {
	if (entry.entnum <= 0 || static_cast<uint32_t>(entry.entnum) >= globals.num_entities)
		return nullptr;

	gentity_t *gib = &g_entities[entry.entnum];
	if (!gib->inuse || gib->spawn_count != entry.spawn_count)
		return nullptr;

	return gib;
}

void PopTrackedFront() {
	tracked_head = (tracked_head + 1) % kMaxTracked;
	--tracked_count;
}

// Retires references whose entity has already gone away on its own terms, so the
// budget measures live gibs rather than throws.
void PruneTrackedFront() {
	while (tracked_count && !ResolveTracked(tracked[tracked_head]))
		PopTrackedFront();
}

void ReleaseOldestTracked() {
	while (tracked_count) {
		gentity_t *gib = ResolveTracked(tracked[tracked_head]);
		PopTrackedFront();
		if (gib) {
			G_FreeEntity(gib);
			return;
		}
	}
}

size_t GibBudget() {
	if (!g_gib_max || g_gib_max->integer <= 0)
		return 0; // unlimited

	return min(static_cast<size_t>(g_gib_max->integer), kMaxTracked);
}

bool ClaimImpactEffect() {
	if (level.time - effect_window_start >= kEffectWindow) {
		effect_window_start = level.time;
		effects_this_window = 0;
	}

	if (effects_this_window >= kEffectsPerWindow)
		return false;

	++effects_this_window;
	return true;
}

void EmitImpactEffect(gentity_t *self, const trace_t &tr, bool organic) {
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(organic ? TE_BLOOD : TE_SPARKS);
	gi.WritePosition(self->s.origin);
	gi.WriteDir(tr.plane.normal);
	gi.multicast(self->s.origin, MULTICAST_PVS, false);

	// Debris has no stock impact sound worth reusing, so metal gets the spark
	// burst alone rather than a borrowed flesh sample.
	if (organic && g_gib_impact_effects->integer > 1)
		gi.sound(self, CHAN_AUTO, gi.soundindex(random_element(kImpactSounds)), 1, ATTN_NORM, 0);
}

TOUCH(MM_Gibs_Touch) (gentity_t *self, gentity_t *other, const trace_t &tr, bool other_touching_self) -> void {
	if (other == self->owner)
		return;

	// A gib that reaches a sky brush has left the map; bouncing it off the
	// inside of the skybox is the one outcome that always looks wrong.
	if (tr.surface && (tr.surface->flags & SURF_SKY)) {
		G_FreeEntity(self);
		return;
	}

	const float speed = self->velocity.length();

	// Settle flat on floors. Vanilla only did this for GIB_UPRIGHT; applying it
	// to every gib as it comes to rest stops elongated bone and limb models from
	// standing on end, but waiting for the gib to slow down first preserves the
	// tumble on the way there.
	if (tr.plane.normal[2] > 0.7f && ((self->style & kStateUpright) || speed < 100.0f)) {
		self->s.angles[PITCH] = clamp(self->s.angles[PITCH], -5.0f, 5.0f);
		self->s.angles[ROLL] = clamp(self->s.angles[ROLL], -5.0f, 5.0f);
	}

	if (!g_gib_impact_effects->integer)
		return;

	if (speed < kMinImpactSpeed || self->count >= kMaxImpactsPerGib)
		return;

	if (level.time < self->pain_debounce_time)
		return;

	if (!ClaimImpactEffect())
		return;

	self->pain_debounce_time = level.time + kImpactDebounce;
	++self->count;

	EmitImpactEffect(self, tr, (self->style & kStateOrganic) != 0);
}

// Fades the gib out as it sinks rather than dropping a fully opaque chunk
// through the floor.
THINK(MM_Gibs_Sink) (gentity_t *ent) -> void {
	if (level.time >= ent->timestamp) {
		ent->svflags |= SVF_NOCLIENT;
		ent->takedamage = false;
		ent->solid = SOLID_NOT;
		G_FreeEntity(ent);
		return;
	}

	if (IsRecyclableSlot(ent)) {
		const float remaining = (ent->timestamp - level.time).seconds() / kSinkDuration.seconds();
		ent->s.alpha = max(kMinFadeAlpha, min(1.0f, remaining));
	}

	ent->s.origin[2] -= 0.5f;
	ent->nextthink = level.time + 50_ms;
}

void BeginSink(gentity_t *ent) {
	ent->think = MM_Gibs_Sink;
	ent->timestamp = level.time + kSinkDuration;
	ent->nextthink = level.time + 50_ms;
	ent->touch = nullptr;
}

// Applies the water handling G_Physics_Toss never had. Both terms are expressed
// per second and converted with the current frame time so behaviour does not
// shift with the server's tick rate.
void ApplyWaterMotion(gentity_t *ent) {
	const bool submerged = ent->waterlevel > WATER_NONE;
	const bool was_submerged = (ent->style & kStateSubmerged) != 0;

	if (submerged != was_submerged) {
		if (submerged) {
			ent->style |= kStateSubmerged;

			// The engine already plays the splash from G_Physics_Toss; this is
			// the trail it never emitted.
			if (ent->velocity.length() > kMinImpactSpeed && ClaimImpactEffect()) {
				gi.WriteByte(svc_temp_entity);
				gi.WriteByte(TE_BUBBLETRAIL);
				gi.WritePosition(ent->s.origin - (ent->velocity * gi.frame_time_s));
				gi.WritePosition(ent->s.origin);
				gi.multicast(ent->s.origin, MULTICAST_PVS, false);
			}
		} else {
			ent->style &= ~kStateSubmerged;
		}
	}

	if (!submerged)
		return;

	const float drag = MM_GibsWaterDrag(kWaterSpeedRetainedPerSecond, gi.frame_time_s);
	ent->velocity *= drag;
	ent->avelocity *= drag;

	// Cancel part of gravity along whichever way it currently points, so gravity
	// zones and inverted-gravity maps stay consistent.
	ent->velocity -= ent->gravityVector *
		(ent->gravity * level.gravity * gi.frame_time_s * kWaterBuoyancy);
}

THINK(MM_Gibs_Think) (gentity_t *self) -> void {
	if (level.time >= self->timestamp) {
		BeginSink(self);
		return;
	}

	// At rest with nothing left to animate: sleep straight through to the sink,
	// so a corridor full of settled gibs costs one think each rather than one a
	// frame. Toss physics zeroes the velocity when a gib comes to rest, which
	// also covers one that has settled on the bottom of a pool.
	if (self->groundentity && self->velocity.lengthSquared() <= 0.0f) {
		self->nextthink = self->timestamp;
		return;
	}

	ApplyWaterMotion(self);

	// Fly nose-first along the velocity, easing back toward the last held
	// orientation as the gib slows, so a chunk that has nearly stopped keeps
	// whatever attitude it had rather than swinging around to follow a dying
	// velocity. Vanilla tumbles pitch and yaw on a fixed random avelocity, which
	// makes long models like bones and limbs drift sideways through the air.
	//
	// Roll is deliberately left alone: MM_Gibs_Finalize keeps a random roll
	// avelocity, and the physics pass adds it after this think runs. That gives
	// every gib its own spin rate for free instead of driving them all at one
	// rate from here.
	const float speed_sq = self->velocity.lengthSquared();
	const float reference_sq = self->speed * self->speed;

	if (speed_sq > 0.0f) {
		const float frac = reference_sq > 0.0f
			? clamp(speed_sq / reference_sq, 0.0f, 1.0f)
			: 1.0f;

		const vec3_t bearing = vectoangles(self->velocity);
		self->s.angles[PITCH] = LerpAngle(self->s.angles[PITCH], bearing[PITCH], frac);
		self->s.angles[YAW] = LerpAngle(self->s.angles[YAW], bearing[YAW], frac);
	}

	self->nextthink = level.time + FRAME_TIME_S;
}

} // namespace

void MM_Gibs_Precache() {
	// sm_meat, arm, bone, bone2, chest, skull, head2 and sm_metal are already
	// registered by the shared gib precache; only the leg model and the impact
	// sounds are new, and both ship with the base game.
	gi.modelindex(kGibLeg);

	for (const char *sound : kImpactSounds)
		gi.soundindex(sound);
}

void MM_Gibs_ClearAll() {
	tracked_head = 0;
	tracked_count = 0;
	effect_window_start = 0_ms;
	effects_this_window = 0;
}

bool MM_Gibs_Enhanced() {
	return g_gib_enhanced && g_gib_enhanced->integer;
}

void MM_Gibs_ClipVelocity(gentity_t *gib) {
	if (!gib)
		return;

	if (!MM_Gibs_Enhanced()) {
		ClipGibVelocity(gib);
		return;
	}

	const mm_gib_launch_t clipped = MM_GibsClipLaunch(
		{ gib->velocity[0], gib->velocity[1], gib->velocity[2] }, kLaunchBounds);

	gib->velocity[0] = clipped.x;
	gib->velocity[1] = clipped.y;
	gib->velocity[2] = clipped.z;
}

void MM_Gibs_Finalize(gentity_t *gib, bool organic, bool upright) {
	if (!gib)
		return;

	// A head gib is the corpse entity itself, so none of these can be assumed
	// to arrive clear.
	gib->count = 0;
	gib->style = 0;
	gib->pain_debounce_time = 0_ms;
	gib->speed = gib->velocity.length();

	if (organic)
		gib->style |= kStateOrganic;
	if (upright)
		gib->style |= kStateUpright;
	if (gib->waterlevel > WATER_NONE)
		gib->style |= kStateSubmerged;

	gib->s.renderfx &= ~RF_FULLBRIGHT;

	if (IsRecyclableSlot(gib)) {
		gib->s.alpha = 0.0f;
		gib->s.scale = (gib->s.scale != 0.0f ? gib->s.scale : 1.0f) *
			frandom(kScaleJitterMin, kScaleJitterMax);
	}

	// Pitch and yaw are owned by MM_Gibs_Think's velocity alignment, so leaving
	// vanilla's tumble on them would just be overwritten every frame. Roll keeps
	// a random rate, in both directions, and the physics pass spins it: that is
	// what stops a spray from looking like one rigid object turning.
	gib->avelocity[0] = 0.0f;
	gib->avelocity[1] = 0.0f;
	gib->avelocity[2] = frandom(kSpinMin, kSpinMax) * (brandom() ? 1.0f : -1.0f);

	// Gibs are SOLID_NOT, so G_Impact only calls touch for them with
	// FL_ALWAYS_TOUCH set. Vanilla granted that to GIB_UPRIGHT alone, which is
	// why nothing else could react to landing.
	gib->touch = MM_Gibs_Touch;
	gib->flags |= FL_ALWAYS_TOUCH;

	gib->think = MM_Gibs_Think;
	gib->nextthink = level.time + FRAME_TIME_S;
}

void MM_Gibs_Track(gentity_t *gib) {
	// Same predicate G_FreeEntity uses to refuse a slot: anything the budget
	// cannot actually free must not occupy a place in it.
	if (!gib || !IsRecyclableSlot(gib))
		return;

	const size_t budget = GibBudget();
	if (!budget)
		return;

	PruneTrackedFront();

	while (tracked_count >= budget)
		ReleaseOldestTracked();

	if (tracked_count >= kMaxTracked)
		ReleaseOldestTracked();

	const size_t slot = (tracked_head + tracked_count) % kMaxTracked;
	tracked[slot].entnum = static_cast<int32_t>(gib - g_entities);
	tracked[slot].spawn_count = gib->spawn_count;
	++tracked_count;
}

int32_t MM_Gibs_Severity(int32_t health) {
	return MM_GibsSeverity(health);
}

int32_t MM_Gibs_PlayerGibCeiling() {
	return MM_GibsWorstCaseCount();
}

void MM_Gibs_ThrowPlayerGibs(gentity_t *self, int32_t damage) {
	if (!self)
		return;

	gi.sound(self, CHAN_BODY, gi.soundindex("misc/udeath.wav"), 1, ATTN_NORM, 0);

	// Deeper overkills get meatier. The thresholds sit further out than
	// vanilla's -80/-120/-160 because the limb and bone models below now carry
	// the variety that the extra chunk count used to stand in for, and because
	// stacking every tier is what makes a single death cost fifty entities.
	struct gib_stage_t {
		int32_t threshold;
		size_t count;
	};

	static constexpr gib_stage_t kStages[] = {
		{ -300, 16 },
		{ -200, 12 },
		{ -100, 10 }
	};

	if (deathmatch->integer) {
		for (const gib_stage_t &stage : kStages)
			if (self->health < stage.threshold)
				ThrowGibs(self, damage, { { stage.count, kGibMeat, GIB_METALLIC } });
	}

	ThrowGibs(self, damage, { { 8, kGibMeat, GIB_METALLIC } });

	if (!MM_Gibs_Enhanced())
		return;

	// Everything below is stock content the base game precaches for monsters and
	// never gives to a player.
	const mm_gib_limb_budget_t budget = MM_GibsLimbBudget(MM_GibsSeverity(self->health));

	struct limb_def_t {
		const char *model;
		int32_t max_count;
	};

	const limb_def_t limbs[] = {
		{ kGibLeg, budget.legs },
		{ kGibBone, budget.bones },
		{ kGibBone2, budget.forearms },
		{ kGibArm, budget.arms },
		{ kGibChest, budget.torsos }
	};

	for (const limb_def_t &limb : limbs) {
		if (limb.max_count <= 0)
			continue;

		const int32_t count = irandom(limb.max_count + 1);
		if (count > 0)
			ThrowGibs(self, damage, { { static_cast<size_t>(count), limb.model, GIB_METALLIC } });
	}
}
