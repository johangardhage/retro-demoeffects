//
// Plasma 3
//
// Three moving sine waves are averaged into a low-resolution color field. One
// wave travels horizontally, another vertically, and a third crosses the image
// diagonally. The diagonal wave continually changes its horizontal and vertical
// slopes, bending the interference bands into drifting blobs and ribbons.
//
// The field is calculated at 80x50 and every sample is expanded to a 4x4 block,
// giving the effect its deliberately chunky pixels. Color indices run through a
// fire palette from dark blue and red to orange, yellow and white.
//
#define RETRO_HEIGHT 200

#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocolor.h"

#define PLASMA_WIDTH (RETRO_WIDTH / 4)
#define PLASMA_HEIGHT (RETRO_HEIGHT / 4)
#define PLASMA_SCALE 4
#define PLASMA_SPEED 70

unsigned char Sin256[256];

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * PLASMA_SPEED, 256);
	int iphase = phase;

	unsigned char *buffer = RETRO_FrameBuffer();

	int slopeX = Sin256[WRAP256(iphase * 3)];
	int slopeY = Sin256[WRAP256(64 + iphase * 5)];

	for (int y = 0; y < PLASMA_HEIGHT; y++) {
		int vertical = Sin256[WRAP256(y * 3 + iphase * 3)];

		for (int x = 0; x < PLASMA_WIDTH; x++) {
			int horizontal = Sin256[WRAP256(x * 3 + iphase * 2)];
			int diagonal = Sin256[WRAP256((x * slopeX) / PLASMA_WIDTH + (y * slopeY) / PLASMA_HEIGHT + iphase)];
			unsigned char color = (horizontal + diagonal + vertical) / 3;

			int left = x * PLASMA_SCALE;
			int top = y * PLASMA_SCALE;
			for (int yy = 0; yy < PLASMA_SCALE; yy++) {
				memset(buffer + (top + yy) * RETRO_WIDTH + left, color, PLASMA_SCALE);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	// Init colors
	RETRO_CreateGradientPalette(0, 8, RETRO_BLACK, RETRO_BLUEBLACK);
	RETRO_CreateGradientPalette(8, 24, RETRO_BLUEBLACK, RETRO_DARKRED);
	RETRO_CreateGradientPalette(24, 56, RETRO_DARKRED, RETRO_SCARLET);
	RETRO_CreateGradientPalette(56, 192, RETRO_SCARLET, RETRO_YELLOW);
	RETRO_CreateGradientPalette(192, 256, RETRO_YELLOW, RETRO_WHITE);

	// Init tables
	for (int i = 0; i < 256; i++) {
		Sin256[i] = 255 * (sin(2 * M_PI * i / 255.0) + 1) / 2;
	}
}
