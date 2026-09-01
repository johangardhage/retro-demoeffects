//
// Elastic type
//
// A compact two-line wordmark whose letters squash and stretch independently.
// A delayed, offset copy gives the motion a soft typographic echo.
//
// Unlike the strip scrollers, no RETRO_GenerateTextImage page is built: each
// glyph is read straight out of RETRO_LoadFont's atlas and resampled into its
// own rectangle, because a letter's bounds are its own,
//
//   wave   = sin(2.15 t + φ)
//   cross  = sin(1.31 t − 1.73 φ)
//   width  = BASE + WAMP · wave + 3 · cross
//   height = 40 − 12 · wave + 5 · cross
//   left   = cx − width / 2
//   top    = cy(row, cross) − height / 2
//   sx     = (x − left) · fontwidth / rectwidth
//   sy     = (y − top) · fontheight / rectheight
//
// Two frequencies keep the squash from marching in lockstep, and φ is per
// letter. The glyph stays centred on its fixed wordmark position as the
// rectangle changes. A zero texel is transparent.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retrogfx.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8

#define PANEL_LEFT 42
#define PANEL_TOP 55
#define PANEL_RIGHT 277
#define PANEL_BOTTOM 184
#define LETTERS 16

struct Glyph {
	char letter;
	int row;
	float cx;
	float phase;
};

static const Glyph Wordmark[LETTERS] = {
	{ 'R', 0,  98, 0.1f }, { 'E', 0, 129, 1.7f }, { 'T', 0, 160, 3.4f }, { 'R', 0, 191, 5.0f }, { 'O', 0, 222, 4.2f },
	{ 'D', 1,  65, 2.5f }, { 'E', 1,  84, 0.8f }, { 'M', 1, 103, 5.5f }, { 'O', 1, 122, 3.7f },
	{ 'E', 1, 141, 1.9f }, { 'F', 1, 160, 0.4f }, { 'F', 1, 179, 2.2f }, { 'E', 1, 198, 4.0f }, { 'C', 1, 217, 5.8f }, { 'T', 1, 236, 3.1f }, { 'S', 1, 255, 1.3f },
};

static RETRO_Font Font;

// Draw one atlas glyph as an independently animated rectangle. time may be
// offset to draw a delayed shadow with the same motion, and the x and y
// offsets place that shadow behind the foreground letter.
static void DrawGlyph(const Glyph &glyph, float time, int xoffset, int yoffset, unsigned char color)
{
	unsigned char code = (unsigned char)glyph.letter;
	int cell = code - Font.firstcharacter;
	int sourcex = cell * Font.width;
	if (cell < 0 || sourcex + Font.width > Font.atlas->width) {
		return;
	}

	float wave = sinf(time * 2.15f + glyph.phase);
	float cross = sinf(time * 1.31f - glyph.phase * 1.73f);
	float basewidth = glyph.row == 0 ? 29.0f : 18.0f;
	float width = basewidth + (glyph.row == 0 ? 9.0f : 6.0f) * wave + 3.0f * cross;
	float height = 40.0f - 12.0f * wave + 5.0f * cross;
	float cy = (glyph.row == 0 ? 101.0f : 139.0f) + 6.0f * cross;
	int left = (int)roundf(glyph.cx - width * 0.5f) + xoffset;
	int top = (int)roundf(cy - height * 0.5f) + yoffset;
	int right = (int)roundf(glyph.cx + width * 0.5f) + xoffset;
	int bottom = (int)roundf(cy + height * 0.5f) + yoffset;
	int rectwidth = MAX(right - left + 1, 1);
	int rectheight = MAX(bottom - top + 1, 1);
	unsigned char *buffer = RETRO_FrameBuffer();

	// Nearest-neighbour into the animated bounds. Clip to the coloured panel
	// so deformed letters cannot spill into the surrounding black border.
	for (int y = MAX(top, PANEL_TOP); y <= MIN(bottom, PANEL_BOTTOM); y++) {
		int sy = CLAMP((y - top) * Font.height / rectheight, 0, Font.height - 1);
		for (int x = MAX(left, PANEL_LEFT); x <= MIN(right, PANEL_RIGHT); x++) {
			int sx = CLAMP((x - left) * Font.width / rectwidth, 0, Font.width - 1);
			if (Font.atlas->data[sy * Font.atlas->width + sourcex + sx] != 0) {
				buffer[y * RETRO_WIDTH + x] = color;
			}
		}
	}
}

void DEMO_Render(double time, double deltatime)
{
	RETRO_DrawRectangle(PANEL_LEFT, PANEL_TOP, PANEL_RIGHT, PANEL_BOTTOM, 1);

	// Draw text
	for (int i = 0; i < LETTERS; i++) DrawGlyph(Wordmark[i], time - 0.18f, 6, 6, 2);
	for (int i = 0; i < LETTERS; i++) DrawGlyph(Wordmark[i], time, 0, 0, 3);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_SetColor(1, RETRO_JASMINE);
	RETRO_SetColor(2, RETRO_TAN);
	RETRO_SetColor(3, RETRO_REBECCAPURPLE);

	Font = RETRO_LoadFont(FONT);
}
