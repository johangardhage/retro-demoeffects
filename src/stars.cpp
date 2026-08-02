//
// stars.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retrocolor.h"

#define NUM_STARS 1000
#define SPEED 60
#define ZMIN 1
#define ZMAX 3

Point3Df Stars[NUM_STARS];

void DEMO_Render(double deltatime)
{
	for (int i = 0; i < NUM_STARS; i++) {
		Stars[i].x += SPEED * Stars[i].z * deltatime;
		while (Stars[i].x >= RETRO_WIDTH) {
			Stars[i].x -= RETRO_WIDTH;
		}

		int x = Stars[i].x;
		int y = Stars[i].y;
		int z = Stars[i].z;

		if (x >= 0 && x < RETRO_WIDTH && y >= 0 && y < RETRO_HEIGHT) {
			int color = z * (63.0 / ZMAX);
			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, 64, RETRO_BLACK, RETRO_WHITE);

	for (int i = 0; i < NUM_STARS; i++) {
		Stars[i].x = RANDOM(RETRO_WIDTH);
		Stars[i].y = RANDOM(RETRO_HEIGHT);
		Stars[i].z = RANDOM(ZMAX) + ZMIN;
	}
}
