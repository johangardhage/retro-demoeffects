//
// plasma.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define PLASMA_FRAMES 720
#define SINE_VALUES (RETRO_WIDTH+PLASMA_FRAMES*2)

float SinTable[SINE_VALUES];

void DEMO_Render(double deltatime)
{
	// Calculate frame
	static double frame_counter = 0;
	frame_counter += deltatime * 100;
	int frame = (int)frame_counter % PLASMA_FRAMES;

	// Draw plasma
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		float yc = 75 + SinTable[y + frame * 2] * 2 + SinTable[y * 2 + frame / 2] + SinTable[y + frame] * 2;

		for (int x = 0; x < RETRO_WIDTH; x++) {
			float xc = 75 + SinTable[x * 2 + frame / 2] + SinTable[x + frame * 2] + SinTable[x / 2 + frame] * 2;

			unsigned char color = xc * yc;
			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init sine table
	for (int i = 0; i < SINE_VALUES; i++) {
		SinTable[i] = cos(i * M_PI / 180);
	}

	// Init palette
	unsigned char r = 0, g = 0, b = 0;
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, 0, 0, 0);
	}
	for (int i = 0; i < 42; i++, r++) {
		RETRO_SetColor(i, r * 4, g * 4, b * 4);
	}
	for (int i = 42; i < 84; i++, g++) {
		RETRO_SetColor(i, r * 4, g * 4, b * 4);
	}
	for (int i = 84; i < 126; i++, b++) {
		RETRO_SetColor(i, r * 4, g * 4, b * 4);
	}
	for (int i = 126; i < 168; i++, r--) {
		RETRO_SetColor(i, r * 4, g * 4, b * 4);
	}
	for (int i = 168; i < 210; i++, g--) {
		RETRO_SetColor(i, r * 4, g * 4, b * 4);
	}
	for (int i = 210; i < 252; i++, b--) {
		RETRO_SetColor(i, r * 4, g * 4, b * 4);
	}
}
