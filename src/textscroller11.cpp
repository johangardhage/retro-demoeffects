//
// Scroller, filled with rasters
//
// Big letters crawl slowly to the left while a sine wave races through them
// several times faster, so the text is thrown up and down rather than carried
// along the wave. The letters are not painted in colors of their own. Every
// scanline of the screen has one color, and a letter is a hole that the
// colors of the scanlines it covers show through. Outside the letters the
// screen is black, so the rasters are only ever seen inside them.
//
// The color of a scanline is set anew every frame, in two layers:
//
//   ground  a cyclic ramp of GROUND_STEPS colors, one to a scanline, sliding
//           down the screen at GROUND_SPEED
//   bars    BARS bars of BAR_HEIGHT scanlines over the ground, each shaded as
//           a tube, riding one cosine up and down, each BAR_LAG behind the
//           bar before it:
//
//             y = HEIGHT/2 + BAR_AMP cos(bar + k BAR_LAG)
//
// The font strip (see FONT below) is sampled a column at a time, and each
// column dropped by the sine at that column,
//
//   texel = strip[row][(x + phase) mod stripwidth]
//   y     = scrolly + WAVE_AMP sin(wave + x WAVE_RATE)
//
// so a letter is sheared up and down, never bent sideways. Where the texel is
// not zero the pixel gets the color of its scanline. The colors belong to the
// screen, not to the text: a letter thrown up by the sine goes up through the
// bars, and a bar on its way down goes through one letter after the other
// without bending with them.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retropalette.h"
#include "lib/retromain.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
#define FONT_SCALE 3

#define SCROLL_SPEED 42 // texels per second
#define WAVE_AMP 40 // pixels either side of the middle a column reaches
#define WAVE_RATE 0.85 // table units of the sine per pixel, so one wave per 300 pixels
#define WAVE_SPEED 230 // table units per second
#define GROUND_STEPS 96 // palette entries, and scanlines, in one turn of the ground
#define GROUND_SPEED 30 // scanlines per second
#define GROUND_RAMP0 1 // palette entry the ground's ramp starts at, past the background
#define BARS 4
#define BAR_HEIGHT 16
#define BAR_CORE 2 // rows of highlight either side of the middle of a bar
#define BAR_RAMP0 (GROUND_RAMP0 + GROUND_STEPS) // palette entry the first bar's ramp starts at
#define BAR_LAG 30 // table units a bar rides behind the bar before it
#define BAR_AMP 62 // pixels either side of the middle a bar reaches
#define BAR_SPEED 75 // table units per second

static const char *const ScrollText[] = { "       RETRO DEMOEFFECTS..." };

static const RETRO_Palette GroundStops[] = {
	{ 30, 60, 200 }, // blue
	{ 130, 40, 190 }, // purple
	{ 0, 130, 150 }, // teal
};

// One color per bar, in draw order
static const RETRO_Palette BarColors[BARS] = { RETRO_RED, RETRO_ORANGE, RETRO_YELLOW, RETRO_GREEN };

static RETRO_Image *ScrollImage;

void DEMO_Render(RETRO_Time time)
{
	// Calculate phase
	double phase = fmod(time.total * SCROLL_SPEED, ScrollImage->width);
	int iphase = (int)phase;
	int scrolly = (RETRO_HEIGHT - ScrollImage->height) / 2;

	// Calculate the phases of the wave the columns ride, of the ground and of
	// the cosine the bars ride
	double wave = fmod(time.total * WAVE_SPEED, RETRO_ANGLES_PER_TURN);
	double ground = fmod(time.total * GROUND_SPEED, GROUND_STEPS);
	double bar = fmod(time.total * BAR_SPEED, RETRO_ANGLES_PER_TURN);

	// Set the color of every scanline, the ground first
	unsigned char raster[RETRO_HEIGHT];
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		raster[y] = GROUND_RAMP0 + WRAP(y - (int)ground, GROUND_STEPS);
	}

	// Then the bars over it, back to front, each riding the cosine a lag behind
	// the one before. Row j of a bar is entry j of that bar's ramp
	for (int k = 0; k < BARS; k++) {
		int y = lround(RETRO_HEIGHT / 2.0 + BAR_AMP * COS(bar - k * BAR_LAG)) - BAR_HEIGHT / 2;
		int ramp = BAR_RAMP0 + k * BAR_HEIGHT;
		int top = MAX(y, 0);
		int bottom = MIN(y + BAR_HEIGHT, RETRO_HEIGHT);

		for (int row = top; row < bottom; row++) {
			raster[row] = ramp + row - y;
		}
	}

	// Draw scroller, a column at a time, each dropped by the sine at that column,
	// its glyph rows clipped to the screen and painted in the colors of their
	// scanlines
	for (int x = 0; x < RETRO_WIDTH; x++) {
		int top = scrolly + lround(WAVE_AMP * SIN(wave + x * WAVE_RATE));
		int first = MAX(-top, 0);
		int last = MIN(RETRO_HEIGHT - top, ScrollImage->height);

		for (int i = first; i < last; i++) {
			if (ScrollImage->data[i * ScrollImage->width + WRAP(x + iphase, ScrollImage->width)] != 0) {
				RETRO_PutPixel(x, i + top, raster[i + top]);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	ScrollImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText, sizeof(ScrollText) / sizeof(ScrollText[0]), FONT_SCALE);

	// Init palette. Entry 0 is the black background, then the cyclic ramp of
	// the ground
	int stops = sizeof(GroundStops) / sizeof(GroundStops[0]);
	int length = GROUND_STEPS / stops;
	RETRO_SetColor(0, RETRO_BLACK);
	for (int i = 0; i < stops; i++) {
		RETRO_CreateGradientPalette(GROUND_RAMP0 + i * length, GROUND_RAMP0 + (i + 1) * length, GroundStops[i], GroundStops[(i + 1) % stops]);
	}

	// Each bar is a tube: a dark rim rising to its own hue over most of the way
	// in, then a white highlight over the BAR_CORE rows around the middle, and
	// the same in reverse below it
	for (int k = 0; k < BARS; k++) {
		RETRO_Palette hue = BarColors[k];
		RETRO_Palette rim = hue * 0.2f;

		int ramp = BAR_RAMP0 + k * BAR_HEIGHT;
		int middle = ramp + BAR_HEIGHT / 2;

		RETRO_CreateGradientPalette(ramp, middle - BAR_CORE, rim, hue);
		RETRO_CreateGradientPalette(middle - BAR_CORE, middle, hue, RETRO_WHITE);
		RETRO_CreateGradientPalette(middle, middle + BAR_CORE, RETRO_WHITE, hue);
		RETRO_CreateGradientPalette(middle + BAR_CORE, ramp + BAR_HEIGHT, hue, rim);
	}
}
