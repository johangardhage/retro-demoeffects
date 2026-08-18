//
// Twister 2
//
// A textured square column, one vertical slice per screen column. The four
// vertices live on the circle (y, z) = RAD (sin φ, cos φ) at 90°
// steps (64 units of a 256-angle table). z is toward the viewer. φ is
//
//   φ(x, phase) = LEAN cos phase + A(phase) sin(x/4 + 2 phase)
//   A(phase)    = TWIST_MIN + (TWIST − TWIST_MIN) · (1 + cos(phase/5)) / 2
//
// in table units, WRAP256. The twist amplitude breathes over five turns of
// the table, so it does not lock to the lean and still closes on
// TWISTER_PERIOD.
// vs = φ / 32 picks which three vertices are the two visible faces
// (the front-most of the four). Each face is a y-span of IMAGE_HEIGHT
// texels with a 1/z shade
//
//   c = 63 · ZRATE / (RAD − z + ZRATE)
//
// added to the texel (0 or 64). The texture u scrolls with x + 2 phase.
// phase lives on 1280, the lcm of the 256-table and the 320-wide scroll.
// The column is centred at y = 159.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retroutils.h"

#define RAD 40
#define ZRATE 20
#define LEAN 100 // table units the column leans as a whole
#define TWIST 80 // table units of twist from one end of the column to the other
#define TWIST_MIN 24 // and the least it falls to as the amplitude breathes
#define TWISTER_CY 159
#define IMAGE_WIDTH 320
#define IMAGE_HEIGHT 32
#define TWISTER_SPEED 60 // table units per second
#define TWISTER_PERIOD 1280 // lcm(256, 160, 5*256): table, u = x+2t, and the breathing twist

int Visible[8][3] = { {3, 0, 1}, {2, 3, 0}, {2, 3, 0}, {1, 2, 3}, {1, 2, 3}, {0, 1, 2}, {0, 1, 2}, {3, 0, 1} };

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * TWISTER_SPEED, TWISTER_PERIOD);
	int iphase = (int)phase;

	unsigned char *image = RETRO_ImageData();

	// Constant over the column, so worked out once rather than per slice
	double twist = TWIST_MIN + (TWIST - TWIST_MIN) * (1 + COS(iphase / 5.0)) / 2;

	// Draw column slices
	for (int x = 0; x < RETRO_WIDTH; x++) {
		int angle = WRAP(LEAN * COS(iphase) + twist * SIN(x / 4.0 + iphase * 2), RETRO_SINCOS_ANGLE);
		int vs = angle / (RETRO_SINCOS_ANGLE / 8);

		int i0 = Visible[vs][0];
		int i1 = Visible[vs][1];
		int i2 = Visible[vs][2];

		double y0 = RAD * SIN(angle + i0 * 64);
		double z0 = RAD * COS(angle + i0 * 64);
		double y1 = RAD * SIN(angle + i1 * 64);
		double z1 = RAD * COS(angle + i1 * 64);
		double y2 = RAD * SIN(angle + i2 * 64);
		double z2 = RAD * COS(angle + i2 * 64);

		int iy0 = y0;
		int iy1 = y1;
		int iy2 = y2;

		float c0 = (ZRATE * 63) / (RAD - z0 + ZRATE);
		float c1 = (ZRATE * 63) / (RAD - z1 + ZRATE);
		float c2 = (ZRATE * 63) / (RAD - z2 + ZRATE);

		float dm0 = 0;
		float dc0 = 0;
		int dh0 = iy1 - iy0;
		if (dh0 != 0) {
			dm0 = (float)IMAGE_HEIGHT / dh0;
			dc0 = (c1 - c0) / dh0;
		}

		float dm1 = 0;
		float dc1 = 0;
		int dh1 = iy2 - iy1;
		if (dh1 != 0) {
			dm1 = (float)IMAGE_HEIGHT / dh1;
			dc1 = (c2 - c1) / dh1;
		}

		// Move scroll
		int u = WRAP(x + iphase * 2, IMAGE_WIDTH);
		float pm0 = 0;
		float pm1 = 0;
		int y = TWISTER_CY + iy0;

		for (int i = 0; i < dh0; i++) {
			unsigned char color = image[(int)pm0 * IMAGE_WIDTH + u];
			RETRO_PutPixel(x, y, c0 + color);
			c0 += dc0;
			pm0 += dm0;
			y++;
		}

		for (int i = 0; i < dh1; i++) {
			unsigned char color = image[(int)pm1 * IMAGE_WIDTH + u];
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
	RETRO_Palette *palette = RETRO_ImagePalette();
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
