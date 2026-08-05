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
// is also turned about the axis and the mouth wanders. twist lives in [0, 360).
//
// Each ring is 360 / RING_STEP dots at even 6°, so the ellipse closes.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocolor.h"

#define RING_COUNT 40 // rings between the eye and the mouth
#define RING_STEP 6 // degrees between the dots around a ring
#define RING_DOTS (RETRO_DEGREES_PER_TURN / RING_STEP) // 6° divides a turn, so the ellipse closes
#define RING_WIDTH 740 // the ring is an ellipse, so it has two semi axes
#define RING_HEIGHT 788
#define TUNNEL_DEPTH 20 // depth of the furthest ring
#define TUNNEL_SPEED 20 // depth travelled per second
#define TWIST_PER_RING 15 // degrees the centre line turns from one ring to the next
#define SWAY_RADIUS 260 // how far that turn throws a ring's centre off the axis
#define TWIST_SPEED 600 // degrees a second the helix turns about the axis

double RingX[RING_DOTS];
double RingY[RING_DOTS];

void DEMO_Render(double deltatime)
{
	double spacing = (double)TUNNEL_DEPTH / RING_COUNT;

	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * TUNNEL_SPEED, spacing);

	static double twist = 0;
	twist = fmod(twist - deltatime * TWIST_SPEED, RETRO_DEGREES_PER_TURN);
	if (twist < 0) {
		twist += RETRO_DEGREES_PER_TURN;
	}

	// Draw rings
	for (int i = 0; i < RING_COUNT; i++) {
		// Ring 0 is the furthest. The nearest ring is still a whole spacing from
		// the eye at its closest, so the divide never blows up.
		double along = i + phase / spacing;
		double depth = TUNNEL_DEPTH + spacing - i * spacing - phase;

		double angle = (twist + along * TWIST_PER_RING) * DEG2RAD;
		double swayx = SWAY_RADIUS * sin(angle);
		double swayy = SWAY_RADIUS * cos(angle);

		unsigned char color = CLAMP256((RETRO_COLORS - 1) * along / RING_COUNT);

		for (int j = 0; j < RING_DOTS; j++) {
			int x = (RingX[j] + swayx) / depth + RETRO_WIDTH / 2;
			int y = (RingY[j] + swayy) / depth + RETRO_HEIGHT / 2;

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
		double angle = i * RING_STEP * DEG2RAD;

		RingX[i] = RING_WIDTH * sin(angle);
		RingY[i] = RING_HEIGHT * cos(angle);
	}
}
