//
// Kaleidoscope
//
// A texture reflected into twelve rotating mirror sectors. For a screen pixel
// at polar coordinates (r, a), the angle is first wrapped into one sector and
// then reflected about its centre line:
//
//   w = (a + rotation) mod (2pi / sectors)
//   m = min(w, 2pi / sectors - w)
//
// Sampling the texture at r(cos(m + spin), sin(m + spin)) makes adjacent
// sectors meet as mirror images. The source also follows a Lissajous orbit and
// breathes in radius, so the image changes without breaking the symmetry.
// Radius, angle and trigonometry are precalculated; the pixel loop uses only
// integer lookup, multiply and wrap operations.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define TEXTURE_WIDTH 256
#define TEXTURE_HEIGHT 256
#define KALEIDOSCOPE_SECTORS 12
#define ANGLE_STEPS 3072
#define SECTOR_STEPS (ANGLE_STEPS / KALEIDOSCOPE_SECTORS)
#define TRIG_SHIFT 14
#define TRIG_SCALE (1 << TRIG_SHIFT)
#define ZOOM_SHIFT 8
#define ANIMATION_SPEED 0.25 // radians a second; the complete motion repeats at 2pi

int RadiusTable[RETRO_HEIGHT][RETRO_WIDTH];
int AngleTable[RETRO_HEIGHT][RETRO_WIDTH];
int SinTable[ANGLE_STEPS];
int CosTable[ANGLE_STEPS];

void DEMO_Render(double deltatime)
{
	static double phase = 0;
	phase = fmod(phase + deltatime * ANIMATION_SPEED, 2 * M_PI);

	unsigned char *image = RETRO_ImageData();
	int mirrorrotation = phase * ANGLE_STEPS / (2 * M_PI);
	int texturespin = -2 * mirrorrotation;
	int panx = 36 * cos(2 * phase);
	int pany = 36 * sin(3 * phase);
	int zoom = (0.85 + 0.20 * sin(phase)) * (1 << ZOOM_SHIFT);

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int angle = WRAP(AngleTable[y][x] + mirrorrotation, ANGLE_STEPS);
			int mirrorangle = angle % SECTOR_STEPS;
			if (mirrorangle > SECTOR_STEPS / 2) {
				mirrorangle = SECTOR_STEPS - mirrorangle;
			}

			int sourceangle = WRAP(mirrorangle + texturespin, ANGLE_STEPS);
			int radius = RadiusTable[y][x] * zoom >> ZOOM_SHIFT;
			int tx = TEXTURE_WIDTH / 2 + panx + (radius * CosTable[sourceangle] >> TRIG_SHIFT);
			int ty = TEXTURE_HEIGHT / 2 + pany + (radius * SinTable[sourceangle] >> TRIG_SHIFT);
			unsigned char color = image[WRAP(ty, TEXTURE_HEIGHT) * TEXTURE_WIDTH + WRAP(tx, TEXTURE_WIDTH)];

			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_Image *image = RETRO_LoadImage("assets/flowers_256x256.pcx", true);
	if (image->width != TEXTURE_WIDTH || image->height != TEXTURE_HEIGHT) {
		RETRO_RageQuit("The image must be 256x256\n");
	}

	for (int i = 0; i < ANGLE_STEPS; i++) {
		double angle = i * 2 * M_PI / ANGLE_STEPS;
		SinTable[i] = lround(sin(angle) * TRIG_SCALE);
		CosTable[i] = lround(cos(angle) * TRIG_SCALE);
	}

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int dx = x - RETRO_WIDTH / 2;
			int dy = y - RETRO_HEIGHT / 2;
			RadiusTable[y][x] = lround(hypot(dx, dy));
			AngleTable[y][x] = lround(atan2(dy, dx) * ANGLE_STEPS / (2 * M_PI));
		}
	}
}
