//
// Gouraud shaded sphere
//
// N·L at each of spherequads.obj's 114 vertex normals (the analytic outward
// normals of a unit sphere, 22.5° apart both ways), then ShadeFractionFromLambert,
// then the shade is interpolated affinely in screen space so a shared edge
// agrees. That is three times denser than a cube's eight corner normals, 70°
// apart, so the highlight can sit tighter than gouraudcube.cpp's without
// falling between samples and vanishing. Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define ROTATION_SPEED 2 // radians a second, about each axis

void DEMO_Render(double time, double deltatime)
{
	// Calculate rotation
	float ax = fmod(time * ROTATION_SPEED, 2 * M_PI);
	float ay = fmod(time * ROTATION_SPEED, 2 * M_PI);
	float az = fmod(time * ROTATION_SPEED, 2 * M_PI);

	// Draw sphere
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_GOURAUD);
}

void DEMO_Initialize(void)
{
	// Init palette. A tighter highlight than gouraudcube.cpp's, since the
	// sphere's vertex normals sit 22.5° apart rather than 70° and can resolve
	// one without it falling between samples
	RETRO_CreatePlasticPalette(RETRO_DEEPPINK, 12);

	Model3D *model = RETRO_Load3DModel("assets/spherequads.obj");
	model->c = RETRO_PHONG_OFFSET;
	model->shades = RETRO_PHONG_SHADES;

	RETRO_InitializeLightSource(0, 0, -1);
}
