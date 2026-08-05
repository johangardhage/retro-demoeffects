//
// Distort, from a precalculated field
//
// An image resampled through a pair of displacement tables. Pixel (x, y) is copied from
//
//   I(x + Sx(wx + x, wy + y),  y + Sy(vx + x, vy + y))
//
// The tables never change. What moves is the screen-sized window (w, v) each is
// read through. A window's two coordinates run at different rates, so it walks a
// Lissajous figure over the whole margin the table has to spare: the tables are
// 2W by 2H, and each corner of the window stays inside them.
// The four rates 197, 224, 205, 231 share only a huge lcm (2^5·3·5·7·11·41·197),
// so the phases drift; t lives on that lcm times 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define DISTORT_WIDTH (RETRO_WIDTH * 2)
#define DISTORT_HEIGHT (RETRO_HEIGHT * 2)
#define DISTORT_SPEED 400 // phase units per second, about a third of a turn a second
// 197, 224=2^5·7, 205=5·41, 231=3·7·11 → lcm = 2^5·3·5·7·11·41·197
#define DISTORT_PERIOD (32.0 * 3 * 5 * 7 * 11 * 41 * 197 * 2 * M_PI)

// Displacements are signed. Plain char is unsigned on some targets, which would
// turn every negative shift into a large positive one.
signed char ShiftX[DISTORT_WIDTH * DISTORT_HEIGHT];
signed char ShiftY[DISTORT_WIDTH * DISTORT_HEIGHT];

void DEMO_Render(double deltatime)
{
	unsigned char *image = RETRO_ImageData();

	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * DISTORT_SPEED, DISTORT_PERIOD);

	// Calculate windows. Each spans [0, W] by [0, H], so the last row and column
	// read is exactly the last one the table holds.
	int xtableleft = (RETRO_WIDTH / 2) + (RETRO_WIDTH / 2 * sin(-phase / 197));
	int xtabletop = (RETRO_HEIGHT / 2) + (RETRO_HEIGHT / 2 * cos(-phase / 224));
	int ytableleft = (RETRO_WIDTH / 2) + (RETRO_WIDTH / 2 * cos(phase / 205));
	int ytabletop = (RETRO_HEIGHT / 2) + (RETRO_HEIGHT / 2 * sin(phase / 231));

	// Draw distort
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int xoffset = (xtabletop + y) * DISTORT_WIDTH + (xtableleft + x);
			int yoffset = (ytabletop + y) * DISTORT_WIDTH + (ytableleft + x);

			int sourcex = x + ShiftX[xoffset];
			int sourcey = y + ShiftY[yoffset];

			if (sourcex >= 0 && sourcex < RETRO_WIDTH && sourcey >= 0 && sourcey < RETRO_HEIGHT) {
				RETRO_PutPixel(x, y, image[sourcey * RETRO_WIDTH + sourcex]);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_Image *image = RETRO_LoadImage("assets/flag_320x240.pcx");
	if (image->width != RETRO_WIDTH || image->height != RETRO_HEIGHT) {
		RETRO_RageQuit("The image must be the size of the screen\n");
	}
	RETRO_SetPalette(RETRO_ImagePalette());

	// Init tables. Each table is a sum of six unit-amplitude sinusoids: four plane waves,
	// one hyperbolic term in x*y, and one ripple about a point far off the
	// table. The periods share no common factor, so the terms never lock into
	// a grid. The six terms span [-6, 6], rounded to whole pixels.
	for (int y = 0; y < DISTORT_HEIGHT; y++) {
		for (int x = 0; x < DISTORT_WIDTH; x++) {
			int offset = y * DISTORT_WIDTH + x;

			ShiftX[offset] = lround(sin(x / 20.0) + sin(x * y / 2000.0) + sin((x + y) / 100.0) + sin((y - x) / 70.0) + sin((x + 4 * y) / 70.0) + sin(hypot(256 - x, (150 - y / 8.0)) / 40.0));
			ShiftY[offset] = lround(cos(x / 31.0) + cos(x * y / 1783.0) + cos((x + y) / 137.0) + cos((y - x) / 55.0) + cos((x + 8 * y) / 57.0) + sin(hypot(384 - x, (274 - y / 9.0)) / 51.0));
		}
	}
}
