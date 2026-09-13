//
// Water, with a lit surface
//
// The same leapfrog height field as water.cpp, then two things that file
// does not do: a disk droplet instead of a point, and a Lambert term from
// the slope, looked up in a shade table of the still picture.
//
//   ∂²h/∂t² = c² ∇²h
//
// Leapfrog with the five-point Laplacian and λ = 1/2, then WATER_DAMP,
// then the (4, 1, 1, 1, 1) / 8 smoothing pass. The one-pixel frame stays
// Dirichlet 0. A droplet is a cosine disk in h, radius WATER_DROP_RADIUS,
// so the first ring is already a circle instead of a pixel that has to
// spread. The picture is sampled at the same first-order slope offset as
// water.cpp. The surface is (x, y, h) with y down and z toward the viewer,
// so
//
//   N = (−hx, −hy, 1) / |…|
//
// hx, hy are the central differences scaled by WATER_BUMP. L is a fixed
// unit direction from above-left. The shade is N·L over WATER_SHADES, and
// the texel is the shade table's match of that color darkened toward black.
// Ambient WATER_AMBIENT keeps a trough from collapsing onto palette black.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retroshadetable.h"
#include "lib/retrovector.h"

#define WATER_DAMP 0.985 // kept per step, so how slowly a ripple dies
#define WATER_REFRACT 18.0 // texels a unit slope slides the sample
#define WATER_DEPTH 40.0 // height subtracted at the centre of one droplet
#define WATER_DROP_STEPS 32 // steps between droplets
#define WATER_DROP_RADIUS 3 // pixels of the cosine disk
#define WATER_BUMP (1.0f / 16.0f) // scales the slope into N, so how steep the relief reads
#define WATER_SHADES 32 // entries in each shade-table ramp
#define WATER_AMBIENT 0.62f // darkest a texel may be, so troughs do not hole

static const vec3 LightDir = { 0.28f, -0.18f, 0.94f }; // from above-left, toward the viewer

float WaterA[RETRO_WIDTH * RETRO_HEIGHT];
float WaterB[RETRO_WIDTH * RETRO_HEIGHT];
float WaterC[RETRO_WIDTH * RETRO_HEIGHT];

float *Water = WaterA;
float *WaterPrevious = WaterB;
float *WaterScratch = WaterC;

unsigned char ShadeTable[RETRO_COLORS * WATER_SHADES];

static void Drop(int cx, int cy, float depth, int radius)
{
	int r2 = radius * radius;
	for (int dy = -radius; dy <= radius; dy++) {
		int y = cy + dy;
		if (y <= 0 || y >= RETRO_HEIGHT - 1) {
			continue;
		}
		for (int dx = -radius; dx <= radius; dx++) {
			int d2 = dx * dx + dy * dy;
			if (d2 > r2) {
				continue;
			}
			int x = cx + dx;
			if (x <= 0 || x >= RETRO_WIDTH - 1) {
				continue;
			}
			float t = sqrtf((float)d2) / (float)radius;
			Water[y * RETRO_WIDTH + x] -= depth * 0.5f * (1.0f + cosf(t * (float)M_PI));
		}
	}
}

void DEMO_FixedUpdate(double timestep)
{
	static int tick = 0;
	if (tick % WATER_DROP_STEPS == 0) {
		Drop(RANDOM(RETRO_WIDTH), RANDOM(RETRO_HEIGHT), WATER_DEPTH, WATER_DROP_RADIUS);
	}
	tick = (tick + 1) % WATER_DROP_STEPS;

	for (int y = 1; y < RETRO_HEIGHT - 1; y++) {
		for (int x = 1; x < RETRO_WIDTH - 1; x++) {
			int i = y * RETRO_WIDTH + x;
			WaterPrevious[i] = ((Water[i - 1] + Water[i + 1] + Water[i - RETRO_WIDTH] + Water[i + RETRO_WIDTH]) * 0.5f - WaterPrevious[i]) * WATER_DAMP;
		}
	}

	for (int y = 1; y < RETRO_HEIGHT - 1; y++) {
		for (int x = 1; x < RETRO_WIDTH - 1; x++) {
			int i = y * RETRO_WIDTH + x;
			WaterScratch[i] = (4 * WaterPrevious[i] + WaterPrevious[i - 1] + WaterPrevious[i + 1] + WaterPrevious[i - RETRO_WIDTH] + WaterPrevious[i + RETRO_WIDTH]) * 0.125f;
		}
	}

	float *unsmoothed = WaterPrevious;
	WaterPrevious = Water;
	Water = WaterScratch;
	WaterScratch = unsmoothed;
}

void DEMO_Render(double time, double deltatime)
{
	unsigned char *image = RETRO_ImageData();
	unsigned char *buffer = RETRO_FrameBuffer();

	for (int y = 1; y < RETRO_HEIGHT - 1; y++) {
		for (int x = 1; x < RETRO_WIDTH - 1; x++) {
			int i = y * RETRO_WIDTH + x;
			float nx = Water[i + 1] - Water[i - 1];
			float ny = Water[i + RETRO_WIDTH] - Water[i - RETRO_WIDTH];
			int rx = CLAMP(x - (int)lroundf(nx * WATER_REFRACT), 1, RETRO_WIDTH - 1);
			int ry = CLAMP(y - (int)lroundf(ny * WATER_REFRACT), 1, RETRO_HEIGHT - 1);
			unsigned char texel = image[ry * RETRO_WIDTH + rx];

			vec3 n = normalize(vec3{ -nx * WATER_BUMP, -ny * WATER_BUMP, 1.0f });
			float lambert = MAX(dot(n, LightDir), 0.0f);
			int shade = (int)(lambert * (WATER_SHADES - 1));
			buffer[i] = ShadeTable[texel * WATER_SHADES + shade];
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/monkey_320x240.pcx", true);
	RETRO_CreatePaletteShadeTable(RETRO_ImagePalette(), RETRO_COLORS, WATER_SHADES, ShadeTable, WATER_AMBIENT);
}
