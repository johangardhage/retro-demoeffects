//
// Newton fractal
//
// Newton's method for a root of a cubic, run from every pixel at once. The
// cubic is given by its three roots,
//
//   p(z) = (z − r0)(z − r1)(z − r2)
//
// and the step z' = z − p(z) / p'(z) needs neither p nor p' multiplied out,
// because the logarithmic derivative of a product is a sum:
//
//   p'(z) / p(z) = Σ 1 / (z − r_k)
//   z' = z − 1 / Σ 1 / (z − r_k)
//
// Pixel (x, y) is the start z0 = 4 (x + 1/2 − W/2, y + 1/2 − H/2) / W. Iterate
// until z is within TOLERANCE of a root, or MAX_ITERATIONS. The hue is the root
// reached. Each root owns the plane around itself, but where two basins meet
// the third is always there too, so the border is a chain of ever smaller
// beads of all three colors.
//
// The shade is how long it took. Close to a root the distance squares at every
// step, so its logarithm doubles, and the count is made continuous by where
// between the last two steps log d crossed log TOLERANCE:
//
//   mu = n − 1 + log(TOLERANCE / d_(n−1)) / log(d_n / d_(n−1))
//
// The roots ride a slowly turning triangle, root k at the angle 2π (phase +
// k/3), each breathing in and out of the unit circle k + 1 times a turn:
//
//   |r_k| = 1 + PULSE sin(2π (k + 1) phase)
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define ROOTS 3
#define MAX_ITERATIONS 32
#define TOLERANCE 1.0e-6 // |z − r|², close enough to call the root reached
#define SHADES 85 // palette entries a basin owns
#define SHADE_ITERATIONS 20.0 // the count that reaches the top of a basin's ramp
#define TURN_SPEED (1.0 / 40) // turns of the triangle a second
#define PULSE 0.5 // how far a root leaves the unit circle

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase. One turn of the triangle.
	double phase = fmod(time * TURN_SPEED, 1.0);

	// Place the roots
	double rootre[ROOTS];
	double rootim[ROOTS];
	for (int k = 0; k < ROOTS; k++) {
		double angle = 2 * M_PI * (phase + (double)k / ROOTS);
		double radius = 1 + PULSE * sin(2 * M_PI * (k + 1) * phase);
		rootre[k] = radius * cos(angle);
		rootim[k] = radius * sin(angle);
	}

	double scale = 4.0 / RETRO_WIDTH;

	// Map pixel
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			double zr = scale * (x + 0.5 - RETRO_WIDTH / 2.0);
			double zi = scale * (y + 0.5 - RETRO_HEIGHT / 2.0);

			double previous = 0;
			int color = 0;

			// Iterate. A start that lands on a critical point divides by zero and
			// never arrives: it stays color 0
			for (int i = 0; i < MAX_ITERATIONS; i++) {
				double sumre = 0;
				double sumim = 0;
				double nearest = INFINITY;
				int root = 0;

				// 1 / (a+bi) = (a - bi) / (a² + b²)
				for (int k = 0; k < ROOTS; k++) {
					double dr = zr - rootre[k];
					double di = zi - rootim[k];
					double lengthsquared = dr * dr + di * di;

					sumre += dr / lengthsquared;
					sumim -= di / lengthsquared;

					if (lengthsquared < nearest) {
						nearest = lengthsquared;
						root = k;
					}
				}

				// At the root
				if (nearest < TOLERANCE) {
					double smooth = i;
					if (i > 0) {
						smooth += log(TOLERANCE / previous) / log(nearest / previous) - 1;
					}

					int shade = CLAMP(smooth * SHADES / SHADE_ITERATIONS, 0, SHADES);
					color = 1 + root * SHADES + shade;
					break;
				}
				previous = nearest;

				// z' = z - 1 / sum
				double lengthsquared = sumre * sumre + sumim * sumim;
				zr -= sumre / lengthsquared;
				zi += sumim / lengthsquared;
			}

			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. One ramp a root, deep where the root is reached at once and
	// glowing along the borders, where it takes longest
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_CreateGradientPalette(1, 1 + SHADES / 2, RETRO_BLUEBLACK, RETRO_AZURE);
	RETRO_CreateGradientPalette(1 + SHADES / 2, 1 + SHADES, RETRO_AZURE, RETRO_WHITE);
	RETRO_CreateGradientPalette(1 + SHADES, 1 + SHADES + SHADES / 2, RETRO_EMBERBLACK, RETRO_SCARLET);
	RETRO_CreateGradientPalette(1 + SHADES + SHADES / 2, 1 + 2 * SHADES, RETRO_SCARLET, RETRO_WHITE);
	RETRO_CreateGradientPalette(1 + 2 * SHADES, 1 + 2 * SHADES + SHADES / 2, RETRO_MOSSBLACK, RETRO_SPRINGGREEN);
	RETRO_CreateGradientPalette(1 + 2 * SHADES + SHADES / 2, 1 + 3 * SHADES, RETRO_SPRINGGREEN, RETRO_WHITE);
}
