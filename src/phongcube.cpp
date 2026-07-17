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
	RETRO_Palette PhongPalette[RETRO_COLORS] = { { 0, 0, 0 } };
	RETRO_CreatePlasticPhongPalette(PhongPalette, 30, 255);
	RETRO_SetPalette(PhongPalette);

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->c = RETRO_PAL_OFFSET;
	model->cintensity = RETRO_PAL_SIZE;

	RETRO_InitializeLightSource(0, 0, -1);
}
