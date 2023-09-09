//
// Environment mapped cube
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
	RETRO_RenderModel(RETRO_POLY_ENVIRONMENT);
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/mask_phongmap_256x256.pcx");
	RETRO_Set6bitPalette(RETRO_ImagePalette());

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->envmap = RETRO_ImageData();
	model->c = 128;
	model->cintensity = 90;
}
