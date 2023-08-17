//
// xorcircles.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define IMAGE_WIDTH 640

void DEMO_Render(double deltatime)
{
	// Calculate frame
	static double frame = 0;
	frame += deltatime * 1;

	unsigned char *image = RETRO_ImageData();

	// Coordinates for first circle
	int slx1 = RETRO_WIDTH / 2 + (RETRO_WIDTH / 2 * cos(frame));
	int sly1 = RETRO_HEIGHT / 2 + (RETRO_HEIGHT / 2 * sin(frame * 3.3));
	unsigned char *image1 = image + (sly1 * IMAGE_WIDTH) + slx1;

	// Coordinates for second circle
	int slx2 = RETRO_WIDTH / 2 + (RETRO_WIDTH / 2 * cos(frame + 1.66));
	int sly2 = RETRO_HEIGHT / 2 + (RETRO_HEIGHT / 2 * sin((frame + 1.66) * 3.3));
	unsigned char *image2 = image + (sly2 * IMAGE_WIDTH) + slx2;;

	// Draw circles
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			RETRO_PutPixel(x, y, image1[y * IMAGE_WIDTH + x] ^ image2[y * IMAGE_WIDTH + x]);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_LoadImage("assets/xorcircles_640x480.pcx");
	RETRO_SetColor(0, 155, 20, 147);
	RETRO_SetColor(255, 255, 102, 204);
}
