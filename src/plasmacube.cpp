//
// Plasma (texture) mapped cube
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropoly.h"
#include "lib/retro3d.h"

#define PLASMA_SIN 1800

int SinTable[PLASMA_SIN];

unsigned char image[16384];

void DEMO_Render(double deltatime)
{
	static float frame;
	frame += deltatime * 200;
	int ap = (int)frame % 720;

	// Generate plasma
	for (int x = 0; x < 128; x++) {
		int c1 = 75 + (SinTable[x * 2 + ap / 2] + SinTable[x + ap * 2] + SinTable[x / 2 + ap] * 2) / 32;
		for (int y = 0; y < 128; y++) {
			int c2 = 75 + (SinTable[y + ap * 2] * 2 + SinTable[y * 2 + ap / 2] + SinTable[y + ap] * 2) / 16;
			image[x + 128 * y] = c1 + c2;
		}
	}

	static float ax, ay, az;
	ax += deltatime * 2;
	ay += deltatime * 2;
	az += deltatime * 2;

	RETRO_RotateMatrix(ax, ay, az);
	RETRO_RotateVertices();
	RETRO_RotateVertexNormals();
	RETRO_ProjectVertices(0.5);
	RETRO_SortVisibleFaces();
	RETRO_RenderModel(RETRO_POLY_TEXTURE);
}

void DEMO_Initialize()
{
	unsigned char r = 0, g = 0, b = 0;
	for (int i = 0; i < 256; i++) {
		RETRO_SetColor(i, 0, 0, 0);
	}
	for (int i = 0; i < 42; i++) {
		RETRO_SetColor(i, r, g, b);
		r++;
	}
	for (int i = 42; i < 84; i++) {
		RETRO_SetColor(i, r, g, b);
		g++;
	}
	for (int i = 84; i < 126; i++) {
		RETRO_SetColor(i, r, g, b);
		b++;
	}
	for (int i = 126; i < 168; i++) {
		RETRO_SetColor(i, r, g, b);
		r--;
	}
	for (int i = 168; i < 210; i++) {
		RETRO_SetColor(i, r, g, b);
		g--;
	}
	for (int i = 210; i < 252; i++) {
		RETRO_SetColor(i, r, g, b);
		b--;
	}

	RETRO_CreateCube3Model();
	RETRO_InitializeVertexNormals();

	Model3D *model = RETRO_Get3DModel();
	model->texture = image;

	for (int i = 0; i < PLASMA_SIN; i++) {
		SinTable[i] = sin((M_PI * i) / 180) * 1024;
	}
}
