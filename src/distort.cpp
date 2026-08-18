//
// Distort
//
// An image resampled through a pair of shears. Pixel (x, y) is copied from
//
//   I(x + A sin(2pi (y + t) / N),  y + (A/2) sin(2pi (x + t) / N))
//
// so a whole row shares its sideways shift and a whole column shares its
// vertical one. Both travel along the same table as time passes, so the
// picture ripples. t lives on N. A sample that lands off the image is
// left as background, and the edges fray.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define SINE_VALUES 64 // entries in the sine table, covering one whole turn
#define DISTORT_AMPLITUDE 5 // pixels a row shifts sideways at the peak
#define DISTORT_SPEED 100 // table entries travelled per second

int ShiftX[SINE_VALUES];
int ShiftY[SINE_VALUES];

void DEMO_Render(double deltatime)
{
	unsigned char *image = RETRO_ImageData();

	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * DISTORT_SPEED, SINE_VALUES);
	int iphase = phase;

	// A column's vertical shift depends only on x, so it is the same for every
	// row: worked out once per frame rather than once per pixel.
	int columnshift[RETRO_WIDTH];
	for (int x = 0; x < RETRO_WIDTH; x++) {
		columnshift[x] = ShiftY[WRAP(x + iphase, SINE_VALUES)];
	}

	// Draw distort
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		int shiftx = ShiftX[WRAP(y + iphase, SINE_VALUES)];

		for (int x = 0; x < RETRO_WIDTH; x++) {
			int sourcex = x + shiftx;
			int sourcey = y + columnshift[x];

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

	// Init tables. One whole turn over N entries, so the table meets itself where the index
	// wraps and the shear has no seam. The vertical shear is A/2, rounded once.
	for (int i = 0; i < SINE_VALUES; i++) {
		double angle = 2 * M_PI * i / SINE_VALUES;

		ShiftX[i] = lround(DISTORT_AMPLITUDE * sin(angle));
		ShiftY[i] = lround(DISTORT_AMPLITUDE * sin(angle) / 2);
	}
}
