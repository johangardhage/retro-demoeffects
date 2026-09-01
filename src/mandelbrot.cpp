//
// Mandelbrot
//
// Escape-time Mandelbrot set. Same recurrence as julia.cpp, but c is the
// pixel and z starts at 0:
//
//   z0 = 0
//   z' = z² + p
//
// p is the same square-aspect map as Julia. The colour is the normalised
// iteration count mu = n + 1 − log2(log |z|), offset by 32; the interior and
// counts past 223 share 255, black in the default 8-bit palette.
//
// Points inside the main cardioid and the period-2 bulb never escape, and
// both regions have a closed form:
//
//   cardioid: q = (x − 1/4)² + y²,  q (q + x − 1/4) ≤ y² / 4
//   bulb:     (x + 1)² + y² ≤ 1/16
//
// The window dives toward the left cusp while the centre pans onto the
// feature underneath it. zoom is RATE^phase and the pan is its integral, so
// both are frame-rate independent. The phase lives on the dive that reaches
// ZOOM_LIMIT, past which double no longer separates neighbouring pixels.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define MAX_ITERATIONS 255
#define BAILOUT 256.0 // |z|², far enough out for the smooth escape estimate
#define CENTER_X (-0.5) // where the dive starts
#define CENTER_Y 0.0
#define PAN_SPEED 0.300654 // plane units a second at zoom 1, so the pan holds the feature
#define ZOOM_RATE 1.34885 // zoom per second
#define ZOOM_LIMIT 1.0e12 // as deep as double carries a distinct pixel spacing

void DEMO_Render(double deltatime)
{
	// Calculate phase. One dive, then back to the top.
	static double phase = 0;
	phase = fmod(phase + deltatime, log(ZOOM_LIMIT) / log(ZOOM_RATE));

	double zoom = pow(ZOOM_RATE, phase);
	double scale = 3.0 / (zoom * RETRO_WIDTH);

	// The integral of PAN_SPEED / zoom
	double moveX = CENTER_X - PAN_SPEED * (1.0 - 1.0 / zoom) / log(ZOOM_RATE);
	double moveY = CENTER_Y;

	// Map pixel
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		double pi = scale * (y + 0.5 - RETRO_HEIGHT / 2.0) + moveY;

		for (int x = 0; x < RETRO_WIDTH; x++) {
			double pr = scale * (x + 0.5 - RETRO_WIDTH / 2.0) + moveX;

			// Inside the cardioid or the period-2 bulb the orbit is bounded
			double cardioidx = pr - 0.25;
			double q = cardioidx * cardioidx + pi * pi;
			if (q * (q + cardioidx) <= 0.25 * pi * pi ||
				(pr + 1) * (pr + 1) + pi * pi <= 0.0625) {
				RETRO_PutPixel(x, y, 255);
				continue;
			}

			double newRe = 0;
			double newIm = 0;
			double lengthsquared = 0;

			int iterations = 0;

			// Iterate
			for (int i = 0; i < MAX_ITERATIONS; i++) {
				double oldRe = newRe;
				double oldIm = newIm;

				// (a+bi)² + p = (a² - b² + Re p) + (2ab + Im p)i
				newRe = oldRe * oldRe - oldIm * oldIm + pr;
				newIm = 2 * oldRe * oldIm + pi;

				lengthsquared = newRe * newRe + newIm * newIm;

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

			int color = CLAMP(smooth + 32, 32, 256);
			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetPalette(RETRO_Default8bitPalette);
}
