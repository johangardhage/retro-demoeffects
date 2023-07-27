//
// Blurred wireframe cube
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropoly.h"
#include "lib/retro3d.h"

void DEMO_Render2(double deltatime)
{
	static float ax, ay, az;
	ax += deltatime * 2;
	ay += deltatime * 2;
	az += deltatime * 2;

	RETRO_RotateMatrix(ax, ay, az);
	RETRO_RotateVertices();
	RETRO_ProjectVertices(0.5);
	RETRO_SortAllFaces();
	RETRO_RenderModel(RETRO_POLY_WIREFRAME);

	unsigned char *dest = RETRO_GetFrameBuffer();

	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT / 2; i++) {
		if (dest[i] != 0) {
			dest[i]--;
		}
		if (dest[i + RETRO_WIDTH * RETRO_HEIGHT / 2] != 0) {
			dest[i + RETRO_WIDTH * RETRO_HEIGHT / 2]--;
		}
	}

	RETRO_Flip();
}

void DEMO_Initialize()
{
	// Init palette
	float p = 1;
	float d = 0, r = d;
	for (int i = 0; i < 256; i++) {
		RETRO_SetColor(i, 0, 0, 0);
	}
	for (int i = 32; i < 96; i++) {
		RETRO_SetColor(i, r, 0, 0);
		r += p;
	}
	r = d;
	for (int i = 96; i < 160; i++) {
		RETRO_SetColor(i, r, 0, r);
		r += p;
	}
	r = d;
	for (int i = 168; i < 208; i++) {
		RETRO_SetColor(i, r, r, r);
		r += p;
	}

	RETRO_CreateCube4Model();

	Model3D *model = RETRO_Get3DModel();
	model->c = 90;
}
