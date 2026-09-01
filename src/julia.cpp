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
// until |z|² > BAILOUT or MAX_ITERATIONS.
//
// The colour is the normalised iteration count, the continuous potential
// estimate
//
//   mu = n + 1 − log2(log |z|)
//
// which needs a bailout well past 2 for the logarithm; 16 (BAILOUT = 256) is
// the usual choice. Offset by 32, so 0..31 stay unused. Counts past 223 and
// the interior share 255, black in the default 8-bit palette.
//
// The zoom is exponential and pixel spacing is 3 / (zoom W), so the dive
// lives on the phase that reaches ZOOM_LIMIT, past which double no longer
// separates neighbouring pixels, and then restarts. zoom = RATE^phase is a
// closed form, so it is frame-rate independent.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define MAX_ITERATIONS 255
#define BAILOUT 256.0 // |z|², far enough out for the smooth escape estimate
#define CONST_RE -0.7 // real part of c, which selects the Julia set
#define CONST_IM 0.27015 // imaginary part of c
#define CENTER_X 0.01101 // the point the dive closes on
#define CENTER_Y 0.0101
#define ZOOM_RATE 3.281 // zoom per second
#define ZOOM_LIMIT 1.0e12 // as deep as double carries a distinct pixel spacing

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase. One dive, then back to the top.
	double phase = fmod(time, log(ZOOM_LIMIT) / log(ZOOM_RATE));

	double zoom = pow(ZOOM_RATE, phase);
	double scale = 3.0 / (zoom * RETRO_WIDTH);

	// Map pixel
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		// z0 from the pixel centre. Both axes use the same scale, so the
		// lattice is square.
		double pi0 = scale * (y + 0.5 - RETRO_HEIGHT / 2.0) + CENTER_Y;

		for (int x = 0; x < RETRO_WIDTH; x++) {
			double pr0 = scale * (x + 0.5 - RETRO_WIDTH / 2.0) + CENTER_X;

			double newRe = pr0;
			double newIm = pi0;
			double lengthsquared = newRe * newRe + newIm * newIm;

			int iterations = 0;

			// Iterate
			for (int i = 0; i < MAX_ITERATIONS; i++) {
				double oldRe = newRe;
				double oldIm = newIm;

				// (a+bi)² + c = (a² - b² + Re c) + (2ab + Im c)i
				newRe = oldRe * oldRe - oldIm * oldIm + CONST_RE;
				newIm = 2 * oldRe * oldIm + CONST_IM;

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
