//
// Burning wireframe cube
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropoly.h"
#include "lib/retro3d.h"

void DEMO_Render2(double deltatime)
{
	static float ax, ay, az;
	ax += deltatime * 2;
	ay += deltatime * 2;
	az += deltatime * 2;

	RETRO_RotateMatrix(ax, ay, az);
	RETRO_RotateVertices();
	RETRO_ProjectVertices(0.5);
	RETRO_SortAllFaces();
	RETRO_RenderModel(RETRO_POLY_WIREFIRE);

	unsigned char *burn = RETRO_GetFrameBuffer();

	for (int a = 0; a < RETRO_WIDTH; a++) {
		for (int b = 0; b < RETRO_HEIGHT - 1; b++) {
			if (burn[a + (b + 1) * RETRO_WIDTH] > 0 || burn[a + b * RETRO_WIDTH] > 0)
				burn[a + b * RETRO_WIDTH] = ((burn[(a - 1) + (b + 1) * RETRO_WIDTH] + burn[(a + 1) + (b + 1) * RETRO_WIDTH] + burn[a + (b + 1) * RETRO_WIDTH] + burn[a + (b + 2) * RETRO_WIDTH]) >> 2);
			if (burn[a + b * RETRO_WIDTH] > 3)
				burn[a + b * RETRO_WIDTH] -= 2;
		}
	}

	RETRO_Flip();
}

void DEMO_Initialize(void)
{
	// Init palette
	unsigned char r = 0, g = 0, b = 0;
	for (int i = 0; i < 256; i++)
		RETRO_SetColor(i, 0, 0, 0);

	for (int i = 0; i < 63; i++) {
		RETRO_SetColor(i, r, 0, 0);
		r++;
	}
	for (int i = 63; i < 127; i++) {
		RETRO_SetColor(i, 63, g, 0);
		g++;
	}
	for (int i = 127; i < 190; i++) {
		RETRO_SetColor(i, 63, 63, b);
		b++;
	}

	RETRO_CreateCube4Model();

	Model3D *model = RETRO_Get3DModel();
	model->c = 80;
	model->cintensity = 100;
}
