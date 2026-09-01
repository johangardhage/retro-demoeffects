//
// Bloom
//
// Orbiting discs painted into a framebuffer that is never cleared, then
// one ring blur per step. Blob i follows
//
//   x = W/2 + (40 + 22 i) cos(phase (0.6 + 0.23 i) + 1.3 i)
//   y = H/2 + (30 + 16 i) sin(phase (0.9 + 0.31 i) + 2.1 i)
//
// phase lives on 200π (every rate is a multiple of 1/100). The discs seed
// 255; the blur is
//
//   T' = max(0, mean(8 neighbours) − 1)
//
// with no self term (RETRO_BLUR_RING), so heat spreads outward and the
// seed itself is replaced. Every tap reads the previous step, so the ring
// stays symmetric. The −1 is extra cooling. How far the trails travel
// follows the step rate, not the orbital speed.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retropalette.h"

#define NUM_BLOBS 5
#define BLOB_RADIUS 4
#define BLOOM_PERIOD (200 * M_PI)

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
// Advance the bloom in fixed steps. It is one ring blur per step into a framebuffer that
// is never cleared, so how far the trails spread follows the step rate, not the orbits.
//
void DEMO_FixedUpdate(double timestep)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + timestep, BLOOM_PERIOD);

	// Draw orbiting blobs
	for (int i = 0; i < NUM_BLOBS; i++) {
		int x = RETRO_WIDTH / 2 + cos(phase * (0.6 + i * 0.23) + i * 1.3) * (40 + i * 22);
		int y = RETRO_HEIGHT / 2 + sin(phase * (0.9 + i * 0.31) + i * 2.1) * (30 + i * 16);
		DrawBlob(x, y, 255);
	}

	// Bleed the trails outwards
	RETRO_Blur(RETRO_BLUR_RING, 1);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, 128, RETRO_BLACK, RETRO_AZURE);
	RETRO_CreateGradientPalette(128, RETRO_COLORS, RETRO_AZURE, RETRO_WHITE);
}
