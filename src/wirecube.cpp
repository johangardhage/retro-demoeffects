//
// Wireframe cube
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
	RETRO_RenderModel(RETRO_POLY_WIREFRAME);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetColor(0, 0, 0, 0);
	RETRO_SetColor(1, RANDOM(RETRO_COLORS), RANDOM(RETRO_COLORS), RANDOM(RETRO_COLORS));

	RETRO_CreateCube4Model();

	Model3D *model = RETRO_Get3DModel();
	model->c = 1;
}
