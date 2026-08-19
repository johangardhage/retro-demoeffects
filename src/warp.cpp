//
// Warp
//
// A log/power distortion, reflected into four quadrants. The table is the
// first quadrant only:
//
//   d_x = log(1 + y / (H/3)) / (3 (1.2 x / W)² + 1) · H/2
//   d_y = log(1 + x / (W/3)) / (3 (1.5 y / W)² + 1) · W/2
//
// Quadrant (sx, sy) samples the texture at (±d_x + dz, ±d_y + dw) with the
// signs of that quadrant. The four walks start at (W/2, H/2) and step away,
// so the centre is one pixel (the second write) rather than two copies of
// table (0, 0). Row 0 and column 0 sit outside those walks and take the
// outer table entry. dz, dw and the two phases live on the wall clock.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

#define TEXTURE_WIDTH 256
#define TEXTURE_HEIGHT 256
#define WARP_WIDTH (RETRO_WIDTH / 2)
#define WARP_HEIGHT (RETRO_HEIGHT / 2)
#define WARP_ALPHA_SPEED 0.12 // radians per second
#define WARP_BETA_SPEED 0.264

Point2D DistortTable[WARP_HEIGHT * WARP_WIDTH];

void DEMO_Render(double deltatime)
{
	static float alpha = 0, beta = 0, dz = 0, dw = 0;

	// Calculate movement
	alpha = fmod(alpha + deltatime * WARP_ALPHA_SPEED, 2 * M_PI);
	beta = fmod(beta + deltatime * WARP_BETA_SPEED, 2 * M_PI);
	dz += (sin(alpha + beta) * 2 + cos(beta) + 0.4) * deltatime * 60;
	dw += (cos(beta - alpha) * 3 + sin(alpha) + 0.2) * deltatime * 60;

	unsigned char *buffer = RETRO_FrameBuffer();
	unsigned char *image = RETRO_ImageData();

	// Draw warp. p1/p3 walk left, p2/p4 walk right, all four starting on
	// the centre pixel so that one texel is the seam.
	for (int y = 0; y < WARP_HEIGHT; y++) {
		unsigned char *p1 = buffer + (WARP_HEIGHT - y) * RETRO_WIDTH + WARP_WIDTH;
		unsigned char *p2 = p1;
		unsigned char *p3 = buffer + (WARP_HEIGHT + y) * RETRO_WIDTH + WARP_WIDTH;
		unsigned char *p4 = p3;

		for (int x = 0; x < WARP_WIDTH; x++) {
			int ddx = DistortTable[y * WARP_WIDTH + x].x;
			int ddy = DistortTable[y * WARP_WIDTH + x].y;

			*p1-- = image[WRAP(-ddy + dw, TEXTURE_HEIGHT) * TEXTURE_WIDTH + WRAP(-ddx + dz, TEXTURE_WIDTH)];
			*p2++ = image[WRAP(ddy + dw, TEXTURE_HEIGHT) * TEXTURE_WIDTH + WRAP(-ddx + dz, TEXTURE_WIDTH)];
			*p3-- = image[WRAP(-ddy + dw, TEXTURE_HEIGHT) * TEXTURE_WIDTH + WRAP(ddx + dz, TEXTURE_WIDTH)];
			*p4++ = image[WRAP(ddy + dw, TEXTURE_HEIGHT) * TEXTURE_WIDTH + WRAP(ddx + dz, TEXTURE_WIDTH)];
		}
	}

	// The walks stop at row 1 and column 1. Copy the outer neighbour so
	// the leftover line is not the cleared background.
	memcpy(buffer, buffer + RETRO_WIDTH, RETRO_WIDTH);
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		buffer[y * RETRO_WIDTH] = buffer[y * RETRO_WIDTH + 1];
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/flowers_256x256.pcx", true);

	// Init distortion table
	for (int y = 0; y < WARP_HEIGHT; y++) {
		for (int x = 0; x < WARP_WIDTH; x++) {
			double f = pow(x * 1.2 / RETRO_WIDTH, 2);
			double d = log(1 + y / (RETRO_HEIGHT / 3.0)) / (3 * f + 1) * RETRO_HEIGHT / 2;
			DistortTable[y * WARP_WIDTH + x].x = d;

			f = pow(y * 1.5 / RETRO_WIDTH, 2);
			d = log(1 + x / (RETRO_WIDTH / 3.0)) / (3 * f + 1) * RETRO_WIDTH / 2;
			DistortTable[y * WARP_WIDTH + x].y = d;
		}
	}
}
