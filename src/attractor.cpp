//
// Strange attractor
//
// Peter de Jong's map, iterated on one point and drawn where it lands:
//
//   x' = sin(a y) - cos(b x)
//   y' = sin(c x) - cos(d y)
//
// It is not a contraction and there is nothing self-similar about it, which is
// what sets it apart from the affine systems in ifs.cpp. The orbit is chaotic:
// two points a hair apart are anywhere at all a few hundred steps later. What
// stays put is where the orbit is dense, and that is the picture.
//
// Both coordinates are a sine less a cosine, so the orbit can never leave
// [-2, 2] whatever the parameters are, and the frame is fixed once rather than
// fitted to each of them.
//
// The parameters do not stay put. Each rides its own slow sine, at three, five
// and seven times the rate of the slowest, so the tuple travels a closed curve
// and comes back round every turn of that one, some three quarters of a minute.
// Rates no whole number relates would take it through the whole box instead and
// never repeat, but the box is not uniformly worth visiting: most of it holds
// parameters for which the map has a cycle and no attractor, and a curve that
// wanders freely sits in those and lurches out of them. This one was chosen to
// stay out, and repeating is what it costs. The attractor is a different set at
// every instant along it and the picture has to be redrawn from scratch, which
// is why the buffer counts points rather than keeping pixels: a count is topped
// up by whatever lands on it and fades in between, so the picture glows where
// the orbit is dense now and darkens where it has moved on.
//
// The fade is a fraction of the count and not a fixed amount, because a fixed
// one has no equilibrium. A pixel taking more than that amount runs to
// saturation and a pixel taking less goes dark, and since an orbit lays down
// well under one point per pixel per step, no constant fade separates the two:
// it either erases the attractor or blows it out. Taking a share of the count
// settles instead, at
//
//   count = (fade steps * points landing a step - 1) << DENSITY_SHIFT
//
// so the sparse haze holds a few counts, a caustic the orbit crosses over and
// over holds a hundred, and neither reaches the end of the ramp. The one that
// goes with the share is what lets a pixel reach zero: a share alone leaves
// anything under 1 << DENSITY_SHIFT standing forever, and the picture would
// keep every place the orbit had ever been.
//
// A chaotic orbit is not evenly chaotic. It sticks: where the drifting tuple
// passes near a cycle the orbit returns to one pixel over and over, thousands
// of times in a step, while the rest of it wanders as before. A count taking
// that much in one go is far past the end of the ramp, and because the fade
// takes a share of it rather than a fixed amount it needs half a second to come
// back down, so the pixel stands over a haze that is nowhere near it for as
// long. What one step may add to one pixel is therefore held to the most the
// fade can carry, which is the count whose equilibrium is the end of the ramp:
//
//   DENSITY_STEP = ((DENSITY_MAX >> DENSITY_SHIFT) + 1) / DENSITY_FADE
//
// A caustic lays down that much every step and still climbs to the top, since
// the equilibrium is where it lands and not where one step takes it. A pixel
// the orbit merely stuck on holds one step of it and fades from there.
//
// The same stickiness taken far enough stops the picture rather than spoiling
// it. Where the tuple crosses a window in which the map has a cycle and no
// attractor at all, the orbit falls onto a few points and draws nothing
// anywhere else for as long as the crossing lasts. A fade owed to the step
// having happened goes on erasing throughout, and a third of a second of that
// is enough to strip the picture to a tenth of itself before the orbit comes
// back. So the fade is owed instead to what the orbit drew: it advances by the
// pixels a step reached, ORBIT_SPREAD of them to the step, and holds still
// while the orbit has nothing to add. Running free the two are the same thing
// and the equilibrium above is what it was.
//
// Holding the picture is better than stripping it, but a second of stillness is
// a fault of its own, and the way out is that there is nothing in such a window
// worth the time it takes to cross. So the tuple crosses one it has stalled in
// at DRIFT_HURRY times its usual speed. A window is left when the orbit spreads
// again and not after any set distance, so hurrying skips no part of the curve:
// the same dead stretch is covered either way, and only the time it takes goes
// down, from a stall of nineteen frames to one of five.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define POINTS_PER_SECOND 1200000 // points the orbit is followed for a second
#define DENSITY_MAX 255 // points a pixel counts before it stops counting
#define DENSITY_FADE 2 // steps between fades of a pixel's count
#define DENSITY_SHIFT 3 // the share of a count a fade takes, as a right shift
#define ATTRACTOR_REACH 2.0 // the furthest sin - cos can carry a coordinate
#define MARGIN 0.98 // of the screen the frame fills
#define DRIFT_SPEED 0.14 // radians a second the slowest parameter travels
#define DENSITY_STEP (((DENSITY_MAX >> DENSITY_SHIFT) + 1) / DENSITY_FADE) // points one step adds to one pixel
#define ORBIT_SPREAD (RETRO_WIDTH * RETRO_HEIGHT / 7) // pixels a step reaches with the orbit running free
#define ORBIT_STALL (ORBIT_SPREAD / 8) // pixels a step reaches below which it is drawing no attractor
#define DRIFT_HURRY 4 // times its usual speed the drift crosses a window the orbit has stalled in

