//
// Text mode fire
//
// A fire in VGA 80×50 text mode: 640×400 pixels as 80 × 50 cells of the
// 8×8 font, each cell one character and one attribute byte,
//
//   attribute = background · 16 + foreground
//
// with both nibbles indexing the 16 text colors. The high background
// colors (8–15) are available because the attribute's top bit is read as
// background intensity, not blink.
//
// The heat field is one byte per cell and runs FIRE_HIDDEN rows below the
// screen, the bottom FIRE_BED of them fuel. A fuel cell is a coal, lit at
// 255 or out at 0. Each step a column of coals is relit with probability
// FIRE_FLICKER, lit with probability FIRE_FUEL, so a coal holds for about
// 1 / FIRE_FLICKER steps and throws a tongue of flame up while it burns.
// Every cell above takes the mean of the three cells below it and the one two
// rows below, less FIRE_DECAY:
//
//   T'(x, y) = max(0, (T(x−1, y+1) + T(x, y+1) + T(x+1, y+1) + T(x, y+2)) / 4
//                     − FIRE_DECAY)
//
// Taps past the side edges read 0, so the flame cools toward them. The
// hidden rows between the bed and the screen average the fuel's noise
// away before it shows. The step runs at the simulation rate, one row of
// rise per step.
//
// A cell can only show two colors, so the shades between them come from
// the CP437 block characters, whose ink covers a quarter, half and three
// quarters of the cell:
//
//   ' '  ░  ▒  ▓   foreground B on background A,  B covering 0, ¼, ½, ¾
//
// Chaining those steps from black through red, light red and yellow to
// white gives SHADES = 4 (RAMP_COLORS − 1) + 1 entries, the last being
// solid white, and the heat indexes it as T · SHADES / 256.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#define RETRO_WIDTH 640
#define RETRO_HEIGHT 400
#define RETRO_SIMULATION_STEP (1.0 / 30.0)

#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrofont.h"
#include "lib/retropalette.h"

#define COLUMNS (RETRO_WIDTH / 8) // text cells across
#define ROWS (RETRO_HEIGHT / 8) // text cells down
#define RAMP_COLORS 5 // text colors the ramp passes through
#define SHADES (4 * (RAMP_COLORS - 1) + 1) // entries in the ramp
#define FIRE_HIDDEN 5 // simulated rows below the screen
#define FIRE_BED 2 // of those, the rows of fuel at the bottom
#define FIRE_FUEL 0.6 // probability that a relit coal burns
#define FIRE_FLICKER 0.15 // probability per step that a column of coals is relit
#define FIRE_DECAY 4 // subtracted after the average, so how fast a flame dies as it rises

enum { BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, LIGHTGRAY, DARKGRAY, LIGHTBLUE, LIGHTGREEN, LIGHTCYAN, LIGHTRED, LIGHTMAGENTA, YELLOW, WHITE };

static const int RampColors[RAMP_COLORS] = { BLACK, RED, LIGHTRED, YELLOW, WHITE };

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

static TextCell TextBuffer[ROWS * COLUMNS];
static TextCell Ramp[SHADES];
static unsigned char Heat[(ROWS + FIRE_HIDDEN) * COLUMNS];

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

//
// Heat at a cell, 0 past the side edges and below the bed
//
static int HeatAt(int column, int row)
{
	if (column < 0 || column >= COLUMNS || row >= ROWS + FIRE_HIDDEN) {
		return 0;
	}
	return Heat[row * COLUMNS + column];
}

void DEMO_FixedUpdate(double timestep)
{
	// Relight coals
	for (int column = 0; column < COLUMNS; column++) {
		if (RAND() < FIRE_FLICKER) {
			unsigned char coal = RAND() < FIRE_FUEL ? 255 : 0;
			for (int row = ROWS + FIRE_HIDDEN - FIRE_BED; row < ROWS + FIRE_HIDDEN; row++) {
				Heat[row * COLUMNS + column] = coal;
			}
		}
	}

	// Rise and cool, top down so every tap still reads the previous step
	for (int row = 0; row < ROWS + FIRE_HIDDEN - FIRE_BED; row++) {
		for (int column = 0; column < COLUMNS; column++) {
			int sum = HeatAt(column - 1, row + 1) + HeatAt(column, row + 1) + HeatAt(column + 1, row + 1) + HeatAt(column, row + 2);
			Heat[row * COLUMNS + column] = MAX(sum / 4 - FIRE_DECAY, 0);
		}
	}
}

void DEMO_Render(double time, double deltatime)
{
	// Shade the heat into the text buffer
	for (int i = 0; i < ROWS * COLUMNS; i++) {
		TextBuffer[i] = Ramp[Heat[i] * SHADES / 256];
	}

	DrawText();
}

void DEMO_Initialize(void)
{
	// Init palette. Entries 0–15 are the 16 text colors.
	RETRO_CreateDefault8bitPalette();

	// Init ramp. From each color to the next: the color alone, then ░ ▒ ▓ of
	// the next one over it, and last the final color solid.
	static const unsigned char steps[4] = { ' ', 176, 177, 178 };
	for (int i = 0; i < SHADES - 1; i++) {
		int from = RampColors[i / 4];
		int to = RampColors[i / 4 + 1];
		Ramp[i] = { steps[i % 4], (unsigned char)(from << 4 | to) };
	}
	Ramp[SHADES - 1] = { 219, (unsigned char)(BLACK << 4 | RampColors[RAMP_COLORS - 1]) };
}
