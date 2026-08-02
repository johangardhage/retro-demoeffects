//
// Flat filled cube
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
	RETRO_RenderModel(RETRO_POLY_FLAT);
}

void DEMO_Initialize(void)
{
	// 0-32 green
	RETRO_CreateGradientPalette(0, 32, {0, 4, 0}, {0, 94, 0});

	Model3D *model = RETRO_Load3DModel("assets/cube4.obj");
	for (int i = 0; i < model->faces; i++) {
		model->face[i].c = i + 15;
	}
}
