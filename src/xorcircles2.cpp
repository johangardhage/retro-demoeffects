//
// XOR circles
//
// A shaded concentric wave XOR'd with alternating solid rings. XOR with
// 255 reverses the shade, giving curved checkerboard tiles with rounded
// lavender highlights and dark grooves. Two independently moving centers
// turn the checkerboard into ripples whenever the centers approach.
// After five seconds, a traveling horizontal sine distortion builds up
// on the solid rings, following Second Reality's TECHNO/KOEA.ASM effect.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define IMAGE_WIDTH (RETRO_WIDTH * 2)
#define IMAGE_HEIGHT (RETRO_HEIGHT * 2)
#define RING_PERIOD 23.0

unsigned char shadedrings[IMAGE_WIDTH * IMAGE_HEIGHT];
unsigned char solidrings[IMAGE_WIDTH * IMAGE_HEIGHT];

void DEMO_Render(double time, double deltatime)
{
	// Keep the centers inside the screen, so both windows always fit in
	// the precomputed images. Absolute time makes motion frame-rate independent.
	double phase = fmod(time, 20.0 * M_PI);
	int x1 = RETRO_WIDTH * (0.5 + 0.235 * sin(1.2 * phase + 0.2));
	int y1 = RETRO_HEIGHT * (0.5 - 0.325 * cos(1.8 * phase));
	int x2 = RETRO_WIDTH * (0.5 + 0.235 * sin(1.5 * phase + 0.2));
	int y2 = RETRO_HEIGHT * (0.5 + 0.325 * cos(1.3 * phase));
	unsigned char *first = shadedrings + (RETRO_HEIGHT - y1) * IMAGE_WIDTH + RETRO_WIDTH - x1;
	unsigned char *second = solidrings + (RETRO_HEIGHT - y2) * IMAGE_WIDTH + RETRO_WIDTH - x2;

	// Original: 15 strength steps, one every 16 frames at 70 Hz, after
	// five seconds. Smooth the ramp but retain its duration and 32px peak.
	double strength = fmin(1.0, fmax(0.0, (time - 5.0) * 70.0 / (16.0 * 15.0)));
	double wavephase = fmod(time * 70.0 * 7.0, 1024.0) * (2.0 * M_PI / 1024.0);

	for (int y = 0; y < RETRO_HEIGHT; ++y) {
		// Nine sine-table entries per original 200-line screen row.
		// The maximum shift is 32 pixels; the window has at least 84
		// pixels of horizontal margin, so sampling stays inside the image.
		double rowphase = (y * 200.0 / RETRO_HEIGHT + 1.0) * 9.0 * (2.0 * M_PI / 1024.0);
		int shift = 32.0 * strength * sin(wavephase + rowphase);
		unsigned char *distortedrow = second + y * IMAGE_WIDTH + shift;
		for (int x = 0; x < RETRO_WIDTH; ++x) {
			RETRO_PutPixel(x, y, first[y * IMAGE_WIDTH + x] ^ distortedrow[x]);
		}
	}
}

void DEMO_Initialize(void)
{
	for (int y = 0; y < IMAGE_HEIGHT; ++y) {
		for (int x = 0; x < IMAGE_WIDTH; ++x) {
			double radius = hypot(x - RETRO_WIDTH, y - RETRO_HEIGHT);
			int offset = y * IMAGE_WIDTH + x;
			shadedrings[offset] = 127.5 - 127.5 * cos(radius * 2.0 * M_PI / RING_PERIOD);
			solidrings[offset] = ((int)(radius * 2.0 / RING_PERIOD) & 1) ? 255 : 0;
		}
	}

	for (int i = 0; i < 256; ++i) {
		RETRO_SetColor(i, 15 + 193 * i / 255, 10 + 167 * i / 255, 20 + 188 * i / 255);
	}
}
