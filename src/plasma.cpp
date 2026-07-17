//
// plasma.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define PLASMA_FRAMES 720
#define SINE_VALUES (RETRO_WIDTH + PLASMA_FRAMES * 2)

float CosTable[SINE_VALUES];

void DEMO_Render(double deltatime)
{
	// Calculate frame
	static double framecounter = 0;
	framecounter += deltatime * 100;
	int frame = WRAP(framecounter, PLASMA_FRAMES);

	// Generate plasma
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		float yc = 75 + CosTable[y + frame * 2] * 2 + CosTable[y * 2 + frame / 2] + CosTable[y + frame] * 2;

		for (int x = 0; x < RETRO_WIDTH; x++) {
			float xc = 75 + CosTable[x * 2 + frame / 2] + CosTable[x + frame * 2] + CosTable[x / 2 + frame] * 2;

			// Wrap into the 252-entry palette cycle
			unsigned char color = (int)(xc * yc) % 252;
			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	unsigned char r = 0, g = 0, b = 0;
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, 0, 0, 0);
	}
	for (int i = 0; i < 42; i++) {
		RETRO_SetColor(i, r * 4, g * 4, b * 4);
		r++;
	}
	for (int i = 42; i < 84; i++) {
		RETRO_SetColor(i, r * 4, g * 4, b * 4);
		g++;
	}
	for (int i = 84; i < 126; i++) {
		RETRO_SetColor(i, r * 4, g * 4, b * 4);
		b++;
	}
	for (int i = 126; i < 168; i++) {
		RETRO_SetColor(i, r * 4, g * 4, b * 4);
		r--;
	}
	for (int i = 168; i < 210; i++) {
		RETRO_SetColor(i, r * 4, g * 4, b * 4);
		g--;
	}
	for (int i = 210; i < 252; i++) {
		RETRO_SetColor(i, r * 4, g * 4, b * 4);
		b--;
	}

	// Init sine table
	for (int i = 0; i < SINE_VALUES; i++) {
		CosTable[i] = cos(i * M_PI / 180);
	}
}
