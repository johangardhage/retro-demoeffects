//
// Matrix rain
//
// Columns of mirrored glyphs streaming down a black screen, each stream a
// white head leaving a green trail that fades behind it.
//
// The screen is a grid of COLUMNS × ROWS cells, each holding a character
// and a brightness. DROPS heads fall down random columns, each at its own
// speed. Every cell a head enters is set to full brightness and given a new
// random character, and every step all cells fade,
//
//   b' = MATRIX_FADE b
//
// so a head trails a stream whose length follows its speed: a fast head
// covers more cells before the first of them fades out. A head that has
// fallen past the bottom, far enough for its trail to have gone, starts
// again above the top of a new column, a random distance up, so the streams
// do not arrive in step. MATRIX_MUTATIONS random cells a step take a new
// character, so a trail keeps flickering while it fades.
//
// A cell is CELL_WIDTH × CELL_HEIGHT pixels, the 8×8 font stretched
// vertically and mirrored left to right. Its color is the brightness on a
// ramp from black through green to pale green, and the cell under a head
// is drawn in white.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#define RETRO_WIDTH 640
#define RETRO_HEIGHT 480

#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrofont.h"
#include "lib/retropalette.h"

#define CELL_WIDTH 8
#define CELL_HEIGHT 12
#define COLUMNS (RETRO_WIDTH / CELL_WIDTH)
#define ROWS (RETRO_HEIGHT / CELL_HEIGHT)
#define DROPS (COLUMNS * 3 / 2) // heads, so some columns carry two streams
#define DROP_MINSPEED 8.0 // rows a second
#define DROP_MAXSPEED 30.0
#define MATRIX_FADE 0.965 // brightness kept per step
#define MATRIX_MUTATIONS 12 // cells a step that change character
#define MATRIX_SHADES 64 // palette entries of the trail ramp
#define MATRIX_HEAD 255 // palette entry of the head

struct Drop {
	int column;
	double row;
	double speed;
};

Drop Drops[DROPS];
unsigned char Characters[ROWS * COLUMNS];
float Brightness[ROWS * COLUMNS];

//
// A random printable character, space excluded
//
static unsigned char RandomCharacter(void)
{
	return 33 + RANDOM(126 - 33 + 1);
}

//
// Start a head above the top of a random column, up to a screen's height up
//
static void Respawn(Drop *drop)
{
	drop->column = RANDOM(COLUMNS);
	drop->row = -RAND() * ROWS;
	drop->speed = DROP_MINSPEED + RAND() * (DROP_MAXSPEED - DROP_MINSPEED);
}

//
// Draw one character in a cell, stretched to the cell and mirrored
//
static void DrawCell(unsigned char *buffer, int column, int row, unsigned char character, unsigned char color)
{
	const unsigned char *glyph = RETRO_Glyph(character);
	for (int y = 0; y < CELL_HEIGHT; y++) {
		unsigned char bits = glyph[y * 8 / CELL_HEIGHT];
		unsigned char *pixel = buffer + (row * CELL_HEIGHT + y) * RETRO_WIDTH + column * CELL_WIDTH;
		for (int x = 0; x < CELL_WIDTH; x++) {
			if (bits & (1 << (CELL_WIDTH - 1 - x))) {
				pixel[x] = color;
			}
		}
	}
}

void DEMO_FixedUpdate(double timestep)
{
	// Fade every cell
	for (float &brightness : Brightness) {
		brightness *= MATRIX_FADE;
	}

	// Move the heads, lighting every cell they enter
	for (Drop &drop : Drops) {
		int oldrow = (int)floor(drop.row);
		drop.row += drop.speed * timestep;
		for (int row = MAX(oldrow + 1, 0); row <= MIN((int)floor(drop.row), ROWS - 1); row++) {
			int i = row * COLUMNS + drop.column;
			Brightness[i] = 1.0f;
			Characters[i] = RandomCharacter();
		}
		if (drop.row >= 2 * ROWS) {
			Respawn(&drop);
		}
	}

	// Flicker a few characters
	for (int i = 0; i < MATRIX_MUTATIONS; i++) {
		Characters[RANDOM(ROWS * COLUMNS)] = RandomCharacter();
	}
}

void DEMO_Render(double time, double deltatime)
{
	unsigned char *buffer = RETRO_FrameBuffer();
	memset(buffer, 0, RETRO_WIDTH * RETRO_HEIGHT);

	// Draw the trails
	for (int row = 0; row < ROWS; row++) {
		for (int column = 0; column < COLUMNS; column++) {
			int i = row * COLUMNS + column;
			int shade = (int)(Brightness[i] * MATRIX_SHADES);
			if (shade > 0) {
				DrawCell(buffer, column, row, Characters[i], MIN(shade, MATRIX_SHADES));
			}
		}
	}

	// Draw the heads over them
	for (Drop &drop : Drops) {
		int row = (int)floor(drop.row);
		if (row >= 0 && row < ROWS) {
			int i = row * COLUMNS + drop.column;
			for (int y = 0; y < CELL_HEIGHT; y++) {
				memset(buffer + (row * CELL_HEIGHT + y) * RETRO_WIDTH + drop.column * CELL_WIDTH, 0, CELL_WIDTH);
			}
			DrawCell(buffer, drop.column, row, Characters[i], MATRIX_HEAD);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. Entry 0 is black, 1 to MATRIX_SHADES the trail from dark
	// green through green to pale green, and the head white
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_CreateGradientPalette(1, 1 + MATRIX_SHADES * 2 / 3, { 0, 24, 4 }, { 20, 190, 60 });
	RETRO_CreateGradientPalette(1 + MATRIX_SHADES * 2 / 3, 1 + MATRIX_SHADES, { 20, 190, 60 }, { 170, 255, 180 });
	RETRO_SetColor(MATRIX_HEAD, { 235, 255, 235 });

	// Init the screen and the heads, spread over their fall so the rain has
	// already started
	for (unsigned char &character : Characters) {
		character = RandomCharacter();
	}
	for (Drop &drop : Drops) {
		Respawn(&drop);
		drop.row = RAND() * 2 * ROWS - ROWS;
	}
}
