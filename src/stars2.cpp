//
// stars.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

#define NUM_STARS 1000
#define SPEED 2
#define ZMIN (-250)
#define ZMAX 250

Point3Df Stars[NUM_STARS];

void DEMO_Render(double deltatime)
{
	for (int i = 1; i < NUM_STARS; i++) {
		if (Stars[i].z < ZMIN) {
			Stars[i].z = RANDOM(ZMAX);
		} else {
			Stars[i].z -= SPEED;
		}

		int x = (RETRO_WIDTH / 2) + (Stars[i].x * 250.0) / (Stars[i].z + 250.0);
		int y = (RETRO_HEIGHT / 2) + (Stars[i].y * 250.0) / (Stars[i].z + 250.0);
		int z = -Stars[i].z;

		if (x >= 0 && x < RETRO_WIDTH && y >= 0 && y < RETRO_HEIGHT) {
			int color = (z + abs(ZMIN)) * (64.0 / (abs(ZMIN) + ZMAX));
			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	for (int i = 0; i < 64; i++) {
		RETRO_SetColor(i, i * 4, i * 4, i * 4);
	}

	for (int i = 1; i < NUM_STARS; i++) {
		Stars[i].x = RANDOM(RETRO_WIDTH) - (RETRO_WIDTH / 2);
		Stars[i].y = RANDOM(RETRO_HEIGHT) - (RETRO_HEIGHT / 2);
		Stars[i].z = RANDOM(ZMAX);
	}
}
