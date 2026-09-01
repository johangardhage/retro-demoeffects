//
// Tile wipe
//
// The picture dealt out a tile at a time. The screen is a grid of TILES_X by
// TILES_Y tiles of TILE_SIZE pixels, and tile k opens as a box grown from its
// own centre over one window of the pass:
//
//   start_k = (rank_k / (TILES - 1)) (1 - WINDOW)
//   u_k     = CLAMP01((p - start_k) / WINDOW)
//   half_k  = round(u_k TILE_SIZE / 2)
//
// p runs from 0 to 1 across the pass and WINDOW is the fraction of it one tile
// spends opening. The last rank is TILES - 1, so dividing by that spreads the
// starts over the whole deal, and scaling by (1 - WINDOW) leaves room for one
// window at the end: the tile dealt first starts at p = 0 and the tile dealt
// last starts at 1 - WINDOW and finishes opening exactly as the pass does.
// Overlapping the windows is what keeps this from looking like a counter:
// several tiles are always part open.
//
// p is also run backwards, and then the same formula closes the tiles in the
// reverse of the order it opened them, no second pass needed.
//
// rank is the order the tiles are dealt in, and Tab swaps it. A shuffle deals
// a random permutation, from a Fisher-Yates pass over the identity: at step i
// it swaps entry i with a uniform pick from [i, TILES), so every permutation
// is equally likely. A checkerboard deals every tile of one parity, (tx + ty)
// even, before any tile of the other, and the picture arrives twice over.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define TILE_SIZE 16 // pixels on a side, even, so a box grows symmetrically
#define TILES_X (RETRO_WIDTH / TILE_SIZE)
#define TILES_Y (RETRO_HEIGHT / TILE_SIZE)
#define TILES (TILES_X * TILES_Y)
#define TILE_WINDOW 0.25 // fraction of the pass one tile spends opening
#define TIME_WIPE 2.5 // seconds a pass takes
#define TIME_HOLD 0.75 // seconds held on the whole picture or on the curtain
#define TIME_CYCLE (2 * (TIME_HOLD + TIME_WIPE))
#define CURTAIN 0 // palette entry an unopened tile leaves behind

enum WipeMode { WIPE_SHUFFLE, WIPE_CHECKERBOARD, WIPE_MODES };

WipeMode Mode = WIPE_SHUFFLE;
int Rank[WIPE_MODES][TILES];

//
// How much of the picture has been dealt at this point of the cycle
//
double Progress(double phase)
{
	if (phase < TIME_HOLD) {
		return 1;
	}
	if (phase < TIME_HOLD + TIME_WIPE) {
		return 1 - (phase - TIME_HOLD) / TIME_WIPE;
	}
	if (phase < 2 * TIME_HOLD + TIME_WIPE) {
		return 0;
	}
	return (phase - 2 * TIME_HOLD - TIME_WIPE) / TIME_WIPE;
}

void DEMO_Render(double time, double deltatime)
{
	if (RETRO_KeyPressed(SDL_SCANCODE_TAB)) {
		Mode = Mode == WIPE_SHUFFLE ? WIPE_CHECKERBOARD : WIPE_SHUFFLE;
	}

	// Calculate phase
	double phase = fmod(time, TIME_CYCLE);
	double progress = Progress(phase);

	unsigned char *image = RETRO_ImageData();
	unsigned char *buffer = RETRO_FrameBuffer();

	// Draw the open box of every tile
	for (int tile = 0; tile < TILES; tile++) {
		double start = (double)Rank[Mode][tile] / (TILES - 1) * (1 - TILE_WINDOW);
		double opening = CLAMP01((progress - start) / TILE_WINDOW);
		int half = lround(opening * TILE_SIZE / 2);

		if (half == 0) {
			continue;
		}

		int centerx = (tile % TILES_X) * TILE_SIZE + TILE_SIZE / 2;
		int centery = (tile / TILES_X) * TILE_SIZE + TILE_SIZE / 2;

		for (int y = centery - half; y < centery + half; y++) {
			int offset = y * RETRO_WIDTH + centerx - half;
			memcpy(buffer + offset, image + offset, 2 * half);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_Image *image = RETRO_LoadImage("assets/flag_320x240.pcx", true);
	if (image->width != RETRO_WIDTH || image->height != RETRO_HEIGHT) {
		RETRO_RageQuit("The image must be the size of the screen\n");
	}

	// One entry is taken back for the curtain, which is what a cleared
	// framebuffer holds. The picture keeps the entry as one of its own colors,
	// so wherever it uses it those pixels are black inside an open tile too.
	RETRO_SetColor(CURTAIN, RETRO_BLACK);

	// Deal the tiles of one parity before the other
	int rank = 0;
	for (int parity = 0; parity < 2; parity++) {
		for (int tile = 0; tile < TILES; tile++) {
			if ((tile % TILES_X + tile / TILES_X) % 2 == parity) {
				Rank[WIPE_CHECKERBOARD][tile] = rank++;
			}
		}
	}

	// Deal the tiles in a random order. Fisher-Yates shuffles the identity in
	// place, taking entry i to a uniform pick from the entries [i, TILES) that
	// have not been settled yet
	for (int tile = 0; tile < TILES; tile++) {
		Rank[WIPE_SHUFFLE][tile] = tile;
	}
	for (int tile = 0; tile < TILES - 1; tile++) {
		int pick = tile + RANDOM(TILES - tile);
		SWAP(Rank[WIPE_SHUFFLE][tile], Rank[WIPE_SHUFFLE][pick]);
	}
}
