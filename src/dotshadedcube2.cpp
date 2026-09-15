//
// Dot cube, shaded, outer dots only
//
// dotshadedcube.cpp's lighting and dotcube2.cpp's visibility test together:
// only vertices of a front-facing face are stamped, each shaded by
// RETRO_ShadeFromLambert(N·L) against its own vertex normal. The ramp still
// keeps a dim floor rather than black so a silhouette fade cannot fall
// through to the background. Euler angles live on 2π.
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
	RETRO_RenderDotModel(Model, true, true);
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_CreateGradientPalette(1, RETRO_COLORS, RETRO_ASHGRAY, RETRO_WHITE);

	Model = RETRO_Load3DModel("assets/subcubequads.obj");
	Model->c = 1;
	Model->shades = RETRO_COLORS - 1;

	RETRO_InitializeLightSource(0, 0, -1);
}
