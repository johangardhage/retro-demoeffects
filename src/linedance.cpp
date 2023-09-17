//
// linedance.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

void DEMO_Render(double deltatime)
{
	static float i1 = 10;
	static float i2 = -20;
	static float i3 = -30;

	i1 += deltatime * 100;
	i2 -= deltatime * 100;
	i3 -= deltatime * 200;

	for (int i = 0; i < RETRO_HEIGHT; i++) {
		float sin1 = sin((i + i1) * 2 * M_PI / 256) * 10;
		float sin2 = cos((i + i2) * 4 * M_PI / 256) * 15;
		float sin3 = sin((i + i3) * 4 * M_PI / 256) * 15;
		float sum = sin1 + sin2 + sin3;

		int x1 = (RETRO_WIDTH / 2) - 50 - sum;
		int x2 = (RETRO_WIDTH / 2) + 50 + sum;

		if (x1 >= 0 && x1 < RETRO_WIDTH && x2 >= 0 && x2 < RETRO_WIDTH) {
			RETRO_DrawLine(x1, i, x2, i, 255);
		}
	}
}

void DEMO_Initialize(void)
{
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, i, i, i);
	}
}
