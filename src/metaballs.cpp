//
// metaballs.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retrocolor.h"

#define NUM_BALLS 5
#define THRESHOLD 50

struct MetaBall {
	Point2Df pos;
	Point2Df vel;
	float radius;
} Balls[NUM_BALLS];

void DEMO_Render(double deltatime)
{
	// Draw balls
	for (int x = 0; x < RETRO_WIDTH; x++) {
		for (int y = 0; y < RETRO_HEIGHT; y++) {
			float sum = 0;
			// Calculate iso-surface
			for (int i = 0; i < NUM_BALLS; i++) {
				float a = x - Balls[i].pos.x;
				float b = y - Balls[i].pos.y;
				float d = MAX(a * a + b * b, 0.0001f); // squared pixel distance from metaball position
				sum += THRESHOLD * Balls[i].radius * Balls[i].radius / d;
			}
			// Threshold
			if (sum >= THRESHOLD) {
				RETRO_PutPixel(x, y, 255);
			}
		}
	}

	// Move balls
	for (int i = 0; i < NUM_BALLS; i++) {
		Balls[i].pos.x += Balls[i].vel.x * deltatime;
		Balls[i].pos.y += Balls[i].vel.y * deltatime;
		while (Balls[i].pos.x < 0 || Balls[i].pos.x > RETRO_WIDTH - 1) {
			if (Balls[i].pos.x < 0) Balls[i].pos.x = -Balls[i].pos.x;
			else Balls[i].pos.x = 2 * (RETRO_WIDTH - 1) - Balls[i].pos.x;
			Balls[i].vel.x = -Balls[i].vel.x;
		}
		while (Balls[i].pos.y < 0 || Balls[i].pos.y > RETRO_HEIGHT - 1) {
			if (Balls[i].pos.y < 0) Balls[i].pos.y = -Balls[i].pos.y;
			else Balls[i].pos.y = 2 * (RETRO_HEIGHT - 1) - Balls[i].pos.y;
			Balls[i].vel.y = -Balls[i].vel.y;
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetColor(255, RETRO_WHITE);

	// Init balls
	for (int i = 0; i < NUM_BALLS; i++) {
		Balls[i].pos.x = RANDOM(RETRO_WIDTH);
		Balls[i].pos.y = RANDOM(RETRO_HEIGHT);
		Balls[i].vel.x = RANDOMF(240) - 120;
		Balls[i].vel.y = RANDOMF(240) - 120;
		Balls[i].radius = RANDOM(10) + 10;
	}
}
