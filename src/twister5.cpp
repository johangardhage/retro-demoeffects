//
// Twister 5
//
// A textured square column, one vertical slice per screen column. The four
// vertices live on the circle (y, z) = RADIUS (sin φ, cos φ) at 90°
// steps (64 units of a 256-angle table). z is toward the viewer. φ is
//
//   φ(x, phase) = TWISTER_LEAN cos phase + A(phase) sin(x/4 + 2 phase)
//   A(phase) = TWISTER_TWIST_MIN
//            + (TWISTER_TWIST_MAX − TWISTER_TWIST_MIN) · (1 + cos(phase/5)) / 2
//
// in table units. The twist amplitude breathes over five turns of the table,
// so it does not lock to the lean and still closes on TWISTER_PERIOD. The
// As in the other twisters, an edge is drawn when its first screen coordinate
// is less than its second; the two ascending edges are the visible faces and
// the descending edges face away. Each face is a y-span of
// TWISTER_IMAGE_HEIGHT texels with a 1/z shade
//
//   c = 63 · TWISTER_DEPTH_RATE / (TWISTER_RADIUS − z + TWISTER_DEPTH_RATE)
//
// added to the texel (0 or 64). The texture u scrolls with x + 2 phase.
// phase lives on 1280, the lcm of the 256-table and the 320-wide scroll.
// The column is centred at y = 159.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrocolor.h"
#include "lib/retromain.h"

#define TWISTER_RADIUS 40
#define TWISTER_DEPTH_RATE 20
#define TWISTER_LEAN 100 // table units the column leans as a whole
#define TWISTER_TWIST_MAX 80 // greatest twist amplitude, in table units
#define TWISTER_TWIST_MIN 24 // least twist amplitude as it breathes
#define TWISTER_CENTER_Y 159
#define TWISTER_IMAGE_WIDTH 320
#define TWISTER_IMAGE_HEIGHT 32
#define TWISTER_SHADES 64
#define TWISTER_SPEED 60 // table units per second
#define TWISTER_PERIOD 1280 // lcm(256, 160, 5*256): table, u = x+2t, and the breathing twist

//
// One vertical slice of one face, half-open in y. The whole texture height
// is mapped across the span while the depth shade is interpolated between
// its two corners.
//
void DrawSpan(int top, int bottom, int x, unsigned char *image, int u, float top_shade, float bottom_shade)
{
	int height = bottom - top;
	if (height <= 0) {
		return;
	}

	float texture_y = 0;
	float texture_step = (float)TWISTER_IMAGE_HEIGHT / height;
	float shade = top_shade;
	float shade_step = (bottom_shade - top_shade) / height;

	for (int y = top; y < bottom; y++) {
		unsigned char texel = image[(int)texture_y * TWISTER_IMAGE_WIDTH + u];
		RETRO_PutPixel(x, TWISTER_CENTER_Y + y, shade + texel);
		texture_y += texture_step;
		shade += shade_step;
	}
}

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * TWISTER_SPEED, TWISTER_PERIOD);

	unsigned char *image = RETRO_ImageData();

	// Constant over the column, so worked out once rather than per slice
	double twist = TWISTER_TWIST_MIN + (TWISTER_TWIST_MAX - TWISTER_TWIST_MIN) * (1 + COS(phase / 5.0)) / 2;

	// Draw column slices
	for (int x = 0; x < RETRO_WIDTH; x++) {
		double angle = TWISTER_LEAN * COS(phase) + twist * SIN(x / 4.0 + phase * 2);
		double sin_radius = TWISTER_RADIUS * SIN(angle);
		double cos_radius = TWISTER_RADIUS * COS(angle);
		int corner_y[4] = {
			(int)lround(sin_radius),
			(int)lround(cos_radius),
			(int)lround(-sin_radius),
			(int)lround(-cos_radius),
		};
		double corner_z[4] = { cos_radius, -sin_radius, -cos_radius, sin_radius };
		float corner_shade[4];
		for (int corner = 0; corner < 4; corner++) {
			corner_shade[corner] = (TWISTER_DEPTH_RATE * (TWISTER_SHADES - 1)) / (TWISTER_RADIUS - corner_z[corner] + TWISTER_DEPTH_RATE);
		}

		// Move scroll
		int u = WRAP(x + phase * 2, TWISTER_IMAGE_WIDTH);

		// As in the other twisters, ascending edges face the viewer and
		// descending edges face away, so DrawSpan rejects the latter.
		for (int corner = 0; corner < 4; corner++) {
			int next = (corner + 1) & 3;
			DrawSpan(corner_y[corner], corner_y[next], x, image, u, corner_shade[corner], corner_shade[next]);
		}
	}
}

void DEMO_Initialize(void)
{
	// Load the texture, then replace its color ramps with two pink ramps
	RETRO_LoadImage("assets/twister_320x32.pcx", true);
	RETRO_CreateGradientPalette(0, 64, RETRO_BLACK, RETRO_DEEPPINK);
	RETRO_CreateGradientPalette(64, 128, RETRO_WINE, RETRO_PINK);
}
