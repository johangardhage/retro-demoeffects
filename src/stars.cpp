//
// Stars
//
// A starfield of flat layers. Star i is the point (x, y) on a discrete
// layer z ∈ {1, 2, 3}. The layer is both the speed and the brightness:
//
//   x' = (x + z · SPEED · dt) mod WIDTH
//   color = z · (SHADES − 1) / LAYER_NEAR
//
// Near layers (z = 3) overtake far ones (z = 1), which is the whole of
// the parallax. Nothing is projected.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retropalette.h"

#define NUM_STARS 1000
#define SPEED 60 // pixels a second travelled by a star in the furthest layer
#define LAYER_NEAR 3 // layers, numbered from the furthest
#define LAYER_FAR 1
#define SHADES 64 // palette entries the layers are shaded over

Point3Df Stars[NUM_STARS];

void DEMO_Render(double deltatime)
{
	// Draw stars
	for (int i = 0; i < NUM_STARS; i++) {
		Stars[i].x = fmod(Stars[i].x + Stars[i].z * SPEED * deltatime, RETRO_WIDTH);

		int color = Stars[i].z * (SHADES - 1) / LAYER_NEAR;

		RETRO_PutPixel(Stars[i].x, Stars[i].y, color);
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, SHADES, RETRO_BLACK, RETRO_WHITE);

	// Init stars
	for (int i = 0; i < NUM_STARS; i++) {
		Stars[i].x = RANDOM(RETRO_WIDTH);
		Stars[i].y = RANDOM(RETRO_HEIGHT);
		Stars[i].z = RANDOM(LAYER_NEAR - LAYER_FAR + 1) + LAYER_FAR;
	}
}
