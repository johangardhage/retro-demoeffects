//
// texturetunnel.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define TEXTURE_WIDTH 256
#define TEXTURE_HEIGHT 256
#define RADIUS 32
#define DIST 256

unsigned int AngleTable[RETRO_WIDTH * RETRO_HEIGHT];
unsigned int DistanceTable[RETRO_WIDTH * RETRO_HEIGHT];

void DEMO_Render(double deltatime)
{
	// Calculate frame
	static double frame = 0;
	frame += M_PI * deltatime * 0.2;

	unsigned char *image = RETRO_GetTextureImage();

	// Calculate movement
	int dx = RETRO_WIDTH * sin(frame);
	int dy = RETRO_HEIGHT * cos(frame);

	// Draw tunnel
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int tx = ((AngleTable[y * RETRO_WIDTH + x] + dx) % TEXTURE_WIDTH) / 2;
			int ty = ((DistanceTable[y * RETRO_WIDTH + x] + dy) % TEXTURE_HEIGHT) / 2;
			unsigned char color = image[ty * TEXTURE_WIDTH / 2 + tx];

			RETRO_PutPixel(x, y, color);
		}
	}

	// Draw filled circle
	int radius = 15;
	for (int y = -radius; y <= radius; y++)
		for (int x = -radius; x <= radius; x++)
			if (x * x + y * y <= radius * radius)
				RETRO_PutPixel(RETRO_WIDTH / 2 + x, RETRO_HEIGHT / 2 + y, 0);
}

void DEMO_Initialize(void)
{
	RETRO_LoadTexture("assets/texturetunnel_128x128.pcx");
	RETRO_SetPaletteFromTexture();

	// Init tables
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			AngleTable[y * RETRO_WIDTH + x] = atan2(x - RETRO_WIDTH / 2, y - RETRO_HEIGHT / 2) * TEXTURE_WIDTH / M_PI;
			DistanceTable[y * RETRO_WIDTH + x] = (RADIUS * DIST) / sqrt((x - RETRO_WIDTH / 2) * (x - RETRO_WIDTH / 2) + (y - RETRO_HEIGHT / 2) * (y - RETRO_HEIGHT / 2));
		}
	}
}
