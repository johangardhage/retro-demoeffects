//
// burncube.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retro3d2.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

#define EYEDISTANCE 256
#define SCALE 50
#define VERTICES 8
#define FACES 6

Point3Df Points[VERTICES] = { {1, 1, 1}, {1, 1, -1}, {1, -1, 1}, {1, -1, -1}, {-1, 1, 1}, {-1, 1, -1}, {-1, -1, 1}, {-1, -1, -1} };
int Faces[FACES][4] = { {4, 0, 1, 5}, {1, 0, 2, 3}, {5, 1, 3, 7}, {4, 5, 7, 6}, {0, 4, 6, 2}, {3, 2, 6, 7} };

void DEMO_Render2(double deltatime)
{
	// Calculate frame
	static double ax, ay, az;
	ax += deltatime * 2;
	ay += deltatime * 1.6;
	az += deltatime * 2;

	Point2D projected[VERTICES];

	for (int i = 0; i < VERTICES; i++) {
		Point3Df rotated = RETRO_Rotate(Points[i], ax, ay, az);
		projected[i] = RETRO_Project(rotated, EYEDISTANCE, SCALE);
	}

	srand(az + ax + ay);
	unsigned char colors[RETRO_WIDTH];
	for (int i = 0; i < RETRO_WIDTH; i++) {
		colors[i] = 80 + rand() % 100;
	}

	for (int i = 0; i < FACES; i++) {
		RETRO_DrawLine2(projected[Faces[i][0]].x, projected[Faces[i][0]].y, projected[Faces[i][1]].x, projected[Faces[i][1]].y, colors);
		RETRO_DrawLine2(projected[Faces[i][1]].x, projected[Faces[i][1]].y, projected[Faces[i][2]].x, projected[Faces[i][2]].y, colors);
		RETRO_DrawLine2(projected[Faces[i][2]].x, projected[Faces[i][2]].y, projected[Faces[i][3]].x, projected[Faces[i][3]].y, colors);
		RETRO_DrawLine2(projected[Faces[i][3]].x, projected[Faces[i][3]].y, projected[Faces[i][0]].x, projected[Faces[i][0]].y, colors);
	}

	RETRO_Blur(RETRO_BLUR_8, 3);
	RETRO_Flip();
}

void DEMO_Initialize()
{
	// Init palette
	unsigned char r = 0, g = 0, b = 0;
	for (int i = 0; i < 256; i++) {
		RETRO_SetColor(i, 0, 0, 0);
	}
	for (int i = 0; i < 63; i++) {
		RETRO_SetColor(i, r * 4, 0, 0);
		r++;
	}
	for (int i = 63; i < 127; i++) {
		RETRO_SetColor(i, 63 * 4, g * 4, 0);
		g++;
	}
	for (int i = 127; i < 190; i++) {
		RETRO_SetColor(i, 63 * 4, 63 * 4, b * 4);
		b++;
	}
	RETRO_SetPalette();
}
