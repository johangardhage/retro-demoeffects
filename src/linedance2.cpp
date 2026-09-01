//
// Linedance 2
//
// A 360-point Lissajous curve, one pixel per degree. The point is
//
//   x = W/2 + 50 sin(b + 2a) + 25 sin(a + 2b) − 50 sin(a + b)
//   y = H/2 + 20 sin(a + 2b) + 15 sin(b + 2a) + 20 sin(a + b)
//
// with a the time phase and b = 2π i / 360. sin(b+2a) has period π in a,
// the other terms 2π, so a lives on 360°. Consecutive samples are joined
// by a segment, so the fast parts of the parametrisation stay a curve
// rather than breaking into dots. Color is 50 + x/2 at the segment's
// midpoint. The framebuffer is cleared each frame.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retropalette.h"

#define LINE_SPEED 60 // degrees of a per second

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * LINE_SPEED, RETRO_DEGREES_PER_TURN);
	double a = phase * M_PI / 180;

	// Every term is 2π periodic in b, so one extra degree closes the loop
	double prevx = 0, prevy = 0;

	for (int i = 0; i <= RETRO_DEGREES_PER_TURN; i++) {
		double b = i * M_PI / 180;
		double x = RETRO_WIDTH / 2 + 50 * sin(b + a * 2) + 25 * sin(a + b * 2) - 50 * sin(a + b);
		double y = RETRO_HEIGHT / 2 + 20 * sin(a + b * 2) + 15 * sin(b + a * 2) + 20 * sin(a + b);

		if (i > 0) {
			// One shade per segment, taken at its midpoint
			unsigned char color = 50 + (prevx + x) / 4;
			RETRO_DrawLine(lround(prevx), lround(prevy), lround(x), lround(y), color);
		}

		prevx = x;
		prevy = y;
	}
}

void DEMO_Initialize(void)
{
	// Init palette. The lines pick their color from their x position, so they
	// heat up from red through orange into white from left to right
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK, RETRO_BLACK);
	RETRO_CreateGradientPalette(50, 95, RETRO_BLACK, RETRO_RED);
	RETRO_CreateGradientPalette(95, 135, RETRO_RED, RETRO_ORANGE);
	RETRO_CreateGradientPalette(135, 200, RETRO_ORANGE, RETRO_WHITE);
}
