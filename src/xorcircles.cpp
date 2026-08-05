//
// XOR circles
//
// Two sliding windows into a 640×480 two-level ring image, XOR'd per
// pixel. Window origins ride Lissajous orbits
//
//   (W/2 + (W/2) cos t,  H/2 + (H/2) sin(33 t / 10))
//
// so the 320×240 windows stay inside the larger picture (amplitude is
// half the slack). The second window is the same orbit delayed by 1.66.
// t lives on 20π, the shared period of cos t and sin(33t/10).
//
// Because the source is only 0 and 255, the XOR is only ever 0 or 255:
// where the rings agree the pixel is dark, where they differ it is
// bright. That is the classic XOR-circle Moiré.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocolor.h"

#define IMAGE_WIDTH 640
#define XOR_SPEED 1.0 // radians of the x-orbit per second
#define XOR_WOBBLE (33.0 / 10.0) // y vs x; with 2π this is a 20π period
#define XOR_PHASE 1.66 // second window, same orbit
#define XOR_PERIOD (20 * M_PI)

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * XOR_SPEED, XOR_PERIOD);

	unsigned char *image = RETRO_ImageData();

	// Coordinates for first circle
	int slx1 = RETRO_WIDTH / 2 + RETRO_WIDTH / 2 * cos(phase);
	int sly1 = RETRO_HEIGHT / 2 + RETRO_HEIGHT / 2 * sin(phase * XOR_WOBBLE);
	unsigned char *image1 = image + sly1 * IMAGE_WIDTH + slx1;

	// Coordinates for second circle
	int slx2 = RETRO_WIDTH / 2 + RETRO_WIDTH / 2 * cos(phase + XOR_PHASE);
	int sly2 = RETRO_HEIGHT / 2 + RETRO_HEIGHT / 2 * sin((phase + XOR_PHASE) * XOR_WOBBLE);
	unsigned char *image2 = image + sly2 * IMAGE_WIDTH + slx2;

	// Draw circles
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			RETRO_PutPixel(x, y, image1[y * IMAGE_WIDTH + x] ^ image2[y * IMAGE_WIDTH + x]);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. The image is black and white, so the xor of the two
	// circles only ever picks color 0 or 255
	RETRO_LoadImage("assets/xorcircles_640x480.pcx");
	RETRO_SetColor(0, RETRO_DARKMAGENTA);
	RETRO_SetColor(255, RETRO_HOTPINK);
}
