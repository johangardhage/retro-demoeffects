//
// Scroller, vertical pages
//
// Pages of centred text (see ScrollText below) are packed into one strip
// and rise from below the screen to above it at a constant rate,
//
//   top = RETRO_HEIGHT - phase
//
// where phase runs from 0, with the whole strip below the screen, to
// RETRO_HEIGHT + strip height, with the whole strip above it. Both ends of
// that range are fully off-screen, so the loop back to phase 0 is seamless.
// A zero texel is transparent, and the plot is clipped against the screen
// rather than the offset being trusted, because RETRO_PutPixel does not clip.
//
// The strip is a single call to RETRO_GenerateTextImage: a null slot is
// an empty row, so each page occupies the inner bound of rows and each
// line is already centered within the widest one. The pages are a 2D
// array of lines rather than strings broken up by '\n', so
// ScrollText[page][row] is already a line's own text, with no scan needed
// to find where it ends. The inner bound is the longest page; unused
// slots are null.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8

#define SCROLL_SPEED 40.0 // pixels per second

static const char *const ScrollText[][12] = {
	{
		"THE TEXT WRITER",
		"",
		"SIMPLE TEXT WRITER",
		"",
		"A PAGE OF TEXT",
		"APPEARS ON SCREEN",
		"ONE CHARACTER",
		"AT A TIME.",
		"",
		"WHEN IT IS DONE",
		"THE NEXT PAGE",
		"BEGINS.",
	},
	{
		"EACH PAGE IS",
		"WRITTEN OUT",
		"THEN HELD,",
		"THEN CLEARED.",
		"",
		"THE WRITER MOVES ON.",
		"",
		"AND THEN IT STARTS",
		"ALL OVER AGAIN.",
		"",
		"RETRO",
		"DEMOEFFECTS...",
	},
};

static RETRO_Image *PageImage;

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase
	double travel = RETRO_HEIGHT + PageImage->height;
	double phase = fmod(time * SCROLL_SPEED, travel);
	int top = RETRO_HEIGHT - (int)phase;
	int left = (RETRO_WIDTH - PageImage->width) / 2;

	int firsty = MAX(0, -top);
	int lasty = MIN(PageImage->height, RETRO_HEIGHT - top);
	int firstx = MAX(0, -left);
	int lastx = MIN(PageImage->width, RETRO_WIDTH - left);

	for (int y = firsty; y < lasty; y++) {
		for (int x = firstx; x < lastx; x++) {
			unsigned char color = PageImage->data[y * PageImage->width + x];
			if (color != 0) {
				RETRO_PutPixel(left + x, top + y, color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	PageImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), &ScrollText[0][0], sizeof(ScrollText) / sizeof(ScrollText[0][0]));
	RETRO_SetPalette(PageImage->palette);
}
