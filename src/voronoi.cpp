//
// Voronoi
//
// A pane of stained glass whose pieces keep changing shape: a Voronoi diagram
// of SEEDS moving points, each cell a beveled piece of colored glass set in
// black lead.
//
// Every pixel belongs to the cell of its nearest seed. The seeds start on a
// jittered grid, so the cells come out about the same size, and each circles
// its own grid point on a Lissajous path, so the cells grow, shrink and trade
// neighbors.
//
// The border between the cells of seeds a and b is the line halfway between
// them, and a pixel p in a's cell lies at a distance
//
//   e = (|p − b|² − |p − a|²) / (2 |b − a|)
//
// from it. The nearest border is the smallest e over every other seed b, so
// the lead is drawn exactly VORONOI_LEAD pixels wide however the cells are
// shaped. Within VORONOI_BEVEL pixels of the lead the glass slopes down to
// it, a face tilted toward that neighbor, and it is lit by the light's dot
// product with the face's normal, so the bevels facing the light shine and
// the ones facing away are in shade.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define SEEDS_X 6 // grid the seeds start on
#define SEEDS_Y 4
#define SEEDS (SEEDS_X * SEEDS_Y)
#define SEED_JITTER 0.35 // of a grid cell, how far a seed's center strays from its grid point
#define SEED_ORBIT 28.0 // pixels, the largest radius of a seed's path
#define SEED_MINSPEED 0.3 // radians a second
#define SEED_MAXSPEED 0.9
#define VORONOI_LEAD 1.5 // pixels of lead between the cells
#define VORONOI_BEVEL 6.0 // pixels of sloping glass inside the lead
#define VORONOI_SLOPE 1.2 // rise of the bevel per pixel inward
#define GLASS_HUES 8
#define GLASS_SHADES 31 // palette entries per hue
#define GLASS 1 // first entry of the hues; entry 0 is the lead

static const RETRO_Palette GlassHues[GLASS_HUES] = { RETRO_SCARLET, RETRO_ORANGE, RETRO_GOLD, RETRO_SPRINGGREEN, RETRO_DARKTURQUOISE, RETRO_AZURE, RETRO_PURPLE, RETRO_HOTPINK };
static const vec3 Light = normalize(vec3{ -0.45f, -0.55f, 0.70f }); // toward the light

struct Seed {
	float x, y; // center of its path
	float ampx, ampy;
	float speedx, speedy;
	float phasex, phasey;
	int hue;
};

static Seed Seeds[SEEDS];

void DEMO_Render(RETRO_Time time)
{
	unsigned char *buffer = RETRO_FrameBuffer();

	// Move the seeds, and find the reciprocal of twice each pair's distance
	float seedx[SEEDS], seedy[SEEDS];
	float invgap[SEEDS][SEEDS];
	for (int i = 0; i < SEEDS; i++) {
		Seed &seed = Seeds[i];
		seedx[i] = seed.x + seed.ampx * sinf((float)fmod(time.total * seed.speedx, 2 * M_PI) + seed.phasex);
		seedy[i] = seed.y + seed.ampy * sinf((float)fmod(time.total * seed.speedy, 2 * M_PI) + seed.phasey);
	}
	for (int i = 0; i < SEEDS; i++) {
		for (int j = 0; j < SEEDS; j++) {
			float dx = seedx[j] - seedx[i], dy = seedy[j] - seedy[i];
			invgap[i][j] = i == j ? 0 : 0.5f / MAX(hypotf(dx, dy), 0.001f);
		}
	}

	// Draw the cells
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			// Nearest seed
			float distance[SEEDS];
			int nearest = 0;
			for (int i = 0; i < SEEDS; i++) {
				float dx = x - seedx[i], dy = y - seedy[i];
				distance[i] = dx * dx + dy * dy;
				if (distance[i] < distance[nearest]) {
					nearest = i;
				}
			}

			// Nearest border
			float edge = 1e9f;
			int neighbor = nearest;
			for (int i = 0; i < SEEDS; i++) {
				float e = (distance[i] - distance[nearest]) * invgap[nearest][i];
				if (i != nearest && e < edge) {
					edge = e;
					neighbor = i;
				}
			}

			unsigned char *pixel = buffer + y * RETRO_WIDTH + x;
			if (edge < VORONOI_LEAD / 2) {
				*pixel = 0;
				continue;
			}

			// Light the glass, flat in the middle and tilted toward the neighbor on the bevel
			float brightness = Light.z;
			if (edge < VORONOI_LEAD / 2 + VORONOI_BEVEL) {
				float nx = (seedx[neighbor] - seedx[nearest]) * 2 * invgap[nearest][neighbor];
				float ny = (seedy[neighbor] - seedy[nearest]) * 2 * invgap[nearest][neighbor];
				brightness = (VORONOI_SLOPE * (nx * Light.x + ny * Light.y) + Light.z) / sqrtf(VORONOI_SLOPE * VORONOI_SLOPE + 1);
			}
			*pixel = GLASS + Seeds[nearest].hue * GLASS_SHADES + CLAMP(brightness * GLASS_SHADES, 0, GLASS_SHADES);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. The lead black, then a ramp per hue from dark glass
	// through the hue to a highlight
	RETRO_SetColor(0, RETRO_BLACK);
	for (int k = 0; k < GLASS_HUES; k++) {
		int ramp = GLASS + k * GLASS_SHADES;
		int middle = ramp + GLASS_SHADES * 3 / 4;
		RETRO_Palette dark = GlassHues[k] * 0.125f;
		RETRO_CreateGradientPalette(ramp, middle, dark, GlassHues[k]);
		RETRO_CreateGradientPalette(middle, ramp + GLASS_SHADES, GlassHues[k], RETRO_WHITE);
	}

	// Init seeds, one per grid cell. A hue repeats no nearer than two cells
	// diagonally, or three columns and a row, so neighbors hardly ever match
	float cellwidth = (float)RETRO_WIDTH / SEEDS_X;
	float cellheight = (float)RETRO_HEIGHT / SEEDS_Y;
	for (int i = 0; i < SEEDS; i++) {
		Seed &seed = Seeds[i];
		int column = i % SEEDS_X;
		int row = i / SEEDS_X;
		seed.x = (column + 0.5f + mix(-SEED_JITTER, SEED_JITTER, RAND())) * cellwidth;
		seed.y = (row + 0.5f + mix(-SEED_JITTER, SEED_JITTER, RAND())) * cellheight;
		seed.ampx = SEED_ORBIT * mix(0.4, 1.0, RAND());
		seed.ampy = SEED_ORBIT * mix(0.4, 1.0, RAND());
		seed.speedx = mix(SEED_MINSPEED, SEED_MAXSPEED, RAND());
		seed.speedy = mix(SEED_MINSPEED, SEED_MAXSPEED, RAND());
		seed.phasex = RANDOMF(2 * M_PI);
		seed.phasey = RANDOMF(2 * M_PI);
		seed.hue = (column + row * 3) % GLASS_HUES;
	}
}
