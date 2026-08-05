//
// melt.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retrocolor.h"

#define NUM_BLOBS 5
#define BLOB_RADIUS 4

void DrawBlob(int xc, int yc, unsigned char color)
{
	for (int y = -BLOB_RADIUS; y <= BLOB_RADIUS; y++) {
		for (int x = -BLOB_RADIUS; x <= BLOB_RADIUS; x++) {
			if (x * x + y * y <= BLOB_RADIUS * BLOB_RADIUS) {
				int px = xc + x;
				int py = yc + y;
				if (px >= 0 && px < RETRO_WIDTH && py >= 0 && py < RETRO_HEIGHT) {
					RETRO_PutPixel(px, py, color);
				}
			}
		}
	}
}

//
// Advance the melt in fixed steps. It is one ring blur per step into a framebuffer that
// is never cleared, so how far the trails spread follows the step rate, not the orbits.
//
void DEMO_Update(double deltatime)
{
	static double t = 0;
	t += deltatime;

	// Draw orbiting blobs
	for (int i = 0; i < NUM_BLOBS; i++) {
		int x = (RETRO_WIDTH / 2) + cos(t * (0.6 + i * 0.23) + i * 1.3) * (40 + i * 22);
		int y = (RETRO_HEIGHT / 2) + sin(t * (0.9 + i * 0.31) + i * 2.1) * (30 + i * 16);
		DrawBlob(x, y, 255);
	}

	// Melt the trails outwards
	RETRO_Blur(RETRO_BLUR_RING, 1);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, 128, RETRO_BLACK, RETRO_AZURE);
	RETRO_CreateGradientPalette(128, RETRO_COLORS, RETRO_AZURE, RETRO_WHITE);
}
