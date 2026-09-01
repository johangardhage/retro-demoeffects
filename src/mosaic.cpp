//
// Mosaic
//
// The picture rebuilt from blocks of b by b pixels, each block filled with the
// single source pixel nearest its centre. The block grows and shrinks:
//
//   b(u) = round(BLOCK_MAX^u),   u in [0, 1]
//
// which is geometric in u, not linear, so equal time buys an equal factor of
// detail rather than an equal number of pixels: 1, 2, 4, 8. A linear ramp
// spends nearly all of the pass among blocks too large to tell apart, while
// the step from one pixel to two, the one the eye actually notices, is over
// before it starts.
//
// b need not divide the screen. A block is clipped where it runs off the right
// edge or the bottom, and it is sampled at its own centre rather than the
// centre of what is left of it, so the last column and the last row stay the
// colors the untruncated grid would have given them.
//
// BLOCK_MAX is the width of the screen, so the pass ends on one block: the
// picture coarsens until it is the single color under its centre, and then
// sharpens back out of it.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define BLOCK_MAX RETRO_WIDTH // pixels on a side of the coarsest block
#define TIME_MOSAIC 3.0 // seconds the picture takes to coarsen or to sharpen
#define TIME_HOLD 0.75 // seconds held on the picture or on the one block
#define TIME_CYCLE (2 * (TIME_HOLD + TIME_MOSAIC))

//
// How far the picture has coarsened at this point of the cycle
//
double Coarseness(double phase)
{
	if (phase < TIME_HOLD) {
		return 0;
	}
	if (phase < TIME_HOLD + TIME_MOSAIC) {
		return (phase - TIME_HOLD) / TIME_MOSAIC;
	}
	if (phase < 2 * TIME_HOLD + TIME_MOSAIC) {
		return 1;
	}
	return 1 - (phase - 2 * TIME_HOLD - TIME_MOSAIC) / TIME_MOSAIC;
}

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase
	double phase = fmod(time, TIME_CYCLE);
	int block = lround(pow(BLOCK_MAX, Coarseness(phase)));

	unsigned char *image = RETRO_ImageData();
	unsigned char *buffer = RETRO_FrameBuffer();

	// Draw one block per source pixel
	for (int y = 0; y < RETRO_HEIGHT; y += block) {
		int sourcey = MIN(y + block / 2, RETRO_HEIGHT - 1);
		int rows = MIN(block, RETRO_HEIGHT - y);

		for (int x = 0; x < RETRO_WIDTH; x += block) {
			int sourcex = MIN(x + block / 2, RETRO_WIDTH - 1);
			int columns = MIN(block, RETRO_WIDTH - x);
			unsigned char color = image[sourcey * RETRO_WIDTH + sourcex];

			for (int row = 0; row < rows; row++) {
				memset(buffer + (y + row) * RETRO_WIDTH + x, color, columns);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_Image *image = RETRO_LoadImage("assets/monkey_320x240.pcx", true);
	if (image->width != RETRO_WIDTH || image->height != RETRO_HEIGHT) {
		RETRO_RageQuit("The image must be the size of the screen\n");
	}
}
