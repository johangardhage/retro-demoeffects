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
	// Calculate frame
	static double frame_counter = 0;
	frame_counter += deltatime * 100;
	int frame = (int)frame_counter;

	unsigned char *image = RETRO_ImageData();

	// Make drop
	if (frame % 60 == 0) {
		int x = RANDOM(RETRO_WIDTH);
		int y = RANDOM(RETRO_HEIGHT);

		if (x > 0 && x < RETRO_WIDTH - 1 && y > 1 && y < RETRO_HEIGHT - 1) {
			Water[y * RETRO_WIDTH + x] -= WATER_DEPTH;
		}
	}

	// Water physics & 1st buffer copy pass
	for (int y = 1; y < RETRO_HEIGHT - 1; y++) {
		for (int x = 1; x < RETRO_WIDTH - 1; x++) {
			int i = y * RETRO_WIDTH + x;

			Water2[i] = ((Water[i - 1] + Water[i + 1] + Water[i - RETRO_WIDTH] + Water[i + RETRO_WIDTH]) * .5f - Water2[i]) * WATER_DAMP;
		}
	}

	// Refraction & render pass
	for (int y = 1; y < RETRO_HEIGHT - 1; y++) {
		for (int x = 1; x < RETRO_WIDTH - 1; x++) {
			int i = y * RETRO_WIDTH + x;

			float nx = Water2[i + 1] - Water2[i - 1];
			float ny = Water2[i + RETRO_WIDTH] - Water2[i - RETRO_WIDTH];

			int rx = x - (int) (nx * WATER_REFLECTIONS);
			int ry = y - (int) (ny * WATER_REFLECTIONS);
			rx = rx < 1 ? 1 : (rx > RETRO_WIDTH - 2 ? RETRO_WIDTH - 2 : rx);
			ry = ry < 1 ? 1 : (ry > RETRO_HEIGHT - 2 ? RETRO_HEIGHT - 2 : ry);

			int color = image[ry * RETRO_WIDTH + rx];
			RETRO_PutPixel(x, y, color);
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

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/monkey_320x240.pcx");
	RETRO_SetPalette(RETRO_ImagePalette());
	RETRO_Blit(RETRO_ImageData());
}
