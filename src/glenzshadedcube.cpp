//
// Glenz filled cube
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
	RETRO_RenderModel(RETRO_POLY_GLENZ, RETRO_SHADE_FLAT);
}

void DEMO_Initialize(void)
{
	// Single faces shade from black into purple, only where three glenz faces
	// sum on top of each other does the purple wash out into almost white
	RETRO_CreateGradientPalette(8, 190, RETRO_BLACK, RETRO_MAGENTA);
	RETRO_CreateGradientPalette(190, RETRO_COLORS, RETRO_MAGENTA, RETRO_WHITE);

	Model3D *model = RETRO_Load3DModel("assets/cube4.obj");
	int c[6] = {30, 30, 30, 30, 30, 30};
	for (int i = 0; i < model->faces; i++) {
		model->face[i].c = c[i];
	}
	model->c = 0;
	model->cintensity = 64;

	RETRO_InitializeLightSource(0, 0, -1);
}
