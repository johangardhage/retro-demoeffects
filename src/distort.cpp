//
// Distort
//
// An image resampled through a pair of shears. Pixel (x, y) is copied from
//
//   I(x + A sin(2pi (y + t) / N),  y + (A/2) sin(2pi (x + t) / N))
//
// so a whole row shares its sideways shift and a whole column shares its
// vertical one. N is the wavelength in pixels and t grows smoothly with
// time, so both waves travel and the picture ripples. Each shift is
// rounded to whole pixels once. A sample that lands off the image is left
// as background, and the edges fray.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

constexpr int DISTORT_WAVELENGTH = 64; // pixels in one whole wave of the shear
constexpr int DISTORT_AMPLITUDE = 5; // pixels a row shifts sideways at the peak
constexpr double DISTORT_SPEED = 100; // pixels the wave travels per second

void DEMO_Render(RETRO_Time time)
{
	unsigned char *image = RETRO_ImageData();

	// Calculate phase
	double phase = fmod(time.total * DISTORT_SPEED * 2 * M_PI / DISTORT_WAVELENGTH, 2 * M_PI);
	double step = 2 * M_PI / DISTORT_WAVELENGTH;

	// A column's vertical shift depends only on x, so it is the same for every
	// row: worked out once per frame rather than once per pixel.
	int columnshift[RETRO_WIDTH];
	for (int x = 0; x < RETRO_WIDTH; x++) {
		columnshift[x] = lround(DISTORT_AMPLITUDE * sin(x * step + phase) / 2);
	}

	// Draw distort
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		int shiftx = lround(DISTORT_AMPLITUDE * sin(y * step + phase));

		for (int x = 0; x < RETRO_WIDTH; x++) {
			int sourcex = x + shiftx;
			int sourcey = y + columnshift[x];

			if (RETRO_OnScreen(sourcex, sourcey)) {
				RETRO_PutPixel(x, y, image[sourcey * RETRO_WIDTH + sourcex]);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/flag_320x240.pcx", true);
}
