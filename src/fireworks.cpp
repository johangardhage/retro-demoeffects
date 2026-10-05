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
// favors the corners and the burst would be a four-pointed star. Each
// spark then falls under the same g, and its color walks down the heat
// ramp as life runs out. One ramp, so the trail blur stays a fire color
// rather than averaging across unrelated hues.
//
// The framebuffer is never cleared. A 4-neighbor diffuse blur (no
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

static struct FireParticle {
	vec2 pos;
	vec2 vel;
	int life;
	unsigned char kind;
} Particles[NUM_PARTICLES];

static FireParticle *AllocParticle(void)
{
	for (FireParticle &particle : Particles) {
		if (particle.kind == DEAD) {
			return &particle;
		}
	}
	return NULL;
}

static void LaunchRocket(void)
{
	FireParticle *particle = AllocParticle();
	if (!particle) {
		return;
	}

	particle->kind = ROCKET;
	particle->pos = { (float)mix(40, RETRO_WIDTH - 40, RAND()), RETRO_HEIGHT - 1.0f };
	particle->vel.x = mix(-0.6, 0.6, RAND());
	float climb = RETRO_HEIGHT - 1 - mix(BURST_HIGH, BURST_LOW, RANDOMF(1));
	particle->vel.y = -sqrtf(2 * GRAVITY * climb);
	particle->life = 0;
}

static void Explode(vec2 pos)
{
	for (int n = 0; n < SPARKS; n++) {
		FireParticle *particle = AllocParticle();
		if (!particle) {
			return;
		}

		float angle = RANDOMF(2 * M_PI);
		float speed = RANDOMF(SPARK_SPEED);

		particle->kind = SPARK;
		particle->pos = pos;
		particle->vel = { (float)(speed * cos(angle)), (float)(speed * sin(angle)) };
		particle->life = SPARK_LIFE - RANDOM(20);
	}
}

void DEMO_FixedUpdate(RETRO_Time time)
{
	static int step = 0;

	if (step % ROCKET_STEPS == 0) {
		LaunchRocket();
	}

	for (FireParticle &particle : Particles) {
		if (particle.kind == DEAD) {
			continue;
		}

		particle.vel.y += GRAVITY;
		particle.pos += particle.vel;

		if (particle.kind == ROCKET) {
			if (particle.vel.y >= 0) {
				Explode(particle.pos);
				particle.kind = DEAD;
				continue;
			}
			if (particle.pos.x >= 0 && particle.pos.x < RETRO_WIDTH &&
				particle.pos.y >= 0 && particle.pos.y < RETRO_HEIGHT) {
				RETRO_PutPixel(particle.pos.x, particle.pos.y, RETRO_COLORS - 1);
			}
		} else {
			particle.life--;
			if (particle.life <= 0 ||
				particle.pos.x < 0 || particle.pos.x >= RETRO_WIDTH ||
				particle.pos.y < 0 || particle.pos.y >= RETRO_HEIGHT) {
				particle.kind = DEAD;
				continue;
			}
			int shade = 40 + particle.life * 215 / SPARK_LIFE;
			int ix = (int)particle.pos.x;
			int iy = (int)particle.pos.y;
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

	LaunchRocket();
	LaunchRocket();
}
