//
// stars.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retrocolor.h"

#define NUM_STARS 1000
#define SPEED 2
#define ZMIN (-250)
#define ZMAX 250

Vertex Stars[NUM_STARS];

void DEMO_Render(double deltatime)
{
	static float ax, ay, az, distance = 1.0;
	ax += deltatime * SPEED;
	ay += deltatime * SPEED;
	az += deltatime * SPEED;

	for (int i = 0; i < NUM_STARS; i++) {
		RETRO_RotateVertex(&Stars[i], ax, ay, az);
		if (distance * Stars[i].rz + 250 <= 1.0f) {
			continue;
		}
		RETRO_ProjectVertex(&Stars[i], distance);

		int x = Stars[i].sx;
		int y = Stars[i].sy;
		int z = -round(Stars[i].rz);

		if (x >= 0 && x < RETRO_WIDTH && y >= 0 && y < RETRO_HEIGHT && z < 150) {
			int color = CLAMP64((z + abs(ZMIN)) * (63.0 / (abs(ZMIN) + ZMAX)));
			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, 64, RETRO_BLACK, RETRO_WHITE);

	for (int i = 0; i < NUM_STARS; i++) {
		Stars[i].x = RANDOM(RETRO_WIDTH * 2) - RETRO_WIDTH;
		Stars[i].y = RANDOM(RETRO_HEIGHT * 2) - RETRO_HEIGHT;
		Stars[i].z = RANDOM(ZMAX * 2) - ZMAX;
	}
}
