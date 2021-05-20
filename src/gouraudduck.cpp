//
// Gouraud shaded duck
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropoly.h"
#include "lib/retro3d.h"

void DEMO_Render(double deltatime)
{
	static float ax, ay, az;
	ax += deltatime * 2;
	ay += deltatime * 2;
	az += deltatime * 2;

	RETRO_RotateMatrix(ax, ay, az);
	RETRO_RotateVertices();
	RETRO_RotateVertexNormals();
	RETRO_ProjectVertices(0.5);
	RETRO_SortVisibleFaces();
	RETRO_RenderModel(RETRO_POLY_GOURAUD);
}

void DEMO_Initialize()
{
	// Init palette
	unsigned char r = 0, g = 0, b = 0;
	for (int i = 0; i < 64; i++) {
		RETRO_SetColor(i, r, 0, 0);
		r++;
	}
	for (int i = 64; i < 96; i++) {
		RETRO_SetColor(i, r, g, b);
		r++;
		g += 11;
		b += 11;
	}
	RETRO_SetPalette();

	RETRO_Load3DModel("assets/duck.asc");
	RETRO_InitializeVertexNormals();
	RETRO_InitializeLightSource(0, 120, 10);

	Model3D *model = RETRO_Get3DModel();
	model->c = 32;
	model->cintensity = 46;
}
