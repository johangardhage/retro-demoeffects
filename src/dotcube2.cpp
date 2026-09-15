//
// Dot cube, outer dots only
//
// Same as dotcube.cpp, but only vertices of a front-facing face are
// stamped - the same screen-space winding RETRO_SortFaces uses for
// hiddenlinecube.cpp, so the far side of the cube disappears. Euler
// angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define ROTATION_SPEED 2 // radians a second, about each axis

static Model3D *Model;

void DEMO_Render(double time, double deltatime)
{
	float ax = fmod(time * ROTATION_SPEED, 2 * M_PI);
	float ay = fmod(time * ROTATION_SPEED, 2 * M_PI);
	float az = fmod(time * ROTATION_SPEED, 2 * M_PI);

	RETRO_RotateModel(ax, ay, az, Model);
	RETRO_ProjectModel(RETRO_PROJECTION_SCALE, RETRO_WIDTH / 2.0, RETRO_HEIGHT / 2.0, Model);
	RETRO_RenderDotModel(Model, false, true);
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_SetColor(1, RETRO_WHITE);

	Model = RETRO_Load3DModel("assets/subcubequads.obj");
	Model->c = 1;
}
