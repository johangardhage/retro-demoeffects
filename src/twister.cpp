//
// twister.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrogfx.h"
#include "lib/retromain.h"

void DEMO_Render(double deltatime)
{
	static float angle = 0;

	float a = angle;
	float b = a + M_PI / 2;
	float c = a + M_PI;
	float d = a + (M_PI / 2) * 3;

	angle += 0.02;

	for (int i = 0; i < RETRO_WIDTH; i++) {
		float v = (float)i / RETRO_HEIGHT * 2;

		Point2Df points[4];
		points[0].x = 100 + (sin(a + v) * 60);
		points[0].y = 0 + i;
		points[1].x = 100 + (sin(b + v) * 60);
		points[1].y = 0 + i;
		points[2].x = 100 + (sin(c + v) * 60);
		points[2].y = 0 + i;
		points[3].x = 100 + (sin(d + v) * 60);
		points[3].y = 0 + i;

		if (points[0].x < points[1].x) {
			RETRO_DrawLine(points[0].x, points[0].y, points[1].x, points[1].y, 1);
		}
		if (points[1].x < points[2].x) {
			RETRO_DrawLine(points[1].x, points[1].y, points[2].x, points[2].y, 2);
		}
		if (points[2].x < points[3].x) {
			RETRO_DrawLine(points[2].x, points[2].y, points[3].x, points[3].y, 3);
		}
		if (points[3].x < points[0].x) {
			RETRO_DrawLine(points[3].x, points[3].y, points[0].x, points[0].y, 4);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetPalette(RETRO_Default8bitPalette);
}
