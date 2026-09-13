//
// Tunnel
//
// A polar map of a still texture, looked up as if the screen were the mouth
// of a tube. A pixel at offset (dx, dy) from the screen centre has
//
//   r = |(dx, dy)|
//   v = RATIO / r                      along the tunnel
//   u = atan2(dx, dy) · TW / π         around it
//
// Large r is the near rim (small v). atan2(dx, dy) is from +y and spans
// [−TW, TW], so the texture wraps twice around the tube. The vanishing
// point sits fixed at screen centre, so the tables need only cover the
// screen itself, W × H, rather than the 2W × 2H a wandering origin would
// need to reach every position such a point could sit at. Adding (sx, sy)
// flies and spins; depth is texture x and angle is texture y.
//
// r < MOUTH_RADIUS is a hole rather than a texel: DepthTable already
// holds RATIO/r, so that test is a threshold on the table's raw value, no
// separate table needed. The picture owns the palette and leaves no entry
// spare, so the hole takes index 0, forced to true black.
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
#define TUNNEL_MOUTH_RADIUS 10.0 // pixels, the hole cut at the vanishing point
#define TUNNEL_HOLE_THRESHOLD (TUNNEL_RATIO / TUNNEL_MOUTH_RADIUS)

int DepthTable[RETRO_HEIGHT][RETRO_WIDTH];
int AngleTable[RETRO_HEIGHT][RETRO_WIDTH];

void DEMO_Render(double time, double deltatime)
{
	unsigned char *image = RETRO_ImageData();

	// Calculate tunnel movement. WRAP folds any int, so phase need not be
	// bounded itself; sx and sy wrap into the texture however large time
	// grows.
	double phase = time * TUNNEL_SPEED;
	int sx = TEXTURE_WIDTH * phase;
	int sy = TEXTURE_HEIGHT * TUNNEL_SPIN * phase;

	// Draw tunnel
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int depth = DepthTable[y][x];
			if (depth > TUNNEL_HOLE_THRESHOLD) {
				RETRO_PutPixel(x, y, 0);
				continue;
			}

			int tx = WRAP(depth + sx, TEXTURE_WIDTH);
			int ty = WRAP(AngleTable[y][x] + sy, TEXTURE_HEIGHT);
			unsigned char color = image[ty * TEXTURE_WIDTH + tx];

			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/flowers_256x256.pcx", true);

	// That entry is all but black already, and claiming it outright costs
	// the picture nothing.
	RETRO_SetColor(0, 0, 0, 0);

	// Init tables
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int dx = x - RETRO_WIDTH / 2;
			int dy = y - RETRO_HEIGHT / 2;
			AngleTable[y][x] = atan2(dx, dy) * TEXTURE_WIDTH / M_PI;
			DepthTable[y][x] = TUNNEL_RATIO / MAX(1.0, sqrt((double)dx * dx + (double)dy * dy));
		}
	}
}
