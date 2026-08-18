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
// Space resets the animation and switches between the two effects.
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

MeltMode Mode = MELT_FREEZE_LINE_OFFSET;
double MeltPosition = 0;
int MeltDirection = 1;

void DEMO_Update(double deltatime)
{
	if (RETRO_KeyPressed(SDL_SCANCODE_SPACE)) {
		Mode = Mode == MELT_MAXIMUM_SCAN_LINE ? MELT_FREEZE_LINE_OFFSET : MELT_MAXIMUM_SCAN_LINE;
		MeltPosition = 0;
		MeltDirection = 1;
	}

	double maximum = Mode == MELT_MAXIMUM_SCAN_LINE ? MSL_MAX : SCANLINES;
	double speed = Mode == MELT_MAXIMUM_SCAN_LINE ? MSL_STEPS_PER_SECOND : FREEZE_STEPS_PER_SECOND;
	MeltPosition += MeltDirection * speed * deltatime;

	// Reflect overshoot at an endpoint so motion stays smooth if a frame catches up.
	while (MeltPosition < 0 || MeltPosition > maximum) {
		if (MeltPosition > maximum) {
			MeltPosition = 2 * maximum - MeltPosition;
			MeltDirection = -1;
		} else {
			MeltPosition = -MeltPosition;
			MeltDirection = 1;
		}
	}
}

void DEMO_Render(double deltatime)
{
	unsigned char *image = RETRO_ImageData();
	unsigned char *buffer = RETRO_FrameBuffer();

	if (Mode == MELT_MAXIMUM_SCAN_LINE) {
		int repeat = (int)MeltPosition + 1;
		for (int y = 0; y < RETRO_HEIGHT; y++) {
			memcpy(buffer + y * RETRO_WIDTH, image + (y / repeat) * RETRO_WIDTH, RETRO_WIDTH);
		}
	} else {
		// The animation has two scanline steps for each image row. Once the
		// moving boundary reaches a row, that row repeats below it.
		int freezeRow = MIN((int)MeltPosition / 2, RETRO_HEIGHT - 1);
		for (int y = 0; y < RETRO_HEIGHT; y++) {
			int sourceRow = MIN(y, freezeRow);
			memcpy(buffer + y * RETRO_WIDTH, image + sourceRow * RETRO_WIDTH, RETRO_WIDTH);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/monkey_320x240.pcx");
	RETRO_SetPalette(RETRO_ImagePalette());
}
