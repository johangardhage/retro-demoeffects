//
// Scroller
//
// The same right-to-left scroll as textscroller.cpp, but with no strip built
// up front: a screen column's sample position in text space,
//
//   sample = (x + phase) mod (sizeof(ScrollText) · FONT.width)
//
// picks out both which character it falls in and which of its columns, so
// the pixel is read straight out of RETRO_LoadFont's atlas,
//
//   character = ScrollText[sample / FONT.width]
//   column    = sample mod FONT.width
//
// A zero texel is transparent, so the letters slide over a cleared
// background. phase lives on the text's pixel width. The row is centred
// vertically.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8

#define SCROLL_SPEED 200 // texels per second

static const char ScrollText[] = "                    RETRO DEMOEFFECTS...";

static RETRO_Font Font;

void DEMO_Render(double time, double deltatime)
{
	int textwidth = (int)(sizeof(ScrollText) - 1) * Font.width;

	// Calculate phase
	double phase = fmod(time * SCROLL_SPEED, textwidth);
	int iphase = (int)phase;
	int y = (RETRO_HEIGHT - Font.height) / 2;

	// Draw scroller
	for (int x = 0; x < RETRO_WIDTH; x++) {
		int sample = WRAP(x + iphase, textwidth);
		unsigned char code = (unsigned char)ScrollText[sample / Font.width];
		int glyph = code - Font.firstcharacter;
		int sourcex = glyph * Font.width + sample % Font.width;

		if (glyph < 0 || sourcex >= Font.atlas->width) {
			continue;
		}
		for (int yy = 0; yy < Font.height; yy++) {
			unsigned char color = Font.atlas->data[yy * Font.atlas->width + sourcex];
			if (color != 0) {
				RETRO_PutPixel(x, y + yy, color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	Font = RETRO_LoadFont(FONT);
	RETRO_SetPalette(Font.atlas->palette);
}
