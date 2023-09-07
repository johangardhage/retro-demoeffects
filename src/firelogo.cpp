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

void DEMO_Render(double deltatime)
{
	static unsigned char firebuffer[RETRO_HEIGHT*RETRO_WIDTH];
	unsigned char *buffer = RETRO_FrameBuffer();
	unsigned char *image = RETRO_ImageData();

	// Create flaming logo
	for (int y = 100; y < 130; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int offset = y * RETRO_WIDTH + x;
			if (image[offset] > buffer[offset]) {
				firebuffer[offset] = rand() & (image[offset]);
			}
		}
	}

	// Create flame at the bottom of the screen
	for (int x = 0; x < RETRO_WIDTH; x++) {
		if (rand() % FIRE_CHAOS == 0) {
			for (int y = RETRO_HEIGHT - FIRE_HEIGHT; y < RETRO_HEIGHT; y++) {
				firebuffer[y * RETRO_WIDTH + x] = 255;
			}
		}
	}

	RETRO_Blur(RETRO_BLUR_8, 3, RETRO_BLUR_WRAP, firebuffer);
	// Only render the top part of the flame
	RETRO_Blit(firebuffer, (RETRO_HEIGHT - FIRE_HEIGHT) * RETRO_WIDTH, buffer + (FIRE_HEIGHT * RETRO_WIDTH));
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