unsigned char Density[RETRO_WIDTH * RETRO_HEIGHT];
unsigned char Landed[RETRO_WIDTH * RETRO_HEIGHT];
unsigned char Shade[DENSITY_MAX + 1];
double PointX, PointY;

//
// Follow the orbit in fixed steps, so what the picture holds is a fixed span of
// it however the frame rate wanders, and a frame that arrives late cannot ask
// for more points than the mainloop's catch up allows
//
void DEMO_FixedUpdate(double timestep)
{
	// Calculate phase. It is an angle and wraps on a whole turn of it, which is
	// a whole number of turns of every rate driven by it and so leaves all four
	// sines where they were. A window the orbit has stalled in is crossed at
	// speed, there being no attractor in one to hold the picture still for
	static double phase = 0;
	static bool stalled = false;
	phase = fmod(phase + timestep * DRIFT_SPEED * (stalled ? DRIFT_HURRY : 1), 2 * M_PI);

	// Drift the parameters. The rates are whole multiples of the slowest, so the
	// curve the tuple travels is closed and stays in the part of the box that
	// holds attractors rather than cycles
	double a = 1.4 + 0.9 * sin(phase);
	double b = -2.3 + 0.7 * sin(phase * 3);
	double c = 2.4 + 0.8 * sin(phase * 5);
	double d = -2.1 + 0.6 * sin(phase * 7);

	// Follow the orbit, keeping alongside it what each pixel has taken this step
	double scale = MARGIN * MIN(RETRO_WIDTH, RETRO_HEIGHT) / (2 * ATTRACTOR_REACH);
	int points = timestep * POINTS_PER_SECOND;
	int reached = 0;

	memset(Landed, 0, sizeof(Landed));

	for (int i = 0; i < points; i++) {
		double x = sin(a * PointY) - cos(b * PointX);
		PointY = sin(c * PointX) - cos(d * PointY);
		PointX = x;

		int sx = RETRO_WIDTH / 2 + scale * PointX;
		int sy = RETRO_HEIGHT / 2 - scale * PointY;

		// The reach is a bound on the orbit, not on the pixel it rounds to
		if (sx >= 0 && sx < RETRO_WIDTH && sy >= 0 && sy < RETRO_HEIGHT) {
			int offset = sy * RETRO_WIDTH + sx;
			if (Landed[offset] == 0) {
				reached++;
			}
			if (Landed[offset] < DENSITY_STEP) {
				Landed[offset]++;
				if (Density[offset] < DENSITY_MAX) {
					Density[offset]++;
				}
			}
		}
	}

	// Fade what the orbit has left behind, by as much of it as the orbit has
	// just redrawn. A step that reached nothing erases nothing
	static int drawn = 0;
	drawn += reached;

	while (drawn >= ORBIT_SPREAD * DENSITY_FADE) {
		drawn -= ORBIT_SPREAD * DENSITY_FADE;
		for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
			int count = Density[i];
			if (count > 0) {
				count -= (count >> DENSITY_SHIFT) + 1;
				Density[i] = MAX(count, 0);
			}
		}
	}

	// A step that reached next to nothing drew no attractor, so the next one
	// takes the parameters further along
	stalled = reached < ORBIT_STALL;
}

void DEMO_Render(double time, double deltatime)
{
	// Draw attractor
	unsigned char *buffer = RETRO_FrameBuffer();

	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		buffer[i] = Shade[Density[i]];
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, 96, RETRO_BLACK, RETRO_ROYALINDIGO);
	RETRO_CreateGradientPalette(96, 192, RETRO_ROYALINDIGO, RETRO_DARKTURQUOISE);
	RETRO_CreateGradientPalette(192, RETRO_COLORS, RETRO_DARKTURQUOISE, RETRO_WHITE);

	// A density ramp, log in the count. Doubling the points that land on a
	// pixel is the same step of the ramp wherever it starts, so the thin arms
	// of an attractor hold their shape instead of being one shade above nothing
	for (int i = 1; i <= DENSITY_MAX; i++) {
		Shade[i] = 1 + (RETRO_COLORS - 2) * log(1 + i) / log(1 + DENSITY_MAX);
	}
}
