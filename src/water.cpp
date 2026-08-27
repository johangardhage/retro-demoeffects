//
// Water
//
// A height field evolved by the 2D wave equation, then used to refract a still
// picture. The field h satisfies
//
//   ∂²h/∂t² = c² ∇²h
//
// Leapfrog with the five-point Laplacian and λ = c² Δt² / Δx² = 1/2
// (the 2D CFL bound: the Nyquist mode is marginal) collapses to
//
//   h' = (h_left + h_right + h_up + h_down) / 2 - h_prev
//
// then multiplied by WATER_DAMP. Long-wave speed is 1/√2 cells a step;
// the stencil's light-cone is one cell, which is why the field is
// advanced at a fixed rate. The one-pixel frame is held at 0 (Dirichlet).
// A droplet is a point impulse in h, not a velocity kick.
//
// A (4, 1, 1, 1, 1) / 8 smoothing pass then damps the new field, out of
// place, over three rotating buffers. Extra viscosity, not part of the
// leapfrog. The weights are not a flat mean because the scheme's stability
// depends on them: with N = 2 cos kx + 2 cos ky and B the smoothing's
// symbol, one step is
//
//   g² − DAMP·B·N/2·g + DAMP·B = 0
//
// and a flat mean's B = −3/5 at Nyquist, where N = −4, puts a root at 1.56.
// B = (4 + N) / 8 is never negative, so |g| = sqrt(DAMP·B) < 1 everywhere.
//
// The picture is sampled at a first-order slope offset:
// (x, y) reads I(x − k (h_{x+1} − h_{x−1}), y − k (h_{y+1} − h_{y−1})).
// The missing /2 of the central difference sits in k. This is not Snell.
// The offset is rounded, not truncated, so slopes under one texel of shift
// still refract.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define WATER_DAMP 0.985 // kept per step, so how slowly a ripple dies
#define WATER_REFRACT 18.0 // texels a unit slope slides the sample
#define WATER_DEPTH 300.0 // height subtracted by one droplet
#define WATER_DROP_STEPS 36 // steps between droplets

// Rotated rather than copied. The one-pixel frame is never written, so it
// stays the Dirichlet boundary.
float WaterA[RETRO_WIDTH * RETRO_HEIGHT];
float WaterB[RETRO_WIDTH * RETRO_HEIGHT];
float WaterC[RETRO_WIDTH * RETRO_HEIGHT];

float *Water = WaterA;      // h
float *WaterPrevious = WaterB;  // h_prev
float *WaterScratch = WaterC;   // what the smoothing pass writes into

//
// Advance the field one fixed step
//
void DEMO_FixedUpdate(double timestep)
{
	// Seed droplet
	static int tick = 0;
	if (tick % WATER_DROP_STEPS == 0) {
		int x = RANDOM(RETRO_WIDTH);
		int y = RANDOM(RETRO_HEIGHT);

		if (x > 0 && x < RETRO_WIDTH - 1 && y > 0 && y < RETRO_HEIGHT - 1) {
			Water[y * RETRO_WIDTH + x] -= WATER_DEPTH;
		}
	}
	tick = (tick + 1) % WATER_DROP_STEPS;

	// h' over the previous field, which is dead once it has been read
	for (int y = 1; y < RETRO_HEIGHT - 1; y++) {
		for (int x = 1; x < RETRO_WIDTH - 1; x++) {
			int i = y * RETRO_WIDTH + x;
			WaterPrevious[i] = ((Water[i - 1] + Water[i + 1] + Water[i - RETRO_WIDTH] + Water[i + RETRO_WIDTH]) * 0.5f - WaterPrevious[i]) * WATER_DAMP;
		}
	}

	// Five-tap smoothing, extra damping beyond WATER_DAMP. Out of place, so every
	// tap is the field as the leapfrog left it.
	for (int y = 1; y < RETRO_HEIGHT - 1; y++) {
		for (int x = 1; x < RETRO_WIDTH - 1; x++) {
			int i = y * RETRO_WIDTH + x;
			WaterScratch[i] = (4 * WaterPrevious[i] + WaterPrevious[i - 1] + WaterPrevious[i + 1] + WaterPrevious[i - RETRO_WIDTH] + WaterPrevious[i + RETRO_WIDTH]) * 0.125f;
		}
	}

	// Rotate: the smoothed field becomes h, h becomes h_prev, and the unsmoothed
	// h' is free to write over.
	float *unsmoothed = WaterPrevious;
	WaterPrevious = Water;
	Water = WaterScratch;
	WaterScratch = unsmoothed;
}

void DEMO_Render(double deltatime)
{
	unsigned char *image = RETRO_ImageData();

	// Draw water
	for (int y = 1; y < RETRO_HEIGHT - 1; y++) {
		for (int x = 1; x < RETRO_WIDTH - 1; x++) {
			int i = y * RETRO_WIDTH + x;
			float nx = Water[i + 1] - Water[i - 1];
			float ny = Water[i + RETRO_WIDTH] - Water[i - RETRO_WIDTH];
			int rx = CLAMP(x - (int)lroundf(nx * WATER_REFRACT), 1, RETRO_WIDTH - 1);
			int ry = CLAMP(y - (int)lroundf(ny * WATER_REFRACT), 1, RETRO_HEIGHT - 1);
			RETRO_PutPixel(x, y, image[ry * RETRO_WIDTH + rx]);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/monkey_320x240.pcx", true);
}
