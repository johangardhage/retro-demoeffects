//
// firelogo.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

#define FIRE_HEIGHT 6
#define FIRE_CHAOS 6

unsigned char FireBuffer[RETRO_HEIGHT*RETRO_WIDTH];

void DEMO_Render(double deltatime)
{
	unsigned char *buffer = RETRO_FrameBuffer();
	unsigned char *image = RETRO_ImageData();

	// Advance the flame in fixed steps. It rises one blur pass at a time through a buffer
	// that is never cleared, and the logo is reseeded once per step, so the step rate sets
	// both speeds.
	while (RETRO_PerformSimulation()) {
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
	}

	// Only render the top part of the flame
	RETRO_Blit(FireBuffer, (RETRO_HEIGHT - FIRE_HEIGHT) * RETRO_WIDTH, buffer + (FIRE_HEIGHT * RETRO_WIDTH));
}

void Gradient(int s, int e, int r1, int g1, int b1, int r2, int g2, int b2)
{
	for (int i = 0; i < e - s; i++) {
		float k = (float) i / (e - s);

		unsigned char r = (r1 + (r2 - r1) * k) * 4;
		unsigned char g = (g1 + (g2 - g1) * k) * 4;
		unsigned char b = (b1 + (b2 - b1) * k) * 4;
		RETRO_SetColor(s + i, r, g, b);
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_LoadImage("assets/logo_320x240.pcx");
	Gradient(0, 24, 0, 0, 0, 0, 0, 31);
	Gradient(24, 48, 0, 0, 31, 63, 0, 0);
	Gradient(48, 64, 63, 0, 0, 63, 63, 0);
	Gradient(64, 128, 63, 63, 0, 63, 63, 63);
	Gradient(128, 256, 63, 63, 63, 63, 63, 63);
}
