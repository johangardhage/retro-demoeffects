//
// dottorus.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"

#define RADIUS 75
#define MAXPOINTS 2020
#define POINTSTEP 0.1
#define ZMIN (-80)
#define ZMAX 80

int NumPoints = 0;
Vertex Torus[MAXPOINTS];

void DEMO_Render(double deltatime)
{
	static float ax, ay, az, distance = 1.0;
	ax += deltatime * 1;
	ay += deltatime * 1;
	az += deltatime * 1;

	for (int i = 0; i < NumPoints; i++) {
		RETRO_RotateVertex(&Torus[i], ax, ay, az);
		RETRO_ProjectVertex(&Torus[i], distance);

		int x = Torus[i].sx;
		int y = Torus[i].sy;
		int z = -round(Torus[i].rz);

		if (x >= 0 && x <= RETRO_WIDTH && y >= 0 && y <= RETRO_HEIGHT)  {
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

	// Generate torus
	for (float alpha = 2 * M_PI; alpha > 0; alpha -= POINTSTEP) {
		for (float beta = M_PI; beta > 0; beta -= POINTSTEP) {
			float r = RADIUS * sin(beta);
			Torus[NumPoints].x = r * cos(alpha) * sin(beta);
			Torus[NumPoints].y = 0.8333 * r * cos(beta);
			Torus[NumPoints].z = r * sin(alpha) * sin(beta);
			if (++NumPoints > MAXPOINTS) {
				RETRO_RageQuit("Too many points\n");
			}
		}
	}
}
