//
// Blinds
//
// A vertical-blind transition inspired by Sanity's Interference. A white
// screen is split into narrow vertical slats. Each slat folds around its
// centre, so its projected width contracts to a line and reveals the black
// curtain behind it. Each slat starts its fold a little later than the one to
// its right - the delay is a linear ramp across the screen - so the fold
// travels leftwards instead of closing every slat at once. The transition runs
// once and remains on black when it is finished.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define BLIND_WIDTH 16
#define BLINDS ((RETRO_WIDTH + BLIND_WIDTH - 1) / BLIND_WIDTH)
#define PHASE_WINDOW 0.72 // part of the transition spent rotating one slat
#define TIME_TRANSITION 2.0
#define TIME_HOLD 0.75
#define CURTAIN 0
#define FIRST_SHADE 1
#define SHADES 32

void DEMO_Render(double deltatime)
{
	static double phase = 0;
	phase += deltatime;
	double progress = CLAMP01((phase - TIME_HOLD) / TIME_TRANSITION);

	unsigned char *buffer = RETRO_FrameBuffer();

	// RETRO clears the framebuffer before this callback, leaving the curtain.
	for (int blind = 0; blind < BLINDS; blind++) {
		int left = blind * BLIND_WIDTH;
		int width = MIN(BLIND_WIDTH, RETRO_WIDTH - left);

		// The rotation starts at the right and travels left. Each slat turns
		// through 270 degrees: edge-on, broadside once more, then edge-on a
		// second time. That second turn is easy to miss in the reference.
		double delay = (double)(BLINDS - 1 - blind) / (BLINDS - 1)
			* (1 - PHASE_WINDOW);
		double rotation = CLAMP01((progress - delay) / PHASE_WINDOW);
		double fold = fabs(cos(rotation * 1.5 * M_PI));

		// Project the rotating slat about its centre: the visible span is the
		// width foreshortened, laid out centred on the slat's own centre. An
		// odd leftover splits half a pixel to the left, which is invisible
		// against a 16-pixel slat.
		int visible = lround(width * fold);
		int first = left + (width - visible) / 2;
		int shade = FIRST_SHADE + lround(fold * (SHADES - 1));

		for (int y = 0; y < RETRO_HEIGHT; y++) {
			int offset = y * RETRO_WIDTH + first;
			memset(buffer + offset, shade, visible);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(CURTAIN, RETRO_BLACK);

	// The original transition does not leave the rotating faces flat white.
	// They lose reflected light as they turn, passing through cold blue-grey
	// to the deep blue visible just before a slat becomes edge-on.
	for (int shade = 0; shade < SHADES; shade++) {
		double light = (double)shade / (SHADES - 1);
		int r = lround(8 + light * (255 - 8));
		int g = lround(24 + light * (255 - 24));
		int b = lround(40 + light * (255 - 40));
		RETRO_SetColor(FIRST_SHADE + shade, r, g, b);
	}
}
