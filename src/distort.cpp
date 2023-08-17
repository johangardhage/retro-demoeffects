//
// distort.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define IMAGE_WIDTH 320

char DistortTableX[(RETRO_WIDTH * 2) * (RETRO_HEIGHT * 2)];
char DistortTableY[(RETRO_WIDTH * 2) * (RETRO_HEIGHT * 2)];

void DEMO_Render(double deltatime)
{
	// Calculate frame
	static double frame = 0;
	frame += deltatime * 400;

	unsigned char *image = RETRO_ImageData();

	// Calculate movement
	int x1 = (RETRO_WIDTH / 2) + (RETRO_WIDTH / 2 * cos(frame / 205));
	int x2 = (RETRO_WIDTH / 2) + (RETRO_WIDTH / 2 * sin(-frame / 197));
	int y1 = (RETRO_HEIGHT / 2) + (RETRO_HEIGHT / 2 * sin(frame / 231));
	int y2 = (RETRO_HEIGHT / 2) + (RETRO_HEIGHT / 2 * cos(-frame / 224));

	// Draw distortion
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int xsrc = y2 * (RETRO_WIDTH * 2) + x2 + y * (RETRO_WIDTH * 2) + x;
			int ysrc = y1 * (RETRO_WIDTH * 2) + x1 + y * (RETRO_WIDTH * 2) + x;

			int tx = x + DistortTableX[xsrc];
			int ty = y + DistortTableY[ysrc];

			if (ty >= 0 && ty < RETRO_HEIGHT && tx >= 0 && tx < RETRO_WIDTH) {
				unsigned char col = image[ty * IMAGE_WIDTH + tx];
				RETRO_PutPixel(x, y, col);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/distort_320x240.pcx");
	RETRO_SetPalette(RETRO_ImagePalette());

	// Init sin table
	int offset = 0;
	for (int y = 0; y < (RETRO_HEIGHT * 2); y++) {
		for (int x = 0; x < (RETRO_WIDTH * 2); x++) {
			DistortTableX[offset] = sin(x / 20) + sin(x * y / 2000) + sin((x + y) / 100) + sin((y - x) / 70) + sin((x + 4 * y) / 70) + sin(hypot(256 - x, (150 - y / 8)) / 40);
			DistortTableY[offset] = cos(x / 31) + cos(x * y / 1783) + cos((x + y) / 137) + cos((y - x) / 55) + cos((x + 8 * y) / 57) + sin(hypot(384 - x, (274 - y / 9)) / 51);
			offset++;
		}
	}
}
