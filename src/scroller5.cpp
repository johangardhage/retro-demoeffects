//
// Scroller, wrapped around a circle
//
// The 16x16 font is assembled into the same horizontal strip used by the
// other scrollers.  Columns from that strip are sampled along the
// circumference of a circle, while font rows are plotted radially:
//
//   column = (arc + phase) mod strip_width
//   angle  = pi / 2 + arc / radius
//   radius = circle_radius + font_height / 2 - row
//
// Two samples are taken per screen pixel of circumference.  This closes
// small holes caused by rounding the polar coordinates to screen pixels.
// The path begins just after the bottom and stops just before returning to
// it, leaving a fixed opening centred on the bottom. A zero texel remains
// transparent.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define FONT_WIDTH 16
#define FONT_HEIGHT 16
#define IMAGE_WIDTH 944 // atlas pitch; 59 glyphs x 16
#define SCROLL_TEXT "                          RETRO DEMOEFFECTS..."
#define SCROLL_LENGTH (sizeof(SCROLL_TEXT) - 1)
#define SCROLL_GAP (FONT_WIDTH * 2) // transparent space between the end and start
#define TEXT_WIDTH (FONT_WIDTH * SCROLL_LENGTH)
#define SCROLL_WIDTH (TEXT_WIDTH + SCROLL_GAP)
#define SCROLL_SPEED 60.0 // strip columns per second
#define CIRCLE_X (RETRO_WIDTH / 2)
#define CIRCLE_Y (RETRO_HEIGHT / 2)
#define CIRCLE_RADIUS 68
#define CIRCLE_GAP 16 // pixels of open circumference before the bottom
#define SAMPLES_PER_PIXEL 2
#define CIRCLE_SAMPLES ((int)((2 * M_PI * CIRCLE_RADIUS - CIRCLE_GAP) * SAMPLES_PER_PIXEL))

unsigned char scroll_bitmap[FONT_HEIGHT * SCROLL_WIDTH];

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase
	double phase = fmod(time * SCROLL_SPEED, SCROLL_WIDTH);

	// Walk once around the circle. Arc is measured in screen pixels, so a
	// font column retains approximately the same width as in the atlas.
	for (int sample = 0; sample < CIRCLE_SAMPLES; sample++) {
		// Screen y grows downward, so pi / 2 is bottom centre. Offset the
		// start by half the gap; the shortened path leaves the other half at
		// its end. Advancing by arc length preserves that opening.
		double angle = M_PI / 2 + CIRCLE_GAP / (2.0 * CIRCLE_RADIUS)
			+ (double)sample / (CIRCLE_RADIUS * SAMPLES_PER_PIXEL);
		double arc = (double)sample / SAMPLES_PER_PIXEL;
		int column = WRAP((int)(arc + phase), SCROLL_WIDTH);
		double cs = cos(angle);
		double sn = sin(angle);

		for (int row = 0; row < FONT_HEIGHT; row++) {
			unsigned char color = scroll_bitmap[row * SCROLL_WIDTH + column];
			if (color == 0) {
				continue;
			}

			// Put the top font row toward the outside of the circle so the
			// lettering is upright and outward-facing across the top.
			double radius = CIRCLE_RADIUS + (FONT_HEIGHT - 1) / 2.0 - row;
			int x = lround(CIRCLE_X + radius * cs);
			int y = lround(CIRCLE_Y + radius * sn);
			if (x >= 0 && x < RETRO_WIDTH && y >= 0 && y < RETRO_HEIGHT) {
				RETRO_PutPixel(x, y, color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/font_16x16.pcx", true);

	unsigned char *image = RETRO_ImageData();
	for (int i = 0; i < (int)SCROLL_LENGTH; i++) {
		unsigned char *src = image + (SCROLL_TEXT[i] - 32) * FONT_WIDTH;
		unsigned char *dst = scroll_bitmap + i * FONT_WIDTH;

		for (int y = 0; y < FONT_HEIGHT; y++) {
			for (int x = 0; x < FONT_WIDTH; x++) {
				dst[y * SCROLL_WIDTH + x] = src[y * IMAGE_WIDTH + x];
			}
		}
	}
}
