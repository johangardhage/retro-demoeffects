//
// dottunnel.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

#define MAXDEGREES		256
#define POINTSTEP       5
#define NUMCIRCLES      87
#define NUMPOINTS		(MAXDEGREES / POINTSTEP)
#define ZSTEP           5
#define ZMIN            (-240)
#define ZMAX            (NUMCIRCLES * ZSTEP)
#define RADIUS          50
#define DIVD			128

Point2Df Circle[NUMPOINTS];

void DEMO_Render(double deltatime)
{
	static double counter = 0;
	counter -= deltatime * 100;
	int frame = counter;

	for (int i = 0; i < NUMCIRCLES; i++) {
		float xo = cos(((frame * 2 + i * 3) * 2.0 * M_PI / MAXDEGREES)) * DIVD / 4 + sin(((frame + i * 2) * 2.0 * M_PI / MAXDEGREES)) * DIVD / 3;
		float yo = cos(((frame * 2 + i * 2) * 2.0 * M_PI / MAXDEGREES)) * DIVD / 5 + sin(((frame * 2 + i * 3) * 2.0 * M_PI / MAXDEGREES)) * DIVD / 4;

		float z = ZMIN + i * ZSTEP;

		for (int j = 0; j < NUMPOINTS; j++) {
			// Project coordinates
			int x = (RETRO_WIDTH / 2) + (Circle[j].x * 250) / (z - 250) + xo;
			int y = (RETRO_HEIGHT / 2) + (Circle[j].y * 250) / (z - 250) + yo;

			if (x >= 0 && x < RETRO_WIDTH && y >= 0 && y < RETRO_HEIGHT) {
				int color = floor((z - ZMIN) * (64.0 / ZMAX));
				RETRO_PutPixel(x, y, color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	for (int i = 0; i < 64; i++) {
		RETRO_SetColor(i, i * 4, i * 4, i * 4);
	}

	// Init circle points
	for (int i = 0; i < NUMPOINTS; i++) {
		Circle[i].x = RADIUS * cos(i * POINTSTEP * 2.0 * M_PI / MAXDEGREES) * DIVD / (DIVD - 20);
		Circle[i].y = RADIUS * sin(i * POINTSTEP * 2.0 * M_PI / MAXDEGREES) * DIVD / (DIVD);
	}
}
