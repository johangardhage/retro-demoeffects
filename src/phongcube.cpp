//
// Phong shaded cube
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrocolor.h"

void DEMO_Render(double deltatime)
{
	static float ax, ay, az;
	ax += deltatime * 2;
	ay += deltatime * 2;
	az += deltatime * 2;

	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_PHONG);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_Palette PhongPalette[RETRO_COLORS];
	RETRO_CreateRandomPhongPalette(PhongPalette);
	RETRO_Set6bitPalette(PhongPalette);

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->c = 0;
	model->cintensity = 63 - model->c;

	RETRO_InitializeLightSource(0, 0, -1);
}
