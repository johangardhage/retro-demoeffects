//
// Dot tunnel, trailing
//
// Rings of dots receding into the distance, the tunnel from Second Reality.
// Ring i sits at depth d = (i + 1) / TUNNEL_RINGS and is projected by
//
//   (sx, sy) = (ring / d) + (WIDTH/2, HEIGHT/2) + sway
//
// so the mouth at d = 1 is one ring wide and the nearest ring is
// TUNNEL_RINGS times that, far past the frame. The near quarter of the
// tunnel is therefore mostly off screen, which is what flying through it
// looks like: a few dots sweeping outward past the eye.
//
// What bends the tunnel is a delay line. The centre line is a 2:3
// Lissajous figure, and a ring is offset by where that path was
// TUNNEL_LAG per ring ago - the mouth carries the newest point on the
// path and the nearest ring the oldest, so the tunnel is the wake the
// path leaves behind it. Walking the path one lag further moves each
// ring's offset onto the ring in front of it, and the shape flows toward
// the eye. The sway is added after the projection, not before, so it
// slides the whole ring across the screen rather than displacing it in
// the tunnel's own space.
//
// The original kept that history as a ring buffer of TUNNEL_RINGS past
// positions, one pushed per frame. The path is analytic, so the position
// is evaluated straight from the lagged phase instead, which costs two
// cosines a ring and drops the buffer, its modular indexing and its tie
// to the frame rate.
//
// TUNNEL_BAND rings are drawn and the same number skipped, stepped along
// with the phase, so bands of rings travel down the tunnel. phase lives
// on TUNNEL_PERIOD: the figure itself closes in a turn, but the bands
// only come back into step after five, and iphase counts the phase in
// rings, which is the unit the bands are laid out in.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define TUNNEL_RINGS 128 // depth slices between the eye and the mouth
#define RING_DOTS 64 // dots around a ring
#define RING_WIDTH 50 // semi axes of a ring at the mouth
#define RING_HEIGHT 37 // 31 on the original's 200 rows, scaled by 240/200 so the ellipse keeps its share of the frame
#define TUNNEL_SHADES 16 // palette entries the depth shading steps over
#define TUNNEL_BAND 5 // rings drawn, then the same number skipped
#define TUNNEL_SWAY 50 // how far the centre line wanders off the axis
#define TUNNEL_SPEED 30 // angle units a second the path is walked, one ring per step at 60 Hz
#define TUNNEL_LAG 0.5 // angle units of path between one ring and the next
#define TUNNEL_XPHASE (RETRO_SINCOS_ANGLE / 8) // offset that keeps the figure from opening on a crossing
#define TUNNEL_PERIOD (5 * RETRO_SINCOS_ANGLE) // five turns of the figure, the shortest span the bands also close over

double RingX[RING_DOTS];
double RingY[RING_DOTS];
double RingScale[TUNNEL_RINGS];

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * TUNNEL_SPEED, TUNNEL_PERIOD);
	int iphase = phase / TUNNEL_LAG;

	for (int i = 0; i < TUNNEL_RINGS; i++) {
		// Leave the gap between two bands
		if (WRAP(iphase + i, 2 * TUNNEL_BAND) < TUNNEL_BAND) {
			continue;
		}

		// Where the path was when this ring was laid down
		double lag = phase - (TUNNEL_RINGS - 1 - i) * TUNNEL_LAG;
		double swayx = TUNNEL_SWAY * COS(2 * lag + TUNNEL_XPHASE);
		double swayy = TUNNEL_SWAY * COS(3 * lag);

		// Shade by depth, so the mouth fades to black
		unsigned char color = TUNNEL_SHADES - 1 - i / (TUNNEL_RINGS / TUNNEL_SHADES);

		// Draw ring
		for (int j = 0; j < RING_DOTS; j++) {
			int x = RETRO_WIDTH / 2 + lround(RingX[j] * RingScale[i] + swayx);
			int y = RETRO_HEIGHT / 2 + lround(RingY[j] * RingScale[i] + swayy);

			if (x >= 0 && x < RETRO_WIDTH && y >= 0 && y < RETRO_HEIGHT) {
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
		double angle = i * (double)RETRO_SINCOS_ANGLE / RING_DOTS;

		RingX[i] = RING_WIDTH * COS(angle);
		RingY[i] = RING_HEIGHT * SIN(angle);
	}

	// Init depth. Ring i sits at depth (i + 1) / TUNNEL_RINGS, and the projection
	// divides by it, so what the draw needs is the reciprocal.
	for (int i = 0; i < TUNNEL_RINGS; i++) {
		RingScale[i] = (double)TUNNEL_RINGS / (i + 1);
	}
}
