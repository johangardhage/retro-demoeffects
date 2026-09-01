//
// Copperbars
//
// Seven horizontal bars, one per rainbow color, on black: the Amiga effect where
// the copper rewrites the background color once per scanline. Here every
// scanline is a palette index instead, and each bar owns a ramp of
// COPPER_BARHEIGHT entries, so a whole frame is a memset per row and all the
// tube shading comes out of the palette.
//
// The bars stay straight and ride one cosine up and down, each a fixed lag
// behind the bar before it,
//
//   y_k(t) = HEIGHT/2 + A cos(t + k LAG)
//
// so a bar arrives where its predecessor was LAG units ago and the seven of
// them travel as a procession. Reading them top to bottom traces the wave
// itself: they bunch where the cosine flattens at the turning points and
// stretch apart where it runs steepest through the middle. The lag is small
// enough that even at the steepest the bars stay all but shoulder to shoulder,
// so the procession reads as a body rather than as separate bars on a string.
//
// The bars are drawn back to front in palette order, giving bar 6 priority
// wherever two overlap, the way a copper list lets the last write to a
// scanline win. t lives on RETRO_SINCOS_ANGLE.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define COPPER_BARS 7
#define COPPER_BARHEIGHT 20
#define COPPER_CORE 3 // rows of highlight either side of the middle of a bar
#define COPPER_RAMP0 1 // palette entry the first bar's ramp starts at, past the background
#define COPPER_LAG 10 // how far behind its predecessor a bar rides
#define COPPER_AMP 95
#define COPPER_SPEED 70 // table units per second

// The rainbow, one color per bar and one bar per color, in draw order, which is
// also the order of the bow: red on the outer edge with the longest wavelength
// down to violet on the inner edge with the shortest. Entry 0 is the black
// background, so bar k ramps over [COPPER_RAMP0 + k * COPPER_BARHEIGHT, ...)
RETRO_Palette BarColors[COPPER_BARS] = {
	RETRO_RED, RETRO_ORANGE, RETRO_YELLOW, RETRO_GREEN,
	RETRO_BLUE, RETRO_INDIGO, RETRO_VIOLET };

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase
	double phase = fmod(time * COPPER_SPEED, RETRO_SINCOS_ANGLE);

	// Draw bars, back to front, each riding the cosine a lag behind the one before.
	// Row j of a bar is entry j of that bar's ramp, so the tube shading comes out
	// of the palette, and a bar that runs off an edge is clipped to the screen
	for (int k = 0; k < COPPER_BARS; k++) {
		int y = lround(RETRO_HEIGHT / 2 + COPPER_AMP * COS(phase + k * COPPER_LAG)) - COPPER_BARHEIGHT / 2;
		int ramp = COPPER_RAMP0 + k * COPPER_BARHEIGHT;
		int top = MAX(y, 0);
		int bottom = MIN(y + COPPER_BARHEIGHT, RETRO_HEIGHT);

		for (int row = top; row < bottom; row++) {
			memset(RETRO_FrameBuffer() + row * RETRO_WIDTH, ramp + row - y, RETRO_WIDTH);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. Entry 0 is the background the framebuffer is cleared to
	RETRO_SetColor(0, RETRO_BLACK);

	// Each bar is a glass tube: a dark rim rising to its own hue over most of the
	// way in, then a white highlight over the COPPER_CORE rows around the middle,
	// and the same in reverse below it
	for (int k = 0; k < COPPER_BARS; k++) {
		RETRO_Palette hue = BarColors[k];
		RETRO_Palette rim = RETRO_Palette{ (unsigned char)(hue.r / 5), (unsigned char)(hue.g / 5), (unsigned char)(hue.b / 5) };

		int ramp = COPPER_RAMP0 + k * COPPER_BARHEIGHT;
		int middle = ramp + COPPER_BARHEIGHT / 2;

		RETRO_CreateGradientPalette(ramp, middle - COPPER_CORE, rim, hue);
		RETRO_CreateGradientPalette(middle - COPPER_CORE, middle, hue, RETRO_WHITE);
		RETRO_CreateGradientPalette(middle, middle + COPPER_CORE, RETRO_WHITE, hue);
		RETRO_CreateGradientPalette(middle + COPPER_CORE, ramp + COPPER_BARHEIGHT, hue, rim);
	}
}
