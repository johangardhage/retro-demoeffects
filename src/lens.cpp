//
// lens.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define LENS_WIDTH 90
#define LENS_HEIGHT 90
#define LENS_ZOOM 20

struct Lens {
	double x = 16;
	double y = 16;
	int xspeed = 100;
	int yspeed = 100;
	unsigned int buffer[LENS_WIDTH * LENS_HEIGHT];
} Lens1;

void DrawLens(Lens *lens, unsigned char *image)
{
	for (int lensy = 0; lensy < LENS_HEIGHT; lensy++) {
		for (int lensx = 0; lensx < LENS_WIDTH; lensx++) {
			if (lens->buffer[lensy * LENS_WIDTH + lensx] != 0) {
				unsigned char color = image[(lensy + (int)lens->y) * RETRO_WIDTH + (int)lens->x + lensx + lens->buffer[lensy * LENS_WIDTH + lensx]];
				RETRO_PutPixel(lens->x + lensx, lens->y + lensy, color);
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
	if (Lens1.x > (RETRO_WIDTH - LENS_WIDTH - 3) || Lens1.x < 3) {
		Lens1.xspeed = -Lens1.xspeed;
	}
	if (Lens1.y > (RETRO_HEIGHT - LENS_HEIGHT - 3) || Lens1.y < 3) {
		Lens1.yspeed = -Lens1.yspeed;
	}

	// Draw background
	RETRO_Blit(image);

	// Draw lens
	DrawLens(&Lens1, image);
}

void InitLens(Lens *lens)
{
	int radius = LENS_WIDTH / 2;
	int ix, iy;

	for (int y = 0; y < LENS_HEIGHT >> 1; y++) {
		for (int x = 0; x < LENS_WIDTH >> 1; x++) {
			if ((x * x + y * y) < (radius * radius)) {
				float shift = LENS_ZOOM / sqrt(LENS_ZOOM * LENS_ZOOM - (x * x + y * y - radius * radius));
				ix = x * shift - x;
				iy = y * shift - y;
			} else {
				ix = 0;
				iy = 0;
			}

			lens->buffer[(radius + y) * LENS_WIDTH + radius + x] = iy * RETRO_WIDTH + ix;
			lens->buffer[(radius + y) * LENS_WIDTH + radius - x] = iy * RETRO_WIDTH - ix;
			lens->buffer[(radius - y) * LENS_WIDTH + radius + x] = -iy * RETRO_WIDTH + ix;
			lens->buffer[(radius - y) * LENS_WIDTH + radius - x] = -iy * RETRO_WIDTH - ix;
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/monkey_320x240.pcx");
	RETRO_SetPalette(RETRO_ImagePalette());
	InitLens(&Lens1);
}
