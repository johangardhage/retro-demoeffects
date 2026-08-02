//
// shadebobs.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retrocolor.h"

#define maxdegrees 256
#define divd 128

unsigned char Image[] = {
	 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
	 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
	 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
	 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0 };

void DrawShadebob(int x, int y, int imagewidth, int imageheight, unsigned char* image, unsigned char *buffer = RETRO.framebuffer)
{
	float xstart = x - imagewidth / 2;
	float ystart = y - imageheight / 2;

	for (int yy = 0; yy < imageheight; yy++) {
		for (int xx = 0; xx < imagewidth; xx++) {
			int xpos = xx + xstart;
			int ypos = yy + ystart;
			if (xpos >= 0 && xpos < RETRO_WIDTH && ypos >= 0 && ypos < RETRO_HEIGHT) {
				buffer[ypos * RETRO_WIDTH + xpos] += image[yy * imagewidth + xx];
			}
		}
	}
}

void DEMO_Render2(double deltatime)
{
	// Advance the bobs in fixed steps. DrawShadebob adds into a framebuffer that is never
	// cleared and wraps at 256 rather than saturating, so the colour banding is a function
	// of how many bobs have been drawn. At 60 Hz a pixel under the path cycles the palette
	// every 256 draws.
	while (RETRO_PerformSimulation()) {
		static float xadd1 = 60, xadd2 = 100, yadd1 = 55, yadd2 = 200;

		xadd1 += 200 * RETRO_SIMULATION_STEP;
		xadd2 += 300 * RETRO_SIMULATION_STEP;
		yadd1 += 300 * RETRO_SIMULATION_STEP;
		yadd2 += 200 * RETRO_SIMULATION_STEP;

		int x, y;

		x = sin(xadd1 * 2.0 * M_PI / maxdegrees) * divd / 2 + sin(xadd2 * 2.0 * M_PI / maxdegrees) * divd / 2;
		y = sin(yadd1 * 2.0 * M_PI / maxdegrees) * divd / 3 + sin(yadd2 * 2.0 * M_PI / maxdegrees) * divd / 3;
		DrawShadebob(160 + x, 120 + y, 16, 16, Image);

		x = sin((xadd1 + 2) * 2.0 * M_PI / maxdegrees) * divd / 2 + sin((xadd2 + 3) * 2.0 * M_PI / maxdegrees) * divd / 2;
		y = sin((yadd1 + 2) * 2.0 * M_PI / maxdegrees) * divd / 3 + sin((yadd2 + 3) * 2.0 * M_PI / maxdegrees) * divd / 3;
		DrawShadebob(160 + x, 120 + y, 16, 16, Image);

		x = sin(xadd1 * 2.0 * M_PI / maxdegrees) * divd / 2 + sin((xadd2 + 3) * 2.0 * M_PI / maxdegrees) * divd / 2;
		y = sin(yadd1 * 2.0 * M_PI / maxdegrees) * divd / 3 + sin((yadd2 + 2) * 2.0 * M_PI / maxdegrees) * divd / 3;
		DrawShadebob(160 + x, 120 + y, 16, 16, Image);

		x = sin((xadd1 + 2) * 2.0 * M_PI / maxdegrees) * divd / 2 + sin(xadd2 * 2.0 * M_PI / maxdegrees) * divd / 2;
		y = sin(yadd1 * 2.0 * M_PI / maxdegrees) * divd / 3 + sin((yadd2 + 2) * 2.0 * M_PI / maxdegrees) * divd / 3;
		DrawShadebob(160 + x, 120 + y, 16, 16, Image);
	}
	RETRO_Flip();
}

void DEMO_Initialize(void)
{
	// Init palette. The bobs heat up from red through yellow into white as
	// they pile on top of each other
	RETRO_CreateGradientPalette(0, 64, RETRO_BLACK, RETRO_RED);
	RETRO_CreateGradientPalette(64, 128, RETRO_RED, RETRO_YELLOW);
	RETRO_CreateGradientPalette(128, RETRO_COLORS, RETRO_YELLOW, RETRO_WHITE);
}
