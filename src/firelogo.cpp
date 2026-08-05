//
// firelogo.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retrocolor.h"

#define FIRE_HEIGHT 6
#define FIRE_CHAOS 6

unsigned char FireBuffer[RETRO_HEIGHT*RETRO_WIDTH];

//
// Advance the flame in fixed steps. It rises one blur pass at a time through a buffer
// that is never cleared, and the logo is reseeded once per step, so the step rate sets
// both speeds.
//
void DEMO_Update(double deltatime)
{
	unsigned char *image = RETRO_ImageData();

	for (int y = 100; y < 130; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int offset = y * RETRO_WIDTH + x;
			if (image[offset] > 0) FireBuffer[offset] = RANDOM(image[offset]);
		}
	}
	for (int x = 0; x < RETRO_WIDTH; x++) {
		if (RANDOM(FIRE_CHAOS) == 0) {
			for (int y = RETRO_HEIGHT - FIRE_HEIGHT; y < RETRO_HEIGHT; y++) {
				FireBuffer[y * RETRO_WIDTH + x] = 255;
			}
		}
	}
	RETRO_Blur(RETRO_BLUR_FIRE, 3, RETRO_BLUR_WRAP, FireBuffer);

	// Only show the top part of the flame
	RETRO_Blit(FireBuffer, (RETRO_HEIGHT - FIRE_HEIGHT) * RETRO_WIDTH, RETRO_FrameBuffer() + (FIRE_HEIGHT * RETRO_WIDTH));
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/logo_320x240.pcx");

	// Init palette
	RETRO_CreateGradientPalette(0, 24, RETRO_BLACK, RETRO_DARKBLUE);
	RETRO_CreateGradientPalette(24, 48, RETRO_DARKBLUE, RETRO_RED);
	RETRO_CreateGradientPalette(48, 64, RETRO_RED, RETRO_YELLOW);
	RETRO_CreateGradientPalette(64, 128, RETRO_YELLOW, RETRO_WHITE);
	RETRO_CreateGradientPalette(128, RETRO_COLORS, RETRO_WHITE, RETRO_WHITE);
}
