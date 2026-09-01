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
// Unlike textwriter-2.cpp, all pages are a single call to
// RETRO_GenerateTextImage: a null slot is an empty row, so each page
// occupies the inner bound of rows and each line is already centered
// within the widest one. Revealing copies a cutoff prefix of that strip,
// the cutoff being the sum of RETRO_CharWidth over the characters already
// written, so a proportional font is not clipped mid-glyph. A line break
// takes one character of the remaining budget, the same as a glyph. The
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

static RETRO_Image *PageImage;

void DEMO_Render(double time, double deltatime)
{
	int pages = sizeof(ScrollText) / sizeof(ScrollText[0]);
	int lines = sizeof(ScrollText[0]) / sizeof(ScrollText[0][0]);
	double slot = fmod(time / PAGE_TIME, pages);
	int page = (int)slot;
	int remaining = (int)((slot - page) * PAGE_TIME * CHARACTERS_PER_SECOND);
	int texty = (RETRO_HEIGHT - lines * FONT.height) / 2;

	for (int row = 0; row < lines && ScrollText[page][row] != NULL && remaining > 0; row++) {
		const char *line = ScrollText[page][row];
		int length = 0;
		int width = 0;
		for (const char *character = line; *character != 0; character++) {
			width += RETRO_CharWidth(FONT, (unsigned char)*character);
			length++;
		}
		int visible = MIN(remaining, length);

		if (visible > 0) {
			int cutoff = 0;
			for (int character = 0; character < visible; character++) {
				cutoff += RETRO_CharWidth(FONT, (unsigned char)line[character]);
			}
			int sourcex = (PageImage->width - width) / 2;
			int screenx = (RETRO_WIDTH - width) / 2;
			int sourcey = (page * lines + row) * FONT.height;
			int screeny = texty + row * FONT.height;

			for (int y = 0; y < FONT.height; y++) {
				for (int column = 0; column < cutoff; column++) {
					unsigned char color = PageImage->data[(sourcey + y) * PageImage->width + sourcex + column];
					if (color != 0) {
						RETRO_PutPixel(screenx + column, screeny + y, color);
					}
				}
			}
		}

		remaining -= length + 1; // The line break takes one character time.
	}
}

void DEMO_Initialize(void)
{
	PageImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), &ScrollText[0][0], sizeof(ScrollText) / sizeof(ScrollText[0][0]));
	RETRO_SetPalette(PageImage->palette);
}
