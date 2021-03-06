//
// Flat shaded cube
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
	RETRO_RotateFaceNormals();
	RETRO_ProjectVertices(0.5);
	RETRO_SortVisibleFaces();
	RETRO_RenderModel(RETRO_POLY_FLAT, RETRO_SHADE_FLAT);
}

void DEMO_Initialize()
{
	float p = 1.8;
	float d = 4, r = d;
	for (int i = 0; i < 32; i++) {
		RETRO_SetColor(i, 0, 0, r);
		r = r + p;
	}
	r = d;
	for (int i = 32; i < 64; i++) {
		RETRO_SetColor(i, r, r, r);
		r = r + p;
	}
	RETRO_SetPalette();

	RETRO_CreateCube3Model();
	RETRO_InitializeFaceNormals();
	RETRO_InitializeLightSource(0, 120, 10);

	Model3D *model = RETRO_Get3DModel();
	int c[model->faces] = {32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0};
	for (int i = 0; i < model->faces; i++) {
		model->face[i].c = c[i];
	}
	model->cintensity = 32;
}
