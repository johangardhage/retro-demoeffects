//
// Glenz filled cube
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
	RETRO_RenderModel(RETRO_POLY_GLENZ, RETRO_SHADE_FLAT);
}

void DEMO_Initialize(void)
{
	// 8-128 purple
	float p = 0.8;
	float d = 10, r = d;
	for (int i = 8; i < 128; i++) {
		RETRO_SetColor(i, r - 15, 0, r);
		r += p;
	}

	Model3D *model = RETRO_Load3DModel("assets/cube4.obj");
	int c[model->faces] = {30, 30, 30, 30, 30, 30};
	for (int i = 0; i < model->faces; i++) {
		model->face[i].c = c[i];
	}
	model->c = 0;
	model->cintensity = 64;

	RETRO_InitializeLightSource(0, 0, -1);
}
