//
// Fire logo
//
// Same 8-tap rising heat field as fire.cpp:
//
//   T' = max(0, mean(eight taps) − FIRE_DECAY)
//
// No self term; six taps sit below, so heat rises. The logo pixels
// reseed their own heat (a random value in [0, texel)) each step, and
// the bottom rows still spark at 255. The blit drops the fuel bed so
// only the risen flame and the logo-shaped heat are visible.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retrocolor.h"

#define FIRE_HEIGHT 6 // rows of fuel along the bottom, cropped off the blit
#define FIRE_CHAOS 6 // a column is sparked with probability 1 / FIRE_CHAOS
#define FIRE_DECAY 3 // subtracted after the 8-tap average

unsigned char FireBuffer[RETRO_HEIGHT * RETRO_WIDTH];

//
// Advance the flame in fixed steps. It rises one blur pass at a time through a buffer
// that is never cleared, and the logo is reseeded once per step, so the step rate sets
// both speeds.
//
void DEMO_Update(double deltatime)
{
	unsigned char *image = RETRO_ImageData();

	// Seed logo
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int offset = y * RETRO_WIDTH + x;
			if (image[offset] > 0) {
				FireBuffer[offset] = RANDOM(image[offset]);
			}
		}
	}

	// Seed bed
	for (int x = 0; x < RETRO_WIDTH; x++) {
		if (RANDOM(FIRE_CHAOS) == 0) {
			for (int y = RETRO_HEIGHT - FIRE_HEIGHT; y < RETRO_HEIGHT; y++) {
				FireBuffer[y * RETRO_WIDTH + x] = 255;
			}
		}
	}

	// Rise
	RETRO_Blur(RETRO_BLUR_FIRE, FIRE_DECAY, RETRO_BLUR_WRAP, FireBuffer);

	// Drop the fuel bed
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
