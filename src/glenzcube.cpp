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
	RETRO_ProjectModel(1);
	RETRO_RenderModel(RETRO_POLY_GLENZ);
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(0, 0, 0, 0);
	RETRO_SetColor(1, 0, 0, 0);
	RETRO_SetColor(2, 40 * 4, 15 * 4, 15 * 4); // dark red
	RETRO_SetColor(3, 60 * 4, 24 * 4, 24 * 4); // light red
	RETRO_SetColor(4, 60 * 4, 60 * 4, 60 * 4); // light white
	RETRO_SetColor(5, 63 * 4, 63 * 4, 63 * 4); // dark white

	Model3D *model = RETRO_Load3DModel("assets/glenz3.obj");
	int c[model->faces] = { 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 2, 2, 1, 1, 2, 2 };

	for (int i = 0; i < model->faces; i++) {
		model->face[i].c = c[i];
	}
}
