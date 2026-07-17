//
// fire.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

#define FIRE_HEIGHT 10
#define FIRE_CHAOS 6
#define SIMULATION_STEP (1.0 / 60.0)

unsigned char FireBuffer[RETRO_HEIGHT*RETRO_WIDTH];

void UpdateFire(void)
{
	for (int x = 0; x < RETRO_WIDTH; x++) {
		if (RANDOM(FIRE_CHAOS) == 0) {
			for (int y = RETRO_HEIGHT - FIRE_HEIGHT; y < RETRO_HEIGHT; y++) {
				FireBuffer[y * RETRO_WIDTH + x] = 255;
			}
		}
	}
	RETRO_Blur(RETRO_BLUR_FIRE, 3, RETRO_BLUR_WRAP, FireBuffer);
}

void DEMO_Render(double deltatime)
{
	static double accumulator = 0;
	unsigned char *buffer = RETRO_FrameBuffer();

	accumulator = MIN(accumulator + deltatime, SIMULATION_STEP * 15);
	while (accumulator >= SIMULATION_STEP) {
		UpdateFire();
		accumulator -= SIMULATION_STEP;
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
	Gradient(0, 16, 0, 0, 0, 5, 0, 5);
	Gradient(16, 32, 5, 0, 5, 0, 0, 12);
	Gradient(32, 64, 0, 0, 12, 63, 0, 0);
	Gradient(64, 128, 63, 0, 0, 63, 63, 0);
	Gradient(128, 256, 63, 63, 63, 63, 63, 63);
}
