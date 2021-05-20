//
// Environment mapped cube
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
	RETRO_RotateVertexNormals();
	RETRO_ProjectVertices(0.5);
	RETRO_SortVisibleFaces();
	RETRO_RenderModel(RETRO_POLY_ENVIRONMENT);
}

void DEMO_Initialize()
{
	RETRO_LoadTexture("assets/texturecube_128x128.pcx");
	RETRO_SetPaletteFromTexture();

	RETRO_CreateCube3Model();
	RETRO_InitializeVertexNormals();

	Model3D *model = RETRO_Get3DModel();
	model->texture = RETRO_GetTextureImage();
	model->c = 64;
	model->cintensity = 64;
}
