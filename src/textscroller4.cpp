//
// Scroller, rows flying in from the right
//
// Row i of the current page flies in from off-screen right to a resting
// position, holds there, then flies back out the same way before the next
// page begins. STAGGER offsets one row's cycle from the next, so they
// cascade in rather than all moving at once.
//
// progress runs 0 (off-screen) to 1 (resting) during entry, holds at 1,
// then runs back to 0 during exit, using the same easing and wave both
// ways. Entry is slower than exit. x eases from off-screen to its resting
// column with a smoothstep, so the row starts and ends its slide gently
// instead of at a constant speed. y is the resting row position plus a
// sine of WAVE_CYCLES turns, scaled by (1 - eased progress) so the wave
// is at full swing off-screen and dies out exactly as the row settles,
// rather than snapping straight to rest.
//
// All pages are a single call to RETRO_GenerateTextImage: a null slot is
// an empty row, so each page occupies the inner bound of rows and each
// line is already centered within the widest one. Row i is then just the
// horizontal band of that strip. The pages are a 2D array of lines rather
// than strings broken up by '\n', so ScrollText[page][row] is already a
// line's own text, with no scan needed to find where it ends. The inner
// bound is the longest page; unused slots are null. Every page is drawn
// in a block that tall, centred, so they all start on the same row.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8

#define ROW_GAP 2 // extra pixels between rows, beyond glyph height

#define ENTRANCE_TIME 1.6 // seconds to fly in
#define EXIT_TIME 0.8 // seconds to fly out
#define HOLD_TIME 4.0 // seconds a row stays at rest before flying out
#define STAGGER 0.25 // seconds after one row starts that the next starts
#define WAVE_AMPLITUDE 40.0 // pixels the row swings above and below rest
#define WAVE_CYCLES 2.0 // sine turns completed over one entrance

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
	double cycle = ENTRANCE_TIME + HOLD_TIME + EXIT_TIME;
	double pagetime = (lines - 1) * STAGGER + cycle;
	double slot = fmod(time / pagetime, pages);
	int page = (int)slot;
	double paget = (slot - page) * pagetime;

	int restx = (RETRO_WIDTH - PageImage->width) / 2;
	int startx = RETRO_WIDTH;
	int rowheight = FONT.height;
	int spacing = rowheight + ROW_GAP;
	int starty = (RETRO_HEIGHT - (lines * spacing - ROW_GAP)) / 2;

	for (int i = 0; i < lines && ScrollText[page][i] != NULL; i++) {
		double t = paget - i * STAGGER;
		if (t < 0 || t >= cycle) {
			continue;
		}

		double progress;
		if (t < ENTRANCE_TIME) {
			progress = t / ENTRANCE_TIME;
		} else if (t < ENTRANCE_TIME + HOLD_TIME) {
			progress = 1.0;
		} else {
			progress = 1.0 - (t - ENTRANCE_TIME - HOLD_TIME) / EXIT_TIME;
		}
		double eased = progress * progress * (3.0 - 2.0 * progress); // ease gently through both ends

		int x = startx + (int)round((restx - startx) * eased);
		int resty = starty + i * spacing;
		int y = resty + (int)round(WAVE_AMPLITUDE * sin(progress * WAVE_CYCLES * 2 * M_PI) * (1.0 - eased));

		int sourcetop = (page * lines + i) * rowheight;
		int firsty = MAX(0, -y);
		int lasty = MIN(rowheight, RETRO_HEIGHT - y);
		int firstx = MAX(0, -x);
		int lastx = MIN(PageImage->width, RETRO_WIDTH - x);

		for (int ry = firsty; ry < lasty; ry++) {
			for (int rx = firstx; rx < lastx; rx++) {
				unsigned char color = PageImage->data[(sourcetop + ry) * PageImage->width + rx];
				if (color != 0) {
					RETRO_PutPixel(x + rx, y + ry, color);
				}
			}
		}
	}
}

void DEMO_Initialize(void)
{
	PageImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), &ScrollText[0][0], sizeof(ScrollText) / sizeof(ScrollText[0][0]));
	RETRO_SetPalette(PageImage->palette);
}
