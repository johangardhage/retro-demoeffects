//
// Stars, flying past
//
// A pinhole starfield. Each star holds a fixed world offset (x, y) in
// a screen-sized rectangle and a depth z. The depth shrinks, and
//
//   sx = W/2 + x · EYE / z
//   sy = H/2 + y · EYE / z
//
// so a star accelerates outwards as it nears the eye. At z = EYE it
// sits on the screen plane (at the stored offset). At z ≤ STAR_NEAR it
// is reborn at STAR_FAR on a fresh offset — that test is before the
// divide. Directions are uniform on the rectangle, not on a sphere.
// Brightness is the remaining depth:
//
//   color = (STAR_FAR − z) · (SHADES − 1) / (STAR_FAR − STAR_NEAR)
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retropalette.h"

#define NUM_STARS 1000
#define SPEED 120 // depth travelled per second
#define EYE 250 // how far the eye sits from the screen
#define STAR_NEAR 1 // nearest a star gets before it has gone past
#define STAR_FAR 500 // and the depth it comes back at
#define SHADES 64 // palette entries the depth shading ramps over

Point3Df Stars[NUM_STARS];

// Direction is measured on the screen, so a star at STAR_FAR lands within a screen of the middle.
void PlaceStar(Point3Df *star, float depth)
{
	star->x = RANDOM(RETRO_WIDTH) - (RETRO_WIDTH / 2);
	star->y = RANDOM(RETRO_HEIGHT) - (RETRO_HEIGHT / 2);
	star->z = depth;
}

void DEMO_Render(double deltatime)
{
	// Draw stars
	for (int i = 0; i < NUM_STARS; i++) {
		Stars[i].z -= SPEED * deltatime;

		if (Stars[i].z <= STAR_NEAR) {
			PlaceStar(&Stars[i], STAR_FAR);
		}

		int x = (RETRO_WIDTH / 2) + (Stars[i].x * EYE) / Stars[i].z;
		int y = (RETRO_HEIGHT / 2) + (Stars[i].y * EYE) / Stars[i].z;

		if (x >= 0 && x < RETRO_WIDTH && y >= 0 && y < RETRO_HEIGHT) {
			int color = (STAR_FAR - Stars[i].z) * (SHADES - 1) / (STAR_FAR - STAR_NEAR);

			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, SHADES, RETRO_BLACK, RETRO_WHITE);

	// Init stars. Spread through the whole field so it starts full rather than filling from the back.
	for (int i = 0; i < NUM_STARS; i++) {
		PlaceStar(&Stars[i], RANDOM(STAR_FAR - STAR_NEAR) + STAR_NEAR);
	}
}
