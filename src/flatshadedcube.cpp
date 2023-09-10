//
// Flat shaded cube
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"

void DEMO_Render(double deltatime)
{
	static float ax, ay, az;
	ax += deltatime * 2;
	ay += deltatime * 2;
	az += deltatime * 2;

	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_FLAT, RETRO_SHADE_FLAT);
}

void DEMO_Initialize(void)
{
	float p = 1.8;
	float d = 4, r = d;
	for (int i = 0; i < 32; i++) {
		RETRO_SetColor(i, 0, r, 0);
		r = r + p;
	}

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->cintensity = 32;

	RETRO_InitializeLightSource(0, 0, -1);
}
