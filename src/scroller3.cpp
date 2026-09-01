//
// Scroller, on a sine in x and y
//
// The same strip as scroller.cpp, sampled a column at a time. As in
// scroller2.cpp a column is dropped by a sine of x, and here it is also
// shifted sideways by a sine of x:
//
//   color = strip[row][(x + phase) mod strip_width]
//   x'    = x + AMP_X sin(xwave + x RATE_X)
//   y'    = SCROLL_Y + AMP_Y sin(ywave + x RATE_Y)
//
// so the text keeps its shape in the strip and is displaced in both
// axes. A letter climbs and dives, and it also slides left and right,
// because the two waves are fixed shapes in x that the letters pass
// through. A crest of the y-wave sits where ywave + x RATE_Y is a
// quarter turn; a sideways peak sits where xwave + x RATE_X is. Both
// travel left as their phases grow, the way the text does.
//
// Source columns are walked a little past each edge so a letter that
// has been shifted onto the screen is still drawn. The plot is clipped
// against the screen rather than the offset being trusted, because
// RETRO_PutPixel does not clip: it asserts in a debug build and writes
// out of bounds in any other.
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
#define SCROLL_SPEED 300 // texels per second
#define SCROLL_Y ((RETRO_HEIGHT - FONT_HEIGHT) / 2)
#define WAVE_Y_AMP 40 // pixels either side of the middle a column reaches
#define WAVE_Y_RATE 1.2 // table units of the sine per pixel, so 1.5 waves across
#define WAVE_Y_SPEED 90 // table units per second
#define WAVE_X_AMP 40 // pixels either side of its column a letter slides
#define WAVE_X_RATE 0.8 // table units of the sine per pixel, so one wave across
#define WAVE_X_SPEED 70 // table units per second

unsigned char scroll_bitmap[FONT_HEIGHT * SCROLL_WIDTH];

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase
	double phase = fmod(time * SCROLL_SPEED, SCROLL_WIDTH);
	int iphase = (int)phase;

	// Calculate the phase of the wave the columns ride in y
	double ywave = fmod(time * WAVE_Y_SPEED, RETRO_SINCOS_ANGLE);

	// Calculate the phase of the wave the columns ride in x
	double xwave = fmod(time * WAVE_X_SPEED, RETRO_SINCOS_ANGLE);

	// Draw scroller, a column at a time, each shifted by the sines at that
	// column and plotted only where it lands on the screen
	for (int x = -WAVE_X_AMP; x < RETRO_WIDTH + WAVE_X_AMP; x++) {
		int px = x + lround(WAVE_X_AMP * SIN(xwave + x * WAVE_X_RATE));
		if (px < 0 || px >= RETRO_WIDTH) {
			continue;
		}

		int top = SCROLL_Y + lround(WAVE_Y_AMP * SIN(ywave + x * WAVE_Y_RATE));
		int first = MAX(-top, 0);
		int last = MIN(RETRO_HEIGHT - top, FONT_HEIGHT);

		for (int i = first; i < last; i++) {
			unsigned char color = scroll_bitmap[i * SCROLL_WIDTH + WRAP(x + iphase, SCROLL_WIDTH)];
			if (color != 0) {
				RETRO_PutPixel(px, i + top, color);
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
