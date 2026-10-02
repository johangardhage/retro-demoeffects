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

void DEMO_Render(double time, double deltatime)
{
	// Calculate rotation
	float ax = fmod(time * ROTATION_SPEED, 2 * M_PI);
	float ay = fmod(time * ROTATION_SPEED, 2 * M_PI);
	float az = fmod(time * ROTATION_SPEED, 2 * M_PI);

	// Draw cube
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderDotModel(RETRO_Get3DModel(), false, true);
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_SetColor(1, RETRO_WHITE);

	Model3D *model = RETRO_Load3DModel("assets/subcubequads.obj");
	model->c = 1;
}
