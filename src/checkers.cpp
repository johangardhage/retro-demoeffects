//
// Checkers
//
// Fly through a stack of checkerboards. Alternate squares are holes,
// revealing the smaller boards behind them along the diagonals. Projected
// cell boundaries stay axis-aligned, so each layer needs only row/column
// parity tables rather than textured polygons.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define CHECKER_LAYERS 24
#define CHECKER_SPACING 0.72
#define CHECKER_FOCAL 180.0

// Each board gets its own opening position, so the flight has to weave to
// pass through every one of them rather than follow a single straight bore.
// The phase step per board is kept small so neighboring boards land close
// together on the curve instead of swinging wildly from one to the next.
// The camera is still shedding its initial exponential rush over these
// first boards, so raw amplitude would cover the same lateral distance in
// far less time than later boards get; ramping it in keeps the early swings
// gentle instead of snapping hard right after the first board is passed.
static double WeaveEnvelope(double board)
{
	double t = board / 10.0;
	t = fmax(0.0, fmin(1.0, t));
	return t * t * (3.0 - 2.0 * t);
}

static double BoardOffsetX(double board)
{
	return WeaveEnvelope(board) * (22.0 * sin(board * 0.252) + 9.0 * sin(board * 0.583 + 1.0));
}

static double BoardOffsetY(double board)
{
	return WeaveEnvelope(board) * (16.0 * sin(board * 0.209 + 0.5) + 7.0 * sin(board * 0.482));
}

// Catmull-Rom keeps velocity continuous across board boundaries, unlike a
// per-segment smoothstep, which eases to a dead stop at every waypoint and
// makes the flight look like it stutters once per board.
static double CatmullRom(double p0, double p1, double p2, double p3, double t)
{
	double t2 = t * t, t3 = t2 * t;
	return 0.5 * (2.0 * p1 + (p2 - p0) * t
		+ (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2
		+ (3.0 * p1 - p0 - 3.0 * p2 + p3) * t3);
}

// Threads a smooth path through the per-board offsets, indexed by absolute
// world depth. Control points before the first board are clamped to that
// board's own offset instead of clamping the progress along the curve, so
// the approach eases up with zero velocity rather than kinking the instant
// the freeze lifts.
static double PathX(double worldz)
{
	double board = worldz / CHECKER_SPACING - 1.0;
	double board0 = floor(board);
	double frac = board - board0;
	return CatmullRom(BoardOffsetX(fmax(board0 - 1.0, 0.0)), BoardOffsetX(fmax(board0, 0.0)),
		BoardOffsetX(fmax(board0 + 1.0, 0.0)), BoardOffsetX(fmax(board0 + 2.0, 0.0)), frac);
}

static double PathY(double worldz)
{
	double board = worldz / CHECKER_SPACING - 1.0;
	double board0 = floor(board);
	double frac = board - board0;
	return CatmullRom(BoardOffsetY(fmax(board0 - 1.0, 0.0)), BoardOffsetY(fmax(board0, 0.0)),
		BoardOffsetY(fmax(board0 + 1.0, 0.0)), BoardOffsetY(fmax(board0 + 2.0, 0.0)), frac);
}

void DEMO_Render(double time, double deltatime)
{
	// Start with a fine grid, then settle into a continuous flight. Wrapping
	// the travel distance replaces a board only once it is behind the eye.
	double travel = 2.4 * time;
	double cameraZ = travel - 9.0 * exp(-time * 0.55);
	double nearest = CHECKER_SPACING - cameraZ;
	if (nearest < 0.0) nearest += CHECKER_SPACING * ceil(-nearest / CHECKER_SPACING);
	double cameraX = PathX(cameraZ);
	double cameraY = PathY(cameraZ);
	unsigned char *dest = RETRO_FrameBuffer();

	// Muted colors from the reference, interpolated over the flight.
	const int colors[][3] = {
		{211, 146, 132}, {242, 240, 209}, {27, 123, 166},
		{191, 119, 43}, {131, 133, 96}, {21, 120, 166},
		{112, 142, 139}, {185, 114, 49}
	};
	double colorphase = fmod(time / 3.0, 8.0);
	int first = (int)colorphase;
	int next = (first + 1) % 8;
	double blend = colorphase - first;
	blend = blend * blend * (3.0 - 2.0 * blend);
	for (int layer = 0; layer < CHECKER_LAYERS; ++layer) {
		double depth = layer * CHECKER_SPACING + fmin(nearest, CHECKER_SPACING);
		double shade = exp(-depth * 0.16);
		int rgb[3];
		for (int c = 0; c < 3; ++c) {
			double base = colors[first][c] * (1.0 - blend) + colors[next][c] * blend;
			rgb[c] = (int)(base * shade);
		}
		RETRO_SetColor(layer + 1, rgb[0], rgb[1], rgb[2]);
	}

	// Paint far to near. Each board is offset by its own point on the weave
	// path, producing the characteristic nested square openings off-center
	// from one another instead of a single straight-through bore.
	for (int layer = CHECKER_LAYERS - 1; layer >= 0; --layer) {
		double z = 0.001 + nearest + layer * CHECKER_SPACING;
		double inversecell = z / CHECKER_FOCAL;
		double offsetX = PathX(cameraZ + z) - cameraX;
		double offsetY = PathY(cameraZ + z) - cameraY;
		bool columns[RETRO_WIDTH];
		for (int x = 0; x < RETRO_WIDTH; ++x) {
			columns[x] = ((int)floor((x + 0.5 - RETRO_WIDTH * 0.5 - offsetX) * inversecell + 0.5) & 1) != 0;
		}
		for (int y = 0; y < RETRO_HEIGHT; ++y) {
			bool row = ((int)floor((y + 0.5 - RETRO_HEIGHT * 0.5 - offsetY) * inversecell + 0.5) & 1) != 0;
			for (int x = 0; x < RETRO_WIDTH; ++x) {
				if (columns[x] != row) dest[y * RETRO_WIDTH + x] = layer + 1;
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(0, 0, 0, 0);
}
