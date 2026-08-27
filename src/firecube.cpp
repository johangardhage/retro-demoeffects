//
// Burning wireframe cube
//
// The cube is drawn as wireframe into a framebuffer that is never cleared,
// then the same 8-tap fire blur as fire.cpp runs once per step. Shade
// SHADE_WIREFIRE writes color + random(intensity) along each edge, so the
// edges seed the flame and the blur lifts it. Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrocolor.h"

#define ROTATION_SPEED 2 // radians a second, about each axis

//
// Advance the trail in fixed steps. It is one blur pass per step into a framebuffer
// that is never cleared, so its length follows the step rate.
//
void DEMO_FixedUpdate(double timestep)
{
	static float ax, ay, az;
	ax = fmod(ax + timestep * ROTATION_SPEED, 2 * M_PI);
	ay = fmod(ay + timestep * ROTATION_SPEED, 2 * M_PI);
	az = fmod(az + timestep * ROTATION_SPEED, 2 * M_PI);

	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_WIREFRAME, RETRO_SHADE_WIREFIRE);

	RETRO_Blur(RETRO_BLUR_FIRE, 3);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK, RETRO_BLACK);
	RETRO_CreateGradientPalette(0, 63, RETRO_BLACK, RETRO_RED);
	RETRO_CreateGradientPalette(63, 127, RETRO_RED, RETRO_YELLOW);
	RETRO_CreateGradientPalette(127, 190, RETRO_YELLOW, RETRO_WHITE);

	Model3D *model = RETRO_Load3DModel("assets/cubequads.obj");
	model->c = 80;
	model->shades = 100;
}
