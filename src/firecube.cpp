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

	RETRO_Blur(RETRO_BLUR_8, 3);
	RETRO_Flip();
}

void DEMO_Initialize(void)
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

	RETRO_CreateCube4Model();

	Model3D *model = RETRO_Get3DModel();
	model->c = 80;
	model->cintensity = 100;
}
