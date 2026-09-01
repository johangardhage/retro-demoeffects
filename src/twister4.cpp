//
// Twister 4
//
// The textured horizontal-scanline column from twister3, with depth shading.
// The flowers picture already owns all 256 palette entries, so shading cannot
// use separate palette ramps. Instead, TwisterShadeTable maps every source texel and
// one of TWISTER_SHADES brightness levels back to the nearest color in the
// picture's own palette.
//
// Each row still draws the two visible faces from leftmost corner through the
// nearest corner to the rightmost. Their corner depths provide the endpoint
// shades, which are interpolated across each textured span.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retroshadetable.h"

#define TWISTER_PERIOD 512
#define TWISTER_CYCLE ((double)RETRO_SINCOS_ANGLE / TWISTER_PERIOD)
#define TWISTER_CENTER_X (RETRO_WIDTH / 2)
#define TWISTER_RADIUS 60
#define TWISTER_DEPTH_RATE 30
#define TWISTER_TURNS 1.5
#define TWISTER_TORSION 1.2
#define TWISTER_TORSION_WAVE 7
#define TWISTER_SPEED 30.0

#define TWISTER_IMAGE_SIZE 256
#define TWISTER_IMAGE_FACE (TWISTER_IMAGE_SIZE / 4)
#define TWISTER_IMAGE_SCROLL 2.0
#define TWISTER_SHADES 64

unsigned char TwisterShadeTable[RETRO_COLORS * TWISTER_SHADES];

//
// One scanline of one face, half-open in x. Texture position and depth shade
// both advance from where the unclipped span begins.
//
void DrawSpan(int left, int right, int y, unsigned char *texels, int base, float left_shade, float right_shade)
{
	if (right <= left) {
		return;
	}

	float du = (float)TWISTER_IMAGE_FACE / (right - left);
	float ds = (right_shade - left_shade) / (right - left);

	int x0 = MAX(left, 0);
	int x1 = MIN(right, RETRO_WIDTH);
	float u = base + (x0 - left) * du;
	float shade = left_shade + (x0 - left) * ds;

	unsigned char *row = RETRO_FrameBuffer() + y * RETRO_WIDTH;
	for (int x = x0; x < x1; x++, u += du, shade += ds) {
		unsigned char texel = texels[(int)u];
		row[x] = TwisterShadeTable[texel * TWISTER_SHADES + CLAMP(shade, 0, TWISTER_SHADES)];
	}
}

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase and torsion
	double phase = fmod(time * TWISTER_SPEED, TWISTER_PERIOD);
	double torsion = TWISTER_TORSION * SIN(phase * TWISTER_TORSION_WAVE * TWISTER_CYCLE) * COS(phase * TWISTER_CYCLE);
	double scroll = phase * TWISTER_IMAGE_SCROLL;

	unsigned char *image = RETRO_ImageData();

	// Draw column
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		double index = y * torsion + phase;
		int v = WRAP(y + scroll, TWISTER_IMAGE_SIZE);
		unsigned char *texels = image + v * TWISTER_IMAGE_SIZE;

		double angle = index * TWISTER_TURNS * TWISTER_CYCLE;
		double sin_radius = TWISTER_RADIUS * SIN(angle);
		double cos_radius = TWISTER_RADIUS * COS(angle);
		int corner_x[4] = {
			(int)lround(TWISTER_CENTER_X - cos_radius),
			(int)lround(TWISTER_CENTER_X + sin_radius),
			(int)lround(TWISTER_CENTER_X + cos_radius),
			(int)lround(TWISTER_CENTER_X - sin_radius),
		};
		double corner_z[4] = {
			sin_radius,
			cos_radius,
			-sin_radius,
			-cos_radius,
		};

		int face = 0;
		for (int corner = 1; corner < 4; corner++) {
			if (corner_x[corner] < corner_x[face]) {
				face = corner;
			}
		}

		int corner0 = face;
		int corner1 = (face + 1) & 3;
		int corner2 = (face + 2) & 3;
		float shade0 = (TWISTER_DEPTH_RATE * (TWISTER_SHADES - 1)) / (TWISTER_RADIUS - corner_z[corner0] + TWISTER_DEPTH_RATE);
		float shade1 = (TWISTER_DEPTH_RATE * (TWISTER_SHADES - 1)) / (TWISTER_RADIUS - corner_z[corner1] + TWISTER_DEPTH_RATE);
		float shade2 = (TWISTER_DEPTH_RATE * (TWISTER_SHADES - 1)) / (TWISTER_RADIUS - corner_z[corner2] + TWISTER_DEPTH_RATE);

		DrawSpan(corner_x[corner0], corner_x[corner1], y, texels, corner0 * TWISTER_IMAGE_FACE, shade0, shade1);
		DrawSpan(corner_x[corner1], corner_x[corner2], y, texels, corner1 * TWISTER_IMAGE_FACE, shade1, shade2);
	}
}

void DEMO_Initialize(void)
{
	// Load the image and keep its full palette
	RETRO_LoadImage("assets/flowers_256x256.pcx", true);
	RETRO_Palette *palette = RETRO_ImagePalette();

	// Map every darkened source color back into the image's own palette
	RETRO_CreatePaletteShadeTable(palette, RETRO_COLORS, TWISTER_SHADES, TwisterShadeTable);
}
