//
// rotozoom.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define TEXTURE_WIDTH 256
#define TEXTURE_HEIGHT 256
#define SINE_VALUES 1024

void DEMO_Render(double deltatime)
{
	// Calculate frame
	static double angle = 0;
	angle += deltatime * 100;

	unsigned char *image = RETRO_ImageData();

	// Calculate movement
	float sina = sin(angle * M_PI / 180.0f);
	float cosa = cos(angle * M_PI / 180.0f);

	float scale = sina + 1.0f;
	float dtx = cosa * scale;
	float dty = sina * scale;

	// Draw texture using DDA (Digital Differential Analyzer) algorithm
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		float tx = (-y * sina) * scale;
		float ty = (y * cosa) * scale;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int itx = WRAP(tx, TEXTURE_WIDTH);
			int ity = WRAP(ty, TEXTURE_HEIGHT);
			int color = image[ity * TEXTURE_WIDTH + itx];

			RETRO_PutPixel(x, y, color);
			tx += dtx;
			ty += dty;
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/flowers_256x256.pcx");
	RETRO_SetPalette(RETRO_ImagePalette());
}
