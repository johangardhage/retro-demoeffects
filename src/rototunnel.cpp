//
// rototunnel.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define TEXTURE_WIDTH 256
#define TEXTURE_HEIGHT 256
#define RATIO 32 * TEXTURE_HEIGHT

int DistanceTable[2 * RETRO_WIDTH][2 * RETRO_HEIGHT];
int AngleTable[2 * RETRO_WIDTH][2 * RETRO_HEIGHT];

void DEMO_Render(double deltatime)
{
	// Calculate frame
	static double frame = 0;
	frame += deltatime * 1;

	unsigned char *image = RETRO_ImageData();

	// Calculate tunnel movement
	int sx = TEXTURE_WIDTH * 1.0 * frame;
	int sy = TEXTURE_HEIGHT * 0.25 * frame;

	// Calculate camera movement
	int dx = RETRO_WIDTH / 2 + RETRO_WIDTH / 2 * cos(frame);
	int dy = RETRO_HEIGHT / 2 + RETRO_HEIGHT / 2 * sin(frame * 3.3);

	// Draw tunnel
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int tx = ((unsigned int)DistanceTable[x + dx][y + dy] + sx) % TEXTURE_WIDTH;
			int ty = ((unsigned int)AngleTable[x + dx][y + dy] + sy) % TEXTURE_HEIGHT;
			unsigned char color = image[ty * TEXTURE_WIDTH + tx];

			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/flowers_256x256.pcx");
	RETRO_SetPalette(RETRO_ImagePalette());

	// Init sine table
	for (int y = 0; y < RETRO_HEIGHT * 2; y++) {
		for (int x = 0; x < RETRO_WIDTH * 2; x++) {
			AngleTable[x][y] = atan2(x - RETRO_WIDTH, y - RETRO_HEIGHT) * TEXTURE_WIDTH / M_PI;
			DistanceTable[x][y] = RATIO / sqrt((x - RETRO_WIDTH) * (x - RETRO_WIDTH) + (y - RETRO_HEIGHT) * (y - RETRO_HEIGHT));
		}
	}
}
