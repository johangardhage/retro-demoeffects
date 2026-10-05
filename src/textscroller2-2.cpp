//
// Scroller, vertical pages
//
// Pages of centered text (see ScrollText below) rise from below the
// screen to above it at a constant rate,
//
//   top = RETRO_HEIGHT - phase
//
// where phase runs from 0, with the whole stack below the screen, to
// RETRO_HEIGHT + stack height, with the whole stack above it. Both ends of
// that range are fully off-screen, so the loop back to phase 0 is seamless.
//
// Unlike textscroller2.cpp, no RETRO_GenerateTextImage page is built: every
// glyph is read straight out of RETRO_LoadFont's atlas at render time. The
// pages are a 2D array of lines rather than strings broken up by '\n', so
// ScrollText[page][row] is already a line's own text, with no scan needed
// to find where it ends. The inner bound is the longest page; unused
// slots are null. Every page occupies that many rows, so the stack height
// that phase needs is counted by the compiler from the array itself.
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

static RETRO_Font Font;

static void DrawChar(unsigned char code, int x, int y)
{
	int copywidth = MIN(Font.width, RETRO_CharWidth(Font, code));
	for (int yy = 0; yy < Font.height; yy++) {
		int py = y + yy;
		if (py < 0 || py >= RETRO_HEIGHT) {
			continue;
		}
		for (int xx = 0; xx < copywidth; xx++) {
			int px = x + xx;
			if (px < 0 || px >= RETRO_WIDTH) {
				continue;
			}
			unsigned char color = RETRO_FontTexel(Font, code, xx, yy);
			if (color != 0) {
				RETRO_PutPixel(px, py, color);
			}
		}
	}
}

void DEMO_Render(RETRO_Time time)
{
	int pages = sizeof(ScrollText) / sizeof(ScrollText[0]);
	int lines = sizeof(ScrollText[0]) / sizeof(ScrollText[0][0]);
	int rows = pages * lines;

	// Calculate phase
	double travel = RETRO_HEIGHT + rows * Font.height;
	double phase = fmod(time.total * SCROLL_SPEED, travel);
	int top = RETRO_HEIGHT - (int)phase;

	for (int row = 0; row < rows; row++) {
		const char *line = ScrollText[row / lines][row % lines];
		if (line == NULL) {
			continue;
		}

		int y = top + row * Font.height;
		if (y + Font.height > 0 && y < RETRO_HEIGHT) {
			int x = (RETRO_WIDTH - RETRO_TextLineWidth(Font, line)) / 2;
			for (const char *character = line; *character != 0; character++) {
				DrawChar((unsigned char)*character, x, y);
				x += RETRO_CharWidth(Font, (unsigned char)*character);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	Font = RETRO_LoadFont(FONT);
	RETRO_SetPalette(Font.atlas->palette);
}
