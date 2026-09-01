//
// Scroller, wrapped around a circle
//
// The font strip (see FONT below) is sampled along the circumference of a
// circle, while font rows are plotted radially:
//
//   column = (arc + phase) mod stripwidth
//   angle  = pi / 2 + arc / radius
//   radius = circleradius + fontheight / 2 - row
//
// Arc is measured in screen pixels, so a font column keeps approximately
// the width it has in the atlas. Two samples are taken per screen pixel of
// circumference, which closes small holes caused by rounding the polar
// coordinates to screen pixels. The path begins just after the bottom and
// stops just before returning to it, leaving a fixed opening centred on
// the bottom. The top font row is toward the outside of the circle, so the
// lettering is upright and outward-facing across the top. A zero texel
// remains transparent.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8
#define SCROLL_SPEED 60.0 // strip columns per second
#define CIRCLE_X (RETRO_WIDTH / 2)
#define CIRCLE_Y (RETRO_HEIGHT / 2)
#define CIRCLE_RADIUS 68
#define CIRCLE_GAP 16 // pixels of open circumference before the bottom
#define SAMPLES_PER_PIXEL 2
#define CIRCLE_SAMPLES ((int)((2 * M_PI * CIRCLE_RADIUS - CIRCLE_GAP) * SAMPLES_PER_PIXEL))

static const char *const ScrollText[] = { "                          RETRO DEMOEFFECTS..." };

RETRO_Image *ScrollImage;

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase
	double phase = fmod(time * SCROLL_SPEED, ScrollImage->width);

	// Walk once around the circle. Arc is measured in screen pixels, so a
	// font column retains approximately the same width as in the atlas.
	for (int sample = 0; sample < CIRCLE_SAMPLES; sample++) {
		// Screen y grows downward, so pi / 2 is bottom centre. Offset the
		// start by half the gap; the shortened path leaves the other half at
		// its end. Advancing by arc length preserves that opening.
		double angle = M_PI / 2 + CIRCLE_GAP / (2.0 * CIRCLE_RADIUS) + (double)sample / (CIRCLE_RADIUS * SAMPLES_PER_PIXEL);
		double arc = (double)sample / SAMPLES_PER_PIXEL;
		int column = WRAP((int)(arc + phase), ScrollImage->width);
		double cs = cos(angle);
		double sn = sin(angle);

		for (int row = 0; row < ScrollImage->height; row++) {
			unsigned char color = ScrollImage->data[row * ScrollImage->width + column];
			if (color == 0) {
				continue;
			}

			// Put the top font row toward the outside of the circle so the
			// lettering is upright and outward-facing across the top.
			double radius = CIRCLE_RADIUS + (ScrollImage->height - 1) / 2.0 - row;
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
	ScrollImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText, sizeof(ScrollText) / sizeof(ScrollText[0]));
	RETRO_SetPalette(ScrollImage->palette);
}
