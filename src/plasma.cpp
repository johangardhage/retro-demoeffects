//
// plasma.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocolor.h"

#define PLASMA_FRAMES 720
#define SINE_VALUES (RETRO_WIDTH + PLASMA_FRAMES * 2)

float CosTable[SINE_VALUES];

void DEMO_Render(double deltatime)
{
	// Calculate frame
	static double framecounter = 0;
	framecounter += deltatime * 100;
	int frame = WRAP(framecounter, PLASMA_FRAMES);

	// Generate plasma
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		float yc = 75 + CosTable[y + frame * 2] * 2 + CosTable[y * 2 + frame / 2] + CosTable[y + frame] * 2;

		for (int x = 0; x < RETRO_WIDTH; x++) {
			float xc = 75 + CosTable[x * 2 + frame / 2] + CosTable[x + frame * 2] + CosTable[x / 2 + frame] * 2;

			// Wrap into the 252-entry palette cycle
			unsigned char color = (int)(xc * yc) % 252;
			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. The 252 cycling colors ramp one channel at a time, from
	// black through red, yellow, white, cyan and blue, and back to black
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK, RETRO_BLACK);
	RETRO_CreateGradientPalette(0, 42, RETRO_BLACK, RETRO_RED);
	RETRO_CreateGradientPalette(42, 84, RETRO_RED, RETRO_YELLOW);
	RETRO_CreateGradientPalette(84, 126, RETRO_YELLOW, RETRO_WHITE);
	RETRO_CreateGradientPalette(126, 168, RETRO_WHITE, RETRO_CYAN);
	RETRO_CreateGradientPalette(168, 210, RETRO_CYAN, RETRO_BLUE);
	RETRO_CreateGradientPalette(210, 252, RETRO_BLUE, RETRO_BLACK);

	// Init sine table
	for (int i = 0; i < SINE_VALUES; i++) {
		CosTable[i] = cos(i * M_PI / 180);
	}
}
