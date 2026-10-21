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
// so the center is one pixel (the second write) rather than two copies of
// table (0, 0). Row 0 and column 0 sit outside those walks and take the
// outer table entry. dz and dw are worked out from the wall clock alone.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrovector.h"

constexpr int TEXTURE_WIDTH = 256;
constexpr int TEXTURE_HEIGHT = 256;
constexpr int WARP_WIDTH = RETRO_WIDTH / 2;
constexpr int WARP_HEIGHT = RETRO_HEIGHT / 2;
constexpr double WARP_ALPHA_SPEED = 0.12; // radians per second
constexpr double WARP_BETA_SPEED = 0.264;

static ivec2 DistortTable[WARP_HEIGHT * WARP_WIDTH];

void DEMO_Render(RETRO_Time time)
{
	// Calculate offsets. They drift at
	//
	//   dz' = 60 (2 sin((a + b) t) + cos(b t) + 0.4)
	//   dw' = 60 (3 cos((b - a) t) + sin(a t) + 0.2)
	//
	// texels a second, a and b the two speeds, and are those integrated from
	// zero at t = 0, so they follow the clock exactly. Wrapped to the texture,
	// so they stay small however long the demo runs.
	double t = time.total;
	double a = WARP_ALPHA_SPEED;
	double b = WARP_BETA_SPEED;
	double dz = mod(60 * (2 * (1 - cos((a + b) * t)) / (a + b) + sin(b * t) / b + 0.4 * t), (double)TEXTURE_WIDTH);
	double dw = mod(60 * (3 * sin((b - a) * t) / (b - a) + (1 - cos(a * t)) / a + 0.2 * t), (double)TEXTURE_HEIGHT);

	unsigned char *buffer = RETRO_FrameBuffer();
	unsigned char *image = RETRO_ImageData();

	// Draw warp. p1/p3 walk left, p2/p4 walk right, all four starting on
	// the center pixel so that one texel is the seam.
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

	// The walks stop at row 1 and column 1. Copy the outer neighbor so
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
