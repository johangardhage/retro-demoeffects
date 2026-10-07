//
// Dot cube, shaded, outer dots only
//
// dotshadedcube.cpp's lighting and dotcube2.cpp's visibility test together:
// only vertices of a front-facing face are stamped, each shaded by
// RETRO_ShadeFractionFromLambert(N·L) against its own vertex normal. The
// ramp still keeps a dim floor rather than black so a silhouette fade cannot
// fall through to the background. Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define ROTATION_SPEED 2 // radians a second, about each axis

void DEMO_Render(RETRO_Time time)
{
	// Calculate rotation
	float ax = fmod(time.total * ROTATION_SPEED, 2 * M_PI);
	float ay = fmod(time.total * ROTATION_SPEED, 2 * M_PI);
	float az = fmod(time.total * ROTATION_SPEED, 2 * M_PI);

	// Draw cube
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderDotModel(RETRO_Get3DModel(), true, true);
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_CreateGradientPalette(1, RETRO_COLORS, RETRO_ASHGRAY, RETRO_WHITE);

	Model3D *model = RETRO_Load3DModel("assets/subcubequads.obj");
	model->c = 1;
	model->shades = RETRO_COLORS - 1;
}
