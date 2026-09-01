//
// Lens
//
// A circular magnifier tracing a Lissajous figure over a still picture. The
// disc is a spherical cap: the sphere that meets the rim with height
// LENS_ZOOM has
//
//   z(r) = sqrt(LENS_ZOOM² + R² - r²)
//
// A pixel at (x, y) from the centre samples the picture at (x, y) · shift,
// with shift = LENS_ZOOM / z. At the rim z = LENS_ZOOM so the sample is
// undisplaced; at the centre shift < 1, so the picture is pulled inward
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

#define LENS_WIDTH 90
#define LENS_HEIGHT 90
#define LENS_RADIUS (LENS_WIDTH / 2)
#define LENS_ZOOM 20 // sphere height at the rim, in pixels
#define LENS_MARGIN 3 // kept between the disc and the screen edge

#define LENS_XCENTER ((RETRO_WIDTH - LENS_WIDTH) / 2)
#define LENS_XAMPLITUDE (LENS_XCENTER - LENS_MARGIN)
#define LENS_YCENTER ((RETRO_HEIGHT - LENS_HEIGHT) / 2)
#define LENS_YAMPLITUDE (LENS_YCENTER - LENS_MARGIN)
#define LENS_XPHASE (RETRO_SINCOS_ANGLE / 16) // offset that keeps the figure from opening on a crossing
#define LENS_PERIOD 14.6 // seconds for the figure to close

struct Lens {
	double x;
	double y;
	int buffer[LENS_WIDTH * LENS_HEIGHT];
} Lens1;

void DrawLens(Lens *lens, unsigned char *image)
{
	for (int lensy = 0; lensy < LENS_HEIGHT; lensy++) {
		for (int lensx = 0; lensx < LENS_WIDTH; lensx++) {
			if (lens->buffer[lensy * LENS_WIDTH + lensx] != 0) {
				unsigned char color = image[(lensy + (int)lens->y) * RETRO_WIDTH + (int)lens->x + lensx + lens->buffer[lensy * LENS_WIDTH + lensx]];
				RETRO_PutPixel((int)lens->x + lensx, (int)lens->y + lensy, color);
			}
		}
	}
}

void DEMO_Render(double time, double deltatime)
{
	unsigned char *image = RETRO_ImageData();

	// Calculate phase
	double phase = fmod(time * RETRO_SINCOS_ANGLE / LENS_PERIOD, RETRO_SINCOS_ANGLE);
	Lens1.x = LENS_XCENTER + LENS_XAMPLITUDE * COS(2 * phase + LENS_XPHASE);
	Lens1.y = LENS_YCENTER + LENS_YAMPLITUDE * COS(3 * phase);

	// Draw background
	RETRO_Blit(image);

	// Draw lens
	DrawLens(&Lens1, image);
}

void InitLens(Lens *lens)
{
	// Init table
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

			lens->buffer[(LENS_RADIUS + y) * LENS_WIDTH + LENS_RADIUS + x] = iy * RETRO_WIDTH + ix;
			lens->buffer[(LENS_RADIUS + y) * LENS_WIDTH + LENS_RADIUS - x] = iy * RETRO_WIDTH - ix;
			lens->buffer[(LENS_RADIUS - y) * LENS_WIDTH + LENS_RADIUS + x] = -iy * RETRO_WIDTH + ix;
			lens->buffer[(LENS_RADIUS - y) * LENS_WIDTH + LENS_RADIUS - x] = -iy * RETRO_WIDTH - ix;
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/monkey_320x240.pcx", true);
	InitLens(&Lens1);
}
