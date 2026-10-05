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
#include "lib/retromath.h"
#include "lib/retropalette.h"
#include "lib/retrovector.h"

#define CHECKER_LAYERS 24
#define CHECKER_SPACING 0.72
#define CHECKER_FOCAL 180.0
#define CHECKER_COLORS 8

// The boards fade from each of these colors to the next
static const RETRO_Palette Colors[CHECKER_COLORS] = {
	RETRO_DARKSALMON, RETRO_ANTIQUEWHITE, RETRO_SEABLUE,
	RETRO_CHOCOLATE, RETRO_OLIVEGRAY, RETRO_DEEPSEABLUE,
	RETRO_TEALGRAY, RETRO_PERU
};

// Each board gets its own opening position, so the flight has to weave to
// pass through every one of them rather than follow a single straight bore.
// The phase step per board is kept small so neighboring boards land close
// together on the curve instead of swinging wildly from one to the next.
// The camera is still shedding its initial exponential rush over these
// first boards, so raw amplitude would cover the same lateral distance in
// far less time than later boards get; envelope ramps that in over the
// first ten boards, keeping the early swings gentle instead of snapping
// hard right after the first board is passed. X and Y share that envelope
// and this one board argument, so the weave is not computed twice over for
// what is really one point on the curve.
static vec2 BoardOffset(double board)
{
	double envelope = smoothstep(0.0, 10.0, board);
	return {
		(float)(envelope * (22.0 * sin(board * 0.252) + 9.0 * sin(board * 0.583 + 1.0))),
		(float)(envelope * (16.0 * sin(board * 0.209 + 0.5) + 7.0 * sin(board * 0.482)))
	};
}

// Threads a Catmull-Rom path through the per-board offsets, indexed by
// absolute world depth. Its tangents match across board boundaries, so the
// velocity is continuous and the flight does not stutter once per board, as it
// would if each segment eased to a stop. Control points before the first board
// are clamped to that board's own offset instead of clamping the progress
// along the curve, so the approach eases up with zero velocity rather than
// kinking the instant the freeze lifts. X and Y ride the same board index and
// blend fraction, so they are solved together rather than as two
// otherwise-identical paths.
static vec2 Path(double worldz)
{
	double board = worldz / CHECKER_SPACING - 1.0;
	double board0 = floor(board);
	float frac = board - board0;
	vec2 p0 = BoardOffset(MAX(board0 - 1.0, 0.0));
	vec2 p1 = BoardOffset(MAX(board0, 0.0));
	vec2 p2 = BoardOffset(MAX(board0 + 1.0, 0.0));
	vec2 p3 = BoardOffset(MAX(board0 + 2.0, 0.0));
	return RETRO_CatmullRom(p0, p1, p2, p3, frac);
}

void DEMO_Render(RETRO_Time time)
{
	// Start with a fine grid, then settle into a continuous flight. Wrapping
	// the travel distance replaces a board only once it is behind the eye.
	double travel = 2.4 * time.total;
	double cameraz = travel - 9.0 * exp(-time.total * 0.55);
	double nearest = CHECKER_SPACING - cameraz;
	if (nearest < 0.0) nearest += CHECKER_SPACING * ceil(-nearest / CHECKER_SPACING);
	vec2 camerapos = Path(cameraz);
	unsigned char *dest = RETRO_FrameBuffer();

	double colorphase = fmod(time.total / 3.0, CHECKER_COLORS);
	int first = (int)colorphase;
	int next = (first + 1) % CHECKER_COLORS;
	double blend = smoothstep(0.0, 1.0, colorphase - first);
	for (int layer = 0; layer < CHECKER_LAYERS; layer++) {
		double depth = layer * CHECKER_SPACING + MIN(nearest, CHECKER_SPACING);
		double shade = exp(-depth * 0.16);
		RETRO_SetColor(layer + 1, mix(Colors[first], Colors[next], blend) * shade);
	}

	// Paint far to near. Each board is offset by its own point on the weave
	// path, producing the characteristic nested square openings off-center
	// from one another instead of a single straight-through bore.
	for (int layer = CHECKER_LAYERS - 1; layer >= 0; layer--) {
		double z = 0.001 + nearest + layer * CHECKER_SPACING;
		double inversecell = z / CHECKER_FOCAL;
		vec2 offset = Path(cameraz + z) - camerapos;
		bool columns[RETRO_WIDTH];
		for (int x = 0; x < RETRO_WIDTH; x++) {
			columns[x] = ((int)floor((x + 0.5 - RETRO_WIDTH * 0.5 - offset.x) * inversecell + 0.5) & 1) != 0;
		}
		for (int y = 0; y < RETRO_HEIGHT; y++) {
			bool row = ((int)floor((y + 0.5 - RETRO_HEIGHT * 0.5 - offset.y) * inversecell + 0.5) & 1) != 0;
			for (int x = 0; x < RETRO_WIDTH; x++) {
				if (columns[x] != row) dest[y * RETRO_WIDTH + x] = layer + 1;
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(0, RETRO_BLACK);
}
