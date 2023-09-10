//
// metaballs.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

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
				float d = sqrt(a * a + b * b); // calculate pixel distance from metaball position
				sum += 50 * Balls[i].radius / d;
			}
			// Threshold
			if (sum >= THRESHOLD) {
				RETRO_PutPixel(x, y, 255);
			}
		}
	}

	// Move balls
	for (int i = 0; i < NUM_BALLS; i++) {
		if (Balls[i].pos.x < 0 || Balls[i].pos.x > RETRO_WIDTH) {
			Balls[i].vel.x *= -1;
		}
		if (Balls[i].pos.y < 0 || Balls[i].pos.y > RETRO_HEIGHT) {
			Balls[i].vel.y *= -1;
		}
		Balls[i].pos.x += Balls[i].vel.x;
		Balls[i].pos.y += Balls[i].vel.y;
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetColor(255, 255, 255, 255);

	// Init balls
	for (int i = 0; i < NUM_BALLS; i++) {
		Balls[i].pos.x = rand() % RETRO_WIDTH;
		Balls[i].pos.y = rand() % RETRO_HEIGHT;
		Balls[i].vel.x = (float)rand() / RAND_MAX * 2;
		Balls[i].vel.y = (float)rand() / RAND_MAX * 2;
		Balls[i].radius = rand() % 20 + 10;
	}
}
