//
// Dot tunnel
//
// Rings of dots receding into the distance. Ring i at depth d is projected by
//
//   (sx, sy) = (ring + sway) / d + (WIDTH/2, HEIGHT/2)
//
// Flying forwards slides the whole tunnel toward the eye by TUNNEL_SPEED and
// wraps once it has moved by one ring spacing, so ring i lands where ring i+1
// was. Brightness follows along, not the ring index, so the recycle does not
// flash. The nearest ring stays a whole spacing from the eye, so the divide
// never hits 0.
//
// The centre line is a helix, not a straight axis. A ring's centre is thrown
// off by angle (twist + along * TWIST_PER_RING). Sliding alone would leave that
// pattern standing still (each ring inherits the one behind it), so the helix
// is also turned about the axis and the mouth wanders. twist lives in
// [0, RETRO_ANGLES_PER_TURN).
//
// Each ring is RING_DOTS dots at an even step: 6° in the original, 360/6 = 60
// exactly, so the ellipse closes with no seam.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define RING_COUNT 40 // rings between the eye and the mouth
#define RING_DOTS 60 // 6° per dot in the original, 360/6 = 60 exactly
#define RING_WIDTH 740 // the ring is an ellipse, so it has two semi axes
#define RING_HEIGHT 788
#define TUNNEL_DEPTH 20 // depth of the furthest ring
#define TUNNEL_SPEED 20 // depth travelled per second
#define TWIST_PER_RING (RETRO_ANGLES_PER_TURN / 24.0) // 15° per ring, in angle units
#define SWAY_RADIUS 260 // how far that turn throws a ring's centre off the axis
#define TWIST_SPEED (RETRO_ANGLES_PER_TURN * 5.0 / 3.0) // 600°/s, in angle units

double RingX[RING_DOTS];
double RingY[RING_DOTS];

void DEMO_Render(double time, double deltatime)
{
	double spacing = (double)TUNNEL_DEPTH / RING_COUNT;

	// Calculate phase
	double phase = fmod(time * TUNNEL_SPEED, spacing);

	double twist = fmod(-time * TWIST_SPEED, RETRO_ANGLES_PER_TURN);
	if (twist < 0) {
		twist += RETRO_ANGLES_PER_TURN;
	}

	// Draw rings
	for (int i = 0; i < RING_COUNT; i++) {
		// Ring 0 is the furthest. The nearest ring is still a whole spacing from
		// the eye at its closest, so the divide never blows up.
		double along = i + phase / spacing;
		double depth = TUNNEL_DEPTH + spacing - i * spacing - phase;

		double angle = twist + along * TWIST_PER_RING;
		double swayx = SWAY_RADIUS * SIN(angle);
		double swayy = SWAY_RADIUS * COS(angle);

		unsigned char color = CLAMP256((RETRO_COLORS - 1) * along / RING_COUNT);

		for (int j = 0; j < RING_DOTS; j++) {
			int x = (RingX[j] + swayx) / depth + RETRO_WIDTH / 2.0;
			int y = (RingY[j] + swayy) / depth + RETRO_HEIGHT / 2.0;

			if (x >= 0 && x < RETRO_WIDTH && y >= 0 && y < RETRO_HEIGHT) {
				RETRO_PutPixel(x, y, color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK, RETRO_WHITE);

	// Init ring
	for (int i = 0; i < RING_DOTS; i++) {
		double angle = i * (double)RETRO_ANGLES_PER_TURN / RING_DOTS;

		RingX[i] = RING_WIDTH * SIN(angle);
		RingY[i] = RING_HEIGHT * COS(angle);
	}
}
