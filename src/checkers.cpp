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
#include "lib/retropalette.h"
#include "lib/retrovector.h"

#define CHECKER_LAYERS 24
#define CHECKER_SPACING 0.72
#define CHECKER_FOCAL 180.0

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
	vec2 offset = {
		(float)(envelope * (22.0 * sin(board * 0.252) + 9.0 * sin(board * 0.583 + 1.0))),
		(float)(envelope * (16.0 * sin(board * 0.209 + 0.5) + 7.0 * sin(board * 0.482)))
	};
	return offset;
}

// The curve between p1 and p2, at t going from 0 to 1; p0 and p3 are only
// there to hand it a tangent at each end, matching the direction between
// that end's own neighbors: (p2-p0)/2 at p1, (p3-p1)/2 at p2. Matching
// tangents this way, rather than easing each segment to a dead stop with a
// smoothstep, is what keeps velocity continuous across board boundaries -
// the flight does not stutter once per board.
//
// This is the textbook Catmull-Rom basis matrix multiplied out into plain
// terms, so no matrix type is needed for four control points:
//
//   [ 0   2   0   0]
//   [-1   0   1   0]  *  [p0 p1 p2 p3]^T  *  [1 t t^2 t^3]^T  /  2
//   [ 2  -5   4  -1]
//   [-1   3  -3   1]
static vec2 CatmullRom(vec2 p0, vec2 p1, vec2 p2, vec2 p3, float t)
{
	float t2 = t * t, t3 = t2 * t;
	vec2 point = (p1 * 2.0f + (p2 - p0) * t
		+ (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2
		+ (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) * 0.5f;
	return point;
}

// Threads a smooth path through the per-board offsets, indexed by absolute
// world depth. Control points before the first board are clamped to that
// board's own offset instead of clamping the progress along the curve, so
// the approach eases up with zero velocity rather than kinking the instant
// the freeze lifts. X and Y ride the same board index and blend fraction,
// so they are solved together rather than as two otherwise-identical paths.
static vec2 Path(double worldz)
{
	double board = worldz / CHECKER_SPACING - 1.0;
	double board0 = floor(board);
	float frac = board - board0;
	vec2 p0 = BoardOffset(fmax(board0 - 1.0, 0.0));
	vec2 p1 = BoardOffset(fmax(board0, 0.0));
	vec2 p2 = BoardOffset(fmax(board0 + 1.0, 0.0));
	vec2 p3 = BoardOffset(fmax(board0 + 2.0, 0.0));
	return CatmullRom(p0, p1, p2, p3, frac);
}

void DEMO_Render(double time, double deltatime)
{
	// Start with a fine grid, then settle into a continuous flight. Wrapping
	// the travel distance replaces a board only once it is behind the eye.
	double travel = 2.4 * time;
	double cameraz = travel - 9.0 * exp(-time * 0.55);
	double nearest = CHECKER_SPACING - cameraz;
	if (nearest < 0.0) nearest += CHECKER_SPACING * ceil(-nearest / CHECKER_SPACING);
	vec2 camerapos = Path(cameraz);
	unsigned char *dest = RETRO_FrameBuffer();

	const RETRO_Palette colors[] = {
		RETRO_DARKSALMON, RETRO_ANTIQUEWHITE, RETRO_SEABLUE,
		RETRO_CHOCOLATE, RETRO_OLIVEGRAY, RETRO_DEEPSEABLUE,
		RETRO_TEALGRAY, RETRO_PERU
	};
	double colorphase = fmod(time / 3.0, 8.0);
	int first = (int)colorphase;
	int next = (first + 1) % 8;
	double blend = colorphase - first;
	blend = smoothstep(0.0, 1.0, blend);
	for (int layer = 0; layer < CHECKER_LAYERS; ++layer) {
		double depth = layer * CHECKER_SPACING + fmin(nearest, CHECKER_SPACING);
		double shade = exp(-depth * 0.16);
		RETRO_Palette color;
		color.r = (unsigned char)((colors[first].r * (1.0 - blend) + colors[next].r * blend) * shade);
		color.g = (unsigned char)((colors[first].g * (1.0 - blend) + colors[next].g * blend) * shade);
		color.b = (unsigned char)((colors[first].b * (1.0 - blend) + colors[next].b * blend) * shade);
		RETRO_SetColor(layer + 1, color);
	}

	// Paint far to near. Each board is offset by its own point on the weave
	// path, producing the characteristic nested square openings off-center
	// from one another instead of a single straight-through bore.
	for (int layer = CHECKER_LAYERS - 1; layer >= 0; --layer) {
		double z = 0.001 + nearest + layer * CHECKER_SPACING;
		double inversecell = z / CHECKER_FOCAL;
		vec2 offset = Path(cameraz + z) - camerapos;
		bool columns[RETRO_WIDTH];
		for (int x = 0; x < RETRO_WIDTH; ++x) {
			columns[x] = ((int)floor((x + 0.5 - RETRO_WIDTH * 0.5 - offset.x) * inversecell + 0.5) & 1) != 0;
		}
		for (int y = 0; y < RETRO_HEIGHT; ++y) {
			bool row = ((int)floor((y + 0.5 - RETRO_HEIGHT * 0.5 - offset.y) * inversecell + 0.5) & 1) != 0;
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
