//
// Scroller, supersampled
//
// The font (see FONT below) is packed into one horizontal strip at startup
// and scrolled at phase = time * SCROLL_SPEED exactly, on every display. A
// whole texel step only fits that at frame rates the speed divides, and any
// other mix of steps judders, so the fraction of phase is drawn as well, by
// supersampling: the strip is in effect drawn at a finer resolution at its
// exact position and averaged down to screen pixels. Moving horizontally,
// each screen pixel covers 1 - f of one strip texel and f of the next, so the
// average of its subpixels is just the mix of those two, taken directly
// without the finer buffer,
//
//   color = mix(strip[x + whole], strip[x + whole + 1], f)
//
// The motion between two pixels is then carried by brightness instead of by
// position. The mix is done in linear light, so a texel half way across a
// pixel gives the pixel half its light and the letters keep their weight as
// they move.
//
// The mixed colors are not in the font's palette. The font's colors keep the
// first entries, the strip is renumbered to them, and every mix of two of them
// at MIX_STEPS fractions is gathered and fitted into the rest of the palette.
// MixTable then holds the nearest entry for each, so a pixel costs two strip
// reads and one lookup. Color 0 is the cleared background, and a pixel that
// mixes to it is left alone.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retropalette.h"
#include "lib/retromain.h"

constexpr RETRO_FontAsset FONT = { "assets/font_16x16.pcx", 16, 16 };
//const RETRO_FontData &FONT = RETRO_FONT_MINECRAFT_8X8;

constexpr int SCROLL_SPEED = 200; // texels per second
constexpr int MIX_COLORS = 16;    // most font colors the mix table holds
constexpr int MIX_STEPS = 16;     // fractions of a texel the mix is drawn at

static const char ScrollText[] = "                       RETRO DEMOEFFECTS...";

static RETRO_Image *ScrollImage;
static unsigned char MixTable[MIX_COLORS * MIX_COLORS * MIX_STEPS];

static vec3 LinearColor(RETRO_Palette color)
{
	return { RETRO_SRGBToLinear(color.r / 255.0f), RETRO_SRGBToLinear(color.g / 255.0f), RETRO_SRGBToLinear(color.b / 255.0f) };
}

// The color of a pixel covering 1 - t of a and t of b
static RETRO_Palette MixColor(RETRO_Palette a, RETRO_Palette b, float t)
{
	return RETRO_LinearToColor(mix(LinearColor(a), LinearColor(b), t));
}

void DEMO_Render(RETRO_Time time)
{
	// Calculate phase: whole texels pick the column, the fraction the mix
	double phase = time.total * SCROLL_SPEED;
	double whole = floor(phase);
	int step = (int)((phase - whole) * MIX_STEPS);
	int iphase = (int)fmod(whole, ScrollImage->width);
	int y = (RETRO_HEIGHT - ScrollImage->height) / 2;

	// Draw scroller
	for (int i = 0; i < ScrollImage->height; i++) {
		const unsigned char *row = ScrollImage->data + i * ScrollImage->width;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int a = row[WRAP(x + iphase, ScrollImage->width)];
			int b = row[WRAP(x + iphase + 1, ScrollImage->width)];
			unsigned char color = MixTable[(a * MIX_COLORS + b) * MIX_STEPS + step];
			if (color != 0) {
				RETRO_PutPixel(x, i + y, color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	ScrollImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText);
	int size = ScrollImage->width * ScrollImage->height;

	// Give each font color a slot, the background first
	bool used[RETRO_COLORS] = {};
	used[0] = true;
	for (int i = 0; i < size; i++) {
		used[ScrollImage->data[i]] = true;
	}
	unsigned char slot[RETRO_COLORS] = {};
	RETRO_Palette held[MIX_COLORS];
	int colors = 0;
	for (int color = 0; color < RETRO_COLORS; color++) {
		if (!used[color]) continue;
		if (colors == MIX_COLORS) {
			RETRO_RageQuit("Font has more than %d colors\n", MIX_COLORS);
		}
		slot[color] = colors;
		held[colors] = ScrollImage->palette[color];
		colors++;
	}
	for (int i = 0; i < size; i++) {
		ScrollImage->data[i] = slot[ScrollImage->data[i]];
	}

	// Fit the mixed colors into the entries after the font's
	RETRO_ColorHistogram *histogram = (RETRO_ColorHistogram *)calloc(1, sizeof(RETRO_ColorHistogram));
	if (histogram == NULL) {
		RETRO_RageQuit("Cannot allocate histogram memory\n");
	}
	for (int a = 0; a < colors; a++) {
		for (int b = 0; b < colors; b++) {
			for (int s = 0; s < MIX_STEPS; s++) {
				RETRO_AddHistogramColor(histogram, MixColor(held[a], held[b], (float)s / MIX_STEPS), 1);
			}
		}
	}
	RETRO_Palette palette[RETRO_COLORS];
	RETRO_CreateHistogramPalette(histogram, palette, held, colors);
	free(histogram);

	// Build mix table
	for (int a = 0; a < colors; a++) {
		for (int b = 0; b < colors; b++) {
			for (int s = 0; s < MIX_STEPS; s++) {
				MixTable[(a * MIX_COLORS + b) * MIX_STEPS + s] = RETRO_NearestPaletteIndex(MixColor(held[a], held[b], (float)s / MIX_STEPS), palette);
			}
		}
	}

	RETRO_SetPalette(palette);
}
