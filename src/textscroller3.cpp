//
// Scroller, on a sine
//
// The font strip (see FONT below) is sampled a column at a time and dropped
// down the screen by a sine of the column,
//
//   color = strip[row][(x + phase) mod stripwidth]
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
#include "lib/retrofont.h"
#include "lib/retromain.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8

#define SCROLL_SPEED 200 // texels per second
#define WAVE_AMP 40 // pixels either side of the middle a column reaches
#define WAVE_RATE 1.2 // table units of the sine per pixel, so 1.5 waves across
#define WAVE_SPEED 90 // table units per second

static const char *const ScrollText[] = { "                    RETRO DEMOEFFECTS..." };

RETRO_Image *ScrollImage;

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase
	double phase = fmod(time * SCROLL_SPEED, ScrollImage->width);
	int iphase = (int)phase;
	int scrolly = (RETRO_HEIGHT - ScrollImage->height) / 2;

	// Calculate the phase of the wave the columns ride
	double wave = fmod(time * WAVE_SPEED, RETRO_SINCOS_ANGLE);

	// Draw scroller, a column at a time, each dropped by the sine at that column
	// and its glyph rows clipped to the screen
	for (int x = 0; x < RETRO_WIDTH; x++) {
		int top = scrolly + lround(WAVE_AMP * SIN(wave + x * WAVE_RATE));
		int first = MAX(-top, 0);
		int last = MIN(RETRO_HEIGHT - top, ScrollImage->height);

		for (int i = first; i < last; i++) {
			unsigned char color = ScrollImage->data[i * ScrollImage->width + WRAP(x + iphase, ScrollImage->width)];
			if (color != 0) {
				RETRO_PutPixel(x, i + top, color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	ScrollImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText, sizeof(ScrollText) / sizeof(ScrollText[0]));
	RETRO_SetPalette(ScrollImage->palette);
}
