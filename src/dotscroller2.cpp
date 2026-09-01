//
// 3D diagonal dot scroller
//
// The 16x16 font atlas is converted to a strip of points. The strip travels
// from the lower right to the upper left while a sine wave bends it in depth.
// Perspective makes the near parts larger and the far parts smaller; every
// visible font texel is still drawn as one pixel only.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define FONT_WIDTH 16
#define FONT_HEIGHT 16
#define IMAGE_WIDTH 944 // atlas pitch; 59 glyphs x 16

#define DOT_SPACING 3
#define SCROLL_SPEED 100 // screen pixels per second
#define DIAGONAL_RISE 0.25 // pixels upward for each pixel travelled left

#define DEPTH_AMPLITUDE 110
#define DEPTH_WAVELENGTH 220
#define DEPTH_SPEED 2.0 // radians per second
#define CAMERA_DISTANCE 360

#define SCROLL_TEXT "       RETRO DEMOEFFECTS..."
#define SCROLL_LENGTH (sizeof(SCROLL_TEXT) - 1)
#define SCROLL_WIDTH (FONT_WIDTH * SCROLL_LENGTH)
#define DISPLAY_WIDTH (SCROLL_WIDTH * DOT_SPACING)

unsigned char ScrollBitmap[FONT_HEIGHT * SCROLL_WIDTH];

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase
	double phase = fmod(time * SCROLL_SPEED, DISPLAY_WIDTH);
	double depthphase = fmod(time * DEPTH_SPEED, 2 * M_PI);

	for (int sy = 0; sy < FONT_HEIGHT; sy++) {
		for (int sx = 0; sx < SCROLL_WIDTH; sx++) {
			unsigned char color = ScrollBitmap[sy * SCROLL_WIDTH + sx];
			if (color == 0) {
				continue;
			}

			// Scroll the point strip and wrap it after its padded tail.
			double worldx = sx * DOT_SPACING - phase;
			if (worldx < 0) {
				worldx += DISPLAY_WIDTH;
			}
			if (worldx > RETRO_WIDTH + DEPTH_AMPLITUDE) {
				continue;
			}

			// Bend the strip toward and away from the camera. The depth wave
			// belongs to the text, so its shape travels with the letters.
			double z = DEPTH_AMPLITUDE * sin(sx * DOT_SPACING * 2 * M_PI /
				DEPTH_WAVELENGTH + depthphase);
			double scale = CAMERA_DISTANCE / (CAMERA_DISTANCE + z);

			double x = (worldx - RETRO_WIDTH / 2.0) * scale + RETRO_WIDTH / 2.0;
			double glyphy = (sy - (FONT_HEIGHT - 1) / 2.0) * DOT_SPACING;
			double pathy = (worldx - RETRO_WIDTH / 2.0) * DIAGONAL_RISE;
			double y = RETRO_HEIGHT / 2.0 + (glyphy + pathy) * scale;

			int px = lround(x);
			int py = lround(y);
			if (px >= 0 && px < RETRO_WIDTH && py >= 0 && py < RETRO_HEIGHT) {
				RETRO_PutPixel(px, py, color);
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
