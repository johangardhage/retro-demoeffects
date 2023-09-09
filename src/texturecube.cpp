//
// Texture mapped cube
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropoly.h"
#include "lib/retro3d.h"
#include "lib/retrorender.h"

void DEMO_Render(double deltatime)
{
	static float ax, ay, az;
	ax += deltatime * 2;
	ay += deltatime * 2;
	az += deltatime * 2;

	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_SortVisibleFaces();
	RETRO_RenderModel(RETRO_POLY_TEXTURE);
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/cube_256x256.pcx");
	RETRO_SetPalette(RETRO_ImagePalette());

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->texmap = RETRO_ImageData();
}
