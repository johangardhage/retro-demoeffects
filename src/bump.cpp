//
// bump.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocolor.h"

#define LIGHTMAP_WIDTH 256
#define LIGHTMAP_HEIGHT 256
#define LIGHT_SIZE 128
#define LIGHT_DEPTH 1
#define LIGHT_COLORS 128
#define SINE_VALUES 360

unsigned char LightMap[LIGHTMAP_HEIGHT*LIGHTMAP_WIDTH];

int CosTable[SINE_VALUES];
int SinTable[SINE_VALUES];

void DEMO_Render(double deltatime)
{
	// Calculate frame
	static double frame_counter = 0;
	frame_counter += deltatime * 100;
	int frame = frame_counter;

	unsigned char *buffer = RETRO_FrameBuffer();
	unsigned char *image = RETRO_ImageData();

	// Light position
	int lx = RETRO_WIDTH / 2 + LIGHT_SIZE + CosTable[frame % SINE_VALUES];
	int ly = RETRO_HEIGHT / 2 + LIGHT_SIZE + SinTable[(2*frame) % SINE_VALUES];

	// Draw bump
	for (int y = 1; y < RETRO_HEIGHT-1; y++) {
		for (int x = 1; x < RETRO_WIDTH-1; x++) {
			int offset = y * RETRO_WIDTH + x;

			int bx = (image[offset + 1] - image[offset - 1]) * LIGHT_DEPTH - (x - lx);
			int by = (image[offset + RETRO_WIDTH] - image[offset - RETRO_WIDTH]) * LIGHT_DEPTH - (y - ly);

			bx = bx < 0 ? 0 : (bx > LIGHTMAP_WIDTH-1 ? LIGHTMAP_WIDTH-1 : bx);
			by = by < 0 ? 0 : (by > LIGHTMAP_HEIGHT-1 ? LIGHTMAP_HEIGHT-1 : by);

			buffer[offset] = LightMap[by * LIGHTMAP_WIDTH + bx];
		}
	}
}

void DEMO_Initialize(void)
{
	// The image is a height map, only its gradients are read
	RETRO_LoadImage("assets/bump_320x240.pcx");

	// Init sin table
	for (int i = 0; i < SINE_VALUES; i++) {
		CosTable[i] = cos(i * M_PI / 180) * LIGHTMAP_WIDTH / 2;
		SinTable[i] = sin(i * M_PI / 180) * LIGHTMAP_WIDTH / 2;
	}

	// Init palette. The light map only indexes the first LIGHT_COLORS entries,
	// where the light ramps from black through red into a white core
	RETRO_CreateGradientPalette(0, LIGHT_COLORS * 3 / 4, RETRO_BLACK, RETRO_RED);
	RETRO_CreateGradientPalette(LIGHT_COLORS * 3 / 4, LIGHT_COLORS, RETRO_RED, RETRO_WHITE);

	// Init light map
	for (int y = 0; y < LIGHTMAP_HEIGHT; y++) {
		for (int x = 0; x < LIGHTMAP_WIDTH; x++) {
			float nx = (x - LIGHT_SIZE) / (float)LIGHT_SIZE;
			float ny = (y - LIGHT_SIZE) / (float)LIGHT_SIZE;
			float nz = 1 - sqrt(nx * nx + ny * ny);
			nz = nz < 0 ? 0 : nz;
			LightMap[y * LIGHTMAP_WIDTH + x] = (LIGHT_COLORS-1) * nz;
		}
	}
}
