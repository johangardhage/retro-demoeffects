//
// Glenz shaded cube
//
// The same additive blit as glenzcube.cpp, but each face writes a
// Lambert shade instead of a bit:
//
//   shade = c + face->c + (N · L) · intensity
//
// Back faces use half the Lambert term. There is no ShadeFractionFromLambert:
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
#include "lib/retropalette.h"

#define ROTATION_SPEED 2 // radians a second, about each axis
#define FACE_SHADE 30 // palette entries a face adds before its Lambert term

void DEMO_Render(RETRO_Time time)
{
	// Calculate rotation
	float ax = fmod(time.total * ROTATION_SPEED, 2 * M_PI);
	float ay = fmod(time.total * ROTATION_SPEED, 2 * M_PI);
	float az = fmod(time.total * ROTATION_SPEED, 2 * M_PI);

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

	// Every face starts FACE_SHADE entries up the ramp, so even one turned
	// edge-on adds something
	Model3D *model = RETRO_Load3DModel("assets/cubequads.obj");
	for (int i = 0; i < model->faces; i++) {
		model->face[i].c = FACE_SHADE;
	}
	model->c = 0;
	model->shades = 64;
}
