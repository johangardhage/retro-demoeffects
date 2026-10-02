//
// Lens
//
// A circular magnifier tracing a Lissajous figure over a still picture. The
// disc is a spherical cap: the sphere that meets the rim with height
// LENS_ZOOM has
//
//   z(r) = sqrt(LENS_ZOOM² + R² - r²)
//
// A pixel at (x, y) from the center samples the picture at (x, y) · shift,
// with shift = LENS_ZOOM / z. At the rim z = LENS_ZOOM so the sample is
// undisplaced; at the center shift < 1, so the picture is pulled inward
// (magnified). Offsets are lround'ed, packed as iy · WIDTH + ix, and
// mirrored into the four quadrants. A packed 0 is undisplaced: the blit
// already shows that pixel.
//
// The disc rides a 2:3 Lissajous figure, x on twice the base rate and y on
// three times, so the path closes after one turn of the phase. Both swings
// are cut to leave a LENS_MARGIN inset, which is what keeps the disc on
// screen - the draw is unclipped and relies on that.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define LENS_RADIUS 45 // pixels
#define LENS_SIZE (2 * LENS_RADIUS)
#define LENS_ZOOM 20 // sphere height at the rim, in pixels
#define LENS_MARGIN 3 // kept between the disc and the screen edge
#define LENS_PERIOD 14.6 // seconds for the disc's path to close

static int LensOffset[LENS_SIZE * LENS_SIZE];

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase. The disc rides a 2:3 Lissajous figure; the eighth of
	// a quarter turn on x keeps it from opening on a crossing
	double phase = fmod(time * RETRO_ANGLES_PER_TURN / LENS_PERIOD, RETRO_ANGLES_PER_TURN);
	int swingx = RETRO_WIDTH / 2 - LENS_RADIUS - LENS_MARGIN;
	int swingy = RETRO_HEIGHT / 2 - LENS_RADIUS - LENS_MARGIN;
	int cx = RETRO_WIDTH / 2 + swingx * COS(2 * phase + RETRO_ANGLES_PER_TURN / 16);
	int cy = RETRO_HEIGHT / 2 + swingy * COS(3 * phase);

	unsigned char *image = RETRO_ImageData();
	unsigned char *buffer = RETRO_FrameBuffer();

	// Draw background
	RETRO_Blit(image);

	// Draw lens
	for (int lensy = 0; lensy < LENS_SIZE; lensy++) {
		for (int lensx = 0; lensx < LENS_SIZE; lensx++) {
			int offset = LensOffset[lensy * LENS_SIZE + lensx];
			if (offset != 0) {
				int pixel = (cy - LENS_RADIUS + lensy) * RETRO_WIDTH + cx - LENS_RADIUS + lensx;
				buffer[pixel] = image[pixel + offset];
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/monkey_320x240.pcx", true);

	// Init table. One quadrant, mirrored into the other three
	for (int y = 0; y < LENS_RADIUS; y++) {
		for (int x = 0; x < LENS_RADIUS; x++) {
			int ix = 0;
			int iy = 0;
			int r2 = x * x + y * y;
			if (r2 < LENS_RADIUS * LENS_RADIUS) {
				float z = sqrt(LENS_ZOOM * LENS_ZOOM + LENS_RADIUS * LENS_RADIUS - r2);
				float shift = LENS_ZOOM / z;
				ix = lround(x * shift - x);
				iy = lround(y * shift - y);
			}

			LensOffset[(LENS_RADIUS + y) * LENS_SIZE + LENS_RADIUS + x] = iy * RETRO_WIDTH + ix;
			LensOffset[(LENS_RADIUS + y) * LENS_SIZE + LENS_RADIUS - x] = iy * RETRO_WIDTH - ix;
			LensOffset[(LENS_RADIUS - y) * LENS_SIZE + LENS_RADIUS + x] = -iy * RETRO_WIDTH + ix;
			LensOffset[(LENS_RADIUS - y) * LENS_SIZE + LENS_RADIUS - x] = -iy * RETRO_WIDTH - ix;
		}
	}
}
