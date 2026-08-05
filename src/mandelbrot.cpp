//
// Mandelbrot
//
// Escape-time Mandelbrot set. Same recurrence as julia.cpp, but c is the
// pixel and z starts at 0:
//
//   z0 = 0
//   z' = z² + p
//
// p is the same square-aspect map as Julia (both axes divide by W, pixel
// centres, 3 / (zoom W)). The color is the escape count + 32; 223–255
// and the interior share 255 (black in the default 8-bit palette). The
// window zooms toward the left cusp while moveX tracks the feature under
// the centre (divided by zoom, so the pan is in pixels of the plane, not
// a fixed world speed).
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

#define MAX_ITERATIONS 255

void DEMO_Render(double deltatime)
{
	// Each iteration: z' = z² + p. p is the pixel; z starts at 0.
	static double zoom = 1;
	static double zoomSpd = 0.005f;
	static double moveX = -0.5;
	static double moveY = 0;

	// Map pixel
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			// p from the pixel centre, zoom and pan. Both axes divide by W so
			// the lattice is square (using H for Im would stretch the set).
			double pr = 3.0 * (x + 0.5 - RETRO_WIDTH / 2.0) / (zoom * RETRO_WIDTH) + moveX;
			double pi = 3.0 * (y + 0.5 - RETRO_HEIGHT / 2.0) / (zoom * RETRO_WIDTH) + moveY;

			double newRe = 0;
			double newIm = 0;

			int iterations = 0;

			// Iterate
			for (int i = 0; i < MAX_ITERATIONS; i++) {
				double oldRe = newRe;
				double oldIm = newIm;

				// (a+bi)² + p = (a² - b² + Re p) + (2ab + Im p)i
				newRe = oldRe * oldRe - oldIm * oldIm + pr;
				newIm = 2 * oldRe * oldIm + pi;

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
	zoomSpd *= pow(1.005, deltatime * 60);
	moveX -= 0.0050109f * deltatime * 60 / zoom;
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetPalette(RETRO_Default8bitPalette);
}
