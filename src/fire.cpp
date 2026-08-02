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

void DEMO_Render(double deltatime)
{
	unsigned char *buffer = RETRO_FrameBuffer();

	// Advance the flame in fixed steps. It rises one blur pass at a time through a buffer
	// that is never cleared, and sparks are seeded once per step, so the step rate sets
	// both speeds.
	while (RETRO_PerformSimulation()) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			if (RANDOM(FIRE_CHAOS) == 0) {
				for (int y = RETRO_HEIGHT - FIRE_HEIGHT; y < RETRO_HEIGHT; y++) {
					FireBuffer[y * RETRO_WIDTH + x] = 255;
				}
			}
		}
		RETRO_Blur(RETRO_BLUR_FIRE, 3, RETRO_BLUR_WRAP, FireBuffer);
	}

	// Only render the top part of the flame
	RETRO_Blit(FireBuffer, (RETRO_HEIGHT - FIRE_HEIGHT) * RETRO_WIDTH, buffer + (FIRE_HEIGHT * RETRO_WIDTH));
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, 16, RETRO_BLACK, {20, 0, 20});
	RETRO_CreateGradientPalette(16, 32, {20, 0, 20}, {0, 0, 48});
	RETRO_CreateGradientPalette(32, 64, {0, 0, 48}, RETRO_RED);
	RETRO_CreateGradientPalette(64, 128, RETRO_RED, RETRO_YELLOW);
	RETRO_CreateGradientPalette(128, RETRO_COLORS, RETRO_WHITE, RETRO_WHITE);
}
