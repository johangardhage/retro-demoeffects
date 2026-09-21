//
// Receding text crawl on a tilted plane. A generated text strip supplies
// one perspective-correct quad per line; distance fades it to black.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retropoly.h"
#include "lib/retropalette.h"
#include "lib/retromath.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }

static const char *const CrawlText[] = {
	"THE TEXT CRAWL",
	"",
	"A PAGE OF TEXT",
	"LIES ON A PLANE",
	"TILTED AWAY",
	"FROM THE VIEWER.",
	"",
	"EACH LINE RISES",
	"FROM BELOW",
	"THE SCREEN,",
	"SHRINKS AS IT",
	"RECEDES",
	"AND FADES",
	"INTO THE DARK.",
	"",
	"WHEN THE LAST LINE",
	"IS GONE",
	"IT STARTS",
	"ALL OVER AGAIN.",
	"",
	"RETRO",
	"DEMOEFFECTS...",
};
#define LINES (int)(sizeof(CrawlText) / sizeof(CrawlText[0]))

// Lines enter fully below the screen, recede, then fade out.
#define LINE_SPACING 34.0
#define ZNEAR 34.0
#define ZSPAN 500.0
#define ZFAR (ZNEAR + ZSPAN)
#define SCROLL_SPEED 50.0
#define PASS_GAP 2.0
#define PASS_CYCLE ((ZSPAN + (LINES - 1) * LINE_SPACING) / SCROLL_SPEED + PASS_GAP)

#define CRAWL_TILT 1.05
#define CAMERA_DISTANCE 320.0
#define CY (float)(RETRO_HEIGHT + 36)
#define SHADES 24

static RETRO_Font Font;
static RETRO_Image *TextStrip;
static mat3 CameraMatrix;
static unsigned char InkShades[2 * SHADES];
static ShadeTable TextShades = { InkShades, 2, SHADES };

// Full ink at ZNEAR, black from ZFAR on.
static float Shade(double depth)
{
	return (float)MAX((1.0 - (depth - ZNEAR) / ZSPAN) * (SHADES - 1), 0.0);
}

static PolygonPoint Project(float x, float z, vec2 uv, float shade)
{
	Vertex vertex = {};
	vertex.pos = { x, 0, z };
	RETRO_RotateVertex(&vertex, CameraMatrix);
	RETRO_ProjectVertex(&vertex, 1.0f, RETRO_WIDTH / 2.0f, CY, CAMERA_DISTANCE);
	return { vertex.spos, shade, uv, vertex.q };
}

void DEMO_Render(double time, double deltatime)
{
	double phase = fmod(time, PASS_CYCLE) * SCROLL_SPEED;
	RETRO_ClearDepthBuffer();
	for (int i = 0; i < LINES; i++) {
		double depth = ZNEAR + phase - i * LINE_SPACING;
		// The glyph top lies farther along the ground than its bottom.
		float top = (float)(depth + Font.height / 2.0);
		float bottom = (float)(depth - Font.height / 2.0);
		if (depth < ZNEAR || bottom >= ZFAR) continue;

		// Only the line's own ink, which the strip centers within its widest line.
		int linewidth = RETRO_TextLineWidth(Font, CrawlText[i]);
		if (linewidth == 0) continue;
		float left = (float)((TextStrip->width - linewidth) / 2);
		float right = left + linewidth;
		float center = TextStrip->width / 2.0f;
		float v = (float)(i * Font.height);
		PolygonPoint quad[4] = {
			Project(left - center, top, { left, v }, Shade(top)),
			Project(right - center, top, { right, v }, Shade(top)),
			Project(right - center, bottom, { right, v + Font.height }, Shade(bottom)),
			Project(left - center, bottom, { left, v + Font.height }, Shade(bottom))
		};
		RETRO_DrawTexMapGouraudPolygon(quad, 4, TextStrip->data,
			TextStrip->width, TextStrip->height, TextShades);
	}
}

void DEMO_Initialize(void)
{
	// The ramp starts at black so the farthest lines fade out rather than pop.
	for (int i = 0; i < SHADES; i++) {
		double t = (double)i / (SHADES - 1);
		RETRO_SetColor(i, RETRO_Palette{ (unsigned char)(t * 60), (unsigned char)(t * 80), (unsigned char)(t * 255) });
		InkShades[SHADES + i] = (unsigned char)i;
	}

	// Zero texels stay black; ink texels use the line's distance shade.
	Font = RETRO_LoadFont(FONT);
	TextStrip = RETRO_GenerateTextImage(Font, CrawlText, LINES);
	for (int i = 0; i < TextStrip->width * TextStrip->height; i++)
		TextStrip->data[i] = TextStrip->data[i] != 0;
	CameraMatrix = rotateX((float)CRAWL_TILT);
}
