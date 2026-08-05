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
// After the copy-swap, a 5-tap mean is applied in place to the new field
// only. h_prev stays the unblurred previous h. That extra viscosity is
// not part of the leapfrog, and it is Gauss–Seidel along the scan.
//
// The picture is sampled at a first-order slope offset:
// (x, y) reads I(x − k (h_{x+1} − h_{x−1}), y − k (h_{y+1} − h_{y−1})).
// The missing /2 of the central difference sits in k. This is not Snell.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define WATER_DAMP 0.985 // kept per step, so how slowly a ripple dies
#define WATER_REFRACT 18.0 // texels a unit slope slides the sample
#define WATER_DEPTH 300.0 // height subtracted by one droplet
#define WATER_DROP_STEPS 36 // steps between droplets

float Water[RETRO_WIDTH * RETRO_HEIGHT];
float Water2[RETRO_WIDTH * RETRO_HEIGHT];

//
// Advance the field one fixed step
//
void DEMO_Update(double deltatime)
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

	// h' into Water2; Water2[i] is still h_prev when the right-hand side is read
	for (int y = 1; y < RETRO_HEIGHT - 1; y++) {
		for (int x = 1; x < RETRO_WIDTH - 1; x++) {
			int i = y * RETRO_WIDTH + x;
			Water2[i] = ((Water[i - 1] + Water[i + 1] + Water[i - RETRO_WIDTH] + Water[i + RETRO_WIDTH]) * 0.5f - Water2[i]) * WATER_DAMP;
		}
	}

	// Water2 was h', Water was h. Swap so Water is current and Water2 is h_prev.
	for (int y = 1; y < RETRO_HEIGHT - 1; y++) {
		for (int x = 1; x < RETRO_WIDTH - 1; x++) {
			int i = y * RETRO_WIDTH + x;
			float w = Water2[i];
			Water2[i] = Water[i];
			Water[i] = w;
		}
	}

	// Five-tap average, extra damping beyond WATER_DAMP
	for (int y = 1; y < RETRO_HEIGHT - 1; y++) {
		for (int x = 1; x < RETRO_WIDTH - 1; x++) {
			int i = y * RETRO_WIDTH + x;
			Water[i] = (Water[i] + Water[i - 1] + Water[i + 1] + Water[i - RETRO_WIDTH] + Water[i + RETRO_WIDTH]) * 0.2f;
		}
	}
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
			int rx = CLAMP(x - (int)(nx * WATER_REFRACT), 1, RETRO_WIDTH - 1);
			int ry = CLAMP(y - (int)(ny * WATER_REFRACT), 1, RETRO_HEIGHT - 1);
			RETRO_PutPixel(x, y, image[ry * RETRO_WIDTH + rx]);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/monkey_320x240.pcx");
	RETRO_SetPalette(RETRO_ImagePalette());
}
