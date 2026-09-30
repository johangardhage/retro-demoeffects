//
// Text mode plasma
//
// A plasma in VGA 80×50 text mode: 640×400 pixels as 80 × 50 cells of the
// 8×8 font, each cell one character and one attribute byte,
//
//   attribute = background · 16 + foreground
//
// with both nibbles indexing the 16 text colors. The high background
// colors (8–15) are available because the attribute's top bit is read as
// background intensity, not blink.
//
// A cell can only show two colors, so the shades between them come from
// the CP437 block characters, whose ink covers a quarter, half and three
// quarters of the cell:
//
//   ' '  ░  ▒  ▓   foreground B on background A,  B covering 0, ¼, ½, ¾
//
// Chaining those steps through RAMP_COLORS gives a cyclic ramp of
// SHADES = 4 (RAMP_COLORS − 1) entries, the last color being the first.
// Each cell takes the plasma at its center, in cell units,
//
//   v = sin(0.16 x + 1.3 t) + sin(0.21 y − 0.9 t)
//     + sin(0.11 (x + y) + 0.7 t) + sin(0.19 |(x, y) − c(t)|)
//   c(t) = (40 + 30 sin 0.37 t,  25 + 18 cos 0.29 t)
//
// and v ∈ [−4, 4] picks the entry (v + 4) / 8 · SHADES · PLASMA_BANDS,
// offset by PLASMA_CYCLE t so the colors also flow through the blobs.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#define RETRO_WIDTH 640
#define RETRO_HEIGHT 400

#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrofont.h"
#include "lib/retropalette.h"

#define COLUMNS (RETRO_WIDTH / 8) // text cells across
#define ROWS (RETRO_HEIGHT / 8) // text cells down
#define RAMP_COLORS 9 // text colors the ramp passes through, the last equal to the first
#define SHADES (4 * (RAMP_COLORS - 1)) // entries in the cyclic ramp
#define PLASMA_BANDS 1.5f // times the ramp repeats across the range of v
#define PLASMA_CYCLE 6.0 // ramp entries the colors flow per second

enum { BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, LIGHTGRAY, DARKGRAY, LIGHTBLUE, LIGHTGREEN, LIGHTCYAN, LIGHTRED, LIGHTMAGENTA, YELLOW, WHITE };

static const int RampColors[RAMP_COLORS] = { BLUE, LIGHTBLUE, LIGHTCYAN, WHITE, YELLOW, LIGHTRED, RED, MAGENTA, BLUE };

// CP437 176–178 and 219 in the 8×8 VGA font, bit 0 leftmost
static const unsigned char ShadeGlyphs[4][8] = {
	{ 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88 }, // ░
	{ 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa }, // ▒
	{ 0xdb, 0xee, 0xdb, 0x77, 0xdb, 0xee, 0xdb, 0x77 }, // ▓
	{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, // █
};

struct TextCell {
	unsigned char character;
	unsigned char attribute;
};

TextCell TextBuffer[ROWS * COLUMNS];
TextCell Ramp[SHADES];

//
// The 8×8 glyph of a character, from the font or, for the block characters, the table above
//
static const unsigned char *Glyph(unsigned char character)
{
	if (character >= 176 && character <= 178) {
		return ShadeGlyphs[character - 176];
	}
	if (character == 219) {
		return ShadeGlyphs[3];
	}
	return RETRO_Glyph(character);
}

//
// Draw the text buffer: glyph bits take the foreground, the rest the background
//
static void DrawText(void)
{
	unsigned char *buffer = RETRO_FrameBuffer();
	for (int row = 0; row < ROWS; row++) {
		for (int column = 0; column < COLUMNS; column++) {
			TextCell cell = TextBuffer[row * COLUMNS + column];
			const unsigned char *glyph = Glyph(cell.character);
			unsigned char foreground = cell.attribute & 15;
			unsigned char background = cell.attribute >> 4;
			for (int y = 0; y < 8; y++) {
				unsigned char *pixel = buffer + (row * 8 + y) * RETRO_WIDTH + column * 8;
				for (int x = 0; x < 8; x++) {
					pixel[x] = glyph[y] & (1 << x) ? foreground : background;
				}
			}
		}
	}
}

void DEMO_Render(double time, double deltatime)
{
	// Calculate center of the radial term
	float t = (float)fmod(time, 1000 * 2 * M_PI);
	float cx = 40 + 30 * sinf(0.37f * t);
	float cy = 25 + 18 * cosf(0.29f * t);
	double shift = fmod(time * PLASMA_CYCLE, SHADES);

	// Generate plasma into the text buffer
	for (int row = 0; row < ROWS; row++) {
		float y = row + 0.5f;
		for (int column = 0; column < COLUMNS; column++) {
			float x = column + 0.5f;
			float v = sinf(0.16f * x + 1.3f * t) + sinf(0.21f * y - 0.9f * t)
				+ sinf(0.11f * (x + y) + 0.7f * t) + sinf(0.19f * sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy)));
			int shade = WRAP((v + 4) / 8 * SHADES * PLASMA_BANDS + shift, SHADES);
			TextBuffer[row * COLUMNS + column] = Ramp[shade];
		}
	}

	DrawText();
}

void DEMO_Initialize(void)
{
	// Init palette. Entries 0–15 are the 16 text colors.
	RETRO_CreateDefault8bitPalette();

	// Init ramp. From each color to the next: the color alone, then ░ ▒ ▓ of
	// the next one over it.
	static const unsigned char Steps[4] = { ' ', 176, 177, 178 };
	for (int i = 0; i < SHADES; i++) {
		int from = RampColors[i / 4];
		int to = RampColors[i / 4 + 1];
		Ramp[i] = { Steps[i % 4], (unsigned char)(from << 4 | to) };
	}
}
