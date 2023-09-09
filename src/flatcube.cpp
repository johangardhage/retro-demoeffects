//
// Flat filled cube
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
	RETRO_RenderModel(RETRO_POLY_FLAT);
}

void DEMO_Initialize(void)
{
	// 0-32 green
	float p = 2.8;
	float r = 4;
	for (int i = 0; i < 32; i++) {
		RETRO_SetColor(i, 0, r, 0);
		r = r + p;
	}

	Model3D *model = RETRO_Load3DModel("assets/cube4.obj");
	for (int i = 0; i < model->faces; i++) {
		model->face[i].c = i + 15;
	}
}
