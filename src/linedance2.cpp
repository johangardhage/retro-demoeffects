//
// linedance.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

void DEMO_Render(double deltatime)
{
	static int a1 = 0;
	a1 += 1;

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
	unsigned char r = 10, g = 0, b = 0;
	for (int i = 0; i < 256; i++) {
		RETRO_SetColor(i, 0, 0, 0);
	}
	for (int i = 50; i < 95; i++) {
		RETRO_SetColor(i, r * 4, 0, 0);
		r++;
	}
	for (int i = 95; i < 135; i++) {
		RETRO_SetColor(i, 55 * 4, g * 4, 0);
		g++;
	}
	for (int i = 135; i < 200; i++) {
		RETRO_SetColor(i, r * 4, 55 * 4, b * 4);
		b++;
	}
}
