//
// Phong shaded cube
//
// The same eight corner normals as gouraudcube.cpp, stored as n q and
// interpolated in screen space. Normalising the interpolated n q is the
// same direction as divide-by-q then normalise (q > 0 in front of the
// near plane). The pixel is then
//
//   I = ShadeFromLambert(max(N · L, 0))
//   color = c + intensity · I
//
// with L = (0, 0, −1). The palette falloff is 30: tight enough to read
// as a highlight, broad enough that a per-pixel N can resolve it. Euler
// angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

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
	RETRO_RenderModel(RETRO_POLY_PHONG);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreatePlasticPhongPalette(30);

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->c = RETRO_PHONG_OFFSET;
	model->shades = RETRO_PHONG_SHADES;

	RETRO_InitializeLightSource(0, 0, -1);
}
