//
// Roto-tunnel
//
// A polar map of a still texture, looked up as if the screen were the mouth
// of a tube. A pixel at offset (dx, dy) from the vanishing point has
//
//   r = |(dx, dy)|
//   v = RATIO / r                      along the tunnel
//   u = atan2(dx, dy) · TW / π         around it
//
// Large r is the near rim (small v). atan2(dx, dy) is from +y and spans
// [−TW, TW], so the texture wraps twice around the tube. The tables are
// 2W × 2H, indexed from a wandering origin; the vanishing point on
// screen is (W − origin_x, H − origin_y). Adding (sx, sy) flies and
// spins. In the draw, distance is texture x and angle is texture y.
// phase lives on 20π (cos θ and sin(33θ/10) share that period).
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define TEXTURE_WIDTH 256
#define TEXTURE_HEIGHT 256
#define TUNNEL_RATIO (32 * TEXTURE_HEIGHT) // v = RATIO/r, so 32 texture rows per 1/r
#define TUNNEL_SPEED 1.0 // seconds of flight per second
#define TUNNEL_SPIN 0.25 // texture turns around the axis per second of flight
#define TUNNEL_WOBBLE (33.0 / 10.0) // nod/sway; with 2π this is a 20π period
#define TUNNEL_PERIOD (20 * M_PI)

int DistanceTable[2 * RETRO_HEIGHT][2 * RETRO_WIDTH];
int AngleTable[2 * RETRO_HEIGHT][2 * RETRO_WIDTH];

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * TUNNEL_SPEED, TUNNEL_PERIOD);

	unsigned char *image = RETRO_ImageData();

	// Calculate tunnel movement
	int sx = TEXTURE_WIDTH * phase;
	int sy = TEXTURE_HEIGHT * TUNNEL_SPIN * phase;

	// Calculate camera movement
	int dx = RETRO_WIDTH / 2 + RETRO_WIDTH / 2 * cos(phase);
	int dy = RETRO_HEIGHT / 2 + RETRO_HEIGHT / 2 * sin(phase * TUNNEL_WOBBLE);

	// Draw tunnel
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int tx = WRAP(DistanceTable[y + dy][x + dx] + sx, TEXTURE_WIDTH);
			int ty = WRAP(AngleTable[y + dy][x + dx] + sy, TEXTURE_HEIGHT);
			unsigned char color = image[ty * TEXTURE_WIDTH + tx];

			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/flowers_256x256.pcx", true);

	// Init tables
	for (int y = 0; y < RETRO_HEIGHT * 2; y++) {
		for (int x = 0; x < RETRO_WIDTH * 2; x++) {
			int dx = x - RETRO_WIDTH;
			int dy = y - RETRO_HEIGHT;
			AngleTable[y][x] = atan2(dx, dy) * TEXTURE_WIDTH / M_PI;
			DistanceTable[y][x] = TUNNEL_RATIO / MAX(1.0, sqrt((double)dx * dx + (double)dy * dy));
		}
	}
}
