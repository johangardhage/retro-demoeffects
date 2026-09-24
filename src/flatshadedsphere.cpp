//
// Flat shaded sphere
//
// One Lambert term per face of spherequads.obj. The color is
//
//   c + intensity · ShadeFractionFromLambert(N · L)
//
// with L = (0, 0, −1) and ShadeFractionFromLambert = 1 − acos(N·L)/(π/2), so the
// shades are even in θ, matching the palette. spherequads.obj's faces carry
// the analytic outward normal at their centre, one per face, so the sphere
// reads as 128 flat facets rather than a smooth globe: the polygon budget
// shows through instead of hiding behind interpolation. The palette is
// matte: a specular highlight would flash a whole facet at once because the
// face has only one normal. Euler angles live on 2π.
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

	// Draw sphere
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_FLAT, RETRO_SHADE_FLAT);
}

void DEMO_Initialize(void)
{
	// Init palette. Matte, because a flat lit face has one normal for all of
	// it and a specular highlight would flash the whole facet at once
	RETRO_CreateMattePalette();

	Model3D *model = RETRO_Load3DModel("assets/spherequads.obj");
	model->c = RETRO_PHONG_OFFSET;
	model->shades = RETRO_PHONG_SHADES;

	RETRO_InitializeLightSource(0, 0, -1);
}
