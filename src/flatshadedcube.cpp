//
// Flat shaded cube
//
// One Lambert term per face. The color is
//
//   c + intensity · ShadeFromLambert(N · L)
//
// with L = (0, 0, −1) and ShadeFromLambert = 1 − acos(N·L)/(π/2), so the
// shades are even in θ, matching the palette. The palette is matte: a
// specular highlight would flash the whole face at once because the
// face has only one normal. Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrocolor.h"

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
	RETRO_RenderModel(RETRO_POLY_FLAT, RETRO_SHADE_FLAT);
}

void DEMO_Initialize(void)
{
	// Init palette. Matte, because a flat lit face has one normal for all of
	// it and a specular highlight would flash the whole face at once
	RETRO_CreateMattePalette();

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->c = RETRO_PHONG_OFFSET;
	model->cintensity = RETRO_PHONG_SHADES;

	RETRO_InitializeLightSource(0, 0, -1);
}
