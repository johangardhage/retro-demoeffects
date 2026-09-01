//
// Scroller, on a sine
//
// The same strip as scroller.cpp, sampled a column at a time and dropped down
// the screen by a sine of the column,
//
//   color = strip[row][(x + phase) mod strip_width]
//   y     = SCROLL_Y + AMP sin(wave + x RATE)
//
// so the text keeps its shape in x and is displaced only in y. A letter is
// therefore never bent sideways, only sheared up and down, and it climbs and
// dives as it travels because the wave it rides is a fixed shape in x that
// the letters pass through.
//
// The wave travels too. RATE is table units of the sine per pixel and wave
// grows with time, so a crest sits where wave + x RATE is a quarter turn and
// moves left as wave grows, the way the text does. The two run on phases of
// their own, one wrapping on the strip and one on the sine table, and each is
// used only as the argument of something that repeats on exactly that period,
// so neither wrap shows.
//
// A zero texel is transparent. The glyph rows are clipped against the screen
// rather than the offset being trusted, because RETRO_PutPixel does not clip:
// it asserts in a debug build and writes out of bounds in any other.
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
#define WAVE_AMP 40 // pixels either side of the middle a column reaches
#define WAVE_RATE 1.2 // table units of the sine per pixel, so 1.5 waves across
#define WAVE_SPEED 90 // table units per second

unsigned char scroll_bitmap[FONT_HEIGHT * SCROLL_WIDTH];

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase
	double phase = fmod(time * SCROLL_SPEED, SCROLL_WIDTH);
	int iphase = (int)phase;

	// Calculate the phase of the wave the columns ride
	double wave = fmod(time * WAVE_SPEED, RETRO_SINCOS_ANGLE);

	// Draw scroller, a column at a time, each dropped by the sine at that column
	// and its glyph rows clipped to the screen
	for (int x = 0; x < RETRO_WIDTH; x++) {
		int top = SCROLL_Y + lround(WAVE_AMP * SIN(wave + x * WAVE_RATE));
		int first = MAX(-top, 0);
		int last = MIN(RETRO_HEIGHT - top, FONT_HEIGHT);

		for (int i = first; i < last; i++) {
			unsigned char color = scroll_bitmap[i * SCROLL_WIDTH + WRAP(x + iphase, SCROLL_WIDTH)];
			if (color != 0) {
				RETRO_PutPixel(x, i + top, color);
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
