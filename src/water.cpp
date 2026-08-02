//
// water.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define WATER_DAMP 0.985
#define WATER_REFLECTIONS 18.0
#define WATER_DEPTH 300.0

float Water[RETRO_WIDTH * RETRO_HEIGHT];
float Water2[RETRO_WIDTH * RETRO_HEIGHT];

void DEMO_Render(double deltatime)
{
	// Advance the water in fixed steps. This is a finite difference wave equation, so the
	// step is baked into the scheme - a ripple travels one cell per step and WATER_DAMP is
	// applied per step. The step rate is the wave speed, and the droplet timer counts steps.
	while (RETRO_PerformSimulation()) {
		static int tick = 0;
		if (tick % 36 == 0) {
			int x = RANDOM(RETRO_WIDTH);
			int y = RANDOM(RETRO_HEIGHT);

			if (x > 0 && x < RETRO_WIDTH - 1 && y > 1 && y < RETRO_HEIGHT - 1) {
				Water[y * RETRO_WIDTH + x] -= WATER_DEPTH;
			}
		}
		tick++;

		// Water physics & 1st buffer copy pass
		for (int y = 1; y < RETRO_HEIGHT - 1; y++) {
			for (int x = 1; x < RETRO_WIDTH - 1; x++) {
				int i = y * RETRO_WIDTH + x;

				Water2[i] = ((Water[i - 1] + Water[i + 1] + Water[i - RETRO_WIDTH] + Water[i + RETRO_WIDTH]) * .5f - Water2[i]) * WATER_DAMP;
			}
		}

		// 2nd buffer copy pass
		for (int y = 1; y < RETRO_HEIGHT - 1; y++) {
			for (int x = 1; x < RETRO_WIDTH - 1; x++) {
				int i = y * RETRO_WIDTH + x;
				float w = Water2[i];
				Water2[i] = Water[i];
				Water[i] = w;
			}
		}

		// Blur pass
		for (int y = 1; y < RETRO_HEIGHT - 1; y++) {
			for (int x = 1; x < RETRO_WIDTH - 1; x++) {
				int i = y * RETRO_WIDTH + x;
				Water[i] = (Water[i] + Water[i - 1] + Water[i + 1] + Water[i - RETRO_WIDTH] + Water[i + RETRO_WIDTH]) * .2f;
			}
		}
	}

	unsigned char *image = RETRO_ImageData();
	for (int y = 1; y < RETRO_HEIGHT - 1; y++) {
		for (int x = 1; x < RETRO_WIDTH - 1; x++) {
			int i = y * RETRO_WIDTH + x;
			float nx = Water[i + 1] - Water[i - 1];
			float ny = Water[i + RETRO_WIDTH] - Water[i - RETRO_WIDTH];
			int rx = CLAMP(x - (int)(nx * WATER_REFLECTIONS), 1, RETRO_WIDTH - 1);
			int ry = CLAMP(y - (int)(ny * WATER_REFLECTIONS), 1, RETRO_HEIGHT - 1);
			RETRO_PutPixel(x, y, image[ry * RETRO_WIDTH + rx]);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/monkey_320x240.pcx");
	RETRO_SetPalette(RETRO_ImagePalette());
	RETRO_Blit(RETRO_ImageData());
}
