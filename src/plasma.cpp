//
// Plasma
//
// A product of two independent sums of cosines, one in x and one in y,
// with angles in degrees. t is the unrounded time times 100, so the flow
// is smooth; it lives on [0, 720) so the t/2 term covers a full period.
//
//   X(x, t) = 75 + cos(2x + t/2) + cos(x + 2t) + 2 cos(x/2 + t)
//   Y(y, t) = 75 + 2 cos(y + 2t) + cos(2y + t/2) + 2 cos(y + t)
//   color   = (X Y) mod 252
//
// Each term is a traveling wave; the product beats them into the
// classic plasma blobs. The 252-entry palette is one RGB cycle
// (black-red-yellow-white-cyan-blue-black), so the wrap is a
// continuous color cycle. X depends only on x and Y only on y, so each is
// worked out once per column and row rather than once per pixel.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

constexpr int PLASMA_FRAMES = 720;

void DEMO_Render(RETRO_Time time)
{
	// Calculate phase
	double phase = fmod(time.total * 100, PLASMA_FRAMES);

	float columns[RETRO_WIDTH];
	for (int x = 0; x < RETRO_WIDTH; x++) {
		columns[x] = 75 + cos(radians(x * 2 + phase / 2)) + cos(radians(x + phase * 2)) + cos(radians(x / 2.0 + phase)) * 2;
	}

	// Generate plasma
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		float yc = 75 + cos(radians(y + phase * 2)) * 2 + cos(radians(y * 2 + phase / 2)) + cos(radians(y + phase)) * 2;

		for (int x = 0; x < RETRO_WIDTH; x++) {
			// Wrap into the 252-entry palette cycle
			unsigned char color = WRAP((int)(columns[x] * yc), 252);
			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. The 252 cycling colors ramp one channel at a time, from
	// black through red, yellow, white, cyan and blue, and back to black
	RETRO_CreateGradientPalette(0, 42, RETRO_BLACK, RETRO_RED);
	RETRO_CreateGradientPalette(42, 84, RETRO_RED, RETRO_YELLOW);
	RETRO_CreateGradientPalette(84, 126, RETRO_YELLOW, RETRO_WHITE);
	RETRO_CreateGradientPalette(126, 168, RETRO_WHITE, RETRO_CYAN);
	RETRO_CreateGradientPalette(168, 210, RETRO_CYAN, RETRO_BLUE);
	RETRO_CreateGradientPalette(210, 252, RETRO_BLUE, RETRO_BLACK);
}
