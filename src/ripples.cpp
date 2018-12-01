//
// ripples.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define WATER_YPOS 185
#define WATER_WAVELENGTH 1.5
#define WATER_AMPLITUDE 3
#define SINE_VALUES 100

int SinTable[SINE_VALUES];

void DEMO_Render(double deltatime)
{
	// Calculate frame
	static double frame_counter = 0;
	frame_counter += deltatime * 30;
	int frame = frame_counter;

	unsigned char *buffer = RETRO_GetSurfaceBuffer();

	// Draw background
	RETRO_Blit(RETRO_GetTextureImage(), RETRO_WIDTH * RETRO_HEIGHT);

	// Draw ripples
	for (int y = WATER_YPOS; y < RETRO_HEIGHT; y++) {
		int ysrc = WATER_YPOS + (WATER_YPOS - y) + SinTable[(y + frame) % SINE_VALUES];
		int ydst = y;

		RETRO_Copy(buffer + ysrc * RETRO_WIDTH, buffer + ydst * RETRO_WIDTH, RETRO_WIDTH);
	}
}

void DEMO_Initialize()
{
	RETRO_LoadTexture("assets/ripples_320x240.pcx");
	RETRO_SetPaletteFromTexture();

	// Init sine table
	for (int i = 0; i < SINE_VALUES; i++) {
		SinTable[i] = sin(i / WATER_WAVELENGTH) * WATER_AMPLITUDE;
	}
}
