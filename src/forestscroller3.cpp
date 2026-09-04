//
// Forest scroller
//
// A banner painted onto a misty forest as background plus font, pixel by
// pixel, and nothing more - no blending, no persistent glow, no decay. Each
// touched screen address is simply overwritten with Background[addr] plus
// whatever the font currently shades there, so a pixel always shows the
// sum of exactly two things: the static picture underneath, and this
// instant's font value at that spot. Font's own two possible values (a
// stroke or 0) select which of two neighbouring ranges of the background
// asset's own palette apply - see FONT_BOOST.
//
// Every fixed tick draws one of three scatter maps (see FOREST_SCATTER
// below) in turn - three interlaced thirds of the banner's dots, refreshed
// one third per tick, so any one instant is a little uneven but a full
// three-tick round is not. A map holds, for each of a CELL_ROWS x CELL_COLS
// grid, the screen addresses to touch there; when its count is 0 nothing
// in the picture answers to that cell, so it is skipped. How each hit is
// shaded comes from the text band, a strip far wider than the grid: cell
// (row, col) samples the text band at column (col + window - CELL_XOFFSET),
// where window counts one full three-tick round as a single step, so as it
// climbs the sampled window slides across the strip and steadily
// different lettering feeds the same fixed scatter shapes. CELL_XOFFSET is
// the lead that window starts with, so a lap opens on blank cells and only
// fills in as the window catches up. A cell whose sample is blank is still
// touched, with a font value of 0, which is what erases a stroke once the
// window has scrolled past it - skip that and old letters would stay lit
// under new ones forever, which is exactly the smear an earlier version of
// this file had. BAND_CYCLE - the strip's width plus that same lead-in,
// now serving as the gap between laps - is what window wraps on, looping
// the banner forever instead of running through it once.
//
// The scatter maps, background, and text band use the original data.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#define RETRO_HEIGHT 200

#include "lib/retro.h"
#include "lib/retromain.h"

#define FOREST_BACKGROUND "assets/forest_320x200.pcx"
#define FOREST_BAND "assets/forest_band_640x32.pcx"
#define FOREST_SCATTER { "assets/forest_scatter1.dat", "assets/forest_scatter2.dat", "assets/forest_scatter3.dat" }

#define CELL_ROWS 31
#define CELL_COLS 237
#define CELL_XOFFSET 104 // the generated text strip column the grid's rightmost column starts sampling at
#define BAND_CYCLE (Band->width + CELL_XOFFSET) // one lap of the window, band plus its lead-in gap
#define SCATTER_SETS 3
#define TICKS_PER_WINDOW SCATTER_SETS // fixed ticks a full round through the three maps takes, so the window advances one column per round
#define FONT_BOOST 128 // nonzero font pixels select the background palette's glow range

#define MAX_SCATTER_POINTS 8192

unsigned short ScatterAddr[SCATTER_SETS][MAX_SCATTER_POINTS];
int ScatterOffset[SCATTER_SETS][CELL_ROWS * CELL_COLS];
int ScatterCount[SCATTER_SETS][CELL_ROWS * CELL_COLS];

unsigned char *Background;
RETRO_Image *Band;

// A scatter map is, cell-major over CELL_ROWS x CELL_COLS cells, a u16 point
// count followed by that many u16 screen addresses. Loading it fills addr as
// one flat list plus, per cell, the offset into it - so a cell's points are
// addr[offset .. offset + count). RageQuits if a map holds more points than
// addr has room for
static void LoadScatterMap(const char *filename, unsigned short *addr, int *offset, int *count)
{
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
		RETRO_RageQuit("Cannot open file: %s\n", filename);
	}

	int total = 0;
	for (int cell = 0; cell < CELL_ROWS * CELL_COLS; cell++) {
		unsigned short n;
		if (fread(&n, sizeof(n), 1, fp) != 1) {
			RETRO_RageQuit("Cannot read scatter cell count: %s\n", filename);
		}
		if (total + n > MAX_SCATTER_POINTS) {
			RETRO_RageQuit("Scatter map has more points than the point list: %s\n", filename);
		}
		if (n > 0 && fread(&addr[total], sizeof(unsigned short), n, fp) != n) {
			RETRO_RageQuit("Cannot read scatter cell addresses: %s\n", filename);
		}

		offset[cell] = total;
		count[cell] = n;
		total += n;
	}

	fclose(fp);
}

void DEMO_FixedUpdate(double timestep)
{
	static int tick = 0;

	unsigned char *buffer = RETRO_FrameBuffer();

	int ishimmer = tick % SCATTER_SETS;
	int window = tick / TICKS_PER_WINDOW;

	for (int row = 0; row < CELL_ROWS; row++) {
		for (int col = 0; col < CELL_COLS; col++) {
			int cell = row * CELL_COLS + col;
			int count = ScatterCount[ishimmer][cell];
			if (count == 0) {
				continue;
			}

			int strip = WRAP(col + window, BAND_CYCLE);
			unsigned char fontvalue = 0;
			if (strip >= CELL_XOFFSET) {
				unsigned char shading = Band->data[(row * Band->height / CELL_ROWS) * Band->width + (strip - CELL_XOFFSET)];
				if (shading) {
					fontvalue = FONT_BOOST + shading;
				}
			}

			int offset = ScatterOffset[ishimmer][cell];
			for (int i = 0; i < count; i++) {
				unsigned short addr = ScatterAddr[ishimmer][offset + i];
				buffer[addr] = Background[addr] + fontvalue;
			}
		}
	}

	tick = WRAP(tick + 1, BAND_CYCLE * TICKS_PER_WINDOW);
}

void DEMO_Initialize(void)
{
	RETRO_Image *background = RETRO_LoadImage(FOREST_BACKGROUND, true);
	Background = background->data;
	RETRO_Blit(Background);

	Band = RETRO_LoadImage(FOREST_BAND);

	const char *scatterfiles[SCATTER_SETS] = FOREST_SCATTER;
	for (int set = 0; set < SCATTER_SETS; set++) {
		LoadScatterMap(scatterfiles[set], ScatterAddr[set], ScatterOffset[set], ScatterCount[set]);
	}
}
