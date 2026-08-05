//
// Rotozoom
//
// A wrapping texture rotated and scaled about the top-left corner. Screen
// point (x, y) samples
//
//   (u, v) = s R_θ (x, y)
//
// with s = 1 + sin θ ∈ [0, 2] and R_θ the 2D rotation by θ. The origin is
// the corner, not the centre, so the picture swings around that point. The
// inner step is one DDA increment (s cos θ, s sin θ) per pixel. θ lives
// on 360°.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define TEXTURE_WIDTH 256
#define TEXTURE_HEIGHT 256
#define ROTATION_SPEED 100 // degrees a second

void DEMO_Render(double deltatime)
{
	// Calculate angle
	static double angle = 0;
	angle = fmod(angle + deltatime * ROTATION_SPEED, RETRO_DEGREES_PER_TURN);

	unsigned char *image = RETRO_ImageData();

	// Calculate movement
	float sina = sin(angle * DEG2RAD);
	float cosa = cos(angle * DEG2RAD);

	float scale = sina + 1.0f;
	float dtx = cosa * scale;
	float dty = sina * scale;

	// Draw texture
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
