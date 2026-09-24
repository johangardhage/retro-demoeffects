//
// Scroller, on a racing sine
//
// Big chunky letters crawl slowly to the left while a sine wave races
// through them several times faster, so the text is thrown up and down
// rather than carried along the wave. As in textscroller3.cpp the font strip
// (see FONT below) is sampled a column at a time and each column dropped by
// the sine at that column,
//
//   texel = strip[row][(x + phase) mod stripwidth]
//   y     = SCROLL_Y + AMP sin(wave + x RATE)
//
// so a letter is sheared up and down, never bent sideways.
//
// The glyphs are used as a mask only. A column is painted in one flat color
// from a cyclic white, pink, red, magenta, purple, lavender ramp, which is
// laid across the screen and slides left on a phase of its own, so the
// colors belong to the screen and a letter changes color as it travels.
//
// A zero texel is transparent. The glyph rows are clipped against the screen
// rather than the offset being trusted, because RETRO_PutPixel does not clip.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retropalette.h"
#include "lib/retromain.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }

#define SCROLL_SPEED 42 // texels per second
#define WAVE_AMP 40 // pixels either side of the middle a column reaches
#define WAVE_RATE 0.85 // table units of the sine per pixel, so one wave per 300 pixels
#define WAVE_SPEED 230 // table units per second
#define COLOR_STEPS 120 // palette entries in one turn of the color ramp
#define COLOR_RATE 0.8 // ramp entries per pixel, so one turn per 150 pixels
#define COLOR_SPEED 100 // ramp entries per second

static const char *const ScrollText[] = { "                    RETRO DEMOEFFECTS..." };

static const RETRO_Palette ColorStops[] = {
	{ 255, 240, 255 }, // white
	{ 255, 130, 210 }, // pink
	{ 240, 20, 120 }, // red
	{ 210, 20, 220 }, // magenta
	{ 140, 30, 255 }, // purple
	{ 200, 160, 255 }, // lavender
};

RETRO_Image *ScrollImage;

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase
	double phase = fmod(time * SCROLL_SPEED, ScrollImage->width);
	int iphase = (int)phase;
	int scrolly = (RETRO_HEIGHT - ScrollImage->height) / 2;

	// Calculate the phases of the wave the columns ride and of the color ramp
	double wave = fmod(time * WAVE_SPEED, RETRO_ANGLES_PER_TURN);
	double colorphase = fmod(time * COLOR_SPEED, COLOR_STEPS);

	RETRO_Clear(0);

	// Draw scroller, a column at a time, each dropped by the sine at that column,
	// painted in that column's color and its glyph rows clipped to the screen
	for (int x = 0; x < RETRO_WIDTH; x++) {
		int top = scrolly + lround(WAVE_AMP * SIN(wave + x * WAVE_RATE));
		int first = MAX(-top, 0);
		int last = MIN(RETRO_HEIGHT - top, ScrollImage->height);
		unsigned char color = 1 + WRAP((int)(x * COLOR_RATE + colorphase), COLOR_STEPS);

		for (int i = first; i < last; i++) {
			if (ScrollImage->data[i * ScrollImage->width + WRAP(x + iphase, ScrollImage->width)] != 0) {
				RETRO_PutPixel(x, i + top, color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	ScrollImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText, sizeof(ScrollText) / sizeof(ScrollText[0]));

	// Color 0 is the black background, 1 to COLOR_STEPS the cyclic ramp
	int stops = sizeof(ColorStops) / sizeof(ColorStops[0]);
	int length = COLOR_STEPS / stops;
	RETRO_SetColor(0, RETRO_BLACK);
	for (int i = 0; i < stops; i++) {
		RETRO_CreateGradientPalette(1 + i * length, 1 + (i + 1) * length, ColorStops[i], ColorStops[(i + 1) % stops]);
	}
}
