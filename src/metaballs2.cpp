//
// Metaballs 2
//
// The same 1/r² field as metaballs.cpp, drawn as density instead of the
// solid F ≥ T:
//
//   F(p) = sum_i  r_i / |p − c_i|²
//   color = clamp(20 F, 0, 255)
//
// r_i is the charge T R_i². One ball of strength r meets color 20 on
// the circle of radius √r, where 20 r / (√r)² = 20. |p−c|² is floored at 10⁻⁴
// as in metaballs.cpp. The four centres ride a shared (cos t, sin t)
// (the table is in degrees) at different amplitudes, so the orbits
// share a phase.
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

#define NUM_BALLS 4
#define SINE_VALUES 360
#define ORBIT_SPEED 100 // degrees of the shared orbit per second

struct MetaBall {
	float x, y, r;
} Balls[NUM_BALLS];

float SinTable[SINE_VALUES];
float CosTable[SINE_VALUES];

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * ORBIT_SPEED, SINE_VALUES);
	int iphase = WRAP(phase, SINE_VALUES);

	// Move balls
	Balls[0].r = 1000;
	Balls[0].x = CosTable[iphase] * -100 + (RETRO_WIDTH / 2);
	Balls[0].y = SinTable[iphase] * -10 + (RETRO_HEIGHT / 2);
	Balls[1].r = 4000;
	Balls[1].x = CosTable[iphase] * 10 + (RETRO_WIDTH / 2);
	Balls[1].y = SinTable[iphase] * 60 + (RETRO_HEIGHT / 2);
	Balls[2].r = 7000;
	Balls[2].x = CosTable[iphase] * -130 + (RETRO_WIDTH / 2);
	Balls[2].y = SinTable[iphase] * -80 + (RETRO_HEIGHT / 2);
	Balls[3].r = 10000;
	Balls[3].x = CosTable[iphase] * -80 + (RETRO_WIDTH / 2);
	Balls[3].y = SinTable[iphase] * 70 + (RETRO_HEIGHT / 2);

	// Draw balls
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			float sum = 0;
			// Sum field
			for (int i = 0; i < NUM_BALLS; i++) {
				float a = x - Balls[i].x;
				float b = y - Balls[i].y;
				float d = MAX(a * a + b * b, 0.0001f); // squared pixel distance from metaball position
				sum += Balls[i].r / d;
			}
			RETRO_PutPixel(x, y, CLAMP256(20 * sum));
		}
	}
}

void DEMO_Initialize(void)
{
	// Init tables
	for (int i = 0; i < SINE_VALUES; i++) {
		SinTable[i] = sin(i / 180.0 * M_PI);
		CosTable[i] = cos(i / 180.0 * M_PI);
	}

	// Init palette. Density index → Lambert-like I, then plastic Phong
	int light = 350;
	int reflect = 130;
	int ambient = 0;
	for (int i = 0; i < RETRO_COLORS; i++) {
		double intensity = cos((255 - i) / 512.0 * M_PI);
		int r = CLAMP256(63 * ambient / 255.0 + 63 * intensity + pow(intensity, reflect) * light);
		int g = CLAMP256(72 * ambient / 255.0 + 72 * intensity + pow(intensity, reflect) * light);
		int b = CLAMP256(128 * ambient / 255.0 + 128 * intensity + pow(intensity, reflect) * light);
		RETRO_SetColor(i, r, g, b);
	}
}
