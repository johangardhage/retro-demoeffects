//
// Text writer, scrubbing head
//
// A disc bounces up and down while moving left across a page of centred
// text. A glyph pixel is drawn only after the disc has passed over it,
// held, then wiped the same way. Pages are one RETRO_GenerateTextImage
// strip; a null slot is an empty row.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8
#define LINE_PITCH 18

#define HEAD_RADIUS 10.0
#define HEAD_STROKES 24
#define PATH_STEPS 2048
#define CROSS_TIME 6.0
#define TIME_HOLD 2.5
#define TIME_BLANK 0.5
#define COLOR_BRUSH 2
#define SHOW_BRUSH false // draw the disc as it scrubs

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
static unsigned char Covered[RETRO_HEIGHT * RETRO_WIDTH];

static void Head(double u, int top, int bottom, bool erasing, double *hx, double *hy)
{
	*hx = RETRO_WIDTH + HEAD_RADIUS - u * (RETRO_WIDTH + 2 * HEAD_RADIUS);
	*hy = (top + bottom) / 2.0 + ((bottom - top) / 2.0 + HEAD_RADIUS) * sin((u * HEAD_STROKES + (erasing ? 0.5 : 0)) * 2 * M_PI);
}

static void PlotDisc(unsigned char *dest, double hx, double hy, unsigned char color)
{
	int first = MAX((int)(hy - HEAD_RADIUS), 0);
	int last = MIN((int)(hy + HEAD_RADIUS) + 1, RETRO_HEIGHT - 1);
	for (int y = first; y <= last; y++) {
		double square = HEAD_RADIUS * HEAD_RADIUS - (y - hy) * (y - hy);
		if (square <= 0) {
			continue;
		}
		double dx = sqrt(square);
		int left = MAX((int)(hx - dx), 0);
		int right = MIN((int)(hx + dx), RETRO_WIDTH - 1);
		for (int x = left; x <= right; x++) {
			dest[y * RETRO_WIDTH + x] = color;
		}
	}
}

void DEMO_Render(double time, double deltatime)
{
	int pages = sizeof(ScrollText) / sizeof(ScrollText[0]);
	int lines = sizeof(ScrollText[0]) / sizeof(ScrollText[0][0]);
	int block = (lines - 1) * LINE_PITCH + FONT.height;
	int top = MAX(RETRO_HEIGHT - block, 0) / 2;
	int bottom = top + block - 1;
	double pagetime = 2 * CROSS_TIME + TIME_HOLD + TIME_BLANK;
	double slot = fmod(time / pagetime, pages);
	int page = (int)slot;
	double t = (slot - page) * pagetime;

	if (t >= pagetime - TIME_BLANK) {
		return;
	}

	bool erasing = t >= CROSS_TIME + TIME_HOLD;
	bool moving = t < CROSS_TIME || erasing;
	double u = 0;
	if (t < CROSS_TIME) {
		u = t / CROSS_TIME;
	} else if (erasing) {
		u = (t - CROSS_TIME - TIME_HOLD) / CROSS_TIME;
	}

	double hx = 0, hy = 0;
	if (moving) {
		memset(Covered, 0, sizeof(Covered));
		int steps = MAX(1, (int)(u * PATH_STEPS));
		for (int i = 0; i <= steps; i++) {
			Head(u * i / steps, top, bottom, erasing, &hx, &hy);
			PlotDisc(Covered, hx, hy, 1);
		}
	}

	int x0 = (RETRO_WIDTH - PageImage->width) / 2;
	unsigned char *buffer = RETRO_FrameBuffer();
	for (int row = 0; row < lines && ScrollText[page][row] != NULL; row++) {
		for (int yy = 0; yy < FONT.height; yy++) {
			const unsigned char *src = PageImage->data + ((page * lines + row) * FONT.height + yy) * PageImage->width;
			for (int xx = 0; xx < PageImage->width; xx++) {
				if (src[xx] == 0) {
					continue;
				}
				int px = x0 + xx;
				int py = top + row * LINE_PITCH + yy;
				if (px < 0 || px >= RETRO_WIDTH || py < 0 || py >= RETRO_HEIGHT) {
					continue;
				}
				if (moving && (Covered[py * RETRO_WIDTH + px] != 0) == erasing) {
					continue;
				}
				buffer[py * RETRO_WIDTH + px] = src[xx];
			}
		}
	}

	if (SHOW_BRUSH && moving) {
		PlotDisc(buffer, hx, hy, COLOR_BRUSH);
	}
}

void DEMO_Initialize(void)
{
	PageImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), &ScrollText[0][0], sizeof(ScrollText) / sizeof(ScrollText[0][0]));
	RETRO_SetPalette(PageImage->palette);
	if (SHOW_BRUSH) {
		RETRO_SetColor(COLOR_BRUSH, RETRO_YELLOW);
	}
}
