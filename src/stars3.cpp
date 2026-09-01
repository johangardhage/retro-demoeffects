//
// Stars, tumbling
//
// A box of stars, half-extents (W, H, BOX_DEPTH), rotated by R = Rz Ry Rx
// (the same sequential Rx, Ry, Rz as RETRO_RotateVertex). The cloud is
// fixed; only the view changes. After rotation a star is the library
// pinhole q = 1/(rz + eye) and is shaded by rotated depth:
//
//   color = (furthest − rz) · (SHADES − 1) / (furthest − STAR_NEAREST)
//
// furthest = √(W² + H² + D²) is the bounding-sphere radius. Stars with
// rz ≤ STAR_NEAREST (−150) are dropped — 100 units in front of the eye
// (eye = 250), which also drops anything behind it, so they do not streak.
// The box is filled half-open: [−W, W) × [−H, H) × [−D, D). Euler angles
// live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retropalette.h"

#define NUM_STARS 1000
#define SPEED 2 // radians a second, about each axis
#define BOX_DEPTH 250 // half the depth of the box; it is two screens wide and two tall
#define PROJECTION_SCALE 1.0 // the box is built in pixels, so the projection adds no scale
#define STAR_NEAREST (-150) // nearest a star may come before it would streak, in rotated depth
#define SHADES 64 // palette entries the depth shading ramps over

Vertex Stars[NUM_STARS];

void DEMO_Render(double time, double deltatime)
{
	// Calculate rotation
	float ax = fmod(time * SPEED, 2 * M_PI);
	float ay = fmod(time * SPEED, 2 * M_PI);
	float az = fmod(time * SPEED, 2 * M_PI);

	double furthest = sqrt((double)RETRO_WIDTH * RETRO_WIDTH + (double)RETRO_HEIGHT * RETRO_HEIGHT + (double)BOX_DEPTH * BOX_DEPTH);

	// Draw stars
	for (int i = 0; i < NUM_STARS; i++) {
		RETRO_RotateVertex(&Stars[i], ax, ay, az);

		if (Stars[i].rz <= STAR_NEAREST) {
			continue;
		}

		RETRO_ProjectVertex(&Stars[i], PROJECTION_SCALE);

		int x = Stars[i].sx;
		int y = Stars[i].sy;

		if (x >= 0 && x < RETRO_WIDTH && y >= 0 && y < RETRO_HEIGHT) {
			int color = (furthest - Stars[i].rz) * (SHADES - 1) / (furthest - STAR_NEAREST);

			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, SHADES, RETRO_BLACK, RETRO_WHITE);

	// Init stars. Fill a box centred on the eye's axis: [-W, W] x [-H, H] x [-BOX_DEPTH, BOX_DEPTH].
	for (int i = 0; i < NUM_STARS; i++) {
		Stars[i].x = RANDOM(RETRO_WIDTH * 2) - RETRO_WIDTH;
		Stars[i].y = RANDOM(RETRO_HEIGHT * 2) - RETRO_HEIGHT;
		Stars[i].z = RANDOM(BOX_DEPTH * 2) - BOX_DEPTH;
	}
}
