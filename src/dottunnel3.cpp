//
// Dot tunnel, trailing
//
// Rings of dots receding into the distance. Ring i sits at depth d = (i + 1 -
// frac) / TUNNEL_RINGS, where frac is how far the phase is past its last whole
// ring, and is projected by
//
//   (sx, sy) = (ring / d) + (WIDTH/2, HEIGHT/2) + sway
//
// so the mouth at d = 1 is one ring wide and the nearest ring is
// TUNNEL_RINGS times that, far past the frame. The near quarter of the
// tunnel is therefore mostly off screen, which is what flying through it
// looks like: a few dots sweeping outward past the eye.
//
// What bends the tunnel is a delay line. The center line is a 2:3
// Lissajous figure, and a ring is offset by where that path was
// TUNNEL_LAG per ring ago - the mouth carries the newest point on the
// path and the nearest ring the oldest, so the tunnel is the wake the
// path leaves behind it. A ring keeps the point it was laid down at and
// glides one ring closer per lag walked, so the shape flows toward the
// eye with no step between frames. The sway is added after the projection, not before, so it
// slides the whole ring across the screen rather than displacing it in
// the tunnel's own space.
//
// The path is analytic, so no history of past positions is kept: each
// ring's position is evaluated straight from the lagged phase, which costs
// two cosines a ring and has no tie to the frame rate. The rings slide by
// frac; without it the rings and bands would jump a whole ring whenever
// frames and steps drift apart.
//
// TUNNEL_BAND rings are drawn and the same number skipped, stepped along
// with the rings, so bands of rings travel down the tunnel. phase lives
// on TUNNEL_PERIOD: the figure itself closes in a turn, but the bands
// only come back into step after five, and iphase counts the phase in
// rings, which is the unit the bands are laid out in.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

constexpr int TUNNEL_RINGS = 128; // depth slices between the eye and the mouth
constexpr int RING_DOTS = 64; // dots around a ring
constexpr int RING_WIDTH = 50; // semi axes of a ring at the mouth
constexpr int RING_HEIGHT = 37;
constexpr int TUNNEL_SHADES = 16; // palette entries the depth shading steps over
constexpr int TUNNEL_BAND = 5; // rings drawn, then the same number skipped
constexpr int TUNNEL_SWAY = 50; // how far the center line wanders off the axis
constexpr int TUNNEL_SPEED = 30; // angle units a second the path is walked, one ring per step at 60 Hz
constexpr double TUNNEL_LAG = 0.5; // angle units of path between one ring and the next
constexpr double TUNNEL_XPHASE = RETRO_ANGLES_PER_TURN / 8.0; // offset that keeps the figure from opening on a crossing
constexpr int TUNNEL_PERIOD = 5 * RETRO_ANGLES_PER_TURN; // five turns of the figure, the shortest span the bands also close over

static double RingX[RING_DOTS];
static double RingY[RING_DOTS];

void DEMO_Render(RETRO_Time time)
{
	// Calculate phase
	double phase = fmod(time.total * TUNNEL_SPEED, TUNNEL_PERIOD);
	int iphase = phase / TUNNEL_LAG;
	double frac = phase / TUNNEL_LAG - iphase;

	for (int i = 0; i < TUNNEL_RINGS; i++) {
		// Leave the gap between two bands
		if (WRAP(iphase + i, 2 * TUNNEL_BAND) < TUNNEL_BAND) {
			continue;
		}

		// Where the path was when this ring was laid down
		double lag = (iphase + i - (TUNNEL_RINGS - 1)) * TUNNEL_LAG;
		double swayx = TUNNEL_SWAY * COS(2 * lag + TUNNEL_XPHASE);
		double swayy = TUNNEL_SWAY * COS(3 * lag);

		// Depth slides toward the eye by the part of a step walked so far
		double scale = TUNNEL_RINGS / (i + 1 - frac);

		// Shade by depth, so the mouth fades to black
		unsigned char color = TUNNEL_SHADES - 1 - i / (TUNNEL_RINGS / TUNNEL_SHADES);

		// Draw ring
		for (int j = 0; j < RING_DOTS; j++) {
			int x = RETRO_WIDTH / 2.0 + lround(RingX[j] * scale + swayx);
			int y = RETRO_HEIGHT / 2.0 + lround(RingY[j] * scale + swayy);

			if (RETRO_OnScreen(x, y)) {
				RETRO_PutPixel(x, y, color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, TUNNEL_SHADES, RETRO_BLACK, RETRO_WHITE);

	// Init ring
	for (int i = 0; i < RING_DOTS; i++) {
		double angle = i * (double)RETRO_ANGLES_PER_TURN / RING_DOTS;

		RingX[i] = RING_WIDTH * COS(angle);
		RingY[i] = RING_HEIGHT * SIN(angle);
	}
}
