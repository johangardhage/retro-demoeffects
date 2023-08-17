//
// distortlogo.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define IMAGE_WIDTH 320
#define IMAGE_HEIGHT 240
#define SINE_VALUES 64

int SinTable[SINE_VALUES];

void DEMO_Render(double deltatime)
{
	// Calculate frame
	static double frame_counter = 0;
	frame_counter += deltatime * 100;
	int frame = frame_counter;

	unsigned char *image = RETRO_ImageData();

	// Draw distortion
	for (int y = 0; y < IMAGE_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int tx = x + SinTable[(y + frame) % SINE_VALUES];
			int ty = y + SinTable[(x + frame) % SINE_VALUES] / 2;

			if (tx >= 0 && tx < IMAGE_HEIGHT && ty >= 0 && ty < IMAGE_HEIGHT) {
				unsigned char col = image[ty * IMAGE_WIDTH + tx];
				RETRO_PutPixel(x, y, col);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/distortlogo_320x240.pcx");
	RETRO_SetPalette(RETRO_ImagePalette());

	// Init sin table
	for (int i = 0; i < SINE_VALUES; i++) {
		SinTable[i] = sin(i * M_PI / 32) * 5;
	}
}
