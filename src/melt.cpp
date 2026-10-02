//
// Melt
//
// Two vertical melting effects applied to a still picture.
//
// The first repeats every row of the picture an increasing number of times.
// The image is progressively crushed toward the top while the rows that remain
// visible stretch into thick horizontal bands, then it smoothly expands back
// to its original shape.
//
// The second displays the picture normally down to a moving horizontal line,
// then fills everything below it by repeating the row at that line. As the
// boundary travels down and back up, the repeated strip pours over and uncovers
// the picture like a vertical curtain.
//
// Tab resets the animation and switches between the two effects.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define MSL_MAX 31
#define SCANLINES (RETRO_HEIGHT * 2)
#define MSL_STEPS_PER_SECOND 35.0
#define FREEZE_STEPS_PER_SECOND 70.0

enum MeltMode { MELT_MAXIMUM_SCAN_LINE, MELT_FREEZE_LINE_OFFSET };

void DEMO_Render(double time, double deltatime)
{
	static MeltMode mode = MELT_FREEZE_LINE_OFFSET;
	static double meltposition = 0;
	static int meltdirection = 1;

	if (RETRO_KeyPressed(SDL_SCANCODE_TAB)) {
		mode = mode == MELT_MAXIMUM_SCAN_LINE ? MELT_FREEZE_LINE_OFFSET : MELT_MAXIMUM_SCAN_LINE;
		meltposition = 0;
		meltdirection = 1;
	}

	double maximum = mode == MELT_MAXIMUM_SCAN_LINE ? MSL_MAX : SCANLINES;
	double speed = mode == MELT_MAXIMUM_SCAN_LINE ? MSL_STEPS_PER_SECOND : FREEZE_STEPS_PER_SECOND;
	meltposition += meltdirection * speed * deltatime;

	// Reflect overshoot at an endpoint so a long frame still bounces cleanly.
	while (meltposition < 0 || meltposition > maximum) {
		if (meltposition > maximum) {
			meltposition = 2 * maximum - meltposition;
			meltdirection = -1;
		} else {
			meltposition = -meltposition;
			meltdirection = 1;
		}
	}

	unsigned char *image = RETRO_ImageData();
	unsigned char *buffer = RETRO_FrameBuffer();

	if (mode == MELT_MAXIMUM_SCAN_LINE) {
		int repeat = (int)meltposition + 1;
		for (int y = 0; y < RETRO_HEIGHT; y++) {
			memcpy(buffer + y * RETRO_WIDTH, image + (y / repeat) * RETRO_WIDTH, RETRO_WIDTH);
		}
	} else {
		// The animation has two scanline steps for each image row. Once the
		// moving boundary reaches a row, that row repeats below it.
		int freezerow = MIN((int)meltposition / 2, RETRO_HEIGHT - 1);
		for (int y = 0; y < RETRO_HEIGHT; y++) {
			int sourcerow = MIN(y, freezerow);
			memcpy(buffer + y * RETRO_WIDTH, image + sourcerow * RETRO_WIDTH, RETRO_WIDTH);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/monkey_320x240.pcx", true);
}
