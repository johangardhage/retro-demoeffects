//
// fire.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retrocolor.h"

#define FIRE_HEIGHT 10
#define FIRE_CHAOS 6

unsigned char FireBuffer[RETRO_HEIGHT*RETRO_WIDTH];

//
// Advance the flame in fixed steps. It rises one blur pass at a time through a buffer
// that is never cleared, and sparks are seeded once per step, so the step rate sets
// both speeds.
//
void DEMO_Update(double deltatime)
{
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
	// Init palette. The flame heats up from a cool blue smoke through red and
	// yellow into white as the blur carries the sparks upwards
	RETRO_CreateGradientPalette(0, 32, RETRO_BLACK, RETRO_BLUEBLACK);
	RETRO_CreateGradientPalette(32, 64, RETRO_BLUEBLACK, RETRO_RED);
	RETRO_CreateGradientPalette(64, 128, RETRO_RED, RETRO_YELLOW);
	RETRO_CreateGradientPalette(128, RETRO_COLORS, RETRO_YELLOW, RETRO_WHITE);
}
