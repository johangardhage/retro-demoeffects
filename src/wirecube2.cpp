//
// Wireframe cube, trailing
//
// The same cube redrawn TRAIL_COUNT times a frame, each copy rotated to an
// earlier instant of the same spin, TRAIL_STEP seconds behind the one ahead
// of it, and each a step dimmer - so a fan of ghosted cubes trails the
// leading one instead of a single wireframe box, all the edges from every
// copy visible at once. Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define ROTATION_SPEED 2 // radians a second, about each axis
#define TRAIL_COUNT 6 // ghost cubes drawn each frame, leading one included
#define TRAIL_STEP 0.015 // seconds each ghost trails the one ahead of it

static Model3D *Cube;

void DEMO_Render(double time, double deltatime)
{
	// Oldest ghost first, leading cube last, so each draw's plain overwrite
	// leaves the brightest copy on top instead of buried under the fainter ones.
	for (int i = TRAIL_COUNT - 1; i >= 0; i--) {
		double t = time - i * TRAIL_STEP;
		float ax = fmod(t * ROTATION_SPEED, 2 * M_PI);
		float ay = fmod(t * ROTATION_SPEED, 2 * M_PI);
		float az = fmod(t * ROTATION_SPEED, 2 * M_PI);

		Cube->c = TRAIL_COUNT - i; // brightest for the leading cube, fading behind it

		RETRO_RotateModel(ax, ay, az, Cube);
		RETRO_ProjectModel(RETRO_PROJECTION_SCALE, RETRO_WIDTH / 2.0, RETRO_HEIGHT / 2.0, Cube);
		RETRO_RenderModel(RETRO_POLY_WIREFRAME, RETRO_SHADE_NONE, Cube);
	}
}

void DEMO_Initialize(void)
{
	// Init palette: one ramp, index TRAIL_COUNT for the leading cube down to
	// 1 for the oldest ghost
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_CreateGradientPalette(1, TRAIL_COUNT + 1, RETRO_BLACK, RETRO_WHITE);

	Cube = RETRO_Load3DModel("assets/cubequads.obj");
}
