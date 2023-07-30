//
// whitenoise.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

void DEMO_Render(double deltatime)
{
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			unsigned char color = rand() & 1 ? 255 : 0;
			RETRO_PutPixel(x, y, color);
		}
	}

	RETRO_Blur(RETRO_BLUR_3);
}

void DEMO_Initialize(void)
{
	// Init palette
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, i, i, i);
	}
}
