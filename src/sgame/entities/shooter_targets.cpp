// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// Projectile-emitting target entities.
#include "g_local.h"

namespace {

using target_use_fn = void (*)(gentity_t *self, gentity_t *other, gentity_t *activator);

void SetupTargetShooter(gentity_t *self, target_use_fn use, const char *sound, int default_damage, float default_speed) {
	self->use = use;
	G_SetMovedir(self->s.angles, self->movedir);
	self->noise_index = gi.soundindex(sound);

	if (!self->dmg)
		self->dmg = default_damage;
	if (!self->speed)
		self->speed = default_speed;

	self->svflags = SVF_NOCLIENT;
}

void PlayTargetShooterSound(gentity_t *self) {
	gi.sound(self, CHAN_VOICE, self->noise_index, 1, ATTN_NORM, 0);
}

/*QUAKED target_shooter_grenade (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Fires a grenade in the set direction when triggered.

dmg		default is 120
speed	default is 600
*/
static USE(use_target_shooter_grenade) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	fire_grenade(self, self->s.origin, self->movedir, self->dmg, static_cast<int>(self->speed), 2.5_sec, self->dmg, crandom_open() * 10.0f, 200 + crandom_open() * 10.0f, true);
	PlayTargetShooterSound(self);
}

/*QUAKED target_shooter_rocket (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Fires a rocket in the set direction when triggered.

dmg		default is 120
speed	default is 600
*/
static USE(use_target_shooter_rocket) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	fire_rocket(self, self->s.origin, self->movedir, self->dmg, static_cast<int>(self->speed), self->dmg, self->dmg);
	PlayTargetShooterSound(self);
}

/*QUAKED target_shooter_bfg (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Fires a BFG projectile in the set direction when triggered.

dmg			default is 200 in DM, 500 in campaigns
speed		default is 400
*/
static USE(use_target_shooter_bfg) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	fire_bfg(self, self->s.origin, self->movedir, self->dmg, static_cast<int>(self->speed), 1000);
	PlayTargetShooterSound(self);
}

/*QUAKED target_shooter_prox (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Fires a prox mine in the set direction when triggered.

dmg			default is 90
speed		default is 600
*/
static USE(use_target_shooter_prox) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	fire_prox(self, self->s.origin, self->movedir, self->dmg, static_cast<int>(self->speed));
	PlayTargetShooterSound(self);
}

/*QUAKED target_shooter_ionripper (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Fires an ionripper projectile in the set direction when triggered.

dmg			default is 20 in DM and 50 in campaigns
speed		default is 800
*/
static USE(use_target_shooter_ionripper) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	fire_ionripper(self, self->s.origin, self->movedir, self->dmg, static_cast<int>(self->speed), EF_IONRIPPER);
	PlayTargetShooterSound(self);
}

/*QUAKED target_shooter_phalanx (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Fires a phalanx projectile in the set direction when triggered.

dmg			default is 80
speed		default is 725
*/
static USE(use_target_shooter_phalanx) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	fire_phalanx(self, self->s.origin, self->movedir, self->dmg, static_cast<int>(self->speed), 120, 30);
	PlayTargetShooterSound(self);
}

/*QUAKED target_shooter_flechette (1 0 0) (-8 -8 -8) (8 8 8) x x x x x x x x NOT_EASY NOT_MEDIUM NOT_HARD NOT_DM NOT_COOP
Fires a flechette in the set direction when triggered.

dmg			default is 10
speed		default is 1150
*/
static USE(use_target_shooter_flechette) (gentity_t *self, gentity_t *other, gentity_t *activator) -> void {
	fire_flechette(self, self->s.origin, self->movedir, self->dmg, static_cast<int>(self->speed), 0);
	PlayTargetShooterSound(self);
}

} // namespace

void SP_target_shooter_grenade(gentity_t *self) {
	SetupTargetShooter(self, use_target_shooter_grenade, "weapons/grenlf1a.wav", 120, 600);
}

void SP_target_shooter_rocket(gentity_t *self) {
	SetupTargetShooter(self, use_target_shooter_rocket, "weapons/rocklf1a.wav", 120, 600);
}

void SP_target_shooter_bfg(gentity_t *self) {
	SetupTargetShooter(self, use_target_shooter_bfg, "makron/bfg_fire.wav", deathmatch->integer ? 200 : 500, 400);
}

void SP_target_shooter_prox(gentity_t *self) {
	SetupTargetShooter(self, use_target_shooter_prox, "weapons/proxlr1a.wav", 90, 600);
}

void SP_target_shooter_ionripper(gentity_t *self) {
	SetupTargetShooter(self, use_target_shooter_ionripper, "weapons/rippfire.wav", deathmatch->integer ? 20 : 50, 800);
}

void SP_target_shooter_phalanx(gentity_t *self) {
	SetupTargetShooter(self, use_target_shooter_phalanx, "weapons/plasshot.wav", 80, 725);
}

void SP_target_shooter_flechette(gentity_t *self) {
	SetupTargetShooter(self, use_target_shooter_flechette, "weapons/nail1.wav", 10, 1150);
}
