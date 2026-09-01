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
// ring's centre (xo, yo) is a pair of sinusoids of both time and the ring
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
#include "lib/retrogfx.h"
#include "lib/retropalette.h"

#define POINTSTEP 5 // angle units between the dots around a ring, of the 256 in a whole turn
#define RING_DOTS (RETRO_SINCOS_ANGLE / POINTSTEP) // 5 does not divide 256; the ring uses 256/n
#define CIRCLE_RADIUS 50
#define RING_COUNT 87 // rings between the mouth and the eye
#define RING_STEP 5 // depth from one ring to the next
#define TUNNEL_START (-240) // depth of the furthest ring
#define EYE 250 // how far the eye sits from the screen
#define SWAY 128 // how far the tunnel's centre wanders off the axis
#define SWAY_SPEED 100 // angle units per second
#define SHADES 64 // palette entries the depth shading ramps over

Point2Df Circle[RING_DOTS];

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase
	double phase = fmod(-time * SWAY_SPEED, RETRO_SINCOS_ANGLE);
	if (phase < 0) {
		phase += RETRO_SINCOS_ANGLE;
	}

	// Draw rings
	for (int i = 0; i < RING_COUNT; i++) {
		float xo = COS(phase * 2 + i * 3) * SWAY / 4 + SIN(phase + i * 2) * SWAY / 3;
		float yo = COS(phase * 2 + i * 2) * SWAY / 5 + SIN(phase * 2 + i * 3) * SWAY / 4;

		float z = TUNNEL_START + i * RING_STEP;
		int color = i * (SHADES - 1) / (RING_COUNT - 1);

		for (int j = 0; j < RING_DOTS; j++) {
			int x = (RETRO_WIDTH / 2) + (Circle[j].x * EYE) / (EYE - z) + xo;
			int y = (RETRO_HEIGHT / 2) + (Circle[j].y * EYE) / (EYE - z) + yo;

			if (x >= 0 && x < RETRO_WIDTH && y >= 0 && y < RETRO_HEIGHT) {
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
		double angle = i * (double)RETRO_SINCOS_ANGLE / RING_DOTS;

		Circle[i].x = CIRCLE_RADIUS * COS(angle);
		Circle[i].y = CIRCLE_RADIUS * SIN(angle);
	}
}
