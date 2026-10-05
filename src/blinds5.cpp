//
// Blinds 5
//
// A checkerboard flip between two pictures. The screen is cut into square
// tiles, each carrying a piece of one picture on its front and the same piece
// of the other picture on its back. Each tile turns half a revolution around
// an axis through its center: its front is squeezed toward a line, and its
// back opens out from that line in its place, with the black curtain showing
// in the gaps while the tile is turned. The axis alternates like the squares of
// a checkerboard: a tile with (tx + ty) even turns around its vertical axis and
// is squeezed sideways, its neighbors turn around their horizontal axis and
// are squeezed upwards and downwards.
//
// Each tile starts its turn a little later than the ones above it and to its
// left - the delay is a linear ramp over tx + ty - so the flip sweeps
// diagonally from the top left corner instead of turning every tile at once.
// The finished transition rests on the second picture for a moment, then runs
// backwards: the tiles turn back to the first picture, the sweep returning to
// the corner it came from, and the cycle starts over.
//
// A tile is darkened as it turns away. The two pictures share one palette, so
// the darker shades of a color are the nearest entries that palette already
// has, looked up in a shade table.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retroshadetable.h"

#define TILE_SIZE 20
#define TILES_X (RETRO_WIDTH / TILE_SIZE)
#define TILES_Y (RETRO_HEIGHT / TILE_SIZE)
#define PHASE_WINDOW 0.5 // part of the transition spent rotating one tile
#define TIME_TRANSITION 2.0
#define TIME_HOLD 1.0 // seconds held on either picture
#define TIME_CYCLE (2 * (TIME_HOLD + TIME_TRANSITION))
#define CURTAIN 0
#define SHADES 16
#define AMBIENT 0.25f // light left on a tile that is nearly edge-on

static RETRO_Image *PictureA;
static RETRO_Image *PictureB;
static unsigned char ShadeTable[RETRO_COLORS][SHADES];

void DEMO_Render(RETRO_Time time)
{
	// Calculate phase: it rises to 1 as the tiles turn over, stays there over
	// the second picture, and falls back to 0 over the last TIME_TRANSITION of
	// the cycle
	double cycle = fmod(time.total, TIME_CYCLE);
	double phase = CLAMP01(MIN(cycle - TIME_HOLD, TIME_CYCLE - cycle) / TIME_TRANSITION);

	unsigned char *buffer = RETRO_FrameBuffer();

	// RETRO clears the framebuffer before this callback, leaving the curtain.
	for (int ty = 0; ty < TILES_Y; ty++) {
		for (int tx = 0; tx < TILES_X; tx++) {
			int left = tx * TILE_SIZE;
			int top = ty * TILE_SIZE;

			// The rotation starts in the top left corner and travels along the
			// diagonals. Each tile turns through 180 degrees: front broadside,
			// edge-on, back broadside.
			double delay = (double)(tx + ty) / (TILES_X + TILES_Y - 2)
				* (1 - PHASE_WINDOW);
			double rotation = CLAMP01((phase - delay) / PHASE_WINDOW);
			double facing = cos(rotation * M_PI);
			double fold = fabs(facing);

			// Project the rotating tile about its center: the visible span is
			// the size foreshortened across the axis and whole along it, laid
			// out centered on the tile's own center.
			int visible = lround(TILE_SIZE * fold);
			bool upright = (tx + ty) % 2 == 0;
			int spanx = upright ? visible : TILE_SIZE;
			int spany = upright ? TILE_SIZE : visible;
			int firstx = left + (TILE_SIZE - spanx) / 2;
			int firsty = top + (TILE_SIZE - spany) / 2;
			int shade = lround(fold * (SHADES - 1));

			// The back carries its piece the way it reads once the tile has
			// turned over, so both sides are sampled in the same direction.
			unsigned char *image = facing < 0 ? PictureB->data : PictureA->data;

			for (int y = 0; y < spany; y++) {
				// The whole piece is squeezed into the visible span, sampled
				// at pixel centers. Along the axis the span is the size and
				// this is the pixel itself.
				int row = (2 * y + 1) * TILE_SIZE / (2 * spany);
				unsigned char *source = image + (top + row) * RETRO_WIDTH + left;
				unsigned char *target = buffer + (firsty + y) * RETRO_WIDTH + firstx;

				for (int x = 0; x < spanx; x++) {
					int column = (2 * x + 1) * TILE_SIZE / (2 * spanx);
					target[x] = ShadeTable[source[column]][shade];
				}
			}
		}
	}
}

void DEMO_Initialize(void)
{
	PictureA = RETRO_LoadImage("assets/monkey_320x240_quantizized.pcx", true);
	PictureB = RETRO_LoadImage("assets/flowers_320x240_quantizized.pcx");

	// One entry is taken back for the curtain. The pictures use it as one of
	// their own colors, so those pixels move to the nearest of the other
	// entries.
	RETRO_Palette *palette = PictureA->palette;
	unsigned char nearest = 1 + RETRO_NearestPaletteIndex(palette[CURTAIN], palette + 1, RETRO_COLORS - 1);
	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		if (PictureA->data[i] == CURTAIN) {
			PictureA->data[i] = nearest;
		}
		if (PictureB->data[i] == CURTAIN) {
			PictureB->data[i] = nearest;
		}
	}
	RETRO_SetColor(CURTAIN, RETRO_BLACK, palette);
	RETRO_SetColor(CURTAIN, RETRO_BLACK);

	// The last shade is every color as it is, so a broadside tile shows its
	// picture untouched. The floor keeps the darkest shades among colors the
	// pictures have plenty of.
	RETRO_CreateShadeTable(palette, RETRO_COLORS, SHADES, &ShadeTable[0][0], AMBIENT);
}
