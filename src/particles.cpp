//
// particles.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retrocolor.h"

#define NUM_PARTICLES 6000
#define PARTICLE_GRAVITY 0.13

struct Particle {
	float x, y, xdir, ydir;
	int col;
} Particles[NUM_PARTICLES];

void CreateExplosion(void)
{
	int x = RANDOM(RETRO_WIDTH);
	int y = RANDOM(RETRO_HEIGHT);

	for (int i = 0; i < NUM_PARTICLES; i++) {
		Particles[i].x = x;
		Particles[i].y = y;
		Particles[i].xdir = RANDOMF(5) - 2.5;
		Particles[i].ydir = RANDOMF(5) - 2.5;
		float dist = RANDOMF(5);

		float len = sqrt(Particles[i].xdir * Particles[i].xdir + Particles[i].ydir * Particles[i].ydir);
		len = len == 0.0 ? 0.0 : 1.0 / len;

		Particles[i].xdir *= len * dist;
		Particles[i].ydir *= len * dist;
		Particles[i].col = 255;
	}
}

//
// Advance the particles in fixed steps. Velocities are integrated once per step with
// PARTICLE_GRAVITY measured in pixels per step, and the explosion timer counts steps,
// so both are tied to the step rate.
//
void DEMO_Update(double deltatime)
{
	static int tick = 0;

	if (tick % 90 == 0) CreateExplosion();
	for (int i = 0; i < NUM_PARTICLES; i++) {
		RETRO_PutPixel(Particles[i].x, Particles[i].y, Particles[i].col);

		Particles[i].x += Particles[i].xdir;
		Particles[i].y += Particles[i].ydir;

		if (Particles[i].y > RETRO_HEIGHT-1) {
			Particles[i].y = RETRO_HEIGHT-1;
			Particles[i].xdir /= 4;
			Particles[i].ydir = -Particles[i].ydir / 2;
		} else if (Particles[i].y < 1) {
			Particles[i].y = 1;
			Particles[i].xdir /= 4;
			Particles[i].ydir = -Particles[i].ydir / 2;
		} else {
			Particles[i].ydir += PARTICLE_GRAVITY;
		}

		if (Particles[i].x < 0) {
			Particles[i].x = 1;
			Particles[i].xdir = -Particles[i].xdir / 2;
			Particles[i].ydir /= 4;
		} else if (Particles[i].x > RETRO_WIDTH-1) {
			Particles[i].x = RETRO_WIDTH-1;
			Particles[i].xdir = -Particles[i].xdir / 2;
			Particles[i].ydir /= 4;
		}

		if (Particles[i].y >= 219) {
			Particles[i].col = RANDOM(128) + 128;
		}
	}

	RETRO_Blur(RETRO_BLUR_DIFFUSE, 3);
	tick++;
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, 64, RETRO_BLACK, RETRO_RED);
	RETRO_CreateGradientPalette(64, 128, RETRO_RED, RETRO_YELLOW);
	RETRO_CreateGradientPalette(128, RETRO_COLORS, RETRO_YELLOW, RETRO_WHITE);
}
