//
// Julia
//
// Escape-time Julia set of z ↦ z² + c, with a fixed
// c = CONST_RE + i CONST_IM. Pixel (x, y) is the start
//
//   z0 = 3 (x + 1/2 − W/2) / (zoom W) + moveX
//      + i [ 3 (y + 1/2 − H/2) / (zoom W) + moveY ]
//
// Both axes divide by W so the pixel lattice is square. The +1/2 is the
// pixel centre: (x − W/2) on even W is one step heavy on the left.
// Iterate
//
//   z' = z² + c
//
// until |z| > 2 (tested as Re² + Im² > 4) or MAX_ITERATIONS. The
// color is the escape count + 32, so 0..31 stay unused. Counts 223–255
// and the interior (never escapes) share 255 — 223 + 32 lands on it and
// higher counts clamp — which is black in the default 8-bit palette. zoom grows and zoomSpd compounds, so the
// window tightens around (moveX, moveY).
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

#define MAX_ITERATIONS 255
#define CONST_RE -0.7 // real part of c, which selects the Julia set
#define CONST_IM 0.27015 // imaginary part of c

void DEMO_Render(double deltatime)
{
	// Each iteration: z' = z² + c. c is fixed; z starts at the pixel.
	static double zoom = 1;
	static double zoomSpd = 0.002f;
	static double moveX = 0.01101;
	static double moveY = 0.0101;

	// Map pixel
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			// z0 from the pixel centre, zoom and pan. Both axes divide by W so
			// the lattice is square (using H for Im would stretch the set).
			double pr = 3.0 * (x + 0.5 - RETRO_WIDTH / 2.0) / (zoom * RETRO_WIDTH) + moveX;
			double pi = 3.0 * (y + 0.5 - RETRO_HEIGHT / 2.0) / (zoom * RETRO_WIDTH) + moveY;

			double newRe = pr;
			double newIm = pi;

			int iterations = 0;

			// Iterate
			for (int i = 0; i < MAX_ITERATIONS; i++) {
				double oldRe = newRe;
				double oldIm = newIm;

				// (a+bi)² + c = (a² - b² + Re c) + (2ab + Im c)i
				newRe = oldRe * oldRe - oldIm * oldIm + CONST_RE;
				newIm = 2 * oldRe * oldIm + CONST_IM;

				// Outside the circle of radius 2: |z|² > 4
				if ((newRe * newRe + newIm * newIm) > 4) {
					break;
				}

				iterations++;
			}

			// Color is the escape count, offset 32
			int color = CLAMP(iterations + 32, 32, 256);
			RETRO_PutPixel(x, y, color);
		}
	}

	// Zoom
	zoom += zoomSpd * deltatime * 60;
	zoomSpd *= pow(1.02, deltatime * 60);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetPalette(RETRO_Default8bitPalette);
}
