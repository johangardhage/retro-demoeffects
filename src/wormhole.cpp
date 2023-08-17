//
// wormhole.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define WORM_XDIR -1
#define WORM_YDIR 1
#define WORM_SPOKES 2400
#define WORM_DIVS 2400
#define TEXTURE_WIDTH 15
#define TEXTURE_HEIGHT 15

unsigned char WormHole[RETRO_WIDTH * RETRO_HEIGHT];

void DEMO_Render(double deltatime)
{
	unsigned char *image = RETRO_ImageData();

	// Calculate frame
	static double frame_counter = 0;
	frame_counter += deltatime * 100;
	unsigned int xframe = frame_counter * WORM_XDIR;
	unsigned int yframe = frame_counter * WORM_YDIR;

	unsigned char newimage[TEXTURE_WIDTH * TEXTURE_HEIGHT];

	// Create new image
	for (int y = 0; y < TEXTURE_HEIGHT; y++) {
		for (int x = 0; x < TEXTURE_WIDTH; x++) {
			newimage[TEXTURE_WIDTH * y + x] = image[TEXTURE_WIDTH * ((y + yframe) % TEXTURE_HEIGHT) + ((x + xframe) % TEXTURE_WIDTH)];
		}
	}

	// Draw wormhole
	unsigned char *buffer = RETRO_FrameBuffer();
	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		buffer[i] = newimage[WormHole[i]];
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_LoadImage("assets/wormhole_15x15.pcx");
	RETRO_SetPalette(RETRO_ImagePalette());

	// Init sin table
	float CosTable[WORM_SPOKES];
	float SinTable[WORM_SPOKES];
	for (int i = 0; i < WORM_SPOKES; i++) {
		CosTable[i] = cos(2 * M_PI * i / WORM_SPOKES);
		SinTable[i] = sin(2 * M_PI * i / WORM_SPOKES);
	}

	// Init wormhole
	for (int d = 0; d < WORM_DIVS; d++) {
		float xd = RETRO_WIDTH * d / WORM_DIVS;
		float yd = RETRO_HEIGHT * d / WORM_DIVS;
		float zd = -10 + log(2.0 * d / WORM_DIVS) * 11;

		for (int s = 0; s < WORM_SPOKES; s++) {
			int x = xd * CosTable[s] + RETRO_WIDTH / 2;
			int y = yd * SinTable[s] + RETRO_HEIGHT / 2 - RETRO_HEIGHT / 4 - zd;

			if ((x >= 0) && (x < RETRO_WIDTH) && (y >= 0) && (y < RETRO_HEIGHT)) {
				unsigned char color = (s / 8) % TEXTURE_WIDTH + (TEXTURE_WIDTH * ((d / 7) % TEXTURE_WIDTH));
				WormHole[y * RETRO_WIDTH + x] = color;
			}
		}
	}
}
