//
// Plasma
//
// A product of two independent sums of cosines, one in x and one in y.
// The table is one turn in degrees, CosTable[i] = cos(i π / 180), and
// indices WRAP360. t lives in [0, 720) so the integer t/2 term still
// covers a full cosine period.
//
//   X(x, t) = 75 + cos(2x + t/2) + cos(x + 2t) + 2 cos(x/2 + t)
//   Y(y, t) = 75 + 2 cos(y + 2t) + cos(2y + t/2) + 2 cos(y + t)
//   color   = (X Y) mod 252
//
// Each term is a travelling wave; the product beats them into the
// classic plasma blobs. The 252-entry palette is one RGB cycle
// (black-red-yellow-white-cyan-blue-black), so the wrap is a
// continuous color cycle.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define PLASMA_FRAMES 720

float CosTable[RETRO_DEGREES_PER_TURN];

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * 100, PLASMA_FRAMES);
	int iphase = (int)phase;

	// Generate plasma
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		float yc = 75 + CosTable[WRAP360(y + iphase * 2)] * 2 + CosTable[WRAP360(y * 2 + iphase / 2)] + CosTable[WRAP360(y + iphase)] * 2;

		for (int x = 0; x < RETRO_WIDTH; x++) {
			float xc = 75 + CosTable[WRAP360(x * 2 + iphase / 2)] + CosTable[WRAP360(x + iphase * 2)] + CosTable[WRAP360(x / 2 + iphase)] * 2;

			// Wrap into the 252-entry palette cycle
			unsigned char color = WRAP((int)(xc * yc), 252);
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

	// Init tables
	for (int i = 0; i < RETRO_DEGREES_PER_TURN; i++) {
		CosTable[i] = cos(i * DEG2RAD);
	}
}
