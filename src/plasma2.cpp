//
// Plasma 2
//
// A static field of four sines, then a cycling palette. The value stored
// at (x, y) is the mean of four [0, 256] waves
//
//   P = 128 + 32 (sin(x/32) + sin(y/16) + sin((x+y)/32) + sin(r/16))
//
// with r = sqrt(x² + y²). The draw adds t to P and wraps the 256-entry
// palette, so the picture never changes, only its colors do. t lives
// on 256.
// The palette itself is three sines of periods 64, 128 and 256
// (sin(π i / T) has period 2T), a slow RGB beat rather than a single
// hue ramp.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

int Plasma[RETRO_HEIGHT][RETRO_WIDTH];

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * 200, RETRO_COLORS);

	// Draw every pixel again with the shifted palette color
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int color = WRAP(Plasma[y][x] + (int)phase, RETRO_COLORS);
			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	for (int i = 0; i < RETRO_COLORS; i++) {
		int r = 127.5 + 127.5 * sin((float)M_PI * i / 32.0);
		int g = 127.5 + 127.5 * sin((float)M_PI * i / 64.0);
		int b = 127.5 + 127.5 * sin((float)M_PI * i / 128.0);
		RETRO_SetColor(i, r, g, b);
	}

	// Init plasma field
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
