//
// Flat filled cube
//
// One constant color per face, no lighting. Face i stores
//
//   c_i = (i + 1) · 234 / faces
//
// so the six sides are spread across a plastic phong palette that is
// used only as a ramp of distinct colors, not as a material. The
// shared cube path then fills each front face with that index. Euler
// angles live on 2π.
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

	// Draw cube
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_FLAT);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreatePlasticPhongPalette(30);

	// The faces are not shaded, so spread their colors over the palette
	Model3D *model = RETRO_Load3DModel("assets/cubequads.obj");
	for (int i = 0; i < model->faces; i++) {
		model->face[i].c = (i + 1) * 234 / model->faces;
	}
}
