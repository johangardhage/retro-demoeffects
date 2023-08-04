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

	unsigned char *image = RETRO_TextureImage();

	// Calculate movement
	float sina = sin(angle * M_PI / 180.0f);
	float cosa = cos(angle * M_PI / 180.0f);

	// Draw texture
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int tx = (unsigned int)(( x * cosa - y * sina) * (sina + 1)) % TEXTURE_WIDTH;
			int ty = (unsigned int)(( x * sina + y * cosa) * (sina + 1)) % TEXTURE_HEIGHT;
			int color = image[ty * TEXTURE_WIDTH + tx];

			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadTexture("assets/rotozoom_256x256.pcx");
	RETRO_SetPalette(RETRO_TexturePalette());
}
