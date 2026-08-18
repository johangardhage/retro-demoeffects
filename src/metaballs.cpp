//
// Metaballs
//
// An implicit field, the 1/r² cousin of the compact Wyvill kernel in
// blobs.cpp. Blinn's original used a Gaussian; this is the algebraic
// form. At pixel p
//
//   F(p) = sum_i  T R_i² / |p − c_i|²
//
// The solid F ≥ T is drawn (the superlevel set, not only the curve
// F = T). |p − c_i|² is a quadratic in x, so along a scanline it is
// stepped by forward differences rather than recomputed.
// One ball alone meets T on the circle of radius R_i:
// T R² / R² = T. Where fields overlap, F exceeds T between them, so
// the discs merge. |p−c|² is floored at 10⁻⁴ so a sample on a centre
// does not divide by zero. The balls fly at constant speed and bounce
// elastically (reflect pos, flip v) off the box.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retrocolor.h"

#define NUM_BALLS 5
#define THRESHOLD 50 // F = T on the circle of radius R around one ball

struct MetaBall {
	Point2Df pos;
	Point2Df vel;
	float radius;
} Balls[NUM_BALLS];

void DEMO_Render(double deltatime)
{
	// Charge of each ball
	float charge[NUM_BALLS];
	for (int i = 0; i < NUM_BALLS; i++) {
		charge[i] = THRESHOLD * Balls[i].radius * Balls[i].radius;
	}

	// Draw balls, rows outside so the framebuffer is walked in order. |p - c|² is
	// a quadratic in x, so it is stepped by forward differences:
	// d(x+1) - d(x) = 2(x - cx) + 1, which itself grows by 2 each pixel.
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		float distancesquared[NUM_BALLS];
		float slope[NUM_BALLS];
		for (int i = 0; i < NUM_BALLS; i++) {
			float a = -Balls[i].pos.x;
			float b = y - Balls[i].pos.y;
			distancesquared[i] = a * a + b * b;
			slope[i] = 2 * a + 1;
		}

		for (int x = 0; x < RETRO_WIDTH; x++) {
			float sum = 0;
			// Calculate iso-surface
			for (int i = 0; i < NUM_BALLS; i++) {
				sum += charge[i] / MAX(distancesquared[i], 0.0001f);
				distancesquared[i] += slope[i];
				slope[i] += 2;
			}
			// Threshold
			if (sum >= THRESHOLD) {
				RETRO_PutPixel(x, y, 255);
			}
		}
	}

	// Move balls
	for (int i = 0; i < NUM_BALLS; i++) {
		Balls[i].pos.x += Balls[i].vel.x * deltatime;
		Balls[i].pos.y += Balls[i].vel.y * deltatime;
		while (Balls[i].pos.x < 0 || Balls[i].pos.x > RETRO_WIDTH - 1) {
			if (Balls[i].pos.x < 0) Balls[i].pos.x = -Balls[i].pos.x;
			else Balls[i].pos.x = 2 * (RETRO_WIDTH - 1) - Balls[i].pos.x;
			Balls[i].vel.x = -Balls[i].vel.x;
		}
		while (Balls[i].pos.y < 0 || Balls[i].pos.y > RETRO_HEIGHT - 1) {
			if (Balls[i].pos.y < 0) Balls[i].pos.y = -Balls[i].pos.y;
			else Balls[i].pos.y = 2 * (RETRO_HEIGHT - 1) - Balls[i].pos.y;
			Balls[i].vel.y = -Balls[i].vel.y;
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetColor(255, RETRO_WHITE);

	// Init balls
	for (int i = 0; i < NUM_BALLS; i++) {
		Balls[i].pos.x = RANDOM(RETRO_WIDTH);
		Balls[i].pos.y = RANDOM(RETRO_HEIGHT);
		Balls[i].vel.x = RANDOMF(240) - 120;
		Balls[i].vel.y = RANDOMF(240) - 120;
		Balls[i].radius = RANDOM(10) + 10;
	}
}
