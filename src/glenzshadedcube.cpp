//
// Glenz shaded cube
//
// The same additive blit as glenzcube.cpp, but each face writes a
// Lambert shade instead of a bit:
//
//   shade = c + face->c + (N · L) · intensity
//
// Back faces use half the Lambert term. There is no ShadeFromLambert:
// the palette is a linear black–magenta–white gradient, and converting
// θ would bend that falloff. Faces add, so a pixel covered three times
// walks far enough up the ramp to wash out toward white. Euler angles
// live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrocolor.h"

#define ROTATION_SPEED 2 // radians a second, about each axis

void DEMO_Render(double deltatime)
{
	// Rotate
	static float ax, ay, az;
	ax = fmod(ax + deltatime * ROTATION_SPEED, 2 * M_PI);
	ay = fmod(ay + deltatime * ROTATION_SPEED, 2 * M_PI);
	az = fmod(az + deltatime * ROTATION_SPEED, 2 * M_PI);

	// Draw cube
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

	Model3D *model = RETRO_Load3DModel("assets/cubequads.obj");
	int c[6] = {30, 30, 30, 30, 30, 30};
	for (int i = 0; i < model->faces; i++) {
		model->face[i].c = c[i];
	}
	model->c = 0;
	model->shades = 64;

	RETRO_InitializeLightSource(0, 0, -1);
}
