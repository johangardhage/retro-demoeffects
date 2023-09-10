//
// magiclines.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retromath.h"

#define POINTS 170

Point2Df Points[POINTS];

void DrawLines(int x, int y, float k)
{
	Points[0].x = x;
	Points[0].y = y;

	for (int i = 1; i < POINTS; i++) {
		Points[i].x = (Points[i].x + Points[i - 1].x) / (2.0 + k / POINTS);
		Points[i].y = (Points[i].y + Points[i - 1].y) / (2.0 + k / POINTS);

		int x1 = CLAMPWIDTH(Points[i].x);
		int x2 = CLAMPWIDTH(Points[i - 1].x);
		int y1 = CLAMPHEIGHT(Points[i].y);
		int y2 = CLAMPHEIGHT(Points[i - 1].y);

		RETRO_DrawLine(x1, y1, x2, y2, 255);
		RETRO_DrawLine(x1, (RETRO_HEIGHT-1) - y1, x2, (RETRO_HEIGHT-1) - y2, 255);
		RETRO_DrawLine((RETRO_WIDTH-1) - x1, y1, (RETRO_WIDTH-1) - x2, y2, 255);
		RETRO_DrawLine((RETRO_WIDTH-1) - x1, (RETRO_HEIGHT-1) - y1, (RETRO_WIDTH-1) - x2, (RETRO_HEIGHT-1) - y2, 255);
	}
}

void DEMO_Render2(double deltatime)
{
	// Calculate frame
	static double frame = 0;
	frame += deltatime * 2.5;

	// Calculate movement
	float aa = frame / 1.37;
	float rx = fabs(sin(sin(frame / 4.1) * M_PI) * 90) + 9;
	float ry = fabs(cos(cos(frame / 1.3) * M_PI) * 90) + 9;
	float xx = cos(cos(frame / 2.0) * M_PI) * rx;
	float yy = sin(cos(frame / 2.7) * M_PI) * ry;

	int x = (RETRO_WIDTH/2) + xx * cos(aa) + yy * sin(aa);
	int y = (RETRO_HEIGHT/2) + xx * -sin(aa) + yy * cos(-aa);
	float k = sin(frame / 15.0) * 1.0 + 0.75;

	// Draw lines
	DrawLines(x, y, k);

	RETRO_Blur(RETRO_BLUR_8, sin(aa / 8.0) * 4 + 5);
	RETRO_Flip();
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetColor(0, 0, 0, 0);
	for (int i = 1; i < RETRO_COLORS; i++) {
		float k = pow(sin(M_PI / 511.0 * i), 16) * 128 + (32.0 * pow((float) i / 256.0, 3));
		k = k > 63 ? 63 : k;
		RETRO_SetColor(i, k * 4, k * 4, i);
	}
}
