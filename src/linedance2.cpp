//
// linedance.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocolor.h"

void DEMO_Render(double deltatime)
{
	static double a1 = 0;
	a1 += deltatime * 60;

	for (int b1 = 0; b1 < 360; b1++) {
		float a2 = a1 * M_PI / 180;
		float b2 = b1 * M_PI / 180;
		float x = 160 + 50 * sin(b2 + a2 * 2) + 25 * sin(a2 + b2 * 2) - 50 * sin(a2 + b2);
		float y = 120 + 20 * sin(a2 + b2 * 2) + 15 * sin(b2 + a2 * 2) + 20 * sin(a2 + b2);
		RETRO_PutPixel(x, y, 50 + x / 2);
	}
}

void DEMO_Initialize(void)
{
	// Init palette. The lines pick their color from their x position, so they
	// heat up from red through orange into white from left to right
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK, RETRO_BLACK);
	RETRO_CreateGradientPalette(50, 95, RETRO_BLACK, RETRO_RED);
	RETRO_CreateGradientPalette(95, 135, RETRO_RED, RETRO_ORANGE);
	RETRO_CreateGradientPalette(135, 200, RETRO_ORANGE, RETRO_WHITE);
}
