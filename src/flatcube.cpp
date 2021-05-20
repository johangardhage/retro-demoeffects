//
// Flat filled cube
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
	RETRO_SortVisibleFaces();
	RETRO_RenderModel(RETRO_POLY_FLAT);
}

void DEMO_Initialize()
{
	// 0-32 green
	float p = 1.8;
	float r = 4;
	for (int i = 0; i < 32; i++) {
		RETRO_SetColor(i, 0, r, 0);
		r = r + p;
	}
	RETRO_SetPalette();

	RETRO_CreateCube4Model();

	Model3D *model = RETRO_Get3DModel();
	for (int i = 0; i < model->faces; i++) {
		model->face[i].c = i + 15;
	}
}
