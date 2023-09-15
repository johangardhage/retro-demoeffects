//
// dotball.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"

#define RADIUS 75
#define MAXPOINTS 2020
#define POINTSTEP 0.1
#define ZMIN 20
#define ZMAX 80

int NumPoints = 0;
Vertex Ball[MAXPOINTS];

void DEMO_Render(double deltatime)
{
	static float ax, ay, az, distance = 1.0;
	ax += deltatime * 1;
	ay += deltatime * 1;
	az += deltatime * 1;

	for (int i = 0; i < NumPoints; i++) {
		RETRO_RotateVertex(&Ball[i], ax, ay, az);
		RETRO_ProjectVertex(&Ball[i], distance);

		int x = Ball[i].sx;
		int y = Ball[i].sy;
		int z = -round(Ball[i].rz);

		if (x >= 0 && x <= RETRO_WIDTH && y >= 0 && y <= RETRO_HEIGHT && z > ZMIN && z < ZMAX) {
			int color = floor((z + abs(ZMIN)) * (64.0 / (abs(ZMIN) + ZMAX)));
			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	for (int i = 0; i < 64; i++) {
		RETRO_SetColor(i, i * 4, i * 4, i * 4);
	}

	// Generate ball
	for (float alpha = 2 * M_PI; alpha > 0; alpha -= POINTSTEP) {
		for (float beta = M_PI; beta > 0; beta -= POINTSTEP) {
			Ball[NumPoints].x = RADIUS * cos(alpha) * sin(beta);
			Ball[NumPoints].y = RADIUS * cos(beta);
			Ball[NumPoints].z = RADIUS * sin(alpha) * sin(beta);
			if (++NumPoints > MAXPOINTS) {
				RETRO_RageQuit("Too many points\n");
			}
		}
	}
}
