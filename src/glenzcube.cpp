//
// Glenz filled cube
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropoly.h"
#include "lib/retro3d.h"

void DEMO_Render(double deltatime)
{
	static float ax, ay, az;
	ax += deltatime * 2;
	ay += deltatime * 2;
	az += deltatime * 2;

	RETRO_RotateMatrix(ax, ay, az);
	RETRO_RotateVertices();
	RETRO_ProjectVertices(0.5);
	RETRO_SortAllFaces();
	RETRO_RenderModel(RETRO_POLY_GLENZ);
}

void DEMO_Initialize()
{
	// 8-128 purple
	float p = 0.8;
	float d = 10, r = d;
	for (int i = 8; i < 128; i++) {
		RETRO_SetColor(i, r - 15, 0, r);
		r += p;
	}
	RETRO_SetPalette();

	RETRO_CreateCube4Model();

	Model3D *model = RETRO_Get3DModel();
	int c[model->faces] = {10, 20, 30, 20, 30, 10};
	for (int i = 0; i < model->faces; i++) {
		model->face[i].c = c[i];
	}
}
