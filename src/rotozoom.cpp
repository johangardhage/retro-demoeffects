//
// rotozoom.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define TEXTURE_WIDTH 256
#define TEXTURE_HEIGHT 256
#define SINE_VALUES 1024

float SinTable[SINE_VALUES];

void DEMO_Render(double deltatime)
{
	// Calculate frame
	static double frame_counter = 0;
	frame_counter += deltatime * 200;
	int frame = frame_counter;

	unsigned char *image = RETRO_GetTextureImage();

	// Calculate movement
	float xstep = SinTable[frame % SINE_VALUES];
	float ystep = SinTable[(frame + 256) % SINE_VALUES];
	float zstep = SinTable[frame % SINE_VALUES];
	float xd = xstep * zstep;
	float yd = ystep * zstep;

	// Draw texture
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int tx = (unsigned int)(yd * y - xd * x) % TEXTURE_WIDTH;
			int ty = (unsigned int)(xd * y + yd * x) % TEXTURE_HEIGHT;
			int color = image[ty * TEXTURE_WIDTH + tx];

			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize()
{
	RETRO_LoadTexture("assets/rotozoom_256x256.pcx");
	RETRO_SetPaletteFromTexture();

	// Init sine table
	for (int i = 0; i < SINE_VALUES; i++) {
		SinTable[i] = sin(i * M_PI / 512);
	}
}
