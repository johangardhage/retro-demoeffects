//
// Dot tunnel, bending
//
// Rings of dots receding into the distance. Ring i at depth z is the pinhole
//
//   sx = WIDTH/2  + x * EYE / (EYE − z) + xo
//   sy = HEIGHT/2 + y * EYE / (EYE − z) + yo
//
// z runs from TUNNEL_START toward the eye at +EYE and never reaches it.
// The tunnel does not slide past the eye. It stands still and bends: each
// ring's center (xo, yo) is a pair of sinusoids of both time and the ring
// index, so no two rings are offset alike and the tunnel writhes along its
// length. Brightness is the ring index (the rings do not recycle, so the
// index is depth). phase lives in [0, 256).
//
// POINTSTEP does not divide 256, so the ring is n = 256/POINTSTEP dots at
// 256/n, not a step of 5 (that leftover seam is a fifth wider).
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retrovector.h"

constexpr int POINTSTEP = 5; // angle units between the dots around a ring, of the 256 in a whole turn
constexpr int RING_DOTS = RETRO_ANGLES_PER_TURN / POINTSTEP; // 5 does not divide 256; the ring uses 256/n
constexpr int CIRCLE_RADIUS = 50;
constexpr int RING_COUNT = 87; // rings between the mouth and the eye
constexpr int RING_STEP = 5; // depth from one ring to the next
constexpr int TUNNEL_START = -240; // depth of the furthest ring
constexpr int EYE = 250; // how far the eye sits from the screen
constexpr int SWAY = 128; // how far the tunnel's center wanders off the axis
constexpr int SWAY_SPEED = 100; // angle units per second
constexpr int SHADES = 64; // palette entries the depth shading ramps over

static vec2 Circle[RING_DOTS];

void DEMO_Render(RETRO_Time time)
{
	// Calculate phase
	double phase = mod(-time.total * SWAY_SPEED, RETRO_ANGLES_PER_TURN);

	// Draw rings
	for (int i = 0; i < RING_COUNT; i++) {
		float xo = COS(phase * 2 + i * 3) * SWAY / 4 + SIN(phase + i * 2) * SWAY / 3;
		float yo = COS(phase * 2 + i * 2) * SWAY / 5 + SIN(phase * 2 + i * 3) * SWAY / 4;

		float z = TUNNEL_START + i * RING_STEP;
		int color = i * (SHADES - 1) / (RING_COUNT - 1);

		for (int j = 0; j < RING_DOTS; j++) {
			double x = (RETRO_WIDTH / 2.0) + (Circle[j].x * EYE) / (EYE - z) + xo;
			double y = (RETRO_HEIGHT / 2.0) + (Circle[j].y * EYE) / (EYE - z) + yo;

			if (RETRO_OnScreen((float)x, (float)y)) {
				RETRO_PutPixel(x, y, color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, SHADES, RETRO_BLACK, RETRO_WHITE);

	// Init ring
	for (int i = 0; i < RING_DOTS; i++) {
		double angle = i * (double)RETRO_ANGLES_PER_TURN / RING_DOTS;

		Circle[i].x = CIRCLE_RADIUS * COS(angle);
		Circle[i].y = CIRCLE_RADIUS * SIN(angle);
	}
}
