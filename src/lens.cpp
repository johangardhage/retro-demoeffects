//
// Lens
//
// A circular magnifier bouncing over a still picture. The disc is a spherical
// cap: the sphere that meets the rim with height LENS_ZOOM has
//
//   z(r) = sqrt(LENS_ZOOM² + R² - r²)
//
// A pixel at (x, y) from the centre samples the picture at (x, y) · shift,
// with shift = LENS_ZOOM / z. At the rim z = LENS_ZOOM so the sample is
// undisplaced; at the centre shift < 1, so the picture is pulled inward
// (magnified). Offsets are lround'ed, packed as iy · WIDTH + ix, and
// mirrored into the four quadrants. A packed 0 is undisplaced: the blit
// already shows that pixel. The disc bounces off a LENS_MARGIN inset.
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

struct Lens {
	double x = 16;
	double y = 16;
	int xspeed = 100;
	int yspeed = 100;
	int buffer[LENS_WIDTH * LENS_HEIGHT];
} Lens1;

void ReflectPosition(double *position, int *speed, double minimum, double maximum)
{
	while (*position < minimum || *position > maximum) {
		if (*position < minimum) *position = 2 * minimum - *position;
		else *position = 2 * maximum - *position;
		*speed = -*speed;
	}
}

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

void DEMO_Render(double deltatime)
{
	unsigned char *image = RETRO_ImageData();

	// Calculate movement
	Lens1.x += Lens1.xspeed * deltatime;
	Lens1.y += Lens1.yspeed * deltatime;
	ReflectPosition(&Lens1.x, &Lens1.xspeed, LENS_MARGIN, RETRO_WIDTH - LENS_WIDTH - LENS_MARGIN);
	ReflectPosition(&Lens1.y, &Lens1.yspeed, LENS_MARGIN, RETRO_HEIGHT - LENS_HEIGHT - LENS_MARGIN);

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
	RETRO_LoadImage("assets/monkey_320x240.pcx");
	RETRO_SetPalette(RETRO_ImagePalette());
	InitLens(&Lens1);
}
