//
// twister.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retroutils.h"

#define RAD 40
#define ZRATE 20
#define IMAGE_WIDTH 320
#define IMAGE_HEIGHT 32

Point3D vertex[4];
int visible[8][3] = { {3, 0, 1}, {2, 3, 0}, {2, 3, 0}, {1, 2, 3}, {1, 2, 3}, {0, 1, 2}, {0, 1, 2}, {3, 0, 1} };

void DEMO_Render(double deltatime)
{
	static int amul = 0;
	amul++;

	for (int x = 0; x < RETRO_WIDTH; x++) {
		int angle = WRAP((100 * COS(amul)) + ((80 * SIN(x / 4 + amul * 2)) * COS(SIN(amul))), RETRO_SINCOS_ANGLE);
		int vs = angle / IMAGE_HEIGHT;

		vertex[visible[vs][0]].y = RAD * SIN(angle + (visible[vs][0] * 64));
		vertex[visible[vs][0]].z = RAD * COS(angle + (visible[vs][0] * 64));
		vertex[visible[vs][1]].y = RAD * SIN(angle + (visible[vs][1] * 64));
		vertex[visible[vs][1]].z = RAD * COS(angle + (visible[vs][1] * 64));
		vertex[visible[vs][2]].y = RAD * SIN(angle + (visible[vs][2] * 64));
		vertex[visible[vs][2]].z = RAD * COS(angle + (visible[vs][2] * 64));

		float c0 = (ZRATE * 63) / (RAD - vertex[visible[vs][0]].z + ZRATE);
		float c1 = (ZRATE * 63) / (RAD - vertex[visible[vs][1]].z + ZRATE);
		float c2 = (ZRATE * 63) / (RAD - vertex[visible[vs][2]].z + ZRATE);

		float dm0 = 0;
		float dc0 = 0;
		float dh0 = vertex[visible[vs][1]].y - vertex[visible[vs][0]].y;
		if (dh0 != 0) {
			dm0 = IMAGE_HEIGHT / dh0;
			dc0 = (c1 - c0) / dh0;
		}

		float dm1 = 0;
		float dc1 = 0;
		float dh1 = vertex[visible[vs][2]].y - vertex[visible[vs][1]].y;
		if (dh1 != 0) {
			dm1 = IMAGE_HEIGHT / dh1;
			dc1 = (c2 - c1) / dh1;
		}

		// Move scroll
		float om = (x + (amul * 2)) % IMAGE_WIDTH;
		float pm0 = 0;
		float pm1 = 0;

		int y = 159 + vertex[visible[vs][0]].y;
		float py = vertex[visible[vs][0]].y;

		unsigned char *image = RETRO_ImageData();

		for (; py < vertex[visible[vs][1]].y; py++) {
			unsigned char color = image[(int)pm0 * IMAGE_WIDTH + (int)om];
			RETRO_PutPixel(x, y, c0 + color);
			c0 += dc0;
			pm0 += dm0;
			y++;
		}

		for (; py < vertex[visible[vs][2]].y; py++) {
			unsigned char color = image[(int)pm1 * IMAGE_WIDTH + (int)om];
			RETRO_PutPixel(x, y, c1 + color);
			c1 += dc1;
			pm1 += dm1;
			y++;
		}
	}
}

void PrepareImage(const char *loadfile, const char *savefile)
{
	RETRO_LoadImage(loadfile);
	unsigned char *image = RETRO_ImageData();
	RETRO_Palette *palette = RETRO_ImagePalette();;
	for (int i = 0; i < IMAGE_WIDTH * IMAGE_HEIGHT; i++) {
		if (image[i] != 0) {
			image[i] = 64;
		} else {
			image[i] = 0;
		}
	}
	for (int i = 0; i < 64; i++) {
		palette[i].r = i;
		palette[i].g = 0;
		palette[i].b = i;
	}
	for (int i = 64; i < 128; i++) {
		palette[i].r = 0;
		palette[i].g = i;
		palette[i].b = i;
	}
	RETRO_SaveImage(savefile, image, palette, IMAGE_WIDTH, IMAGE_HEIGHT);
	RETRO_FreeImage();
}

void DEMO_Initialize(void)
{
//	PrepareImage("assets/image.pcx", "assets/twister_320x32.pcx");
	RETRO_LoadImage("assets/twister_320x32.pcx");
	RETRO_SetPalette(RETRO_ImagePalette());
}
