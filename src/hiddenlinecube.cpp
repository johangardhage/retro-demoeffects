//
// Hidden line cube
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
	RETRO_RenderModel(RETRO_POLY_HIDDENLINE);
}

void DEMO_Initialize()
{
	// Init palette
	RETRO_SetColor(0, 0, 0, 0);
	RETRO_SetColor(1, RANDOM(RETRO_COLORS), RANDOM(RETRO_COLORS), RANDOM(RETRO_COLORS));
	RETRO_SetPalette();

	RETRO_CreateCube4Model();
	RETRO_InitializeVertexNormals();

	Model3D *model = RETRO_Get3DModel();
	model->c = 1;
}
