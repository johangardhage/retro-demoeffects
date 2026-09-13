//
// Dot cube, shaded
//
// Same dot cube as dotcube.cpp, but each vertex takes its brightness from
// RETRO_ShadeFromLambert(N·L), N being the vertex normal RETRO_InitializeVertexNormals
// averaged in from the faces around it. That clamps a vertex turned away from
// the light to the ramp's own floor, so the floor is a dim gray rather than
// black - every vertex still shows, just dimmer the more it faces away.
// Euler angles live on 2π.
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
	RETRO_RenderDotModel(Model, true);
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_CreateGradientPalette(1, RETRO_COLORS, RETRO_RGB(0x484848), RETRO_WHITE);

	Model = RETRO_Load3DModel("assets/rubbercubequads.obj");
	Model->c = 1;
	Model->shades = RETRO_COLORS - 1;

	RETRO_InitializeLightSource(0, 0, -1);
}
