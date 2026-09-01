//
// Dweezil zoomer
//
// A block-based feedback rotozoomer, after the MS-DOS effect by Dweezil.
//
// Every step rebuilds the buffer from itself, one 16x16 tile at a time, and a
// tile copies a whole block from somewhere near where it already sits. The
// offset is affine in the tile index rather than in the pixel, so for tile
// (i, j) centred as (u, v):
//
//   source = tile origin + (z*u - r*v, z*v + r*u)
//
// which is the linear part of a rotozoom - a scale by z, a rotation by r -
// evaluated once per tile instead of once per pixel. Sampling the previous step
// through it applies the transform again and again, dragging a fleck seeded at
// the centre outwards and spinning it further every step. Copying whole blocks
// is what gives the look: a mosaic that shears at every tile seam, not a smooth
// warp. z and r are whole pixels per tile, so both are -1, 0 or 1, which at 16
// pixels per tile is a scale of 1 +- 1/16 and about 3.6 degrees a step.
//
// Random color is seeded into a 9x9 patch at the centre, the one point the
// transform leaves standing, and the fire palette turns the drift outwards into
// a cooling curve.
//
// Each step also picks a shift s in [0, 16), samples every tile s pixels further
// along both axes, then writes the mosaic back s pixels down and right. Those
// cancel, so the picture does not drift. What is left is that the tile seams no
// longer fall on a fixed grid, so the mosaic edges dance instead of standing
// still.
//
// Controls:
//   R - cycle rotation through -1, 0 and 1 pixels per tile
//   Z - cycle zoom through -1, 0 and 1 pixels per tile
//   S - toggle the random shift
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define PIECE_SIZE 16
#define PIECES_X 23
#define PIECES_Y 17
#define BUFFER_WIDTH (PIECE_SIZE * PIECES_X)
#define BUFFER_HEIGHT (PIECE_SIZE * PIECES_Y)
#define BUFFER_SIZE (BUFFER_WIDTH * BUFFER_HEIGHT)

// The screen is cut out of the middle of the buffer with a whole tile of margin
// on every side. The margin absorbs the edges: the rows and columns the shifted
// write-back leaves standing, and the smear where an out-of-range sample clamps.
#define SOURCE_X ((BUFFER_WIDTH - RETRO_WIDTH) / 2)
#define SOURCE_Y ((BUFFER_HEIGHT - RETRO_HEIGHT) / 2)
static_assert(BUFFER_WIDTH >= RETRO_WIDTH + 2 * PIECE_SIZE, "The buffer needs a tile of margin on each side");
static_assert(BUFFER_HEIGHT >= RETRO_HEIGHT + 2 * PIECE_SIZE, "The buffer needs a tile of margin on each side");

unsigned char FrameBuffer[BUFFER_SIZE];
unsigned char TileBuffer[BUFFER_SIZE];
static int rotation = 1;
static int zoom = -1;
static bool doshift = true;

//
// Copy one 16x16 block out of the previous step into the mosaic being built
//
// A sample outside the buffer clamps to its edge, per axis. The original wrapped
// instead, over the buffer as one flat array, so a block reaching past the right
// edge continued at the left of the next row down - which carried a strip of the
// picture clear across the screen. That stayed out of sight behind its 192x184
// crop and does not here, so the edges smear rather than teleport.
//
static void CopyTile(int sourcex, int sourcey, int destx, int desty)
{
	for (int y = 0; y < PIECE_SIZE; y++) {
		unsigned char *source = FrameBuffer + CLAMP(sourcey + y, 0, BUFFER_HEIGHT) * BUFFER_WIDTH;
		unsigned char *dest = TileBuffer + (desty + y) * BUFFER_WIDTH + destx;

		for (int x = 0; x < PIECE_SIZE; x++) {
			dest[x] = source[CLAMP(sourcex + x, 0, BUFFER_WIDTH)];
		}
	}
}

void DEMO_FixedUpdate(double timestep)
{
	// Pick this step's sub-tile shift
	int shift = doshift ? RANDOM(PIECE_SIZE) : 0;

	// Seed random color at the centre, the one point the transform leaves standing
	for (int y = 0; y <= PIECE_SIZE / 2; y++) {
		for (int x = 0; x <= PIECE_SIZE / 2; x++) {
			FrameBuffer[(BUFFER_HEIGHT / 2 + y) * BUFFER_WIDTH + BUFFER_WIDTH / 2 + x] = RANDOM(RETRO_COLORS);
		}
	}

	// Rebuild the mosaic, one tile at a time, from the previous step
	for (int tiley = 0; tiley < PIECES_Y; tiley++) {
		for (int tilex = 0; tilex < PIECES_X; tilex++) {
			int u = tilex - PIECES_X / 2;
			int v = tiley - PIECES_Y / 2;
			int sourcex = tilex * PIECE_SIZE + shift + u * zoom - v * rotation;
			int sourcey = tiley * PIECE_SIZE + shift + v * zoom + u * rotation;

			CopyTile(sourcex, sourcey, tilex * PIECE_SIZE, tiley * PIECE_SIZE);
		}
	}

	// Write the mosaic back shifted, cancelling the shift the sampling added. Row by
	// row, so a row that runs off the right edge is dropped instead of continuing at
	// the left of the next one. The top and left the shift vacates keep the previous
	// step, which the margin hides.
	for (int y = shift; y < BUFFER_HEIGHT; y++) {
		memcpy(FrameBuffer + y * BUFFER_WIDTH + shift,
			TileBuffer + (y - shift) * BUFFER_WIDTH,
			BUFFER_WIDTH - shift);
	}
}

void DEMO_Render(double time, double deltatime)
{
	if (RETRO_KeyPressed(SDL_SCANCODE_R)) {
		rotation = rotation < 1 ? rotation + 1 : -1;
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_Z)) {
		zoom = zoom < 1 ? zoom + 1 : -1;
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_S)) {
		doshift = !doshift;
	}

	// Draw the screen-sized cutout from the middle of the buffer
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		memcpy(RETRO_FrameBuffer() + y * RETRO_WIDTH,
			FrameBuffer + (SOURCE_Y + y) * BUFFER_WIDTH + SOURCE_X,
			RETRO_WIDTH);
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, 8, RETRO_BLACK, RETRO_BLUEBLACK);
	RETRO_CreateGradientPalette(8, 24, RETRO_BLUEBLACK, RETRO_DARKRED);
	RETRO_CreateGradientPalette(24, 56, RETRO_DARKRED, RETRO_SCARLET);
	RETRO_CreateGradientPalette(56, 128, RETRO_SCARLET, RETRO_AMBER);
	RETRO_CreateGradientPalette(128, 192, RETRO_AMBER, RETRO_YELLOW);
	RETRO_CreateGradientPalette(192, RETRO_COLORS, RETRO_YELLOW, RETRO_WHITE);
}
