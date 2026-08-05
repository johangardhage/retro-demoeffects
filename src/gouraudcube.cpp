//
// Gouraud shaded cube
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
	RETRO_RenderModel(RETRO_POLY_GOURAUD);
}

void DEMO_Initialize(void)
{
	// Init palette. A broad highlight, because gouraud samples the lighting
	// only at the 8 corner normals, 70 degrees apart, and a tight highlight
	// falls between them and shows up as a glint on one corner
	RETRO_CreatePlasticPhongPalette(5);

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->c = RETRO_PHONG_OFFSET;
	model->cintensity = RETRO_PHONG_SHADES;

	RETRO_InitializeLightSource(0, 0, -1);
}
