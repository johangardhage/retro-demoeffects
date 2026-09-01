//
// Twister
//
// A square column, one scanline at a time. At row i the four vertices sit
// on the circle x = CENTER_X + R sin(θ + k π/2), k = 0..3, with
//
//   torsion(phase) = TWIST sin(2 phase) cos(phase)
//   θ(y) = phase + torsion(phase) y / HEIGHT
//
// so the column winds up, straightens, and twists in the other direction as
// it turns, like twister3's animated torsion. An edge is drawn only when its
// left x is smaller than its right x: that is the facing test for a 2D
// silhouette (a back edge has x_left > x_right). Each face has its own color.
// phase lives on 2π.
//
// Vertices are rounded once and the spans are half-open, [x_left, x_right),
// so two faces that share a vertex tile exactly.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define TWISTER_CENTER_X (RETRO_WIDTH / 2)
#define TWISTER_RADIUS 60
#define TWISTER_TWIST 2.0 // peak radians of twist from row 0 to row HEIGHT
#define TWISTER_TORSION_WAVE 2 // wind-up cycles over one rotation
#define TWISTER_SPEED 1.2 // radians per second
#define TWISTER_PERIOD (2 * M_PI)

#define TWISTER_FACE_COLOR 33 // base color of the first face
#define TWISTER_FACE_STEP 16 // step from one face color to the next

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
	// Calculate phase and torsion
	static double phase = 0;
	phase = fmod(phase + deltatime * TWISTER_SPEED, TWISTER_PERIOD);
	double torsion = TWISTER_TWIST * sin(TWISTER_TORSION_WAVE * phase) * cos(phase);

	// Draw column
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		double theta = phase + torsion * y / RETRO_HEIGHT;
		double sin_radius = TWISTER_RADIUS * sin(theta);
		double cos_radius = TWISTER_RADIUS * cos(theta);
		int corner_x[4] = {
			(int)lround(TWISTER_CENTER_X + sin_radius),
			(int)lround(TWISTER_CENTER_X + cos_radius),
			(int)lround(TWISTER_CENTER_X - sin_radius),
			(int)lround(TWISTER_CENTER_X - cos_radius),
		};

		for (int corner = 0; corner < 4; corner++) {
			DrawSpan(corner_x[corner], corner_x[(corner + 1) & 3], y, TWISTER_FACE_COLOR + corner * TWISTER_FACE_STEP);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, 8, RETRO_BLACK, RETRO_BLUEBLACK);
	RETRO_CreateGradientPalette(8, 32, RETRO_BLUEBLACK, RETRO_WINE);
	RETRO_CreateGradientPalette(32, 96, RETRO_WINE, RETRO_ROSE);
	RETRO_CreateGradientPalette(96, 192, RETRO_ROSE, RETRO_PINK);
	RETRO_CreateGradientPalette(192, RETRO_COLORS, RETRO_PINK, RETRO_WHITE);
}
