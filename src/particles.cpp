//
// Particles
//
// Point masses under gravity, bouncing off the walls, floor and ceiling.
// y grows down, so g = (0, +PARTICLE_GRAVITY). Each step is symplectic
// (semi-implicit) Euler, with no exceptions:
//
//   v' = v + (0, g)
//   x' = x + v'
//
// A bounce reflects the overshoot about the wall, rather than clamping to
// it, and reverses the normal component, damping both:
//
//   x_n' = 2 wall − x_n
//   v_n' = −BOUNCE_RESTITUTION · v_n
//   v_t' =  BOUNCE_FRICTION    · v_t
//
// Below a bounce that cannot clear one step of gravity the normal velocity is
// zeroed and the particle placed on the wall, so it settles rather than buzz.
//
// The explosion is uniform in angle and speed, not in a square (a square
// sample favors the corners). The framebuffer is never cleared; a
// 4-neighbor diffuse blur (no self) subtracts TRAIL_DECAY each step.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retropalette.h"
#include "lib/retrovector.h"

#define NUM_PARTICLES 6000
#define PARTICLE_SPEED 5 // pixels a step, at the fastest
#define PARTICLE_GRAVITY 0.13 // pixels a step added to the downward speed each step
#define EXPLOSION_STEPS 90 // steps between explosions

#define BOUNCE_RESTITUTION 0.5 // speed kept across a bounce, in the direction that reverses
#define BOUNCE_FRICTION 0.25 // and along the wall

#define EMBER_ROWS 21 // rows above the floor in which a particle glows like an ember
#define EMBER_SHADES 128 // the top half of the ramp, where the yellows and whites are

#define TRAIL_DECAY 3 // brightness the blur takes off each step, so the trails fade

static struct Particle {
	vec2 pos;
	vec2 dir;
	unsigned char color;
} Particles[NUM_PARTICLES];

//
// Throw every particle out from one point
//
// Direction is an angle, not a point in a square. A square sample favors the
// corners (they are further out than the edges) and the explosion would be a
// four-pointed star:
//
//   v = s * (cos theta, sin theta),   theta ~ U[0, 2pi),   s ~ U[0, PARTICLE_SPEED)
//
static void CreateExplosion(void)
{
	int x = RANDOM(RETRO_WIDTH);
	int y = RANDOM(RETRO_HEIGHT);

	for (Particle &particle : Particles) {
		float angle = RANDOMF(2 * M_PI);
		float speed = RANDOMF(PARTICLE_SPEED);

		particle.pos = { (float)x, (float)y };
		particle.dir = { (float)(speed * cos(angle)), (float)(speed * sin(angle)) };
		particle.color = RETRO_COLORS - 1;
	}
}

void DEMO_FixedUpdate(RETRO_Time time)
{
	static int step = 0;

	// Seed explosion
	if (step % EXPLOSION_STEPS == 0) {
		CreateExplosion();
	}

	// Draw and move particles
	for (Particle &particle : Particles) {
		RETRO_PutPixel(particle.pos.x, particle.pos.y, particle.color);

		// Symplectic Euler: v' = v + g, then x' = x + v'
		particle.dir.y += PARTICLE_GRAVITY;

		particle.pos += particle.dir;

		// Floor and ceiling
		bool onfloor = false;
		while (particle.pos.y < 0 || particle.pos.y > RETRO_HEIGHT - 1) {
			if (particle.pos.y < 0) {
				particle.pos.y = -particle.pos.y;
			} else {
				particle.pos.y = 2 * (RETRO_HEIGHT - 1) - particle.pos.y;
				onfloor = true;
			}
			particle.dir.y *= -BOUNCE_RESTITUTION;
			particle.dir.x *= BOUNCE_FRICTION;
		}

		// Resting contact: a bounce this small cannot clear one step of gravity
		if (onfloor && fabsf(particle.dir.y) < 2 * PARTICLE_GRAVITY) {
			particle.dir.y = 0;
			particle.pos.y = RETRO_HEIGHT - 1;
		}

		// Side walls
		while (particle.pos.x < 0 || particle.pos.x > RETRO_WIDTH - 1) {
			if (particle.pos.x < 0) {
				particle.pos.x = -particle.pos.x;
			} else {
				particle.pos.x = 2 * (RETRO_WIDTH - 1) - particle.pos.x;
			}
			particle.dir.x *= -BOUNCE_RESTITUTION;
			particle.dir.y *= BOUNCE_FRICTION;
		}

		if (particle.pos.y >= RETRO_HEIGHT - EMBER_ROWS) {
			particle.color = RANDOM(EMBER_SHADES) + EMBER_SHADES;
		}
	}

	// Blur trail
	RETRO_Blur(RETRO_BLUR_DIFFUSE, TRAIL_DECAY);

	step = (step + 1) % EXPLOSION_STEPS;
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, 64, RETRO_BLACK, RETRO_RED);
	RETRO_CreateGradientPalette(64, 128, RETRO_RED, RETRO_YELLOW);
	RETRO_CreateGradientPalette(128, RETRO_COLORS, RETRO_YELLOW, RETRO_WHITE);
}
