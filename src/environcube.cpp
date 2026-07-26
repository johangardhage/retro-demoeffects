//
// Environment mapped cube
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrocolor.h"

#define PHONGMAP_SIZE 512

static unsigned char PhongMap[PHONGMAP_SIZE * PHONGMAP_SIZE];

void DEMO_Render(double deltatime)
{
	static float ax, ay, az;
	ax += deltatime * 2;
	ay += deltatime * 2;
	az += deltatime * 2;

	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_ENVIRONMENT, RETRO_SHADE_PHONG);
}

void DEMO_Initialize(void)
{
	RETRO_Palette PhongPalette[RETRO_COLORS] = { { 0, 0, 0 } };
	RETRO_CreatePlasticPhongPalette(PhongPalette, 30, 255);
	RETRO_SetPalette(PhongPalette);
	RETRO_CreatePhongMap(PhongMap, PHONGMAP_SIZE, PHONGMAP_SIZE);

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->envmap = PhongMap;
	model->envmapwidth = PHONGMAP_SIZE;
	model->envmapheight = PHONGMAP_SIZE;
	model->c = PHONGMAP_SIZE / 2;
	model->cintensity = PHONGMAP_SIZE / 2;
}
