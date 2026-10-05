//
// Burning ship
//
// Escape-time burning ship. Same recurrence as fractalmandelbrot.cpp, but both
// parts of z are folded into the positive quadrant before it is squared:
//
//   z0 = 0
//   z' = (|Re z| + i |Im z|)² + p
//
// The fold is not analytic, so the set has none of the Mandelbrot set's round
// bulbs: it is a hull with masts, and it is not symmetric about the real axis.
// Screen y grows down and so does Im p here, which is the way up the ship is
// always shown.
//
// The color is the normalized iteration count mu = n + 1 − log2(log |z|); the
// interior is 255, black.
//
// On the real axis the fold changes nothing: z stays real and the orbit is the
// Mandelbrot set's own. So the waterline running out to the left of the hull
// is the Mandelbrot set's antenna and carries its windows, each one a small
// ship, and the largest, the period-3 window near −1.755, is the ship the set
// is known by. The dive is into that one. A point on one of its masts, TARGET,
// is held at the screen position (FOCUS_X, FOCUS_Y) while everything else
// spreads away from it:
//
//   p = TARGET + 3.75 (x + 1/2 − FOCUS_X, y + 1/2 − FOCUS_Y) / (zoom W)
//
// zoom is RATE^phase, so it is frame-rate independent. The phase lives on the
// dive that reaches ZOOM_LIMIT, where the mast has opened into its lattice of
// decks and arches and MAX_ITERATIONS still tells them apart.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define MAX_ITERATIONS 255
#define BAILOUT 256.0 // |z|², far enough out for the smooth escape estimate
#define TARGET_X (-1.7722) // a mast of the ship in the antenna
#define TARGET_Y (-0.0424)
#define FOCUS_X (RETRO_WIDTH * 0.22) // the pixel the target is held at
#define FOCUS_Y (RETRO_HEIGHT * 0.6)
#define ZOOM_RATE 1.4 // zoom per second
#define ZOOM_LIMIT 1500.0 // as deep as MAX_ITERATIONS resolves the mast

void DEMO_Render(RETRO_Time time)
{
	// Calculate phase. One dive, then back to the top.
	double phase = fmod(time.total, log(ZOOM_LIMIT) / log(ZOOM_RATE));

	double zoom = pow(ZOOM_RATE, phase);
	double scale = 3.75 / (zoom * RETRO_WIDTH);

	// Map pixel
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		double pi = scale * (y + 0.5 - FOCUS_Y) + TARGET_Y;

		for (int x = 0; x < RETRO_WIDTH; x++) {
			double pr = scale * (x + 0.5 - FOCUS_X) + TARGET_X;

			double newre = 0;
			double newim = 0;
			double lengthsquared = 0;

			int iterations = 0;

			// Iterate
			for (int i = 0; i < MAX_ITERATIONS; i++) {
				double oldre = fabs(newre);
				double oldim = fabs(newim);

				// (a+bi)² + p = (a² - b² + Re p) + (2ab + Im p)i
				newre = oldre * oldre - oldim * oldim + pr;
				newim = 2 * oldre * oldim + pi;

				lengthsquared = newre * newre + newim * newim;

				// Outside the bailout circle
				if (lengthsquared > BAILOUT) {
					break;
				}

				iterations++;
			}

			// The interior never escapes and keeps the raw count
			double smooth = iterations;
			if (iterations < MAX_ITERATIONS && lengthsquared > 1.0) {
				smooth += 1.0 - log2(0.5 * log(lengthsquared));
			}

			int color = CLAMP256(smooth);
			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. Night around the ship, then the fire: the slower a point
	// escapes the hotter it is, and what never escapes is the black hull
	RETRO_CreateGradientPalette(0, 8, RETRO_BLACK, RETRO_EMBERBLACK);
	RETRO_CreateGradientPalette(8, 24, RETRO_EMBERBLACK, RETRO_SCARLET);
	RETRO_CreateGradientPalette(24, 56, RETRO_SCARLET, RETRO_AMBER);
	RETRO_CreateGradientPalette(56, 120, RETRO_AMBER, RETRO_CREAM);
	RETRO_CreateGradientPalette(120, 255, RETRO_CREAM, RETRO_WHITE);
	RETRO_SetColor(255, RETRO_BLACK);
}
