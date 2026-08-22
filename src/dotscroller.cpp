//
// Dot scroller
//
// A right-to-left scroller made from the 16x16 font atlas. Each nonzero
// font texel becomes a small, separated dot. The scroll text is assembled
// into a bitmap at startup and repeated after it has crossed the screen.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define FONT_WIDTH 16
#define FONT_HEIGHT 16
#define IMAGE_WIDTH 944 // atlas pitch; 59 glyphs x 16

#define DOT_SIZE 1
#define DOT_SPACING 3
#define SCROLL_SPEED 120 // screen pixels per second

#define SCROLL_TEXT "        RETRO DEMOEFFECTS..."
#define SCROLL_LENGTH (sizeof(SCROLL_TEXT) - 1)
#define SCROLL_WIDTH (FONT_WIDTH * SCROLL_LENGTH)
#define DISPLAY_WIDTH (SCROLL_WIDTH * DOT_SPACING)
#define DISPLAY_HEIGHT ((FONT_HEIGHT - 1) * DOT_SPACING + DOT_SIZE)
#define SCROLL_Y ((RETRO_HEIGHT - DISPLAY_HEIGHT) / 2)

unsigned char ScrollBitmap[FONT_HEIGHT * SCROLL_WIDTH];

void DEMO_Render(double deltatime)
{
	static double phase = 0;
	phase = fmod(phase + deltatime * SCROLL_SPEED, DISPLAY_WIDTH);
	int iphase = (int)phase;

	// Plot each lit font texel as a dot. Adding DISPLAY_WIDTH wraps the
	// strip seamlessly once its trailing padding has left the screen.
	for (int sy = 0; sy < FONT_HEIGHT; sy++) {
		for (int sx = 0; sx < SCROLL_WIDTH; sx++) {
			unsigned char color = ScrollBitmap[sy * SCROLL_WIDTH + sx];
			if (color == 0) {
				continue;
			}

			int x = sx * DOT_SPACING - iphase;
			if (x < -DOT_SIZE + 1) {
				x += DISPLAY_WIDTH;
			}
			if (x >= RETRO_WIDTH) {
				continue;
			}

			int y = SCROLL_Y + sy * DOT_SPACING;
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
	RETRO_LoadImage("assets/font_16x16.pcx", true);

	unsigned char *image = RETRO_ImageData();
	for (int i = 0; i < (int)SCROLL_LENGTH; i++) {
		unsigned char *src = image + (SCROLL_TEXT[i] - 32) * FONT_WIDTH;
		unsigned char *dst = ScrollBitmap + i * FONT_WIDTH;

		for (int y = 0; y < FONT_HEIGHT; y++) {
			for (int x = 0; x < FONT_WIDTH; x++) {
				dst[y * SCROLL_WIDTH + x] = src[y * IMAGE_WIDTH + x];
			}
		}
	}
}
