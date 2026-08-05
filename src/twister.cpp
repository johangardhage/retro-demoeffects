//
// Twister
//
// A square column, one scanline at a time. At row i the four vertices sit
// on the circle x = CX + R sin(θ + k π/2), k = 0..3, with
//
//   θ(i) = angle + 2 i / HEIGHT
//
// so the column twists two radians over the picture. An edge is drawn only
// when its left x is smaller than its right x: that is the facing test for
// a 2D silhouette (a back edge has x_left > x_right). Each face has its
// own color. angle lives on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

#define TWISTER_CX 100
#define TWISTER_RADIUS 60
#define TWISTER_TWIST 2.0 // radians of twist from row 0 to row HEIGHT
#define TWISTER_SPEED 1.2 // radians per second
#define TWISTER_PERIOD (2 * M_PI)

void DEMO_Render(double deltatime)
{
	// Calculate angle
	static double angle = 0;
	angle = fmod(angle + deltatime * TWISTER_SPEED, TWISTER_PERIOD);

	// Draw column
	for (int i = 0; i < RETRO_HEIGHT; i++) {
		double theta = angle + TWISTER_TWIST * i / RETRO_HEIGHT;
		double x0 = TWISTER_CX + TWISTER_RADIUS * sin(theta);
		double x1 = TWISTER_CX + TWISTER_RADIUS * sin(theta + M_PI / 2);
		double x2 = TWISTER_CX + TWISTER_RADIUS * sin(theta + M_PI);
		double x3 = TWISTER_CX + TWISTER_RADIUS * sin(theta + 3 * M_PI / 2);

		if (x0 < x1) {
			RETRO_DrawLine(x0, i, x1, i, 1);
		}
		if (x1 < x2) {
			RETRO_DrawLine(x1, i, x2, i, 2);
		}
		if (x2 < x3) {
			RETRO_DrawLine(x2, i, x3, i, 3);
		}
		if (x3 < x0) {
			RETRO_DrawLine(x3, i, x0, i, 4);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetPalette(RETRO_Default8bitPalette);
}
