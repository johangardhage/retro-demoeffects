//
// Elastic type
//
// A compact two-line wordmark whose letters squash and stretch independently.
// A delayed, offset copy gives the motion a soft typographic echo. Glyphs come
// from the shared 16x16 font used by the other text effects in the collection.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retrogfx.h"

#define PANEL_LEFT 42
#define PANEL_TOP 55
#define PANEL_RIGHT 277
#define PANEL_BOTTOM 184
#define FONT_WIDTH 16
#define FONT_HEIGHT 16
#define FONT_IMAGE_WIDTH 944
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

static unsigned char Font[59][FONT_HEIGHT][FONT_WIDTH];

// Draw one atlas glyph as an independently animated rectangle. The two waves
// change its width, height, and vertical position at different rates, while
// glyph.phase prevents neighboring letters from moving in lockstep. The glyph
// remains centered on its fixed wordmark position as its dimensions change.
// time may be offset to draw a delayed shadow with the same motion, and the x
// and y offsets place that shadow behind the foreground letter.
static void DrawGlyph(const Glyph &glyph, float time, int xoffset, int yoffset, unsigned char color)
{
	// Two frequencies avoid an obviously repeating sine-wave procession.
	float wave = sinf(time * 2.15f + glyph.phase);
	float cross = sinf(time * 1.31f - glyph.phase * 1.73f);
	float baseWidth = glyph.row == 0 ? 29.0f : 18.0f;
	float width = baseWidth + (glyph.row == 0 ? 9.0f : 6.0f) * wave + 3.0f * cross;
	float height = 40.0f - 12.0f * wave + 5.0f * cross;
	float cy = (glyph.row == 0 ? 101.0f : 139.0f) + 6.0f * cross;
	int left = (int)roundf(glyph.cx - width * 0.5f) + xoffset;
	int top = (int)roundf(cy - height * 0.5f) + yoffset;
	int right = (int)roundf(glyph.cx + width * 0.5f) + xoffset;
	int bottom = (int)roundf(cy + height * 0.5f) + yoffset;
	const unsigned char *pixels = &Font[glyph.letter - 32][0][0];
	unsigned char *buffer = RETRO_FrameBuffer();

	// Resample the 16x16 glyph into the animated bounds using nearest-neighbor
	// lookup. Clip to the colored panel so deformed letters cannot spill into
	// the surrounding black border.
	for (int y = MAX(top, PANEL_TOP); y <= MIN(bottom, PANEL_BOTTOM); y++) {
		int sy = CLAMP((y - top) * FONT_HEIGHT / MAX(bottom - top + 1, 1), 0, FONT_HEIGHT);
		for (int x = MAX(left, PANEL_LEFT); x <= MIN(right, PANEL_RIGHT); x++) {
			int sx = CLAMP((x - left) * FONT_WIDTH / MAX(right - left + 1, 1), 0, FONT_WIDTH);
			if (pixels[sy * FONT_WIDTH + sx] != 0) {
				buffer[y * RETRO_WIDTH + x] = color;
			}
		}
	}
}

void DEMO_Render(double deltatime)
{
	static double phase;
	phase += deltatime;

	RETRO_DrawRectangle(PANEL_LEFT, PANEL_TOP, PANEL_RIGHT, PANEL_BOTTOM, 1);

	// Draw text
	for (int i = 0; i < LETTERS; i++) DrawGlyph(Wordmark[i], phase - 0.18f, 6, 6, 2);
	for (int i = 0; i < LETTERS; i++) DrawGlyph(Wordmark[i], phase, 0, 0, 3);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_SetColor(1, RETRO_JASMINE);
	RETRO_SetColor(2, RETRO_TAN);
	RETRO_SetColor(3, RETRO_REBECCAPURPLE);

	// Init font
	RETRO_LoadImage("assets/font_16x16.pcx");
	unsigned char *atlas = RETRO_ImageData();
	for (int character = 0; character < 59; character++) {
		for (int y = 0; y < FONT_HEIGHT; y++) {
			memcpy(Font[character][y], atlas + y * FONT_IMAGE_WIDTH + character * FONT_WIDTH, FONT_WIDTH);
		}
	}
}
