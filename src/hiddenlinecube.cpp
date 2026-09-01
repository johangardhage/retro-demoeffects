//
// Hidden line cube
//
// Same wireframe as wirecube.cpp, but only the front faces. The
// screen-space cross (s1 − s0) × (s2 − s0) < 0 drops the back faces, so
// an edge that belongs only to a back face disappears. Shared edges of a
// front face stay. Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

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
	RETRO_RenderModel(RETRO_POLY_HIDDENLINE);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_SetColor(1, RANDOM(RETRO_COLORS), RANDOM(RETRO_COLORS), RANDOM(RETRO_COLORS));

	Model3D *model = RETRO_Load3DModel("assets/cubequads.obj");
	model->c = 1;
}
