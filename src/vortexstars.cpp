//
// Vortex starfield
//
// Stars on cylinders, flying toward the eye with a twist. Star i holds a
// radius R, an angle θ and a depth z. The pinhole is the same as stars2
//
//   sx = W/2 + R cos(θ + k/z) · EYE / z
//   sy = H/2 + R sin(θ + k/z) · EYE / z
//
// except the angle is taken at θ + k/z rather than at θ. k/z is a
// stronger twist near the eye, so the cylinders read as a vortex
// rather than as a straight tube. z shrinks at SPEED; at z ≤ STAR_NEAR
// the star is reborn at STAR_FAR on a fresh angle. Brightness is the
// remaining depth, as in stars2. Three radii keep the tube from being
// a single ring.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocolor.h"

#define NUM_STARS 1400
#define SPEED 180 // depth travelled per second
#define SPIN 1.4 // radians of θ per second, the cylinder's own turn
#define EYE 250 // how far the eye sits from the screen
#define STAR_NEAR 12 // nearest a star gets before it has gone past
#define STAR_FAR 520 // and the depth it comes back at
#define TWIST 90 // extra radians of θ at z = 1; at z it is this over z
#define SHADES 64 // palette entries the depth shading ramps over

struct VortexStar {
	float angle;
	float radius;
	float z;
} Stars[NUM_STARS];

static const float BandRadius[3] = { 55, 90, 130 };

void PlaceStar(VortexStar *star, float depth)
{
	star->angle = RANDOMF(2 * M_PI);
	star->radius = BandRadius[RANDOM(3)];
	star->z = depth;
}

void DEMO_Render(double deltatime)
{
	float cx = RETRO_WIDTH / 2.0f;
	float cy = RETRO_HEIGHT / 2.0f;

	for (int i = 0; i < NUM_STARS; i++) {
		Stars[i].z -= SPEED * deltatime;
		Stars[i].angle += SPIN * deltatime;

		if (Stars[i].z <= STAR_NEAR) {
			PlaceStar(&Stars[i], STAR_FAR);
		}

		float a = Stars[i].angle + TWIST / Stars[i].z;
		float q = EYE / Stars[i].z;
		int x = cx + Stars[i].radius * cos(a) * q;
		int y = cy + Stars[i].radius * sin(a) * q;

		if (x >= 0 && x < RETRO_WIDTH && y >= 0 && y < RETRO_HEIGHT) {
			int color = (STAR_FAR - Stars[i].z) * (SHADES - 1) / (STAR_FAR - STAR_NEAR);
			RETRO_PutPixel(x, y, color);

			// Near stars take a neighbour so they read as a streak along the
			// vortex rather than as a single pixel.
			if (color > SHADES / 2) {
				int x2 = x + (x > cx ? 1 : -1);
				int y2 = y + (y > cy ? 1 : -1);
				if (x2 >= 0 && x2 < RETRO_WIDTH && y2 >= 0 && y2 < RETRO_HEIGHT) {
					RETRO_PutPixel(x2, y, color);
					RETRO_PutPixel(x, y2, color / 2);
				}
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_CreateGradientPalette(0, SHADES, RETRO_BLACK, RETRO_WHITE);

	for (int i = 0; i < NUM_STARS; i++) {
		PlaceStar(&Stars[i], RANDOM(STAR_FAR - STAR_NEAR) + STAR_NEAR);
	}
}
