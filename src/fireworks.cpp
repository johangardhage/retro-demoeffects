//
// Fireworks
//
// Rockets and the sparks they throw. y grows down, so a rocket's climb
// is a negative vy and gravity is +g, the same sign as particles.cpp.
// A rocket is symplectic Euler with no exceptions
//
//   v' = v + (0, g)
//   x' = x + v'
//
// A rocket bursts at apogee and nowhere else, so its launch speed is
// picked from where it should burst rather than the other way round:
// climbing h costs sqrt(2gh), and h is drawn between BURST_LOW and
// BURST_HIGH so the bursts spread over the middle of the screen
// instead of all piling up against the top edge.
//
// At apogee (vy ≥ 0) it is replaced by SPARKS sparks whose directions
// are uniform in angle and speed, not in a square — a square sample
// favours the corners and the burst would be a four-pointed star. Each
// spark then falls under the same g, and its colour walks down the heat
// ramp as life runs out. One ramp, so the trail blur stays a fire colour
// rather than averaging across unrelated hues.
//
// The framebuffer is never cleared. A 4-neighbour diffuse blur (no
// self) subtracts TRAIL_DECAY each step, which is the trail.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retropalette.h"
#include "lib/retrovector.h"

#define NUM_PARTICLES 1800
#define SPARKS 140 // sparks a rocket becomes
#define ROCKET_STEPS 28 // steps between launches
#define BURST_LOW (RETRO_HEIGHT * 0.60f) // y of the lowest burst
#define BURST_HIGH (RETRO_HEIGHT * 0.12f) // y of the highest burst
#define SPARK_SPEED 4.6f // pixels a step, at the fastest spark
#define GRAVITY 0.065f // pixels a step added to the downward speed each step
#define SPARK_LIFE 110 // steps a spark lives
#define TRAIL_DECAY 2 // brightness the blur takes off each step

enum { DEAD, ROCKET, SPARK };

struct FireParticle {
	vec2 pos;
	vec2 vel;
	int life;
	unsigned char kind;
} Particles[NUM_PARTICLES];

static int AllocParticle(void)
{
	for (int i = 0; i < NUM_PARTICLES; i++) {
		if (Particles[i].kind == DEAD) {
			return i;
		}
	}
	return -1;
}

static void LaunchRocket(void)
{
	int i = AllocParticle();
	if (i < 0) {
		return;
	}

	Particles[i].kind = ROCKET;
	Particles[i].pos = { 40 + RANDOMF(RETRO_WIDTH - 80), RETRO_HEIGHT - 1.0f };
	Particles[i].vel.x = RANDOMF(1.2f) - 0.6f;
	float climb = RETRO_HEIGHT - 1 - (BURST_HIGH + RANDOMF(BURST_LOW - BURST_HIGH));
	Particles[i].vel.y = -sqrtf(2 * GRAVITY * climb);
	Particles[i].life = 0;
}

static void Explode(vec2 pos)
{
	for (int n = 0; n < SPARKS; n++) {
		int i = AllocParticle();
		if (i < 0) {
			return;
		}

		float angle = RANDOMF(2 * M_PI);
		float speed = RANDOMF(SPARK_SPEED);

		Particles[i].kind = SPARK;
		Particles[i].pos = pos;
		Particles[i].vel = { (float)(speed * cos(angle)), (float)(speed * sin(angle)) };
		Particles[i].life = SPARK_LIFE - RANDOM(20);
	}
}

void DEMO_FixedUpdate(double timestep)
{
	static int step = 0;

	if (step % ROCKET_STEPS == 0) {
		LaunchRocket();
	}

	for (int i = 0; i < NUM_PARTICLES; i++) {
		if (Particles[i].kind == DEAD) {
			continue;
		}

		Particles[i].vel.y += GRAVITY;
		Particles[i].pos.x += Particles[i].vel.x;
		Particles[i].pos.y += Particles[i].vel.y;

		if (Particles[i].kind == ROCKET) {
			if (Particles[i].vel.y >= 0) {
				Explode(Particles[i].pos);
				Particles[i].kind = DEAD;
				continue;
			}
			if (Particles[i].pos.x >= 0 && Particles[i].pos.x < RETRO_WIDTH &&
				Particles[i].pos.y >= 0 && Particles[i].pos.y < RETRO_HEIGHT) {
				RETRO_PutPixel(Particles[i].pos.x, Particles[i].pos.y, RETRO_COLORS - 1);
			}
		} else {
			Particles[i].life--;
			if (Particles[i].life <= 0 ||
				Particles[i].pos.x < 0 || Particles[i].pos.x >= RETRO_WIDTH ||
				Particles[i].pos.y < 0 || Particles[i].pos.y >= RETRO_HEIGHT) {
				Particles[i].kind = DEAD;
				continue;
			}
			int shade = 40 + Particles[i].life * 215 / SPARK_LIFE;
			int ix = (int)Particles[i].pos.x;
			int iy = (int)Particles[i].pos.y;
			unsigned char color = CLAMP256(shade);
			RETRO_PutPixel(ix, iy, color);
			if (ix + 1 < RETRO_WIDTH) {
				RETRO_PutPixel(ix + 1, iy, color);
			}
			if (iy + 1 < RETRO_HEIGHT) {
				RETRO_PutPixel(ix, iy + 1, color);
			}
		}
	}

	RETRO_Blur(RETRO_BLUR_DIFFUSE, TRAIL_DECAY);

	step++;
}

void DEMO_Initialize(void)
{
	RETRO_CreateGradientPalette(0, 64, RETRO_BLACK, RETRO_RED);
	RETRO_CreateGradientPalette(64, 160, RETRO_RED, RETRO_YELLOW);
	RETRO_CreateGradientPalette(160, RETRO_COLORS, RETRO_YELLOW, RETRO_WHITE);

	for (int i = 0; i < NUM_PARTICLES; i++) {
		Particles[i].kind = DEAD;
	}

	LaunchRocket();
	LaunchRocket();
}
