//
// Dot scroller
//
// The font (see FONT below) is packed into one horizontal strip at startup
// and sampled as
//
//   color = strip[row][column]
//   x     = column · DOT_SPACING − phase
//   y     = scrolly + row · DOT_SPACING
//
// Each nonzero texel becomes a DOT_SIZE square, spaced DOT_SPACING apart.
// phase lives on displaywidth = stripwidth · DOT_SPACING, in screen pixels
// per second. Adding displaywidth wraps the strip seamlessly once its
// trailing padding has left the screen. A zero texel is transparent.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8

#define DOT_SIZE 1
#define DOT_SPACING 3
#define SCROLL_SPEED 120 // screen pixels per second

static const char *const ScrollText[] = { "        RETRO DEMOEFFECTS..." };

RETRO_Image *ScrollImage;

void DEMO_Render(double time, double deltatime)
{
	int displaywidth = ScrollImage->width * DOT_SPACING;
	int displayheight = (ScrollImage->height - 1) * DOT_SPACING + DOT_SIZE;
	int scrolly = (RETRO_HEIGHT - displayheight) / 2;

	// Calculate phase
	double phase = fmod(time * SCROLL_SPEED, displaywidth);
	int iphase = (int)phase;

	// Plot each lit font texel as a dot. Adding displaywidth wraps the
	// strip seamlessly once its trailing padding has left the screen.
	for (int sy = 0; sy < ScrollImage->height; sy++) {
		for (int sx = 0; sx < ScrollImage->width; sx++) {
			unsigned char color = ScrollImage->data[sy * ScrollImage->width + sx];
			if (color == 0) {
				continue;
			}

			int x = sx * DOT_SPACING - iphase;
			if (x < -DOT_SIZE + 1) {
				x += displaywidth;
			}
			if (x >= RETRO_WIDTH) {
				continue;
			}

			int y = scrolly + sy * DOT_SPACING;
			for (int dy = 0; dy < DOT_SIZE; dy++) {
				for (int dx = 0; dx < DOT_SIZE; dx++) {
					int px = x + dx;
					if (px >= 0 && px < RETRO_WIDTH) {
						RETRO_PutPixel(px, y + dy, color);
					}
				}
			}
		}
	}
}

void DEMO_Initialize(void)
{
	ScrollImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText, sizeof(ScrollText) / sizeof(ScrollText[0]));
	RETRO_SetPalette(ScrollImage->palette);
}
