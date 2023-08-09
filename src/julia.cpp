//
// julia.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

#define MAX_ITERATIONS 255
#define CONST_RE -0.7 // Real part of the constant c, determinate shape of the Julia Set
#define CONST_IM 0.27015 // Imaginary part of the constant c, determinate shape of the Julia Set

void DEMO_Render(double deltatime)
{
	// Each iteration, it calculates: new = old*old + c, where c is a constant and old starts at current pixel
	static double zoom = 1;
	static double zoomSpd = 0.002f;
	static double moveX = 0.01101;
	static double moveY = 0.0101;

	// Loop through every pixel
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			// Calculate the initial real and imaginary part of z, based on the pixel location and zoom and position values
			double pr = 1.5 * (x - RETRO_WIDTH / 2) / (0.5 * zoom * RETRO_WIDTH) + moveX;
			double pi = (y - RETRO_HEIGHT / 2) / (0.5 * zoom * RETRO_HEIGHT) + moveY;

			double newRe = pr;
			double newIm = pi;

			int iterations = 0;

			// Start the iteration process
			for (int i = 0; i < MAX_ITERATIONS; i++) {
				// Remember value of previous iteration
				double oldRe = newRe;
				double oldIm = newIm;

				// Calculate the real and imaginary part
				newRe = oldRe * oldRe - oldIm * oldIm + CONST_RE;
				newIm = 2 * oldRe * oldIm + CONST_IM;

				// If the point is outside the circle with radius 2: stop
				if ((newRe * newRe + newIm * newIm) > 4) {
					break;
				}

				iterations++;
			}

			// Draw the pixel
			int color = CLAMP(iterations + 32, 32, 256);
			RETRO_PutPixel(x, y, color);
		}
	}

	zoom += zoomSpd;
	zoomSpd *= 1.02;
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetPalette(RETRO_Default8bitPalette);
}
