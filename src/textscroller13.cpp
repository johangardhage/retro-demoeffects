//
// Sideways Star Wars scroller: text moves up a stationary, yawed plane.
// The right side is farther away. Roll makes upward travel lean left;
// perspective makes baselines fan out above and below the vanishing point.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retromath.h"
#include "lib/retropoly.h"

#define FONT RETRO_FONT_TOPAZ30_8X8

static const char *const CrawlText[] = {
	"THE SIDEWAYS",
	"CRAWL",
	"",
	"A PAGE OF TEXT",
	"ON A PLANE",
	"TURNED AWAY",
	"TO THE RIGHT.",
	"",
	"EACH LINE RISES",
	"FROM BELOW",
	"AND LEANS LEFT",
	"AS IT CLIMBS,",
	"ITS FAR END",
	"SHRINKING",
	"TOWARD THE",
	"VANISHING POINT.",
	"",
	"WHEN THE LAST",
	"LINE IS GONE",
	"IT STARTS",
	"ALL OVER AGAIN.",
	"",
	"RETRO",
	"DEMOEFFECTS...",
};
#define LINES (int)(sizeof(CrawlText) / sizeof(CrawlText[0]))

// Font pixels to plane units; glyphs are stretched vertically before the
// plane is turned.
#define GLYPH_WIDTH 1.4f
#define GLYPH_HEIGHT 5.6f
#define LINE_SPACING 50.0
#define SCROLL_SPEED 65.0
#define CRAWL_YAW -0.95f
#define CRAWL_ROLL -0.23f
#define CAMERA_DISTANCE 300.0f
#define CX (RETRO_WIDTH * 0.28f)
#define CY (RETRO_HEIGHT * 0.50f)

// A line whose top is below ENTRY_Y, or whose bottom is above EXIT_Y, is
// offscreen at both ends, for lines up to 16 characters wide. Later lines
// follow the first, and the loop waits for the last to leave.
#define ENTRY_Y 170.0
#define EXIT_Y -145.0
#define PASS_GAP 1.5
#define LINE_HEIGHT (Font.height * GLYPH_HEIGHT)
#define PASS_CYCLE ((ENTRY_Y - EXIT_Y + LINE_HEIGHT + (LINES - 1) * LINE_SPACING) / SCROLL_SPEED + PASS_GAP)

// The ink flashes white on a beat and decays back to its base gray.
#define FLASH_PERIOD 0.95
#define FLASH_DECAY 0.18
#define INK RETRO_Palette{ 192, 187, 193 }

static RETRO_Font Font;
static RETRO_Image *TextStrip;
static mat3 CameraMatrix;
static unsigned char Background[RETRO_WIDTH * RETRO_HEIGHT];

static PolygonPoint Project(float x, float y, vec2 uv)
{
	Vertex vertex = {};
	vertex.pos = { x, y, 0 };
	RETRO_RotateVertex(&vertex, CameraMatrix);
	RETRO_ProjectVertex(&vertex, 1.0f, CX, CY, CAMERA_DISTANCE);
	return { vertex.spos, 0, uv, vertex.q };
}

void DEMO_Render(double time, double deltatime)
{
	unsigned char *screen = RETRO_FrameBuffer();
	memset(screen, 0, sizeof(Background));
	double flash = exp(-fmod(time, FLASH_PERIOD) / FLASH_DECAY);
	RETRO_SetColor(3, (unsigned char)(INK.r + (255 - INK.r) * flash),
		(unsigned char)(INK.g + (255 - INK.g) * flash),
		(unsigned char)(INK.b + (255 - INK.b) * flash));

	double firstrow = ENTRY_Y - fmod(time, PASS_CYCLE) * SCROLL_SPEED;
	RETRO_ClearDepthBuffer();
	for (int i = 0; i < LINES; i++) {
		float top = (float)(firstrow + i * LINE_SPACING);
		float bottom = top + LINE_HEIGHT;
		if (bottom < EXIT_Y || top > ENTRY_Y) continue;

		// Only the line's own ink, which the strip centers within its widest line.
		int linewidth = RETRO_TextLineWidth(Font, CrawlText[i]);
		if (linewidth == 0) continue;
		float left = (float)((TextStrip->width - linewidth) / 2);
		float right = left + linewidth;
		float center = TextStrip->width / 2.0f;
		float v = (float)(i * Font.height);
		PolygonPoint quad[4];
		quad[0] = Project((left - center) * GLYPH_WIDTH, top, { left, v });
		quad[1] = Project((right - center) * GLYPH_WIDTH, top, { right, v });
		quad[2] = Project((right - center) * GLYPH_WIDTH, bottom, { right, v + Font.height });
		quad[3] = Project((left - center) * GLYPH_WIDTH, bottom, { left, v + Font.height });
		RETRO_DrawTexMapPolygon(quad, 4, TextStrip->data, TextStrip->width, TextStrip->height, false, {});
	}

	// The lines share one plane and never overlap, so a single mask of all
	// of them is enough: the background shows wherever it holds no ink.
	for (int pixel = 0; pixel < RETRO_WIDTH * RETRO_HEIGHT; pixel++)
		if (!screen[pixel]) screen[pixel] = Background[pixel];
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(0, RETRO_Palette{ 0, 0, 0 });
	RETRO_SetColor(1, RETRO_Palette{ 68, 82, 118 });
	RETRO_SetColor(2, RETRO_Palette{ 33, 49, 84 });

	Font = RETRO_LoadFont(FONT);
	TextStrip = RETRO_GenerateTextImage(Font, CrawlText, LINES);
	// Use a two-color ink mask for nearest-neighbor texture sampling.
	for (int i = 0; i < TextStrip->width * TextStrip->height; i++)
		TextStrip->data[i] = TextStrip->data[i] != 0 ? 3 : 0;
	CameraMatrix = rotateZ(CRAWL_ROLL) * rotateY(CRAWL_YAW);

	const vec2 edge[] = { { 205, 24 }, { 233, 51 }, { 219, 66 },
		{ 247, 91 }, { 205, 133 }, { 233, 161 }, { 219, 176 },
		{ 233, 190 }, { 205, 216 } };
	for (int segment = 0; segment < 8; segment++) {
		vec2 a = edge[segment], b = edge[segment + 1];
		for (int y = (int)a.y; y < (int)b.y; y++) {
			int right = (int)(a.x + (b.x - a.x) * (y - a.y) / (b.y - a.y));
			for (int x = RETRO_WIDTH - right; x < RETRO_WIDTH; x++)
				Background[y * RETRO_WIDTH + x] = y < 133 ? 1 : 2;
		}
	}
}
