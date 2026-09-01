//
// Text writer, one character at a time
//
// Pages of text appear in place, one character at a time (see FONT below).
// Nothing scrolls: a page is written, held, then replaced by the next.
// The number of visible characters comes from elapsed time, so the writing
// speed does not depend on the frame rate. Each page is given PAGE_TIME
// seconds: leftover time after the last character is a hold, then the
// next page is written from a clear screen. The cycle wraps, so the
// first page follows the last.
//
// Unlike textwriter.cpp, no RETRO_GenerateTextImage strip is built: every
// glyph is read straight out of RETRO_LoadFont's atlas at render time. The
// pages are a 2D array of lines rather than strings broken up by '\n', so
// ScrollText[page][row] is already a line's own text, with no scan needed
// to find where it ends. The inner bound is the longest page; unused
// slots are null. Every page is drawn in a block that tall, centred, so
// they all start on the same row.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8

#define CHARACTERS_PER_SECOND 20.0
#define PAGE_TIME 10.0 // write + hold, must cover the longest page

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

void DEMO_Render(double time, double deltatime)
{
	int pages = sizeof(ScrollText) / sizeof(ScrollText[0]);
	int lines = sizeof(ScrollText[0]) / sizeof(ScrollText[0][0]);
	double slot = fmod(time / PAGE_TIME, pages);
	int page = (int)slot;
	int remaining = (int)((slot - page) * PAGE_TIME * CHARACTERS_PER_SECOND);
	int texty = (RETRO_HEIGHT - lines * Font.height) / 2;

	for (int row = 0; row < lines && ScrollText[page][row] != NULL && remaining > 0; row++) {
		const char *line = ScrollText[page][row];
		int length = 0;
		int width = 0;
		for (const char *character = line; *character != 0; character++) {
			width += RETRO_CharWidth(Font, (unsigned char)*character);
			length++;
		}
		int visible = MIN(remaining, length);

		int screenx = (RETRO_WIDTH - width) / 2;
		int screeny = texty + row * Font.height;

		for (int character = 0; character < visible; character++) {
			unsigned char code = (unsigned char)line[character];
			int glyph = code - Font.firstcharacter;
			int sourcex = glyph * Font.width;

			if (glyph >= 0 && sourcex + Font.width <= Font.atlas->width) {
				for (int y = 0; y < Font.height; y++) {
					for (int x = 0; x < Font.width; x++) {
						unsigned char color = Font.atlas->data[y * Font.atlas->width + sourcex + x];
						if (color != 0) {
							RETRO_PutPixel(screenx + x, screeny + y, color);
						}
					}
				}
			}
			screenx += RETRO_CharWidth(Font, code);
		}

		remaining -= length + 1; // The line break takes one character time.
	}
}

void DEMO_Initialize(void)
{
	Font = RETRO_LoadFont(FONT);
	RETRO_SetPalette(Font.atlas->palette);
}
