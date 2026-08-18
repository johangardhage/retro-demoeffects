//
// White noise
//
// Salt-and-pepper, then a 5-tap plus-with-center blur. Each pixel is
// independently 0 or 255 (P = 1/2); RETRO_BLUR_SMOOTH replaces it with
// the mean of itself and its four neighbours, every tap read from the
// field before the pass, so the softening has no direction. Averaging
// five independent ±127.5 draws leaves a standard deviation of
// 127.5/√5, so the result is gray grain rather than hard dots. The
// framebuffer is cleared and redrawn every frame.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retrocolor.h"

void DEMO_Render(double deltatime)
{
	// Draw noise
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			unsigned char color = RANDOM(2) ? 255 : 0;
			RETRO_PutPixel(x, y, color);
		}
	}

	// Soften
	RETRO_Blur(RETRO_BLUR_SMOOTH);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK, RETRO_WHITE);
}
