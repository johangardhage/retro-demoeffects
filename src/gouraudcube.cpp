//
// Gouraud shaded cube
//
// N·L at each of the eight corner normals (the cube's vn are the
// diagonals (±1, ±1, ±1)/√3, about 70° apart), then ShadeFromLambert,
// then the shade is interpolated affinely in screen space so a shared
// edge agrees. A tight highlight falls between those corners and shows
// up as a glint on one vertex. The palette falloff is 5 to spread the
// sheen across that gap. Euler angles live on 2π.
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
	RETRO_RenderModel(RETRO_POLY_GOURAUD);
}

void DEMO_Initialize(void)
{
	// Init palette. A broad highlight, because gouraud samples the lighting
	// only at the 8 corner normals, 70 degrees apart, and a tight highlight
	// falls between them and shows up as a glint on one corner
	RETRO_CreatePlasticPhongPalette(5);

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->c = RETRO_PHONG_OFFSET;
	model->cintensity = RETRO_PHONG_SHADES;

	RETRO_InitializeLightSource(0, 0, -1);
}
