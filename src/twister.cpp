//
// Twister
//
// A square column, one scanline at a time. At row i the four vertices sit
// on the circle x = CX + R sin(θ + k π/2), k = 0..3, with
//
//   θ(i) = angle + 2 i / HEIGHT
//
// so the column twists two radians over the picture. An edge is drawn only
// when its left x is smaller than its right x: that is the facing test for
// a 2D silhouette (a back edge has x_left > x_right). Each face has its
// own color. angle lives on 2π.
//
// Vertices are rounded once and the spans are half-open, [x_left, x_right),
// so two faces that share a vertex tile exactly.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

#define TWISTER_CX 100
#define TWISTER_RADIUS 60
#define TWISTER_TWIST 2.0 // radians of twist from row 0 to row HEIGHT
#define TWISTER_SPEED 1.2 // radians per second
#define TWISTER_PERIOD (2 * M_PI)

//
// One scanline of one face, half-open in x. A back-facing edge has
// left >= right and covers nothing, which is the silhouette test.
//
void DrawSpan(int left, int right, int y, unsigned char color)
{
	left = MAX(left, 0);
	right = MIN(right, RETRO_WIDTH);

	unsigned char *row = RETRO_FrameBuffer() + y * RETRO_WIDTH;
	for (int x = left; x < right; x++) {
		row[x] = color;
	}
}

void DEMO_Render(double deltatime)
{
	// Calculate angle
	static double angle = 0;
	angle = fmod(angle + deltatime * TWISTER_SPEED, TWISTER_PERIOD);

	// Draw column
	for (int i = 0; i < RETRO_HEIGHT; i++) {
		double theta = angle + TWISTER_TWIST * i / RETRO_HEIGHT;
		int x[4];
		for (int j = 0; j < 4; j++) {
			x[j] = lround(TWISTER_CX + TWISTER_RADIUS * sin(theta + j * M_PI / 2));
		}

		for (int j = 0; j < 4; j++) {
			DrawSpan(x[j], x[(j + 1) % 4], i, j + 1);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetPalette(RETRO_Default8bitPalette);
}
