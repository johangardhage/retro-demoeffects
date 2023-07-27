//
// tottunnel.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define NUM_RINGS 40
#define RING_STEP 6
#define RING_RADIUS 1000

double XCoords[360 / RING_STEP];
double YCoords[360 / RING_STEP];

void DEMO_Render(double deltatime)
{
	// Calculate movement
	static double move = 0;
	static double click = 0;
	double depth = 20;
	double dadd = depth / NUM_RINGS;
	double add = click;

	depth += move;
	move -= deltatime * 20;
	click -= deltatime * 20;

	if (move <= 0) {
		move += (dadd * 2);
		click -= (dadd * 59);
	}

	// Draw tunnel
	for (int i = 0; i < NUM_RINGS; i++) {
		double xsine = 260 * sin((add + i * 15) * RAD2DEG);
		double ysine = 260 * cos((add + i * 15) * RAD2DEG);

		for (int j = 0; j < 360 / RING_STEP; j++ ) {
			int x = (XCoords[j] + xsine) / (depth - i * dadd) + RETRO_WIDTH / 2;
			int y = (YCoords[j] + ysine) / (depth - i * dadd) + RETRO_HEIGHT / 2;

			if (x > 0 && x < RETRO_WIDTH && y > 0 && y < RETRO_HEIGHT) {
				RETRO_PutPixel(x, y, (i + 1) * 7);
			}
		}
	}
}

void DEMO_Initialize()
{
	// Init palette
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, i, i, i);
	}

	// Init rings
	double yadd0 = 75;
	double yadd1 = RING_RADIUS + 300 * sin((yadd0 * 4) * RAD2DEG);
	double yadd2 = RING_RADIUS + 300 * cos((yadd0 * 3) * RAD2DEG);
	for (int i = 0; i < 360 / RING_STEP; i++) {
		XCoords[i] = sin((i * RING_STEP + yadd0 * 2) * RAD2DEG) * yadd1;
		YCoords[i] = cos((i * RING_STEP + yadd0 * 2) * RAD2DEG) * yadd2;
	}
}
