//
// Stars, flying past
//
// A pinhole starfield. Each star holds a fixed world offset (x, y) in
// a screen-sized rectangle and a depth z. The depth shrinks, and
//
//   sx = W/2 + x · EYE / z
//   sy = H/2 + y · EYE / z
//
// so a star accelerates outward as it nears the eye. At z = EYE it
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
#include "lib/retropalette.h"
#include "lib/retrovector.h"

#define NUM_STARS 1000
#define SPEED 120 // depth traveled per second
#define EYE 250 // how far the eye sits from the screen
#define STAR_NEAR 1 // nearest a star gets before it has gone past
#define STAR_FAR 500 // and the depth it comes back at
#define SHADES 64 // palette entries the depth shading ramps over

static vec3 Stars[NUM_STARS];

// Direction is measured on the screen, so a star at STAR_FAR lands within a screen of the middle.
static void PlaceStar(vec3 *star, float depth)
{
	star->x = mix(-RETRO_WIDTH / 2, RETRO_WIDTH / 2, RAND());
	star->y = mix(-RETRO_HEIGHT / 2, RETRO_HEIGHT / 2, RAND());
	star->z = depth;
}

void DEMO_Render(RETRO_Time time)
{
	// Draw stars
	for (int i = 0; i < NUM_STARS; i++) {
		Stars[i].z -= SPEED * time.delta;

		if (Stars[i].z <= STAR_NEAR) {
			PlaceStar(&Stars[i], STAR_FAR);
		}

		int x = (RETRO_WIDTH / 2) + (Stars[i].x * EYE) / Stars[i].z;
		int y = (RETRO_HEIGHT / 2) + (Stars[i].y * EYE) / Stars[i].z;

		if (RETRO_OnScreen(x, y)) {
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
		PlaceStar(&Stars[i], mix(STAR_NEAR, STAR_FAR, RAND()));
	}
}
