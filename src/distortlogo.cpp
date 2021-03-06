//
// distortlogo.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define SINE_VALUES 64

int SinTable[SINE_VALUES];

void DEMO_Render(double deltatime)
{
	// Calculate frame
	static double frame_counter = 0;
	frame_counter += deltatime * 100;
	int frame = frame_counter;

	Texture *texture = RETRO_GetTexture();

	// Draw distortion
	for (int y = 0; y < texture->height; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int tx = x + SinTable[(y + frame) % SINE_VALUES];
			int ty = y + SinTable[(x + frame) % SINE_VALUES] / 2;

			if (tx >= 0 && tx < texture->height && ty >= 0 && ty < texture->height) {
				unsigned char col = texture->image[ty * texture->width + tx];
				RETRO_PutPixel(x, y, col);
			}
		}
	}
}

void DEMO_Initialize()
{
	RETRO_LoadTexture("assets/distortlogo_320x240.pcx");
	RETRO_SetPaletteFromTexture();

	// Init sin table
	for (int i = 0; i < SINE_VALUES; i++) {
		SinTable[i] = sin(i * M_PI / 32) * 5;
	}
}
