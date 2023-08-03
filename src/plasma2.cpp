//
// plasma2.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

unsigned int Plasma[RETRO_HEIGHT][RETRO_WIDTH];

void DEMO_Render(double deltatime)
{
	// Calculate frame
	static double frame = 0;
	frame += deltatime * 200;

	// Draw every pixel again with the shifted palette color
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int color = (Plasma[y][x] + (int)frame) % RETRO_COLORS;
			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	for (int i = 0; i < RETRO_COLORS; i++) {
		int r = 128.0 + 128 * sin((float)M_PI * i / 32.0);
		int g = 128.0 + 128 * sin((float)M_PI * i / 64.0);
		int b = 128.0 + 128 * sin((float)M_PI * i / 128.0);
		RETRO_SetColor(i, r, g, b);
	}

	// Generate the plasma once
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int color = (
				128.0 + (128.0 * sin(x / 32.0))
				+ 128.0 + (128.0 * sin(y / 16.0))
				+ 128.0 + (128.0 * sin((x + y) / 32.0))
				+ 128.0 + (128.0 * sin(sqrt((double)(x * x + y * y)) / 16.0))
			) / 4;
			Plasma[y][x] = color;
		}
	}
}
