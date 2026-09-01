//
// Copperbars, from one scanline
//
// The effect known as Kefrens bars. A single line buffer of WIDTH bytes is
// stamped with one bar and copied to row 0, stamped again and copied to row 1,
// and so on down the screen, and it is never cleared between rows, so row y
// carries every bar stamped above it and the image is the bar's trail. On the
// Amiga this was one bitplane line and a copper list pointing every scanline
// at it, which is why a whole screen cost one line of drawing.
//
// The bar at row y sits at
//
//   x(y) = CX + A1 sin(phase + y S1) + A2 sin(RATIO phase + y S2)
//
// Two sines of different rate, so the path folds back over itself and the bar
// sweeps the same column several times on the way down. A stamp overwrites,
// so where the fold crosses its own trail the lower row wins: later is in
// front, the way the last write to a scanline wins a copper list. RATIO is a
// fraction and phase runs over RATIO_DEN turns rather than one, because a
// wrap is invisible only when it moves both sines a whole number of turns.
//
// Color is chosen by row alone and does not move, one fixed band per BARS of
// the screen. A bar keeps the color it was stamped in for every row below, so
// a band holds its own color and the trails of every band above it. Across its
// width the bar is a ramp, dark rim to hue to a white core, so the trail reads
// as a tube rather than a flat stripe.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define COPPER_BARWIDTH 28
#define COPPER_BARS 8 // colors the bar cycles through down the screen
#define COPPER_CORE 3 // columns of highlight either side of the middle of the bar
#define COPPER_RAMP0 1 // palette entry the first bar's ramp starts at, past the background
#define COPPER_BANDHEIGHT (RETRO_HEIGHT / COPPER_BARS) // rows one color lasts
#define COPPER_CX ((RETRO_WIDTH - COPPER_BARWIDTH) / 2)
#define COPPER_AMP1 90
#define COPPER_AMP2 45
#define COPPER_RATE1 1.7 // table units the first sine turns per row
#define COPPER_RATE2 -2.9 // table units the second sine turns per row
#define COPPER_RATIO_NUM 5 // the second sine's phase against the first, as a fraction
#define COPPER_RATIO_DEN 8 // so that both sines are whole turns when phase wraps
#define COPPER_PERIOD (RETRO_SINCOS_ANGLE * COPPER_RATIO_DEN)
#define COPPER_SPEED 55 // table units per second

// One color per band and one band per color, in the order of the bow
RETRO_Palette BarColors[COPPER_BARS] = {
	RETRO_RED, RETRO_ORANGE, RETRO_YELLOW, RETRO_GREEN,
	RETRO_CYAN, RETRO_AZURE, RETRO_INDIGO, RETRO_VIOLET };

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * COPPER_SPEED, COPPER_PERIOD);

	// The one line every scanline is a copy of. Cleared once per frame, not once
	// per row: what a row inherits from the rows above it is the whole effect
	static unsigned char line[RETRO_WIDTH];
	memset(line, 0, sizeof(line));

	// Stamp one bar per row and copy the line down. Column i of the bar is entry
	// i of that band's ramp, so the tube shading comes out of the palette, and a
	// bar that runs off an edge is clipped to the line
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		int x = lround(COPPER_CX + COPPER_AMP1 * SIN(phase + y * COPPER_RATE1) + COPPER_AMP2 * SIN((double)COPPER_RATIO_NUM / COPPER_RATIO_DEN * phase + y * COPPER_RATE2));
		int ramp = COPPER_RAMP0 + (y / COPPER_BANDHEIGHT) * COPPER_BARWIDTH;
		int left = MAX(x, 0);
		int right = MIN(x + COPPER_BARWIDTH, RETRO_WIDTH);

		for (int i = left; i < right; i++) {
			line[i] = ramp + i - x;
		}

		memcpy(RETRO_FrameBuffer() + y * RETRO_WIDTH, line, RETRO_WIDTH);
	}
}

void DEMO_Initialize(void)
{
	// Init palette. Entry 0 is the background the line is cleared to
	RETRO_SetColor(0, RETRO_BLACK);

	// Each band is a glass tube seen end on: a dark rim rising to its own hue over
	// most of the way in, then a white highlight over the COPPER_CORE columns
	// around the middle, and the same in reverse on the far side
	for (int k = 0; k < COPPER_BARS; k++) {
		RETRO_Palette hue = BarColors[k];
		RETRO_Palette rim = RETRO_Palette{ (unsigned char)(hue.r / 5), (unsigned char)(hue.g / 5), (unsigned char)(hue.b / 5) };

		int ramp = COPPER_RAMP0 + k * COPPER_BARWIDTH;
		int middle = ramp + COPPER_BARWIDTH / 2;

		RETRO_CreateGradientPalette(ramp, middle - COPPER_CORE, rim, hue);
		RETRO_CreateGradientPalette(middle - COPPER_CORE, middle, hue, RETRO_WHITE);
		RETRO_CreateGradientPalette(middle, middle + COPPER_CORE, RETRO_WHITE, hue);
		RETRO_CreateGradientPalette(middle + COPPER_CORE, ramp + COPPER_BARWIDTH, hue, rim);
	}
}
