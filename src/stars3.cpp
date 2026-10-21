//
// Stars, vortex
//
// Stars on cylinders, flying toward the eye with a twist. Star i holds a
// radius R, an angle θ and a depth z, and is projected through a pinhole
//
//   sx = W/2 + R cos(θ + k/z) · EYE / z
//   sy = H/2 + R sin(θ + k/z) · EYE / z
//
// with the angle taken at θ + k/z rather than at θ. k/z is a stronger twist
// near the eye, so the cylinders read as a vortex rather than as a straight
// tube. z shrinks at SPEED; at z ≤ STAR_NEAR the star is reborn at STAR_FAR
// on a fresh angle. Brightness is the remaining depth. Three radii keep the
// tube from being a single ring.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

constexpr int NUM_STARS = 1400;
constexpr int SPEED = 180; // depth traveled per second
constexpr double SPIN = 1.4; // radians of θ per second, the cylinder's own turn
constexpr int EYE = 250; // how far the eye sits from the screen
constexpr int STAR_NEAR = 12; // nearest a star gets before it has gone past
constexpr int STAR_FAR = 520; // and the depth it comes back at
constexpr int TWIST = 90; // extra radians of θ at z = 1; at z it is this over z
constexpr int SHADES = 64; // palette entries the depth shading ramps over

static struct VortexStar {
	float angle;
	float radius;
	float z;
} Stars[NUM_STARS];

static const float BandRadius[3] = { 55, 90, 130 };

static void PlaceStar(VortexStar *star, float depth)
{
	star->angle = RANDOMF(2 * M_PI);
	star->radius = BandRadius[RANDOM(3)];
	star->z = depth;
}

void DEMO_Render(RETRO_Time time)
{
	float cx = RETRO_WIDTH / 2.0f;
	float cy = RETRO_HEIGHT / 2.0f;

	for (int i = 0; i < NUM_STARS; i++) {
		Stars[i].z -= SPEED * time.delta;
		Stars[i].angle += SPIN * time.delta;

		if (Stars[i].z <= STAR_NEAR) {
			PlaceStar(&Stars[i], STAR_FAR);
		}

		float a = Stars[i].angle + TWIST / Stars[i].z;
		float q = EYE / Stars[i].z;
		double x = cx + Stars[i].radius * cos(a) * q;
		double y = cy + Stars[i].radius * sin(a) * q;

		if (RETRO_OnScreen((float)x, (float)y)) {
			int color = (STAR_FAR - Stars[i].z) * (SHADES - 1) / (STAR_FAR - STAR_NEAR);
			RETRO_PutPixel(x, y, color);

			// Near stars take a neighbor so they read as a streak along the
			// vortex rather than as a single pixel.
			if (color > SHADES / 2) {
				double x2 = x + (x > cx ? 1 : -1);
				double y2 = y + (y > cy ? 1 : -1);
				if (RETRO_OnScreen((float)x2, (float)y)) {
					RETRO_PutPixel(x2, y, color);
				}
				if (RETRO_OnScreen((float)x, (float)y2)) {
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
		PlaceStar(&Stars[i], mix(STAR_NEAR, STAR_FAR, RAND()));
	}
}
