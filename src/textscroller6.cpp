//
// Scroller, Star Wars crawl
//
// Pages of text are packed into one strip and sampled as a plane receding
// toward a horizon: rows become narrower and more compressed as they
// travel up the screen.
//
//   distance = y − HORIZON_Y
//   scale    = distance / (BOTTOM_Y − HORIZON_Y)
//   sourcey  = phase − PERSPECTIVE_DEPTH / distance
//   u        = (x − WIDTH/2) / scale + stripwidth / 2
//
// scale is 1 at the bottom and 0 at the horizon, so a strip row that is
// still near the bottom fills the screen and the same row, later, pinches
// toward the vanishing point. sourcey runs backward in distance, which
// is what makes the text crawl up: a larger phase is a strip row that has
// already receded. phase starts at the sourcey lag of the bottom row, so
// the first line is already entering at t = 0 rather than waiting below
// the frame for that lag (and a further glyph-height) to close. A zero
// texel is transparent.
//
// The strip is a single call to RETRO_GenerateTextImage: a null slot is
// an empty row, so each page occupies the inner bound of rows and each
// line is already centered within the widest one. The pages are a 2D
// array of lines rather than strings broken up by '\n', so
// ScrollText[page][row] is already a line's own text, with no scan needed
// to find where it ends. The inner bound is the longest page; unused
// slots are null.
//
// PERSPECTIVE_DEPTH, CRAWL_SPEED and the horizon geometry are all tuned by
// feel against the reference 16x16 PCX font. FONT_SCALE brings whichever
// font is active up to that same glyph height, so every constant below
// keeps meaning what it was tuned to mean regardless of the source font.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
#define FONT_SCALE 1
//#define FONT RETRO_FONT_MINECRAFT_8X8
//#define FONT_SCALE 2
#define HORIZON_Y 42
#define BOTTOM_Y RETRO_HEIGHT
#define BOTTOM_DISTANCE (BOTTOM_Y - HORIZON_Y)
#define CRAWL_SPEED 22.0 // page rows per second
#define PERSPECTIVE_DEPTH 3600.0
#define PHASE_BOTTOM (PERSPECTIVE_DEPTH / BOTTOM_DISTANCE) // sourcey lag at y = BOTTOM_Y
#define CRAWL_GAP 180 // extra rows after the strip before the crawl wraps

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
	double phase = fmod(time * CRAWL_SPEED, PageImage->height + CRAWL_GAP) + PHASE_BOTTOM;

	for (int y = HORIZON_Y + 1; y < BOTTOM_Y; y++) {
		double distance = y - HORIZON_Y;
		double scale = distance / BOTTOM_DISTANCE;
		double sourcey = phase - PERSPECTIVE_DEPTH / distance;
		int sy = (int)sourcey;
		if (sy < 0 || sy >= PageImage->height) {
			continue;
		}

		int halfwidth = (int)(PageImage->width * scale / 2.0);
		if (halfwidth < 1) {
			continue;
		}
		int left = RETRO_WIDTH / 2 - halfwidth;
		int right = RETRO_WIDTH / 2 + halfwidth;

		for (int x = MAX(left, 0); x < MIN(right, RETRO_WIDTH); x++) {
			double u = (x - RETRO_WIDTH / 2) / scale + PageImage->width / 2.0;
			int sx = (int)u;
			if (sx < 0 || sx >= PageImage->width) {
				continue;
			}

			unsigned char color = PageImage->data[sy * PageImage->width + sx];
			if (color != 0) {
				RETRO_PutPixel(x, y, color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	PageImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), &ScrollText[0][0], sizeof(ScrollText) / sizeof(ScrollText[0][0]), FONT_SCALE);
	RETRO_SetPalette(PageImage->palette);
}
