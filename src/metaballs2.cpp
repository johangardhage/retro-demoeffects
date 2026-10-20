//
// Metaballs, as density
//
// A 1/r² field of metaballs, drawn as density instead of the solid F ≥ T:
//
//   F(p) = sum_i  r_i / |p − c_i|²
//   color = clamp(20 F, 0, 255)
//
// r_i is the charge T R_i². One ball of strength r meets color 20 on the
// circle of radius √r, where 20 r / (√r)² = 20. |p−c|² is floored at 10⁻⁴.
// Each center rides its own (cos, sin) at its own whole multiple of the phase
// and its own offset, so the four do not move as one. Whole multiples keep
// every orbit closed over one 360° turn of the phase. The angle is taken
// unrounded; whole-degree steps times the rate would jump several pixels.
//
// The palette is not a Phong of ∇F. Density is treated as a Lambert
// term I = cos(π (255 − i) / 512), then
//
//   C = C₀ I + 350 I^130 ,   C₀ = (63, 72, 128)
//
// a blue plastic ramp: diffuse from the density, a sharp white core.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrovector.h"

constexpr int NUM_BALLS = 4;
constexpr int ORBIT_SPEED = 40; // degrees of the base orbit per second; the rates below multiply it

struct MetaBall {
	vec2 pos;
	float r;
};

void DEMO_Render(RETRO_Time time)
{
	// Calculate phase
	double phase = fmod(time.total * ORBIT_SPEED, 360);

	// Move balls
	static const int rate[NUM_BALLS] = { 4, 3, 1, 2 };
	static const int offset[NUM_BALLS] = { 0, 90, 210, 300 };
	static const float charge[NUM_BALLS] = { 1000, 4000, 7000, 10000 };
	static const float amplitudex[NUM_BALLS] = { -100, 10, -130, -80 };
	static const float amplitudey[NUM_BALLS] = { -10, 60, -80, 70 };
	MetaBall balls[NUM_BALLS];

	for (int i = 0; i < NUM_BALLS; i++) {
		double angle = radians(phase * rate[i] + offset[i]);
		balls[i].r = charge[i];
		balls[i].pos = { (float)(cos(angle) * amplitudex[i] + (RETRO_WIDTH / 2.0)), (float)(sin(angle) * amplitudey[i] + (RETRO_HEIGHT / 2.0)) };
	}

	// Draw balls
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			float sum = 0;
			// Sum field
			for (int i = 0; i < NUM_BALLS; i++) {
				vec2 delta = vec2{ (float)x, (float)y } - balls[i].pos;
				float d = MAX(dot(delta, delta), 0.0001f); // squared pixel distance from metaball position
				sum += balls[i].r / d;
			}
			RETRO_PutPixel(x, y, CLAMP256(20 * sum));
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. Density index → Lambert-like I, then plastic Phong
	for (int i = 0; i < RETRO_COLORS; i++) {
		double intensity = cos((255 - i) / 512.0 * M_PI);
		double highlight = 350 * pow(intensity, 130);
		int r = CLAMP256(63 * intensity + highlight);
		int g = CLAMP256(72 * intensity + highlight);
		int b = CLAMP256(128 * intensity + highlight);
		RETRO_SetColor(i, r, g, b);
	}
}
