//
// particles.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

#define NUM_PARTICLES 6000
#define PARTICLE_GRAVITY 0.13

struct Particle {
	float x, y, xdir, ydir;
	int col;
} Particles[NUM_PARTICLES];

void CreateExplosion()
{
	int x = RANDOM(RETRO_WIDTH);
	int y = RANDOM(RETRO_HEIGHT);

	for (int i = 0; i < NUM_PARTICLES; i++) {
		Particles[i].x = x;
		Particles[i].y = y;
		Particles[i].xdir = RANDOM(5) - 1.5;
		Particles[i].ydir = RANDOM(5) - 1.5;
		float dist = RANDOM(5);

		float len = sqrt(Particles[i].xdir * Particles[i].xdir + Particles[i].ydir * Particles[i].ydir);
		len = len == 0.0 ? 0.0 : 1.0 / len;

		Particles[i].xdir *= len * dist;
		Particles[i].ydir *= len * dist;
		Particles[i].col = 255;
	}
}

void DrawParticles()
{
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
			Particles[i].col = (rand() & 127) + 128;
		}
	}
}

void DEMO_Render2(double deltatime)
{
	// Calculate frame
	static double frame_counter = 0;
	frame_counter += deltatime * 10;
	int frame = frame_counter;

	if (frame % 15 == 0) {
		CreateExplosion();
	}

	DrawParticles();

	RETRO_Blur(RETRO_BLUR_4, 3);
	RETRO_Flip();
}

void DEMO_Initialize()
{
	// Init palette
	for (int i = 0; i < 64; i++) {
		RETRO_SetColor(i, i * 4, 0, 0);
	}
	for (int i = 64; i < 128; i++) {
		RETRO_SetColor(i, 63 * 4, (i - 64) * 4, 0);
	}
	for (int i = 128; i < 256; i++) {
		RETRO_SetColor(i, 63 * 4, 63 * 4, ((i - 128) >> 1) * 4);
	}
	RETRO_SetPalette();
}
