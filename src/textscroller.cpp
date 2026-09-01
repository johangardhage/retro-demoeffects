//
// Scroller
//
// The font (see FONT below) is packed into one horizontal strip at startup
// and sampled as
//
//   color = strip[row][(x + phase) mod stripwidth]
//
// A zero texel is transparent, so the letters slide over a cleared
// background. phase lives on stripwidth, in texels per second. The strip
// is centred vertically and is as tall as one glyph, so every screen row
// it occupies maps 1:1 onto a strip row. When phase returns to 0 the same
// column is under x = 0 again, and the wrap is seamless.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8

#define SCROLL_SPEED 200 // texels per second

static const char *const ScrollText[] = { "                    RETRO DEMOEFFECTS..." };

RETRO_Image *ScrollImage;

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase
	double phase = fmod(time * SCROLL_SPEED, ScrollImage->width);
	int iphase = (int)phase;
	int y = (RETRO_HEIGHT - ScrollImage->height) / 2;

	// Draw scroller
	for (int i = 0; i < ScrollImage->height; i++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			unsigned char color = ScrollImage->data[i * ScrollImage->width + WRAP(x + iphase, ScrollImage->width)];
			if (color != 0) {
				RETRO_PutPixel(x, i + y, color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	ScrollImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText, sizeof(ScrollText) / sizeof(ScrollText[0]));
	RETRO_SetPalette(ScrollImage->palette);
}
