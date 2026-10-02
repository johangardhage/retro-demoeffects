//
// Dot cube, shaded
//
// Same dot cube as dotcube.cpp, but each vertex takes its brightness from
// RETRO_ShadeFractionFromLambert(N·L), N being the vertex normal
// RETRO_InitializeVertexNormals averaged in from the faces around it. That
// clamps a vertex turned away from the light to the ramp's own floor, so the
// floor is a dim gray rather than black - every vertex still shows, just
// dimmer the more it faces away. Euler angles live on 2π.
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
	RETRO_RenderDotModel(RETRO_Get3DModel(), true);
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_CreateGradientPalette(1, RETRO_COLORS, RETRO_ASHGRAY, RETRO_WHITE);

	Model3D *model = RETRO_Load3DModel("assets/subcubequads.obj");
	model->c = 1;
	model->shades = RETRO_COLORS - 1;

	RETRO_InitializeLightSource(0, 0, -1);
}
