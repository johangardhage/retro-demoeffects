//
// melt.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

#define NUM_BLOBS 5
#define BLOB_RADIUS 4
#define SIMULATION_STEP (1.0 / 60.0)

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

void UpdateMelt(void)
{
	static double t = 0;
	t += SIMULATION_STEP;

	// Draw orbiting blobs
	for (int i = 0; i < NUM_BLOBS; i++) {
		int x = (RETRO_WIDTH / 2) + cos(t * (0.6 + i * 0.23) + i * 1.3) * (40 + i * 22);
		int y = (RETRO_HEIGHT / 2) + sin(t * (0.9 + i * 0.31) + i * 2.1) * (30 + i * 16);
		DrawBlob(x, y, 255);
	}

	// Melt the trails outwards
	RETRO_Blur(RETRO_BLUR_RING, 1);
}

void DEMO_Render2(double deltatime)
{
	static double accumulator = 0;
	accumulator = MIN(accumulator + deltatime, SIMULATION_STEP * 15);
	while (accumulator >= SIMULATION_STEP) {
		UpdateMelt();
		accumulator -= SIMULATION_STEP;
	}
	RETRO_Flip();
}

void DEMO_Initialize(void)
{
	// Init palette
	for (int i = 0; i < 128; i++) {
		RETRO_SetColor(i, 0, i, i * 2);
	}
	for (int i = 128; i < 256; i++) {
		RETRO_SetColor(i, (i - 128) * 2, i, 255);
	}
}
