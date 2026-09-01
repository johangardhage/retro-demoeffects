//
// Scroller
//
// A 16×16 font blitted into a long strip, then sampled as
//
//   color = strip[row][(x + phase) mod strip_width]
//
// A zero texel is transparent, so the letters slide over a cleared
// background. The strip is FONT_WIDTH columns per character from the
// 944-wide atlas (glyphs U+0020 onward, one row). phase lives on
// strip_width. The strip is centred at y = (H − 16) / 2.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define FONT_WIDTH 16
#define FONT_HEIGHT 16
#define IMAGE_WIDTH 944 // atlas pitch; 59 glyphs × 16
#define SCROLL_TEXT "                    RETRO DEMOEFFECTS..."
#define SCROLL_LENGTH (sizeof(SCROLL_TEXT) - 1)
#define SCROLL_WIDTH (FONT_WIDTH * SCROLL_LENGTH)
#define SCROLL_SPEED 400 // texels per second
#define SCROLL_Y ((RETRO_HEIGHT - FONT_HEIGHT) / 2)

unsigned char scroll_bitmap[FONT_HEIGHT * SCROLL_WIDTH];

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase
	double phase = fmod(time * SCROLL_SPEED, SCROLL_WIDTH);
	int iphase = (int)phase;

	// Draw scroller
	for (int i = 0; i < FONT_HEIGHT; i++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			unsigned char color = scroll_bitmap[i * SCROLL_WIDTH + WRAP(x + iphase, SCROLL_WIDTH)];
			if (color != 0) {
				RETRO_PutPixel(x, i + SCROLL_Y, color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/font_16x16.pcx", true);

	// Init scroll bitmap
	unsigned char *image = RETRO_ImageData();

	for (int i = 0; i < (int)SCROLL_LENGTH; i++) {
		unsigned char *src = image + ((SCROLL_TEXT[i] - 32) * FONT_WIDTH);
		unsigned char *dst = scroll_bitmap + (i * FONT_WIDTH);

		for (int y = 0; y < FONT_HEIGHT; y++) {
			for (int x = 0; x < FONT_WIDTH; x++) {
				dst[SCROLL_WIDTH * y + x] = src[IMAGE_WIDTH * y + x];
			}
		}
	}
}
